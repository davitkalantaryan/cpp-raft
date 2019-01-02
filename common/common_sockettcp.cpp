
// common_sockettcp.cpp
// 2017 Jul 06

#include "common/common_sockettcp.hpp"
#define DO_NOT_CALL_SELECT	-2003

#ifdef _WIN32
#include <WinSock2.h>
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


#ifdef _MSC_VER
#if(_MSC_VER >= 1400)
//#define		_WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable : 4996)
#endif
#endif


common::SocketTCP::~SocketTCP()
{
}


int common::SocketTCP::connectC(const char *a_svrName, int a_port, int a_connectionTimeoutMs)
{	
	const char *host = NULL;
	fd_set rfds;
	struct sockaddr_in addr;
	unsigned long ha;
	int rtn = -1;
	int maxsd = 0;
	int addr_len;
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

	addr_len = sizeof(addr);
	rtn = ::connect(m_socket, (struct sockaddr *) &addr, addr_len);

	if (rtn != 0){
        struct timeval  aTimeout2;
        struct timeval* pTimeout;
		int nErrno = errno;///?
		if (!SOCKET_INPROGRESS(nErrno)){return E_NO_CONNECT;}

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

	return 0;
}


int common::SocketTCP::readC(void* a_pBuffer, int a_nSize)const
{
	return recv(m_socket, (char*)a_pBuffer, a_nSize, MSG_WAITALL);
}


int common::SocketTCP::writeC(const void* a_cpBuffer, int a_nSize)
{
#define MAX_NUMBER_OF_ITERS	100000
	const char* pcBuffer = (const char*)a_cpBuffer;
	const char *cp = NULL;
	int len_to_write = 0;
	int len_wrote = 0;
	int n = 0;

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


common::IODevice* common::SocketTCP::Clone()const
{
	return new SocketTCP(*this);
}
