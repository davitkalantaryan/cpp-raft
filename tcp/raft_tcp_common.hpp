//
// file:		raft_tcp_common.hpp 
// created on:	2018 May 11
// 
// Include file for common part of Raft TCP server client
// To include   #include "raft_tcp_common.hpp"
//

#ifndef __raft_tcp_common_hpp__
#define __raft_tcp_common_hpp__

#ifndef __printfWithTime_defined
#define printfWithTime_defined
#endif

#include <common/common_sockettcp.hpp>
#include <stdint.h>
#include <string>
#include "raft_macroses_and_functions.h"

#if !defined(_WIN32) && !defined(Sleep)
#define Sleep(_x) (  ((_x)>100000) ? sleep((_x)/1000) : usleep(1000*(_x))  )
#endif

#define MAX_IP4_LEN				24
#define SOCK_TIMEOUT_MS			100000

namespace raft {namespace tcp {

extern const char g_ccResponceOk;

typedef struct NodeIdentifierKey{ 
	char ip4Address[MAX_IP4_LEN]; int32_t port;
	/*----------------------------------------------*/
	void set_ip4Address(const std::string& ip4Address);
	void set_addressAndPort(char* addressAndPort, int32_t defaultPort);
	bool operator==(const NodeIdentifierKey& aM)const;
	bool isSame(const char* a_ip4Address, int32_t a_port)const;
	static void generateKey(const char* a_ip4Address, int32_t a_port, std::string* a_pKey);
}NodeIdentifierKey;


bool ConnectAndGetEndian(common::SocketTCP* a_pSock,const NodeIdentifierKey& nodeInfo, char cRequest, uint32_t* pIsEndianDiffer);

}}  // namespace raft {namespace tcp {


#define DEBUG_APP_WITH_NODE(_logLevel,_nodeKey,...)	\
	do{ \
		DEBUG_APPLICATION_NO_NEW_LINE((_logLevel),"%s:%d  ",(_nodeKey)->ip4Address,(int)((_nodeKey)->port)); \
		DEBUG_APPLICATION_NO_ADD_INFO((_logLevel),__VA_ARGS__); DEBUG_APPLICATION_NO_ADD_INFO((_logLevel),"\n"); \
	}while(0)


#endif  // #ifndef __raft_tcp_common_hpp__
