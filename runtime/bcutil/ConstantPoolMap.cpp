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
 * ConstantPoolMap.cpp
 *
 * NOTE: See ConstantPoolMap.hpp for an explanation of why and how constant pool mapping is done.
 */

#include "ConstantPoolMap.hpp"

#include "BufferManager.hpp"
#include "ClassFileOracle.hpp"
#include "ROMClassCreationContext.hpp"
#include "ROMClassVerbosePhase.hpp"

#include "ut_j9bcu.h"

#define MAX_CALL_SITE_COUNT (1 << 16)
#define MAX_LDC_COUNT (1 << 8)
#define VARHANDLE_CLASS_NAME "java/lang/invoke/VarHandle"

ConstantPoolMap::ConstantPoolMap(BufferManager *bufferManager, ROMClassCreationContext *context) :
	_context(context),
	_classFileOracle(NULL),
	_bufferManager(bufferManager),
	_constantPoolEntries(NULL),
	_romConstantPoolEntries(NULL),
	_romConstantPoolTypes(NULL),
	_staticSplitEntries(NULL),
	_specialSplitEntries(NULL),
	_methodTypeCount(0),
	_varHandleMethodTypeCount(0),
	_varHandleMethodTypeLookupTable(NULL),
	_callSiteCount(0),
	_ramConstantPoolCount(0),
	_romConstantPoolCount(0),
	_staticSplitEntryCount(0),
	_specialSplitEntryCount(0),
	_buildResult(OutOfMemory)
{
}

ConstantPoolMap::~ConstantPoolMap()
{
	PORT_ACCESS_FROM_PORT(_context->portLibrary());
	_bufferManager->free(_constantPoolEntries);
	_bufferManager->free(_romConstantPoolEntries);
	_bufferManager->free(_romConstantPoolTypes);
	j9mem_free_memory(_varHandleMethodTypeLookupTable);
}

void
ConstantPoolMap::setClassFileOracleAndInitialize(ClassFileOracle *classFileOracle)
{
	ROMClassVerbosePhase v(_context, ConstantPoolMapping, &_buildResult);

	_classFileOracle = classFileOracle;

	UDATA cpCount = classFileOracle->getConstantPoolCount();
	_constantPoolEntries = (ConstantPoolEntry *) _bufferManager->alloc(cpCount * sizeof(ConstantPoolEntry));
	if (NULL != _constantPoolEntries) {
		memset(_constantPoolEntries, 0, cpCount * sizeof(ConstantPoolEntry));
		_buildResult = OK;
	}
}

