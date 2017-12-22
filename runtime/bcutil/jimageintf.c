/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#include "bcutil_api.h"
#include "j9vmnls.h"
#include "jimagereader.h"
#include "jniport.h"
#include "jvminit.h"
#include "libjimage.h"
#include "ut_j9bcu.h"
#include "vm_api.h"

static I_32 maplibJImageToJImageIntfError(I_32 error);
static I_32 lookupSymbolsInJImageLib(J9PortLibrary *portLibrary, UDATA libJImageHandle);
static UDATA loadJImageLib(J9JavaVM *vm);
static I_32 initJImageIntfCommon(J9JImageIntf **jimageIntf, J9JavaVM *vm, J9PortLibrary *portLibrary, UDATA libJImageHandle);


static libJImageOpenType libJImageOpen;
static libJImageCloseType libJImageClose;
static libJImageFindResourceType libJImageFindResource;
static libJImageGetResourceType libJImageGetResource;
static libJImagePackageToModuleType libJImagePackageToModule;

/**
 * Maps error code returned by libjimage library to internal error code of jimage interface.
 *
 * @param [in] error error code returned by libjimage library
 *
 * @return error code used by jimage interface
 */
static I_32
maplibJImageToJImageIntfError(I_32 error) {
	switch(error) {
	case JIMAGE_BAD_MAGIC:
		return J9JIMAGE_BAD_MAGIC;
	case JIMAGE_BAD_VERSION:
		return J9JIMAGE_BAD_VERSION;
	case JIMAGE_CORRUPTED:
		return J9JIMAGE_CORRUPTED;
	case JIMAGE_NOT_FOUND:
		return J9JIMAGE_RESOURCE_NOT_FOUND;
	default:
		return J9JIMAGE_UNKNOWN_ERROR;
	}
}

/**
 * Looks up symbols in jimage library.
 *
 * @param [in] portLibrary Pointer to J9PortLibrary
 * @param [in] libJImageHandle handle to jimage library
 *
 * @return 0 on success; any other value on failure
 */
static I_32
lookupSymbolsInJImageLib(J9PortLibrary *portLibrary, UDATA libJImageHandle)
{
	I_32 rc = 0;
	PORT_ACCESS_FROM_PORT(portLibrary);

	/* lookup functions in jimage library */
	rc = (I_32)j9sl_lookup_name(libJImageHandle, "JIMAGE_Open", (UDATA *)&libJImageOpen, "LLI");
	if (0 != rc) {
		j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JIMAGE_INTF_LIBJIMAGE_SYMBOL_LOOKUP_FAILED, "JIMAGE_Open");
		goto _end;
	}
	rc = (I_32)j9sl_lookup_name(libJImageHandle, "JIMAGE_Close", (UDATA *)&libJImageClose, "VL");
	if (0 != rc) {
		j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JIMAGE_INTF_LIBJIMAGE_SYMBOL_LOOKUP_FAILED, "JIMAGE_Close");
		goto _end;
	}
	rc = (I_32)j9sl_lookup_name(libJImageHandle, "JIMAGE_FindResource", (UDATA *)&libJImageFindResource, "JLLLLL");
	if (0 != rc) {
		j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JIMAGE_INTF_LIBJIMAGE_SYMBOL_LOOKUP_FAILED, "JIMAGE_FindResource");
		goto _end;
	}
	rc = (I_32)j9sl_lookup_name(libJImageHandle, "JIMAGE_GetResource", (UDATA *)&libJImageGetResource, "JLJLJ");
	if (0 != rc) {
		j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JIMAGE_INTF_LIBJIMAGE_SYMBOL_LOOKUP_FAILED, "JIMAGE_GetResource");
		goto _end;
	}
	rc = (I_32)j9sl_lookup_name(libJImageHandle, "JIMAGE_PackageToModule", (UDATA *)&libJImagePackageToModule, "LLL");
	if (0 != rc) {
		j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JIMAGE_INTF_LIBJIMAGE_SYMBOL_LOOKUP_FAILED, "JIMAGE_PackageToModule");
		goto _end;
	}

_end:
	return rc;
}

/**
 * Loads jimage library and looks up functions.
 *
 * @param [in] vm Pointer to J9JavaVM
 *
 * @return handle to the library if successfully loaded and all functions lookups succeed, 0 otherwise
 */
