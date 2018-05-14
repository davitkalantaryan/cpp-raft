
#ifndef HEADER_SIZE_needed
#define HEADER_SIZE_needed
#endif

#include "raft_tcp_server.hpp"
#include <string>
#include <stdint.h>
#include <signal.h>

#define MIN_REP_RATE_MS					5
#define DEF_REP_RATE_MS					5000
#define	TIMEOUTS_RATIO_MIN				5
#define SOCK_TIMEOUT_MS					100000
#define REPORT_ON_FAULT(_faultyNode)

#ifdef _WIN32
#else
#define closesocket	close
#endif

#ifdef HANDLE_SIG_ACTIONS
#include <pthread.h>
static void SigActionFunction (int, siginfo_t *, void *);
static void AddNewRaftServer(raft::tcp::Server* a_pServer);
static void RemoveRaftServer(raft::tcp::Server* a_pServer);
#endif  // #ifdef HANDLE_SIG_ACTIONS

static std::mutex s_mutexForRaftSend;

namespace raft{namespace tcp{
int g_nLogLevel = 0;
}}

#define MAX_NUMBER_OF_PINGS				2
#define MAX_UNANSWERED_PINGS			5

typedef struct { common::SocketTCP dataSocket, raftSocket; int isEndianDiffer; }NodeTools;

static int CreateEmptySocket();

#define FOLLOWER_SEES_ERR(...)	Sleep(2000)

raft::tcp::Server::Server()
	:
	m_nWork(0),
	m_nPeriodForPeriodic(DEF_REP_RATE_MS),
	m_nPortOwn(0),
	m_pLeaderNode(NULL)
{
	raft_cbs_t aClbks;

#ifdef HANDLE_SIG_ACTIONS
    AddNewRaftServer(this);
#endif

	m_nWork = 0;
    m_infoSocketForRcvRaft2 = -1;
    m_infoSocketForRcvData2 = -1;

	aClbks.send     = &raft::tcp::Server::SendClbkFunction;
	aClbks.log      = &raft::tcp::Server::LogClbkFunction;
	aClbks.applylog = &raft::tcp::Server::ApplyLogClbkFunction;
	this->set_callbacks(&aClbks, this);
}


raft::tcp::Server::~Server()
{
	this->StopServer();
#ifdef HANDLE_SIG_ACTIONS
    RemoveRaftServer(this);
#endif //#ifdef HANDLE_SIG_ACTIONS
}


RaftNode2* raft::tcp::Server::RemoveNode(RaftNode2* a_node)
{
	NodeTools* pNodeTool = (NodeTools*)a_node->get_udata();
	RaftNode2* pRet = RaftServer::RemoveNode(a_node);

	if(pNodeTool){
		delete pNodeTool;
	}
	return pRet;
}


void raft::tcp::Server::ReceiveFromDataSocket(RaftNode2*) // this should be overriten by child
{
}


int raft::tcp::Server::RunServerOnOtherThreads(int a_nRaftPort, const std::vector<NodeIdentifierKey>& a_vectPossibleNodes, int a_nWorkersCount)
{
    DEBUG_HANGING();
	std::thread* pWorker;

#ifdef HANDLE_SIG_ACTIONS
    struct sigaction newAction;

    m_starterThread = pthread_self();

    newAction.sa_flags = SA_SIGINFO;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_restorer = NULL;
    newAction.sa_sigaction = SigActionFunction;

    sigaction(SIGPIPE,&newAction,NULL);
    sigaction(SIGINT,&newAction,NULL);
    //sigaction(SIG,&newAction,NULL);

#endif  // #ifdef HANDLE_SIG_ACTIONS

	if (m_nWork) {return -1;}

	m_nPortOwn = a_nRaftPort;
	if(m_nPeriodForPeriodic<MIN_REP_RATE_MS){ m_nPeriodForPeriodic = DEF_REP_RATE_MS;}
	if(this->request_timeout < m_nPeriodForPeriodic) { this->request_timeout = m_nPeriodForPeriodic;}
	if(this->election_timeout < (TIMEOUTS_RATIO_MIN*this->request_timeout)) { this->election_timeout =(TIMEOUTS_RATIO_MIN*this->request_timeout);}
	this->timeout_elapsed = 0;

	CheckAllPossibleSeeds(a_vectPossibleNodes);

	m_nWork = 1;

	m_threadTcpListen = std::thread(&Server::ThreadFunctionListen,this);
	m_threadPeriodic = std::thread(&Server::ThreadFunctionPeriodic, this);
	m_threadRcvRaftInfo = std::thread(&Server::ThreadFunctionRcvRaftInfo, this);
	m_threadRcvData = std::thread(&Server::ThreadFunctionRcvData, this);
	m_threadAddRemoveNode = std::thread(&Server::ThreadFunctionAddRemoveNode, this);
	for(int i(0);i<a_nWorkersCount;++i){
		pWorker = new std::thread(&Server::ThreadFunctionWorker, this);
		m_vectThreadsWorkers.push_back(pWorker);
	}

	return 0;
}


void raft::tcp::Server::StopServer()
{
	size_t i,unWorkersCount;

	if (m_nWork == 0) {return;}
	DEBUG_APPLICATION(1, "Stopping server");
	m_nWork = 0;

    InterruptRaftRcv();
    InterruptDataRcv();
	m_serverTcp.StopServer();

	m_semaAddRemove.post();

	unWorkersCount = m_vectThreadsWorkers.size();

	for(i=0;i<unWorkersCount;++i){
		m_semaWorker.post();
	}
	for(i=0;i<unWorkersCount;++i){
		m_vectThreadsWorkers[i]->join();
		delete m_vectThreadsWorkers[i];
	}
	m_vectThreadsWorkers.clear();

    DEBUG_APPLICATION(2," ");

    m_threadAddRemoveNode.join();
    DEBUG_APPLICATION(2," ");
	m_threadRcvData.join();
    DEBUG_APPLICATION(2," ");
	m_threadRcvRaftInfo.join();
    DEBUG_APPLICATION(2," ");
	m_threadPeriodic.join();
    DEBUG_APPLICATION(2," ");
	m_threadTcpListen.join();
    DEBUG_APPLICATION(2," ");


#if 0
	std::thread										m_threadTcpListen;
	std::thread										m_threadPeriodic;
	std::thread										m_threadRcvRaftInfo;
	std::thread										m_threadRcvData;
	std::vector<std::thread*>						m_vectThreadsWorkers;
#endif
}


