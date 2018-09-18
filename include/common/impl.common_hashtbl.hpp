
// common_hashtbl.impl.hpp
// 2017 Jul 21

#ifndef __impl_common_hashtbl_hpp__
#define __impl_common_hashtbl_hpp__

#include <malloc.h>
#include <memory.h>
#include <utility>

#ifndef __common_hashtbl_hpp__
#error do not include this header directly
#include "common_hashtbl.hpp"
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

static inline uint32_t hash1_(const void* key, uint32_t keySize);
#ifdef __CPP11_DEFINED__
static void* CreateKey(const void* a_pKey, size_t a_nKeyLen);
#endif

}}

template <typename DataType>
common::HashTblRaw<DataType>::HashTblRaw(uint32_t a_tInitSize)
	:
	m_fpHashFnc(&hashFncs::hash1_)
{
	uint32_t i(0);
	uint32_t tRet(a_tInitSize);

	if (!a_tInitSize) { tRet = DEFAULT_TABLE_SIZE; goto prepareHashTable; }
	for (; tRet; tRet = (a_tInitSize >> ++i));
	tRet = ((uint32_t)1) << (i - 1);
	if (tRet != a_tInitSize){tRet <<= 1;}

prepareHashTable:
	m_unRoundedTableSizeMin1 = tRet-1;
	//m_pTable = (DataType**)calloc(tRet,sizeof(DataType*));
	m_pTable = new DataType;
	if(!m_pTable){throw "Low memory!";}
}


template <typename DataType>
common::HashTblRaw<DataType>::~HashTblRaw()
{
#if 0
	DataType tData, tDataTmp;
	uint32_t tRet(m_unRoundedTableSizeMin1+1);
	for(uint32_t i(0); i<tRet;++i){
		tData = m_pTable[i];
		while(pItem){
			tDataTmp = pItem->nextH;
			delete pItem;
			pItem = pItemTmp;
		}
	}
#endif
	delete m_pTable;
}


template <typename DataType>
bool common::HashTblRaw<DataType>::AddEntry(const void* a_key, uint32_t a_nKeyLen, const DataType& a_data)
{
	try {
		DataType aData(a_data);
        return AddEntryMv(a_key, a_nKeyLen,aData);
	}
	catch (...)
	{
	}

	return false;
}

#ifdef __CPP11_DEFINED__
template <typename DataType>
bool common::HashTblRaw<DataType>::AddEntryMv(const void* a_key, uint32_t a_nKeyLen, DataType&& a_data)
{
	try {
		uint32_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);

		a_data->key = common::hashFncs::CreateKey(a_key, a_nKeyLen);
		if (!a_data->key) {return false;}
		a_data->keyLength = a_nKeyLen;
		a_data->prevH = (DataType)0;

		a_data->nextH = m_pTable[unHash];
		if(m_pTable[unHash]){m_pTable[unHash]->prevH=m_pTable[unHash]=std::move(a_data);}
		else{m_pTable[unHash] = std::move(a_data);}
		return true;
	}
	catch (...)
	{
	}

	return false;
}
#endif


template <typename DataType>
bool common::HashTblRaw<DataType>::RemoveEntry(const void* a_key, uint32_t a_nKeyLen)
{
	DataType aData;
	return RemoveAndGet(a_key, a_nKeyLen, &aData);
}


template <typename DataType>
bool common::HashTblRaw<DataType>::RemoveAndGet(const void* a_key, uint32_t a_nKeyLen, DataType* a_pData)
{
	uint32_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);
	DataType aData = m_pTable[unHash];

	while(aData != ((DataType)0)){
		if((a_nKeyLen== aData->keyLength)&&(memcmp(aData->key,a_key,a_nKeyLen)==0)){
			if(aData ==m_pTable[unHash]){m_pTable[unHash]= aData->nextH;}
			if(aData->prevH){ aData->prevH->nextH= aData->nextH;}
			if(aData->nextH){ aData->nextH->prevH= aData->prevH;}
			aData->prevH = aData->nextH = (DataType)0;
			free(aData->key); aData->key = NULL;
			aData->keyLength = 0;
			*a_pData = aData;
			return true;
		}
		aData = aData->nextH;
	}

	return false;
}


template <typename DataType>
bool common::HashTblRaw<DataType>::FindEntry(const void* a_key, uint32_t a_nKeyLen, DataType* a_pData)const
{
	uint32_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);
	DataType aData = m_pTable[unHash];

    while (aData) {
		if ((a_nKeyLen == aData->keyLength) && (memcmp(aData->key, a_key, a_nKeyLen) == 0)) {
			*a_pData = aData;
			return true;
		}
		aData = aData->nextH;
	}

	return false;
}



/****************************************************************************************************************************/
/****************************************************************************************************************************/
/****************************************************************************************************************************/

template <typename DataType>
common::HashTbl<DataType>::HashTbl(uint32_t a_tInitSize)
	:
	m_fpHashFnc(&hashFncs::hash1_)
{
	uint32_t i(0);
	uint32_t tRet(a_tInitSize);

	if (!a_tInitSize) { tRet = DEFAULT_TABLE_SIZE; goto prepareHashTable; }
	for (; tRet; tRet = (a_tInitSize >> ++i));
	tRet = ((uint32_t)1) << (i - 1);
	if (tRet != a_tInitSize){tRet <<= 1;}

prepareHashTable:
	m_unRoundedTableSizeMin1 = tRet-1;
	m_pTable = (HashItem<DataType>**)calloc(tRet,sizeof(HashItem<DataType>*));
	if(!m_pTable){throw "Low memory!";}
}


