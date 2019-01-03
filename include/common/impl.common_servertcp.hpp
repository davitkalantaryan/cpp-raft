
// common_servertcp.impl.hpp
// 2017 Aug 02

#ifndef __impl_common_servertcp_hpp__
#define __impl_common_servertcp_hpp__

#ifndef __common_servertcp_hpp__
#error do not include this file directly
#include "common_servertcp.hpp"
#endif


template <typename Type>
int common::ServerTCP::StartServer(
	Type* a_owner,
	void(Type::*a_fpAddClient)(common::SocketTCP& clientSock, const sockaddr_in*remoteAddr),
        int a_nPort, bool a_bReuse, bool a_bLoopback, int a_lnTimeout, int* a_pnRetCode)
{
	return StartServerS(
		//(TypeAccept)GetFuncPointer_common(1, a_fpAddClient), 
		*(reinterpret_cast<TypeAccept*>(&a_fpAddClient)),
		(void*)a_owner,
                a_nPort, a_bReuse, a_bLoopback,a_lnTimeout,a_pnRetCode);
}


#endif  // #ifndef __impl_common_servertcp_hpp__
