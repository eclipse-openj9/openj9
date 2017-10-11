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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "j9.h"
#include "j9port.h"
#include "bcutil_api.h"
#include "cfr.h"
#include "cfreader.h"
#include "j2sever.h"
#include "j9socket.h"
#include "j9protos.h"
#include "j9consts.h"
#include "jimage.h"
#include "jimagereader.h"
#include "jvminit.h"
#include "romcookie.h"

#ifdef J9VM_OPT_ZIP_SUPPORT
#include "vmi.h"
#endif /* J9VM_OPT_ZIP_SUPPORT */

#include "dynload.h"
#include "ut_j9bcu.h"
#include "vm_api.h"

#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT))  /* File Level Build Flags */

static const U_8 classSuffix[] = ".class";
#define SUFFIX_LENGTH 6

static IDATA readFile (J9JavaVM * javaVM);
static IDATA convertToClassFilename (J9JavaVM * javaVM, U_8 * className, UDATA classNameLength);
#if (defined(J9VM_OPT_ZIP_SUPPORT)) 
static IDATA readZip (J9JavaVM * javaVM, J9ClassPathEntry * cpEntry);
#endif /* J9VM_OPT_ZIP_SUPPORT */
static IDATA convertToOSFilename (J9JavaVM * javaVM, U_8 * dir, UDATA dirLength, U_8 * moduleName, U_8 * className, UDATA classNameLength);
static IDATA checkSunClassFileBuffers (J9JavaVM * javaVM, U_32 sunClassFileSize);
static IDATA searchClassInModule(J9VMThread * vmThread, J9Module * j9module, U_8 * className, UDATA classNameLength, BOOLEAN verbose, J9TranslationLocalBuffer *localBuffer);
static IDATA searchClassInClassPath(J9VMThread * vmThread, J9ClassPathEntry * classPath, UDATA classPathCount, U_8 * className, UDATA classNameLength, BOOLEAN verbose, J9TranslationLocalBuffer *localBuffer);
static IDATA searchClassInPatchPaths(J9VMThread * vmThread, J9ClassPathEntry * patchPaths, UDATA patchPathCount, U_8 * className, UDATA classNameLength, BOOLEAN verbose, J9TranslationLocalBuffer *localBuffer);
static IDATA searchClassInCPEntry(J9VMThread * vmThread, J9ClassPathEntry * cpEntry, J9Module * j9module, U_8 * moduleName, U_8 * className, UDATA classNameLength, BOOLEAN verbose);
static IDATA readFileFromJImage (J9VMThread * vmThread, J9Module * j9module, U_8 * moduleName, J9ClassPathEntry *cpEntry);