template <typename DataType>
common::HashTbl<DataType>::~HashTbl()
{
	HashItem<DataType> *pItem, *pItemTmp;
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
int common::HashTbl<DataType>::AddEntry(const void* a_key, uint32_t a_nKeyLen, const DataType& a_data)
{
	try {
		HashItem<DataType> *pItem = new HashItem<DataType>(a_key, a_nKeyLen, a_data);
		uint32_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);

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
void* common::HashTbl<DataType>::AddEntry2(const void* a_key, uint32_t a_nKeyLen, const DataType& a_data)
{
	try {
		HashItem<DataType> *pItem = new HashItem<DataType>(a_key, a_nKeyLen, a_data);
		uint32_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);

		if (!pItem) { return NULL; }
		pItem->next = m_pTable[unHash];
		if(m_pTable[unHash]){m_pTable[unHash]->prev=pItem;}
		m_pTable[unHash] = pItem;
		return pItem->key;
	}
	catch (...)
	{
	}

	return NULL;
}


template <typename DataType>
bool common::HashTbl<DataType>::RemoveEntry(const void* a_key, uint32_t a_nKeyLen)
{
	DataType aData;
	return RemoveEntry2(a_key, a_nKeyLen, &aData);
}


template <typename DataType>
bool common::HashTbl<DataType>::RemoveEntry2(const void* a_key, uint32_t a_nKeyLen, DataType* a_pData)
{
	HashItem<DataType> *pItem;
	uint32_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);

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
bool common::HashTbl<DataType>::FindEntry(const void* a_key, uint32_t a_nKeyLen, DataType* a_pData)const
{
	HashItem<DataType> *pItem;
	uint32_t unHash(((*m_fpHashFnc)(a_key, a_nKeyLen))&m_unRoundedTableSizeMin1);

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


template <typename DataType>
void common::HashTbl<DataType>::MoveContentToEmptyHash(HashTbl<DataType>* a_pOther)
{
	//hashFncs::TypeHashFnc		m_fpHashFnc;
	//HashItem<DataType>**		m_pTable;
	//uint32_t					m_unRoundedTableSizeMin1;
	
	HashItem<DataType>** pTable = m_pTable;
	uint32_t unRoundedTableSizeMin1 = m_unRoundedTableSizeMin1;

	a_pOther->m_fpHashFnc = m_fpHashFnc;
	m_pTable = a_pOther->m_pTable;
	m_unRoundedTableSizeMin1 = a_pOther->m_unRoundedTableSizeMin1;
	a_pOther->m_pTable=pTable;
	a_pOther->m_unRoundedTableSizeMin1=unRoundedTableSizeMin1;
}


/*/////////////////////////////////////////////////*/
template <typename DataType1>
template <typename DataType2>
common::HashTbl<DataType1>::HashItem<DataType2>::HashItem(const void* a_key, uint32_t a_nKeyLen, const DataType2& a_data)
	:	prev(NULL),next(NULL),key(NULL),data(a_data),dataSize(0)
{
	key = malloc(a_nKeyLen);
	if(!key){throw "low memory";}
	dataSize = a_nKeyLen;
	memcpy(key,a_key,a_nKeyLen);
}


template <typename DataType1>
template <typename DataType2>
common::HashTbl<DataType1>::HashItem<DataType2>::~HashItem()
{
	if(prev){prev->next=next;}
	if(next){next->prev=prev;}
	free(key);
}



/*////////////////////////////////////////////////////////////////////////*/
namespace common { namespace hashFncs{

static inline uint32_t hash1_( const void* a_pKey, uint32_t a_unKeySize )
{
	register uint8_t *k = (uint8_t *)a_pKey;
	register uint32_t a,b,c;  /* the internal state */
	
	uint32_t          len;    /* how many key bytes still need mixing */
	
	/* Set up the internal state */
	len = a_unKeySize;
	a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
	c = 13;         /* variable initialization of internal state */
	
	/*---------------------------------------- handle most of the key */
	while (len >= 12)
	{
		a=a+(k[0]+((uint32_t)k[1]<<8)+((uint32_t)k[2]<<16) +((uint32_t)k[3]<<24));
		b=b+(k[4]+((uint32_t)k[5]<<8)+((uint32_t)k[6]<<16) +((uint32_t)k[7]<<24));
		c=c+(k[8]+((uint32_t)k[9]<<8)+((uint32_t)k[10]<<16)+((uint32_t)k[11]<<24));
		mix2(a,b,c);
		k = k+12; len = len-12;
	}
	
	
	/*------------------------------------- handle the last 11 bytes */
	c = c+a_unKeySize;
	
	switch(len)              /* all the case statements fall through */
	{
	case 11: c=c+((uint32_t)k[10]<<24);
	case 10: c=c+((uint32_t)k[9]<<16);
	case 9 : c=c+((uint32_t)k[8]<<8);
		
		/* the first byte of c is reserved for the length */
	case 8 : b=b+((uint32_t)k[7]<<24);
	case 7 : b=b+((uint32_t)k[6]<<16);
	case 6 : b=b+((uint32_t)k[5]<<8);
	case 5 : b=b+k[4];
	case 4 : a=a+((uint32_t)k[3]<<24);
	case 3 : a=a+((uint32_t)k[2]<<16);
	case 2 : a=a+((uint32_t)k[1]<<8);
	case 1 : a=a+k[0];
		/* case 0: nothing left to add */
	}
	mix2(a,b,c);
	/*-------------------------------------------- report the result */
	
	return c;
}

#ifdef __CPP11_DEFINED__
static void* CreateKey(const void* a_pKey, size_t a_nKeyLen)
{
	void* pKey = malloc(a_nKeyLen);
	if (!pKey) { return NULL; }
	memcpy(pKey, a_pKey, a_nKeyLen);
	return pKey;
}
#endif

}}


#endif  // #ifndef __impl_common_hashtbl_hpp__
