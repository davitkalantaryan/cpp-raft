//
// file:		newlockguards.impl.hpp
// created on:	2018 May 23
//

#ifndef __impl_common_newlockguards_hpp__
#define __impl_common_newlockguards_hpp__

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
void common::NewLockGuard<TypeMutex>::SetAndLockMutex(TypeMutex* a_pMutex)
{
	if(a_pMutex){
		a_pMutex->lock();
	}
	m_pMutex = a_pMutex;
}


template <typename TypeMutex>
void common::NewLockGuard<TypeMutex>::UnsetAndUnlockMutex()
{
	TypeMutex* pMutex = m_pMutex;
	m_pMutex = NULL;
	if(pMutex){pMutex->unlock();}
}

/*//////////////////////////////////////////////////////////////////////////*/

template <typename TypeMutex>
common::NewSharedLockGuard<TypeMutex>::NewSharedLockGuard()
	:
	m_pMutex(NULL)
{
}


template <typename TypeMutex>
common::NewSharedLockGuard<TypeMutex>::~NewSharedLockGuard()
{
	if(m_pMutex){m_pMutex->unlock_shared();}
}


template <typename TypeMutex>
void common::NewSharedLockGuard<TypeMutex>::SetAndLockMutex(TypeMutex* a_pMutex)
{
	a_pMutex->lock_shared();
	m_pMutex = a_pMutex;
}


template <typename TypeMutex>
void common::NewSharedLockGuard<TypeMutex>::UnsetAndUnlockMutex()
{
	TypeMutex* pMutex = m_pMutex;
	m_pMutex = NULL;
	if(pMutex){pMutex->unlock_shared();}
}

#endif  // #ifndef __impl_common_newlockguards_hpp__