IDATA 
findLocallyDefinedClass(J9VMThread * vmThread, J9Module * j9module, U_8 * className, U_32 classNameLength, J9ClassLoader * classLoader, J9ClassPathEntry * classPath, UDATA classPathEntryCount, UDATA options, J9TranslationLocalBuffer *localBuffer)
{
	IDATA result = 0;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9TranslationBufferSet *dynamicLoadBuffers = javaVM->dynamicLoadBuffers;
	J9DynamicLoadStats *dynamicLoadStats = dynamicLoadBuffers->dynamicLoadStats;
	J9Module *module = j9module;
	BOOLEAN verbose = dynamicLoadBuffers->flags & BCU_VERBOSE;
#define LOCAL_MAX 80
	U_8 localString[LOCAL_MAX];
	UDATA mbLength = classNameLength;
	U_8 *mbString = localString;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	/* localBuffer should not be NULL */
	Trc_BCU_Assert_True(NULL != localBuffer);

	localBuffer->loadLocationType = 0;
	localBuffer->entryIndex = J9_CP_INDEX_NONE;
	localBuffer->cpEntryUsed = NULL;
	dynamicLoadBuffers->classFileError = NULL;

	if (mbLength >= LOCAL_MAX) {
		mbString = j9mem_allocate_memory(mbLength + 1, J9MEM_CATEGORY_CLASSES);
		if (NULL == mbString) {
			return -1;
		}
	}
	mbString[mbLength] = 0;		/* ensure null terminated */
	memcpy(mbString, className, classNameLength);
	
	CHECK_BUFFER(dynamicLoadStats->name, dynamicLoadStats->nameBufferLength, (mbLength + 1));
	dynamicLoadStats->nameLength = mbLength;
	memcpy(dynamicLoadStats->name, mbString, mbLength + 1);
	
	Trc_BCU_findLocallyDefinedClass_Entry(mbString, classPathEntryCount);
	
	/* Search patch path of the module before looking up in bootclasspath */
	if (J2SE_VERSION(javaVM) >= J2SE_19) {
		if (NULL == module) {
			if (J9_ARE_NO_BITS_SET(javaVM->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
				module = javaVM->javaBaseModule;
			} else {
				char *packageEnd = strrchr((const char*)mbString, '/');

				if (NULL != packageEnd) {
					omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);
					module = J9_VM_FUNCTION(vmThread, findModuleForPackage)(vmThread, classLoader, mbString, (U_32)((U_8 *)packageEnd - mbString));
					omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);
				}
			}
		}
		if (NULL != module) {
			if (NULL != classLoader->moduleExtraInfoHashTable) {
				J9ModuleExtraInfo *moduleInfo = NULL;

				omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);
				moduleInfo = J9_VM_FUNCTION(vmThread, findModuleInfoForModule)(vmThread, classLoader, module);
				omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);

				if ((NULL != moduleInfo) && (NULL != moduleInfo->patchPathEntries)) {
					result = searchClassInPatchPaths(vmThread, moduleInfo->patchPathEntries, moduleInfo->patchPathCount, mbString, mbLength, verbose, localBuffer);
					if (0 == result) {
						goto _end;
					}
				}
			}

			/* If not found in patch paths, search the class in its module */
			result = searchClassInModule(vmThread, module, mbString, mbLength, verbose, localBuffer);
			if (1 == result) {
				/* If we failed to find the class in the module passed as parameter and J9_FINDCLASS_FLAG_FIND_MODULE_ON_FAIL is set,
				* then check the class in other modules defined by the loader.
				*/
				if ((NULL != j9module)
					&& J9_ARE_ALL_BITS_SET(options, J9_FINDCLASS_FLAG_FIND_MODULE_ON_FAIL)
				) {
					J9Module *j9moduleFromPackage = NULL;
					char *packageEnd = strrchr((const char*)className, '/');

					if (NULL != packageEnd) {
						omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);

						j9moduleFromPackage = J9_VM_FUNCTION(vmThread, findModuleForPackage)(vmThread, classLoader, className, (U_32)((U_8 *)packageEnd - className));

						if ((NULL != j9moduleFromPackage) && (j9moduleFromPackage != j9module)) {
							J9ModuleExtraInfo *moduleInfo = J9_VM_FUNCTION(vmThread, findModuleInfoForModule)(vmThread, classLoader, j9moduleFromPackage);

							if ((NULL != moduleInfo) && (NULL != moduleInfo->patchPathEntries)) {
								result = searchClassInPatchPaths(vmThread, moduleInfo->patchPathEntries, moduleInfo->patchPathCount, className, classNameLength, verbose, localBuffer);
							}
							if (0 != result) {
								result = searchClassInModule(vmThread, j9moduleFromPackage, mbString, mbLength, verbose, localBuffer);
							}
						}
						omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);
					}
				}
			} else {
				goto _end;
			}
		}
	}

	/* If the class is still not found, search it in classpath */
	result = searchClassInClassPath(vmThread, classPath, classPathEntryCount, mbString, mbLength, verbose, localBuffer);

_end:
	if (0 != result) {
		Trc_BCU_findLocallyDefinedClass_Failed(mbString);
		result = -1;
	} else {
		Trc_BCU_findLocallyDefinedClass_Exit(mbString, dynamicLoadBuffers->searchFilenameBuffer);
	}
	if ((U_8 *)mbString != localString) {
		j9mem_free_memory(mbString);
	}
	return result;
}

/**
 * Search a class in the specified module.
 *
 * @param [in] vmThread pointer to current J9VMThread
 * @param [in] j9module module in which to search the class
 * @param [in] className name of the class to be searched
 * @param [in] classNameLength length of the className
 * @param [in] verbose if TRUE record the class loading stats
 * @param [in/out] localBuffer contains values for entryIndex, loadLocationType and cpEntryUsed. This pointer can't be NULL.
 *
 * @return 0 on success, 1 if the class is not found, -1 on error
 */
