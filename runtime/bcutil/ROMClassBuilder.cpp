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
 * ROMClassBuilder.cpp
 */

#include "ut_j9bcu.h"
#include "bcutil_api.h"
#include "stackmap_api.h"
#include "SCQueryFunctions.h"

#include "ROMClassBuilder.hpp"

#include "AllocationStrategy.hpp"
#include "BufferManager.hpp"
#include "ClassFileOracle.hpp"
#include "ClassFileParser.hpp"
#include "ClassFileWriter.hpp"
#include "ConstantPoolMap.hpp"
#include "ComparingCursor.hpp"
#include "Cursor.hpp"
#include "J9PortAllocationStrategy.hpp"
#include "ROMClassCreationContext.hpp"
#include "ROMClassStringInternManager.hpp"
#include "ROMClassSegmentAllocationStrategy.hpp"
#include "ROMClassVerbosePhase.hpp"
#include "ROMClassWriter.hpp"
#include "SCStoreTransaction.hpp"
#include "SCStringTransaction.hpp"
#include "SRPKeyProducer.hpp"
#include "SuppliedBufferAllocationStrategy.hpp"
#include "WritingCursor.hpp"
#include "j9protos.h"
#include "ut_j9bcu.h"

static const UDATA INITIAL_CLASS_FILE_BUFFER_SIZE = 4096;
static const UDATA INITIAL_BUFFER_MANAGER_SIZE = 32768 * 10;

ROMClassBuilder::ROMClassBuilder(J9JavaVM *javaVM, J9PortLibrary *portLibrary, UDATA maxStringInternTableSize, U_8 * verifyExcludeAttribute, VerifyClassFunction verifyClassFunction) :
	_javaVM(javaVM),
	_portLibrary(portLibrary),
	_verifyExcludeAttribute(verifyExcludeAttribute),
	_verifyClassFunction(verifyClassFunction),
	_classFileParserBufferSize(INITIAL_CLASS_FILE_BUFFER_SIZE),
	_bufferManagerSize(INITIAL_BUFFER_MANAGER_SIZE),
	_classFileBuffer(NULL),
	_bufferManagerBuffer(NULL),
	_anonClassNameBuffer(NULL),
	_anonClassNameBufferSize(0),
	_stringInternTable(javaVM, portLibrary, maxStringInternTableSize)
{
}

ROMClassBuilder::~ROMClassBuilder()
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	if (_javaVM != NULL && _javaVM->dynamicLoadBuffers != NULL) {
		if (_javaVM->dynamicLoadBuffers->classFileError == _classFileBuffer) {
			/* Set dynamicLoadBuffers->classFileError to NULL to prevent a
			 * crash in j9bcutil_freeAllTranslationBuffers() on JVM shutdown
			 */
			_javaVM->dynamicLoadBuffers->classFileError = NULL;
		}
	}
	j9mem_free_memory(_classFileBuffer);
	j9mem_free_memory(_bufferManagerBuffer);
	j9mem_free_memory(_anonClassNameBuffer);
}

ROMClassBuilder *
ROMClassBuilder::getROMClassBuilder(J9PortLibrary *portLibrary, J9JavaVM *vm)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	ROMClassBuilder *romClassBuilder = (ROMClassBuilder *)vm->dynamicLoadBuffers->romClassBuilder;
	if ( NULL == romClassBuilder ) {
		romClassBuilder = (ROMClassBuilder *)j9mem_allocate_memory(sizeof(ROMClassBuilder), J9MEM_CATEGORY_CLASSES);
		if ( NULL != romClassBuilder ) {
			J9BytecodeVerificationData * verifyBuffers =  vm->bytecodeVerificationData;
			new(romClassBuilder) ROMClassBuilder(vm, portLibrary,
					vm->maxInvariantLocalTableNodeCount,
					(NULL == verifyBuffers ? NULL : verifyBuffers->excludeAttribute),
					(NULL == verifyBuffers ? NULL : j9bcv_verifyClassStructure));
			if (romClassBuilder->isOK()) {
				ROMClassBuilder **romClassBuilderPtr = (ROMClassBuilder **)&(vm->dynamicLoadBuffers->romClassBuilder);
				*romClassBuilderPtr = romClassBuilder;
			} else {
				romClassBuilder->~ROMClassBuilder();
				j9mem_free_memory(romClassBuilder);
				romClassBuilder = NULL;
			}
		}
	}
	return romClassBuilder;
}

extern "C" void
shutdownROMClassBuilder(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	ROMClassBuilder *romClassBuilder = (ROMClassBuilder *)vm->dynamicLoadBuffers->romClassBuilder;
	if ( NULL != romClassBuilder ) {
		vm->dynamicLoadBuffers->romClassBuilder = NULL;
		romClassBuilder->~ROMClassBuilder();
		j9mem_free_memory(romClassBuilder);
	}
}

#if defined(J9DYN_TEST)
extern "C"  IDATA
j9bcutil_compareRomClass(
		U_8 * classFileBytes,
		U_32 classFileSize,
		J9PortLibrary * portLib,
		struct J9BytecodeVerificationData * verifyBuffers,
		UDATA bctFlags,
		UDATA bcuFlags,
		J9ROMClass *romClass)
{
	ROMClassBuilder romClassBuilder(NULL, portLib, 0, NULL == verifyBuffers ? NULL : verifyBuffers->excludeAttribute, NULL == verifyBuffers ? NULL : j9bcv_verifyClassStructure);
	ROMClassCreationContext context(portLib, classFileBytes, classFileSize, bctFlags, bcuFlags, romClass);
	return (IDATA)romClassBuilder.buildROMClass(&context);
}
#endif

extern "C"  IDATA
j9bcutil_buildRomClassIntoBuffer(
		U_8 * classFileBytes,
		UDATA classFileSize,
		J9PortLibrary * portLib,
		J9BytecodeVerificationData * verifyBuffers,
		UDATA bctFlags,
		UDATA bcuFlags,
		UDATA findClassFlags,
		U_8 * romSegment,
		UDATA romSegmentSize,
		U_8 * lineNumberBuffer,
		UDATA lineNumberBufferSize,
		U_8 * varInfoBuffer,
		UDATA varInfoBufferSize,
		U_8 ** classFileBufferPtr
)
{
	SuppliedBufferAllocationStrategy suppliedBufferAllocationStrategy(romSegment, romSegmentSize, lineNumberBuffer, lineNumberBufferSize, varInfoBuffer, varInfoBufferSize);
	ROMClassBuilder romClassBuilder(NULL, portLib, 0, NULL == verifyBuffers ? NULL : verifyBuffers->excludeAttribute, NULL == verifyBuffers ? NULL : j9bcv_verifyClassStructure);
	ROMClassCreationContext context(portLib, classFileBytes, classFileSize, bctFlags, bcuFlags, findClassFlags, &suppliedBufferAllocationStrategy);
	IDATA result = IDATA(romClassBuilder.buildROMClass(&context));
	if (NULL != classFileBufferPtr) {
		*classFileBufferPtr = romClassBuilder.releaseClassFileBuffer();
	}
	return result;
}


extern "C"  IDATA
j9bcutil_buildRomClass(J9LoadROMClassData *loadData, U_8 * intermediateData, UDATA intermediateDataLength, J9JavaVM *javaVM, UDATA bctFlags, UDATA classFileBytesReplaced, UDATA isIntermediateROMClass, J9TranslationLocalBuffer *localBuffer)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	UDATA bcuFlags = javaVM->dynamicLoadBuffers->flags;
	UDATA findClassFlags = loadData->options;

	ROMClassSegmentAllocationStrategy romClassSegmentAllocationStrategy(javaVM, loadData->classLoader);
	ROMClassBuilder *romClassBuilder = ROMClassBuilder::getROMClassBuilder(PORTLIB, javaVM);
	if (NULL == romClassBuilder) {
		return BCT_ERR_OUT_OF_MEMORY;
	}

	ROMClassCreationContext context(
			PORTLIB, javaVM, loadData->classData, loadData->classDataLength, bctFlags, bcuFlags, findClassFlags, &romClassSegmentAllocationStrategy,
			loadData->className, loadData->classNameLength, intermediateData, (U_32) intermediateDataLength, loadData->romClass, loadData->classBeingRedefined,
			loadData->classLoader, (0 != classFileBytesReplaced), (TRUE == isIntermediateROMClass), localBuffer);

	BuildResult result = romClassBuilder->buildROMClass(&context);
	loadData->romClass = context.romClass();
	context.reportStatistics(localBuffer);

	return IDATA(result);
}