void
ConstantPoolMap::computeConstantPoolMapAndSizes()
{
	/* CFR_CONSTANT_* to J9CPTYPE_* map. The remainder of the array maps to 0 (== J9CPTYPE_UNUSED). */
	static const U_8 cpTypeMap[256] = {
		J9CPTYPE_UNUSED, /* CFR_CONSTANT_Null    */
		J9CPTYPE_ANNOTATION_UTF8, /* CFR_CONSTANT_Utf8    */
		J9CPTYPE_UNUSED, /* 2 */
		J9CPTYPE_INT,    /* CFR_CONSTANT_Integer */
		J9CPTYPE_FLOAT,  /* CFR_CONSTANT_Float   */
		J9CPTYPE_LONG,   /* CFR_CONSTANT_Long    */
		J9CPTYPE_DOUBLE, /* CFR_CONSTANT_Double  */
		J9CPTYPE_CLASS,  /* CFR_CONSTANT_Class   */
		J9CPTYPE_STRING, /* CFR_CONSTANT_String  */
		J9CPTYPE_FIELD,  /* CFR_CONSTANT_Fieldref */
		J9CPTYPE_UNUSED, /* CFR_CONSTANT_Methodref */
		J9CPTYPE_INTERFACE_METHOD, /* CFR_CONSTANT_InterfaceMethodref */
		J9CPTYPE_UNUSED, /* CFR_CONSTANT_NameAndType */
		J9CPTYPE_UNUSED, /* 13 */
		J9CPTYPE_UNUSED, /* 14 */
		J9CPTYPE_METHODHANDLE, /* CFR_CONSTANT_MethodHandle */
		J9CPTYPE_METHOD_TYPE, /* CFR_CONSTANT_MethodType */
		J9CPTYPE_CONSTANT_DYNAMIC, /* CFR_CONSTANT_Dynamic */
		J9CPTYPE_UNUSED, /* CFR_CONSTANT_InvokeDynamic */
	};

	ROMClassVerbosePhase v(_context, ConstantPoolMapping, &_buildResult);

	/* Count entries */
	UDATA ldcSlotCount = 0;
	UDATA singleSlotCount = 0;
	UDATA doubleSlotCount = 0;
	UDATA cfrCPCount = _classFileOracle->getConstantPoolCount();

	for (U_16 cfrCPIndex = 0; cfrCPIndex < cfrCPCount; cfrCPIndex++) {
		if (_constantPoolEntries[cfrCPIndex].isUsedByLDC) {
			ldcSlotCount += 1;
		} else if (_constantPoolEntries[cfrCPIndex].isReferenced) {
			switch (getCPTag(cfrCPIndex)) {
				case CFR_CONSTANT_Double: /* fall through */
				case CFR_CONSTANT_Long:
					doubleSlotCount += 1;
					break;
				case CFR_CONSTANT_InterfaceMethodref: /* fall through */
				case CFR_CONSTANT_Methodref:
					if (isStaticSplit(cfrCPIndex)) {
						_staticSplitEntryCount += 1;
					}
					if (isSpecialSplit(cfrCPIndex)) {
						_specialSplitEntryCount += 1;
					}
					/* fall through */
				case CFR_CONSTANT_Fieldref: /* fall through */
				case CFR_CONSTANT_Class:
				case CFR_CONSTANT_String: /* fall through */
				case CFR_CONSTANT_Integer: /* fall through */
				case CFR_CONSTANT_Float: /* fall through */
				case CFR_CONSTANT_MethodHandle: /* fall through */
				case CFR_CONSTANT_MethodType:
					singleSlotCount += 1;
					break;
				case CFR_CONSTANT_Dynamic:
					if (isMarked(cfrCPIndex)) {
						/*
						 * All Constant Dynamic entries [ldc, ldc_w, ldc2w] are treated as single slot
						 * to always have a RAM constantpool entry created
						 */
						singleSlotCount += 1;
					}
					break;
				case CFR_CONSTANT_Utf8:
					if (isMarked(cfrCPIndex, ANNOTATION)) {
						singleSlotCount += 1;
					}
					break;
				case CFR_CONSTANT_InvokeDynamic: /* fall through */
				case CFR_CONSTANT_NameAndType:
					/* Ignore */
					break;
				default:
					Trc_BCU_Assert_ShouldNeverHappen();
					break;
			}
		}
	}

	if (_callSiteCount > MAX_CALL_SITE_COUNT) {
		_buildResult = GenericError;
		return;
	}

	if (ldcSlotCount > MAX_LDC_COUNT) {
		_buildResult = GenericError;
		return;
	}

	_ramConstantPoolCount = U_16(singleSlotCount + ldcSlotCount + 1);
	_romConstantPoolCount = U_16(_ramConstantPoolCount + doubleSlotCount);

	_romConstantPoolEntries = (U_16 *) _bufferManager->alloc(_romConstantPoolCount * sizeof(U_16));
	_romConstantPoolTypes = (U_8 *) _bufferManager->alloc(_romConstantPoolCount * sizeof(U_8));
	_staticSplitEntries = (U_16 *) _bufferManager->alloc(_staticSplitEntryCount * sizeof(U_16));
	_specialSplitEntries = (U_16 *) _bufferManager->alloc(_specialSplitEntryCount * sizeof(U_16));

	if ((NULL == _romConstantPoolEntries)
		|| (NULL == _romConstantPoolTypes)
		|| (NULL == _staticSplitEntries)
		|| (NULL == _specialSplitEntries)
	) {
		_buildResult = OutOfMemory;
		return;
	}

	/* Skip the zeroth entry. */
	U_16 ldcCPIndex = 1;
	U_16 romCPIndex = U_16(ldcCPIndex + ldcSlotCount);
	U_16 doubleSlotIndex = _ramConstantPoolCount;
	U_16 currentCallSiteIndex = 0;
	U_16 staticSplitTableIndex = 0;
	U_16 specialSplitTableIndex = 0;

	/*
	 * Fill in entries, ignoring InvokeDynamic, Utf8 and NameAndType entries.
	 * These entries take no space in ROMConstant Pool.
	 */
	for (U_16 cfrCPIndex = 0; cfrCPIndex < cfrCPCount; cfrCPIndex++) {
#define SET_ROM_CP_INDEX(cfrCPIdx, splitIdx, romCPIdx) _constantPoolEntries[(cfrCPIdx)].romCPIndex = (romCPIdx)
		if (_constantPoolEntries[cfrCPIndex].isUsedByLDC) {
			U_8 cpTag = getCPTag(cfrCPIndex);
			U_8 cpType = cpTypeMap[cpTag];

			Trc_BCU_Assert_NotEquals(cpType, J9CPTYPE_UNUSED);

			_romConstantPoolEntries[ldcCPIndex] = cfrCPIndex;
			_romConstantPoolTypes[ldcCPIndex] = cpType;
			SET_ROM_CP_INDEX(cfrCPIndex, 0, ldcCPIndex++);
		} else if (_constantPoolEntries[cfrCPIndex].isReferenced) {
			U_8 cpTag = getCPTag(cfrCPIndex);
			switch (cpTag) {
				case CFR_CONSTANT_Double: /* fall through */
				case CFR_CONSTANT_Long:
					_romConstantPoolEntries[doubleSlotIndex] = cfrCPIndex;
					_romConstantPoolTypes[doubleSlotIndex] = cpTypeMap[cpTag];
					SET_ROM_CP_INDEX(cfrCPIndex, 0, doubleSlotIndex++);
					break;
				case CFR_CONSTANT_InterfaceMethodref: /* fall through */
				case CFR_CONSTANT_Methodref: {
					if (isStaticSplit(cfrCPIndex)) {
						_staticSplitEntries[staticSplitTableIndex] = romCPIndex;
						_constantPoolEntries[cfrCPIndex].staticSplitTableIndex = staticSplitTableIndex;
						staticSplitTableIndex += 1;
					}
					if (isSpecialSplit(cfrCPIndex)) {
						_specialSplitEntries[specialSplitTableIndex] = romCPIndex;
						_constantPoolEntries[cfrCPIndex].specialSplitTableIndex = specialSplitTableIndex;
						specialSplitTableIndex += 1;
					}					
					_romConstantPoolEntries[romCPIndex] = cfrCPIndex;
					if (isMarked(cfrCPIndex, INVOKE_INTERFACE)) {
						_romConstantPoolTypes[romCPIndex] = J9CPTYPE_INTERFACE_METHOD;
					} else if (isMarked(cfrCPIndex, INVOKE_SPECIAL)) {
						if (CFR_CONSTANT_InterfaceMethodref == cpTag) {
							_romConstantPoolTypes[romCPIndex] = J9CPTYPE_INTERFACE_INSTANCE_METHOD;
						} else {
							_romConstantPoolTypes[romCPIndex] = J9CPTYPE_INSTANCE_METHOD;
						}
					} else if (isMarked(cfrCPIndex, INVOKE_HANDLEEXACT) || isMarked(cfrCPIndex, INVOKE_HANDLEGENERIC)) {
						_romConstantPoolTypes[romCPIndex] = J9CPTYPE_HANDLE_METHOD;
					} else if (isMarked(cfrCPIndex, INVOKE_STATIC)) {
						if (CFR_CONSTANT_InterfaceMethodref == cpTag) {
							_romConstantPoolTypes[romCPIndex] = J9CPTYPE_INTERFACE_STATIC_METHOD;
						} else {
							_romConstantPoolTypes[romCPIndex] = J9CPTYPE_STATIC_METHOD;
						}

					} else if (isMarked(cfrCPIndex, INVOKE_VIRTUAL)) {
						_romConstantPoolTypes[romCPIndex] = J9CPTYPE_INSTANCE_METHOD;
					} else {
						Trc_BCU_Assert_ShouldNeverHappen();
					}
					SET_ROM_CP_INDEX(cfrCPIndex, 0, romCPIndex++);
					break;
				}
				case CFR_CONSTANT_Fieldref: /* fall through */
				case CFR_CONSTANT_Class: /* fall through */
				case CFR_CONSTANT_String: /* fall through */
				case CFR_CONSTANT_Integer: /* fall through */
				case CFR_CONSTANT_Float: /* fall through */
				case CFR_CONSTANT_MethodHandle: /* fall through */
				case CFR_CONSTANT_MethodType: /* fall through */
				case CFR_CONSTANT_Dynamic:
					_romConstantPoolEntries[romCPIndex] = cfrCPIndex;
					_romConstantPoolTypes[romCPIndex] = cpTypeMap[cpTag];
					SET_ROM_CP_INDEX(cfrCPIndex, 0, romCPIndex++);
					break;
				case CFR_CONSTANT_InvokeDynamic:
		   			/*
					 * Update the starting indexing for this InvokeDynamic constant. Index will be used by
		   			 * the BytecodeFixups in the writeBytecodes when dealing with invokedynamic
		   			 */
					SET_ROM_CP_INDEX(cfrCPIndex, 0, currentCallSiteIndex);
		   			currentCallSiteIndex += _constantPoolEntries[cfrCPIndex].callSiteReferenceCount;
					break;
				case CFR_CONSTANT_NameAndType:
					/* Ignore */
					break;
				case CFR_CONSTANT_Utf8:
					if (isMarked(cfrCPIndex, ANNOTATION)) {
						_romConstantPoolEntries[romCPIndex] = cfrCPIndex;
						_romConstantPoolTypes[romCPIndex] = cpTypeMap[cpTag];
						SET_ROM_CP_INDEX(cfrCPIndex, 0, romCPIndex++);
					}
					break;
				default:
					Trc_BCU_Assert_ShouldNeverHappen();
					break;
			}
		}
	}
}

