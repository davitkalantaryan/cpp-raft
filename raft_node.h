#ifndef INCLUDED_RAFT_NODE_H
#define INCLUDED_RAFT_NODE_H

class RaftNode2 {

public:

	RaftNode2(void* udata);
	int is_leader();
	int get_next_idx();
	void set_next_idx(int nextIdx);
	void* get_udata();
	void set_udata(void* a_udata);
	void makeLeader(int isLeader=1);
	void SetVotesForMe(int vote = 1);
	int  GetVotesForMe()const;
	int  keyLen2()const {return m_keyLen;}
	void*  key2() { return m_key; }
	void setKeyAndKeylen(void* key, int keyLen);
 
public:
	RaftNode2 * prev, *next;
private:
	void*		m_d_udata;
	void*		m_key;
	int			next_idx;
	int			m_isLeader;
	int			m_votes_for_me;
	int			m_keyLen;
};

#endif  //INCLUDED_RAFT_NODE_H
