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


void raft::tcp::Client::SetNodeDetails(const char* a_hostNameOrIp4Address, int a_port)
{
	if (a_hostNameOrIp4Address) { m_nodeId.set_ip4Address1(a_hostNameOrIp4Address); }
	if (a_port>0) { m_nodeId.port = a_port; }
	else { m_nodeId.port = 9030; }
}


int raft::tcp::Client::pingToNode( ::std::string* a_pPingResult )
{
	::common::SocketTCP aSocket;
	uint32_t isEndianDiffer;
	int nReturn(-1);

	try {  // we put into try block because std::vector::resize can throw exception
		if (!ConnectAndGetEndian2(&aSocket, m_nodeId, raft::connect::fromClient2::ping, &isEndianDiffer)) { goto returnPoint; }
		*a_pPingResult = std::string("ping is ok! remEndian is ") + (isEndianDiffer ? "differ " : "same ");
		nReturn = 0;

	}
	catch(...){
	}


returnPoint:
	aSocket.closeC();
	return nReturn;
}


int raft::tcp::Client::GetStartTime(int32_t* a_pStartTime)
{
	::common::SocketTCP aSocket;
	uint32_t isEndianDiffer;
	int nReturn(-1);

	try {  // we put into try block because std::vector::resize can throw exception
		int nSndRcv;
		if (!ConnectAndGetEndian2(&aSocket, m_nodeId, raft::connect::fromClient2::startTime, &isEndianDiffer)) { goto returnPoint; }
		nSndRcv = aSocket.readC(a_pStartTime,4);
		if(nSndRcv!=4){
			ERROR_LOGGING2("Unable to receive server start time");
			goto returnPoint;
		}
		if (isEndianDiffer) { SWAP4BYTES(*a_pStartTime); }
		nReturn = 0;

	}
	catch(...){
	}


returnPoint:
	aSocket.closeC();
	return nReturn;
}


int raft::tcp::Client::ReceiveAllNodes( ::std::vector<NodeIdentifierKey>* a_pNodes )
{
	struct {int nodesCount,leaderIndex;}nl;
	common::SocketTCP aSocket;
	int i,nTotalSize,nSndRcv;
	uint32_t isEndianDiffer;
	int nReturn(-1);

	try {  // we put into try block because std::vector::resize can throw exception
		if (!ConnectAndGetEndian2(&aSocket, m_nodeId, raft::connect::fromClient2::allNodesInfo, &isEndianDiffer)) { goto returnPoint; }

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
