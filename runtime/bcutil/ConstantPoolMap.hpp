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
 * ConstantPoolMap.hpp
 */

#ifndef CONSTANTPOOLMAP_HPP_
#define CONSTANTPOOLMAP_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"
#include "j9.h"
#include "cfr.h"

#include "BuildResult.hpp"
#include "ClassFileOracle.hpp"
#include "ROMClassCreationContext.hpp"

class BufferManager;

/*
 * The ConstantPoolMap class handles mapping from class file constant pool indices
 * to ROM class constant pool indices.
 *
 * The resulting ROM constant pool looks like this.
 *
 * +------------+-------------------+------------------+--------------------+
 * | CP entry 0 | HEADER CP entries | split CP entries | FOOTER CP entries  |
 * +------------+-------------------+------------------+--------------------+
 *
 * The CP entry at index 0 is reserved - as it has a special meaning (e.g. null).
 *
 * The HEADER CP entries are those that must be at the beginning of the constant pool.
 * These are required to be at the beginning because they are indexed by U_8 (and must
 * thus be within the first 256 values). These entries are referred by 'ldc' bytecode,
 * and since only these entries can have objects, storing them together improves
 * the locality of objects in the heap.
 *
 * The FOOTER CP entries are the ones that must be at the end of the constant pool.
 * The FOOTER entries are constants that do not need runtime-resolution, and thus
 * do not get included in the RAM constant pool.
 *
 * All other CP entries are in the middle. A single CP entry in the class file may be split
 * into multiple ROM (and consequently RAM) CP entries. This is done because a single constant
 * pool entry may be used for different purposes, and thus requires a different value when it
 * is resolved in the RAM constant pool. The ROM and RAM constant pools are parallel except
 * for the FOOTER CP entries, which do not get included in the RAM constant pool.
 *
 * For example, suppose that during the marking phase a MethodRef constant pool entry has been
 * marked that it is used by both invoke virtual and invoke special. Since the RAM constant pool
 * contents for a MethodRef entry will be different based on how it's used - it is necessary that
 * the original constant pool entry be split into (in this case) two constant pool entries and
 * that the constant pool indices in the byte code be mapped to the reflect this.
 *
 * Each SPLIT* type (see EntryFlags) indicates a different SPLIT 'slot'. Concrete uses of these
 * are specified by EntryUseFlags. All uses for a single SPLIT type must be mutually exclusive.
 *
 * A split constant pool entry will occupy consecutive constant pool slots for its different
 * splits. For example, if class file constant pool entry 12 was marked with SPLIT1, SPLIT2 and
 * SPLIT4 flags, then it will map to the following indices in the ROM constant pool:
 *
 * 		_constantPoolEntries[12].romCPBaseIndex     for SPLIT1
 * 		_constantPoolEntries[12].romCPBaseIndex + 1 for SPLIT2
 * 		_constantPoolEntries[12].romCPBaseIndex + 2 for SPLIT4
 *
 * The above can be queried by using the getROMClassCPIndex() function after all marking is done.
 */
class ConstantPoolMap
{
public:

	class ConstantPoolVisitor
	{
	public:
		virtual void visitClass(U_16 cfrCPIndex) = 0;
		virtual void visitString(U_16 cfrCPIndex) = 0;
		virtual void visitMethodType(U_16 cfrCPIndex, U_16 forMethodHandleInvocation) = 0;
		virtual void visitMethodHandle(U_16 kind, U_16 cfrCPIndex) = 0;
		virtual void visitConstantDynamic(U_16 bsmIndex, U_16 cfrCPIndex, U_32 primitiveFlag) = 0;
		virtual void visitSingleSlotConstant(U_32 slot1) = 0;
		virtual void visitDoubleSlotConstant(U_32 slot1, U_32 slot2) = 0;
		virtual void visitFieldOrMethod(U_16 classRefCPIndex, U_16 nameAndSignatureCfrCPIndex) = 0;
	};

	class ConstantPoolEntryTypeVisitor
	{
	public:
		virtual void visitEntryType(U_32 entryType) = 0;
	};

