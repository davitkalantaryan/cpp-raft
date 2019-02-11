
// common_socketbase.cpp
// 2017 Jul 06

#include "common/common_socketbase.hpp"

#include <string.h>
#include <time.h>
#include <signal.h>

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <memory.h>
#include <ifaddrs.h>
#endif

#ifdef _WIN32
#ifndef __SOCET_LIB_LOADED
#pragma comment(lib, "Ws2_32.lib")
#define __SOCET_LIB_LOADED
#endif //__SOCET_LIB_LOADED

#else
#endif


common::SocketBase::SocketBase()
	:
	m_socket(-1)
{
}


common::SocketBase::~SocketBase()
{
	closeC();
}


void common::SocketBase::closeC(void)
{
	IODevice::closeC();
	m_socket = -1;
}


bool common::SocketBase::isOpenC(void)const
{
	return (m_socket >= 0);
}


void common::SocketBase::closeHard(void)
{
	if (m_socket > 0)
#ifdef	_WIN32
		closesocket(m_socket);
#else
		close(m_socket);
#endif
	m_socket = -1;
}


int common::SocketBase::setTimeout(int a_nTimeoutMs)
{

	char* pInput;
	int nInputLen;
#ifdef _WIN32
	DWORD dwTimeout;
	if (a_nTimeoutMs >= 0) {dwTimeout = a_nTimeoutMs;}
	//else {dwTimeout = 1000000000;}
	else {dwTimeout=2147483647;}
	pInput = (char*)&dwTimeout;
	nInputLen = sizeof(DWORD);
#else
	struct timeval tv;
	if (a_nTimeoutMs >= 0) {
		tv.tv_sec = a_nTimeoutMs / 1000;
		tv.tv_usec = (a_nTimeoutMs % 1000) * 1000;
		pInput = (char*)&tv;
		nInputLen = sizeof(struct timeval);
	}
	else {
		//pInput = NULL;
		//nInputLen = 0;

		tv.tv_sec = 21474836;
		tv.tv_usec = 1000;
		pInput = (char*)&tv;
		nInputLen = sizeof(struct timeval);
	}
#endif

	if (setsockopt(m_socket,SOL_SOCKET,SO_RCVTIMEO,pInput,nInputLen) < 0){return -1;}

	return 0;
}


common::SocketBase& common::SocketBase::operator=(const common::SocketBase& a_aM)
{
	IODevice::operator=(a_aM);
	m_socket = a_aM.m_socket;
	return *this;
}


common::SocketBase::operator const int&()const
{
	return m_socket;
}


common::SocketBase::operator int&()
{
	return m_socket;
}


void common::SocketBase::SetNewSocketDescriptor(int a_rawSocket)
{
	closeC();
	m_socket = a_rawSocket;
}


void common::SocketBase::ResetSocketWithoutClose()
{
	m_pPrev = m_pNext = NULL;  // or this is not needed
	m_socket = -1;
}


int common::SocketBase::GetAndResetSocket()
{
	int nRet(m_socket);
	m_pPrev = m_pNext = NULL;
	m_socket = -1;
	return nRet;
}


int common::SocketBase::DublicateSocket(int a_nProcID, void* a_pProtInfo)const
{
#ifdef _WIN32
	WSAPROTOCOL_INFOW* pProtInfo = (WSAPROTOCOL_INFOW*)a_pProtInfo;
	return WSADuplicateSocketW(m_socket, (DWORD)a_nProcID, pProtInfo);
#else
	throw "For berkley sockets not implemented!";
	return (a_nProcID == (int)((size_t)a_pProtInfo)) ? 1 : 0;
#endif
}

namespace common{ namespace socketN{

static void SignalHandler(int)
{
    //
}

int Initialize()
{
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		return 0;
	}

	/* Confirm that the WinSock DLL supports 2.2.		*/
	/* Note that if the DLL supports versions greater	*/
	/* than 2.2 in addition to 2.2, it will still return*/
	/* 2.2 in wVersion since that is the version we		*/
	/* requested.										*/

	if (LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		return 0;
	}

#else    // #ifdef _WIN32

	struct sigaction newAction;

	newAction.sa_flags = 0;
	newAction.sa_handler =SignalHandler;
	sigemptyset(&newAction.sa_mask);
	newAction.sa_restorer = NULL;
	sigaction(SIGPIPE,&newAction,NULL);

#endif   // #ifdef _WIN32