void raft::tcp::Server::AddClient(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)		// 1. connect
{
	SWorkerData aWorkerData;

	aWorkerData.reqType = workRequest::handleConnection;
	aWorkerData.sockDescriptor = a_clientSock;
	memcpy(&aWorkerData.remAddress, a_remoteAddr,sizeof(sockaddr_in));
	a_clientSock.ResetSocketWithoutClose();  // let us assume, that worker will close in the case of necessirty
	m_fifoWorker.AddElement(aWorkerData);
	m_semaWorker.post();

}


void raft::tcp::Server::connect_allNodes_newNode(common::SocketTCP& a_clientSock)
{
	NodeIdentifierKey* pLeaderKey = (NodeIdentifierKey*)m_pLeaderNode->key2();
	a_clientSock.writeC(pLeaderKey, sizeof(NodeIdentifierKey));
}


bool raft::tcp::Server::connect_leader_newNode(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr,int a_nIsEndianDiffer)
{
	// struct NodeIdentifierKey { char ip4Address[MAX_IP4_LEN]; int32_t port;};
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
	NodeIdentifierKey	*pAllNodesInfo=NULL, *pTmpKey;
	NodeTools*			pNewNodeTool = NULL;
	NodeTools*			pOldNodeTool;
	RaftNode2*			pNode;
	NodeIdentifierKey*	pExistingNodeKey;
	SAddRemData			addNodeData;
	int i,nTotalSize, nLeadersCount,nBytesOfLeaders;
	int nSndRcv;
	char cRequest;
	bool bOk(false);

	addNodeData.nodeKey.set_ip4Address(common::socketN::GetIPAddress(a_remoteAddr));

	nSndRcv= a_clientSock.readC(&addNodeData.nodeKey.port,4);
	if(nSndRcv!= 4){ goto returnPoint;}
	if(a_nIsEndianDiffer){ SWAP4BYTES(addNodeData.nodeKey.port);}
	
	pNewNodeTool = new NodeTools;
	if (!pNewNodeTool) { HANDLE_MEM_DEF(" "); }
	pNewNodeTool->isEndianDiffer = a_nIsEndianDiffer;

	m_mutexShrd.lock_shared();

	nTotalSize = m_nNodesCount * sizeof(NodeIdentifierKey);
	pAllNodesInfo = (NodeIdentifierKey*)malloc(nTotalSize);
	if (!pAllNodesInfo) { HANDLE_MEM_DEF(" "); }

	// first let us to send to the new node all current nodes, that it is prepared
	// collect info
	i = 0;
	pNode = m_firstNode;
	while(pNode){
		pExistingNodeKey = (NodeIdentifierKey*)pNode->key2();
		pAllNodesInfo[i].set_ip4Address(pExistingNodeKey->ip4Address);
		pAllNodesInfo[i].port = pExistingNodeKey->port;
		pNode = pNode->next;
		++i;
	}

	m_mutexShrd.unlock_shared();

	nSndRcv= a_clientSock.writeC(&m_nNodesCount,4);
	if(nSndRcv!= 4){ goto returnPoint;}

	nSndRcv= a_clientSock.writeC(pAllNodesInfo,nTotalSize);
	if(nSndRcv!= nTotalSize){ goto returnPoint;}

	// Now let's try to see if there is a other leader in the net
	nSndRcv=a_clientSock.readC(&nLeadersCount, 4);
	if ((nSndRcv != 4)||(nLeadersCount<1)) { goto returnPoint; }
	nBytesOfLeaders=nLeadersCount*sizeof(NodeIdentifierKey);
	if(nTotalSize<nBytesOfLeaders){
		pTmpKey=(NodeIdentifierKey*)realloc(pAllNodesInfo,nBytesOfLeaders);
		if(!pTmpKey){HANDLE_MEM_DEF(" ");}
		pAllNodesInfo = pTmpKey;
	}
	nSndRcv = a_clientSock.readC(pAllNodesInfo,nBytesOfLeaders);
	if(nLeadersCount==1){free(pAllNodesInfo); pAllNodesInfo = NULL;}

	// now send to all existing nodes the info on new node
	cRequest = raft::receive::leader::newNode;
	m_mutexShrd.lock_shared();

	pNode = m_firstNode;
	while(pNode){
		if((pNode != m_thisNode)&&!pNode->isProblematic()){
			pOldNodeTool = (NodeTools*)pNode->get_udata();
			s_mutexForRaftSend.lock();
			nSndRcv=pOldNodeTool->raftSocket.writeC(&cRequest, 1); 
			if(nSndRcv!=1){s_mutexForRaftSend.unlock();pNode->setProblematic(1);goto nextNodePoint;}
			nSndRcv=pOldNodeTool->raftSocket.writeC(&addNodeData.nodeKey,sizeof(NodeIdentifierKey));  // let us assume that no error
			s_mutexForRaftSend.unlock();
			if(nSndRcv!= sizeof(NodeIdentifierKey)){pNode->setProblematic(1);goto nextNodePoint;}
			nSndRcv=pOldNodeTool->raftSocket.readC(&cRequest, 1); // 'o'->ok, 'e'->err
			if (nSndRcv != 1) { pNode->setProblematic(1); }
		}
nextNodePoint:
		pNode = pNode->next;
	}

	m_mutexShrd.unlock_shared();


	// finally let us add this new node here
	pNewNodeTool->raftSocket.SetNewSocketDescriptor(a_clientSock);
	addNodeData.pNode = new RaftNode2(pNewNodeTool);
	if(!addNodeData.pNode){HANDLE_MEM_DEF(" ");}
	a_clientSock.writeC("o", 1);
	
	// todo
	m_fifoAddDel.AddElement(addNodeData);
	m_semaAddRemove.post();

	if(nLeadersCount>1){
		DEBUG_APPLICATION(0,"!!!!!!!!!!!!!!!!!!!!!!! Other leaders exist. The info below");
		pExistingNodeKey = (NodeIdentifierKey*)m_thisNode->key2();
		for(i=0;i<nLeadersCount;++i){
			if((*pExistingNodeKey)== pAllNodesInfo[i]){continue;}
			DEBUG_APPLICATION_NO_ADD_INFO(0,"\t%s:%d\n",pAllNodesInfo[i].ip4Address,(int)pAllNodesInfo[i].port);
		}
	}

	bOk = true;
returnPoint:
	free(pAllNodesInfo);
	return bOk;
}


