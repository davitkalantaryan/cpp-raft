
#ifndef HEADER_SIZE_needed
#define HEADER_SIZE_needed
#endif

#include "raft_tcp_server.hpp"
#include <string>
#include <stdint.h>
#include <signal.h>
#include "common/newlockguards.hpp"
#ifndef _WIN32
#include <pthread.h>
#endif

// rep. rate is the rate of periodic raft job

#ifdef MAKE_LONG_WAIT_DEBUG

#define MIN_REP_RATE_MS					5
#define DEF_REP_RATE_MS					5000
#define	TIMEOUTS_RATIO_MIN				5
#define REPORT_ON_FAULT(_faultyNode)
#define MAX_NUMBER_OF_PINGS				2
#define MAX_UNANSWERED_PINGS			10
#define MAX_ITER_OK_COUNT				6

#else

#define MIN_REP_RATE_MS					5		// les than this rep rate is not possible
#define DEF_REP_RATE_MS					2000	// [ms] default rep rate is 2 seconds
#define	TIMEOUTS_RATIO_MIN				11		// this is minimum ratio for follower between time of leader wait and rep. rate (to start election)
#define MAX_NUMBER_OF_PINGS				2		// maximum number of pings that really will be done by leader
#define MAX_UNANSWERED_PINGS			14		// number of virtual pings, after this leader will remove follower
#define MAX_ITER_OK_COUNT				5		// used for syncronization

#endif

#ifdef _WIN32
#else
#define closesocket	close
#endif

struct ServersList { raft::tcp::Server *server; ServersList *prev, *next; };
static struct ServersList* s_pFirst = NULL;
//static pthread_rwlock_t s_pRWlockForServers = PTHREAD_RWLOCK_INITIALIZER;
static STDN::shared_mutex s_pRWlockForServers;

//static void SigActionFunction (int, siginfo_t *, void *); last 2 arguments are not used
static void AddNewRaftServer(raft::tcp::Server* a_pServer);
static void RemoveRaftServer(raft::tcp::Server* a_pServer);

static std::mutex s_mutexForRaftSend;

#define LOCK_RAFT_SEND_MUTEX(_node)		LOCK_SEND_SOCKET_MUTEX2(&s_mutexForRaftSend)

namespace raft{namespace tcp{
int g_nApplicationRun = 0;
}}

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


void raft::tcp::Server::StateChangedAfter(char a_state, NodeIdentifierKey a_key, void* a_clbkData)
{
}


void raft::tcp::Server::StateChangedBeforeNoLock(const SAddRemData& a_changeData)
{
}


void raft::tcp::Server::StateChangedLocked(const SAddRemData& a_changeData)
{
}


void raft::tcp::Server::SignalHandler(int )
{
}


void raft::tcp::Server::AddAdditionalDataToNode(RaftNode2* a_newNode)
{
}


void raft::tcp::Server::CleanNodeData(RaftNode2* a_node)
{
	NodeTools* pNodeTool = (NodeTools*)a_node->get_udata();

	if(pNodeTool){delete pNodeTool;}
	RaftServer::CleanNodeData(a_node);
}


void raft::tcp::Server::ReceiveFromDataSocket(RaftNode2*) // this should be overriten by child
{
}


int raft::tcp::Server::RunServerOnOtherThreads(const std::vector<NodeIdentifierKey>& a_vectPossibleNodes, int a_nWorkersCount, int a_nRaftPort)
{
    DEBUG_HANGING();
	std::thread* pWorker;

#ifndef _WIN32
	m_starterThread = pthread_self();
#endif  // #ifdef HANDLE_SIG_ACTIONS

	if (m_nWork) {return -1;}

	if(a_nRaftPort>0){m_nPortOwn = a_nRaftPort;} // otherwise child class inited m_nPortOwn
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
	NodeIdentifierKey* pLeaderKey = (NodeIdentifierKey*)m_pLeaderNode->key;
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
	if(strcmp(addNodeData.nodeKey.ip4Address,"127.0.0.1")==0){
		common::socketN::GetOwnIp4Address(addNodeData.nodeKey.ip4Address, MAX_IP4_LEN);
	}

	nSndRcv= a_clientSock.readC(&unRemEndian,2);
	if(nSndRcv!= 2){ goto returnPoint;}
	if(unRemEndian!=1){ nEndianDiffer=1;}

	nSndRcv= a_clientSock.readC(&addNodeData.nodeKey.port,4);
	if(nSndRcv!= 4){ goto returnPoint;}
	if(nEndianDiffer){ SWAP4BYTES(addNodeData.nodeKey.port);}

	pNewNodeTool = new NodeTools;
	HANDLE_MEM_DEF2(pNewNodeTool," ");
	pNewNodeTool->isEndianDiffer = nEndianDiffer;
	
	// first let's check if the node does not exist
	if(m_Nodes.FindEntry(&addNodeData.nodeKey,sizeof(NodeIdentifierKey),&pNode)){
		// send 0, as a sign, that node exists
		nTotalSize = raft::response::error::nodeExist;
		a_clientSock.writeC(&nTotalSize, 4);
		goto returnPoint;
	}

	nNodesCount = m_Nodes.count();
	pAllNodesInfo=CollectAllNodesDataNotThrSafe(&nTotalSize,NULL);

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


RaftNode2* raft::tcp::Server::connect_toAnyNode_bridgeToNodeRaft(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)
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
	if(nSndRcv!=2){return NULL;}
	if(unRemEndian!=1){nEndianDiffer=1;}

	nSndRcv = a_clientSock.readC(&aRemHost.port,4);							// port
	if(nSndRcv!=4){return NULL;}
	if(nEndianDiffer){SWAP4BYTES(aRemHost.port);}

	if(!m_Nodes.FindEntry(&aRemHost,sizeof(NodeIdentifierKey),&pNode)){
		return NULL;
	}
	
	pNodeTools = (NodeTools*)pNode->get_udata();
	pNodeTools->isEndianDiffer = nEndianDiffer;
	pNodeTools->raftSocket2.SetNewSocketDescriptor(a_clientSock);

	InterruptRaftRcv();

	a_clientSock.writeC(&g_ccResponceOk, 1);
	a_clientSock.ResetSocketWithoutClose();
	return pNode;
}


