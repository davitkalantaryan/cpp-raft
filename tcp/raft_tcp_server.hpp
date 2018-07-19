

#ifndef __common_raft_tcp_server_hpp__
#define __common_raft_tcp_server_hpp__

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
	char				action;
	NodeIdentifierKey*	pNodeKey;
	std::string			extraData;
	void*				pForUser;
	RaftNode2*			informNode2;
	int					socketNum;
	uint32_t			isEndianDiffer;
	bool				bApplyData;
	SAddRemData();
	SAddRemData(SAddRemData&& a_rightSide);
	SAddRemData& operator=(SAddRemData&&);
	~SAddRemData();
	NodeIdentifierKey* operator->();
    const NodeIdentifierKey* nodeKey()const;
};

typedef struct NodeTools {
	void*	clbkData;
	common::SocketTCP dataSocket, raftSocket;
	uint32_t isEndianDiffer : 1;
	/*-----------------------------------------*/
	NodeTools() { clbkData = NULL; isEndianDiffer = 0; }
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

	int					RunServerOnOtherThreads2(const std::vector<NodeIdentifierKey>& vectPossibleNodes, int workersCount, int raftPort=-1);
	void				StopServer();

	static void			Initialize();
	static void			Cleanup();

protected:
	virtual void		become_leader() OVERRIDE;
	virtual void		become_candidate()OVERRIDE;
	virtual void		AddAdditionalDataToNode2(RaftNode2* newNode, void* a_changeData)OVERRIDE;

	virtual void		ReceiveFromDataSocket(RaftNode2* anyNode);
	virtual void		CleanNodeData(RaftNode2*, void* a_changeData) OVERRIDE;
	virtual bool		HandleDefaultConnection(char code,common::SocketTCP& clientSock, const sockaddr_in* remoteAddr, SAddRemData* a_changeData);
	
	virtual bool		newNode_prepareInfo_forLeader(std::string* a_pBufferForInfo);
	virtual void		StateChangedBeforeLock(SAddRemData* changeData);
	virtual void		StateChangedLockedPre(SAddRemData* changeData);
	virtual void		StateChangedLockedPost(SAddRemData* changeData);
	virtual void		StateChangedAfterLock( SAddRemData* changeData);
	
	virtual void		SignalHandler(int sigNum);

	//virtual void		newLeader_prepareInform_on_oldLeader_died(std::string* bufferForAdditionalData);
	//virtual void		leader_prepareInform_on_newNode(std::string* bufferForAdditionalData, bool* a_pbSendBack);
	//virtual void		leader_prepareInform_on_removeNode(std::string* bufferForAdditionalData);
	//virtual void		newNode_prepareInform_toLeader(std::string* bufferForAdditionalData);

	void				ThreadFunctionListen();
	void				ThreadFunctionPeriodic();
	void				ThreadFunctionRcvRaftInfo();
	void				ThreadFunctionRcvData();
	void				ThreadFunctionLockedAction();
	void				ThreadFunctionWorker();
	template <typename Type>
	void				ThreadFunctionOtherPeriodic(void (Type::*a_fpClbk)(int), int a_nPeriod, Type* a_pObj, int a_nIndex);

	template <typename Type>
	void				StartOtherPriodic(void (Type::*a_fpClbk)(int), int a_nPeriod, Type* a_pObj);

    void				FunctionForMultiRcv(volatile int* a_pnSocketForInfo, void (Server::*a_fpRcvFnc)(RaftNode2*), bool isRaft);

	void				AddClient(common::SocketTCP& clientSock, const sockaddr_in*remoteAddr);

	NodeIdentifierKey*	TryFindLeaderThrdSafe(const NodeIdentifierKey& nodeInfo, SAddRemData* a_pDtaFromRem);
	void				AddOwnNode();

	// family of receive functions
	bool				raft_receive_fromLeader_newNode_private(common::SocketTCP& a_socket,SAddRemData*	a_pNodeData);
	bool				raft_receive_fromLeader_newNode(RaftNode2* a_pNode, SAddRemData* a_pClbkData);
	bool				raft_receive_fromLeader_removeNode(RaftNode2* a_pNode, SAddRemData* a_pClbkData);

	// utils
	bool				raft_connect_toFollower_raftBridge(common::SocketTCP& sock, const sockaddr_in*remoteAddr);
	bool				raft_connect_toAnyNode_dataBridge(common::SocketTCP& sock, const sockaddr_in* remoteAddr);

	void				raft_connect_toAnyNode_leaderInfoRequest(common::SocketTCP& sock);
	bool				raft_connect_toLeader_newNode(common::SocketTCP& sock, const sockaddr_in*remoteAddr, SAddRemData* a_clbkData, int* a_isEndianDiffer);
	void				raft_connect_fromClient_allNodesInfo(common::SocketTCP& sock);
	void				raft_connect_toAnyNode_otherLeaderFound(common::SocketTCP& sock);

	void				CheckAllPossibleSeeds(const std::vector<NodeIdentifierKey>& vectPossibleNodes, SAddRemData* a_pDtaFromRem);

	void				HandleSeedClbk(RaftNode2* anyNode);
	void				ReceiveFromRaftSocket(RaftNode2* followerNode);

    void				InterruptRaftRcv();
    void				InterruptDataRcv();
	bool				ReceiveExtraData(common::SocketTCP& a_sockt, int a_isEndianDiffer, std::string* a_pBufForData);

	NodeIdentifierKey*	CollectAllNodesDataNotThrSafe(int* pnTotalSize, int* a_pnLeaderIndex);
	void				FindClusterAndInit(const std::vector<NodeIdentifierKey>& vectPossibleNodes, SAddRemData* a_pDtaFromRem, int raftPort=-1);
	void				RunAllThreadPrivate(int workersCount);

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
	std::thread										m_threadLockedActions;
	std::vector<std::thread*>						m_vectThreadsWorkers;
	std::vector<std::thread*>						m_vectThreadsOtherPeriodic;
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
	uint64_t										m_isInited : 1;
};

}} // namespace raft { namespace tcp{

#include "impl.raft_tcp_server.hpp"

#endif  // #ifndef __common_raft_tcp_server_hpp__

