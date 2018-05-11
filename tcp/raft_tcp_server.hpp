

#ifndef __common_raft_server2_hpp__
#define __common_raft_server2_hpp__

#include <common_servertcp.hpp>
#include <raft_server.h>
#include "raft_tcp_common.hpp"
#include <thread>
#include <shared_mutex>
#include <string>
//#include <unordered_map>
#include <raft_node.h>
#include <common_hashtbl.hpp>
#include <stdint.h>
#include "common_macroses_and_functions.h"
#include "common_unnamedsemaphorelite.hpp"
#include "common_fifofast.hpp"
#include <vector>



namespace raft { namespace tcp{



class Server : protected RaftServer
{
public:
	Server();
	virtual ~Server();

	int RunServerOnOtherThreads(int raftPort, const std::vector<NodeIdentifierKey>& vectPossibleNodes);
	void WaitServer();
	void StopServer();

protected:
	virtual void ReceiveFromDataSocket(RaftNode2* anyNode);

	void ThreadFunctionListen();
	void ThreadFunctionPeriodic();
	void ThreadFunctionRcvRaftInfo();
	void ThreadFunctionRcvData();
	void ThreadFunctionWorker();

	void FunctionForMultiRcv(int* a_pnSocketForInfo, void (RaftServerTcp::*a_fpRcvFnc)(RaftNode2*), bool a_bAsLeader);

	void AddClient(common::SocketTCP& clientSock, const sockaddr_in*remoteAddr);

	static int SendClbkFunction(void *cb_ctx, void *udata, RaftNode2* node, int msg_type, const unsigned char *send_data, int d_len);
	static void LogClbkFunction(void *cb_ctx, void *src, const char *buf, ...);
	static int ApplyLogClbkFunction(void *cb_ctx, void *udata, const unsigned char *d_data, int d_len);

	bool TryFindLeader(const NodeIdentifierKey& nodeInfo);
	void AddOwnNode();

	// utils
	void connect_allNodes_newNode(common::SocketTCP& sock);
	bool connect_leader_newNode2(common::SocketTCP& sock, const sockaddr_in*remoteAddr,int isEndianDiffer);
	bool connect_allNodes_bridgeToNewNode2(common::SocketTCP& sock);

	virtual RaftNode2* RemoveNode(RaftNode2* node) OVERRIDE;

	void CheckAllPossibleSeeds(const std::vector<NodeIdentifierKey>& vectPossibleNodes);

	void HandleSeedClbk(int msg_type,RaftNode2* anyNode);
	void ReceiveFromRaftSocket(RaftNode2* followerNode);

protected:
	common::ServerTCP								m_serverTcp;
	std::thread										m_threadTcpListen;
	std::thread										m_threadPeriodic;
	std::thread										m_threadRcvRaftInfo;
	std::thread										m_threadRcvData;
	std::vector<std::thread*>						m_vectThreadsWorkers;
	std::shared_mutex								m_mutexShrd;
	common::UnnamedSemaphoreLite					m_semaWorker;
	common::FifoFast<sockaddr_in>					m_fifoWorker;
	volatile int									m_nWork;
	volatile int									m_nServerRuns;
	int												m_nPeriodForPeriodic;
	int												m_nPortOwn;
	RaftNode2*										m_pLeaderNode;
	//SocketTCP										m_raftSocket;
	int												m_infoSocketForRcvThread;
	int												m_infoSocketForLeaderRcvThread;
};

}} // namespace raft { namespace tcp{

#endif  // #ifndef __common_raft_server2_hpp__

