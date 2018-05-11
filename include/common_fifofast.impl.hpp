/*
 *	File: common_fifofast.impl.hpp
 *
 *	Created on: Jun 15, 2016
 *	Author    : Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *
 */

#ifndef __fifofast_impl_hpp__
#define __fifofast_impl_hpp__

#ifndef __COMMON_FIFOFAST_HPP__
#error This file should be included only from fifofast.h
#include "common_fifofast.hpp"
#else
#endif  // #ifndef __COMMON_FIFOFAST_HPP__


//////////////////////////////////////////////////////////////
////////// class SimpleStack
//////////////////////////////////////////////////////////////

template <class Type,int CASH_SIZE_>
common::SimpleStack<Type,CASH_SIZE_>::SimpleStack()
    :
      m_nInTheStack(CASH_SIZE_)
{
}


template <class Type,int CASH_SIZE_>
common::SimpleStack<Type,CASH_SIZE_>::~SimpleStack()
{
}


template <class Type,int CASH_SIZE_>
bool common::SimpleStack<Type,CASH_SIZE_>::GetFromStack(Type* a_buf)
{
    bool bRet(false);

    m_mutex.lock();

    if(LIKELY(m_nInTheStack)){
        *a_buf = m_vBufferForStack[--m_nInTheStack];
        bRet = true;
    }

    m_mutex.unlock();
    return bRet;
}


template <class Type,int CASH_SIZE_>
bool common::SimpleStack<Type,CASH_SIZE_>::SetToStack(const Type& a_data)
{
    bool bRet(false);

    m_mutex.lock();

    if(LIKELY(m_nInTheStack<CASH_SIZE_)){
        m_vBufferForStack[m_nInTheStack++] = a_data;
        bRet = true;
    }

    m_mutex.unlock();
    return bRet;
}


template <class Type,int CASH_SIZE_>
template <typename TypeOwner>
void common::SimpleStack<Type,CASH_SIZE_>::IterateOverStack(TypeOwner* a_owner, void (TypeOwner::*a_fpFnc)(Type*))
{
    m_mutex.lock();
    //for (int i(0); i < CASH_SIZE_; ++i)
    for (int i(0); i < m_nInTheStack; ++i) // here we can have memory leak
    {
        (a_owner->*(a_fpFnc))(&m_vBufferForStack[i]);
    }
    m_mutex.unlock();
}

//////////////////////////////////////////////////////////////
////////// end class SimpleStack
//////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////
////////// class FifoFast_base
//////////////////////////////////////////////////////////////

template <class Type>
const int common::FifoFast_base<Type>::m_scnCashSizeForStacks = DEFAULT_STACK_SIZE;

template <class Type>
common::FifoFast_base<Type>::FifoFast_base(int a_nMaxSize, int a_nCashSize, SListStr<Type>**a_ppCashedEntries)
	:	m_cnMaxSize(a_nMaxSize),
		m_cnCashSize(a_nCashSize),
		m_nIndexInCashPlus1(a_nCashSize),
		m_nNumOfElemets(0),
		m_Mutex(),
		m_pFirst(NULL),
		m_pLast(NULL),
		m_ppCashedEntries(a_ppCashedEntries)
{
    if (!m_ppCashedEntries) throw "Low memory!";
	for (int i(0); i < a_nCashSize; ++i)
	{
		m_ppCashedEntries[i] = new SListStr<Type>;
	}
}


template <class Type>
common::FifoFast_base<Type>::~FifoFast_base()
{
	SListStr<Type> *pToDel;

	for (int i(0); i < m_nIndexInCashPlus1; ++i)
	{
		delete m_ppCashedEntries[i];
	}

	while (m_pFirst)
	{
        pToDel = m_pFirst->next;
		delete m_pFirst;
		m_pFirst = pToDel;
	}
}


template <class Type>
bool common::FifoFast_base<Type>::AddElement(const Type& a_ptNew)
{
	bool bRet(true);
	SListStr<Type>* pNewEntr;

        m_Mutex.lock();
	if (m_nNumOfElemets<m_cnMaxSize)
	{
        pNewEntr = LIKELY(m_nIndexInCashPlus1) ? m_ppCashedEntries[--m_nIndexInCashPlus1] : new SListStr < Type >;
        pNewEntr->value = a_ptNew;pNewEntr->next = NULL;

		if (!m_nNumOfElemets++) { m_pLast = m_pFirst = pNewEntr; }
        else { m_pLast->next = pNewEntr; m_pLast = pNewEntr; }
	}
	else
	{
		bRet = false;
	}
        m_Mutex.unlock();

	return bRet;
}

