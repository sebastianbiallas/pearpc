/*
 *	HT Editor
 *	data.h
 *
 *	Copyright (C) 2002, 2003 Stefan Weyergraf
 *	Copyright (C) 2002, 2003 Sebastian Biallas (sb@biallas.net)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __DATA_H__
#define __DATA_H__

#define HAVE_HT_OBJECTS

#include "system/types.h"

#ifdef HAVE_HT_OBJECTS
typedef unsigned long ObjectID;

/* actually a str => bigendian-int */
/** used to define ObjectIDs */
#define MAGIC16(magic) (((uint16)magic[0]<<8) | (uint16)magic[1])
#define MAGIC32(magic) (((uint32)magic[0]<<24) | ((uint32)magic[1]<<16) | ((uint32)magic[2]<<8) | (uint32)magic[3])

/** No/invalid object */
#define OBJID_INVALID			((ObjectID)0)
/** A placeholder object id */
#define OBJID_TEMP			((ObjectID)-1)

#define OBJID_OBJECT			MAGIC32("DAT\x00")

#define OBJID_ARRAY			MAGIC32("DAT\x10")
#define OBJID_STACK			MAGIC32("DAT\x11")

#define OBJID_BINARY_TREE		MAGIC32("DAT\x20")
#define OBJID_AVL_TREE			MAGIC32("DAT\x21")
#define OBJID_SET			MAGIC32("DAT\x22")

#define OBJID_LINKED_LIST		MAGIC32("DAT\x30")
#define OBJID_QUEUE			MAGIC32("DAT\x31")
#define OBJID_DBL_LINKED_LIST		MAGIC32("DAT\x32")

#define OBJID_KEYVALUE			MAGIC32("DAT\x40")
#define OBJID_SINT			MAGIC32("DAT\x41")
#define OBJID_SINT64			MAGIC32("DAT\x42")
#define OBJID_UINT			MAGIC32("DAT\x43")
#define OBJID_UINT64			MAGIC32("DAT\x44")
#define OBJID_FLOAT			MAGIC32("DAT\x45")

#define OBJID_MEMAREA			MAGIC32("DAT\x48")

#define OBJID_STRING			MAGIC32("DAT\x50")
#define OBJID_ISTRING			MAGIC32("DAT\x51")

#define OBJID_AUTO_COMPARE		MAGIC32("DAT\xc0")

#endif

/**
 *	This is THE base class.
 */
class Object {
public:
				Object() {};
	virtual			~Object() {};
		void		init() {};
	virtual	void		done() {};
/* new */

/**
 *	Standard object duplicator.
 *	@returns copy of object
 */
	virtual	Object *	clone() const;
/**
 *	Standard Object comparator.
 *	@param obj object to compare to
 *	@returns 0 for equality, negative number if |this<obj| and positive number if |this>obj|
 */
	virtual	int		compareTo(const Object *obj) const;
/**
 *	Stringify object.
 *	Stringify object in string-buffer <i>s</i>. Never writes more than
 *	<i>maxlen</i> characters to <i>s</i>. If <i>maxlen</i> is > 0, a
 *	trailing zero-character is written.
 *
 *	@param s pointer to buffer that receives object stringification
 *	@param maxlen size of buffer that receives object stringification
 *	@returns number of characters written to <i>s</i>, not including the trailing zero
 */
	virtual	int		toString(char *buf, int buflen) const;
#ifdef HAVE_HT_OBJECTS
/**
 *	Standard Object idle function.
 *	Overwrite and register with htidle.cc::register_idle()
 *	(FIXME)
 *
 *	@returns true if working, false if really idle
 */
	virtual	bool		idle();
/**
 *	@returns true if <i>this</i> is an object or derived from an object of type <i>id</i>.
 */
	virtual	bool		instanceOf(ObjectID id) const;
/**
 *	@returns true if <i>this</i> is an object or derived from an object of the same type as <i>obj</i>.
 */
		bool		instanceOf(Object *obj) const;
/**
 *	@returns unique object id.
 */
	virtual	ObjectID	getObjectID() const;
#endif
};

