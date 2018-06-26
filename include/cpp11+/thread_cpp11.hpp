/*****************************************************************************
 * File:    thread_cpp11.hpp
 * created: 2017 Apr 24
 *****************************************************************************
 * Author:	D.Kalantaryan, Tel:+49(0)33762/77552 kalantar
 * Email:	davit.kalantaryan@desy.de
 * Mail:	DESY, Platanenallee 6, 15738 Zeuthen
 *****************************************************************************
 * Description
 *   ...
 ****************************************************************************/
#ifndef THREAD_CPP11_HPP
#define THREAD_CPP11_HPP

#include "common_defination.h"

#ifdef __CPP11_DEFINED__
#include <thread>
//#define  thread_native_handle_type native_handle_type

namespace STDN {
typedef std::thread  thread;
}

#else  // #ifdef __CPP11_DEFINED__

#define  thread_native_handle_type STDN::thread_native_handle

#include <stddef.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <pthread.h>
#endif

namespace STDN{

#ifdef _WIN32
typedef ::HANDLE thread_native_handle;
typedef DWORD SYSTHRRETTYPE;
#define THREDAPI	WINAPI
typedef LPVOID	THRINPUT;
#define ExitThreadMcr	ExitThread
#else
typedef ::pthread_t thread_native_handle;
typedef void* SYSTHRRETTYPE;
#define THREDAPI
typedef void*	THRINPUT;
#define ExitThreadMcr pthread_exit
#endif

typedef SYSTHRRETTYPE (THREDAPI * TypeThread)(THRINPUT arg);
typedef void(*TypeClb1)(void);
typedef void(*TypeClbK2)(void*owner);

class thread
{
public:
	enum STATES {JOINED,RUN};
    thread();
    thread(STDN::TypeClb1 fnc);
    thread(STDN::TypeClbK2 fnc,void* arg);
    template<typename TClass>
    thread(void (TClass::*a_fpClbK)(),TClass* a_owner);
    template<typename TClass,typename TArg>
    thread(void (TClass::*a_fpClbK)(TArg a_arg),TClass* owner,TArg arg);
    virtual ~thread();

    STDN::thread& operator=(const STDN::thread& rS);

    void join();
    bool joinable() const;

    STDN::thread_native_handle native_handle();

protected:
    void ConstructThread(TypeThread fnc,void* arg);
	void DetachFromResourse();
    
protected:
	struct SResourse {thread_native_handle handle; int touched,state;
		SResourse():handle((thread_native_handle)0), touched(1),state(JOINED) {}
	} *m_pResource;

};

} // namespace STD{

#include "thread_cpp11.impl.hpp"

#endif // #ifdef __CPP11_DEFINED__

#endif // THREAD_CPP11_HPP
