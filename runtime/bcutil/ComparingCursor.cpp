/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
 * ComparingCursor.cpp
 */

#include "ComparingCursor.hpp"

#include "ClassFileOracle.hpp"
#include "SRPKeyProducer.hpp"
#include "SCQueryFunctions.h"

#include "ut_j9bcu.h"
#include "bcnames.h"
#include "pcstack.h"
#include "rommeth.h"
#include "vrfytbl.h"
#include "bytecodewalk.h"


ComparingCursor::ComparingCursor(J9JavaVM *javaVM, SRPOffsetTable *srpOffsetTable,
		SRPKeyProducer *srpKeyProducer, ClassFileOracle *classFileOracle, U_8 *romClass, 
		bool romClassIsShared, ROMClassCreationContext * context) :
	Cursor(0, srpOffsetTable, context),
	_javaVM(javaVM),
	_checkRangeInSharedCache(romClassIsShared),
	_classFileOracle(classFileOracle),
	_srpKeyProducer(srpKeyProducer),
	_romClass(romClass),
	_mode(Cursor::MAIN_CURSOR),
	_storePointerToVariableInfo(NULL),
	_basePointerToVariableInfo(NULL),
	_mainHelper(srpOffsetTable, romClass, context),
	_lineNumberHelper(srpOffsetTable, romClass, context),
	_varInfoHelper(srpOffsetTable, romClass, context),
	_isEqual(true)
{
	if (!_checkRangeInSharedCache && (NULL != javaVM)) {
		/* Enter mutex in order to safely iterate over the segments in getMaximumValidLengthForPtrInSegment(). */
		omrthread_monitor_enter(javaVM->classMemorySegments->segmentMutex);
	}

}

ComparingCursor::~ComparingCursor()
{
	if (!_checkRangeInSharedCache && (NULL != _javaVM)) {
		/* Exit mutex that was entered in the constructor. */
		omrthread_monitor_exit(_javaVM->classMemorySegments->segmentMutex);
	}
}

UDATA 
ComparingCursor::getCount() 
{
	/* There is no call to getCount for the compare cursor 
	 * that is interested in _lineNumberHelper or _varInfoHelper
	 * counts.
	 */
	return _mainHelper.getCount();
}

void
ComparingCursor::writeU8(U_8 u8Value, DataType dataType)
{
	ComparingCursorHelper * countingcursor = getCountingCursor(dataType);
	if ( shouldCheckForEquality(dataType) ) {
		if ( !isRangeValid(sizeof(U_8), dataType) || (u8Value != *(U_8*)(countingcursor->getBaseAddress() + countingcursor->getCount())) ) {
			markUnEqual();
		}
	}
	countingcursor->writeU8(u8Value, dataType);
}

void
ComparingCursor::writeU16(U_16 u16Value, DataType dataType)
{
	ComparingCursorHelper * countingcursor = getCountingCursor(dataType);
	if ( shouldCheckForEquality(dataType) ) {
		if ( !isRangeValid(sizeof(U_16), dataType) || (u16Value != *(U_16 *)(countingcursor->getBaseAddress() + countingcursor->getCount())) ) {
			markUnEqual();
		}
	}
	countingcursor->writeU16(u16Value, dataType);
}

void
ComparingCursor::writeU32(U_32 u32Value, DataType dataType)
{
	ComparingCursorHelper * countingcursor = getCountingCursor(dataType);
	if ( shouldCheckForEquality(dataType, u32Value) ) {
		U_32 * tmpu32 = (U_32 *)(countingcursor->getBaseAddress() + countingcursor->getCount());
		if ( !isRangeValid(sizeof(U_32), dataType) || (u32Value != *tmpu32) ) {
			markUnEqual();
		}
	}
	countingcursor->writeU32(u32Value, dataType);
}

U_32 
ComparingCursor::peekU32(DataType dataType) {
	ComparingCursorHelper * countingcursor = getCountingCursor(dataType);
	U_32 * tmpptr = (U_32 *)(countingcursor->getBaseAddress() + countingcursor->getCount());
	return *tmpptr;
}