bool
ConstantPoolMap::isVarHandleMethod(U_32 classIndex, U_32 nasIndex) {
	bool result = false;
	U_16 classNameLength = _classFileOracle->getUTF8Length(getCPSlot1(classIndex));

	if ((sizeof(VARHANDLE_CLASS_NAME) - 1) == classNameLength) {
		const char *classNameData = (const char *)_classFileOracle->getUTF8Data(getCPSlot1(classIndex));
		if (0 == strcmp(classNameData, VARHANDLE_CLASS_NAME)) {
			U_32 methodNameIndex = getCPSlot1(nasIndex);
			U_16 methodNameLength = _classFileOracle->getUTF8Length(methodNameIndex);
			const char *methodNameData = (const char *)_classFileOracle->getUTF8Data(methodNameIndex);

			switch (methodNameLength) {
			case 3:
				if ((0 == strcmp(methodNameData, "get"))
				 || (0 == strcmp(methodNameData, "set"))
				) {
					result = true;
				}
				break;
			case 9:
				if ((0 == strcmp(methodNameData, "getOpaque"))
				 || (0 == strcmp(methodNameData, "setOpaque"))
				 || (0 == strcmp(methodNameData, "getAndSet"))
				 || (0 == strcmp(methodNameData, "getAndAdd"))
				) {
					result = true;
				}
				break;
			case 10:
				if ((0 == strcmp(methodNameData, "getAcquire"))
				 || (0 == strcmp(methodNameData, "setRelease"))
				) {
					result = true;
				}
				break;
			case 11:
				if ((0 == strcmp(methodNameData, "getVolatile"))
				 || (0 == strcmp(methodNameData, "setVolatile"))
				) {
					result = true;
				}
				break;
			case 16:
				if ((0 == strcmp(methodNameData, "getAndSetAcquire"))
				 || (0 == strcmp(methodNameData, "getAndSetRelease"))
				 || (0 == strcmp(methodNameData, "getAndAddAcquire"))
				 || (0 == strcmp(methodNameData, "getAndAddRelease"))
				 || (0 == strcmp(methodNameData, "getAndBitwiseAnd"))
				 || (0 == strcmp(methodNameData, "getAndBitwiseXor"))
				) {
					result = TRUE;
				}
				break;
			case 22:
				if ((0 == strcmp(methodNameData, "getAndBitwiseOrAcquire"))
				 || (0 == strcmp(methodNameData, "getAndBitwiseOrRelease"))
				 || (0 == strcmp(methodNameData, "weakCompareAndSetPlain"))
				) {
					result = TRUE;
				}
				break;
			case 23:
				if ((0 == strcmp(methodNameData, "getAndBitwiseAndAcquire"))
				 || (0 == strcmp(methodNameData, "getAndBitwiseAndRelease"))
				 || (0 == strcmp(methodNameData, "getAndBitwiseXorAcquire"))
				 || (0 == strcmp(methodNameData, "getAndBitwiseXorRelease"))
				) {
					result = TRUE;
				}
				break;
			case 24:
				if ((0 == strcmp(methodNameData, "weakCompareAndSetAcquire"))
				 || (0 == strcmp(methodNameData, "weakCompareAndSetRelease"))
				) {
					result = true;
				}
				break;
			case 25:
				if ((0 == strcmp(methodNameData, "compareAndExchangeAcquire"))
				 || (0 == strcmp(methodNameData, "compareAndExchangeRelease"))
				) {
					result = true;
				}
				break;
			default:
				if ((0 == strcmp(methodNameData, "compareAndSet"))
				 || (0 == strcmp(methodNameData, "getAndBitwiseOr"))
				 || (0 == strcmp(methodNameData, "weakCompareAndSet"))
				 || (0 == strcmp(methodNameData, "compareAndExchange"))
				) {
					result = true;
				}
				break;
			}
		}
	}
	return result;
}

