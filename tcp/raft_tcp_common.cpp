//
// file:		raft_tcp_common.cpp
// created on:	2018 May 11
//

#include "raft_tcp_common.hpp"
#include <memory.h>

#pragma warning(disable:4996)


bool raft::tcp::NodeIdentifierKey::operator==(const NodeIdentifierKey& a_aM)const
{
	return (memcmp(this, &a_aM, sizeof(a_aM)) == 0);
}


void raft::tcp::NodeIdentifierKey::set_ip4Address(const std::string& a_hostName)
{
	memset(this->ip4Address, 0, MAX_IP4_LEN);
	strncpy(this->ip4Address, a_hostName.c_str(), MAX_IP4_LEN);
}