/* If skip is called on a comparing cursor then this function 
 * must be called to ensure the correct helpers '_count' is 
 * incremented
 */
void 
ComparingCursor::skip(UDATA byteCount, DataType dataType)
{
	Cursor * countingcursor = getCountingCursor(dataType);
	countingcursor->skip(byteCount);
}

void
ComparingCursor::writeU64(U_32 u32ValueHigh, U_32 u32ValueLow, DataType dataType)
{
	ComparingCursorHelper * countingcursor = getCountingCursor(dataType);
	if ( shouldCheckForEquality(dataType) ) {
		if ( isRangeValid(sizeof(U_64), dataType) ) {
#ifdef J9VM_ENV_LITTLE_ENDIAN
			U_64 u64Value = (U_64(u32ValueLow) << 32) + u32ValueHigh;
#else
			U_64 u64Value = (U_64(u32ValueHigh) << 32) + u32ValueLow;
#endif
			if ( u64Value != *(U_64 *)(countingcursor->getBaseAddress() + countingcursor->getCount()) ) {
				markUnEqual();
			}
		} else {
			markUnEqual();
		}
	}
	countingcursor->writeU64(u32ValueHigh, u32ValueLow, dataType);
}

void
ComparingCursor::writeSRP(UDATA srpKey, DataType dataType)
{
	ComparingCursorHelper * countingcursor = getCountingCursor(dataType);
	U_8 * currentAddr = countingcursor->getBaseAddress() + countingcursor->getCount();
	if ( shouldCheckForEquality(dataType) ) {
		if ( isRangeValid(sizeof(J9SRP), dataType) ) {
			switch (dataType) {
			case Cursor::SRP_TO_DEBUG_DATA:
			case Cursor::SRP_TO_GENERIC:
				/* ignore this type */
				break;
			case Cursor::SRP_TO_LOCAL_VARIABLE_DATA:
			 {
				/* test that the SRP is valid */
				void * tmpvardata = SRP_PTR_GET(currentAddr, void *);
				if ( NULL == tmpvardata ) {
					if (!isSRPNull(srpKey)) {
						markUnEqual();
					}
				} else if ( (_context->shouldPreserveLocalVariablesInfo() && isSRPNull(srpKey)) || !isRangeValidForPtr((U_8 *)tmpvardata,sizeof(void*)) ) {
					/* If variable information is not required then this SRP will not be marked during the 'mark & count' phase.
					 * Checking shouldPreserveLocalVariablesInfo() ensure we isSRPNull() only if it could have been marked.
					 */
					markUnEqual();
				}
				break;
			}
			case Cursor::SRP_TO_SOURCE_DEBUG_EXT: {
				/* test that the SRP is valid */
				void * debugext = SRP_PTR_GET(currentAddr, void *);

				if ( NULL == debugext ) {
					if (!isSRPNull(srpKey)) {
						markUnEqual();
					}
				} else if ( isSRPNull(srpKey) || !isRangeValidForPtr((U_8 *)debugext,sizeof(void*)) ) {
					markUnEqual();
				}
				break;
			}
			case Cursor::LOCAL_VARIABLE_DATA_SRP_TO_UTF8:
			case Cursor::OPTINFO_SOURCE_FILE_NAME:
			case Cursor::SRP_TO_UTF8: {
				/* test that the UTF8's are identical */
				J9UTF8 * utf8 = SRP_PTR_GET(currentAddr, J9UTF8 *);

				if ( NULL == utf8 ) {
					if (!isSRPNull(srpKey)) {
						markUnEqual();
					}
				} else if ( isSRPNull(srpKey) || !isRangeValidForUTF8Ptr(utf8) ) {
					markUnEqual();
				} else {
					U_16 cpIndex = _srpKeyProducer->mapKeyToCfrConstantPoolIndex(srpKey);

					if ( J9UTF8_LENGTH(utf8) != _classFileOracle->getUTF8Length(cpIndex) ) {
						markUnEqual();
					} else if (0 != memcmp(J9UTF8_DATA(utf8), _classFileOracle->getUTF8Data(cpIndex), J9UTF8_LENGTH(utf8))) {
						markUnEqual();
					}
				}
				break;
			}
			case Cursor::SRP_TO_NAME_AND_SIGNATURE: {
				J9ROMNameAndSignature *nas = SRP_PTR_GET(currentAddr, J9ROMNameAndSignature *);

				if ( NULL == nas ) {
					if (!isSRPNull(srpKey)) {
						markUnEqual();
					}
				} else if ( isRangeValidForPtr((U_8*)nas, sizeof(J9ROMNameAndSignature)) ) {
					J9UTF8 *name = SRP_GET(nas->name, J9UTF8 *);
					J9UTF8 *signature = SRP_GET(nas->signature, J9UTF8 *);

					if ( isSRPNull(srpKey) || !isRangeValidForUTF8Ptr(name) || !isRangeValidForUTF8Ptr(signature) ) {
						markUnEqual();
					} else {
						U_16 cpIndex = _srpKeyProducer->mapKeyToCfrConstantPoolIndex(srpKey);

						if ( (J9UTF8_LENGTH(name) != _classFileOracle->getNameAndSignatureNameUTF8Length(cpIndex)) ||
							 (J9UTF8_LENGTH(signature) != _classFileOracle->getNameAndSignatureSignatureUTF8Length(cpIndex))
						) {
							markUnEqual();
						} else if ( (0 != memcmp(J9UTF8_DATA(name), _classFileOracle->getNameAndSignatureNameUTF8Data(cpIndex), J9UTF8_LENGTH(name))) ||
									(0 != memcmp(J9UTF8_DATA(signature), _classFileOracle->getNameAndSignatureSignatureUTF8Data(cpIndex), J9UTF8_LENGTH(signature)))
						) {
							markUnEqual();
						}
					}
				} else {
					markUnEqual();
				}
				break;
			}
			default:
				Trc_BCU_Assert_ShouldNeverHappen();
				break;
			}
		} else {
			/* Invalid range. */
			markUnEqual();
		}
	}
	countingcursor->writeSRP(srpKey, dataType);
}

