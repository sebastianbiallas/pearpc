/*
 *	HT Editor
 *	data.cc
 *
 *	Copyright (C) 2002 Stefan Weyergraf (stefan@weyergraf.de)
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

#include <new>
#include <cstring>
#include <cstdlib>
#include <typeinfo>

#include "data.h"

#ifdef HAVE_HT_OBJECTS
#include "atom.h"
#include "except.h"
#endif

#include "data.h"
#include "debug.h"
#include "snprintf.h"
#include "stream.h"

int autoCompare(const Object *a, const Object *b)
{
#ifdef HAVE_HT_OBJECTS
// FIXME: better use instanceOf
// SB: warum auskommentieren?
// SW: weil nicht so gute logik
//     wie gesagt FIXME, aber bin mir unsicher wie genau

/*	uint	ida = a->getObjectID();
	uint idb = b->getObjectID();
	if (ida != idb) return ida-idb;*/
#endif
	return a->compareTo(b);
}

/*
 *	Class Object
 */

Object::Object(BuildCtorArg)
{
}

Object::Object()
{
}

Object::~Object()
{
}

void Object::init()
{
}

void Object::done()
{
}

int Object::compareTo(const Object *obj) const
{
#ifdef HAVE_HT_OBJECTS
	throw new NotImplementedException(HERE);
#else
	return 1;
#endif
}

int Object::toString(char *buf, int buflen) const
{
#ifdef HAVE_HT_OBJECTS
	ObjectID oid = getObjectID();
	unsigned char c[16];
	int l = 4;
	c[0] = (oid >> 24) & 0xff;
	c[1] = (oid >> 16) & 0xff;
	c[2] = (oid >>  8) & 0xff;
	c[3] = oid & 0xff;
	for (int i=0; i<l; i++) {
		if ((c[i]<32) || (c[i]>127)) {
			c[i+1] = "0123456789abcdef"[c[i]>>4];
			c[i+2] = "0123456789abcdef"[c[i]&0xf];
			c[i] = '\\';
			l += 2;
		}
	}
	c[l] = 0;
	return ht_snprintf(buf, buflen, "Object-%s", c);
#else
	return ht_snprintf(buf, buflen, "Object");
#endif
}

Object *Object::clone() const
{
#ifdef HAVE_HT_OBJECTS
	throw new NotImplementedException(HERE);
#else
	return NULL;
#endif
}

#ifdef HAVE_HT_OBJECTS

bool	Object::idle()
{
	return false;
}

bool	Object::instanceOf(ObjectID id) const
{
	return id == getObjectID();
}

bool	Object::instanceOf(Object *o) const
{
	return instanceOf(o->getObjectID());
}

void Object::load(ObjectStream &s)
{
}

ObjectID Object::getObjectID() const
{
	return OBJID_OBJECT;
}

void	Object::store(ObjectStream &s) const
{
}

#endif /* HAVE_HT_OBJECTS */

/*
 *	Class Enumerator
 */

Enumerator::Enumerator(BuildCtorArg a) : Object(a)
{
}

Enumerator::Enumerator()
{
}

Object *Enumerator::operator [] (int idx) const
{
	ObjHandle h = findByIdx(idx);
	return (h != InvObjHandle) ? get(h) : NULL;
}

ObjHandle Enumerator::find(const Object *key) const
{
	ObjHandle h = findFirst();
	while (h != InvObjHandle) {
		if (compareObjects(get(h), key) == 0) return h;
		h = findNext(h);
	}
	return InvObjHandle;
}

ObjHandle Enumerator::findGE(const Object *key) const
{
	ObjHandle h = findFirst();
	ObjHandle best = InvObjHandle;
	while (h != InvObjHandle) {
		int c = compareObjects(get(h), key);
		if (c == 0) return h;
		if (c > 0) {
			// h is greater than key
			if (best != InvObjHandle) {
				if (compareObjects(get(h), get(best)) < 0) best = h;
			} else {
				best = h;
			}
		}
		h = findNext(h);
	}
	return best;
}

ObjHandle Enumerator::findLE(const Object *key) const
{
	ObjHandle h = findFirst();
	ObjHandle best = InvObjHandle;
	while (h != InvObjHandle) {
		int c = compareObjects(get(h), key);
		if (c == 0) return h;
		if (c < 0) {
			// h is lower than key
			if (best != InvObjHandle) {
				if (compareObjects(get(h), get(best)) > 0) best = h;
			} else {
				best = h;
			}
		}
		h = findNext(h);
	}
	return best;
}

int Enumerator::toString(char *buf, int buflen) const
{
	ObjHandle h = findFirst();
	int n = 0;
	if (buflen>1) { *buf++ = '('; buflen--; n++; }
	while ((buflen>0) && h) {
		Object *d = get(h);
		int k = d->toString(buf, buflen);
		buflen -= k;
		buf += k;
		n += k;
		bool comma;
		if (buflen>1) { *buf++ = ','; buflen--; n++; comma = true; } else comma = false;
		h = findNext(h);
		if (!h && comma) { buf--; buflen++; n--; }
	}
	if (buflen>1) { *buf++ = ')'; buflen--; n++; }
	if (buflen>0) *buf++ = 0;
	return n;
}

/*
 *	Class Container
 */

Container::Container(BuildCtorArg a) : Enumerator(a)
{
}

Container::Container()
{
	hom_objid = OBJID_TEMP;
}

bool Container::delObj(Object *sig)
{
	return del(find(sig));
}

ObjHandle Container::findOrInsert(Object *obj, bool &inserted)
{
	ObjHandle h = find(obj);
	if (h == InvObjHandle) {
		h = insert(obj);
		inserted = true;
	} else {
		inserted = false;
	}
	return h;
}

void Container::notifyInsertOrSet(const Object *o)
{
	if (!o) return;
	if (hom_objid == OBJID_TEMP) {
		hom_objid = o->getObjectID();
	} else if (hom_objid != OBJID_INVALID) {
		if (hom_objid != o->getObjectID()) hom_objid = OBJID_INVALID;
	}
}

Object *Container::removeObj(const Object *sig)
{
	return remove(find(sig));
}

/*
 *	List
 */
List::List(BuildCtorArg a) : Container(a)
{
}

List::List()
{
}

/*
 *	Class Array
 */

#define ARRAY_ALLOC_MIN			4


/*
 *	grow array size by factor (ARRAY_ALLOC_GROW_NUM / ARRAY_ALLOC_GROW_DENOM)
 *	but never by more than ARRAY_ALLOC_GROW_ABSMAX.
 */
#define ARRAY_ALLOC_GROW_NUM		3
#define ARRAY_ALLOC_GROW_DENOM	2

#define ARRAY_ALLOC_GROW_ABSMAX	64*1024

Array::Array(BuildCtorArg a) : List(a)
{
}

Array::Array(bool oo, int prealloc)
{
	own_objects = oo;
	ecount = 0;
	acount = 0;
	elems = NULL;

	realloc(prealloc);
}

Array::~Array()
{
	delAll();
}

void	Array::delAll()
{
	// SB: Doppelte ueberpruefung von oo
	if (elems) {
		if (own_objects) {
			for (uint i=0; i<ecount; i++) {
				freeObj(elems[i]);
			}
		}
		free(elems);
	}
	ecount = 0;
	acount = 0;
	elems = NULL;
}

