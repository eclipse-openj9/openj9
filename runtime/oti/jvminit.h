/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#ifndef JVMINIT_H
#define JVMINIT_H

#include "vmargs_core_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Do not have more than 32 flags */

#define LOAD_BY_DEFAULT 0x1
#define FORCE_LATE_LOAD 0x2
#define FORCE_UNLOAD 0x4
#define FAILED_TO_LOAD 0x8
#define FAILED_TO_UNLOAD 0x10
#define LOADED 0x20
#define NOT_A_LIBRARY 0x40
#define XRUN_LIBRARY 0x80
#define SILENT_NO_DLL 0x100
#define FATAL_NO_DLL 0x200
#define FREE_ERROR_STRING 0x400
#define MAGIC_LOAD 0x800			/* Used by hook, which is loaded magically */
#define NO_J9VMDLLMAIN 0x1000
#define EARLY_LOAD 0x2000
#define ALLOW_POST_INIT_LOAD 0x4000
#define ALTERNATE_LIBRARY_NAME 0x8000
#define ALTERNATE_LIBRARY_USED 0x10000
#define AGENT_XRUN 0x20000
#define NEVER_CLOSE_DLL 0x40000
#define BUNDLED_COMP 0x80000
	
/* Value for RC_SILENT_EXIT should not be changed as this is
 * used by launcher to hide message "Could not create the Java virtual machine"
 */
#define RC_SILENT_EXIT -72

#define J9VMDLLMAIN_SILENT_EXIT_VM -2
#define J9VMDLLMAIN_FAILED -1
#define J9VMDLLMAIN_OK 0
#define J9VMDLLMAIN_NON_UTILITY_OK 1

#define EXACT_MATCH 1
#define STARTSWITH_MATCH 2
#define EXACT_MEMORY_MATCH 3
#define OPTIONAL_LIST_MATCH 4
#define OPTIONAL_LIST_MATCH_USING_EQUALS 5
#define SEARCH_FORWARD 0x1000
#define STOP_AT_INDEX_SHIFT 16
#define REAL_MATCH_MASK 0xFFF

#define INVALID_OPTION 1
#define EXACT_INVALID_OPTION 3
#define STARTSWITH_INVALID_OPTION 5

#define MAP_TWO_COLONS_TO_ONE 8
#define EXACT_MAP_NO_OPTIONS 16
#define EXACT_MAP_WITH_OPTIONS 32
#define MAP_MEMORY_OPTION 64
#define STARTSWITH_MAP_NO_OPTIONS 128
#define MAP_ONE_COLON_TO_TWO 256
#define MAP_WITH_INCLUSIVE_OPTIONS 512

#define POSTINIT_LIBRARY_NOT_FOUND -100
#define POSTINIT_NOT_PERMITTED -101
#define POSTINIT_LOAD_FAILED -102

#define GET_OPTION 1
#define GET_OPTIONS 2
#define GET_OPTION_OPT 3
#define GET_MAPPED_OPTION 4
#define GET_MEM_VALUE 5
#define GET_INT_VALUE 6
#define GET_PRC_VALUE 7
#define GET_COMPOUND 8
#define GET_COMPOUND_OPTS 9
#define GET_DBL_VALUE 10

#define OPTION_OK 0
#define OPTION_MALFORMED -1
#define OPTION_OVERFLOW -2
#define OPTION_ERROR -3
#define OPTION_BUFFER_OVERFLOW -4
#define OPTION_OUTOFRANGE -5

#define FIND_DLL_TABLE_ENTRY(name) vm->internalVMFunctions->findDllLoadInfo(vm->dllLoadTable, name)
#define FIND_ARG_IN_VMARGS(match, optionName, optionValue) vm->internalVMFunctions->findArgInVMArgs(vm->portLibrary, vm->vmArgsArray, match, optionName, optionValue, FALSE)
#define FIND_NEXT_ARG_IN_VMARGS(match, optionName, optionValue, lastArgIndex) ((lastArgIndex==0) ? -1 : vm->internalVMFunctions->findArgInVMArgs(vm->portLibrary, vm->vmArgsArray, (match | (lastArgIndex << STOP_AT_INDEX_SHIFT)), optionName, optionValue, FALSE))
#define FIND_AND_CONSUME_ARG(match, optionName, optionValue) vm->internalVMFunctions->findArgInVMArgs(vm->portLibrary, vm->vmArgsArray, match, optionName, optionValue, TRUE)

#define FIND_ARG_IN_VMARGS_FORWARD(match, optionName, optionValue) vm->internalVMFunctions->findArgInVMArgs(vm->portLibrary, vm->vmArgsArray, (match | SEARCH_FORWARD), optionName, optionValue, FALSE)
#define FIND_NEXT_ARG_IN_VMARGS_FORWARD(match, optionName, optionValue, lastArgIndex) vm->internalVMFunctions->findArgInVMArgs(vm->portLibrary, vm->vmArgsArray, ((match | ((lastArgIndex+1) << STOP_AT_INDEX_SHIFT)) | SEARCH_FORWARD), optionName, optionValue, FALSE)
#define FIND_AND_CONSUME_ARG_FORWARD(match, optionName, optionValue) vm->internalVMFunctions->findArgInVMArgs(vm->portLibrary, vm->vmArgsArray, (match | SEARCH_FORWARD), optionName, optionValue, TRUE)

/* REMOVE - FOR BACKWARDS COMPATIBILITY */
#define FIND_AND_CONSUME_ARG2(match, optionName, optionValue) vm->internalVMFunctions->findArgInVMArgs(vm->portLibrary, vm->vmArgsArray, match, optionName, optionValue, TRUE)
#define VMARGS_OPTION(element) vm->vmArgsArray->actualVMArgs->options[element].optionString
#define GET_OPTION_VALUE2(element, delimChar, resultPtr) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, element, GET_OPTION, resultPtr, 0, delimChar, 0, NULL)
/* *** */

