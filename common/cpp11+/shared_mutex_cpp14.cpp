/*****************************************************************************
 * File:    shared_mutex_cpp14.cpp
 * created: 2017 Apr 24
 *****************************************************************************
 * Author:	D.Kalantaryan, Tel:+49(0)33762/77552 kalantar
 * Email:	davit.kalantaryan@desy.de
 * Mail:	DESY, Platanenallee 6, 15738 Zeuthen
 *****************************************************************************
 * Description
 *   ...
 ****************************************************************************/

#ifndef CINTERFACE
#ifdef _WIN32
#define CINTERFACE
#endif
#endif

#include "cpp11+/shared_mutex_cpp14.hpp"

#ifndef __CPP14_DEFINED__

#include <stdio.h>

STDN::shared_mutex_base::shared_mutex_base(SHRD_BASE_TYPE_AND_ARG)
{
#ifdef _WIN32
	m_plReadersCount = a_pReadersCountBuf;
	*m_plReadersCount = 0;
	m_lockPermanent = m_semaNewConcurse = (HANDLE)0;
#else   // #ifdef _WIN32
    //m_lockPermanent = (pthread_rwlock_t)0;
    m_bInited = false;
#endif  // #ifdef _WIN32
}


STDN::shared_mutex_base::~shared_mutex_base()
{
	clearAll();
}



int STDN::shared_mutex_base::createSharedMutex(const char* a_resName)
{
	int nReturn(-1);

#ifdef _WIN32
	const char *cpcPermName, *cpcStatName;
	char vcBufferPerm[512], vcBuffStat[512];

    if(m_lockPermanent){return 1;}

	m_lockPermanent = m_semaNewConcurse = (HANDLE)0;

	if(a_resName){
		snprintf(vcBufferPerm, 511, "%s-perm",a_resName);cpcPermName=vcBufferPerm;
		snprintf(vcBuffStat, 511, "%s-newConcurse",a_resName); cpcStatName =vcBuffStat;
	}
	else{
		cpcPermName = NULL;
		cpcStatName = NULL;
	}

	m_lockPermanent = CreateSemaphoreA(NULL, 1, 5, cpcPermName);
	if(!m_lockPermanent){return -1;}
	m_semaNewConcurse = CreateSemaphoreA(NULL, 1, 5, cpcStatName);
	if (!m_semaNewConcurse) { clearAll();return -2; }
	nReturn = 0;
#else   // #ifdef _WIN32
	if(m_bInited){return 1;}
	if(a_resName){
		printf("name=%s\n", a_resName);
	}
	nReturn=pthread_rwlock_init(&m_lockPermanent,NULL);
	m_bInited=true;
#endif  // #ifdef _WIN32

	return nReturn;
}


void STDN::shared_mutex_base::clearAll()
{
#ifdef _WIN32
	if(m_semaNewConcurse){
		CloseHandle(m_semaNewConcurse);
		m_semaNewConcurse = (HANDLE)0;
	}
	if(m_lockPermanent){
		CloseHandle(m_lockPermanent);
		m_lockPermanent = (HANDLE)0;
	}
#else   // #ifdef _WIN32
	if(m_bInited){
		pthread_rwlock_destroy(&m_lockPermanent);
		m_bInited = false;
	}
#endif  // #ifdef _WIN32
}



void STDN::shared_mutex_base::lock_shared()
{
#ifdef _WIN32
	LONG lReadersCountPrevious = InterlockedIncrement(m_plReadersCount);
	printf("++++++  shared_locking (tid=%d)!\n", ::GetCurrentThreadId());

	if(lReadersCountPrevious>1){
		WaitForSingleObject(m_semaNewConcurse, INFINITE);
		ReleaseSemaphore(m_semaNewConcurse, 1, NULL);
	}
	else{
		WaitForSingleObject(m_lockPermanent, INFINITE);
	}
	
#else   // #ifdef _WIN32
	pthread_rwlock_rdlock(&m_lockPermanent);
#endif  // #ifdef _WIN32
}


void STDN::shared_mutex_base::lock()
{
#ifdef _WIN32
	printf("++++++  locking (tid=%d)!\n",::GetCurrentThreadId());
	WaitForSingleObject(m_semaNewConcurse, INFINITE);
	WaitForSingleObject(m_lockPermanent, INFINITE);
	ReleaseSemaphore(m_semaNewConcurse, 1, NULL);
#else   // #ifdef _WIN32
	pthread_rwlock_wrlock(&m_lockPermanent);
#endif  // #ifdef _WIN32
}


void STDN::shared_mutex_base::unlock()
{
#ifdef _WIN32
	ReleaseSemaphore(m_lockPermanent, 1, NULL);
	printf("------  unlocked!\n");
#else   // #ifdef _WIN32
	pthread_rwlock_unlock(&m_lockPermanent);
#endif  // #ifdef _WIN32
}


void STDN::shared_mutex_base::unlock_shared()
{
#ifdef _WIN32
	LONG lReadersCountPrevious = InterlockedDecrement(m_plReadersCount);
	if(lReadersCountPrevious==0){ReleaseSemaphore(m_lockPermanent, 1, NULL);}
	printf("------  shared_unlocking (tid=%d)!\n", ::GetCurrentThreadId());
#else   // #ifdef _WIN32
	pthread_rwlock_unlock(&m_lockPermanent);
#endif  // #ifdef _WIN32
}


/*////////////////////////////////////////////////////////////////////////////////////////////////*/
STDN::shared_mutex::shared_mutex()
	:
	STDN::shared_mutex_base(CONVERT_TO_ARG(&m_nReadersCount))
{
	createSharedMutex(NULL);
}


STDN::shared_mutex::~shared_mutex()
{
	clearAll();
}


#endif  // #ifndef __CPP11_DEFINED__