typedef int (*Comparator)(const Object *a, const Object *b);

int autoCompare(const Object *a, const Object *b);

typedef void* ObjHandle;
#define InvObjHandle		NULL
#define InvIdx			((uint)-1)

/**
 *	An Enumerator.
 */
class Enumerator: public Object {
public:
				Enumerator();
/* extends Object */
	virtual	Enumerator *	clone() const = 0;
	virtual	int		toString(char *buf, int buflen) const;
/* new */

/**
 *	Count elements.
 *
 *	@returns number of objects contained in this structure
 *	@throws NotImplementedException if counting of elements is not supported
 */
	virtual	uint		count() const = 0;

/**
 *	Compare objects.
 *	Compare objects <i>a</i> and <i>b</i> and determine their (logical)
 *	linear order.
 *
 *	@param a object a
 *	@param b object b
 *	@returns 0 if <i>a</i> equals <i>b</i>,
 *	a value >0 if <i>a</i> is greater than <i>b</i> and
 *	a value <0 if <i>a</i> is less than <i>b</i>
 */
	virtual	int		compareObjects(const Object *a, const Object *b) const = 0;
/**
 *	Test if contained.
 *	Test if an object like <i>obj</i> is contained in this structure
 *
 *	@param obj signature of object to find
 *	@returns true if an object like <i>obj</i> is contained, false otherwise
 */
	inline	bool		contains(const Object *obj) const
	{
		return find(obj) != InvObjHandle;
	}
/**
 *	Test if empty.
 *	@returns true if empty
 */
	inline bool		isEmpty() const
	{
		return count() == 0;
	}
/**
 *	Find equal object.
 *	Find first object equaling <i>obj</i> in this structure
 *	and if found return it's object handle.
 *
 *	@param obj signature of object to find
 *	@returns object handle of found object or <i>InvObjHandle</i> if not found
 */
	virtual	ObjHandle	find(const Object *obj) const;
/**
 *	Find greater-or-equal object.
 *	Find first object being greater or equal compared to <i>obj</i> in this structure
 *	and if found return it's object handle.
 *
 *	@param obj signature of object to find
 *	@returns object handle of found object or <i>InvObjHandle</i> if not found
 */
	virtual	ObjHandle	findGE(const Object *obj) const;
/**
 *	Find less-or-equal object.
 *	Find first object being less or equal compared to <i>obj</i> in this structure
 *	and if found return it's object handle.
 *
 *	@param obj signature of object to find
 *	@returns object handle of found object or <i>InvObjHandle</i> if not found
 */
	virtual	ObjHandle	findLE(const Object *obj) const;
/**
 *	Find object's handle by index.
 *
 *	@param i index of object to find
 *	@returns object handle of found object or <i>InvObjHandle</i> if not found
 */	
	virtual	ObjHandle	findByIdx(int i) const = 0;
/**
 *	Find (logically) last element's object handle.
 *
 *	@returns object handle of the last element or <i>InvObjHandle</i>
 *	if the structure is empty
 */
	virtual	ObjHandle	findLast() const = 0;
/**
 *	Find (logically) previous element's object handle.
 *	Find logically previous element (predecessor) of the object identified
 *	by <i>h</i>. Predecessor of "the invalid object" is the last element
 *	in this structure by definition. (ie. <i>findPrev(InvObjHandle) :=
 *	findLast()</i>).
 *
 *	@param h object handle to find a predecessor to
 *	@returns object handle of predecessor or <i>InvObjHandle</i> if <i>h</i>
 *	identifies the first element.
 */
	virtual	ObjHandle	findPrev(ObjHandle h) const = 0;
/**
 *	Find (logically) first element's object handle.
 *
 *	@returns object handle of the first element or <i>InvObjHandle</i>
 *	if this structure is empty
 */
	virtual	ObjHandle	findFirst() const = 0;
/**
 *	Find (logically) next element's object handle.
 *	Find logically next element (successor) of the object, identified
 *	by <i>h</i>. Successor of "the invalid object" is the first element
 *	in this structure by definition. (ie. <i>findNext(InvObjHandle) :=
 *	findFirst()</i>).
 *
 *	@param h object handle to find a successor to
 *	@returns object handle of successor or <i>InvObjHandle</i> if <i>h</i>
 *	identifies the last element.
 */
	virtual	ObjHandle	findNext(ObjHandle h) const = 0;
/**
 *	Get object pointer from object handle.
 *
 *	@param h object handle
 *	@returns object pointer if <i>h</i> is valid, <i>NULL</i> otherwise
 */
	virtual	Object *	get(ObjHandle h) const = 0;
/**
 *	Get object's index from its handle.
 *
 *	@param h object handle of object
 *	@returns index of object if <i>h</i> is valid, <i>InvIdx</i> otherwise.
 */	
	virtual	uint		getObjIdx(ObjHandle h) const = 0;
/**
 *	Get element by index.
 *	Get the element with index <i>idx</i> (if possible).
 *
 *	@param idx index of element to get
 *	@returns pointer to the requested element or <i>NULL</i> if <i>idx</i>
 *	is invalid.
 */
		Object *	operator [] (int idx) const;
};

