/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef J9SRPHASHTABLE_H
#define J9SRPHASHTABLE_H

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C" {
#endif


/* DO NOT DIRECTLY INCLUDE THIS FILE! */
/* Include srphashtable_api.h instead */


#include "j9comp.h"
#include "j9port.h"
#include "simplepool_api.h"


struct J9SRPHashTable; /* Forward struct declaration */
struct J9SRPHashTableInternal; /* Forward struct declaration */
typedef UDATA (*J9SRPHashTableHashFn) (void *key, void *userData); /* Forward struct declaration */
typedef UDATA (*J9SRPHashTableEqualFn) (void *existingEntry, void *key, void *userData); /* Forward struct declaration */
typedef void (*J9SRPHashTablePrintFn) (J9PortLibrary *portLibrary, void *entry, void *userData); /* Forward struct declaration */
typedef UDATA (*J9SRPHashTableDoFn) (void *entry, void *userData); /* Forward struct declaration */
typedef struct J9SRPHashTable {
    const char* tableName;
    struct J9SRPHashTableInternal* srpHashtableInternal;
    UDATA  ( *hashFn)(void *key, void *userData) ;
    UDATA  ( *hashEqualFn)(void *existingEntry, void *key, void *userData) ;
    void  ( *printFn)(J9PortLibrary *portLibrary, void *key, void *userData) ;
    struct J9PortLibrary * portLibrary;
    void* functionUserData;
    UDATA flags;
} J9SRPHashTable;

typedef struct J9SRPHashTableInternal {
    U_32 tableSize;
    U_32 numberOfNodes;
    U_32 entrySize;
    U_32 nodeSize;
    U_32 flags;
    J9SRP nodes;
    J9SRP nodePool;
} J9SRPHashTableInternal;

#define J9SRPHASHTABLEINTERNAL_NODES(base) SRP_GET((base)->nodes, J9SRP*)
#define J9SRPHASHTABLEINTERNAL_NODEPOOL(base) SRP_GET((base)->nodePool, struct J9SimplePool*)

#ifdef __cplusplus
}
#endif

#endif /* J9SRPHASHTABLE_H */