#define GET_OPTION_VALUE(element, delimChar, resultPtr) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, element, GET_OPTION, (char**)resultPtr, 0, delimChar, 0, NULL)
#define COPY_OPTION_VALUE(element, delimChar, buffer, bufSize) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, element, GET_OPTION, buffer, bufSize, delimChar, 0, NULL)
#define GET_OPTION_VALUES(element, delimChar, sepChar, buffer, bufSize) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, element, GET_OPTIONS, buffer, bufSize, delimChar, sepChar, NULL)
#define GET_COMPOUND_VALUE(element, delimChar, buffer, bufSize) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, element, GET_COMPOUND, buffer, bufSize, delimChar, 0, NULL)
#define GET_COMPOUND_VALUES(element, delimChar, sepChar, buffer, bufSize) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, element, GET_COMPOUND_OPTS, buffer, bufSize, delimChar, sepChar, NULL)
#define GET_MEMORY_VALUE(element, optname, result) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, (element), GET_MEM_VALUE, (char **)&(optname), 0, 0, 0, &(result))
#define GET_INTEGER_VALUE(element, optname, result) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, (element), GET_INT_VALUE, (char **)&(optname), 0, 0, 0, &(result))
#define GET_PERCENT_VALUE(element, optname, result) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, (element), GET_PRC_VALUE,(char **) &(optname), 0, 0, 0, &(result))
#define GET_OPTION_OPTION(element, delimChar, delimChar2, resultPtr) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, (element), GET_OPTION_OPT, (resultPtr), 0, (delimChar), (delimChar2), NULL)
#define GET_DOUBLE_VALUE(element, optname, result) vm->internalVMFunctions->optionValueOperations(vm->portLibrary, vm->vmArgsArray, (element), GET_DBL_VALUE, (char **) &(optname), 0, 0, 0, &(result))
#define HAS_MAPPING(args, element) (args->j9Options[element].mapping!=NULL)
#define MAPPING_FLAGS(args, element) (args->j9Options[element].mapping->flags)
#define MAPPING_J9NAME(args, element) (args->j9Options[element].mapping->j9Name)
#define MAPPING_MAPNAME(args, element) (args->j9Options[element].mapping->mapName)
#define FROM_ENVVAR(args, element) (args->j9Options[element].fromEnvVar!=NULL)

#define CONSUME_ARG(j9vm_args, element) (j9vm_args->j9Options[element].flags |= ARG_CONSUMED)
#define IS_CONSUMED(j9vm_args, element) (j9vm_args->j9Options[element].flags & ARG_CONSUMED)
#define IS_CONSUMABLE(j9vm_args, element) (j9vm_args->j9Options[element].flags & CONSUMABLE_ARG)
#define REQUIRES_LIBRARY(j9vm_args, element) (j9vm_args->j9Options[element].flags & ARG_REQUIRES_LIBRARY)

#define COMPLETE_STAGE(flags, stage) (flags |= ((UDATA)1 << stage))
#define IS_STAGE_COMPLETED(flags, stage) (flags & ((UDATA)1 << stage))

#define J9_ADDRMODE_64 64

typedef struct CheckPostStageData {
	J9JavaVM* vm;
	IDATA stage;
	jint success;
} CheckPostStageData;

typedef struct LoadInitData {
	J9JavaVM* vm;
	UDATA flags;
} LoadInitData;

typedef struct RunDllMainData {
	J9JavaVM* vm;
	IDATA stage;
	void* reserved;
	UDATA filterFlags;
} RunDllMainData;

#define LOAD_STAGE -1
#define UNLOAD_STAGE -2
#define XRUN_INIT_STAGE -3
#define JVM_EXIT_STAGE -4
#define POST_INIT_STAGE -5

/* Do not have more than 32 stages, and remember to put an entry in
 * jvminit.c:getNameForStage()*/
enum INIT_STAGE {
	PORT_LIBRARY_GUARANTEED,
	ALL_DEFAULT_LIBRARIES_LOADED,
	ALL_LIBRARIES_LOADED,
	DLL_LOAD_TABLE_FINALIZED,
	VM_THREADING_INITIALIZED,
	HEAP_STRUCTURES_INITIALIZED,
	ALL_VM_ARGS_CONSUMED,
	BYTECODE_TABLE_SET,
	SYSTEM_CLASSLOADER_SET,
	DEBUG_SERVER_INITIALIZED,
	TRACE_ENGINE_INITIALIZED,
	JIT_INITIALIZED,
	AGENTS_STARTED,
	ABOUT_TO_BOOTSTRAP,
	JCL_INITIALIZED, 
	VM_INITIALIZATION_COMPLETE,
	INTERPRETER_SHUTDOWN,
	LIBRARIES_ONUNLOAD,
	HEAP_STRUCTURES_FREED,
	GC_SHUTDOWN_COMPLETE,
	/* this stage will only be invoked for the jcl shared library when it is being run remotely */ 
	OFFLOAD_JCL_PRECONFIGURE


};


