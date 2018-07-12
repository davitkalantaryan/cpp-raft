
// common_sockettcp.cpp
// 2017 Jul 06

#include "common/nonblockingsockettcp.hpp"
#define DO_NOT_CALL_SELECT	-2003

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
typedef u_long  red_buf_len;
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <memory.h>
#include <sys/ioctl.h>
#define ioctlsocket ioctl
typedef int  red_buf_len;
#endif


common::NonblockingSocketTCP::~NonblockingSocketTCP()
{
}

#ifdef _MSC_VER
#if(_MSC_VER >= 1400)
//#define		_WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable : 4996)
#endif
#endif

int common::NonblockingSocketTCP::connectC(const char *a_svrName, int a_port, int a_connectionTimeoutMs)
{	
	const char *host = NULL;
//#ifdef MAKE_SOCKET_NONBLOCK
	fd_set rfds;
	int maxsd = 0;
//#endif
	struct sockaddr_in addr;
	unsigned long ha;
	int rtn = -1;
	char l_host[MAX_HOSTNAME_LENGTH];

	closeHard();

	m_socket = (int)::socket(AF_INET, SOCK_STREAM, 0);
	if (CHECK_FOR_SOCK_INVALID(m_socket)){
		m_socket = -1;
		return E_NO_SOCKET;
	}	

	host = a_svrName;
	if (host == NULL || *host == '\0'){
		if (::gethostname(l_host, MAX_HOSTNAME_LENGTH) < 0){return E_UNKNOWN_HOST;}
		host = l_host;
	}

	memset((char *)&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(a_port);

	if ((ha = inet_addr(host)) == INADDR_NONE){
		struct hostent* hostent_ptr = gethostbyname(a_svrName);
		if (!hostent_ptr){return E_UNKNOWN_HOST;}
		a_svrName = inet_ntoa(*(struct in_addr *)hostent_ptr->h_addr_list[0]);
		if ((ha = inet_addr(a_svrName)) == INADDR_NONE){return E_UNKNOWN_HOST;}
	}

	memcpy((char *)&addr.sin_addr, (char *)&ha, sizeof(ha));

//#ifdef MAKE_SOCKET_NONBLOCK
#ifdef	_WIN32
	unsigned long on = 1;
	ioctlsocket(m_socket, FIONBIO, &on);
#else  /* #ifdef	_WIN32 */
	int status;
	if ((status = fcntl(m_socket, F_GETFL, 0)) != -1){
		status |= O_NONBLOCK;
		fcntl(m_socket, F_SETFL, status);
	}
#endif  /* #ifdef	_WIN32 */
//#else // #ifdef MAKE_SOCKET_NONBLOCK
//	this->setTimeout(a_connectionTimeoutMs);
//#endif  // #ifdef MAKE_SOCKET_NONBLOCK

	int addr_len = sizeof(addr);
	rtn = ::connect(m_socket, (struct sockaddr *) &addr, addr_len);

//#ifdef MAKE_SOCKET_NONBLOCK
	if (rtn != 0){
        struct timeval  aTimeout2;
        struct timeval* pTimeout;
		int nErrno2 = errno;///?
		if (!SOCKET_INPROGRESS(nErrno2)){return E_NO_CONNECT;}

        FD_ZERO(&rfds);
        FD_SET((unsigned int)m_socket, &rfds);
        maxsd = (int)(m_socket + 1);

        if (a_connectionTimeoutMs >= 0){
            aTimeout2.tv_sec = a_connectionTimeoutMs / 1000L;
            aTimeout2.tv_usec = (a_connectionTimeoutMs % 1000L) * 1000L;
            pTimeout = &aTimeout2;
        }else{pTimeout = NULL;}

        rtn = ::select(maxsd, (fd_set *)0, &rfds, (fd_set *)0, pTimeout);

        switch (rtn)
        {
        case 0:	/* time out */
            return _SOCKET_TIMEOUT_;
        case SOCKET_ERROR:
            if (errno == EINTR){/*interrupted by signal*/return _EINTR_ERROR_;}
            return E_SELECT;
        default:
			rtn = 0;
            break;
        }

        if (!FD_ISSET(m_socket, &rfds)){return E_FATAL;}
	}
//#endif  // #ifdef MAKE_SOCKET_NONBLOCK

#ifndef MAKE_SOCKET_NONBLOCK
#ifdef	_WIN32
	on = 0;
	ioctlsocket(m_socket, FIONBIO, &on);
#else  /* #ifdef	_WIN32 */
	if ((status = fcntl(m_socket, F_GETFL, 0)) != -1) {
		status &= ~O_NONBLOCK;
		fcntl(m_socket, F_SETFL, status);
	}
#endif  /* #ifdef	_WIN32 */
	this->setTimeout(a_connectionTimeoutMs);
#endif  // #ifdef MAKE_SOCKET_NONBLOCK

	return rtn;
}


#ifndef MSEC
#include <sys/timeb.h>
#define MSEC(finish, start)	( (long)( (finish).millitm - (start).millitm ) + \
							(long)( (finish).time - (start).time ) * 1000 )