#define foreach(XTYPE, X, E, code...)\
for (ObjHandle temp0815 = (E).findFirst(); temp0815 != InvObjHandle;) {\
	XTYPE *X = (XTYPE*)(E).get(temp0815);                          \
	temp0815 = (E).findNext(temp0815);                             \
	{code;}                                                        \
}

#define foreachbwd(XTYPE, X, E, code...)\
for (ObjHandle temp0815 = (E).findLast(); temp0815 != InvObjHandle;) {\
	XTYPE *X = (XTYPE*)(E).get(temp0815);                         \
	temp0815 = (E).findPrev(temp0815);                            \
	{code;}                                                       \
}

#define firstThat(XTYPE, X, E, condition) \
{                                         \
	Object *Y = NULL;                 \
	foreach(XTYPE, X, E,              \
		if (condition) {          \
			Y = X;            \
			break;            \
		}                         \
	)                                 \
	X = Y;                            \
}

#define lastThat(XTYPE, X, E, condition)  \
{                                         \
	Object *Y = NULL;                 \
	foreachbwd(XTYPE, X, E,           \
		if (condition) {          \
			Y = X;            \
			break;            \
		}                         \
	)                                 \
	X = Y;                            \
}

/**
 *	A Container.
 */
class Container: public Enumerator {
protected:
#ifdef HAVE_HT_OBJECTS
	ObjectID	hom_objid;
#endif

	virtual	void		notifyInsertOrSet(const Object *o);
public:
				Container();
				