extern "C" IDATA
j9bcutil_transformROMClass(J9JavaVM *javaVM, J9PortLibrary *portLibrary, J9ROMClass *romClass, U_8 **classData, U_32 *size)
{
	ClassFileWriter classFileWriter(javaVM, portLibrary, romClass);
	if (classFileWriter.isOK()) {
		*size = (U_32) classFileWriter.classFileSize();
		*classData = classFileWriter.classFileData();
	}

	return IDATA(classFileWriter.getResult());
}


BuildResult
ROMClassBuilder::buildROMClass(ROMClassCreationContext *context)
{
	BuildResult result = OK;
	ROMClassVerbosePhase v0(context, ROMClassCreation, &result);

	context->recordLoadStart();

	context->recordParseClassFileStart();
	ClassFileParser classFileParser(_portLibrary, _verifyClassFunction);
	result = classFileParser.parseClassFile(context, &_classFileParserBufferSize, &_classFileBuffer);
	context->recordParseClassFileEnd();

	if ( OK == result ) {
		ROMClassVerbosePhase v1(context, ROMClassTranslation, &result);
		context->recordTranslationStart();
		result = OutOfMemory;
		while( OutOfMemory == result ) {
			BufferManager bufferManager = BufferManager(_portLibrary, _bufferManagerSize, &_bufferManagerBuffer);
			if (!bufferManager.isOK()) {
				/*
				 * not enough native memory to complete this ROMClass load
				 */
				break;
			}
			result = prepareAndLaydown( &bufferManager, &classFileParser, context );
			if (OutOfMemory == result) {
				context->recordOutOfMemory(_bufferManagerSize);
				/* Restore the original method bytecodes, as we may have transformed them. */
				classFileParser.restoreOriginalMethodBytecodes();
				/* set up new bufferSize for top of loop */
				_bufferManagerSize = _bufferManagerSize * 2;
			}
		}
	}
	if ( OK == result ) {
		context->recordTranslationEnd();
	}

	context->recordLoadEnd(result);
	return result;
}

BuildResult
ROMClassBuilder::handleAnonClassName(J9CfrClassFile *classfile)
{
	J9CfrConstantPoolInfo* constantPool = classfile->constantPool;
	U_32 cpThisClassUTF8Slot = constantPool[classfile->thisClass].slot1;
	U_32 originalStringLength = constantPool[cpThisClassUTF8Slot].slot1;
	const char* originalStringBytes = (const char*)constantPool[cpThisClassUTF8Slot].bytes;
	U_16 newUtfCPEntry = classfile->constantPoolCount - 1; /* last cpEntry is reserved for anonClassName utf */
	U_32 i = 0;
	BOOLEAN stringOrNASReferenceToClassName = FALSE;
	BOOLEAN newCPEntry = TRUE;
	UDATA newAnonClassNameLength = originalStringLength + 1 + ROM_ADDRESS_LENGTH + 1;
	BuildResult result = OK;
	PORT_ACCESS_FROM_PORT(_portLibrary);

	/* Find if there are any Constant_String or CFR_CONSTANT_NameAndType references to the className.
	 * If there are none we don't need to make a new cpEntry, we can overwrite the existing
	 * one since the only reference to it is the classRef
	 */
	for (i = 0; i < classfile->constantPoolCount; i++) {
		if ((CFR_CONSTANT_String == constantPool[i].tag)
			|| (CFR_CONSTANT_NameAndType == constantPool[i].tag)
		) {
			if (cpThisClassUTF8Slot == constantPool[i].slot1) {
				stringOrNASReferenceToClassName = TRUE;
			}
		}
	}


	if (!stringOrNASReferenceToClassName) {
		/* do not need the new cpEntry so fix up classfile->constantPoolCount */
		newCPEntry = FALSE;
		newUtfCPEntry = cpThisClassUTF8Slot;
		classfile->constantPoolCount -= 1;
	}

	J9CfrConstantPoolInfo *anonClassName = &classfile->constantPool[newUtfCPEntry];
	/*
	 * alloc an array for the new name with the following format:
	 * [className]/[ROMClassAddress]\0
	 */

	if ((0 == _anonClassNameBufferSize) || (newAnonClassNameLength > _anonClassNameBufferSize)) {
		j9mem_free_memory(_anonClassNameBuffer);
		_anonClassNameBuffer = (U_8 *)j9mem_allocate_memory(newAnonClassNameLength, J9MEM_CATEGORY_CLASSES);
		if (NULL == _anonClassNameBuffer) {
			result = OutOfMemory;
			goto done;
		}
		_anonClassNameBufferSize = newAnonClassNameLength;
	}
	constantPool[newUtfCPEntry].bytes = _anonClassNameBuffer;

	if (newCPEntry) {
		constantPool[classfile->lastUTF8CPIndex].nextCPIndex = newUtfCPEntry;
	}

	/* calculate the size of the new string and create new cpEntry*/
	anonClassName->slot1 = originalStringLength + ROM_ADDRESS_LENGTH + 1;
	if (newCPEntry) {
		anonClassName->slot2 = 0;
		anonClassName->tag = CFR_CONSTANT_Utf8;
		anonClassName->flags1 = 0;
		anonClassName->nextCPIndex = 0;
		anonClassName->romAddress = 0;
	}

	constantPool[classfile->thisClass].slot1 = newUtfCPEntry;

	/* copy the name into the new location and add the special character, fill the rest with zeroes */
	memcpy (constantPool[newUtfCPEntry].bytes, originalStringBytes, originalStringLength);
	*(U_8*)((UDATA) constantPool[newUtfCPEntry].bytes + originalStringLength) = ANON_CLASSNAME_CHARACTER_SEPARATOR;
	memset(constantPool[newUtfCPEntry].bytes + originalStringLength + 1, '0', ROM_ADDRESS_LENGTH);
	*(U_8*)((UDATA) constantPool[newUtfCPEntry].bytes + originalStringLength + 1 + ROM_ADDRESS_LENGTH) = '\0';

	/* search constantpool for all other identical classRefs. We have not actually
	 * tested this scenario as javac will not output more than one classRef or utfRef of the
	 * same kind.
	 */
	for (i = 0; i < classfile->constantPoolCount; i++) {
		if (CFR_CONSTANT_Class == constantPool[i].tag) {
			U_32 classNameSlot = constantPool[i].slot1;
			if (classNameSlot != newUtfCPEntry) {
				U_32 classNameLength = constantPool[classNameSlot].slot1;
				if ((classNameLength == originalStringLength)
					&& (0 == strncmp(originalStringBytes, (const char*)constantPool[classNameSlot].bytes, originalStringLength)))
				{
					/* if it is the same class, point to original class name slot */
					constantPool[i].slot1 = newUtfCPEntry;
				}
			}
		}
	}

done:
	return result;
}

