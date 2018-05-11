
#ifndef HEADER_SIZE_needed
#define HEADER_SIZE_needed
#endif

#include "raft_tcp_server.hpp"
#include <string>
#include <stdint.h>

#define DEF_REP_RATE_MS					100
#define	TIMEOUTS_RATIO_MIN				2
#define SOCK_TIMEOUT_MS					100000
#define REPORT_ON_FAULT(_faultyNode)

#ifdef _WIN32
#else
#define closesocket	close
#endif


typedef struct { common::SocketTCP socket, raftSocket; int isEndianDiffer; }NodeTools;

static int CreateEmptySocket();

#define GetOwnHostIdentifier2(_node)	(_node)->set_ip4Address()

#define FOLLOWER_SEES_ERR(...)	Sleep(2000)

raft::tcp::Server::Server()
	:
	m_nWork(0),
	m_nPeriodForPeriodic(DEF_REP_RATE_MS),
	m_nPortOwn(0),
	m_pLeaderNode(NULL)
{
	raft_cbs_t aClbks;

	m_infoSocketForRcvThread = -1;
	m_infoSocketForLeaderRcvThread = -1;
	m_nServerRuns = 0;

	aClbks.send     = &raft::tcp::Server::SendClbkFunction;
	aClbks.log      = &raft::tcp::Server::LogClbkFunction;
	aClbks.applylog = &raft::tcp::Server::ApplyLogClbkFunction;
	this->set_callbacks(&aClbks, this);
}


raft::tcp::Server::~Server()
{
	this->StopServer();
}


RaftNode2* raft::tcp::Server::RemoveNode(RaftNode2* a_node)
{
	NodeTools* pNodeTool = (NodeTools*)a_node->get_udata();
	RaftNode2* pRet = RaftServer::RemoveNode(a_node);

	if(pRet){delete pNodeTool;}
	return pRet;
}


void raft::tcp::Server::ReceiveFromDataSocket(RaftNode2*) // this should be overriten by child
{
}


int raft::tcp::Server::RunServerOnOtherThreads(int a_nRaftPort, const std::vector<NodeIdentifierKey>& a_vectPossibleNodes)
{
	if (m_nServerRuns) { return -1; }

	m_nPortOwn = a_nRaftPort;
	if(m_nPeriodForPeriodic<1){ m_nPeriodForPeriodic = DEF_REP_RATE_MS;}
	if(this->request_timeout < 1) { this->request_timeout =DEF_REP_RATE_MS;}
	if(this->election_timeout < (TIMEOUTS_RATIO_MIN*this->request_timeout)) { this->election_timeout =(TIMEOUTS_RATIO_MIN*this->request_timeout);}
	this->timeout_elapsed = 0;

	CheckAllPossibleSeeds(a_vectPossibleNodes);

	m_nWork = 1;

#if 0
	std::thread										m_threadTcpListen;
	std::thread										m_threadPeriodic;
	std::thread										m_threadRcvRaftInfo;
	std::thread										m_threadRcvData;
	std::vector<std::thread*>						m_vectThreadsWorkers;
#endif


	if(is_leader()){
		m_threadRcvRaftInfo = std::thread(&Server::ThreadFunctionRcvRaftInfo, this);
		m_nLeaderRuns = 1;
	}
	m_threadTcpListen = std::thread(&Server::ThreadFunctionListen,this);
	m_threadPeriodic = std::thread(&Server::ThreadFunctionPeriodic, this);
	m_threadRcvFromAllNodes = std::thread(&Server::ThreadFunctionRcvData, this);
	m_nServerRuns = 1;

	if(is_follower()){
		m_threadFollower = std::thread(&Server::ThreadFunctionFollower, this);
		m_nFollowerRuns = 1;
	}

	return 0;
}