raft::tcp::NodeIdentifierKey* raft::tcp::Server::CollectAllNodesDataNotThrSafe(int* a_pnTotalSize, int* a_pnLeaderIndex)
{
	RaftNode2* pNode;
	NodeIdentifierKey *pAllNodesInfo, *pExistingNodeKey;
	int i(0);
	
	*a_pnTotalSize = m_Nodes.count() * sizeof(NodeIdentifierKey);
	pAllNodesInfo = (NodeIdentifierKey*)malloc(*a_pnTotalSize);
	HANDLE_MEM_DEF2(pAllNodesInfo, " ");

	// collect info
	pNode = m_Nodes.first();
	while (pNode) {
		pExistingNodeKey = (NodeIdentifierKey*)pNode->key;
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

	nl.nodesCount = m_Nodes.count();
	pAllNodesInfo = CollectAllNodesDataNotThrSafe(&nTotalSize,&nl.leaderIndex);

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

	DEBUG_APP_WITH_NODE(0, newLeaderKey, " [this is a correct leader]");
}


RaftNode2* raft::tcp::Server::connect_toAnyNode_bridgeToNodeData(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)
{
	// this socket should remain 
	RaftNode2* pNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey aRemHost;
	int nSndRcv, nEndianDiffer(0);
	uint16_t unRemEndian;

	aRemHost.set_ip4Address(common::socketN::GetIPAddress(a_remoteAddr));	// let us specify host IP

	nSndRcv = a_clientSock.readC(&unRemEndian,2);							// endian
	if(nSndRcv!=2){return NULL;}
	if(unRemEndian!=1){nEndianDiffer=1;}

	nSndRcv = a_clientSock.readC(&aRemHost.port,4);							// port
	if(nSndRcv!=4){return NULL;}
	if(nEndianDiffer){SWAP4BYTES(aRemHost.port);}

	if(!m_Nodes.FindEntry(&aRemHost,sizeof(NodeIdentifierKey),&pNode)){
		return NULL;
	}
	
	pNodeTools = (NodeTools*)pNode->get_udata();
	pNodeTools->isEndianDiffer = nEndianDiffer;
	pNodeTools->dataSocket.SetNewSocketDescriptor(a_clientSock);

	a_clientSock.writeC(&g_ccResponceOk, 1);
	a_clientSock.ResetSocketWithoutClose();

	InterruptDataRcv();

	return pNode;
}


void raft::tcp::Server::ThreadFunctionListen()
{
	m_serverTcp.setTimeout(SOCK_TIMEOUT_MS);
	m_serverTcp.StartServer(this, &raft::tcp::Server::AddClient,m_nPortOwn);
}


void raft::tcp::Server::HandleSeedClbk(RaftNode2* a_anyNode)
{
	NodeTools* pTools = (NodeTools*)a_anyNode->get_udata();
	NodeIdentifierKey* pNodeKey = (NodeIdentifierKey*)a_anyNode->key;
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
		DEBUG_APP_WITH_NODE(1,*pNodeKey,"RAFT_MSG_REQUESTVOTE");
		nSndRcv = pTools->raftSocket2.readC(&reqVote, sizeof(msg_requestvote_t));
		if (nSndRcv != sizeof(msg_requestvote_t)) { goto returnPoint; }
		recv_requestvote(a_anyNode, &reqVote);
	}
	break;
	case RAFT_MSG_REQUESTVOTE_RESPONSE:
	{
		msg_requestvote_response_t  reqVoteResp;
		DEBUG_APP_WITH_NODE(1, *pNodeKey,"RAFT_MSG_REQUESTVOTE_RESPONSE");
		nSndRcv = pTools->raftSocket2.readC(&reqVoteResp, sizeof(msg_requestvote_response_t));
		if (nSndRcv != sizeof(msg_requestvote_response_t)) { goto returnPoint; }
		a_anyNode->pingReceived();
		recv_requestvote_response(a_anyNode, &reqVoteResp);
	}
	break;
	case RAFT_MSG_APPENDENTRIES:
	{
		MsgAppendEntries2 appEntries;
		DEBUG_APP_WITH_NODE(3, *pNodeKey,"RAFT_MSG_APPENDENTRIES");
		nSndRcv = pTools->raftSocket2.readC(&appEntries, SIZE_OF_INITIAL_RCV_OF_MSG_APP);
		if (nSndRcv != SIZE_OF_INITIAL_RCV_OF_MSG_APP) { goto returnPoint; }
		if(appEntries.getNEntries()){
			nToReceive = appEntries.getNEntries() * sizeof(msg_entry_t);
			nSndRcv=pTools->raftSocket2.readC(appEntries.entries(),nToReceive);
			if (nSndRcv != nToReceive) { goto returnPoint; }
		}
		recv_appendentries(true,a_anyNode, &appEntries);
		ftime(&(this->m_lastPingByLeader));
	}
	break;
	case RAFT_MSG_APPENDENTRIES_RESPONSE:
	{
		msg_appendentries_response_t aApndResp;
		DEBUG_APP_WITH_NODE(3, *pNodeKey,"RAFT_MSG_APPENDENTRIES_RESPONSE");
		nSndRcv = pTools->raftSocket2.readC(&aApndResp, sizeof(msg_appendentries_response_t));
		if (nSndRcv != sizeof(msg_appendentries_response_t)) { goto returnPoint; }
		a_anyNode->pingReceived();
		this->recv_appendentries_response(a_anyNode, &aApndResp);
		// a_anyNode->pingReceived(); // this does not work because of exception
	}
	break;
	default:
		DEBUG_APP_WITH_NODE(0, *pNodeKey,"raft-receive: default:");
		goto returnPoint;
	}

	bProblematic = false;