void
ConstantPoolMap::findVarHandleMethodRefs()
{
	PORT_ACCESS_FROM_PORT(_context->portLibrary());
	U_16 *varHandleMethodTable = NULL;

	for (U_16 i = 1; i < _romConstantPoolCount; i++) {
		if ((J9CPTYPE_INSTANCE_METHOD == _romConstantPoolTypes[i])
		|| (J9CPTYPE_INTERFACE_INSTANCE_METHOD == _romConstantPoolTypes[i])
		) {
			U_16 cfrCPIndex = _romConstantPoolEntries[i];
			U_32 slot1 = getCPSlot1(cfrCPIndex);
			U_32 slot2 = getCPSlot2(cfrCPIndex);

			if (isVarHandleMethod(slot1, slot2)) {
				if (NULL == varHandleMethodTable) {
					/* Allocate a temporary array for storing indices of VarHandle methodrefs */
					varHandleMethodTable = (U_16*) j9mem_allocate_memory(_romConstantPoolCount * sizeof(U_16), OMRMEM_CATEGORY_VM);
					if (NULL == varHandleMethodTable) {
						_buildResult = OutOfMemory;
						break;
					}
				}
				varHandleMethodTable[_varHandleMethodTypeCount] = i;
				_varHandleMethodTypeCount++;
			}
		}
	}

	/* Store trimmed varHandleMethodTable into _varHandleMethodTypeLookupTable. */
	if (NULL != varHandleMethodTable) {
		_varHandleMethodTypeLookupTable = (U_16*) j9mem_allocate_memory(_varHandleMethodTypeCount * sizeof(U_16), OMRMEM_CATEGORY_VM);
		if (NULL == _varHandleMethodTypeLookupTable) {
			_buildResult = OutOfMemory;
		} else {
			memcpy(_varHandleMethodTypeLookupTable, varHandleMethodTable, _varHandleMethodTypeCount * sizeof(U_16));
		}
		j9mem_free_memory(varHandleMethodTable);
	}
}


