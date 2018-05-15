//
// file:		raft_tcp_common.hpp 
// created on:	2018 May 11
// 
// Include file for common part of Raft TCP server client
// To include   #include "raft_tcp_common.hpp"
//


#include <common_socketbase.hpp>
#include <stdint.h>
#include <string>

#if !defined(_WIN32) && !defined(Sleep)
#define Sleep(_x) (  ((_x)>100000) ? sleep((_x)/1000) : usleep(1000*(_x))  )
#endif

#define MAX_IP4_LEN				24
#define SOCK_TIMEOUT_MS			100000

namespace raft {namespace tcp {

typedef struct NodeIdentifierKey{ 
	char ip4Address[MAX_IP4_LEN]; int32_t port;
	/*----------------------------------------------*/
	void set_ip4Address(const std::string& ip4Address);
	bool operator==(const NodeIdentifierKey& aM)const;
}NodeIdentifierKey;

}}