void raft::tcp::Server::WaitServer()
{
	if (!m_nServerRuns) { return; }

	if (m_nFollowerRuns) { m_threadFollower.join(); m_nFollowerRuns = 0; }

	m_threadRcvFromAllNodes.join();
	m_threadPeriodic.join();
	m_threadTcpListen.join();
	if (m_nLeaderRuns) { m_threadRcvRaftInfo.join(); m_nLeaderRuns = 0; }
	m_nServerRuns = 0;
}


void raft::tcp::Server::StopServer()
{
	if (m_nWork == 0) {return;}
	m_nWork = 0;
	if(m_infoSocketForRcvThread>0){closesocket(m_infoSocketForRcvThread);}
	if(m_infoSocketForLeaderRcvThread>0){closesocket(m_infoSocketForLeaderRcvThread);}
	m_serverTcp.StopServer();
	this->WaitServer();
}


void raft::tcp::Server::AddClient(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr)		// 1. connect
{
	char vcHostName[MAX_HOSTNAME_LENGTH];
	int nSndRcv, nError = -1, isEndianDiffer=0;
	int16_t	snEndian = 1;
	char cRequest;
	bool bResetDescriptor(false);

	LOG_REP("conntion from %s(%s)",
		common::socketN::GetHostName(a_remoteAddr, vcHostName, MAX_HOSTNAME_LENGTH),common::socketN::GetIPAddress(a_remoteAddr));
	a_clientSock.setTimeout(SOCK_TIMEOUT_MS);

	nSndRcv = a_clientSock.writeC(&snEndian,2);																// 2. Send endian				
	if (nSndRcv != 2) {
		ERR_REP("Connection broken");
		goto returnPoint;
	}

	nSndRcv = a_clientSock.readC(&snEndian,2);																// 3. rcv endian
	if (nSndRcv != 2) {
		ERR_REP("Unable to read request");
		goto returnPoint;
	}
	if (snEndian != 1) { isEndianDiffer = 1; }

	nSndRcv = a_clientSock.readC(&cRequest, 1);																// 4. rcv request
	if (nSndRcv != 1) {
		ERR_REP("Unable to read request");
		goto returnPoint;
	}

	switch (cRequest)
	{
	case raft::connect::anyNode::newNode:
		connect_allNodes_newNode(a_clientSock);
		break;
	case raft::connect::leader::newNode:
		bResetDescriptor=connect_leader_newNode2(a_clientSock, a_remoteAddr,isEndianDiffer);
		break;
	case raft::connect::anyNode::bridgeToNewNode:
		bResetDescriptor=connect_allNodes_bridgeToNewNode2(a_clientSock);
		printf("raft::connect::anyNode::bridgeToNewNode, bResetDescriptor=%s\n", bResetDescriptor?"true":"false");
		break;
	default:
		ERR_REP("wrong request");
		goto returnPoint;
	}

	nError = 0;
returnPoint:
	if (nError) { a_clientSock.writeC("e", 1); }
	else { a_clientSock.writeC("o", 1); }
	if(bResetDescriptor){a_clientSock.ResetSocketWithoutClose();}
}


void raft::tcp::Server::connect_allNodes_newNode(common::SocketTCP& a_clientSock)
{
	NodeIdentifierKey* pLeaderKey = (NodeIdentifierKey*)m_pLeaderNode->key2();
	a_clientSock.writeC(pLeaderKey, sizeof(NodeIdentifierKey));
}