	struct CallSiteVisitor
	{
		virtual void visitCallSite(U_16 nameAndSignatureCfrCPIndex, U_16 bootstrapMethodIndex) = 0;
	};

	struct SplitEntryVisitor
	{
		virtual void visitSplitEntry(U_16 cpIndex) = 0;
	};

	ConstantPoolMap(BufferManager *bufferManager, ROMClassCreationContext *context);
	~ConstantPoolMap();

	void setClassFileOracleAndInitialize(ClassFileOracle *classFileOracle);

	void computeConstantPoolMapAndSizes();

	void findVarHandleMethodRefs();

	bool isOK() const { return OK == _buildResult; }
	BuildResult getBuildResult() const { return _buildResult; }

	void constantPoolDo(ConstantPoolVisitor *visitor);
	void constantPoolEntryTypesDo(ConstantPoolEntryTypeVisitor *visitor)
	{
		for (UDATA i = 1; i < _romConstantPoolCount; i++) {
			visitor->visitEntryType(_romConstantPoolTypes[i]);
		}
	}

	void callSitesDo(CallSiteVisitor *visitor)
	{
		U_16 constantPoolCount = _classFileOracle->getConstantPoolCount();
		for (U_16 i = 0; i < constantPoolCount; ++i) {
			U_32 callSiteReferenceCount = _constantPoolEntries[i].callSiteReferenceCount;
			for (U_32 j = 0; j < callSiteReferenceCount; ++j) {
				visitor->visitCallSite(U_16(getCPSlot2(i)), U_16(getCPSlot1(i)));
			}
		}
	}

	void staticSplitEntriesDo(SplitEntryVisitor *visitor)
	{
		for (U_16 i = 0; i < _staticSplitEntryCount; i++) {
			visitor->visitSplitEntry(_staticSplitEntries[i]);
		}
	}

	void specialSplitEntriesDo(SplitEntryVisitor *visitor)
	{
		for (U_16 i = 0; i < _specialSplitEntryCount; i++) {
			visitor->visitSplitEntry(_specialSplitEntries[i]);
		}
	}

	U_32 getMethodTypeCount() const { return _methodTypeCount; }
	U_32 getVarHandleMethodTypeCount() const { return _varHandleMethodTypeCount; }
	U_32 getVarHandleMethodTypePaddedCount() const { return _varHandleMethodTypeCount + (_varHandleMethodTypeCount & 0x1); /* Rounding up to an even number */ }
	U_16 *getVarHandleMethodTypeLookupTable() const { return _varHandleMethodTypeLookupTable; }
	U_32 getCallSiteCount() const { return _callSiteCount; }
	U_16 getRAMConstantPoolCount() const { return _ramConstantPoolCount; }
	U_16 getROMConstantPoolCount() const { return _romConstantPoolCount; }
	U_16 getStaticSplitEntryCount() const { return _staticSplitEntryCount; }
	U_16 getSpecialSplitEntryCount() const { return _specialSplitEntryCount; }

	U_16 getROMClassCPIndex(U_16 cfrCPIndex, UDATA splitType) const
	{
		return _constantPoolEntries[cfrCPIndex].romCPIndex;
	}
	U_16 getROMClassCPIndex(U_16 cfrCPIndex) const { return getROMClassCPIndex(cfrCPIndex, 0); }

	/*
	 * Note: InvokeDynamicInfo CP entries are not marked in the same way as other CP entries. In particular,
	 * isMarked(cfrCPIndex, INVOKE_DYNAMIC) is always false. The correct check is
	 * computeConstantPoolMapAndSizes, is (0 != _constantPoolEntries[cfrCPIndex].callSiteReferenceCount).
	 */
	U_16 getCallSiteIndex(U_16 cfrCPIndex)
	{
		U_16 romClassCPIndex = getROMClassCPIndex(cfrCPIndex);
		U_16 index = _constantPoolEntries[cfrCPIndex].currentCallSiteIndex++;
		Trc_BCU_Assert_True(index < _constantPoolEntries[cfrCPIndex].callSiteReferenceCount);
		return romClassCPIndex + index;
	}

