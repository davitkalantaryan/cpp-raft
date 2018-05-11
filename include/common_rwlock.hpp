/*
 *	File: common_rwlock.hpp
 *
 *	Created on: 05 Feb 2017
 *	Created by: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *  This file implements ...
 *
 */
#ifndef COMMON_TOOLS_RWLOCK_HPP
#define COMMON_TOOLS_RWLOCK_HPP

#ifdef _WIN32
//#include <WinSock2.h>
//#include <WS2tcpip.h>
//#include <windows.h>
#include <shared_mutex>
#else  // #ifdef _WIN32
#include <pthread.h>
#endif  // #ifdef _WIN32

namespace common{

class RWLock
{
public:
    RWLock();
    virtual ~RWLock();

    void read_lock();
    void write_lock();
    void unlock_read();
	void unlock_write();

protected:
#ifdef _WIN32
    //int m_nReadersCount;
    //HANDLE      m_vRWMutexes[2];
	std::shared_mutex			m_mutex;
#else  // #ifdef WIN32
    pthread_rwlock_t    m_rwLock;
#endif  // #ifdef WIN32
};

}  // namespace common{

#endif // COMMON_TOOLS_RWLOCK_HPP