Object *Array::clone() const
{
	Array *a = new Array(own_objects, ecount);
	for (uint i = 0; i<ecount; i++) {
		Object *e = get(findByIdx(i));
		if (own_objects) e = e->clone();
		a->insert(e);
	}
	return a;
}

// SB: kann man die funktionen nicht alphabetisch sortieren?
// SW: doch aber hab keinen bock dazu, kommt am schluss

#ifdef HAVE_HT_OBJECTS
bool	Array::instanceOf(ObjectID id) const
{
	return (id == getObjectID()) || List::instanceOf(id);
}

void Array::load(ObjectStream &s)
{
	own_objects = true;

	GET_INT32D(s, ecount);
	GET_INT32X(s, hom_objid);
	acount = 0;
	elems = NULL;
	realloc(ecount);

	for (uint i=0; i<ecount; i++) {
		Object *obj;
		s.getObject(obj, "element", hom_objid);
		elems[i] = obj;
	}
}

ObjectID Array::getObjectID() const
{
	return OBJID_ARRAY;
}

void	Array::store(ObjectStream &s) const
{
	PUT_INT32D(s, ecount);
	PUT_INT32X(s, hom_objid);

	ASSERT((ecount == 0) || (hom_objid != OBJID_TEMP));
	for (uint i = 0; i<ecount; i++) {
		s.putObject(elems[i], "element", hom_objid);
	}
}

#endif /* HAVE_HT_OBJECTS */

int Array::calcNewBufferSize(int curbufsize, int min_newbufsize) const
{
	int n = curbufsize;
	if (n < ARRAY_ALLOC_MIN) n = ARRAY_ALLOC_MIN;
	if (n < min_newbufsize) {
		while (n < min_newbufsize) {
			n *= ARRAY_ALLOC_GROW_NUM;
			n /= ARRAY_ALLOC_GROW_DENOM;
		}
	} else {
		while (n > min_newbufsize) {
			n *= ARRAY_ALLOC_GROW_DENOM;
			n /= ARRAY_ALLOC_GROW_NUM;
		}
	}
	if (n-curbufsize > ARRAY_ALLOC_GROW_ABSMAX) {
		n = curbufsize + ARRAY_ALLOC_GROW_ABSMAX;
	}
	if (n < ARRAY_ALLOC_MIN) n = ARRAY_ALLOC_MIN;
	if (n < min_newbufsize) {
		n = min_newbufsize;
	}

	return n;
}

void Array::checkShrink()
{
	// FIXME: implement automatic shrinking
}

void Array::freeObj(Object *obj)
{              	
	if (own_objects && obj) {
		obj->done();
		delete obj;
	}
}

void Array::realloc(int n)
{
	if (n == 0) n = 1;	/* alloc'ing 0 bytes not allowed */
	ASSERT((uint)n >= ecount);
	elems = (Object**)::realloc(elems, sizeof (*elems) * n);
	acount = n;
	memset(elems+ecount, 0, (acount-ecount)*sizeof(*elems));
}

/**
 *	Prepare a write access
 *	@param i position of planned write access
 */
void Array::prepareWriteAccess(int i)
{
	uint n = calcNewBufferSize(acount, i+1);
	if (n > acount) realloc(n);
}

uint Array::count() const
{
	return ecount;
}

int Array::compareObjects(const Object *a, const Object *b) const
{
	return autoCompare(a, b);
}

void Array::forceSetByIdx(int i, Object *obj)
{
// FIXME: sanity check, better idea ?
	if (i<0) ASSERT(0);
	prepareWriteAccess(i);
	freeObj(elems[i]);
	elems[i] = obj;
	notifyInsertOrSet(obj);
	if (i>=ecount) ecount = i+1;
}

Object *Array::get(ObjHandle h) const
{
	uint i = handleToNative(h);
	return validHandle(h) ? elems[i] : NULL;
}

uint Array::getObjIdx(ObjHandle h) const
{
	return InvObjHandle ? InvIdx : handleToNative(h);
}

ObjHandle Array::findByIdx(int i) const
{
	return validHandle((ObjHandle)(i+1)) ? (ObjHandle)(i+1) : InvObjHandle;
}

ObjHandle Array::findFirst() const
{
	return ecount ? nativeToHandle(0) : InvObjHandle;
}

ObjHandle Array::findLast() const
{
	return ecount ? nativeToHandle(ecount-1) : InvObjHandle;
}

ObjHandle Array::findNext(ObjHandle h) const
{
	if (!validHandle(h)) return findFirst();
	uint i = handleToNative(h);
	return (i<ecount-1) ? nativeToHandle(i+1) : InvObjHandle;
}

ObjHandle Array::findPrev(ObjHandle h) const
{
	if (!validHandle(h)) return findLast();
	uint i = handleToNative(h);
	return (i>0) ? nativeToHandle(i-1) : InvObjHandle;
}

ObjHandle Array::insert(Object *obj)
{
	prepareWriteAccess(ecount);
	elems[ecount++] = obj;
	notifyInsertOrSet(obj);
	return nativeToHandle(ecount-1);
}

bool Array::del(ObjHandle h)
{
	if (!validHandle(h)) return false;
	freeObj(remove(h));
	return true;
}

bool Array::moveTo(ObjHandle from, ObjHandle to)
{
	if (!validHandle(from)) return false;
	if (!validHandle(to)) return false;
	uint i = handleToNative(from);
	uint t = handleToNative(to);
	Object *o = elems[i];
	memmove(elems+i, elems+i+1, sizeof (*elems) * (ecount - i - 1));
	ecount--;
	insertAt(nativeToHandle(t), o);
	return true;
}

Object *Array::remove(ObjHandle h)
{
	if (!validHandle(h)) return NULL;
	uint i = handleToNative(h);
	Object *o = elems[i];
	memmove(elems+i, elems+i+1, sizeof (*elems) * (ecount - i - 1));
	ecount--;
	checkShrink();
	return o;
}

bool Array::set(ObjHandle h, Object *obj)
{
	if (!validHandle(h)) return false;
	uint i = handleToNative(h);
	freeObj(elems[i]);
	elems[i] = obj;
	notifyInsertOrSet(obj);
	return true;
}

bool Array::swap(ObjHandle h, ObjHandle i)
{
	if (!validHandle(h)) return false;
	if (!validHandle(i)) return false;
	uint H = handleToNative(h);
	uint I = handleToNative(i);
	Object *t = elems[H];
	elems[H] = elems[I];
	elems[I] = t;
	return true;
}

bool Array::validHandle(ObjHandle h) const
{
	return (handleToNative(h) < ecount);
}

uint Array::handleToNative(ObjHandle h) const
{
	return ((int)h)-1;
}

ObjHandle Array::nativeToHandle(int i) const
{
	return (ObjHandle)(i+1);
}

void Array::insertAt(ObjHandle h, Object *obj)
{
	if (!validHandle(h)) {
		insert(obj);
	} else {
		uint i = handleToNative(h);
		if (i<ecount) {
			prepareWriteAccess(ecount);
			memmove(elems+i+1, elems+i, sizeof (*elems) * (ecount - i));
			ecount++;
		} else {
			prepareWriteAccess(i);
			memset(elems+ecount, 0, sizeof (*elems) * (i - ecount));
			ecount = i+1;
		}
		elems[i] = obj;
		notifyInsertOrSet(obj);
	}
}

/*
 *	Class Stack
 */

Stack::Stack(BuildCtorArg a) : Array(a)
{
}

