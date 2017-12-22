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

#ifndef util_internal_h
#define util_internal_h

/**
* @file util_internal.h
* @brief Internal prototypes used within the UTIL module.
*
* This file contains implementation-private function prototypes and
* type definitions for the UTIL module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "util_api.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ---------------- fltdmath.c ---------------- */

/**
* @brief
* @param *dp
* @param s
* @return void
*/
void scaleUpDouble(double *dp, int s);


/**
* @brief
* @param *lp
* @param *linp
* @param e
* @return void
*/
void shiftLeft64(U_64 *lp, U_64 *linp, int e);


/* ---------------- j9list.c ---------------- */

/**
* @typedef
* @struct
*/
typedef struct node_struct {
	struct node_struct *next;
	char *key;
	void *data;
} node;

/**
* @typedef
* @struct
*/
typedef struct lst {
	node *first;
	J9PortLibrary *portLibrary;
} j9list;

/**
* @brief
* @param *aList
* @return void
*/
void list_dump(j9list *aList);


/**
* @brief
* @param *aList
* @param *key
* @return node *
*/
node *list_find(j9list *aList, char *key);


/**
* @brief
* @param *aList
* @return node *
*/
node *list_first(j9list *aList);


/**
* @brief
* @param *aList
* @param *key
* @param *data
* @return node *
*/
node *list_insert(j9list *aList, char *key, void *data);


/**
* @brief
* @param *aList
* @return BOOLEAN
*/
BOOLEAN list_isEmpty(j9list *aList);


/**
* @brief
* @param *aList
* @return void
*/
void list_kill(j9list *aList);


/**
* @brief
* @param *portLib
* @return j9list *
*/
j9list *list_new(J9PortLibrary *portLib);


/**
* @brief
* @param *aNode
* @return node *
*/
node *list_next(node *aNode);


/**
* @brief
* @param *aList
* @param *key
* @return node *
*/
node *list_no_case_find(j9list *aList, char *key);


/**
* @brief
* @param *aList
* @param *aNode
* @return void *
*/
void *list_remove(j9list *aList, node *aNode);


/**
* @brief
* @param *aNode
* @return void *
*/
void *node_data(node *aNode);


/**
* @brief
* @param *aNode
* @return char *
*/
char *node_key(node *aNode);

/* ---------------- strrchr.c ---------------- */

/* ---------------- thrinfo.c ---------------- */
/**
 * Wrapper for @ref monitorTablePeek. Gets the J9ThreadAbstractMonitor associated with the J9ObjectMonitor 
 * returned by @ref monitorTablePeek. 
 * 
 * @brief
 * @param vm				the J9JavaVM
 * @param targetVMThread	Used solely by the multi-tenant JVM: identifies the TenantContext of the object, 
 * 								which is needed to determine which monitor table to search
 * @param object			the object who's monitor we are looking for
 * @return J9ThreadAbstractMonitor *
 */
J9ThreadAbstractMonitor *
monitorTablePeekMonitor(J9JavaVM *vm, J9VMThread *targetVMThread, j9object_t object);

/**
 * Search the monitor tables in vm->monitorTableList for the inflated monitor corresponding to an object.
 * Similar to monitorTableAt(), but doesn't add the monitor if it isn't found in the hashtable.
 *
 * This function may block on vm->monitorTableMutex.
 * This function can work out-of-process.
 *
 * @param[in] vm the JavaVM. For out-of-process: may be a local or target pointer.
 * vm->monitorTable must be a target value.
 * @param targetVMThread	Used solely by the multi-tenant JVM: identifies the TenantContext of the object, which is needed 
 * 								to determine which monitor table to search	
 * @param[in] object the object. For out-of-process: a target pointer.
 * @returns a J9ObjectMonitor from the monitor hashtable
 * @retval NULL There is no corresponding monitor in vm->monitorTable.
 *
 * @see monitorTablePeekMonitor
 */
J9ObjectMonitor *
monitorTablePeek(J9JavaVM *vm, J9VMThread *targetVMThread, j9object_t object);

#ifdef __cplusplus
}
#endif

#endif /* util_internal_h */

