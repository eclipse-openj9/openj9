/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include <string.h>

#include "j9.h"
#include "verbose_api.h"
#include "omrlinkedlist.h"
#include "VerboseGCInterface.h"
#ifdef J9VM_INTERP_NATIVE_SUPPORT
#include "MethodMetaData.h"
#endif
#define walkFrame walkFrameVerbose
#define walkStackFrames walkStackFramesVerbose
#include "j9protos.h"
#undef walkFrame
#undef walkStackFrames
#ifdef J9VM_INTERP_NATIVE_SUPPORT
#define jitExceptionHandlerSearch jitExceptionHandlerSearchVerbose
#define jitWalkStackFrames jitWalkStackFramesVerbose
#define jitGetStackMapFromPC jitGetStackMapFromPCVerbose
#define jitGetOwnedObjectMonitors jitGetOwnedObjectMonitorsVerbose
#include "jitprotos.h"
#undef jitExceptionHandlerSearch
#undef jitWalkStackFrames
#undef jitGetStackMapFromPC
#undef jitGetOwnedObjectMonitors
#endif

#include "bytecodewalk.h"
#include "bcverify.h"
#include "cfreader.h"
#include "jni.h"
#include "j9port.h"
#include "j9consts.h"
#include "jvminit.h"
#include "rommeth.h"
#include "verbosenls.h"
#include "mmhook.h"
#include "mmomrhook.h"
#include "j2sever.h"
#include "util_api.h"
#include "verbose_internal.h"
#include "zip_api.h"
#include "vmzipcachehook.h"
#if defined(J9VM_OPT_SHARED_CLASSES)
#include "SCQueryFunctions.h"
#endif /*J9VM_OPT_SHARED_CLASSES*/

#define _UTE_STATIC_
#include "ut_j9vrb.h"

#define THIS_DLL_NAME J9_VERBOSE_DLL_NAME
#define OPT_VERBOSE "-verbose"
#define OPT_JNI "jni"
#define OPT_CLASS "class"
#define OPT_NOCLASS "noclass"
#define OPT_GCTERSE "gcterse"
#define OPT_GC "gc"
#define OPT_NOGC "nogc"
#define OPT_SIZES "sizes"
#define OPT_XSNW "-Xsnw"
#define OPT_DYNLOAD "dynload"
#define OPT_STACK "stack"
#define OPT_DEBUG "debug"
#define OPT_STACKWALK_EQUALS "stackwalk="
#define OPT_STACKWALK "stackwalk"
#define OPT_STACKTRACE "stacktrace"
#define OPT_SHUTDOWN "shutdown"
#define OPT_INIT "init"
#define OPT_RELOCATIONS "relocations"
#define OPT_ROMCLASS "romclass"
#define OPT_NONE "none"
#define OPT_VERBOSE_JNI "-verbose:jni"
#define OPT_VERBOSE_NONE "-verbose:none"
#define DEFAULT_STACKWALK_VERBOSE_LEVEL 100

#define GET_SUPERCLASS(clazz) \
	((J9CLASS_DEPTH(classDepthAndFlags) == 0) ? NULL : \
		(clazz)->superclasses[J9CLASS_DEPTH(clazz) - 1])

