/**
 * Copyright (c) 2013, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @file
 * @brief Implementation of a Raft server
 * @author Willem Thiart
 * @version 0.1
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdexcept>
#include <time.h>

#include <stdarg.h>

#include "state_mach.h"
#include "raft.h"
#include "raft_server.h"
#include "raft_logger.h"
#include "raft_node.h"
#include "raft_private.h"
#include "raft_macroses_and_functions.h"
#include <iostream>

#define MINIMUM_RAND_FACTOR	6

static void __log(void *src, const char *fmt, ...) {
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
}

static int raft_votes_is_majority(int a_num_nodes, int a_nvotes);

RaftServer::RaftServer() :
        current_term(0),log(new RaftLogger()),commit_idx(0),last_applied_idx(0),current_idx(1), timeout_elapsed(0), election_timeout(
                1000), request_timeout(200), cb_ctx(NULL)
{
	d_state.set(RAFT_STATE_FOLLOWER);
	//RaftNode2	*m_first, *m_last;
	//size_t		m_unNodesCount;
    m_thisNode = m_lastNode=m_firstNode=NULL;
	m_nNodesCount = 0;
	m_voted_for = NULL;
	m_nLeaderCommit = 0;
}

void RaftServer::set_callbacks(raft_cbs_t *funcs, void *cb_ctx) {
	memcpy(&this->cb, funcs, sizeof(raft_cbs_t));
	this->cb_ctx = cb_ctx;
}

RaftServer::~RaftServer() 
{
	ClearAllNodes();
	//delete this->log;
}


void RaftServer::ClearAllNodes()
{
	RaftNode2 *pNextNode, *pToDelete=m_firstNode;

	while(pToDelete){
		pNextNode = RemoveNode(pToDelete);
		pToDelete = pNextNode;
	}

	m_nNodesCount = 0;
}


void RaftServer::AddNode(RaftNode2* a_node, const void* a_key, int a_keyLen)
{
	RaftNode2* pNodeTmp;
	void* keyOnHash;

	if(m_hashNodes.FindEntry(a_key,a_keyLen,&pNodeTmp)){return;}

	if(m_lastNode){m_lastNode->next=a_node;}
	else{m_firstNode=a_node;}

	a_node->prev = m_lastNode;
	m_lastNode = a_node;
	keyOnHash=m_hashNodes.AddEntry2(a_key, a_keyLen, a_node);
	a_node->setKeyAndKeylen(keyOnHash, a_keyLen);
	++m_nNodesCount;
}


RaftNode2* RaftServer::FindNode(const void* a_key, int a_keyLen)
{
	RaftNode2* pNodeTmp;
	if(m_hashNodes.FindEntry(a_key,a_keyLen,&pNodeTmp)){return pNodeTmp;}
	return NULL;
}


RaftNode2* RaftServer::RemoveNode(RaftNode2* a_node)
{
	RaftNode2* pNext;
	if(!m_hashNodes.RemoveEntry(a_node->key2(),a_node->keyLen2())){return NULL;}
	pNext = a_node->next;
	if(a_node->prev){ a_node->prev->next=a_node->next; }
	if(a_node->next){ a_node->next->prev=a_node->prev; }
	if(a_node==m_firstNode){m_firstNode=a_node->next;}
	if(a_node==m_lastNode){ m_lastNode =a_node->prev;}
	delete a_node;
	--m_nNodesCount;
	return pNext;
}


void RaftServer::RemoveNode(const void* a_key, int a_keyLen)
{
	RaftNode2* pNode;
	if(!m_hashNodes.FindEntry(a_key,a_keyLen,&pNode)){return;}
	RemoveNode(pNode);
}


void RaftServer::forAllNodesExceptSelf(std::function<void(RaftNode2*)> a_callback, bool a_bSkipLeader) {

	RaftNode2* pNode;

	// close with mutex
	pNode = m_firstNode;

	while(pNode){
		if (pNode == m_thisNode) {goto nextNodePoint;}
		if(a_bSkipLeader && (pNode->is_leader())){goto nextNodePoint;}
		a_callback(pNode);
nextNodePoint:
		pNode = pNode->next;
	}

	// open mutex
}

void RaftServer::election_start() {

	__log(NULL, "election starting: %d %d, term: %d", this->election_timeout, this->timeout_elapsed,
			this->current_term);

	become_candidate();
}

void RaftServer::become_leader() {

	__log(NULL, "becoming leader");

	d_state.set(RAFT_STATE_LEADER);
	//this->voted_for = -1;
	this->m_voted_for = NULL;
	forAllNodesExceptSelf([this](RaftNode2* a_node) {
		a_node->set_next_idx(get_current_idx() + 1);
		send_appendentries(a_node);
	}, true);
}

void RaftServer::become_candidate() {
	int nRandFactor = (m_nNodesCount > MINIMUM_RAND_FACTOR) ? m_nNodesCount : MINIMUM_RAND_FACTOR;

	__log(NULL, "becoming candidate");

	//this->votes_for_me.clear(); // automatic every vote is false
	srand((unsigned)time(NULL));
	this->current_term += ( 1 + (rand()%nRandFactor) );
	//vote(this->nodeid);
	vote(this->m_thisNode);
	d_state.set(RAFT_STATE_CANDIDATE);

	/* we need a random factor here to prevent simultaneous candidates */
	this->timeout_elapsed = rand() % 500;

	/* request votes from nodes */
	forAllNodesExceptSelf(&RaftServer::send_requestvote, true);
}