template <class Type>
bool common::FifoFast_base<Type>::Extract(Type*const& a_ptBuf)
{
	bool bRet(true);

        m_Mutex.lock();
	if (m_nNumOfElemets)
	{
        *a_ptBuf = m_pFirst->value;
		if (LIKELY(m_nNumOfElemets <= m_cnCashSize))
		{
			m_ppCashedEntries[m_nIndexInCashPlus1++] = m_pFirst;
            m_pFirst = m_pFirst->next;
		}
		else
		{
			SListStr<Type>* pForDelete = m_pFirst;
            m_pFirst = m_pFirst->next;
			delete pForDelete;
		}

		--m_nNumOfElemets;
	}
	else
	{
		bRet = false;
	}
        m_Mutex.unlock();
	return bRet;
}


template <class Type>
int common::FifoFast_base<Type>::size()const
{
	int nRet;
        m_Mutex.lock();
	nRet = m_nNumOfElemets;
        m_Mutex.unlock();
	return nRet;
}
//////////////////////////////////////////////////////////////
////////// end class FifoFast_base
//////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////
////////// class FifoFast
//////////////////////////////////////////////////////////////

template <class Type,int CASH_SIZE_>
const int common::FifoFast<Type,CASH_SIZE_>::m_scnCashSizeForStacks = CASH_SIZE_;

//////////////////////////////////////////////////////////////
////////// end class FifoFast
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
////////// class FifoFastDyn
//////////////////////////////////////////////////////////////
template <class Type>
common::FifoFastDyn<Type>::FifoFastDyn(int a_nMaxSize, int a_nCashSize)
	:	FifoFast_base<Type>(a_nMaxSize, a_nCashSize, 
                                (SListStr<Type>**)malloc(sizeof(SListStr<Type>*)*a_nCashSize))
{
}


template <class Type>
common::FifoFastDyn<Type>::~FifoFastDyn()
{
	free(FifoFast_base<Type>::m_ppCashedEntries);
}
//////////////////////////////////////////////////////////////
////////// end class FifoFastDyn
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
////////// class FifoAndStack
//////////////////////////////////////////////////////////////
template <class TypeFifo,class Type>
common::FifoAndStack<TypeFifo,Type>::FifoAndStack(int a_nCashSize, SListStr<Type>** a_ppCashedEntries,int a_nMaxSize)
    :	TypeFifo(a_nMaxSize<0?a_nCashSize:a_nMaxSize,a_nCashSize,a_ppCashedEntries)
{
}


template <class TypeFifo,class Type>
common::FifoAndStack<TypeFifo,Type>::FifoAndStack(int a_nCashSize, int a_nMaxSize)
    :	TypeFifo(a_nMaxSize<0?a_nCashSize:a_nMaxSize,a_nCashSize)
{
}


template <class TypeFifo,class Type>
common::FifoAndStack<TypeFifo,Type>::FifoAndStack(int a_nMaxSize)
    :	TypeFifo(a_nMaxSize<0?TypeFifo::m_scnCashSizeForStacks:a_nMaxSize)
{
}


template <class TypeFifo,class Type>
common::FifoAndStack<TypeFifo,Type>::~FifoAndStack()
{
}


#if 1
template <class TypeFifo,class Type>
bool common::FifoAndStack<TypeFifo,Type>::GetFromStack(Type* a_buf)
{
    bool bRet(false);

    TypeFifo::m_Mutex.lock();

    if(LIKELY(TypeFifo::m_nIndexInCashPlus1)){
        *a_buf = TypeFifo::m_ppCashedEntries[--TypeFifo::m_nIndexInCashPlus1]->value;
        bRet = true;
    }

    TypeFifo::m_Mutex.unlock();
    return bRet;
}


template <class TypeFifo,class Type>
bool common::FifoAndStack<TypeFifo,Type>::SetToStack(const Type& a_data)
{
    bool bRet(false);

    TypeFifo::m_Mutex.lock();

    if(LIKELY(TypeFifo::m_nIndexInCashPlus1<TypeFifo::m_cnCashSize)){
        TypeFifo::m_ppCashedEntries[TypeFifo::m_nIndexInCashPlus1++]->value = a_data;
        bRet = true;
    }

    TypeFifo::m_Mutex.unlock();
    return bRet;
}
#endif


template <class TypeFifo,class Type>
template <typename TypeOwner>
void common::FifoAndStack<TypeFifo,Type>::IterateOverStack(TypeOwner* a_owner, void (TypeOwner::*a_fpFnc)(Type*))
{
    for (int i(0); i < TypeFifo::m_cnCashSize; ++i)
    {
        (a_owner->*(a_fpFnc))(&TypeFifo::m_ppCashedEntries[i]->value);
    }
}

//////////////////////////////////////////////////////////////
////////// end class FifoAndStack
//////////////////////////////////////////////////////////////



#endif  // #ifndef __fifofast_tos__