#define VMOPT_EXIT "exit"
#define VMOPT_ABORT "abort"
#define VMOPT_BP_JXE "_jxe"
#define VMOPT_NEEDS_JCL "_needs_jcl"
#define VMOPT_J2SE_J9 "_j2se_j9"
#define VMOPT_XJCL_COLON "-Xjcl:"
#define VMOPT_XFUTURE "-Xfuture"
#define VMOPT_ALL "all"
#define VMOPT_XSIGQUITTOFILE "-XsigquitToFile"
#define VMOPT_XDEBUG "-Xdebug"
#define VMOPT_XNOAGENT "-Xnoagent"
#define VMOPT_XINCGC "-Xincgc"
#define VMOPT_XMIXED "-Xmixed"
#define VMOPT_XPROF "-Xprof"
#define VMOPT_XBATCH "-Xbatch"
#define VMOPT_PORT_LIBRARY "_port_library"
#define VMOPT_BFU_JAVA "_bfu_java"
#define VMOPT_VFPRINTF "vfprintf"
#define VMOPT_XTHR_COLON "-Xthr:"
#define VMOPT_XJNI_COLON "-Xjni:"
#define VMOPT_XINT "-Xint"
#define VMOPT_XJIT "-Xjit"
#define VMOPT_XJIT_COLON "-Xjit:"
#define VMOPT_XNOJIT "-Xnojit"
#define VMOPT_XVERIFY "-Xverify"
#define VMOPT_XVERIFY_COLON "-Xverify:"
#define VMOPT_XZERO "-Xzero"
#define VMOPT_XZERO_COLON "-Xzero:"
#define VMOPT_ZERO_NONE "none"
#define VMOPT_ZERO_J9ZIP "j9zip"
#define VMOPT_ZERO_SHAREZIP "sharezip"
#define VMOPT_ZERO_SHAREBOOTZIP "sharebootzip"
#define VMOPT_ZERO_SHARESTRING "sharestring"
#define VMOPT_ZERO_DESCRIBE "describe"
#define VMOPT_XNOAOT "-Xnoaot"
#define VMOPT_XAOT "-Xaot"
#define VMOPT_XAOT_COLON "-Xaot:"
#define VMOPT_XNOLINENUMBERS "-Xnolinenumbers"
#define VMOPT_XLINENUMBERS "-Xlinenumbers"
#define VMOPT_XRS "-Xrs"
#define VMOPT_XRUN "-Xrun"
#define VMOPT_XBOOTCLASSPATH_COLON "-Xbootclasspath:"
#define VMOPT_XBOOTCLASSPATH_A_COLON "-Xbootclasspath/a:"
#define VMOPT_XBOOTCLASSPATH_P_COLON "-Xbootclasspath/p:"
#define VMOPT_XSNW "-Xsnw"
#define VMOPT_XNOSIGCHAIN "-Xnosigchain"
#define VMOPT_XSIGCHAIN "-Xsigchain"
#define VMOPT_XNOSIGINT "-Xnosigint"
#define VMOPT_XALLOWCONTENDEDCLASSLOAD "-Xallowcontendedclassloads"
#define VMOPT_XFASTRESOLVE "-Xfastresolve"
#define VMOPT_XSHARECLASSES "-Xshareclasses"
#define VMOPT_XSHARECLASSES_COLON "-Xshareclasses:"
#define VMOPT_XSERVICE_EQUALS "-Xservice="
#define VMOPT_XISS "-Xiss"
#define VMOPT_XSSI "-Xssi"
#define VMOPT_XSS "-Xss"
#define VMOPT_XITS "-Xits"
#define VMOPT_XITSN "-Xitsn"
#define VMOPT_XITN "-Xitn"
#define VMOPT_XMSO "-Xmso"
#define VMOPT_XMSCL "-Xmscl"
#define VMOPT_XMXCL "-Xmxcl"
#define VMOPT_XMX "-Xmx"
#define VMOPT_XMS "-Xms"
#define VMOPT_XDUMP  "-Xdump"
#define VMOPT_XDUMP_NONE  "-Xdump:none"
#define VMOPT_XDUMP_DIRECTORY_EQUALS  "-Xdump:directory="
#define VMOPT_XDUMP_TOOL_OUTOFMEMORYERROR_EXEC_EQUALS "-Xdump:tool:events=systhrow,filter=java/lang/OutOfMemoryError,exec="
#define VMOPT_XARGENCODING "-Xargencoding"
#define VMOPT_XARGENCODINGCOLON "-Xargencoding:"
#define VMOPT_XARGENCODINGUTF8 "-Xargencoding:utf8"
#define VMOPT_XARGENCODINGLATIN "-Xargencoding:latin"
#define VMOPT_XNOARGSCONVERSION "-Xnoargsconversion"
#define VMOPT_XQUICKSTART "-Xquickstart"
#define VMOPT_XNOQUICKSTART "-Xnoquickstart"
#define VMOPT_XASCII_FILETAG "-Xascii_filetag"
#define VMOPT_XIPT "-Xipt"
#define VMOPT_EA "-ea"
#define VMOPT_ENABLE_ASSERTIONS "-enableassertions"
#define VMOPT_DA "-da"
#define VMOPT_DISABLE_ASSERTIONS "-disableassertions"
#define VMOPT_ESA "-esa"
#define VMOPT_ENABLE_SYSTEM_ASSERTIONS "-enablesystemassertions"
#define VMOPT_DSA "-dsa"
#define VMOPT_DISABLE_SYSTEM_ASSERTIONS "-disablesystemassertions"
#define VMOPT_AGENTPATH_COLON "-agentpath:"
#define VMOPT_AGENTLIB_COLON "-agentlib:"
#define VMOPT_XSCMX "-Xscmx"
#define VMOPT_XSCDMX "-Xscdmx"
#define VMOPT_XX "-XX:"
#define VMOPT_XJVM "-Xjvm:"
#define VMOPT_SERVER "-server"
#define VMOPT_CLIENT "-client"
#define VMOPT_XJ9 "-Xj9"
#define VMOPT_XSCMINAOT "-Xscminaot"
#define VMOPT_XSCMAXAOT "-Xscmaxaot"
#define VMOPT_XSCMINJITDATA "-Xscminjitdata"
#define VMOPT_XSCMAXJITDATA "-Xscmaxjitdata"
#define VMOPT_XXSHARED_CACHE_HARD_LIMIT_EQUALS "-XX:SharedCacheHardLimit="
#define VMOPT_XXLAZYCLASSVERIFICATION "-XXlazyclassverification"
#define VMOPT_XDFPBD "-Xdfpbd"
#define VMOPT_SHOWVERSION "-showversion"
#define VMOPT_INTERNALVERSION "-Xinternalversion"
#define VMOPT_XXALLOWVMSHUTDOWN "-XXallowvmshutdown:true"
#define VMOPT_XXDISABLEVMSHUTDOWN "-XXallowvmshutdown:false"
#define VMOPT_XXVM_IGNOREUNRECOGNIZED "-XXvm:ignoreUnrecognized"
#define VMOPT_XXIGNOREUNRECOGNIZEDVMOPTIONSENABLE "-XX:+IgnoreUnrecognizedVMOptions"
#define VMOPT_XXIGNOREUNRECOGNIZEDVMOPTIONSDISABLE "-XX:-IgnoreUnrecognizedVMOptions"
#define VMOPT_XXIGNOREUNRECOGNIZEDXXCOLONOPTIONSENABLE "-XX:+IgnoreUnrecognizedXXColonOptions"
#define VMOPT_XXIGNOREUNRECOGNIZEDXXCOLONOPTIONSDISABLE "-XX:-IgnoreUnrecognizedXXColonOptions"
#define VMOPT_X142BOOSTGCTHRPRIO "-X142BoostGCThrPrio"
#define VMOPT_XREALTIME "-Xrealtime"
#define VMOPT_XNORTSJ "-Xnortsj"
#define VMOPT_XXNOSTACKTRACEINTHROWABLE "-XX:-StackTraceInThrowable"
#define VMOPT_XXSTACKTRACEINTHROWABLE "-XX:+StackTraceInThrowable"
#define VMOPT_XXNOPAGEALIGNDIRECTMEMORY "-XX:-PageAlignDirectMemory"
#define VMOPT_XXPAGEALIGNDIRECTMEMORY   "-XX:+PageAlignDirectMemory"
#define VMOPT_XXVMLOCKCLASSLOADERENABLE "-XX:+VMLockClassLoader"
#define VMOPT_XXVMLOCKCLASSLOADERDISABLE "-XX:-VMLockClassLoader"
#define VMOPT_XXNOVERBOSEVERIFICATION "-XX:-VerboseVerification"
#define VMOPT_XXVERBOSEVERIFICATION "-XX:+VerboseVerification"
#define VMOPT_XXNOVERIFYERRORDETAILS "-XX:-VerifyErrorDetails"
#define VMOPT_XXVERIFYERRORDETAILS "-XX:+VerifyErrorDetails"
#define VMOPT_XXNODEBUGINTERPRETER "-XX:-DebugInterpreter"
#define VMOPT_XXDEBUGINTERPRETER "-XX:+DebugInterpreter"
#define VMOPT_XXNOHANDLESIGXFSZ "-XX:-HandleSIGXFSZ"
#define VMOPT_XXHANDLESIGXFSZ "-XX:+HandleSIGXFSZ"
#define VMOPT_XXNOHANDLESIGABRT "-XX:-HandleSIGABRT"
#define VMOPT_XXHANDLESIGABRT "-XX:+HandleSIGABRT"
#define VMOPT_XXHEAPDUMPONOOM "-XX:+HeapDumpOnOutOfMemoryError"
#define VMOPT_XXNOHEAPDUMPONOOM "-XX:-HeapDumpOnOutOfMemoryError"
#define VMOPT_XDUMP_EXIT_OUTOFMEMORYERROR "-Xdump:exit:events=systhrow,filter=java/lang/OutOfMemoryError"
#define VMOPT_XDUMP_EXIT_OUTOFMEMORYERROR_DISABLE "-Xdump:exit:none:events=systhrow,filter=java/lang/OutOfMemoryError"

