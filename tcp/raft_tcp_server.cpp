
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

#define NO_LEADER_KEY_PORT		-1

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
	m_isInited = 0;

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


void raft::tcp::Server::StateChangedBeforeLock(SAddRemData* )
{
}


void raft::tcp::Server::StateChangedLockedPre(SAddRemData*)
{
}


void raft::tcp::Server::StateChangedLockedPost(SAddRemData*)
{
}


void raft::tcp::Server::StateChangedAfterLock(SAddRemData*)
{
}


void raft::tcp::Server::SignalHandler(int )
{
}


void raft::tcp::Server::newNode_prepareInfo_forLeader(std::string* )
{
}


void raft::tcp::Server::AddAdditionalDataToNode(RaftNode2*)
{
}


void raft::tcp::Server::CleanNodeData(RaftNode2* a_node)
{
	NodeTools* pNodeTool = GET_NODE_TOOLS(a_node);

	if(pNodeTool){delete pNodeTool;}
	RaftServer::CleanNodeData(a_node);
}


void raft::tcp::Server::ReceiveFromDataSocket(RaftNode2*) // this should be overriten by child
{
}


int raft::tcp::Server::RunServerOnOtherThreads(const std::vector<NodeIdentifierKey2>& a_vectPossibleNodes, int a_nWorkersCount, int a_nRaftPort)
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
	m_threadLockedActions = std::thread(&Server::ThreadFunctionLockedAction, this);
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

    m_threadLockedActions.join();
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
	m_fifoWorker.AddElement1(aWorkerData);
	m_semaWorker.post();

}


void raft::tcp::Server::raft_connect_toAnyNode_leaderInfoRequest(common::SocketTCP& a_clientSock)
{
	int nNodeKeyLen;
	if(m_pLeaderNode){
		nNodeKeyLen = NODE_KEY_LEN(m_pLeaderNode);
		a_clientSock.writeC(&nNodeKeyLen, 4);
		a_clientSock.writeC(NODE_KEY2(m_pLeaderNode), nNodeKeyLen);
	}
	else{
		NodeIdentifierKey2 noLeaderKey;
		noLeaderKey.set_ip4addressAndPort("no_leader", NO_LEADER_KEY_PORT);
		nNodeKeyLen = (int)noLeaderKey.key().size();
		a_clientSock.writeC(&nNodeKeyLen, 4);
		a_clientSock.writeC(noLeaderKey.key().data(), nNodeKeyLen);
	}
}


bool raft::tcp::Server::raft_connect_toLeader_newNode(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr, SAddRemData* a_clbkData, int* a_pnEndianDiffer)
{
	// struct NodeIdentifierKey { char ip4Address[MAX_IP4_LEN]; int32_t port;};
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
    char				*pAllNodesInfoC=NULL;
	RaftNode2*			pNode;
	NodeTools*			pNewNodeTool = NULL;
	int&  nEndianDiffer = *a_pnEndianDiffer;
    int nNodesCount,nTotalSize;
	int nSndRcv;
	int32_t  nPort;
	uint16_t unRemEndian;
	bool bOk(false);
	char vcIp4Address[MAX_IP4_LEN2];

	if (!is_leader()) {
		ERROR_LOGGING2("This node is not leader, but reques is done for leader!");
		return false;
	}

	nEndianDiffer = 0;
	if(strcmp(common::socketN::GetIPAddress(a_remoteAddr),"127.0.0.1")==0){
		common::socketN::GetOwnIp4Address(vcIp4Address, MAX_IP4_LEN2);
	}
	else{
		strncpy(vcIp4Address, common::socketN::GetIPAddress(a_remoteAddr), MAX_IP4_LEN2);
	}

	a_clbkData->pNode = NULL;

	nSndRcv= a_clientSock.readC(&unRemEndian,2);
	if(nSndRcv!= 2){ goto returnPoint;}
	if(unRemEndian!=1){ nEndianDiffer=1;}

	nSndRcv= a_clientSock.readC(&nPort,4);
	if(nSndRcv!= 4){ goto returnPoint;}
	if(nEndianDiffer){ SWAP4BYTES(nPort);}

	a_clbkData->nodeKey.set_ip4addressAndPort(vcIp4Address, nPort);
	
	// first let's check if the node does not exist
	if(m_Nodes.FindEntry(a_clbkData->nodeKey.key().data(),a_clbkData->nodeKey.key().size(),&pNode)){
		// send 0, as a sign, that node exists
		nTotalSize = raft::response::error::nodeExist;
		a_clientSock.writeC(&nTotalSize, 4);
		goto returnPoint;
	}

	pNewNodeTool = new NodeTools;
	HANDLE_MEM_DEF2(pNewNodeTool, " ");
	pNewNodeTool->isEndianDiffer = nEndianDiffer;

	nNodesCount = m_Nodes.count();
	pAllNodesInfoC=CollectAllNodesDataNotThrSafe(&nTotalSize,NULL);

	// add this new node here
	pNewNodeTool->raftSocket.SetNewSocketDescriptor(a_clientSock);
	a_clbkData->pNode = new RaftNode2(pNewNodeTool);
	HANDLE_MEM_DEF2(a_clbkData->pNode," ");
	//addNodeData.action = raft::internal2::leader::newNode;

	nSndRcv = a_clientSock.writeC(&nNodesCount, 4);
	if (nSndRcv != 4) { goto returnPoint; }

	nSndRcv = a_clientSock.writeC(pAllNodesInfoC, nTotalSize);
	if (nSndRcv != nTotalSize) { goto returnPoint; }
	free(pAllNodesInfoC); pAllNodesInfoC = NULL;

	bOk = true;
returnPoint:
	if (bOk) {a_clientSock.ResetSocketWithoutClose();}
	else { 
		delete pNewNodeTool;
		delete a_clbkData->pNode;
		a_clbkData->pNode = NULL;
		free(pAllNodesInfoC);
	}
	return bOk;
}