void
ComparingCursor::writeWSRP(UDATA srpKey, DataType dataType)
{
	Cursor * countingcursor = getCountingCursor(dataType);
	if ( shouldCheckForEquality(dataType) ) {
		if ( isRangeValid(sizeof(J9WSRP), dataType) ) {
			switch (dataType) {
			case Cursor::SRP_TO_GENERIC:  /* fall through */
			case Cursor::SRP_TO_INTERMEDIATE_CLASS_DATA:
				/* ignore this type */
				break;
			default:
				Trc_BCU_Assert_ShouldNeverHappen();
				break;
			}
		} else {
			markUnEqual();
		}
	}
	countingcursor->writeWSRP(srpKey, dataType);
}

void
ComparingCursor::writeData(U_8* bytes, UDATA length, DataType dataType)
{
	ComparingCursorHelper * countingcursor = getCountingCursor(dataType);
	U_8 * baseAddress = countingcursor->getBaseAddress();
	U_8 * currentAddr = baseAddress + countingcursor->getCount();
	if ( shouldCheckForEquality(dataType) ) {
		if ( isRangeValid(length, dataType) ) {
			switch ( dataType ) {
			case Cursor::INTERMEDIATE_CLASS_DATA: {
				J9ROMClass *romClass = (J9ROMClass *)baseAddress;
				if (length != (UDATA)romClass->intermediateClassDataLength) {
					markUnEqual();
				} else {
					U_8* intermediates = J9ROMCLASS_INTERMEDIATECLASSDATA(romClass);
					if ( 0 != memcmp(bytes, intermediates, length) ) {
						markUnEqual();
					}
				}
				break;
			}
			case Cursor::BYTECODE: {
				U_8 *code = bytes;
				U_8 *romClassCode = currentAddr;
				UDATA codeIndex = 0;

				/*
				 * An exception must be made for JBgenericReturn bytecodes to allow comparison
				 * against ROMClasses that have been added to the shared cache. Such ROMClasses
				 * will have had their return bytecodes "fixed" with fixReturnBytecodes, so any
				 * of the equivalent return bytecodes is allowed to match an attempt to write a
				 * JBgenericReturn.
			 	 */

				while (codeIndex < length) {
					UDATA startCodeIndex = codeIndex;
					U_8 instruction = code[codeIndex];
					U_8 romInstruction = romClassCode[codeIndex];

					/* Check if the bytecode matches with a special case for JBgenericReturn. */
					if (instruction != romInstruction) {
						if ((JBgenericReturn != instruction) ||
							(RTV_RETURN != (J9JavaBytecodeVerificationTable[romInstruction] >> 8))
						) {
							markUnEqual();
							break;
						}
					}

					if (JBtableswitch == instruction) {
						codeIndex += 4 - (codeIndex & 3); /* step past instruction + pad */
						codeIndex += sizeof(I_32); /* default offset */
						I_32 low = *(I_32 *)&code[codeIndex];
						codeIndex += sizeof(I_32);
						I_32 high = *(I_32 *)&code[codeIndex];
						codeIndex += sizeof(I_32);
						I_32 noffsets = high - low + 1;
						codeIndex += noffsets * sizeof(I_32);
					} else if (JBlookupswitch == instruction) {
						codeIndex += 4 - (codeIndex & 3); /* step past instruction + pad */
						codeIndex += sizeof(I_32); /* default offset */
						I_32 npairs = *(I_32 *)&code[codeIndex];
						codeIndex += sizeof(I_32);
						codeIndex += npairs * 2 * sizeof(I_32);
					} else {
						UDATA step = (J9JavaInstructionSizeAndBranchActionTable[instruction] & 0x0f);
						if (0 == step) {
							/* Bad bytecodes? */
							markUnEqual();
							break;
						}

						codeIndex += step;
					}

					if (codeIndex > length) {
						/* Stepped out of range? */
						markUnEqual();
						break;
					}

					/* Check if the instruction payload matches. */
					UDATA payloadLength = codeIndex - startCodeIndex - 1;
					if ( (0 != payloadLength) &&
						 (0 != memcmp(code + startCodeIndex + 1, romClassCode + startCodeIndex + 1, payloadLength))
					) {
						markUnEqual();
						break;
					}
				}
				break;
			}
			case SRP_TO_INTERMEDIATE_CLASS_DATA:
				/* ignore this type */
				break;
			default: 
				if ( 0 != memcmp(bytes, currentAddr, length) ) {
					markUnEqual();
				}
				break;
			}
		}
	} else {
		if (dataType == LINE_NUMBER_DATA) {
			/* If we are here then the line number data needs to be skipped over,
			 * in this case we use the length for the existing rom class.
			 * 
			 * The bytes argument is ignored in the call to Cursor::writeData below.
			 */
			length = _context->romMethodCompressedLineNumbersLength();
		}
	}
	countingcursor->writeData(bytes, length, dataType);
}

