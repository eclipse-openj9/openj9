/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef dbgext_internal_h
#define dbgext_internal_h

/**
* @file dbgext_internal.h
* @brief Internal prototypes used within the DBGEXT module.
*
* This file contains implementation-private function prototypes and
* type definitions for the DBGEXT module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "dbgext_api.h"
#include "jni.h"
#if defined(J9VM_OPT_SHARED_CLASSES)
#include "shcdatatypes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* ---------------- dbgpool.c ---------------- */

/**
* @typedef
* @struct
*/
typedef struct J9PoolWalkData {
	UDATA counter;
	J9Pool *head;
	J9PoolPuddle *puddle;
} J9PoolWalkData;

/**
* @brief
* @param *aPool
* @param (*aFunction)(void*, J9PoolWalkData*)
* @param *userData
* @return void
*/
void 
walkAndCall(J9Pool *aPool, void (*aFunction)(void*,J9PoolWalkData*), J9PoolWalkData *userData);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_walkj9pool(const char *args);


/**
* @brief
* @param addr
* @return J9Pool *
*/
J9Pool *
dbgMapPool(UDATA addr);


/**
* @brief
* @param *pool
* @return void
*/
void 
dbgUnmapPool(J9Pool *pool);


/**
* @brief
* @param *addr
* @param *data
* @return void
*/
void 
defaultJ9PoolWalkPFunc(void *addr, J9PoolWalkData *data);


/**
* @brief
* @param addr
* @param (*pFunc)(void*
* @param J9PoolWalkData*)
* @param J9PoolWalkData*data
* @return void
*/
void 
walkJ9Pool(UDATA addr, void (*pFunc)(void*,J9PoolWalkData*), J9PoolWalkData*data);


/* ---------------- j9dbgext.c ---------------- */

/**
* @brief
* @param stringObject
* @param athread
* @return char *
*/
char *
copyStringIntoUTF8(J9VMThread* athread, j9object_t stringObject);


/**
* @brief
* @param state
* @param vm
* @param flags
* @return J9ClassLoader*
*/
J9ClassLoader*
dbgAllClassLoadersStartDo(J9ClassLoaderWalkState* state, J9JavaVM* vm, UDATA flags);


/**
* @brief
* @param state
* @return J9ClassLoader*
*/
J9ClassLoader*
dbgAllClassLoadersNextDo(J9ClassLoaderWalkState* state);


/**
* @brief
* @param state
* @return J9Class*
*/
J9Class * 
dbgAllClassesNextDo(J9ClassWalkState* state);


/**
* @brief
* @param state
* @param vmPtr
* @param classLoader
* @return J9Class*
*/
J9Class * 
dbgAllClassesStartDo(J9ClassWalkState* state, J9JavaVM* vmPtr, J9ClassLoader* classLoader);


/**
* @brief
* @param state
* @return J9ROMClass*
*/
J9ROMClass*
dbgAllROMClassesNextDo(J9ClassWalkState* state);


/**
* @brief
* @param state
* @param vmPtr
* @param classLoader
* @return J9ROMClass*
*/
J9ROMClass*
dbgAllROMClassesStartDo(J9ClassWalkState* state, J9JavaVM* vmPtr, J9ClassLoader* classLoader);


/**
* @brief
* @param romMethod
* @return J9ROMMethod*
*/
J9ROMMethod*
dbgNextROMMethod(J9ROMMethod *romMethod);


#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
/**
* @brief
* @param remoteTree
* @param searchValue
* @return struct J9JITHashTable *
*/
struct J9JITHashTable * 
dbgAvlSearch(J9AVLTree * remoteTree, UDATA searchValue);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


/**
* @brief
* @param *remoteList
* @return void
*/
void 
dbgDumpSegmentList(void *remoteList);