Stack::Stack(bool own_objects) : Array(own_objects)
{
}

void	Stack::push(Object *obj)
{
	insert(obj);
}

Object *Stack::pop()
{
	return remove(findLast());
}

#ifdef HAVE_HT_OBJECTS
bool Stack::instanceOf(ObjectID id) const
{
	return (id == getObjectID()) || Array::instanceOf(id);
}

ObjectID Stack::getObjectID() const
{
	return OBJID_STACK;
}
#endif
/*
 *	Class LinkedList
 */

LinkedList::LinkedList(BuildCtorArg a) : List(a)
{
}

LinkedList::LinkedList(bool oo)
{
	own_objects = oo;
	ecount = 0;
	first = last = NULL;
}

LinkedList::~LinkedList()
{
	delAll();
}

void LinkedList::delAll()
{
	LinkedListNode *n = first, *m;
	while (n) {
		m = n->next;
		freeObj(n->obj);
		deleteNode(n);
		n = m;
	}
	ecount = 0;
	first = last = NULL;
}

LinkedListNode *LinkedList::allocNode() const
{
	return new LinkedListNode;
}

void	LinkedList::deleteNode(LinkedListNode *node) const
{
	delete node;
}

void LinkedList::freeObj(Object *obj) const
{
	if (own_objects && obj) {
		obj->done();
		delete obj;
	}
}

Object *LinkedList::clone() const
{
	LinkedList *l = new LinkedList(own_objects);
	LinkedListNode *n = first, *m;
	while (n) {
		m = n->next;
		Object *o = m->obj;
		if (own_objects) o = o->clone();
		l->insert(o);
		n = m;
	}
	return l;
}

#ifdef HAVE_HT_OBJECTS
bool LinkedList::instanceOf(ObjectID id) const
{
	return (id == getObjectID()) || List::instanceOf(id);
}

void LinkedList::load(ObjectStream &s)
{
	own_objects = true;
	first = last = NULL;

	ecount = 0;
	int ecount;
	GET_INT32D(s, ecount);
	GET_INT32X(s, hom_objid);

	for (int i=0; i<ecount; i++) {
		Object *obj;
		s.getObject(obj, "element", hom_objid);
		insert(obj);
	}
}

ObjectID LinkedList::getObjectID() const
{
	return OBJID_LINKED_LIST;
}

void LinkedList::store(ObjectStream &s) const
{
	PUT_INT32D(s, ecount);
	PUT_INT32X(s, hom_objid);

	ObjHandle h = findFirst();
	ASSERT((h == InvObjHandle) || (hom_objid != OBJID_TEMP));
	while (h != InvObjHandle) {
		Object *o = get(h);

		s.putObject(o, "element", hom_objid);
		h = findNext(h);
	}
}

#endif
uint LinkedList::count() const
{
	return ecount;
}

int LinkedList::compareObjects(const Object *a, const Object *b) const
{
	return autoCompare(a, b);
}

void LinkedList::forceSetByIdx(int idx, Object *obj)
{
	// FIXME:
	throw new NotImplementedException(HERE);
}

Object *LinkedList::get(ObjHandle h) const
{
	LinkedListNode *n = handleToNative(h);
	return validHandle(h) ? n->obj : NULL;
}

uint LinkedList::getObjIdx(ObjHandle g) const
{
	int i = 0;
	ObjHandle h = findFirst();
	Object *obj = get(g);
	while (h != InvObjHandle) {
		if (compareObjects(get(h), obj) == 0) return i;
		i++;
		h = findNext(h);
	}
	return InvIdx;
}

ObjHandle LinkedList::findByIdx(int i) const
{
	ObjHandle h = findFirst();
	while (h != InvObjHandle) {
		if (!i--) break;
		h = findNext(h);
	}
	return h;
}

ObjHandle LinkedList::findFirst() const
{
	return nativeToHandle(first);
}

ObjHandle LinkedList::findLast() const
{
	return nativeToHandle(last);
}

ObjHandle LinkedList::findNext(ObjHandle h) const
{
	if (!validHandle(h)) return findFirst();
	LinkedListNode *n = handleToNative(h);
	return nativeToHandle(n->next);
}

ObjHandle LinkedList::findPrev(ObjHandle g) const
{
	if (!validHandle(g)) return findLast();
	LinkedListNode *ng = handleToNative(g);
	if (ng == first) return InvObjHandle;
	ObjHandle h = findFirst();
	while (h != InvObjHandle) {
		LinkedListNode *nh = handleToNative(h);
		if (nh->next == ng) return nativeToHandle(nh);
		h = findNext(h);
	}
	return InvObjHandle;
}

bool LinkedList::del(ObjHandle h)
{
	if (!h) return false;
	freeObj(remove(h));
	return true;
}

ObjHandle LinkedList::insert(Object *obj)
{
	LinkedListNode *n = allocNode();
	n->obj = obj;
	n->next = NULL;
	if (last) {
		last->next = n;
		last = n;
	} else {
		first = last = n;
	}
	ecount++;
	notifyInsertOrSet(obj);
	return nativeToHandle(n);
}

Object *LinkedList::remove(ObjHandle h)
{
	if (!validHandle(h)) return NULL;
	LinkedListNode *n = handleToNative(h);
	Object *o = n->obj;
	if (n == first) {
		if (n == last) {
			first = NULL;
			last = NULL;
		} else {
			first = n->next;
		}
	} else {
		LinkedListNode *p = handleToNative(findPrev(h));
		p->next = n->next;
		if (n == last) last = p;
	}
	deleteNode(n);
	ecount--;
	return o;
}

void LinkedList::insertAt(ObjHandle h, Object *obj)
{
	uint i = ((uint)h)-1;
	if (i>ecount-1) {
		insert(obj);
		return;
	}
	ObjHandle q = i ? findByIdx(i-1) : InvObjHandle;
	LinkedListNode *n;
	if (q != InvObjHandle) {
		n = (LinkedListNode*)q;
	} else if (i==0) {
		n = NULL;
	} else {
		insert(obj);
		return;
	}		
	LinkedListNode *m = allocNode();
	m->obj = obj;
	if (n) {
		m->next = n->next;
		n->next = m;
		if (n == last) last = m;
	} else {
		m->next = first;
		first = m;
		if (!last) last = m;
	}
	ecount++;
	notifyInsertOrSet(obj);
}

bool LinkedList::moveTo(ObjHandle from, ObjHandle to)
{
	// FIXME:
	throw new NotImplementedException(HERE);
}

bool	LinkedList::set(ObjHandle h, Object *obj)
{
	LinkedListNode *n = handleToNative(h);
	if (!n) return false;
	freeObj(n->obj);
	n->obj = obj;
	return true;
}

bool LinkedList::swap(ObjHandle h, ObjHandle i)
{
	LinkedListNode *H = handleToNative(h);
	LinkedListNode *I = handleToNative(i);
	Object *t = H->obj;
	H->obj = I->obj;
	I->obj = t;
	return true;
}

bool LinkedList::validHandle(ObjHandle h) const
{
	return h != InvObjHandle;
}

LinkedListNode* LinkedList::handleToNative(ObjHandle h) const
{
	return (LinkedListNode*)h;
}

ObjHandle LinkedList::nativeToHandle(LinkedListNode *n) const
{
	return (ObjHandle)n;
}

/*
 *	Class Queue
 */

