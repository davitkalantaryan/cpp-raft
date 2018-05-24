
#ifndef HEADER_SIZE_needed
#define HEADER_SIZE_needed
#endif

#include "raft_tcp_server.hpp"
#include <string>
#include <stdint.h>
#include <signal.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#ifdef MAKE_LONG_WAIT_DEBUG

#define MIN_REP_RATE_MS					5
#define DEF_REP_RATE_MS					5000
#define	TIMEOUTS_RATIO_MIN				5
#define REPORT_ON_FAULT(_faultyNode)
#define MAX_NUMBER_OF_PINGS				2
#define MAX_UNANSWERED_PINGS			10
#define MAX_ITER_OK_COUNT				6

#else

#define MIN_REP_RATE_MS					5
#define DEF_REP_RATE_MS					2000
#define	TIMEOUTS_RATIO_MIN				4
#define MAX_NUMBER_OF_PINGS				2
#define MAX_UNANSWERED_PINGS			5
#define MAX_ITER_OK_COUNT				5

#endif

#ifdef _WIN32
#else
#define closesocket	close
#endif

struct ServersList { raft::tcp::Server *server; ServersList *prev, *next; };
static struct ServersList* s_pFirst = NULL;
//static pthread_rwlock_t s_pRWlockForServers = PTHREAD_RWLOCK_INITIALIZER;
static newSharedMutex s_pRWlockForServers;

//static void SigActionFunction (int, siginfo_t *, void *); last 2 arguments are not used
static void AddNewRaftServer(raft::tcp::Server* a_pServer);
static void RemoveRaftServer(raft::tcp::Server* a_pServer);

static std::mutex s_mutexForRaftSend;

namespace raft{namespace tcp{
int g_nLogLevel = 0;
int g_nApplicationRun = 0;
}}

typedef struct NodeTools{ 
	common::SocketTCP dataSocket, raftSocket2; 
	uint32_t isEndianDiffer : 1,okCount : 3;
	/*-----------------------------------------*/
	NodeTools(){isEndianDiffer=okCount=0;} 
}NodeTools;

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

	m_nWork = 0;
    m_infoSocketForRcvRaft2 = -1;
    m_infoSocketForRcvData2 = -1;

	aClbks.send     = &raft::tcp::Server::SendClbkFunction;
	aClbks.log      = &raft::tcp::Server::LogClbkFunction;
	aClbks.applylog = &raft::tcp::Server::ApplyLogClbkFunction;
	this->set_callbacks(&aClbks, this);

	AddNewRaftServer(this);
}


raft::tcp::Server::~Server()
{
	RemoveRaftServer(this);
	this->StopServer();
}


void raft::tcp::Server::Initialize()
{
	struct sigaction newAction;

	newAction.sa_handler = &Server::SigHandlerStatic;
#if !defined(_WIN32) || defined(_WLAC_USED)
	newAction.sa_flags = 0;
	sigemptyset(&newAction.sa_mask);
	newAction.sa_restorer = NULL;

	sigaction(SIGPIPE, &newAction, NULL);
#else
#endif

	sigaction(SIGABRT, &newAction, NULL_ACTION);
	sigaction(SIGFPE, &newAction, NULL_ACTION);
	sigaction(SIGILL, &newAction, NULL_ACTION);
	sigaction(SIGINT, &newAction, NULL_ACTION);
	sigaction(SIGSEGV, &newAction, NULL_ACTION);
	sigaction(SIGTERM, &newAction, NULL_ACTION);

	common::socketN::Initialize();

	g_nApplicationRun = 1;
}


void raft::tcp::Server::Cleanup()
{
	g_nApplicationRun = 0;
	common::socketN::Cleanup();
}


