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

#include "CheckEngine.hpp"
#include "CheckClassLoaders.hpp"
#include "ClassLoaderIterator.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"

GC_Check *
GC_CheckClassLoaders::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckClassLoaders *check = (GC_CheckClassLoaders *) forge->allocate(sizeof(GC_CheckClassLoaders), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckClassLoaders(javaVM, engine);
	}
	return check;
}

void
GC_CheckClassLoaders::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckClassLoaders::check()
{
	GC_ClassLoaderIterator classLoaderIterator(_javaVM->classLoaderBlocks);
	J9ClassLoader *classLoader;

	while((classLoader = classLoaderIterator.nextSlot()) != NULL) {
		if (J9_GC_CLASS_LOADER_DEAD != (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD) ) {
			J9Object **slot = J9GC_J9CLASSLOADER_CLASSLOADEROBJECT_EA(classLoader);
			if (_engine->checkSlotPool(_javaVM, slot, classLoader) != J9MODRON_SLOT_ITERATOR_OK){
				return;
			}
		}
	}
}

void
GC_CheckClassLoaders::print()
{
	J9Pool *pool = _javaVM->classLoaderBlocks;
	GC_ClassLoaderIterator classLoaderIterator(pool);
	J9ClassLoader *classLoader;
	PORT_ACCESS_FROM_PORT(_portLibrary);

	j9tty_printf(PORTLIB, "<gc check: Start scan classLoaderBlocks (%p)>\n", pool);
	while((classLoader = classLoaderIterator.nextSlot()) != NULL) {
		j9tty_printf(PORTLIB, "  <classLoader (%p)>\n", classLoader);
		j9tty_printf(PORTLIB, "    <flags=%zu, classLoaderObject=%p>\n", classLoader->gcFlags, J9GC_J9CLASSLOADER_CLASSLOADEROBJECT(classLoader));
	}
	j9tty_printf(PORTLIB, "<gc check: End scan classLoaderBlocks (%p)>\n", pool);
}
