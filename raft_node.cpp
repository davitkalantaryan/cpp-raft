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


RaftNode2::RaftNode2() 
	:
	prev(NULL),
	next(NULL),
	m_d_udata(NULL),
	next_idx(0),
	m_votes_for_me(0)
{
	m_isLeader = 0;
	m_isAbleToVote = 1;
	m_okCount = INITIAL_PING_COUNT;
	//
	m_isTimeToPing =0;
	m_hasData = 0;
	m_lockCount = 0;
	ftime(&this->m_lastSeen);

	this->key = NULL;
	this->keyLength = 0;
}


RaftNode2::~RaftNode2()
{
}


void RaftNode2::incrementLock()
{
	++m_lockCount;
}


void RaftNode2::decrementLock()
{
	--m_lockCount;
}


uint64_t RaftNode2::lockCount()const
{
	return m_lockCount;
}


void RaftNode2::pingReceived()
{
	if(m_okCount<0){m_okCount=0;}
	m_isAbleToVote = 1;
	ftime(&this->m_lastSeen);
}


void RaftNode2::setUnableToVote()
{
	m_isAbleToVote=0;
}


uint64_t RaftNode2::isAbleToVote()const
{
	return m_isAbleToVote;
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

void RaftNode2::setVotesForMe(int a_vote)
{
	m_votes_for_me = a_vote;
}


int RaftNode2::getVotesForMe()const
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