void raft::tcp::Server::SignalHandler(int )
{
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

#ifndef _WIN32
	m_starterThread = pthread_self();
#endif  // #ifdef HANDLE_SIG_ACTIONS

	if (m_nWork) {return -1;}

	m_nPortOwn = a_nRaftPort;
	if(m_nPeriodForPeriodic<MIN_REP_RATE_MS){ m_nPeriodForPeriodic = DEF_REP_RATE_MS;}
	if(this->request_timeout < m_nPeriodForPeriodic) { this->request_timeout = m_nPeriodForPeriodic;}
	if(this->election_timeout < (TIMEOUTS_RATIO_MIN*this->request_timeout)) { this->election_timeout =(TIMEOUTS_RATIO_MIN*this->request_timeout);}
	this->timeout_elapsed = 0;

	CheckAllPossibleSeeds(a_vectPossibleNodes);

	m_nWork = 1;

#if !defined(_WIN32) || defined(_WLAC_USED)
	// make a such that only main thread is interrupted 
	// during SIGINT
	struct sigaction newAction, oldAction;
	newAction.sa_handler = SIG_IGN;
	newAction.sa_flags = 0;
	sigemptyset(&newAction.sa_mask);
	newAction.sa_restorer = NULL;

	sigaction(SIGINT, &newAction, &oldAction);
#endif

	m_threadTcpListen = std::thread(&Server::ThreadFunctionListen,this);
	m_threadPeriodic = std::thread(&Server::ThreadFunctionPeriodic, this);
	m_threadRcvRaftInfo = std::thread(&Server::ThreadFunctionRcvRaftInfo, this);
	m_threadRcvData = std::thread(&Server::ThreadFunctionRcvData, this);
	m_threadAddRemoveNode = std::thread(&Server::ThreadFunctionAddRemoveNode, this);
	for(int i(0);i<a_nWorkersCount;++i){
		pWorker = new std::thread(&Server::ThreadFunctionWorker, this);
		m_vectThreadsWorkers.push_back(pWorker);
	}

#if !defined(_WIN32) || defined(_WLAC_USED)
	sigaction(SIGINT, &oldAction, NULL_ACTION);
#endif

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


void raft::tcp::Server::connect_toAnyNode_newNode(common::SocketTCP& a_clientSock)
{
	NodeIdentifierKey* pLeaderKey = (NodeIdentifierKey*)m_pLeaderNode->key2();
	a_clientSock.writeC(pLeaderKey, sizeof(NodeIdentifierKey));
}


void raft::tcp::Server::connect_toLeader_newNode(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)
{
	// struct NodeIdentifierKey { char ip4Address[MAX_IP4_LEN]; int32_t port;};
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
    NodeIdentifierKey	*pAllNodesInfo=NULL;
	NodeTools*			pNewNodeTool = NULL;
	RaftNode2*			pNode;
	SAddRemData			addNodeData;
    int nNodesCount,nTotalSize;
	int nSndRcv, nEndianDiffer(0);
	uint16_t unRemEndian;
	bool bOk(false);

	addNodeData.nodeKey.set_ip4Address(common::socketN::GetIPAddress(a_remoteAddr));

	nSndRcv= a_clientSock.readC(&unRemEndian,2);
	if(nSndRcv!= 2){ goto returnPoint;}
	if(unRemEndian!=1){ nEndianDiffer=1;}

	nSndRcv= a_clientSock.readC(&addNodeData.nodeKey.port,4);
	if(nSndRcv!= 4){ goto returnPoint;}
	if(nEndianDiffer){ SWAP4BYTES(addNodeData.nodeKey.port);}

	pNewNodeTool = new NodeTools;
	HANDLE_MEM_DEF2(pNewNodeTool," ");
	pNewNodeTool->isEndianDiffer = nEndianDiffer;
	
	m_mutexShrd.lock_shared();

	// first let's check if the node does not exist
	if(m_hashNodes.FindEntry(&addNodeData.nodeKey,sizeof(NodeIdentifierKey),&pNode)){
		m_mutexShrd.unlock_shared();
		// send 0, as a sign, that node exists
		nTotalSize = raft::response::error::nodeExist;
		a_clientSock.writeC(&nTotalSize, 4);
		goto returnPoint;
	}

	nNodesCount = m_nNodesCount;
	pAllNodesInfo=CollectAllNodesDataNotThrSafe(&nTotalSize,NULL);

	m_mutexShrd.unlock_shared();


	// add this new node here
	pNewNodeTool->raftSocket2.SetNewSocketDescriptor(a_clientSock);
	addNodeData.pNode = new RaftNode2(pNewNodeTool);
	HANDLE_MEM_DEF2(addNodeData.pNode," ");
	addNodeData.action = raft::leaderInternal::newNode;

	// start process adding and informing other nodes
	// Informing other nodes will be done in the addRemove thread
	m_fifoAddDel.AddElement(addNodeData);
	m_semaAddRemove.post();

	nSndRcv= a_clientSock.writeC(&nNodesCount,4);
	if(nSndRcv!= 4){ goto returnPoint;}

	nSndRcv= a_clientSock.writeC(pAllNodesInfo,nTotalSize);
	if(nSndRcv!= nTotalSize){ goto returnPoint;}
	free(pAllNodesInfo); pAllNodesInfo = NULL;

	bOk = true;
returnPoint:
	if (bOk) {a_clientSock.ResetSocketWithoutClose();}
	else { delete pNewNodeTool; }
	free(pAllNodesInfo);
}


void raft::tcp::Server::connect_toAnyNode_bridgeToNodeRaft(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)
{
	// this socket should remain 
	RaftNode2* pNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey aRemHost;
	int nEndianDiffer(0);
	int nSndRcv;
	uint16_t unRemEndian;

	aRemHost.set_ip4Address(common::socketN::GetIPAddress(a_remoteAddr));	// let us specify host IP

	nSndRcv = a_clientSock.readC(&unRemEndian,2);							// endian
	if(nSndRcv!=2){return;}
	if(unRemEndian!=1){nEndianDiffer=1;}

	nSndRcv = a_clientSock.readC(&aRemHost.port,4);							// port
	if(nSndRcv!=4){return;}
	if(nEndianDiffer){SWAP4BYTES(aRemHost.port);}

	m_mutexShrd.lock_shared();
	if(!m_hashNodes.FindEntry(&aRemHost,sizeof(NodeIdentifierKey),&pNode)){
		m_mutexShrd.unlock_shared();
		return;
	}
	
	pNodeTools = (NodeTools*)pNode->get_udata();
	pNodeTools->isEndianDiffer = nEndianDiffer;
	pNodeTools->raftSocket2.SetNewSocketDescriptor(a_clientSock);
	m_mutexShrd.unlock_shared();

	InterruptRaftRcv();

	a_clientSock.writeC(&g_ccResponceOk, 1);
	a_clientSock.ResetSocketWithoutClose();
}


raft::tcp::NodeIdentifierKey* raft::tcp::Server::CollectAllNodesDataNotThrSafe(int* a_pnTotalSize, int* a_pnLeaderIndex)
{
	RaftNode2* pNode;
	NodeIdentifierKey *pAllNodesInfo, *pExistingNodeKey;
	int i(0);
	
	*a_pnTotalSize = m_nNodesCount * sizeof(NodeIdentifierKey);
	pAllNodesInfo = (NodeIdentifierKey*)malloc(*a_pnTotalSize);
	HANDLE_MEM_DEF2(pAllNodesInfo, " ");

	// collect info
	pNode = m_firstNode;
	while (pNode) {
		pExistingNodeKey = (NodeIdentifierKey*)pNode->key2();
		pAllNodesInfo[i].set_ip4Address(pExistingNodeKey->ip4Address);
		pAllNodesInfo[i].port = pExistingNodeKey->port;
		if(a_pnLeaderIndex && pNode->is_leader()){*a_pnLeaderIndex = i;}
		pNode = pNode->next;
		++i;
	}

	return pAllNodesInfo;
}


void raft::tcp::Server::connect_fromClient_allNodesInfo(common::SocketTCP& a_clientSock)
{
	struct { int nodesCount, leaderIndex; }nl;
	NodeIdentifierKey *pAllNodesInfo;
	int nSndRcv,nTotalSize;

	m_mutexShrd.lock_shared();
	nl.nodesCount = m_nNodesCount;
	pAllNodesInfo = CollectAllNodesDataNotThrSafe(&nTotalSize,&nl.leaderIndex);
	m_mutexShrd.unlock_shared();

	if(!pAllNodesInfo){return;}

	nSndRcv=a_clientSock.writeC(&nl,8);
	if(nSndRcv!=8){free(pAllNodesInfo);return;}

	a_clientSock.writeC(pAllNodesInfo,nTotalSize);
	free(pAllNodesInfo);
}


void raft::tcp::Server::connect_toAnyNode_otherLeaderFound(common::SocketTCP& a_clientSock)
{
	NodeIdentifierKey newLeaderKey;
	int nSndRcv;
	uint32_t isEndianDiffer;
	uint16_t unRemEndian;

	nSndRcv=a_clientSock.readC(&unRemEndian, 2);
	if(nSndRcv!=2){return;}
	if (unRemEndian == 1) { isEndianDiffer = 0; }
	else{ isEndianDiffer = 1; }

	nSndRcv=a_clientSock.readC(&newLeaderKey,sizeof(NodeIdentifierKey));
	if(nSndRcv!=sizeof(NodeIdentifierKey)){return;}
	if(isEndianDiffer){SWAP4BYTES(newLeaderKey.port);}

	DEBUG_APP_WITH_NODE(0, newLeaderKey, " [this a correct leader]");
}


void raft::tcp::Server::connect_toAnyNode_bridgeToNodeData(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)
{
	// this socket should remain 
	RaftNode2* pNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey aRemHost;
	int nSndRcv, nEndianDiffer(0);
	uint16_t unRemEndian;

	aRemHost.set_ip4Address(common::socketN::GetIPAddress(a_remoteAddr));	// let us specify host IP

	nSndRcv = a_clientSock.readC(&unRemEndian,2);							// endian
	if(nSndRcv!=2){return;}
	if(unRemEndian!=1){nEndianDiffer=1;}

	nSndRcv = a_clientSock.readC(&aRemHost.port,4);							// port
	if(nSndRcv!=4){return;}
	if(nEndianDiffer){SWAP4BYTES(aRemHost.port);}

	m_mutexShrd.lock_shared();
	if(!m_hashNodes.FindEntry(&aRemHost,sizeof(NodeIdentifierKey),&pNode)){
		m_mutexShrd.unlock_shared();
		return;
	}
	
	pNodeTools = (NodeTools*)pNode->get_udata();
	pNodeTools->isEndianDiffer = nEndianDiffer;
	pNodeTools->dataSocket.SetNewSocketDescriptor(a_clientSock);
	m_mutexShrd.unlock_shared();

	InterruptDataRcv();

	a_clientSock.writeC(&g_ccResponceOk, 1);
	a_clientSock.ResetSocketWithoutClose();
}


void raft::tcp::Server::ThreadFunctionListen()
{
	m_serverTcp.setTimeout(SOCK_TIMEOUT_MS);
	m_serverTcp.StartServer(this, &raft::tcp::Server::AddClient,m_nPortOwn);
}


void raft::tcp::Server::HandleSeedClbk(RaftNode2* a_anyNode)
{
	NodeTools* pTools = (NodeTools*)a_anyNode->get_udata();
	int nSndRcv, nToReceive;
	int msg_type;
	bool bProblematic(true);

	nSndRcv = pTools->raftSocket2.readC(&msg_type, 4);
	if (nSndRcv != 4) { goto returnPoint; }

	switch (msg_type) 
	{
	case RAFT_MSG_REQUESTVOTE:
	{
		msg_requestvote_t reqVote(0,0,0,0);
		DEBUG_APPLICATION(1,"RAFT_MSG_REQUESTVOTE");
		nSndRcv = pTools->raftSocket2.readC(&reqVote, sizeof(msg_requestvote_t));
		if (nSndRcv != sizeof(msg_requestvote_t)) { goto returnPoint; }
		recv_requestvote(a_anyNode, &reqVote);
	}
	break;
	case RAFT_MSG_REQUESTVOTE_RESPONSE:
	{
		msg_requestvote_response_t  reqVoteResp;
		DEBUG_APPLICATION(1,"RAFT_MSG_REQUESTVOTE_RESPONSE");
		nSndRcv = pTools->raftSocket2.readC(&reqVoteResp, sizeof(msg_requestvote_response_t));
		if (nSndRcv != sizeof(msg_requestvote_response_t)) { goto returnPoint; }
		recv_requestvote_response(a_anyNode, &reqVoteResp);
	}
	break;
	case RAFT_MSG_APPENDENTRIES:
	{
		MsgAppendEntries2 appEntries;
		DEBUG_APPLICATION(3,"RAFT_MSG_APPENDENTRIES");
		nSndRcv = pTools->raftSocket2.readC(&appEntries, SIZE_OF_INITIAL_RCV_OF_MSG_APP);
		if (nSndRcv != SIZE_OF_INITIAL_RCV_OF_MSG_APP) { goto returnPoint; }
		if(appEntries.getNEntries()){
			nToReceive = appEntries.getNEntries() * sizeof(msg_entry_t);
			nSndRcv=pTools->raftSocket2.readC(appEntries.entries(),nToReceive);
			if (nSndRcv != nToReceive) { goto returnPoint; }
		}
		recv_appendentries(a_anyNode, &appEntries);
	}
	break;
	case RAFT_MSG_APPENDENTRIES_RESPONSE:
	{
		msg_appendentries_response_t aApndResp;
		DEBUG_APPLICATION(3,"RAFT_MSG_APPENDENTRIES_RESPONSE");
		nSndRcv = pTools->raftSocket2.readC(&aApndResp, sizeof(msg_appendentries_response_t));
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

	nSndRcv = pTools->raftSocket2.readC(&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}

	try {
		switch (cRequest)
		{
		case raft::response::ok:
			++pTools->okCount;
			break;
		case raft::receive::anyNode::clbkCmd:
			DEBUG_APPLICATION(2,"raft::receive::anyNode::clbkCmd");
			HandleSeedClbk(a_pNode);
			break;
		case raft::receive::fromLeader::newNode:
			DEBUG_APPLICATION(1,"raft::receive::fromLeader::newNode");
			if (a_pNode != m_pLeaderNode) { goto returnPoint; }
			nSndRcv = pTools->raftSocket2.readC(&nodeData.nodeKey, sizeof(NodeIdentifierKey));
			if (nSndRcv != sizeof(NodeIdentifierKey)) { goto returnPoint; }
			if (pTools->isEndianDiffer) { SWAP4BYTES(nodeData.nodeKey.port); }
			pNewNodeTools = new NodeTools;
			HANDLE_MEM_DEF2(pNewNodeTools, " ");
			nodeData.pNode = new RaftNode2(pNewNodeTools);
			HANDLE_MEM_DEF2(nodeData.pNode, " ");

			nodeData.action = raft::receive::fromLeader::newNode;
			m_fifoAddDel.AddElement(nodeData);
			m_semaAddRemove.post();

			break;
		case raft::receive::fromLeader::removeNode:
			DEBUG_APPLICATION(1,"raft::receive::fromLeader::removeNode");
			if (a_pNode != m_pLeaderNode) { goto returnPoint; }
			nSndRcv = pTools->raftSocket2.readC(&nodeData.nodeKey, sizeof(NodeIdentifierKey));
			if (nSndRcv != sizeof(NodeIdentifierKey)) { goto returnPoint; }
			if (pTools->isEndianDiffer) { SWAP4BYTES(nodeData.nodeKey.port); }

			if(m_hashNodes.FindEntry(&nodeData.nodeKey,sizeof(NodeIdentifierKey),&nodeData.pNode)){
				nodeData.action = raft::receive::fromLeader::removeNode;
				m_fifoAddDel.AddElement(nodeData);
				m_semaAddRemove.post();
			}

			break;
		case raft::receive::fromNewLeader::oldLeaderDied:
			DEBUG_APPLICATION(1,"raft::receive::fromNewLeader::oldLeaderDied");
			nSndRcv = pTools->raftSocket2.readC(&nodeData.nodeKey, sizeof(NodeIdentifierKey));
			if (nSndRcv != sizeof(NodeIdentifierKey)) { goto returnPoint; }
			if (pTools->isEndianDiffer) { SWAP4BYTES(nodeData.nodeKey.port); }
			if (!(nodeData.nodeKey == *(NodeIdentifierKey*)m_pLeaderNode->key2())) { goto returnPoint; }  // wrong old leader specified

			// old leader node will be known in the remover thread
			// here we will specify new leaer node
			nodeData.pNode = a_pNode;
			nodeData.action = raft::receive::fromNewLeader::oldLeaderDied;
			m_fifoAddDel.AddElement(nodeData);
			m_semaAddRemove.post();
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
				nCurrentSocket = a_bIsRaft?pNodeTools->raftSocket2:pNodeTools->dataSocket;
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
            DEBUG_APPLICATION(2,"Select returned negative value");
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
                nCurrentSocket = a_bIsRaft?pNodeTools->raftSocket2:pNodeTools->dataSocket;
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
    int nSndRcv;
	int16_t	snEndian;
	char vcHostName[MAX_HOSTNAME_LENGTH];
	char cRequest;

enterWhilePoint:

	try {
		while (m_nWork) {
			m_semaWorker.wait();

			while (m_fifoWorker.Extract(&dataFromProducer) && m_nWork) {

				DEBUG_APPLICATION(1,
					"conntion from %s(%s)",
					common::socketN::GetHostName(&dataFromProducer.remAddress, vcHostName, MAX_HOSTNAME_LENGTH),
					common::socketN::GetIPAddress(&dataFromProducer.remAddress));
				aClientSock.SetNewSocketDescriptor(dataFromProducer.sockDescriptor);
				aClientSock.setTimeout(SOCK_TIMEOUT_MS);

				snEndian = 1;
				nSndRcv = aClientSock.writeC(&snEndian, 2);																// 2. Send endian				
				if (nSndRcv != 2) {
					DEBUG_APPLICATION(1, "Could not receive the endian of the connected pear");
					aClientSock.closeC();
					continue;
				}

				nSndRcv = aClientSock.readC(&cRequest, 1);																// 4. rcv request
				if (nSndRcv != 1) {
					DEBUG_APPLICATION(1, "Unable to read request type");
					aClientSock.closeC();
					continue;
				}

				switch (cRequest)
				{
				case raft::connect::toAnyNode::newNode:
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode::newNode");
					connect_toAnyNode_newNode(aClientSock);
					break;
				case raft::connect::toLeader::newNode:
					connect_toLeader_newNode(aClientSock, &dataFromProducer.remAddress);
					DEBUG_APPLICATION(1, "raft::connect::toLeader::newNode");
					break;
				case raft::connect::toAnyNode::raftBridge:
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode::raftBridge");
					connect_toAnyNode_bridgeToNodeRaft(aClientSock, &dataFromProducer.remAddress);
					break;
				case raft::connect::toAnyNode::dataBridge:
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode::dataBridge");
					connect_toAnyNode_bridgeToNodeData(aClientSock, &dataFromProducer.remAddress);
					break;
				case raft::connect::fromClient::allNodesInfo:
					DEBUG_APPLICATION(1, "raft::connect::fromClient::allNodesInfo");
					connect_fromClient_allNodesInfo(aClientSock);
					break;
				case raft::connect::toAnyNode::otherLeaderFound:
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode::otherLeaderFound");
					connect_toAnyNode_otherLeaderFound(aClientSock);
					break;
				default:
					HandleNewConnection(cRequest, aClientSock, &dataFromProducer.remAddress);
					break;
				}
				aClientSock.closeC();
			} // while (m_fifoWorker.Extract(&dataFromProducer) && m_nWork) {
		}  // while (m_nWork) {
	}
	catch(...){
		goto enterWhilePoint;
	}
}


void raft::tcp::Server::HandleNewConnection(char,common::SocketTCP&, const sockaddr_in*)
{
	// this function should be overritten
}


void raft::tcp::Server::ThreadFunctionAddRemoveNode()
{
	RaftNode2* pSkipNode(NULL);
	RaftNode2* pNextNode;
	NodeTools* pNodeTools;
	NodeTools* pNodeToolsLeader;
	NodeIdentifierKey* pKeyForDelete;
	NodeIdentifierKey* pKeyForNewLeader;
	NodeIdentifierKey* pKeyToInform=NULL;
	SAddRemData aData;
	int nSndRcv, nIter;
	uint32_t unOkCount;
	char cRequest;
	bool bInformFollowers, bWaitDone;


	while(m_nWork){
		m_semaAddRemove.wait();

		while(m_fifoAddDel.Extract(&aData) && m_nWork){

			switch (aData.action)
			{
			case raft::newLeaderInternal::becomeLeader:
				cRequest = raft::receive::fromNewLeader::oldLeaderDied;
				bInformFollowers = true;
				bWaitDone = false;
				pSkipNode = m_pLeaderNode;
				pKeyToInform = (NodeIdentifierKey*)m_pLeaderNode->key2();
				break;
			case raft::leaderInternal::newNode:
				cRequest = raft::receive::fromLeader::newNode;
				bInformFollowers = true;
				bWaitDone = true;
				pSkipNode = NULL;
				pKeyToInform = &aData.nodeKey;
				break;
			case raft::leaderInternal::removeNode:
				cRequest = raft::receive::fromLeader::removeNode;
				bInformFollowers = true;
				bWaitDone = false;
				pSkipNode = aData.pNode;
				pKeyToInform = (NodeIdentifierKey*)aData.pNode->key2();
				break;
			default:
				bInformFollowers = false;
				break;
			}  // switch (aData.action)

			if(bInformFollowers){
				m_mutexShrd.lock_shared();
				pNextNode = m_firstNode;
				while (pNextNode) {
					if ((pNextNode != m_thisNode) && (pNextNode!=pSkipNode) && !pNextNode->isProblematic()) {
						pNodeTools = (NodeTools*)pNextNode->get_udata();
						unOkCount = pNodeTools->okCount;
						s_mutexForRaftSend.lock();
						nSndRcv = pNodeTools->raftSocket2.writeC(&cRequest, 1);
						if (nSndRcv != 1) { s_mutexForRaftSend.unlock(); pNextNode->setProblematic(1); goto nextNodePoint; }
						nSndRcv = pNodeTools->raftSocket2.writeC(pKeyToInform,sizeof(NodeIdentifierKey));
						s_mutexForRaftSend.unlock();
						if (nSndRcv != sizeof(NodeIdentifierKey)) { pNextNode->setProblematic(1); goto nextNodePoint; }

						if (bWaitDone) { // wait untill done
							nIter = 0;
							while ((unOkCount==pNodeTools->okCount) && (nIter<MAX_ITER_OK_COUNT)){Sleep(1);++nIter;}
						}

					}
				nextNodePoint:
					pNextNode = pNextNode->next;
				}  // while (pNextNode) {

				m_mutexShrd.unlock_shared();
			} // if(bInformFollowers){

			m_mutexShrd.lock();
			switch (aData.action)
			{
			case raft::leaderInternal::newNode: case raft::receive::fromLeader::newNode:
				DEBUG_APPLICATION(1, "Node (add): %s:%d, numOfNodes=%d", aData.nodeKey.ip4Address, (int)aData.nodeKey.port, m_nNodesCount + 1);
				this->AddNode(aData.pNode, &aData.nodeKey, sizeof(NodeIdentifierKey));
				break;
			case raft::receive::fromNewLeader::oldLeaderDied:
				pKeyForDelete = (NodeIdentifierKey*)m_pLeaderNode->key2();
				pKeyForNewLeader = (NodeIdentifierKey*)aData.pNode->key2();
				DEBUG_APPLICATION(1, "old leader (%s:%d) will be deleted",pKeyForDelete->ip4Address, (int)pKeyForDelete->port);
				DEBUG_APPLICATION(1, "New leader is  %s:%d. NodesCount=%d",pKeyForNewLeader->ip4Address,(int)pKeyForNewLeader->port,m_nNodesCount-1);
				this->RemoveNode(m_pLeaderNode);
				m_pLeaderNode = aData.pNode;
				m_pLeaderNode->makeLeader(1);
				this->become_follower();
				break;
			case raft::leaderInternal::removeNode: case raft::receive::fromLeader::removeNode:
				pKeyForDelete = (NodeIdentifierKey*)aData.pNode->key2();
				DEBUG_APPLICATION(1, "Node (del): %s:%d, numOfNodes=%d", pKeyForDelete->ip4Address, (int)pKeyForDelete->port, m_nNodesCount - 1);
				this->RemoveNode(aData.pNode);
				break;
			case raft::newLeaderInternal::becomeLeader:
				pKeyForDelete = (NodeIdentifierKey*)m_pLeaderNode->key2();
				DEBUG_APP_WITH_NODE(0, *pKeyForDelete, "old leader died");
				DEBUG_APPLICATION(0, "This node will be the leader");
				this->RemoveNode(m_pLeaderNode);
				m_pLeaderNode = m_thisNode;
				m_pLeaderNode->makeLeader(1);
				break;
			default:
				DEBUG_APPLICATION(0, "default");
				break;
			}
			m_mutexShrd.unlock();
            InterruptRaftRcv();
            InterruptDataRcv();

			m_mutexShrd.lock_shared();
			switch (aData.action)
			{
			case raft::leaderInternal::newNode:
				pNodeTools = (NodeTools*)aData.pNode->get_udata();
				s_mutexForRaftSend.lock();
				pNodeTools->raftSocket2.writeC(&g_ccResponceOk, 1);
				s_mutexForRaftSend.unlock();
				break;
			case raft::receive::fromLeader::newNode:
				pNodeToolsLeader = (NodeTools*)m_pLeaderNode->get_udata();
				s_mutexForRaftSend.lock();
				pNodeToolsLeader->raftSocket2.writeC(&g_ccResponceOk, 1);
				s_mutexForRaftSend.unlock();
				break;
			default:
				break;
			}
			m_mutexShrd.unlock_shared();

		} // while(m_fifoAddDel.Extract(&aData) && m_nWork){

	}  // while(m_nWork){
}


void raft::tcp::Server::CheckAllPossibleSeeds(const std::vector<NodeIdentifierKey>& a_vectPossibleNodes)
{
	const char* cpcPosibleSeedIp;
	RaftNode2* pNode;
	NodeIdentifierKey* pNodeKey, *pNodesFromLeader=NULL;
	char vcOwnIp4Address[MAX_IP4_LEN];
	std::vector<NodeIdentifierKey>  vectLeaders;
	common::SocketTCP aSocket;
	const int cnSize((int)a_vectPossibleNodes.size());
	int i,j, nSuccsessIndex(cnSize),nThisIndex(cnSize);
	uint32_t isEndianDiffer;
	int nSndRcv;
	const uint16_t unEndian=1;
	bool bFound;
	
    DEBUG_HANGING();
	common::socketN::GetOwnIp4Address(vcOwnIp4Address,MAX_IP4_LEN);
    DEBUG_HANGING();
 
	try {

		for(i=0;i<cnSize;++i){
			if(  (strncmp(vcOwnIp4Address,a_vectPossibleNodes[i].ip4Address,MAX_IP4_LEN)==0)&&(m_nPortOwn==a_vectPossibleNodes[i].port) ){nThisIndex=i;continue;}
			else {
				cpcPosibleSeedIp = common::socketN::GetIp4AddressFromHostName(a_vectPossibleNodes[i].ip4Address);
				if (cpcPosibleSeedIp && (strncmp(vcOwnIp4Address, cpcPosibleSeedIp, MAX_IP4_LEN) == 0) && (m_nPortOwn == a_vectPossibleNodes[i].port)) { nThisIndex=i; continue; }
			}
            DEBUG_HANGING();
			pNodesFromLeader=TryFindLeaderThrdSafe(a_vectPossibleNodes[i]);
			if(pNodesFromLeader){nSuccsessIndex=i;break;}
            DEBUG_HANGING();
		}

		for(i=nSuccsessIndex+1;i<cnSize;++i){
			bFound = false;
			if(i==nThisIndex){bFound=true;}
			if(  (strncmp(vcOwnIp4Address,a_vectPossibleNodes[i].ip4Address,MAX_IP4_LEN)==0)&&(m_nPortOwn==a_vectPossibleNodes[i].port) ){bFound=true;}
			else {
				cpcPosibleSeedIp = common::socketN::GetIp4AddressFromHostName(a_vectPossibleNodes[i].ip4Address);
				if (cpcPosibleSeedIp && (strncmp(vcOwnIp4Address, cpcPosibleSeedIp, MAX_IP4_LEN) == 0) && (m_nPortOwn == a_vectPossibleNodes[i].port)) { bFound = true; }
				else if(!cpcPosibleSeedIp){cpcPosibleSeedIp=a_vectPossibleNodes[i].ip4Address;}
			}
			for(j=0;(j<m_nNodesCount)&&(!bFound);++j){
				if ((strncmp(pNodesFromLeader[j].ip4Address,cpcPosibleSeedIp,MAX_IP4_LEN)==0) && (pNodesFromLeader[j].port == a_vectPossibleNodes[i].port))
				{bFound=true;break;}
				// no need to try to convert to IP, because in the bottom we did it
			}
			if(!bFound){
				// try to connect and tell about existing leader
				if(!ConnectAndGetEndian(&aSocket, a_vectPossibleNodes[i],raft::connect::toAnyNode::otherLeaderFound,&isEndianDiffer)){continue;}

				//unEndian = 1;
				nSndRcv = aSocket.writeC(&unEndian, 2);
				if (nSndRcv != 2) {goto socketClosePoint;}

				nSndRcv = aSocket.writeC(m_pLeaderNode->key2(),sizeof(NodeIdentifierKey));
				if (nSndRcv != sizeof(NodeIdentifierKey)) { goto socketClosePoint; }
				
				DEBUG_APP_WITH_NODE(0, a_vectPossibleNodes[i]," [possible secondary leader (informed)]");

			socketClosePoint:
				aSocket.closeC();
			}
		}
		
		if (pNodesFromLeader) {
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

	free(pNodesFromLeader);
}


void raft::tcp::Server::ThreadFunctionPeriodic()
{
	try {

		int nIteration = 0;

		while (m_nWork) {
			if(is_leader() && (nIteration++ % 100)==0){
				DEBUG_APPLICATION(2,"Leader node (leaderIteration=%d)", nIteration);
			}
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
	HANDLE_MEM_DEF2(pNode, " ");
	m_thisNode = pNode;
	if(is_leader()){m_pLeaderNode=pNode;pNode->makeLeader(1);}
	this->AddNode(pNode, &aOwnHost, sizeof(NodeIdentifierKey));
}


// const NodeIdentifierKey& nodeInfo, std::vector<NodeIdentifierKey>* pExisting
raft::tcp::NodeIdentifierKey* raft::tcp::Server::TryFindLeaderThrdSafe(const NodeIdentifierKey& a_nodeInfo)
{
	NodeIdentifierKey *pNodesInfo = NULL;
	NodeTools* pTools;
	RaftNode2* pNewNode;
	NodeIdentifierKey leaderNodeKey;
	common::SocketTCP aSocket;
    int i,nSndRcv, nBytesToReceive,numberOfNodes;
	uint32_t  isEndianDiffer;
	uint16_t snEndian2;
	char cRequest;
	bool bOk(false);

	if(!ConnectAndGetEndian(&aSocket,a_nodeInfo,raft::connect::toAnyNode::newNode,&isEndianDiffer)){goto returnPoint;}	// 1. connect, getEndian and sendRequest
	
	nSndRcv= aSocket.readC(&leaderNodeKey,sizeof(NodeIdentifierKey));													// 2. get LeaderAddress
	if(nSndRcv!=sizeof(NodeIdentifierKey)){goto returnPoint;}
	if (isEndianDiffer) { SWAP4BYTES(leaderNodeKey.port); }
	
	aSocket.closeC();

	/*******************************************************************************************************************************************/

	if(!ConnectAndGetEndian(&aSocket,a_nodeInfo,raft::connect::toLeader::newNode,&isEndianDiffer)){goto returnPoint;}	// 1. connect, getEndian and sendRequest
	
	snEndian2 = 1;
	nSndRcv = aSocket.writeC(&snEndian2, 2);																			// 2. send endian
	if (nSndRcv != 2) { goto returnPoint; }

	nSndRcv = aSocket.writeC(&m_nPortOwn, 4);																			// 3. send port number
	if (nSndRcv != 4) { goto returnPoint; }

	nSndRcv = aSocket.readC(&numberOfNodes, 4);																			// 4. rcv number of nodes
	if (nSndRcv != 4) { goto returnPoint; }
	if (isEndianDiffer) { SWAP4BYTES(numberOfNodes); }
	if (numberOfNodes < 1) { goto returnPoint; }

	nBytesToReceive = numberOfNodes * sizeof(NodeIdentifierKey);
	pNodesInfo = (NodeIdentifierKey*)malloc(nBytesToReceive);
	HANDLE_MEM_DEF2(pNodesInfo, " ");

	nSndRcv = aSocket.readC(pNodesInfo, nBytesToReceive);																// 5. receive all nodes info
	if (nSndRcv != nBytesToReceive) { goto returnPoint; }

	if(isEndianDiffer){for(i=0;i<numberOfNodes;++i){SWAP4BYTES(pNodesInfo[i].port);}}									// 6. swap if needed

	nSndRcv = aSocket.readC(&cRequest, 1);																				// 7. wait untill other nodes are ready
	if ((nSndRcv != 1) || (cRequest != response::ok)) { goto returnPoint; }
	bOk = true;  // whith leader everything is Ok


	for(i=0;i<numberOfNodes;++i){
		pTools = new NodeTools;
		HANDLE_MEM_DEF2(pTools, " ");
		pNewNode = new RaftNode2(pTools);
		HANDLE_MEM_DEF2(pNewNode, " ");
		this->AddNode(pNewNode, &pNodesInfo[i], sizeof(NodeIdentifierKey));
		if(leaderNodeKey== pNodesInfo[i]){
			pNewNode->makeLeader(1);
			pTools->raftSocket2.SetNewSocketDescriptor(aSocket);
			aSocket.ResetSocketWithoutClose();
			pTools->isEndianDiffer = isEndianDiffer;
			m_pLeaderNode=pNewNode;
		}
		else {
			// let's connect to all nodes and ask permanent raft socket
			// we will not remove any node in the case of error, removing should 
			// be done in the case of leader request
			if(!ConnectAndGetEndian(&pTools->raftSocket2,pNodesInfo[i],raft::connect::toAnyNode::raftBridge,&isEndianDiffer)){continue;}	// 1. connect, getEndian and sendRequest
			pTools->isEndianDiffer = isEndianDiffer;
			
			snEndian2 = 1;
			nSndRcv=pTools->raftSocket2.writeC(&snEndian2, 2);
			if (nSndRcv != 2) { pTools->raftSocket2.closeC(); continue; }

			nSndRcv=pTools->raftSocket2.writeC(&m_nPortOwn,4);
			if (nSndRcv != 4) { pTools->raftSocket2.closeC(); continue; }
			
			nSndRcv=pTools->raftSocket2.readC(&cRequest, 1);
			if((nSndRcv!=1)||(cRequest!=response::ok)){pTools->raftSocket2.closeC(); continue;}
		}
		
		// Finally let's connect to all nodes and ask permanent data socket
		// we will not remove any node in the case of error, removing should 
		// be done in the case of leader request
		if(!ConnectAndGetEndian(&pTools->dataSocket,pNodesInfo[i],raft::connect::toAnyNode::dataBridge,&isEndianDiffer)){continue;}	// 1. connect, getEndian and sendRequest
		
		snEndian2 = 1;
		nSndRcv=pTools->dataSocket.writeC(&snEndian2, 2);
		if(nSndRcv!=2){pTools->dataSocket.closeC(); continue;}
		
		nSndRcv=pTools->dataSocket.writeC(&m_nPortOwn,4);
		if (nSndRcv != 4) { pTools->dataSocket.closeC(); continue; }

		nSndRcv = pTools->dataSocket.readC(&cRequest, 1);
		if ((nSndRcv != 1) || (cRequest != response::ok)) { pTools->dataSocket.closeC(); continue; }

	}

returnPoint:
	if((!bOk)&&pNodesInfo){free(pNodesInfo);pNodesInfo=NULL;}
	return pNodesInfo;

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
	if(m_pLeaderNode){
		SAddRemData remData;
		remData.action = raft::newLeaderInternal::becomeLeader;
		m_fifoAddDel.AddElement(remData);
		m_semaAddRemove.post();
		//while(m_pLeaderNode!=m_thisNode){Sleep(1);}
	}
	RaftServer::become_leader();
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
	NodeIdentifierKey* pNodeKey = (NodeIdentifierKey*)a_node->key2();
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
			SAddRemData remData;
			remData.action = raft::leaderInternal::removeNode;
			remData.pNode = a_node;
			pServer->m_fifoAddDel.AddElement(remData);
			pServer->m_semaAddRemove.post();
		}
		break;
	default:
		break;
	}

	if(unPingCount>MAX_NUMBER_OF_PINGS){
		DEBUG_APP_WITH_NODE(1, *pNodeKey,"pingCount=%d", unPingCount);
		goto returnPoint;
	}

	s_mutexForRaftSend.lock(); bMutexLocked = true;
	nSndRcv=pTools->raftSocket2.writeC(&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}
	nSndRcv=pTools->raftSocket2.writeC(&a_msg_type,4);
	if(nSndRcv!=4){goto returnPoint;}
	nSndRcv=pTools->raftSocket2.writeC(send_data, d_len);
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


void raft::tcp::Server::SigHandlerStatic(int a_nSigNum)
{
    ServersList* pServer;

#ifndef _WIN32
	pthread_t interruptThread=pthread_self();
#endif

    DEBUG_APPLICATION(0,"Interrupt (No:%d)",a_nSigNum);

	s_pRWlockForServers.lock_shared();
    DEBUG_APPLICATION(1,"rd_lock");

    pServer = s_pFirst;
    while(pServer){

		switch (a_nSigNum)
		{
		case SIGABRT:
			break;
		case SIGFPE:
			break;
		case SIGILL:
			break;
		case SIGINT:
		{
			static int snSigIntCount = 0;
			DEBUG_APPLICATION(0, "Global flag set to 0, next SIGINT will stop server");

			if(snSigIntCount++==0){
				raft::tcp::g_nApplicationRun = 0;
				break;
			}

			DEBUG_APPLICATION(0, "Process will be terminated");

#ifdef _WIN32
			pServer->server->StopServer();
#else
			if (interruptThread != pServer->server->m_starterThread) {
				pthread_kill(pServer->server->m_starterThread, SIGINT);
			}
			else {
				pServer->server->StopServer();
			}
#endif
		}
		break;
		case SIGSEGV:
			break;
		case SIGTERM:
			break;
#if !defined(_WIN32) || defined(_WLAC_USED)
		case SIGPIPE:
			break;
#endif
		default:
			break;
		}
		
		pServer->server->SignalHandler(a_nSigNum);
        pServer = pServer->next;
    }

	s_pRWlockForServers.unlock_shared();
    DEBUG_APPLICATION(1,"unlock");
}


/********************************************************************************************************************/
static int CreateEmptySocket()
{
	int nSocket = (int)::socket(AF_INET, SOCK_STREAM, 0);
	return nSocket;
}

static void AddNewRaftServer(raft::tcp::Server* a_pServer)
{
    ServersList* pServerList = (ServersList*)calloc(1,sizeof(ServersList));
	HANDLE_MEM_DEF2(pServerList, " ");
    pServerList->server = a_pServer;
    a_pServer->m_pReserved1 = pServerList;
	s_pRWlockForServers.lock();
    DEBUG_APPLICATION(1,"wr_lock");
    if(s_pFirst){s_pFirst->prev =pServerList;}
    pServerList->next = s_pFirst;
    s_pFirst = pServerList;
	s_pRWlockForServers.unlock();
    DEBUG_APPLICATION(1,"unlock");
}


static void RemoveRaftServer(raft::tcp::Server* a_pServer)
{
    ServersList* pListItem = (ServersList*)a_pServer->m_pReserved1;
	s_pRWlockForServers.lock();
    DEBUG_APPLICATION(1,"wr_lock");
    if(pListItem->next){pListItem->next->prev=pListItem->prev;}
    if(pListItem->prev){pListItem->prev->next=pListItem->next;}
    if(pListItem==s_pFirst){s_pFirst=pListItem->next;}
	s_pRWlockForServers.unlock();
    DEBUG_APPLICATION(1,"unlock");
    free(pListItem);
}
