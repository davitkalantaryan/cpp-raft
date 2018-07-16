//
// file:		raft_tcp_common.cpp
// created on:	2018 May 11
//

#include "raft_tcp_common.hpp"
#include <memory.h>
#include "raft_macroses_and_functions.h"
#include <cpp11+/mutex_cpp11.hpp>
#include <utility>

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#define STRLEN_CALC_DIFF2	5


raft::tcp::NodeIdentifierKey2::NodeIdentifierKey2()
{
	generateKey2("",0, &m_portAndIpV4);
}


raft::tcp::NodeIdentifierKey2::NodeIdentifierKey2(NodeIdentifierKey2&& a_tmpKey)
	:
	m_portAndIpV4(a_tmpKey.m_portAndIpV4)
{
}


bool raft::tcp::NodeIdentifierKey2::isSameNode1(const char* a_otherKey, size_t a_unKeyLen)const
{
	return (a_unKeyLen==m_portAndIpV4.size()) && (memcmp(m_portAndIpV4.data(),a_otherKey,a_unKeyLen)==0);
}


#if 0
bool raft::tcp::NodeIdentifierKey2::isSameNode2(const std::string& a_ip4Address, int32_t a_port)const
{
	//const int32_t* pnPort =
	const char* pcData = m_portAndIpV4.data();
	const int32_t& nThisPort = *((const int32_t*)pcData);
	const size_t sizeStringThis(m_portAndIpV4.size() - STRLEN_CALC_DIFF);

	pcData += STRING_START_OFFSET;
	return ((nThisPort==a_port) && (sizeStringThis== a_ip4Address.length()) && (memcmp(pcData,a_ip4Address.c_str(),sizeStringThis)==0));
}
#endif


const std::string& raft::tcp::NodeIdentifierKey2::key()const
{
	return m_portAndIpV4;
}


const char* raft::tcp::NodeIdentifierKey2::ip4Address()const
{
	return hostNameFromKey(m_portAndIpV4.data());
}


int32_t raft::tcp::NodeIdentifierKey2::port()const
{
	return portFromKey(m_portAndIpV4.data());
}


char* raft::tcp::NodeIdentifierKey2::data()
{
	return const_cast<char*>(m_portAndIpV4.data());
}


const char* raft::tcp::NodeIdentifierKey2::data()const
{
	return m_portAndIpV4.data();
}


void raft::tcp::NodeIdentifierKey2::swapPort()
{
	SWAP4BYTES(*((int32_t*)const_cast<char*>(m_portAndIpV4.data())));
}


void raft::tcp::NodeIdentifierKey2::generateKey2(const std::string& a_ip4Address, int32_t a_port, std::string* a_pKey)
{
	char* pcBuffer;

	//a_pKey->resize(STRLEN_CALC_DIFF + a_ip4Address.length(),0);
	size_t unCopySize = a_ip4Address.length();
	unCopySize = (unCopySize>MAX_IP4_LEN2) ? MAX_IP4_LEN2 : unCopySize;
	a_pKey->resize(MAX_IP4_LEN2+4, 0);
	
	pcBuffer = const_cast<char*>(a_pKey->data());
	*((int32_t*)pcBuffer) = a_port;
	if(unCopySize){memcpy(pcBuffer + STRING_START_OFFSET, a_ip4Address.c_str(), unCopySize);}
}


raft::tcp::NodeIdentifierKey2& raft::tcp::NodeIdentifierKey2::operator=(NodeIdentifierKey2&& a_rightSide)
{
	this->m_portAndIpV4 = std::move(a_rightSide.m_portAndIpV4);
	return *this;
}


void raft::tcp::NodeIdentifierKey2::set_ip4addressAndPort(const std::string& a_ip4Address, int32_t a_port)
{
	generateKey2(a_ip4Address, a_port, &m_portAndIpV4);
}


