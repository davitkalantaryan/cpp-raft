
// common_servertcp.hpp
// 2017 Jul 06

#ifndef __common_servertcp_hpp__
#define __common_servertcp_hpp__

#include "common/common_sockettcp.hpp"
#include "cpp11+/common_defination.h"
#ifndef _WIN32
#include <pthread.h>
#endif

struct sockaddr_in;
typedef void(__THISCALL__*TypeAccept)(void* owner,common::SocketTCP& clientSock,const sockaddr_in* remoteAddr);

namespace common{

class ServerTCP : public SocketBase
{
public:
	ServerTCP();
	virtual ~ServerTCP();

	template <typename Type>
	int	StartServer(
		Type* owner, 
		void(Type::*fpAddClient)(common::SocketTCP& clientSock,const sockaddr_in*remoteAddr),
		int a_nPort, int* retCode=NULL, long int a_lnTimeout = 1000,
        bool bReuse = true, bool a_bLoopback = false);

	int	StartServerS(
		TypeAccept fpAddClient,void* owner, 
		int a_nPort, int* retCode=NULL, long int a_lnTimeout = 1000,
        bool bReuse = true, bool a_bLoopback = false);
	
	void StopServer(void);

private:
	//virtual void AddClient(common::SocketTCP& clientSocket, const sockaddr_in* remoteAddress)= 0;

protected:
	/*
	 * ServerAccept: server waiting for new connection
	 * Parameter:
	 *	a_ppClient:	accepted client socket
	 * Return:
	 *    < 0:	error
	 *   	0:	timeout
	 *	1:	ok
	 */
	int	ServerAccept(int& a_nClientSocket, int a_lnTimeout, sockaddr_in* remAddress);

	/*
	 * CreateServer: create server
	 * Parameter:
	 *	port: 	port number
	 * Return:
	 * 	0:	ok
	 *    < 0:	error
	 */
	int	 CreateServer(int a_nPort, bool bReuse,bool a_bLoopback);
	void RunServer(int a_lnTimeout,TypeAccept addClientFnc, void* owner);

private:
	// closed functions
	int		readC(void*, int)const { return 0; }
	int		writeC(const void*, int) { return 0; }

protected:
	volatile int	m_nWorkStatus;
	int				m_nServerThreadId;
#ifndef _WIN32
    pthread_t       m_serverThread;
#endif
};

}

#include "impl.common_servertcp.hpp"



#endif // #ifndef __common_servertcp_hpp__
