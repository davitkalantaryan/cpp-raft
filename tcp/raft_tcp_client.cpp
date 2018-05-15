//
// File:	raft_tcp_client.cpp
// Created on:	2018 May 15
//

#include "raft_tcp_client.hpp"
#include <stdint.h>
#include "raft_macroses_and_functions.h"
#include <stdlib.h>

raft::tcp::Client::Client()
{
}


raft::tcp::Client::~Client()
{
}


bool raft::tcp::Client::ReceiveAllNodes(const char* a_nodeIp, int a_port, std::vector<NodeIdentifierKey>* a_pNodes)
{
	NodeIdentifierKey* pInfo = NULL;
	common::SocketTCP aSocket;
	int i,isEndianDiffer(0), nNodesCount, nTotalSize,nSndRcv;
	uint16_t unRemEndian;
	bool bReturn(false);

	if(aSocket.connectC(a_nodeIp,a_port)){goto returnPoint;}
	aSocket.setTimeout(SOCK_TIMEOUT_MS);

	nSndRcv=aSocket.readC(&unRemEndian,2);
	if(nSndRcv!=2){goto returnPoint;}
	if(unRemEndian!=1){ isEndianDiffer =1;}

	nSndRcv=aSocket.readC(&nNodesCount, 4);
	if(nSndRcv!=4){goto returnPoint;}
	if (isEndianDiffer) {SWAP4BYTES(nNodesCount);}
	if(nNodesCount<1){goto returnPoint;}

	nTotalSize = nNodesCount * sizeof(NodeIdentifierKey);
	pInfo = (NodeIdentifierKey*)malloc(nTotalSize);
	if (!pInfo) { goto returnPoint; }

	nSndRcv = aSocket.readC(pInfo, nTotalSize);
	if (nSndRcv != nTotalSize) { goto returnPoint; }

	a_pNodes->resize(nNodesCount);

	for(i=0;i<nNodesCount;++i){
		if(isEndianDiffer){SWAP4BYTES(pInfo[i].port);}
		(*a_pNodes)[i] = pInfo[i];
	}

	bReturn = true;
returnPoint:
	free(pInfo);
	return bReturn;
}