#define VMOPT_XSOFTREFTHRESHOLD "-XSoftRefThreshold"
#define VMOPT_XAGGRESSIVE "-Xaggressive"
#define VMOPT_XXMAXDIRECTMEMORYSIZEEQUALS "-XX:MaxDirectMemorySize="
#define VMOPT_AGENTLIB_HEALTHCENTER "-agentlib:healthcenter"
#define VMOPT_AGENTLIB_HEALTHCENTER_EQUALS "-agentlib:healthcenter="
#define VMOPT_AGENTLIB_DGCOLLECTOR "-agentlib:dgcollector"
#define VMOPT_AGENTLIB_DGCOLLECTOR_EQUALS "-agentlib:dgcollector="
#define VMOPT_XLOCKWORD "-Xlockword:"
#define VMOPT_XXGLOBALLOCKRESERVATION "-XX:+GlobalLockReservation"
#define VMOPT_XXGLOBALLOCKRESERVATIONCOLON "-XX:+GlobalLockReservation:"
#define VMOPT_XXNOGLOBALLOCKRESERVATION "-XX:-GlobalLockReservation"
#define VMOPT_OBJECT_MONITOR_CACHE_BITS_EQUALS "-XmonitorLookupCacheBits="
#define VMOPT_XXALWAYSCOPYJNICRITICAL "-XX:+AlwaysCopyJNICritical"
#define VMOPT_XXNOALWAYSCOPYJNICRITICAL "-XX:-AlwaysCopyJNICritical"
#define VMOPT_XXALWAYSUSEJNICRITICAL "-XX:+AlwaysUseJNICritical"
#define VMOPT_XXNOALWAYSUSEJNICRITICAL "-XX:-AlwaysUseJNICritical"
#define VMOPT_XXDEBUGVMACCESS "-XX:+DebugVMAccess"
#define VMOPT_XXNODEBUGVMACCESS "-XX:-DebugVMAccess"
#define VMOPT_XXMHALLOWI2J	"-XX:+MHAllowI2J"
#define VMOPT_XXNOMHALLOWI2J	"-XX:-MHAllowI2J"
#define VMOPT_XXMHDEBUGTARGETS "-XX:+MHDebugTargets"
#define VMOPT_XXNOMHDEBUGTARGETS "-XX:-MHDebugTargets"
#define VMOPT_XXMHCOMPILECOUNT_EQUALS "-XX:MHCompileCount="
#define VMOPT_XHEAPONLYRTSJ "-Xheaponlyrtsj"
#define VMOPT_XSOFTMX "-Xsoftmx"
#define VMOPT_XXNODISCLAIMVIRTUALMEMORY "-XX:-DisclaimVirtualMemory"
#define VMOPT_XXDISCLAIMVIRTUALMEMORY "-XX:+DisclaimVirtualMemory"
#define VMOPT_OPT_XXNOINTERLEAVEMEMORY "-XX:-InterleaveMemory"
#define VMOPT_OPT_XXINTERLEAVEMEMORY "-XX:+InterleaveMemory"
#define VMOPT_ROMMETHODSORTTHRESHOLD_EQUALS "-XX:ROMMethodSortThreshold="
#define VMOPT_VALUEFLATTENINGTHRESHOLD_EQUALS "-XX:ValueTypeFlatteningThreshold="
#define VMOPT_VTARRAYFLATTENING_EQUALS "-XX:+EnableArrayFlattening"
#define VMOPT_VTDISABLEARRAYFLATTENING_EQUALS "-XX:-EnableArrayFlattening"
#define VMOPT_XXREDUCECPUMONITOROVERHEAD "-XX:+ReduceCPUMonitorOverhead"
#define VMOPT_XXNOREDUCECPUMONITOROVERHEAD "-XX:-ReduceCPUMonitorOverhead"
#define VMOPT_XXENABLECPUMONITOR "-XX:+EnableCPUMonitor"
#define VMOPT_XXDISABLECPUMONITOR "-XX:-EnableCPUMonitor"
#define VMOPT_XXENABLEOSRSAFEPOINT "-XX:+OSRSafePoint"
#define VMOPT_XXDISABLEOSRSAFEPOINT "-XX:-OSRSafePoint"
#define VMOPT_XXENABLEOSRSAFEPOINTFV "-XX:+OSRSafePointFV"
#define VMOPT_XXDISABLEOSRSAFEPOINTFV "-XX:-OSRSafePointFV"
#define VMOPT_XXENABLEJITWATCH "-XX:+JITInlineWatches"
#define VMOPT_XXDISABLEJITWATCH "-XX:-JITInlineWatches"
#define VMOPT_XXENABLEALWAYSSPLITBYTECODES "-XX:+AlwaysSplitBytecodes"
#define VMOPT_XXDISABLEALWAYSSPLITBYTECODES "-XX:-AlwaysSplitBytecodes"
#define VMOPT_XXENABLEPOSITIVEHASHCODE "-XX:+PositiveIdentityHash"
#define VMOPT_XXDISABLEPOSITIVEHASHCODE "-XX:-PositiveIdentityHash"
#define VMOPT_XXENABLEORIGINALJDK8HEAPSIZECOMPATIBILITY "-XX:+OriginalJDK8HeapSizeCompatibilityMode"
#define VMOPT_XXDISABLEORIGINALJDK8HEAPSIZECOMPATIBILITY "-XX:-OriginalJDK8HeapSizeCompatibilityMode"
#define VMOPT_XXDISABLELEGACYMANGLING "-XX:-UseLegacyJNINameEscaping"
#define VMOPT_XXENABLELEGACYMANGLING "-XX:+UseLegacyJNINameEscaping"

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
#define VMOPT_XXENABLEVALHALLA "-XX:+EnableValhalla"
#define VMOPT_XXDISABLEVALHALLA "-XX:-EnableValhalla"
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

