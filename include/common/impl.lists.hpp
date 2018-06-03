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

template <typename Type>
common::ListSpecial<Type>::ListSpecial()
	:
	m_pFirst(NULL),
	m_pLast(NULL),
	m_nCount(0)
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
	a_pNewData->next = NULL;
	++m_nCount;
}


template <typename Type>
Type* common::ListSpecial<Type>::RemoveData(Type* a_pDataToRemove)
{
	if(a_pDataToRemove->prev){a_pDataToRemove->prev->next=a_pDataToRemove->next;}
	if(a_pDataToRemove->next){a_pDataToRemove->next->prev=a_pDataToRemove->prev;}
	if(a_pDataToRemove==m_pFirst){m_pFirst=m_pLast=NULL;}
	--m_nCount;
	return a_pDataToRemove->next;
}


/*////////////////////////////////////////////////////////////////////////////////*/
template <typename Type>
common::List<Type>::~List()
{
}


template <typename Type>
common::listN::ListItem<Type>* common::List<Type>::AddData(const Type& a_newData)
{
	//common::listN::ListItem<Type>* pNewItem = (common::listN::ListItem<Type>*)malloc(sizeof(common::listN::ListItem<Type>));
	common::listN::ListItem<Type>* pNewItem = new common::listN::ListItem<Type>(a_newData);
	if(!pNewItem){return NULL;}
	return m_listSp.AddData(pNewItem);
}


template <typename Type>
common::listN::ListItem<Type>* common::List<Type>::RemoveData(common::listN::ListItem<Type>* a_itemToRemove)
{
	common::listN::ListItem<Type>* pReturn = m_listSp.RemoveData(a_itemToRemove);
	delete a_itemToRemove;
	return pReturn;
}

#endif  // #ifndef __common_impl_lists_hpp__
