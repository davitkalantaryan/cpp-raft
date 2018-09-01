
#include <iostream>
#include <string.h>
#include <common/common_sockettcp.hpp>

#define CLIENT_VERSION  "test client version 1.0.0"


int main()
{
    int nReturn;
    ::common::SocketTCP aSocket;

    ::common::socketN::Initialize();
    ::std::cout<< CLIENT_VERSION << ::std::endl;

    nReturn = aSocket.connectC("kalantar.pagekite.me",9030,2000);
    if(nReturn){goto returnPoint; }

    aSocket.writeC(CLIENT_VERSION,strlen(CLIENT_VERSION));

returnPoint:
    aSocket.closeC();
    ::common::socketN::Cleanup();

    return 0;
}