void
ComparingCursor::padToAlignment(UDATA byteAlignment, DataType dataType)
{
	Cursor * countingcursor = getCountingCursor(dataType);
	if ( shouldCheckForEquality(dataType) ) {
		// TODO 
	}
	countingcursor->padToAlignment(byteAlignment, dataType);
}

bool
ComparingCursor::shouldCheckForEquality(DataType dataType, U_32 u32Value)
{
	if ( !isEqual() ) {
		return false;
	}

	switch (dataType) {
	case BYTECODE: /* fall through */
	case GENERIC: /* fall through */
	case SRP_TO_DEBUG_DATA: /* fall through */
	case SRP_TO_GENERIC: /* fall through */
	case SRP_TO_UTF8: /* fall through */
	case SRP_TO_NAME_AND_SIGNATURE: /* fall through */
	case SRP_TO_INTERMEDIATE_CLASS_DATA:
		/* do nothing -- return true at bottom of function */
		break;
	case METHOD_DEBUG_SIZE: /* fall through */
	case ROM_SIZE: /* fall through */
	case INTERMEDIATE_CLASS_DATA: /* fall through */
	case INTERMEDIATE_CLASS_DATA_LENGTH:
		return false;
	case ROM_CLASS_MODIFIERS:
		if (u32Value == (u32Value & _context->romMethodModifiers())) {
			/*It's ok if the existing rom class has extra debug information and it is not required*/
			return false;
		}
		break;
	case OPTIONAL_FLAGS:
		if (u32Value == (u32Value & _context->romClassOptionalFlags())) {
			/*During compare it is ok if an existing class has more information*/
			return false;
		}
		break;
	case SRP_TO_SOURCE_DEBUG_EXT: /* fall through */
	case SOURCE_DEBUG_EXT_LENGTH: /* fall through */
	case SOURCE_DEBUG_EXT_DATA:
		if (_context->romClassHasSourceDebugExtension() && !(_classFileOracle->hasSourceDebugExtension() && _context->shouldPreserveSourceDebugExtension())) {
			/* If the existing rom class contains a source debug extension,
			 * and it is not required by the new class then skip the comparison.
			 */
			return false;
		}
		break;
	case OPTINFO_SOURCE_FILE_NAME:
		if (_context->romClassHasSourceFileName() && !(_classFileOracle->hasSourceFile() && _context->shouldPreserveSourceFileName())) {
			/* If the existing rom class contains a source filename and isn't needed, then skip the comparison.
			 */
			return false;
		}
		break;
	case SRP_TO_LOCAL_VARIABLE_DATA:
		/* fall through
		 * 
		 * The SRP_TO_LOCAL_VARIABLE_DATA is stored in the header of the method debug data.
		 * 
		 * If there is no LINE_NUMBER_DATA to be preserved, then the header is also skipped
		 * and this value is not needed.
		 */
	case LINE_NUMBER_DATA:
		if (!(_context->shouldPreserveLineNumbers()) && _context->romMethodHasDebugData()) {
			/* If line number information doesn't need to be preserved, and there is 
			 * existing data then skip the comparison.
			 */
			return false;
		}
		break;
	case LOCAL_VARIABLE_COUNT: /* fall through */
	case LOCAL_VARIABLE_DATA_SRP_TO_UTF8: /* fall through */
	case LOCAL_VARIABLE_DATA:
		if (!(_context->shouldPreserveLocalVariablesInfo()) && _context->romMethodHasDebugData()) {
			/* If variable information doesn't need to be preserved, and there is 
			 * existing data then skip the comparison.
			 */
			return false;
		}
		break;
	default:
		Trc_BCU_Assert_ShouldNeverHappen();
		break;
	}

	return true;
}