returnPoint:
	if (bProblematic) { a_anyNode->setProblematic(); }
}


void raft::tcp::Server::ReceiveFromRaftSocket(RaftNode2* a_pNode)
{
	NodeTools *pNewNodeTools,*pTools = (NodeTools*)a_pNode->get_udata();
	NodeIdentifierKey* pNodeKey = (NodeIdentifierKey*)a_pNode->key;
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
		case raft::receive::follower::resetPing:
			a_pNode->pingReceived();
			DEBUG_APP_WITH_NODE(2, *pNodeKey, "raft::receive::follower::resetPing");
			break;
		case raft::receive::anyNode::clbkCmd:
			HandleSeedClbk(a_pNode);
			DEBUG_APP_WITH_NODE(2, *pNodeKey, "raft::receive::anyNode::clbkCmd");
			break;
		case raft::receive::fromLeader::newNode:
			DEBUG_APP_WITH_NODE(1,*pNodeKey,"raft::receive::fromLeader::newNode");
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
			DEBUG_APP_WITH_NODE(1, *pNodeKey,"raft::receive::fromLeader::removeNode");
			if (a_pNode != m_pLeaderNode) { goto returnPoint; }
			nSndRcv = pTools->raftSocket2.readC(&nodeData.nodeKey, sizeof(NodeIdentifierKey));
			if (nSndRcv != sizeof(NodeIdentifierKey)) { goto returnPoint; }
			if (pTools->isEndianDiffer) { SWAP4BYTES(nodeData.nodeKey.port); }

			if(m_Nodes.FindEntry(&nodeData.nodeKey,sizeof(NodeIdentifierKey),&nodeData.pNode)){
				nodeData.action = raft::receive::fromLeader::removeNode;
				m_fifoAddDel.AddElement(nodeData);
				m_semaAddRemove.post();
			}

			break;
		case raft::receive::fromNewLeader::oldLeaderDied:
			DEBUG_APP_WITH_NODE(1, *pNodeKey,"raft::receive::fromNewLeader::oldLeaderDied");
			nSndRcv = pTools->raftSocket2.readC(&nodeData.nodeKey, sizeof(NodeIdentifierKey));
			if (nSndRcv != sizeof(NodeIdentifierKey)) { goto returnPoint; }
			if (pTools->isEndianDiffer) { SWAP4BYTES(nodeData.nodeKey.port); }
			if (!(nodeData.nodeKey == *(NodeIdentifierKey*)m_pLeaderNode->key)) { goto returnPoint; }  // wrong old leader specified

			// old leader node will be known in the remover thread
			// here we will specify new leaer node
			nodeData.pNode = a_pNode;
			nodeData.action = raft::receive::fromNewLeader::oldLeaderDied;
			m_fifoAddDel.AddElement(nodeData);
			m_semaAddRemove.post();
			break;
		default:
			DEBUG_APP_WITH_NODE(0, *pNodeKey,"default: (num=%d)",(int)cRequest);
			break;
		}
	}
	catch (...){
	}


	bProblematic = false;
	a_pNode->pingReceived();
returnPoint:
	if(bProblematic){ a_pNode->setProblematic();}
}