#endif

#include <stdio.h>


int common::NonblockingSocketTCP::readC(void* a_pBuffer, int a_nSize)const
{
#if 1
	int nReturn = recv(m_socket, (char*)a_pBuffer, a_nSize, MSG_WAITALL);

	if(nReturn<0){
#ifdef _WIN32
		int nError = WSAGetLastError();
#else
		int nError = errno;
#endif
		nReturn = (nError>0)?(-nError):(   (nError<0)?nError:nReturn  );
	}

#if 0
	if(nReturn<0){
		int nError = WSAGetLastError();
		switch (nError)
		{
		case WSAETIMEDOUT:case WSAEWOULDBLOCK: nReturn = _SOCKET_TIMEOUT_;
		default: break;
		}
	}
#endif // better to do independent function, to decide timeout

	return nReturn;
#else
	struct timeval*	pTimeout;
	struct timeval	aTimeout2;
	fd_set rfds;
	red_buf_len	unDataAvlb;
	int maxsd;
	int nSelectReturn;

	FD_ZERO(&rfds);
	FD_SET((unsigned int)m_socket, &rfds);
	maxsd = m_socket + 1;

	if (a_lnSelectTm >= 0){
		aTimeout2.tv_sec = a_lnSelectTm / 1000;
		aTimeout2.tv_usec = (a_lnSelectTm % 1000) * 1000;
		pTimeout = &aTimeout2;
	}else{pTimeout = NULL;}

	nSelectReturn = ::select(maxsd, &rfds, (fd_set *)0, (fd_set *)0, pTimeout);
	switch (nSelectReturn)
	{
	case 0:	/* time out */
		return _SOCKET_TIMEOUT_;
	case SOCKET_ERROR:
		if (errno == EINTR){/*interrupted by signal*/return _EINTR_ERROR_;}
		return E_SELECT;
	default:
		break;
	}
	if (!FD_ISSET(m_socket, &rfds)){return E_FATAL;}

	ioctlsocket(m_socket, FIONREAD, &unDataAvlb);
	a_nSize=a_nSize>((int)unDataAvlb)?((int)unDataAvlb):a_nSize;

	nSelectReturn=Read2(a_pBuffer,a_nSize,DO_NOT_CALL_SELECT,-1);
	return nSelectReturn < 0 ? nSelectReturn : (int)unDataAvlb;
#endif
}


#define MAX_NUMBER_OF_ITERS	100000

int common::NonblockingSocketTCP::writeC(const void* a_cpBuffer, int a_nSize)
{
	const char* pcBuffer = (const char*)a_cpBuffer;
	const char *cp = NULL;
	int len_to_write = 0;
	int len_wrote = 0;
	int n = 0;

#ifdef DEBUG_SOCKET_FUNCTIONS
	static int nRectIter = 0;
	printf("%.2d -> fl:\"%s\",ln:%d,fnc:\"%s(%d)\" => %.2d\n",
		++s_nRecvAndSend, __MY_FILE__, __LINE__, __FUNCTION__, a_nSize, ++nRectIter);
#endif // #ifdef DEBUG_SOCKET_FUNCTIONS
	//if (a_nSize <= 0){return 0;}

	len_to_write = a_nSize;
	cp = pcBuffer;
	for (int i(0);len_to_write > 0;++i)
	{
		n = ::send(m_socket, cp, len_to_write, 0);
		if (CHECK_FOR_SOCK_ERROR(n)){
			if (SOCKET_WOULDBLOCK(errno)){
				if(i<MAX_NUMBER_OF_ITERS){SWITCH_SCHEDULING(0);continue;}
				else{return _SOCKET_TIMEOUT_;}
			}else{return E_SEND;}
		}
		else{
			cp += n;
			len_to_write -= n;
			len_wrote += n;
		}
	}

	return len_wrote;
}


common::IODevice* common::NonblockingSocketTCP::Clone()const
{
	return new NonblockingSocketTCP(*this);
}
