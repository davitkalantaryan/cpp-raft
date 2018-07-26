
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
#include <utility>

#define NO_LEADER_KEY_PORT		-1

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

#define MIN_REP_RATE_MS						5		// les than this rep rate is not possible
#define DEF_REP_RATE_MS						2000	// [ms] default rep rate is 2 seconds
#define	TIMEOUTS_RATIO_MIN					11		// this is minimum ratio for follower between time of leader wait and rep. rate (to start election)
#define MAX_NUMBER_OF_PINGS					3		// maximum number of pings that really will be done by leader
#define MIN_UNSEEN_TIME_TO_DISPLAY			6000	// maximum number of pings that really will be done by leader
#define MAX_UNSEEN_TIME_TO_CHANGE_STATE		20000	// number of virtual pings, after this leader will remove follower
#define MAX_ITER_OK_COUNT					1000	// used for syncronization

#endif

#if !defined(_WIN32) || defined(_WLAC_USED)
#else
static HANDLE	s_mainThreadHandle = (HANDLE)0;
static VOID PAPCFUNC_static(_In_ ULONG_PTR){}
#endif

#ifdef _WIN32
#else
#define closesocket	close
#endif

#define VALIDATE_INDEX_INVALID(_toolPtr,_index) ((_index)<NUMBER_OF_TOOLS_SOCKETS)

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

	sigaction(SIGPIPE, &newAction, NULL);
#else
	s_mainThreadHandle = GetCurrentThread();
#endif

	sigaction(SIGABRT, &newAction, NULL_ACTION);
	sigaction(SIGFPE, &newAction, NULL_ACTION);
	sigaction(SIGILL, &newAction, NULL_ACTION);
	sigaction(SIGINT, &newAction, NULL_ACTION);
    //sigaction(SIGSEGV, &newAction, NULL_ACTION);
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


void raft::tcp::Server::AddAdditionalDataToNode(RaftNode2* a_pNode, std::string* a_pDataFromAdder, bool a_bIsAdder)
{
	NodeTools* pTools = new NodeTools;
	HANDLE_MEM_DEF2(pTools," ");
	RaftServer::AddAdditionalDataToNode(a_pNode, a_pDataFromAdder, a_bIsAdder);
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
#ifndef _WIN32
	m_starterThread = pthread_self();
#endif  // #ifdef HANDLE_SIG_ACTIONS

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

}


