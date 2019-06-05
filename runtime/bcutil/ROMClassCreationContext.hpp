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

#ifndef ROMCLASSCREATIONCONTEXT_HPP_
#define ROMCLASSCREATIONCONTEXT_HPP_

#include "bcutil_api.h"
#include "j9.h"
#include "j9comp.h"
#include "j9port.h"
#include "jvminit.h"
#include "rommeth.h"
#include "SCQueryFunctions.h"
#include "ut_j9bcu.h"

#include "AllocationStrategy.hpp"
#include "BuildResult.hpp"
#include "ROMClassCreationPhase.hpp"

class ROMClassCreationContext
{
public:
	ROMClassCreationContext(J9PortLibrary *portLibrary, U_8 *classFileBytes, UDATA classFileSize, UDATA bctFlags, UDATA bcuFlags, J9ROMClass *romClass) :
		_portLibrary(portLibrary),
		_javaVM(NULL),
		_classFileBytes(classFileBytes),
		_classFileSize(classFileSize),
		_bctFlags(bctFlags),
		_bcuFlags(bcuFlags),
		_findClassFlags(0),
		_allocationStrategy(NULL),
		_romClass(romClass),
		_clazz(NULL),
		_className(NULL),
		_classNameLength(0),
		_intermediateClassData(NULL),
		_intermediateClassDataLength(0),
		_classLoader(NULL),
		_cpIndex(0),
		_loadLocation(0),
		_dynamicLoadStats(NULL),
		_sharedStringInternTable(NULL),
		_classFileBytesReplaced(false),
		_retransformAllowed(false),
		_interningEnabled(false),
		_verboseROMClass(false),
		_verboseLastBufferSizeExceeded(0),
		_verboseOutOfMemoryCount(0),
		_verboseCurrentPhase(ROMClassCreation),
		_buildResult(OK),
		_forceDebugDataInLine(false),
		_doDebugCompare(false),
		_existingRomMethod(NULL),
		_reusingIntermediateClassData(false),
		_creatingIntermediateROMClass(false)
	{
	}

	ROMClassCreationContext(J9PortLibrary *portLibrary, U_8 *classFileBytes, UDATA classFileSize, UDATA bctFlags, UDATA bcuFlags, UDATA findClassFlags, AllocationStrategy *allocationStrategy) :
		_portLibrary(portLibrary),
		_javaVM(NULL),
		_classFileBytes(classFileBytes),
		_classFileSize(classFileSize),
		_bctFlags(bctFlags),
		_bcuFlags(bcuFlags),
		_findClassFlags(findClassFlags),
		_allocationStrategy(allocationStrategy),
		_romClass(NULL),
		_clazz(NULL),
		_className(NULL),
		_classNameLength(0),
		_intermediateClassData(NULL),
		_intermediateClassDataLength(0),
		_classLoader(NULL),
		_cpIndex(0),
		_loadLocation(0),
		_dynamicLoadStats(NULL),
		_sharedStringInternTable(NULL),
		_classFileBytesReplaced(false),
		_retransformAllowed(false),
		_interningEnabled(false),
		_verboseROMClass(false),
		_verboseLastBufferSizeExceeded(0),
		_verboseOutOfMemoryCount(0),
		_verboseCurrentPhase(ROMClassCreation),
		_buildResult(OK),
		_forceDebugDataInLine(false),
		_doDebugCompare(false),
		_existingRomMethod(NULL),
		_reusingIntermediateClassData(false),
		_creatingIntermediateROMClass(false)
	{
	}

