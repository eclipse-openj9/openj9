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
 * ComparingCursor.hpp
 */

#ifndef COMPARINGCURSOR_HPP_
#define COMPARINGCURSOR_HPP_

/* @ddr_namespace: default */
#include "Cursor.hpp"
#include "ComparingCursorHelper.hpp"

class SRPKeyProducer;
class ClassFileOracle;

class ComparingCursor : public Cursor
{
public:
	ComparingCursor(J9JavaVM *javaVM, SRPOffsetTable *srpOffsetTable, SRPKeyProducer *srpKeyProducer,
		ClassFileOracle *classFileOracle, U_8 *romClass, bool romClassIsShared, ROMClassCreationContext * context);
	~ComparingCursor();

	UDATA getCount();
	void writeU8(U_8 u8Value, DataType dataType);
	void writeU16(U_16 u16Value, DataType dataType);
	void writeU32(U_32 u32Value, DataType dataType);
	void writeU64(U_32 u32ValueHigh, U_32 u32ValueLow, DataType dataType);
	void writeData(U_8* bytes, UDATA length, DataType dataType);
	void padToAlignment(UDATA byteAlignment, DataType dataType);
	void writeSRP(UDATA srpKey, DataType dataType);
	void writeWSRP(UDATA srpKey, DataType dataType);
	void mark(UDATA srpKey) { /* do nothing */ }
	void notifyDebugDataWriteStart();
	void notifyVariableTableWriteEnd();
	void notifyDebugDataWriteEnd() { _context->endDebugCompare(); }
	U_32 peekU32(DataType dataType);
	void skip(UDATA byteCount, DataType dataType = Cursor::GENERIC);
	bool isEqual() const { return _isEqual; }

private:
	J9JavaVM *_javaVM;
	bool _checkRangeInSharedCache;
	ClassFileOracle *_classFileOracle;
	SRPKeyProducer *_srpKeyProducer;
	U_8 *_romClass;
	Cursor::Mode _mode;
	U_8 * _storePointerToVariableInfo;
	U_8 * _basePointerToVariableInfo;
	ComparingCursorHelper _mainHelper;
	ComparingCursorHelper _lineNumberHelper;
	ComparingCursorHelper _varInfoHelper;
	bool _isEqual;
	void markUnEqual() { _isEqual = false; }
	bool isRangeValidForPtr(U_8 *ptr, UDATA length);
	UDATA getMaximumValidLengthForPtrInSegment(U_8 *ptr);

	/*Helper verification methods*/
	bool shouldCheckForEquality(DataType dataType, U_32 u32Value = 0);
	bool isRangeValid(UDATA length, DataType dataType);
	bool isRangeValidForUTF8Ptr(J9UTF8 *utf8);
	
	/*Methods to get the correct helper (aka counter) for compare*/
	ComparingCursorHelper * getCountingCursor(DataType dataType);
};

#endif /* COMPARINGCURSOR_HPP_ */
