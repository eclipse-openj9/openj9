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

#ifndef bcutil_api_h
#define bcutil_api_h

/**
* @file bcutil_api.h
* @brief Public API for the BCUTIL module.
*
* This file contains public function prototypes and
* type definitions for the BCUTIL module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "jni.h"
#include "cfr.h"
#include "jimage.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- bcutil.c ---------------- */

/**
* @brief
* @param *portLib
* @return J9TranslationBufferSet *
*/
J9TranslationBufferSet * 
j9bcutil_allocTranslationBuffers(J9PortLibrary *portLib);


struct J9TranslationBufferSet;
struct J9ClassLoader;
/**
* @brief
* @param classFileBytes
* @param classFileSize
* @param portLib
* @param verifyBuffers
* @param flags
* @param romSegment
* @param romSegmentSize
* @param classFileBufferPtr
* @return IDATA
*/
IDATA
j9bcutil_buildRomClassIntoBuffer(U_8 * classFileBytes, UDATA classFileSize, J9PortLibrary * portLib,
		struct J9BytecodeVerificationData * verifyBuffers, UDATA bctFlags, UDATA bcuFlags, UDATA findClassFlags,
		U_8 * romSegment, UDATA romSegmentSize,
		U_8 * lineNumberBuffer, UDATA lineNumberBufferSize,
		U_8 * varInfoBuffer, UDATA varInfoBufferSize,
		U_8 ** classFileBufferPtr);

/**
* @brief
* @param javaVM
* @param portLibrary
* @param romClass
* @param classData
* @param size
* @return IDATA
*/
IDATA
j9bcutil_transformROMClass(J9JavaVM *javaVM, J9PortLibrary *portLibrary, J9ROMClass *romClass, U_8 **classData, U_32 *size);

/**
* @brief
* @param loadData
* @param intermediateData
* @param intermediateDataLength
* @param javaVM
* @param bctFlags
* @param classFileBytesReplaced
* @param isIntermediateROMClass
* @param [in/out] localBuffer contains values for entryIndex, loadLocationType and cpEntryUsed. This pointer can't be NULL.
* @return IDATA
*/
IDATA
j9bcutil_buildRomClass(J9LoadROMClassData *loadData, U_8 * intermediateData, UDATA intermediateDataLength, J9JavaVM *javaVM, UDATA bctFlags, UDATA classFileBytesReplaced, UDATA isIntermediateROMClass, J9TranslationLocalBuffer *localBuffer);

void
shutdownROMClassBuilder(J9JavaVM *vm);

#if defined(J9DYN_TEST)
IDATA
j9bcutil_compareRomClass(
		U_8 * classFileBytes,
		U_32 classFileSize,
		J9PortLibrary * portLib,
		struct J9BytecodeVerificationData * verifyBuffers,
		UDATA bctFlags,
		UDATA bcuFlags,
		J9ROMClass *romClass );
#endif
/**
* @brief
* @param *portLib
* @param *translationBuffers
* @return IDATA
*/
IDATA 
j9bcutil_freeAllTranslationBuffers ( J9PortLibrary *portLib, J9TranslationBufferSet *translationBuffers );


/**
* @brief
* @param portLib
* @param translationBuffers
* @return IDATA
*/
IDATA 
j9bcutil_freeTranslationBuffers (J9PortLibrary * portLib, J9TranslationBufferSet * translationBuffers);


/**
* @brief
* @param *dest
* @param *source
* @param length
* @return I_32
*/
I_32 
j9bcutil_verifyCanonisizeAndCopyUTF8  (U_8 *dest, U_8 *source, U_32 length);


/**
* @brief
* @param vm
* @param stage
* @param reserved
* @return IDATA
*/
IDATA 
bcutil_J9VMDllMain (J9JavaVM* vm, IDATA stage, void* reserved);


/* ---------------- cfreader.c ---------------- */

