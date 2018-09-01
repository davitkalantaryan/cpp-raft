
#include <iostream>
#include <string.h>
#include <common/common_sockettcp.hpp>
#include "tcp_server_client_common.h"

#define CLIENT_VERSION  "test client version 1.0.1"


int main( int argc, char* argv[])
{
    int nReturn;
    ::common::SocketTCP aSocket;

    if(argc<2){
        ::std::cerr<< "host to connect is not provided" << ::std::endl;
        return 1;
    }

    ::common::socketN::Initialize();
    ::std::cout<< CLIENT_VERSION << ::std::endl;

    nReturn = aSocket.connectC(argv[1],TCP_SERVER_CLIENT_TEST,2000);
    if(nReturn){goto returnPoint; }

    aSocket.writeC(CLIENT_VERSION,strlen(CLIENT_VERSION));

returnPoint:
    aSocket.closeC();
    ::common::socketN::Cleanup();

    return 0;
}
