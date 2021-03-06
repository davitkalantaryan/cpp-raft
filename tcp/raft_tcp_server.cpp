
#ifndef HEADER_SIZE_needed
#define HEADER_SIZE_needed
#endif

#ifndef CINTERFACE
#define CINTERFACE
#endif

#include "raft_tcp_server.hpp"
#include <string>
#include <stdint.h>
#include <signal.h>
#include "common/newlockguards.hpp"
#ifndef _WIN32
#include <pthread.h>
#endif
#include <utility>

#define NO_LEADER_KEY_PORT		-1

// rep. rate is the rate of periodic raft job

#ifdef MAKE_LONG_WAIT_DEBUG

#define MIN_REP_RATE_MS					5
#define DEF_REP_RATE_MS					5000
#define	TIMEOUTS_RATIO_MIN				5
#define REPORT_ON_FAULT(_faultyNode)
#define MAX_UNANSWERED_PINGS			10

#else

#define MIN_REP_RATE_MS						5		// les than this rep rate is not possible
#define DEF_REP_RATE_MS						2000	// [ms] default rep rate is 2 seconds
#define	TIMEOUTS_RATIO_MIN					11		// this is minimum ratio for follower between time of leader wait and rep. rate (to start election)
#define MIN_UNSEEN_TIME_TO_DISPLAY			6000	// maximum number of pings that really will be done by leader
#define MAX_UNSEEN_TIME_TO_CHANGE_STATE		20000	// number of virtual pings, after this leader will remove follower

#endif

#if !defined(_WIN32) || defined(_WLAC_USED)
static pthread_t	s_mainThreadHandle = (pthread_t)0;
#else
#include <WinSock2.h>
static HANDLE		s_mainThreadHandle = (HANDLE)0;
static void PAPCFUNC_static(_In_ ULONG_PTR){}
#endif

#ifdef _WIN32
#else
#define closesocket	close
#endif

#define VALIDATE_INDEX_INVALID(_toolPtr,_index) ((_index)<NUMBER_OF_TOOLS_SOCKETS)
#define LOCK_RAFT_SEND_MUTEX2(_node)		LOCK_SEND_SOCKET_MUTEX2((_node),raft::tcp::socketTypes::raft)
#define LOCK_RAFT_RCV_MUTEX2(_node)			LOCK_RCV_SOCKET_MUTEX2((_node),raft::tcp::socketTypes::raft)

struct ServersList { raft::tcp::Server *server; ServersList *prev, *next; };
static struct ServersList* s_pFirst = NULL;
static STDN::shared_mutex s_pRWlockForServers;

//static void SigActionFunction (int, siginfo_t *, void *); last 2 arguments are not used
static void AddNewRaftServer(raft::tcp::Server* a_pServer);
static void RemoveRaftServer(raft::tcp::Server* a_pServer);

namespace raft{namespace tcp{
int g_nApplicationRun = 0;
}}

static int CreateEmptySocket();

//#define FOLLOWER_SEES_ERR(...)	SleepMs(2000)