static UDATA
loadJImageLib(J9JavaVM *vm)
{
	J9VMDllLoadInfo jimageLoadInfo = {0};
	UDATA handle = 0;
	I_32 rc = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);

	jimageLoadInfo.loadFlags |= (XRUN_LIBRARY | SILENT_NO_DLL);
	strncpy((char *) &jimageLoadInfo.dllName, JIMAGE_LIBRARY_NAME, sizeof(JIMAGE_LIBRARY_NAME));
	if (J9_VM_FUNCTION_VIA_JAVAVM(vm, loadJ9DLL)(vm, &jimageLoadInfo) != TRUE) {
		/* Disable this message for time being as the libjimage.so is not present in espresso builds */
#if 0
		j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JIMAGE_INTF_LIBJIMAGE_OPEN_FAILED);
#endif /* if 0 */
	} else {
		handle = jimageLoadInfo.descriptor;
		rc = lookupSymbolsInJImageLib(PORTLIB, handle);
		if (0 != rc) {
			j9sl_close_shared_library(handle);
			handle = 0;
		}
	}
	return handle;
}

/**
 * Initializes J9JImageIntf and sets the function pointers in it.
 *
 * @param [out] jimageIntf On successful return *jimageIntf points to a valid J9JImageIntf
 * @param [in] vm Pointer to J9JavaVM
 * @param [in] portLibrary Pointer to J9PortLibrary
 * @param [in] libJImageHandle handle to jimage library
 *
 * @return J9JIMAGE_NO_ERROR if J9JImageIntf is successfully initialized, a negative error code on failure.
 */
static I_32
initJImageIntfCommon(J9JImageIntf **jimageIntf, J9JavaVM *vm, J9PortLibrary *portLibrary, UDATA libJImageHandle)
{
	J9JImageIntf *intf = NULL;
	I_32 rc = J9JIMAGE_NO_ERROR;
	PORT_ACCESS_FROM_PORT(portLibrary);

	intf = j9mem_allocate_memory(sizeof(J9JImageIntf), J9MEM_CATEGORY_CLASSES);
	if (NULL != intf) {
		intf->vm = vm;
		intf->portLib = portLibrary;
		intf->libJImageHandle = libJImageHandle;

		intf->jimageOpen = jimageOpen;
		intf->jimageClose = jimageClose;
		intf->jimageFindResource = jimageFindResource;
		intf->jimageFreeResourceLocation = jimageFreeResourceLocation;
		intf->jimageGetResource = jimageGetResource;
		intf->jimagePackageToModule = jimagePackageToModule;

		 *jimageIntf = intf;
	} else {
		*jimageIntf = NULL;
		rc = J9JIMAGE_OUT_OF_MEMORY;
	}

	return rc;
}

I_32
initJImageIntf(J9JImageIntf **jimageIntf, J9JavaVM *vm, J9PortLibrary *portLibrary)
{
	J9JImageIntf *intf = NULL;
	UDATA libJImageHandle = 0;
	IDATA argIndex1 = -1;
	IDATA argIndex2 = -1;
	I_32 rc = J9JIMAGE_NO_ERROR;
	PORT_ACCESS_FROM_PORT(portLibrary);

	Trc_BCU_Assert_True(NULL != jimageIntf);

	/* Check for -XX:+UseJ9JImageReader and -XX:-UseJ9JImageReader; whichever comes later wins. */
	argIndex1 = FIND_ARG_IN_VMARGS(EXACT_MATCH, VMOPT_XXUSEJ9JIMAGEREADER, NULL);
	argIndex2 = FIND_ARG_IN_VMARGS(EXACT_MATCH, VMOPT_XXNOUSEJ9JIMAGEREADER, NULL);

	if (argIndex1 > argIndex2) {
		vm->extendedRuntimeFlags |= (UDATA)J9_EXTENDED_RUNTIME_USE_J9JIMAGE_READER;
	}

	if (J9_ARE_NO_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_USE_J9JIMAGE_READER)) {
		libJImageHandle = loadJImageLib(vm);
		if (0 == libJImageHandle) {
			rc = J9JIMAGE_LIBJIMAGE_LOAD_FAILED;
			Trc_BCU_initJImageIntf_libJImageFailed();
			goto _end;
		} else {
			if (0 != (vm->verboseLevel & VERBOSE_DYNLOAD)) {
				j9tty_printf(PORTLIB, "JImage interface is using jimage library\n");
			}
			Trc_BCU_initJImageIntf_usingLibJImage();
		}
	} else {
		if (0 != (vm->verboseLevel & VERBOSE_DYNLOAD)) {
			j9tty_printf(PORTLIB, "JImage interface is using internal implementation of jimage reader\n");
		}
		Trc_BCU_initJImageIntf_usingJ9JImageReader();
	}

	rc = initJImageIntfCommon(jimageIntf, vm, portLibrary, libJImageHandle);