U_8 *
ROMClassBuilder::releaseClassFileBuffer()
{
	U_8 *result = _classFileBuffer;
	_classFileBuffer = NULL;
	return result;
}
void
ROMClassBuilder::getSizeInfo(ROMClassCreationContext *context, ROMClassWriter *romClassWriter, SRPOffsetTable *srpOffsetTable, bool *countDebugDataOutOfLine, SizeInformation *sizeInformation)
{
		/* create a new scope to allow the ROMClassVerbosePhase to properly record the amount of time spent in
		 * preparation */
		ROMClassVerbosePhase v(context, PrepareROMClass);
		Cursor mainAreaCursor(RC_TAG, srpOffsetTable, context);
		Cursor lineNumberCursor(LINE_NUMBER_TAG, srpOffsetTable, context);
		Cursor variableInfoCursor(VARIABLE_INFO_TAG, srpOffsetTable, context);
		Cursor utf8Cursor(UTF8_TAG, srpOffsetTable, context);
		Cursor classDataCursor(INTERMEDIATE_TAG, srpOffsetTable, context);
		/*
		 * The need to have separate lineNumber and variableInfo Cursors from mainAreaCursor only exists
		 * if it is possible to place debug information out of line.  That is currently only done when
		 * shared classes is enabled and it is possible to share the class OR when the allocation strategy
		 * permits it.
		 */
		if (context->canPossiblyStoreDebugInfoOutOfLine()) {
			/* It's possible that debug information can be stored out of line.
			 * Calculate sizes and offsets with out of line debug information.*/
			*countDebugDataOutOfLine = true;
			romClassWriter
			  ->writeROMClass(&mainAreaCursor,
					&lineNumberCursor,
					&variableInfoCursor,
					&utf8Cursor,
					(context->isIntermediateDataAClassfile()) ? &classDataCursor : NULL,
					0, 0, 0, 0,
					ROMClassWriter::MARK_AND_COUNT_ONLY);
		} else {
			context->forceDebugDataInLine();
			romClassWriter
			  ->writeROMClass(&mainAreaCursor,
					&mainAreaCursor,
					&mainAreaCursor,
					&utf8Cursor,
					(context->isIntermediateDataAClassfile()) ? &classDataCursor : NULL,
					0, 0, 0, 0,
					ROMClassWriter::MARK_AND_COUNT_ONLY);
		}
		sizeInformation->rcWithOutUTF8sSize = mainAreaCursor.getCount();
		sizeInformation->lineNumberSize = lineNumberCursor.getCount();
		sizeInformation->variableInfoSize = variableInfoCursor.getCount();
		sizeInformation->utf8sSize = utf8Cursor.getCount();
		/* In case of intermediateData being stored as ROMClass, rawClassDataSize will be 0. */
		sizeInformation->rawClassDataSize = classDataCursor.getCount();
		sizeInformation->varHandleMethodTypeLookupTableSize = romClassWriter->getVarHandleMethodTypePaddedSize();
}

