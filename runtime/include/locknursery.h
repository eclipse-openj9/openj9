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

#ifndef locknursery_h
#define locknursery_h

#if defined(J9VM_THR_LOCK_NURSERY)

#define JAVA_LANG_CLASS_CLASSNAME 	"java/lang/Class"
#define JAVA_LANG_STRING_CLASSNAME 	"java/lang/String"

#define LOCKNURSERY_ALGORITHM_ALL_BUT_ARRAY														0
#define LOCKNURSERY_ALGORITHM_ALL_INHERIT														1
#define LOCKNURSERY_ALGORITHM_MINIMAL_WITH_SYNCHRONIZED_METHODS									2
#define LOCKNURSERY_ALGORITHM_MINIMAL_AND_SYNCHRONIZED_METHODS_AND_INNER_LOCK_CANDIDATES		3

#define LOCKNURSERY_HASHTABLE_ENTRY_MASK	0x1
#define REMOVE_LOCKNURSERY_HASHTABLE_ENTRY_MASK(entry) (((UDATA) entry)& ~LOCKNURSERY_HASHTABLE_ENTRY_MASK )
#define SET_LOCKNURSERY_HASHTABLE_ENTRY_MASK(entry) (((UDATA) entry)| LOCKNURSERY_HASHTABLE_ENTRY_MASK )
#define IS_LOCKNURSERY_HASHTABLE_ENTRY_MASKED(entry) (((UDATA) entry) & LOCKNURSERY_HASHTABLE_ENTRY_MASK )

#if !defined(J9VM_OUT_OF_PROCESS)
#define ROM_FIELD_WALK_MODIFIERS(javavm) ((javavm)->romFieldsWalkModifiers)
#else /* !defined(J9VM_OUT_OF_PROCESS) */
#define ROM_FIELD_WALK_MODIFIERS(javavm) dbgReadU32((U_32*)&(javavm)->romFieldsWalkModifiers)
#endif /* !defined(J9VM_OUT_OF_PROCESS) */
#else /* defined(J9VM_THR_LOCK_NURSERY) */
#define ROM_FIELD_WALK_MODIFIERS(javavm) 0

#endif /* defined(J9VM_THR_LOCK_NURSERY) */

#endif /* locknursery_h */