void raft::tcp::Server::StopServer()
{
	size_t i,nThreadsCount;

	if (m_nWork == 0) {return;}
	DEBUG_APPLICATION(1, "Stopping server");
	m_nWork = 0;

	InterruptPeriodicThread();
    for(i=0;i<NUMBER_OF_TOOLS_SOCKETS;++i){
		InterruptReceivercThread(i);
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

	DEBUG_APPLICATION(1,"connnection from host %s",common::socketN::GetIPAddress(a_remoteAddr));

	nSocketDescr = (int)a_clientSock;
	a_clientSock.ResetSocketWithoutClose();  // let us assume, that worker will close in the case of necessirty
	AddConnectForWorker(a_remoteAddr,nSocketDescr);

}


bool raft::tcp::Server::raft_connect_toAnyNode_newNode(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr, std::string* a_extraFromNewNode, NodeIdentifierKey* a_newNodeKey)
{
	// struct NodeIdentifierKey { char ip4Address[MAX_IP4_LEN]; int32_t port;};
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
    NodeIdentifierKey	*pAllNodesInfo=NULL;
	std::string dataFromNewNode;
	NodeIdentifierKey& newNodeKey = *a_newNodeKey;
	int nSndRcv;
	int32_t nTotalSize, nNodesCount, nLeaderIndex;
	uint32_t isEndianDiffer(0);
	uint16_t unRemEndian;
	bool bOk(false);

	newNodeKey.set_ip4Address2(a_remoteAddr);

	// 1. receive remote endian
	nSndRcv= a_clientSock.readC(&unRemEndian,2);
	if(nSndRcv!= 2){ goto returnPoint;}
	if(unRemEndian!=1){ isEndianDiffer=1;}

	// 2. receive remote network port
	nSndRcv= a_clientSock.readC(&newNodeKey.port,4);
	if(nSndRcv!= 4){ goto returnPoint;}
	if(isEndianDiffer){ SWAP4BYTES(newNodeKey.port);}
	
	nNodesCount = this->nodesCount();
	pAllNodesInfo=CollectAllNodesDataNotThrSafe(&nTotalSize,&nLeaderIndex);

	// 3. Send index of leader 
	nSndRcv = a_clientSock.writeC(&nLeaderIndex, 4);
	if (nSndRcv != 4) { goto returnPoint; }

	// 4. Send nodes count
	nSndRcv = a_clientSock.writeC(&nNodesCount, 4);
	if (nSndRcv != 4) { goto returnPoint; }

	// 5. send info on all nodes
	nSndRcv = a_clientSock.writeC(pAllNodesInfo, nTotalSize);
	if (nSndRcv != nTotalSize) { goto returnPoint; }
	free(pAllNodesInfo); pAllNodesInfo = NULL;

	// 6. receive extra data from new node
	//    meanwhile this is a confirmation that new node has already added all existing nodes
	bOk = ReceiveExtraData(a_clientSock,isEndianDiffer, a_extraFromNewNode);

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


void raft::tcp::Server::raft_connect_toAnyNode_otherLeaderFound(common::SocketTCP& a_clientSock)
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

	if( *NODE_KEY(m_pLeaderNode) == newLeaderKey ){
		a_clientSock.writeC("o", 1);
	}
	else {
		a_clientSock.writeC("e", 1);
		DEBUG_APP_WITH_NODE(0, &newLeaderKey, " [this is a correct leader]");
		// todo: make steps to connect to real leader
	}
}


void raft::tcp::Server::raft_connect_toAnyNode_permanentBridge(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)  // 0->raft
{
	// this socket should remain 
	RaftNode2* pNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey aRemHost;
	int nSndRcv;
	uint32_t isEndianDiffer(0);
	int32_t nIndex;
	uint16_t unRemEndian;

	aRemHost.set_ip4Address2(a_remoteAddr);	// let us specify host IP

	nSndRcv = a_clientSock.readC(&unRemEndian,2);							// endian
	if(nSndRcv!=2){
		ERROR_LOGGING2("Unable to get endian from host %s", common::socketN::GetIPAddress(a_remoteAddr));
		return;
	}
	if(unRemEndian!=1){ isEndianDiffer =1;}

	nSndRcv = a_clientSock.readC(&aRemHost.port,4);							// port
	if(nSndRcv!=4){
		ERROR_LOGGING2("Unable to get port from host %s", common::socketN::GetIPAddress(a_remoteAddr));
		return;
	}
	if(isEndianDiffer){SWAP4BYTES(aRemHost.port);}

	if(!this->FindNode(&aRemHost,sizeof(NodeIdentifierKey),&pNode)){
		ERROR_LOGGING2("Node (%s:%d) is not found", common::socketN::GetIPAddress(a_remoteAddr),(int)aRemHost.port);
		return;
	}
	pNodeTools = GET_NODE_TOOLS(pNode);
	pNodeTools->isEndianDiffer = isEndianDiffer;

	nSndRcv = a_clientSock.readC(&nIndex, 4);							// port
	if (nSndRcv != 4) {
		ERROR_LOGGING2("Unable to get index of bridge of node (%s:%d)", aRemHost.ip4Address,(int)aRemHost.port);
		return;
	}
	if (isEndianDiffer) { SWAP4BYTES(nIndex); }
	
	pNodeTools->setSocket(nIndex,(int)a_clientSock);
	a_clientSock.ResetSocketWithoutClose();

	InterruptReceivercThread(nIndex);
}


void raft::tcp::Server::ThreadFunctionFindOtherChains()
{
	RaftNode2* pNode;
	NodeIdentifierKey* pNodeKey;
	common::SocketTCP aSocket;
	size_t i, nPossibleNodesCount;
	int nSndRcv;
	uint32_t isEndianDiffer;
	uint16_t unEndian;
	bool bFound;
	char cRequest;

	while(m_nWork){

		m_semaForSolvingDublicates2.wait();

		nPossibleNodesCount = m_allPossibleNodes.size();
		for (i = 0; i<nPossibleNodesCount; ++i) {

			bFound = false;
			m_shrdMutexForNodes2.lock_shared();
			pNode = this->firstNode();

			while(pNode){
				pNodeKey = NODE_KEY(pNode);
				if((*(pNodeKey))==m_allPossibleNodes[i]){
					bFound = true;
					break;
				}
				pNode = pNode->next;
			}

			m_shrdMutexForNodes2.unlock_shared();

			if(!bFound){
				if (!ConnectAndGetEndian(&aSocket, m_allPossibleNodes[i], raft::connect::toAnyNode2::otherLeaderFound, &isEndianDiffer)) { continue; }

				//unEndian = 1;
				nSndRcv = aSocket.writeC(&unEndian, 2);
				if (nSndRcv != 2) { goto socketClosePoint; }

				nSndRcv = aSocket.writeC(m_pLeaderNode->key, sizeof(NodeIdentifierKey));
				if (nSndRcv != sizeof(NodeIdentifierKey)) { goto socketClosePoint; }

				nSndRcv = aSocket.readC(&cRequest, 1);
				if ((nSndRcv == 1) && (cRequest == 'e')) {
					DEBUG_APP_WITH_NODE(0, &m_allPossibleNodes[i], " [possible secondary leader (informed)]");
				}

			socketClosePoint:
				aSocket.closeC();
			}
		}  // for (i = 0; i<nPossibleNodesCount; ++i) {
	}  // while(m_nWork){

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


bool raft::tcp::Server::raft_receive_fromLeader_removeNode(RaftNode2* a_pNode, std::string* a_extraDataFromLeader, NodeIdentifierKey* a_pNewNodeKey)
{
	NodeTools *pTools = GET_NODE_TOOLS(a_pNode);
	int nSndRcv ;

	if (a_pNode != m_pLeaderNode) {
		ERROR_LOGGING2("node (%s:%d) is not leader, but tries to confirm leader action", NODE_KEY(a_pNode)->ip4Address, (int)NODE_KEY(a_pNode)->port);
		return false;
	}
	if (!is_follower()) {
		DEBUG_APP_WITH_NODE(0,NODE_KEY(a_pNode),"own node is not follower, but request is for follower");
		return false;
	}

	nSndRcv = pTools->readC(raft::tcp::socketTypes::raft, a_pNewNodeKey, sizeof(NodeIdentifierKey));
	if (nSndRcv != sizeof(NodeIdentifierKey)) {
		return false; 
	}
	if (pTools->isEndianDiffer) { SWAP4BYTES(a_pNewNodeKey->port); }

	return true;
}


bool raft::tcp::Server::raft_receive_fromAdder_newNode(RaftNode2* a_pNode, std::string* a_bufferForAdderData, NodeIdentifierKey* a_pNewNodeKey)
{
	RaftNode2* pNode;
	NodeTools* pNodeTools = GET_NODE_TOOLS(a_pNode);
	common::SocketTCP* pRaftSocket = pNodeTools->socketPtr(raft::tcp::socketTypes::raft);
	int nSndRcv;

	nSndRcv = pNodeTools->readC(raft::tcp::socketTypes::raft, a_pNewNodeKey, sizeof(NodeIdentifierKey));
	if (nSndRcv != sizeof(NodeIdentifierKey)) {
		ERROR_LOGGING2("is not able to get new node data from node (%s:%d) ", NODE_KEY(a_pNode)->ip4Address, (int)NODE_KEY(a_pNode)->port);
		return false;
	}
	if (pNodeTools->isEndianDiffer) { SWAP4BYTES(a_pNewNodeKey->port); }

	if (FindNode(a_pNewNodeKey, sizeof(NodeIdentifierKey), &pNode)) {
		ERROR_LOGGING2(
			"Node (%s:%d) tries to add already existing (%s:%d) node",
			NODE_KEY(a_pNode)->ip4Address, (int)NODE_KEY(a_pNode)->port,
			a_pNewNodeKey->ip4Address, (int)a_pNewNodeKey->port);
		return false;
	}

	return ReceiveExtraData(*pRaftSocket,pNodeTools->isEndianDiffer,a_bufferForAdderData);

}


void raft::tcp::Server::HandleNewConnectionPrivate(int a_nSocketDescr, const sockaddr_in& a_remoteAddr, NodeIdentifierKey* a_newNodeKey, std::string* a_pDataFromClient)
{
	RaftNode2* pNewNode = NULL;
	common::SocketTCP aClientSock;
	common::NewSharedLockGuard<STDN::shared_mutex> aSharedGuard;
	common::NewLockGuard<STDN::shared_mutex> aGuard;
	int nSndRcv;
	uint16_t snEndian;
	char vcHostName[MAX_HOSTNAME_LENGTH];
	char cRequest;

	DEBUG_APPLICATION(1,
		"conntion from %s(%s)",
		common::socketN::GetHostName(&a_remoteAddr, vcHostName, MAX_HOSTNAME_LENGTH),
		common::socketN::GetIPAddress(&a_remoteAddr));
	aClientSock.SetNewSocketDescriptor(a_nSocketDescr);
	aClientSock.setTimeout(SOCK_TIMEOUT_MS);

	snEndian = 1;
	nSndRcv = aClientSock.writeC(&snEndian, 2);																// 2. Send endian				
	if (nSndRcv != 2) {
		DEBUG_APPLICATION(1, "Could not send the endian of the connected pear nSndRcv=%d, socket=%d", nSndRcv, a_nSocketDescr);
		aClientSock.closeC();
		return;
	}

	nSndRcv = aClientSock.readC(&cRequest, 1);																// 4. rcv request
	if (nSndRcv != 1) {
		DEBUG_APPLICATION(1, "Unable to read request type");
		aClientSock.closeC();
		return;
	}

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	if(!handleNewConnectionBeforeLock(aClientSock,a_remoteAddr,cRequest,a_newNodeKey,a_pDataFromClient)){goto returnPoint;}
	aSharedGuard.UnsetAndUnlockMutex();

	aGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	pNewNode = handleNewConnectionLocked(aClientSock, a_remoteAddr, cRequest, a_newNodeKey, a_pDataFromClient);
	aGuard.UnsetAndUnlockMutex();

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	handleNewConnectionAfterLock(aClientSock, a_remoteAddr, cRequest, a_newNodeKey, a_pDataFromClient, pNewNode);
	aSharedGuard.UnsetAndUnlockMutex();


returnPoint:
	aGuard.UnsetAndUnlockMutex();
	aSharedGuard.UnsetAndUnlockMutex();

}


void raft::tcp::Server::HandleReceiveFromNodePrivate(RaftNode2* a_pNode, int32_t a_index, NodeIdentifierKey* a_pNodeKey, std::string* a_bBufferForReceive)
{
	common::NewSharedLockGuard<STDN::shared_mutex> aSharedGuard;
	common::NewLockGuard<STDN::shared_mutex> aGuard;
	common::NewLockGuard<STDN::mutex> aSockGuard;
	int nSndRcv;
	char cRequest;

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	aSockGuard.SetAndLockMutex(GET_NODE_TOOLS(a_pNode)->senderMutex(a_index));

	nSndRcv = GET_NODE_TOOLS(a_pNode)->readC(a_index, &cRequest, 1);
	if (nSndRcv != 1) { goto returnPoint; }

	if(!handleReceiveFromNodeBeforeLock(cRequest,a_pNode, a_index,a_pNodeKey,a_bBufferForReceive)){goto returnPoint;}
	if (a_pNode) { a_pNode->decrementLock2(); }

	aSockGuard.UnsetAndUnlockMutex();
	aSharedGuard.UnsetAndUnlockMutex();

	aGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	if(!handleReceiveFromNodeLocked(cRequest,a_pNode, a_index,a_pNodeKey,a_bBufferForReceive)){goto returnPoint;}
	aGuard.UnsetAndUnlockMutex();

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	handleReceiveFromNodeAfterLock(cRequest,a_pNode, a_index,a_pNodeKey,a_bBufferForReceive);
	aSharedGuard.UnsetAndUnlockMutex();

returnPoint:
	aSockGuard.UnsetAndUnlockMutex();
	aGuard.UnsetAndUnlockMutex();
	aSharedGuard.UnsetAndUnlockMutex();
}


void raft::tcp::Server::HandleInternalPrivate(char a_cRequest, RaftNode2* a_pNode, NodeIdentifierKey* a_pNodeKey, std::string* a_pBufferForReceive)
{
	common::NewSharedLockGuard<STDN::shared_mutex> aSharedGuard;
	common::NewLockGuard<STDN::shared_mutex> aGuard;

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	if(!handleInternalBeforeLock(a_cRequest,a_pNode,a_pNodeKey,a_pBufferForReceive)){goto returnPoint;}
	if (a_pNode) { a_pNode->decrementLock2(); }
	aSharedGuard.UnsetAndUnlockMutex();

	aGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	if(!handleInternalLocked(a_cRequest,a_pNode,a_pNodeKey,a_pBufferForReceive)){goto returnPoint;}
	aGuard.UnsetAndUnlockMutex();

	aSharedGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
	handleInternalAfterLock(a_cRequest, a_pNode, a_pNodeKey, a_pBufferForReceive);
	aSharedGuard.UnsetAndUnlockMutex();

returnPoint:
	aGuard.UnsetAndUnlockMutex();
	aSharedGuard.UnsetAndUnlockMutex();
}



void raft::tcp::Server::ReceiveFromSocketAndInform(RaftNode2* a_pNode, int32_t a_index)
{
	if (!VALIDATE_INDEX_INVALID(GET_NODE_TOOLS(a_pNode), a_index)) {
		ERROR_LOGGING2("Wrong index provided");
		return;
	}

	AddReceiveForWorker(a_pNode,a_index);
}


bool raft::tcp::Server::ReceiveExtraData(common::SocketTCP& a_socket, uint32_t a_isEndianDiffer, std::string* a_pBufForData)
{
	int nAddDataLen;
	int nSndRcv = a_socket.readC(&nAddDataLen, 4);
	if (nSndRcv != 4) { return false; }
	if (a_isEndianDiffer) { SWAP4BYTES(nAddDataLen); }

	if (nAddDataLen>0) {
		a_pBufForData->resize(nAddDataLen);
		nSndRcv = a_socket.readC(const_cast<char*>(a_pBufForData->data()),nAddDataLen);
        if (nSndRcv != nAddDataLen) { return false; }
	}
	else { a_pBufForData->clear(); }
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

			for (nIteration=0;(okCount==pNode->okCount())&&(nIteration<200);++nIteration) {
				SleepMs(10);
			}
		}
	} // if(a_bWait){
}