void raft::tcp::Server::FunctionForMultiRcv(volatile int* a_pnSocketForInfo, void (Server::*a_fpRcvFnc)(RaftNode2*), bool a_bIsRaft)
{
	NodeTools* pNodeTools;
	RaftNode2* pNode;
	common::NewSharedLockGuard<STDN::shared_mutex> aShrdLockGuard;
	fd_set rFds, eFds;
	int nMax, nCurrentSocket, nSelectReturn, nSocketsFound, nSocketToIgnore=-1, nLastSocket;
    volatile int& nSocketForInfo = *a_pnSocketForInfo;

enterLoopPoint:
	try{

		while (m_nWork) {
			FD_ZERO(&rFds); FD_ZERO(&eFds);
			aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);  // --> locking
			if (nSocketForInfo <= 0) { nSocketForInfo = CreateEmptySocket(); }
			nMax = nSocketForInfo;
#ifdef _WIN32
			FD_SET(nSocketForInfo, &rFds);
#else
			FD_SET(nSocketForInfo, &eFds);
#endif
			pNode = m_Nodes.first();
			nLastSocket = -1;
			while (pNode) {
				if ((pNode != m_thisNode) && (!pNode->isProblematic())) {
					pNodeTools = (NodeTools*)pNode->get_udata();
					nCurrentSocket = a_bIsRaft ? pNodeTools->raftSocket2 : pNodeTools->dataSocket;
					if ((nCurrentSocket>0) && (nCurrentSocket != nSocketToIgnore)) {
						FD_SET(nCurrentSocket, &rFds);
						FD_SET(nCurrentSocket, &eFds);
						if (nCurrentSocket>nMax) { nMax = nCurrentSocket; }
					}  // if(pNodeTools->socket>0){
				}  // if(pNode!=m_thisNode){
				pNode = pNode->next;
			}// while(pNode){
			aShrdLockGuard.UnsetAndUnlockMutex();  // --> unlocking

			nSelectReturn = ::select(++nMax, &rFds, NULL, &eFds, NULL);
			if (!m_nWork) { break; }
			if (nSelectReturn < 0) {
				DEBUG_APPLICATION(2, "Select returned negative value");
				//Sleep(2000);  // most probably last client was problematic
				nSocketToIgnore = nLastSocket;
			} // will never happen (or?)
			else { nSocketToIgnore = -1; }
			nSocketsFound = 0;
			aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);  // --> locking
			if (FD_ISSET(nSocketForInfo, &rFds) || FD_ISSET(nSocketForInfo, &eFds)) {
				++nSocketsFound;
				closesocket(nSocketForInfo);  //  is it necessary?
				nSocketForInfo = -1;
			}

			pNode = m_Nodes.first();
			while (pNode && (nSocketsFound<nSelectReturn)) {
				if (pNode != m_thisNode) {
					pNodeTools = (NodeTools*)pNode->get_udata();
					nCurrentSocket = a_bIsRaft ? pNodeTools->raftSocket2 : pNodeTools->dataSocket;
					if (FD_ISSET(nCurrentSocket, &rFds)) {
						// call receive ...
						nLastSocket = nCurrentSocket;
						(this->*a_fpRcvFnc)(pNode);
						++nSocketsFound;
					}  // if(pNodeTools->socket>0){
					if (FD_ISSET(nCurrentSocket, &eFds)) {
						pNode->setProblematic();
						++nSocketsFound;
					}  // if(pNodeTools->socket>0){
				}  // if(pNode!=m_thisNode){
				pNode = pNode->next;
			}// while(pNode){
			aShrdLockGuard.UnsetAndUnlockMutex();  // --> unlocking

		} // while (m_nWork){
	}
	catch(...){
		aShrdLockGuard.UnsetAndUnlockMutex();
		goto enterLoopPoint;
	}

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
	RaftNode2* pNode;
	common::SocketTCP aClientSock;
	SWorkerData dataFromProducer;
	common::NewSharedLockGuard<STDN::shared_mutex> aShrdLockGuard;
    int nSndRcv;
	int16_t	snEndian;
	char vcHostName[MAX_HOSTNAME_LENGTH];
	char cRequest;
	
enterLoopPoint:
	try {
		while (m_nWork) {
			m_semaWorker.wait();

			while (m_fifoWorker.Extract(&dataFromProducer) && m_nWork) {

				pNode = NULL;
				DEBUG_APPLICATION(1,
					"conntion from %s(%s)",
					common::socketN::GetHostName(&dataFromProducer.remAddress, vcHostName, MAX_HOSTNAME_LENGTH),
					common::socketN::GetIPAddress(&dataFromProducer.remAddress));
				aClientSock.SetNewSocketDescriptor(dataFromProducer.sockDescriptor);
				aClientSock.setTimeout(SOCK_TIMEOUT_MS);

				snEndian = 1;
				nSndRcv = aClientSock.writeC(&snEndian, 2);																// 2. Send endian				
				if (nSndRcv != 2) {
					DEBUG_APPLICATION(1, "Could not send the endian of the connected pear nSndRcv=%d, socket=%d",nSndRcv, dataFromProducer.sockDescriptor);
					aClientSock.closeC();
					continue;
				}

				nSndRcv = aClientSock.readC(&cRequest, 1);																// 4. rcv request
				if (nSndRcv != 1) {
					DEBUG_APPLICATION(1, "Unable to read request type");
					aClientSock.closeC();
					continue;
				}

				aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);					// --> shared lock
				switch (cRequest)
				{
				case raft::connect::toAnyNode::newNode:
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode::newNode");
					connect_toAnyNode_newNode(aClientSock);
					break;
				case raft::connect::toLeader::newNode:
					if(!is_leader()){
						ERROR_LOGGING2("This node is not leader, but reques is done for leader!");
					}
					else{
						connect_toLeader_newNode(aClientSock, &dataFromProducer.remAddress);
						DEBUG_APPLICATION(1, "raft::connect::toLeader::newNode");
					}
					break;
				case raft::connect::toAnyNode::raftBridge:
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode::raftBridge");
					pNode=connect_toAnyNode_bridgeToNodeRaft(aClientSock, &dataFromProducer.remAddress);
					break;
				case raft::connect::toAnyNode::dataBridge:
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode::dataBridge");
					pNode=connect_toAnyNode_bridgeToNodeData(aClientSock, &dataFromProducer.remAddress);
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
					break;
				}
				HandleNewConnection(cRequest, aClientSock, &dataFromProducer.remAddress,pNode);
				aShrdLockGuard.UnsetAndUnlockMutex();								// --> shared unlock
				aClientSock.closeC();
			} // while (m_fifoWorker.Extract(&dataFromProducer) && m_nWork) {
		}  // while (m_nWork) {
	}
	catch(...){
		goto enterLoopPoint;
	}
}


