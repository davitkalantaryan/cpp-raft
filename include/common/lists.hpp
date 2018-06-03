// 
// file:		lists.hpp
// created on:	2018 Jun 02
// created by:	D. Kalantaryan (davit.kalantaryan@desy.de)
//

#ifndef __common_lists_hpp__
#define __common_lists_hpp__

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
	int   count()const {return m_nCount;}

protected:
	Type	*m_pFirst, *m_pLast;
	int		m_nCount;
};

template <typename Type>
class List;

namespace listN {

template <typename ItemType>
struct ListItem {
	ListItem *prev, *next;
	ItemType data;
	/*---------------------*/
	friend class List<ItemType>;
private:
	ListItem(const ItemType& a_data):data(a_data){}
	~ListItem() {}
};

} // namespace listN {


template <typename Type>
class List
{
public:
	virtual ~List();

	common::listN::ListItem<Type>* AddData(const Type& newData);
	common::listN::ListItem<Type>* RemoveData(common::listN::ListItem<Type>* itemToRemove);

	common::listN::ListItem<Type>* first() {return m_listSp.first();}
	int   count()const {return m_listSp.count();}

protected:
	ListSpecial<common::listN::ListItem<Type>>	m_listSp;
};


} // namespace common {

#include "impl.lists.hpp"


#endif  // #ifndef __common_lists_hpp__