const char* raft::tcp::NodeIdentifierKey2::hostNameFromKey(const void* a_key)
{
	return ((const char*)a_key) + STRING_START_OFFSET;
}


int32_t raft::tcp::NodeIdentifierKey2::portFromKey(const void* a_key)
{
	return *((const int32_t*)a_key);
}


namespace raft{namespace tcp{

const char g_ccResponceOk= response::ok;
int g_nLogLevel = 0;

bool ConnectAndGetEndian(common::SocketTCP* a_pSock, const NodeIdentifierKey2& a_nodeInfo,char a_cRequest, uint32_t* a_pIsEndianDiffer)
{
	int nSndRcv;
	uint16_t unRemEndian;

	if(a_pSock->connectC(a_nodeInfo.ip4Address(), a_nodeInfo.port(),500)){
		a_pSock->closeC();
		DEBUG_APP_WITH_KEY(2,a_nodeInfo.data(),"Unable to connect");
		return false;
	}
	a_pSock->setTimeout(SOCK_TIMEOUT_MS);

	nSndRcv = a_pSock->readC(&unRemEndian, 2);
	if(nSndRcv!=2){
		a_pSock->closeC();
		DEBUG_APP_WITH_KEY(2,a_nodeInfo.data(), "Unable to get endian. retCode=%d", nSndRcv);
		return false;
	}
	if(unRemEndian==1){*a_pIsEndianDiffer=0;}
	else {*a_pIsEndianDiffer = 1;}

	//cRequest = a_connectionCode;
	nSndRcv = a_pSock->writeC(&a_cRequest,1);
	if (nSndRcv != 1) { 
		a_pSock->closeC(); 
		DEBUG_APP_WITH_KEY(1,a_nodeInfo.data(), "Unable to send request");
		return false; 
	}

	return true;
}


bool ConnectAndGetEndian(common::SocketTCP* a_pSock, const char* a_nodeKey,char a_cRequest, uint32_t* a_pIsEndianDiffer)
{
	int nSndRcv;
	uint16_t unRemEndian;

	if(a_pSock->connectC(a_nodeInfo.ip4Address(), a_nodeInfo.port(),500)){
		a_pSock->closeC();
		DEBUG_APP_WITH_KEY(2,a_nodeInfo.data(),"Unable to connect");
		return false;
	}
	a_pSock->setTimeout(SOCK_TIMEOUT_MS);

	nSndRcv = a_pSock->readC(&unRemEndian, 2);
	if(nSndRcv!=2){
		a_pSock->closeC();
		DEBUG_APP_WITH_KEY(2,a_nodeInfo.data(), "Unable to get endian. retCode=%d", nSndRcv);
		return false;
	}
	if(unRemEndian==1){*a_pIsEndianDiffer=0;}
	else {*a_pIsEndianDiffer = 1;}

	//cRequest = a_connectionCode;
	nSndRcv = a_pSock->writeC(&a_cRequest,1);
	if (nSndRcv != 1) { 
		a_pSock->closeC(); 
		DEBUG_APP_WITH_KEY(1,a_nodeInfo.data(), "Unable to send request");
		return false; 
	}

	return true;
}


}}  // namespace raft{namespace tcp{

#include <sys/timeb.h>


int fprintfWithTime(FILE* a_fpFile, const char* a_cpcFormat, ...)
{
	static STDN::mutex    smutexForCtime;
	timeb	aCurrentTime;
	char* pcTimeline;
	int nRet;
	va_list aList;

	va_start(aList, a_cpcFormat);
	smutexForCtime.lock();   //
	ftime(&aCurrentTime);
	pcTimeline = ctime(&(aCurrentTime.time));
	nRet = fprintf(a_fpFile, "[%.19s.%.3hu %.4s] ", pcTimeline, aCurrentTime.millitm, &pcTimeline[20]);
	nRet += vfprintf(a_fpFile, a_cpcFormat, aList);
	smutexForCtime.unlock(); //
	va_end(aList);
	return nRet;
}
