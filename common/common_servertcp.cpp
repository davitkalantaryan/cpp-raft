//
//
//

#include "common/common_servertcp.hpp"

#include <signal.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#define gettidNew	GetCurrentThreadId
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <memory.h>
#include <sys/syscall.h>
#define gettidNew(...)	syscall(SYS_gettid)
static void SigActionFunction (int a_nSigNum, siginfo_t * , void *);
#endif


namespace WORK_STATUSES {enum{STOPPED=0,TRYING_TO_STOP,RUN,INITED2};}


common::ServerTCP::ServerTCP()
	:
	m_nWorkStatus(WORK_STATUSES::STOPPED),
	m_nServerThreadId(0)
{
}


common::ServerTCP::~ServerTCP()
{
	StopServer();
}


int common::ServerTCP::StartServerS(
	TypeAccept a_fpAddClient, void* a_owner,
    int a_nPort, bool a_bReuse, bool a_bLoopback, int a_lnTimeout, int* a_pnRetCode)
{
	int nError;
	int& nRetCode = a_pnRetCode ? *a_pnRetCode : nError;

	if(m_nWorkStatus != WORK_STATUSES::STOPPED){return -1;}

#ifndef _WIN32
    struct sigaction newAction;

    m_serverThread = pthread_self();

    newAction.sa_flags = SA_SIGINFO;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_restorer = NULL;
    newAction.sa_sigaction = SigActionFunction;

    sigaction(SIGPIPE,&newAction,NULL);
#endif

#ifdef _CD_VERSION__
	nRetCode = CreateServer( a_nPort, a_bReuse,true );
#else
	nRetCode = CreateServer( a_nPort,a_bReuse, a_bLoopback) ;
#endif

	if(nRetCode != 0 ){
		closeC();
		return nRetCode;
	}

	RunServer(a_lnTimeout,a_fpAddClient,a_owner);
	return 0;
}


int common::ServerTCP::InitServer(int a_nPort, bool a_bReuse, bool a_bLoopback)
{
    int nRetCode;

    if(m_nWorkStatus != WORK_STATUSES::STOPPED){return 0;}

#ifndef _WIN32
    struct sigaction newAction;

    m_serverThread = pthread_self();

    newAction.sa_flags = SA_SIGINFO;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_restorer = NULL;
    newAction.sa_sigaction = SigActionFunction;

    sigaction(SIGPIPE,&newAction,NULL);
#endif

#ifdef _CD_VERSION__
    nRetCode = CreateServer( a_nPort, a_bReuse,true );
#else
    nRetCode = CreateServer( a_nPort,a_bReuse, a_bLoopback) ;
#endif

    if(nRetCode != 0 ){
        closeC();
        return nRetCode;
    }

    m_nServerThreadId = static_cast<int>(gettidNew());
    m_nWorkStatus = WORK_STATUSES::INITED2;

    return 0;
}


int common::ServerTCP::WaitForConnection(int a_nTimeoutMs, sockaddr_in* a_pRemoteAddr)
{
    int nError, nClientSocket;

    if ((nError=ServerAccept(nClientSocket,a_nTimeoutMs,a_pRemoteAddr)) == 1){
        return nClientSocket;
    }

    if(nError<0){return nError;}
    return 0;
}



void common::ServerTCP::RunServer(int a_lnTimeout, TypeAccept a_fpAddClient, void* a_owner)
{
	sockaddr_in remoteAddress;
	SocketTCP aClientSocket;
	int nError, nClientSocket;

    m_nServerThreadId = static_cast<int>(gettidNew());
	m_nWorkStatus = WORK_STATUSES::RUN;

	while(m_nWorkStatus== WORK_STATUSES::RUN){
		if ((nError=ServerAccept(nClientSocket,a_lnTimeout,&remoteAddress)) == 1){
			aClientSocket.SetNewSocketDescriptor( nClientSocket );
			(*a_fpAddClient)(a_owner,aClientSocket,&remoteAddress);
			aClientSocket.closeC();
		}

		if(nError<0){break;}

#ifndef _CD_VERSION__
#endif
	}

	closeC();
	m_nWorkStatus = WORK_STATUSES::STOPPED;
	m_nServerThreadId = 0;
}



void common::ServerTCP::StopServer(void)
{
    int nCurrentThreadId(static_cast<int>(gettidNew()));
    if((m_nWorkStatus != WORK_STATUSES::RUN)||(m_nWorkStatus != WORK_STATUSES::INITED2)){return;}
	
	if(nCurrentThreadId!=m_nServerThreadId){
		m_nWorkStatus = WORK_STATUSES::TRYING_TO_STOP;
		closeC();
#ifndef _WIN32
        pthread_kill(m_serverThread,SIGPIPE);
#endif
        while((m_nWorkStatus != WORK_STATUSES::INITED2) && (m_nWorkStatus== WORK_STATUSES::TRYING_TO_STOP)){SWITCH_SCHEDULING(1);}
        m_nWorkStatus = WORK_STATUSES::STOPPED;
	}
	else {
		m_nWorkStatus = WORK_STATUSES::STOPPED;
		m_nServerThreadId = 0;
		closeC();
	}
}