static void verboseStackDump (J9VMThread * vmThread, const char * msg);
#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)) 
void hookDynamicLoadReporting (J9TranslationBufferSet *dynamicLoadBuffers);
#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */
static void printClass (J9VMThread* vmThread, J9Class* clazz, char* message, UDATA bootLoaderOnly);
static void installVerboseStackWalker (J9JavaVM * vm);
static void verboseHookGC (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void sniffAndWhackHookGC (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void dumpMemorySizes (J9JavaVM *jvm);
static void dumpXlpCodeCache(J9JavaVM *jvm);
static UDATA getQualifiedSize (UDATA byteSize, const char **qualifierString);
static void verboseHookClassLoad (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#if (defined(J9VM_OPT_ZIP_SUPPORT))
static void zipCachePoolHookCallback(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif
#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) 
static void verboseHookClassUnload (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)) 
void reportDynloadStatistics (struct J9JavaVM *javaVM, struct J9ClassLoader *loader, struct J9ROMClass *romClass, struct J9TranslationLocalBuffer *localBuffer);
#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */
static void dumpQualifiedSize (J9PortLibrary* portLib, UDATA byteSize, const char* optionName, U_32 module_name, U_32 message_num);
static void verboseEmptyOSlotIterator (J9VMThread * currentThread, J9StackWalkState * walkState, j9object_t * objectSlot, const void * stackLocation);
static IDATA initializeVerbosegclog (J9JavaVM* vm, IDATA vbgclogIndex);

static void verboseClassVerificationStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void verboseClassVerificationFallback(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void verboseClassVerificationEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void verboseMethodVerificationStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void verboseStackMapFrameVerification(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

typedef struct VerboseVerificationBuffer {
	UDATA size;
	UDATA cursor;
	U_8* buffer;
} VerboseVerificationBuffer;

static void initVerboseVerificationBuffer(VerboseVerificationBuffer* buf, UDATA size, char* byteArray);
static void releaseVerboseVerificationBuffer(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, char byteArray[]);
static J9UTF8* toExternalQualifiedName(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, J9UTF8* utf);
static void printStackMapFrame(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, J9BytecodeVerificationData* verifyData, J9BranchTargetStack* stackMapFrame);
static void printVerificationInfo(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, const char* msgFormat, ...);
static void flushVerificationBuffer(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf);
static UDATA printDataType(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, J9BytecodeVerificationData* verifyData, UDATA encodedType, const char* fmt);
static UDATA constructPrintFormat(UDATA encodedType, char* format, UDATA fmtSize);

jint JNICALL JVM_OnLoad(JavaVM * jvm, char *commandLineOptions, void *reserved0)
{
/*	printf("JVM_OnLoad called\n"); */
	return JNI_OK;
}



jint JNICALL JVM_OnUnload(JavaVM * jvm, void *reserved0)
{
/*	printf("JVM_OnUnload called\n");	*/
	return JNI_OK;
}



/* Output is intentionally ordered the same as "j9 -X" */
static void 
dumpMemorySizes(J9JavaVM *jvm)
{
	PORT_ACCESS_FROM_JAVAVM(jvm);

	J9MemoryManagerVerboseInterface *mmFuncTable = (J9MemoryManagerVerboseInterface *)jvm->memoryManagerFunctions->getVerboseGCFunctionTable(jvm);
	mmFuncTable->gcDumpMemorySizes(jvm);

	dumpXlpCodeCache(jvm);

	dumpQualifiedSize(PORTLIB, jvm->defaultOSStackSize, "-Xmso", J9NLS_VERB_SIZES_XMSO);
#if defined(J9VM_INTERP_GROWABLE_STACKS)
	dumpQualifiedSize(PORTLIB, jvm->initialStackSize, "-Xiss", J9NLS_VERB_SIZES_XISS);
	dumpQualifiedSize(PORTLIB, jvm->stackSizeIncrement, "-Xssi", J9NLS_VERB_SIZES_XSSI);
	dumpQualifiedSize(PORTLIB, jvm->stackSize, "-Xss", J9NLS_VERB_SIZES_XSS_GROWABLE);
#else
	dumpQualifiedSize(PORTLIB, jvm->stackSize, "-Xss", J9NLS_VERB_SIZES_XSS_NONGROWABLE);
#endif /* J9VM_INTERP_GROWABLE_STACKS */

#if defined(J9VM_OPT_SHARED_CLASSES)
	if (J2SE_VERSION(jvm) && jvm->sharedClassPreinitConfig) {				/* Only show for J2SE */
		/* Init updatedWithDefaults to the current values in jvm->sharedClassPreinitConfig, 
		 * b/c if new members are added to J9SharedClassPreinitConfig this will cause the 
		 * default value (set by VMInitStages() in jvminit.c) to be used.
		 */
		J9JavaVM *vm = jvm;

		J9SharedClassPreinitConfig updatedWithDefaults = *(jvm->sharedClassPreinitConfig);
		j9shr_Query_PopulatePreinitConfigDefaults(jvm, &updatedWithDefaults);
		dumpQualifiedSize(PORTLIB, updatedWithDefaults.sharedClassCacheSize, "-XX:SharedCacheHardLimit=", J9NLS_VERB_SIZES_XXSHARED_CACHE_HARD_LIMIT_EQUALS);
		dumpQualifiedSize(PORTLIB, updatedWithDefaults.sharedClassSoftMaxBytes, "-Xscmx", J9NLS_VERB_SIZES_XSCMX_V1);
		dumpQualifiedSize(PORTLIB, updatedWithDefaults.sharedClassDebugAreaBytes, "-Xscdmx", J9NLS_VERB_SIZES_XSCDMX);
		dumpQualifiedSize(PORTLIB, updatedWithDefaults.sharedClassMinAOTSize, "-Xscminaot", J9NLS_VERB_SIZES_XSCMINAOT);
		dumpQualifiedSize(PORTLIB, updatedWithDefaults.sharedClassMaxAOTSize, "-Xscmaxaot", J9NLS_VERB_SIZES_XSCMAXAOT);
		dumpQualifiedSize(PORTLIB, updatedWithDefaults.sharedClassMinJITSize, "-Xscminjitdata", J9NLS_VERB_SIZES_XSCMINJITDATA);
		dumpQualifiedSize(PORTLIB, updatedWithDefaults.sharedClassMaxJITSize, "-Xscmaxjitdata", J9NLS_VERB_SIZES_XSCMAXJITDATA);
	}
#endif

	return;
}

/**
 * Dump the value of large page size (and page type, if applicable) being used by JIT for code cache allocation.
 * If JIT is not using large pages then default page size will be displayed.
 * Also dump available large page size(s) for JIT code cache.
 *
 * Note that above information is dumped only if system supports large page size for executable pages.
 *
 * @param [in] jvm the J9JavaVM structure
 *
 * @return void
 */
static void
dumpXlpCodeCache(J9JavaVM *jvm)
{
	UDATA *pageSizes = NULL;
	UDATA *pageFlags = NULL;
	UDATA pageSizeEntry = 0;
	UDATA pageFlagsEntry = J9PORT_VMEM_PAGE_FLAG_NOT_USED;
	BOOLEAN isSupported = FALSE;
	UDATA pageIndex = 0;
	BOOLEAN executableLargePageSupported = FALSE;
	J9JITConfig *jitConfig = jvm->jitConfig;
	PORT_ACCESS_FROM_JAVAVM(jvm);

	/* If jitConfig is NULL, we are running without JIT. Nothing to do then */
	if (NULL != jitConfig) {
		pageSizes = j9vmem_supported_page_sizes();
		pageFlags = j9vmem_supported_page_flags();

		/* Loop to determine if the system supports executable large pages */
		for(pageIndex = 0; 0 != pageSizes[pageIndex]; pageIndex++) {
			pageSizeEntry = pageSizes[pageIndex];
			pageFlagsEntry = pageFlags[pageIndex];
			j9vmem_find_valid_page_size(J9PORT_VMEM_MEMORY_MODE_EXECUTE, &pageSizeEntry, &pageFlagsEntry, &isSupported);
			if (TRUE == isSupported) {
				executableLargePageSupported = TRUE;
				break;
			}
		}

		if (TRUE == executableLargePageSupported) {
			UDATA codePageSize = jitConfig->largeCodePageSize;
			UDATA codePageFlags = jitConfig->largeCodePageFlags;
			const char* optionDescription = NULL;
			const char *optionName = "-Xlp:codecache:pagesize=";
			const char *qualifier = NULL;
			const U_32 spaceCount = 15;

			if (0 == codePageSize) {
				codePageSize = pageSizes[0];
				codePageFlags = pageFlags[0];
			}

			/* Obtain the qualified size (e.g. 4K) */
			codePageSize = getQualifiedSize(codePageSize, &qualifier);

			/* look up the appropriate translation */
			optionDescription = j9nls_lookup_message(J9NLS_DO_NOT_APPEND_NEWLINE | J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
													J9NLS_VERB_SIZES_XLP_CODECACHE,
													NULL);

			j9tty_printf(PORTLIB, "  %s%zu%s", optionName, codePageSize, qualifier);
			if (J9PORT_VMEM_PAGE_FLAG_NOT_USED != (J9PORT_VMEM_PAGE_FLAG_NOT_USED & codePageFlags)) {
				j9tty_printf(PORTLIB, ",%s", getPageTypeString(codePageFlags));
			}
			j9tty_printf(PORTLIB, "\t %s\n", optionDescription);

			/* Now list all available page sizes that support executable pages. */
			optionDescription = j9nls_lookup_message(J9NLS_DO_NOT_APPEND_NEWLINE | J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
													J9NLS_VERB_SIZES_AVAILABLE_XLP_CODECACHE,
													NULL);

			j9tty_printf(PORTLIB, "  %*s %s", spaceCount, " ", optionDescription);

			for(pageIndex = 0; 0 != pageSizes[pageIndex]; pageIndex++) {
				pageSizeEntry = pageSizes[pageIndex];
				pageFlagsEntry = pageFlags[pageIndex];
				isSupported = FALSE;

				j9vmem_find_valid_page_size(J9PORT_VMEM_MEMORY_MODE_EXECUTE, &pageSizeEntry, &pageFlagsEntry, &isSupported);
				if (TRUE == isSupported) {
					pageSizeEntry = getQualifiedSize(pageSizeEntry, &qualifier);
					j9tty_printf(PORTLIB, "\n  %*s %zu%s", spaceCount, " ", pageSizeEntry, qualifier);
					if (J9PORT_VMEM_PAGE_FLAG_NOT_USED != (J9PORT_VMEM_PAGE_FLAG_NOT_USED & pageFlagsEntry)) {
						j9tty_printf(PORTLIB, " %s", getPageTypeString(pageFlagsEntry));
					}
				} else {
					/* This page size does not support executable pages, skip it. */
				}
			}
			j9tty_printf(PORTLIB, "\n");
		}
	}
}

static void 
dumpQualifiedSize(J9PortLibrary* portLib, UDATA byteSize, const char* optionName, U_32 module_name, U_32 message_num)
{
	char buffer[16];
	UDATA size, paramSize;
	const char* qualifier;
	const char* optionDescription;
	PORT_ACCESS_FROM_PORT(portLib);

	size = getQualifiedSize(byteSize, &qualifier);

	optionDescription = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(
		OMRPORT_FROM_J9PORT(PORTLIB),
		J9NLS_DO_NOT_APPEND_NEWLINE | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, 
		module_name, 
		message_num, 
		NULL);

	paramSize = j9str_printf(PORTLIB, buffer, 16, "%zu%s", size, qualifier);
	paramSize = 15 - paramSize;
	paramSize += strlen(optionDescription);
	paramSize -= strlen(optionName);
	j9tty_printf(PORTLIB, "  %s%s %*s\n", optionName, buffer, paramSize, optionDescription);

	return;
}

#if 0
static void 
printClassShape(J9VMThread* vmThread, J9Class* clazz)
{
	J9ROMClass* romClass;
	J9JavaVM* vm = vmThread->javaVM;
	PORT_ACCESS_FROM_VMC(vmThread);

	romClass = clazz->romClass;
	if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass) == 0) {
		UDATA len = clazz->totalInstanceSize / sizeof(U_32);
		U_8* shape = j9mem_allocate_memory(len + 1, OMRMEM_CATEGORY_VM);

		if (shape != NULL) {
			memset(shape, '?', len);
			shape[len] = '\0';
			
			while (clazz != NULL) {
				J9ROMFieldWalkState walkState;
				J9ROMFieldShape* romField = romFieldsStartDo(clazz->romClass, &walkState);
				
				while (romField != NULL) {
					if (0 == (romField->modifiers & J9AccStatic)) {
						J9UTF8* name = J9ROMFIELDSHAPE_NAME(romField);
						J9UTF8* signature = J9ROMFIELDSHAPE_SIGNATURE(romField);
						UDATA offset = vm->internalVMFunctions->instanceFieldOffset(
							vmThread,
							clazz,
							J9UTF8_DATA(name),
							J9UTF8_LENGTH(name),
							J9UTF8_DATA(signature),
							J9UTF8_LENGTH(signature),
							NULL, /* defining class return value */
							NULL, /* romFieldShape return value */
							J9_LOOK_NO_JAVA);
							
						offset /= sizeof(U_32);
							
						switch (J9UTF8_DATA(signature)[0]) {
						case 'B':
						case 'Z':
						case 'C':
						case 'S':
						case 'F':
						case 'I':
							shape[offset] = J9UTF8_DATA(signature)[0];
							break;
						case 'D':
						case 'J':
							shape[offset+0] = J9UTF8_DATA(signature)[0];
							shape[offset+1] = J9UTF8_DATA(signature)[0];
							break;
						default:
							memset(shape + offset, J9UTF8_DATA(signature)[0], sizeof(fj9object_t) / sizeof(U_32));
							break;
						}
					}
				
					romField = romFieldsNextDo(&walkState);	
				}
			
				clazz = GET_SUPERCLASS(clazz);	
			}	
		}
		
		j9tty_printf(PORTLIB, "\tSHAPE: %s\n", shape);
		j9mem_free_memory(shape);
	}
}
#endif

static void
verboseHookClassLoad(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMClassLoadEvent* event = eventData;
	J9VMThread* vmThread = event->currentThread;

	printClass(event->currentThread, event->clazz, "class load", TRUE);

#if 0
	printClassShape(event->currentThread, event->clazz);
#endif
}

#ifdef J9VM_OPT_ZIP_SUPPORT
static void zipCachePoolHookCallback(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMZipLoadEvent* event = eventData;
	I_32 rc = event->returnCode;
	
	PORT_ACCESS_FROM_PORT(event->portlib);
	
	switch (rc) {
		case 0:
			return;
		case ZIP_ERR_FILE_OPEN_ERROR:
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VERB_CLASS_BAD_CPENTRY_ZIP_FILE_OPEN_ERROR, event->cpPath);
			break;
		case ZIP_ERR_FILE_READ_ERROR:
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VERB_CLASS_BAD_CPENTRY_ZIP_FILE_READ_ERROR, event->cpPath);
			break;
		case ZIP_ERR_FILE_CORRUPT:
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VERB_CLASS_BAD_CPENTRY_ZIP_FILE_CORRUPT, event->cpPath);
			break;
		case ZIP_ERR_UNSUPPORTED_FILE_TYPE:
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VERB_CLASS_BAD_CPENTRY_ZIP_UNSUPPORTED_FILE_TYPE, event->cpPath);
			break;
		case ZIP_ERR_UNKNOWN_FILE_TYPE:
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VERB_CLASS_BAD_CPENTRY_ZIP_UNKNOWN_FILE_TYPE, event->cpPath);
			break;
		default:
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VERB_CLASS_BAD_CPENTRY_UNKNOWN_ERROR, event->cpPath);
	}
}
#endif

