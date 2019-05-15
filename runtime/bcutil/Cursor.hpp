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

/*
 * Cursor.hpp
 */

#ifndef CURSOR_HPP_
#define CURSOR_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"
#include "j9.h"
#include "SRPOffsetTable.hpp"
#include "ROMClassCreationContext.hpp"

class Cursor
{
public:

	/* 
	 * DataType is used to identify the type of data being written.
	 */
	enum DataType {
		// Actual bytecodes
		BYTECODE, 
		// Generic non-specific data
		GENERIC,
		// The romSize field of the J9ROMClass structure
		ROM_SIZE,
		// The size of debug information store in a ROMMethod
		METHOD_DEBUG_SIZE,
		// Self Relative Pointer (SRP) to non-specific data
		SRP_TO_GENERIC,
		// SRP to a UTF8 string
		SRP_TO_UTF8,
		// SRP to a Name and Signature structure
		SRP_TO_NAME_AND_SIGNATURE,
		// Class file bytes
		INTERMEDIATE_CLASS_DATA,
		// Class file bytes size
		INTERMEDIATE_CLASS_DATA_LENGTH,
		// SRP to a method debug data
		SRP_TO_DEBUG_DATA,
		// Optional flags for ROMClass
		OPTIONAL_FLAGS,
		// SRP to source debug extension information
		SRP_TO_SOURCE_DEBUG_EXT,
		// The length of source debug extension information
		SOURCE_DEBUG_EXT_LENGTH,
		// The source debug extension information
		SOURCE_DEBUG_EXT_DATA,
		// Optional file name information
		OPTINFO_SOURCE_FILE_NAME,
		// All the class modiers
		ROM_CLASS_MODIFIERS,
		// Line number data for a rom class method
		LINE_NUMBER_DATA,
		// Number of local variables
		LOCAL_VARIABLE_COUNT,
		//SRP to local variable data start
		SRP_TO_LOCAL_VARIABLE_DATA,
		// Local variable data
		LOCAL_VARIABLE_DATA,
		// Local variable data SRP to UTF8 data
		LOCAL_VARIABLE_DATA_SRP_TO_UTF8,
		// SRP to intermediate class data
		SRP_TO_INTERMEDIATE_CLASS_DATA
	};

	/* 
	 * Mode is used to indicate the transition from one cursor type 
	 * to another.  This feature was specifically added to enable
	 * the comparison of ROMClass that had debug information out of line.
	 * 
	 * This was required to support the ComparingCursor and ComparingCursorHelper model.
	 */
	enum Mode { 
		// In the mainline of the ROMClass
		MAIN_CURSOR,
		// Writing debug information and line number data
		DEBUG_INFO,
		// Writing variable information
		VAR_INFO
	};

	Cursor(UDATA tag, SRPOffsetTable *srpOffsetTable, ROMClassCreationContext * context) :
		_srpOffsetTable(srpOffsetTable),
		_count(0),
		_tag(tag),
		_context(context)
	{
	}

	virtual UDATA getCount() { return _count; }

	/* Note: WritingCursor's versions of the methods below do not call these ones for
	 * performance reasons on Linux PPC (since XLC fails to inline them). If you make
	 * a change that needs to affect WritingCursor, you must change those versions too.
	 */
	virtual void writeU8(U_8 u8Value, DataType dataType) { _count += sizeof(U_8); }
	virtual void writeU16(U_16 u16Value, DataType dataType) { _count += sizeof(U_16); }
	virtual void writeU32(U_32 u32Value, DataType dataType) { _count += sizeof(U_32); }
	virtual void writeU64(U_32 u32ValueHigh, U_32 u32ValueLow, DataType dataType) { _count += sizeof(U_64); }
	virtual void writeUTF8(U_8* utf8Data, U_16 utf8Length, DataType dataType)
	{
		writeU16(utf8Length, Cursor::GENERIC);
		writeData(utf8Data, utf8Length, Cursor::GENERIC);
		/* Pad manually as it is significantly faster than using padToAlignment(). */
		if (0 != (utf8Length & 1)) {
			writeU8(0, Cursor::GENERIC);
		}
	}
	virtual void writeData(U_8* bytes, UDATA length, DataType dataType) { _count += length; }
	virtual void padToAlignment(UDATA byteAlignment, DataType dataType) {
		/* Note: byteAlignment must be a power of 2. */
		UDATA alignmentBits = (byteAlignment - 1);
		UDATA adjustedCount = (_count + alignmentBits) & ~alignmentBits;
		UDATA bytesToPad = adjustedCount - _count;

		if (0 != bytesToPad) {
			_count += bytesToPad;
		}
	}
	virtual void skip(UDATA byteCount, DataType dataType = Cursor::GENERIC) { _count += byteCount; }
	virtual U_32 peekU32() { return 0; }
	virtual void notifyDebugDataWriteStart() { /* do nothing */ }
	virtual void notifyVariableTableWriteEnd() { /* do nothing */ }
	virtual void notifyDebugDataWriteEnd() { /* do nothing */ }

	/*
	 * write(W)SRP will write a NULL SRP if srpKey has not been marked with mark(srpKey)
	 */
	virtual void writeSRP(UDATA srpKey, DataType dataType) { _count += sizeof(J9SRP); }
	virtual void writeWSRP(UDATA srpKey, DataType dataType) { _count += sizeof(J9WSRP); }
	virtual void mark(UDATA srpKey) { _srpOffsetTable->insert(srpKey, _count, _tag); }

#ifdef J9VM_ENV_LITTLE_ENDIAN
	void writeBigEndianU16(U_16 u16Value, DataType dataType) { writeU16(swapU16(u16Value), dataType); }
#else
	void writeBigEndianU16(U_16 u16Value, DataType dataType) { writeU16(u16Value, dataType); }
#endif

	bool isSRPNull(UDATA srpKey) { return !_srpOffsetTable->isNotNull(srpKey); }
	J9SRP computeSRP(UDATA key, J9SRP *srpAddr) { return _srpOffsetTable->computeSRP(key, srpAddr); }
	J9WSRP computeWSRP(UDATA key, J9WSRP *wsrpAddr) { return _srpOffsetTable->computeWSRP(key, wsrpAddr); }
	UDATA getOffsetForSRPKey(UDATA srpKey) { return _srpOffsetTable->get(srpKey); }

protected:
	/*
	 * This functionality supports the ComparingCursor/ComparingCursorHelper model.
	 * Allowing the helpers to be re-based with a new baseAddress, which requires the 
	 * count to also be reset.
	 */
	void resetCount() { _count = 0; }

	UDATA _count;
	ROMClassCreationContext * _context;

private:
#ifdef J9VM_ENV_LITTLE_ENDIAN
	static U_16 swapU16(U_16 u16) { return ((u16 & 0xff00) >> 8) | ((u16 & 0x00ff) << 8); }
#endif

	SRPOffsetTable *_srpOffsetTable;
	UDATA _tag;
};

#endif /* CURSOR_HPP_ */