static IDATA
searchClassInModule(J9VMThread * vmThread, J9Module * j9module, U_8 * className, UDATA classNameLength, BOOLEAN verbose, J9TranslationLocalBuffer *localBuffer)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	char moduleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *moduleName = moduleNameBuf;
	BOOLEAN freeModuleName = FALSE;
	IDATA rc = 1;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	/* localBuffer should not be NULL */
	Trc_BCU_Assert_True(NULL != localBuffer);

	if (j9module == javaVM->javaBaseModule) {
		moduleName = JAVA_BASE_MODULE;
	} else {
		moduleName = J9_VM_FUNCTION(vmThread, copyStringToUTF8WithMemAlloc)(
			vmThread, j9module->moduleName, J9_STR_NONE, "", moduleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
		if (NULL == moduleName) {
			rc = -1;
			goto _end;
		}
		if (moduleNameBuf != moduleName) {
			freeModuleName = TRUE;
		}
	}

	rc = searchClassInCPEntry(vmThread, javaVM->modulesPathEntry, j9module, (U_8*)moduleName, className, classNameLength, verbose);
	if (0 == rc) {
		localBuffer->loadLocationType = LOAD_LOCATION_MODULE;
	}

	if (TRUE == freeModuleName) {
		j9mem_free_memory(moduleName);
	}
_end:
	return rc;
}

/**
 * Search a class in the classpath.
 *
 * @param [in] vmThread pointer to current J9VMThread
 * @param [in] classPath array of class path entries in which to search the class
 * @param [in] classPathCount number of entries in classPath array
 * @param [in] className name of the class to be searched
 * @param [in] classNameLength length of the className
 * @param [in] verbose if TRUE record the class loading stats
 * @param [in/out] localBuffer contains values for entryIndex, loadLocationType and cpEntryUsed. This pointer can't be NULL.
 *
 * @return 0 on success, 1 if the class is not found, -1 on error
 */
static IDATA
searchClassInClassPath(J9VMThread * vmThread, J9ClassPathEntry * classPath, UDATA classPathCount, U_8 * className, UDATA classNameLength, BOOLEAN verbose, J9TranslationLocalBuffer *localBuffer)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = javaVM->internalVMFunctions;
	J9ClassPathEntry *cpEntry = NULL;
	IDATA rc = 1;
	UDATA i = 0;

	/* localBuffer should not be NULL */
	Trc_BCU_Assert_True(NULL != localBuffer);

	for (i = 0; i < classPathCount; i++) {
		cpEntry = &classPath[i];

		/* Warm up the entry */
		vmFuncs->initializeClassPathEntry(javaVM, cpEntry);
		rc = searchClassInCPEntry(vmThread, cpEntry, NULL, NULL, className, classNameLength, verbose);
		if (0 == rc) {
			localBuffer->cpEntryUsed = cpEntry;
			localBuffer->loadLocationType = LOAD_LOCATION_CLASSPATH;
			localBuffer->entryIndex = i;
			break;
		}
	}

	return rc;
}

/**
 * Search a class in the patch paths.
 *
 * @param [in] vmThread pointer to current J9VMThread
 * @param [in] patchPaths array of patch path entries in which to search the class
 * @param [in] patchPathCount number of entries in patchPaths array
 * @param [in] className name of the class to be searched
 * @param [in] classNameLength length of the className
 * @param [in] verbose if TRUE record the class loading stats
 * @param [in/out] localBuffer contains values for entryIndex, loadLocationType and cpEntryUsed. This pointer can't be NULL.
 *
 * @return 0 on success, 1 if the class is not found, -1 on error
 */
static IDATA
searchClassInPatchPaths(J9VMThread * vmThread, J9ClassPathEntry * patchPaths, UDATA patchPathCount, U_8 * className, UDATA classNameLength, BOOLEAN verbose, J9TranslationLocalBuffer *localBuffer)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = javaVM->internalVMFunctions;
	J9ClassPathEntry *patchEntry = NULL;
	IDATA rc = 1;
	UDATA i = 0;

	/* localBuffer should not be NULL */
	Trc_BCU_Assert_True(NULL != localBuffer);

	for (i = 0; i < patchPathCount; i++) {
		patchEntry = &patchPaths[i];
		vmFuncs->initializeClassPathEntry(javaVM, patchEntry);
		rc = searchClassInCPEntry(vmThread, patchEntry, NULL, NULL, className, classNameLength, verbose);
		if (0 == rc) {
			localBuffer->cpEntryUsed = patchEntry;
			localBuffer->loadLocationType = LOAD_LOCATION_PATCH_PATH;
			localBuffer->entryIndex = i;
			break;
		}
	}
	return rc;
}