void raft::tcp::Server::raft_connect_toFollower_raftBridge(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)
{
	// this socket should remain 
	RaftNode2* pNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey2 aRemHost;
	std::string strBuffForAddInfo;
	int nEndianDiffer(0);
	int nSndRcv;
	int32_t nPort;
	uint16_t unRemEndian;
	char vcIp4Address[MAX_IP4_LEN2];

	if(is_leader()){
		ERROR_LOGGING2(" ");
		return;
	}

	if (strcmp(common::socketN::GetIPAddress(a_remoteAddr), "127.0.0.1") == 0) {
		common::socketN::GetOwnIp4Address(vcIp4Address, MAX_IP4_LEN2);
	}
	else {
		strncpy(vcIp4Address, common::socketN::GetIPAddress(a_remoteAddr), MAX_IP4_LEN2);
	}

	//aRemHost.set_ip4Address(common::socketN::GetIPAddress(a_remoteAddr));	// let us specify host IP

	nSndRcv = a_clientSock.readC(&unRemEndian,2);							// endian
	if(nSndRcv!=2){
		DEBUG_APPLICATION(0,"Unable to read endian from %s host", vcIp4Address);
		return;
	}
	if(unRemEndian!=1){nEndianDiffer=1;}

	nSndRcv = a_clientSock.readC(&nPort,4);							// port
	if(nSndRcv!=4){
		DEBUG_APPLICATION(0, "Unable to read port from %s host", vcIp4Address);
		return;
	}
	if(nEndianDiffer){SWAP4BYTES(nPort);}
	aRemHost.set_ip4addressAndPort(vcIp4Address, nPort);

	if(!m_Nodes.FindEntry(aRemHost.key().data(), aRemHost.key().size(),&pNode)){
		DEBUG_APPLICATION(0, "Unable to find specified node (%s:%d)",aRemHost.ip4Address(),(int)aRemHost.port());
		return;
	}
	
	pNodeTools = GET_NODE_TOOLS(pNode);
	pNodeTools->isEndianDiffer = nEndianDiffer;
	
	pNodeTools->raftSocket.SetNewSocketDescriptor(a_clientSock);
	a_clientSock.writeC(&g_ccResponceOk, 1);
	a_clientSock.ResetSocketWithoutClose();
	pNode->setUsable();

}


char* raft::tcp::Server::CollectAllNodesDataNotThrSafe(int* a_pnTotalSize, int* a_pnLeaderIndex)
{
	RaftNode2* pNode;
	char  *pAllNodesInfoRet, *pAllNodesInfoTmp;
	const char *cpExistingNodeKey;
	int i(0);
	const int cnSingleSize(MAX_IP4_LEN2 + 4);

	*a_pnTotalSize = cnSingleSize *m_Nodes.count();
	
	pAllNodesInfoRet = (char*)malloc(*a_pnTotalSize);
	HANDLE_MEM_DEF2(pAllNodesInfoRet, " ");
	pAllNodesInfoTmp = pAllNodesInfoRet;

	// collect info
	pNode = m_Nodes.first();
	while (pNode) {
		cpExistingNodeKey = NODE_KEY2(pNode);
		memcpy(pAllNodesInfoTmp, cpExistingNodeKey, cnSingleSize);
		if(a_pnLeaderIndex && pNode->is_leader()){*a_pnLeaderIndex = i;}
		pNode = pNode->next;
		++i;
		pAllNodesInfoTmp += cnSingleSize;
	}

	return pAllNodesInfoRet;
}


void raft::tcp::Server::raft_connect_fromClient_allNodesInfo(common::SocketTCP& a_clientSock)
{
	struct { int nodesCount, leaderIndex; }nl;
	char *pAllNodesInfo;
	int nSndRcv,nTotalSize;

	nl.nodesCount = m_Nodes.count();
	pAllNodesInfo = CollectAllNodesDataNotThrSafe(&nTotalSize,&nl.leaderIndex);

	if(!pAllNodesInfo){return;}

	nSndRcv=a_clientSock.writeC(&nl,8);
	if(nSndRcv!=8){free(pAllNodesInfo);return;}

	a_clientSock.writeC(pAllNodesInfo,nTotalSize);
	free(pAllNodesInfo);
}


void raft::tcp::Server::raft_connect_toAnyNode_otherLeaderFound(common::SocketTCP& a_clientSock)
{
	NodeIdentifierKey2 newLeaderKey;
	int nSndRcv;
	uint32_t isEndianDiffer;
	uint16_t unRemEndian;

	nSndRcv=a_clientSock.readC(&unRemEndian, 2);
	if(nSndRcv!=2){return;}
	if (unRemEndian == 1) { isEndianDiffer = 0; }
	else{ isEndianDiffer = 1; }

	nSndRcv=a_clientSock.readC(newLeaderKey.data(), NODE_KEY_DATA_LEN(newLeaderKey));
	if(nSndRcv!=NODE_KEY_DATA_LEN(newLeaderKey)){return;}
	if(isEndianDiffer){ newLeaderKey.swapPort();}

	if( newLeaderKey.isSameNode1(NODE_KEY2(m_pLeaderNode),NODE_KEY_LEN(m_pLeaderNode))  ){
		a_clientSock.writeC("o", 1);
	}
	else {
		a_clientSock.writeC("e", 1);
		DEBUG_APP_WITH_KEY(0, newLeaderKey.data(), " [this is a correct leader]");
		// todo: make steps to connect to real leader
	}
}


void raft::tcp::Server::raft_connect_toAnyNode_dataBridge(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)
{
	// this socket should remain 
	RaftNode2* pNode;
	NodeTools* pNodeTools;
	NodeIdentifierKey2 aRemHost;
	int nSndRcv, nEndianDiffer(0);
	int32_t nPort;
	uint16_t unRemEndian;
	char vcIp4Address[MAX_IP4_LEN2];

	if (strcmp(common::socketN::GetIPAddress(a_remoteAddr), "127.0.0.1") == 0) {
		common::socketN::GetOwnIp4Address(vcIp4Address, MAX_IP4_LEN2);
	}
	else {
		strncpy(vcIp4Address, common::socketN::GetIPAddress(a_remoteAddr), MAX_IP4_LEN2);
	}

	nSndRcv = a_clientSock.readC(&unRemEndian,2);							// endian
	if(nSndRcv!=2){
		DEBUG_APPLICATION(0, "Unable to read %s host endian", vcIp4Address);
		return;
	}
	if(unRemEndian!=1){nEndianDiffer=1;}

	nSndRcv = a_clientSock.readC(&nPort,4);							// port
	if(nSndRcv!=4){
		DEBUG_APPLICATION(0, "Unable to read port name from the %s host", vcIp4Address);
		return;
	}
	if(nEndianDiffer){SWAP4BYTES(nPort);}

	aRemHost.set_ip4addressAndPort(vcIp4Address, nPort);
	if(!m_Nodes.FindEntry(aRemHost.key().data(), aRemHost.key().size(),&pNode)){
		DEBUG_APPLICATION(0, "Unable to find node(%s:%d)", vcIp4Address, aRemHost.port());
		return;
	}
	
	pNodeTools = GET_NODE_TOOLS(pNode);
	pNodeTools->isEndianDiffer = nEndianDiffer;
	pNodeTools->dataSocket.SetNewSocketDescriptor(a_clientSock);

	a_clientSock.writeC(&g_ccResponceOk, 1);
	a_clientSock.ResetSocketWithoutClose();

	InterruptDataRcv();

	return;
}


void raft::tcp::Server::ThreadFunctionListen()
{
	m_serverTcp.setTimeout(SOCK_TIMEOUT_MS);
	m_serverTcp.StartServer(this, &raft::tcp::Server::AddClient,m_nPortOwn);
}