	ROMClassCreationContext(
			J9PortLibrary *portLibrary, J9JavaVM *javaVM, U_8 *classFileBytes, UDATA classFileSize, UDATA bctFlags, UDATA bcuFlags, UDATA findClassFlags, AllocationStrategy *allocationStrategy,
			U_8 *className, UDATA classNameLength, U_8 *intermediateClassData, U_32 intermediateClassDataLength, J9ROMClass *romClass, J9Class *clazz, J9ClassLoader *classLoader, bool classFileBytesReplaced,
			bool creatingIntermediateROMClass, J9TranslationLocalBuffer *localBuffer) :
		_portLibrary(portLibrary),
		_javaVM(javaVM),
		_classFileBytes(classFileBytes),
		_classFileSize(classFileSize),
		_bctFlags(bctFlags),
		_bcuFlags(bcuFlags),
		_findClassFlags(findClassFlags),
		_allocationStrategy(allocationStrategy),
		_romClass(romClass),
		_clazz(clazz),
		_className(className),
		_classNameLength(classNameLength),
		_intermediateClassData(intermediateClassData),
		_intermediateClassDataLength(intermediateClassDataLength),
		_classLoader(classLoader),
		_cpIndex(0),
		_loadLocation(0),
		_dynamicLoadStats(NULL),
		_sharedStringInternTable(NULL),
		_classFileBytesReplaced(classFileBytesReplaced),
#if defined(J9VM_OPT_JVMTI)
		_retransformAllowed((NULL != javaVM) && (0 != (javaVM->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_ALLOW_RETRANSFORM))),
#else
		_retransformAllowed(false),
#endif
		_interningEnabled(false),
		_verboseROMClass((NULL != javaVM) && (0 != (javaVM->verboseLevel & VERBOSE_ROMCLASS))),
		_verboseLastBufferSizeExceeded(0),
		_verboseOutOfMemoryCount(0),
		_verboseCurrentPhase(ROMClassCreation),
		_buildResult(OK),
		_forceDebugDataInLine(false),
		_doDebugCompare(false),
		_existingRomMethod(NULL),
		_reusingIntermediateClassData(false),
		_creatingIntermediateROMClass(creatingIntermediateROMClass)
	{
		if ((NULL != _javaVM) && (NULL != _javaVM->dynamicLoadBuffers)) {
			/* localBuffer should not be NULL */
			Trc_BCU_Assert_True(NULL != localBuffer);
			_cpIndex = localBuffer->entryIndex;
			_loadLocation = localBuffer->loadLocationType;
			_sharedStringInternTable = _javaVM->sharedInvariantInternTable;
			_interningEnabled = J9_ARE_ALL_BITS_SET(_bcuFlags, BCU_ENABLE_INVARIANT_INTERNING) && J9_ARE_NO_BITS_SET(_findClassFlags, J9_FINDCLASS_FLAG_ANON);
			if (0 != (bcuFlags & BCU_VERBOSE)) {
				_dynamicLoadStats = _javaVM->dynamicLoadBuffers->dynamicLoadStats;
			}
		}
		if (_verboseROMClass) {
			memset(&_verboseRecords[0], 0, sizeof(_verboseRecords));
		}
	}
	
	bool isCreatingIntermediateROMClass() const { return _creatingIntermediateROMClass; }
	U_8 *classFileBytes() const { return _classFileBytes; }
	UDATA classFileSize() const { return _classFileSize; }
	UDATA findClassFlags() const {return _findClassFlags; }
	U_8* className() const {return _className; }
	UDATA classNameLength() const {return _classNameLength; }
	U_32 bctFlags() const { return (U_32) _bctFlags; } /* Only use this for j9bcutil_readClassFileBytes */
	AllocationStrategy *allocationStrategy() const { return _allocationStrategy; }
	bool classFileBytesReplaced() const { return _classFileBytesReplaced; }
#if defined(J9VM_OPT_JVMTI)
	bool isRetransformAllowed() const { return _retransformAllowed; }
#else
	bool isRetransformAllowed() const { return false; }
#endif
	bool isVerbose() const { return _verboseROMClass; }
	J9ClassLoader *classLoader() const { return _classLoader; }
	J9JavaVM *javaVM() const { return _javaVM; }
	J9VMThread *currentVMThread() const { return _javaVM->internalVMFunctions->currentVMThread(_javaVM); }
	J9PortLibrary *portLibrary() const { return _portLibrary; }
	UDATA cpIndex() const { return _cpIndex; }
	UDATA loadLocation() const {return _loadLocation; }
	J9SharedInvariantInternTable *sharedStringInternTable() const { return _sharedStringInternTable; }

	U_8 *intermediateClassData() const { return _intermediateClassData; }
	U_32 intermediateClassDataLength() const { return _intermediateClassDataLength; }

	U_8 *getIntermediateClassDataFromPreviousROMClass() const { return (NULL != _clazz) ? WSRP_GET(_clazz->romClass->intermediateClassData, U_8*) : NULL; }
	U_32 getIntermediateClassDataLengthFromPreviousROMClass() const { return (NULL != _clazz) ? _clazz->romClass->intermediateClassDataLength : 0; }