/**
 * Search a class in the given J9ClassPathEntry.
 * The J9ClassPathEntry may denote a class path entry or a patch path entry.
 *
 * @param [in] vmThread pointer to current J9VMThread
 * @param [in] cpEntry an entry in which to search the class
 * @param [in] j9module module in which to search the class
 * @param [in] moduleName name of the module
 * @param [in] className name of the class to be searched
 * @param [in] classNameLength length of the className
 * @param [in] verbose if TRUE record the class loading stats
 *
 * @return 0 on success, 1 if the class is not found, -1 on error
 */
static IDATA
searchClassInCPEntry(J9VMThread * vmThread, J9ClassPathEntry * cpEntry, J9Module * j9module, U_8 *moduleName, U_8 * className, UDATA classNameLength, BOOLEAN verbose)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9TranslationBufferSet *dynamicLoadBuffers = javaVM->dynamicLoadBuffers;
	J9DynamicLoadStats *dynamicLoadStats = dynamicLoadBuffers->dynamicLoadStats;
	IDATA rc = 1;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	switch (cpEntry->type)
	{
		case CPE_TYPE_DIRECTORY:
			if (convertToOSFilename(javaVM, cpEntry->path, cpEntry->pathLength, moduleName, className, classNameLength)) {
				rc = -1;
				break;
			}
			if(verbose) {
				dynamicLoadStats->readStartTime = j9time_usec_clock();
			}
			rc = readFile(javaVM);
			if(verbose) {
				dynamicLoadStats->readEndTime = j9time_usec_clock();
			}
			break;

#ifdef J9VM_OPT_ZIP_SUPPORT
		case CPE_TYPE_JAR:
			if (convertToClassFilename(javaVM, className, classNameLength)) {
				rc = -1;
				break;
			}
			if(verbose) {
				dynamicLoadStats->readStartTime = j9time_usec_clock();
			}
			rc = readZip(javaVM, cpEntry);
			if(verbose) {
				dynamicLoadStats->readEndTime = j9time_usec_clock();
			}
			break;
#endif

		case CPE_TYPE_JIMAGE:
			if (NULL != j9module) {
				if (convertToClassFilename(javaVM, className, classNameLength)) {
					rc = -1;
					break;
				}
				if(verbose) {
					dynamicLoadStats->readStartTime = j9time_usec_clock();
				}
				rc = readFileFromJImage(vmThread, j9module, moduleName, javaVM->modulesPathEntry);
				if(verbose) {
					dynamicLoadStats->readEndTime = j9time_usec_clock();
				}
			}
			break;

		case CPE_TYPE_UNUSABLE:
			/* Skip this entry */
			rc = 1;
			break;

		default:
			/* Should never get here. */
			rc = 1;
			Trc_BCU_searchClassInCPEntry_UnexpectedCPE(cpEntry->path, cpEntry->type);
			Trc_BCU_Assert_ShouldNeverHappen();
			break;
	}

	return rc;
}



/* 
	Compute the file system name for a class called className in the specified classPath entry.
	Return 0 on success, non-zero on error.
*/

static IDATA 
convertToOSFilename (J9JavaVM * javaVM, U_8 * dir, UDATA dirLength, U_8 * moduleDir, U_8 * className, UDATA classNameLength) {

	PORT_ACCESS_FROM_JAVAVM(javaVM);
	U_32 i;
	UDATA filenameLength;
	U_8 *filename;
	U_8 *writePos;
	U_8 pathSeparator;
	UDATA moduleDirLength = 0;
	UDATA requiredSize = dirLength + 1 + classNameLength + SUFFIX_LENGTH + 1;

	if (NULL != moduleDir) {
		moduleDirLength = strlen((const char*)moduleDir);
		requiredSize += (moduleDirLength + 1);
	}

	CHECK_BUFFER(javaVM->dynamicLoadBuffers->searchFilenameBuffer,
				 javaVM->dynamicLoadBuffers->searchFilenameSize,
				 ROUND_TO(ROUNDING_GRANULARITY, requiredSize));

	filenameLength = javaVM->dynamicLoadBuffers->searchFilenameSize;
	filename = javaVM->dynamicLoadBuffers->searchFilenameBuffer;

	pathSeparator = (U_8) javaVM->pathSeparator;

	/* copy the possible directory name into the search buffer */
	memcpy(filename, dir, dirLength);
	writePos = &filename[dirLength];
	if (filename[dirLength - 1] != pathSeparator
#if defined(WIN32) || defined(OS2)
		&& filename[dirLength - 1] != ':'	/* so that 'J:' doesn't turn into 'J:\' */
#endif
		) {
		*writePos++ = pathSeparator;
	}

	if (NULL != moduleDir) {
		memcpy(writePos, moduleDir, moduleDirLength);
		writePos += moduleDirLength;
		*writePos++ = pathSeparator;
	}

	/* WARNING: this does not handle multi-byte walks yet */
	for (i = 0; i < classNameLength; i++) {
		if (className[i] == '/') {
			*writePos++ = pathSeparator;
		} else {
			*writePos++ = className[i];
		}
	}

	/* tack on the final .class */
	memcpy(writePos, classSuffix, SUFFIX_LENGTH);
	writePos[SUFFIX_LENGTH] = 0;
	return 0;
}