	return 1;
}

/*
 * Cleanup socket library
 */
void Cleanup()
{
#ifdef _WIN32
	WSACleanup();
#endif
}


int SleepN(long long int a_nanoSec)
{
#ifdef _WIN32
	struct timeval tv;
	long int lnNs;
	int maxsd, fh1;
	fd_set rfds;

	fh1 = (int)::socket(AF_INET, SOCK_STREAM, 0);

	//fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET((unsigned int)fh1, &rfds);
	maxsd = fh1 + 1;

	//struct timeval tv;
	lnNs = (a_nanoSec % 1000000000L);
	tv.tv_sec = (long)(a_nanoSec / 1000000000L);
	tv.tv_usec = (lnNs%1000)?(lnNs/1000)+1: (lnNs / 1000);

	return (long int)(1000*::select(maxsd, &rfds, 0, 0, &tv));
#else
	struct timespec waitspec;
        waitspec.tv_sec = a_nanoSec / 1000000000;
        waitspec.tv_nsec = a_nanoSec % 1000000000;
	return (long)nanosleep(&waitspec, NULL);
#endif
}

#ifdef sockaddr_in_using_ok

#ifdef _MSC_VER
#if(_MSC_VER >= 1400)
//#define		_WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable : 4996)
#endif
#endif


const char* GetIPAddress(
	const sockaddr_in* a_addr)
{
	if (!a_addr){return "NULL";}
	return ::inet_ntoa(a_addr->sin_addr);
}


#ifdef _WIN32
//#include <Ws2tcpip.h>
//#include <Wspiapi.h>
//#include <ws2def>
#ifndef NI_NUMERICSERV
#define NI_NUMERICSERV  0x08  /* Return numeric form of the service (port #) */
#endif
typedef int  (WSAAPI* getnameinfo_type) (const struct sockaddr*,
	socklen_t, char*, DWORD,
	char*, DWORD, int);
#endif


const char* GetHostName(
	const sockaddr_in* a_addr, 
	char* a_pcBuffer, int a_nBuffLen)
{
	if (!a_addr) { return "NULL"; }
#ifndef _WIN32
	getnameinfo((struct sockaddr *)a_addr, sizeof(sockaddr), a_pcBuffer, a_nBuffLen, NULL, 0, NI_NUMERICSERV);
#else
	HMODULE h = ::GetModuleHandle(TEXT("ws2_32.dll"));
	getnameinfo_type getnameinfo_ptr = (getnameinfo_type)::GetProcAddress(h, "getnameinfo");
	if (getnameinfo_ptr) {
		(*getnameinfo_ptr)((sockaddr *)a_addr,sizeof(sockaddr),a_pcBuffer,a_nBuffLen,NULL,0,NI_NUMERICSERV);
	}else{return "NULL";}
#endif
	return a_pcBuffer;
}



int GetPort(const sockaddr_in* a_addr)
{
	if (!a_addr){return 0;}
	return (int)ntohs(a_addr->sin_port);
}


const char* GetOwnIp4Address(char* a_pcBuffer, int a_bufferLength)
{

	// for time being
	memset(a_pcBuffer, 0, a_bufferLength);

#ifdef _WIN32
	
	const char* cpcOwnIp4Address;
	struct hostent* hostent_ptr;
	char vcHostname[MAX_HOSTNAME_LENGTH];

	if (::gethostname(vcHostname, MAX_HOSTNAME_LENGTH)<0) { return " "; }

	hostent_ptr = gethostbyname(vcHostname);
	if (!hostent_ptr) { return " "; }
	cpcOwnIp4Address = inet_ntoa(*(struct in_addr *)hostent_ptr->h_addr_list[0]);
	strncpy(a_pcBuffer, cpcOwnIp4Address, a_bufferLength);

#else  // #ifdef _WIN32

	// https://stackoverflow.com/questions/4130147/get-local-ip-address-in-c-linux
    
	struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    a_pcBuffer[0]=0;
    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa ->ifa_addr->sa_family==AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, a_pcBuffer, a_bufferLength);

            if((ifa->ifa_name[0]=='l') && (ifa->ifa_name[1]=='o') && (ifa->ifa_name[2]==0)){continue;}

            //printf("'%s': %s\n", ifa->ifa_name, addressBuffer);
            break;
         } else if (ifa->ifa_addr->sa_family==AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            //tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            //char addressBuffer[INET6_ADDRSTRLEN];
            //inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            //printf("'%s': %s\n", ifa->ifa_name, addressBuffer);
        }
    }

#endif // #ifdef _WIN32

	return a_pcBuffer;

}


const char* GetIp4AddressFromHostName(const char* a_hostName)
{
	const char* cpcIp4Address;
	struct hostent* hostent_ptr = gethostbyname(a_hostName);
	
	if (!hostent_ptr) { return NULL; }
	cpcIp4Address = inet_ntoa(*(struct in_addr *)hostent_ptr->h_addr_list[0]);

	return cpcIp4Address;
}

#endif  // #ifdef sockaddr_in_using_ok

}} // namespace common{ namespace socketN{