bool raft::tcp::Server::connect_anyNode_bridgeToNodeRaft(common::SocketTCP& a_clientSock, int a_isEndianDiffer)
{
	// this socket should remain 
	RaftNode2* pNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey aRemHost;
	int nSndRcv = a_clientSock.readC(&aRemHost, sizeof(NodeIdentifierKey));
	bool bOk(false);

	if (nSndRcv != sizeof(NodeIdentifierKey)) {
		REPORT_ON_FAULT(pNodeInfo[i]);
		//continue; // let us assume, that leader will stop faulty node
		goto returnPoint;
	}

	m_mutexShrd.lock_shared();
	if(!m_hashNodes.FindEntry(&aRemHost,sizeof(NodeIdentifierKey),&pNode)){
		m_mutexShrd.unlock_shared();
		goto returnPoint;
	}
	
	pNodeTools = (NodeTools*)pNode->get_udata();
	pNodeTools->isEndianDiffer = a_isEndianDiffer;
	pNodeTools->raftSocket.SetNewSocketDescriptor(a_clientSock);
    InterruptRaftRcv();
	m_mutexShrd.unlock_shared();

	bOk = true;
returnPoint:
	return bOk;
}


bool raft::tcp::Server::connect_anyNode_bridgeToNodeData(common::SocketTCP& a_clientSock, int a_isEndianDiffer)
{
	// this socket should remain 
	RaftNode2* pNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey aRemHost;
	int nSndRcv = a_clientSock.readC(&aRemHost, sizeof(NodeIdentifierKey));
	bool bOk(false);

	if (nSndRcv != sizeof(NodeIdentifierKey)) {
		REPORT_ON_FAULT(pNodeInfo[i]);
		//continue; // let us assume, that leader will stop faulty node
		goto returnPoint;
	}

	m_mutexShrd.lock_shared();
	if(!m_hashNodes.FindEntry(&aRemHost,sizeof(NodeIdentifierKey),&pNode)){
		m_mutexShrd.unlock_shared();
		goto returnPoint;
	}
	
	pNodeTools = (NodeTools*)pNode->get_udata();
	pNodeTools->isEndianDiffer = a_isEndianDiffer;
	pNodeTools->dataSocket.SetNewSocketDescriptor(a_clientSock);
    InterruptDataRcv();
	m_mutexShrd.unlock_shared();

	bOk = true;
returnPoint:
	return bOk;
}


void raft::tcp::Server::ThreadFunctionListen()
{
	m_serverTcp.setTimeout(SOCK_TIMEOUT_MS);
	m_serverTcp.StartServer(this, &raft::tcp::Server::AddClient,m_nPortOwn);
}


void raft::tcp::Server::HandleSeedClbk(RaftNode2* a_anyNode)
{
	NodeTools* pTools = (NodeTools*)a_anyNode->get_udata();
	NodeIdentifierKey aKey;
	int nSndRcv, nToReceive;
	int msg_type;
	bool bProblematic(true);

	nSndRcv = pTools->raftSocket.readC(&msg_type, 4);
	if (nSndRcv != 4) { goto returnPoint; }

	switch (msg_type) 
	{
	case RAFT_MSG_REQUESTVOTE:
	{
		msg_requestvote_t reqVote(0,0,0,0);
		DEBUG_APPLICATION(1,"RAFT_MSG_REQUESTVOTE");
		nSndRcv = pTools->raftSocket.readC(&reqVote, sizeof(msg_requestvote_t));
		if (nSndRcv != sizeof(msg_requestvote_t)) { goto returnPoint; }
		recv_requestvote(a_anyNode, &reqVote);
	}
	break;
	case RAFT_MSG_REQUESTVOTE_RESPONSE:
	{
		msg_requestvote_response_t  reqVoteResp;
		DEBUG_APPLICATION(1,"RAFT_MSG_REQUESTVOTE_RESPONSE");
		nSndRcv = pTools->raftSocket.readC(&reqVoteResp, sizeof(msg_requestvote_response_t));
		if (nSndRcv != sizeof(msg_requestvote_response_t)) { goto returnPoint; }
		recv_requestvote_response(a_anyNode, &reqVoteResp);
	}
	break;
	case RAFT_MSG_APPENDENTRIES:
	{
		MsgAppendEntries2 appEntries;
		DEBUG_APPLICATION(3,"RAFT_MSG_APPENDENTRIES");
		nSndRcv = pTools->raftSocket.readC(&appEntries, SIZE_OF_INITIAL_RCV_OF_MSG_APP);
		if (nSndRcv != SIZE_OF_INITIAL_RCV_OF_MSG_APP) { goto returnPoint; }
		if(appEntries.getNEntries()){
			nToReceive = appEntries.getNEntries() * sizeof(msg_entry_t);
			nSndRcv=pTools->raftSocket.readC(appEntries.entries(),nToReceive);
			if (nSndRcv != nToReceive) { goto returnPoint; }
		}
		recv_appendentries(a_anyNode, &appEntries);
	}
	break;
	case RAFT_MSG_APPENDENTRIES_RESPONSE:
	{
		msg_appendentries_response_t aApndResp;
		DEBUG_APPLICATION(3,"RAFT_MSG_APPENDENTRIES_RESPONSE");
		nSndRcv = pTools->raftSocket.readC(&aApndResp, sizeof(msg_appendentries_response_t));
		if (nSndRcv != sizeof(msg_appendentries_response_t)) { goto returnPoint; }
		a_anyNode->pingReceived();
		this->recv_appendentries_response(a_anyNode, &aApndResp);
		// a_anyNode->pingReceived(); // this does not work because of exception
	}
	break;
	default:
		DEBUG_APPLICATION(0,"raft-receive: default:");
		goto returnPoint;
	}

	bProblematic = false;
returnPoint:
	if (bProblematic) { a_anyNode->setProblematic(1); }
	else { a_anyNode->setProblematic(0); }
}