void
ConstantPoolMap::constantPoolDo(ConstantPoolVisitor *visitor)
{
	for (UDATA i = 1; i < _romConstantPoolCount; i++) {
		U_16 cfrCPIndex = _romConstantPoolEntries[i];
		U_32 slot1 = getCPSlot1(cfrCPIndex);
		U_32 slot2 = getCPSlot2(cfrCPIndex);

		switch (_romConstantPoolTypes[i]) {
			case J9CPTYPE_INT: /* fall through */
			case J9CPTYPE_FLOAT:
				visitor->visitSingleSlotConstant(slot1);
				break;
			case J9CPTYPE_LONG: /* fall through */
			case J9CPTYPE_DOUBLE:
				visitor->visitDoubleSlotConstant(slot1, slot2);
				break;
			case J9CPTYPE_CLASS:
				visitor->visitClass(slot1);
				break;
			case J9CPTYPE_STRING:
				if (CFR_CONSTANT_Utf8 == getCPTag(cfrCPIndex)) {
					visitor->visitString(cfrCPIndex);
				} else {
					visitor->visitString(slot1);
				}
				break;
			case J9CPTYPE_ANNOTATION_UTF8:
				visitor->visitString(cfrCPIndex);
				break;
			case J9CPTYPE_FIELD: /* fall through */
			case J9CPTYPE_INSTANCE_METHOD: /* fall through */
			case J9CPTYPE_HANDLE_METHOD: /* fall through */
			case J9CPTYPE_STATIC_METHOD: /* fall through */
			case J9CPTYPE_INTERFACE_METHOD: /* fall through */
			case J9CPTYPE_INTERFACE_INSTANCE_METHOD: /* fall through */
			case J9CPTYPE_INTERFACE_STATIC_METHOD:
				visitor->visitFieldOrMethod(getROMClassCPIndexForReference(slot1), U_16(slot2));
				break;
			case J9CPTYPE_METHOD_TYPE:
				if (CFR_CONSTANT_Methodref == getCPTag(cfrCPIndex)) {
					/* Will be a MethodRef { class, nas } - dig out nas->slot2 */
					U_32 methodTypeOriginFlags = 0;
					if (_classFileOracle->getUTF8Length(getCPSlot1(slot2)) == (sizeof("invokeExact") - 1)) {
						methodTypeOriginFlags = J9_METHOD_TYPE_ORIGIN_INVOKE_EXACT;
					} else {
						methodTypeOriginFlags = J9_METHOD_TYPE_ORIGIN_INVOKE;
					}
					visitor->visitMethodType(getCPSlot2(slot2), methodTypeOriginFlags);
				} else {
					visitor->visitMethodType(slot1, J9_METHOD_TYPE_ORIGIN_LDC);
				}
				break;
			case J9CPTYPE_METHODHANDLE:
				visitor->visitMethodHandle(slot1, slot2);
				break;
			case J9CPTYPE_CONSTANT_DYNAMIC:
				/* Check if the return type of constant dynamic entry is a primitive type
				 * Set the J9DescriptionCpPrimitiveType flag so interpreter know to unbox
				 * the resolved object before returning it.
				 */
				{
					char fieldDescriptor = (char)*_classFileOracle->getUTF8Data(getCPSlot2(slot2));
					switch (fieldDescriptor) {
					case 'B':
						visitor->visitConstantDynamic(slot1, slot2, (J9DescriptionReturnTypeByte << J9DescriptionReturnTypeShift));
						break;
					case 'C':
						visitor->visitConstantDynamic(slot1, slot2, (J9DescriptionReturnTypeChar << J9DescriptionReturnTypeShift));
						break;
					case 'D':
						visitor->visitConstantDynamic(slot1, slot2, (J9DescriptionReturnTypeDouble << J9DescriptionReturnTypeShift));
						break;
					case 'F':
						visitor->visitConstantDynamic(slot1, slot2, (J9DescriptionReturnTypeFloat << J9DescriptionReturnTypeShift));
						break;
					case 'I':
						visitor->visitConstantDynamic(slot1, slot2, (J9DescriptionReturnTypeInt << J9DescriptionReturnTypeShift));
						break;
					case 'J':
						visitor->visitConstantDynamic(slot1, slot2, (J9DescriptionReturnTypeLong << J9DescriptionReturnTypeShift));
						break;
					case 'S':
						visitor->visitConstantDynamic(slot1, slot2, (J9DescriptionReturnTypeShort << J9DescriptionReturnTypeShift));
						break;
					case 'Z':
						visitor->visitConstantDynamic(slot1, slot2, (J9DescriptionReturnTypeBoolean << J9DescriptionReturnTypeShift));
						break;
					default:
						visitor->visitConstantDynamic(slot1, slot2, 0);
						break;
					}
				}
				break;
			default:
				Trc_BCU_Assert_ShouldNeverHappen();
				break;
		}
	}
}
