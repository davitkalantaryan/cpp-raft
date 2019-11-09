/*****************************************************************************
 * File:    common_defination.h
 * created: 2017 Apr 24
 *****************************************************************************
 * Author:	D.Kalantaryan, Tel:+49(0)33762/77552 kalantar
 * Email:	davit.kalantaryan@desy.de
 * Mail:	DESY, Platanenallee 6, 15738 Zeuthen
 *****************************************************************************
 * Description
 *   ...
 ****************************************************************************/
#ifndef COMMON_DEFINATION_H
#define COMMON_DEFINATION_H

#define	CURRENT_SERIALIZER_VERSION2		5
#define	CURRENT_SERIALIZER_TYPE2		1

#ifndef THISCALL2
#ifdef _MSC_VER
#define THISCALL2 __thiscall
#else
#define THISCALL2
#endif
#endif

// Is C++11
#ifndef NOT_USE_CPP11_2
#ifndef CPP11_DEFINED2
#if defined(_MSC_VER)
#if _MSVC_LANG >= 201100L
#define CPP11_DEFINED2
#endif // #if __cplusplus >= 199711L
#elif defined(__GNUC__) // #if defined(_MSC_VER)
#if __cplusplus > 199711L
#define CPP11_DEFINED2
#endif // #if __cplusplus > 199711L
#else // #if defined(_MSC_VER)
#error this compiler is not supported
#endif // #if defined(_MSC_VER)
#endif  // #ifndef CPP11_DEFINED2
#endif  // #ifndef NOT_USE_CPP11_2

// Is C++14
#ifndef NOT_USE_CPP14_2
#ifndef CPP14_DEFINED2
#if defined(_MSC_VER)
#if _MSVC_LANG >= 201400L
#define CPP14_DEFINED2
#endif // #if __cplusplus >= 199711L
#elif defined(__GNUC__) // #if defined(_MSC_VER)
#if __cplusplus > 201103L
#define CPP14_DEFINED2
#endif // #if __cplusplus > 199711L
#else // #if defined(_MSC_VER)
#error this compiler is not supported
#endif // #if defined(_MSC_VER)
#endif  // #ifndef CPP14_DEFINED2
#endif  // #ifndef NOT_USE_CPP14_2

// This should be done after check
#ifndef OVERRIDE_AND_FINAL
#define OVERRIDE_AND_FINAL
#ifdef CPP11_DEFINED2
#define OVERRIDE2	override
#define FINAL2  	final
#else
#define OVERRIDE2
#define FINAL2
#endif
#endif

#ifdef CPP11_DEFINED2
#ifdef nullptr
#undef nullptr
#endif
#define NEWNULLPTRRAFT      nullptr
#define REGISTERRAFT
#else
#define NEWNULLPTRRAFT      NULL
#define REGISTERRAFT        register
#endif


#endif // COMMON_DEFINATION_H
