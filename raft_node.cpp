#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "raft.h"
#include "raft_node.h"

#if 0
RaftNode2 * prev, *next;
void*		m_d_udata;
void*		m_key;
int			next_idx;
int			m_isLeader;
int			m_votes_for_me;
int			m_keyLen;
#endif

const uint32_t	g_cunRaftMaxPing = (1 << BITS_OF_PING_COUNT)-1;

RaftNode2::RaftNode2(void* a_udata) :
	prev(NULL),
	next(NULL),
	m_d_udata(a_udata),
	m_key(NULL),
	next_idx(0),
	m_votes_for_me(0),
	m_keyLen(0)
{
	m_isLeader = 0;
	m_isProblematic = 0;
	m_isAbleToVote = 1;
	m_unPingCount = 0;
}


uint32_t RaftNode2::unansweredPingCount()const
{
	return m_unPingCount;
}


void RaftNode2::pingReceived()
{
	m_isProblematic = 0;
	m_unPingCount = 0;
	m_isAbleToVote = 1;
}


uint32_t RaftNode2::makePing(uint32_t a_unCount)
{
	if((m_unPingCount+ a_unCount)<=g_cunRaftMaxPing){m_unPingCount += a_unCount;}
	return m_unPingCount;
}


void RaftNode2::SetUnableToVote()
{
	m_isAbleToVote=0;
}


uint32_t RaftNode2::isAbleToVote()const
{
	return m_isAbleToVote;
}


uint32_t RaftNode2::isProblematic()const
{
	return m_isProblematic;
}


void RaftNode2::setProblematic(uint32_t a_problematic)
{
	m_isProblematic = a_problematic;
}


void RaftNode2::setKeyAndKeylen(void* a_key, int a_keyLen)
{
	m_key = a_key;
	m_keyLen = a_keyLen;
}


int RaftNode2::is_leader()
{
    // TODO
    return m_isLeader;
}

int RaftNode2::get_next_idx()
{
    return this->next_idx;
}

void RaftNode2::set_next_idx(int nextIdx)
{
    this->next_idx = nextIdx;
}

void RaftNode2::SetVotesForMe(int a_vote)
{
	m_votes_for_me = a_vote;
}


int RaftNode2::GetVotesForMe()const
{
	return m_votes_for_me;
}

void* RaftNode2::get_udata()
{
    return m_d_udata;
}


void RaftNode2::set_udata(void* a_udata)
{ 
	m_d_udata = a_udata; 
}


void RaftNode2::makeLeader(int a_isLeader)
{
	m_isLeader = a_isLeader;
}
