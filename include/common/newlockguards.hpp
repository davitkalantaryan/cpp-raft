//
// file:		newlockguards.hpp
// created on:	2018 May 23
//

#ifndef __common_newlockguards_hpp__
#define __common_newlockguards_hpp__

namespace common {

template <typename TypeMutex>
class NewLockGuard
{
public:
	NewLockGuard();
	virtual ~NewLockGuard();

	void SetMutex(TypeMutex* pMutex);

protected:
	TypeMutex* m_pMutex;
};

/*//////////////////////////////////////////*/
template <typename TypeMutex>
class NewReadLockGuard
{
public:
	NewReadLockGuard();
	virtual ~NewReadLockGuard();

	void SetMutex(TypeMutex* pMutex);

protected:
	TypeMutex * m_pMutex;
};

} // namespace common {

#include "newlockguards.impl.hpp"

#endif  // #ifndef __common_newlockguards_hpp__