void raft::tcp::Server::HandleSeedClbk(RaftNode2* a_pNode)
{
	NodeTools* pTools = GET_NODE_TOOLS(a_pNode);
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
		DEBUG_APP_WITH_NODE1(1, a_pNode,"RAFT_MSG_REQUESTVOTE");
		nSndRcv = pTools->raftSocket.readC(&reqVote, sizeof(msg_requestvote_t));
		if (nSndRcv != sizeof(msg_requestvote_t)) { goto returnPoint; }
		recv_requestvote(a_pNode, &reqVote);
	}
	break;
	case RAFT_MSG_REQUESTVOTE_RESPONSE:
	{
		msg_requestvote_response_t  reqVoteResp;
		DEBUG_APP_WITH_NODE1(1, a_pNode,"RAFT_MSG_REQUESTVOTE_RESPONSE");
		nSndRcv = pTools->raftSocket.readC(&reqVoteResp, sizeof(msg_requestvote_response_t));
		if (nSndRcv != sizeof(msg_requestvote_response_t)) { goto returnPoint; }
		a_pNode->pingReceived();
		recv_requestvote_response(a_pNode, &reqVoteResp);
	}
	break;
	case RAFT_MSG_APPENDENTRIES:
	{
		MsgAppendEntries2 appEntries;
		DEBUG_APP_WITH_NODE1(3, a_pNode,"RAFT_MSG_APPENDENTRIES");
		nSndRcv = pTools->raftSocket.readC(&appEntries, SIZE_OF_INITIAL_RCV_OF_MSG_APP);
		if (nSndRcv != SIZE_OF_INITIAL_RCV_OF_MSG_APP) { goto returnPoint; }
		if(appEntries.getNEntries()){
			nToReceive = appEntries.getNEntries() * sizeof(msg_entry_t);
			nSndRcv=pTools->raftSocket.readC(appEntries.entries(),nToReceive);
			if (nSndRcv != nToReceive) { goto returnPoint; }
		}
		recv_appendentries(true,a_pNode, &appEntries);
		ftime(&(this->m_lastPingByLeader));
	}
	break;
	case RAFT_MSG_APPENDENTRIES_RESPONSE:
	{
		msg_appendentries_response_t aApndResp;
		DEBUG_APP_WITH_NODE1(3, a_pNode,"RAFT_MSG_APPENDENTRIES_RESPONSE");
		nSndRcv = pTools->raftSocket.readC(&aApndResp, sizeof(msg_appendentries_response_t));
		if (nSndRcv != sizeof(msg_appendentries_response_t)) { goto returnPoint; }
		a_pNode->pingReceived();
		this->recv_appendentries_response(a_pNode, &aApndResp);
		// a_anyNode->pingReceived(); // this does not work because of exception
	}
	break;
	default:
		DEBUG_APP_WITH_NODE1(0, a_pNode,"raft-receive: default:");
		goto returnPoint;
	}

	bProblematic = false;
	a_pNode->pingReceived();
returnPoint:
	if (bProblematic) { a_pNode->setProblematic(); }
}


bool raft::tcp::Server::raft_receive_fromLeader_removeNode(RaftNode2* a_pNode, SAddRemData*	a_clbkData)
{
	NodeTools *pTools = GET_NODE_TOOLS(a_pNode);
	SAddRemData&	 nodeData = *a_clbkData;
	int nSndRcv ;
	bool bOk(false);

	if (a_pNode != m_pLeaderNode) {
		ERROR_LOGGING2("node (%s:%d) is not leader, but tries to confirm leader action", IP_ADDRESS_OF_NODE(a_pNode), (int)PORT_FROM_NODE(a_pNode));
		goto returnPoint;
	}
	if (!is_follower()) {
		DEBUG_APP_WITH_NODE1(0,a_pNode,"own node is not follower, but request is for follower");
		goto returnPoint;
	}

	if (a_pNode != m_pLeaderNode) { goto returnPoint; }
	nSndRcv = pTools->raftSocket.readC(nodeData.nodeKey.data(), NODE_KEY_DATA_LEN(nodeData.nodeKey));
	if (nSndRcv != NODE_KEY_DATA_LEN(nodeData.nodeKey)) {
		ERROR_LOGGING2("Unable to get info from leader");
		goto returnPoint; 
	}
	if (pTools->isEndianDiffer) { nodeData.nodeKey.swapPort(); }

	if (!m_Nodes.FindEntry(nodeData.nodeKey.key().data(), nodeData.nodeKey.key().size(), &nodeData.pNode)) {
		ERROR_LOGGING2("Unable to find node (%s:%d) for deletion provided by leader", HOST_NAME_FROM_KEY(nodeData.nodeKey.data()), PORT_FROM_KEY(nodeData.nodeKey.data()));
		goto returnPoint;
	}

	nodeData.action = raft::internal2::follower::removeNodeRequestFromLeader;
	
	bOk = true;
returnPoint:
	if(!bOk){
		a_pNode->setProblematic();
		// todo :
		//ERROR_LOGGING2("not able to remove node (%s:%d). Request comes from node (%s:%d) ", 
		//	nodeData.nodeKey.set_ip4Address.ip4Address, (int)nodeData.nodeKey.set_ip4Address.port,
		//	NODE_KEY(a_pNode)->ip4Address, (int)NODE_KEY(a_pNode)->port);
	}
	return bOk;
}


bool raft::tcp::Server::raft_receive_fromLeader_newNode_private(common::SocketTCP& a_socket, int a_isEndianDiffer, SAddRemData* a_clbkData)
{
	PREPARE_SEND_SOCKET_GUARD();
	RaftNode2* pNewNode = NULL;
	NodeTools* pNewNodeTools=NULL;
	SAddRemData& nodeData = *a_clbkData;
	int nSndRcv;
	bool bOk(false);

	nodeData.pNode = NULL;

	nSndRcv = a_socket.readC(nodeData.nodeKey.data(), NODE_KEY_DATA_LEN(nodeData.nodeKey));
	if (nSndRcv != NODE_KEY_DATA_LEN(nodeData.nodeKey)) {goto returnPoint; }
	if (a_isEndianDiffer) { nodeData.nodeKey.swapPort(); }

	if(nodeData.nodeKey.isSameNode1(NODE_KEY2(m_thisNode),NODE_KEY_LEN(m_thisNode))){
		nodeData.pNode = m_thisNode;
	}
	else{
		pNewNodeTools = new NodeTools;
		HANDLE_MEM_DEF2(pNewNodeTools, " ");
		pNewNode=nodeData.pNode = new RaftNode2(pNewNodeTools);
		HANDLE_MEM_DEF2(nodeData.pNode, " ");
	}

	if (!m_isInited) {
		LOCK_RAFT_SEND_MUTEX(a_pNode);
		a_socket.writeC(&g_ccResponceOk, 1);
		UNLOCK_SEND_SOCKET_MUTEX2();
		m_isInited = 1;
	}
	bOk = true;
returnPoint:
	if(!bOk){
		delete pNewNode;
		delete pNewNodeTools;
		nodeData.pNode = NULL;
	}
	return bOk;
}


