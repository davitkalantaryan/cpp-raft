//
// File:	raft_tcp_client.hpp
// Created on:	2018 May 15
// To include:	#include "raft_tcp_client.hpp"
//

#ifndef __raft_tcp_client_hpp__
#define __raft_tcp_client_hpp__


#include <common_sockettcp.hpp>


namespace raft { namespace tcp{

class Client
{
public:
	Client();
	virtual ~Client();
};

}} // namespace raft { namespace tcp{


#endif  // #ifndef __raft_tcp_client_hpp__