	U_16 getStaticSplitTableIndex(U_16 cfrCPIndex) { return _constantPoolEntries[cfrCPIndex].staticSplitTableIndex; }
	U_16 getSpecialSplitTableIndex(U_16 cfrCPIndex) { return _constantPoolEntries[cfrCPIndex].specialSplitTableIndex; }

	void mark(U_16 cfrCPIndex) { _constantPoolEntries[cfrCPIndex].isReferenced = true; }
	void mark(U_16 cfrCPIndex, UDATA useType)
	{
		mark(cfrCPIndex);
		_constantPoolEntries[cfrCPIndex].isUsedBy[useType] = true;
	}

	bool isMarked(U_16 cfrCPIndex) const { return _constantPoolEntries[cfrCPIndex].isReferenced; }
	bool isMarked(U_16 cfrCPIndex, UDATA useType) const { return _constantPoolEntries[cfrCPIndex].isUsedBy[useType]; }

	U_8  getCPTag(U_16 cfrCPIndex)   const { return _classFileOracle->getCPTag(cfrCPIndex); }
	U_32 getCPSlot1(U_16 cfrCPIndex) const { return _classFileOracle->getCPSlot1(cfrCPIndex); }
	U_32 getCPSlot2(U_16 cfrCPIndex) const { return _classFileOracle->getCPSlot2(cfrCPIndex); }

	U_16 getROMClassCPIndexForReference(U_16 cfrCPIndex)  const { return getROMClassCPIndex(cfrCPIndex); }
	U_16 getROMClassCPIndexForAnnotation(U_16 cfrCPIndex) const { return getROMClassCPIndex(cfrCPIndex); }

	bool isUTF8ConstantReferenced(U_16 cfrCPIndex) const { return isMarked(cfrCPIndex); }
	bool isNATConstantReferenced(U_16 cfrCPIndex)  const { return isMarked(cfrCPIndex); }

	bool isStaticSplit(U_16 cfrCPIndex) const
	{
		/* A CP index needs to be added to static split side table if any of the following is true:
		 * 	1. it is shared between 'invokestatic' and 'invokeinterface' bytecodes,
		 * 	2. it is shared between 'invokestatic' and 'invokespecial' bytecodes
		 *
		 * First condition is required to support 'default' methods introduced as part of JSR 335.
		 * Sharing of CP index between 'invokestatic' and 'invokespecial' can also be handled using the side table mechanism
		 * instead of marking it as J9CPTYPE_SHARED_METHOD, and hence the second condition.
		 * See Jazz103 Design 40047.
		 */
		return (isMarked(cfrCPIndex, INVOKE_STATIC)
				&& (_context->alwaysSplitBytecodes()
					|| isMarked(cfrCPIndex, INVOKE_INTERFACE)
					|| isMarked(cfrCPIndex, INVOKE_SPECIAL)
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
					|| isMarked(cfrCPIndex, INVOKE_VIRTUAL)
#endif /* J9VM_OPT_VALHALLA_NESTMATES */
				));
	}

	bool isSpecialSplit(U_16 cfrCPIndex) const
	{
		/* A constant pool index needs to be added to special split table if
		 * it is referred by 'invokespecial' and 'invokeinterface' bytecodes.
		 * This is required to support 'default' methods introduced as part of JSR 335.
		 * See Jazz103 Design 40047.
		 */
		return (isMarked(cfrCPIndex, INVOKE_SPECIAL)
				&& (_context->alwaysSplitBytecodes()
					|| isMarked(cfrCPIndex, INVOKE_INTERFACE)
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
					|| isMarked(cfrCPIndex, INVOKE_VIRTUAL)
#endif /* J9VM_OPT_VALHALLA_NESTMATES */
				));
	}

	bool hasStaticSplitTable() const { return _staticSplitEntryCount != 0; }
	bool hasSpecialSplitTable() const { return _specialSplitEntryCount != 0; }