/* Option to turn on exception on synchronization on instances of value-based classes */
#define VMOPT_XXDIAGNOSE_SYNC_ON_VALUEBASED_CLASSES_EQUALS1 "-XX:DiagnoseSyncOnValueBasedClasses=1"
/* Option to turn on warning on synchronization on instances of value-based classes */
#define VMOPT_XXDIAGNOSE_SYNC_ON_VALUEBASED_CLASSES_EQUALS2 "-XX:DiagnoseSyncOnValueBasedClasses=2"

#define VMOPT_XX_NOSUBALLOC32BITMEM "-XXnosuballoc32bitmem"

#define VMOPT_XUSE_CEEHDLR "-XCEEHDLR"
#define VMOPT_XUSE_CEEHDLR_PERCOLATE "-Xsignal:userConditionHandler=percolate"
#define VMOPT_SIGNAL_POSIX_SIGNAL_HANDLER_COOPERATIVE_SHUTDOWN "-Xsignal:posixSignalHandler=cooperativeShutdown"

#define VMOPT_XPRELOADUSER32 "-Xpreloaduser32"
#define VMOPT_XNOPRELOADUSER32 "-Xnopreloaduser32"
#define VMOPT_XPROTECTCONTIGUOUS "-Xprotectcontiguous"
#define VMOPT_XNOPROTECTCONTIGUOUS "-Xnoprotectcontiguous"
#define VMOPT_XXDISCLAIMJITSCRATCH		"-XX:+DisclaimJitScratch"
#define VMOPT_XXNODISCLAIMJITSCRATCH	"-XX:-DisclaimJitScratch"

#define VMOPT_TUNE_VIRTUALIZED "-Xtune:virtualized"

#define VMOPT_XXCOMPACTSTRINGS "-XX:+CompactStrings"
#define VMOPT_XXNOCOMPACTSTRINGS "-XX:-CompactStrings"

#define VMOPT_XXSHARECLASSESENABLEBCI "-XX:ShareClassesEnableBCI"
#define VMOPT_XXSHARECLASSESDISABLEBCI "-XX:ShareClassesDisableBCI"