void RaftServer::become_follower() {

	__log(NULL, "becoming follower");

	d_state.set(RAFT_STATE_FOLLOWER);
	this->m_voted_for = NULL;
}

int RaftServer::periodic(int msec_since_last_period) {

	__log(NULL, "periodic elapsed time: %d", this->timeout_elapsed);

	switch (d_state.get()) {
	case RAFT_STATE_FOLLOWER:
		if (this->last_applied_idx < this->commit_idx) {
			if (0 == apply_entry())
				return 0;
		}
		break;
	default:
		break;
	}

	this->timeout_elapsed += msec_since_last_period;

	if (d_state.get() == RAFT_STATE_LEADER) {
		if (this->request_timeout <= this->timeout_elapsed) {
			send_appendentries_all();
			this->timeout_elapsed = 0;
		}
	} else {
		if (this->election_timeout <= this->timeout_elapsed) {
			election_start();
		}
	}

	return 1;
}

raft_entry_t& RaftServer::get_entry_from_idx(int etyidx) {
	return this->log->log_get_from_idx(etyidx);
}

int RaftServer::recv_appendentries_response(RaftNode2* a_node, msg_appendentries_response_t *r) {

	static int nIter = 0;
	__log(NULL, "received appendentries response from: %p", a_node);

	//p = get_node(node);
	//assert(p != this->nodes.end());

	if (1 == r->success) {
		int i;
		DEBUG_APPLICATION( 1,"Increase Success %d",++nIter);
		a_node->set_next_idx(r->current_idx);
		for (i = r->first_idx; i <= r->current_idx; i++)
			this->log->log_mark_node_has_committed(i);

		while (this->get_commit_idx() < r->current_idx) {
			raft_entry_t& e = this->log->log_get_from_idx(this->last_applied_idx + 1);
			/* majority has this */
			if (m_nNodesCount / 2 <= e.d_num_nodes) {
				if (1 != apply_entry())
					throw std::runtime_error("Could Not Apply!");
				if (e.getId() == r->current_idx) {
					return 1;
				}
			} else {
				break;
			}
		}
	} else {
		/* If AppendEntries fails because of log inconsistency:
		 decrement nextIndex and retry (�5.3) */
		//assert(0 <= a_node->get_next_idx());   // comented by DK
		// TODO does this have test coverage?
		// TODO can jump back to where node is different instead of iterating
		//a_node->set_next_idx(a_node->get_next_idx() - 1);  // comented by DK
		send_appendentries(a_node);
	}

	return 1;
}

int RaftServer::recv_appendentries(RaftNode2* a_node, MsgAppendEntries2 *ae) {
	msg_appendentries_response_t r;

	this->timeout_elapsed = 0;

	__log(NULL, "received appendentries from: %p", a_node);

	r.term = this->current_term;

	/* we've found a leader who is legitimate */
	if (d_state.is_leader() && this->current_term <= ae->getTerm())
		become_follower();

	/* 1. Reply false if term < currentTerm (�5.1) */
	if (ae->getTerm() < this->current_term) {
		__log(NULL, "AE term is less than current term");
		r.success = 0;
		goto done;
	}

	/* not the first appendentries we've received */
	if (ae->hasAnyLogs()) {
		try {
			raft_entry_t& e = get_entry_from_idx(ae->getPrevLogIdx());
			/* 2. Reply false if log doesn�t contain an entry at prevLogIndex
			 whose term matches prevLogTerm (�5.3) */
			if (e.d_term != ae->getPrevLogTerm()) {
				__log(NULL, "AE term doesn't match prev_idx");
				r.success = 0;
				goto done;
			}
			/* 3. If an existing entry conflicts with a new one (same index
			 but different terms), delete the existing entry and all that
			 follow it (�5.3) */
			try {
                //raft_entry_t& e2 = get_entry_from_idx(ae->getPrevLogIdx() + 1);
				this->log->log_delete(ae->getPrevLogIdx() + 1);
            } catch (std::runtime_error&){
			}

        } catch (std::runtime_error&) {
			__log(NULL, "AE no log at prev_idx");
			r.success = 0;
			goto done;
			//assert(0);
		}
	}

	/* 5. If leaderCommit > commitIndex, set commitIndex =
	 min(leaderCommit, last log index) */
	if (get_commit_idx() < ae->getLeaderCommit()) {
		try {
			const raft_entry_t& e = this->log->log_peektail();
			set_commit_idx(e.d_id < ae->getLeaderCommit() ? e.d_id : ae->getLeaderCommit());
			set_last_applied_idx(e.d_id < ae->getLeaderCommit() ? e.d_id : ae->getLeaderCommit());
			while (1 == apply_entry())
				;
		} catch (std::runtime_error& err) {

		}
	}

	if (d_state.is_candidate())
		become_follower();

	set_current_term(ae->getTerm());

	/* append all entries to log */
	for (int i = 0; i < ae->getNEntries(); i++) {
		/* TODO: replace malloc with mempoll/arena */
		raft_entry_t c;
		c.d_term = this->current_term;
		c.populateFromMsgEntry(ae->getEntry(i));
		if (0 == append_entry(c)) {
			__log(NULL, "AE failure; couldn't append entry");
			r.success = 0;
			goto done;
		}
	}

	r.success = 1;
	r.current_idx = get_current_idx();
	r.first_idx = ae->getPrevLogIdx() + 1;

	done: if (this->cb.send)
		this->cb.send(this->cb_ctx, this, a_node, RAFT_MSG_APPENDENTRIES_RESPONSE, (const unsigned char*) &r,
				sizeof(msg_appendentries_response_t));
	return 1;
}