void raft::tcp::Server::ReceiveFromRaftSocket(RaftNode2* a_pNode)
{
	NodeTools *pNewNodeTools,*pTools = (NodeTools*)a_pNode->get_udata();
	SAddRemData			nodeData;
	int nSndRcv;
	bool bProblematic(true);
	char cRequest;

	nSndRcv = pTools->raftSocket.readC(&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}

	try {
		switch (cRequest)
		{
		case raft::receive::anyNode::clbkCmd:
			DEBUG_APPLICATION(2,"raft::receive::anyNode::clbkCmd");
			HandleSeedClbk(a_pNode);
			break;
		case raft::receive::leader::newNode:
			DEBUG_APPLICATION(1,"raft::receive::leader::newNode");
			if (a_pNode != m_pLeaderNode) { goto returnPoint; }
			nSndRcv = pTools->raftSocket.readC(&nodeData.nodeKey, sizeof(NodeIdentifierKey));
			if (nSndRcv != sizeof(NodeIdentifierKey)) { goto returnPoint; }
			if (pTools->isEndianDiffer) { SWAP4BYTES(nodeData.nodeKey.port); }
			pNewNodeTools = new NodeTools;
			if (!pNewNodeTools) { HANDLE_MEM_DEF(" "); }
			nodeData.pNode = new RaftNode2(pNewNodeTools);
			if (!nodeData.pNode) { HANDLE_MEM_DEF(" "); }

			nodeData.bAdd = true;
			m_fifoAddDel.AddElement(nodeData);
			m_semaAddRemove.post();

			cRequest = 'o';
			s_mutexForRaftSend.lock();
			pTools->raftSocket.writeC(&cRequest, 1);
			s_mutexForRaftSend.unlock();
			break;
		case raft::receive::leader::removeNode:
			DEBUG_APPLICATION(1,"raft::receive::leader::removeNode");
			if (a_pNode != m_pLeaderNode) { goto returnPoint; }
			nSndRcv = pTools->raftSocket.readC(&nodeData.nodeKey, sizeof(NodeIdentifierKey));
			if (nSndRcv != sizeof(NodeIdentifierKey)) { goto returnPoint; }
			if (pTools->isEndianDiffer) { SWAP4BYTES(nodeData.nodeKey.port); }

			if(m_hashNodes.FindEntry(&nodeData.nodeKey,sizeof(NodeIdentifierKey),&nodeData.pNode)){
				nodeData.bAdd = false;
				m_fifoAddDel.AddElement(nodeData);
				m_semaAddRemove.post();
			}

			cRequest = 'o';
			s_mutexForRaftSend.lock();
			pTools->raftSocket.writeC(&cRequest, 1);
			s_mutexForRaftSend.unlock();
			break;
		case raft::receive::leader::oldLeaderDied:
			DEBUG_APPLICATION(1,"raft::receive::leader::oldLeaderDied");
			nSndRcv = pTools->raftSocket.readC(&nodeData.nodeKey, sizeof(NodeIdentifierKey));
			if (nSndRcv != sizeof(NodeIdentifierKey)) { goto returnPoint; }
			if (pTools->isEndianDiffer) { SWAP4BYTES(nodeData.nodeKey.port); }
			if (!(nodeData.nodeKey == *(NodeIdentifierKey*)m_pLeaderNode->key2())) { goto returnPoint; }  // wrong old leader specified

			m_fifoAddDel.AddElement(m_pLeaderNode);
			m_semaAddRemove.post();
			m_pLeaderNode = a_pNode;
			a_pNode->makeLeader(1);

			cRequest = 'o';
			s_mutexForRaftSend.lock();
			pTools->raftSocket.writeC(&cRequest, 1);
			s_mutexForRaftSend.unlock();
			break;
		default:
			DEBUG_APPLICATION(0,"default:");
			break;
		}
	}
	catch (...){
	}


	bProblematic = false;
returnPoint:
	if(bProblematic){ a_pNode->setProblematic(1);}
	else{ a_pNode->setProblematic(0);}
}


void raft::tcp::Server::FunctionForMultiRcv(volatile int* a_pnSocketForInfo, void (Server::*a_fpRcvFnc)(RaftNode2*), bool a_bIsRaft)
{
	NodeTools* pNodeTools;
	RaftNode2* pNode;
	fd_set rFds, eFds;
	int nMax, nCurrentSocket, nSelectReturn, nSocketsFound, nSocketToIgnore=-1, nLastSocket;
    volatile int& nSocketForInfo = *a_pnSocketForInfo;
	while (m_nWork){
		FD_ZERO(&rFds); FD_ZERO(&eFds);
		m_mutexShrd.lock_shared();
		if(nSocketForInfo<=0){nSocketForInfo=CreateEmptySocket();}
		nMax = nSocketForInfo;
#ifdef _WIN32
		FD_SET(nSocketForInfo, &rFds);
#else
		FD_SET(nSocketForInfo, &eFds);
#endif
		pNode = m_firstNode;
		nLastSocket = -1;
		while(pNode){
			if((pNode!=m_thisNode)&&(!pNode->isProblematic())){
				pNodeTools = (NodeTools*)pNode->get_udata();
				nCurrentSocket = a_bIsRaft?pNodeTools->raftSocket:pNodeTools->dataSocket;
				if((nCurrentSocket>0)&&(nCurrentSocket!=nSocketToIgnore)){
					FD_SET(nCurrentSocket,&rFds);
					FD_SET(nCurrentSocket,&eFds);
					if(nCurrentSocket>nMax){nMax= nCurrentSocket;}
				}  // if(pNodeTools->socket>0){
			}  // if(pNode!=m_thisNode){
			pNode = pNode->next;
		}// while(pNode){
		m_mutexShrd.unlock_shared();

		nSelectReturn = ::select(++nMax, &rFds, NULL, &eFds, NULL);
		if (!m_nWork) { break; }
		if (nSelectReturn < 0) { 
            ERR_REP("Select returned negative value");
            //Sleep(2000);  // most probably last client was problematic
			nSocketToIgnore = nLastSocket;
		} // will never happen
        else {nSocketToIgnore=-1;}
        nSocketsFound = 0;
        m_mutexShrd.lock_shared();
        if(FD_ISSET(nSocketForInfo,&rFds)||FD_ISSET(nSocketForInfo,&eFds)){
            ++nSocketsFound;
            closesocket(nSocketForInfo);  //  is it necessary?
            nSocketForInfo = -1;
        }

        pNode = m_firstNode;
        while (pNode && (nSocketsFound<nSelectReturn)) {
            if (pNode != m_thisNode) {
                pNodeTools = (NodeTools*)pNode->get_udata();
                nCurrentSocket = a_bIsRaft?pNodeTools->raftSocket:pNodeTools->dataSocket;
                if (FD_ISSET(nCurrentSocket, &rFds)) {
                    // call receive ...
                    nLastSocket = nCurrentSocket;
                    (this->*a_fpRcvFnc)(pNode);
                    ++nSocketsFound;
                }  // if(pNodeTools->socket>0){
                if (FD_ISSET(nCurrentSocket, &eFds)) {
                    pNode->setProblematic(1);
                    ++nSocketsFound;
                }  // if(pNodeTools->socket>0){
            }  // if(pNode!=m_thisNode){
            pNode = pNode->next;
        }// while(pNode){
        m_mutexShrd.unlock_shared();

	} // while (m_nWork){

	if(nSocketForInfo>0){closesocket(nSocketForInfo);nSocketForInfo=-1;}
}


