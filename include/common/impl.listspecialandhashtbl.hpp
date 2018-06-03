//
// file:		impl.listspecialandhashtbl.hpp
// created on:	2018 Jun 02
// created by:	D. Kalantaryan (davit.kalantaryan@desy.de)
//

#ifndef __common__impl_listspecialandhashtbl_hpp__
#define __common__impl_listspecialandhashtbl_hpp__


#ifndef __common_listspecialandhashtbl_hpp__
#error do not include this file directly
#include "listspecialandhashtbl.hpp"
#endif

template <typename Type>
common::ListspecialAndHashtbl<Type>::ListspecialAndHashtbl()
{
}


template <typename Type>
common::ListspecialAndHashtbl<Type>::~ListspecialAndHashtbl()
{
}


template <typename Type>
bool common::ListspecialAndHashtbl<Type>::AddData(Type* a_newData, const void* a_key, size_t a_keyLen)
{
	Type* pNodeTmp;

	if(m_hash.FindEntry(a_key,a_keyLen,&pNodeTmp)){return false;}

	m_list.AddData(a_newData);
	a_newData->key = m_hash.AddEntry2(a_key, a_keyLen, a_newData);
	a_newData->keyLength = a_keyLen;

	return true;
}


template <typename Type>
Type* common::ListspecialAndHashtbl<Type>::RemoveData(Type* a_dataToRemove)
{
	if(!m_hash.RemoveEntry(a_dataToRemove->key, a_dataToRemove->keyLength)){
		return NULL;
	}
	return m_list.RemoveData(a_dataToRemove);
}


template <typename Type>
Type* common::ListspecialAndHashtbl<Type>::RemoveData(const void* a_key, size_t a_keyLength)
{
	Type* pNodeTmp;
	if(!m_hash.RemoveEntry2(a_key, a_keyLength,&pNodeTmp)){
		return NULL;
	}
	return m_list.RemoveData(pNodeTmp);
}


template <typename Type>
bool common::ListspecialAndHashtbl<Type>::FindEntry(const void* a_key, size_t a_keyLength, Type** a_ppData)
{
	return m_hash.FindEntry(a_key, a_keyLength, a_ppData);
}

#endif   // #ifndef __common__impl_listspecialandhashtbl_hpp__
