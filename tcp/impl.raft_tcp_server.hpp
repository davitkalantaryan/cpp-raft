

#ifndef __common_impl_raft_tcp_server_hpp__
#define __common_impl_raft_tcp_server_hpp__

#ifndef __common_raft_tcp_server_hpp__
#error do not include this file directly
#include "raft_tcp_server.hpp"
#endif
#include "common/newlockguards.hpp"
#include "cpp11+/thread_cpp11.hpp"


template <typename Type>
void raft::tcp::Server::ThreadFunctionOtherPeriodic(void (Type::*a_fpClbk)(int), int a_nPeriod, Type* a_pObj, int a_nIndex)
{
	common::NewSharedLockGuard<STDN::shared_mutex> aShrdLockGuard;

	if(a_nPeriod<m_nPeriodForPeriodic){ a_nPeriod =m_nPeriodForPeriodic;}


enterLoopPoint:
	try {
		while (m_nWork) {

			aShrdLockGuard.SetAndLockMutex(&m_shrdMutexForNodes);
			(a_pObj->*a_fpClbk)(a_nIndex);
			aShrdLockGuard.UnsetAndUnlockMutex();
			SleepMs(a_nPeriod);
		}
	}
	catch (...) {
		aShrdLockGuard.UnsetAndUnlockMutex();
		goto enterLoopPoint;
		return;
	}
}


template <typename Type>
void raft::tcp::Server::StartOtherPriodic(void (Type::*a_fpClbk)(int), int a_nPeriod, Type* a_pObj)
{
	int nIndex = (int)m_vectThreadsOtherPeriodic.size();
	typename STDN::thread* pThread = new typename STDN::thread(&Server::ThreadFunctionOtherPeriodic<Type>, this, a_fpClbk, a_nPeriod, a_pObj, nIndex);
	HANDLE_MEM_DEF2(pThread," ");
	m_vectThreadsOtherPeriodic.push_back(pThread);
}




#endif  // #ifndef __common_impl_raft_tcp_server_hpp__