void raft::tcp::Server::ThreadFunctionRcvData()
{
#ifndef _WIN32
    m_rcvDataThread = pthread_self();
#endif
    FunctionForMultiRcv(&m_infoSocketForRcvData2,&raft::tcp::Server::ReceiveFromDataSocket,false);
}


void raft::tcp::Server::ThreadFunctionRcvRaftInfo()
{
#ifndef _WIN32
    m_rcvRaftThread = pthread_self();
#endif
    FunctionForMultiRcv(&m_infoSocketForRcvRaft2,&raft::tcp::Server::ReceiveFromRaftSocket,true);
}


void raft::tcp::Server::ThreadFunctionWorker()
{
	common::SocketTCP aClientSock;
	SWorkerData dataFromProducer;
	int nSndRcv, nError, isEndianDiffer;
	int16_t	snEndian;
	char vcHostName[MAX_HOSTNAME_LENGTH];
	char cRequest;
	bool bKeepSocket, bWriteFinalEcho;

	while(m_nWork){
		m_semaWorker.wait();

		while(m_fifoWorker.Extract(&dataFromProducer)&&m_nWork){
			bWriteFinalEcho = true;
			bKeepSocket = false;
			snEndian = 1;
			nError = -1;
			isEndianDiffer = 0;
			LOG_REP(
				"conntion from %s(%s)",
				common::socketN::GetHostName(&dataFromProducer.remAddress,vcHostName,MAX_HOSTNAME_LENGTH),
				common::socketN::GetIPAddress(&dataFromProducer.remAddress));
			aClientSock.SetNewSocketDescriptor(dataFromProducer.sockDescriptor);
			aClientSock.setTimeout(SOCK_TIMEOUT_MS);

			nSndRcv = aClientSock.writeC(&snEndian, 2);																// 2. Send endian				
			if (nSndRcv != 2) {
				ERR_REP("Connection broken");
				goto handlingEndPoint;
			}

			nSndRcv = aClientSock.readC(&snEndian, 2);																// 3. rcv endian
			if (nSndRcv != 2) {
				ERR_REP("Unable to read request");
				goto handlingEndPoint;
			}
			if (snEndian != 1) { isEndianDiffer = 1; }

			nSndRcv = aClientSock.readC(&cRequest, 1);																// 4. rcv request
			if (nSndRcv != 1) {
				ERR_REP("Unable to read request");
				goto handlingEndPoint;
			}

			switch (cRequest)
			{
			case raft::connect::anyNode::newNode:
				DEBUG_APPLICATION(1, "raft::connect::anyNode::newNode");
				connect_allNodes_newNode(aClientSock);
				break;
			case raft::connect::leader::newNode:
				bKeepSocket = connect_leader_newNode(aClientSock,&dataFromProducer.remAddress,isEndianDiffer);
				if (bKeepSocket) { bWriteFinalEcho=false;}
				DEBUG_APPLICATION(1, "raft::connect::leader::newNode  (bKeepSocket=%s)",bKeepSocket?"true":"false");
				break;
			case raft::connect::anyNode::raftBridge:
				bKeepSocket = connect_anyNode_bridgeToNodeRaft(aClientSock, isEndianDiffer);
				DEBUG_APPLICATION(1,"raft::connect::anyNode::raftBridge (bKeepSocket=%s)",bKeepSocket?"true":"false");
				break;
			case raft::connect::anyNode::dataBridge:
				bKeepSocket = connect_anyNode_bridgeToNodeData(aClientSock, isEndianDiffer);
				DEBUG_APPLICATION(1,"raft::connect::anyNode::dataBridge (bKeepSocket=%s)",bKeepSocket?"true":"false");
				break;
			default:
				ERR_REP("wrong request");
				DEBUG_APPLICATION(0," ");
				goto handlingEndPoint;
			}

			nError = 0;
handlingEndPoint:
			if (nError) { aClientSock.writeC("e", 1); }
			else if(bWriteFinalEcho){ aClientSock.writeC("o", 1); }
			if (bKeepSocket) { aClientSock.ResetSocketWithoutClose(); }
			else { aClientSock.closeC(); }
		}
	}
}


void raft::tcp::Server::ThreadFunctionAddRemoveNode()
{
	RaftNode2* pNextNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey* pKeyForDelete;
	SAddRemData aData;
	int nSndRcv;
	char cRequest;

	while(m_nWork){
		m_semaAddRemove.wait();

		while(m_fifoAddDel.Extract(&aData) && m_nWork){
			if (is_leader()) {
				if (!aData.bAdd){
					
					cRequest = aData.pNode->is_leader() ? raft::receive::leader::oldLeaderDied : raft::receive::leader::removeNode;
					m_mutexShrd.lock_shared();

					pNextNode = m_firstNode;
					while(pNextNode){
						if((pNextNode!=m_thisNode)&&(pNextNode!=aData.pNode)){
							pNodeTools = (NodeTools*)pNextNode->get_udata();
							s_mutexForRaftSend.lock();
							nSndRcv=pNodeTools->raftSocket.writeC(&cRequest,1);
							if(nSndRcv!=1){ pNextNode->setProblematic(1);goto gotoNextNode;}
							nSndRcv = pNodeTools->raftSocket.writeC(aData.pNode->key2(), sizeof(NodeIdentifierKey));
							s_mutexForRaftSend.unlock();
							if (nSndRcv != sizeof(NodeIdentifierKey)) { pNextNode->setProblematic(1); goto gotoNextNode; }
						}
						gotoNextNode:
						pNextNode = pNextNode->next;
					}

					m_mutexShrd.unlock_shared();

				}  // if (!aData.bAdd){
			} // if (is_leader()) {
			m_mutexShrd.lock();
			if(aData.bAdd){
				DEBUG_APPLICATION(1,"Node (add): %s:%d, numOfNodes=%d", aData.nodeKey.ip4Address,(int)aData.nodeKey.port,m_nNodesCount+1);
				this->AddNode(aData.pNode,&aData.nodeKey,sizeof(NodeIdentifierKey));
			}
			else {
				pKeyForDelete = (NodeIdentifierKey*)aData.pNode->key2();
				DEBUG_APPLICATION(1,"Node (del): %s:%d, numOfNodes=%d", pKeyForDelete->ip4Address,(int)pKeyForDelete->port, m_nNodesCount-1);
				this->RemoveNode(aData.pNode);
			}
			m_mutexShrd.unlock();
            InterruptRaftRcv();
            InterruptDataRcv();
		}
	}
}


