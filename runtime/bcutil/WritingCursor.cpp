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
/*
 * WritingCursor.cpp
 */

#include "ROMClassStringInternManager.hpp"
#include "WritingCursor.hpp"
#include <stdlib.h>
#include "ut_j9bcu.h"
#include "j9.h"

void
WritingCursor::writeU8(U_8 u8Value, DataType dataType)
{
	U_8 *u8Addr = (U_8 *)(_baseAddress + _count);
	*u8Addr = u8Value;
	_count += sizeof(U_8);
}

void
WritingCursor::writeU16(U_16 u16Value, DataType dataType)
{
	U_16 *u16Addr = (U_16 *)(_baseAddress + _count);
	*u16Addr = u16Value;
	_count += sizeof(U_16);
}

void
WritingCursor::writeU32(U_32 u32Value, DataType dataType)
{
	U_32 *u32Addr = (U_32 *)(_baseAddress + _count);
	*u32Addr = u32Value;
	_count += sizeof(U_32);
}

void
WritingCursor::writeU64(U_32 u32ValueHigh, U_32 u32ValueLow, DataType dataType)
{
	U_64 *u64Addr = (U_64 *)(_baseAddress + _count);

#ifdef J9VM_ENV_LITTLE_ENDIAN
	*u64Addr = (U_64(u32ValueLow) << 32) + u32ValueHigh;
#else
	*u64Addr = (U_64(u32ValueHigh) << 32) + u32ValueLow;
#endif

	_count += sizeof(U_64);
}

void
WritingCursor::writeUTF8(U_8* UTF8Data, U_16 UTF8Length, DataType dataType)
{
	/* intern this string */
	J9UTF8 *utf8Ptr = (J9UTF8 *)(_baseAddress + _count);
	Cursor::writeUTF8(UTF8Data, UTF8Length, dataType);
	_internManager->internString(utf8Ptr);
}

void
WritingCursor::writeSRP(UDATA srpKey, DataType dataType)
{
	J9SRP *srpAddr = (J9SRP *)(_baseAddress + _count);
	*srpAddr = computeSRP(srpKey, srpAddr);
	_count += sizeof(J9SRP);
}

void
WritingCursor::writeWSRP(UDATA srpKey, DataType dataType)
{
	J9WSRP *wsrpAddr = (J9WSRP *)(_baseAddress + _count);
	*wsrpAddr = computeWSRP(srpKey, wsrpAddr);
	_count += sizeof(J9WSRP);
}

void
WritingCursor::writeData(U_8* bytes, UDATA length, DataType dataType)
{
	U_8 *u8Addr = (U_8 *)(_baseAddress + _count);
	memcpy(u8Addr, bytes, length);
	_count += length;
}

void
WritingCursor::padToAlignment(UDATA byteAlignment, DataType dataType)
{
	U_8 *startAddr = (U_8 *)(_baseAddress + _count);
	Cursor::padToAlignment(byteAlignment, dataType);
	U_8 *endAddr = (U_8 *)(_baseAddress + _count);
	memset(startAddr, 0, endAddr - startAddr);
}

void
WritingCursor::mark(UDATA srpKey)
{
	Trc_BCU_Assert_Equals(_count, getOffsetForSRPKey(srpKey));
}
