#ifndef RAFT_SERVER_H
#define RAFT_SERVER_H

#include "state_mach.h"
#include "raft.h"
#include "raft_msg.h"
#include <cstddef>
#include <functional>
#include <vector>
#include <memory>
#include <common/listspecialandhashtbl.hpp>
#include <string>

#ifndef SleepMs
#ifdef _WIN32
#ifndef CINTERFACE
#define CINTERFACE
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#define SleepMs(_x) SleepEx((_x),TRUE)
#else
#define SleepMs(_x) (  ((_x)>100000) ? sleep((_x)/1000) : usleep(1000*(_x))  )
#endif
#endif  // #ifndef SleepMs

class RaftLogger;
class RaftNode2;


class RaftServer {

protected: // in order to make usable variables by inherites
	/* Persistent state: */
	///the server's best guess of what the current term is starts at zero
	int current_term;

	/// The candidate the server voted for in its current term, or Nil if it hasn't voted for any.
	RaftNode2* m_voted_for;

	/// the log which is replicated
	std::shared_ptr<RaftLogger> log;

	/* Volatile state: */
	/// idx of highest log entry known to be committed
	int commit_idx;

	/// idx of highest log entry applied to state machine
	int last_applied_idx;

	/// follower/leader/candidate indicator
	Raft::State d_state;

	/// most recently append idx, also indicates size of log
	int current_idx;

	/// amount of time left till timeout
	int timeout_elapsed;

	/// who has voted for me. This is an array with N = 'num_nodes' elements
	//std::vector<int> votes_for_me;
	//std::vector<RaftNode2> nodes;
	
private:
	common::ListspecialAndHashtbl<RaftNode2*>	m_Nodes;
protected:
	
	int							m_nLeaderCommit;

	int election_timeout;
	int request_timeout;

	/// callbacks
	raft_cbs_t cb;
	void* cb_ctx;

	/// my node ID
	//size_t nodeid;
	RaftNode2* m_thisNode;

	typedef void (RaftServer::*PerNodeCallback)(RaftNode2*);
	void forAllNodesExceptSelf(std::function<void(RaftNode2*)> callback, bool skipLeader);
	void inline forAllNodesExceptSelf(PerNodeCallback a_callback, bool a_skipLeader) {
		forAllNodesExceptSelf(std::bind(a_callback, this, std::placeholders::_1), a_skipLeader);
	}

public:

	/**
	 * Initialise a new raft server
	 *
	 * Request timeout defaults to 200 milliseconds
	 * Election timeout defaults to 1000 milliseconds
	 * @return newly initialised raft server */
	RaftServer();

	virtual ~RaftServer();

	/**
	 * Set callbacks
	 * @param funcs Callbacks
	 * @param cb_ctx The context that we include with all callbacks */
	void set_callbacks(raft_cbs_t* funcs, void* cb_ctx);

	void election_start();

	virtual void become_leader();
	virtual void become_candidate();
	virtual void become_follower();

	/**
	 * Run actions that are dependent on time passing
	 * @return 0 on error */
	int periodic(int msec_since_last_period);

	/**
	 * @param idx The entry's index
	 * @return entry from index */
	raft_entry_t& get_entry_from_idx(int etyidx);

	/**
	 * Receive a response from an appendentries message we sent
	 * @param node Who sent us the response
	 * @param r The appendentries response
	 * @return 0 on error */
	int recv_appendentries_response(RaftNode2* node, msg_appendentries_response_t* r);
	/**
	 * Receive an appendentries message
	 * @param node Who sent us the response
	 * @param ae The appendentries message
	 * @return 0 on error */
	int recv_appendentries(bool a_bAsResponce,RaftNode2* node, MsgAppendEntries2* ae);
	/**
	 * Receive a requestvote message
	 * @param node Who sent us the message
	 * @param vr The requestvote message
	 * @return 0 on error */
	int recv_requestvote(RaftNode2* node, msg_requestvote_t* vr);