Queue::Queue(BuildCtorArg a) : LinkedList(a)
{
}

Queue::Queue(bool own_objects) : LinkedList(own_objects)
{
}


#ifdef HAVE_HT_OBJECTS
bool Queue::instanceOf(ObjectID id) const
{
	return (id == getObjectID()) || LinkedList::instanceOf(id);
}

ObjectID Queue::getObjectID() const
{
	return OBJID_QUEUE;
}
#endif

/*
 *	Class BinaryTree
 */

BinaryTree::BinaryTree(BuildCtorArg a) : Container(a)
{
}

BinaryTree::BinaryTree(bool oo, Comparator comp)
{
	root = NULL;
	own_objects = oo;
	compare = comp;
	ecount = 0;
}

BinaryTree::~BinaryTree()
{
	delAll();
}

void BinaryTree::delAll()
{
	freeAll(root);
	root = NULL;
	ecount = 0;
}

BinTreeNode *BinaryTree::allocNode() const
{
	return new BinTreeNode;
}

void BinaryTree::deleteNode(BinTreeNode *node) const
{
	delete node;
}

BinTreeNode **BinaryTree::findNodePtr(BinTreeNode **nodeptr, const Object *obj) const
{
	BinTreeNode **x = nodeptr;
	while (x) {
		int c = compareObjects((*x)->key, obj);
		if (c < 0) {
			x = &(*x)->right;
		} else if (c > 0) {
			x = &(*x)->left;
		} else break;
	}
	return x;
}

BinTreeNode *BinaryTree::findNode(BinTreeNode *node, const Object *obj) const
{
	BinTreeNode *x = node;
	while (x) {
		int c = compareObjects(x->key, obj);
		if (c < 0) {
			x = x->right;
		} else if (c > 0) {
			x = x->left;
		} else break;
	}
	return x;
}

BinTreeNode *BinaryTree::findNodeGE(BinTreeNode *node, const Object *obj) const
{
	BinTreeNode *x = node;
	BinTreeNode *lastGreater = NULL;
	while (x) {
		int c = compareObjects(x->key, obj);
		if (c < 0) {
			if (!x->right) return lastGreater;
			x = x->right;
		} else if (c > 0) {
			if (!x->left) return x;
			lastGreater = x;
			x = x->left;
		} else return x;
	}
	return NULL;
}

BinTreeNode *BinaryTree::findNodeLE(BinTreeNode *node, const Object *obj) const
{
	BinTreeNode *x = node;
	BinTreeNode *lastLower = NULL;
	while (x) {
		int c = compareObjects(x->key, obj);
		if (c < 0) {
			if (!x->right) return x;
			lastLower = x;
			x = x->right;
		} else if (c > 0) {
			if (!x->left) return lastLower;
			x = x->left;
		} else return x;
	}
	return NULL;
}

void BinaryTree::freeAll(BinTreeNode *n)
{
	if (!n) return;
	freeAll(n->left);
	freeObj(n->key);
	freeAll(n->right);
	deleteNode(n);
}

void BinaryTree::freeObj(Object *obj) const
{
	if (own_objects && obj) {
		obj->done();
		delete obj;
	}
}

ObjHandle BinaryTree::findByIdxR(BinTreeNode *n, int &i) const
{
	if (!n) return InvObjHandle;
	ObjHandle h;
	if ((h = findByIdxR(n->left, i))) return h;
	if (i == 0) return (ObjHandle)n;
	i--;
	if ((h = findByIdxR(n->right, i))) return h;
	return InvObjHandle;
}

BinTreeNode *BinaryTree::getLeftmost(BinTreeNode *node) const
{
	if (node) while (node->left) node = node->left;
	return node;
}

BinTreeNode *BinaryTree::getRightmost(BinTreeNode *node) const
{
	if (node) while (node->right) node = node->right;
	return node;
}

BinTreeNode **BinaryTree::getLeftmostPtr(BinTreeNode **p) const
{
	if (*p) while ((*p)->left) p = &(*p)->left;
	return p;
}

BinTreeNode **BinaryTree::getRightmostPtr(BinTreeNode **p) const
{
	if (*p) while ((*p)->right) p = &(*p)->right;
	return p;
}

void BinaryTree::cloneR(BinTreeNode *node)
{
	if (!node) return;
	Object *o = own_objects ? node->key->clone() : node->key;
	// SB: nicht gut: (unnoetige compares)
	insert(o);

	cloneR(node->left);
	cloneR(node->right);
}

Object *BinaryTree::clone() const
{
	BinaryTree *c = new BinaryTree(own_objects, compare);
	c->cloneR(root);
	return c;
}

#ifdef HAVE_HT_OBJECTS
bool	BinaryTree::instanceOf(ObjectID id) const
{
	return (id == getObjectID()) || Container::instanceOf(id);
}

void BinaryTree::loadR(ObjectStream &s, BinTreeNode **n, int l, int r)
{
	if (l > r) {
		*n = NULL;
		return;
	}
	*n = allocNode();
	uint m = (l+r)/2;
	loadR(s, &(*n)->left, l, m-1);

	s.getObject((*n)->key, "element", hom_objid);

	loadR(s, &(*n)->right, m+1, r);
}

void BinaryTree::load(ObjectStream &s)
{
	const void *m = getAtomValue(GETX_INT32(s, "comparator"));
	if (!m) throw new MsgException("BinaryTree::load() : invalid 'comparator' !");
	compare = (Comparator)m;

	GET_INT32D(s, ecount);
	GET_INT32X(s, hom_objid);
	root = NULL;
	own_objects = true;

	if (ecount) loadR(s, &root, 0, ecount-1);
}

ObjectID BinaryTree::getObjectID() const
{
	return OBJID_BINARY_TREE;
}

void BinaryTree::storeR(ObjectStream &s, BinTreeNode *n) const
{
	if (!n) return;
	storeR(s, n->left);

	s.putObject(n->key, "element", hom_objid);

	storeR(s, n->right);
}

void	BinaryTree::store(ObjectStream &s) const
{
	int aId = getAtomId((void*)compare);
	if (!aId) throw new MsgException("BinaryTree::store() : comparator not registered !");
	PUTX_INT32X(s, aId, "comparator");

	PUT_INT32D(s, ecount);
	PUT_INT32X(s, hom_objid);

	ASSERT(hom_objid != OBJID_TEMP);
	storeR(s, root);
}

#endif

uint BinaryTree::count() const
{
	return ecount;
}

int BinaryTree::compareObjects(const Object *a, const Object *b) const
{
	return compare(a, b);
}

ObjHandle BinaryTree::find(const Object *key) const
{
	return findNode(root, key);
}

ObjHandle BinaryTree::findGE(const Object *key) const
{
	return findNodeGE(root, key);
}

ObjHandle BinaryTree::findLE(const Object *key) const
{
	return findNodeLE(root, key);
}

Object *BinaryTree::get(ObjHandle h) const
{
	BinTreeNode *n = handleToNative(h);
	return validHandle(h) ? n->key : NULL;
}

uint BinaryTree::getObjIdx(ObjHandle h) const
{
	// FIXME: implement it
	throw new NotImplementedException(HERE);
}

ObjHandle BinaryTree::findByIdx(int i) const
{
	return findByIdxR(root, i);
}

ObjHandle BinaryTree::findFirst() const
{
	return nativeToHandle(getLeftmost(root));
}

