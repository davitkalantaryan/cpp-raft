#ifndef INCLUDED_RAFT_NODE_H
#define INCLUDED_RAFT_NODE_H

#include <stdint.h>

#define BITS_OF_PING_COUNT	6

extern const uint32_t	g_cunRaftMaxPing/* = (1 << BITS_OF_PING_COUNT) - 1*/;

class RaftNode2 {

public:

	RaftNode2(void* udata);
	int			is_leader();
	int			get_next_idx();
	void		set_next_idx(int nextIdx);
	void*		get_udata();
	void		set_udata(void* a_udata);
	void		makeLeader(int isLeader=1);
	void		SetVotesForMe(int vote = 1);
	int			GetVotesForMe()const;
	uint32_t	isProblematic()const;
	void		setProblematic(uint32_t problematic=1);
	uint32_t	unansweredPingCount()const;
	void		pingReceived();
	uint32_t	makePing(uint32_t a_unCount);
	void		SetUnableToVote();
	uint32_t	isAbleToVote()const;
 
public:
	RaftNode2 * prev, *next;
	void*		key;
	size_t		keyLength;
private:
	void*		m_d_udata;
	int			next_idx;
	int			m_votes_for_me;
	uint32_t	m_isLeader : 1;
	uint32_t	m_isProblematic : 1;
	uint32_t	m_isAbleToVote : 1;
	uint32_t	m_unPingCount : BITS_OF_PING_COUNT;
};

#endif  //INCLUDED_RAFT_NODE_H
