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

#define MAX_IP4_LEN2				24
#define SOCK_TIMEOUT_MS				100000
#define NODE_KEY_DATA_LEN(_key)		(MAX_IP4_LEN2+4)
#define STRING_START_OFFSET			4

namespace raft {namespace tcp {

extern const char g_ccResponceOk;

class NodeIdentifierKey2
{ 
public:
	NodeIdentifierKey2();
	NodeIdentifierKey2(NodeIdentifierKey2&& tmpKey);
	NodeIdentifierKey2& operator=(NodeIdentifierKey2&& a_rightSide);
	void set_ip4addressAndPort(const std::string& ip4Address, int32_t defaultPort);
	//bool isSameNode2(const std::string& a_ip4Address, int32_t a_port)const;
	bool isSameNode1(const char* otherKey, size_t a_unKeyLen)const;
	const std::string& key()const;
	const char* ip4Address()const;
	int32_t port()const;
	char* data();
	const char* data()const;
	void swapPort();
	
	static void generateKey2(const std::string& a_ip4Address, int32_t a_port, std::string* a_pKey);
	static const char* hostNameFromKey(const void* key);
	static int32_t portFromKey(const void* key);

private:
	//char ip4Address[MAX_IP4_LEN]; int32_t port;
	std::string		m_portAndIpV4;
};


bool ConnectAndGetEndian(common::SocketTCP* a_pSock,const NodeIdentifierKey2& nodeInfo, char cRequest, uint32_t* pIsEndianDiffer);

}}  // namespace raft {namespace tcp {


#define HOST_NAME_FROM_KEY(_nodeKey)	((_nodeKey)+STRING_START_OFFSET)
#define PORT_FROM_KEY(_nodeKey)			(  *((int32_t*)(_nodeKey))  )

#define DEBUG_APP_WITH_KEY(_logLevel,_nodeKey,...)	\
	do{ \
		DEBUG_APPLICATION_NO_NEW_LINE((_logLevel),"%s:%d  ",HOST_NAME_FROM_KEY(_nodeKey),PORT_FROM_KEY(_nodeKey)); \
		DEBUG_APPLICATION_NO_ADD_INFO((_logLevel),__VA_ARGS__); DEBUG_APPLICATION_NO_ADD_INFO((_logLevel),"\n"); \
	}while(0)


#endif  // #ifndef __raft_tcp_common_hpp__
