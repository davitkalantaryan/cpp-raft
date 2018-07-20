#ifndef INCLUDED_RAFT_NODE_H
#define INCLUDED_RAFT_NODE_H

#include <stdint.h>

#define BITS_OF_PING_COUNT	10

extern const int64_t	g_cnRaftMaxPing/* = (1 << BITS_OF_PING_COUNT) - 1*/;

class RaftNode2 {

	RaftNode2(const RaftNode2&) {}
	RaftNode2(RaftNode2&&) {}
public:
	RaftNode2();
	~RaftNode2();
	int			is_leader();
	int			get_next_idx();
	void		set_next_idx(int nextIdx);
	void*		get_udata();
	void		set_udata(void* a_udata);
	void		makeLeader();
	void		resetLeader();
	void		SetVotesForMe(int vote);
	int			GetVotesForMe()const;
	uint64_t	isProblematic()const;
	void		setProblematic();
	int64_t		pingCount()const;
	void		pingReceived();
	int64_t		makePing();
	void		SetUnableToVote();
	uint64_t	isAbleToVote()const;
	uint64_t	okCount2()const {return m_okCount2;}
	void		incrementOkCount() {++m_okCount2;}
 
public:
	RaftNode2 * prev, *next;
	void*		key;
	size_t		keyLength;
private:
	void*		m_d_udata;
	int			next_idx;
	int			m_votes_for_me;
	uint64_t	m_isLeader : 1;
	uint64_t	m_isProblematic : 1;
	uint64_t	m_isAbleToVote : 1;
	int64_t		m_nPingCount : BITS_OF_PING_COUNT;
	uint64_t	m_okCount2 : 10;
	// for future use
public:
	uint64_t	m_isTimeToPing : 1;
	uint64_t	m_hasData : 1;
};

#endif  //INCLUDED_RAFT_NODE_H