#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) 
static void
verboseHookClassUnload(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMClassUnloadEvent* event = eventData;

	printClass(event->currentThread, event->clazz, "class unload", FALSE);
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */


#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)) 
void reportDynloadStatistics(struct J9JavaVM *javaVM, struct J9ClassLoader *loader, struct J9ROMClass *romClass, struct J9TranslationLocalBuffer *localBuffer)
{
	J9DynamicLoadStats *dynamicLoadStats = javaVM->dynamicLoadBuffers->dynamicLoadStats;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	/* localBuffer should not be NULL */
	Assert_VRB_true(NULL != localBuffer);

	if (NULL != localBuffer->cpEntryUsed) {
		j9tty_printf(PORTLIB,
			 "<Loaded %.*s from %.*s>\n<  Class size %i; ROM size %i; debug size %i>\n<  Read time %i usec; Load time %i usec; Translate time %i usec>\n",
			dynamicLoadStats->nameLength,
			dynamicLoadStats->name,
			localBuffer->cpEntryUsed->pathLength,
			localBuffer->cpEntryUsed->path,
			dynamicLoadStats->sunSize, dynamicLoadStats->romSize,
			dynamicLoadStats->debugSize,
			dynamicLoadStats->readEndTime - dynamicLoadStats->readStartTime,
			dynamicLoadStats->loadEndTime - dynamicLoadStats->loadStartTime,
			dynamicLoadStats->translateEndTime - dynamicLoadStats->translateStartTime);
	} else {
		J9UTF8 *jrtURL = NULL;

		if (LOAD_LOCATION_MODULE == localBuffer->loadLocationType) {
			J9InternalVMFunctions const * const vmFuncs = javaVM->internalVMFunctions;
			J9VMThread *currentThread = vmFuncs->currentVMThread(javaVM);
			U_8 *className = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass));
			UDATA length = packageNameLength(romClass);
			J9Module *module = NULL;

			omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);

			module = vmFuncs->findModuleForPackage(currentThread, loader, className, (U_32)length);

			if (NULL == module) {
				module = javaVM->javaBaseModule;
			}

			jrtURL = getModuleJRTURL(currentThread, loader, module);

			omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);
		}

		if (NULL != jrtURL) {
			j9tty_printf(PORTLIB,
				"<Loaded %.*s from %.*s>\n<  Class size %i; ROM size %i; debug size %i>\n<  Read time %i usec; Load time %i usec; Translate time %i usec>\n",
				J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)),
				J9UTF8_LENGTH(jrtURL),
				J9UTF8_DATA(jrtURL),
				dynamicLoadStats->sunSize,
				dynamicLoadStats->romSize, dynamicLoadStats->debugSize,
				dynamicLoadStats->readEndTime - dynamicLoadStats->readStartTime,
				dynamicLoadStats->loadEndTime - dynamicLoadStats->loadStartTime,
				dynamicLoadStats->translateEndTime - dynamicLoadStats->translateStartTime);
		} else {
			j9tty_printf(PORTLIB,
				"<Loaded %.*s>\n<  Class size %i; ROM size %i; debug size %i>\n<  Read time %i usec; Load time %i usec; Translate time %i usec>\n",
				J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)), dynamicLoadStats->sunSize,
				dynamicLoadStats->romSize, dynamicLoadStats->debugSize,
				dynamicLoadStats->readEndTime - dynamicLoadStats->readStartTime,
				dynamicLoadStats->loadEndTime - dynamicLoadStats->loadStartTime,
				dynamicLoadStats->translateEndTime - dynamicLoadStats->translateStartTime);
		}
	}

	return;
}

#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */


#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)) 
void hookDynamicLoadReporting(J9TranslationBufferSet *dynamicLoadBuffers)
{
	if(dynamicLoadBuffers) {
		dynamicLoadBuffers->flags |= BCU_VERBOSE;
		dynamicLoadBuffers->reportStatisticsFunction = reportDynloadStatistics;
	}
	return;
}

#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */


static void
printClass(J9VMThread* vmThread, J9Class* clazz, char* message, UDATA bootLoaderOnly)
{
	J9ROMClass* romClass;
	PORT_ACCESS_FROM_VMC(vmThread);

	romClass = clazz->romClass;
	if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass) == 0) {
		if (!bootLoaderOnly
				|| (clazz->classLoader == vmThread->javaVM->systemClassLoader)
				|| (clazz->classLoader == vmThread->javaVM->anonClassLoader)
		) {

			J9UTF8* utf = J9ROMCLASS_CLASSNAME(romClass);
			UDATA pathLength = 0;
			U_8* path = getClassLocation(vmThread, clazz, &pathLength);

			if (NULL == path) {
				char* template = "%s: %.*s %s\n";
				char* extra = "";
				Trc_VRB_printClass_Event1(vmThread, message, (UDATA)J9UTF8_LENGTH(utf), J9UTF8_DATA(utf), extra);
				j9tty_printf(PORTLIB, template, message, (UDATA)J9UTF8_LENGTH(utf), J9UTF8_DATA(utf), extra);
			} else {
				char* template = "%s: %.*s from: %.*s %s\n";
				char* extra = "";
				Trc_VRB_printClass_Event2(vmThread, message, (UDATA)J9UTF8_LENGTH(utf), J9UTF8_DATA(utf), path, extra);
				j9tty_printf(PORTLIB, template, message, (UDATA)J9UTF8_LENGTH(utf), J9UTF8_DATA(utf), pathLength, path, extra);
			}
		}
	}
}

/**
 * parse a set of -verbose: options and update a settings struct.
 * @param options set of null-separated verbose options (e.g. "class", "gc", etc.  Terminated by a double null
 * @param verboseOptions STructure of flags indicating which verbosity to turn on
 * @return 0 if error, 1 if success
 */

static UDATA 
parseVerboseArgument(char* options, J9VerboseSettings* verboseOptions, char** errorString) {
	UDATA result = TRUE;
	
	if (!options || !*options) {
		verboseOptions->vclass = VERBOSE_SETTINGS_SET; /* default argument is OPT_CLASS */
	} else while (*options) {
		if( strcmp(options, OPT_NOCLASS)==0 ) {
			/* clearing class verbosity is different from not enabling it. enable and disable cancel. disable alone invokes explicit action */
			verboseOptions->vclass = (VERBOSE_SETTINGS_SET == verboseOptions->vclass)? VERBOSE_SETTINGS_IGNORE: VERBOSE_SETTINGS_CLEAR;
		} else if( strcmp(options, OPT_CLASS)==0 ) {
			verboseOptions->vclass = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_GCTERSE)==0) {
			verboseOptions->gcterse = 1;
		} else if(strcmp(options, OPT_GC)==0) {
			verboseOptions->gc = VERBOSE_SETTINGS_SET;
		} else if( strcmp( options, OPT_NOGC ) == 0 ) {
			verboseOptions->gc = (VERBOSE_SETTINGS_SET == verboseOptions->gc)? VERBOSE_SETTINGS_IGNORE: VERBOSE_SETTINGS_CLEAR;
		} else
#ifdef J9VM_OPT_DYNAMIC_LOAD_SUPPORT
		if(strcmp(options, OPT_DYNLOAD)==0) {
			verboseOptions->dynload = VERBOSE_SETTINGS_SET;
		} else
#endif
		if(strncmp(options, OPT_STACKWALK_EQUALS, sizeof(OPT_STACKWALK_EQUALS) - 1)==0) {
			char * scanCursor = options + sizeof(OPT_STACKWALK_EQUALS) - 1;
			verboseOptions->stackwalk = VERBOSE_SETTINGS_SET;

			if (scan_udata(&scanCursor, &(verboseOptions->stackWalkVerboseLevel)) || *scanCursor) {
				if( errorString ) {
					*errorString = "invalid stackwalk trace level";
				}
				result = FALSE;
			}
		} else if(strcmp(options, OPT_STACKWALK)==0) {
			verboseOptions->stackwalk = VERBOSE_SETTINGS_SET;
			verboseOptions->stackWalkVerboseLevel = DEFAULT_STACKWALK_VERBOSE_LEVEL;
		} else if(strcmp(options, OPT_SIZES)==0) {	
			verboseOptions->sizes = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_JNI)==0) {
			verboseOptions->jni = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_STACK)==0) {
			verboseOptions->stack = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_STACKTRACE)==0) {
			verboseOptions->stacktrace = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_SHUTDOWN)==0) {
			verboseOptions->shutdown = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_DEBUG)==0) {
			verboseOptions->debug = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_INIT)==0) {
			verboseOptions->init = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_RELOCATIONS)==0) { 
			verboseOptions->relocations = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_ROMCLASS)==0) {
			verboseOptions->romclass = VERBOSE_SETTINGS_SET;
		} else if(strcmp(options, OPT_NONE)==0) {
			/* Do nothing. Only gets here if user specified -verbose:class,none or similar nonsense. */
			memset(verboseOptions, 0, sizeof(J9VerboseSettings));
		} else {
			if( errorString ) {
				*errorString = "unrecognised option for -verbose:<opt>";
			}
			return 0; /* option not recognized */
		}
		options += strlen(options)+1; /* advance past the null */
	}
	return result;
}
/**
 * find and consume all -verbose: options, then set the values accordingly.
 * @param vm J9JavaVM
 * @param J9VMDllLoadInfo* loadInfo
 * @return 0 if error, 1 if success
 */