int RaftServer::recv_requestvote(RaftNode2* a_node, msg_requestvote_t *vr) {
	msg_requestvote_response_t r;

	if (get_current_term() < vr->term()) {
		this->m_voted_for = NULL;
	}

	if (vr->term() < get_current_term() || /* we've already voted */
	NULL != this->m_voted_for || /* we have a more up-to-date log */
	vr->last_log_idx() < this->current_idx) {
		r.vote_granted = 0;
	} else {
		vote(a_node);
		r.vote_granted = 1;
	}

	__log(NULL, "node requested vote: %p replying: %s", a_node, r.vote_granted == 1 ? "granted" : "not granted");

	r.term = get_current_term();
	if (this->cb.send)
		this->cb.send(this->cb_ctx, this, a_node, RAFT_MSG_REQUESTVOTE_RESPONSE, (const unsigned char *) &r,
				sizeof(msg_requestvote_response_t));

	return 0;
}

void RaftServer::recv_requestvote_response(RaftNode2* a_node, msg_requestvote_response_t *r) {

	__log(NULL, "node responded to requestvote: %p status: %s", a_node, r->vote_granted == 1 ? "granted" : "not granted");

	if (d_state.is_leader())
		return;

	if (1 == r->vote_granted) {
		a_node->SetVotesForMe(1);
		if (raft_votes_is_majority(m_nNodesCount-1, get_nvotes_for_me())){ // -1 because old leader is not normal, but still counted
			printf("!!!!!!!!!!!!! line:%d, trying to became leader\n",__LINE__);
			become_leader();
		}
	}
	else{
		a_node->SetVotesForMe(0);
	}
}


int RaftServer::send_entry_response(RaftNode2* a_node, int etyid, int was_committed) {
	msg_entry_response_t res;

	__log(NULL, "send entry response to: %p", a_node);

	res.id = etyid;
	res.was_committed = was_committed;
	if (this->cb.send)
		this->cb.send(this->cb_ctx, this, a_node, RAFT_MSG_ENTRY_RESPONSE, (const unsigned char*) &res,
				sizeof(msg_entry_response_t));
	return 0;
}


int RaftServer::recv_entry(RaftNode2* a_node, msg_entry_t *e) {
	raft_entry_t ety(this->current_term, e->id(), reinterpret_cast<char*>(e->data()), e->len());
	__log(NULL, "received entry from: %p", a_node);
	send_entry_response(a_node, e->id(), append_entry(ety));
	forAllNodesExceptSelf(std::bind(&RaftServer::send_appendentries, this, std::placeholders::_1),false);
	return 0;
}


void RaftServer::send_requestvote(RaftNode2* a_node) {
	msg_requestvote_t rv(current_term, 0, get_current_idx(), 0);

	__log(NULL, "sending requestvote to: %p", a_node);

	if (this->cb.send)
		this->cb.send(this->cb_ctx, this, a_node, RAFT_MSG_REQUESTVOTE, (const unsigned char*) &rv,
				sizeof(msg_requestvote_t));
}

int RaftServer::append_entry(const raft_entry_t& c) {

	if (1 == this->log->log_append_entry(c)) {
		this->current_idx += 1;
		return 1;
	}
	return 0;
}