BuildResult
ROMClassBuilder::prepareAndLaydown( BufferManager *bufferManager, ClassFileParser *classFileParser, ROMClassCreationContext *context )
{
	bool countDebugDataOutOfLine = false;
	BuildResult result = OK;
	ROMClassVerbosePhase v0(context, ROMClassPrepareAndLayDown, &result);
	PORT_ACCESS_FROM_PORT(_portLibrary);
	/*
	 * If retransforming is enabled, intermediate class data is laid down after the first corresponding ROMClass.
	 * When a ROMClass is retransformed, the intermediateClassData of the new ROMClass points to the data laid down
	 * after the old ROMClass. Hence we only lay down intermediate class data if retransforming is enabled, but we
	 * are not currently retransforming.
	 *
	 * With -Xshareclasses:enableBCI sub-option, intermediateClassData is laid down for every ROMClass which is not
	 * modified by the BCI agent.
	 */
	Trc_BCU_Assert_False(context->isRetransforming() && !context->isRetransformAllowed());

	if (context->isClassAnon()) {
		BuildResult res = handleAnonClassName(classFileParser->getParsedClassFile());
		if (OK != res) {
			return res;
		}
	}


	ConstantPoolMap constantPoolMap(bufferManager, context);
	ClassFileOracle classFileOracle(bufferManager, classFileParser->getParsedClassFile(), &constantPoolMap, _verifyExcludeAttribute, context);
	if ( !classFileOracle.isOK() ) {
		return classFileOracle.getBuildResult();
	}

	SRPKeyProducer srpKeyProducer(&classFileOracle);

	/*
	 * The ROMClassWriter must be constructed before the SRPOffsetTable because it generates additional SRP keys.
	 * There must be no SRP keys generated after the SRPOffsetTable is initialized.
	 */
	ROMClassWriter romClassWriter(bufferManager, &classFileOracle, &srpKeyProducer, &constantPoolMap, context);
	if ( !romClassWriter.isOK() ) {
		return romClassWriter.getBuildResult();
	}
	SRPOffsetTable srpOffsetTable(&srpKeyProducer, bufferManager, MAX_TAG, context);
	if ( !srpOffsetTable.isOK() ) {
		return srpOffsetTable.getBuildResult();
	}

	/* Pass the SRPOffsetTable to the ROMClassWriter to complete its initialization. */
	romClassWriter.setSRPOffsetTable(&srpOffsetTable);

	U_32 modifiers = classFileOracle.getAccessFlags();
	U_32 extraModifiers = computeExtraModifiers(&classFileOracle, context);
	U_32 optionalFlags = computeOptionalFlags(&classFileOracle, context);

	/*
	 * calculate the amount of space required to write out this ROMClass without UTF8s
	 * and calculate the maximum amount of space required for UTF8s
	 * also prepare the SRP offset information
	 */
	SizeInformation sizeInformation;
	getSizeInfo(context, &romClassWriter, &srpOffsetTable, &countDebugDataOutOfLine, &sizeInformation);

	if ( context->shouldCompareROMClassForEquality() ) {
		ROMClassVerbosePhase v(context, CompareHashtableROMClass);

		/*
		 * Check if the supplied ROMClass is identical to the one being created. If it is, simply return OK.
		 *
		 * This is done either when there is an orphan ROM class (without a RAM class) that has been created
		 * previously, or for ROMClass comparison on behalf of dyntest.
		 */
		J9ROMClass *romClass = context->romClass();
		bool romClassIsShared = (j9shr_Query_IsAddressInCache(_javaVM, romClass, romClass->romSize) ? true : false);

		if (compareROMClassForEquality((U_8*)romClass, romClassIsShared, &romClassWriter,
				&srpOffsetTable, &srpKeyProducer, &classFileOracle, modifiers, extraModifiers, optionalFlags, context)
		) {
			return OK;
		} else {
			/* ROM classes not equal so remove from creation context */
			context->recordROMClass(NULL);
			/* need to recalculate size info and srp offsets, as comparing may have added bogus debug info */
			srpOffsetTable.clear();
			getSizeInfo(context, &romClassWriter, &srpOffsetTable, &countDebugDataOutOfLine, &sizeInformation);
		}

#if defined(J9DYN_TEST)
		if (NULL == context->allocationStrategy()) {
			/*
			 * No allocationStrategy is a dyntest request to compare to the existing ROMClass supplied in romClassPtr.
			 */
			return GenericError;
		}
#endif
	}

	UDATA maxRequiredSize = sizeInformation.rcWithOutUTF8sSize +
			sizeInformation.lineNumberSize +
			sizeInformation.variableInfoSize +
			sizeInformation.utf8sSize +
			sizeInformation.rawClassDataSize +
			sizeInformation.varHandleMethodTypeLookupTableSize;

#if defined(J9VM_OPT_SHARED_CLASSES)
	if (context->isROMClassShareable()) {
		UDATA loadType = J9SHR_LOADTYPE_NORMAL;
		if (context->isRedefining()) {
			loadType = J9SHR_LOADTYPE_REDEFINED;
		} else if (context->isRetransforming()) {
			loadType = J9SHR_LOADTYPE_RETRANSFORMED;
		}

		SCStoreTransaction sharedStoreClassTransaction =
				SCStoreTransaction(context->currentVMThread(),
				                   context->classLoader(),
				                   context->cpIndex(),
				                   loadType,
				                   classFileOracle.getUTF8Length(classFileOracle.getClassNameIndex()), classFileOracle.getUTF8Data(classFileOracle.getClassNameIndex()),
				                   context->classFileBytesReplaced(),
				                   context->isCreatingIntermediateROMClass());

		if ( sharedStoreClassTransaction.isOK() ) {
			/*
			 * Shared Classes is enabled, there may be an existing ROMClass
			 * that can be used in place of the one being created.
			 *
			 * Attempt to find it.
			 * 
			 * Note: When comparing classes it is expected that the context contains the 
			 * rom class being compared to. 'prevROMClass' is used to backup the romClass 
			 * currently in the context, so the compare loop can set the romClass in the 
			 * context accordingly.
			 */
			J9ROMClass * prevROMClass = context->romClass();
			for (
			   J9ROMClass *existingROMClass = sharedStoreClassTransaction.nextSharedClassForCompare();
			   NULL != existingROMClass;
			   existingROMClass = sharedStoreClassTransaction.nextSharedClassForCompare()
			) {
				ROMClassVerbosePhase v(context, CompareSharedROMClass);
				if (!context->isIntermediateDataAClassfile()
					&& ((U_8 *)existingROMClass == context->intermediateClassData())
				) {
					/* 'existingROMClass' is same as the ROMClass corresponding to intermediate class data.
					 * Based on the assumption that an agent is actually modifying the class file
					 * instead of just returning a copy of the classbytes it receives,
					 * this comparison can be avoided.
					 */
					continue;
				}
				context->recordROMClass(existingROMClass);
				if (compareROMClassForEquality((U_8*)existingROMClass, /* romClassIsShared = */ true, &romClassWriter,
						&srpOffsetTable, &srpKeyProducer, &classFileOracle, modifiers, extraModifiers, optionalFlags, context)
				) {
					/*
					 * Found an existing ROMClass in the shared cache that is equivalent
					 * to the ROMClass that was about to be created.
					 * Make the found ROMClass available to our caller and leave the routine.
					 */
					/* no need to checkDebugInfoCompression when class comes from sharedClassCache since the romClass
					 * was validated when in was placed in the sharedClassCache */
					return OK;
				}
			}
			context->recordROMClass(prevROMClass);

			/*
			 * A shared ROMClass equivalent to the one being created was NOT found.
			 *
			 * Attempt to obtain space in the shared cache to laydown (i.e., share)
			 * the ROMClass being created.
			 */
			ROMClassVerbosePhase v(context, CreateSharedClass);
			J9RomClassRequirements sizeRequirements;

			sizeRequirements.romClassMinimalSize =
					U_32(sizeInformation.rcWithOutUTF8sSize
					+ sizeInformation.utf8sSize + sizeInformation.rawClassDataSize + sizeInformation.varHandleMethodTypeLookupTableSize);
			/* round up to sizeof(U_64) */
			sizeRequirements.romClassMinimalSize += (sizeof(U_64) - 1);
			sizeRequirements.romClassMinimalSize &= ~(sizeof(U_64) - 1);

			sizeRequirements.romClassSizeFullSize =
					U_32(sizeRequirements.romClassMinimalSize
					+ sizeInformation.lineNumberSize
					+ sizeInformation.variableInfoSize);
			/* round up to sizeof(U_64) */
			sizeRequirements.romClassSizeFullSize += (sizeof(U_64) - 1);
			sizeRequirements.romClassSizeFullSize &= ~(sizeof(U_64)-1);


			sizeRequirements.lineNumberTableSize = U_32(sizeInformation.lineNumberSize);
			sizeRequirements.localVariableTableSize = U_32(sizeInformation.variableInfoSize);

			if ( sharedStoreClassTransaction.allocateSharedClass(&sizeRequirements) ){
				U_8 *romClassBuffer = (U_8*)sharedStoreClassTransaction.getRomClass();
				/*
				 * Make note that the laydown is occurring in SharedClasses
				 */
				U_32 romSize = finishPrepareAndLaydown(
						(U_8*)sharedStoreClassTransaction.getRomClass(),
						(U_8*)sharedStoreClassTransaction.getLineNumberTable(),
						(U_8*)sharedStoreClassTransaction.getLocalVariableTable(),
						&sizeInformation, modifiers, extraModifiers, optionalFlags,
						true, sharedStoreClassTransaction.hasSharedStringTableLock(),
						&classFileOracle, &srpOffsetTable, &srpKeyProducer, &romClassWriter,
						context, &constantPoolMap
						);

				fixReturnBytecodes(_portLibrary, (J9ROMClass *)romClassBuffer);

				/*
				 * inform the shared class transaction what the final ROMSize is
				 */
				sharedStoreClassTransaction.updateSharedClassSize(romSize);
				context->recordROMClass((J9ROMClass *)romClassBuffer);
				if ((NULL != _javaVM) && (_javaVM->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_CHECK_DEBUG_INFO_COMPRESSION)) {
					checkDebugInfoCompression((J9ROMClass *)romClassBuffer, classFileOracle, &srpKeyProducer, &constantPoolMap, &srpOffsetTable);
				}
				return OK;
			}
		}

		/*
		 * There is insufficient space available in the shared cache
		 * to accommodate the ROMClass that is being built.
		 *
		 * Terminate the attempted Store Class Transaction.
		 *
		 * Simply exit the scope. This will invoke the destructor for
		 * sharedStoreClassTransaction and terminate the transaction.
		 */
	}

	if (context->isIntermediateDataAClassfile()) {
		/* For some reason we are not storing ROMClass in the cache.
		 * In case we earlier decided to store Intermediate Class File bytes along with ROMClass,
		 * need to check if we still need them. If not, modify sizeRequirements accordingly.
		 *
		 * We don't need to store Intermediate Class File if we fail to store ROMClass in the cache
		 * when running with -Xshareclasses:enableBCI and class file bytes are not modified and re-transformation is not enabled.
		 */
		if (context->shouldStripIntermediateClassData()) {
			maxRequiredSize -= sizeInformation.rawClassDataSize;
			sizeInformation.rawClassDataSize = 0;
		}

		/* In case ROMClass is not stored in cache, and we are re-transforming,
		 * try to re-use intermediateClassData from existing ROMClass.
		 */
		if (false == context->isReusingIntermediateClassData()) {
			romClassWriter.reuseIntermediateClassData();
			/* Modify size requirements if we are able to reuse intermediate class data now */
			if (true == context->isReusingIntermediateClassData()) {
				maxRequiredSize -= sizeInformation.rawClassDataSize;
				sizeInformation.rawClassDataSize = 0;
			}
		}
	}
#endif /* J9VM_OPT_SHARED_CLASSES */
	/*
	 * Shared Classes is disabled, unavailable, or failed; use the regular allocation strategy.
	 */

	/*
	 * request the maximum amount of space to laydown this ROMClass
	 */
	U_8 *romClassBuffer = NULL;
	U_8 *lineNumberBuffer = NULL;
	U_8 *variableInfoBuffer = NULL;
	AllocationStrategy::AllocatedBuffers allocatedBuffers;

	if ( context->allocationStrategy()->allocateWithOutOfLineData( &allocatedBuffers,
			sizeInformation.rcWithOutUTF8sSize + sizeInformation.utf8sSize + sizeInformation.rawClassDataSize + sizeInformation.varHandleMethodTypeLookupTableSize,
			sizeInformation.lineNumberSize, sizeInformation.variableInfoSize)
	) {
		romClassBuffer = allocatedBuffers.romClassBuffer;
		lineNumberBuffer = allocatedBuffers.lineNumberBuffer;
		variableInfoBuffer = allocatedBuffers.variableInfoBuffer;
	} else {
		if (countDebugDataOutOfLine) {
			/* Pad maxRequiredSize to the size to sizeof(U_64) in order to prevent memory corruption.  
			 * 
			 * In rare cases it is possible that maxRequiredSize is 2 bytes too small, due to
			 * counting the data 'out-of-line', and then writing the debug data 'in-line' with 
			 * padding U_64 adjustments. The extra 2 bytes is a result of a switch from U_16
			 * alignment for debug data (when writing out of line), to U_32 alignment (when 
			 * writing inline). It should also be noted that these extra 2 bytes are only 
			 * visible when only one method with debug data exists. If there are more, then the removal 
			 * of 'SRPs to debug data' from each method will hide the extra padding for in-line 
			 * debug data.
			 * 
			 * Without this padding any new segment returned by the below call to allocate could be 
			 * too small to contain the rom class that is about to be written.
			 * 
			 * Note: When the problem described above occurs maxRequiredSize will not be U_64 aligned 
			 *       until the below code runs.
			 */
			UDATA alignmentBits = (sizeof (U_64) - 1);
			UDATA adjustedCount = (maxRequiredSize + alignmentBits) & ~alignmentBits;
			UDATA bytesToPad = adjustedCount - maxRequiredSize;
			if (0 != bytesToPad) {
				maxRequiredSize += bytesToPad;
			}
		}
		romClassBuffer = context->allocationStrategy()->allocate(maxRequiredSize);
	}
	if ( romClassBuffer == NULL ) {
		return OutOfROM;
	}

	/*
	 *  Use an if statement here and call finishPrepareAndLaydown() in both cases to allow the scope of SCStringTransaction() to survive the life of the call to
	 *  finishPrepareAndLaydown(). Otherwise, the scope of SCStringTransaction() would end early and it would not be safe to us interned strings.
	 */
	U_32 romSize = 0;
	if (J9_ARE_ALL_BITS_SET(context->findClassFlags(), J9_FINDCLASS_FLAG_ANON)) {
		U_16 classNameIndex = classFileOracle.getClassNameIndex();
		U_8* classNameBytes = classFileOracle.getUTF8Data(classNameIndex);
		U_16 classNameFullLength =  classFileOracle.getUTF8Length(classNameIndex);
		U_16 classNameRealLenghth = classNameFullLength - ROM_ADDRESS_LENGTH;
		char* nameString = NULL;
		char message[ROM_ADDRESS_LENGTH + 1];
		if (J9_ARE_ALL_BITS_SET(context->findClassFlags(), J9_FINDCLASS_FLAG_REDEFINING)
			|| J9_ARE_ALL_BITS_SET(context->findClassFlags(), J9_FINDCLASS_FLAG_RETRANSFORMING)
		) {
			/* When redefining we need to use the original class name */
			nameString = ((char*) context->className() + classNameRealLenghth);
		} else {
			/* fix up the ROM className with segment Address 
			 * write the name into a buffer first because j9str_printf automatically adds a NULL terminator
			 * at the end, and J9UTF8 are not NULL terminated
			 */
			j9str_printf(PORTLIB, message, ROM_ADDRESS_LENGTH + 1, ROM_ADDRESS_FORMAT, (UDATA)romClassBuffer);
			nameString = (char*) message;
		}
		memcpy((char*) (classNameBytes + classNameRealLenghth), nameString, ROM_ADDRESS_LENGTH);
	}

#if defined(J9VM_OPT_SHARED_CLASSES)
	if (NULL != context->javaVM()) {
		SCStringTransaction scStringTransaction = SCStringTransaction(context->currentVMThread());
		romSize = finishPrepareAndLaydown(romClassBuffer, lineNumberBuffer, variableInfoBuffer, &sizeInformation, modifiers, extraModifiers, optionalFlags,
				false, scStringTransaction.isOK(), &classFileOracle, &srpOffsetTable, &srpKeyProducer, &romClassWriter, context, &constantPoolMap);
	} else
#endif
	{
		romSize = finishPrepareAndLaydown(romClassBuffer, lineNumberBuffer, variableInfoBuffer, &sizeInformation, modifiers, extraModifiers, optionalFlags,
				false, false, &classFileOracle, &srpOffsetTable, &srpKeyProducer, &romClassWriter, context, &constantPoolMap);
	}

	/* This assert will detect memory corruption when a new segment
	 * for the ROM class was allocated using maxRequiredSize.
	 */
	Trc_BCU_Assert_True(romSize <= maxRequiredSize);

	/*
	 * inform the allocator what the final ROMSize is
	 */
	if (J9_ARE_ALL_BITS_SET(context->findClassFlags(), J9_FINDCLASS_FLAG_ANON)) {
		/* for anonClasses lie about the size report that it is full so no  one else can use the segment */
		romSize = (U_32) ((ROMClassSegmentAllocationStrategy*) context->allocationStrategy())->getSegmentSize();
	}
	context->allocationStrategy()->updateFinalROMSize(romSize);

	context->recordROMClass((J9ROMClass *)romClassBuffer);


	if ((NULL != _javaVM) && (_javaVM->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_CHECK_DEBUG_INFO_COMPRESSION)) {
		checkDebugInfoCompression((J9ROMClass *)romClassBuffer, classFileOracle, &srpKeyProducer, &constantPoolMap, &srpOffsetTable);
	}

	return OK;
}