/*
 * ServerAccept: server waiting for new connection
 * Parameter:
 *	a_ppClient:	accepted client socket
 * Return:
 *    < 0:	error
 *   	0:	timeout
 *	1:	ok
 */
int common::ServerTCP::ServerAccept(int& a_nClientSocket, int a_lnTimeout, sockaddr_in* a_bufForRemAddress)
{
	struct timeval*		pTimeout;
	struct timeval		aTimeout2;
	struct sockaddr_in addr;
	fd_set rfds;
	socklen_t addr_len;
	int maxsd = 0;
	int rtn = 0;

	FD_ZERO( &rfds );
	FD_SET( (unsigned int)m_socket, &rfds );
	
	maxsd = m_socket + 1;

	if( a_lnTimeout >= 0 ){
		aTimeout2.tv_sec = a_lnTimeout/1000L;
		aTimeout2.tv_usec = (a_lnTimeout%1000L)*1000L ;
		pTimeout = &aTimeout2;
	}else{pTimeout = NULL;}

	rtn = select(maxsd, &rfds, (fd_set *) 0, (fd_set *) 0, pTimeout);

	switch(rtn)
	{
		case 0:	/* time out */
			return 0;
		case SOCKET_ERROR_NEW:
			if( errno == EINTR ){/*interrupted by signal*/return 2;}
			return(E_SELECT);
		default:
			break;
	}

	if( !FD_ISSET( m_socket, &rfds ) ){return(E_FATAL);}

	addr_len = sizeof(addr);
	a_nClientSocket = (int)accept( m_socket, (struct sockaddr *)&addr, &addr_len);

	if(CHECK_FOR_SOCK_INVALID(a_nClientSocket) ){return 0;}

#ifdef MAKE_SOCKET_NONBLOCK
#ifdef	_WIN32
	{
		u_long non = 0;
		ioctlsocket( a_nClientSocket, FIONBIO, &non);
	}
#else  // #ifdef	_WIN32
	int status;
	if( (status = fcntl( a_nClientSocket, F_GETFL, 0 )) != -1)
	{
		status |= O_NONBLOCK;
		fcntl( a_nClientSocket, F_SETFL, status );
	}
#endif // #ifdef	_WIN32
#endif // #ifdef MAKE_SOCKET_NONBLOCK

	if (a_bufForRemAddress){
		//struct sockaddr_in* pIncAddr = (struct sockaddr_in*)a_pIncAddr;
		*a_bufForRemAddress = addr;
	}

	return 1;
}



int common::ServerTCP::CreateServer(int a_nPort, bool a_bReuse,bool a_bLoopback)
{
	struct sockaddr_in addr;
	int rtn = -1,addr_len;
	char l_host[MAX_HOSTNAME_LENGTH];

    m_socket = (int)socket( AF_INET, SOCK_STREAM, 0 );
	if (CHECK_FOR_SOCK_INVALID(m_socket)) { return(E_NO_SOCKET); }
	
	if(a_bReuse){int i(1);setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&i, sizeof(i));}

	if (gethostname(l_host, MAX_HOSTNAME_LENGTH) < 0){return E_UNKNOWN_HOST;}

	memset( (char *)&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family	= AF_INET;
	addr.sin_port = htons( a_nPort );

	addr.sin_addr.s_addr = htonl( (a_bLoopback ? INADDR_LOOPBACK : INADDR_ANY ) );
	//addr.sin_addr.s_addr = htonl((a_bLoopback ? INADDR_LOOPBACK : INADDR_ANY));

#ifdef MAKE_SOCKET_NONBLOCK
#ifdef	_WIN32
	{
		u_long non = 1;
		ioctlsocket( m_socket, FIONBIO, &non);
	}
#else  // #ifdef	_WIN32
	int status;
	if( (status = fcntl( m_socket, F_GETFL, 0 )) != -1)
	{
		status |= O_NONBLOCK;
		fcntl( m_socket, F_SETFL, status );
	}
#endif
#endif  // #ifdef MAKE_SOCKET_NONBLOCK

	addr_len = sizeof(addr);
	rtn = bind( m_socket, (struct sockaddr *) &addr, addr_len );
	if( CHECK_FOR_SOCK_ERROR(rtn) ){return(E_NO_BIND);}

	rtn = ::listen( m_socket, 64);
	if (CHECK_FOR_SOCK_ERROR(rtn)) { return(E_NO_LISTEN); }

	return 0;
}


#ifndef _WIN32
static void SigActionFunction (int, siginfo_t * , void *)
{
    //
}
#endif  // #ifndef _WIN32