int RaftServer::apply_entry() {
	raft_entry_t e;

	try {
		e = this->log->log_get_from_idx(this->last_applied_idx + 1);
	} catch (std::runtime_error& err) {
		return 0;
	}

	__log(NULL, "applying log: %d", this->last_applied_idx);

	this->last_applied_idx++;
	if (get_commit_idx() < this->last_applied_idx)
		set_commit_idx(this->last_applied_idx);
	if (this->cb.applylog)
		this->cb.applylog(this->cb_ctx, this, reinterpret_cast<const unsigned char*>(e.d_data), e.d_len);
	return 1;
}

void RaftServer::send_appendentries(RaftNode2* a_node) 
{

	__log(NULL, "sending appendentries to: %p", a_node);

	if (!(this->cb.send))
		return;

	MsgAppendEntries2 ae(this->current_term, 0, a_node->get_next_idx(), 0, m_nLeaderCommit);
	this->cb.send(this->cb_ctx, this, a_node, RAFT_MSG_APPENDENTRIES, (const unsigned char*) &ae,
			SIZE_OF_INITIAL_RCV_OF_MSG_APP);
	if (ae.getNEntries()) {
		this->cb.send(this->cb_ctx, this,a_node,RAFT_MSG_APPENDENTRIES, 
			(const unsigned char*)ae.entries(),ae.getNEntries()*sizeof(msg_entry_t));
	}
}

void RaftServer::send_appendentries_all() {
	++m_nLeaderCommit;
	forAllNodesExceptSelf(&RaftServer::send_appendentries, false);
}

#if 0
void RaftServer::set_configuration(const std::vector<raft_node_configuration_t>& a_nodes, raft_node_configuration_t a_myNode)
{
	RaftNode2* pNode;
	const size_t cunSize(a_nodes.size());

	this->ClearAllNodes();
	for (size_t i(0); i<cunSize; ++i) {
		pNode = new RaftNode2(a_nodes[i].pUserData); // let us assume that memory is enough
		AddNode(pNode, a_nodes[i].key, a_nodes[i].keyLen);
		if(memcmp(&a_nodes[i],&a_myNode,sizeof(a_myNode))==0){this->m_thisNode=pNode;}
	}
}
#endif

int RaftServer::get_nvotes_for_me() 
{
	RaftNode2*	pNode = m_firstNode;
	int votes = 0;

	while(pNode){
		if(pNode==m_thisNode){ pNode= pNode->next; continue;}
		if (1 == pNode->GetVotesForMe()) {
				votes += 1;
		}
		pNode = pNode->next;
	}

	if (this->m_voted_for == this->m_thisNode)
		votes += 1;

	return votes;
}

void RaftServer::vote(RaftNode2* a_node) {
	m_voted_for = a_node;
}


void RaftServer::set_election_timeout(int millisec) {
	this->election_timeout = millisec;
}

void RaftServer::set_request_timeout(int millisec) {
	this->request_timeout = millisec;
}

//int RaftServer::get_nodeid() {
//	return this->nodeid;
//}

int RaftServer::get_election_timeout() {
	return this->election_timeout;
}

int RaftServer::get_request_timeout() {
	return this->request_timeout;
}

size_t RaftServer::get_num_nodes() {
	return m_nNodesCount;
}

int RaftServer::get_timeout_elapsed() {
	return this->timeout_elapsed;
}

int RaftServer::get_log_count() {
	return this->log->log_count();
}

RaftNode2* RaftServer::get_voted_for() {
	return this->m_voted_for;
}

void RaftServer::set_current_term(int term) {
	this->current_term = term;
}

int RaftServer::get_current_term() {
	return this->current_term;
}

void RaftServer::set_current_idx(int idx) {
	this->current_idx = idx;
}

int RaftServer::get_current_idx() {
	return this->current_idx;
}

RaftNode2* RaftServer::get_myNode() {
	return this->m_thisNode;
}

void RaftServer::set_commit_idx(int idx) {
	this->commit_idx = idx;
}

void RaftServer::set_last_applied_idx(int idx) {
	this->last_applied_idx = idx;
}

int RaftServer::get_last_applied_idx() {
	return this->last_applied_idx;
}

int RaftServer::get_commit_idx() {
	return this->commit_idx;
}


bool RaftServer::is_follower()
{
	return this->d_state.is_follower();
}

bool RaftServer::is_leader()
{
	return this->d_state.is_leader();
}


bool RaftServer::is_candidate()
{
	return this->d_state.is_candidate();
}


/*////////////////////////////////////////////////////////////////*/
static int raft_votes_is_majority(int a_num_nodes, int a_nvotes) 
{
	int half;

	if (a_num_nodes < a_nvotes)
		return 0;
	half = a_num_nodes / 2;
	return half + 1 <= a_nvotes;
}


/*--------------------------------------------------------------79-characters-*/