/**
* @brief
* @param portLib
* @param verifyFunction
* @param data
* @param dataLength
* @param segment
* @param segmentLength
* @param flags
* @param hasRET
* @param segmentFreePointer
* @param verboseContext
* @param findClassFlags
* @param romClassSortingThreshold
* @return I_32
*/
I_32 
j9bcutil_readClassFileBytes (J9PortLibrary *portLib, IDATA (*verifyFunction) (J9PortLibrary *aPortLib, J9CfrClassFile* classfile, U_8* segment, U_8* segmentLength, U_8* freePointer, U_32 vmVersionShifted, U_32 flags, I_32 *hasRET), U_8* data, UDATA dataLength, U_8* segment, UDATA segmentLength, U_32 flags, U_8** segmentFreePointer, void *verboseContext, UDATA findClassFlags, UDATA romClassSortingThreshold);

/* ---------------- defineclass.c ---------------- */

#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT))  /* File Level Build Flags */

/**
* @brief
* @param vmThread
* @param className
* @param classNameLength
* @param classData
* @param classDataLength
* @param classDataObject
* @param classLoader
* @param protectionDomain
* @param options
* @param existingROMClass
* @param hostClass the class creating the anonymous class
* @param [in/out] localBuffer contains values for entryIndex, loadLocationType and cpEntryUsed. This pointer can't be NULL.
* @return J9Class*
*/
J9Class* 
internalDefineClass (
	J9VMThread* vmThread, 
	void* className, 
	UDATA classNameLength, 
	U_8* classData, 
	UDATA classDataLength, 
	j9object_t classDataObject, 
	J9ClassLoader* classLoader, 
	j9object_t protectionDomain, 
	UDATA options,
	J9ROMClass *existingROMClass,
	J9Class *hostClass,
	J9TranslationLocalBuffer *localBuffer);


/**
* @brief
* @param vmThread
* @param loadData
* @param [in/out] localBuffer contains values for entryIndex, loadLocationType and cpEntryUsed. This pointer can't be NULL.
* @return UDATA
*/
UDATA 
internalLoadROMClass(J9VMThread *vmThread, J9LoadROMClassData *loadData, J9TranslationLocalBuffer *localBuffer);


#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */ /* End File Level Build Flags */


/* ---------------- dynload.c ---------------- */

#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT))  /* File Level Build Flags */

/**
 * Searches for the definition of the class called className in the patch path of the module, or the classPath.
 * It first searches the patch path of the module to which the class belongs.
 * If the definition is not found, then it examines the classPath entries in turn.
 * If a class is located copy the contents and size of the Sun
 * format class file into the sunClassFileBuffer element of the global vm translation buffers.
 * The @className parameter is a UTF8 encoded buffer delimited by forward slashes.
 * Answer zero on success, -1 on failure.
 *
* @param vmThread pointer to J9VMThread
* @param moduleName name of the module containing the class; can be NULL
* @param className name of the class to be located
* @param classNameLength lenght of className
* @param classLoader pointer to J9ClassLoader loading the class
* @param classPath pointer to class path entries
* @param classPathEntryCount number of class path entries in classPath
* @param options load options such as J9_FINDCLASS_FLAG_EXISTING_ONLY
* @param flags flags such as BCU_BOOTSTRAP_ENTRIES_ONLY
* @param [in/out] localBuffer contains values for entryInfex, loadLocationType and cpEntryUsed. This pointer can't be NULL.
*
* @return zero on success, -1 on failure.
*/
IDATA 
findLocallyDefinedClass(J9VMThread * vmThread, J9Module *j9module, U_8 * className, U_32 classNameLength, J9ClassLoader * classLoader, J9ClassPathEntry * classPath, UDATA classPathEntryCount, UDATA options, J9TranslationLocalBuffer *localBuffer);


#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */ /* End File Level Build Flags */

/* ---------------- jimageintf.c ---------------- */

/**
 * Creates and initializes J9JImageIntf and returns its pointer in *jimageIntf.
 *
 * @param [out] jimageIntf on success points to J9JImageIntf; should not be NULL
 * @param [in] vm pointer to J9JavaVM; may be NULL for unit testing
 * @param [in] portLibrary pointer to J9PortLibrary
 *
 * @return J9JIMAGE_NO_ERROR on success, negative error code on failure
 */