void raft::tcp::Server::CheckAllPossibleSeeds(const std::vector<NodeIdentifierKey>& a_vectPossibleNodes)
{
	const char* cpcPosibleSeedIp;
	RaftNode2* pNode;
	NodeIdentifierKey* pNodeKey;
	char vcOwnHostName[MAX_IP4_LEN];
	std::vector<NodeIdentifierKey>  vectLeaders;
	const int cnSize((int)a_vectPossibleNodes.size());
	int i;
	bool bLeaderFound(false);
	
    DEBUG_HANGING();
	common::socketN::GetOwnIp4Address(vcOwnHostName,MAX_IP4_LEN);
    DEBUG_HANGING();
 
	try {

		for(i=0;i<cnSize;++i){
			if(  (strncmp(vcOwnHostName,a_vectPossibleNodes[i].ip4Address,MAX_IP4_LEN)==0)&&(m_nPortOwn==a_vectPossibleNodes[i].port)  ){continue;}
			else {
				cpcPosibleSeedIp = common::socketN::GetIp4AddressFromHostName(a_vectPossibleNodes[i].ip4Address);
				if( cpcPosibleSeedIp && (strncmp(vcOwnHostName,cpcPosibleSeedIp, MAX_IP4_LEN)== 0) && (m_nPortOwn == a_vectPossibleNodes[i].port) ){continue;}
			}
            DEBUG_HANGING();
			TryFindNewLeaderThrdSafe(a_vectPossibleNodes[i], &vectLeaders);
            DEBUG_HANGING();
		}

		bLeaderFound=AskInfoFromLeadersThrdSafe(vectLeaders);
		
		if (bLeaderFound) {
			d_state.set(RAFT_STATE_FOLLOWER);
		}
		else{
			this->d_state.set(RAFT_STATE_LEADER);
		}

		AddOwnNode();

		DEBUG_APPLICATION(1, "NumberOfNodes=%d", m_nNodesCount);
		pNode = m_firstNode;
		while(pNode){
			pNodeKey = (NodeIdentifierKey*)pNode->key2();
			DEBUG_APPLICATION_NO_NEW_LINE(1,"%s:%d ",pNodeKey->ip4Address,(int)pNodeKey->port);
			if(pNode==m_thisNode){ DEBUG_APPLICATION_NO_ADD_INFO(1,"(this) ");}
			if(pNode==m_pLeaderNode){ DEBUG_APPLICATION_NO_ADD_INFO(1,"(leader) ");}
			DEBUG_APPLICATION_NEW_LINE(1);
			pNode = pNode->next;
		}
	}
	catch (...) {
	}
}


void raft::tcp::Server::ThreadFunctionPeriodic()
{
	try {

		while (m_nWork) {
			m_mutexShrd.lock_shared();
			this->periodic(m_nPeriodForPeriodic);
			m_mutexShrd.unlock_shared();
			Sleep(m_nPeriodForPeriodic);
		}
	}
	catch (...) {
	}
}


void raft::tcp::Server::AddOwnNode()
{
	//typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
	NodeTools* pTools = new NodeTools;
	RaftNode2* pNode;
	NodeIdentifierKey aOwnHost;

	if(pTools){pTools->isEndianDiffer=0;}
	common::socketN::GetOwnIp4Address(aOwnHost.ip4Address, MAX_IP4_LEN);
	aOwnHost.port = m_nPortOwn;
	pNode = new RaftNode2(pTools);
	if (!pNode) { HANDLE_MEM_DEF(" "); }
	m_thisNode = pNode;
	if(is_leader()){m_pLeaderNode=pNode;pNode->makeLeader(1);}
	this->AddNode(pNode, &aOwnHost, sizeof(NodeIdentifierKey));
}


// const NodeIdentifierKey& nodeInfo, std::vector<NodeIdentifierKey>* pExisting
void raft::tcp::Server::TryFindNewLeaderThrdSafe(const NodeIdentifierKey& a_nodeInfo, std::vector<NodeIdentifierKey>* a_pExisting)
{
	NodeIdentifierKey leaderNodeKey;
	common::SocketTCP aSocket;
	int16_t  snEndian;
    int i,nSndRcv, isEndianDiffer=0, nLeadersCount((int)a_pExisting->size());
	char cRequest;
	bool bFound(false);

    DEBUG_HANGING();

	if(aSocket.connectC(a_nodeInfo.ip4Address,a_nodeInfo.port)){return;}							// 1. connect
	aSocket.setTimeout(SOCK_TIMEOUT_MS);

    DEBUG_HANGING();
	
	nSndRcv= aSocket.readC(&snEndian,2);															// 2. rcv endian
	if(nSndRcv!=2){return;}
	if (snEndian != 1) { isEndianDiffer = 1; }
	
	snEndian = 1;
	nSndRcv= aSocket.writeC(&snEndian,2);															// 3. send endian
	if(nSndRcv!=2){return;}
	
	cRequest = raft::connect::anyNode::newNode;
	nSndRcv = aSocket.writeC(&cRequest,1);															// 4. Send request
	if(nSndRcv!=1){return;}
	//pNodeInfo = (NodeIdentifierKey*)malloc(sizeof(NodeIdentifierKey));
	//if (!pNodeInfo) { HANDLE_MEM_DEF(" "); return false; }
	nSndRcv= aSocket.readC(&leaderNodeKey,sizeof(NodeIdentifierKey));								// 6. get responce  (5. send add info is not used)
	if(nSndRcv!=sizeof(NodeIdentifierKey)){return;}
	/*nSndRcv=*/aSocket.readC(&cRequest, 1);														// 8. rcv finall echo  (7. rcv add info skipped)
	// cRequest=='o'->ok, cRequest=='e'->error
	//aSocket.closeC();  will be closed on return

	if(isEndianDiffer){SWAP4BYTES(leaderNodeKey.port);}

	for(i=0;i<nLeadersCount;++i){
		if(leaderNodeKey==(*(a_pExisting))[i]){bFound=true;break;}
	}

	if(!bFound){a_pExisting->push_back(leaderNodeKey);}

}