#define VERBOSE_OPTION_BUF_SIZE 256
UDATA 
parseVerboseArgumentList(J9JavaVM* vm, J9VMDllLoadInfo* loadInfo, char **errorString) {
	char valuesBuffer[VERBOSE_OPTION_BUF_SIZE];				/* Should be long enough to cope with all possible options */
	char* valuesBufferPtr = valuesBuffer;
	IDATA bufEmptySpace = VERBOSE_OPTION_BUF_SIZE;
	IDATA verboseIndex = 0;
	UDATA foundArg = 0;
	J9VerboseSettings verboseOptions;
	IDATA xverbvrfyIndex = -1;
	IDATA xnoverbvrfyIndex = -1;
	IDATA verifyErrorDetailsIndex = -1;
	IDATA noVerifyErrorDetailsIndex = -1;
	
	PORT_ACCESS_FROM_JAVAVM(vm);

	memset(valuesBuffer, '\0', VERBOSE_OPTION_BUF_SIZE);
	memset(&verboseOptions, 0, sizeof(J9VerboseSettings));
	do {
		verboseIndex = FIND_AND_CONSUME_ARG( SEARCH_FORWARD | OPTIONAL_LIST_MATCH | (verboseIndex << STOP_AT_INDEX_SHIFT), OPT_VERBOSE, NULL );
		/* start the search from where we left off last time */
		if (verboseIndex >= 0) {
			foundArg = 1;

			GET_OPTION_VALUES( verboseIndex, ':', ',', &valuesBufferPtr, VERBOSE_OPTION_BUF_SIZE );			/* Returns values separated by single NULL chars */
			if (!parseVerboseArgument(valuesBuffer, &verboseOptions, errorString)) {
				return FALSE;
			}
			++verboseIndex;
		}
	} while (verboseIndex >= 0);

	xverbvrfyIndex = FIND_ARG_IN_VMARGS(EXACT_MATCH, VMOPT_XXVERBOSEVERIFICATION, NULL);
	xnoverbvrfyIndex = FIND_ARG_IN_VMARGS(EXACT_MATCH, VMOPT_XXNOVERBOSEVERIFICATION, NULL);
	if (xverbvrfyIndex > xnoverbvrfyIndex) {
		verboseOptions.verification = VERBOSE_SETTINGS_SET;
		foundArg = 1;
	}

	verifyErrorDetailsIndex = FIND_ARG_IN_VMARGS(EXACT_MATCH, VMOPT_XXVERIFYERRORDETAILS, NULL);
	noVerifyErrorDetailsIndex = FIND_ARG_IN_VMARGS(EXACT_MATCH, VMOPT_XXNOVERIFYERRORDETAILS, NULL);
	if (verifyErrorDetailsIndex >= noVerifyErrorDetailsIndex) {
		verboseOptions.verifyErrorDetails = VERBOSE_SETTINGS_SET;
		foundArg = 1;
	}

	if (foundArg) {
		J9VerboseStruct* verboseStruct;
		verboseStruct = (J9VerboseStruct* ) j9mem_allocate_memory(sizeof(J9VerboseStruct), OMRMEM_CATEGORY_VM);
		if (verboseStruct == NULL) {
			loadInfo->fatalErrorStr = "cannot allocate verboseStruct in verbose init";
			return 0;
		}
		vm->verboseStruct = verboseStruct;
		if (!setVerboseState( vm, &verboseOptions, errorString ) ) {
			return FALSE;
		}
	}
	return 1;
}

IDATA J9VMDllMain(J9JavaVM* vm, IDATA stage, void* reserved) {
	IDATA returnVal = J9VMDLLMAIN_OK;
	J9VMDllLoadInfo* loadInfo;
	IDATA verbosegclogIndex; 

#define VERBOSE_INIT_STAGE DLL_LOAD_TABLE_FINALIZED	/* defined separately for consistency of dependencies */
#define VERBOSE_EXIT_STAGE JVM_EXIT_STAGE	/* defined separately for consistency of dependencies */
#define VERBOSE_TEARDOWN_STAGE HEAP_STRUCTURES_FREED	/* defined separately for consistency of dependencies */

	PORT_ACCESS_FROM_JAVAVM(vm);

	J9MemoryManagerVerboseInterface *mmFuncTable = (J9MemoryManagerVerboseInterface *) ((vm->memoryManagerFunctions == NULL) ? NULL : vm->memoryManagerFunctions->getVerboseGCFunctionTable(vm));
	
	switch(stage) {
		case ALL_DEFAULT_LIBRARIES_LOADED:
			if (0 != initZipLibrary(vm->portLibrary, vm->j2seRootDirectory)) {
				goto _error;
			}
			break;

		case VERBOSE_INIT_STAGE :
			loadInfo = FIND_DLL_TABLE_ENTRY( THIS_DLL_NAME );

			vm->verboseLevel = 0;
			vm->setVerboseState = &setVerboseState;
			omrthread_monitor_init( &vm->verboseStateMutex, 0 );
			if( vm->verboseStateMutex == NULL ) {
				loadInfo->fatalErrorStr = "cannot allocate verboseStateMutex in verbose init";
				goto _error;
			}

			/* Note - verboseStruct not needed for modron verbose gc */
			initializeVerboseFunctionTable(vm);

			verbosegclogIndex = FIND_AND_CONSUME_ARG( OPTIONAL_LIST_MATCH, OPT_XVERBOSEGCLOG, NULL );
			if (verbosegclogIndex >= 0) {
				if (!initializeVerbosegclog(vm, verbosegclogIndex)) {
					loadInfo->fatalErrorStr =  (char*)j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG|J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VERB_FAILED_TO_INITIALIZE,"Failed to initialize.");
					goto _error;
				}
				vm->verboseLevel |= VERBOSE_GC;
			}
			if (!parseVerboseArgumentList(vm, loadInfo, &loadInfo->fatalErrorStr)) {
				goto _error;
			}

			if (FIND_AND_CONSUME_ARG(EXACT_MATCH, OPT_XSNW, NULL) >= 0) {
				J9HookInterface ** gcOmrHook = vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM);

				(*gcOmrHook)->J9HookRegisterWithCallSite(gcOmrHook, J9HOOK_MM_OMR_GLOBAL_GC_START, sniffAndWhackHookGC, OMR_GET_CALLSITE(), NULL);
				(*gcOmrHook)->J9HookRegisterWithCallSite(gcOmrHook, J9HOOK_MM_OMR_GLOBAL_GC_END, sniffAndWhackHookGC, OMR_GET_CALLSITE(), NULL);
				(*gcOmrHook)->J9HookRegisterWithCallSite(gcOmrHook, J9HOOK_MM_OMR_LOCAL_GC_START, sniffAndWhackHookGC, OMR_GET_CALLSITE(), NULL);
				(*gcOmrHook)->J9HookRegisterWithCallSite(gcOmrHook, J9HOOK_MM_OMR_LOCAL_GC_END, sniffAndWhackHookGC, OMR_GET_CALLSITE(), NULL);

				vm->runtimeFlags |= J9_RUNTIME_SNIFF_AND_WHACK;
				vm->whackedPointerCounter = 1;
				installVerboseStackWalker(vm);
			}

			break;

		_error :
			returnVal = J9VMDLLMAIN_FAILED;
			break;

		case JIT_INITIALIZED :
			/* Register this module with trace */
			UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(vm));
			Trc_VRB_VMInitStages_Event1(vm->mainThread);
			break;

		case ABOUT_TO_BOOTSTRAP :
			if (0 != (vm->verboseLevel & VERBOSE_DUMPSIZES)) {
				dumpMemorySizes(vm);
			}
			break;

		case POST_INIT_STAGE:
			initializeVerboseFunctionTable(vm);
			break;

		case LIBRARIES_ONUNLOAD :
			loadInfo = FIND_DLL_TABLE_ENTRY( THIS_DLL_NAME );
			if( IS_STAGE_COMPLETED( loadInfo->completedBits, VERBOSE_INIT_STAGE ) && vm->verboseStruct ) {
				j9mem_free_memory(vm->verboseStruct );
				vm->verboseStruct = NULL;
			}
			if( vm->verboseStateMutex != NULL ) {
				omrthread_monitor_destroy( vm->verboseStateMutex );
			}
			break;

		case VERBOSE_EXIT_STAGE : /* function table may not be populated if VM failed to start */
			if ((NULL != mmFuncTable) && (NULL != mmFuncTable->gcDebugVerboseShutdownLogging)){
				mmFuncTable->gcDebugVerboseShutdownLogging(vm, 0);
			}
			break;

		case VERBOSE_TEARDOWN_STAGE :
			if ((NULL != mmFuncTable) && (NULL != mmFuncTable->gcDebugVerboseShutdownLogging)){
				mmFuncTable->gcDebugVerboseShutdownLogging(vm, 1);
			}
			break;
	}
	return returnVal;
}


