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

#define MAX_IP4_LEN				24
#define SOCK_TIMEOUT_MS			100000

namespace raft {namespace tcp {

extern const char g_ccResponceOk;
extern const int32_t g_cnResponceOk;

typedef struct NodeIdentifierKey{ 
	int32_t port;  char ip4Address[MAX_IP4_LEN];
	/*----------------------------------------------*/
	void set_ip4Address1(const std::string& ip4Address);
	void set_ip4Address2(const sockaddr_in*remoteAddr);
	void set_addressAndPort(char* addressAndPort, int32_t defaultPort);
	bool operator==(const NodeIdentifierKey& aM)const;
	bool isSame(const char* a_ip4Address, int32_t a_port)const;
	static void generateKey(const char* a_ip4Address, int32_t a_port, std::string* a_pKey);
}NodeIdentifierKey;


bool ConnectAndGetEndian(common::SocketTCP* a_pSock,const NodeIdentifierKey& nodeInfo, char cRequest, uint32_t* pIsEndianDiffer, int a_nSockTimeout= SOCK_TIMEOUT_MS);
void SendErrorWithString(::common::SocketTCP& a_clientSock, const char* a_cpcErrorString);

}}  // namespace raft {namespace tcp {


#define DEBUG_APP_WITH_NODE(_logLevel,_nodeKey,...)	\
	do{ \
		DEBUG_APPLICATION_NO_NEW_LINE((_logLevel),"%s:%d  ",(_nodeKey)->ip4Address,(int)((_nodeKey)->port)); \
		DEBUG_APPLICATION_NO_ADD_INFO((_logLevel),__VA_ARGS__); DEBUG_APPLICATION_NO_ADD_INFO((_logLevel),"\n"); \
	}while(0)


#endif  // #ifndef __raft_tcp_common_hpp__