	void setReusingIntermediateClassData() { _reusingIntermediateClassData = true; }
	bool isReusingIntermediateClassData() const { return _reusingIntermediateClassData; }
	bool isInterningEnabled() const { return _interningEnabled; }
	bool isRedefining() const { return J9_FINDCLASS_FLAG_REDEFINING == (_findClassFlags & J9_FINDCLASS_FLAG_REDEFINING); }
	bool isRetransforming() const { return J9_FINDCLASS_FLAG_RETRANSFORMING == (_findClassFlags & J9_FINDCLASS_FLAG_RETRANSFORMING); }
	bool isClassLoaderSharedClassesEnabled() const { return NULL != _classLoader && (_classLoader->flags & J9CLASSLOADER_SHARED_CLASSES_ENABLED); }
	bool isSharedClassesEnabled() const { return (NULL != _javaVM) && (NULL != _javaVM->sharedClassConfig); }
	bool isSharedClassesCacheFull() const { return (NULL != _javaVM) && j9shr_Query_IsCacheFull(_javaVM); }

	/* Note: Always call isSharedClassesEnabled() before using isSharedClassesBCIEnabled() */
	bool isSharedClassesBCIEnabled() const { return (0 != _javaVM->sharedClassConfig->isBCIEnabled(_javaVM)); }

	/* Determine if this is the bootclassloader.  Allow null JavaVM to get bootstrap behaviour for cfdump */
	bool isBootstrapLoader() { return (NULL == _javaVM) || (_classLoader == _javaVM->systemClassLoader); }

	/* Only the bootstrap classloader can load classes in the "java" package. */
	bool shouldCheckPackageName() { return (NULL != _javaVM) && (_classLoader != _javaVM->systemClassLoader); }
	bool shouldCompareROMClassForEquality() const { return NULL != _romClass; }
	bool shouldPreserveLineNumbers() const { return 0 == (_bctFlags & (BCT_StripDebugLines|BCT_StripDebugAttributes)); }
	bool shouldPreserveLocalVariablesInfo() const { return 0 == (_bctFlags & (BCT_StripDebugVars|BCT_StripDebugAttributes)); }
	bool shouldPreserveSourceFileName() const { return 0 == (_bctFlags & (BCT_StripDebugSource|BCT_StripDebugAttributes)); }
	bool shouldPreserveSourceDebugExtension() const { return 0 == (_bctFlags & (BCT_StripSourceDebugExtension|BCT_StripDebugAttributes)); }
	bool isClassUnsafe() const { return J9_FINDCLASS_FLAG_UNSAFE == (_findClassFlags & J9_FINDCLASS_FLAG_UNSAFE); }
	bool isClassAnon() const { return J9_FINDCLASS_FLAG_ANON == (_findClassFlags & J9_FINDCLASS_FLAG_ANON); }
	bool alwaysSplitBytecodes() const { return J9_ARE_ANY_BITS_SET(_bctFlags, BCT_AlwaysSplitBytecodes); }

	bool isClassUnmodifiable() const {
		bool unmodifiable = false;
		if (NULL != _javaVM) {
			if ((J2SE_VERSION(_javaVM) >= J2SE_V11) && isClassAnon()) {
				unmodifiable = true;
			} else if (NULL == J9VMJAVALANGOBJECT_OR_NULL(_javaVM)) {
				/* Object is currently only allowed to be redefined in fast HCR */
				if (areExtensionsEnabled(_javaVM)) {
					unmodifiable = true;
				}
			}
		}
		return unmodifiable;
	}

	bool isIntermediateClassDataShareable() const {
		bool shareable = false;
		U_8 *intermediateData = getIntermediateClassDataFromPreviousROMClass();
		U_32 intermediateDataLength = getIntermediateClassDataLengthFromPreviousROMClass();
		if ((NULL != intermediateData)
			&& (TRUE == j9shr_Query_IsAddressInCache(_javaVM, intermediateData, intermediateDataLength))
		) {
			shareable = true;
		}
		return shareable;
	}

	bool shouldStripIntermediateClassData() const {
		return (isSharedClassesEnabled()
				&& isSharedClassesBCIEnabled()
				&& !classFileBytesReplaced()
				&& !isRetransformAllowed());
	}

	bool canRomMethodsBeSorted(UDATA threshold) const {
		if (!isRedefining() && !isRetransforming()) {
			if ((NULL != _javaVM) && (_javaVM->romMethodSortThreshold <= threshold)) {
				return true;
			}
		}
		return false;
	}

