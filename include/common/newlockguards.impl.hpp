//
// file:		newlockguards.impl.hpp
// created on:	2018 May 23
//

#ifndef __common_newlockguards_impl_hpp__
#define __common_newlockguards_impl_hpp__

#ifndef __common_newlockguards_hpp__
#error does not include this file directly
#include "newlockguards.hpp"
#endif

#include <stddef.h>

template <typename TypeMutex>
common::NewLockGuard<TypeMutex>::NewLockGuard()
	:
	m_pMutex(NULL)
{
}


template <typename TypeMutex>
common::NewLockGuard<TypeMutex>::~NewLockGuard()
{
	if(m_pMutex){m_pMutex->unlock();}
}



template <typename TypeMutex>
void common::NewLockGuard<TypeMutex>::SetMutex(TypeMutex* a_pMutex)
{
	m_pMutex = a_pMutex;
}

/*//////////////////////////////////////////////////////////////////////////*/

template <typename TypeMutex>
common::NewReadLockGuard<TypeMutex>::NewReadLockGuard()
	:
	m_pMutex(NULL)
{
}


template <typename TypeMutex>
common::NewReadLockGuard<TypeMutex>::~NewReadLockGuard()
{
	if(m_pMutex){m_pMutex->unlock_shared();}
}



template <typename TypeMutex>
void common::NewReadLockGuard<TypeMutex>::SetMutex(TypeMutex* a_pMutex)
{
	m_pMutex = a_pMutex;
}


#endif  // #ifndef __common_newlockguards_hpp__