bool
ComparingCursor::isRangeValid(UDATA length, DataType dataType)
{
	ComparingCursorHelper * countingcursor = getCountingCursor(dataType);
	
	if (countingcursor != &(_mainHelper)) {
		if (_checkRangeInSharedCache) {
			return j9shr_Query_IsAddressInCache(_javaVM, countingcursor->getBaseAddress() + countingcursor->getCount(), length) ? true : false;
		}
		return true;
	} else {
		J9ROMClass * rc = (J9ROMClass *)countingcursor->getBaseAddress();
		return (countingcursor->getCount() + length) <= rc->romSize;
	}
}

bool
ComparingCursor::isRangeValidForPtr(U_8 *ptr, UDATA length)
{
	if (_checkRangeInSharedCache) {
		return (j9shr_Query_IsAddressInCache(_javaVM, ptr, length) ? true : false);
	} else {
		return length < getMaximumValidLengthForPtrInSegment(ptr);
	}
}

bool
ComparingCursor::isRangeValidForUTF8Ptr(J9UTF8 *utf8)
{
	U_8 *ptr = (U_8*)utf8;

	if (_checkRangeInSharedCache) {
		return j9shr_Query_IsAddressInCache(_javaVM, utf8, sizeof(J9UTF8)) &&
			   j9shr_Query_IsAddressInCache(_javaVM, ptr + offsetof(J9UTF8, data), J9UTF8_LENGTH(utf8));
	} else {
		UDATA maxLength = getMaximumValidLengthForPtrInSegment(ptr);

		return (sizeof(J9UTF8) < maxLength) && (J9UTF8_LENGTH(utf8) < (maxLength - offsetof(J9UTF8, data)));
	}
}