bool raft::tcp::Server::AskInfoFromLeadersThrdSafe(const std::vector<NodeIdentifierKey>& a_vectLeaders)
{
	// struct NodeIdentifierNet { int32_t port, isLeader; char hostName[MAX_HOSTNAME_LENGTH];};
	NodeTools* pTools;
	RaftNode2* pNewNode;
	NodeIdentifierKey leaderNodeKey2,thisNodeKey;
	NodeIdentifierKey *pNodeInfo2 = NULL;
	common::SocketTCP aSocket;
	int16_t  snEndian;
	int32_t  numberOfNodes;
	std::vector<raft_node_configuration_t> vectInput;
	int i,nSndRcv, nBytesToReceive,isEndianDiffer=0, nLeadersCount((int)a_vectLeaders.size()), nBytesOfLeaders(nLeadersCount*sizeof(NodeIdentifierKey));
	char cRequest;
	bool bFound(false);

	common::socketN::GetOwnIp4Address(thisNodeKey.ip4Address, MAX_IP4_LEN);
	thisNodeKey.port = m_nPortOwn;

	cRequest = raft::connect::leader::newNode;

	for(i=0; i<nLeadersCount;++i){
		isEndianDiffer = 0;
		if (aSocket.connectC(a_vectLeaders[i].ip4Address,a_vectLeaders[i].port)){goto nextLeaderPoint;}			// 1. connect
		aSocket.setTimeout(SOCK_TIMEOUT_MS);
		
		nSndRcv = aSocket.readC(&snEndian, 2);																	// 2. rcv endian
		if (nSndRcv != 2) { goto nextLeaderPoint;}
		if (snEndian != 1) {isEndianDiffer = 1; }
		
		snEndian = 1;
		nSndRcv = aSocket.writeC(&snEndian, 2);																	// 3. send endian
		if (nSndRcv != 2) { goto nextLeaderPoint; }

		nSndRcv = aSocket.writeC(&cRequest, 1);																	// 4. Send request
		if (nSndRcv != 1) { goto nextLeaderPoint; }

		nSndRcv = aSocket.writeC(&m_nPortOwn, 4);																// 5. send additional data
		if (nSndRcv != 4) { goto nextLeaderPoint;}

		nSndRcv = aSocket.readC(&numberOfNodes, 4);																// 6. rcv answer  (5. send add info skipped)
		if (nSndRcv != 4) { goto nextLeaderPoint; }
		if (isEndianDiffer) { SWAP4BYTES(numberOfNodes); }
		if (numberOfNodes < 1) {goto nextLeaderPoint;}

		nBytesToReceive = numberOfNodes * sizeof(NodeIdentifierKey);
		pNodeInfo2 = (NodeIdentifierKey*)malloc(nBytesToReceive);
		if (!pNodeInfo2) { HANDLE_MEM_DEF(" "); }

		nSndRcv = aSocket.readC(pNodeInfo2, nBytesToReceive);													// 7. get additional info
		if (nSndRcv != nBytesToReceive) { goto nextLeaderPoint; }

		// 
		nSndRcv = aSocket.writeC(&nLeadersCount, 4);															// 8. send extra additional info
		if (nSndRcv != 4) { goto nextLeaderPoint; }

		nSndRcv = aSocket.writeC(a_vectLeaders.data(), nBytesOfLeaders);											// 9. send extra additional info
		if (nSndRcv != nBytesOfLeaders) { goto nextLeaderPoint; }

		nSndRcv=aSocket.readC(&cRequest, 1);
		if ((nSndRcv!= 1)||(cRequest!='o')) { goto nextLeaderPoint; }

		leaderNodeKey2 = a_vectLeaders[i];
		bFound = true;
		break;


	nextLeaderPoint:
		aSocket.closeC();
	}

	if (!bFound) { free(pNodeInfo2); return false; }
	
	vectInput.resize(numberOfNodes);
	common::socketN::GetOwnIp4Address(thisNodeKey.ip4Address, MAX_IP4_LEN);
	thisNodeKey.port = m_nPortOwn;

	for(i=0;i<numberOfNodes;++i){
		pTools = new NodeTools;
		if(!pTools){HANDLE_MEM_DEF(" ");}
		pNewNode = new RaftNode2(pTools);
		if (!pNewNode) { HANDLE_MEM_DEF(" "); }
		this->AddNode(pNewNode, &pNodeInfo2[i], sizeof(NodeIdentifierKey));
		if(leaderNodeKey2== pNodeInfo2[i]){
			pNewNode->makeLeader(1);
			pTools->raftSocket.SetNewSocketDescriptor(aSocket);
			pTools->isEndianDiffer = isEndianDiffer;
			m_pLeaderNode=pNewNode;
		}
		else {
			// Finally let's connect to all nodes and ask permanent raft socket
			pTools->raftSocket.connectC(pNodeInfo2[i].ip4Address, pNodeInfo2[i].port);
			pTools->raftSocket.readC(&snEndian, 2);
			if (snEndian != 1) { pTools->isEndianDiffer = 1; }
			snEndian = 1;
			pTools->raftSocket.writeC(&snEndian, 2);
			cRequest = raft::connect::anyNode::raftBridge;
			pTools->raftSocket.writeC(&cRequest, 1);
			pTools->raftSocket.writeC(&thisNodeKey, sizeof(NodeIdentifierKey));
			pTools->raftSocket.readC(&cRequest, 1);
		}
		
		// Finally let's connect to all nodes and ask permanent data socket
		pTools->dataSocket.connectC(pNodeInfo2[i].ip4Address, pNodeInfo2[i].port);
		pTools->dataSocket.readC(&snEndian, 2);
		if (snEndian != 1) { pTools->isEndianDiffer = 1; }
		snEndian = 1;
		pTools->dataSocket.writeC(&snEndian, 2);
		cRequest = raft::connect::anyNode::dataBridge;
		pTools->dataSocket.writeC(&cRequest, 1);
		pTools->dataSocket.writeC(&thisNodeKey, sizeof(NodeIdentifierKey));
		pTools->dataSocket.readC(&cRequest, 1);
	}


	free(pNodeInfo2);
	if (bFound) {
		//pTools = (NodeTools*)m_pLeaderNode->get_udata();
		//pTools->raftSocket.SetNewSocketDescriptor(aSocket);
		aSocket.ResetSocketWithoutClose();
	}
	//else{ aSocket.closeC(); } // this will be done in the destructor
	return bFound;
}


void raft::tcp::Server::InterruptRaftRcv()
{
#ifdef _WIN32
    if(m_infoSocketForRcvRaft2>0){closesocket(m_infoSocketForRcvRaft2);}
#else
    pthread_kill(m_rcvRaftThread,SIGPIPE);
#endif
}