static IDATA
initializeVerbosegclogFromOptions(J9JavaVM* vm, char* vbgclogBuffer) 
{
	char* vbgclogBufferPtr;
	char* gclogName = NULL;
	UDATA fileCount = 0;
	UDATA blockCount = 0;
	UDATA scanResult;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9MemoryManagerVerboseInterface *mmFuncTable;
	
	vbgclogBufferPtr = vbgclogBuffer;
	
	if (*vbgclogBufferPtr) {
		/* User might not specify the filename, so we may be assigning a NULL value here */
		gclogName = vbgclogBufferPtr;
	} else {
		/* -Xverbosegclog was specified on its own, so generate a default filename format */
		gclogName = "verbosegc.%Y%m%d.%H%M%S.%pid.txt";
	}

	/* Now look for fileCount */
	vbgclogBufferPtr += strlen(vbgclogBufferPtr) + 1;
	if (*vbgclogBufferPtr) {
		scanResult = scan_udata(&vbgclogBufferPtr, &fileCount);
		if (scanResult || (0 == fileCount)) {
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VERB_XVERBOSEGCLOG_NUM_FILES);
			return FALSE;
		}
	}

	/* Now look for blockCount */	
	vbgclogBufferPtr += strlen(vbgclogBufferPtr) + 1;
	if (*vbgclogBufferPtr) {
		scanResult = scan_udata(&vbgclogBufferPtr, &blockCount);
		if (scanResult || (0 == blockCount)) {
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VERB_XVERBOSEGCLOG_NUM_CYCLES);
			return FALSE;
		}
	}

	if ((0 != fileCount) && (0 == blockCount)) {
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VERB_XVERBOSEGCLOG_NUM_CYCLES);
		return FALSE;
	}

	if ((0 == fileCount) && (0 != blockCount)) {
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VERB_XVERBOSEGCLOG_NUM_FILES);
		return FALSE;
	}

	if (!gclogName && (blockCount || fileCount)) {
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VERB_XVERBOSEGCLOG_FILENAME);
		return FALSE;
	}

	mmFuncTable = (J9MemoryManagerVerboseInterface *)vm->memoryManagerFunctions->getVerboseGCFunctionTable(vm);
	return mmFuncTable->gcDebugVerboseStartupLogging(vm, gclogName, fileCount, blockCount);
}

static IDATA 
initializeVerbosegclog(J9JavaVM* vm, IDATA vbgclogIndex) 
{
	char* vbgclogBuffer = NULL;
	UDATA bufferSize = 128;
	IDATA result;
	PORT_ACCESS_FROM_JAVAVM(vm);

	do {
		bufferSize *= 2;
		j9mem_free_memory(vbgclogBuffer);
		vbgclogBuffer = j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_VM);
		if (NULL == vbgclogBuffer) {
			return J9VMDLLMAIN_FAILED;
		}
	} while (OPTION_BUFFER_OVERFLOW == GET_OPTION_VALUES(vbgclogIndex, ':', ',', &vbgclogBuffer, bufferSize));

	result = initializeVerbosegclogFromOptions(vm, vbgclogBuffer);
	
	j9mem_free_memory(vbgclogBuffer);
	
	return result;
}


static void installVerboseStackWalker(J9JavaVM * vm)
{
	vm->verboseStackDump = verboseStackDump;
	vm->walkFrame = walkFrameVerbose;
	vm->walkStackFrames = walkStackFramesVerbose;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	vm->jitWalkStackFrames = jitWalkStackFramesVerbose;
	vm->jitExceptionHandlerSearch = jitExceptionHandlerSearchVerbose;
	vm->jitGetOwnedObjectMonitors = jitGetOwnedObjectMonitorsVerbose;
#endif
}


static void verboseStackDump(J9VMThread * vmThread, const char * msg)
{
	J9JavaVM * vm = vmThread->javaVM;
	UDATA oldVerboseLevel;
	J9StackWalkState walkState;

	if (msg) {
		PORT_ACCESS_FROM_JAVAVM(vm);

		j9tty_printf(PORTLIB, "<%p> Verbose stack walk due to: %s\n", vmThread, msg);
	}

	oldVerboseLevel = vm->stackWalkVerboseLevel;
	vm->stackWalkVerboseLevel = DEFAULT_STACKWALK_VERBOSE_LEVEL;

	walkState.objectSlotWalkFunction = verboseEmptyOSlotIterator;
	walkState.walkThread = vmThread;
	walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
	vm->walkStackFrames(vmThread, &walkState);

	vm->stackWalkVerboseLevel = oldVerboseLevel;
}

static void verboseEmptyOSlotIterator(J9VMThread * currentThread, J9StackWalkState * walkState, j9object_t * objectSlot, const void * stackLocation)
{
}

static void 
verboseHookGC(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	PORT_ACCESS_FROM_PORT(userData);

	switch (eventNum) {
		case J9HOOK_MM_OMR_LOCAL_GC_START:
			j9tty_printf(PORTLIB, "{");
			break;
		case J9HOOK_MM_OMR_LOCAL_GC_END:
			j9tty_printf(PORTLIB, "}");
			break;
		case J9HOOK_MM_OMR_GLOBAL_GC_START:
			j9tty_printf(PORTLIB, "\n<GGC ...");
			break;
		case J9HOOK_MM_OMR_GLOBAL_GC_END:
			j9tty_printf(PORTLIB, ">");
			break;
	}
}

/* Modify the verbose reporting state based on the value of option.
	On success, returns true. On failure, returns false and may set *errorString to
	point to an error message if errorString is non-null.
	This method is being adapted to be callable repeatedly during a run to turn options
	on and off. Currently, the only option that can be toggled is class, by passing "class" or
	"noclass" as the option. In order to make other options fully support the same behaviour,
	they need to set and check a bit in verboseLevel to avoid enabling the option twice,
	support a "nofoo" option to disable the option if enabled, and support re-enabling the
	option after it has been disabled.
	This method holds the verboseStateMutex to prevent multiple threads from modifying
	verbose options simultaneously. */
IDATA
setVerboseState( J9JavaVM *vm, J9VerboseSettings *verboseOptions, char **errorString )
{
	J9HookInterface **vmHooks, **gcOmrHooks, **vmZipCachePoolHooks;
	J9MemoryManagerVerboseInterface *mmFuncTable = (J9MemoryManagerVerboseInterface *)vm->memoryManagerFunctions->getVerboseGCFunctionTable(vm);
	IDATA result = TRUE;

	omrthread_monitor_enter( vm->verboseStateMutex );

	if(VERBOSE_SETTINGS_CLEAR == verboseOptions->vclass) {
		if( vm->verboseLevel & VERBOSE_CLASS ) {
			/* turn verbose:class off */
			vm->verboseLevel &= ~VERBOSE_CLASS;

			vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);
			(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASS_LOAD, verboseHookClassLoad, NULL);

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
			(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASS_UNLOAD, verboseHookClassUnload, NULL);
#endif
		}
	} else	if(VERBOSE_SETTINGS_SET == verboseOptions->vclass ) {
		if( !(vm->verboseLevel & VERBOSE_CLASS) ) {
			/* turn verbose:class on */
			vm->verboseLevel |= VERBOSE_CLASS;

			vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);
			(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_LOAD, verboseHookClassLoad, OMR_GET_CALLSITE(), NULL);

			#ifdef J9VM_OPT_ZIP_SUPPORT
			vmZipCachePoolHooks = zip_getVMZipCachePoolHookInterface(vm->zipCachePool);
			(*vmZipCachePoolHooks)->J9HookRegisterWithCallSite(vmZipCachePoolHooks, J9HOOK_VM_ZIP_LOAD, zipCachePoolHookCallback, OMR_GET_CALLSITE(), NULL);
#endif

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
			(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_UNLOAD, verboseHookClassUnload, OMR_GET_CALLSITE(), NULL);
#endif
		}
	} 
	if(VERBOSE_SETTINGS_SET == verboseOptions->gcterse) {
		gcOmrHooks = vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM);

		(*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, verboseHookGC, OMR_GET_CALLSITE(), vm->portLibrary);
		(*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_LOCAL_GC_END, verboseHookGC, OMR_GET_CALLSITE(), vm->portLibrary);
		(*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, verboseHookGC, OMR_GET_CALLSITE(), vm->portLibrary);
		(*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, verboseHookGC, OMR_GET_CALLSITE(), vm->portLibrary);

	}
	if(VERBOSE_SETTINGS_SET == verboseOptions->gc) {
		/* If verbosegclog has started, we do not need to initialize gc logging */
		if( !(vm->verboseLevel & VERBOSE_GC) ) {
			if(!mmFuncTable->gcDebugVerboseStartupLogging(vm, NULL, 0, 0)) {
				if( errorString ) {
					*errorString = "unrecognised option for -verbose:<opt>";
				}
				result = FALSE;
			} else {
				vm->verboseLevel |= VERBOSE_GC;
			}
		}
	} else if(VERBOSE_SETTINGS_CLEAR == verboseOptions->gc) {
		if( vm->verboseLevel & VERBOSE_GC ) {
			mmFuncTable->configureVerbosegc( vm, 0, NULL, 0, 0 );
			vm->verboseLevel &= ~VERBOSE_GC;
		}
	}