	bool isROMClassShareable() const {
		/*
		 * Any of the following conditions prevent the sharing of a ROMClass:
		 *  - classloader is not shared classes enabled
		 *  - cache is full
		 *  - Unsafe classes are not shared
		 *  - shared cache is BCI enabled and class is modified by BCI agent
		 *  - shared cache is BCI enabled and ROMClass being store is intermediate ROMClass
		 *  - the class is loaded from a patch path
		 */
		if (isSharedClassesEnabled()
			&& isClassLoaderSharedClassesEnabled()
			&& !isClassUnsafe()
			&& !(isSharedClassesBCIEnabled()
			&& (classFileBytesReplaced() || isCreatingIntermediateROMClass()))
			&& (LOAD_LOCATION_PATCH_PATH != loadLocation())
		) {
			return true;
		} else {
			return false;
		}
	}

	/*
	 * Returns true if any of the following conditions is true
	 *  - BCT_IntermediateDataIsClassfile is set
	 *  - class is being redefined and the previous class as J9AccClassIntermediateDataIsClassfile set.
	 */
	bool isIntermediateDataAClassfile() const {
		if (J9_ARE_ALL_BITS_SET(_bctFlags, BCT_IntermediateDataIsClassfile)) {
			return true;
		} else if ((NULL != _clazz) && (J9ROMCLASS_IS_INTERMEDIATE_DATA_A_CLASSFILE(_clazz->romClass))) {
			return true;
		} else {
			return false;
		}
	}

	bool canPossiblyStoreDebugInfoOutOfLine() const {
		if (!shouldPreserveLineNumbers() && !shouldPreserveLocalVariablesInfo()) {
			/*
			 * Can't store debug information out of line, if none of it is going to be stored at all
			 */
			return false;
		}
		if (isROMClassShareable() && !isSharedClassesCacheFull() ) {
			/*
			 * shared classes is enabled, and there is the potential to store the class in the cache.
			 */
			return true;
		}
		if (_allocationStrategy && _allocationStrategy->canStoreDebugDataOutOfLine()) {
			/*
			 * the allocation strategy permits out of line debug information storage.
			 */
			return true;
		}
		/*
		 * no reason to store debug information separately
		 */
		return false;
	}


	J9ROMClass *romClass() const { return _romClass; }

	BuildResult checkClassName(U_8 *className, U_16 classNameLength)
	{
		if (!isRedefining() && !isRetransforming()) {
			if (NULL != _className) {
				if ((0 == J9UTF8_DATA_EQUALS(_className, _classNameLength, className, classNameLength))) {
#define J9WRONGNAME " (wrong name: "
					PORT_ACCESS_FROM_PORT(_portLibrary);
					UDATA errorStringSize = _classNameLength + sizeof(J9WRONGNAME) + 1 + classNameLength;
					U_8 *errorUTF = (U_8 *) j9mem_allocate_memory(errorStringSize, J9MEM_CATEGORY_CLASSES);
					if (NULL != errorUTF) {
						U_8 *current = errorUTF;

						memcpy(current, _className, _classNameLength);
						current += _classNameLength;
						memcpy(current, J9WRONGNAME, sizeof(J9WRONGNAME) - 1);
						current += sizeof(J9WRONGNAME) - 1;
						memcpy(current, className, classNameLength);
						current += classNameLength;
						*current++ = (U_8) ')';
						*current = (U_8) '\0';

					}
					recordCFRError(errorUTF);
					return ClassNameMismatch;
#undef J9WRONGNAME
				}
			} else if (shouldCheckPackageName()  /* classname is null */
					&& (0 == strncmp((char *) className, "java/", 5))
					&& (!isClassAnon())) {
				/*
				 * Non-bootstrap classloaders may not load nto the "java" package.
				 * if classname is not null, the JCL or JNI has already checked it
				 */
				return IllegalPackageName;
			}

		}
		return OK;
	}