/* 
	Verify that the internal dynamic loader buffers used to hold the Sun class file are large enough
	to accomodate @sunClassFileSize bytes.  Grow buffers if neccessary.

	Return 0 on success, non-zero on error.
*/

static IDATA 
checkSunClassFileBuffers (J9JavaVM * javaVM, U_32 sunClassFileSize) {

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	/* Users assume that the buffer pointer is NULLed in the allocation failure path in the MACRO */
	CHECK_BUFFER(javaVM->dynamicLoadBuffers->sunClassFileBuffer,
				 javaVM->dynamicLoadBuffers->sunClassFileSize, ROUND_TO(ROUNDING_GRANULARITY, sunClassFileSize));

	return 0;
}

/**	
 * Compute the file system name for a class called className in the specified classPath entry.
 * Return 0 on success, non-zero on error.
 */
static IDATA 
convertToClassFilename (J9JavaVM * javaVM, U_8 * className, UDATA classNameLength) {

	PORT_ACCESS_FROM_JAVAVM(javaVM);
	UDATA filenameLength;
	U_8 *filename;

	CHECK_BUFFER(javaVM->dynamicLoadBuffers->searchFilenameBuffer,
				 javaVM->dynamicLoadBuffers->searchFilenameSize, ROUND_TO(ROUNDING_GRANULARITY, (classNameLength + SUFFIX_LENGTH + 1)));

	filenameLength = javaVM->dynamicLoadBuffers->searchFilenameSize;
	filename = javaVM->dynamicLoadBuffers->searchFilenameBuffer;

	/* copy the class name into the search buffer */
	memcpy(filename, className, classNameLength);

	/* tack on the final .class */
	memcpy(&filename[classNameLength], classSuffix, SUFFIX_LENGTH);
	filename[classNameLength + SUFFIX_LENGTH] = 0;
	return 0;
}

/* 
	Attempt to read the file whose name is in the global name search buffer.
	
		Return 0 on success.
		1, no such file (non fatal error)
		-1, for read error
*/

static IDATA 
readFile (J9JavaVM * javaVM) {

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	I_64 actualFileSize;
	IDATA bytesRead;
	U_32 fileSize;
	IDATA fd = j9file_open((char *) javaVM->dynamicLoadBuffers->searchFilenameBuffer, EsOpenRead, 0);

	if (fd == -1) {
		return 1;
	}

	actualFileSize = j9file_seek(fd, 0, EsSeekEnd);
	/* Restrict class file size to < 2G */
	if ((actualFileSize == -1) || (actualFileSize > J9CONST64(0x7FFFFFFF))) {
		goto _failedFileRead;
	}
	fileSize = (U_32)actualFileSize;

	if (checkSunClassFileBuffers (javaVM, fileSize)) {
		goto _failedFileRead;
	}

	j9file_seek(fd, 0, EsSeekSet);
	bytesRead = j9file_read(fd, javaVM->dynamicLoadBuffers->sunClassFileBuffer, fileSize);
	if ((U_32) bytesRead != fileSize) {
		goto _failedFileRead;
	}
	javaVM->dynamicLoadBuffers->currentSunClassFileSize = fileSize;
	j9file_close(fd);
	return 0;

_failedFileRead:
	j9file_close(fd);
	return -1;
}