ObjHandle BinaryTree::findLast() const
{
	return nativeToHandle(getRightmost(root));
}

ObjHandle BinaryTree::findNext(ObjHandle h) const
{
	if (!validHandle(h)) return findFirst();
	BinTreeNode *n = handleToNative(h);
	if (n->right) return nativeToHandle(getLeftmost(n->right));
	BinTreeNode *x = root, *result = NULL;
	while (x) {
		int c = compareObjects(x->key, n->key);
		if (c > 0) {
			result = x;
			x = x->left;
		} else {
			x = x->right;
		}
	}
	return nativeToHandle(result);
}

ObjHandle BinaryTree::findPrev(ObjHandle h) const
{
	if (!validHandle(h)) return findLast();
	BinTreeNode *n = handleToNative(h);
	if (n->left) return nativeToHandle(getRightmost(n->left));
	BinTreeNode *x = root, *result = NULL;
	while (x) {
		int c = compareObjects(x->key, n->key);
		if (c < 0) {
			result = x;
			x = x->right;
		} else {
			x = x->left;
		}
	}
	return nativeToHandle(result);
}

bool BinaryTree::del(ObjHandle h)
{
	if (!validHandle(h)) return false;
	BinTreeNode *n = handleToNative(h);
	Object *obj = n->key;
	bool r = remove(h);
	freeObj(obj);
	return r;
}

// SB: ich haette gerne noch ein findOrInsert (besserer Name noetig),
//     das entweder einfuegt oder - wenns das schon gibt -
//     das ObjHandle zurueckgibt (bzw immer das objHandle zurueckgibt)
// SW: interface + naive implementierung in Container sind da
ObjHandle BinaryTree::insert(Object *obj)
{
	return insertR(root, obj);
}

ObjHandle BinaryTree::insertR(BinTreeNode *&node, Object *obj)
{
	if (!node) {
		node = allocNode();
		node->key = obj;
		node->left = NULL;
		node->right = NULL;
		ecount++;
		notifyInsertOrSet(obj);
		return nativeToHandle(node);
	}
	int c = compareObjects(obj, node->key);
	if (c > 0) {
		return insertR(node->right, obj);
	} else if (c < 0) {
		return insertR(node->left, obj);
	} else return InvObjHandle;
}

Object *BinaryTree::remove(ObjHandle h)
{
	if (!validHandle(h)) return NULL;
	/* n is the node, whose key has to be removed */
	BinTreeNode *n = handleToNative(h);
	/* d is node that is to be removed - not necessarily n. */
	BinTreeNode *d;

	Object *o = n->key;
	if (n->left && n->right) {
		/* p is pointer to left/right inside Parent(d) with: *p = d */
		BinTreeNode **p = getLeftmostPtr(&n->right);
		d = *p;
		*p = (*p)->right;
	} else if (n->left || n->right) {
		d = n->left ? n->left : n->right;
		n->left = d->left;
		n->right = d->right;
	} else {
		// SB: hier wuerde ein remove(Object *o) mit integriertem find()
		//     auch (etwas) schneller sein, da man kein findNodePtr mehr braucht
		// SW: interface ist da: remove(Object *o)
		BinTreeNode **p = findNodePtr(&root, n->key);
		d = *p;
		*p = NULL;
	}

	n->key = d->key;
	deleteNode(d);
	ecount--;
	return o;
}

bool BinaryTree::validHandle(ObjHandle h) const
{
	return (h != InvObjHandle);
}

BinTreeNode *BinaryTree::handleToNative(ObjHandle h) const
{
	return (BinTreeNode*)h;
}

ObjHandle BinaryTree::nativeToHandle(BinTreeNode *n) const
{
	return (ObjHandle*)n;
}

/*
 *	Class AVLTree
 */

AVLTree::AVLTree(BuildCtorArg a) : BinaryTree(a)
{
}

AVLTree::AVLTree(bool aOwnObjects, Comparator aComparator)
 : BinaryTree(aOwnObjects, aComparator)
{

}

void debugOutNode(FILE *f, AVLTreeNode *n, AVLTreeNode *p)
{
	if (n) {
		char b[1024];
		ht_snprintf(b, sizeof b, "node: { title: \"%y\" label: \"%y (%d)\" }\n", n->key, n->key, n->unbalance);
		fputs(b, f);
		if (p) {
			ht_snprintf(b, sizeof b, "edge: { sourcename: \"%y\" targetname: \"%y\" }\n", p->key, n->key);
			fputs(b, f);
		}
		debugOutNode(f, (AVLTreeNode *)n->right, n);
		debugOutNode(f, (AVLTreeNode *)n->left, n);
	}
}

void AVLTree::debugOut()
{
	FILE *f = fopen("test.vcg", "wb");
	fputs("graph: {\nlayoutalgorithm: tree\n", f);
	debugOutNode(f, (AVLTreeNode *)root, NULL);
	fputs("}\n", f);
	fclose(f);
}

bool AVLTree__expensiveCheck(BinTreeNode *n, int &height)
{
	if (n) {
		int left, right;
		if (!AVLTree__expensiveCheck(n->left, left)) return false;
		if (!AVLTree__expensiveCheck(n->right, right)) return false;
		height = MAX(left, right)+1;
		if (left < right) {
			return ((AVLTreeNode *)n)->unbalance == 1;
		} else if (left > right) {
			return ((AVLTreeNode *)n)->unbalance == -1;
		} else {
			return ((AVLTreeNode *)n)->unbalance == 0;
		}
	} else {
		height = 0;
		return true;
	}
}

bool AVLTree::expensiveCheck()
{
	int dummy;
	return AVLTree__expensiveCheck(root, dummy);
}

AVLTreeNode *AVLTree::allocNode() const
{
	return new AVLTreeNode;
}


void AVLTree::cloneR(AVLTreeNode *node)
{
	if (!node) return;
	Object *o = own_objects ? node->key->clone() : node->key;
	// SB: nicht gut: (unnoetige compares)
	insert(o);

	cloneR((AVLTreeNode *)node->left);
	cloneR((AVLTreeNode *)node->right);
}

Object *AVLTree::clone() const
{
	AVLTree *c = new AVLTree(own_objects, compare);
	c->cloneR((AVLTreeNode *)root);
	return c;
}

#ifdef HAVE_HT_OBJECTS
bool AVLTree::instanceOf(ObjectID id) const
{
	return (id == getObjectID()) || BinaryTree::instanceOf(id);
}

int AVLTree::loadR(ObjectStream &s, BinTreeNode *&n, int l, int r)
{
	if (l > r) {
		n = NULL;
		return 0;
	}
	n = allocNode();
	uint m = (l+r)/2;

	int L = loadR(s, n->left, l, m-1);

	s.getObject(n->key, "element", hom_objid);

	int R = loadR(s, n->right, m+1, r);

	if (L < R) {
		((AVLTreeNode *)n)->unbalance = +1;
	} else if (L > R) {
		((AVLTreeNode *)n)->unbalance = -1;
	} else {
		((AVLTreeNode *)n)->unbalance = 0;
	}
	return MAX(L, R)+1;
}

void AVLTree::load(ObjectStream &s)
{
	const void *m = getAtomValue(GETX_INT32X(s, "comparator"));
	if (!m) throw new MsgException("AVLTree::load() : invalid 'comparator' !");
	compare = (Comparator)m;

	GET_INT32D(s, ecount);
	GET_INT32X(s, hom_objid);
	root = NULL;
	own_objects = true;

	loadR(s, root, 0, ecount-1);
}