	virtual Container *	clone() const = 0;
/* new */

/**
 *	Delete all objects. (ie. remove and free all objects)
 */
	virtual	void		delAll() = 0;
/**
 *	Delete object.
 *	Delete (ie. remove and free) first object like <i>sig</i> in
 *	this structure (if possible).
 *
 *	@param sig signature of object to delete (may be inserted in the structure)
 *	@returns true if an object has been deleted, false otherwise
 */
	virtual	bool		delObj(Object *sig);
/**
 *	Delete object.
 *	Delete (ie. remove and free) object identified by <i>h</i>.
 *
 *	@param h object handle of the object to delete
 *	@returns true if the object has been deleted, false otherwise
 */
	virtual	bool		del(ObjHandle h) = 0;
/**
 *	Find or insert object.
 *	Find first object like <i>obj</i> and if that fails, insert <i>obj</i>.
 *	Ie. after call of this function it is guaranteed that <i>contains(obj)</i>.
 *
 *	@param obj object to find similar one to or object to insert
 *	@param inserted indicates if <i>obj</i> has been inserted
 *	@returns object handle of existing or inserted object (never <i>InvObjHandle</i>)
 */
	virtual	ObjHandle	findOrInsert(Object *obj, bool &inserted);
/**
 *	Insert object.
 *	Insert <i>obj</i>
 *
 *	@param obj object to insert
 *	@returns object handle of inserted object (never <i>InvObjHandle</i>)
 */
	virtual	ObjHandle	insert(Object *obj) = 0;
/**
 *	Remove object.
 *	Remove first object like <i>sig</i> from this structure.
 *	Returned object must be freed explicitly.
 *
 *	@param sig signature of object to remove
 *	@returns removed object
 */
	virtual	Object *	removeObj(const Object *sig);
/**
 *	Remove object.
 *	Remove object identified by <i>h</i>.
 *	Returned object must be freed explicitly.
 *
 *	@param h object handle of object to remove
 *	@returns removed object
 */
	virtual	Object *	remove(ObjHandle h) = 0;
/**
 *	Insert object. (operator-form)
 */
	inline	ObjHandle	operator += (Object *obj) { return insert(obj); }
/**
 *	Delete object. (operator-form)
 */
	inline	bool		operator -= (ObjHandle h) { return del(h); }
/**
 *	Delete object. (operator-form)
 */
	inline	bool		operator -= (Object *sig) { return (*this -= find(sig)); }
/**
 *	Delete object by index.
 *
 *	@param idx index of object to delete
 *	@returns true if the object has been deleted, false otherwise
 */
	inline	bool		operator -= (int idx) { return (*this -= findByIdx(idx)); }
};

/**
 *   An abstract list
 */
class List: public Container {
public:
				List();
	virtual List *		clone() const = 0;
/* new */

/**
 *	Insert object at position.
 *	Insert object <i>obj</i> at the position specified by <i>h</i>.
 *	if <i>h</i> does not specify a valid object handle (eg. InvObjHandle),
 *	this function works like <i>insert(obj)</i>.
 *
 *	@param h position to insert object at
 *	@param obj pointer to object to insert
 *	@returns true on success, false otherwise
 */
	virtual	void		insertAt(ObjHandle h, Object *obj) = 0;
/**
 *	Move element.
 *	Move element from position <i>from</i> to position <i>to</i>.
 *
 *	@param from position of element to move
 *	@param to position to move element to
 *	@returns true on success, false otherwise
 */
	virtual	bool		moveTo(ObjHandle from, ObjHandle to) = 0;
/**
 *	Prepend object.
 *	Prepend object <i>obj</i>.
 *
 *	@param obj pointer to object to prepend
 *	@returns object handle of inserted object (never <i>InvObjHandle</i>)
 */
	inline	ObjHandle	prepend(Object *obj)
	{
		insertAt(findFirst(), obj);
		return findFirst();
	}
/**
 *	Set element.
 *	Replace element at position <i>h</i> with object <i>obj</i>
 *	and delete replaced object.
 *
 *	@param h object handle of element to replace
 *	@param obj object to replace element
 *	@returns true on success, false otherwise
 */
	virtual	bool		set(ObjHandle h, Object *obj) = 0;
/**
 *	Force: Set element by index.
 *	Set element at index <i>i</i> to object <i>obj</i>
 *	and delete object previously located at this index if the index is valid.
 *	If the index <i>i</i> does not specify a valid list-index,
 *	the list is extended, so that <i>obj</i> will be the last element
 *	and the newly created entries in the list will be <i>NULL</i>s.
 *
 *	@param i index at which to set
 *	@param obj object to set
 */
	virtual	void		forceSetByIdx(int idx, Object *obj) = 0;
/**
 *	Swap two element.
 *	Swap element at position <i>h</i> with element at position <i>i</i>.
 *
 *	@param h handle of one object
 *	@param i handle of the other object
 *	@returns true on success, false otherwise
 */
	virtual	bool		swap(ObjHandle h, ObjHandle i) = 0;
};

#define ARRAY_CONSTR_ALLOC_DEFAULT		4

