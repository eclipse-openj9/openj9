
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

#if !defined(ARRAY_COPY_INTERFACE_H_)
#define ARRAY_COPY_INTERFACE_H_



#include "j9.h"
#include "j9cfg.h"
#include "j9modron.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Signature for a generic reference array copy using array indices
 * @{
 */
typedef I_32 (*J9ReferenceArrayCopyIndexSig)(J9VMThread *, J9IndexableObject *, J9IndexableObject *, I_32, I_32, I_32);
/** @} */

/**
 * Table of reference array copy tables.
 * Each field is a pointer to a table containing one function for each of
 * the difference write barrier types.
 * @see J9WriteBarrierType
 */
typedef struct J9ReferenceArrayCopyTable {
	J9ReferenceArrayCopyIndexSig referenceArrayCopyIndex;
	J9ReferenceArrayCopyIndexSig backwardReferenceArrayCopyIndex[j9gc_modron_wrtbar_count];
	J9ReferenceArrayCopyIndexSig forwardReferenceArrayCopyWithCheckIndex[j9gc_modron_wrtbar_count];
	J9ReferenceArrayCopyIndexSig forwardReferenceArrayCopyWithoutCheckIndex[j9gc_modron_wrtbar_count];
} J9ReferenceArrayCopyTable;

extern void initializeReferenceArrayCopyTable(J9ReferenceArrayCopyTable *table);

#ifdef __cplusplus
} /* extern "C" { */
#endif /* __cplusplus */

#endif /* ARRAY_COPY_INTERFACE_H_ */
