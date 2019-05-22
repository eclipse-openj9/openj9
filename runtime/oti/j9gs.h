/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

/** Masks used for gsParameters->flags */
#define J9PORT_GS_ENABLED ((uint32_t) 0x1)
#define J9PORT_GS_INITIALIZED ((uint32_t) 0x2)

/** Macros to check if GS is initialized/enabled */
#define IS_GS_INITIALIZED(gsParameters) (J9PORT_GS_INITIALIZED == ((gsParameters)->flags & J9PORT_GS_INITIALIZED))
#define IS_GS_ENABLED(gsParameters) (J9PORT_GS_ENABLED == ((gsParameters)->flags & J9PORT_GS_ENABLED))
#define IS_THREAD_GS_INITIALIZED(vmThread) (IS_GS_INITIALIZED(&(vmThread)->gsParameters))
#define IS_THREAD_GS_ENABLED(vmThread) (IS_GS_ENABLED(&(vmThread)->gsParameters))

/**
 * Define datastructures for guarded storage control blocks
 */
typedef struct J9GSControlBlock {
#if defined(S390) || defined(J9ZOS390)
	/* Word 0/1 - Reserved */
	uint64_t reserved;
	/* Word 2/3 - Guarded-Storage Designation
	 * |0------------------------GSO---------------------31|
	 * |32---GSO cont.--J|//..//|53--GSL--55|//|58--GSC--63|
	 *
	 * GSO - Guarded-Storage Origin bits 0-J, J depends on GSC, bits J-53 are reserved and have to be 0
	 * GSL - Guarded-Load Shift bits 53-55
	 * GSC - Guarded-Storage Characteristic bits 58-63, bits 56-58 are reserved and have to be 0
	 */
	uint64_t designationRegister;
	/* Word 4/5 - Guarded-Storage Section Mask */
	uint64_t sectionMask;
	/* Word 6/7 - Guarded-Storage-Event Parameter-List Address */
	uint64_t paramListAddr;
#else
	uint32_t placeHolder;
#endif
} J9GSControlBlock;