	/**
	 * Report an invalid annotation error against a particular member (field or method).
	 * This will attempt to construct and set an error message based on the supplied
	 * NLS key. Regardless, an appropriate BuildResult value will be returned.
	 * 
	 * @param className	UTF8 data representing the class containing the member
	 * @param classNameLength	length of UTF8 data representing the class containing the member
	 * @param memberName	UTF8 data representing the member with the annotation
	 * @param memberNameLength	length of UTF8 data representing the member with the annotation
	 * @param module_name	the module portion of the NLS key
	 * @param message_num	the message portion of the NLS key
	 * @return the BuildResult value 
	 */
	BuildResult 
	reportInvalidAnnotation(U_8 *className, U_16 classNameLength, U_8 *memberName, U_16 memberNameLength, U_32 module_name, U_32 message_num)
	{
		const char* nlsMessage = NULL;
		PORT_ACCESS_FROM_PORT(_portLibrary);
		
		/* Call direct through the port library to circumvent the macro, 
		 * which assumes that the key is a single parameter.
		 */
		nlsMessage = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(OMRPORT_FROM_J9PORT(PORTLIB), J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, module_name, message_num, NULL);
		if(NULL != nlsMessage) {
			UDATA messageLength = j9str_printf(PORTLIB, NULL, 0, nlsMessage, memberNameLength, memberName, classNameLength, className);
			U_8* message = (U_8*)j9mem_allocate_memory(messageLength, OMRMEM_CATEGORY_VM);
			if(NULL != message) {
				j9str_printf(PORTLIB, (char*)message, messageLength, nlsMessage, memberNameLength, memberName, classNameLength, className);
				recordCFRError(message);
			}
		}
		return InvalidAnnotation;
	}


	void recordLoadStart()
	{
		Trc_BCU_buildRomClass_Entry(_classNameLength, _className);
		if (NULL != _dynamicLoadStats) {
			PORT_ACCESS_FROM_PORT(_portLibrary);
			_dynamicLoadStats->sunSize = _classFileSize;
			_dynamicLoadStats->loadStartTime = j9time_usec_clock();
		}
	}

	void recordParseClassFileStart()
	{
		Trc_BCU_buildRomClass_ParseClassFileStart(_classNameLength, _className, _classLoader);
	}

	void recordParseClassFileEnd()
	{
		Trc_BCU_buildRomClass_ParseClassFileEnd(_classNameLength, _className, _classFileSize, _classLoader);

	}
	void recordTranslationStart()
	{
		Trc_BCU_buildRomClass_TranslateClassFileStart(_classNameLength, _className, _classLoader);
		if (NULL != _dynamicLoadStats) {
			PORT_ACCESS_FROM_PORT(_portLibrary);
			_dynamicLoadStats->loadEndTime = _dynamicLoadStats->translateStartTime = j9time_usec_clock();
		}
	}

