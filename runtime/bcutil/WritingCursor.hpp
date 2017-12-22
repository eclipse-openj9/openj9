/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

/*
 * WritingCursor.hpp
 */

#ifndef WRITINGCURSOR_HPP_
#define WRITINGCURSOR_HPP_

/* @ddr_namespace: default */
#include "Cursor.hpp"

class ROMClassStringInternManager;

class WritingCursor : public Cursor
{
public:
	WritingCursor(UDATA tag, SRPOffsetTable *srpOffsetTable, ROMClassStringInternManager *internManager, ROMClassCreationContext * context) :
		Cursor(tag, srpOffsetTable, context),
		_baseAddress(srpOffsetTable->getBaseAddressForTag(tag)),
		_internManager(internManager)
	{
	}

	void writeU8(U_8 u8Value, DataType dataType);
	void writeU16(U_16 u16Value, DataType dataType);
	void writeU32(U_32 u32Value, DataType dataType);
	void writeU64(U_32 u32ValueHigh, U_32 u32ValueLow, DataType dataType);
	void writeUTF8(U_8* UTF8Data, U_16 UTF8Length, DataType dataType);
	void writeData(U_8* bytes, UDATA length, DataType dataType);
	void padToAlignment(UDATA byteAlignment, DataType dataType);

	void writeSRP(UDATA srpKey, DataType dataType);
	void writeWSRP(UDATA srpKey, DataType dataType);
	void mark(UDATA srpKey);

private:
	U_8 * _baseAddress;
	ROMClassStringInternManager *_internManager;
};

#endif /* WRITINGCURSOR_HPP_ */
