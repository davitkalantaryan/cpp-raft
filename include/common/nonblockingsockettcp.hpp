
// common_sockettcp.hpp
// 2017 Jul 06

#ifndef __common_nonblockingsockettcp_hpp__
#define __common_nonblockingsockettcp_hpp__

#include "common_socketbase.hpp"


namespace common{

class NonblockingSocketTCP : public SocketBase
{
public:
	NonblockingSocketTCP() {}
	NonblockingSocketTCP(int a_sock) {m_socket=a_sock;}
	virtual ~NonblockingSocketTCP();

	virtual int		connectC(const char *svrName, int port, int connectionTimeoutMs = 1000);
	virtual int		readC(void* buffer, int bufferLen)const;
	virtual int		writeC(const void* buffer, int bufferLen);

protected:
	virtual common::IODevice* Clone()const;
};

}


#endif  // #ifndef __common_nonblockingsockettcp_hpp__