	void recordTranslationEnd()
	{
		J9ROMClass *romClass = this->romClass();
		Trc_BCU_buildRomClass_TranslateClassFileEnd(J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)), romClass->romSize, _classLoader);
		if (NULL != _dynamicLoadStats) {
			PORT_ACCESS_FROM_PORT(_portLibrary);
			_dynamicLoadStats->romSize = romClass->romSize;
			_dynamicLoadStats->translateEndTime = j9time_usec_clock();
		}
	}

	void recordROMClass(J9ROMClass *romClass)
	{
		_romClass = romClass;
	}

	void recordCFRError(U_8 *cfrError)
	{
		if ((NULL != _javaVM) && (NULL != _javaVM->dynamicLoadBuffers)) {
			_javaVM->dynamicLoadBuffers->classFileError = cfrError;
		}
	}

	void freeClassFileBuffer(U_8 *buffer)
	{
		PORT_ACCESS_FROM_PORT(_portLibrary);
		/* It is possible that through the use of recordCFRError that an internally allocated buffer has been let loose
		 * into _javaVM->dynamicLoadBuffers->classFileError, if the internal buffer that is free'd matches the one in
		 * _javaVM->dynamicLoadBuffers->classFileError, then it must be set to NULL to avoid a double free in
		 * j9bcutil_freeTranslationBuffers()*/
		if ((NULL != _javaVM) && (NULL != _javaVM->dynamicLoadBuffers) && (buffer == _javaVM->dynamicLoadBuffers->classFileError)) {
			_javaVM->dynamicLoadBuffers->classFileError = NULL;
		}
		j9mem_free_memory(buffer);
	}

	void recordLoadEnd(BuildResult result)
	{
		Trc_BCU_buildRomClass_Exit(result);
		_buildResult = result;
	}

	void recordPhaseStart(ROMClassCreationPhase phase)
	{
		if (_verboseROMClass) {
			PORT_ACCESS_FROM_PORT(_portLibrary);
			_verboseRecords[phase].lastStartTime = j9time_usec_clock();
			_verboseRecords[phase].parentPhase = _verboseCurrentPhase;
			_verboseCurrentPhase = phase;
		}
	}

	void recordPhaseEnd(ROMClassCreationPhase phase, BuildResult buildResult)
	{
		if (_verboseROMClass) {
			PORT_ACCESS_FROM_PORT(_portLibrary);
			_verboseRecords[phase].accumulatedTime += j9time_usec_clock() - _verboseRecords[phase].lastStartTime;
			_verboseRecords[phase].buildResult = buildResult;
			_verboseCurrentPhase = _verboseRecords[phase].parentPhase;
		}
	}

	void recordOutOfMemory(UDATA bufferSize)
	{
		if (_verboseROMClass) {
			_verboseLastBufferSizeExceeded = bufferSize;
			_verboseOutOfMemoryCount++;
			for (ROMClassCreationPhase phase = ROMClassCreation; phase != ROMClassCreationPhaseCount; ++phase) {
				_verboseRecords[phase].failureTime = _verboseRecords[phase].accumulatedTime;
			}
		}
	}

	void reportStatistics(J9TranslationLocalBuffer *localBuffer)
	{
		if (NULL != _dynamicLoadStats) {
			if ((NULL != _javaVM) && (NULL != _javaVM->dynamicLoadBuffers) && (NULL != _javaVM->dynamicLoadBuffers->reportStatisticsFunction)) {
				_javaVM->dynamicLoadBuffers->reportStatisticsFunction(_javaVM, classLoader(), romClass(), localBuffer);
			}
			memset(&_dynamicLoadStats->nameLength, 0, sizeof(J9DynamicLoadStats) - sizeof(UDATA) - sizeof(U_8 *));
		}
		if (_verboseROMClass) {
			reportVerboseStatistics();
		}
	}
	
	void forceDebugDataInLine()
	{
		_forceDebugDataInLine = true;
	}

	bool shouldWriteDebugDataInline()
	{
		if (_doDebugCompare) {
			return romMethodDebugDataIsInline();
		} else {
			return _forceDebugDataInLine;
		}
	}

	void startDebugCompare() 
	{
		_doDebugCompare = true;
	}

	void endDebugCompare() 
	{
		_doDebugCompare = false;
	}

	/*Methods that find an existing romMethod at offset in _romClass, and then query the method*/
	U_32 romMethodModifiers()
	{
		J9ROMMethod * romMethod = romMethodGetCachedMethod();
		if (NULL != romMethod) {
			return romMethod->modifiers;
		} else {
			return 0;
		}
	}

	bool romMethodHasDebugData()
	{
		return (0 != (romMethodModifiers() & J9AccMethodHasDebugInfo));
	}
	
	bool romMethodHasLineNumberCountCompressed()
	{
		bool retval = false;
		J9ROMMethod * romMethod = romMethodGetCachedMethod();
		if (NULL != romMethod) {
			J9MethodDebugInfo * debuginfo = getMethodDebugInfoFromROMMethod(romMethod);
			if (NULL != debuginfo) {
				retval = (0x0 == (debuginfo->lineNumberCount & 0x1));
			}
		}
		return retval;
	}
	
	U_32 romMethodCompressedLineNumbersLength()
	{
		U_32 retval = 0;
		J9ROMMethod * romMethod = romMethodGetCachedMethod();
		if (NULL != romMethod) {
			J9MethodDebugInfo * debuginfo = getMethodDebugInfoFromROMMethod(romMethod);
			if (NULL != debuginfo) {
				if (0x1 == (debuginfo->lineNumberCount & 0x1)) {
					/*lineNumbersInfoCompressedSize is the U_32 following debuginfo in memory*/
					retval = *(U_32 *)(debuginfo+1);
				} else {
					/*lineNumbersInfoCompressedSize is stored encoded in the lineNumberCount value*/
					retval = ((debuginfo->lineNumberCount & 0xffff0000) >> 16);
				}
			}/*else there is no debug data*/
		}
		return retval;
	}

	void * romMethodLineNumbersDataStart()
	{
		J9ROMMethod * romMethod = romMethodGetCachedMethod();
		if (NULL != romMethod) {
			return (void *)getMethodDebugInfoFromROMMethod(romMethod);
		} else {
			return NULL;
		}
	}

	bool romMethodDebugDataIsInline() 
	{
		bool retval = true;
		J9ROMMethod * romMethod = romMethodGetCachedMethod();	
		/* romMethodHasDebugData() is called to ensure methodDebugInfoFromROMMethod() 
		 * will return a valid address to inspect.
		 */
		if ((NULL != romMethod) && (true == romMethodHasDebugData())) {
			/* methodDebugInfoFromROMMethod() is called instead of getMethodDebugInfoFromROMMethod()
			 * because getMethodDebugInfoFromROMMethod() will follow the srp to 'out of line' debug 
			 * data (and cause problems checking "1 ==(debugInfo->srpToVarInfo & 1)")
			 */
			J9MethodDebugInfo* debugInfo = methodDebugInfoFromROMMethod(romMethod);
			retval = (1 ==(debugInfo->srpToVarInfo & 1));
		}
		return retval;
	}

	void romMethodCacheCurrentRomMethod(IDATA offset) 
	{
		_existingRomMethod = romMethodFromOffset(offset);
	}

	J9ROMMethod * romMethodGetCachedMethod() 
	{
		return _existingRomMethod;
	}

	void * romMethodVariableDataStart()
	{
		void * retval = NULL;
		J9ROMMethod * romMethod = romMethodGetCachedMethod();
		if (NULL != romMethod) {
			J9MethodDebugInfo* debugInfo = getMethodDebugInfoFromROMMethod(romMethod);
			if (NULL != debugInfo) {
				retval = (void *)getVariableTableForMethodDebugInfo(debugInfo);
			}
		}
		return retval;
	}

	/*Methods that query _romClass*/
	U_32 romClassOptionalFlags()
	{
		if (NULL != _romClass) {
			return _romClass->optionalFlags;
		} else {
			return 0;
		}
	}
	
	bool romClassHasSourceDebugExtension()
	{
		return (0 != (romClassOptionalFlags() & J9_ROMCLASS_OPTINFO_SOURCE_DEBUG_EXTENSION));
	}

	bool romClassHasSourceFileName()
	{
		return (0 != (romClassOptionalFlags() & J9_ROMCLASS_OPTINFO_SOURCE_FILE_NAME));
	}
	