bool raft::tcp::Server::raft_receive_fromLeader_newNode(RaftNode2* a_pNode, SAddRemData* a_clbkData)
{
	PREPARE_SEND_SOCKET_GUARD();
	RaftNode2* pNewNode = NULL;
	NodeTools* pNewNodeTools=NULL;
	NodeTools *pTools = GET_NODE_TOOLS(a_pNode);
	SAddRemData& nodeData = *a_clbkData;
	bool bOk(true);

	nodeData.pNode = NULL;
	if (a_pNode != m_pLeaderNode) {
		ERROR_LOGGING2("node (%s:%d) is not leader, but tries to confirm leader action", IP_ADDRESS_OF_NODE(a_pNode), (int)PORT_FROM_NODE(a_pNode));
		goto returnPoint;
	}
	if (!is_follower()) {
		DEBUG_APP_WITH_NODE1(0,a_pNode, "own node is not follower, but request is for follower");
		goto returnPoint;
	}

	nodeData.action = raft::internal2::follower::newNodeFromLeader;
	bOk=raft_receive_fromLeader_newNode_private(pTools->raftSocket,pTools->isEndianDiffer,a_clbkData);

	if (!m_isInited && bOk) {
		LOCK_RAFT_SEND_MUTEX(a_pNode);
		GET_NODE_TOOLS(a_pNode)->raftSocket.writeC(&g_ccResponceOk, 1);
		UNLOCK_SEND_SOCKET_MUTEX2();
		m_isInited = 1;
	}
returnPoint:
	if(!bOk){
		ERROR_LOGGING2("is not able to get new data from leader node (%s:%d) ", IP_ADDRESS_OF_NODE(a_pNode), (int)PORT_FROM_NODE(a_pNode));
	}
	return bOk;
}


void raft::tcp::Server::ReceiveFromRaftSocket(RaftNode2* a_pNode)
{
	NodeTools *pTools = GET_NODE_TOOLS(a_pNode);
	SAddRemData clbkData;
	int nSndRcv;
	char cRequest;
	bool bWakeLocked(false);

	nSndRcv = pTools->raftSocket.readC(&cRequest,1);
	if(nSndRcv!=1){a_pNode->setProblematic();return;}

	switch (cRequest)
	{
	case raft::response::ok:
		++pTools->okCount;
		a_pNode->setUsable();
		a_pNode->pingReceived();
		break;
	case raft::receive::fromFollower::resetPing:
		a_pNode->setUsable();
		a_pNode->pingReceived();
		DEBUG_APP_WITH_NODE1(2, a_pNode, "raft::receive::fromFollower::resetPing");
		break;
	case raft::receive::fromAnyNode2::clbkCmd:
		HandleSeedClbk(a_pNode);
		DEBUG_APP_WITH_NODE1(2, a_pNode, "raft::receive::anyNode::clbkCmd");
		break;
	case raft::receive::fromLeader2::newNode:
		bWakeLocked=raft_receive_fromLeader_newNode(a_pNode,&clbkData);
		DEBUG_APP_WITH_NODE1(1, a_pNode, "raft::receive::fromLeader::newNode");
		break;
	case raft::receive::fromLeader2::removeNode:
		bWakeLocked=raft_receive_fromLeader_removeNode(a_pNode,&clbkData);
		DEBUG_APP_WITH_NODE1(1, a_pNode, "raft::receive::fromLeader::removeNode");
		break;
	case raft::receive::fromNewLeader2::oldLeaderDied:
		if (is_leader()) {
			ERROR_LOGGING2("own node is leader, but node (%s:%d) tries to provide other leader", IP_ADDRESS_OF_NODE(a_pNode), (int)PORT_FROM_NODE(a_pNode));
			return;
		}

		clbkData.action = raft::internal2::follower::oldLeaderDied;
		clbkData.pNode = a_pNode;

		bWakeLocked = true;
		DEBUG_APP_WITH_NODE1(1, a_pNode, "raft::receive::fromNewLeader2::oldLeaderDied");
		break;
	default:
		DEBUG_APP_WITH_NODE1(0, a_pNode, "default: (num=%d)", (int)cRequest);
		break;
	}

	if(bWakeLocked){

		if(GetAdditionalData(pTools->raftSocket, pTools->isEndianDiffer, &clbkData.additionalData)){
			m_fifoAddDel.AddElement2(clbkData);
			m_semaAddRemove.post();
		}
	}

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
					pNodeTools = GET_NODE_TOOLS(pNode);
					nCurrentSocket = a_bIsRaft ? pNodeTools->raftSocket : pNodeTools->dataSocket;
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
					pNodeTools = GET_NODE_TOOLS(pNode);
					nCurrentSocket = a_bIsRaft ? pNodeTools->raftSocket : pNodeTools->dataSocket;
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
	SAddRemData clbkData;
	common::SocketTCP aClientSock;
	SWorkerData dataFromProducer;
	common::NewSharedLockGuard<STDN::shared_mutex> aShrdLockGuard;
    int nSndRcv, nEndianDiffer(0);
	int16_t	snEndian;
	char vcHostName[MAX_HOSTNAME_LENGTH];
	char cRequest;
	bool bWakeLocked, bOk;
	
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
					ERROR_LOGGING2("Could not send the endian of the connected pear nSndRcv=%d, socket=%d",nSndRcv, dataFromProducer.sockDescriptor);
					aClientSock.closeC();
					continue;
				}

				nSndRcv = aClientSock.readC(&cRequest, 1);																// 4. rcv request
				if (nSndRcv != 1) {
					ERROR_LOGGING2("Unable to read request type");
					aClientSock.closeC();
					continue;
				}

				bWakeLocked = false;
				aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);													// --> shared lock
				switch (cRequest)
				{
				case raft::connect::toAnyNode2::leaderInfoRequest:
					raft_connect_toAnyNode_leaderInfoRequest(aClientSock);
					bWakeLocked = false;
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode::newNode");
					break;
				case raft::connect::toLeader2::newNode:
					bOk=raft_connect_toLeader_newNode(aClientSock,&dataFromProducer.remAddress,&clbkData, &nEndianDiffer);
					bWakeLocked = true;
					DEBUG_APPLICATION(1, "raft::connect::toLeader::newNode");
					break;
				case raft::connect::toFollower2::raftBridge:
					raft_connect_toFollower_raftBridge(aClientSock, &dataFromProducer.remAddress);
					DEBUG_APPLICATION(1, "raft::connect::toFollower2::raftBridge");
					break;
				case raft::connect::toAnyNode2::dataBridge:
					raft_connect_toAnyNode_dataBridge(aClientSock, &dataFromProducer.remAddress);
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode2::dataBridge");
					break;
				case raft::connect::fromClient2::allNodesInfo:
					raft_connect_fromClient_allNodesInfo(aClientSock);
					DEBUG_APPLICATION(1, "raft::connect::fromClient::allNodesInfo");
					break;
				case raft::connect::toAnyNode2::otherLeaderFound:
					raft_connect_toAnyNode_otherLeaderFound(aClientSock);
					DEBUG_APPLICATION(1, "raft::connect::toAnyNode::otherLeaderFound");
					break;
				default:
					bWakeLocked = HandleNonDefaultConnection(cRequest, aClientSock, &dataFromProducer.remAddress, pNode);
					break;
				}
				if(bOk && bWakeLocked){

					if (GetAdditionalData(aClientSock, nEndianDiffer, &clbkData.additionalData)) {
						m_fifoAddDel.AddElement2(clbkData);
						m_semaAddRemove.post();
					}

				}
				aShrdLockGuard.UnsetAndUnlockMutex();								// --> shared unlock
				aClientSock.closeC();
			} // while (m_fifoWorker.Extract(&dataFromProducer) && m_nWork) {
		}  // while (m_nWork) {
	}
	catch(...){
		goto enterLoopPoint;
	}
}


