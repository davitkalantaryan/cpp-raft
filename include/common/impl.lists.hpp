//
// file:		impl.lists.hpp
// created on:	2018 Jun 02
// created by:	D. Kalantaryan
//

#ifndef __common_impl_lists_hpp__
#define __common_impl_lists_hpp__


#ifndef __common_lists_hpp__
#error do not include this file directly
#include "lists.hpp"
#endif

#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#endif

#ifdef __CPP11_DEFINED__
#include <utility>
#endif
#include "newlockguards.hpp"

template <typename Type>
common::ListSpecial<Type>::ListSpecial()
	:
	m_first((Type)0),
	m_last((Type)0),
	m_nCount(0)
{
}


template <typename Type>
common::ListSpecial<Type>::~ListSpecial()
{
}


template <typename Type>
void common::ListSpecial<Type>::AddDataRaw(Type a_newData)
{
	a_newData->prev = m_last;
	a_newData->next = (Type)0;
	if(!m_first){m_first=a_newData;}
	else{m_last->next= a_newData;}
	m_last = a_newData;
	++m_nCount;
}


template <typename Type>
Type common::ListSpecial<Type>::RemoveDataRaw(Type a_dataToRemove)
{
	if(a_dataToRemove->prev){ a_dataToRemove->prev->next= a_dataToRemove->next;}
	if(a_dataToRemove->next){ a_dataToRemove->next->prev= a_dataToRemove->prev;}
	if(a_dataToRemove == m_first){m_first=((Type)a_dataToRemove->next);}
	if(a_dataToRemove == m_last){m_last= ((Type)a_dataToRemove->prev);}
	--m_nCount;
	return (Type)a_dataToRemove->next;
}


template <typename Type>
void common::ListSpecial<Type>::MoveContentToOtherList(ListSpecial<Type>* a_pOther)
{
	if(a_pOther->m_last){a_pOther->m_last->next = m_first;}
	else { a_pOther->m_first=m_first;}
	if(m_first){m_first->prev=a_pOther->m_last;a_pOther->m_last = m_last;}
	a_pOther->m_nCount += m_nCount;
	m_first = m_last = (Type)0;
	m_nCount = 0;
}


template <typename Type>
void common::ListSpecial<Type>::MoveItemToOtherList(ListSpecial<Type>* a_pOther,Type a_item)
{
	this->RemoveDataRaw(a_item);
	a_pOther->AddDataRaw(a_item);
}


/*////////////////////////////////////////////////////////////////////////////////*/
template <typename Type>
common::List<Type>::~List()
{
}


template <typename Type>
common::listN::ListItem<Type>* common::List<Type>::AddData(const Type& a_newData)
{
	common::listN::ListItem<Type>* pNewItem = new common::listN::ListItem<Type>(a_newData);
	if(!pNewItem){return NULL;}
	ListSpecial<common::listN::ListItem<Type>* >::AddDataRaw(pNewItem);
	return pNewItem;
}


#ifdef __CPP11_DEFINED__
template <typename Type>
common::listN::ListItem<Type>* common::List<Type>::AddDataMv(Type&& a_newData)
{
	common::listN::ListItem<Type>* pNewItem = new common::listN::ListItem<Type>(std::move(a_newData));
	if (!pNewItem) { return NULL; }
	ListSpecial<common::listN::ListItem<Type>* >::AddDataRaw(pNewItem);
	return pNewItem;
}
#endif


template <typename Type>
common::listN::ListItem<Type>* common::List<Type>::RemoveData(common::listN::ListItem<Type>* a_itemToRemove)
{
	common::listN::ListItem<Type>* pReturn = ListSpecial<common::listN::ListItem<Type>* >::RemoveDataRaw(a_itemToRemove);
	delete a_itemToRemove;
	return pReturn;
}



/*//////////////////////*/
template <typename Type>
common::listN::Fifo<Type>::Fifo()
{
}


template <typename Type>
common::listN::Fifo<Type>::~Fifo()
{
}


#ifdef __CPP11_DEFINED__
template <typename Type>
void common::listN::Fifo<Type>::AddElementMv(Type&& a_newData)
{
    ::common::NewLockGuard< ::STDN::mutex > aGuard;
	aGuard.SetAndLockMutex(&m_mutex);
	m_list.AddDataMv(std::move(a_newData));
	aGuard.UnsetAndUnlockMutex();
}

#endif

template <typename Type>
bool common::listN::Fifo<Type>::AddElement(const Type& a_newData)
{
    ::common::NewLockGuard< ::STDN::mutex > aGuard;
    aGuard.SetAndLockMutex(&m_mutex);
    m_list.AddData(a_newData);
    aGuard.UnsetAndUnlockMutex();
    return true;
}


template <typename Type>
template <typename ContType>
bool common::listN::Fifo<Type>::AddElements(const ContType* a_pElements, size_t a_unCount)
{
	size_t i;
	::common::NewLockGuard< ::STDN::mutex > aGuard;
	aGuard.SetAndLockMutex(&m_mutex);
	for(i=0;i<a_unCount;++i){
		m_list.AddData((Type)a_pElements[i]);
	}
	aGuard.UnsetAndUnlockMutex();
	return true;
}

template <typename Type>
bool common::listN::Fifo<Type>::Extract(Type* a_pDataBuffer)
{
    bool bRet(false);
    ::common::NewLockGuard< ::STDN::mutex > aGuard;
    aGuard.SetAndLockMutex(&m_mutex);
    if(m_list.count()){
#ifdef __CPP11_DEFINED__
        *a_pDataBuffer = std::move(m_list.first()->data);
#else
        *a_pDataBuffer = m_list.first()->data;
#endif
        m_list.RemoveData(m_list.first());
        bRet = true;
    }
    aGuard.UnsetAndUnlockMutex();
    return bRet;
}

#endif  // #ifndef __common_impl_lists_hpp__