UDATA 
ComparingCursor::getMaximumValidLengthForPtrInSegment(U_8 *ptr)
{
	Trc_BCU_Assert_False(_checkRangeInSharedCache);

	if (NULL != _javaVM) {
		/* There is an AVL tree of class memory segments, use it to efficiently find a potential segment that
		 * could contain the pointer value. */
		J9MemorySegment *segment = (J9MemorySegment *) avl_search(&(_javaVM->classMemorySegments->avlTreeData), UDATA(ptr));
		if ( (NULL != segment) && (ptr >= segment->heapBase) && (ptr < segment->heapTop) ) {
			return UDATA(segment->heapTop) - UDATA(ptr);
		}

		return 0;
	}

	/* If _checkRangeInSharedCache is false and _javaVM is NULL, don't validate address ranges. */
	return UDATA_MAX;
}

void 
ComparingCursor::notifyDebugDataWriteStart() {
	/*Inform the context that _romClass should be used to determine if debug data is inline or out of line*/
	_context->startDebugCompare();
	/* This method notifies the cursor that writes to the method debug data is about to occur.
	 * The notification occurs just before deciding to write the SRP to debug area
	 */ 
	if (!_context->romMethodDebugDataIsInline()) {
		/* If the debug data is out of line, then call resetBaseMemoryLocation to
		 * point the compare helpers to the correct memory.
		 *
		 * Calling resetBaseMemoryLocation will also set _count on the helpers
		 * to zero since they are indexing a different memory offset.
		 */
		U_8 * lineNumberStartAddr = (U_8 *) _context->romMethodLineNumbersDataStart();
		U_8 * varTableStartAddr = (U_8 *) _context->romMethodVariableDataStart();
		_lineNumberHelper.resetBaseMemoryLocation(lineNumberStartAddr);
		_varInfoHelper.resetBaseMemoryLocation(varTableStartAddr);
	}
}

void
ComparingCursor::notifyVariableTableWriteEnd() {
	if (_context->shouldWriteDebugDataInline() && !(_context->shouldPreserveLocalVariablesInfo())) {
		/* This method is required to skip over inline variable table data in an existing rom class during compare.
		 * This is only done if the variable table is not required by the new class, and it is inline.
		 */
		J9MethodDebugInfo * debugDataStart = (J9MethodDebugInfo *) _context->romMethodLineNumbersDataStart();
		void * variableTableStart = _context->romMethodVariableDataStart();
		if (NULL != variableTableStart) {
			/*Note: if variableTableStart is null then the variable count is 0 for the existing method*/
			U_32 existingClassDebugInfoSize = (debugDataStart->srpToVarInfo & ~0x1);
			U_32 sizeWithoutVariableTable = (U_32)((UDATA)variableTableStart - (UDATA)debugDataStart);
			U_32 inlineVarInfoToSkip = (existingClassDebugInfoSize - sizeWithoutVariableTable);
			/*The debug data is inline so increment the count of _mainHelper*/
			_mainHelper.skip(inlineVarInfoToSkip);
		}
	}
}

ComparingCursorHelper *
ComparingCursor::getCountingCursor(DataType dataType) {
	switch (dataType) {
		case LOCAL_VARIABLE_COUNT:
		case SRP_TO_LOCAL_VARIABLE_DATA:
		case LINE_NUMBER_DATA:
			if (!(_context->shouldWriteDebugDataInline())) {
				return &_lineNumberHelper;

			}
			/*If the debug data is is inline return _mainHelper*/
			break;
		case LOCAL_VARIABLE_DATA_SRP_TO_UTF8:
		case LOCAL_VARIABLE_DATA:
			if (!(_context->shouldWriteDebugDataInline())) {
				return &_varInfoHelper;				
			}
			/*If the debug data is is inline return _mainHelper*/
			break;
		default:
			break;
	}
	return &_mainHelper;
}