bool raft::tcp::Server::connect_leader_newNode2(common::SocketTCP& a_clientSock, const sockaddr_in* a_remoteAddr,int a_nIsEndianDiffer)
{
	// struct NodeIdentifierKey { char ip4Address[MAX_IP4_LEN]; int32_t port;};
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
	NodeIdentifierKey*	pAllNodesInfoToBeSent=NULL;
	NodeTools*			pNewNodeTool = NULL;
	NodeTools*			pOldNodeTool;
	RaftNode2*			pNode;
	NodeIdentifierKey*	pExistingNodeKey;
	NodeIdentifierKey	newNodeKey;
	int i,nTotalSize;
	int nSndRcv;
	char cRequest;
	bool bOk(false);

	SetHostNameToTheKey(common::socketN::GetIPAddress(a_remoteAddr), &newNodeKey);

	nSndRcv= a_clientSock.readC(&newNodeKey.port,4);
	if(nSndRcv!= 4){ goto returnPoint;}
	if(a_nIsEndianDiffer){ SWAP4BYTES(newNodeKey.port);}
	
	pNewNodeTool = new NodeTools;
	if (!pNewNodeTool) { HANDLE_MEM_DEF(" "); }
	pNewNodeTool->isEndianDiffer = a_nIsEndianDiffer;

	m_mutexShrd.lock_shared();

	nTotalSize = m_nNodesCount * sizeof(NodeIdentifierKey);
	pAllNodesInfoToBeSent = (NodeIdentifierKey*)malloc(nTotalSize);
	if (!pAllNodesInfoToBeSent) { HANDLE_MEM_DEF(" "); }

	// first let us to send to the new node all current nodes, that it is prepared
	// collect info
	i = 0;
	pNode = m_firstNode;
	while(pNode){
		pExistingNodeKey = (NodeIdentifierKey*)pNode->key2();
		SetHostNameToTheKey(pExistingNodeKey->ip4Address, &pAllNodesInfoToBeSent[i]);
		pAllNodesInfoToBeSent[i].port = pExistingNodeKey->port;
		pNode = pNode->next;
		++i;
	}

	m_mutexShrd.unlock_shared();

	nSndRcv= a_clientSock.writeC(&m_nNodesCount,4);
	if(nSndRcv!= 4){ goto returnPoint;}

	nSndRcv= a_clientSock.writeC(pAllNodesInfoToBeSent,nTotalSize);
	if(nSndRcv!= nTotalSize){ goto returnPoint;}
	free(pAllNodesInfoToBeSent); pAllNodesInfoToBeSent = NULL;

	// now send to all existing nodes the info on new node
	m_mutexShrd.lock_shared();

	pNode = m_firstNode;
	while(pNode){
		if(pNode != m_thisNode){
			pOldNodeTool = (NodeTools*)pNode->get_udata();
			cRequest = raft::receive::leader::newNode;
			pOldNodeTool->raftSocket.writeC(&cRequest, 1);  // let us assume that no error
			pOldNodeTool->raftSocket.writeC(&newNodeKey,sizeof(NodeIdentifierKey));  // let us assume that no error
			pOldNodeTool->raftSocket.readC(&cRequest, 1); // 'o'->ok, 'e'->err
		}		
		pNode = pNode->next;
	}

	m_mutexShrd.unlock_shared();

	// finally let us add this new node here
	pNewNodeTool->raftSocket.SetNewSocketDescriptor(a_clientSock);
	pNode = new RaftNode2(pNewNodeTool);
	if(!pNode){HANDLE_MEM_DEF(" ");}
	m_mutexShrd.lock();
	this->AddNode(pNode, &newNodeKey, sizeof(NodeIdentifierKey));
	if(m_infoSocketForLeaderRcvThread>0){closesocket(m_infoSocketForLeaderRcvThread);}  // can be also after unlock???
	m_mutexShrd.unlock();
	
	bOk = true;
returnPoint:
	free(pAllNodesInfoToBeSent);
	return bOk;
}


bool raft::tcp::Server::connect_allNodes_bridgeToNewNode2(common::SocketTCP& a_clientSock)
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

	m_mutexShrd.lock();
	if(!m_hashNodes.FindEntry(&aRemHost,sizeof(NodeIdentifierKey),&pNode)){
		m_mutexShrd.unlock_shared();
		goto returnPoint;
	}
	
	pNodeTools = (NodeTools*)pNode->get_udata();
	pNodeTools->socket.SetNewSocketDescriptor(a_clientSock);
	if(m_infoSocketForRcvThread>0){closesocket(m_infoSocketForRcvThread);}
	m_mutexShrd.unlock();

	bOk = true;
returnPoint:
	return bOk;
}