/**
* @brief
* @param format
* @param ...
* @return void
*/
void 
dbgError(const char* format, ...);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_bytecodes(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_dumpallclassloaders(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_dumpallsegments(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_dumpjitmethodstore(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_dumpromclass(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_dumpallromclasslinear(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_dumpromclasslinear(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void
dbgext_queryromclass(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void
dbgext_dumpsegmentsinlist(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void dbgext_findmethodfrompc(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_findstackvalue(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_findvm(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_j9help(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_j9reg(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_jextract(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_jitmetadatafrompc(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_jitstack(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_jitstackslots(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_localmap(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void
dbgext_setvm(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_stack(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_stackmap(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_stackslots(const char *args);


/**
* @brief
* @param *args
* @return void
*/ 
void 
dbgext_slots(const char *args);
 
/**
* @brief
* @param *args
* @return void
*/
void
dbgext_module(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_threads(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void
dbgext_findcallsite(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void
dbgext_findfreedcallsite(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void
dbgext_findallcallsites(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void
dbgext_findfreedcallsites(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void
dbgext_printallcallsites(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void
dbgext_printfreedcallsites(const char *args);


/**
* @brief
* @param *args
* @return void
*/
void
dbgext_findheader(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_findpattern(const char *args);

/**
* @brief
* @param *args
* @return void
*/

void 
dbgext_fj9object(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_j9objecttofj9object(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_fj9objecttoj9object(const char *args);

#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
/**
* @brief
* @param *remoteTree
* @param searchValue
* @return struct J9JITExceptionTable *
*/
struct J9JITExceptionTable * 
dbgFindJITMetaData(J9AVLTree *remoteTree, UDATA searchValue);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


/**
* @brief
* @param pattern
* @param patternLength
* @param patternAlignment
* @param startSearchFrom
* @param bytesToSearch
* @param bytesSearched
* @return void*
*/
void* 
dbgFindPatternInRange(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA bytesToSearch, UDATA* bytesSearched);


/**
* @brief
* @param addr
* @return void
*/
void 
dbgFree(void * addr);


/**
* @brief
* @param void
* @return void
*/
void 
dbgFreeAll(void);


/**
* @brief
* @param *clazzAddr
* @return char *
*/
char *
dbgGetClassNameFromROMClass(void *clazzAddr);


/**
* @brief
* @param *clazzAddr
* @return char *
*/
char *
dbgGetClassNameFromClass(void *clazzAddr);


/**
* @brief
* @param *addr
* @return char *
*/
char *
dbgGetClassNameFromObject(void *addr);


/**
* @brief
* @param addr
* @return UDATA
*/
UDATA 
dbgGetLocalBlockRelocated(void * addr);


/**
* @brief
* @param vmPtr
* @param pc
* @return J9Method *
*/
J9Method * 
dbgGetMethodFromPC(void* vmPtr, const U_8* pc);


/**
* @brief
* @param *currentThread
* @param *pc
* @param *constantPool
* @return J9Method *
*/
J9Method * 
dbgGetMethodFromPCAndConstantPool(J9VMThread *currentThread, const U_8 *pc, const J9ConstantPool *constantPool);


/**
* @brief
* @param *ramMethodAddr
* @return char *
*/
char *
dbgGetNameFromRAMMethod(void *ramMethodAddr);


/**
* @brief
* @param *remoteROMMethod
* @param *ramClassName
* @return char *
*/
char *
dbgGetNameFromROMMethod(J9ROMMethod *remoteROMMethod, char *ramClassName);


/**
* @brief
* @param void
* @return J9PortLibrary *
*/
J9PortLibrary * 
dbgGetPortLibrary(void);


/**
* @brief
* @param *ptr
* @return char *
*/
char *
dbgGetStringFromUTF(void *ptr);


/**
* @brief
* @param void
* @return void*
*/
void* dbgGetThreadLibrary(void);


/**
* @brief
* @param addr
* @return void *
*/
void * 
dbgLocalToTarget(void * addr);

j9object_t
mapFJ9objectToJ9object(fj9object_t fieldObject);

/**
* @brief
* @param size
* @param *originalAddress
* @return void *
*/
void *
dbgMalloc(UDATA size, void *originalAddress);


/**
* @brief
* @param size
* @param remoteAddress
* @return void *
*/
void *
dbgMallocAndRead(UDATA size, void * remoteAddress);


/**
* @brief
* @param address
* @return UDATA
*/
UDATA 
dbgObjectSizeInBytes(j9object_t address);


/**
* @brief
* @param *address
* @param **cachePtr
* @return UDATA
*/
UDATA 
dbgObjectSizeInBytesCached(j9object_t address, J9HashTable **cachePtr);


/**
* @brief
* @param originalArgs
* @param argValues
* @param maxArgs
* @return UDATA
*/
UDATA 
dbgParseArgs(const char * originalArgs, UDATA * argValues, UDATA maxArgs);


/**
* @brief
* @param localThread
* @return UDATA
*/
UDATA 
dbgPrepareThreadForStackWalk(J9VMThread * localThread);


/**
* @brief
* @param message
* @param ...
* @return void
*/
void 
dbgPrint (const char* message, ...);


/**
* @brief
* @param remoteAddress
* @return U_8
*/
U_8 
dbgReadByte(U_8 * remoteAddress);


/**
* @brief
* @param remoteClass
* @return J9Class *
*/
J9Class * 
dbgReadClass(J9Class * remoteClass);


/**
* @brief
* @param remoteObject
* @return J9Object *
*/ 
J9Object *
dbgReadObject(J9Object * remoteObject);


/**
* @brief
* @param remoteCP
* @return J9ConstantPool *
*/
J9ConstantPool * 
dbgReadCP(J9ConstantPool * remoteCP);


/**
* @brief
* @param remoteVM
* @return J9JavaVM *
*/
J9JavaVM * 
dbgReadJavaVM(J9JavaVM * remoteVM);


struct J9JITHashTable;
#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
/**
* @brief
* @param remoteTable
* @return struct J9JITHashTable *
*/
struct J9JITHashTable * 
dbgReadJITHashTable(struct J9JITHashTable* remoteTable);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


struct J9JITExceptionTable;
#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
/**
* @brief
* @param *remoteMetaData
* @return struct J9JITExceptionTable *
*/
struct J9JITExceptionTable * 
dbgReadJITMetaData(struct J9JITExceptionTable *remoteMetaData);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


/**
* @brief
* @param remoteMethod
* @return J9Method *
*/
J9Method * dbgReadMethod(J9Method * remoteMethod);


/**
* @brief
* @param remotePool
* @return J9Pool *
*/
J9Pool * 
dbgReadPool(J9Pool* remotePool);


/**
* @brief
* @param remoteROMClass
* @return J9ROMClass *
*/
J9ROMClass * 
dbgReadROMClass(J9ROMClass * remoteROMClass);


/**
* @brief
* @param remoteSRPAddress
* @return void *
*/
void * 
dbgReadSRP(J9SRP * remoteSRPAddress);


/**
* @brief
* @param remoteAddress
* @return char*
*/
char* 
dbgReadString(char* remoteAddress);


/**
* @brief
* @param remoteAddress
* @return U_16
*/
U_16 
dbgReadU16(U_16 * remoteAddress);


/**
* @brief
* @param remoteAddress
* @return U_32
*/
U_32 
dbgReadU32(U_32 * remoteAddress);


/**
* @brief
* @param remoteAddress
* @return UDATA
*/
UDATA 
dbgReadUDATA(UDATA * remoteAddress);

/**
* @brief
* @param remoteAddress
* @return U_64
*/
U_64 
dbgReadU64(U_64 * remoteAddress);

/**
 * @brief
 * @param remoteAddress
 * @param size
 * @return UDATA
 */
UDATA
dbgReadPrimitiveType(UDATA remoteAddress, UDATA size);

/**
* @brief
* @param remoteThread
* @return J9VMThread *
*/
J9VMThread * 
dbgReadVMThreadForGC(J9VMThread * remoteThread);


/**
* @brief
* @param bufPointer
* @return void*
*/
void * 
dbgSetHandler(void* bufPointer);


/**
* @brief
* @param addr
* @param value
* @return void
*/
void 
dbgSetLocalBlockRelocated(void * addr, UDATA value);


/**
* @brief
* @param vmPtr
* @return void
*/
void 
dbgSetVM(void* vmPtr);


/**
* @brief
* @param void
* @return J9JavaVM*
*/
J9JavaVM* 
dbgSniffForJavaVM(void);


/**
* @brief
* @param addr
* @return void *
*/
void * 
dbgTargetToLocal(void * addr);


/**
* @brief
* @param addr
* @param size
* @return void *
*/
void * 
dbgTargetToLocalWithSize(void * addr, UDATA size);


/**
* @brief
* @param message
* @param arg_ptr
* @return void
*/
void 
dbgVPrint (const char* message, va_list arg_ptr);

/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_j9x(const char *args);

/**
* @brief
* @param *args
* @return void
*/
void
dbgext_j9vtables(const char * args);

/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_allclasses(const char *args);

/**
* @brief parse arguments for dumping structures
* @param args[in] the arguments to parse
* @param structAddress[out] the address of the struct to dump
* @param needle[out] a wildcard needle (see wildcard.c)
* @param needleLength[out] a wildcard needleLength (see wildcard.c)
* @param matchFlag[out] a wildcard matchFlag (see wildcard.c)
* @return 0 on success
*/
IDATA 
dbgParseArgForStructDump(const char * args, UDATA* structAddress, const char** needle, UDATA* needleLength, U_32 * matchFlag);

/**
* @brief
* @param args
* @return void
*/
void
dbgext_shrc(const char *args);

#if (defined(J9VM_OPT_SHARED_CLASSES))
/**
* @brief
* @param remoteVM
* @param length
* @param firstEntry
* @param reloc
* @return void *
*/
void *
dbgReadSharedCacheMetadata(J9JavaVM* remoteVM, UDATA* length, ShcItemHdr** firstEntry);
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
/* ---------------- trdbgext.c ---------------- */

#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
/**
* @brief
* @param void
* @return UDATA
*/
UDATA 
dbgTrInitialize(void);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


/**
* @brief
* @param void
* @return UDATA
*/
void
dbgPrintJ9ClassRuntimeModule(UDATA addr);

#ifdef __cplusplus
}
#endif

#endif /* dbgext_internal_h */