I_32
initJImageIntf(J9JImageIntf **jimageIntf, J9JavaVM *vm, J9PortLibrary *portLibrary);

/**
 * Loads the library specified by libjimage.
 * If the library is successfully loaded, then it creates and initializes J9JImageIntf
 * and returns its pointer in *jimageIntf.
 *
 * @param [out] jimageIntf on success points to J9JImageIntf; should not be NULL
 * @param [in] portLibrary pointer to J9PortLibrary
 * @param [in] libjimage name of jimage library to use for reading jimage files
 *
 * @return J9JIMAGE_NO_ERROR on success, negative error code on failure
 */
I_32
initJImageIntfWithLibrary(J9JImageIntf **jimageIntf, J9PortLibrary *portLibrary, const char *libjimage);

/**
 * Cleans up jimageIntf and frees up any resources held by it.
 *
 * @param [in] jimageIntf pointer to J9JImageIntf
 *
 * @return void
 */
void
closeJImageIntf(J9JImageIntf *jimageIntf);

/**
 * Opens jimage file and returns its handle in pointer passed as the argument.
 *
 * @param [in] jimageIntf pointer to J9JImageIntf
 * @param [in] name name of the jimage file to be opened
 * @param [in/out] handle on success handle to jimage file is returned in this parameter; should not be NULL
 *
 * @return J9JIMAGE_NO_ERROR on success; negative error code on failure
 */
I_32
jimageOpen(J9JImageIntf *jimageIntf, const char *name, UDATA *handle);

/**
 * Close the jimage file corresponding to the handle provided and free up any resources.
 *
 * @param [in] jimageIntf pointer to J9JImageIntf
 * @param [in] handle handle to the jimage file
 *
 * @return void
 */
void
jimageClose(J9JImageIntf *jimageIntf, UDATA handle);

/**
 * Locates a resource given the module name and the resource name.
 * Resource name should be specified in following format : [package/]name[.extension] eg java/lang/Object.class
 *
 * @param [in] jimageIntf pointer to J9JImageIntf
 * @param [in] handle handle to the jimage file
 * @param [in] module name of the module
 * @param [in] name name of the resource
 * @param [in/out] resourceLocation location of the resource is returned in this parameter; should not be NULL
 * @param [in/out] size size of the resource is returned in this parameter; should not be NULL
 *
 * @return J9JIMAGE_NO_ERROR on success; negative error code on failure
 */
I_32
jimageFindResource(J9JImageIntf *jimageIntf, UDATA handle, const char *moduleName, const char* name, UDATA *resourceLocation, I_64 *size);

/**
 * Frees up any resources held by resource location.
 *
 * @param [in] jimageIntf pointer to J9JImageIntf
 * @param [in] handle handle to the jimage file
 * @param [in] resourceLocation location of the resource
 */
void
jimageFreeResourceLocation(J9JImageIntf *jimageIntf, UDATA handle, UDATA resourceLocation);

/**
 * Returns the contents of the resource specified by given location in the jimage file in the buffer provided.
 * If the buffer size is not large enough then the resource is truncated.
 * The actual size of the resource is returned in the last parameter if it is not NULL.
 *
 * @param [in] jimageIntf pointer to J9JImageIntf
 * @param [in] handle handle to the jimage file
 * @param [in] resourceLocation location of the resource
 * @param [out] buffer a byte buffer for storing contents of the resource; should not be NULL
 * @param [in] bufferSize size of the buffer
 * @param [out] resourceSize actual size of the resource
 *
 * @return J9JIMAGE_NO_ERROR if the resource contents are successfully stored in the buffer;
 * 		   J9JIMAGE_RESOURCE_TRUNCATED if the buffer size is not large enough to store the whole resource in the buffer;
 * 		   negative error code in other cases
 */
