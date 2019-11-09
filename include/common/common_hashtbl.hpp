//
// common/common_hashtbl.hpp
// 2017 Jul 21
//

#ifndef COMMON_COMMON_HASHTBL_HPP
#define COMMON_COMMON_HASHTBL_HPP

#include <stdint.h>
#include <stddef.h>
#include <cpp11+/common_defination.h>

#define DEFAULT_TABLE_SIZE	64

namespace common {

namespace hashFncs{
typedef size_t (*TypeHashFnc)(const void* key, size_t keySize);
}

template <typename DataType>
class HashTblRaw
{
public:
    HashTblRaw(size_t tableSize= DEFAULT_TABLE_SIZE);
	virtual ~HashTblRaw();

    bool  AddEntry (const void* key, size_t keyLen, const DataType& data);
#ifdef CPP11_DEFINED2
    bool  AddEntryMv(const void* key, size_t keyLen, DataType&& data);
#endif
    bool  RemoveEntry(const void* key, size_t keyLen);
    bool  RemoveAndGet(const void* key, size_t keyLen, DataType* dataPtr);
    bool  FindEntry(const void* key, size_t keyLen, DataType* dataPtr)const;

protected:
	hashFncs::TypeHashFnc		m_fpHashFnc;
	DataType*					m_pTable;
    size_t  					m_unRoundedTableSizeMin1;
};


template <typename DataType>
class HashTbl
{
public:
    HashTbl(size_t tableSize= DEFAULT_TABLE_SIZE);
	virtual ~HashTbl();

    int   AddEntry (const void* key, size_t keyLen, const DataType& data);
    void* AddEntry2(const void* key, size_t keyLen, const DataType& data);  // NULL is error
    bool  RemoveEntry(const void* key, size_t keyLen);
    bool  RemoveEntry2(const void* key, size_t keyLen, DataType* dataPtr);
    bool  FindEntry(const void* key, size_t keyLen, DataType* dataPtr)const;

	void  MoveContentToEmptyHash(HashTbl* pOther);

protected:
    template <typename DataTypeIt>
	struct HashItem {
        HashItem(const void* key, size_t keyLen, const DataTypeIt& data);
		~HashItem();
        HashItem *prev, *next; void* key; DataType data; size_t dataSize;
	};

protected:
	hashFncs::TypeHashFnc		m_fpHashFnc;
	HashItem<DataType>**		m_pTable;
    size_t  					m_unRoundedTableSizeMin1;
};

}

#ifndef COMMON_IMPL_COMMON_HASHTBL_HPP
#include "impl.common_hashtbl.hpp"
#endif



#endif  // #ifndef COMMON_COMMON_HASHTBL_HPP