/*
 * Test the compression of the ROMclass'debug information when running the command
 * Tests the line number compression and the local variable table compression
 * -Xcheck:vm:debuginfo
 */
void
ROMClassBuilder::checkDebugInfoCompression(J9ROMClass *romClass, ClassFileOracle classFileOracle, SRPKeyProducer *srpKeyProducer, ConstantPoolMap *constantPoolMap, SRPOffsetTable *srpOffsetTable)
{
    PORT_ACCESS_FROM_PORT(_portLibrary);
    J9ROMMethod *currentMethod;
    currentMethod = (J9ROMMethod*)(J9ROMCLASS_ROMMETHODS(romClass));
    for (ClassFileOracle::MethodIterator methodIterator = classFileOracle.getMethodIterator();
				methodIterator.isNotDone();
				methodIterator.next()) {

			/* 1) Test the line number compression */
		    UDATA lineNumbersInfoSize = methodIterator.getLineNumbersCount() * sizeof (J9CfrLineNumberTableEntry);
		    if (0 != lineNumbersInfoSize) {
		    	J9CfrLineNumberTableEntry *lineNumbersInfo = (J9CfrLineNumberTableEntry*)j9mem_allocate_memory(lineNumbersInfoSize, J9MEM_CATEGORY_CLASSES);
				if (NULL != lineNumbersInfo) {
					classFileOracle.sortLineNumberTable(methodIterator.getIndex(), lineNumbersInfo);
					J9LineNumber lineNumber;
					lineNumber.lineNumber = 0;
					lineNumber.location = 0;

					J9MethodDebugInfo *methodInfo = getMethodDebugInfoFromROMMethod(currentMethod);
					if (NULL != methodInfo) {
						U_8 *currentLineNumber = getLineNumberTable(methodInfo);
						U_32 lineNumbersCount = getLineNumberCount(methodInfo);
						UDATA index;
						Trc_BCU_Assert_CorrectLineNumbersCount(lineNumbersCount, methodIterator.getLineNumbersCount());

						if (0 != lineNumbersCount) {
							for (index = 0; index < lineNumbersCount; index++) {
								/* From the original class */
								U_32 pcOriginal = lineNumbersInfo[index].startPC;
								U_32 lineNumberOriginal = lineNumbersInfo[index].lineNumber;

								/* From the compressed class */
								getNextLineNumberFromTable(&currentLineNumber, &lineNumber);
								if ((lineNumber.lineNumber != lineNumberOriginal) || (lineNumber.location != pcOriginal)) {
									J9UTF8* name = J9ROMCLASS_CLASSNAME(romClass);
									PORT_ACCESS_FROM_PORT(_portLibrary);
									j9tty_printf(PORTLIB, "Error while uncompressing the debug information for the class %.*s\n", (UDATA)J9UTF8_LENGTH(name), J9UTF8_DATA(name));
									j9tty_printf(PORTLIB, "lineNumber.lineNumber(%d) / lineNumberOriginal(%d)\n", lineNumber.lineNumber,lineNumberOriginal);
									j9tty_printf(PORTLIB, "lineNumber.location(%d) / pcOriginal(%d)\n", lineNumber.location, pcOriginal);
									Trc_BCU_Assert_ShouldNeverHappen_CompressionMissmatch();
								}
							}
						}
					}
					j9mem_free_memory(lineNumbersInfo);
				} else {
					Trc_BCU_Assert_Compression_OutOfMemory();
				}
		    }
		    /* 2) Test local variable table table compression */
		    U_32 localVariablesCount = methodIterator.getLocalVariablesCount();
			if (0 != localVariablesCount) {
				J9MethodDebugInfo *methodDebugInfo = getMethodDebugInfoFromROMMethod(currentMethod);
				if (NULL != methodDebugInfo) {
					J9VariableInfoWalkState state;
					J9VariableInfoValues *values = NULL;

					Trc_BCU_Assert_Equals_Level1(methodDebugInfo->varInfoCount, localVariablesCount);

					values = variableInfoStartDo(methodDebugInfo, &state);
					if (NULL != values) {
						for (ClassFileOracle::LocalVariablesIterator localVariablesIterator = methodIterator.getLocalVariablesIterator();
							localVariablesIterator.isNotDone();
							localVariablesIterator.next()) {
							if (NULL == values) {
								/* The number of compressed variableTableInfo is less than the original number */
								Trc_BCU_Assert_ShouldNeverHappen_CompressionMissmatch();
							}
							Trc_BCU_Assert_Equals_Level1(values->startVisibility, localVariablesIterator.getStartPC());
							Trc_BCU_Assert_Equals_Level1(values->visibilityLength, localVariablesIterator.getLength());
							Trc_BCU_Assert_Equals_Level1(values->slotNumber, localVariablesIterator.getIndex());

							Trc_BCU_Assert_Equals_Level1(
									(J9UTF8*)(values->name),
									(J9UTF8*)(srpOffsetTable->computeWSRP(srpKeyProducer->mapCfrConstantPoolIndexToKey(localVariablesIterator.getNameIndex()), 0))
								);
							Trc_BCU_Assert_Equals_Level1(
									(J9UTF8*)(values->signature),
									(J9UTF8*)(srpOffsetTable->computeWSRP(srpKeyProducer->mapCfrConstantPoolIndexToKey(localVariablesIterator.getDescriptorIndex()), 0))
								);
							if (localVariablesIterator.hasGenericSignature()) {
								Trc_BCU_Assert_Equals_Level1(
										(J9UTF8*)(values->genericSignature),
										(J9UTF8*)(srpOffsetTable->computeWSRP(srpKeyProducer->mapCfrConstantPoolIndexToKey(localVariablesIterator.getGenericSignatureIndex()), 0))
									);
							} else {
								Trc_BCU_Assert_Equals_Level1((J9UTF8*)(values->genericSignature), NULL);
							}

							values = variableInfoNextDo(&state);
						}
					}
				}
			}

		    /* 3) next method */
			currentMethod = nextROMMethod(currentMethod);
		}
}

