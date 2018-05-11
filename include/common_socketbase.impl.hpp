
// common_socketbase.impl.hpp
// 2017 Jul 07

#ifndef __common_socketbase_impl_hpp__
#define __common_socketbase_impl_hpp__

#ifndef __common_socketbase_hpp__
#error do not include this header directly
#include "common_socketbase.hpp"
#endif

#ifdef _SELECT_NEEDED_

#ifdef WIN32
//#include <windows.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <memory.h>
#endif

namespace common{ namespace socketN{

template <typename TypeCntRd, typename TypeCntWr, typename TypeCntEr, typename TypeCntRet>
int	selectMltCpp3(
	int a_lnTimeoutMs, TypeCntRet* a_returnPtr,
	const TypeCntRd* a_rdfdP, int* a_pnStopSocket,
	const TypeCntWr* a_wrfdP, const TypeCntEr* a_erfdP)
{
	struct timeval*pTimeout;
	struct timeval aTimeout;
	TypeCntRet& a_return = *a_returnPtr;
	fd_set rfds, wfds, efds;
	fd_set* pRfds = NULL;
	STypeAndIndex sReturn;
	size_t unNsoksRd,unNsoksWr,unNsoksEr, i;
	int nIndex,nMaxPlus1(0),nSelectReturn;

	FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);

	unNsoksRd = a_rdfdP? a_rdfdP->size():0;
	for (i=0; i<unNsoksRd; ++i) {
		if (((int)(a_rdfdP->operator[](i)))>nMaxPlus1) {nMaxPlus1 = (int)(a_rdfdP->operator[](i));} 
		FD_SET((int)(a_rdfdP->operator[](i)), &rfds);
	}
	unNsoksWr = a_wrfdP? a_wrfdP->size():0;
	for (i=0; i<unNsoksWr; ++i) {
		if (((int)(a_wrfdP->operator[](i)))>nMaxPlus1) {nMaxPlus1 = (int)(a_wrfdP->operator[](i));} 
		FD_SET((int)(a_wrfdP->operator[](i)), &wfds);
	}
	unNsoksEr = a_erfdP? a_erfdP->size():0;
	for (i=0; i<unNsoksEr; ++i) {
		if (((int)(a_erfdP->operator[](i)))>nMaxPlus1) {nMaxPlus1 = (int)(a_erfdP->operator[](i));} 
		FD_SET((int)(a_erfdP->operator[](i)), &efds);
	}
	++nMaxPlus1;

	if(a_pnStopSocket){
		if((*a_pnStopSocket)<0){(*a_pnStopSocket) = (int)socket(AF_INET, SOCK_STREAM, 0);}
		if((*a_pnStopSocket)<0){return (*a_pnStopSocket);}
#ifdef WIN32
		pRfds = &rfds;
		FD_SET((*a_pnStopSocket), &rfds);
#else
		pRfds = &efds;
		FD_SET((*a_pnStopSocket), &efds);
#endif
	}

	if (a_lnTimeoutMs >= 0) {
		aTimeout.tv_sec = a_lnTimeoutMs / 1000;
		aTimeout.tv_usec = (a_lnTimeoutMs % 1000) * 1000;
		pTimeout = &aTimeout;
	}
	else { pTimeout = NULL; }

	nSelectReturn = select(nMaxPlus1, &rfds, &wfds, &efds, pTimeout);
	if (nSelectReturn < 1) { return nSelectReturn; }

	if(pRfds && (FD_ISSET(*a_pnStopSocket, pRfds))){a_return.resize(nSelectReturn-1);}
	else { a_return.resize(nSelectReturn); }
	nIndex = 0;

	for (i = 0; i<unNsoksRd; ++i) {
		sReturn.whichd = 0;
		sReturn.index = (int)i;
		if (FD_ISSET((int)(a_rdfdP->operator[](i)), &rfds)) { 
			SET_BIT(&(sReturn.whichd), READ1_BIT, 1); 
			a_return[nIndex++] = sReturn;
		}
	}
	for (i = 0; i<unNsoksWr; ++i) {
		sReturn.whichd = 0;
		sReturn.index = (int)i;
		if (FD_ISSET((int)(a_wrfdP->operator[](i)), &wfds)) { 
			SET_BIT(&(sReturn.whichd), WRITE_BIT, 1); 
			a_return[nIndex++] = sReturn;
		}
		
	}
	for (i = 0; i<unNsoksEr; ++i) {
		sReturn.whichd = 0;
		sReturn.index = (int)i;
		if (FD_ISSET((int)(a_erfdP->operator[](i)), &efds)) { 
			SET_BIT(&(sReturn.whichd), ERROR_BIT, 1); 
			a_return[nIndex++] = sReturn;
		}	
	}

	return nSelectReturn;
}

}}  // namespace common{ namespace socket{

#endif  // #ifdef _SELECT_NEEDED_



#endif  // #ifndef __common_socketbase_impl_hpp__
