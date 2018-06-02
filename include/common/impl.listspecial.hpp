//
// file:		impl.list.hpp
// created on:	2018 Jun 02
// created by:	D. Kalantaryan
//

#ifndef __common_impl_listspecial_hpp__
#define __common_impl_listspecial_hpp__


#ifndef __common_listspecial_hpp__
#error do not include this file directly
#include "listspecial.hpp"
#endif


template <typename Type>
common::ListSpecial<Type>::ListSpecial()
	:
	m_pFirst(NULL),
	m_pLast(NULL)
{
}


template <typename Type>
common::ListSpecial<Type>::~ListSpecial()
{
}


template <typename Type>
void common::ListSpecial<Type>::AddData(Type* a_pNewData)
{
	if(!m_pFirst){m_pFirst=a_pNewData;}
	else{m_pLast->next=a_pNewData;}
	a_pNewData->prev = m_pLast;
	m_pLast = a_pNewData;
}


template <typename Type>
Type* common::ListSpecial<Type>::RemoveData(Type* a_pDataToRemove)
{
	if(a_pDataToRemove->prev){a_pDataToRemove->prev->next=a_pDataToRemove->next;}
	if(a_pDataToRemove->next){a_pDataToRemove->next->prev=a_pDataToRemove->prev;}
	if(a_pDataToRemove==m_pFirst){m_pFirst=m_pLast=NULL;}
	return a_pDataToRemove->next;
}

#endif  // #ifndef __common_impl_listspecial_hpp__
