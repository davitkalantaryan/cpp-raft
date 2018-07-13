

#ifndef __common_raft_server2_hpp__
#define __common_raft_server2_hpp__

#include <common/common_servertcp.hpp>
#include <raft_server.h>
#include "raft_tcp_common.hpp"
#include <thread>
#include <string>
//#include <unordered_map>
#include <raft_node.h>
#include <stdint.h>
#include "raft_macroses_and_functions.h"
#include "common/common_unnamedsemaphorelite.hpp"
#include "common/common_fifofast.hpp"
#include <vector>
#if defined(HANDLE_SIG_ACTIONS) || defined(WLAC_USED)
#include <pthread.h>
#endif
#include <sys/timeb.h>

namespace raft { namespace tcp{

namespace workRequest{enum Type{none,handleConnection};}
struct SWorkerData { workRequest::Type reqType;int sockDescriptor;sockaddr_in remAddress; };
struct SAddRemData {
	char action;
	union {
		struct {RaftNode2 *pNode; NodeIdentifierKey nodeKey;};
		void* forUser;
		struct {
			char* leaderBuf;
			int   leaderBufSize;
		}lb;
	}; 
	SAddRemData() :pNode(NULL){}
};

typedef struct NodeTools {
	void*	clbkData;
	common::SocketTCP dataSocket, raftSocket2;
	uint32_t isEndianDiffer : 1, okCount : 3;
	/*-----------------------------------------*/
	NodeTools() { clbkData = NULL; isEndianDiffer = okCount = 0; }
}NodeTools;

#define GET_NODE_TOOLS(_node)	((raft::tcp::NodeTools*)((_node)->get_udata()))
#define GET_CLBK_DATA(_node)	(GET_NODE_TOOLS((_node))->clbkData)
#define NODE_KEY(_node)			((raft::tcp::NodeIdentifierKey*)((_node)->key))

#define PREPARE_SEND_SOCKET_GUARD()					common::NewLockGuard<std::mutex> aDataSendLockGuard
#define LOCK_SEND_SOCKET_MUTEX2(_mutexPtr)			aDataSendLockGuard.SetAndLockMutex((_mutexPtr))
#define UNLOCK_SEND_SOCKET_MUTEX2()					aDataSendLockGuard.UnsetAndUnlockMutex()

extern int g_nApplicationRun;

class Server : protected RaftServer
{
public:
	Server();
	virtual ~Server();

	int  RunServerOnOtherThreads(const std::vector<NodeIdentifierKey>& vectPossibleNodes, int workersCount, int raftPort=-1);
	void StopServer();

	static void Initialize();
	static void Cleanup();

protected:
	virtual void		ReceiveFromDataSocket(RaftNode2* anyNode);
	virtual void		AddAdditionalDataToNode(RaftNode2* newNode);
	virtual void		CleanNodeData(RaftNode2*) OVERRIDE;
	virtual void		HandleNewConnection(char code,common::SocketTCP& clientSock, const sockaddr_in* remoteAddr, RaftNode2* pNode);
	virtual void		StateChangedBeforeNoLock(const SAddRemData& changeData);
	virtual void		StateChangedLocked(const SAddRemData& changeData);
	virtual void		StateChangedAfter(char state, raft::tcp::NodeIdentifierKey key, void* clbkData);
	virtual void		SignalHandler(int sigNum);

	void				ThreadFunctionListen();
	void				ThreadFunctionPeriodic();
	void				ThreadFunctionRcvRaftInfo();
	void				ThreadFunctionRcvData();
	void				ThreadFunctionAddRemoveNode();
	void				ThreadFunctionWorker();

    void				FunctionForMultiRcv(volatile int* a_pnSocketForInfo, void (Server::*a_fpRcvFnc)(RaftNode2*), bool isRaft);

	void				AddClient(common::SocketTCP& clientSock, const sockaddr_in*remoteAddr);

	NodeIdentifierKey*	TryFindLeaderThrdSafe(const NodeIdentifierKey& nodeInfo);
	void				AddOwnNode();

	// utils
	void				connect_toAnyNode_newNode(common::SocketTCP& sock);
	void				connect_toLeader_newNode(common::SocketTCP& sock, const sockaddr_in*remoteAddr);
	RaftNode2*			connect_toAnyNode_bridgeToNodeRaft(common::SocketTCP& sock, const sockaddr_in*remoteAddr);
	RaftNode2*			connect_toAnyNode_bridgeToNodeData(common::SocketTCP& sock, const sockaddr_in* remoteAddr);
	void				connect_fromClient_allNodesInfo(common::SocketTCP& sock);
	void				connect_toAnyNode_otherLeaderFound(common::SocketTCP& sock);

	void				CheckAllPossibleSeeds(const std::vector<NodeIdentifierKey>& vectPossibleNodes);

	void				HandleSeedClbk(RaftNode2* anyNode);
	void				ReceiveFromRaftSocket(RaftNode2* followerNode);

	void				become_leader() OVERRIDE;
	void				become_candidate()OVERRIDE;

    void				InterruptRaftRcv();
    void				InterruptDataRcv();

	NodeIdentifierKey*	CollectAllNodesDataNotThrSafe(int* pnTotalSize, int* a_pnLeaderIndex);

	static int	SendClbkFunction(void *cb_ctx, void *udata, RaftNode2* node, int msg_type, const unsigned char *send_data, int d_len);
	static void LogClbkFunction(void *cb_ctx, void *src, const char *buf, ...);
	static int	ApplyLogClbkFunction(void *cb_ctx, void *udata, const unsigned char *d_data, int d_len);
	static void SigHandlerStatic(int a_nSigNum);

protected:
	common::ServerTCP								m_serverTcp;
	std::thread										m_threadTcpListen;
	std::thread										m_threadPeriodic;
	std::thread										m_threadRcvRaftInfo;
	std::thread										m_threadRcvData;
	std::thread										m_threadAddRemoveNode;
	std::vector<std::thread*>						m_vectThreadsWorkers;
private:
    STDN::shared_mutex                              m_shrdMutexForNodes2;
protected:
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

	timeb											m_lastPingByLeader;
};

}} // namespace raft { namespace tcp{

#endif  // #ifndef __common_raft_server2_hpp__

