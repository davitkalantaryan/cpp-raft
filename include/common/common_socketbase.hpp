
// common_socketbase.hpp
// 2017 Jul 06

#ifndef __common_socketbase_hpp__
#define __common_socketbase_hpp__

#if defined(_MSC_VER) & (_MSC_VER>=1913)
#ifndef CINTERFACE
#define CINTERFACE
#endif
#endif

#if !defined(E_FATAL) & !defined(E_NO_BIND)

#include <errno.h>

#define E_FATAL				-1	/* fatal error */
#define E_NO_BIND			-7	/* can not bind address to port */
#define E_NO_CONNECT		-8	/* can not connect to server */
#define E_NO_LISTEN			-14	/* can not listen */
#define E_NO_SOCKET			-18	/* no socket generated */
#define E_RECEIVE			-25	/* error by receive */
#define E_SELECT			-28	/* error by select */
#define E_SEND				-29	/* error by send */
#define E_UNKNOWN_HOST		-38	/* can not find host */
#define E_WINSOCK_VERSION	-42	/* WINSOCK DLL version not requested */
#define	E_CONN_CLOSED		-46	/* connection closed by peer */

#ifdef	_WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#ifndef SOCKET_INPROGRESS
#define	SOCKET_INPROGRESS(e)	(WSAGetLastError() == WSAEWOULDBLOCK)
#define SleepIntr(_x)           SleepEx((_x),TRUE)
#endif
#else
#include <netinet/in.h>
#include <unistd.h>
#if defined(EALREADY) && defined(EAGAIN)
#define	SOCKET_INPROGRESS(e)	(e == EINPROGRESS || e == EALREADY || e == EAGAIN)
#else
#ifdef  EALREADY
#define	SOCKET_INPROGRESS(e)	(e == EINPROGRESS || e == EALREADY)
#else
#ifdef  EAGAIN
#define	SOCKET_INPROGRESS(e)	(e == EINPROGRESS || e == EAGAIN)
#else
#define	SOCKET_INPROGRESS(e)	(e == EINPROGRESS)
#endif
#endif
#endif
#define SleepIntr(_x)           usleep(1000*(_x))
#endif

#ifndef INADDR_NONE
#define INADDR_NONE     0xffffffff
#endif

#ifdef	_WIN32
#define	SOCKET_WOULDBLOCK(e)	(WSAGetLastError() == WSAEWOULDBLOCK)
#else
#if defined(EAGAIN) && defined(EWOULDBLOCK)
#define	SOCKET_WOULDBLOCK(e)	(e == EAGAIN || e == EWOULDBLOCK)
#else
#ifdef	EWOULDBLOCK
#define	SOCKET_WOULDBLOCK(e)	(e == EWOULDBLOCK)
#else
#define	SOCKET_WOULDBLOCK(e)	(e == EAGAIN)
#endif
#endif
#endif

#ifdef _WIN32
#else
#define WSAGetLastError		errno
#define WSAETIMEDOUT		EAGAIN 
#define WSAEWOULDBLOCK		EWOULDBLOCK
#endif

#ifdef	_WIN32
#define CHECK_FOR_SOCK_INVALID(_a_socket_)	((_a_socket_) == INVALID_SOCKET)
#define	SOCKET_ERROR_NEW	SOCKET_ERROR
#define CHECK_FOR_SOCK_ERROR(_a_return_)	((_a_return_) == SOCKET_ERROR_NEW)
#define SWITCH_SCHEDULING(_t_)	Sleep((_t_))
#else
#define CHECK_FOR_SOCK_INVALID(_a_socket_)	((_a_socket_) < 0)
#define	SOCKET_ERROR_NEW	-1
#define CHECK_FOR_SOCK_ERROR(_a_return_)	((_a_return_) < 0)
#define SWITCH_SCHEDULING(_t_)	usleep((_t_))
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif
#endif

#define	_SOCKET_TIMEOUT_		-2001
#define	_NEGATIVE_ERROR_(x)		(((x)>0) ? -(x) : (x))
#define	_EINTR_ERROR_			_NEGATIVE_ERROR_(EINTR)


#define MAX_HOSTNAME_LENGTH     64

#define READ1_BIT	0
#define	WRITE_BIT	1
#define	ERROR_BIT	2

#endif/* #if !defined(E_FATAL) & !defined(E_NO_BIND) */

#define GET_BIT(_variable_,_bitNum_) (  (1<<(_bitNum_)) & (_variable_)  )
#define SET_BIT(_variablePtr_,_bitNum_,_bitVal_) \
	*(_variablePtr_)=(  ((~(1<<(_bitNum_)))&(*(_variablePtr_))) | ((_bitVal_)<<(_bitNum_))   )

#if defined(__cplusplus) && !defined(only_socket_macroses_are_used)

#include "common_iodevice.hpp"
#include <vector>

#define sockaddr_in_using_ok
struct sockaddr_in;

namespace common{

class SocketBase : public IODevice
{
public:
	SocketBase();
	virtual ~SocketBase();

	virtual bool isOpenC(void)const;
	virtual void closeC(void);
	virtual int  setTimeout(int timeoutMs);
	
	common::SocketBase& operator=(const common::SocketBase& a_nSocket);

	operator const int&()const;
	operator int&();
	void SetNewSocketDescriptor(int rawSocket);
	void ResetSocketWithoutClose();
	int GetAndResetSocket();
	int DublicateSocket(int procID, void* protInfo)const;

protected:
	virtual void closeHard(void);

protected:
	int	   m_socket;
};

namespace socketN {
#ifdef _SELECT_NEEDED_
	struct STypeAndIndex { int index; char whichd; char reserver[3]; };
	template <typename TypeCntRd, typename TypeCntWr = TypeCntRd, typename TypeCntEr = TypeCntWr,
		typename TypeCntRet = std::vector<STypeAndIndex>>
		int	selectMltCpp3(
			int timeoutMs, TypeCntRet* aReturns,
			const TypeCntRd* rdfd, int* socketForStop = NULL,
			const TypeCntWr* wrfd = NULL, const TypeCntEr* erfd = NULL);
#endif

#ifdef sockaddr_in_using_ok
	const char*	GetIPAddress(const sockaddr_in* addr);
	const char*	GetHostName(const sockaddr_in* addr, char* buffer, int buffLen);
	int  GetPort(const sockaddr_in* a_addr);
	int  Initialize();
	void Cleanup();
	int SleepN(long long int nanoSec);
    const char* GetOwnIp4Address(char* pcBuffer, int bufferLength);
	const char* GetIp4AddressFromHostName(const char* hostName);
#endif  // #ifdef sockaddr_in_using_ok
}

}

#include "impl.common_socketbase.hpp"

#endif // #if defined(__cplusplus) && !defined(only_socket_macroses_are_used)

#endif  // #ifndef __common_socketbase_hpp__