	bool hasCallSites() const { return 0 != _callSiteCount; }
	bool hasVarHandleMethodRefs() const { return 0 != _varHandleMethodTypeCount; }

	void markConstantAsReferencedDoubleSlot(U_16 cfrCPIndex) { mark(cfrCPIndex); }
	void markConstantAsUsedByLDC(U_8 cfrCPIndex) { _constantPoolEntries[cfrCPIndex].isUsedByLDC = true; }

	void markConstantAsReferenced(U_16 cfrCPIndex)            { mark(cfrCPIndex, REFERENCED); }

	void markConstantUTF8AsReferenced(U_16 cfrCPIndex)        { mark(cfrCPIndex); }
	void markConstantNameAndTypeAsReferenced(U_16 cfrCPIndex) { mark(cfrCPIndex); }

	void markConstantAsUsedByAnnotationUTF8(U_16 cfrCPIndex)  { mark(cfrCPIndex, ANNOTATION); }

	void markClassAsUsedByInstanceOf(U_16 classCfrCPIndex)     { mark(classCfrCPIndex, INSTANCE_OF); }
	void markClassAsUsedByCheckCast(U_16 classCfrCPIndex)      { mark(classCfrCPIndex, CHECK_CAST); }
	void markClassAsUsedByMultiANewArray(U_16 classCfrCPIndex) { mark(classCfrCPIndex, MULTI_ANEW_ARRAY); }
	void markClassAsUsedByANewArray(U_16 classCfrCPIndex)      { mark(classCfrCPIndex, ANEW_ARRAY); }
	void markClassAsUsedByNew(U_16 classCfrCPIndex)            { mark(classCfrCPIndex, NEW); }
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	void markClassAsUsedByDefaultValue(U_16 classCfrCPIndex)    { mark(classCfrCPIndex, DEFAULT_VALUE); }
	void markFieldRefAsUsedByWithField(U_16 fieldRefCfrCPIndex) { mark(fieldRefCfrCPIndex, WITH_FIELD); }
#endif

	void markFieldRefAsUsedByGetStatic(U_16 fieldRefCfrCPIndex) { mark(fieldRefCfrCPIndex, GET_STATIC); }
	void markFieldRefAsUsedByPutStatic(U_16 fieldRefCfrCPIndex) { mark(fieldRefCfrCPIndex, PUT_STATIC); }
	void markFieldRefAsUsedByGetField(U_16 fieldRefCfrCPIndex)  { mark(fieldRefCfrCPIndex, GET_FIELD); }
	void markFieldRefAsUsedByPutField(U_16 fieldRefCfrCPIndex)  { mark(fieldRefCfrCPIndex, PUT_FIELD); }

	void markMethodRefAsUsedByInvokeVirtual(U_16 methodRefCfrCPIndex)       { mark(methodRefCfrCPIndex, INVOKE_VIRTUAL); }
	void markMethodRefAsUsedByInvokeSpecial(U_16 methodRefCfrCPIndex)       { mark(methodRefCfrCPIndex, INVOKE_SPECIAL); }
	void markMethodRefAsUsedByInvokeStatic(U_16 methodRefCfrCPIndex)        { mark(methodRefCfrCPIndex, INVOKE_STATIC); }
	void markMethodRefAsUsedByInvokeInterface(U_16 methodRefCfrCPIndex)     { mark(methodRefCfrCPIndex, INVOKE_INTERFACE); }
	void markMethodRefAsUsedByInvokeHandle(U_16 methodRefCfrCPIndex)        { mark(methodRefCfrCPIndex, INVOKE_HANDLEEXACT); _methodTypeCount++; }
	void markMethodRefAsUsedByInvokeHandleGeneric(U_16 methodRefCfrCPIndex) { mark(methodRefCfrCPIndex, INVOKE_HANDLEGENERIC); _methodTypeCount++; }

