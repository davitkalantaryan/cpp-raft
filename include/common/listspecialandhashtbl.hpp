//
// file:		listspecialandhashtbl.hpp
// created on:	2018 Jun 02
// created by:	D. Kalantaryan (davit.kalantaryan@desy.de)
//

#ifndef __common_listspecialandhashtbl_hpp__
#define __common_listspecialandhashtbl_hpp__

#include "common_hashtbl.hpp"
#include "lists.hpp"

// By special we assume that
// template argument Type has following 4 fields 
// 1. Type *prev;
// 2. Type *next;
// 3. void* key;
// 4. size_t keyLength
// All this fileds should be public

namespace common{

template <typename Type>
class ListspecialAndHashtbl
{
public:
	ListspecialAndHashtbl();
	virtual ~ListspecialAndHashtbl();

	bool  AddData(Type* newData, const void* a_key, size_t a_keyLen);			// is added, or this is a dublication
	Type* RemoveData(Type* dataToRemove);
	Type* RemoveData(const void* key, size_t keyLength);
	bool  FindEntry(const void* key, size_t keyLength, Type** a_ppData);

	Type* first() { return m_list.first(); }
	int   count()const {return m_list.count();}

protected:
	HashTbl<Type*>		m_hash;
	ListSpecial<Type>	m_list;
};

} // namespace common{

#include "impl.listspecialandhashtbl.hpp"

#endif  // #ifndef __common_listspecialandhashtbl_hpp__
