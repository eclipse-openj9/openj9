
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

/**
 * @file
 * @ingroup GC_Structs
 */

#include "ClassLoaderClassesIterator.hpp"

#include "GCExtensionsBase.hpp"

GC_ClassLoaderClassesIterator::GC_ClassLoaderClassesIterator(MM_GCExtensionsBase *extensions, J9ClassLoader *classLoader) :
	_javaVM((J9JavaVM* )extensions->getOmrVM()->_language_vm)
	,_classLoader(classLoader)
	,_vmSegmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS)
	,_vmClassSlotIterator((J9JavaVM* )extensions->getOmrVM()->_language_vm)
	,_mode(TABLE_CLASSES)
{

	if ((classLoader->flags & J9CLASSLOADER_ANON_CLASS_LOADER) != 0) {
		_mode = ANONYMOUS_CLASSES;
	}
	_nextClass = firstClass();
}

J9Class *
GC_ClassLoaderClassesIterator::nextSystemClass() 
{
	J9Class * result = NULL;
	J9Class ** slot = NULL;
	while (NULL != (slot = _vmClassSlotIterator.nextSlot())) {
		result = *slot;
		if (NULL != result) {
			break;
		}
	}
	return result;
}

J9Class *
GC_ClassLoaderClassesIterator::firstClass()
{
	J9Class * result;
	if (ANONYMOUS_CLASSES == _mode) {
		result = nextAnonymousClass();
	} else {
		result = _javaVM->internalVMFunctions->hashClassTableStartDo(_classLoader, &_walkState);
		if ( (NULL == result) && switchToSystemMode() ) {
			result = nextSystemClass();
		}
	}
	return result;
}

J9Class *
GC_ClassLoaderClassesIterator::nextAnonymousClass()
{
	J9Class * result = NULL;
	J9MemorySegment *segment;
	if (NULL != (segment = _vmSegmentIterator.nextSegment())) {
		GC_ClassHeapIterator classHeapIterator(_javaVM, segment);
		result = classHeapIterator.nextClass();
	}
	return result;
}


J9Class *
GC_ClassLoaderClassesIterator::nextTableClass()
{
	J9Class * result = _javaVM->internalVMFunctions->hashClassTableNextDo(&_walkState);
	if ( (NULL == result) && switchToSystemMode() ) {
		result = nextSystemClass();
	}
	return result;
}


bool 
GC_ClassLoaderClassesIterator::switchToSystemMode()
{
	bool isSystemClassLoader = (_classLoader == _javaVM->systemClassLoader);
	if (isSystemClassLoader) {
		_mode = SYSTEM_CLASSES;
	}
	return isSystemClassLoader;
}

J9Class *
GC_ClassLoaderClassesIterator::nextClass()
{
	J9Class * result = _nextClass;
	
	if (NULL != result) {
		if (ANONYMOUS_CLASSES == _mode) {
			_nextClass = nextAnonymousClass();
		} else {
			if ( (result->classLoader == _classLoader) && (NULL != result->arrayClass) ) {
				/* this class is defined in the loader, so follow its array classes */
				_nextClass = result->arrayClass;
			} else if (TABLE_CLASSES == _mode) {
				_nextClass = nextTableClass();
			} else {
				_nextClass = nextSystemClass();
			}
		}
	}
	
	return result;
}