/**
 *   An array
 */
class Array: public List {
private:
	bool own_objects;
	uint ecount;
	uint acount;
	Object **elems;

	virtual	int		calcNewBufferSize(int curbufsize, int min_newbufsize) const;
	virtual	void		checkShrink();
	virtual	void		freeObj(Object *obj);
		void		prepareWriteAccess(int i);
		void		realloc(int n);
	inline	bool		validHandle(ObjHandle h) const;
	inline	uint		handleToNative(ObjHandle h) const;
	inline	ObjHandle	nativeToHandle(uint i) const;
public:
				Array(bool own_objects, int prealloc = ARRAY_CONSTR_ALLOC_DEFAULT);
	virtual			~Array();
/* extends Object */
	virtual	Array *		clone() const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
/* extends Enumerator */
	virtual	uint		count() const;
	virtual	int		compareObjects(const Object *a, const Object *b) const;
	virtual	ObjHandle	findByIdx(int i) const;
	virtual	ObjHandle	findFirst() const;
	virtual	ObjHandle	findLast() const;
	virtual	ObjHandle	findNext(ObjHandle h) const;
	virtual	ObjHandle	findPrev(ObjHandle h) const;
	virtual	Object *	get(ObjHandle h) const;
	virtual	uint		getObjIdx(ObjHandle h) const;
/* extends Container */
	virtual	void		delAll();
	virtual	bool		del(ObjHandle h);
	virtual	ObjHandle	insert(Object *obj);
	virtual	Object *	remove(ObjHandle h);
/* extends List */
	virtual	void		forceSetByIdx(int idx, Object *obj);
	virtual	void		insertAt(ObjHandle h, Object *obj);
	virtual	bool		moveTo(ObjHandle from, ObjHandle to);
	virtual	bool		set(ObjHandle h, Object *obj);
	virtual	bool		swap(ObjHandle h, ObjHandle i);
/* new */
	inline	Object *	operator [](int aIndex) const
	{
		return get(findByIdx(aIndex));
	}	
};

/**
 *   A stack
 */
class Stack: public Array {
public:
				Stack(bool own_objects);
/* new */
	virtual Object *	pop();
	virtual void		push(Object *obj);
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/**
 *   LinkedList's node structure
 */
struct LinkedListNode {
	Object *obj;
	LinkedListNode *next;
};

/**
 *   A (simply) linked list
 */
class LinkedList: public List {
private:
	bool own_objects;
	uint ecount;
	LinkedListNode *first, *last;

	virtual	LinkedListNode *allocNode() const;
	virtual	void		deleteNode(LinkedListNode *node) const;
	virtual	void		freeObj(Object *obj) const;
	inline	bool		validHandle(ObjHandle h) const;
	inline	LinkedListNode *handleToNative(ObjHandle h) const;
	inline	ObjHandle	nativeToHandle(LinkedListNode *n) const;
public:
				LinkedList(bool own_objects);
	virtual			~LinkedList();
/* extends Object */
	virtual	LinkedList *	clone() const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
/* extends Enumerator */
	virtual	uint		count() const;
	virtual	int		compareObjects(const Object *a, const Object *b) const;
	virtual	ObjHandle	findByIdx(int i) const;
	virtual	ObjHandle	findFirst() const;
	virtual	ObjHandle	findLast() const;
	virtual	ObjHandle	findNext(ObjHandle h) const;
	virtual	ObjHandle	findPrev(ObjHandle h) const;
	virtual	Object *	get(ObjHandle h) const;
	virtual	uint		getObjIdx(ObjHandle h) const;
/* extends Container */
	virtual	void		delAll();
	virtual	bool		del(ObjHandle h);
	virtual	ObjHandle	insert(Object *obj);
	virtual	Object *	remove(ObjHandle h);
/* extends List */
	virtual	void		forceSetByIdx(int idx, Object *obj);
	virtual	void		insertAt(ObjHandle h, Object *obj);
	virtual	bool		moveTo(ObjHandle from, ObjHandle to);
	virtual	bool		set(ObjHandle h, Object *obj);
	virtual	bool		swap(ObjHandle h, ObjHandle i);
};

/*
struct DblLinkedNode: public LinkedListNode {
	LinkedListNode *prev;
};
*/

/**
 *   A queue
 */
class Queue: public LinkedList {
public:
				Queue(bool own_objects);
/* new */

/**
 *	De-queue element.
 *	Remove and return next element of the queue.
 *
 *	@returns next element of the queue
 */
	inline	Object *	deQueue()
	{
		return remove(findFirst());
	}

/**
 *	En-queue element.
 *	Append element <i>obj</i> to the queue.
 *
 *	@param obj pointer to object to en-queue
 */
	inline	void		enQueue(Object *obj)
	{
		insert(obj);
	}

#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/**
 *   BinaryTree's node structure
 */
struct BinTreeNode {
	Object *key;
	BinTreeNode *left, *right;
	int unbalance;
};

/**
 *   A simple binary tree
 */
class BinaryTree: public Container {
protected:
	bool own_objects;
	uint ecount;
	BinTreeNode *root;
	Comparator compare;