bool raft::tcp::Server::HandleNonDefaultConnection(char,common::SocketTCP&, const sockaddr_in*, RaftNode2*)
{
	// this function should be overritten
}


void raft::tcp::Server::ThreadFunctionLockedAction()
{
	PREPARE_SEND_SOCKET_GUARD();
	RaftNode2* pSkipNode(NULL);
	RaftNode2* pNextNode;
	NodeTools* pNodeTools;
	NodeTools* pNodeToolsLeader;
	const char* pKeyToInformC;
	SAddRemData aData;
	common::NewSharedLockGuard<STDN::shared_mutex> aShrdLockGuard;
	common::NewLockGuard<STDN::shared_mutex> aLockGuard;
	int nSndRcv, nIter, nAdditionalDataLen;
	uint32_t unOkCount;
	char cRequestOut, cRequestIn;
	bool bInformFollowers, bWaitDone, bSendBack;
	
enterLoopPoint:
	try {
		while (m_nWork) {
			m_semaAddRemove.wait();

			while (m_fifoAddDel.Extract(&aData) && m_nWork) {

				pKeyToInformC = NULL;

				aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);    // --> shared locking
				StateChangedBeforeLock(&aData);
				aShrdLockGuard.UnsetAndUnlockMutex();    // --> shared unlocking

				aLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);   // --> locking
				StateChangedLockedPre(&aData);
				switch (aData.action)
				{
				case raft::internal2::leader::newNode:
					AddAdditionalDataToNode(aData.pNode);
					m_Nodes.AddData(aData.pNode, aData.nodeKey.data(), aData.nodeKey.key().size());
					DEBUG_APPLICATION(1, "Node (add): %s:%d, numOfNodes=%d", aData.nodeKey.ip4Address(), (int)aData.nodeKey.port(), m_Nodes.count());
					break;
				case raft::internal2::follower::newNodeFromLeader:
					if(aData.pNode!=m_thisNode){
						AddAdditionalDataToNode(aData.pNode);
						m_Nodes.AddData(aData.pNode, aData.nodeKey.data(), aData.nodeKey.key().size());
						DEBUG_APPLICATION(1, "Node (add): %s:%d, numOfNodes=%d", aData.nodeKey.ip4Address(), (int)aData.nodeKey.port(), m_Nodes.count());
					}
					break;
				case raft::internal2::follower::oldLeaderDied:
					DEBUG_APPLICATION(1, "old leader (%s:%d) will be deleted", IP_ADDRESS_OF_NODE(m_pLeaderNode),(int)PORT_FROM_NODE(m_pLeaderNode));
					DEBUG_APPLICATION(1, "New leader is  %s:%d. NodesCount=%d", IP_ADDRESS_OF_NODE(aData.pNode), (int)PORT_FROM_NODE(aData.pNode), m_Nodes.count() - 1);
					this->RemoveNode2(m_pLeaderNode);
					m_pLeaderNode = aData.pNode;
					m_pLeaderNode->makeLeader();
					this->become_follower();
					break;
				case raft::internal2::leader::removeNode: case raft::internal2::follower::removeNodeRequestFromLeader:
					this->RemoveNode2(aData.pNode);
					DEBUG_APPLICATION(1, "Node (del): %s:%d, numOfNodes=%d", IP_ADDRESS_OF_NODE(aData.pNode), (int)PORT_FROM_NODE(aData.pNode), m_Nodes.count() - 1);
					break;
				case raft::internal2::newLeader::becomeLeader:
					DEBUG_APP_WITH_NODE1(0, m_pLeaderNode, "old leader died");
					this->RemoveNode2(m_pLeaderNode);
					DEBUG_APPLICATION(0, "This node will be the leader (numberOfNodes=%d)", m_Nodes.count());
					m_pLeaderNode = m_thisNode;
					m_pLeaderNode->makeLeader();
					break;
				default:
					DEBUG_APPLICATION(3, "default");
					break;
				}
				StateChangedLockedPost(&aData);
				aLockGuard.UnsetAndUnlockMutex();							// --> unlocking (after this point we have parallel stream)


				bSendBack = false;
				nAdditionalDataLen = 0;
	
				aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes2);    // --> shared locking

				switch (aData.action)
				{
				case raft::internal2::newLeader::becomeLeader:
					cRequestOut = raft::receive::fromNewLeader2::oldLeaderDied;
					bInformFollowers = true;
					bWaitDone = false;
					pSkipNode = m_pLeaderNode;
					//newLeader_prepareInform_on_oldLeader_died(&strAdditionalData); // this is done in StatChange
					break;
				case raft::internal2::leader::newNode:
					cRequestOut = raft::receive::fromLeader2::newNode;
					bInformFollowers = true;
					bWaitDone = true;
					pSkipNode = NULL;
					pKeyToInformC = NODE_KEY2(aData.pNode);
					//leader_prepareInform_on_newNode(&strAdditionalData, &bSendBack);   // this is done in StatChange
					break;
				case raft::internal2::leader::removeNode:
					cRequestOut = raft::receive::fromLeader2::removeNode;
					bInformFollowers = true;
					bWaitDone = false;
					//pSkipNode = aData.pNode;
					break;
				default:
					bInformFollowers = false;
					break;
				}  // switch (aData.action)

				if(bSendBack){pSkipNode=NULL;}

				if (bInformFollowers) {
					nAdditionalDataLen = (int)aData.additionalData.size();
					pNextNode = m_Nodes.first();
					while (pNextNode) {
						if ((pNextNode != m_thisNode) && (pNextNode != pSkipNode) && (!pNextNode->isProblematic())) {
							pNodeTools = GET_NODE_TOOLS(pNextNode);
							unOkCount = pNodeTools->okCount;
							LOCK_RAFT_SEND_MUTEX(pNextNode);
							nSndRcv = pNodeTools->raftSocket.writeC(&cRequestOut, 1);
							if (nSndRcv != 1) { pNextNode->setProblematic(); goto nextNodePoint; }
							if(pKeyToInformC){
								nSndRcv = pNodeTools->raftSocket.writeC(pKeyToInformC, NODE_KEY_DATA_LEN(pKeyToInformC));
								if (nSndRcv != NODE_KEY_DATA_LEN(pKeyToInformC)) { pNextNode->setProblematic(); goto nextNodePoint; }
							}
							nSndRcv = pNodeTools->raftSocket.writeC(&nAdditionalDataLen, 4);
							if (nSndRcv != 4) { pNextNode->setProblematic(); goto nextNodePoint; }
							if (nAdditionalDataLen>0) {
								nSndRcv = pNodeTools->raftSocket.writeC(aData.additionalData.data(), nAdditionalDataLen);
								if (nSndRcv != nAdditionalDataLen) { pNextNode->setProblematic(); goto nextNodePoint; }
							}
							UNLOCK_SEND_SOCKET_MUTEX2();

							if (bWaitDone && (pNextNode!=aData.pNode)) { // wait untill done
								nIter = 0;
								while ((unOkCount == pNodeTools->okCount) && (nIter<MAX_ITER_OK_COUNT)) { Sleep(1); ++nIter; }
							}  // if ((pNextNode2 != m_thisNode) && (pNextNode2 != pSkipNode) && !pNextNode2->isProblematic()) {
						nextNodePoint:
							UNLOCK_SEND_SOCKET_MUTEX2();
						}
						pNextNode = pNextNode->next;
					}  // while (pNextNode) {

				} // if(bInformFollowers){

				switch (aData.action)
				{
				case raft::internal2::leader::newNode:
					pNodeTools = GET_NODE_TOOLS(aData.pNode);
					pNodeTools->raftSocket.writeC(&g_ccResponceOk, 1);
					pNodeTools->raftSocket.readC(&cRequestIn, 1);

					aData.pNode->setUsable();
					InterruptRaftRcv();
					InterruptDataRcv();
					break;
				case raft::internal2::follower::newNodeFromLeader:
					pNodeToolsLeader = GET_NODE_TOOLS(m_pLeaderNode);
					s_mutexForRaftSend.lock();
					pNodeToolsLeader->raftSocket.writeC(&g_ccResponceOk, 1);
					s_mutexForRaftSend.unlock();
					InterruptRaftRcv();
					InterruptDataRcv();
					break;
				default:
					break;
				}
				StateChangedAfterLock(&aData);
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


void raft::tcp::Server::CheckAllPossibleSeeds(const std::vector<NodeIdentifierKey2>& a_vectPossibleNodes)
{
	const char* cpcPosibleSeedIp;
	RaftNode2* pNode;
	char *pNodesFromLeaderC = NULL, *pNodeFromLeaderTmp2;
	char vcOwnIp4Address[MAX_IP4_LEN2];
	std::vector<NodeIdentifierKey2>  vectLeaders;
	common::SocketTCP aSocket;
	const int cnSize((int)a_vectPossibleNodes.size());
	int i,j, nSuccsessIndex(cnSize),nThisIndex(cnSize);
	int nNodesCount(0);
	uint32_t isEndianDiffer;
	int nSndRcv;
	const uint16_t unEndian=1;
	bool bFound;
	char cRequest;
	//char vcHostName[MAX_IP4_LEN2];
	
    DEBUG_HANGING();
	common::socketN::GetOwnIp4Address(vcOwnIp4Address,MAX_IP4_LEN2);
    DEBUG_HANGING();
 
	try {

		for(i=0;i<cnSize;++i){
			DEBUG_APP_WITH_KEY(2, a_vectPossibleNodes[i].key().data(), "trying to connect");
			if(  (strncmp(vcOwnIp4Address,a_vectPossibleNodes[i].ip4Address(),MAX_IP4_LEN2)==0)&&(m_nPortOwn==a_vectPossibleNodes[i].port()) ){nThisIndex=i;continue;}
			else {
				cpcPosibleSeedIp = common::socketN::GetIp4AddressFromHostName(a_vectPossibleNodes[i].ip4Address());
				if (cpcPosibleSeedIp) {
					DEBUG_APP_WITH_KEY(3,a_vectPossibleNodes[i].key().data(),"cpcPosibleSeedIp=%s, m_nPortOwn=%d", cpcPosibleSeedIp, m_nPortOwn);
					if(strcmp(cpcPosibleSeedIp,"127.0.0.1")==0){
						if(m_nPortOwn== a_vectPossibleNodes[i].port()){nThisIndex = i; continue;}
					}
					else if(strncmp(vcOwnIp4Address, cpcPosibleSeedIp, MAX_IP4_LEN2) == 0){
						if(m_nPortOwn== a_vectPossibleNodes[i].port()){nThisIndex = i; continue;}
					}
				}
			}
            DEBUG_HANGING();
			pNodesFromLeaderC=TryFindLeaderThrdSafe(a_vectPossibleNodes[i]);
			if(pNodesFromLeaderC){nNodesCount=m_Nodes.count();nSuccsessIndex=i;pNodeFromLeaderTmp2=pNodesFromLeaderC;break;}
            DEBUG_HANGING();
		}

		for(i=nSuccsessIndex+1;i<cnSize;++i){
			bFound = false;
			if(i==nThisIndex){bFound=true;}
			if(  (strncmp(vcOwnIp4Address,a_vectPossibleNodes[i].ip4Address(),MAX_IP4_LEN2)==0)&&(m_nPortOwn==a_vectPossibleNodes[i].port()) ){bFound=true;}
			else {
				cpcPosibleSeedIp = common::socketN::GetIp4AddressFromHostName(a_vectPossibleNodes[i].ip4Address());
				if (cpcPosibleSeedIp && (strncmp(vcOwnIp4Address, cpcPosibleSeedIp, MAX_IP4_LEN2) == 0) && (m_nPortOwn == a_vectPossibleNodes[i].port())) { bFound = true; }
				else if(!cpcPosibleSeedIp){cpcPosibleSeedIp=a_vectPossibleNodes[i].ip4Address();}
			}
			for(j=0;(j<nNodesCount)&&(!bFound);++j){
				if ((PORT_FROM_KEY(pNodeFromLeaderTmp2) == a_vectPossibleNodes[i].port()) && (strncmp(HOST_NAME_FROM_KEY(pNodeFromLeaderTmp2),cpcPosibleSeedIp,MAX_IP4_LEN2)==0))
				{bFound=true;break;}
				// no need to try to convert to IP, because in the bottom we did it
			}
			if(!bFound){
				// try to connect and tell about existing leader
				if(!ConnectAndGetEndian(&aSocket, a_vectPossibleNodes[i],raft::connect::toAnyNode2::otherLeaderFound,&isEndianDiffer)){continue;}

				//unEndian = 1;
				nSndRcv = aSocket.writeC(&unEndian, 2);
				if (nSndRcv != 2) {goto socketClosePoint;}

				nSndRcv = aSocket.writeC(NODE_KEY2(m_pLeaderNode),NODE_KEY_LEN(m_pLeaderNode));
				if (nSndRcv != NODE_KEY_LEN(m_pLeaderNode)) { goto socketClosePoint; }

				nSndRcv = aSocket.readC(&cRequest,1);
				if ((nSndRcv == 1)&&(cRequest=='e')) {
					DEBUG_APP_WITH_KEY(0,a_vectPossibleNodes[i].data(), " [possible secondary leader (informed)]");
				}

			socketClosePoint:
				aSocket.closeC();
			}
		}
		
		if (pNodesFromLeaderC) {
			d_state.set(RAFT_STATE_FOLLOWER);
		}
		else{
			this->d_state.set(RAFT_STATE_LEADER);
		}

		AddOwnNode();

		DEBUG_APPLICATION(1, "NumberOfNodes=%d", m_Nodes.count());
		pNode = m_Nodes.first();
		while(pNode){
			DEBUG_APPLICATION_NO_NEW_LINE(1,"%s:%d ", IP_ADDRESS_OF_NODE(pNode),(int)PORT_FROM_NODE(pNode));
			if(pNode==m_thisNode){ DEBUG_APPLICATION_NO_ADD_INFO(1,"(this) ");}
			if(pNode==m_pLeaderNode){ DEBUG_APPLICATION_NO_ADD_INFO(1,"(leader) ");}
			DEBUG_APPLICATION_NEW_LINE(1);
			pNode = pNode->next;
		}
	}
	catch (...) {
	}

	free(pNodesFromLeaderC);
}


#define MSEC(finish, start)	( (int)( (finish).millitm - (start).millitm ) + \
							(int)( (finish).time - (start).time ) * 1000 )


