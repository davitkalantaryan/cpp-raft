
// common/hashtbl.impl.hpp
// 2017 Jul 21

#ifndef COMMON_HASHTBL_IMPL_HPP
#define COMMON_HASHTBL_IMPL_HPP

#include <malloc.h>
#include <memory.h>
#include <utility>
#include <new>

#ifndef COMMON_HASHTBL_HPP
//#error do not include this header directly
#include "hashtbl.hpp"
#endif

/* The mixing step */
#define mix2(a,b,c) \
{ \
  a=a-b;  a=a-c;  a=a^(c>>13); \
  b=b-c;  b=b-a;  b=b^(a<<8);  \
  c=c-a;  c=c-b;  c=c^(b>>13); \
  a=a-b;  a=a-c;  a=a^(c>>12); \
  b=b-c;  b=b-a;  b=b^(a<<16); \
  c=c-a;  c=c-b;  c=c^(b>>5);  \
  a=a-b;  a=a-c;  a=a^(c>>3);  \
  b=b-c;  b=b-a;  b=b^(a<<10); \
  c=c-a;  c=c-b;  c=c^(b>>15); \
}

namespace common { namespace hashFncs{

static inline size_t hash1_(const void* key, size_t keySize);

}}

/****************************************************************************************************************************/
/****************************************************************************************************************************/
/****************************************************************************************************************************/

template <typename DataType>
common::HashTbl<DataType>::HashTbl(size_t a_tInitSize)
	:
	m_fpHashFnc(&hashFncs::hash1_)
{
    size_t i(0);
    size_t tRet(a_tInitSize);

	if (!a_tInitSize) { tRet = DEFAULT_TABLE_SIZE; goto prepareHashTable; }
    for (; tRet; tRet = (a_tInitSize >> ++i))
        ;
    tRet = 1 << (i - 1);
	if (tRet != a_tInitSize){tRet <<= 1;}

prepareHashTable:
	m_unRoundedTableSizeMin1 = tRet-1;
    m_pTable = static_cast<HashItem**>(calloc(tRet,sizeof(HashItem*)));
    if(!m_pTable){throw ::std::bad_alloc();}
}


template <typename DataType>
common::HashTbl<DataType>::~HashTbl()
{
    HashItem *pItem, *pItemTmp;
	uint32_t tRet(m_unRoundedTableSizeMin1+1);

	for(uint32_t i(0); i<tRet;++i){
		pItem = m_pTable[i];
		while(pItem){
			pItemTmp = pItem->next;
			delete pItem;
			pItem = pItemTmp;
		}
	}
	free(m_pTable);
}


template <typename DataType>
int common::HashTbl<DataType>::AddEntry(const void* a_key, size_t a_nKeyLen, const DataType& a_data)
{
	try {
        HashItem *pItem = new HashItem(a_key, a_nKeyLen, a_data);
        size_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);

		if (!pItem) { return -1; }
		pItem->next = m_pTable[unHash];
		if(m_pTable[unHash]){m_pTable[unHash]->prev=pItem;}
		m_pTable[unHash] = pItem;
		return 0;
	}
	catch (...)
	{
	}

	return -2;
}


template <typename DataType>
bool common::HashTbl<DataType>::RemoveEntry(const void* a_key, size_t a_nKeyLen)
{
	DataType aData;
    return RemoveEntryAndGetRemovedData(a_key, a_nKeyLen, &aData);
}


template <typename DataType>
bool common::HashTbl<DataType>::RemoveEntryAndGetRemovedData(const void* a_key, size_t a_nKeyLen, DataType* a_pData)
{
    HashItem *pItem;
    size_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);

	pItem = m_pTable[unHash];

	while(pItem){
		if((a_nKeyLen==pItem->dataSize)&&(memcmp(pItem->key,a_key,a_nKeyLen)==0)){
			if (pItem == m_pTable[unHash]) { m_pTable[unHash] = pItem->next; }
			if(pItem->prev){pItem->prev->next=pItem->next;}
			if(pItem->next){pItem->next->prev=pItem->prev;}
			*a_pData = pItem->data;
			delete pItem;
			return true;
		}
		pItem = pItem->next;
	}

	return false;
}


template <typename DataType>
bool common::HashTbl<DataType>::FindEntry(const void* a_key, size_t a_nKeyLen, DataType* a_pData)const
{
    HashItem *pItem;
    size_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);

	pItem = m_pTable[unHash];

	while (pItem) {
		if ((a_nKeyLen == pItem->dataSize) && (memcmp(pItem->key, a_key, a_nKeyLen) == 0)) {
			*a_pData = pItem->data;
			return true;
		}
		pItem = pItem->next;
	}

	return false;
}



/*/////////////////////////////////////////////////*/
template <typename DataType>
common::HashTbl<DataType>::HashItem::HashItem(const void* a_key, size_t a_nKeyLen, const DataType& a_data)
    :	prev(NEWNULLPTRRAFT),next(NEWNULLPTRRAFT),key(NEWNULLPTRRAFT),data(a_data),dataSize(0)
{
	key = malloc(a_nKeyLen);
	if(!key){throw "low memory";}
	dataSize = a_nKeyLen;
	memcpy(key,a_key,a_nKeyLen);
}


template <typename DataType>
common::HashTbl<DataType>::HashItem::~HashItem()
{
	if(prev){prev->next=next;}
	if(next){next->prev=prev;}
	free(key);
}



/*////////////////////////////////////////////////////////////////////////*/
namespace common { namespace hashFncs{

static inline size_t hash1_( const void* a_pKey, size_t a_unKeySize )
{
    REGISTERRAFT const uint8_t *k = static_cast<const uint8_t *>(a_pKey);
    REGISTERRAFT size_t a,b,c;  /* the internal state */
	
    size_t          len;    /* how many key bytes still need mixing */
	
	/* Set up the internal state */
	len = a_unKeySize;
	a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
	c = 13;         /* variable initialization of internal state */
	
	/*---------------------------------------- handle most of the key */
	while (len >= 12)
	{
        a=a+(k[0]+((size_t)k[1]<<8)+((size_t)k[2]<<16) +((size_t)k[3]<<24));
        b=b+(k[4]+((size_t)k[5]<<8)+((size_t)k[6]<<16) +((size_t)k[7]<<24));
        c=c+(k[8]+((size_t)k[9]<<8)+((size_t)k[10]<<16)+((size_t)k[11]<<24));
		mix2(a,b,c);
		k = k+12; len = len-12;
	}
	
	
	/*------------------------------------- handle the last 11 bytes */
	c = c+a_unKeySize;
	
	switch(len)              /* all the case statements fall through */
	{
    case 11: c=c+((size_t)k[10]<<24);
    case 10: c=c+((size_t)k[9]<<16);
    case 9 : c=c+((size_t)k[8]<<8);
		
		/* the first byte of c is reserved for the length */
    case 8 : b=b+((size_t)k[7]<<24);
    case 7 : b=b+((size_t)k[6]<<16);
    case 6 : b=b+((size_t)k[5]<<8);
	case 5 : b=b+k[4];
    case 4 : a=a+((size_t)k[3]<<24);
    case 3 : a=a+((size_t)k[2]<<16);
    case 2 : a=a+((size_t)k[1]<<8);
	case 1 : a=a+k[0];
		/* case 0: nothing left to add */
	}
	mix2(a,b,c);
	/*-------------------------------------------- report the result */
	
	return c;
}


}}


#endif  // #ifndef COMMON_HASHTBL_IMPL_HPP
