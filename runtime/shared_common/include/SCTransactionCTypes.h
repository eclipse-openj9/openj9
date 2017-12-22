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

#if !defined(J9SC_TRANSACTION_CTYPES_H_INCLUDED)
#define J9SC_TRANSACTION_CTYPES_H_INCLUDED

#include "j9.h"

typedef struct J9RomClassRequirements {
	U_32 romClassSizeFullSize;
	U_32 romClassMinimalSize;
	U_32 lineNumberTableSize;
	U_32 localVariableTableSize;
} J9RomClassRequirements;

#define J9SC_ROMCLASS_PIECES_USED_FULL_SIZE 0x1
#define J9SC_ROMCLASS_PIECES_DEBUG_DATA_OUT_OF_LINE 0x2

typedef struct J9SharedRomClassPieces {
	void *romClass;
	void *lineNumberTable;
	void *localVariableTable;
	U_32 flags;
} J9SharedRomClassPieces;

#endif /* J9SC_TRANSACTION_CTYPES_H_INCLUDED */