#if (defined(J9VM_OPT_ZIP_SUPPORT)) 
/* 
	Attempt to read the file whose name is in the global name search buffer from the
	zip file specified by the cpEntry.
	
		Return 0 on success.
		1, no such file (non fatal error)
		-1, for read error
*/

static IDATA 
readZip (J9JavaVM * javaVM, J9ClassPathEntry * cpEntry) {

	VMI_ACCESS_FROM_JAVAVM((JavaVM*)javaVM);
	VMIZipFunctionTable* zipFunctions = (*VMI)->GetZipFunctions(VMI);

	VMIZipFile *zipFile;
	VMIZipEntry entry;
	I_32 result;
	U_32 size;
	IDATA filenameLength;

	zipFile = (VMIZipFile *) (cpEntry->extraInfo);

	/* Find the desired entry, if it is present. */
	filenameLength = strlen((const char*)javaVM->dynamicLoadBuffers->searchFilenameBuffer);
	zipFunctions->zip_initZipEntry (VMI, &entry);
	result = zipFunctions->zip_getZipEntryWithSize (VMI, zipFile, &entry, (char *) javaVM->dynamicLoadBuffers->searchFilenameBuffer, filenameLength, ZIP_FLAG_READ_DATA_POINTER);
	if (result) {
		/* Class not found. */
		result = 1;
		goto finished;
	}

	size = entry.uncompressedSize;
	if (checkSunClassFileBuffers(javaVM, size)) {
		/* Out of memory. */
		result = -1;
		goto finished;
	}

	result = zipFunctions->zip_getZipEntryData (VMI, zipFile, &entry, javaVM->dynamicLoadBuffers->sunClassFileBuffer, size);
	if (result) {
		/* Error extracting data. */
		result = 1;
		goto finished;
	}

	javaVM->dynamicLoadBuffers->currentSunClassFileSize = size;

  finished:
  	zipFunctions->zip_freeZipEntry(VMI, &entry);
	return result;
}

#endif /* J9VM_OPT_ZIP_SUPPORT */

/**
 * Attempts to locate file whose name is in the global name search buffer in the JImage file specified by cpEntry in the give module.
 *
 * Returns 0 on success, 1 file is not found, -1 on error
 */
static IDATA
readFileFromJImage (J9VMThread *vmThread, J9Module *j9module, U_8 *moduleName, J9ClassPathEntry *cpEntry)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9JImageIntf *jimageIntf = javaVM->jimageIntf;
	UDATA jimageHandle = (UDATA)cpEntry->extraInfo;
	UDATA resourceLocation = 0;
	I_64 size = 0;
	J9TranslationBufferSet *dynamicLoadBuffers = javaVM->dynamicLoadBuffers;
	const char *resourceName = (const char *)dynamicLoadBuffers->searchFilenameBuffer;
	I_32 rc = J9JIMAGE_NO_ERROR;
	
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	Trc_BCU_readFileFromJImage_Entry(resourceName);

	Trc_BCU_Assert_True(NULL != j9module);

	rc = jimageIntf->jimageFindResource(jimageIntf, jimageHandle, (const char *)moduleName, resourceName, &resourceLocation, &size);
	if (J9JIMAGE_NO_ERROR == rc) {
		Trc_BCU_readFileFromJImage_LookupPassed_V1(moduleName, resourceName);
		if (checkSunClassFileBuffers(javaVM, (U_32)size)) {
			/* Out of memory. */
			Trc_BCU_readFileFromJImage_BufferAllocationFailed_V1(moduleName, resourceName, size);
			rc = -1;
		} else {
			rc = jimageIntf->jimageGetResource(jimageIntf, jimageHandle, resourceLocation, (char *)dynamicLoadBuffers->sunClassFileBuffer, dynamicLoadBuffers->sunClassFileSize, NULL);
			if (J9JIMAGE_NO_ERROR == rc) {
				dynamicLoadBuffers->currentSunClassFileSize = (UDATA)size;
				rc = 0;
			} else {
				rc = -1;
			}
		}
		jimageIntf->jimageFreeResourceLocation(jimageIntf, jimageHandle, resourceLocation);
	} else {
		Trc_BCU_readFileFromJImage_LookupFailed_V1(moduleName, resourceName, rc);
		rc = (J9JIMAGE_RESOURCE_NOT_FOUND == rc) ? 1 : -1;
	}

	Trc_BCU_readFileFromJImage_Exit(rc);

	return rc;
}

#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */ /* End File Level Build Flags */