I_32
jimageGetResource(J9JImageIntf *jimageIntf, UDATA handle, UDATA resourceLocation, char *buffer, I_64 bufferSize, I_64 *resourceSize);

/**
 * Finds the module for the given package.
 *
 * @param [in] jimageIntf pointer to J9JImageIntf
 * @param [in] handle handle to the jimage file
 * @param [in] package name of the package in the format com/ibm/oti
 *
 * @return returns module name for the given package.
 */
const char *
jimagePackageToModule(J9JImageIntf *jimageIntf, UDATA handle, const char *packageName);

/* ---------------- jimagereader.c ----------------*/

/**
 * This function loads jimage file specified by fileName. As part of loading it performs following operation:
 * 1) Open jimage file and read the header
 * 2) Verify header
 * 3) Memory map jimage file upto start of resources
 * 4) Create J9JImage and J9JImageHeader structure and populate the fields
 *
 * @param [in] vm pointer to J9JavaVM
 * @param [in] fileName	name of jimage file
 * @param [out] pjimage	double pointer to J9JImage; on successful exit points to valid J9JImage
 *
 * @return if no error occurs returns J9JIMAGE_NO_ERROR and *pjimage points to a valid J9JImage,
 * 			otherwise returns negative value error code and *pjimage is set to NULL.
 */
I_32
j9bcutil_loadJImage(J9PortLibrary *portlib, const char *fileName, J9JImage **pjimage);

/**
 * Dump information about jimage header and resources in a readable format.
 *
 * @param [in] portlib pointer to J9PortLibrary
 * @param [in] jimage pointer to J9JImage representing jimage file
 *
 * @return void
 */
void 
j9bcutil_dumpJImageInfo(J9PortLibrary *portlib, J9JImage *jimage);

/**
 * Looks up a resource with name "resourceName" in the given jimage file.
 * If the resource is found, it verifies it
 *
 * @param [in] portlib pointer to J9PortLibrary
 * @param [in] jimage pointer to J9JImage representing a jimage file; must not be NULL
 * @param [out] j9jimageLocation pointer to J9JImageLocation; if not NULL then on success it is filled with metadata about the requested resource
 * @param [in] resourceName name of resource to be searched in jimage file
 *
 * @return if resource is found and no error occurs then returns J9JIMAGE_NO_ERROR,
 * 		   if resource is not found, returns J9JIMAGE_RESOURCE_NOT_FOUND,
 * 		   if any error occurs, returns a negative error code
 */
I_32
j9bcutil_lookupJImageResource(J9PortLibrary *portlib, J9JImage *jimage, J9JImageLocation *j9jimageLocation, const char *resourceName);

/**
 * Creates J9JImageLocation structure corresponding to image location in the jimage, and
 * verifies that the J9JImageLocation corresponds to the given resource name in the jimage file.
 *
 * Verification is done by comparing the strings corresponding to BASE, PARENT and EXTENSION attribute types with the resource name.
 *
 * @param [in] portlib pointer to J9PortLibrary
 * @param [in] jimage pointer to J9JImage; must not be NULL
 * @param [in] resourceName name of the resource in jimage
 * @param [in] imageLocation pointer to image location in the mmapped region of jimage
 * @param [out] j9jimageLocation pointer to J9JImageLocation; if non-NULL, then on success it is filled with metadata pointed by imageLocation
 *
 * @return if no error occurs then returns J9JIMAGE_NO_ERROR,
 * 		   if verification fails, returns J9JIMAGE_LOCATION_VERIFICATION_FAIL,
 * 		   if any other error occurs, returns a negative value error code
 */
I_32
j9bcutil_createAndVerifyJImageLocation(J9PortLibrary *portlib, J9JImage *jimage, const char *resourceName, void *imageLocation, J9JImageLocation *j9jimageLocation);

