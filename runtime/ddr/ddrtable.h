/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#ifndef ddrtable_h
#define ddrtable_h

#include "j9ddr.h"
#include "j9.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const J9DDRStructDefinition** initializeDDRComponents(OMRPortLibrary* portLib);

#define J9DDR_COMPONENT_COUNT 10

#define J9DDR_COMPONENT_GC
#define J9DDR_COMPONENT_JIT
#define J9DDR_COMPONENT_JIT_AUTOMATED
#define J9DDR_COMPONENT_OMR
#define J9DDR_COMPONENT_VM
#define J9DDR_COMPONENT_STACKWALK
#define J9DDR_COMPONENT_HYPORT
#define J9DDR_COMPONENT_J9PORT
#define J9DDR_COMPONENT_ALGORITHM_VERSIONS
#define J9DDR_COMPONENT_DDRCPPSUPPORT

#ifdef __cplusplus
}
#endif

#endif /* ddrtable_h */