void raft::tcp::Server::ThreadFunctionListen()
{
	m_serverTcp.setTimeout(SOCK_TIMEOUT_MS);
	m_serverTcp.StartServer(this, &raft::tcp::Server::AddClient,m_nPortOwn);
}


void raft::tcp::Server::HandleSeedClbk(int a_msg_type, RaftNode2* a_anyNode)
{
	NodeTools* pTools = (NodeTools*)a_anyNode->get_udata();
	NodeIdentifierKey aKey;
	int nSndRcv, nToReceive;
	bool bProblematic(true);

	nSndRcv=pTools->socket.readC(&aKey, sizeof(NodeIdentifierKey));
	if (nSndRcv != sizeof(NodeIdentifierKey)) { goto returnPoint; }
	//pTools->socket.readC(send_data, d_len);
	//if (nSndRcv != d_len) { goto returnPoint; }

	switch (a_msg_type) 
	{
	case RAFT_MSG_REQUESTVOTE:
	{
		msg_requestvote_t reqVote(0,0,0,0);
		nSndRcv = pTools->socket.readC(&reqVote, sizeof(msg_requestvote_t));
		if (nSndRcv != sizeof(msg_requestvote_t)) { goto returnPoint; }
		recv_requestvote(a_anyNode, &reqVote);
	}
	break;
	case RAFT_MSG_APPENDENTRIES:
	{
		MsgAppendEntries2 appEntries;
		nSndRcv = pTools->socket.readC(&appEntries, SIZE_OF_INITIAL_RCV_OF_MSG_APP);
		if (nSndRcv != SIZE_OF_INITIAL_RCV_OF_MSG_APP) { goto returnPoint; }
		if(appEntries.getNEntries()){
			nToReceive = appEntries.getNEntries() * sizeof(msg_entry_t);
			nSndRcv=pTools->socket.readC(appEntries.entries(),nToReceive);
			if (nSndRcv != nToReceive) { goto returnPoint; }
		}
		recv_appendentries(a_anyNode, &appEntries);
	}
	break;
	default:
		goto returnPoint;
	}

	bProblematic = false;
returnPoint:
	if (bProblematic) { a_anyNode->setProblematic(1); }
	else { a_anyNode->setProblematic(0); }
}


void raft::tcp::Server::ReceiveFromRaftSocket(RaftNode2* a_followerNode)
{
	NodeTools* pTools = (NodeTools*)a_followerNode->get_udata();
	int nSndRcv;
	bool bProblematic(true);
	char cRequest;

	nSndRcv = pTools->socket.readC(&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}

	switch (cRequest)
	{
	case 0:
		break;
	default:
		goto returnPoint;
	}

	bProblematic = false;
returnPoint:
	if(bProblematic){ a_followerNode->setProblematic(1);}
	else{ a_followerNode->setProblematic(0);}
}