/**
 * Reads up a resource from the resources area in jimage file at offset J9JImageLocation->resourceOffset.
 * Amount of data to be read is given by J9JImageLocation->uncompressedSize.
 *
 * @param [in] portlib pointer to J9PortLibrary
 * @param [in] jimage pointer to J9JImage representing jimage file; must not be NULL
 * @param [in] j9jimageLocation pointer to J9JImageLocation containing metadata of the resource
 * @param [in] dataBuffer pointer to memory area where the resource data is to be copied; must have at least J9JImageLocation->uncompressedSize bytes of space
 * @param [in] dataBufferSize size of memory area pointed by buffer; must be atleast J9JImageLocation->uncompressedSize bytes
 *
 * @return if no error occurs then returns J9JIMAGE_NO_ERROR and dataBuffer contains the resource data, else returns a negative error code
 * Note: caller should ensure buffer is large enough to hold J9JImageLocation->uncompressedSize bytes of data.
 */
I_32
j9bcutil_getJImageResource(J9PortLibrary *portlib, J9JImage *jimage, J9JImageLocation *j9jimageLocation, void *dataBuffer, U_64 dataBufferSize);

/**
 * Returns name of the resource by concatenating module, parent, base and extension strings.
 *
 * @param [in] portlib pointer to J9PortLibrary
 * @param [in] jimage pointer to J9JImage
 * @param [in] module pointer to module string; may be NULL
 * @param [in] parent pointer to parent string; may be NULL
 * @param [in] base pointer to base string; should not be NULL
 * @param [in] extension pointer to extension string; may be NULL
 * @param [out] resourceName on success *resourceName points to \0-terminated name of the resource, on error *resourceName is set to NULL
 *
 * @return on success returns J9JIMAGE_NO_ERROR, otherwise returns negative error code
 *
 * @note caller is responsible for freeing *resourceName
 */
I_32
j9bcutil_getJImageResourceName(J9PortLibrary *portlib, J9JImage *jimage, const char *module, const char *parent, const char *base, const char *extension, char **resourceName);

/**
 * Finds the module for the given package.
 *
 * @param [in] portlib pointer to J9PortLibrary
 * @param [in] jimage pointer to J9JImage
 * @param [in] package name of the package in the format com/ibm/oti
 *
 * @return returns module name for the given package.
 *
 * Note: The char * returned the by this function points to jimage file mmapped by the jimagereader.
 * It should never be freed by the caller of this function.
 * It is expected that the mmapped region of jimage will be available for the lifetime of the classloader,
 * so the char * should also be valid for the lifetime of the classloader.
 */
const char *
j9bcutil_findModuleForPackage(J9PortLibrary *portlib, J9JImage *jimage, const char *package);

/**
 * Frees up native resources held by J9JImage.
 *
 * @param [in] portlib pointer to J9PortLibrary
 * @param [in] jimage pointer to J9JImage representing a jimage file
 *
 * returns void
 */
void
j9bcutil_unloadJImage(J9PortLibrary *portlib, J9JImage *jimage);

/* ---------------- jsrinliner.c ---------------- */


/**
* @brief
* @param classfile
* @return I_32
*/
I_32 
checkForJsrs (J9CfrClassFile * classfile);


/**
* @brief
* @param hasRET
* @param classfile
* @param inlineBuffers
* @return void
*/
void 
inlineJsrs (I_32 hasRET, J9CfrClassFile * classfile, J9JSRIData * inlineBuffers);


/**
* @brief
* @param inlineBuffers
* @return void
*/
void 
releaseInlineBuffers  (J9JSRIData * inlineBuffers);




/* ---------------- verifyerrstring.c ---------------- */

/**
* @brief
* @param *javaVM
* @param *error
* @param className
* @param classNameLength
* @return char *
*/
const char * 
buildVerifyErrorString ( J9JavaVM *javaVM, J9CfrError *error, U_8* className, UDATA classNameLength);


typedef struct J9TenantIsolationFilter {
	const U_8 *filter;		/**< The name or prefix to match */
	BOOLEAN startsWith; 		/**< Specifies whether or not we want the filter to only match the beginning of the string */
} J9TenantIsolationFilter;


#ifdef __cplusplus
}
#endif

#endif /* bcutil_api_h */

