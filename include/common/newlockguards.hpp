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

	void SetAndLockMutex(TypeMutex* pMutex);
	void UnsetAndUnlockMutex();

protected:
	TypeMutex* m_pMutex;
#if defined(_WIN32) && defined(_DEBUG) && defined(DEBUG_LOCKS)
	int			m_lockerId;
#endif
};

/*//////////////////////////////////////////*/
template <typename TypeMutex>
class NewSharedLockGuard
{
public:
	NewSharedLockGuard();
	virtual ~NewSharedLockGuard();

	void SetAndLockMutex(TypeMutex* pMutex);
	void UnsetAndUnlockMutex();

protected:
	TypeMutex * m_pMutex;
};

} // namespace common {

#include "impl.newlockguards.hpp"

#endif  // #ifndef __common_newlockguards_hpp__