void raft::tcp::Server::ThreadFunctionPeriodic()
{
	PREPARE_SEND_SOCKET_GUARD();
	timeb	aCurrentTime;
	int nTimeDiff;
	const char cRequest = raft::receive::fromFollower::resetPing;
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
			if (is_follower() && (m_pLeaderNode->makePing(1)<10)) {
			//if (is_follower()) {
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
	std::string  nodeKey;
	char vcOwnHost[MAX_IP4_LEN2];

	HANDLE_MEM_DEF2(pTools, " ");

	pTools->isEndianDiffer=0;
	common::socketN::GetOwnIp4Address(vcOwnHost, MAX_IP4_LEN2);
	NodeIdentifierKey2::generateKey2(vcOwnHost, m_nPortOwn, &nodeKey);
	pNode = new RaftNode2(pTools);
	HANDLE_MEM_DEF2(pNode, " ");
	m_thisNode = pNode;
	if(is_leader()){m_pLeaderNode=pNode;pNode->makeLeader();}
	AddAdditionalDataToNode(pNode);
	m_Nodes.AddData(pNode,nodeKey.data(),nodeKey.size());
}


// const NodeIdentifierKey& nodeInfo, std::vector<NodeIdentifierKey>* pExisting
char* raft::tcp::Server::TryFindLeaderThrdSafe(const NodeIdentifierKey2& a_nodeInfo)
{
	char *pNodesInfoC = NULL, *pNodeInfoTmp;
	NodeTools* pTools;
	RaftNode2* pNewNode;
	NodeIdentifierKey2 leaderNodeKey2;
	common::SocketTCP aSocket;
	std::string strAddInfo;
    int i,nSndRcv, nBytesToReceive,numberOfNodes, nAddInfo;
	uint32_t  isEndianDiffer;
	uint16_t snEndian2;
	char cRequest;
	bool bOk(false);

	if(!ConnectAndGetEndian(&aSocket,a_nodeInfo,raft::connect::toAnyNode2::leaderInfoRequest,&isEndianDiffer)){goto returnPoint;}	// 1. connect, getEndian and sendRequest
	
	nSndRcv= aSocket.readC(leaderNodeKey2.data(), NODE_KEY_DATA_LEN(leaderNodeKey2));													// 2. get LeaderAddress
	if(nSndRcv!= NODE_KEY_DATA_LEN(leaderNodeKey2)){goto returnPoint;}
	if (isEndianDiffer) { leaderNodeKey2.swapPort(); }
	
	aSocket.closeC();

	/*******************************************************************************************************************************************/
	DEBUG_APP_WITH_KEY(1,leaderNodeKey2.key().data(),"connect to leader");
	if(!ConnectAndGetEndian(&aSocket, leaderNodeKey2,raft::connect::toLeader2::newNode,&isEndianDiffer)){goto returnPoint;}	// 1. connect, getEndian and sendRequest
	
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

	nBytesToReceive = numberOfNodes * NODE_KEY_DATA_LEN(leaderNodeKey2);
	pNodesInfoC = (char*)malloc(nBytesToReceive);
	HANDLE_MEM_DEF2(pNodesInfoC, " ");

	nSndRcv = aSocket.readC(pNodesInfoC, nBytesToReceive);														// 5. receive all nodes info
	if (nSndRcv != nBytesToReceive) { goto returnPoint; }
	
	pNodeInfoTmp = pNodesInfoC;

	if(isEndianDiffer){for(i=0;i<numberOfNodes;++i){SWAP4BYTES(*((int32_t*)pNodeInfoTmp));pNodeInfoTmp+=4;}}	// 6. swap if needed

	//
	newNode_prepareInfo_forLeader(&strAddInfo);
	nAddInfo = (int)strAddInfo.size();
	nSndRcv = aSocket.writeC(&nAddInfo, 4);																// 5. receive all nodes info
	if (nSndRcv != 4) { goto returnPoint; }
	if(nAddInfo>0){
		nSndRcv = aSocket.writeC(const_cast<char*>(strAddInfo.data()),nAddInfo);						// 5. receive all nodes info
		if (nSndRcv != nAddInfo) { goto returnPoint; }
	}

	nSndRcv = aSocket.readC(&cRequest, 1);																// 5. receive all nodes info
	if ((nSndRcv != 1)||(cRequest!= raft::receive::fromLeader2::newNode)) { goto returnPoint; }

	bOk = true;  // whith leader everything is Ok
	pNodeInfoTmp = pNodesInfoC;

	for(i=0;i<numberOfNodes;++i){
		pTools = new NodeTools;
		HANDLE_MEM_DEF2(pTools, " ");
		pNewNode = new RaftNode2(pTools);
		HANDLE_MEM_DEF2(pNewNode, " ");
		AddAdditionalDataToNode(pNewNode);
		pTools->okCount = 1;
		pNewNode->setUsable();
		m_Nodes.AddData(pNewNode, pNodeInfoTmp, NODE_KEY_DATA_LEN(pNodeInfoTmp));
		if(leaderNodeKey2.isSameNode1(pNodeInfoTmp, NODE_KEY_DATA_LEN(pNodeInfoTmp)) ){
			pNewNode->makeLeader();
			pTools->raftSocket.SetNewSocketDescriptor(aSocket);
			aSocket.ResetSocketWithoutClose();
			pTools->isEndianDiffer = isEndianDiffer;
			m_pLeaderNode=pNewNode;
		}
		else {
			// let's connect to all nodes and ask permanent raft socket
			// we will not remove any node in the case of error, removing should 
			// be done in the case of leader request
			if(!ConnectAndGetEndian(&pTools->raftSocket,pNodesInfo[i],raft::connect::toFollower2::raftBridge,&isEndianDiffer)){
				pTools->raftSocket.closeC();
				DEBUG_APPLICATION(2, "Unable to connect to raft socket!");
				continue;
			}	// 1. connect, getEndian and sendRequest
			pTools->isEndianDiffer = isEndianDiffer;
			
			snEndian2 = 1;
			nSndRcv=pTools->raftSocket.writeC(&snEndian2, 2);
			if (nSndRcv != 2) { pNewNode->setProblematic(); continue; }

			nSndRcv=pTools->raftSocket.writeC(&m_nPortOwn,4);
			if (nSndRcv != 4) { pNewNode->setProblematic(); continue; }
		
			nSndRcv=pTools->raftSocket.readC(&cRequest, 1);
			if(  ((nSndRcv != 1) || (cRequest != response::ok)  ) && (!(leaderNodeKey == pNodesInfo[i]))  ){ pNewNode->setProblematic(); continue;}
		}
		
		// Finally let's connect to all nodes and ask permanent data socket
		// we will not remove any node in the case of error, removing should 
		// be done in the case of leader request
		if(!ConnectAndGetEndian(&pTools->dataSocket,pNodesInfo[i],raft::connect::toAnyNode2::dataBridge,&isEndianDiffer)){
			pTools->dataSocket.closeC();
			ERROR_LOGGING2("Unable to connect and get data socket");
			continue;
		}	// 1. connect, getEndian and sendRequest
		
		snEndian2 = 1;
		nSndRcv=pTools->dataSocket.writeC(&snEndian2, 2);
		if(nSndRcv!=2){ pNewNode->setProblematic(); continue;}
		
		nSndRcv=pTools->dataSocket.writeC(&m_nPortOwn,4);
		if (nSndRcv != 4) { pNewNode->setProblematic(); continue; }

		nSndRcv = pTools->dataSocket.readC(&cRequest, 1);
		if (  ((nSndRcv != 1) || (cRequest != response::ok)  ) && (!(leaderNodeKey == pNodesInfo[i])) ) { pNewNode->setProblematic(); continue; }

	}

	GET_NODE_TOOLS(m_pLeaderNode)->raftSocket.writeC(&g_ccResponceOk, 1);
	//if (nSndRcv != 1) { goto returnPoint; }

	InterruptRaftRcv();
	InterruptDataRcv();

returnPoint:
	if(!bOk){free(pNodesInfo); pNodesInfo = NULL;}
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
		remData.action = raft::internal2::newLeader::becomeLeader;
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
	NodeTools* pTools = GET_NODE_TOOLS(a_node);
	NodeIdentifierKey* pNodeKey = NODE_KEY(a_node);
	int nSndRcv;
	uint32_t unPingCount=0;
	char cRequest=raft::receive::fromAnyNode2::clbkCmd;
	bool bProblematic(true);

	if((!a_node->isUsable()) ||(!GET_NODE_TOOLS(a_node)->okCount)){return 0;}

	switch (a_msg_type)
	{
	case RAFT_MSG_APPENDENTRIES:
		if(a_node->isProblematic()){a_node->makePing(1);}  // make extra ping
		unPingCount = (int)a_node->makePing(1);
		if((unPingCount>MAX_UNANSWERED_PINGS)&& pServer->is_leader()){
			SAddRemData remData;
			remData.action = raft::internal2::leader::removeNode;
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
		DEBUG_APP_WITH_NODE(1,pNodeKey,"pingCount=%d", unPingCount);
		goto returnPoint;
	}

	LOCK_RAFT_SEND_MUTEX(a_node);
	nSndRcv=pTools->raftSocket.writeC(&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}
	nSndRcv=pTools->raftSocket.writeC(&a_msg_type,4);
	if(nSndRcv!=4){goto returnPoint;}
	nSndRcv=pTools->raftSocket.writeC(send_data, d_len);
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


bool raft::tcp::Server::GetAdditionalData(common::SocketTCP& a_socket, int a_isEndianDiffer, std::string* a_pAddData)
{
	int nAddDataLen;
	int nSndRcv = a_socket.readC(&nAddDataLen, 4);
	if (nSndRcv != 4) { return false; }
	if (a_isEndianDiffer) { SWAP4BYTES(nAddDataLen); }

	if (nAddDataLen>0) {
		a_pAddData->resize(nAddDataLen);
		nSndRcv = a_socket.readC(const_cast<char*>(a_pAddData->data()), nAddDataLen);
		if (nSndRcv != nAddDataLen) {return false; }
	}
	else { a_pAddData->clear(); }
	return true;
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
raft::tcp::SAddRemData::SAddRemData()
	:
	action(0),
	pNode(NULL),
	nodeKey(),
	additionalData(),
	pForUser(NULL)
{
}


raft::tcp::SAddRemData::SAddRemData(char a_action, RaftNode2* a_pNode, NodeIdentifierKey2&& a_nodeKey, std::string&& a_addData, void* a_forUser)
	:
	action(a_action),
	pNode(a_pNode),
	nodeKey(a_nodeKey),
	additionalData(a_addData),
	pForUser(a_forUser)
{
}


raft::tcp::SAddRemData& raft::tcp::SAddRemData::operator= (SAddRemData&& a_rightSide)
{
	this->action = a_rightSide.action;
	this->pNode = a_rightSide.pNode;
	this->nodeKey = std::move(a_rightSide.nodeKey);
	this->additionalData = std::move(a_rightSide.additionalData);
	this->pForUser = a_rightSide.pForUser;

	return *this;
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