		BinTreeNode *	allocNode() const;
		void		cloneR(BinTreeNode *node);
	virtual	void		deleteNode(BinTreeNode *node) const;
		BinTreeNode *	findNode(BinTreeNode *node, const Object *obj) const;
		BinTreeNode *	findNodeG(BinTreeNode *node, const Object *obj) const;
		BinTreeNode *	findNodeGE(BinTreeNode *node, const Object *obj) const;
		BinTreeNode *	findNodeL(BinTreeNode *node, const Object *obj) const;
		BinTreeNode *	findNodeLE(BinTreeNode *node, const Object *obj) const;
		BinTreeNode **	findNodePtr(BinTreeNode **nodeptr, const Object *obj) const;
		void		freeAll(BinTreeNode *n);
		void		freeObj(Object *obj) const;
		BinTreeNode *	getLeftmost(BinTreeNode *node) const;
		BinTreeNode *	getRightmost(BinTreeNode *node) const;
		BinTreeNode **	getLeftmostPtr(BinTreeNode **nodeptr) const;
		BinTreeNode **	getRightmostPtr(BinTreeNode **nodeptr) const;
		ObjHandle	findByIdxR(BinTreeNode *n, int &i) const;
		ObjHandle	insertR(BinTreeNode *&node, Object *obj);
	virtual	void		setNodeIdentity(BinTreeNode *node, BinTreeNode *newident);
	inline	bool		validHandle(ObjHandle h) const { return (h != InvObjHandle); }
	inline	BinTreeNode *	handleToNative(ObjHandle h) const { return (BinTreeNode*)h; }
	inline	ObjHandle	nativeToHandle(BinTreeNode *n) const { return (ObjHandle*)n; }
public:
				BinaryTree(bool own_objects, Comparator comparator = autoCompare);
	virtual			~BinaryTree();
	/* extends Object */
	virtual	BinaryTree *	clone() const;
	virtual	ObjectID	getObjectID() const;
	/* extends Enumerator */
	virtual	void		delAll();
	virtual	uint		count() const;
	virtual	int		compareObjects(const Object *a, const Object *b) const;
	virtual	ObjHandle	find(const Object *obj) const;
	virtual	ObjHandle	findG(const Object *obj) const;
	virtual	ObjHandle	findGE(const Object *obj) const;
	virtual	ObjHandle	findL(const Object *obj) const;
	virtual	ObjHandle	findLE(const Object *obj) const;
	virtual	ObjHandle	findByIdx(int i) const;
	virtual	ObjHandle	findFirst() const;
	virtual	ObjHandle	findLast() const;
	virtual	ObjHandle	findNext(ObjHandle h) const;
	virtual	ObjHandle	findPrev(ObjHandle h) const;
	virtual	Object *	get(ObjHandle h) const;
	virtual	uint		getObjIdx(ObjHandle h) const;
	/* extends Container */
	virtual	bool		del(ObjHandle h);
	virtual	ObjHandle	insert(Object *obj);
	virtual	Object *	remove(ObjHandle h);
};


/**
 *   A height-balanced binary tree (AVL)
 */
class AVLTree: public BinaryTree {
private:
		void		cloneR(BinTreeNode *node);
		BinTreeNode *	removeR(Object *key, BinTreeNode *&root, int &change, int cmp);
public:
				AVLTree(bool own_objects, Comparator comparator = autoCompare);