_end:
	return rc;
}

I_32
initJImageIntfWithLibrary(J9JImageIntf **jimageIntf, J9PortLibrary *portLibrary, const char *libjimage)
{
	UDATA libJImageHandle = 0;
	I_32 rc = J9JIMAGE_NO_ERROR;
	PORT_ACCESS_FROM_PORT(portLibrary);

	Trc_BCU_Assert_True(NULL != jimageIntf);

	rc = (I_32)j9sl_open_shared_library((char *)libjimage, &libJImageHandle, 0);
	if (0 == rc) {
		rc = lookupSymbolsInJImageLib(PORTLIB, libJImageHandle);
		if (0 != rc) {
			j9sl_close_shared_library(libJImageHandle);
			libJImageHandle = 0;
			rc = J9JIMAGE_LIBJIMAGE_LOAD_FAILED;
		} else {
			rc = initJImageIntfCommon(jimageIntf, NULL /* vm */, portLibrary, libJImageHandle);
		}
	} else {
		rc = J9JIMAGE_LIBJIMAGE_LOAD_FAILED;
	}

	return rc;
}

void
closeJImageIntf(J9JImageIntf *jimageIntf) {
	PORT_ACCESS_FROM_PORT(jimageIntf->portLib);

	if (jimageIntf->libJImageHandle) {
		j9sl_close_shared_library(jimageIntf->libJImageHandle);
	}
	j9mem_free_memory(jimageIntf);
}

I_32
jimageOpen(J9JImageIntf *jimageIntf, const char *name, UDATA *handle)
{
	I_32 rc = J9JIMAGE_NO_ERROR;
	PORT_ACCESS_FROM_PORT(jimageIntf->portLib);

	Trc_BCU_Assert_True(NULL != handle);

	if (jimageIntf->libJImageHandle) {
		I_32 error = 0;

		JImageFile *jimageFile = libJImageOpen(name, &error);
		if (NULL == jimageFile) {
			if (error > 0) {
				/* Some operating system error */
				rc = J9JIMAGE_INTERNAL_ERROR;
			} else {
				rc = maplibJImageToJImageIntfError(error);
			}
		} else {
			*handle = (UDATA)jimageFile;
		}
	} else {
		J9JImage *j9jimage = NULL;

		rc = j9bcutil_loadJImage(PORTLIB, name, &j9jimage);
		if (J9JIMAGE_NO_ERROR == rc) {
			*handle = (UDATA)j9jimage;
		}
	}
	return rc;
}

void
jimageClose(J9JImageIntf *jimageIntf, UDATA handle)
{
	PORT_ACCESS_FROM_PORT(jimageIntf->portLib);

	if (jimageIntf->libJImageHandle) {
		JImageFile *jimage = (JImageFile *)handle;
		libJImageClose(jimage);
	} else {
		J9JImage *jimage = (J9JImage *)handle;
		j9bcutil_unloadJImage(PORTLIB, jimage);
	}
}