bool raft::tcp::Server::SendInformationToNode(RaftNode2* a_pNode, int32_t a_index, char a_cRequest, const std::string* a_extraData, const NodeIdentifierKey* a_pNodeKey)
{
	PREPARE_SEND_SOCKET_GUARD();
	int32_t nExtraDataLen;
	int nSndRcv;
	bool bRet(false);

	LOCK_SEND_SOCKET_MUTEX(a_pNode,a_index);
	nSndRcv = GET_NODE_TOOLS(a_pNode)->writeC(a_index,a_pNode,m_nPortOwn,&a_cRequest, 1);
	if (nSndRcv != 1) { goto nextNodePoint; }
	if(a_pNodeKey){
		nSndRcv = GET_NODE_TOOLS(a_pNode)->writeC(a_index,a_pNode,m_nPortOwn,a_pNodeKey,sizeof(NodeIdentifierKey));
		if (nSndRcv != sizeof(NodeIdentifierKey)) { goto nextNodePoint; }
	}
	if(a_extraData){
		nExtraDataLen = (int32_t)a_extraData->size();
		nSndRcv = GET_NODE_TOOLS(a_pNode)->writeC(a_index, a_pNode, m_nPortOwn, &nExtraDataLen, 4);
		if (nSndRcv != 4) { goto nextNodePoint; }
		if(nExtraDataLen>0){
			nSndRcv = GET_NODE_TOOLS(a_pNode)->writeC(a_index, a_pNode, m_nPortOwn, a_extraData->data(),nExtraDataLen);
			if (nSndRcv != nExtraDataLen) { goto nextNodePoint; }
		}
	}

	bRet = true;
nextNodePoint:
	UNLOCK_SEND_SOCKET_MUTEX();
	return bRet;
}