void raft::tcp::Server::HandleNewConnection(char,common::SocketTCP&, const sockaddr_in*, RaftNode2*)
{
	// this function should be overritten
}


void raft::tcp::Server::ThreadFunctionAddRemoveNode()
{
	PREPARE_SEND_SOCKET_GUARD();
	void* clbkData;
	RaftNode2* pSkipNode(NULL);
	RaftNode2* pNextNode;
	NodeTools* pNodeTools;
	NodeTools* pNodeToolsLeader;
	NodeIdentifierKey* pKeyForDelete;
	NodeIdentifierKey* pKeyForNewLeader;
	NodeIdentifierKey* pKeyToInform=NULL;
	NodeIdentifierKey keyForInform;
	SAddRemData aData;
	common::NewSharedLockGuard<STDN::shared_mutex> aShrdLockGuard;
	common::NewLockGuard<STDN::shared_mutex> aLockGuard;
	int nSndRcv, nIter;
	uint32_t unOkCount;
	char cRequest;
	bool bInformFollowers, bWaitDone;
	
enterLoopPoint:
	try {
		while (m_nWork) {
			m_semaAddRemove.wait();

			while (m_fifoAddDel.Extract(&aData) && m_nWork) {

				aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);    // --> shared locking
				StateChangedBeforeNoLock(aData);

				switch (aData.action)
				{
				case raft::newLeaderInternal::becomeLeader:
					cRequest = raft::receive::fromNewLeader::oldLeaderDied;
					bInformFollowers = true;
					bWaitDone = false;
					pSkipNode = m_pLeaderNode;
					pKeyToInform = (NodeIdentifierKey*)m_pLeaderNode->key;
					keyForInform = *pKeyToInform;
					clbkData = NULL;
					break;
				case raft::leaderInternal::newNode:
					cRequest = raft::receive::fromLeader::newNode;
					bInformFollowers = true;
					bWaitDone = true;
					pSkipNode = NULL;
					pKeyToInform = &aData.nodeKey;
					keyForInform = *pKeyToInform;
					clbkData = NULL;
					break;
				case raft::leaderInternal::removeNode:
					cRequest = raft::receive::fromLeader::removeNode;
					bInformFollowers = true;
					bWaitDone = false;
					pSkipNode = aData.pNode;
					pKeyToInform = (NodeIdentifierKey*)aData.pNode->key;
					keyForInform = *pKeyToInform;
					clbkData = GET_CLBK_DATA(aData.pNode);
					break;
				default:
					bInformFollowers = false;
					//if(aData.pNode){keyForInform=*((NodeIdentifierKey*)aData.pNode->key2());clbkData=GET_CLBK_DATA(aData.pNode);}
					//else {keyForInform=aData.nodeKey;clbkData=NULL;}
					keyForInform = aData.nodeKey; clbkData = NULL;
					break;
				}  // switch (aData.action)

				if (bInformFollowers) {
					pNextNode = m_Nodes.first();
					while (pNextNode) {
						if ((pNextNode != m_thisNode) && (pNextNode != pSkipNode) && !pNextNode->isProblematic()) {
							pNodeTools = (NodeTools*)pNextNode->get_udata();
							unOkCount = pNodeTools->okCount;
							LOCK_RAFT_SEND_MUTEX(pNextNode);
							nSndRcv = pNodeTools->raftSocket2.writeC(&cRequest, 1);
							if (nSndRcv != 1) {pNextNode->setProblematic(); goto nextNodePoint; }
							nSndRcv = pNodeTools->raftSocket2.writeC(pKeyToInform, sizeof(NodeIdentifierKey));
							UNLOCK_SEND_SOCKET_MUTEX2();
							if (nSndRcv != sizeof(NodeIdentifierKey)) { pNextNode->setProblematic(); goto nextNodePoint; }

							if (bWaitDone) { // wait untill done
								nIter = 0;
								while ((unOkCount == pNodeTools->okCount) && (nIter<MAX_ITER_OK_COUNT)) { Sleep(1); ++nIter; }
							}
						nextNodePoint:
							UNLOCK_SEND_SOCKET_MUTEX2();
						}
						pNextNode = pNextNode->next;
					}  // while (pNextNode) {

				} // if(bInformFollowers){

				aShrdLockGuard.UnsetAndUnlockMutex();    // --> shared unlocking

				aLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);   // --> locking
				StateChangedLocked(aData);
				switch (aData.action)
				{
				case raft::leaderInternal::newNode: case raft::receive::fromLeader::newNode:
					DEBUG_APPLICATION(1, "Node (add): %s:%d, numOfNodes=%d", aData.nodeKey.ip4Address, (int)aData.nodeKey.port, m_Nodes.count() + 1);
					AddAdditionalDataToNode(aData.pNode);
					m_Nodes.AddData(aData.pNode, &aData.nodeKey, sizeof(NodeIdentifierKey));
					break;
				case raft::receive::fromNewLeader::oldLeaderDied:
					pKeyForDelete = (NodeIdentifierKey*)m_pLeaderNode->key;
					pKeyForNewLeader = (NodeIdentifierKey*)aData.pNode->key;
					DEBUG_APPLICATION(1, "old leader (%s:%d) will be deleted", pKeyForDelete->ip4Address, (int)pKeyForDelete->port);
					DEBUG_APPLICATION(1, "New leader is  %s:%d. NodesCount=%d", pKeyForNewLeader->ip4Address, (int)pKeyForNewLeader->port, m_Nodes.count() - 1);
					this->RemoveNode2(m_pLeaderNode);
					m_pLeaderNode = aData.pNode;
					m_pLeaderNode->makeLeader();
					this->become_follower();
					break;
				case raft::leaderInternal::removeNode: case raft::receive::fromLeader::removeNode:
					pKeyForDelete = (NodeIdentifierKey*)aData.pNode->key;
					DEBUG_APPLICATION(1, "Node (del): %s:%d, numOfNodes=%d", pKeyForDelete->ip4Address, (int)pKeyForDelete->port, m_Nodes.count() - 1);
					this->RemoveNode2(aData.pNode);
					break;
				case raft::newLeaderInternal::becomeLeader:
					pKeyForDelete = (NodeIdentifierKey*)m_pLeaderNode->key;
					DEBUG_APP_WITH_NODE(0, *pKeyForDelete, "old leader died");
					this->RemoveNode2(m_pLeaderNode);
					DEBUG_APPLICATION(0, "This node will be the leader (numberOfNodes=%d)", m_Nodes.count());
					m_pLeaderNode = m_thisNode;
					m_pLeaderNode->makeLeader();
					break;
				default:
					DEBUG_APPLICATION(2, "default");
					break;
				}
				aLockGuard.UnsetAndUnlockMutex();						// --> unlocking
				InterruptRaftRcv();
				InterruptDataRcv();

				aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);		// --> shared locking
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
				StateChangedAfter(aData.action,keyForInform,clbkData);
				aShrdLockGuard.UnsetAndUnlockMutex();						// --> shared unlocking

			} // while(m_fifoAddDel.Extract(&aData) && m_nWork){

		}  // while(m_nWork){
	}
	catch(...){
		UNLOCK_SEND_SOCKET_MUTEX2();
		aShrdLockGuard.UnsetAndUnlockMutex();
		aLockGuard.UnsetAndUnlockMutex();
		goto enterLoopPoint;
	}
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
	int nNodesCount(0);
	uint32_t isEndianDiffer;
	int nSndRcv;
	const uint16_t unEndian=1;
	bool bFound;
	
    DEBUG_HANGING();
	common::socketN::GetOwnIp4Address(vcOwnIp4Address,MAX_IP4_LEN);
    DEBUG_HANGING();
 
	try {

		for(i=0;i<cnSize;++i){
			DEBUG_APP_WITH_NODE(2, a_vectPossibleNodes[i], "trying to connect");
			if(  (strncmp(vcOwnIp4Address,a_vectPossibleNodes[i].ip4Address,MAX_IP4_LEN)==0)&&(m_nPortOwn==a_vectPossibleNodes[i].port) ){nThisIndex=i;continue;}
			else {
				cpcPosibleSeedIp = common::socketN::GetIp4AddressFromHostName(a_vectPossibleNodes[i].ip4Address);
				if (cpcPosibleSeedIp) {
					DEBUG_APP_WITH_NODE(3, a_vectPossibleNodes[i],"cpcPosibleSeedIp=%s, m_nPortOwn=%d", cpcPosibleSeedIp, m_nPortOwn);
					if(strcmp(cpcPosibleSeedIp,"127.0.0.1")==0){
						if(m_nPortOwn== a_vectPossibleNodes[i].port){nThisIndex = i; continue;}
					}
					else if(strncmp(vcOwnIp4Address, cpcPosibleSeedIp, MAX_IP4_LEN) == 0){
						if(m_nPortOwn== a_vectPossibleNodes[i].port){nThisIndex = i; continue;}
					}
				}
			}
            DEBUG_HANGING();
			pNodesFromLeader=TryFindLeaderThrdSafe(a_vectPossibleNodes[i]);
			if(pNodesFromLeader){nNodesCount=m_Nodes.count();nSuccsessIndex=i;break;}
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
			for(j=0;(j<nNodesCount)&&(!bFound);++j){
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

				nSndRcv = aSocket.writeC(m_pLeaderNode->key,sizeof(NodeIdentifierKey));
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

		DEBUG_APPLICATION(1, "NumberOfNodes=%d", m_Nodes.count());
		pNode = m_Nodes.first();
		while(pNode){
			pNodeKey = (NodeIdentifierKey*)pNode->key;
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


#define MSEC(finish, start)	( (int)( (finish).millitm - (start).millitm ) + \
							(int)( (finish).time - (start).time ) * 1000 )


void raft::tcp::Server::ThreadFunctionPeriodic()
{
	PREPARE_SEND_SOCKET_GUARD();
	timeb	aCurrentTime;
	int nTimeDiff;
	const char cRequest = raft::receive::follower::resetPing;
	common::NewSharedLockGuard<STDN::shared_mutex> aShrdLockGuard;
	int nIteration(0), nSndRcv;
	
enterLoopPoint:
	try {
		
		while (m_nWork) {
			if(is_leader() && (nIteration++ % 100)==0){
				DEBUG_APPLICATION(2,"Leader node (leaderIteration=%d)", nIteration);
			}
			aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
			this->periodic(m_nPeriodForPeriodic);
			aShrdLockGuard.UnsetAndUnlockMutex();
			if (is_follower() && (!m_pLeaderNode->isProblematic())) {
				ftime(&aCurrentTime);
				nTimeDiff = MSEC(aCurrentTime,this->m_lastPingByLeader);
				if(nTimeDiff>(2*m_nPeriodForPeriodic)){
					LOCK_RAFT_SEND_MUTEX(m_pLeaderNode);
					nSndRcv = GET_NODE_TOOLS(m_pLeaderNode)->raftSocket2.writeC(&cRequest, 1);
					UNLOCK_SEND_SOCKET_MUTEX2();
					if (nSndRcv != 1) {
						ERROR_LOGGING2("ERROR: leader is problematic");
						m_pLeaderNode->setProblematic();
					}
				}  // if(nTimeDiff>(2*m_nPeriodForPeriodic)){
			}  // if (is_follower() && (!m_pLeaderNode->isProblematic())) {
			Sleep(m_nPeriodForPeriodic);
		}
	}
	catch (...) {
		UNLOCK_SEND_SOCKET_MUTEX2();
		aShrdLockGuard.UnsetAndUnlockMutex();
		goto enterLoopPoint;
	}
}


void raft::tcp::Server::AddOwnNode()
{
	//typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
	NodeTools* pTools = new NodeTools;
	RaftNode2* pNode;
	NodeIdentifierKey aOwnHost;

	HANDLE_MEM_DEF2(pTools, " ");

	pTools->isEndianDiffer=0;
	common::socketN::GetOwnIp4Address(aOwnHost.ip4Address, MAX_IP4_LEN);
	aOwnHost.port = m_nPortOwn;
	pNode = new RaftNode2(pTools);
	HANDLE_MEM_DEF2(pNode, " ");
	m_thisNode = pNode;
	if(is_leader()){m_pLeaderNode=pNode;pNode->makeLeader();}
	AddAdditionalDataToNode(pNode);
	m_Nodes.AddData(pNode, &aOwnHost, sizeof(NodeIdentifierKey));
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
	DEBUG_APP_WITH_NODE(1, leaderNodeKey,"connect to leader");
	if(!ConnectAndGetEndian(&aSocket, leaderNodeKey,raft::connect::toLeader::newNode,&isEndianDiffer)){goto returnPoint;}	// 1. connect, getEndian and sendRequest
	
	snEndian2 = 1;
	nSndRcv = aSocket.writeC(&snEndian2, 2);																			// 2. send endian
	if (nSndRcv != 2) { 
		DEBUG_APPLICATION(2, "ERROR:");
		goto returnPoint; 
	}

	nSndRcv = aSocket.writeC(&m_nPortOwn, 4);																			// 3. send port number
	if (nSndRcv != 4) { 
		DEBUG_APPLICATION(2, "ERROR:");
		goto returnPoint; 
	}

	nSndRcv = aSocket.readC(&numberOfNodes, 4);																			// 4. rcv number of nodes
	if (nSndRcv != 4) { 
		DEBUG_APPLICATION(2, "ERROR:");
		goto returnPoint; 
	}
	if (isEndianDiffer) { SWAP4BYTES(numberOfNodes); }
	if (numberOfNodes < 1) { 
		if(numberOfNodes== raft::response::error::nodeExist){
			ERROR_LOGGING2("Node with the info similar to this already exist in the cluster, program will exit");
			exit(1);
		}
		goto returnPoint; 
	}

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
		AddAdditionalDataToNode(pNewNode);
		m_Nodes.AddData(pNewNode, &pNodesInfo[i], sizeof(NodeIdentifierKey));
		if(leaderNodeKey== pNodesInfo[i]){
			pNewNode->makeLeader();
			pTools->raftSocket2.SetNewSocketDescriptor(aSocket);
			aSocket.ResetSocketWithoutClose();
			pTools->isEndianDiffer = isEndianDiffer;
			m_pLeaderNode=pNewNode;
		}
		else {
			// let's connect to all nodes and ask permanent raft socket
			// we will not remove any node in the case of error, removing should 
			// be done in the case of leader request
			if(!ConnectAndGetEndian(&pTools->raftSocket2,pNodesInfo[i],raft::connect::toAnyNode::raftBridge,&isEndianDiffer)){
				DEBUG_APPLICATION(2, "ERROR:");
				continue;
			}	// 1. connect, getEndian and sendRequest
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
		if(!ConnectAndGetEndian(&pTools->dataSocket,pNodesInfo[i],raft::connect::toAnyNode::dataBridge,&isEndianDiffer)){
			DEBUG_APPLICATION(2, "ERROR:");
			continue;
		}	// 1. connect, getEndian and sendRequest
		
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
	RaftNode2* pNexNode = m_Nodes.first();
	int nNodesWillVote(0);

	m_pLeaderNode->SetUnableToVote();
	while(pNexNode){
		if(pNexNode->isAbleToVote()){++nNodesWillVote;}
		pNexNode = pNexNode->next;
	}

	DEBUG_APPLICATION(1,"Number of nodes in elections is: %d",nNodesWillVote);

	if(nNodesWillVote<2){ // no node to take part on election, so become leader
		become_leader();
	}
	else{
		RaftServer::become_candidate();
	}

}


/*//////////////////////////////////////////////////////////////////////////////*/

int raft::tcp::Server::SendClbkFunction(void *a_cb_ctx, void *udata, RaftNode2* a_node, int a_msg_type, const unsigned char *send_data,int d_len)
{
	PREPARE_SEND_SOCKET_GUARD();
	//RaftServerTcp* pServer = (RaftServerTcp*)a_cb_ctx;
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
	Server* pServer = (Server*)a_cb_ctx;
	NodeTools* pTools = (NodeTools*)a_node->get_udata();
	NodeIdentifierKey* pNodeKey = (NodeIdentifierKey*)a_node->key;
	int nSndRcv;
	uint32_t unPingCount=0;
	char cRequest=raft::receive::anyNode::clbkCmd;
	bool bProblematic(true);

	switch (a_msg_type)
	{
	case RAFT_MSG_APPENDENTRIES:
		if(a_node->isProblematic()){a_node->makePing(1);}  // make extra ping
		unPingCount = (int)a_node->makePing(1);
		if((unPingCount>MAX_UNANSWERED_PINGS)&& pServer->is_leader()){
			SAddRemData remData;
			remData.action = raft::leaderInternal::removeNode;
			remData.pNode = a_node;
			pServer->m_fifoAddDel.AddElement(remData);
			pServer->m_semaAddRemove.post();
		}
		break;
	case RAFT_MSG_REQUESTVOTE:
		if(a_node->isProblematic()){a_node->makePing(4);}  // make extra ping
		unPingCount = (int)a_node->makePing(1);
		if((unPingCount>MAX_UNANSWERED_PINGS)&& pServer->is_candidate()){
			a_node->SetUnableToVote();
			pServer->become_candidate();
		}
		break;
	default:
		break;
	}

	if(unPingCount>MAX_NUMBER_OF_PINGS){
		DEBUG_APP_WITH_NODE(1, *pNodeKey,"pingCount=%d", unPingCount);
		goto returnPoint;
	}

	LOCK_RAFT_SEND_MUTEX(a_node);
	nSndRcv=pTools->raftSocket2.writeC(&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}
	nSndRcv=pTools->raftSocket2.writeC(&a_msg_type,4);
	if(nSndRcv!=4){goto returnPoint;}
	nSndRcv=pTools->raftSocket2.writeC(send_data, d_len);
	UNLOCK_SEND_SOCKET_MUTEX2();
	if(nSndRcv!= d_len){goto returnPoint;}
	
	bProblematic = false;
returnPoint:
    if(bProblematic){a_node->setProblematic();}
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

    DEBUG_APPLICATION(1,"Interrupt (No:%d)",a_nSigNum);

	s_pRWlockForServers.lock_shared();
    DEBUG_APPLICATION(4,"rd_lock");

    pServer = s_pFirst;
    while(pServer){

		switch (a_nSigNum)
		{
		case SIGABRT:
			DEBUG_APPLICATION(0,"SIGABRT");
			break;
		case SIGFPE:
			DEBUG_APPLICATION(0,"SIGFPE");
			break;
		case SIGILL:
			DEBUG_APPLICATION(0,"SIGILL");
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
			DEBUG_APPLICATION(0,"SIGSEGV");
			break;
		case SIGTERM:
			break;
#if !defined(_WIN32) || defined(_WLAC_USED)
		case SIGPIPE:
			DEBUG_APPLICATION(1,"SIGPIPE");
			break;
#endif
		default:
			DEBUG_APPLICATION(1,"default:");
			break;
		}
		
		pServer->server->SignalHandler(a_nSigNum);
        pServer = pServer->next;
    }

	s_pRWlockForServers.unlock_shared();
    DEBUG_APPLICATION(4,"unlock");
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