#define VMOPT_XXPORTABLESHAREDCACHE "-XX:+PortableSharedCache"
#define VMOPT_XXNOPORTABLESHAREDCACHE "-XX:-PortableSharedCache"

#define VMOPT_XXENABLESHAREANONYMOUSCLASSES "-XX:+ShareAnonymousClasses"
#define VMOPT_XXDISABLESHAREANONYMOUSCLASSES "-XX:-ShareAnonymousClasses"

#define VMOPT_XXENABLESHAREUNSAFECLASSES "-XX:+ShareUnsafeClasses"
#define VMOPT_XXDISABLESHAREUNSAFECLASSES "-XX:-ShareUnsafeClasses"

#define VMOPT_XXFORCECLASSFILEASINTERMEDIATEDATA "-XX:ForceClassfileAsIntermediateData"
#define VMOPT_XXRECREATECLASSFILEONLOAD "-XX:RecreateClassfileOnload"

#define VMOPT_XXSETHWPREFETCH_NONE "-XXsetHWPrefetch:none"
#define VMOPT_XXSETHWPREFETCH_OS_DEFAULT "-XXsetHWPrefetch:os-default"
#define VMOPT_XXSETHWPREFETCH_EQUALS "-XXsetHWPrefetch="

#define VMOPT_XXLAZYSYMBOLRESOLUTION "-XX:+LazySymbolResolution"
#define VMOPT_XXNOLAZYSYMBOLRESOLUTION "-XX:-LazySymbolResolution"

#define VMOPT_XXFASTCLASSHASHTABLE "-XX:+FastClassHashTable"
#define VMOPT_XXNOFASTCLASSHASHTABLE "-XX:-FastClassHashTable"

#define VMOPT_XXJITDIRECTORY_EQUALS "-XXjitdirectory="

#define VMOPT_XXDECOMP_COLON "-XXdecomp:"

#define VMOPT_XLP_CODECACHE "-Xlp:codecache:"
#define VMOPT_XTLHPREFETCH "-XtlhPrefetch"

#define VMOPT_XXALLOWNONVIRTUALCALLS "-XX:+AllowNonVirtualCalls"
#define VMOPT_XXDONTALLOWNONVIRTUALCALLS "-XX:-AllowNonVirtualCalls"

#define VMOPT_XXRESTRICTCONTENDED "-XX:+RestrictContended"
#define VMOPT_XXNORESTRICTCONTENDED "-XX:-RestrictContended"
#define VMOPT_XXCONTENDEDFIELDS "-XX:+ContendedFields"
#define VMOPT_XXNOCONTENDEDFIELDS "-XX:-ContendedFields"

#define VMOPT_XXRESTRICTIFA "-XX:+RestrictIFA"
#define VMOPT_XXNORESTRICTIFA "-XX:-RestrictIFA"

#define VMOPT_XXUSEJ9JIMAGEREADER "-XX:+UseJ9JImageReader"
#define VMOPT_XXNOUSEJ9JIMAGEREADER "-XX:-UseJ9JImageReader"

#define VMOPT_XXENABLEHCR "-XX:+EnableHCR"
#define VMOPT_XXNOENABLEHCR "-XX:-EnableHCR"

#define VMOPT_XXUSECONTAINERSUPPORT "-XX:+UseContainerSupport"
#define VMOPT_XXNOUSECONTAINERSUPPORT "-XX:-UseContainerSupport"

#define VMOPT_PORT_VMEM_HUGE_PAGES_MMAP_ENABLED "-XX:+HugePagesWithMmap"
#define VMOPT_PORT_VMEM_HUGE_PAGES_MMAP_DISABLED "-XX:-HugePagesWithMmap"

#define VMOPT_XXREADIPINFOFORRAS "-XX:+ReadIPInfoForRAS"
#define VMOPT_XXNOREADIPINFOFORRAS "-XX:-ReadIPInfoForRAS"
#define VMOPT_ENABLE_PREVIEW "--enable-preview"

#define VMOPT_XXNLSMESSAGES "-XX:+NLSMessages"
#define VMOPT_XXNONLSMESSAGES  "-XX:-NLSMessages"

#define VMOPT_XCOMPRESSEDREFS "-Xcompressedrefs"
#define VMOPT_XNOCOMPRESSEDREFS "-Xnocompressedrefs"

#define VMOPT_XXTRANSPARENT_HUGEPAGE "-XX:+TransparentHugePage"
#define VMOPT_XXNOTRANSPARENT_HUGEPAGE "-XX:-TransparentHugePage"

#define VMOPT_XXDEEP_SCAN "-XX:+GCDeepStructurePriorityScan"
#define VMOPT_XXNODEEP_SCAN "-XX:-GCDeepStructurePriorityScan"

#define VMOPT_XXFORCE_FULL_HEAP_ADDRESS_RANGE_SEARCH "-XX:+ForceFullHeapAddressRangeSearch"
#define VMOPT_XXNOFORCE_FULL_HEAP_ADDRESS_RANGE_SEARCH "-XX:-ForceFullHeapAddressRangeSearch"

#define VMOPT_XXCLASSRELATIONSHIPVERIFIER "-XX:+ClassRelationshipVerifier"
#define VMOPT_XXNOCLASSRELATIONSHIPVERIFIER "-XX:-ClassRelationshipVerifier"

#define MAPOPT_AGENTLIB_JDWP_EQUALS "-agentlib:jdwp="

