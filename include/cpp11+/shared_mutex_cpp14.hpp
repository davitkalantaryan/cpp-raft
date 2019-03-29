/*****************************************************************************
 * File:    shared_mutex_cpp14.hpp
 * created: 2018 Jun 22
 *****************************************************************************
 * Author:	D.Kalantaryan, Tel:+49(0)33762/77552 kalantar
 * Email:	davit.kalantaryan@desy.de
 * Mail:	DESY, Platanenallee 6, 15738 Zeuthen
 *****************************************************************************
 * Description
 *   ...
 ****************************************************************************/
#ifndef SHARED_MUTEX_CPP14_HPP
#define SHARED_MUTEX_CPP14_HPP

#include "common_defination.h"

#ifdef __CPP14_DEFINED__
#include <shared_mutex>
namespace STDN {
#ifdef __GNUC__
        typedef ::std::__shared_mutex_pthread   shared_mutex;
#else
        typedef std::shared_mutex   shared_mutex;
#endif
}
#else  // #ifdef __CPP14_DEFINED__

#define  thread_native_handle_type STDN::thread_native_handle

#include <stddef.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#define CONVERT_TO_ARG(__arg)	(__arg)
#define SHRD_BASE_TYPE_AND_ARG  LONG* a_pReadersCountBuf
#else
#include <pthread.h>
#define CONVERT_TO_ARG(__arg)
#define SHRD_BASE_TYPE_AND_ARG 
#endif

namespace STDN{

class shared_mutex_base
{
public:
    shared_mutex_base(SHRD_BASE_TYPE_AND_ARG);
    virtual ~shared_mutex_base();

	virtual int  createSharedMutex(const char* a_semaName);
	virtual void clearAll();

	virtual void lock();
	virtual void unlock();
	virtual void lock_shared();
	virtual void unlock_shared();

protected:
#ifdef _WIN32
	HANDLE				m_lockPermanent;
	HANDLE				m_semaNewConcurse;
	LONG*				m_plReadersCount;
#else
	pthread_rwlock_t	m_lockPermanent;
	bool				m_bInited;
#endif
};  // class shared_mutex_base


class shared_mutex : public shared_mutex_base
{
public:
	shared_mutex();
    virtual ~shared_mutex();

protected:
#ifdef _WIN32
	LONG				m_nReadersCount;
#else
#endif
};

} // namespace STDN{

#endif // #ifdef __CPP14_DEFINED__

#endif // SHARED_MUTEX_CPP14_HPP