		void		debugOut();
		bool		expensiveCheck() const;
	/* extends Object */
	virtual	AVLTree *	clone() const;
	virtual	ObjectID	getObjectID() const;
	/* extends Container */
	virtual	ObjHandle	insert(Object *obj);
	virtual	Object *	remove(ObjHandle h);
};

/**
 *	A finite set
 */
class Set: public AVLTree {
public:
				Set(bool own_objects);
/* new */
			void	intersectWith(Set *b);
			void	unionWith(Set *b);
	inline	bool	operator &(Object *obj) const
	{
		return contains(obj);
	}

#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/**
 *	Maintains a key-value pair for easy inserting objects with "simple" keys
 *	into Containers.
 *	Key and Value will be <code>delete</code>d in the destructor.
 */
class KeyValue: public Object {
public:
	Object		*mKey;
	Object		*mValue;

				KeyValue(Object *aKey, Object *aValue);
	virtual			~KeyValue();

	virtual	KeyValue *	clone() const;
	virtual	int		compareTo(const Object *obj) const;
	virtual	int		toString(char *buf, int buflen) const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/**
 *	A signed Integer
 */
class SInt: public Object {
public:
	signed int value;

				SInt(signed int i);
/* extends Object */
	virtual	SInt *		clone() const;
	virtual	int		compareTo(const Object *obj) const;
	virtual	int		toString(char *buf, int buflen) const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

typedef SInt Integer;

/**
 *	A signed Integer (64-bit)
 */
class SInt64: public Object {
public:
	sint64 value;

				SInt64(sint64 i);
/* extends Object */
	virtual	SInt64 *	clone() const;
	virtual	int		compareTo(const Object *obj) const;
	virtual	int		toString(char *buf, int buflen) const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/**
 *	An unsigned Integer
 */
class UInt: public Object {
public:
	unsigned int value;

				UInt(unsigned int i);
/* extends Object */
	virtual	UInt *		clone() const;
	virtual	int		compareTo(const Object *obj) const;
	virtual	int		toString(char *buf, int buflen) const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/**
 *	An unsigned Integer (64-bit)
 */
class UInt64: public Object {
public:
	uint64 value;

				UInt64(uint64 i);
/* extends Object */
	virtual	UInt64 *	clone() const;
	virtual	int		compareTo(const Object *obj) const;
	virtual	int		toString(char *buf, int buflen) const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/**
 *	A floating-point number	(FIXME: no portable storage yet)
 */
class Float: public Object {
public:
	double value;

				Float(double d);
/* extends Object */
	virtual	Float *		clone() const;
	virtual	int		compareTo(const Object *obj) const;
	virtual	int		toString(char *buf, int buflen) const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/**
 *	A pointer. Cannot be stored.
 */
class Pointer: public Object {
public:
	void *value;

	Pointer(void *p);
};

/**
 *	A memory area.
 */
class MemArea: public Object {
private:
	bool duplicate;
public:
	void *ptr;
	uint size;

				MemArea(const void *p, uint size, bool duplicate = false);
				~MemArea();
/* extends Object */
	virtual	MemArea *	clone() const;
	virtual	int		compareTo(const Object *obj) const;
	virtual	int		toString(char *buf, int buflen) const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/*
 *	sorter
 */
bool quickSort(List &l);

/*
 *	Module Init/Done
 */

bool initData();
void doneData();

#endif /* __DATA_H__ */
