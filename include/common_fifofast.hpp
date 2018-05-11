/*
 *	File: common_fifofast.hpp
 *
 *	Created on: Jun 15, 2016
 *	Author    : Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *
 */
#ifndef __COMMON_FIFOFAST_HPP__
#define __COMMON_FIFOFAST_HPP__

#include <stdlib.h>
#include "mutex_cpp11.hpp"

#ifndef LIKELY
#define LIKELY(_x_) ((_x_))
#endif

#define SET_CASH_SIZE -1
#define DEFAULT_STACK_SIZE  32

namespace common{

template <typename Type, int CASH_SIZE_=DEFAULT_STACK_SIZE>
class SimpleStack
{
public:
    SimpleStack();
    virtual ~SimpleStack();

    bool GetFromStack(Type* buf);
    bool SetToStack(const Type& data);
    template <typename TypeOwner>
    void IterateOverStack(TypeOwner* owner, void (TypeOwner::*fpFnc)(Type*));

private:
    Type                m_vBufferForStack[CASH_SIZE_];
    mutable STDN::mutex	m_mutex;
    int                 m_nInTheStack;
};

template <typename TypeS>struct SListStr{SListStr* next;TypeS value;};

template <typename Type>
class FifoFast_base
{
public:
    FifoFast_base(int maxSize, int cashSize, SListStr<Type>**ppCashedEntries);
	virtual ~FifoFast_base();

	bool	AddElement(const Type& a_ptNew);
	bool	Extract(Type*const& a_ptBuf);
	int		size()const;

protected:
	const int			m_cnMaxSize;
	const int			m_cnCashSize;
	int					m_nIndexInCashPlus1;
	int					m_nNumOfElemets;
	mutable STDN::mutex	m_Mutex;
	SListStr<Type>*		m_pFirst;
	SListStr<Type>*		m_pLast;
	SListStr<Type>**	m_ppCashedEntries;
    static const int    m_scnCashSizeForStacks;
};



template <typename Type,int CASH_SIZE_=DEFAULT_STACK_SIZE>
class FifoFast : public FifoFast_base<Type>
{
public:
    FifoFast(int maxSize=CASH_SIZE_) : FifoFast_base<Type>(maxSize, CASH_SIZE_, m_vpCashedEntries){}
	virtual ~FifoFast(){}

protected:
        SListStr<Type>* m_vpCashedEntries[CASH_SIZE_];
        static const int m_scnCashSizeForStacks;
};


template <typename Type>
class FifoFastDyn : public FifoFast_base<Type>
{
public:
	FifoFastDyn(int maxSize, int cashSize);
	virtual ~FifoFastDyn();
};


template <typename TypeFifo, typename Type>
class FifoAndStack : public TypeFifo
{
public:
    FifoAndStack(int cashSize, SListStr<Type>**ppCashedEntries,int maxSize=SET_CASH_SIZE);
    FifoAndStack(int cashSize,int maxSize=SET_CASH_SIZE);
    FifoAndStack(int maxSize=SET_CASH_SIZE);
    virtual ~FifoAndStack();

    bool GetFromStack(Type* buf);
    bool SetToStack(const Type& data);
    template <typename TypeOwner>
    void IterateOverStack(TypeOwner* owner, void (TypeOwner::*fpFnc)(Type*));
};


} // namespace common

#include "common_fifofast.impl.hpp"

#endif // __COMMON_FIFOFAST_HPP__