void raft::tcp::Server::FunctionForMultiRcv(void (Server::*a_fpRcvFnc)(RaftNode2*,int32_t), int32_t a_index)
{
	NodeTools* pNodeTools;
	RaftNode2* pNode;
	common::NewSharedLockGuard<STDN::shared_mutex> aShrdLockGuard;
	fd_set rFds, eFds;
	int nMax, nCurrentSocket, nSelectReturn, nSocketsFound, nSocketToIgnore=-1, nLastSocket;
    volatile int& nSocketForInfo = m_intrptSocketForRcv[a_index];

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
			pNode = firstNode();
			nLastSocket = -1;
			while (pNode) {
				if (pNode != m_thisNode) {
					pNodeTools = GET_NODE_TOOLS(pNode);
					nCurrentSocket = pNodeTools->getSocketNum(a_index);
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
				//SleepMs(2000);  // most probably last client was problematic
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

	if(nSocketForInfo>0){closesocket(nSocketForInfo);nSocketForInfo=-1;}
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
		break;
	case raft::internal2::newLeader::becomeLeader:
		if (!is_candidate()) {
			POSSIBLE_BUG("not a candidate but trying to act as candidate");
			return false;
		}
		this->RemoveNode2(m_pLeaderNode, a_pBufferToSendToOthers);
		m_pLeaderNode = m_thisNode;
		m_pLeaderNode->makeLeader();
		RaftServer::become_leader();
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
bool raft::tcp::Server::handleReceiveFromNodeBeforeLock(char a_cRequest,RaftNode2* a_pNode, int32_t a_index, NodeIdentifierKey* a_pNodeKey, std::string* a_pBufferForReceive)
{
	switch (a_cRequest)
	{
	case raft::response::ok:
		a_pNode->pingReceived();
		a_pNode->incrementOkCount();
		break;
	case raft::receive::fromFollower::resetPing:
		a_pNode->pingReceived();
		DEBUG_APP_WITH_NODE(2, NODE_KEY(a_pNode), "raft::receive::fromFollower::resetPing");
		break;
	case raft::receive::fromAnyNode2::clbkCmd:
		HandleSeedClbk(a_pNode);
		DEBUG_APP_WITH_NODE(2, NODE_KEY(a_pNode), "raft::receive::anyNode::clbkCmd");
		break;
	case raft::receive::fromAdder::newNode:
		DEBUG_APP_WITH_NODE(1, NODE_KEY(a_pNode), "raft::receive::fromAdder::newNode");
		if(!raft_receive_fromAdder_newNode(a_pNode,a_pBufferForReceive, a_pNodeKey)){
			ERROR_LOGGING2("Unable to get data from adder");
			return false;
		}
		break;
	case raft::receive::fromLeader2::removeNode:
		DEBUG_APP_WITH_NODE(1, NODE_KEY(a_pNode), "raft::receive::fromLeader::removeNode");
		if(!raft_receive_fromLeader_removeNode(a_pNode, a_pBufferForReceive, a_pNodeKey)){
			ERROR_LOGGING2("Unable to get data on node remove from leader");
			return false;
		}
		break;
	case raft::receive::fromNewLeader2::oldLeaderDied:
		if (is_leader()) {
			ERROR_LOGGING2("own node is leader, but node (%s:%d) tries to provide other leader", NODE_KEY(a_pNode)->ip4Address, (int)NODE_KEY(a_pNode)->port);
			return false;
		}
		DEBUG_APP_WITH_NODE(1, NODE_KEY(a_pNode), "raft::receive::fromNewLeader2::oldLeaderDied");
		break;
	default:
		ERROR_LOGGING2("default from node(%s:%d): requestNm=%d", NODE_KEY(a_pNode)->ip4Address,(int)NODE_KEY(a_pNode)->port,(int)a_cRequest);
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
		pNode = this->AddNode(a_pNodeKey, sizeof(NodeIdentifierKey), a_receivedData, false);
		DEBUG_APP_WITH_NODE(1, NODE_KEY(a_pNode), "Add node numberOfNodes=%d,  pNode=%p", this->nodesCount(), pNode);
		if(!pNode){return false;}
		break;
	case raft::receive::fromLeader2::removeNode:
		this->RemoveNode1(a_pNodeKey,sizeof(NodeIdentifierKey),a_receivedData);
		a_pNode = NULL;
		DEBUG_APP_WITH_NODE(1, NODE_KEY(a_pNode), "raft::receive::fromLeader::removeNode");
		break;
	default:
		return false;
	}

	return true;
}


void raft::tcp::Server::handleReceiveFromNodeAfterLock(char a_cRequest, RaftNode2* a_pNode, int32_t a_index, NodeIdentifierKey* a_pNodeKey, std::string* a_receivedData)
{
	PREPARE_SEND_SOCKET_GUARD();

	switch (a_cRequest)
	{
	case raft::receive::fromAdder::newNode:
		LOCK_SEND_SOCKET_MUTEX(a_pNode, raft::tcp::socketTypes::raft);
		GET_NODE_TOOLS(a_pNode)->writeC(raft::tcp::socketTypes::raft,a_pNode,m_nPortOwn,&g_ccResponceOk,1);
		UNLOCK_SEND_SOCKET_MUTEX();
		break;
	default:
		return;
	}

	return ;
}


/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
bool raft::tcp::Server::handleNewConnectionBeforeLock(common::SocketTCP& a_socket, const sockaddr_in& a_remoteAddr, char a_cRequest, NodeIdentifierKey* a_newNodeKey, std::string* a_pDataFromClient)
{

	bool bRet(false);

	switch (a_cRequest)
	{
	case raft::connect::toAnyNode2::newNode:
		bRet = raft_connect_toAnyNode_newNode(a_socket, &a_remoteAddr, a_pDataFromClient, a_newNodeKey);
		if (!bRet) {
			ERROR_LOGGING2("Unable to complete new node adding");
			return false;
		}
		DEBUG_APPLICATION(1, "raft::connect::toLeader::newNode");
		break;
	case raft::connect::toAnyNode2::permanentBridge:
		raft_connect_toAnyNode_permanentBridge(a_socket, &a_remoteAddr);
		DEBUG_APPLICATION(1, "raft::connect::toAnyNode2::permanentBridge");
		break;
	case raft::connect::fromClient2::allNodesInfo:
		raft_connect_fromClient_allNodesInfo(a_socket);
		DEBUG_APPLICATION(1, "raft::connect::fromClient::allNodesInfo");
		break;
	case raft::connect::toAnyNode2::otherLeaderFound:
		raft_connect_toAnyNode_otherLeaderFound(a_socket);
		DEBUG_APPLICATION(1, "raft::connect::toAnyNode::otherLeaderFound");
		break;
	default:
		break;
	}

	return bRet;

}


RaftNode2* raft::tcp::Server::handleNewConnectionLocked(common::SocketTCP& a_socket, const sockaddr_in&a_remoteAddr, char a_cRequest, NodeIdentifierKey* a_newNodeKey, std::string* a_pDataFromClient)
{
	RaftNode2* pNewNode = NULL;
	switch (a_cRequest)
	{
	case raft::connect::toAnyNode2::newNode:
		pNewNode = this->AddNode(a_newNodeKey, sizeof(NodeIdentifierKey), a_pDataFromClient, true);
		DEBUG_APPLICATION(1, "raft::connect::toLeader::newNode (pNewNode=%p)", pNewNode);
		if (!pNewNode) { return NULL; }
		break;
	default:
		DEBUG_APPLICATION(1, "Wrong case provided");
		break;
	}
	return pNewNode;
}


void raft::tcp::Server::handleNewConnectionAfterLock(common::SocketTCP& a_socket, const sockaddr_in&a_remoteAddr, char a_cRequest, NodeIdentifierKey* a_newNodeKey, 
													std::string* a_pDataToOthers, RaftNode2* a_pNodeToSkip)
{
	switch (a_cRequest)
	{
	case raft::connect::toAnyNode2::newNode:
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


void raft::tcp::Server::AddReceiveForWorker(RaftNode2* a_pNode, int32_t a_sockIndex)
{
	AddJobForWorkerPrivate(workRequest::handleReceive, 0,a_pNode, NULL, a_sockIndex);
}


void raft::tcp::Server::AddJobForWorkerPrivate(workRequest::Type a_type, char a_cRequest, RaftNode2* a_pNode, const sockaddr_in* a_pRemote, int32_t a_sockIndexOrSocket)
{
	SWorkerData aJobData(a_type);

	switch (a_type)
	{
	case workRequest::handleConnection:
		aJobData.pNode = NULL;
		aJobData.pear.con.remAddress = *a_pRemote;
		aJobData.pear.con.sockDescriptor = a_sockIndexOrSocket;
		break;
	case workRequest::handleInternal:
		aJobData.pNode = a_pNode;
		aJobData.pear.intr.cRequest = a_cRequest;
		break;
	case workRequest::handleReceive:
		aJobData.pNode = a_pNode;
		aJobData.pear.rcv.m_index = a_sockIndexOrSocket;
		break;
	default:
		break;
	}

	if(a_pNode){a_pNode->incrementLock2();}
	m_fifoWorker.AddElement2(std::move(aJobData));
	m_semaWorker.post();
}


void raft::tcp::Server::ThreadFunctionWorker()
{
	SWorkerData dataFromProducer(workRequest::none);
	NodeIdentifierKey nodeKey;
	std::string extraDataBuffer;
	
enterLoopPoint:
	try {
		while (m_nWork) {
			m_semaWorker.wait();

			while (m_fifoWorker.Extract(&dataFromProducer) && m_nWork) {

				switch (dataFromProducer.reqType) 
				{
				case raft::tcp::workRequest::handleConnection:
					HandleNewConnectionPrivate(dataFromProducer.pear.con.sockDescriptor, dataFromProducer.pear.con.remAddress,&nodeKey,&extraDataBuffer);
					break;
				case raft::tcp::workRequest::handleReceive:
					HandleReceiveFromNodePrivate(dataFromProducer.pNode, dataFromProducer.pear.rcv.m_index,&nodeKey,&extraDataBuffer);
					break;
				case raft::tcp::workRequest::handleInternal:
					HandleInternalPrivate(dataFromProducer.pear.intr.cRequest, dataFromProducer.pNode,&nodeKey,&extraDataBuffer);
					break;
				default:
					break;
				}				

			} // while (m_fifoWorker.Extract(&dataFromProducer) && m_nWork) {
		}  // while (m_nWork) {
	}
	catch(...){
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
	//PREPARE_SEND_SOCKET_GUARD();
	//RaftNode2* pNode;
	//timeb	aCurrentTime;
	//int nTimeDiff;
	//const char cRequest = raft::receive::fromFollower::resetPing;
	common::NewSharedLockGuard<STDN::shared_mutex> aShrdLockGuard;
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
#if 0
		cRequestReg = raft::response::ok;
		aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);
		pNode = firstNode();
		while(pNode){
			LOCK_SEND_SOCKET_MUTEX(pNode,raft::tcp::socketTypes::raft);
			GET_NODE_TOOLS(pNode)->writeC(raft::tcp::socketTypes::raft,pNode,m_nPortOwn,&cRequestReg, 1);
			UNLOCK_SEND_SOCKET_MUTEX();
			pNode = pNode->next;
		}
		aShrdLockGuard.UnsetAndUnlockMutex();
#endif

		if(m_pLeaderNode && (m_pLeaderNode!=m_thisNode)){
			GET_NODE_TOOLS(m_pLeaderNode)->writeC(raft::tcp::socketTypes::raft, m_pLeaderNode, m_nPortOwn, &g_ccResponceOk, 1);
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
		m_thisNode = this->AddNode(&aOwnHost, sizeof(NodeIdentifierKey), a_pAdderInfo, false);
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
	uint16_t snEndian;
	bool bOk(false);
	char cRequest;

	if(!ConnectAndGetEndian(&aSocket,a_nodeInfo,raft::connect::toAnyNode2::newNode,&isEndianDiffer)){goto returnPoint;}	// 1. connect, getEndian and sendRequest

	// !!!!!!!!!!!!!!!!!!!!!!! we found a node of cluster
	
	// 1. let's send the endian
	snEndian = 1;
	nSndRcv = aSocket.writeC(&snEndian, 2);
	if (nSndRcv != 2) { 
		DEBUG_APPLICATION(3,"Founded node (%s:%d) does not behave properly",a_nodeInfo.ip4Address,(int)a_nodeInfo.port);
		goto returnPoint;
	}

	// 2. let's send own port
	nSndRcv = aSocket.writeC(&m_nPortOwn,4);
	if (nSndRcv != 4) { 
		DEBUG_APPLICATION(3,"Founded node (%s:%d) does not behave properly",a_nodeInfo.ip4Address,(int)a_nodeInfo.port);
		goto returnPoint;
	}

	// 3. let's get the index of leader
	nSndRcv = aSocket.readC(&nLeaderIndex,4);
	if (nSndRcv != 4) { 
		DEBUG_APPLICATION(3,"Unable to get leader index from the node (%s:%d)",a_nodeInfo.ip4Address,(int)a_nodeInfo.port);
		goto returnPoint;
	}

	// 4. getting number of nodes in the cluster
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

	// 5. getting all nodes info
	nSndRcv = aSocket.readC(pNodesInfo, nBytesToReceive);
	if (nSndRcv != nBytesToReceive) { 
		DEBUG_APPLICATION(3, "Unable to get nodes info from the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint; 
	}

	if (isEndianDiffer) { for (i = 0; i<nNumberOfNodes; ++i) { SWAP4BYTES(pNodesInfo[i].port); } }

	m_pLeaderNode = NULL;
	for (i = 0; i<nNumberOfNodes; ++i) {
		pNode = this->AddNode(&pNodesInfo[i], sizeof(NodeIdentifierKey), NULL, false);
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

	// 6. sending extra data length to the adder node
	//    this is also trigger that adding all ndes info here is done
	nSndRcv = aSocket.writeC(&nExtraDataLenOut, 4);
	if (nSndRcv != 4) { 
		DEBUG_APPLICATION(3, "Unable to send extra data length to the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint; 
	}

	// 7. sending extra data itself to adder node
	//    this is also trigger that adding all ndes info here is done
	nSndRcv = aSocket.writeC(a_extraDataForAndFromAdder->data(),nExtraDataLenOut);// todo
	if (nSndRcv != nExtraDataLenOut) {
		DEBUG_APPLICATION(3, "Unable to send extra data length to the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint;
	}

	// 8. Wait confirmation that all nodes know about this node
	//    this is also trigger that adding all ndes info here is done
	//    meanwhile the size of extra data len
	nSndRcv = aSocket.readC(&cRequest,1);
	if ((nSndRcv != 1)||(cRequest!= raft::receive::fromAdder::toNewNodeAddPartitions)) {
		DEBUG_APPLICATION(3, "Unable to send extra data length to the node (%s:%d)", a_nodeInfo.ip4Address, (int)a_nodeInfo.port);
		goto returnPoint;
	}

	// 9. Wait confirmation that all nodes know about this node
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

	if (m_intrptSocketForRcv[a_index]>0) { closesocket(m_intrptSocketForRcv[a_index]);}

#ifdef _WIN32
    // something with windows?
#else
    if(m_rcvDataThread){pthread_kill(m_rcvThreadIds[a_index],SIGPIPE);}
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
		become_leader();
	}
	else{
		RaftServer::become_candidate();
	}

}


int raft::tcp::Server::SendClbkFunction(void *a_cb_ctx, void *udata, RaftNode2* a_pNode, int a_msg_type, const unsigned char *send_data,int d_len)
{
	PREPARE_SEND_SOCKET_GUARD();
	//RaftServerTcp* pServer = (RaftServerTcp*)a_cb_ctx;
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
	Server* pServer = (Server*)a_cb_ctx;
	NodeTools* pNodeTools = GET_NODE_TOOLS(a_pNode);
	NodeIdentifierKey* pNodeKey = NODE_KEY(a_pNode);
	struct timeb currentTime;
	int nSndRcv;
	int64_t nTimeoutOfLastSeen;
	char cRequest=raft::receive::fromAnyNode2::clbkCmd;
	bool bProblematic(false);

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
		if((nTimeoutOfLastSeen>MAX_UNSEEN_TIME_TO_CHANGE_STATE)&& pServer->is_leader()){
			pServer->AddInternalForWorker(raft::internal2::leader::removeNode,a_pNode);
			return 0;
		}
		break;
	case RAFT_MSG_REQUESTVOTE:
		if((nTimeoutOfLastSeen>MAX_UNSEEN_TIME_TO_CHANGE_STATE) && pServer->is_candidate()){
			a_pNode->setUnableToVote();
			pServer->become_candidate();  // todo: change state in locked part
		}
		break;
	default:
		break;
	}

	if (nTimeoutOfLastSeen>MIN_UNSEEN_TIME_TO_DISPLAY) {
		DEBUG_APP_WITH_NODE(1, pNodeKey, "node is not responding for %d ms", (int)nTimeoutOfLastSeen);
	}

	LOCK_SEND_SOCKET_MUTEX(a_pNode,raft::tcp::socketTypes::raft);
	nSndRcv=pNodeTools->writeC(raft::tcp::socketTypes::raft,a_pNode,pServer->m_nPortOwn,&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}
	nSndRcv= pNodeTools->writeC(raft::tcp::socketTypes::raft, a_pNode, pServer->m_nPortOwn,&a_msg_type,4);
	if(nSndRcv!=4){goto returnPoint;}
	nSndRcv= pNodeTools->writeC(raft::tcp::socketTypes::raft, a_pNode, pServer->m_nPortOwn, send_data, d_len);
	UNLOCK_SEND_SOCKET_MUTEX();

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
			raft::tcp::g_nApplicationRun = 0;
			if (s_mainThreadHandle) { QueueUserAPC(PAPCFUNC_static,s_mainThreadHandle,NULL ); }
			//pServer->server->StopServer();
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

int raft::tcp::NodeTools::writeC(int32_t a_index, RaftNode2* a_pNode, int32_t a_nPort, const void* a_data, int a_dataLen)
{
	if(!VALIDATE_INDEX_INVALID(this,a_index)){
		ERROR_LOGGING2("Wrong index provided (index=%d)",(int)a_index);
		return -1;
	}

	if(!m_sockets[a_index].isOpenC()){
		int nSndRcv;
		uint32_t  isEndianDiffer;
		uint16_t  snEndian(1);
		if(!ConnectAndGetEndian(&m_sockets[a_index], *NODE_KEY(a_pNode),raft::connect::toAnyNode2::permanentBridge,&isEndianDiffer)){
			m_sockets[a_index].closeC(); 
			return -1;
		}
		nSndRcv = m_sockets[a_index].writeC(&snEndian,2);
		if (nSndRcv != 2) { m_sockets[a_index].closeC(); return -3; }
		nSndRcv=m_sockets[a_index].writeC(&a_nPort, 4);
		if(nSndRcv!=4){m_sockets[a_index].closeC();return -3;}
		nSndRcv = m_sockets[a_index].writeC(&a_index, 4);
		if (nSndRcv != 4) { m_sockets[a_index].closeC(); return -4; }
		this->isEndianDiffer = isEndianDiffer;
	}

	return m_sockets[a_index].writeC(a_data, a_dataLen);
}


int raft::tcp::NodeTools::readC(int32_t a_index, void* a_buffer, int a_bufferLen)
{
	if (!VALIDATE_INDEX_INVALID(this, a_index)) {
		ERROR_LOGGING2("Wrong index provided (index=%d)", (int)a_index);
		return -1;
	}
	return m_sockets[a_index].readC(a_buffer, a_bufferLen);
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
