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


#include <string.h>
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "util_internal.h"

#if (defined(J9VM_INTERP_NATIVE_SUPPORT))  /* File Level Build Flags */

#define LOW_BIT_SET(value) ((UDATA) (value) & (UDATA) 1)
#define SET_LOW_BIT(value) ((UDATA) (value) | (UDATA) 1)
#define REMOVE_LOW_BIT(value) ((UDATA) (value) & (UDATA) -2)
#define DETERMINE_BUCKET(value, start, buckets) (((((UDATA)(value) - (UDATA)(start)) >> (UDATA) 9) * (UDATA)sizeof(UDATA)) + (UDATA)(buckets))

J9JITExceptionTable* hash_jit_artifact_search(J9JITHashTable *table, UDATA searchValue) {
   J9JITExceptionTable *entry, **bucket;
   if (searchValue >= table->start && searchValue < table->end) {
      /* The search value is in this hash table */
      bucket = (J9JITExceptionTable**)DETERMINE_BUCKET(searchValue, table->start, table->buckets);
      if (*bucket) {
         /* The bucket for this search value is not empty */
         if (LOW_BIT_SET(*bucket)) {
            /* The bucket consists of a single low-tagged J9JITExceptionTable pointer */
            entry = *bucket;
         }
         else {
            /* The bucket consists of an array of J9JITExceptionTable pointers,
             * the last of which is low-tagged */

            /* Search all but the last entry in the array */
            bucket = (J9JITExceptionTable**)(*bucket);
            for ( ; ; bucket++) {
               entry = *bucket;
               if (LOW_BIT_SET(entry)) {
                  break;
               }
               if (searchValue >= entry->startPC && searchValue < entry->endWarmPC)
                  return entry;
               if (entry->startColdPC && searchValue >= entry->startColdPC && searchValue < entry->endPC)
                  return entry;
            }
         }

         /* Search the last (or only) entry in the bucket, which is low-tagged */
         entry = (J9JITExceptionTable*)REMOVE_LOW_BIT(entry);
         if (searchValue >= entry->startPC && searchValue < entry->endWarmPC)
            return entry;
         if (entry->startColdPC && searchValue >= entry->startColdPC && searchValue < entry->endPC)
            return entry;
      }
   }
   return NULL;
}


J9JITExceptionTable* jit_artifact_search(J9AVLTree *tree, UDATA searchValue) {
        /* find the right hash table to look in */
        J9JITHashTable *table = (J9JITHashTable*)avl_search(tree, searchValue);
        if (table) {
                /* return the result of looking in the correct hash table */
                return hash_jit_artifact_search(table, searchValue);
        }
        return NULL;
}


#endif /* J9VM_INTERP_NATIVE_SUPPORT */ /* End File Level Build Flags */
