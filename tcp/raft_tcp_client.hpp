//
// File:	raft_tcp_client.hpp
// Created on:	2018 May 15
// To include:	#include "raft_tcp_client.hpp"
//

#ifndef __raft_tcp_client_hpp__
#define __raft_tcp_client_hpp__


#include <common/common_sockettcp.hpp>
#include <vector>
#include "raft_tcp_common.hpp"

namespace raft { namespace tcp{

class Client
{
public:
	Client();
	virtual ~Client();

	static int ReceiveAllNodes(const char* nodeIp, int port, std::vector<NodeIdentifierKey>* pNodes);

protected:
	//common::SocketTCP	m_socket;
};

}} // namespace raft { namespace tcp{


#endif  // #ifndef __raft_tcp_client_hpp__
