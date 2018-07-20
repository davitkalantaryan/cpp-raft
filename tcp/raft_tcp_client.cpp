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


int raft::tcp::Client::ReceiveAllNodes(const char* a_nodeIp, int a_port, std::vector<NodeIdentifierKey>* a_pNodes)
{
	struct {int nodesCount,leaderIndex;}nl;
	common::SocketTCP aSocket;
	NodeIdentifierKey aNodeKey;
	int i,nTotalSize,nSndRcv;
	uint32_t isEndianDiffer;
	int nReturn(-1);

	aNodeKey.set_ip4Address1(a_nodeIp);
	aNodeKey.port = a_port;

	try {  // we put into try block because std::vector::resize can throw exception
		if (!ConnectAndGetEndian(&aSocket, aNodeKey, raft::connect::fromClient2::allNodesInfo, &isEndianDiffer)) { goto returnPoint; }

		nSndRcv = aSocket.readC(&nl, 8);
		if (nSndRcv != 8) { goto returnPoint; }
		if (isEndianDiffer) { SWAP4BYTES(nl.nodesCount); }
		if (isEndianDiffer) { SWAP4BYTES(nl.leaderIndex); }
		if ((nl.nodesCount<1) || (nl.leaderIndex<0) || (nl.leaderIndex >= nl.nodesCount)) { goto returnPoint; }

		nTotalSize = nl.nodesCount * sizeof(NodeIdentifierKey);
		a_pNodes->resize(nl.nodesCount);

		nSndRcv = aSocket.readC(a_pNodes->data(), nTotalSize);
		if (nSndRcv != nTotalSize) { goto returnPoint; }

		if(isEndianDiffer){for(i=0;i<nl.nodesCount;++i){SWAP4BYTES((*a_pNodes)[i].port);}}
	}
	catch(...){
	}

	nReturn = nl.leaderIndex;

returnPoint:
	aSocket.closeC();
	return nReturn;
}