#define MAPOPT_XCOMP "-Xcomp"
#define MAPOPT_XDISABLEJAVADUMP "-Xdisablejavadump"
#define MAPOPT_XDUMP_JAVA_NONE "-Xdump:java:none"
#define MAPOPT_XJIT_COUNT0 "-Xjit:count=0"
#define MAPOPT_XJIT_COUNT_EQUALS "-Xjit:count="
#define MAPOPT_XVERIFY_REMOTE "-Xverify:remote"
#define MAPOPT_XSIGCATCH "-Xsigcatch"
#define MAPOPT_XNOSIGCATCH "-Xnosigcatch"
#define MAPOPT_XINITACSH "-Xinitacsh"
#define MAPOPT_XINITSH "-Xinitsh"
#define MAPOPT_XINITTH "-Xinitth"
#define MAPOPT_XTHR_TW_EQUALS "-Xthr:tw="
#define MAPOPT_VERBOSE_XGCCON "-verbose:Xgccon"
#define MAPOPT_VERBOSE_GC "-verbose:gc"
#define MAPOPT_VERBOSEGC "-verbosegc"
#define MAPOPT_XLOGGC "-Xloggc:"
#define MAPOPT_XVERBOSEGCLOG "-Xverbosegclog:"
#define MAPOPT_JAVAAGENT_COLON "-javaagent:"
#define MAPOPT_AGENTLIB_INSTRUMENT_EQUALS "-agentlib:instrument="
#define MAPOPT_AGENTLIB_HYINSTRUMENT_EQUALS "-agentlib:hyinstrument="
#define MAPOPT_XK "-Xk"
#define MAPOPT_XP "-Xp"
#define MAPOPT_XHEALTHCENTER "-Xhealthcenter"
#define MAPOPT_XHEALTHCENTER_COLON "-Xhealthcenter:"
#define MAPOPT_XSOFTREFTHRESHOLD "-Xsoftrefthreshold"
#define MAPOPT_XXJITDIRECTORY "-XXjitdirectory="
#define MAPOPT_XSHARE_ON "-Xshare:on"
#define MAPOPT_XSHARE_OFF "-Xshare:off"
#define MAPOPT_XSHARE_AUTO "-Xshare:auto"
#define MAPOPT_XSHARECLASSES_UTILITIES "-Xshareclasses:utilities"
#define MAPOPT_XSHARECLASSES_NONFATAL "-Xshareclasses:nonfatal"
#define MAPOPT_XXDISABLEEXPLICITGC "-XX:+DisableExplicitGC"
#define MAPOPT_XXENABLEEXPLICITGC "-XX:-DisableExplicitGC"
#define MAPOPT_XXHEAPDUMPPATH_EQUALS "-XX:HeapDumpPath="
#define MAPOPT_XXMAXHEAPSIZE_EQUALS "-XX:MaxHeapSize="
#define MAPOPT_XXINITIALHEAPSIZE_EQUALS "-XX:InitialHeapSize="
#define MAPOPT_XXONOUTOFMEMORYERROR_EQUALS "-XX:OnOutOfMemoryError="
#define MAPOPT_XXENABLEEXITONOUTOFMEMORYERROR "-XX:+ExitOnOutOfMemoryError"
#define MAPOPT_XXDISABLEEXITONOUTOFMEMORYERROR "-XX:-ExitOnOutOfMemoryError"
#define MAPOPT_XXPARALLELCMSTHREADS_EQUALS "-XX:ParallelCMSThreads="
#define MAPOPT_XXCONCGCTHREADS_EQUALS "-XX:ConcGCThreads="
#define MAPOPT_XXPARALLELGCTHREADS_EQUALS "-XX:ParallelGCThreads="
#define MAPOPT_XXPARALLELGCMAXTHREADS_EQUALS "-XX:ParallelGCMaxThreads="

#define VMOPT_XXACTIVEPROCESSORCOUNT_EQUALS "-XX:ActiveProcessorCount="

#define VMOPT_XXDUMPLOADEDCLASSLIST "-XX:DumpLoadedClassList"

#define VMOPT_XXIDLETUNINGMINIDLEWATITIME_EQUALS "-XX:IdleTuningMinIdleWaitTime="
#define VMOPT_XXIDLETUNINGMINFREEHEAPONIDLE_EQUALS "-XX:IdleTuningMinFreeHeapOnIdle="
#define VMOPT_XXIDLETUNINGGCONIDLEDISABLE "-XX:-IdleTuningGcOnIdle"
#define VMOPT_XXIDLETUNINGGCONIDLEENABLE "-XX:+IdleTuningGcOnIdle"
#define VMOPT_XXIDLETUNINGCOMPACTONIDLEDISABLE "-XX:-IdleTuningCompactOnIdle"
#define VMOPT_XXIDLETUNINGCOMPACTONIDLEENABLE "-XX:+IdleTuningCompactOnIdle"
#define VMOPT_XXIDLETUNINGIGNOREUNRECOGNIZEDOPTIONSDISABLE "-XX:-IdleTuningIgnoreUnrecognizedOptions"
#define VMOPT_XXIDLETUNINGIGNOREUNRECOGNIZEDOPTIONSENABLE "-XX:+IdleTuningIgnoreUnrecognizedOptions"
#define VMOPT_XCONCURRENTBACKGROUND "-Xconcurrentbackground"
#define VMOPT_XGCTHREADS "-Xgcthreads"
#define VMOPT_XGCMAXTHREADS "-Xgcmaxthreads"

#define VMOPT_XXSHOW_EXTENDED_NPE_MESSAGE "-XX:+ShowCodeDetailsInExceptionMessages"
#define VMOPT_XXNOSHOW_EXTENDED_NPE_MESSAGE "-XX:-ShowCodeDetailsInExceptionMessages"

#define VMOPT_XXPRINTFLAGSFINALENABLE "-XX:+PrintFlagsFinal"
#define VMOPT_XXPRINTFLAGSFINALDISABLE "-XX:-PrintFlagsFinal"

#define VMOPT_XXLEGACYXLOGOPTION "-XX:+LegacyXlogOption"
#define VMOPT_XXNOLEGACYXLOGOPTION "-XX:-LegacyXlogOption"
#define MAPOPT_XLOG_OPT "-Xlog"
#define MAPOPT_XLOG_OPT_COLON "-Xlog:"
#define VMOPT_XSYSLOG_OPT "-Xsyslog"
#define MAPOPT_XSYSLOG_OPT_COLON "-Xsyslog:"

