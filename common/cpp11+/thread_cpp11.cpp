/*****************************************************************************
 * File:    thread_cpp11.cpp
 * created: 2017 Apr 24
 *****************************************************************************
 * Author:	D.Kalantaryan, Tel:+49(0)33762/77552 kalantar
 * Email:	davit.kalantaryan@desy.de
 * Mail:	DESY, Platanenallee 6, 15738 Zeuthen
 *****************************************************************************
 * Description
 *   ...
 ****************************************************************************/

#include "cpp11+/thread_cpp11.hpp"

#if !defined(CPP11_DEFINED2) || defined(CPP11_THREADS_IMPLEMENTED_HERE)

#ifdef _WIN32
#include <process.h>
#endif

static STDN::SYSTHRRETTYPE THREDAPI ThreadStartupRoutine1(STDN::THRINPUT a_thisThr);
static STDN::SYSTHRRETTYPE THREDAPI ThreadStartupRoutine2(STDN::THRINPUT a_thisThr);
//static STDN::SYSTHRRETTYPE THREDAPI ThreadStartupRoutine3(STDN::THRINPUT a_thisThr);

namespace STDN{
struct SThreadArgs2{
	SThreadArgs2(TypeClbK2 a_stFnc,void* a_thrArg):startRoutine(a_stFnc),thrArg(a_thrArg){}
	TypeClbK2 startRoutine; void* thrArg;
};
}

STDN::thread::thread()
	:
	m_pResource(NULL)
{
}


STDN::thread::thread(STDN::TypeClb1 a_fnc)
	:
	m_pResource(NULL)
{
	void* pArgs = FUNCTION_POINTER_TO_VOID_POINTER(a_fnc);
	ConstructThread(ThreadStartupRoutine1,pArgs);
}


STDN::thread::thread(STDN::TypeClbK2 a_fnc, void* a_arg)
	:
	m_pResource(NULL)
{
	SThreadArgs2* pArgs = new SThreadArgs2(a_fnc, a_arg);
	ConstructThread(ThreadStartupRoutine2, pArgs);
}


STDN::thread::~thread()
{
	DetachFromResourse();
}

STDN::thread& STDN::thread::operator=(const STDN::thread& a_rS)
{

	if (m_pResource){ 
		if(m_pResource->touched>0){--m_pResource->touched; }
		if (m_pResource->touched == 0) {delete m_pResource; }
	}
	m_pResource = a_rS.m_pResource;
	if(m_pResource){++m_pResource->touched;}

    return *this;
}


bool STDN::thread::joinable() const
{
    if(m_pResource && m_pResource->handle && (m_pResource->state==thread::RUN))
    {
        return true;
    }

    return false;
}


STDN::thread_native_handle STDN::thread::native_handle()
{
    return m_pResource->handle;
}


void STDN::thread::join()
{
    if(joinable())
    {
#ifdef _WIN32
		WaitForSingleObject(m_pResource->handle, INFINITE);
#else // #ifdef _WIN32
        pthread_join(m_pResource->handle,NULL);
#endif // #ifdef _WIN32
		m_pResource->state = thread::JOINED;
		m_pResource->handle = (thread_native_handle)0;
    }
}


void STDN::thread::DetachFromResourse()
{
	if (m_pResource) {
		if ((--m_pResource->touched) == 0) { 
			join();
			delete m_pResource; 
		}
		m_pResource = NULL;
	}
}


void STDN::thread::ConstructThread(TypeThread a_fnc, void* a_arg)
{
	if (m_pResource && m_pResource->handle) { return; /*Already prepared*/ }
	if (!m_pResource) { m_pResource = new SResourse; if (!m_pResource) { return;/*exception*/ } }

#ifdef _WIN32
	DWORD threadID;
	m_pResource->handle = (HANDLE)CreateThread( NULL, 0, a_fnc, a_arg, 0, &threadID);
#else  // #ifdef _WIN32
    pthread_attr_t      tattr;
    pthread_attr_init(&tattr);
    //pthread_attr_setscope(&tattr, PTHREAD_SCOPE_PROCESS);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE);
    pthread_create( &m_pResource->handle,  &tattr, a_fnc, a_arg);
    pthread_attr_destroy(&tattr);

#endif // #ifdef _WIN32

	m_pResource->state = RUN;

}


static STDN::SYSTHRRETTYPE THREDAPI ThreadStartupRoutine1(STDN::THRINPUT a_thrArgs)
{
	STDN::TypeClb1 fpFunc = (STDN::TypeClb1)a_thrArgs;
	(*fpFunc)();
	ExitThreadMcr((STDN::SYSTHRRETTYPE)0);
	return (STDN::SYSTHRRETTYPE)0;
}


static STDN::SYSTHRRETTYPE THREDAPI ThreadStartupRoutine2(STDN::THRINPUT a_thrArgs)
{
    STDN::SThreadArgs2* pArgs = (STDN::SThreadArgs2*)a_thrArgs;
    (*(pArgs->startRoutine))(pArgs->thrArg);
	delete pArgs;
	ExitThreadMcr((STDN::SYSTHRRETTYPE)0);
    return (STDN::SYSTHRRETTYPE)0;
}


#endif  // #ifndef CPP11_DEFINED2
