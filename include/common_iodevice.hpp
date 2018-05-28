
// common_iodevice.hpp
// 2017 Jul 6

#ifndef __common_iodevice_hpp__
#define __common_iodevice_hpp__

#if defined(_MSC_VER) & (_MSC_VER>=1913)
#ifndef CINTERFACE
#define CINTERFACE
#endif
#endif

#include <stddef.h>

namespace common{


class IODevice
{
public:
	IODevice();
	IODevice(const common::IODevice& cM);
	virtual ~IODevice();

	virtual bool	isOpenC(void)const { return false; }
	virtual void	closeC(void);

	virtual int		readC(void* buffer, int bufferLen)const=0;
	virtual int		writeC(const void* buffer, int bufferLen)=0;

	virtual int		setTimeout(int timeoutMS) = 0;

	common::IODevice& operator=(const common::IODevice& aM);
	virtual common::IODevice* Clone()const;

protected:
	virtual void	closeHard(void) {}
	virtual void	cloneFromOther(const common::IODevice& other);

protected:
	const IODevice*		m_pPrev;
	mutable IODevice*	m_pNext;
};

};


#endif  // #ifndef __common_iodevice_hpp__
