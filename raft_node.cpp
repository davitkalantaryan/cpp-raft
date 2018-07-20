#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "raft.h"
#include "raft_node.h"

#define INITIAL_PING_COUNT	(-25)

#if 0
RaftNode2 * prev, *next;
void*		m_d_udata;
void*		m_key;
int			next_idx;
int			m_isLeader;
int			m_votes_for_me;
int			m_keyLen;
#endif

const int64_t	g_cnRaftMaxPing = (int64_t)((1 << BITS_OF_PING_COUNT)-4);

RaftNode2::RaftNode2() 
	:
	prev(NULL),
	next(NULL),
	m_d_udata(NULL),
	next_idx(0),
	m_votes_for_me(0)
{
	m_isLeader = 0;
	m_isProblematic = 0;
	m_isAbleToVote = 1;
	m_nPingCount = INITIAL_PING_COUNT;
	//
	m_isTimeToPing =0;
	m_hasData = 0;
	m_okCount2 = 0;

	this->key = NULL;
	this->keyLength = 0;
}


RaftNode2::~RaftNode2()
{
}


int64_t RaftNode2::pingCount()const
{
	return m_nPingCount;
}


void RaftNode2::pingReceived()
{
	m_isProblematic = 0;
	m_nPingCount = 0;
	m_isAbleToVote = 1;
}


int64_t RaftNode2::makePing()
{
	int64_t nPingCount(m_nPingCount);
	m_nPingCount=(nPingCount<g_cnRaftMaxPing)?(nPingCount+1): g_cnRaftMaxPing;
	return m_nPingCount;
}


void RaftNode2::SetUnableToVote()
{
	m_isAbleToVote=0;
}


uint64_t RaftNode2::isAbleToVote()const
{
	return m_isAbleToVote;
}


uint64_t RaftNode2::isProblematic()const
{
	return m_isProblematic;
}


void RaftNode2::setProblematic()
{
	m_isProblematic = 1;
}


int RaftNode2::is_leader()
{
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


void RaftNode2::makeLeader()
{
	m_isLeader = 1;
}


void RaftNode2::resetLeader()
{
	m_isLeader = 0;
}
