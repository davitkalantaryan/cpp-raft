
// common_sockettcp.hpp
// 2017 Jul 06

#ifndef __common_sockettcp_hpp__
#define __common_sockettcp_hpp__

#include <common_socketbase.hpp>

namespace common{

class SocketTCP : public SocketBase
{
public:
	SocketTCP() {}
	SocketTCP(int a_sock) {m_socket=a_sock;}
	virtual ~SocketTCP();

	virtual int		connectC(const char *svrName, int port, int connectionTimeoutMs = 1000);
	virtual int		readC(void* buffer, int bufferLen)const;
	virtual int		writeC(const void* buffer, int bufferLen);

protected:
	virtual common::IODevice* Clone()const;
};

}


#endif  // #ifndef __common_sockettcp_hpp__
