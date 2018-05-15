

#ifndef __common_raft_server2_hpp__
#define __common_raft_server2_hpp__

#include <common_servertcp.hpp>
#include <raft_server.h>
#include "raft_tcp_common.hpp"
#include <thread>
#ifdef _WIN32
#include <shared_mutex>
typedef std::shared_mutex   newSharedMutex;
#else
#include <common_rwlock.hpp>
typedef common::RWLock      newSharedMutex;
#endif
#include <string>
//#include <unordered_map>
#include <raft_node.h>
#include <common_hashtbl.hpp>
#include <stdint.h>
#include "raft_macroses_and_functions.h"
#include "common_unnamedsemaphorelite.hpp"
#include "common_fifofast.hpp"
#include <vector>
#if defined(HANDLE_SIG_ACTIONS) || defined(WLAC_USED)
#include <pthread.h>
#endif

namespace raft { namespace tcp{

namespace workRequest{enum Type{none,handleConnection};}
struct SWorkerData { workRequest::Type reqType;int sockDescriptor;sockaddr_in remAddress; };
struct SAddRemData { RaftNode2 *pNode; NodeIdentifierKey nodeKey; bool bAdd; SAddRemData();SAddRemData(RaftNode2*);};

class Server : protected RaftServer
{
public:
	Server();
	virtual ~Server();

	int RunServerOnOtherThreads(int raftPort, const std::vector<NodeIdentifierKey>& vectPossibleNodes, int workersCount);
	void StopServer();

protected:
	virtual void		ReceiveFromDataSocket(RaftNode2* anyNode);
	virtual RaftNode2*	RemoveNode(RaftNode2* node) OVERRIDE;
	virtual void		HandleNewConnection(char code,common::SocketTCP& clientSock, const sockaddr_in* remoteAddr);

	void ThreadFunctionListen();
	void ThreadFunctionPeriodic();
	void ThreadFunctionRcvRaftInfo();
	void ThreadFunctionRcvData();
	void ThreadFunctionAddRemoveNode();
	void ThreadFunctionWorker();

    void FunctionForMultiRcv(volatile int* a_pnSocketForInfo, void (Server::*a_fpRcvFnc)(RaftNode2*), bool isRaft);

	void AddClient(common::SocketTCP& clientSock, const sockaddr_in*remoteAddr);

	NodeIdentifierKey* TryFindLeaderThrdSafe(const NodeIdentifierKey& nodeInfo);
	void AddOwnNode();

	// utils
	void connect_toAnyNode_newNode(common::SocketTCP& sock);
	void connect_toLeader_newNode(common::SocketTCP& sock, const sockaddr_in*remoteAddr);
	void connect_toAnyNode_bridgeToNodeRaft(common::SocketTCP& sock, const sockaddr_in*remoteAddr);
	void connect_toAnyNode_bridgeToNodeData(common::SocketTCP& sock, const sockaddr_in* remoteAddr);
	void connect_fromClient_allNodesInfo(common::SocketTCP& sock);

	void CheckAllPossibleSeeds(const std::vector<NodeIdentifierKey>& vectPossibleNodes);

	void HandleSeedClbk(RaftNode2* anyNode);
	void ReceiveFromRaftSocket(RaftNode2* followerNode);

	void become_leader() OVERRIDE;
	void become_candidate()OVERRIDE;

    void InterruptRaftRcv();
    void InterruptDataRcv();

	raft::tcp::NodeIdentifierKey* CollectAllNodesDataNotThrSafe(int* pnTotalSize);

	static int	SendClbkFunction(void *cb_ctx, void *udata, RaftNode2* node, int msg_type, const unsigned char *send_data, int d_len);
	static void LogClbkFunction(void *cb_ctx, void *src, const char *buf, ...);
	static int	ApplyLogClbkFunction(void *cb_ctx, void *udata, const unsigned char *d_data, int d_len);

protected:
	common::ServerTCP								m_serverTcp;
	std::thread										m_threadTcpListen;
	std::thread										m_threadPeriodic;
	std::thread										m_threadRcvRaftInfo;
	std::thread										m_threadRcvData;
	std::thread										m_threadAddRemoveNode;
	std::vector<std::thread*>						m_vectThreadsWorkers;
    newSharedMutex                                  m_mutexShrd;
	common::UnnamedSemaphoreLite					m_semaWorker;
	common::UnnamedSemaphoreLite					m_semaAddRemove;
	common::FifoFast<SWorkerData>					m_fifoWorker;
	common::FifoFast<SAddRemData>					m_fifoAddDel;
	volatile int									m_nWork;
	int												m_nPeriodForPeriodic;
	int												m_nPortOwn;
	RaftNode2*										m_pLeaderNode;
    volatile int									m_infoSocketForRcvRaft2;
    volatile int									m_infoSocketForRcvData2;

#ifndef _WIN32
    pthread_t                                       m_rcvRaftThread;
    pthread_t                                       m_rcvDataThread;
#endif

public:
    void*                                           m_pReserved1;
#if defined(HANDLE_SIG_ACTIONS) || defined(WLAC_USED)
    pthread_t                                       m_starterThread;
#endif
};

}} // namespace raft { namespace tcp{

#endif  // #ifndef __common_raft_server2_hpp__

