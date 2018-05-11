/*
 *	File: common_rwlock.cpp
 *
 *	Created on: 05 Feb 2017
 *	Created by: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *  This file implements ...
 *
 */


#include "common_rwlock.hpp"
#include <stddef.h>
#include <stdio.h>


common::RWLock::RWLock()
{

#ifdef _WIN32
    //m_nReadersCount = 0;
    //m_vRWMutexes[0] = CreateMutex(NULL,FALSE,NULL);
    //m_vRWMutexes[1] = CreateEvent(NULL,TRUE,TRUE,NULL);
#else  // #ifdef WIN32
    pthread_rwlock_init(&m_rwLock,NULL);
#endif  // #ifdef WIN32

}



common::RWLock::~RWLock()
{
#ifdef _WIN32
    //CloseHandle(m_vRWMutexes[0]);
    //CloseHandle(m_vRWMutexes[1]);
#else  // #ifdef WIN32
    pthread_rwlock_destroy(&m_rwLock);
#endif  // #ifdef WIN32
}


void common::RWLock::read_lock()
{
#ifdef _WIN32
	m_mutex.lock_shared();
#if 0
    WaitForSingleObject(m_vRWMutexes[0],INFINITE);
    if(!m_nReadersCount) // This should be done with real atomic routine (InterlockedExchange...)
    {
        ResetEvent(m_vRWMutexes[1]);
    }
    ReleaseMutex(m_vRWMutexes[0]);
#endif
#else  // #ifdef WIN32
    pthread_rwlock_rdlock(&m_rwLock);
#endif  // #ifdef WIN32

}


void common::RWLock::write_lock()
{

#ifdef _WIN32
    //WaitForMultipleObjectsEx(2,m_vRWMutexes,TRUE/*wait all*/,INFINITE,TRUE);
	m_mutex.lock();
#else  // #ifdef WIN32
    pthread_rwlock_wrlock(&m_rwLock);
#endif  // #ifdef WIN32

}


void common::RWLock::unlock_read()
{
#ifdef _WIN32
//#error windows part should be done (lasy to do now)
	m_mutex.unlock_shared();
#else  // #ifdef WIN32
    pthread_rwlock_unlock(&m_rwLock);
#endif  // #ifdef WIN32
}


void common::RWLock::unlock_write()
{
#ifdef _WIN32
	//#error windows part should be done (lasy to do now)
	m_mutex.unlock();
#else  // #ifdef WIN32
	pthread_rwlock_unlock(&m_rwLock);
#endif  // #ifdef WIN32
}