U_32
ROMClassBuilder::finishPrepareAndLaydown(
		U_8 *romClassBuffer,
		U_8 *lineNumberBuffer,
		U_8 *variableInfoBuffer,
		SizeInformation *sizeInformation,
		U_32 modifiers,
		U_32 extraModifiers,
		U_32 optionalFlags,
		bool sharingROMClass,
		bool hasStringTableLock,
		ClassFileOracle *classFileOracle,
		SRPOffsetTable *srpOffsetTable,
		SRPKeyProducer *srpKeyProducer,
		ROMClassWriter *romClassWriter,
		ROMClassCreationContext *context,
		ConstantPoolMap *constantPoolMap)
{
	U_8 * romClassBufferEndAddress = romClassBuffer + sizeInformation->rcWithOutUTF8sSize + sizeInformation->utf8sSize + sizeInformation->rawClassDataSize;
	ROMClassStringInternManager internManager(
			context,
			&_stringInternTable,
			srpOffsetTable,
			srpKeyProducer,
			romClassBuffer,
			romClassBufferEndAddress,
			sharingROMClass,
			hasStringTableLock);

	/*
	 * Identify interned strings.
	 */
	if (internManager.isInterningEnabled()) {
		ROMClassVerbosePhase v(context, WalkUTF8sAndMarkInterns);
		/**
		 * It is not common that part of shared cache is in the SRP range of ROM class, and other part is not.
		 * But it is possible.
		 * For instance, on AIX platforms, shared cache is always not in SRP range of local ROM classes.
		 * On the other hand, for linux machines, shared cache is in SRP range of local ROM class very mostly.
		 * Therefore;
		 * 	 if shared cache is totally out of the SRP range of local ROM classes, then local ROM classes can not use UTF8s in shared cache.
		 * 	 if shared cache is totally in the SRP range of local ROM classes, then local ROM classes can use UTF8s in shared cache safely.
		 *   if part of shared cache is in the SRP range, and part of it is not,
		 *   then for each UTF8 in the local ROM class, SRP range check is required in order to use shared UTFs in the shared cache.
		 *
		 * 	Values of sharedCacheSRPRangeInfo:
		 *  1: (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE) Shared cache is out of the SRP range
		 *  2: (SC_COMPLETELY_IN_THE_SRP_RANGE) Shared cache is in the SRP range
		 *  3: (SC_PARTIALLY_IN_THE_SRP_RANGE)	Part of shared cache is in the SRP range, part of it is not.
		 *
		 *  Shared cache is always in the SRP range of any address in shared cache.
		 */
		SharedCacheRangeInfo sharedCacheSRPRangeInfo = SC_COMPLETELY_IN_THE_SRP_RANGE;
#if defined(J9VM_OPT_SHARED_CLASSES)
#if defined(J9VM_ENV_DATA64)
		sharedCacheSRPRangeInfo = SC_PARTIALLY_IN_THE_SRP_RANGE;
		if (!internManager.isSharedROMClass()) {
			UDATA romClassStartRangeCheck = getSharedCacheSRPRangeInfo(romClassBuffer);
			UDATA romClassEndRangeCheck = getSharedCacheSRPRangeInfo(romClassBufferEndAddress);

			if (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE == romClassStartRangeCheck) {
				if (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE == romClassEndRangeCheck) {
					sharedCacheSRPRangeInfo = SC_COMPLETELY_OUT_OF_THE_SRP_RANGE;
				}
			} else if (SC_COMPLETELY_IN_THE_SRP_RANGE == romClassStartRangeCheck ) {
				if (SC_COMPLETELY_IN_THE_SRP_RANGE == romClassEndRangeCheck) {
					sharedCacheSRPRangeInfo = SC_COMPLETELY_IN_THE_SRP_RANGE;
				}
			}
		} else {
			/* Shared cache is always in the range of shared rom classes in it */
			sharedCacheSRPRangeInfo = SC_COMPLETELY_IN_THE_SRP_RANGE;
		}
#endif
#endif
		{
			ROMClassVerbosePhase v(context, VisitUTF8Block);
			for (ClassFileOracle::UTF8Iterator iterator = classFileOracle->getUTF8Iterator();
					iterator.isNotDone();
					iterator.next()) {
				U_16 cpIndex = iterator.getCPIndex();

				if (constantPoolMap->isUTF8ConstantReferenced(cpIndex)) {
					internManager.visitUTF8(cpIndex, iterator.getUTF8Length(), iterator.getUTF8Data(), sharedCacheSRPRangeInfo);
				}
			}
		}
	}

	/*
	 * Determine what kind of recount needs to be performed, either only the UTF8 section or the entire ROMClass.
	 *
	 * The UTF8 section always needs to be when string interning is on.
	 *
	 * The entire ROMClass needs to be when calculating debug information out of line and then NOT sharing the class
	 * in an out of line fashion.
	 */
	if ( (sizeInformation->lineNumberSize > 0) && (NULL == lineNumberBuffer) ) {
		/*
		 * Debug Information has been calculated out of line
		 *  --and--
		 * there is no room for out of line debug information
		 *
		 * Need to recalculate the entire ROMClass.
		 */
		Cursor mainAreaCursor(RC_TAG, srpOffsetTable, context);
		Cursor utf8Cursor(UTF8_TAG, srpOffsetTable, context);
		Cursor classDataCursor(INTERMEDIATE_TAG, srpOffsetTable, context);

		context->forceDebugDataInLine();
		romClassWriter->writeROMClass(&mainAreaCursor,
									&mainAreaCursor,
									&mainAreaCursor,
									&utf8Cursor,
									(context->isIntermediateDataAClassfile()) ? &classDataCursor : NULL,
									0, 0, 0, 0,
									ROMClassWriter::MARK_AND_COUNT_ONLY);

		sizeInformation->rcWithOutUTF8sSize = mainAreaCursor.getCount();
		sizeInformation->lineNumberSize = 0;
		sizeInformation->variableInfoSize = 0;
		sizeInformation->utf8sSize = utf8Cursor.getCount();
		sizeInformation->rawClassDataSize = classDataCursor.getCount();
		sizeInformation->varHandleMethodTypeLookupTableSize = romClassWriter->getVarHandleMethodTypePaddedSize();
	} else if (internManager.isInterningEnabled()) {
		/*
		 * With the interned strings known, do a second pass on the UTF8 block to update SRP offset information
		 * and determine the final size for UTF8s
		 */
		ROMClassVerbosePhase v(context, PrepareUTF8sAfterInternsMarked);
		Cursor utf8Cursor(UTF8_TAG, srpOffsetTable, context);
		romClassWriter->writeUTF8s(&utf8Cursor);
		sizeInformation->utf8sSize = utf8Cursor.getCount();
	}

	/*
	 * Record the romSize as the final size of the ROMClass with interned strings space removed.
	 */
	U_32 romSize = U_32(sizeInformation->rcWithOutUTF8sSize + sizeInformation->utf8sSize + sizeInformation->rawClassDataSize + sizeInformation->varHandleMethodTypeLookupTableSize);
	/* round up to sizeof(U_64) */
	romSize += (sizeof(U_64) - 1);
	romSize &= ~(sizeof(U_64) - 1);

	/*
	 * update the SRP Offset Table with the base addresses for main ROMClass section (RC_TAG),
	 * the UTF8 Block (UTF8_TAG) and the Intermediate Class Data Block (INTERMEDIATE_TAG)
	 */
	srpOffsetTable->setBaseAddressForTag(RC_TAG, romClassBuffer);
	srpOffsetTable->setBaseAddressForTag(UTF8_TAG, romClassBuffer+sizeInformation->rcWithOutUTF8sSize);
	if (context->isIntermediateDataAClassfile()) {
		/* Raw class data is always written inline and the calculation
		 * of where to start writing must be done after string interning is done.
		 */
		srpOffsetTable->setBaseAddressForTag(INTERMEDIATE_TAG, romClassBuffer + sizeInformation->rcWithOutUTF8sSize + sizeInformation->utf8sSize);
	}
	srpOffsetTable->setBaseAddressForTag(LINE_NUMBER_TAG, lineNumberBuffer);
	srpOffsetTable->setBaseAddressForTag(VARIABLE_INFO_TAG, variableInfoBuffer);

	/*
	 * write ROMClass to memory
	 */
	layDownROMClass(romClassWriter, srpOffsetTable, romSize, modifiers, extraModifiers, optionalFlags, &internManager, context, sizeInformation);
	return romSize;
}