ObjectID AVLTree::getObjectID() const
{
	return OBJID_AVL_TREE;
}

#endif

ObjHandle AVLTree::insert(Object *obj)
{
#if 0
	// to be removed:
	if (!root) {
		root = allocNode();
		root->key = obj;
		root->left = root->right = NULL;
		((AVLTreeNode *)root)->unbalance = 0;
		ecount++;
		return true;
	}
	
	AVLTreeNode *t = NULL;
	AVLTreeNode *s = (AVLTreeNode *)root;
	AVLTreeNode *p = (AVLTreeNode *)root;     
	AVLTreeNode *q;
	// Search
	while (1) {
		int c = compareObjects(obj, p->key);
		if (c < 0) {
			q = (AVLTreeNode *)p->left;
			if (!q) {
				p->left = q = allocNode();
				break;
			} else {
				if (q->unbalance) {
					t = p;
					s = q;
				}
			}
		} else if (c>0) {
			q = (AVLTreeNode *)p->right;
			if (!q) {
				p->right = q = allocNode();
				break;
			} else {
				if (q->unbalance) {
					t = p;
					s = q;
				}
			}
		} else {
			// element found
			return false;
		}
		p = q;
	}
	// Insert
	q->key = obj;
	q->left = q->right = NULL;
	q->unbalance = 0;
	ecount++;
	// Rebalance
	int a;
	AVLTreeNode *r;
	if (compareObjects(obj, s->key) < 0) {
		a = -1;
		r = p = (AVLTreeNode *)s->left;
	} else {
		a = 1;
		r = p = (AVLTreeNode *)s->right;
	}
	while (p != q) {
		if (compareObjects(obj, p->key) < 0) {
			p->unbalance = -1;
			p = (AVLTreeNode *)p->left;
		} else {
			p->unbalance = 1;
			p = (AVLTreeNode *)p->right;
		}
	}
	if (!s->unbalance) {
		// tree was balanced before insertion
		s->unbalance = a;
	} else if (s->unbalance == -a) {
		// tree has become more balanced
		s->unbalance = 0;
	} else {
		// tree is out of balance
		if (r->unbalance == a) {
			// single rotation
			p = r;
			if (a < 0) {
				s->left = r->right;
				r->right = s;
			} else {
				s->right = r->left;
				r->left = s;
			}
			s->unbalance = r->unbalance = 0;
		} else {
			// double rotation
			if (a < 0) {
				p = (AVLTreeNode *)r->right;
				r->right = p->left;
				p->left = r;
				s->left = p->right;
				p->right = s;
			} else {
				p = (AVLTreeNode *)r->left;
				r->left = p->right;
				p->right = r;
				s->right = p->left;
				p->left = s;
			}
			// to be optimized
			if (p->unbalance == a) {
				s->unbalance = -a;
				r->unbalance = 0;
			} else if (p->unbalance == -a) {
				s->unbalance = 0;
				r->unbalance = a;
			} else {
				s->unbalance = 0;
				r->unbalance = 0;
			}
		}
		// finalization
		if (t == NULL) {
			root = p;
		} else if (s == t->right) {
			t->right = p;
		} else {
			t->left = p;
		}
	}
	return true;
#else
	/* t will point to the node where rebalancing may be necessary */
	AVLTreeNode **t = (AVLTreeNode **)&root;
	/* *pp will walk through the tree */
	AVLTreeNode **pp = (AVLTreeNode **)&root;
	// Search
	while (*pp) {
		int c = compareObjects(obj, (*pp)->key);
		if (c < 0) {
			pp = (AVLTreeNode **)&(*pp)->left;
		} else if (c > 0) {
			pp = (AVLTreeNode **)&(*pp)->right;
		} else {
			// element found
			return InvObjHandle;
		}
		if (*pp && (*pp)->unbalance) {
			t = pp;
		}
	}

	/* s points to the node where rebalancing may be necessary */
	AVLTreeNode *s = *t;

	// Insert
	*pp = allocNode();
	AVLTreeNode *retval = *pp;
	retval->key = obj;
	retval->left = retval->right = NULL;
	retval->unbalance = 0;
	ecount++;
	notifyInsertOrSet(obj);
	if (!s) return nativeToHandle(retval);

	// Rebalance
	int a;
	AVLTreeNode *r;
	AVLTreeNode *p;
	if (compareObjects(obj, s->key) < 0) {
		a = -1;
		r = p = (AVLTreeNode *)s->left;
	} else {
		a = 1;
		r = p = (AVLTreeNode *)s->right;
	}
	while (p != retval) {
		if (compareObjects(obj, p->key) < 0) {
			p->unbalance = -1;
			p = (AVLTreeNode *)p->left;
		} else {
			p->unbalance = 1;
			p = (AVLTreeNode *)p->right;
		}
	}
	if (!s->unbalance) {
		// tree was balanced before insertion
		s->unbalance = a;
	} else if (s->unbalance == -a) {
		// tree has become more balanced
		s->unbalance = 0;
	} else {
		// tree is out of balance
		if (r->unbalance == a) {
			// single rotation
			p = r;
			if (a < 0) {
				s->left = r->right;
				r->right = s;
			} else {
				s->right = r->left;
				r->left = s;
			}
			s->unbalance = r->unbalance = 0;
		} else {
			// double rotation
			if (a < 0) {
				p = (AVLTreeNode *)r->right;
				r->right = p->left;
				p->left = r;
				s->left = p->right;
				p->right = s;
			} else {
				p = (AVLTreeNode *)r->left;
				r->left = p->right;
				p->right = r;
				s->right = p->left;
				p->left = s;
			}
			s->unbalance = (p->unbalance == a) ? -a : 0;
			r->unbalance = (p->unbalance == -a) ? a : 0;
			p->unbalance = 0;
		}
		// finalization
		*t = p;
	}
	ASSERT(root);
	return nativeToHandle(retval);
#endif
}

