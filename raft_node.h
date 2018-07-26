#ifndef INCLUDED_RAFT_NODE_H
#define INCLUDED_RAFT_NODE_H

#include <stdint.h>
#include <sys/timeb.h>
#include <atomic>

#define BITS_OF_OK_COUNT	10

#define MSEC(_finish, _start)	( (int64_t)( (_finish).millitm - (_start).millitm ) + \
							(int64_t)( (_finish).time - (_start).time ) * 1000 )

extern const int64_t	g_cnRaftMaxPing/* = (1 << BITS_OF_PING_COUNT) - 1*/;

class RaftNode2 {

	RaftNode2(const RaftNode2&) {}
	RaftNode2(RaftNode2&&) {}
public:
	RaftNode2();
	~RaftNode2();
	int				is_leader();
	int				get_next_idx();
	void			set_next_idx(int nextIdx);
	void*			get_udata();
	void			set_udata(void* a_udata);
	void			makeLeader();
	void			resetLeader();
	void			setVotesForMe(int vote);
	int				getVotesForMe()const;
	void			pingReceived();
	void			setUnableToVote();
	uint64_t		isAbleToVote()const;
	int64_t			okCount()const {return m_okCount;}
	void			incrementOkCount() {++m_okCount;}
	void			incrementLock2();
	void			decrementLock2();
	int				lockCount2()const;
	const timeb&	lastSeen()const {return m_lastSeen;}
 
public:
	RaftNode2 * prev, *next;
	void*		key;
	size_t		keyLength;
private:
	std::atomic<int>	m_locksCount;
	void*		m_d_udata;
	int			next_idx;
	int			m_votes_for_me;
	uint64_t	m_isLeader : 1;
	uint64_t	m_isAbleToVote : 1;
	int64_t		m_okCount : BITS_OF_OK_COUNT;
	timeb		m_lastSeen;

	// for future use
public:
	uint64_t	m_isTimeToPing : 1;
	uint64_t	m_hasData : 1;
};

#endif  //INCLUDED_RAFT_NODE_H