/*
 * ROMClass->extraModifiers  what does each bit represent?
 *
 * 0000 0000 0000 0000 0000 0000 0000 0000
 *                                       + UNUSED
 *                                      + UNUSED
 *                                     + UNUSED
 *                                    + UNUSED
 *
 *                                  + UNUSED
 *                                 + UNUSED
 *                                + UNUSED
 *                               + UNUSED
 *
 *                             + UNUSED
 *                            + UNUSED
 *                           + UNUSED
 *                          + AccClassAnonClass
 *
 *                        + AccSynthetic (matches Oracle modifier position)
 *                       + AccClassUseBisectionSearch
 *                      + AccClassInnerClass
 *                     + UNUSED
 *
 *                   + AccClassNeedsStaticConstantInit
 *                  + AccClassIntermediateDataIsClassfile
 *                 + AccClassUnsafe
 *                + AccClassAnnnotionRefersDoubleSlotEntry
 *
 *              + AccClassBytecodesModified
 *             + AccClassHasEmptyFinalize
 *            + AccClassIsUnmodifiable
 *           + AccClassHasVerifyData
 *
 *         + AccClassIsContended
 *        + AccClassHasFinalFields
 *       + AccClassHasClinit
 *      + AccClassHasNonStaticNonAbstractMethods
 *
 *    + (shape)
 *   + (shape)
 *  + AccClassFinalizeNeeded
 * + AccClassCloneable
 */

U_32
ROMClassBuilder::computeExtraModifiers(ClassFileOracle *classFileOracle, ROMClassCreationContext *context)
{
	ROMClassVerbosePhase v(context, ComputeExtraModifiers);

	U_32 modifiers = 0;

	if ( context->isClassUnsafe() ) {
		modifiers |= J9AccClassUnsafe;
	}

	if ( context->isClassAnon() ) {
		modifiers |= J9AccClassAnonClass;
	}

	if ( context->classFileBytesReplaced() ) {
		modifiers |= J9AccClassBytecodesModified;
	}

	if ( classFileOracle->hasFinalFields() ) {
		modifiers |= J9AccClassHasFinalFields;
	}

	if ( classFileOracle->hasNonStaticNonAbstractMethods() ) {
		modifiers |= J9AccClassHasNonStaticNonAbstractMethods;
	}

	if ( classFileOracle->isCloneable() ) {
		modifiers |= J9AccClassCloneable;
	}

	if ( classFileOracle->isClassContended() ) {
		modifiers |= J9AccClassIsContended;
	}

	if ( classFileOracle->isClassUnmodifiable() ) {
		modifiers |= J9AccClassIsUnmodifiable;
	}

	U_32 classNameindex = classFileOracle->getClassNameIndex();

#define WEAK_NAME "java/lang/ref/WeakReference"
#define SOFT_NAME "java/lang/ref/SoftReference"
#define PHANTOM_NAME "java/lang/ref/PhantomReference"
	if ( classFileOracle->isUTF8AtIndexEqualToString(classNameindex, WEAK_NAME, sizeof(WEAK_NAME)) ) {
		modifiers |= J9AccClassReferenceWeak;
	} else if( classFileOracle->isUTF8AtIndexEqualToString(classNameindex, SOFT_NAME, sizeof(SOFT_NAME)) ) {
		modifiers |= J9AccClassReferenceSoft;
	} else if( classFileOracle->isUTF8AtIndexEqualToString(classNameindex, PHANTOM_NAME, sizeof(PHANTOM_NAME)) ) {
		modifiers |= J9AccClassReferencePhantom;
	}
#undef WEAK_NAME
#undef SOFT_NAME
#undef PHANTOM_NAME

	if ( classFileOracle->hasFinalizeMethod() ) {
		if ( classFileOracle->hasEmptyFinalizeMethod() ) {
			/* If finalize() is empty, mark this class so it does not inherit CFR_ACC_FINALIZE_NEEDED from its superclass */
			modifiers |= J9AccClassHasEmptyFinalize;
		} else {
			modifiers |= J9AccClassFinalizeNeeded;
		}
	}

	if ( classFileOracle->getMajorVersion() >= (BCT_Java6MajorVersionShifted >> BCT_MajorClassFileVersionMaskShift) ) {
		/* Expect verify data for Java 6 and later versions */
		modifiers |= J9AccClassHasVerifyData;
	} else {
		/* Detect if there is verify data for at least one method for versions less than Java 6 */
		for (ClassFileOracle::MethodIterator methodIterator = classFileOracle->getMethodIterator();
				methodIterator.isNotDone();
				methodIterator.next()) {
			if ( methodIterator.hasStackMap() ) {
				modifiers |= J9AccClassHasVerifyData;
				break;
			}
		}
	}



	if ( classFileOracle->isSynthetic() ) {
		/* handle the synthetic attribute. In java 1.5 synthetic may be specified in the access flags as well so do not unset bit here */
		//		Trc_BCU_createRomClassEndian_Synthetic(romClass);
		modifiers |= J9AccSynthetic;
	}

	if (classFileOracle->hasClinit()) {
		modifiers |= J9AccClassHasClinit;
	}

	if (classFileOracle->annotationRefersDoubleSlotEntry()) {
		modifiers |= J9AccClassAnnnotionRefersDoubleSlotEntry;
	}

	if (context->isIntermediateDataAClassfile()) {
		modifiers |= J9AccClassIntermediateDataIsClassfile;
	}

	if (context->canRomMethodsBeSorted(classFileOracle->getMethodsCount())) {
		modifiers |= J9AccClassUseBisectionSearch;
	}

	if (classFileOracle->isInnerClass()) {
		modifiers |= J9AccClassInnerClass;
	}

	if (classFileOracle->needsStaticConstantInit()) {
		modifiers |= J9AccClassNeedsStaticConstantInit;
	}

	return modifiers;
}

U_32
ROMClassBuilder::computeOptionalFlags(ClassFileOracle *classFileOracle, ROMClassCreationContext *context)
{
	ROMClassVerbosePhase v(context, ComputeOptionalFlags);

	U_32 optionalFlags = 0;

	if (classFileOracle->hasSourceFile() && context->shouldPreserveSourceFileName()){
		optionalFlags |= J9_ROMCLASS_OPTINFO_SOURCE_FILE_NAME;
	}
	if (classFileOracle->hasGenericSignature()){
		optionalFlags |= J9_ROMCLASS_OPTINFO_GENERIC_SIGNATURE;
	}
	if (classFileOracle->hasSourceDebugExtension() && context->shouldPreserveSourceDebugExtension()){
		optionalFlags |= J9_ROMCLASS_OPTINFO_SOURCE_DEBUG_EXTENSION;
	}
	if (classFileOracle->hasClassAnnotations()) {
		optionalFlags |= J9_ROMCLASS_OPTINFO_CLASS_ANNOTATION_INFO;
	}
	if (classFileOracle->hasTypeAnnotations()) {
		optionalFlags |= J9_ROMCLASS_OPTINFO_TYPE_ANNOTATION_INFO;
	}
	if (classFileOracle->hasEnclosingMethod()){
		optionalFlags |= J9_ROMCLASS_OPTINFO_ENCLOSING_METHOD;
	}
	if (classFileOracle->hasSimpleName()){
		optionalFlags |= J9_ROMCLASS_OPTINFO_SIMPLE_NAME;
	}
	if (classFileOracle->hasVerifyExcludeAttribute()) {
		optionalFlags |= J9_ROMCLASS_OPTINFO_VERIFY_EXCLUDE;
	}
	return optionalFlags;
}