raft::tcp::Server::Server()
	:
	m_nWork(0),
	m_nPeriodForPeriodic(DEF_REP_RATE_MS),
	m_nPortOwn(0),
	m_pLeaderNode(NULL)
{
	int32_t i;
	raft_cbs_t aClbks;

	m_nWork = 0;
	for (i=0; i<NUMBER_OF_TOOLS_SOCKETS; ++i) {
		m_intrptSocketForRcv[i] = -1;
	}
	m_isInited = 0;

	this->election_timeout = MAX_UNSEEN_TIME_TO_CHANGE_STATE;

#ifdef _WIN32
	m_periodicThreadId = (HANDLE)0;
#else
	m_periodicThreadId = (pthread_t)0;

	for(i=0; i<NUMBER_OF_TOOLS_SOCKETS; ++i){
		m_rcvThreadIds[i]= (pthread_t)0;
	}

#endif

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

	s_mainThreadHandle = pthread_self();

	sigaction(SIGPIPE, &newAction, NULL);
#ifdef _USE_LOG_FILES
    sigaction(SIGTSTP, &newAction, NULL);
#endif
#else
	s_mainThreadHandle = GetCurrentThread();
#endif

	//sigaction(SIGSEGV, &newAction, NULL_ACTION);
	sigaction(SIGABRT, &newAction, NULL_ACTION);
	sigaction(SIGFPE, &newAction, NULL_ACTION);
	sigaction(SIGILL, &newAction, NULL_ACTION);
	sigaction(SIGINT, &newAction, NULL_ACTION);
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


void raft::tcp::Server::AddAdditionalDataToNode(RaftNode2* a_pNode, std::string* a_pDataFromAdder, bool a_bIsAdder, bool a_bIsThis)
{
	NodeTools* pTools = new NodeTools;
	HANDLE_MEM_DEF2(pTools," ");
	RaftServer::AddAdditionalDataToNode(a_pNode, a_pDataFromAdder, a_bIsAdder, a_bIsThis);
	a_pNode->set_udata(pTools);
}


void raft::tcp::Server::CleanNodeData(RaftNode2* a_pNode, std::string* a_pDataFromLeader)
{
	NodeTools* pNodeTool = GET_NODE_TOOLS(a_pNode);

	if(pNodeTool){
		delete pNodeTool;
		a_pNode->set_udata(NULL);
	}
	RaftServer::CleanNodeData(a_pNode, a_pDataFromLeader);
}


void raft::tcp::Server::FindClusterAndInit(const std::vector<NodeIdentifierKey>& a_vectPossibleNodes, std::string* a_extraDataForAndFromAdder,int a_nRaftPort)
{
	if(a_nRaftPort>0){m_nPortOwn = a_nRaftPort;} // otherwise child class inited m_nPortOwn
	if(m_nPeriodForPeriodic<MIN_REP_RATE_MS){ m_nPeriodForPeriodic = DEF_REP_RATE_MS;}
	if(this->request_timeout < m_nPeriodForPeriodic) { this->request_timeout = m_nPeriodForPeriodic;}
	if(this->election_timeout < (TIMEOUTS_RATIO_MIN*this->request_timeout)) { this->election_timeout =(TIMEOUTS_RATIO_MIN*this->request_timeout);}
	this->timeout_elapsed = 0;

	CheckAllPossibleSeeds(a_vectPossibleNodes, a_extraDataForAndFromAdder);
}


int raft::tcp::Server::RunServerOnOtherThreads2(const std::vector<NodeIdentifierKey>& a_vectPossibleNodes, int a_nWorkersCount, int a_nRaftPort)
{
	std::string strExtraDataToAndFromAdder;
	FindClusterAndInit(a_vectPossibleNodes, &strExtraDataToAndFromAdder,a_nRaftPort);
	RunAllThreadPrivate(a_nWorkersCount);
	return 0;
}


void raft::tcp::Server::RunAllThreadPrivate(int32_t a_nWorkersCount)
{
	std::thread* pWorker;
	int32_t i;

	if (m_nWork) {return;}
	m_nWork = 1;

	for (i = 0; i<NUMBER_OF_TOOLS_SOCKETS; ++i) {
		m_intrptSocketForRcv[i] = CreateEmptySocket();
	}

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

	m_threadFixDoubleCycles = STDN::thread(&Server::ThreadFunctionFindOtherChains, this);
	m_threadTcpListen = STDN::thread(&Server::ThreadFunctionListen,this);
	m_threadPeriodic = STDN::thread(&Server::ThreadFunctionPeriodic, this);
	for (i = 0; i < NUMBER_OF_TOOLS_SOCKETS;++i){
		m_threadsReceives[i] = STDN::thread(&Server::ThreadFunctionRcvFromSocket,this,i);
	}
	for(i=0;i<a_nWorkersCount;++i){
		pWorker = new STDN::thread(&Server::ThreadFunctionWorker, this);
		m_vectThreadsWorkers.push_back(pWorker);
	}

#if !defined(_WIN32) || defined(_WLAC_USED)
	sigaction(SIGINT, &oldAction, NULL_ACTION);
#endif

	time(&m_serverStartTime);
}


void raft::tcp::Server::StopServer()
{
	size_t i,nThreadsCount;

	if (m_nWork == 0) {return;}
	DEBUG_APPLICATION(1, "Stopping server");
	m_nWork = 0;

	InterruptPeriodicThread();
	for (i=0; i<NUMBER_OF_TOOLS_SOCKETS; ++i) {
		closesocket(m_intrptSocketForRcv[i]);
		m_intrptSocketForRcv[i] = -1;
#ifdef _WIN32
		// something with windows?
#else
		if (m_rcvThreadIds[i]) { pthread_kill(m_rcvThreadIds[i], SIGPIPE); }
#endif
	}
	m_serverTcp.StopServer();

	m_semaForSolvingDublicates2.post();

	nThreadsCount = m_vectThreadsOtherPeriodic.size();
	for (i = 0; i<nThreadsCount; ++i) {
		m_vectThreadsOtherPeriodic[i]->join();
		delete m_vectThreadsOtherPeriodic[i];
	}
	m_vectThreadsOtherPeriodic.clear();

	nThreadsCount = m_vectThreadsWorkers.size();
	for(i=0;i<nThreadsCount;++i){
		m_semaWorker.post();
	}
	for(i=0;i<nThreadsCount;++i){
		m_vectThreadsWorkers[i]->join();
		delete m_vectThreadsWorkers[i];
	}
	m_vectThreadsWorkers.clear();

    DEBUG_APPLICATION(2," ");

	for (i = 0; i < NUMBER_OF_TOOLS_SOCKETS;++i){
		m_threadsReceives[i].join();
	}
    
	DEBUG_APPLICATION(2," ");
	m_threadPeriodic.join();
    DEBUG_APPLICATION(2," ");
	m_threadTcpListen.join();
    DEBUG_APPLICATION(2," ");

	m_threadFixDoubleCycles.join();

}


void raft::tcp::Server::AddClient(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)		// 1. connect
{
	int nSocketDescr;

	nSocketDescr = (int)a_clientSock;
	a_clientSock.ResetSocketWithoutClose();  // let us assume, that worker will close in the case of necessirty
	AddConnectForWorker(a_remoteAddr,nSocketDescr);

}


bool raft::tcp::Server::raft_connect_toAnyNode_newNode(common::SocketTCP& a_clientSock, SWorkerData* a_pData, uint32_t a_isEndianDiffer)
{
	// struct NodeIdentifierKey { char ip4Address[MAX_IP4_LEN]; int32_t port;};
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
    NodeIdentifierKey	*pAllNodesInfo=NULL;
	int nSndRcv;
	int32_t nTotalSize, nNodesCount, nLeaderIndex;
	bool bOk(false);

	a_pData->nodeKey.set_ip4Address2(&a_pData->pear.con.remAddress);

	// 1. receive remote network port
	nSndRcv= a_clientSock.readC(&a_pData->nodeKey.port,4);
	if(nSndRcv!= 4){ goto returnPoint;}
	if(a_isEndianDiffer){ SWAP4BYTES(a_pData->nodeKey.port);}
	
	nNodesCount = this->nodesCount();
	pAllNodesInfo=CollectAllNodesDataNotThrSafe(&nTotalSize,&nLeaderIndex);

	// 2. Send index of leader 
	nSndRcv = a_clientSock.writeC(&nLeaderIndex, 4);
	if (nSndRcv != 4) { goto returnPoint; }

	// 3. Send nodes count
	nSndRcv = a_clientSock.writeC(&nNodesCount, 4);
	if (nSndRcv != 4) { goto returnPoint; }

	// 4. send info on all nodes
	nSndRcv = a_clientSock.writeC(pAllNodesInfo, nTotalSize);
	if (nSndRcv != nTotalSize) { goto returnPoint; }
	free(pAllNodesInfo); pAllNodesInfo = NULL;

	// 5. receive extra data from new node
	//    meanwhile this is a confirmation that new node has already added all existing nodes
	bOk = ReceiveExtraData(a_clientSock,a_isEndianDiffer, &a_pData->extraStr());

returnPoint:
	if (!bOk) {
		free(pAllNodesInfo);
	}
	return bOk;
}


raft::tcp::NodeIdentifierKey* raft::tcp::Server::CollectAllNodesDataNotThrSafe(int32_t* a_pnTotalSize, int32_t* a_pnLeaderIndex)
{
	RaftNode2* pNode;
	NodeIdentifierKey *pAllNodesInfo, *pExistingNodeKey;
	int32_t i(0);
	
	*a_pnTotalSize = nodesCount() * sizeof(NodeIdentifierKey);
	pAllNodesInfo = (NodeIdentifierKey*)malloc(*a_pnTotalSize);
	HANDLE_MEM_DEF2(pAllNodesInfo, " ");
	if(a_pnLeaderIndex){*a_pnLeaderIndex=-1;}

	// collect info
	pNode = firstNode();
	while (pNode) {
		pExistingNodeKey = NODE_KEY(pNode);
		memcpy(&pAllNodesInfo[i], pExistingNodeKey,sizeof(NodeIdentifierKey));
		if(pNode->is_leader() && a_pnLeaderIndex){*a_pnLeaderIndex = i;}
		pNode = pNode->next;
		++i;
	}

	return pAllNodesInfo;
}


void raft::tcp::Server::raft_connect_fromClient_ping(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)
{
	char vcHostName[64];
	DEBUG_APPLICATION(1, "ping from %s(%s)", ::common::socketN::GetHostName(a_remoteAddr, vcHostName, 63), ::common::socketN::GetIPAddress(a_remoteAddr));
}


void raft::tcp::Server::raft_connect_fromClient_startTime(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)
{
	uint32_t unStartTime = (uint32_t)this->m_serverStartTime;
	char vcHostName[64];

	a_clientSock.writeC(&unStartTime, 4);
	DEBUG_APPLICATION(1, "startTime request from %s(%s)", ::common::socketN::GetHostName(a_remoteAddr, vcHostName, 63), ::common::socketN::GetIPAddress(a_remoteAddr));
}


void raft::tcp::Server::raft_connect_fromClient_allNodesInfo(common::SocketTCP& a_clientSock)
{
	struct { int nodesCount, leaderIndex; }nl;
	NodeIdentifierKey *pAllNodesInfo;
	int nSndRcv,nTotalSize;

	nl.nodesCount = nodesCount();
	pAllNodesInfo = CollectAllNodesDataNotThrSafe(&nTotalSize,&nl.leaderIndex);

	if(!pAllNodesInfo){return;}

	nSndRcv=a_clientSock.writeC(&nl,8);
	if(nSndRcv!=8){free(pAllNodesInfo);return;}

	a_clientSock.writeC(pAllNodesInfo,nTotalSize);
	free(pAllNodesInfo);
}


void raft::tcp::Server::raft_connect_toAnyNode_otherLeaderFound(common::SocketTCP& a_clientSock, uint32_t a_isEndianDiffer)
{
	NodeIdentifierKey newLeaderKey;
	int nSndRcv;

	nSndRcv=a_clientSock.readC(&newLeaderKey,sizeof(NodeIdentifierKey));
	if(nSndRcv!=sizeof(NodeIdentifierKey)){return;}
	if(a_isEndianDiffer){SWAP4BYTES(newLeaderKey.port);}

	if( *NODE_KEY(m_pLeaderNode) == newLeaderKey ){
		a_clientSock.writeC("o", 1);
	}
	else {
		a_clientSock.writeC("e", 1);
		DEBUG_APP_WITH_NODE(0, &newLeaderKey, " [this is a correct leader]");
		// todo: make steps to connect to real leader
		// and distribute throw all cluster new node info
	}
}


void raft::tcp::Server::raft_connect_toAnyNode_permanentBridge(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr, uint32_t a_isEndianDiffer)  // 0->raft
{
	// this socket should remain 
	RaftNode2* pNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey aRemHost;
	int nSndRcv;
	int32_t nIndex;

	aRemHost.set_ip4Address2(a_remoteAddr);	// let us specify host IP

	nSndRcv = a_clientSock.readC(&aRemHost.port,4);							// port
	if(nSndRcv!=4){
		ERROR_LOGGING2("Unable to get port from host %s", common::socketN::GetIPAddress(a_remoteAddr));
		return;
	}
	if(a_isEndianDiffer){SWAP4BYTES(aRemHost.port);}

	if(!this->FindNode(&aRemHost,sizeof(NodeIdentifierKey),&pNode)){
		ERROR_LOGGING2("Node (%s:%d) is not found", common::socketN::GetIPAddress(a_remoteAddr),(int)aRemHost.port);
		return;
	}
	pNodeTools = GET_NODE_TOOLS(pNode);
	pNodeTools->isEndianDiffer = a_isEndianDiffer;

	nSndRcv = a_clientSock.readC(&nIndex, 4);							// port
	if (nSndRcv != 4) {
		ERROR_LOGGING2("Unable to get index of bridge of node (%s:%d)", aRemHost.ip4Address,(int)aRemHost.port);
		return;
	}
	if (a_isEndianDiffer) { SWAP4BYTES(nIndex); }
	
	pNodeTools->setSocket(nIndex,(int)a_clientSock);
	a_clientSock.ResetSocketWithoutClose();

	InterruptReceivercThread(nIndex);
}


void raft::tcp::Server::ThreadFunctionFindOtherChains()
{
	common::NewSharedLockGuard<NewSharedMutex> aShrdGuard;
	RaftNode2* pNode;
	NodeIdentifierKey* pNodeKey;
	common::SocketTCP aSocket;
	size_t i, nPossibleNodesCount;
	int nSndRcv;
	uint32_t isEndianDiffer;
	bool bFound;
	char cRequest;

	
enterTryPoint:
	try {
		while (m_nWork) {

			m_semaForSolvingDublicates2.wait();

			nPossibleNodesCount = m_allPossibleNodes.size();
			for (i = 0; i<nPossibleNodesCount; ++i) {

				bFound = false;
				aShrdGuard.SetAndLockMutex (&m_shrdMutexForNodes2);
				pNode = this->firstNode();

				while (pNode) {
					pNodeKey = NODE_KEY(pNode);
					if ((*(pNodeKey)) == m_allPossibleNodes[i]) {
						bFound = true;
						break;
					}
					pNode = pNode->next;
				}

				aShrdGuard.UnsetAndUnlockMutex();

				if (!bFound) {
					if (!ConnectAndGetEndian2(&aSocket, m_allPossibleNodes[i], raft::connect::toAnyNode2::otherLeaderFound, &isEndianDiffer)) { continue; }

					nSndRcv = aSocket.writeC(m_pLeaderNode->key, sizeof(NodeIdentifierKey));
					if (nSndRcv != sizeof(NodeIdentifierKey)) { goto socketClosePoint; }

					nSndRcv = aSocket.readC(&cRequest, 1);
					if ((nSndRcv == 1) && (cRequest == 'e')) {
						DEBUG_APP_WITH_NODE(0, &m_allPossibleNodes[i], " [possible secondary leader (informed)]");
					}

				socketClosePoint:
					aSocket.closeC();
				}

				SleepMs(1000);

			}  // for (i = 0; i<nPossibleNodesCount; ++i) {
		}  // while(m_nWork){
	}
	catch(...){
		aShrdGuard.UnsetAndUnlockMutex();
		goto enterTryPoint;
	}

}


void raft::tcp::Server::ThreadFunctionListen()
{
	m_serverTcp.setTimeout(SOCK_TIMEOUT_MS);
	m_serverTcp.StartServer(this, &raft::tcp::Server::AddClient,m_nPortOwn);
}


void raft::tcp::Server::HandleSeedClbk(RaftNode2* a_pNode)
{
	NodeTools* pTools = GET_NODE_TOOLS(a_pNode);
	NodeIdentifierKey* pNodeKey = NODE_KEY(a_pNode);
	int nSndRcv, nToReceive;
	int msg_type;
	bool bPingReceived(false);

	nSndRcv = pTools->readC(raft::tcp::socketTypes::raft,&msg_type, 4);
	if (nSndRcv != 4) { goto returnPoint; }

	switch (msg_type) 
	{
	case RAFT_MSG_REQUESTVOTE:
	{
		msg_requestvote_t reqVote(0,0,0,0);
		DEBUG_APP_WITH_NODE(1,pNodeKey,"RAFT_MSG_REQUESTVOTE");
		if ((this->election_timeout/2) <= this->timeout_elapsed){
			nSndRcv = pTools->readC(raft::tcp::socketTypes::raft,&reqVote, sizeof(msg_requestvote_t));
			if (nSndRcv != sizeof(msg_requestvote_t)) { goto returnPoint; }
			recv_requestvote(a_pNode, &reqVote);
		}
		bPingReceived = true;
	}
	break;
	case RAFT_MSG_REQUESTVOTE_RESPONSE:
	{
		msg_requestvote_response_t  reqVoteResp;
		DEBUG_APP_WITH_NODE(1,pNodeKey,"RAFT_MSG_REQUESTVOTE_RESPONSE");
		nSndRcv = pTools->readC(raft::tcp::socketTypes::raft,&reqVoteResp, sizeof(msg_requestvote_response_t));
		if (nSndRcv != sizeof(msg_requestvote_response_t)) { goto returnPoint; }
		a_pNode->pingReceived();
		recv_requestvote_response(a_pNode, &reqVoteResp);
		bPingReceived = true;
	}
	break;
	case RAFT_MSG_APPENDENTRIES:
	{
		MsgAppendEntries2 appEntries;
		DEBUG_APP_WITH_NODE(3,pNodeKey,"RAFT_MSG_APPENDENTRIES");
		nSndRcv = pTools->readC(raft::tcp::socketTypes::raft,&appEntries, SIZE_OF_INITIAL_RCV_OF_MSG_APP);
		if (nSndRcv != SIZE_OF_INITIAL_RCV_OF_MSG_APP) { goto returnPoint; }
		if(appEntries.getNEntries()){
			nToReceive = appEntries.getNEntries() * sizeof(msg_entry_t);
			nSndRcv=pTools->readC(raft::tcp::socketTypes::raft,appEntries.entries(),nToReceive);
			if (nSndRcv != nToReceive) { goto returnPoint; }
		}
		recv_appendentries(true,a_pNode, &appEntries);
		ftime(&(this->m_lastPingByLeader));
		bPingReceived = true;
	}
	break;
	case RAFT_MSG_APPENDENTRIES_RESPONSE:
	{
		msg_appendentries_response_t aApndResp;
		DEBUG_APP_WITH_NODE(3,pNodeKey,"RAFT_MSG_APPENDENTRIES_RESPONSE");
		nSndRcv = pTools->readC(raft::tcp::socketTypes::raft,&aApndResp, sizeof(msg_appendentries_response_t));
		if (nSndRcv != sizeof(msg_appendentries_response_t)) { goto returnPoint; }
		a_pNode->pingReceived();
		this->recv_appendentries_response(a_pNode, &aApndResp);
		bPingReceived = true;
	}
	break;
	default:
		DEBUG_APP_WITH_NODE(0,pNodeKey,"raft-receive: default:");
		goto returnPoint;
	}


returnPoint:
	if (bPingReceived) { a_pNode->pingReceived(); }
}


bool raft::tcp::Server::raft_receive_fromLeader_removeNode(SWorkerData* a_pWorkerData)
{
	NodeTools *pTools = GET_NODE_TOOLS(a_pWorkerData->pNode);
	int nSndRcv ;
	int32_t nExtraDataLen;

	if (a_pWorkerData->pNode != m_pLeaderNode) {
		ERROR_LOGGING2(
			"node (%s:%d) is not leader, but tries to confirm leader action", 
			NODE_KEY(a_pWorkerData->pNode)->ip4Address, (int)NODE_KEY(a_pWorkerData->pNode)->port);
		return false;
	}
	if (!is_follower()) {
		DEBUG_APP_WITH_NODE(0,NODE_KEY(a_pWorkerData->pNode),"own node is not follower, but request is for follower");
		return false;
	}

	nSndRcv = pTools->readC(raft::tcp::socketTypes::raft, &a_pWorkerData->nodeKey, sizeof(NodeIdentifierKey));
	if (nSndRcv != sizeof(NodeIdentifierKey)) {
		return false; 
	}
	if (pTools->isEndianDiffer) { SWAP4BYTES(a_pWorkerData->nodeKey.port); }

	nSndRcv = GET_NODE_TOOLS(a_pWorkerData->pNode)->readC(raft::tcp::socketTypes::raft,&nExtraDataLen, 4);
	if (nSndRcv != 4) { return false; }

	if (nExtraDataLen>0) {
		a_pWorkerData->resize(nExtraDataLen);
		nSndRcv = GET_NODE_TOOLS(a_pWorkerData->pNode)->readC(raft::tcp::socketTypes::raft, a_pWorkerData->buffer(), nExtraDataLen);
		if (nSndRcv != nExtraDataLen) { return false; }
	}

	return true;
}


bool raft::tcp::Server::raft_receive_fromAdder_newNode(SWorkerData* a_pData)
{
	RaftNode2* pNode;
	NodeTools* pNodeTools = GET_NODE_TOOLS(a_pData->pNode);
	common::SocketTCP* pRaftSocket = pNodeTools->socketPtr(raft::tcp::socketTypes::raft);
	int nSndRcv;

	nSndRcv = pNodeTools->readC(raft::tcp::socketTypes::raft, &a_pData->nodeKey, sizeof(NodeIdentifierKey));
	if (nSndRcv != sizeof(NodeIdentifierKey)) {
		ERROR_LOGGING2("is not able to get new node data from node (%s:%d) ", NODE_KEY(a_pData->pNode)->ip4Address, (int)NODE_KEY(a_pData->pNode)->port);
		return false;
	}
	if (pNodeTools->isEndianDiffer) { SWAP4BYTES(a_pData->nodeKey.port); }

	if (FindNode(&a_pData->nodeKey, sizeof(NodeIdentifierKey), &pNode)) {
		ERROR_LOGGING2(
			"Node (%s:%d) tries to add already existing (%s:%d) node",
			NODE_KEY(a_pData->pNode)->ip4Address, (int)NODE_KEY(a_pData->pNode)->port,
			a_pData->nodeKey.ip4Address, (int)a_pData->nodeKey.port);
		return false;
	}

	return ReceiveExtraData(*pRaftSocket,pNodeTools->isEndianDiffer, &a_pData->extraStr());

}


void raft::tcp::Server::HandleNewConnectionPrivate(SWorkerData* a_pWorkerData)
{
	RaftNode2* pNewNode = NULL;
	common::SocketTCP aClientSock;
	common::NewSharedLockGuard<NewSharedMutex> aSharedGuard;
	common::NewLockGuard<NewSharedMutex> aGuard;
	int nSndRcv;
	uint16_t snEndian;
	char vcHostName[MAX_HOSTNAME_LENGTH];
	uint32_t unRequestFromClient;
	uint32_t isEndianDiffer;

	DEBUG_APPLICATION(1,
		"conntion from %s(%s)",
		common::socketN::GetHostName(&a_pWorkerData->pear.con.remAddress, vcHostName, MAX_HOSTNAME_LENGTH),
		common::socketN::GetIPAddress(&a_pWorkerData->pear.con.remAddress));
	aClientSock.SetNewSocketDescriptor(a_pWorkerData->pear.con.sockDescriptor);
	aClientSock.setTimeout(SOCK_TIMEOUT_MS);

	snEndian = 1;
	nSndRcv = aClientSock.writeC(&snEndian, 2);																// 2. Send endian				
	if (nSndRcv != 2) {
		DEBUG_APPLICATION(1, "Could not send the endian of the connected pear (%s) nSndRcv=%d, socket=%d",::common::socketN::GetIPAddress(&a_pWorkerData->pear.con.remAddress), nSndRcv, a_pWorkerData->pear.con.sockDescriptor);
		aClientSock.closeC();
		return;
	}

	nSndRcv = aClientSock.readC(&unRequestFromClient, 4);																// 4. rcv request
	if (nSndRcv != 4) {
		DEBUG_APPLICATION(1, "Unable to read request type");
		aClientSock.closeC();
		return;
	}

	isEndianDiffer = IS_ENDIAN_DIFFER_AND_REQUEST(unRequestFromClient);

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	if(!handleNewConnectionBeforeLock(unRequestFromClient, aClientSock, a_pWorkerData, isEndianDiffer)){return;}
	aSharedGuard.UnsetAndUnlockMutex();

	aGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	pNewNode = handleNewConnectionLocked(aClientSock, a_pWorkerData->pear.con.remAddress, unRequestFromClient, &a_pWorkerData->nodeKey, &a_pWorkerData->extraStr(), isEndianDiffer);
	aGuard.UnsetAndUnlockMutex();

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	handleNewConnectionAfterLock(aClientSock, a_pWorkerData->pear.con.remAddress, unRequestFromClient, &a_pWorkerData->nodeKey, &a_pWorkerData->extraStr(), pNewNode, isEndianDiffer);
	aSharedGuard.UnsetAndUnlockMutex();

}


void raft::tcp::Server::HandleReceiveFromNodePrivate(char a_cRequest,RaftNode2* a_pNode, int32_t a_index, NodeIdentifierKey* a_pNodeKey, std::string* a_bBufferForReceive)
{
	common::NewSharedLockGuard<NewSharedMutex> aSharedGuard;
	common::NewLockGuard<NewSharedMutex> aGuard;

	aGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	if (a_pNode) { a_pNode->decrementLock2(); }
	if(!handleReceiveFromNodeLocked(a_cRequest,a_pNode, a_index,a_pNodeKey,a_bBufferForReceive)){return;}
	aGuard.UnsetAndUnlockMutex();

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	handleReceiveFromNodeAfterLock(a_cRequest,a_pNode, a_index,a_pNodeKey,a_bBufferForReceive);
	aSharedGuard.UnsetAndUnlockMutex();
}


void raft::tcp::Server::HandleInternalPrivate(char a_cRequest, RaftNode2* a_pNode, NodeIdentifierKey* a_pNodeKey, std::string* a_pBufferForReceive)
{
	common::NewSharedLockGuard<NewSharedMutex> aSharedGuard;
	common::NewLockGuard<NewSharedMutex> aGuard;

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	if(!handleInternalBeforeLock(a_cRequest,a_pNode,a_pNodeKey,a_pBufferForReceive)){
		if (a_pNode) { a_pNode->decrementLock2(); }
		return;
	}
	aSharedGuard.UnsetAndUnlockMutex();

	aGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	if (a_pNode) { a_pNode->decrementLock2(); }
	if(!handleInternalLocked(a_cRequest,a_pNode,a_pNodeKey,a_pBufferForReceive)){return;}
	aGuard.UnsetAndUnlockMutex();

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	handleInternalAfterLock(a_cRequest, a_pNode, a_pNodeKey, a_pBufferForReceive);
	aSharedGuard.UnsetAndUnlockMutex();

}



void raft::tcp::Server::ReceiveFromSocketAndInform(RaftNode2* a_pNode, int32_t a_index)
{
	SWorkerData aJobData(workRequest::handleReceive);
	int nSndRcv;

	if(a_pNode->isMarkedForDeletion()){return;}

	if (!VALIDATE_INDEX_INVALID(GET_NODE_TOOLS(a_pNode), a_index)) {
		ERROR_LOGGING2("Wrong index provided");
		return;
	}

	nSndRcv = GET_NODE_TOOLS(a_pNode)->readC(a_index, &aJobData.pear.rcv.cRequest, 1);
	if (nSndRcv != 1) { 
		if(!a_pNode->pingCount()){ERROR_LOGGING2("Unable to receive from the node (%s:%d)",NODE_KEY(a_pNode)->ip4Address,(int)NODE_KEY(a_pNode)->port);}
		return; 
	}

	aJobData.pNode = a_pNode;
	aJobData.pear.rcv.index = a_index;
	if (!handleReceiveFromNodeBeforeLockRcvContext(a_index, &aJobData)) { return; }
	
	a_pNode->incrementLock2();
	m_fifoWorker.AddElement2(std::move(aJobData));
	m_semaWorker.post();

}


bool raft::tcp::Server::ReceiveExtraData(common::SocketTCP& a_socket, uint32_t a_isEndianDiffer, std::string* a_pData)
{
	int nAddDataLen;
	int nSndRcv = a_socket.readC(&nAddDataLen, 4);
	if (nSndRcv != 4) { return false; }
	if (a_isEndianDiffer) { SWAP4BYTES(nAddDataLen); }

	if (nAddDataLen>0) {
		a_pData->resize(nAddDataLen);
		nSndRcv = a_socket.readC(const_cast<char*>(a_pData->data()),nAddDataLen);
        if (nSndRcv != nAddDataLen) { return false; }
	}
	else { a_pData->resize(0); }
	return true;
}


void raft::tcp::Server::SendInformationToAllNodes(int32_t a_index, char a_cRequest, const std::string* a_extraData, const NodeIdentifierKey* a_pNodeKey, RaftNode2* a_pNodeToSkip, bool a_bWait)
{
	RaftNode2* pNode = firstNode();

	while(pNode){
		if((pNode!=a_pNodeToSkip)&&(pNode!=m_thisNode)){
			SendInformationToNode(pNode, a_index, a_cRequest, a_extraData, a_pNodeKey);
		}
		pNode = pNode->next;
	}

	if(a_bWait){
		int64_t okCount;
		int nIteration;
		while (pNode) {
			if ((pNode != a_pNodeToSkip) && (pNode != m_thisNode) && (pNode->okCount()>=0)) {
				okCount = pNode->okCount();
				SendInformationToNode(pNode, a_index, a_cRequest, a_extraData, a_pNodeKey);
			}
			pNode = pNode->next;

			for (nIteration=0;(okCount==pNode->okCount())&&(nIteration<500);++nIteration) {
				SleepMs(10);
			}
		}
	} // if(a_bWait){
}


bool raft::tcp::Server::SendInformationToNode(RaftNode2* a_pNode, int32_t a_index, char a_cRequest, const std::string* a_extraData, const NodeIdentifierKey* a_pNodeKey)
{
	PREPARE_SOCKET_GUARD();
	int32_t nExtraDataLen;
	int nSndRcv;
	bool bRet(false);

	LOCK_SEND_SOCKET_MUTEX(a_pNode,a_index);
	nSndRcv = GET_NODE_TOOLS(a_pNode)->writeC(a_index,a_pNode,this,&a_cRequest, 1);
	if (nSndRcv != 1) { goto nextNodePoint; }
	if(a_pNodeKey){
		nSndRcv = GET_NODE_TOOLS(a_pNode)->writeC(a_index,a_pNode,this,a_pNodeKey,sizeof(NodeIdentifierKey));
		if (nSndRcv != sizeof(NodeIdentifierKey)) { goto nextNodePoint; }
	}
	if(a_extraData){
		nExtraDataLen = (int32_t)a_extraData->size();
		nSndRcv = GET_NODE_TOOLS(a_pNode)->writeC(a_index, a_pNode, this, &nExtraDataLen, 4);
		if (nSndRcv != 4) { goto nextNodePoint; }
		if(nExtraDataLen>0){
			nSndRcv = GET_NODE_TOOLS(a_pNode)->writeC(a_index, a_pNode, this, a_extraData->data(),nExtraDataLen);
			if (nSndRcv != nExtraDataLen) { goto nextNodePoint; }
		}
	}

	bRet = true;
nextNodePoint:
	UNLOCK_SOCKET_MUTEX();
	return bRet;
}


void raft::tcp::Server::FunctionForMultiRcv(void (Server::*a_fpRcvFnc)(RaftNode2*,int32_t), int32_t a_index)
{
	NodeTools* pNodeTools;
	RaftNode2* pNode;
	common::NewSharedLockGuard<NewSharedMutex> aShrdLockGuard;
	fd_set rFds, eFds;
	int nCountInFd,nMax, nCurrentSocket, nSelectReturn, nSocketsFound, nSocketToIgnore=-1, nLastSocket;
    volatile int& nSocketForInfo2 = m_intrptSocketForRcv[a_index];

enterLoopPoint:
	try{

		while (m_nWork) {
			FD_ZERO(&rFds); FD_ZERO(&eFds);
			aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);  // --> locking
			nCountInFd = 0;
			if (nSocketForInfo2 > 0) {
				nMax = nSocketForInfo2;
#ifdef _WIN32
				FD_SET(nSocketForInfo2, &rFds);
#else
				FD_SET(nSocketForInfo2, &eFds);
#endif
				++nCountInFd;
			}
			pNode = firstNode();
			nLastSocket = -1;
			while (pNode) {
				if (pNode != m_thisNode) {
					pNodeTools = GET_NODE_TOOLS(pNode);
					nCurrentSocket = pNodeTools->getSocketNum(a_index);
					if ((nCurrentSocket>0) && (nCurrentSocket != nSocketToIgnore)) {
						FD_SET(nCurrentSocket, &rFds);
						FD_SET(nCurrentSocket, &eFds);
						++nCountInFd;
						if (nCurrentSocket>nMax) { nMax = nCurrentSocket; }
					}  // if(pNodeTools->socket>0){
				}  // if(pNode!=m_thisNode){
				pNode = pNode->next;
			}// while(pNode){
			aShrdLockGuard.UnsetAndUnlockMutex();  // --> unlocking

			if(nCountInFd<1){
				SleepMs(100);
				goto enterLoopPoint;
			}

			nSelectReturn = ::select(++nMax, &rFds, NULL, &eFds, NULL);
			if (!m_nWork) { break; }
			if (nSelectReturn < 0) {
				DEBUG_APPLICATION(2, "Select returned negative value");
				//SleepMs(2000);  // most probably last client was problematic
				nSocketToIgnore = nLastSocket;
			} // will never happen (or?)
			else { nSocketToIgnore = -1; }
			nSocketsFound = 0;
			aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);  // --> locking

			pNode = firstNode();
			while (pNode && (nSocketsFound<nSelectReturn)) {
				if (pNode != m_thisNode) {
					pNodeTools = GET_NODE_TOOLS(pNode);
					nCurrentSocket = pNodeTools->getSocketNum(a_index);
					if ((nCurrentSocket>0) && FD_ISSET(nCurrentSocket, &rFds)) {
						// call receive ...
						nLastSocket = nCurrentSocket;
						(this->*a_fpRcvFnc)(pNode, a_index);
						++nSocketsFound;
					}  // if(pNodeTools->socket>0){
					if ((nCurrentSocket>0) && FD_ISSET(nCurrentSocket, &eFds)) {
						//pNode->setProblematic();
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

}


void raft::tcp::Server::ThreadFunctionRcvFromSocket(int32_t a_index)
{
#ifndef _WIN32
	m_rcvThreadIds[a_index] = pthread_self();
#endif
	FunctionForMultiRcv(&raft::tcp::Server::ReceiveFromSocketAndInform, a_index);
}


/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
bool raft::tcp::Server::handleInternalBeforeLock(char, RaftNode2*, NodeIdentifierKey*, std::string*)
{
	return true;
}


bool raft::tcp::Server::handleInternalLocked(char a_cRequest, RaftNode2* a_pNode, NodeIdentifierKey* a_pNodeKey, std::string* a_pBufferToSendToOthers)
{
	RaftNode2* pNode(NULL);
	switch (a_cRequest)
	{
	case raft::internal2::leader::removeNode:
		if (!is_leader()) {
			POSSIBLE_BUG("not a leader but trying to do as leader");
			return false;
		}
		a_pNodeKey->set_ip4Address1(NODE_KEY(a_pNode)->ip4Address);
		a_pNodeKey->port = NODE_KEY(a_pNode)->port;
		this->RemoveNode2(a_pNode, a_pBufferToSendToOthers);
		DEBUG_APPLICATION(1,"Node (%s:%d) removed. NumberOfNodes=%d",a_pNodeKey->ip4Address,(int)a_pNodeKey->port,(int)nodesCount());
		break;
	case raft::internal2::newLeader::becomeLeader:
		if (!is_candidate()) {
			POSSIBLE_BUG("not a candidate but trying to act as candidate");
			return false;
		}
		pNode = m_pLeaderNode;
		DEBUG_APPLICATION(1, "Node (%s:%d) (old leader) removed. NumberOfNodes=%d", NODE_KEY(pNode)->ip4Address, (int)NODE_KEY(pNode)->port, (int)(nodesCount()-1));
		m_pLeaderNode = m_thisNode;
		m_pLeaderNode->makeLeader();
		RaftServer::become_leader();
		this->RemoveNode2(pNode, a_pBufferToSendToOthers);
		break;
	default:
		ERROR_LOGGING2("Unhandled internal case");
		return false;
	}
	return true;
}


void raft::tcp::Server::handleInternalAfterLock(char a_cRequest, RaftNode2* a_pNode, NodeIdentifierKey* a_pNodeKey, std::string* a_pBufferToSendToOthers)
{
	switch (a_cRequest)
	{
	case raft::internal2::leader::removeNode:
		this->SendInformationToAllNodes(raft::tcp::socketTypes::raft,raft::receive::fromLeader2::removeNode,a_pBufferToSendToOthers,a_pNodeKey,NULL,false);
		break;
	case raft::internal2::newLeader::becomeLeader:
		this->SendInformationToAllNodes(raft::tcp::socketTypes::raft,raft::receive::fromNewLeader2::oldLeaderDied,a_pBufferToSendToOthers,NULL,NULL,false);
		break;
	default:
		ERROR_LOGGING2("Unhandled internal case");
		break;
	}
}


/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
bool raft::tcp::Server::handleReceiveFromNodeBeforeLockRcvContext(int32_t a_index, SWorkerData* a_pWorkerData)
{
	switch (a_pWorkerData->pear.rcv.cRequest)
	{
	case raft::response::ok:
		a_pWorkerData->pNode->pingReceived();
		a_pWorkerData->pNode->incrementOkCount();
		break;
	case raft::receive::fromFollower::resetPing:
		a_pWorkerData->pNode->pingReceived();
		DEBUG_APP_WITH_NODE(2, NODE_KEY(a_pWorkerData->pNode), "raft::receive::fromFollower::resetPing");
		break;
	case raft::receive::fromAnyNode2::clbkCmd:
		HandleSeedClbk(a_pWorkerData->pNode);
		DEBUG_APP_WITH_NODE(3, NODE_KEY(a_pWorkerData->pNode), "raft::receive::anyNode::clbkCmd");
		break;
	case raft::receive::fromAdder::newNode:
		DEBUG_APP_WITH_NODE(1, NODE_KEY(a_pWorkerData->pNode), "raft::receive::fromAdder::newNode");
		if(!raft_receive_fromAdder_newNode(a_pWorkerData)){
			ERROR_LOGGING2("Unable to get data from adder");
			return false;
		}
		break;
	case raft::receive::fromLeader2::removeNode:
		DEBUG_APP_WITH_NODE(1, NODE_KEY(a_pWorkerData->pNode), "raft::receive::fromLeader::removeNode");
		if(!raft_receive_fromLeader_removeNode(a_pWorkerData)){
			ERROR_LOGGING2("Unable to get data on node remove from leader");
			return false;
		}
		break;
	case raft::receive::fromNewLeader2::oldLeaderDied:
		//if (!is_candidate()) {
		if (is_leader()) {
			ERROR_LOGGING2(
				"own node is not candidate, but node (%s:%d) tries to became leader", 
				NODE_KEY(a_pWorkerData->pNode)->ip4Address, (int)NODE_KEY(a_pWorkerData->pNode)->port);
			return false;
		}
		DEBUG_APP_WITH_NODE(1, NODE_KEY(a_pWorkerData->pNode), "raft::receive::fromNewLeader2::oldLeaderDied");
		ReceiveExtraData(*GET_NODE_TOOLS(a_pWorkerData->pNode)->socketPtr(raft::tcp::socketTypes::raft), GET_NODE_TOOLS(a_pWorkerData->pNode)->isEndianDiffer, &a_pWorkerData->extraStr());
		break;
	default:
		ERROR_LOGGING2(
			"default from node(%s:%d): requestNm=%d", 
			NODE_KEY(a_pWorkerData->pNode)->ip4Address,(int)NODE_KEY(a_pWorkerData->pNode)->port,(int)a_pWorkerData->pear.rcv.cRequest);
		return false;
	}

	return true;
}


bool raft::tcp::Server::handleReceiveFromNodeLocked(char a_cRequest,RaftNode2* a_pNode, int32_t a_index, NodeIdentifierKey* a_pNodeKey, std::string* a_receivedData)
{
	RaftNode2* pNode;
	switch (a_cRequest)
	{
	case raft::receive::fromAdder::newNode:
		pNode = this->AddNode(a_pNodeKey, sizeof(NodeIdentifierKey), a_receivedData, false,false);
		if(!pNode){
			ERROR_LOGGING2("Unable to add node (%s:%d)", a_pNodeKey->ip4Address, (int)a_pNodeKey->port);
			return false;
		}
		DEBUG_APPLICATION(1, "Node (%s:%d) add. NumberOfNodes=%d", NODE_KEY(pNode)->ip4Address, (int)NODE_KEY(pNode)->port, (int)nodesCount());
		break;
	case raft::receive::fromLeader2::removeNode:
		DEBUG_APPLICATION(1, "Node (%s:%d) remove. NumberOfNodes=%d", NODE_KEY(a_pNode)->ip4Address, (int)NODE_KEY(a_pNode)->port, (int)(nodesCount() - 1));
		this->RemoveNode1(a_pNodeKey,sizeof(NodeIdentifierKey),a_receivedData);
		break;
	case raft::receive::fromNewLeader2::oldLeaderDied:
		if(!m_pLeaderNode){return false;}
		DEBUG_APPLICATION(1, 
			"\n"
			"                                          Node (%s:%d) [old leader] remove. NumberOfNodes=%d.\n"
			"                                          NewLeader is: (%s:%d)",
			NODE_KEY(m_pLeaderNode)->ip4Address, (int)NODE_KEY(m_pLeaderNode)->port, (int)(nodesCount() - 1),
			NODE_KEY(a_pNode)->ip4Address, (int)NODE_KEY(a_pNode)->port);
		this->RemoveNode2(m_pLeaderNode, a_receivedData);
		m_pLeaderNode = a_pNode;
		m_pLeaderNode->makeLeader();
		break;
	default:
		return false;
	}

	return true;
}


void raft::tcp::Server::handleReceiveFromNodeAfterLock(char a_cRequest, RaftNode2* a_pNode, int32_t a_index, NodeIdentifierKey* a_pNodeKey, std::string* a_receivedData)
{
	PREPARE_SOCKET_GUARD();

	switch (a_cRequest)
	{
	case raft::receive::fromAdder::newNode:
		LOCK_SEND_SOCKET_MUTEX(a_pNode, raft::tcp::socketTypes::raft);
		GET_NODE_TOOLS(a_pNode)->writeC(raft::tcp::socketTypes::raft,a_pNode,this,&g_ccResponceOk,1);
		UNLOCK_SOCKET_MUTEX();
		break;
	default:
		return;
	}

	return ;
}


/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
bool raft::tcp::Server::handleNewConnectionBeforeLock(uint32_t a_unRequest, common::SocketTCP& a_socket, SWorkerData* a_pWorkerData, uint32_t a_isEndianDiffer)
{

	bool bRet(false);

	switch (a_unRequest)
	{
	case raft::connect::toAnyNode2::newNode:
		bRet = raft_connect_toAnyNode_newNode(a_socket, a_pWorkerData, a_isEndianDiffer);
		if (!bRet) {
			ERROR_LOGGING2("Unable to complete new node adding");
			return false;
		}
		DEBUG_APPLICATION(1, "raft::connect::toLeader::newNode");
		break;
	case raft::connect::toAnyNode2::permanentBridge:
		raft_connect_toAnyNode_permanentBridge(a_socket, &a_pWorkerData->pear.con.remAddress, a_isEndianDiffer);
		DEBUG_APPLICATION(1, "raft::connect::toAnyNode2::permanentBridge");
		break;
	case raft::connect::fromClient2::allNodesInfo:
		raft_connect_fromClient_allNodesInfo(a_socket);
		DEBUG_APPLICATION(1, "raft::connect::fromClient::allNodesInfo");
		break;
	case raft::connect::fromClient2::ping:
		raft_connect_fromClient_ping(a_socket, &a_pWorkerData->pear.con.remAddress);
		DEBUG_APPLICATION(3, "raft::connect::fromClient::ping");
		break;
	case raft::connect::fromClient2::startTime:
		raft_connect_fromClient_startTime(a_socket, &a_pWorkerData->pear.con.remAddress);
		DEBUG_APPLICATION(3, "raft::connect::fromClient::ping");
		break;
	case raft::connect::toAnyNode2::otherLeaderFound:
		raft_connect_toAnyNode_otherLeaderFound(a_socket, a_isEndianDiffer);
		DEBUG_APPLICATION(1, "raft::connect::toAnyNode::otherLeaderFound");
		break;
	default:
		break;
	}

	return bRet;

}


RaftNode2* raft::tcp::Server::handleNewConnectionLocked(
	common::SocketTCP& a_socket, const sockaddr_in&a_remoteAddr, uint32_t a_unRequest, 
	NodeIdentifierKey* a_newNodeKey, std::string* a_pDataFromClient, uint32_t /*a_isEndianDiffer*/)
{
	RaftNode2* pNewNode = NULL;
	switch (a_unRequest)
	{
	case raft::connect::toAnyNode2::newNode:
		pNewNode = this->AddNode(a_newNodeKey, sizeof(NodeIdentifierKey), a_pDataFromClient, true,false);
		if (!pNewNode) { 
			const char cRequest = raft::receive::fromAdder::nodeWithKeyAlreadyExist;
			a_socket.writeC(&cRequest, 1);
			ERROR_LOGGING2("Unable to add node (%s:%d)", a_newNodeKey->ip4Address, (int)a_newNodeKey->port);
			return NULL; 
		}
		DEBUG_APPLICATION(1, "Node (%s:%d) added. NumberOfNodes=%d", a_newNodeKey->ip4Address, (int)a_newNodeKey->port,(int)nodesCount());
		break;
	default:
		DEBUG_APPLICATION(1, "Wrong case provided");
		break;
	}
	return pNewNode;
}


void raft::tcp::Server::handleNewConnectionAfterLock(common::SocketTCP& a_socket, const sockaddr_in&a_remoteAddr, uint32_t a_unRequest, NodeIdentifierKey* a_newNodeKey,
													std::string* a_pDataToOthers, RaftNode2* a_pNodeToSkip, uint32_t /*a_isEndianDiffer*/)
{
	switch (a_unRequest)
	{
	case raft::connect::toAnyNode2::newNode:
		if(!a_pNodeToSkip){return;}
		this->SendInformationToAllNodes(raft::tcp::socketTypes::raft,raft::receive::fromAdder::newNode,a_pDataToOthers,a_newNodeKey,a_pNodeToSkip,true);
		GET_NODE_TOOLS(a_pNodeToSkip)->setSocket(raft::tcp::socketTypes::raft,(int)a_socket);
		this->InterruptReceivercThread(raft::tcp::socketTypes::raft);
		this->SendInformationToNode(a_pNodeToSkip,raft::tcp::socketTypes::raft, raft::receive::fromAdder::toNewNodeAddPartitions, a_pDataToOthers, NULL);
		a_socket.ResetSocketWithoutClose();
		break;
	default:
		DEBUG_APPLICATION(1, "Wrong case provided");
		break;
	}
	return;
}
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

void raft::tcp::Server::AddConnectForWorker(const sockaddr_in* a_pRemote, int a_socketDescr)
{
	AddJobForWorkerPrivate(workRequest::handleConnection, 0, NULL, a_pRemote, a_socketDescr);
}


void raft::tcp::Server::AddInternalForWorker(char a_cRequest, RaftNode2* a_pNode)
{
	AddJobForWorkerPrivate(workRequest::handleInternal,a_cRequest,a_pNode, NULL,-1);
}


void raft::tcp::Server::AddJobForWorkerPrivate(workRequest::Type a_type, char a_cRequest, RaftNode2* a_pNode, const sockaddr_in* a_pRemote, int a_socketDescriptor)
{
	SWorkerData aJobData(a_type);

	if (a_pNode) {
		if (a_pNode->isMarkedForDeletion()) { return; }
		a_pNode->incrementLock2();
	}

	switch (a_type)
	{
	case workRequest::handleConnection:
		aJobData.pNode = NULL;
		aJobData.pear.con.remAddress = *a_pRemote;
		aJobData.pear.con.sockDescriptor = a_socketDescriptor;
		break;
	case workRequest::handleInternal:
		aJobData.pNode = a_pNode;
		aJobData.pear.intr.cRequest = a_cRequest;
		break;
	default:
		ERROR_LOGGING2(" ");
		return;
	}

	m_fifoWorker.AddElement2(std::move(aJobData));
	m_semaWorker.post();
}


void raft::tcp::Server::ThreadFunctionWorker()
{
	SWorkerData dataFromProducer(workRequest::none);
	
enterLoopPoint:
	try {
		while (m_nWork) {	
			m_semaWorker.wait();

			while (m_fifoWorker.Extract(&dataFromProducer) && m_nWork) {

				if(dataFromProducer.pNode && dataFromProducer.pNode->isMarkedForDeletion()){
					// node marked for deletion
					dataFromProducer.pNode->decrementLock2();
					goto enterLoopPoint;
				}

				switch (dataFromProducer.reqType) 
				{
				case raft::tcp::workRequest::handleConnection:
					HandleNewConnectionPrivate(&dataFromProducer);
					break;
				case raft::tcp::workRequest::handleReceive:
					HandleReceiveFromNodePrivate(dataFromProducer.pear.rcv.cRequest, dataFromProducer.pNode, dataFromProducer.pear.rcv.index,&dataFromProducer.nodeKey,&dataFromProducer.extraStr());
					break;
				case raft::tcp::workRequest::handleInternal:
					HandleInternalPrivate(dataFromProducer.pear.intr.cRequest, dataFromProducer.pNode,&dataFromProducer.nodeKey,&dataFromProducer.extraStr());
					break;
				default:
					break;
				}				

			} // while (m_fifoWorker.Extract(&dataFromProducer) && m_nWork) {
		}  // while (m_nWork) {
	}
	catch(...){
		ERROR_LOGGING2("Unknown exception thrown");
		goto enterLoopPoint;
	}
}


void raft::tcp::Server::CheckAllPossibleSeeds(const std::vector<NodeIdentifierKey>& a_vectPossibleNodes, std::string* a_extraDataForAndFromAdder)
{
	const char* cpcPosibleSeedIp;
	RaftNode2* pNode;
	NodeIdentifierKey* pNodeKey, *pNodesFromLeader=NULL;
	NodeIdentifierKey nextNode;
	char vcOwnIp4Address[MAX_IP4_LEN];
	std::vector<NodeIdentifierKey>  vectLeaders;
	common::SocketTCP aSocket;
	const int cnSize((int)a_vectPossibleNodes.size());
	int i;
	
    DEBUG_HANGING();
	common::socketN::GetOwnIp4Address(vcOwnIp4Address,MAX_IP4_LEN);
    DEBUG_HANGING();
 
	try {

		for(i=0;i<cnSize;++i){
			DEBUG_APP_WITH_NODE(2, &a_vectPossibleNodes[i], "trying to connect");
			if(  (strncmp(vcOwnIp4Address,a_vectPossibleNodes[i].ip4Address,MAX_IP4_LEN)==0)&&(m_nPortOwn==a_vectPossibleNodes[i].port) ){continue;}
			else {
				cpcPosibleSeedIp = common::socketN::GetIp4AddressFromHostName(a_vectPossibleNodes[i].ip4Address);
				if (cpcPosibleSeedIp) {
					DEBUG_APP_WITH_NODE(3,&a_vectPossibleNodes[i],"cpcPosibleSeedIp=%s, m_nPortOwn=%d", cpcPosibleSeedIp, m_nPortOwn);
					if(strcmp(cpcPosibleSeedIp,"127.0.0.1")==0){
						if(m_nPortOwn== a_vectPossibleNodes[i].port){continue;}
						else{
							nextNode.set_ip4Address1(vcOwnIp4Address);
							nextNode.port = a_vectPossibleNodes[i].port;
							m_allPossibleNodes.push_back(nextNode);
						}
					}
					else if(strncmp(vcOwnIp4Address, cpcPosibleSeedIp, MAX_IP4_LEN) == 0){
						if(m_nPortOwn== a_vectPossibleNodes[i].port){continue;}
						else {
							nextNode.set_ip4Address1(vcOwnIp4Address);
							nextNode.port = a_vectPossibleNodes[i].port;
							m_allPossibleNodes.push_back(nextNode);
						}
					}
				}  // if (cpcPosibleSeedIp) {
				else {
					nextNode.set_ip4Address1(a_vectPossibleNodes[i].ip4Address);
					nextNode.port = a_vectPossibleNodes[i].port;
					m_allPossibleNodes.push_back(nextNode);
				}
			}
            DEBUG_HANGING();
			if(!pNodesFromLeader){
				pNodesFromLeader = TryFindClusterThredSafe(a_vectPossibleNodes[i], a_extraDataForAndFromAdder);
			}
            DEBUG_HANGING();
		}
		
		
		if (pNodesFromLeader) {
			if(m_pLeaderNode){d_state.set(RAFT_STATE_FOLLOWER);}
			else { d_state.set(RAFT_STATE_CANDIDATE); }
		}
		else{
			AddOwnNode(true,NULL);
			this->d_state.set(RAFT_STATE_LEADER);
		}

		DEBUG_APPLICATION(1, "NumberOfNodes=%d", nodesCount());
		pNode = firstNode();
		while(pNode){
			pNodeKey = NODE_KEY(pNode);
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

	if(is_follower()){
		//
	}
}


void raft::tcp::Server::ThreadFunctionPeriodic()
{
	PREPARE_SOCKET_GUARD();
	RaftNode2* pNode;
	//timeb	aCurrentTime;
	//int nTimeDiff;
	const char cRequest = raft::response::ok;
	common::NewSharedLockGuard<NewSharedMutex> aShrdLockGuard;
	int nIteration(0);
	//int nSndRcv;
	//char cRequestReg;

#ifdef _WIN32
	m_periodicThreadId = GetCurrentThread();
#else
	m_periodicThreadId = pthread_self();
#endif
	
enterLoopPoint:
	try {
		aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
		pNode = firstNode();
		while(m_nWork && pNode){
			if(pNode!=m_thisNode){
				LOCK_SEND_SOCKET_MUTEX(pNode, raft::tcp::socketTypes::raft);
				GET_NODE_TOOLS(pNode)->writeC(raft::tcp::socketTypes::raft, pNode, this, &cRequest, 1);
				UNLOCK_SOCKET_MUTEX();
				pNode->pingReceived();
			}
			pNode = pNode->next;
		}
		aShrdLockGuard.UnsetAndUnlockMutex();

		if(m_pLeaderNode && (m_pLeaderNode!=m_thisNode)){
			GET_NODE_TOOLS(m_pLeaderNode)->writeC(raft::tcp::socketTypes::raft, m_pLeaderNode, this, &g_ccResponceOk, 1);
		}
		
		while (m_nWork) {
			
			aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
			this->periodic(m_nPeriodForPeriodic);
			
#if 0
			if (is_follower() && (m_pLeaderNode->makePing()<10)) {
				ftime(&aCurrentTime);
				nTimeDiff = MSEC(aCurrentTime,this->m_lastPingByLeader);
				if(nTimeDiff>(2*m_nPeriodForPeriodic)){
					LOCK_RAFT_SEND_MUTEX(m_pLeaderNode);
					nSndRcv = GET_NODE_TOOLS(m_pLeaderNode)->raftSocket.writeC(&cRequest, 1);
					UNLOCK_SEND_SOCKET_MUTEX2();
					if (nSndRcv != 1) {
						ERROR_LOGGING2("leader is problematic");
						m_pLeaderNode->setProblematic();
					}
				}  // if(nTimeDiff>(2*m_nPeriodForPeriodic)){
				if(m_pLeaderNode->isProblematic()){m_pLeaderNode->makePing();}
			}  // if (is_follower() && (!m_pLeaderNode->isProblematic())) {

#endif

			aShrdLockGuard.UnsetAndUnlockMutex();

			if ((nIteration++ % 500) == 0) {
				m_semaForSolvingDublicates2.post();
				if (is_leader()) { DEBUG_APPLICATION(4, "Leader node (leaderIteration=%d)", nIteration); }
			}

			SleepMs(m_nPeriodForPeriodic);
		}
	}
	catch (...) {
		//UNLOCK_SEND_SOCKET_MUTEX();
		aShrdLockGuard.UnsetAndUnlockMutex();
		goto enterLoopPoint;
	}
}


void raft::tcp::Server::AddOwnNode(bool a_bIsLeader, std::string* a_pAdderInfo)
{
	if(!m_thisNode){
		NodeIdentifierKey aOwnHost;
		common::socketN::GetOwnIp4Address(aOwnHost.ip4Address, MAX_IP4_LEN);
		aOwnHost.port = m_nPortOwn;
		m_thisNode = this->AddNode(&aOwnHost, sizeof(NodeIdentifierKey), a_pAdderInfo, false,true);
		if(a_bIsLeader && m_thisNode){
			m_pLeaderNode = m_thisNode;
			m_pLeaderNode->makeLeader();
		}
	}
}


raft::tcp::NodeIdentifierKey* raft::tcp::Server::TryFindClusterThredSafe(const NodeIdentifierKey& a_nodeInfo, std::string* a_extraDataForAndFromAdder)
{
	NodeIdentifierKey *pNodesInfo = NULL;
	RaftNode2* pNode;
	//NodeTools* pAddersTool = NULL;
	common::SocketTCP aSocket;
    int nSndRcv;
	uint32_t isEndianDiffer;
	int32_t nLeaderIndex;
	int32_t i,nNumberOfNodes, nBytesToReceive;
	int32_t nExtraDataLenOut((int32_t)a_extraDataForAndFromAdder->size());
	bool bOk(false);
	char cRequest;

	if(!ConnectAndGetEndian2(&aSocket,a_nodeInfo,raft::connect::toAnyNode2::newNode,&isEndianDiffer)){goto returnPoint;}	// 1. connect, getEndian and sendRequest

	// !!!!!!!!!!!!!!!!!!!!!!! we found a node of cluster
	
	// 1. let's send own port
	nSndRcv = aSocket.writeC(&m_nPortOwn,4);
	if (nSndRcv != 4) { 
		DEBUG_APPLICATION(3,"Founded node (%s:%d) does not behave properly",a_nodeInfo.ip4Address,(int)a_nodeInfo.port);
		goto returnPoint;
	}

	// 2. let's get the index of leader
	nSndRcv = aSocket.readC(&nLeaderIndex,4);
	if (nSndRcv != 4) { 
		DEBUG_APPLICATION(3,"Unable to get leader index from the node (%s:%d)",a_nodeInfo.ip4Address,(int)a_nodeInfo.port);
		goto returnPoint;
	}

	// 3. getting number of nodes in the cluster
	nSndRcv = aSocket.readC(&nNumberOfNodes,4);
	if (nSndRcv != 4) { 
		DEBUG_APPLICATION(3,"Unable to get number of nodes from the node (%s:%d)",a_nodeInfo.ip4Address,(int)a_nodeInfo.port);
		goto returnPoint;
	}

	if(isEndianDiffer){
		SWAP4BYTES(nLeaderIndex);
		SWAP4BYTES(nNumberOfNodes);
	}

	if(nNumberOfNodes<1){
		DEBUG_APPLICATION(2, "wrong number of nodes from the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint;
	}

	nBytesToReceive = nNumberOfNodes * sizeof(NodeIdentifierKey);
	pNodesInfo = (NodeIdentifierKey*)malloc(nBytesToReceive);
	HANDLE_MEM_DEF2(pNodesInfo, " ");

	// 4. getting all nodes info
	nSndRcv = aSocket.readC(pNodesInfo, nBytesToReceive);
	if (nSndRcv != nBytesToReceive) { 
		DEBUG_APPLICATION(3, "Unable to get nodes info from the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint; 
	}

	if (isEndianDiffer) { for (i = 0; i<nNumberOfNodes; ++i) { SWAP4BYTES(pNodesInfo[i].port); } }

	m_pLeaderNode = NULL;
	for (i = 0; i<nNumberOfNodes; ++i) {
		pNode = this->AddNode(&pNodesInfo[i], sizeof(NodeIdentifierKey), NULL, false,false);
		if (!pNode) { 
			ERROR_LOGGING2("!!!!!!!!!!!!!!!!!!! probable bug");
			continue; 
		}

		if(pNodesInfo[i]== a_nodeInfo){
			GET_NODE_TOOLS(pNode)->setSocket(raft::tcp::socketTypes::raft, (int)aSocket);
		}

		if(i== nLeaderIndex){
			m_pLeaderNode = pNode;
			m_pLeaderNode->makeLeader();
		}
	}

	// 5. sending extra data length to the adder node
	//    this is also trigger that adding all ndes info here is done
	nSndRcv = aSocket.writeC(&nExtraDataLenOut, 4);
	if (nSndRcv != 4) { 
		DEBUG_APPLICATION(3, "Unable to send extra data length to the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint; 
	}

	// 6. sending extra data itself to adder node
	//    this is also trigger that adding all ndes info here is done
	nSndRcv = aSocket.writeC(a_extraDataForAndFromAdder->data(),nExtraDataLenOut);// todo
	if (nSndRcv != nExtraDataLenOut) {
		DEBUG_APPLICATION(3, "Unable to send extra data length to the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint;
	}

	// 7. Wait confirmation that all nodes know about this node
	//    this is also trigger that adding all ndes info here is done
	//    meanwhile the size of extra data len
	nSndRcv = aSocket.readC(&cRequest,1);
	if (nSndRcv != 1) {
		DEBUG_APPLICATION(3, "Unable to send extra data length to the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint;
	}
	if (cRequest == raft::receive::fromAdder::nodeWithKeyAlreadyExist) {
		ERROR_LOGGING2("node with the key of this node key already exist in the cluster");
		exit(0);
	}
	if (cRequest != raft::receive::fromAdder::toNewNodeAddPartitions) {
		DEBUG_APPLICATION(3, "Unable to send extra data length to the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint;
	}

	// 10. Wait confirmation that all nodes know about this node
	//    this is also trigger that adding all ndes info here is done
	//    meanwhile the size of extra data len
	if(!ReceiveExtraData(aSocket,isEndianDiffer, a_extraDataForAndFromAdder)){
		ERROR_LOGGING2("Unable to get extra data from the node (%s:%d)",a_nodeInfo.ip4Address,(int)a_nodeInfo.port);
		goto returnPoint;
	}

	AddOwnNode(false, a_extraDataForAndFromAdder);

	if ( !m_pLeaderNode ) {	
		//become_candidate();
	}
	else{
		//become_follower();
	}

	bOk = true;
returnPoint:
	if(bOk){
		aSocket.ResetSocketWithoutClose();
	}
	else{
		ClearAllNodes(NULL);
		aSocket.closeC();
		free(pNodesInfo); pNodesInfo = NULL;
	}
	return pNodesInfo;
}


void raft::tcp::Server::InterruptPeriodicThread()
{
#ifdef _WIN32
	if (m_periodicThreadId) { QueueUserAPC(PAPCFUNC_static, m_periodicThreadId, NULL); }
#else
    if(m_periodicThreadId){pthread_kill(m_periodicThreadId,SIGINT);}
#endif
}


void raft::tcp::Server::InterruptReceivercThread(int32_t a_index)
{
	if(!VALIDATE_INDEX_INVALID(GET_NODE_TOOLS(m_thisNode),a_index)){
		ERROR_LOGGING2("Wrong index provided");
		return;
	}

	if (m_intrptSocketForRcv[a_index]>0) { 
		int nSocketForInfoOld = m_intrptSocketForRcv[a_index];
		int nSocketForInfoNew = CreateEmptySocket();
		m_intrptSocketForRcv[a_index] = nSocketForInfoNew;
		closesocket(nSocketForInfoOld);
	}

#ifdef _WIN32
    // something with windows?
#else
    if(m_rcvThreadIds[a_index]){pthread_kill(m_rcvThreadIds[a_index],SIGPIPE);}
#endif
}


void raft::tcp::Server::become_leader()
{
	if(m_pLeaderNode){
		AddInternalForWorker(raft::internal2::newLeader::becomeLeader,NULL);
	}
	else {
		RaftServer::become_leader();
	}
}


void raft::tcp::Server::become_candidate()
{
	RaftNode2* pNexNode = firstNode();
	int nNodesWillVote(0);

	if (m_pLeaderNode) { m_pLeaderNode->setUnableToVote(); }
	while(pNexNode){
		if(pNexNode->isAbleToVote()){++nNodesWillVote;}
		pNexNode = pNexNode->next;
	}

	DEBUG_APPLICATION(1,"NumberOfNodes=%d nodes will ellect is: %d",nodesCount(),nNodesWillVote);

	if(nNodesWillVote<2){ // no node to take part on election, so become leader
		RaftServer::become_candidate();
		AddInternalForWorker(raft::internal2::newLeader::becomeLeader, NULL);
	}
	else{
		RaftServer::become_candidate();
	}

}


int raft::tcp::Server::SendClbkFunction(void *a_cb_ctx, void *udata, RaftNode2* a_pNode, int a_msg_type, const unsigned char *send_data,int d_len)
{
	PREPARE_SOCKET_GUARD();
	//RaftServerTcp* pServer = (RaftServerTcp*)a_cb_ctx;
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
	Server* pServer = (Server*)a_cb_ctx;
	NodeTools* pNodeTools = GET_NODE_TOOLS(a_pNode);
	NodeIdentifierKey* pNodeKey = NODE_KEY(a_pNode);
	struct timeb currentTime;
	int nSndRcv;
	int64_t nTimeoutOfLastSeen;
	char cRequest=raft::receive::fromAnyNode2::clbkCmd;

	if(a_pNode->okCount()<0){
		a_pNode->incrementOkCount();
		return 0;
		// still waiting untill node ready to play
	}


	ftime(&currentTime);
	nTimeoutOfLastSeen = MSEC(currentTime, a_pNode->lastSeen());

	switch (a_msg_type)
	{
	case RAFT_MSG_APPENDENTRIES:
		if((nTimeoutOfLastSeen>MAX_UNSEEN_TIME_TO_CHANGE_STATE)&& pServer->is_leader() && (a_pNode->pingCount()>2)){
			pServer->AddInternalForWorker(raft::internal2::leader::removeNode,a_pNode);
			return 0;
		}
		break;
	case RAFT_MSG_REQUESTVOTE:
		if((nTimeoutOfLastSeen>MAX_UNSEEN_TIME_TO_CHANGE_STATE) /*&& pServer->is_candidate()*/ && (a_pNode->pingCount()>2)){
			a_pNode->setUnableToVote();
			//pServer->become_candidate();  // todo: change state in locked part
		}
		break;
	default:
		break;
	}

	if (nTimeoutOfLastSeen>MIN_UNSEEN_TIME_TO_DISPLAY) {
		DEBUG_APP_WITH_NODE(1, pNodeKey, "node is not responding for %d ms", (int)nTimeoutOfLastSeen);
		if(a_pNode->pingCount()<4){a_pNode->ping();}
		else {return 0;}
	}

	LOCK_SEND_SOCKET_MUTEX(a_pNode,raft::tcp::socketTypes::raft);
	nSndRcv=pNodeTools->writeC(raft::tcp::socketTypes::raft,a_pNode,pServer,&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}
	nSndRcv= pNodeTools->writeC(raft::tcp::socketTypes::raft, a_pNode, pServer,&a_msg_type,4);
	if(nSndRcv!=4){goto returnPoint;}
	nSndRcv= pNodeTools->writeC(raft::tcp::socketTypes::raft, a_pNode, pServer, send_data, d_len);
	UNLOCK_SOCKET_MUTEX();

returnPoint:
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
	::common::NewSharedLockGuard< ::STDN::shared_mutex > aSharedGuard;
    ServersList* pServer;

#ifndef _WIN32
	pthread_t interruptThread=pthread_self();
#endif

    DEBUG_APPLICATION(1,"Interrupt (No:%d)",a_nSigNum);

	aSharedGuard.SetAndLockMutex(&s_pRWlockForServers);
    DEBUG_APPLICATION(4,"rd_lock");

    pServer = s_pFirst;
    while(pServer){

		switch (a_nSigNum)
		{
		case SIGABRT:
			DEBUG_APPLICATION(2,"SIGABRT");
#if !defined(_WIN32) || defined(_WLAC_USED)
        case SIGTSTP:
            DEBUG_APPLICATION(2, "SIGTSTP");
#endif
#ifdef _USE_LOG_FILES
			FlushLogFilesIfNonNull();
            return;
#endif
			break;
		case SIGFPE:
			DEBUG_APPLICATION(0,"SIGFPE");
			break;
		case SIGILL:
			DEBUG_APPLICATION(0,"SIGILL");
			break;
		case SIGINT: case SIGTERM:
		{
			static int snSigIntCount = 0;

			raft::tcp::g_nApplicationRun = 0;

#ifdef _WIN32
			if (s_mainThreadHandle) { 
				QueueUserAPC(PAPCFUNC_static,s_mainThreadHandle,NULL ); 
				s_mainThreadHandle = (HANDLE)0;
				break;
			}
#else
			if (interruptThread != s_mainThreadHandle) {
				pthread_kill(s_mainThreadHandle, SIGINT);
				break;
			}
			
#endif
			if (snSigIntCount++>1) {
				ERROR_LOGGING2("Normal shutdown was not sucessfull. Program will be killed");
				exit(1);
			}

			DEBUG_APPLICATION(0, "Global flag set to 0, next SIGINT will kill server");
			break;
		}
		case SIGSEGV:
			DEBUG_APPLICATION(0,"SIGSEGV");
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

    DEBUG_APPLICATION(4,"unlock");
}

/********************************************************************************************************************/

int raft::tcp::NodeTools::writeC(int32_t a_index, RaftNode2* a_pNode, Server* a_pServer, const void* a_data, int a_dataLen)
{
	if(!VALIDATE_INDEX_INVALID(this,a_index)){
		ERROR_LOGGING2("Wrong index provided (index=%d)",(int)a_index);
		return -1;
	}

	if(!m_sockets[a_index].isOpenC()){
		int nSndRcv;
		uint32_t  isEndianDiffer;
		if(!ConnectAndGetEndian2(&m_sockets[a_index], *NODE_KEY(a_pNode),raft::connect::toAnyNode2::permanentBridge,&isEndianDiffer,100)){
			m_sockets[a_index].closeC(); 
			return -1;
		}
		nSndRcv=m_sockets[a_index].writeC(&(a_pServer->m_nPortOwn), 4);
		if(nSndRcv!=4){m_sockets[a_index].closeC();return -3;}
		nSndRcv = m_sockets[a_index].writeC(&a_index, 4);
		if (nSndRcv != 4) { m_sockets[a_index].closeC(); return -4; }
		this->isEndianDiffer = isEndianDiffer;
		a_pServer->InterruptReceivercThread(a_index);
	}

	return m_sockets[a_index].writeC(a_data, a_dataLen);
}


int raft::tcp::NodeTools::readC(int32_t a_index, void* a_buffer, int a_bufferLen)
{
	int nSndRcv;
	if (!VALIDATE_INDEX_INVALID(this, a_index)) {
		ERROR_LOGGING2("Wrong index provided (index=%d)", (int)a_index);
		return -1;
	}
	nSndRcv= m_sockets[a_index].readC(a_buffer, a_bufferLen);
	if(nSndRcv!=a_bufferLen){m_sockets[a_index].closeC();}
	return nSndRcv;
}


void raft::tcp::NodeTools::setSocket(int32_t a_index, int a_socketNum)
{
	if (!VALIDATE_INDEX_INVALID(this, a_index)) {
		ERROR_LOGGING2("Wrong index provided (index=%d)", (int)a_index);
		return;
	}
	m_sockets[a_index].closeC();
	m_sockets[a_index].SetNewSocketDescriptor(a_socketNum);
}



int raft::tcp::NodeTools::getSocketNum(int32_t a_index)
{
	if (!VALIDATE_INDEX_INVALID(this, a_index)) {
		ERROR_LOGGING2("Wrong index provided (index=%d)", (int)a_index);
		return -2;
	}
	return (int)m_sockets[a_index];
}


common::SocketTCP* raft::tcp::NodeTools::socketPtr(int32_t a_index)
{
	if (!VALIDATE_INDEX_INVALID(this, a_index)) {
		ERROR_LOGGING2("Wrong index provided (index=%d)", (int)a_index);
		return NULL;
	}

	return &m_sockets[a_index];
}


STDN::mutex* raft::tcp::NodeTools::senderMutex(size_t a_index)
{
	if (!VALIDATE_INDEX_INVALID(this, a_index)) {
		ERROR_LOGGING2("Wrong index provided (index=%d)", (int)a_index);
		return NULL;
	}

	return &m_socketSendMutexes[a_index];
}


/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
raft::tcp::SWorkerData::SWorkerData(workRequest::Type a_reqType)
{
#ifdef RAW_BUF_IMPLEMENTATION
	memset(this, 0, sizeof(*this));
#else
	this->pNode = NULL;
	memset(&this->pear, 0, sizeof(this->pear));
#endif
	this->reqType = a_reqType; 
}


raft::tcp::SWorkerData::SWorkerData(SWorkerData&& a_moveData)
{
	*this = std::move(a_moveData);
}


raft::tcp::SWorkerData::~SWorkerData()
{
}


raft::tcp::SWorkerData& raft::tcp::SWorkerData::operator=(SWorkerData&& a_moveData)
{
#ifdef RAW_BUF_IMPLEMENTATION
	char* thisBuffer = this->m_pcBuffer;
	uint32_t thisLength = this->m_bufLength;
	uint32_t thisAllocated = this->m_allocatedSize;

	this->m_pcBuffer = a_moveData.m_pcBuffer;
	this->m_bufLength = a_moveData.m_bufLength;
	this->m_allocatedSize = a_moveData.m_allocatedSize;

	a_moveData.m_pcBuffer = thisBuffer;
	a_moveData.m_bufLength = thisLength;
	a_moveData.m_allocatedSize = thisAllocated;
#else
	this->m_extraDataBuffer = std::move(a_moveData.m_extraDataBuffer);
#endif

	this->reqType = a_moveData.reqType;
	this->pNode = a_moveData.pNode;
	this->nodeKey = std::move(a_moveData.nodeKey);

	switch (this->reqType)
	{
	case workRequest::handleConnection:
		this->pear.con.remAddress = std::move(a_moveData.pear.con.remAddress);
		this->pear.con.sockDescriptor = a_moveData.pear.con.sockDescriptor;
		break;
	case workRequest::handleReceive:
		this->pear.rcv.index = a_moveData.pear.rcv.index;
		this->pear.rcv.cRequest = a_moveData.pear.rcv.cRequest;
		break;
	case workRequest::handleInternal:
		this->pear.intr.cRequest = a_moveData.pear.intr.cRequest;
		break;
	default:
		break;
	}

	return *this;
}


char* raft::tcp::SWorkerData::buffer()
{
#ifdef RAW_BUF_IMPLEMENTATION
	return this->m_pcBuffer;
#else
	return const_cast<char*>(m_extraDataBuffer.data());
#endif
}


void raft::tcp::SWorkerData::resize(uint32_t a_newSize)
{
#ifdef RAW_BUF_IMPLEMENTATION
	if(a_newSize>this->m_allocatedSize){
		char* pcBuffer = (char*)realloc(this->m_pcBuffer, a_newSize);
		HANDLE_MEM_DEF2(pcBuffer, " ");
		this->m_pcBuffer = pcBuffer;
		this->m_allocatedSize = a_newSize;
	}
	this->m_bufLength = a_newSize;
#else
	m_extraDataBuffer.resize(a_newSize);
#endif
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
