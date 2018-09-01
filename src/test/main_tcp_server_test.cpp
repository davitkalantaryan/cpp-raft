
#include <iostream>
#include <string.h>
#include <common/common_servertcp.hpp>


class ServerClass
{
public:
    void AddClient(common::SocketTCP& clientSock,const sockaddr_in*remoteAddr);
};


int main()
{
    ::common::ServerTCP aServer;
    ServerClass aServCls;

    ::common::socketN::Initialize();
    ::std::cout<<"test server version 1.0.0\n";

    aServer.StartServer(&aServCls,&ServerClass::AddClient,9030);

    ::common::socketN::Cleanup();

    return 0;
}



void ServerClass::AddClient(common::SocketTCP& a_clientSock,const sockaddr_in* a_remoteAddr)
{
    char vcHostAndRcvNameBuffer[256];

    ::std::cout
           <<"hostName:"<< ::common::socketN::GetHostName(a_remoteAddr,vcHostAndRcvNameBuffer,255)
           <<"(ip:"<< ::common::socketN::GetIPAddress(a_remoteAddr)<<")\n";

    a_clientSock.setTimeout(1000);
    memset(vcHostAndRcvNameBuffer,0,sizeof(vcHostAndRcvNameBuffer));
    a_clientSock.readC(vcHostAndRcvNameBuffer,255);
    ::std::cout<<"remBuffer: "<< vcHostAndRcvNameBuffer << ::std::endl;
    a_clientSock.closeC();
}