void
ROMClassBuilder::layDownROMClass(
		ROMClassWriter *romClassWriter, SRPOffsetTable *srpOffsetTable, U_32 romSize, U_32 modifiers, U_32 extraModifiers, U_32 optionalFlags,
		ROMClassStringInternManager *internManager, ROMClassCreationContext *context, SizeInformation *sizeInformation)
{
	ROMClassVerbosePhase v(context, LayDownROMClass);
	WritingCursor writingCursor(RC_TAG, srpOffsetTable, internManager, context);
	WritingCursor lineNumberCursor(LINE_NUMBER_TAG, srpOffsetTable, internManager, context);
	WritingCursor variableInfoCursor(VARIABLE_INFO_TAG, srpOffsetTable, internManager, context);
	WritingCursor utf8Cursor(UTF8_TAG, srpOffsetTable, internManager, context);
	WritingCursor classDataCursor(INTERMEDIATE_TAG, srpOffsetTable, internManager, context);
	WritingCursor *lineNumberCursorPtr = &writingCursor;
	WritingCursor *variableInfoCursorPtr = &writingCursor;

	if ( sizeInformation->lineNumberSize > 0 ) {
		lineNumberCursorPtr = &lineNumberCursor;
		variableInfoCursorPtr = &variableInfoCursor;
	} else {
		context->forceDebugDataInLine();
	}

	romClassWriter->writeROMClass(&writingCursor,
			lineNumberCursorPtr,
			variableInfoCursorPtr,
			&utf8Cursor,
			(context->isIntermediateDataAClassfile()) ? &classDataCursor : NULL,
			romSize, modifiers, extraModifiers, optionalFlags,
			ROMClassWriter::WRITE);
}


bool
ROMClassBuilder::compareROMClassForEquality(U_8 *romClass,bool romClassIsShared,
		ROMClassWriter *romClassWriter, SRPOffsetTable *srpOffsetTable, SRPKeyProducer *srpKeyProducer,
		ClassFileOracle *classFileOracle, U_32 modifiers, U_32 extraModifiers, U_32 optionalFlags,
		ROMClassCreationContext * context)
{
	ComparingCursor compareCursor(_javaVM, srpOffsetTable, srpKeyProducer, classFileOracle, romClass, romClassIsShared, context);
	romClassWriter->writeROMClass(&compareCursor,
			&compareCursor,
			&compareCursor,
			NULL,
			NULL,
			0, modifiers, extraModifiers, optionalFlags,
			ROMClassWriter::WRITE);

	return compareCursor.isEqual();
}

#if defined(J9VM_OPT_SHARED_CLASSES)
#if defined(J9VM_ENV_DATA64)
/**
 *	This function checks how much of shared cache is in SRP range of the given address.
 *	This check is done only for 64 bit environments.
 *	@param address	Given address whose SRP range will be checked against shared cache.
 *	@return 1 (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE) if shared cache is totally out of the SRP range.
 *	@return 2 (SC_COMPLETELY_IN_THE_SRP_RANGE) if shared cache is totally in the SRP range.
 *	@return 3 (SC_PARTIALLY_IN_THE_SRP_RANGE) if part of the shared cache is in the SRP range, and part of it is not.
 *
 *
 */
SharedCacheRangeInfo
ROMClassBuilder::getSharedCacheSRPRangeInfo(void *address)
{
	if ((NULL != _javaVM) && (NULL != _javaVM->sharedClassConfig)) {
		J9SharedClassCacheDescriptor *cache = _javaVM->sharedClassConfig->cacheDescriptorList;
		if (NULL == cache) {
			/*
			 * If there is no shared cache,
			 * just return 1:out of range to avoid trying to find utf8 in shared cache in the future
			 */
			return SC_COMPLETELY_OUT_OF_THE_SRP_RANGE;
		}
		/*This is initial value to set the bits to zero*/
		SharedCacheRangeInfo sharedCacheRangeInfo = SC_NO_RANGE_INFO;

		/**
		 * It checks whether shared cache(s) in the SRP range completely, not at all or partially against the specific address passed to this function.
		 * Shared cache(s) are SC_COMPLETELY_IN_THE_SRP_RANGE if and only if all the start and end addresses of shared cache(s) are in the srp range.
		 * Shared cache(s) are SC_COMPLETELY_OUT_OF_THE_SRP_RANGE if and only if all the start and end addresses of shared cache(s) are out of the srp range.
		 * In all other cases, shared cache(s) are SC_PARTIALLY_IN_THE_SRP_RANGE.
		 */
		while (NULL != cache) {
			void * cacheStart = (void *)cache->cacheStartAddress;
			void * cacheEnd = (void *)(((U_8 *)cacheStart) + cache->cacheSizeBytes - 1);

			if (StringInternTable::areAddressesInSRPRange(cacheStart, address)) {
				/* Check whether the end of cache is in the SRP range of the address */
				if (StringInternTable::areAddressesInSRPRange(cacheEnd, address)) {
					/**
					 * Current cache is in SRP range. Check whether previous cache(s) were in range or not if there is any.
					 * If current cache is the first cache in the list, then sharedCacheRangeInfo is set to SC_NO_RANGE_INFO.
					 * If all the previous caches are SC_COMPLETELY_OUT_OF_THE_SRP_RANGE, then return SC_PARTIALLY_IN_THE_SRP_RANGE since current cache is in range.
					 * If they are all SC_COMPLETELY_IN_THE_SRP_RANGE, then set sharedCacheRangeInfo to SC_COMPLETELY_IN_THE_SRP_RANGE
					 *
					 */
					if (sharedCacheRangeInfo == SC_COMPLETELY_OUT_OF_THE_SRP_RANGE) {
						return SC_PARTIALLY_IN_THE_SRP_RANGE;
					} else {
						sharedCacheRangeInfo = SC_COMPLETELY_IN_THE_SRP_RANGE;
					}
				} else {
					return SC_PARTIALLY_IN_THE_SRP_RANGE;
				}
			} else {
				if (StringInternTable::areAddressesInSRPRange(cacheEnd, address)) {
					return SC_PARTIALLY_IN_THE_SRP_RANGE;
				} else {
					/**
					 * Current cache is out of SRP range. Check whether previous cache(s) were in range or not if there is any.
					 * If current cache is the first cache in the list, then sharedCacheRangeInfo is set to SC_NO_RANGE_INFO.
					 * If all the previous caches are SC_PARTIALLY_IN_THE_SRP_RANGE, then return SC_PARTIALLY_IN_THE_SRP_RANGE since current cache is out of range.
					 * If they are all SC_COMPLETELY_OUT_OF_THE_SRP_RANGE, then set sharedCacheRangeInfo to SC_COMPLETELY_OUT_OF_THE_SRP_RANGE
					 *
					 */
					if (sharedCacheRangeInfo == SC_PARTIALLY_IN_THE_SRP_RANGE) {
						return SC_PARTIALLY_IN_THE_SRP_RANGE;
					} else {
						sharedCacheRangeInfo = SC_COMPLETELY_OUT_OF_THE_SRP_RANGE;
					}
				}
			}

			if (cache->next == _javaVM->sharedClassConfig->cacheDescriptorList) {
				/* Break out of the circular descriptor list. */
				break;
			}
				cache = cache->next;
		}

		return sharedCacheRangeInfo;
	}
	 /* 	Either _javaVM or _JavaVM->sharedClassConfig  is null.
	  *		Try doing range check for each UTF8.
	  *		In normal circumstances, we should never be here.
	  */
	return SC_PARTIALLY_IN_THE_SRP_RANGE;
}
#endif
#endif