	/**
	 * Receive a response from a requestvote message we sent
	 * @param node Who sent us the response
	 * @param r The requestvote response
	 */
	void recv_requestvote_response(RaftNode2* node, msg_requestvote_response_t* r);

	int send_entry_response(RaftNode2* node, int etyid, int was_committed);

	/**
	 * Receive an entry message from client.
	 * Append the entry to the log
	 * Send appendentries to followers
	 * @param node The node this response was sent by
	 * @param e The entry message */
	int recv_entry(RaftNode2* node, msg_entry_t* e);

	void send_requestvote(RaftNode2* node);

	int append_entry(const raft_entry_t& c);

	int apply_entry();

	void send_appendentries(RaftNode2* node);

	void send_appendentries_all();
	/**
	 * Set configuration
	 * @param nodes Array of nodes, end of array is marked by NULL entry
	 * @param my_idx Which node is myself */
	//void set_configuration(const std::vector<raft_node_configuration_t>& nodes, raft_node_configuration_t myNode);

	/**
	 * @return number of votes this server has received this election */
	int get_nvotes_for_me();

	void vote(RaftNode2* node);

	/**
	 * @param node The node's index
	 * @return node pointed to by node index
	 */
	//NodeIter get_node(size_t nodeid);

	/**
	 * @return 1 if follower; 0 otherwise */
	bool is_follower();

	/**
	 * @return 1 if node is leader; 0 otherwise */
	bool is_leader();

	/**
	 * @return 1 if candidate; 0 otherwise */
	bool is_candidate();

	/**
	 * Set election timeout
	 * @param millisec Election timeout in milliseconds */
	void set_election_timeout(int millisec);

	/**
	 * Set request timeout in milliseconds
	 * @param millisec Request timeout in milliseconds */
	void set_request_timeout(int millisec);

	/**
	 * @return the server's node ID */
	//int get_nodeid();

	/**
	 * @return currently configured election timeout in milliseconds */
	int get_election_timeout();

	int get_request_timeout();

	/**
	 * @return number of nodes that this server has */
	size_t get_num_nodes();

	/**
	 * @return currently elapsed timeout in milliseconds */
	int get_timeout_elapsed();

	/**
	 * @return number of items within log */
	int get_log_count();

	/**
	 * @return node ID of who I voted for */
	RaftNode2* get_voted_for();

	void set_current_term(int term);

	/**
	 * @return current term */
	int get_current_term();

	void set_current_idx(int idx);

	/**
	 * @return current log index */
	int get_current_idx();

	RaftNode2* get_myNode();

	void set_commit_idx(int idx);

	void set_last_applied_idx(int idx);

	/**
	 * @return index of last applied entry */
	int get_last_applied_idx();

	int get_commit_idx();

	inline Raft::State& get_state() {
		return d_state;
	}

public:
	const common::ListspecialAndHashtbl<RaftNode2*>& allNodes()const {return m_Nodes;}
	bool			FindNode(const void* a_key, size_t a_keyLength, RaftNode2** a_ppNode)const;
	RaftNode2*		firstNode()const;
	int				nodesCount()const;
	void			ClearAllNodes(std::string* a_pDataFromLeader);  // this will be called during destructing
	void			RemoveNode1(const void* a_pKey, size_t a_keySize, std::string* a_pDataFromLeader);
	void			RemoveNode2(RaftNode2* node, std::string* a_pDataFromLeader);
	RaftNode2*		AddNode(const void* a_pKey, size_t a_keySize, std::string* a_pDataFromAdder, bool isAdder, bool isThis);
protected:
	virtual void	CleanNodeData(RaftNode2* pNode, std::string* a_pDataFromLeader);
	virtual void	AddAdditionalDataToNode(RaftNode2* newNode, std::string* a_dataFromAdderAndToOtherNodes, bool isAdder, bool a_bThis);

};

#endif //RAFT_SERVER_H