void raft::tcp::Server::FunctionForMultiRcv(int* a_pnSocketForInfo, void (Server::*a_fpRcvFnc)(RaftNode2*), bool a_bAsLeader)
{
	NodeTools* pNodeTools;
	RaftNode2* pNode;
	fd_set rFds, eFds;
	int nMax, nCurrentSocket, nSelectReturn, nSocketsFound, nSocketToIgnore=-1, nLastSocket=-1;
	int& nSocketForInfo = *a_pnSocketForInfo;
	while (m_nWork){
		FD_ZERO(&rFds); FD_ZERO(&eFds);
		m_mutexShrd.lock_shared();
		if(nSocketForInfo<=0){nSocketForInfo=CreateEmptySocket();}
		nMax = nSocketForInfo;
		FD_SET(nSocketForInfo, &rFds);
		FD_SET(nSocketForInfo, &eFds);
		pNode = m_firstNode;
		while(pNode){
			if((pNode!=m_thisNode)&&(!pNode->isProblematic())){
				pNodeTools = (NodeTools*)pNode->get_udata();
				nCurrentSocket = a_bAsLeader? pNodeTools->raftSocket:pNodeTools->socket;
				if((nCurrentSocket>0)&&(nCurrentSocket!=nSocketToIgnore)){
					FD_SET(nCurrentSocket,&rFds);
					FD_SET(nCurrentSocket,&eFds);
					if(nCurrentSocket>nMax){nMax=pNodeTools->socket;}
				}  // if(pNodeTools->socket>0){
			}  // if(pNode!=m_thisNode){
			pNode = pNode->next;
		}// while(pNode){
		m_mutexShrd.unlock_shared();

		nSelectReturn = ::select(++nMax, &rFds, NULL, &eFds, NULL);
		if (!m_nWork) { break; }
		if (nSelectReturn < 0) { 
			Sleep(2000);  // most probably last client was problematic
			nSocketToIgnore = nLastSocket;
		} // will never happen
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
				nCurrentSocket = a_bAsLeader?pNodeTools->raftSocket:pNodeTools->socket;
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
	FunctionForMultiRcv(&m_infoSocketForRcvThread,&raft::tcp::Server::ReceiveFromDataSocket,false);
}


void raft::tcp::Server::ThreadFunctionRcvRaftInfo()
{
	FunctionForMultiRcv(&m_infoSocketForLeaderRcvThread,&raft::tcp::Server::ReceiveFromRaftSocket,true);
}


void raft::tcp::Server::ThreadFunctionWorker()
{
	void* pDataFromProducer;
	while(m_nWork){
		m_semaWorker.wait();

		while(m_fifoWorker.Extract(&pDataFromProducer)&&m_nWork){
		}
	}
}


#if 0   // this is also the part of the thread  // m_threadRcvRaftInfo
void raft::tcp::Server::ThreadFunctionFollower()
{
	// struct NodeIdentifierNet { int32_t port, isLeader; char hostName[MAX_HOSTNAME_LENGTH];};
	RaftNode2* pNewNode;
	NodeTools* pNewNodeTools;
	NodeIdentifierKey newNodeKey;
	int isLeaderEndianDiffer= ((NodeTools*)m_pLeaderNode->get_udata())->isEndianDiffer;
	int nSndRcv;
	char cRequest;

	m_socketToLeader.setTimeout(this->election_timeout);

	while((!this->is_leader())&&m_nWork){
		nSndRcv = m_socketToLeader.readC(&cRequest, 1);
		if(nSndRcv!=1){
			// do something
			FOLLOWER_SEES_ERR();
			continue;
		}
		switch (cRequest)
		{
		case raft::receive::leader::newNode:
			nSndRcv = m_socketToLeader.readC(&newNodeKey, sizeof(NodeIdentifierKey));
			if (nSndRcv != sizeof(NodeIdentifierKey)) {
				// do something
				FOLLOWER_SEES_ERR();
				continue;
			}
			if (isLeaderEndianDiffer){SWAP4BYTES(newNodeKey.port);}
			pNewNodeTools = new NodeTools;
			if(!pNewNodeTools){HANDLE_MEM_DEF(" ");}
			pNewNode = new RaftNode2(pNewNodeTools);
			if(!pNewNode){HANDLE_MEM_DEF(" ");}
			m_mutexShrd.lock();
			this->AddNode(pNewNode, &newNodeKey, sizeof(NodeIdentifierKey));
			m_mutexShrd.unlock();
			cRequest = 'o';
			m_socketToLeader.writeC(&cRequest, 1);
			break;
		default: break;
		}
	}
}


#endif  // 


void raft::tcp::Server::CheckAllPossibleSeeds(const std::vector<NodeIdentifierKey>& a_vectPossibleNodes)
{
	const int cnSize((int)a_vectPossibleNodes.size());
	int i;
	bool bLeaderFound(false);

	try {

		for(i=0;i<cnSize;++i){
			if(TryFindLeader(a_vectPossibleNodes[i])){ bLeaderFound=true;break;}
		}
		
		if (bLeaderFound) {
			d_state.set(RAFT_STATE_FOLLOWER);
		}
		else{
			this->d_state.set(RAFT_STATE_LEADER);
		}

		AddOwnNode();
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
	GetOwnHostIdentifier2(&aOwnHost);
	aOwnHost.port = m_nPortOwn;
	pNode = new RaftNode2(pTools);
	if (!pNode) { HANDLE_MEM_DEF(" "); }
	m_thisNode = pNode;
	if(is_leader()){m_pLeaderNode=pNode;pNode->makeLeader(1);}
	this->AddNode(pNode, &aOwnHost, sizeof(NodeIdentifierKey));
}


bool raft::tcp::Server::TryFindLeader(const NodeIdentifierKey& a_nodeInfo)
{
	// struct NodeIdentifierNet { int32_t port, isLeader; char hostName[MAX_HOSTNAME_LENGTH];};
	NodeTools* pTools;
	RaftNode2* pNewNode;
	NodeIdentifierKey leaderNodeKey, thisNodeKey;
	NodeIdentifierKey *pNodeInfo2 = NULL;
	common::SocketTCP aSocket2;
	int16_t  snEndian;
	int32_t  numberOfNodes;
	std::vector<raft_node_configuration_t> vectInput;
	int i,nSndRcv, nBytesToReceive, isEndianDiffer=0;
	char cRequest;
	bool bReturn(false);

	if(aSocket2.connectC(a_nodeInfo.ip4Address,a_nodeInfo.port)){return false;}						// 1. connect
	aSocket2.setTimeout(SOCK_TIMEOUT_MS);
	
	nSndRcv= aSocket2.readC(&snEndian,2);															// 2. rcv endian
	if(nSndRcv!=2){return false;}
	if (snEndian != 1) { isEndianDiffer = 1; }
	
	snEndian = 1;
	nSndRcv= aSocket2.writeC(&snEndian,2);															// 3. send endian
	if(nSndRcv!=2){return false;}
	
	cRequest = raft::connect::anyNode::newNode;
	nSndRcv = aSocket2.writeC(&cRequest,1);															// 4. Send request
	if(nSndRcv!=1){return false;}
	//pNodeInfo = (NodeIdentifierKey*)malloc(sizeof(NodeIdentifierKey));
	//if (!pNodeInfo) { HANDLE_MEM_DEF(" "); return false; }
	nSndRcv= aSocket2.readC(&leaderNodeKey,sizeof(NodeIdentifierKey));								// 6. get responce  (5. send add info is not used)
	if(nSndRcv!=sizeof(NodeIdentifierKey)){goto returnPoint;}
	/*nSndRcv=*/aSocket2.readC(&cRequest, 1);														// 8. rcv finall echo  (7. rcv add info skipped)
	// cRequest=='o'->ok, cRequest=='e'->error
	aSocket2.closeC();

	if(isEndianDiffer){SWAP4BYTES(leaderNodeKey.port);}

	/***************************************************/
	isEndianDiffer = 0;
	if(m_socketToLeader.connectC(leaderNodeKey.ip4Address, leaderNodeKey.port)){ goto returnPoint;}	// 1. connect
	m_socketToLeader.setTimeout(SOCK_TIMEOUT_MS);

	nSndRcv= m_socketToLeader.readC(&snEndian,2);													// 2. rcv endian
	if(nSndRcv!=2){return false;}
	if (snEndian != 1) { isEndianDiffer = 1; }

	snEndian = 1;
	nSndRcv= m_socketToLeader.writeC(&snEndian,2);													// 3. send endian
	if(nSndRcv!=2){return false;}

	cRequest = raft::connect::leader::newNode;
	nSndRcv = m_socketToLeader.writeC(&cRequest,1);													// 4. Send request
	if(nSndRcv!=1){return false;}

	nSndRcv = m_socketToLeader.writeC(&m_nPortOwn,4);												// 6. rcv answer  (5. send add info skipped)
	if (nSndRcv != 4) { return false; }

	nSndRcv = m_socketToLeader.readC(&numberOfNodes,4);												// 6. rcv answer  (5. send add info skipped)
	if (nSndRcv != 4) { return false; }
	if (isEndianDiffer) { SWAP4BYTES(numberOfNodes); }
	if (numberOfNodes < 1) {return false;}
	
	nBytesToReceive = numberOfNodes*sizeof(NodeIdentifierKey);
	pNodeInfo2 = (NodeIdentifierKey*)malloc(nBytesToReceive);
	if (!pNodeInfo2) { return false; }

	nSndRcv = m_socketToLeader.readC(pNodeInfo2,nBytesToReceive);									// 7. get additional info
	if(nSndRcv!=nBytesToReceive){goto returnPoint;}
	/*nSndRcv=*/m_socketToLeader.readC(&cRequest,1);
	// vcBuffer[0]=='o'->ok, vcBuffer[0]=='e'->error

	vectInput.resize(numberOfNodes);
	GetOwnHostIdentifier2(&thisNodeKey);
	thisNodeKey.port = m_nPortOwn;

	for(i=0;i<numberOfNodes;++i){
		pTools = new NodeTools;
		if(!pTools){HANDLE_MEM_DEF(" ");}
		pNewNode = new RaftNode2(pTools);
		if (!pNewNode) { HANDLE_MEM_DEF(" "); }
		if(leaderNodeKey== pNodeInfo2[i]){pNewNode->makeLeader(1);m_pLeaderNode=pNewNode;}
		this->AddNode(pNewNode, &pNodeInfo2[i], sizeof(NodeIdentifierKey));
		
		// Finally let's connect to all nodes and ask permanent socket
		pTools->socket.connectC(pNodeInfo2[i].ip4Address, pNodeInfo2[i].port);
		pTools->socket.readC(&snEndian,2);
		if(snEndian!=1){pTools->isEndianDiffer=1;}
		snEndian = 1;
		pTools->socket.writeC(&snEndian,2);
		cRequest = raft::connect::anyNode::bridgeToNewNode;
		pTools->socket.writeC(&cRequest,1);
		pTools->socket.writeC(&thisNodeKey,sizeof(common::NodeIdentifierKey));
		pTools->socket.readC(&cRequest,1);
	}

	bReturn = true;
returnPoint:
	free(pNodeInfo2);
	if(!bReturn){ m_socketToLeader.closeC();}
	return bReturn;
}


/*//////////////////////////////////////////////////////////////////////////////*/


/*//////////////////////////////////////////////////////////////////////////////*/

int common::RaftServerTcp::SendClbkFunction(void *a_cb_ctx, void *udata, RaftNode2* a_node, int a_msg_type, const unsigned char *send_data,int d_len)
{
	//RaftServerTcp* pServer = (RaftServerTcp*)a_cb_ctx;
	// typedef struct { common::SocketTCP socket, socketToFollower; int isEndianDiffer; }NodeTools;
	NodeTools* pTools = (NodeTools*)a_node->get_udata();
	int nSndRcv;
	char cRequest=raft::receive::anyNode::clbkCmd;
	bool bProblematic(true);

	nSndRcv=pTools->socket.writeC(&cRequest,1);
	if(nSndRcv!=1){goto returnPoint;}
	pTools->socket.writeC(&a_msg_type,4);
	if(nSndRcv!=4){goto returnPoint;}
	pTools->socket.writeC(send_data, d_len);
	if(nSndRcv!= d_len){goto returnPoint;}
	
	bProblematic = false;
returnPoint:
	return 0;
}


void common::RaftServerTcp::LogClbkFunction(void *cb_ctx, void *src, const char *buf, ...)
{
}


int common::RaftServerTcp::ApplyLogClbkFunction(void *cb_ctx, void *udata, const unsigned char *d_data, int d_len)
{
	return 0;
}



/********************************************************************************************************************/
static int CreateEmptySocket()
{
	int nSocket = (int)::socket(AF_INET, SOCK_STREAM, 0);
	return nSocket;
}