/* Modularity command line options */
#define VMOPT_MODULE_UPGRADE_PATH "--upgrade-module-path"
#define VMOPT_MODULE_PATH "--module-path"
#define VMOPT_ADD_MODULES "--add-modules"
#define VMOPT_LIMIT_MODULES "--limit-modules"
#define VMOPT_ADD_READS "--add-reads"
#define VMOPT_ADD_EXPORTS "--add-exports"
#define VMOPT_ADD_OPENS "--add-opens"
#define VMOPT_PATCH_MODULE "--patch-module"
#define VMOPT_ILLEGAL_ACCESS "--illegal-access="

#define ENVVAR_IBM_MIXED_MODE_THRESHOLD "IBM_MIXED_MODE_THRESHOLD"
#define ENVVAR_JAVA_COMPILER "JAVA_COMPILER"
#define ENVVAR_JAVA_OPTIONS "JAVA_OPTIONS"
#define ENVVAR_OPENJ9_JAVA_OPTIONS "OPENJ9_JAVA_OPTIONS"
#define ENVVAR_IBM_JAVA_OPTIONS "IBM_JAVA_OPTIONS"
#define ENVVAR_JAVA_TOOL_OPTIONS "JAVA_TOOL_OPTIONS"
#define ENVVAR_IBM_NOSIGHANDLER "IBM_NOSIGHANDLER"
#define ENVVAR_JAVA_THREAD_MODEL "JAVA_THREAD_MODEL"
#define ENVVAR_IBM_JAVA_ENABLE_ASCII_FILETAG "IBM_JAVA_ENABLE_ASCII_FILETAG"
#define ENVVAR_IBM_JAVA_JITLIB "IBM_JAVA_JITLIB"

#define SYSPROP_DJAVA_COMPILER_EQUALS "-Djava.compiler="

#define BOOT_PATH_SYS_PROP "sun.boot.class.path"
#define BOOT_CLASS_PATH_APPEND_PROP "jdk.boot.class.path.append"

/* System properties required for modularity for B148+ level */
#define SYSPROP_JDK_MODULE_ADDOPENS "jdk.module.addopens."

/* System properties required for modularity for B132+ level */
#define SYSPROP_JDK_MODULE_UPGRADE_PATH "jdk.module.upgrade.path"
#define SYSPROP_JDK_MODULE_PATH "jdk.module.path"
#define SYSPROP_JDK_MODULE_ADDMODS "jdk.module.addmods."
#define SYSPROP_JDK_MODULE_LIMITMODS "jdk.module.limitmods"
#define SYSPROP_JDK_MODULE_ADDREADS "jdk.module.addreads."
#define SYSPROP_JDK_MODULE_ADDEXPORTS "jdk.module.addexports."
#define SYSPROP_JDK_MODULE_PATCH "jdk.module.patch."
#define SYSPROP_JDK_MODULE_ILLEGALACCESS "jdk.module.illegalAccess"
#define JAVA_BASE_MODULE "java.base"
#define JAVA_MODULE_VERSION "9"

#define SYSPROP_COM_SUN_MANAGEMENT "-Dcom.sun.management."

#ifdef J9VM_INTERP_VERBOSE

#define JVMINIT_VERBOSE_INIT_TRACE(flags, txt) do { \
	if (PORTLIB && (flags & VERBOSE_INIT)) { \
		j9tty_printf(PORTLIB, txt); \
	} \
} while (0);
#define JVMINIT_VERBOSE_INIT_TRACE1(flags, txt, p1) do { \
	if (PORTLIB && (flags & VERBOSE_INIT)) { \
		j9tty_printf(PORTLIB, txt, p1); \
	} \
} while (0);
#define JVMINIT_VERBOSE_INIT_TRACE2(flags, txt, p1, p2) do { \
	if (PORTLIB && (flags & VERBOSE_INIT)) { \
		j9tty_printf(PORTLIB, txt, p1, p2); \
	} \
} while (0);
#define JVMINIT_VERBOSE_INIT_VM_TRACE(vm, txt) do { \
	PORT_ACCESS_FROM_JAVAVM(vm); \
	JVMINIT_VERBOSE_INIT_TRACE(vm->verboseLevel, txt); \
} while (0);
#define JVMINIT_VERBOSE_INIT_VM_TRACE1(vm, txt, p1) do { \
	PORT_ACCESS_FROM_JAVAVM(vm); \
	JVMINIT_VERBOSE_INIT_TRACE1(vm->verboseLevel, txt, p1); \
} while (0);
#define JVMINIT_VERBOSE_INIT_VM_TRACE2(vm, txt, p1, p2) do { \
	PORT_ACCESS_FROM_JAVAVM(vm); \
	JVMINIT_VERBOSE_INIT_TRACE2(vm->verboseLevel, txt, p1, p2); \
} while (0);

#else
#define JVMINIT_VERBOSE_INIT_TRACE(flags, txt)
#define JVMINIT_VERBOSE_INIT_TRACE1(flags, txt, p1)
#define JVMINIT_VERBOSE_INIT_TRACE2(flags, txt, p1, p2)
#define JVMINIT_VERBOSE_INIT_VM_TRACE(vm, txt)
#define JVMINIT_VERBOSE_INIT_VM_TRACE1(vm, txt, p1)
#define JVMINIT_VERBOSE_INIT_VM_TRACE2(vm, txt, p1, p2)
#endif

#ifndef JVMINIT_VERBOSE_INIT_TRACE_WORKING_SET
#define JVMINIT_VERBOSE_INIT_TRACE_WORKING_SET(vm)
#endif


#ifdef __cplusplus
}
#endif

#endif /* JVMINIT_H */

