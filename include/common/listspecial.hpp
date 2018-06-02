// 
// file:		list.hpp
// created on:	2018 Jun 02
// created by:	D. Kalantaryan (davit.kalantaryan@desy.de)
//

#ifndef __common_listspecial_hpp__
#define __common_listspecial_hpp__

#include <stddef.h>

namespace common {

// By special we assume that
// template argument Type has 2 fields Type *prev, *next

template <typename Type>
class ListSpecial
{
public:
	ListSpecial();
	virtual ~ListSpecial();

	void  AddData(Type* newData);
	Type* RemoveData(Type* dataToRemove);

	Type* first() {return m_pFirst;}

protected:
	Type	*m_pFirst, *m_pLast;
};

} // namespace common {

#include "impl.listspecial.hpp"


#endif  // #ifndef __common_list_hpp__
