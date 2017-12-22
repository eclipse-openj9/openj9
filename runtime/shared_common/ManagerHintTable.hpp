/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(MANAGER_HINT_TABLE_H_INCLUDED)
#define MANAGER_HINT_TABLE_H_INCLUDED

/* @ddr_namespace: default */
#include "j9.h"
#include "j9protos.h"

class SH_CompositeCache;

class ManagerHintTable
{
public:
	ManagerHintTable() :
		_hinthash(NULL) /*, _lock(NULL) */{
	}
	bool initialize(J9PortLibrary* portLib);
	void destroy();
	bool addHint(J9VMThread* currentThread, UDATA hashValue, SH_CompositeCache* cachelet);
	void removeHint(J9VMThread* currentThread, UDATA hashValue, SH_CompositeCache* cachelet);
	SH_CompositeCache* findHint(J9VMThread* currentThread, UDATA hashValue);

private:
	struct HintItem {
		UDATA _hashValue;
		SH_CompositeCache* _cachelet;

		HintItem(UDATA hashValue_, SH_CompositeCache* cachelet_) :
			_hashValue(hashValue_), _cachelet(cachelet_) {
		}
	};

	J9HashTable* _hinthash;

	static UDATA hashFn(void* item, void *userData);
	static UDATA hashEqualFn(void* left, void* right, void *userData);

	/* to populate a hint:
	 * while (findHint()) {
	 *   removeHint()
	 *   add hint to real table
	 * }
	 */
};

#endif /* MANAGER_HINT_TABLE_H_INCLUDED */