I_32
jimageFindResource(J9JImageIntf *jimageIntf, UDATA handle, const char *moduleName, const char* name, UDATA *resourceLocation, I_64 *size)
{
	I_32 rc = J9JIMAGE_NO_ERROR;
	PORT_ACCESS_FROM_PORT(jimageIntf->portLib);

	Trc_BCU_Assert_True(NULL != resourceLocation);
	Trc_BCU_Assert_True(NULL != size);

	if (jimageIntf->libJImageHandle) {
		JImageFile *jimage = (JImageFile *)handle;
		JImageLocationRef *locationRef = (JImageLocationRef *)j9mem_allocate_memory(sizeof(JImageLocationRef), J9MEM_CATEGORY_CLASSES);;

		if (NULL != locationRef) {
			*locationRef = libJImageFindResource(jimage, moduleName, JIMAGE_VERSION_NUMBER, name, (jlong *)size);
			if (JIMAGE_NOT_FOUND != *(I_64 *)locationRef) {
				*resourceLocation = (UDATA)locationRef;
			} else {
				j9mem_free_memory(locationRef);
				rc = maplibJImageToJImageIntfError(JIMAGE_NOT_FOUND);
			}
		} else {
			rc = J9JIMAGE_OUT_OF_MEMORY;
		}
	} else {
		J9JImage *jimage = (J9JImage *)handle;
		J9JImageLocation *j9jimageLocation = j9mem_allocate_memory(sizeof(J9JImageLocation), J9MEM_CATEGORY_CLASSES);
		IDATA resourceNameLen = 1 + strlen(moduleName) + 1 + strlen(name) + 1; /* +1 for preceding '/' and, +1 for '/' between module and resource name, and +1 for \0 character */
		char *resourceName = j9mem_allocate_memory(resourceNameLen, J9MEM_CATEGORY_CLASSES);
		if ((NULL != j9jimageLocation) && (NULL != resourceName)) {
			j9str_printf(PORTLIB, resourceName, resourceNameLen, "/%s/%s", moduleName, name);
			rc = j9bcutil_lookupJImageResource(PORTLIB, jimage, j9jimageLocation, resourceName);
			j9mem_free_memory(resourceName);
			if (J9JIMAGE_NO_ERROR == rc) {
				*size = j9jimageLocation->uncompressedSize;
				*resourceLocation = (UDATA)j9jimageLocation;
			} else {
				j9mem_free_memory(j9jimageLocation);
			}
		} else {
			if (NULL != resourceName) {
				j9mem_free_memory(resourceName);
			}
			if (NULL != j9jimageLocation) {
				j9mem_free_memory(j9jimageLocation);
			}
			rc = J9JIMAGE_OUT_OF_MEMORY;
		}
	}

	return rc;
}

void
jimageFreeResourceLocation(J9JImageIntf *jimageIntf, UDATA handle, UDATA resourceLocation)
{
	PORT_ACCESS_FROM_PORT(jimageIntf->portLib);

	if (0 != resourceLocation) {
		if (jimageIntf->libJImageHandle) {

			j9mem_free_memory((JImageLocationRef *)resourceLocation);
		} else {
			j9mem_free_memory((J9JImageLocation *)resourceLocation);
		}
	}
}

I_32
jimageGetResource(J9JImageIntf *jimageIntf, UDATA handle, UDATA resourceLocation, char *buffer, I_64 bufferSize, I_64 *resourceSize)
{
	I_32 rc = J9JIMAGE_NO_ERROR;
	PORT_ACCESS_FROM_PORT(jimageIntf->portLib);

	Trc_BCU_Assert_True(NULL != buffer);

	if (jimageIntf->libJImageHandle) {
		JImageFile *jimage = (JImageFile *)handle;
		JImageLocationRef *location = (JImageLocationRef *)resourceLocation;
		I_64 size = libJImageGetResource(jimage, *location, buffer, bufferSize);

		if (bufferSize < size) {
			rc = J9JIMAGE_RESOURCE_TRUNCATED;
		}
		if (NULL != resourceSize) {
			*resourceSize = size;
		}
	} else {
		J9JImage *jimage = (J9JImage *)handle;
		J9JImageLocation *j9jimageLocation = (J9JImageLocation *)resourceLocation;

		rc = j9bcutil_getJImageResource(PORTLIB, jimage, j9jimageLocation, buffer, bufferSize);

		if ((J9JIMAGE_NO_ERROR == rc) || (J9JIMAGE_RESOURCE_TRUNCATED == rc)) {
			if (NULL != resourceSize) {
				*resourceSize = j9jimageLocation->uncompressedSize;
			}
		}
	}
	return rc;
}

const char *
jimagePackageToModule(J9JImageIntf *jimageIntf, UDATA handle, const char *packageName)
{
	PORT_ACCESS_FROM_PORT(jimageIntf->portLib);

	if (jimageIntf->libJImageHandle) {
		JImageFile *jimage = (JImageFile *)handle;
		return libJImagePackageToModule(jimage, packageName);
	} else {
		J9JImage *jimage = (J9JImage *)handle;
		return j9bcutil_findModuleForPackage(PORTLIB, jimage, packageName);
	}
}
