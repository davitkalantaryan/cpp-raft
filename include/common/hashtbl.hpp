//
// common/hashtbl.hpp
// 2017 Jul 21
//

#ifndef COMMON_HASHTBL_HPP
#define COMMON_HASHTBL_HPP

#include <stdint.h>
#include <stddef.h>
#include <cpp11+/common_defination.h>

#define DEFAULT_TABLE_SIZE	64

namespace common {

namespace hashFncs{
typedef size_t (*TypeHashFnc)(const void* key, size_t keySize);
}

template <typename DataType>
class HashTbl
{
public:
    HashTbl(size_t tableSize= DEFAULT_TABLE_SIZE);
	virtual ~HashTbl();

    int   AddEntry (const void* key, size_t keyLen, const DataType& data);
    bool  RemoveEntry(const void* key, size_t keyLen);
    bool  RemoveEntryAndGetRemovedData(const void* key, size_t keyLen, DataType* dataPtr);
    bool  FindEntry(const void* key, size_t keyLen, DataType* dataPtr)const;

protected:
	struct HashItem {
        HashItem(const void* key, size_t keyLen, const DataType& data);
		~HashItem();
        HashItem *prev, *next; void* key; DataType data; size_t dataSize;
	};

protected:
	hashFncs::TypeHashFnc		m_fpHashFnc;
    HashItem**                  m_pTable;
    size_t  					m_unRoundedTableSizeMin1;
};

}

#ifndef COMMON_HASHTBL_IMPL_HPP
#include "hashtbl.impl.hpp"
#endif


#endif  // #ifndef COMMON_HASHTBL_HPP
