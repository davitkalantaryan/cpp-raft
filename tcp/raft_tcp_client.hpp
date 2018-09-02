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
#include <string>
#include <stdint.h>

namespace raft { namespace tcp{

class Client
{
public:
	Client();
	virtual ~Client();

	void SetNodeDetails(const char* hostNameOrIp4Address, int port);
	int ReceiveAllNodes( ::std::vector<NodeIdentifierKey>* pNodes );
	int pingToNode( ::std::string* a_pPingResult );
    int GetStartTime(int32_t* a_pStartTime);

protected:
	//common::SocketTCP	m_socket;
	NodeIdentifierKey	m_nodeId;
};

}} // namespace raft { namespace tcp{


#endif  // #ifndef __raft_tcp_client_hpp__
