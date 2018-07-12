//
// file:		raft_tcp_common.cpp
// created on:	2018 May 11
//

#include "raft_tcp_common.hpp"
#include <memory.h>
#include "raft_macroses_and_functions.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif


bool raft::tcp::NodeIdentifierKey::operator==(const NodeIdentifierKey& a_aM)const
{
	return (memcmp(this, &a_aM, sizeof(a_aM)) == 0);
}


bool raft::tcp::NodeIdentifierKey::isSame(const char* a_ip4Address, int32_t a_port)const
{
	return (  (strncmp(this->ip4Address, a_ip4Address, MAX_IP4_LEN) == 0) && (this->port == a_port)   );
}


void raft::tcp::NodeIdentifierKey::generateKey(const char* a_ip4Address, int32_t a_port, std::string* a_pKey)
{
	a_pKey->resize(MAX_IP4_LEN+4,0);
	strncpy(const_cast<char*>(a_pKey->data()),a_ip4Address,MAX_IP4_LEN);
	*((int32_t*)(const_cast<char*>(a_pKey->data()) + MAX_IP4_LEN)) = a_port;
}


void raft::tcp::NodeIdentifierKey::set_ip4Address(const std::string& a_hostName)
{
	memset(this->ip4Address, 0, MAX_IP4_LEN);
	strncpy(this->ip4Address, a_hostName.c_str(), MAX_IP4_LEN);
}


void raft::tcp::NodeIdentifierKey::set_addressAndPort(char* a_addressAndPort, int32_t a_defaultPort)
{
	char* pcPortStart = strchr(a_addressAndPort, ':');
	
	memset(this->ip4Address, 0, MAX_IP4_LEN);
	if(pcPortStart){
		*pcPortStart = 0;
		this->port = atoi(pcPortStart + 1);
	}
	else {this->port=a_defaultPort;}
	strncpy(this->ip4Address, a_addressAndPort, MAX_IP4_LEN);
}


namespace raft{namespace tcp{

const char g_ccResponceOk= response::ok;
int g_nLogLevel = 0;

bool ConnectAndGetEndian(common::SocketTCP* a_pSock, const NodeIdentifierKey& a_nodeInfo,char a_cRequest, uint32_t* a_pIsEndianDiffer)
{
	int nSndRcv;
	uint16_t unRemEndian;

	if(a_pSock->connectC(a_nodeInfo.ip4Address, a_nodeInfo.port,500)){
		a_pSock->closeC();
		DEBUG_APP_WITH_NODE(2, a_nodeInfo,"Unable to connect");
		return false;
	}
	a_pSock->setTimeout(-1);

	nSndRcv = a_pSock->readC(&unRemEndian, 2);
	a_pSock->setTimeout(SOCK_TIMEOUT_MS);
	if(nSndRcv!=2){
		a_pSock->closeC();
		DEBUG_APP_WITH_NODE(2, a_nodeInfo, "Unable to get endian. retCode=%d", nSndRcv);
		return false;
	}
	if(unRemEndian==1){*a_pIsEndianDiffer=0;}
	else {*a_pIsEndianDiffer = 1;}

	//cRequest = a_connectionCode;
	nSndRcv = a_pSock->writeC(&a_cRequest,1);
	if (nSndRcv != 1) { 
		a_pSock->closeC(); 
		DEBUG_APP_WITH_NODE(1, a_nodeInfo, "Unable to send request");
		return false; 
	}

	return true;
}


}}  // namespace raft{namespace tcp{

#include <sys/timeb.h>


int printfWithTime(const char* a_cpcFormat, ...)
{
	timeb	aCurrentTime;
	char* pcTimeline;
	int nRet;
	va_list aList;

	ftime(&aCurrentTime);
	pcTimeline = ctime(&(aCurrentTime.time));

	nRet = fprintf(stdout, "[%.19s.%.3hu %.4s] ", pcTimeline, aCurrentTime.millitm, &pcTimeline[20]);
	va_start(aList, a_cpcFormat);
	nRet += vfprintf(stdout, a_cpcFormat, aList);
	va_end(aList);
	return nRet;
}