private:
	void reportVerboseStatistics();
	void verbosePrintPhase(ROMClassCreationPhase phase, bool *printedPhases, UDATA indent);
	const char *buildResultString(BuildResult result);

	struct VerboseRecord
	{
		UDATA lastStartTime; /* last time this phase started - temporary */
		UDATA accumulatedTime; /* total time spent in this phase */
		UDATA failureTime; /* total time wasted in this phase due to later failure */
		BuildResult buildResult; /* last build result from this phase */
		ROMClassCreationPhase parentPhase; /* phase in which this phase was last nested */
	};

	J9PortLibrary *_portLibrary;
	J9JavaVM * _javaVM;
	U_8 * _classFileBytes;
	UDATA _classFileSize;
	UDATA _bctFlags;
	UDATA _bcuFlags;
	UDATA _findClassFlags;
	AllocationStrategy *_allocationStrategy;
	J9ROMClass *_romClass;
	J9Class *_clazz;
	U_8 *_className;
	UDATA _classNameLength;
	U_8 *_intermediateClassData;
	U_32 _intermediateClassDataLength;
	J9ClassLoader *_classLoader;
	UDATA _cpIndex;
	UDATA _loadLocation;
	J9DynamicLoadStats *_dynamicLoadStats;
	J9SharedInvariantInternTable *_sharedStringInternTable;
	bool _classFileBytesReplaced;
	bool _retransformAllowed;
	bool _interningEnabled;
	bool _verboseROMClass;
	UDATA _verboseLastBufferSizeExceeded;
	UDATA _verboseOutOfMemoryCount;
	ROMClassCreationPhase _verboseCurrentPhase;
	BuildResult _buildResult;
	VerboseRecord _verboseRecords[ROMClassCreationPhaseCount];
	bool _forceDebugDataInLine;
	bool _doDebugCompare;
	J9ROMMethod * _existingRomMethod;
	bool _reusingIntermediateClassData;
	bool _creatingIntermediateROMClass;
	
	J9ROMMethod * romMethodFromOffset(IDATA offset);
};

#endif /* ROMCLASSCREATIONCONTEXT_HPP_ */