#ifdef J9VM_OPT_DYNAMIC_LOAD_SUPPORT
	if(VERBOSE_SETTINGS_SET == verboseOptions->dynload) {
		vm->verboseLevel |= VERBOSE_DYNLOAD;
		vm->verboseStruct->hookDynamicLoadReporting = hookDynamicLoadReporting;
	}
#endif
	if(VERBOSE_SETTINGS_SET == verboseOptions->stackwalk) {
		vm->stackWalkVerboseLevel=verboseOptions->stackWalkVerboseLevel;
		installVerboseStackWalker(vm);
	} 
	if(VERBOSE_SETTINGS_SET == verboseOptions->sizes) {	
		vm->verboseLevel |= VERBOSE_DUMPSIZES;
	}
	if(VERBOSE_SETTINGS_SET == verboseOptions->jni) {
		vm->checkJNIData.options |= JNICHK_VERBOSE;
	} else if(VERBOSE_SETTINGS_CLEAR == verboseOptions->jni) {
		vm->checkJNIData.options &= ~JNICHK_VERBOSE;
	}
	if(VERBOSE_SETTINGS_SET == verboseOptions->stack) {
		vm->verboseLevel |= VERBOSE_STACK;
	} else if(VERBOSE_SETTINGS_CLEAR == verboseOptions->stack) {
		vm->verboseLevel &= ~VERBOSE_STACK;
	} 
	if(VERBOSE_SETTINGS_SET == verboseOptions->stacktrace) {
		vm->verboseLevel |= VERBOSE_STACKTRACE;
	} else if(VERBOSE_SETTINGS_CLEAR == verboseOptions->stacktrace) {
		vm->verboseLevel &= ~VERBOSE_STACKTRACE;
	}
	if(VERBOSE_SETTINGS_SET == verboseOptions->shutdown) {
		vm->verboseLevel |= VERBOSE_SHUTDOWN;
	} else if(VERBOSE_SETTINGS_CLEAR == verboseOptions->shutdown) {
		vm->verboseLevel &= ~VERBOSE_SHUTDOWN;
	}
	if(VERBOSE_SETTINGS_SET == verboseOptions->debug) {
		vm->verboseLevel |= VERBOSE_DEBUG;
	} else if(VERBOSE_SETTINGS_CLEAR == verboseOptions->debug) {
		vm->verboseLevel &= ~VERBOSE_DEBUG;
	}
	if(VERBOSE_SETTINGS_SET == verboseOptions->init) {
		vm->verboseLevel |= VERBOSE_INIT;
	} else if(VERBOSE_SETTINGS_CLEAR == verboseOptions->init) {
		vm->verboseLevel &= ~VERBOSE_INIT;
	}
	if(VERBOSE_SETTINGS_SET == verboseOptions->relocations) { 
		vm->verboseLevel |= VERBOSE_RELOCATIONS;
	} else if(VERBOSE_SETTINGS_CLEAR == verboseOptions->relocations) { 
		vm->verboseLevel &= ~VERBOSE_RELOCATIONS;
	}
	if(VERBOSE_SETTINGS_SET == verboseOptions->romclass) {
		vm->verboseLevel |= VERBOSE_ROMCLASS;
	} else if(VERBOSE_SETTINGS_CLEAR == verboseOptions->romclass) {
		vm->verboseLevel &= ~VERBOSE_ROMCLASS;
	}
	if (VERBOSE_SETTINGS_SET == verboseOptions->verification) {
		vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_VERIFICATION_START, verboseClassVerificationStart, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_VERIFICATION_FALLBACK, verboseClassVerificationFallback, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_VERIFICATION_END, verboseClassVerificationEnd, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_METHOD_VERIFICATION_START, verboseMethodVerificationStart, OMR_GET_CALLSITE(), NULL);
		(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_STACKMAPFRAME_VERIFICATION, verboseStackMapFrameVerification, OMR_GET_CALLSITE(), NULL);
	} else if (VERBOSE_SETTINGS_CLEAR == verboseOptions->verification) {
		vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASS_VERIFICATION_START, verboseClassVerificationStart, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASS_VERIFICATION_FALLBACK, verboseClassVerificationFallback, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASS_VERIFICATION_END, verboseClassVerificationEnd, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_METHOD_VERIFICATION_START, verboseMethodVerificationStart, NULL);
		(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_STACKMAPFRAME_VERIFICATION, verboseStackMapFrameVerification, NULL);
	}
	
	/* Jazz 82615: Register the callback functions for the error message framework if the VerifyErrorDetails option is specified */
	if(VERBOSE_SETTINGS_SET == verboseOptions->verifyErrorDetails) {
		vm->verboseStruct->getCfrExceptionDetails = generateJ9CfrExceptionDetails;
		vm->verboseStruct->getRtvExceptionDetails = generateJ9RtvExceptionDetails;
	}
	omrthread_monitor_exit( vm->verboseStateMutex );

	return result;
}

static void 
sniffAndWhackHookGC(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	/* Assume all gc start/end events have currentThread in the same position */
	J9VMThread * currentThread = (J9VMThread*)((MM_GlobalGCStartEvent *) eventData)->currentThread->_language_vmthread;
	J9JavaVM * vm = currentThread->javaVM;
	J9VMThread * walkThread;

	walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (walkThread != NULL) {
		J9StackWalkState walkState;

		walkState.objectSlotWalkFunction = verboseEmptyOSlotIterator;
		walkState.walkThread = walkThread;
		walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS;
		vm->walkStackFrames(currentThread, &walkState);

		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}
}


static UDATA
getQualifiedSize(UDATA byteSize, const char **qualifierString)
{
	UDATA size = byteSize;
	const char *qualifier = "";
	if(!(size % 1024)) {
		size /= 1024;
		qualifier = "K";
		if(size && !(size % 1024)) {
			size /= 1024;
			qualifier = "M";
			if(size && !(size % 1024)) {
				size /= 1024;
				qualifier = "G";
			}
		}
	}

	*qualifierString = qualifier;
	return size;
}

/*
 * This event callback is triggered before starting the verification for the class.
 */
static void
verboseClassVerificationStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9BytecodeVerificationData *verifyData = ((J9VMClassVerificationStartEvent *)eventData)->verifyData;
	BOOLEAN newFormat = ((J9VMClassVerificationStartEvent *)eventData)->newFormat;
	PORT_ACCESS_FROM_JAVAVM(verifyData->javaVM);
	J9UTF8* utf = J9ROMCLASS_CLASSNAME(verifyData->romClass);
	char nameArray[256];
	VerboseVerificationBuffer nameBuf;
	J9UTF8* qualified = NULL;
	char byteArray[1024];
	VerboseVerificationBuffer buf;

	initVerboseVerificationBuffer(&nameBuf, sizeof(nameArray), nameArray);
	initVerboseVerificationBuffer(&buf, sizeof(byteArray), byteArray);
	qualified = toExternalQualifiedName(PORTLIB, &nameBuf, utf);

	if (NULL != qualified) {
		printVerificationInfo(PORTLIB, &buf, "Verifying class %.*s with %s format\n",
				(UDATA)J9UTF8_LENGTH(qualified), J9UTF8_DATA(qualified),
				newFormat ? "new" : "old");
		flushVerificationBuffer(PORTLIB, &buf);
	}
	releaseVerboseVerificationBuffer(PORTLIB, &buf, byteArray);
	releaseVerboseVerificationBuffer(PORTLIB, &nameBuf, nameArray);
}

/*
 * This event callback is triggered after completed the verification for the class.
 */