void raft::tcp::Server::InterruptDataRcv()
{
#ifdef _WIN32
    if(m_infoSocketForRcvData2>0){closesocket(m_infoSocketForRcvData2);}
#else
    pthread_kill(m_rcvDataThread,SIGPIPE);
#endif
}


void raft::tcp::Server::become_leader()
{
	// First send to all nodes an info, that this node is leader
	RaftServer::become_leader();
	m_thisNode->makeLeader(1);
	if(m_pLeaderNode){
		m_fifoAddDel.AddElement(m_pLeaderNode);
		m_semaAddRemove.post();
		m_pLeaderNode = m_thisNode;
	}
	DEBUG_APPLICATION(1,"this node is leader now");
}


void raft::tcp::Server::become_candidate()
{
	if(m_nNodesCount<3){ // only died leader remains no need to make election
		become_leader();
	}
	else{
		RaftServer::become_candidate();
	}

}


/*//////////////////////////////////////////////////////////////////////////////*/

int raft::tcp::Server::SendClbkFunction(void *a_cb_ctx, void *udata, RaftNode2* a_node, int a_msg_type, const unsigned char *send_data,int d_len)
{
	//RaftServerTcp* pServer = (RaftServerTcp*)a_cb_ctx;
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
	Server* pServer = (Server*)a_cb_ctx;
	NodeTools* pTools = (NodeTools*)a_node->get_udata();
	int nSndRcv;
	uint32_t unPingCount=0;
	char cRequest=raft::receive::anyNode::clbkCmd;
	bool bProblematic(true);
	bool bMutexLocked(false);

	switch (a_msg_type)
	{
	case RAFT_MSG_APPENDENTRIES:
		if(a_node->isProblematic()){a_node->makePing();}  // make extra ping
		unPingCount = (int)a_node->makePing();
		if((unPingCount>MAX_UNANSWERED_PINGS)&& pServer->is_leader()){
			DEBUG_APPLICATION(1,"pingCount=%d", unPingCount);
			pServer->m_fifoAddDel.AddElement(a_node);
			pServer->m_semaAddRemove.post();
		}
		break;
	default:
		break;
	}

	if(unPingCount>MAX_NUMBER_OF_PINGS){
		DEBUG_APPLICATION(1, "pingCount=%d", unPingCount);
		goto returnPoint;
	}

	s_mutexForRaftSend.lock(); bMutexLocked = true;
	nSndRcv=pTools->raftSocket.writeC(&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}
	nSndRcv=pTools->raftSocket.writeC(&a_msg_type,4);
	if(nSndRcv!=4){goto returnPoint;}
	nSndRcv=pTools->raftSocket.writeC(send_data, d_len);
	s_mutexForRaftSend.unlock(); bMutexLocked = false;
	if(nSndRcv!= d_len){goto returnPoint;}
	
	bProblematic = false;
returnPoint:
	if(bMutexLocked){ s_mutexForRaftSend.unlock(); }
    if(bProblematic){a_node->setProblematic(1);}
	return 0;
}


void raft::tcp::Server::LogClbkFunction(void *cb_ctx, void *src, const char *buf, ...)
{
}


int raft::tcp::Server::ApplyLogClbkFunction(void *cb_ctx, void *udata, const unsigned char *d_data, int d_len)
{
	return 0;
}


/*******************************************************************************************************************/

raft::tcp::SAddRemData::SAddRemData()
	:
	pNode(NULL),
	bAdd(true)
{
}

raft::tcp::SAddRemData::SAddRemData(RaftNode2* a_node)
	:
	pNode(a_node),
	bAdd(false)
{
}



/********************************************************************************************************************/
static int CreateEmptySocket()
{
	int nSocket = (int)::socket(AF_INET, SOCK_STREAM, 0);
	return nSocket;
}


#ifdef HANDLE_SIG_ACTIONS
struct ServersList{raft::tcp::Server *server;ServersList *prev, *next;};
static struct ServersList* s_pFirst = NULL;
static pthread_rwlock_t s_pRWlockForServers = PTHREAD_RWLOCK_INITIALIZER;

static void AddNewRaftServer(raft::tcp::Server* a_pServer)
{
    ServersList* pServerList = (ServersList*)calloc(1,sizeof(ServersList));
    if(!pServerList){HANDLE_MEM_DEF(" ");}
    pServerList->server = a_pServer;
    a_pServer->m_pReserved1 = pServerList;
    pthread_rwlock_wrlock(&s_pRWlockForServers);
    DEBUG_APPLICATION(1,"wr_lock");
    if(s_pFirst){s_pFirst->prev =pServerList;}
    pServerList->next = s_pFirst;
    s_pFirst = pServerList;
    pthread_rwlock_unlock(&s_pRWlockForServers);
    DEBUG_APPLICATION(1,"unlock");
}


static void RemoveRaftServer(raft::tcp::Server* a_pServer)
{
    ServersList* pListItem = (ServersList*)a_pServer->m_pReserved1;
    pthread_rwlock_wrlock(&s_pRWlockForServers);
    DEBUG_APPLICATION(1,"wr_lock");
    if(pListItem->next){pListItem->next->prev=pListItem->prev;}
    if(pListItem->prev){pListItem->prev->next=pListItem->next;}
    if(pListItem==s_pFirst){s_pFirst=pListItem->next;}
    pthread_rwlock_unlock(&s_pRWlockForServers);
    DEBUG_APPLICATION(1,"unlock");
    free(pListItem);
}


static void SigActionFunction (int a_nSigNum, siginfo_t * , void *)
{
    ServersList* pServer;
    sigval aNewSig;
    pthread_t interruptThread=pthread_self();

    DEBUG_APPLICATION(0,"Interrupt (No:%d)",a_nSigNum);

    pthread_rwlock_rdlock(&s_pRWlockForServers);
    DEBUG_APPLICATION(1,"rd_lock");

    pServer = s_pFirst;
    while(pServer){
        switch(a_nSigNum)
        {
        case SIGINT:
            if(interruptThread != pServer->server->m_starterThread){
                aNewSig.sival_ptr = NULL;
                pthread_sigqueue(pServer->server->m_starterThread,SIGINT,aNewSig);
            }
            else{
                pServer->server->StopServer();
            }
            break;
        case SIGPIPE:
            break;
        default:
            break;
        }
        pServer = pServer->next;
    }

    pthread_rwlock_unlock(&s_pRWlockForServers);
    DEBUG_APPLICATION(1,"unlock");
}
#endif  // #ifdef HANDLE_SIG_ACTIONS