	void markInvokeDynamicInfoAsUsedByInvokeDynamic(U_16 cfrCPIndex)
	{
		/*
		 * Note: Unlike the other markFooAsUsedByBar(U_16 cfrCPIndex) methods above, uses of InvokeDynamicInfo CP entries are not marked
		 *       with mark(cfrCPIndex, INVOKE_DYNAMIC) because the romCPBaseIndex for these entries is an index into the callSites table
		 *       instead of the ROM Constant Pool. Also see computeConstantPoolMapAndSizes.
		 */
		mark(cfrCPIndex);
		_constantPoolEntries[cfrCPIndex].callSiteReferenceCount++;
		++_callSiteCount;
	}

private:
	bool isVarHandleMethod(U_32 classIndex, U_32 nasIndex);

	/* TODO turn EntryFlags into static const UDATAs instead of an enum type that is never used */

	/* Indices into the ConstantPoolEntry.flags bool array. See comment at the top of the file for more info. */
	enum EntryFlags
	{
		SPLIT1 = 0,
		SPLIT2 = 1,
		SPLIT3 = 2,
		SPLIT4 = 3,
		SPLIT5 = 4,
		/* SPLIT_FLAG_COUNT is the number of possible split types. */
		SPLIT_FLAG_COUNT = 5
	};

	struct ConstantPoolEntry
	{
		/* If callSiteReferenceCount > 0 then romCPIndexes[0] is the Call Site base index for this entry. */
		U_32 callSiteReferenceCount;
		U_16 currentCallSiteIndex;
		U_16 staticSplitTableIndex; /* maps this cpIndex to J9ROMClass.staticSplitMethodRefIndexes */
		U_16 specialSplitTableIndex; /* maps this cpIndex to J9ROMClass.specialSplitMethodRefIndexes */
		U_16 romCPIndex;
		bool isUsedBy[SPLIT_FLAG_COUNT];
		bool isReferenced;
		bool isUsedByLDC;
	};

	ROMClassCreationContext *_context;
	ClassFileOracle *_classFileOracle;
	BufferManager *_bufferManager;

	ConstantPoolEntry *_constantPoolEntries;
	U_16 *_romConstantPoolEntries;
	U_8 *_romConstantPoolTypes;
	U_16 *_staticSplitEntries;
	U_16 *_specialSplitEntries;
	U_32 _methodTypeCount;
	U_16 _varHandleMethodTypeCount;
	U_16 *_varHandleMethodTypeLookupTable;
	U_32 _callSiteCount;
	U_16 _ramConstantPoolCount;
	U_16 _romConstantPoolCount;
	U_16 _staticSplitEntryCount;
	U_16 _specialSplitEntryCount;
	BuildResult _buildResult;

public:

	/* Specific flags that are aliases for EntryFlags constants. See comment at the top of the file for more info. */
	enum EntryUseFlags
	{
		REFERENCED = SPLIT1,
		LDC2W = SPLIT1,
		PUT_STATIC = SPLIT1,
		GET_STATIC = SPLIT1,
		PUT_FIELD = SPLIT1,
		GET_FIELD = SPLIT1,
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		DEFAULT_VALUE = SPLIT1,
		WITH_FIELD = SPLIT1,
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		NEW = SPLIT1,
		INVOKE_HANDLEGENERIC = SPLIT5,
		INVOKE_HANDLEEXACT = SPLIT5,
		INVOKE_STATIC = SPLIT4,
		INVOKE_SPECIAL = SPLIT3,
		INVOKE_VIRTUAL = SPLIT2,
		INVOKE_INTERFACE = SPLIT1,
		ANEW_ARRAY = SPLIT1,
		CHECK_CAST = SPLIT1,
		INSTANCE_OF = SPLIT1,
		MULTI_ANEW_ARRAY = SPLIT1,
		ANNOTATION = SPLIT1,
		/* INVOKE_DYNAMIC does not use a constant pool entry. It is used only for ClassFileOracle::BytecodeFixupEntry.type. */
		INVOKE_DYNAMIC = SPLIT_FLAG_COUNT,
		/* LDC does not use a constant pool entry. It is used only for ClassFileOracle::BytecodeFixupEntry.type. */
		LDC = SPLIT_FLAG_COUNT + 1
	};

};

#endif /* CONSTANTPOOLMAP_HPP_ */
