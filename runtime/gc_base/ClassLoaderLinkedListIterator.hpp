/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#if !defined(CLASSLOADERLINKEDLISTITERATOR_HPP_)
#define CLASSLOADERLINKEDLISTITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "omrlinkedlist.h"

#include "ClassLoaderManager.hpp"

/**
 * Iterate over all slots in a class which contain an object reference
 */
class GC_ClassLoaderLinkedListIterator {
public:
protected:
private:
	MM_ClassLoaderManager *_classLoaderManager; /**< Pointer to the classLoader manager which holds the linked list of classloaders */
	J9ClassLoader *_currentLoader; /**< Current classloader in the iterator */
	J9ClassLoader *_nextLoader; /**< Prefetched next classloader in the linked list */
	
public:
	GC_ClassLoaderLinkedListIterator(MM_EnvironmentBase *env, MM_ClassLoaderManager *classLoaderManager) :
		_classLoaderManager(classLoaderManager),
		_currentLoader(NULL),
		_nextLoader(J9_LINEAR_LINKED_LIST_START_DO(_classLoaderManager->_classLoaders))
	{};

	J9ClassLoader *nextSlot();
	void removeSlot();
};

#endif /* CLASSLOADERLINKEDLISTITERATOR_HPP_ */

