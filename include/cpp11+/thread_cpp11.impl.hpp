/*****************************************************************************
 * File:    thread_cpp11.tos
 * created: 2017 Apr 24
 *****************************************************************************
 * Author:	D.Kalantaryan, Tel:+49(0)33762/77552 kalantar
 * Email:	davit.kalantaryan@desy.de
 * Mail:	DESY, Platanenallee 6, 15738 Zeuthen
 *****************************************************************************
 * Description
 *   ...
 ****************************************************************************/
#ifndef THREAD_CPP11_IMPL_HPP
#define THREAD_CPP11_IMPL_HPP

#ifndef THREAD_CPP11_HPP
#error this file should be included from thread_cpp11.hpp
#include "thread_cpp11.hpp"
#endif  // #ifndef THREAD_CPP11_HPP

#ifndef __CPP11_DEFINED__

template<typename TClass>
static inline STDN::SYSTHRRETTYPE THREDAPI ThreadStartupRoutine3_II(STDN::THRINPUT a_thrArgs);
template<typename TClass, typename TArg>
static inline STDN::SYSTHRRETTYPE THREDAPI ThreadFunctionWithArg(STDN::THRINPUT a_thrArgs);

namespace STDN {
template<typename TClass>
struct SArgs2 {
	void (TClass::*fnc)(); TClass* owner;
	SArgs2(void (TClass::*a_fnc)(), TClass* a_owner) :fnc(a_fnc), owner(a_owner){}
};
}

template<typename TClass,typename TArg>
struct SArgs1{
	void (TClass::*fnc)(TArg); TClass* owner; TArg arg;
    SArgs1(void (TClass::*a_fnc)(TArg),TClass* a_owner,const TArg& a_arg):
		fnc(a_fnc),owner(a_owner),arg(a_arg){}
};

template <typename TClass>
STDN::thread::thread(void (TClass::*a_fpClbK)(),TClass* a_owner)
	:
	m_pResource(NULL)
{
	SArgs2<TClass>* pArgs = new SArgs2<TClass>(a_fpClbK, a_owner);
	ConstructThread(ThreadStartupRoutine3_II<TClass>, pArgs);
}


template<typename TClass,typename TArg>
STDN::thread::thread(void (TClass::*a_fpClbK)(TArg aa_arg),TClass* a_owner,TArg a_arg)
	:
	m_pResource(NULL)
{
    SArgs1<TClass,TArg>* pArgs = new SArgs1<TClass,TArg>(a_fpClbK,a_owner,a_arg);
	ConstructThread(ThreadFunctionWithArg<TClass,TArg>, pArgs);
}


/*/////////////////////////////////////////////////////*/
template<typename TClass,typename TArg>
static inline STDN::SYSTHRRETTYPE THREDAPI ThreadFunctionWithArg(STDN::THRINPUT a_thrArgs)
{
    SArgs1<TClass,TArg>* pArgs = (SArgs1<TClass,TArg>*)a_thrArgs;
    (pArgs->owner->*pArgs->fnc)(pArgs->arg);
    delete pArgs;
	ExitThreadMcr((STDN::SYSTHRRETTYPE)0);
	return (STDN::SYSTHRRETTYPE)0;
}


template<typename TClass>
static inline STDN::SYSTHRRETTYPE THREDAPI ThreadStartupRoutine3_II(STDN::THRINPUT a_thrArgs)
{
	STDN::SArgs2<TClass>* pArgs = (STDN::SArgs2<TClass>*)a_thrArgs;
	(pArgs->owner->*pArgs->fnc)();
	delete pArgs;
	ExitThreadMcr((STDN::SYSTHRRETTYPE)0);
	return (STDN::SYSTHRRETTYPE)0;
}


#endif //#ifndef __CPP11_DEFINED__

#endif // THREAD_CPP11_IMPL_HPP