static void
verboseClassVerificationEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9BytecodeVerificationData *verifyData = ((J9VMClassVerificationEndEvent *)eventData)->verifyData;
	BOOLEAN newFormat = ((J9VMClassVerificationStartEvent *)eventData)->newFormat;
	PORT_ACCESS_FROM_JAVAVM(verifyData->javaVM);
	J9UTF8* utf = J9ROMCLASS_CLASSNAME(verifyData->romClass);
	char nameArray[256];
	VerboseVerificationBuffer nameBuf;
	J9UTF8* qualified = NULL;
	char byteArray[1024];
	VerboseVerificationBuffer buf;

	initVerboseVerificationBuffer(&nameBuf, sizeof(nameArray), nameArray);
	initVerboseVerificationBuffer(&buf, sizeof(byteArray), byteArray);
	qualified = toExternalQualifiedName(PORTLIB, &nameBuf, utf);

	if (NULL != qualified) {
		if (newFormat && ((-1 != verifyData->errorModule) || (-1 != verifyData->errorCode))) {
			printVerificationInfo(PORTLIB, &buf, "Verification for %.*s failed\n",
					(UDATA)J9UTF8_LENGTH(qualified), J9UTF8_DATA(qualified));
		}

		printVerificationInfo(PORTLIB, &buf, "End class verification for: %.*s\n",
				(UDATA)J9UTF8_LENGTH(qualified), J9UTF8_DATA(qualified));

		flushVerificationBuffer(PORTLIB, &buf);
	}
	releaseVerboseVerificationBuffer(PORTLIB, &buf, byteArray);
	releaseVerboseVerificationBuffer(PORTLIB, &nameBuf, nameArray);
}

/*
 * This event callback is triggered before retrying verification with old format(no stack map frame verification) on a
 * class with version smaller that 51
 */
static void
verboseClassVerificationFallback(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9BytecodeVerificationData *verifyData = ((J9VMMethodVerificationEndEvent *)eventData)->verifyData;
	BOOLEAN newFormat = ((J9VMClassVerificationStartEvent *)eventData)->newFormat;
	PORT_ACCESS_FROM_JAVAVM(verifyData->javaVM);
	J9UTF8* utf = J9ROMCLASS_CLASSNAME(verifyData->romClass);
	char nameArray[256];
	VerboseVerificationBuffer nameBuf;
	J9UTF8* qualified = NULL;
	char byteArray[1024];
	VerboseVerificationBuffer buf;

	initVerboseVerificationBuffer(&nameBuf, sizeof(nameArray), nameArray);
	initVerboseVerificationBuffer(&buf, sizeof(byteArray), byteArray);
	qualified = toExternalQualifiedName(PORTLIB, &nameBuf, utf);

	if (NULL != qualified) {
		printVerificationInfo(PORTLIB, &buf, "Fail over class verification to old verifier for: %.*s\n",
				(UDATA)J9UTF8_LENGTH(qualified), J9UTF8_DATA(qualified));
		printVerificationInfo(PORTLIB, &buf, "Verifying class %.*s with %s format\n",
				(UDATA)J9UTF8_LENGTH(qualified), J9UTF8_DATA(qualified),
				newFormat ? "new" : "old");

		flushVerificationBuffer(PORTLIB, &buf);
	}
	releaseVerboseVerificationBuffer(PORTLIB, &buf, byteArray);
	releaseVerboseVerificationBuffer(PORTLIB, &nameBuf, nameArray);
}

/*
 * This event callback is triggered before starting the verification of each method.
 */
static void
verboseMethodVerificationStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9BytecodeVerificationData *verifyData = ((J9VMMethodVerificationStartEvent *)eventData)->verifyData;
	PORT_ACCESS_FROM_JAVAVM(verifyData->javaVM);
	J9UTF8* utf = J9ROMCLASS_CLASSNAME(verifyData->romClass);
	J9UTF8* utfMethod = J9ROMMETHOD_NAME(verifyData->romMethod);
	J9UTF8* utfSignature = J9ROMMETHOD_SIGNATURE(verifyData->romMethod);
	char nameArray[256];
	VerboseVerificationBuffer nameBuf;
	J9UTF8* qualified = NULL;
	char byteArray[1024];
	VerboseVerificationBuffer buf;

	initVerboseVerificationBuffer(&nameBuf, sizeof(nameArray), nameArray);
	initVerboseVerificationBuffer(&buf, sizeof(byteArray), byteArray);
	qualified = toExternalQualifiedName(PORTLIB, &nameBuf, utf);

	if (NULL != qualified) {
		printVerificationInfo(PORTLIB, &buf, "Verifying method %.*s.%.*s%.*s\n",
				(UDATA)J9UTF8_LENGTH(qualified), J9UTF8_DATA(qualified),
				(UDATA)J9UTF8_LENGTH(utfMethod), J9UTF8_DATA(utfMethod),
				(UDATA)J9UTF8_LENGTH(utfSignature), J9UTF8_DATA(utfSignature));

		flushVerificationBuffer(PORTLIB, &buf);
	}
	releaseVerboseVerificationBuffer(PORTLIB, &buf, byteArray);
	releaseVerboseVerificationBuffer(PORTLIB, &nameBuf, nameArray);
}

/*
 * This event callback is triggered after completed the verification of a method.
 */
static void
verboseStackMapFrameVerification(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9BytecodeVerificationData *verifyData = ((J9VMStackMapFrameVerificationEvent *)eventData)->verifyData;
	PORT_ACCESS_FROM_JAVAVM(verifyData->javaVM);
	J9BranchTargetStack * stackMapFrame = NULL;
	IDATA stackMapsCount = 0;
	char byteArray[1024];
	VerboseVerificationBuffer buf;

	initVerboseVerificationBuffer(&buf, sizeof(byteArray), byteArray);

	printVerificationInfo(PORTLIB, &buf, "StackMapTable: frame_count = %d\ntable = { \n",
			verifyData->stackMapsCount);
	/* walk through each stack map frame. */
	for (stackMapsCount = 0; stackMapsCount < verifyData->stackMapsCount; stackMapsCount++) {
		flushVerificationBuffer(PORTLIB, &buf);
		stackMapFrame = BCV_INDEX_STACK(stackMapsCount);
		printStackMapFrame(PORTLIB, &buf, verifyData, stackMapFrame);
	}

	printVerificationInfo(PORTLIB, &buf, " }\n");
	flushVerificationBuffer(PORTLIB, &buf);

	releaseVerboseVerificationBuffer(PORTLIB, &buf, byteArray);
}

/*
 * Prints out PC corresponds to the stack map frame.
 * Prints out local variables' data type on the stack map frame.
 * Prints out data type on the stack
 */
static void
printStackMapFrame(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, J9BytecodeVerificationData* verifyData, J9BranchTargetStack* stackMapFrame)
{
	IDATA slot = 0;
	IDATA lastLocal = 0;
	char format[266];
	BOOLEAN firstEntry = TRUE;
	UDATA encodedType = BCV_BASE_TYPE_TOP;

	PORT_ACCESS_FROM_PORT(portLibrary);

	printVerificationInfo(PORTLIB, buf, "  bci: @%d\n  flags: {%s}\n",
			stackMapFrame->pc,
			stackMapFrame->uninitializedThis ? " flagThisUninit " : " ");

	/* print locals */
	printVerificationInfo(PORTLIB, buf, "  locals: {");

	/* entries from the beginning of stackElements to the last non-TOP entry are valid */
	for (lastLocal = stackMapFrame->stackBaseIndex - 1; lastLocal > 0; lastLocal--) {
		if ((BCV_BASE_TYPE_TOP != stackMapFrame->stackElements[lastLocal])) {
			break;
		}
	}

	for (slot = 0; slot <= lastLocal;) {
		printVerificationInfo(PORTLIB, buf, (firstEntry ? " " : ", "));

		encodedType = stackMapFrame->stackElements[slot];
		/* if we see a wide data type, double or long, skip the next entry. */
		slot += constructPrintFormat(encodedType, format, sizeof(format));
		printDataType(PORTLIB, buf, verifyData, encodedType, format);

		firstEntry = FALSE;
	}

	printVerificationInfo(PORTLIB, buf, " }\n");

	/*  print stack	 */
	printVerificationInfo(PORTLIB, buf, "  stack: {");

	firstEntry = TRUE;
	for (slot = stackMapFrame->stackBaseIndex; slot < stackMapFrame->stackTopIndex;) {
		printVerificationInfo(PORTLIB, buf, (firstEntry ? " " : ", "));

		encodedType = stackMapFrame->stackElements[slot];
		/* if we see a wide data type, double or long, skip the next entry. */
		slot += constructPrintFormat(encodedType, format, sizeof(format));
		printDataType(PORTLIB, buf, verifyData, encodedType, format);
		firstEntry = FALSE;
	}

	printVerificationInfo(PORTLIB, buf, " }\n");
}

/*
 * Construct print format based on BCV type
 * format buffer size is 266. it is guaranteed to be able to fit the maximum
 * format ('[[[[...254[L%.*s')
 */
