/* 
 *	HT Editor
 *	atom.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf
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

#include "atom.h"
#include "data.h"

#include <cstring>

//static 
AVLTree atoms(true);

class Atom: public Object {
public:
	uint id;
	void *value;

	Atom(uint i, void *v)
	{
		id = i;
		value = v;
	}

	int compareTo(const Object *obj) const
	{
		Atom *a = (Atom*)obj;
		return id - a->id;
	}
};

void *getAtomValue(uint id)
{
	if (id != invalid_atom_id) {
		Atom a(id, NULL);
		Atom *f = (Atom*)atoms.get(atoms.find(&a));
		if (f) return f->value;
	}
	return NULL;
}

uint getAtomId(void *value)
{
	if (value) {
		foreach(Atom, a, atoms,
			if (a->value == value) return a->id;
		);
	}
	return invalid_atom_id;
}

#include "debug.h"
bool registerAtom(uint id, void *value)
{
	ASSERT(value);
	atoms.insert(new Atom(id, value));
	return true;
}

bool unregisterAtom(uint id)
{
	Atom a(id, NULL);
	atoms.delObj(&a);
	return true;
}

/*
 *	Module Init/Done
 */
 
bool initAtom()
{
	return true;
} 
 
void doneAtom()
{
}