Object *AVLTree::removeR(Object *key, BinTreeNode *&node, int &change, int cmp)
{
	if (node == NULL) {
		change = 0;
		return NULL;
	}

	Object *found = NULL;
	int decrease = 0;

	int result;
	if (!cmp) {
		result = compareObjects(key, node->key);
		if (result < 0) {
			result = -1;
		} else if (result > 0) {
			result = 1;
		}
	} else if (cmp < 0) {
		result = (node->left == NULL) ? 0 : -1;
	} else {
		result = (node->right == NULL) ? 0 : 1;
	}
	
	if (result) {
		found = removeR(key, (result < 0) ? node->left : node->right, change, cmp);
		if (!found) return NULL;
		decrease = result * change;
	} else  {
		found = node->key;
		
		/*
		 *	Same logic as in BinaryTree::remove()
		 */

		if (!node->left && !node->right) {
			deleteNode(node);
			node = NULL;
			change = 1;
			return found;
		} else if (!node->left || !node->right) {
			AVLTreeNode *toDelete = (AVLTreeNode *)node;
			node = node->right ? node->right : node->left;
			change = 1;
			toDelete->left = toDelete->right = NULL;
			deleteNode(toDelete);
			return found;
		} else {
			node->key = removeR(key, node->right, decrease, -1);
		}
	}

	((AVLTreeNode *)node)->unbalance -= decrease;

	if (decrease) {
		if (((AVLTreeNode *)node)->unbalance) {
			change = 0;
			int a;
			AVLTreeNode *r = NULL;
			if (((AVLTreeNode *)node)->unbalance < -1) {
				a = -1;
				r = (AVLTreeNode *)node->left;
			} else if (((AVLTreeNode *)node)->unbalance > 1) {
				a = 1;
				r = (AVLTreeNode *)node->right;
			} else {
				a = 0;
			}
			if (a) {
				/*
				 *	If r->unbalance == 0 do also a single rotation.
				 *	This case cant occure with insert operations.
				 */
				if (r->unbalance == -a) {
					/*
					 *	double rotation.
					 *	See insert
					 */
					AVLTreeNode *p;
					if (a > 0) {
						p = (AVLTreeNode *)r->left;
						r->left = p->right;
						p->right = r;
						node->right = p->left;
						p->left = node;
					} else {
						p = (AVLTreeNode *)r->right;
						r->right = p->left;
						p->left = r;
						node->left = p->right;
						p->right = node;
					}
					((AVLTreeNode*)node)->unbalance = (p->unbalance == a) ? -a: 0;
					r->unbalance = (p->unbalance == -a) ? a: 0;
					p->unbalance = 0;
					node = p;
					change = 1;
				} else {
					/*
					 *	single rotation
					 *	Height of tree changes if r is/was unbalanced
					 */
					change = r->unbalance?1:0;
					if (a > 0) {
						node->right = r->left;
						r->left = node;
						r->unbalance--;
					} else {
						node->left = r->right;
						r->right = node;
						r->unbalance++;
					}
					((AVLTreeNode *)node)->unbalance = - r->unbalance;
					node = r;
				}
			}
		} else {
			/*
			 *	Tree has become more balanced
			 */
			change = 1;
		}
	} else {
		change = 0;
	}

	return found;
}

Object *AVLTree::remove(ObjHandle h)
{
	if (!validHandle(h)) return NULL;
	BinTreeNode *n = handleToNative(h);
	Object *o = n->key;
	int change;
	Object *obj = removeR(n->key, root, change, 0);
		ASSERT(obj == o);
	ecount--;
	return o;
}

/*
 *   Class Set
 */

Set::Set(BuildCtorArg a) : AVLTree(a)
{
}

Set::Set(bool oo)
: AVLTree(oo)
{
}

#ifdef HAVE_HT_OBJECTS
bool Set::instanceOf(ObjectID id) const
{
	return (id == getObjectID()) || AVLTree::instanceOf(id);
}

ObjectID Set::getObjectID() const
{
	return OBJID_SET;
}
#endif

void Set::intersectWith(Set *b)
{
	foreach(Object, elem, *this,
		if (!b->contains(elem)) delObj(elem);
	);
}

void Set::unionWith(Set *b)
{
	foreach(Object, elem, *b,
		if (!contains(elem)) insert(own_objects ? elem->clone() : elem);
	);
}

/*
 *
 */

KeyValue::KeyValue(BuildCtorArg a) : Object(a)
{
}

KeyValue::KeyValue(Object *aKey, Object *aValue)
{
	mKey = aKey;
	mValue = aValue;
}

KeyValue::~KeyValue()
{
	mKey->done();
	delete mKey;
//	mValue->done();
	delete mValue;
}

Object *KeyValue::clone() const
{
	return new KeyValue(mKey->clone(), mValue ? mValue->clone() : NULL);
}

int KeyValue::compareTo(const Object *obj) const
{
	return mKey->compareTo(((KeyValue*)obj)->mKey);
}

int KeyValue::toString(char *buf, int buflen) const
{
	return ht_snprintf(buf, buflen, "[Key: %y; Value: %y]", mKey, mValue);
}

#ifdef HAVE_HT_OBJECTS
bool KeyValue::instanceOf(ObjectID id) const
{
	return id == getObjectID();
}

void KeyValue::load(ObjectStream &s)
{
	GET_OBJECT(s, mKey);
	GET_OBJECT(s, mValue);
}

ObjectID KeyValue::getObjectID() const
{
	return OBJID_KEYVALUE;
}

void KeyValue::store(ObjectStream &s) const
{
	PUT_OBJECT(s, mKey);
	PUT_OBJECT(s, mValue);
}

#endif

/*
 *	Class SInt
 */

SInt::SInt(BuildCtorArg a) : Object(a)
{
}

SInt::SInt(signed int i)
{
	value = i;
}

Object *SInt::clone() const
{
	return new SInt(value);
}

int SInt::compareTo(const Object *obj) const
{
	SInt *s = (SInt*)obj;
	return value - s->value;
}

int SInt::toString(char *buf, int buflen) const
{
	return ht_snprintf(buf, buflen, "%d", value);
}

#ifdef HAVE_HT_OBJECTS

bool SInt::instanceOf(ObjectID id) const
{
	return id == getObjectID();
}

void SInt::load(ObjectStream &s)
{
	GET_INT32D(s, value);
}

ObjectID SInt::getObjectID() const
{
	return OBJID_SINT;
}

void SInt::store(ObjectStream &s) const
{
	PUT_INT32D(s, value);
}

#endif

/*
 *	A signed Integer (64-bit)
 */
#ifdef HAVE_HT_OBJECTS
SInt64::SInt64(BuildCtorArg) {}
#endif

SInt64::SInt64(sint64 i)
{
	value = i;
}

Object *SInt64::clone() const
{
	return new SInt64(value);
}

int SInt64::compareTo(const Object *obj) const
{
	SInt64 *u = (SInt64*)obj;

	if (value < u->value) {
		return -1;
	} else if (value > u->value) {
		return 1;
	} else {
		return 0;
	}
}

int SInt64::toString(char *buf, int buflen) const
{
	return ht_snprintf(buf, buflen, "%qd", &value);
}

#ifdef HAVE_HT_OBJECTS
bool SInt64::instanceOf(ObjectID id) const
{
	return id == getObjectID();
}

void SInt64::load(ObjectStream &s)
{
	GET_INT64D(s, value);
}

ObjectID SInt64::getObjectID() const
{
	return OBJID_SINT64;
}

void SInt64::store(ObjectStream &s) const
{
	PUT_INT64D(s, value);
}
#endif

/*
 *	Class UInt
 */

UInt::UInt(BuildCtorArg a) : Object(a)
{
}

UInt::UInt(unsigned int i)
{
	value = i;
}

Object *UInt::clone() const
{
	return new UInt(value);
}

int UInt::compareTo(const Object *obj) const
{
	UInt *u = (UInt*)obj;

	if (value < u->value) {
		return -1;
	} else if (value > u->value) {
		return 1;
	} else {
		return 0;
	}
}

int UInt::toString(char *buf, int buflen) const
{
	return ht_snprintf(buf, buflen, "%u", value);
}

#ifdef HAVE_HT_OBJECTS

bool UInt::instanceOf(ObjectID id) const
{
	return id == getObjectID();
}

void UInt::load(ObjectStream &s)
{
	GET_INT32D(s, value);
}

ObjectID UInt::getObjectID() const
{
	return OBJID_UINT;
}

void UInt::store(ObjectStream &s) const
{
	PUT_INT32D(s, value);
}

#endif

/*
 *	A unsigned Integer (64-bit)
 */
#ifdef HAVE_HT_OBJECTS
UInt64::UInt64(BuildCtorArg) {}
#endif