static UDATA
constructPrintFormat(UDATA encodedType, char* format, UDATA fmtSize)
{
	UDATA wideType = 1;
	UDATA typeTag = encodedType & BCV_TAG_MASK;
	UDATA arity;
	char* cursor = format;
	const char baseFormat[] = "%.*s";
	const char wideBaseFormat[] = "%.*s, %.*s_2nd";
	BOOLEAN isObjectArray = FALSE;

	if (BCV_TAG_BASE_TYPE_OR_TOP == typeTag) {
		if (0 != (encodedType & BCV_WIDE_TYPE_MASK)) {
			wideType += 1;
			strncpy(format, wideBaseFormat, sizeof(wideBaseFormat));
		} else {
			strncpy(format, baseFormat, sizeof(baseFormat));
		}
	} else {
		arity = BCV_ARITY_FROM_TYPE(encodedType);
		if (BCV_TAG_BASE_ARRAY_OR_NULL == typeTag) {
			arity += 1;
			arity &= BCV_ARITY_MASK >> BCV_ARITY_SHIFT;
			if (0 == arity) {
				strncpy(format, baseFormat, sizeof(baseFormat));
				return wideType;
			}
		}
		if ((BCV_TAG_BASE_ARRAY_OR_NULL != typeTag) && (arity > 0)) {
			isObjectArray = TRUE;
		}

		*cursor++ = '\'';
		for (; arity > 0; arity--) {
			*cursor++ = '[';
		}
		if (isObjectArray) {
			*cursor++ = 'L';
		}
		strncpy(cursor, baseFormat, sizeof(baseFormat));
		cursor += sizeof(baseFormat) - 1;
		if (isObjectArray) {
			*cursor++ = ';';
		}
		*cursor++ = '\'';
		*cursor = '\0';
	}

	Assert_VRB_true(cursor < (format + fmtSize));

	return wideType;
}

/*
 * BCV_TAG_BASE_TYPE_OR_TOP and BCV_TAG_BASE_ARRAY_OR_NULL each represents 8 basic types
 * each basic type uses the following strings as its name
 */

#define BASE_ARRAY_INDEX_OFFSET 8
static const char*
baseTypeNames[] = {
	"top",	"integer", "float", "long",	"double", "short", "byte", "char",
	"null",	"I",       "F",     "J",    "D",      "S",     "B",    "C"
};

/*
 * convert BCV_BASE_TYPE to a index to baseTypeNames table.
 */
static IDATA
baseTypeToIndex(UDATA encodedType)
{
	IDATA index;

	switch (encodedType & 0xFE0) {
	case BCV_BASE_TYPE_INT_BIT:
		index = 1;
		break;
	case BCV_BASE_TYPE_FLOAT_BIT:
		index = 2;
		break;
	case BCV_BASE_TYPE_LONG_BIT:
		index = 3;
		break;
	case BCV_BASE_TYPE_DOUBLE_BIT:
		index = 4;
		break;
	case BCV_BASE_TYPE_SHORT_BIT:
		index = 5;
		break;
	case BCV_BASE_TYPE_BYTE_BIT:
		index = 6;
		break;
	case BCV_BASE_TYPE_CHAR_BIT:
		index = 7;
		break;
	default:
		index = 0;
		break;
	}

	return index;
}

/*
 * convert a BCV encoded data type to a human readable string based on specified format.
 */
static UDATA
printDataType(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, J9BytecodeVerificationData* verifyData, UDATA encodedType, const char* fmt)
{
	J9ROMClass* romClass = verifyData->romClass;
	struct J9UTF8* utf = NULL;
	U_32* offset = 0;
	UDATA typeTag = encodedType & BCV_TAG_MASK;
	UDATA msgLen = 0;
	UDATA cpIndex = 0;
	const char* typeString = NULL;
	J9ROMConstantPoolItem *constantPool = NULL;
	U_8* code = NULL;
	PORT_ACCESS_FROM_PORT(portLibrary);

	switch (typeTag) {
	case BCV_TAG_BASE_TYPE_OR_TOP:
		typeString = baseTypeNames[baseTypeToIndex(encodedType)];
		msgLen = strlen(typeString);

		if (0 != (encodedType & BCV_WIDE_TYPE_MASK)) {
			printVerificationInfo(PORTLIB, buf, fmt, msgLen, typeString, msgLen, typeString);
		} else {
			printVerificationInfo(PORTLIB, buf, fmt, msgLen, typeString);
		}
		break;

	case BCV_TAG_BASE_ARRAY_OR_NULL:
		typeString = baseTypeNames[baseTypeToIndex(encodedType)+BASE_ARRAY_INDEX_OFFSET];
		msgLen = strlen(typeString);
		printVerificationInfo(PORTLIB, buf, fmt, msgLen, typeString);
		break;

	case BCV_SPECIAL_NEW:
		code = J9_BYTECODE_START_FROM_ROM_METHOD(verifyData->romMethod);
		constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
		code += BCV_INDEX_FROM_TYPE(encodedType) + 1;
#ifdef J9VM_ENV_LITTLE_ENDIAN
		NEXT_U16_ENDIAN(FALSE, cpIndex, code);
#else /* BIG_ENDIAN */
		NEXT_U16_ENDIAN(TRUE, cpIndex, code);
#endif /* J9VM_ENV_LITTLE_ENDIAN */
		utf = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) (&constantPool[cpIndex]));
		printVerificationInfo(PORTLIB, buf, fmt, J9UTF8_LENGTH(utf), J9UTF8_DATA(utf));
		break;
	case BCV_OBJECT_OR_ARRAY: /* FALLTHROUGH */
	case BCV_SPECIAL_INIT: /* FALLTHROUGH */
	default:
		offset = (U_32*) verifyData->classNameList[BCV_INDEX_FROM_TYPE(encodedType)];
		msgLen = (U_32) J9UTF8_LENGTH(offset + 1);
		if (offset[0] == 0) {
			typeString = (const char*)J9UTF8_DATA(offset + 1);
		} else {
			typeString = (const char*)((UDATA) offset[0] + (UDATA) romClass);
		}
		printVerificationInfo(PORTLIB, buf, fmt, msgLen, typeString);
		break;
	}

	return msgLen;
}

/*
 * This function print formatted message to VerboseVerificationBuffer if there is enough room available.
 * If not enough room left in the buffer and the buffer could hold all formatted message if it were empty,
 * it will flush the contents in the buffer first and then print formatted message.
 *
 * If the buffer is not going to fit for the formatted message any way at all,  it will flush out its current
 * contents and then print out the formatted message directly to terminal.
 * @param minMsgLen minimum length of a formatted message.
 */
static void
printVerificationInfo(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, const char* msgFormat, ...)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA printable = 0;
	UDATA msgLen = 0;

	va_list args;
	
	if ((NULL == msgFormat) || ('\0' == msgFormat[0])) {
		return;
	}

	va_start(args, msgFormat);

	printable = buf->size - buf->cursor;
	msgLen = j9str_vprintf((char*)&buf->buffer[buf->cursor], printable, msgFormat, args);
	/*
	 * str_vprintf always null terminates and
	 * returns number characters written excluding the null terminator
	 */
	if (printable > (msgLen + 1)) {
		buf->cursor += msgLen;
		return;
	}

	/*
	 * If the available buffer is too small, or even is the exact size,
	 * we have to determine the real message length then print to the buffer.
	 */

	msgLen = j9str_vprintf(NULL, (U_32)-1, msgFormat, args);

	if (buf->size < msgLen) {
		flushVerificationBuffer(PORTLIB, buf);
		j9tty_vprintf(msgFormat, args);
	} else {
		while (buf->cursor < buf->size) {
			printable = buf->size - buf->cursor;
			if (msgLen <= printable) {
				buf->cursor += j9str_vprintf((char*)&buf->buffer[buf->cursor], msgLen, msgFormat, args);
				break;
			} else {
				flushVerificationBuffer(PORTLIB, buf);
			}
		}
	}

	va_end(args);
}

/*
 * flush out contents of VerboseVerificationBuffer to terminal
 */
static void
flushVerificationBuffer(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	buf->buffer[buf->cursor] = '\0';
	j9tty_printf(PORTLIB, (const char*)buf->buffer);
	buf->cursor = 0;
}

/*
 * duplicate the specified utf string and replace '/' to '.' for class name
 * caller is responsible for passing in an empty buffer
 */
static J9UTF8*
toExternalQualifiedName(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, J9UTF8* utf)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9UTF8* qualified = NULL;

	if (NULL != utf) {
		U_8* data = NULL;
		U_8* extData = NULL;

		/* if existing buffer dose not fit the data, use allocated buffer from heap */
		if (buf->size < (UDATA)J9UTF8_LENGTH(utf)) {
			qualified = (J9UTF8*)j9mem_allocate_memory((sizeof(utf->length) + utf->length), OMRMEM_CATEGORY_VM);
			if (NULL == qualified) {
				Trc_VRB_Allocate_Memory_Failed(sizeof(utf->length) + utf->length);
				return NULL;
			} else {
				buf->buffer = (U_8*)qualified;
			}
		} else {
			qualified = (J9UTF8*)buf->buffer;
		}

		data = utf->data;
		extData = qualified->data;
		qualified->length = 0;
		while (qualified->length != utf->length) {
			*extData = (('/' == *data) ? '.' : *data);
			extData += 1;
			data += 1;
			qualified->length += 1;
		}

		buf->cursor = sizeof(qualified->length) + qualified->length;
	}

	return qualified;
}

static void
releaseVerboseVerificationBuffer(J9PortLibrary* portLibrary, VerboseVerificationBuffer* buf, char byteArray[])
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	if (buf->buffer != (U_8*)byteArray) {
		j9mem_free_memory(buf->buffer);
	}
}

static void
initVerboseVerificationBuffer(VerboseVerificationBuffer* buf, UDATA size, char* byteArray)
{
	buf->size = size;
	buf->cursor = 0;
	buf->buffer = (U_8*)byteArray;
}

