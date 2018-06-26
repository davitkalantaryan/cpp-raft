
// common_iodevice.cpp
// 2017 Jul 6

#include "common/common_iodevice.hpp"

common::IODevice::IODevice()
	:
	m_pPrev(NULL),
	m_pNext(NULL)
{
}


common::IODevice::IODevice(const common::IODevice& a_cM)
{
	cloneFromOther(a_cM);
}


common::IODevice::~IODevice() 
{
	closeC();
}


void common::IODevice::closeC(void)
{
	if(isOpenC()){
		if (!m_pPrev && !m_pNext) {
			this->closeHard();
		}
		else {
			if (m_pPrev) { m_pPrev->m_pNext = m_pNext; }
			if (m_pNext) { m_pNext->m_pPrev = m_pPrev; }
		}
		m_pPrev = m_pNext = NULL;
	}
}


common::IODevice& common::IODevice::operator=(const common::IODevice& a_cM)
{
	cloneFromOther(a_cM);
	return *this;
}


void common::IODevice::cloneFromOther(const common::IODevice& a_cM)
{
	if (a_cM.isOpenC()) {
		IODevice* pNext = a_cM.m_pNext;
		a_cM.m_pNext = this;
		m_pNext = pNext;
		if (pNext) { pNext->m_pPrev = this; }
		m_pPrev = &a_cM;
	}
}


common::IODevice* common::IODevice::Clone()const
{
	return NULL;
}