UInt64::UInt64(uint64 i)
{
	value = i;
}

Object *UInt64::clone() const
{
	return new UInt64(value);
}

int UInt64::compareTo(const Object *obj) const
{
	UInt64 *u = (UInt64*)obj;

	if (value < u->value) {
		return -1;
	} else if (value > u->value) {
		return 1;
	} else {
		return 0;
	}
}

int UInt64::toString(char *buf, int buflen) const
{
	return ht_snprintf(buf, buflen, "%qu", &value);
}

#ifdef HAVE_HT_OBJECTS
bool UInt64::instanceOf(ObjectID id) const
{
	return id == getObjectID();
}

void UInt64::load(ObjectStream &s)
{
	GET_INT64D(s, value);
}

ObjectID UInt64::getObjectID() const
{
	return OBJID_UINT64;
}

void UInt64::store(ObjectStream &s) const
{
	PUT_INT64D(s, value);
}

#endif

/*
 *	A floating-point number	(FIXME: no portable storage yet)
 */

#ifdef HAVE_HT_OBJECTS
Float::Float(BuildCtorArg)
{
}
#endif

Float::Float(double d)
{
	value = d;
}

Object *Float::clone() const
{
	return new Float(value);
}

int Float::compareTo(const Object *obj) const
{
// FIXME: do we want to compare for equality using some error term epsilon ?
	Float *f = (Float*)obj;

	if (value < f->value) {
		return -1;
	} else if (value > f->value) {
		return 1;
	} else {
		return 0;
	}
}

int Float::toString(char *buf, int buflen) const
{
	return ht_snprintf(buf, buflen, "%f", value);
}

#ifdef HAVE_HT_OBJECTS
bool Float::instanceOf(ObjectID id) const
{
	return id == getObjectID();
}

//	virtual	void		load(ObjectStream &s);

ObjectID Float::getObjectID() const
{
	return OBJID_FLOAT;
}

//	virtual	void		store(ObjectStream &s) const;

#endif

/*
 *	Class Pointer
 */

Pointer::Pointer(void *p)
{
	value = p;
}

/**
 *	A memory area.
 */

MemArea::MemArea(BuildCtorArg)
{
}

MemArea::MemArea(const void *p, uint s, bool d)
{
	duplicate = d;
	size = s;
	if (duplicate) {
		ptr = malloc(size);
		if (!ptr) throw std::bad_alloc();
		memcpy(ptr, p, size);
	} else {
		// FIXME: un-const'ing p
		ptr = (void*)p;
	}
}

MemArea::~MemArea()
{
	if (duplicate) free(ptr);
}

Object *MemArea::clone() const
{
	return new MemArea(ptr, size, true);
}

int MemArea::compareTo(const Object *obj) const
{
	const MemArea *a = this;
	const MemArea *b = (const MemArea*)obj;
	if (a->size != b->size) return a->size - b->size;
	return memcmp(a->ptr, b->ptr, a->size);
}

int MemArea::toString(char *buf, int buflen) const
{
	throw new NotImplementedException(HERE);
}

#ifdef HAVE_HT_OBJECTS
bool MemArea::instanceOf(ObjectID id) const
{
	return (id == getObjectID()) || Object::instanceOf(id);
}

void MemArea::load(ObjectStream &s)
{
	GET_INT32D(s, size);
	ptr = malloc(size);
	if (!ptr) throw std::bad_alloc();
	GET_BINARY(s, ptr, size);
}

ObjectID MemArea::getObjectID() const
{
	return OBJID_MEMAREA;
}

void MemArea::store(ObjectStream &s) const
{
	PUT_INT32D(s, size),
	PUT_BINARY(s, ptr, size);
}

#endif

/*
 *	sorter
 */

static void quickSortR(List &list, int l, int r)
{
	int m = (l+r)/2;
	int L = l;
	int R = r;
	Object *c = list[m];
	do {
		while ((l<=r) && (list.compareObjects(list[l], c)<0)) l++;
		while ((l<=r) && (list.compareObjects(list[r], c)>0)) r--;
		if (l<=r) {
			list.swap(list.findByIdx(l), list.findByIdx(r));
			l++;
			r--;
		}
	} while (l<r);
	if (L<r) quickSortR(list, L, r);
	if (l<R) quickSortR(list, l, R);
}

bool quickSort(List &l)
{
	int c = l.count();
	if (c) quickSortR(l, 0, c-1);
	return true;
}

/*
 *	Module Init/Done
 */

BUILDER(OBJID_OBJECT, Object);

BUILDER(OBJID_ARRAY, Array);
BUILDER(OBJID_STACK, Stack);

BUILDER(OBJID_BINARY_TREE, BinaryTree);
BUILDER(OBJID_AVL_TREE, AVLTree);
BUILDER(OBJID_SET, Set);

BUILDER(OBJID_LINKED_LIST, LinkedList);
BUILDER(OBJID_QUEUE, Queue);
//BUILDER(OBJID_DBL_LINKED_LIST, DblLinkedList);

BUILDER(OBJID_KEYVALUE, KeyValue);
BUILDER(OBJID_SINT, SInt);
BUILDER(OBJID_UINT, UInt);
BUILDER(OBJID_MEMAREA, MemArea);

BUILDER(OBJID_STRING, String);
BUILDER(OBJID_ISTRING, IString);

bool initData()
{
	registerAtom(OBJID_AUTO_COMPARE, (void*)&autoCompare);

	REGISTER(OBJID_OBJECT, Object);

	REGISTER(OBJID_ARRAY, Array);
	REGISTER(OBJID_STACK, Stack);

	REGISTER(OBJID_BINARY_TREE, BinaryTree);
	REGISTER(OBJID_AVL_TREE, AVLTree);
	REGISTER(OBJID_SET, Set);

	REGISTER(OBJID_LINKED_LIST, LinkedList);
	REGISTER(OBJID_QUEUE, Queue);
//	REGISTER(OBJID_DBL_LINKED_LIST, DblLinkedList);

	REGISTER(OBJID_KEYVALUE, KeyValue);
	REGISTER(OBJID_SINT, SInt);
	REGISTER(OBJID_UINT, UInt);
	REGISTER(OBJID_MEMAREA, MemArea);

	REGISTER(OBJID_STRING, String);
	REGISTER(OBJID_ISTRING, IString);
	return true;
} 
 
void doneData()
{
	unregisterAtom(OBJID_AUTO_COMPARE);

	UNREGISTER(OBJID_OBJECT, Object);

	UNREGISTER(OBJID_ARRAY, Array);
	UNREGISTER(OBJID_STACK, Stack);

	UNREGISTER(OBJID_BINARY_TREE, BinaryTree);
	UNREGISTER(OBJID_AVL_TREE, AVLTree);
	UNREGISTER(OBJID_SET, Set);

	UNREGISTER(OBJID_LINKED_LIST, LinkedList);
	UNREGISTER(OBJID_QUEUE, Queue);
//	UNREGISTER(OBJID_DBL_LINKED_LIST, DblLinkedList);

	UNREGISTER(OBJID_KEYVALUE, KeyValue);
	UNREGISTER(OBJID_SINT, SInt);
	UNREGISTER(OBJID_UINT, UInt);
	UNREGISTER(OBJID_MEMAREA, MemArea);

	UNREGISTER(OBJID_STRING, String);
	UNREGISTER(OBJID_ISTRING, IString);
}
