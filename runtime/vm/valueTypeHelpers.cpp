/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "j9.h"
#include "ut_j9vm.h"
#include "ObjectAccessBarrierAPI.hpp"

extern "C" {
void
defaultValueWithUnflattenedFlattenables(J9VMThread *currentThread, J9Class *clazz, j9object_t instance)
{
        J9FlattenedClassCacheEntry * entry = NULL;
        J9Class * entryClazz = NULL;
        UDATA length = clazz->flattenedClassCache->numberOfEntries;
        UDATA const objectHeaderSize = J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
        for (UDATA index = 0; index < length; index++) {
                entry = J9_VM_FCC_ENTRY_FROM_CLASS(clazz, index);
                entryClazz = entry->clazz;
                if (J9_ARE_NO_BITS_SET(J9ClassIsFlattened, entryClazz->classFlags)) {
                        if (entry->offset == UDATA_MAX) {
                                J9Class *definingClass = NULL;
                                J9ROMFieldShape *field = NULL;
                                J9ROMFieldShape *entryField = entry->field;
                                J9UTF8 *name = J9ROMFIELDSHAPE_NAME(entryField);
                                J9UTF8 *signature = J9ROMFIELDSHAPE_SIGNATURE(entryField);
                                entry->offset = instanceFieldOffset(currentThread, clazz, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, 0);
                                Assert_VM_notNull(field);
                        }
                        MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
                        objectAccessBarrier.inlineMixedObjectStoreObject(currentThread, 
                                                                                instance, 
                                                                                entry->offset + objectHeaderSize, 
                                                                                entryClazz->flattenedClassCache->defaultValue, 
                                                                                false);
                }
        }
}
} /* extern "C" */
