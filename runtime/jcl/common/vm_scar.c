/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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

#include <string.h>
#ifdef WIN32
#include <windows.h>
#endif
#ifdef LINUX
#include <dlfcn.h>
#endif

#include "j9.h"
#include "j9consts.h"
#include "j9jclnls.h"
#include "jni.h"
#include "j9protos.h"
#include "jvminit.h"
/* Avoid renaming malloc/free */
#define LAUNCHERS
#include "../../j9vm/jvm.h"
#include "jclprots.h"
#include "omrlinkedlist.h"
#include "ut_j9jcl.h"

#include "j2sever.h"

#include "sunvmi_api.h"
#include "jcl_internal.h"

#define OPT_XJCL_COLON "-Xjcl:"
#define JAVA_ASSISTIVE_STR "JAVA_ASSISTIVE"
#define JAVA_FONTS_STR "JAVA_FONTS"
#define OFFLOAD_PREFIX "offload_"

#if defined(J9VM_JCL_SE7_BASIC)
#define J9_DLL_NAME J9_JAVA_SE_7_BASIC_DLL_NAME
#elif defined(J9VM_JCL_SE9)
#define J9_DLL_NAME J9_JAVA_SE_9_DLL_NAME
#else
#error Unknown J9VM_JCL_SE
#endif

/* this file is owned by the VM-team.  Please do not modify it without consulting the VM team */

#if defined(LINUX) || defined(RS6000) || defined(J9ZOS390)
#define J9VM_UNIX
#endif

#ifdef J9VM_OPT_DYNAMIC_LOAD_SUPPORT

/* Maximum number of entries the classlib.properties file. */
#define MAX_PROPSFILE_BOOTPATH_ENTRIES 64

/* Reserved for entries added by addVMSpecificDirectories(). */
#define NUM_RESERVED_BOOTPATH_ENTRIES 4

/* Maximum entries on the bootpath. */
#define MAX_BOOTPATH_ENTRIES (MAX_PROPSFILE_BOOTPATH_ENTRIES+NUM_RESERVED_BOOTPATH_ENTRIES)

/*
 * Array of bootpath entries, NULL-terminated.
 * Entries which begin with '!' have been allocated on the heap and are
 * expected to be freed by getDefaultBootstrapClassPath() in bpinit.c.
 */
const char* jclBootstrapClassPath[MAX_BOOTPATH_ENTRIES+1]; /* +1 for null-term */

/* Array of memory-allocated bootpath entries, NULL-terminated. 
 * We need this because in the jclBootstrapClassPath array some entries are 
 * allocated and some are statically declared and this second arrays keeps track
 * of which ones were allocated so that we can free them when appropriate */
char* jclBootstrapClassPathAllocated[MAX_BOOTPATH_ENTRIES+1] = {NULL}; /* +1 for null-term */

/* Allocated memory to store string read from classlib.properties file. */
char *iniBootpath = NULL;

#endif

const U_64 jclConfig = J9CONST64(0x7363617237306200);		/* 'scar70b' */


jint scarInit(J9JavaVM *vm);
jint scarPreconfigure(J9JavaVM *vm);
static UDATA addBFUSystemProperties(J9JavaVM* javaVM);
static IDATA addVMSpecificDirectories(J9JavaVM *vm, UDATA *cursor, char *subdirName);
static IDATA loadClasslibPropertiesFile(J9JavaVM *vm, UDATA *cursor);
static void setFatalErrorStringInDLLTableEntry(J9JavaVM* vm, char *errorString);

jint
JNICALL JVM_OnLoad(JavaVM * jvm, char *options, void *reserved)
{
	return 0;
}


/**
 * Add additional system properties required by the Sun class library.
 * @param javaVM
 * @return J9SYSPROP_ERROR_NONE on success, or a J9SYSPROP_ERROR_* value on failure.
 */
static UDATA
addBFUSystemProperties(J9JavaVM* javaVM)
{
	int fontPathSize = 0;
	char* fontPathBuffer = "";
	char* propValue;
	UDATA rc;
	const J9InternalVMFunctions* vmfunc = javaVM->internalVMFunctions;

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	if ( (fontPathSize = (int)j9sysinfo_get_env(JAVA_FONTS_STR, NULL, 0)) > 0 ) {
		fontPathBuffer = j9mem_allocate_memory(fontPathSize, J9MEM_CATEGORY_VM_JCL);
		if (fontPathBuffer) {
			javaVM->jclSysPropBuffer = fontPathBuffer;		/* Use jclSysPropBuffer for all allocated sys prop values */
			j9sysinfo_get_env(JAVA_FONTS_STR, fontPathBuffer, fontPathSize);
		}
	}
	
#ifdef WIN32
	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "java.awt.graphicsenv", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "java.awt.graphicsenv", "sun.awt.Win32GraphicsEnvironment", 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}

	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "java.awt.fonts", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "java.awt.fonts", fontPathBuffer, 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}

	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "awt.toolkit", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "awt.toolkit", "sun.awt.windows.WToolkit", 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}

	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "java.awt.printerjob", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "java.awt.printerjob", "sun.awt.windows.WPrinterJob", 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}
#endif /* WIN32 */

#ifdef J9VM_UNIX
	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "java.awt.graphicsenv", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "java.awt.graphicsenv", "sun.awt.X11GraphicsEnvironment", 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}

	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "java.awt.fonts", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "java.awt.fonts", "", 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}

#if defined(J9ZOS390)
	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "sun.security.policy.utf8", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "sun.security.policy.utf8", "false", 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}
#endif

	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "awt.toolkit", NULL)) {
#if defined(J9ZTPF)
		rc = vmfunc->addSystemProperty(javaVM, "awt.toolkit", "sun.awt.HToolkit", 0);
#else /* defined(J9ZTPF) */
		rc = vmfunc->addSystemProperty(javaVM, "awt.toolkit", "sun.awt.X11.XToolkit", 0);
#endif /* defined(J9ZTPF) */
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}

	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "java.awt.printerjob", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "java.awt.printerjob", "sun.print.PSPrinterJob", 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}
	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "java.util.prefs.PreferencesFactory", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "java.util.prefs.PreferencesFactory", "java.util.prefs.FileSystemPreferencesFactory", 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}
	if (fontPathSize >= 0) {
		if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "sun.java2d.fontpath", NULL)) {
			rc = vmfunc->addSystemProperty(javaVM, "sun.java2d.fontpath", fontPathBuffer, 0);
			if (J9SYSPROP_ERROR_NONE != rc) {
				return rc;
			}
		}
	}
#endif /* J9VM_UNIX */

	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "sun.arch.data.model", NULL)) {
#if defined(J9VM_ENV_DATA64)
		propValue = "64";
#else
		propValue = "32";
#endif
		rc = vmfunc->addSystemProperty(javaVM, "sun.arch.data.model", propValue, 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}

#ifdef J9VM_ENV_LITTLE_ENDIAN
	propValue = "UnicodeLittle";
#else
	propValue = "UnicodeBig";
#endif
	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "sun.io.unicode.encoding", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "sun.io.unicode.encoding", propValue, 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}

#if defined(LINUX)
	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "sun.desktop", NULL)) {
		char tempString[2];
		/* We don't care about the value of the env var, just whether it is not null */
		if (j9sysinfo_get_env("GNOME_DESKTOP_SESSION_ID", tempString, 2) != -1) {
			rc = vmfunc->addSystemProperty(javaVM, "sun.desktop", "gnome", 0);
			if (J9SYSPROP_ERROR_NONE != rc) {
				return rc;
			}
		}
	}
#endif /* LINUX */

#if defined(WIN32)
	if (J9SYSPROP_ERROR_NOT_FOUND == vmfunc->getSystemProperty(javaVM, "sun.desktop", NULL)) {
		rc = vmfunc->addSystemProperty(javaVM, "sun.desktop", "windows", 0);
		if (J9SYSPROP_ERROR_NONE != rc) {
			return rc;
		}
	}
#endif
	
	return J9SYSPROP_ERROR_NONE;
}


jint
scarInit(J9JavaVM * vm)
{
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	UDATA handle;
	jint result;
#if defined(WIN32)
	char *dbgwrapperStr = NULL;
	UDATA jclVersion = J2SE_VERSION(vm) & J2SE_VERSION_MASK;
#endif

	/* We need to overlay java.dll functions */
	result = (jint)vmFuncs->registerBootstrapLibrary( vm->mainThread, J9_DLL_NAME, (J9NativeLibrary**)&handle, FALSE);
	if( result != 0 ) {
		return result;
	}
	((J9NativeLibrary*)handle)->flags |= J9NATIVELIB_FLAG_ALLOW_INL;

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
	if (0 != ((J9NativeLibrary*)handle)->doSwitching) {
		Assert_JCL_Natives_Not_Offloaded();
		/* Trace engine may not be running at this point, refuse to load */
		return 1;
	}
#endif

#if defined(WIN32)
	switch (jclVersion) {
	case J2SE_17:
		dbgwrapperStr = "dbgwrapper70";
		break;
	case J2SE_18:
	case J2SE_19:	/* This will need to be modified when the JCL team provides a dbgwrapper90 */
		dbgwrapperStr = "dbgwrapper80";
		break;
	default:
		Assert_JCL_unreachable();
	}

	/* Preload this dependency from the JVM directories so the system does not load it from system path, as the wrong version could be found. */
	result = (jint)vmFuncs->registerBootstrapLibrary( vm->mainThread, dbgwrapperStr, (J9NativeLibrary**)&handle, FALSE);
	if (0 != result) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		/* If library can not be found, print a warning and proceed anyway as the dependency on this library may have been removed. */
		j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JCL_WARNING_DLL_COULDNOT_BE_REGISTERED_AS_BOOTSTRAP_LIB, dbgwrapperStr, result);
	}
#endif /* defined(WIN32) */

	
#if defined(J9VM_INTERP_MINIMAL_JCL)
	result = standardInit(vm, J9_DLL_NAME);
#else /* J9VM_INTERP_MINIMAL_JCL */
	result = (jint)vmFuncs->registerBootstrapLibrary( vm->mainThread, "java", (J9NativeLibrary**)&handle, FALSE);

	if( result == 0 ) {
		result = managementInit( vm );
	}

	if (result == 0) {
		initializeReflection(vm);
		result = standardInit(vm, J9_DLL_NAME);
		if (result == 0) {
			preloadReflectWrapperClasses(vm);
		}
	}
#endif /* J9VM_INTERP_MINIMAL_JCL */
	return result;
}


IDATA
J9VMDllMain(J9JavaVM* vm, IDATA stage, void* reserved)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA returnVal = J9VMDLLMAIN_OK;
	IDATA result = 0;
	IDATA* returnPointer = &result;
	I_32 hookReturnValue = JNI_OK;
	I_32* returnValuePointer = &hookReturnValue;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	switch(stage) {
		case ALL_LIBRARIES_LOADED:
			returnVal = initializeUnsafeMemoryTracking(vm);
			if (returnVal != JNI_OK) {
				return J9VMDLLMAIN_FAILED;
			}

			/* TODO: Can this be removed? */
			vm->jclFlags |=
				J9_JCL_FLAG_REFERENCE_OBJECTS | 
				J9_JCL_FLAG_FINALIZATION | 
				J9_JCL_FLAG_THREADGROUPS;
			vm->jclSysPropBuffer = NULL;
			
			/* give outside modules a crack at doing the preconfigure.  If there is no hook then 
			 * run scarPreconfigure directly */
			TRIGGER_J9HOOK_VM_SCAR_PRECONFIGURE(vm->hookInterface, vm, returnValuePointer, returnPointer);
			if (result == 0 ){	
				/* no hook ran so call scarPreconfigure directly */
				if (scarPreconfigure(vm) != JNI_OK) {
					returnVal = J9VMDLLMAIN_FAILED;
					break;
				}
			} else {
				/* return the result from the hook */
				if (hookReturnValue != JNI_OK){
					returnVal = J9VMDLLMAIN_FAILED;
					break;
				}
			}

			break;

		case ALL_VM_ARGS_CONSUMED :
			FIND_AND_CONSUME_ARG(STARTSWITH_MATCH, OPT_XJCL_COLON, NULL);
			break;

		case JCL_INITIALIZED :
			returnVal = SunVMI_LifecycleEvent(vm, JCL_INITIALIZED, NULL);
			if (J9VMDLLMAIN_OK != returnVal) {
				break;
			}

			result = (IDATA) scarInit(vm);
			if (JNI_OK == result) {
				result = (IDATA) completeInitialization(vm);
			}
			if (JNI_OK != result) {
				J9VMThread *vmThread = vm->mainThread;

				if ((NULL != vmThread->currentException) || (NULL == vmThread->threadObject)) {
					vmFuncs->internalEnterVMFromJNI(vmThread);
					vmFuncs->internalExceptionDescribe(vmThread);
					vmFuncs->internalReleaseVMAccess(vmThread);
					returnVal = J9VMDLLMAIN_SILENT_EXIT_VM;
				} else {
					returnVal = J9VMDLLMAIN_FAILED;
				}
			}
			break;
			
		case VM_INITIALIZATION_COMPLETE :
			returnVal = SunVMI_LifecycleEvent(vm, VM_INITIALIZATION_COMPLETE, NULL);
			break;

		case LIBRARIES_ONUNLOAD :
			if (vm->jclSysPropBuffer) {
				j9mem_free_memory(vm->jclSysPropBuffer);
			}
			managementTerminate( vm );
			if (JCL_OnUnload(vm, NULL) != JNI_OK) {
				returnVal = J9VMDLLMAIN_FAILED;
			}
			if (NULL != iniBootpath) {
				j9mem_free_memory(iniBootpath);
				iniBootpath = NULL;
			}
			freeUnsafeMemory(vm);
			break;
			
		case OFFLOAD_JCL_PRECONFIGURE:
			if (scarPreconfigure(vm) != JNI_OK) {
				returnVal = J9VMDLLMAIN_FAILED;
			}
			break;

		case INTERPRETER_SHUTDOWN:
			break;
	}

	return returnVal;
}

/**
 * Helper function which looks up the entry for this DLL
 * in the VM's table and sets the fatalErrorString specified
 * if the entry is found.
 *
 * @param[in] vm			A pointer to the Java VM
 * @param[in] errorString	The error string to set in the VM's dll entry
 */
static void
setFatalErrorStringInDLLTableEntry(J9JavaVM* vm, char *errorString)
{
	J9VMDllLoadInfo* loadInfo = FIND_DLL_TABLE_ENTRY( J9_DLL_NAME );
	if (NULL != loadInfo) {
		loadInfo->fatalErrorStr = errorString;
	}
}

jint
scarPreconfigure(J9JavaVM * vm)
{
	/* There are a fixed number of entries in jclBootstrapClassPath, ensure that you don't exceed the maximum number */
	UDATA i = 0;
	IDATA rc = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (J9SYSPROP_ERROR_NONE != addBFUSystemProperties(vm)) {
		return JNI_ERR;
	}

	if (J2SE_VERSION(vm) < J2SE_19) {
		char *vmSpecificDirectoryPath = "jclSC180";

		rc = addVMSpecificDirectories(vm, &i, vmSpecificDirectoryPath);
		if (0 != rc) {
			goto fail;
		}

		rc = loadClasslibPropertiesFile(vm, &i);
		if (rc <= 0) {
			/* loadClasslibPropertiesFile can't find any bootpath from classlib.properties, bail. */
			goto error;
		}

		Assert_JCL_true(i <= MAX_BOOTPATH_ENTRIES);
	}

	/* Terminate the path entries with a NULL */
	jclBootstrapClassPath[i] = NULL;

	/* Call the usual preconfigure routine */
	return standardPreconfigure((JavaVM*)vm);

error:
	rc = JNI_ERR;

fail:
	/* clean up already allocated entries */
	while (i > 0) {
		char * entry = NULL;

		i -= 1;
		entry = jclBootstrapClassPathAllocated[i];

		if (NULL != entry) {
			j9mem_free_memory(entry);
		} else {
			entry = (char *)jclBootstrapClassPath[i];

			if ((NULL != entry) && ('!' == entry[0])) {
				j9mem_free_memory(entry);
			}
		}

		jclBootstrapClassPath[i] = NULL;
		jclBootstrapClassPathAllocated[i] = NULL;
	}

	return (jint)rc;
}


/**
 * Loads the classlib.properties file and initialize the bootstrap
 * classpath based on data found there.
 *
 * @param vm
 * @param cursor
 *
 * @return Number of entries added on success, negative error code on failure (-1 on malloc failure, -2 on too many entries).
 * @note Allocates the iniBootpath global and fills in the jclBootstrapClassPath global.
 */
static IDATA
loadClasslibPropertiesFile(J9JavaVM *vm, UDATA *cursor)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	char propsPath[EsMaxPath];
	j9props_file_t classlibProps;
	const char* bootpath = NULL;
	UDATA startCount = *cursor;
	UDATA count = 0;

#define RELATIVE_PROPSPATH DIR_SEPARATOR_STR"lib"DIR_SEPARATOR_STR"classlib.properties"

	j9str_printf(PORTLIB, propsPath, sizeof(propsPath), "%s"RELATIVE_PROPSPATH, vm->javaHome);

#undef RELATIVE_PROPSPATH

	classlibProps = props_file_open(PORTLIB, propsPath, NULL, 0);
	if (NULL == classlibProps) {
		/* File not found is not an error. */
		return 0;
	}
	
	bootpath = props_file_get(classlibProps, "bootpath");
	if ((NULL != bootpath) && ('\0' != bootpath[0])) {
		char* currentEntry = NULL;
		UDATA pathLength = strlen(bootpath);
		UDATA i = 0;
		char c = '\0';

		iniBootpath = j9mem_allocate_memory(pathLength + 1, J9MEM_CATEGORY_VM_JCL);
		if (NULL == iniBootpath) {
			j9tty_printf(PORTLIB, "Error: Could not allocate memory for classlib.properties file.\n");
			return -1;
		}

		currentEntry = iniBootpath;
		do {
			c = bootpath[i];
			if ((':' == c) || ('\0' == c)) {
				iniBootpath[i] = '\0';
				/* Add the new entry if it's not empty. */
				if ('\0' != *currentEntry) {
					if (MAX_PROPSFILE_BOOTPATH_ENTRIES <= count) {
						j9tty_printf(PORTLIB, "Error: MAX_PROPSFILE_BOOTPATH_ENTRIES (%d) exceeded in classlib.properties file.\n", MAX_PROPSFILE_BOOTPATH_ENTRIES);
						j9mem_free_memory(iniBootpath);
						iniBootpath = NULL;
						return -2;
					}
					jclBootstrapClassPath[startCount + count] = currentEntry;
					count += 1;
				}
				currentEntry = &iniBootpath[i + 1];
			} else {
				iniBootpath[i] = c;
			}
			i += 1;
		} while ('\0' != c);

		(*cursor) = startCount + count;
	}

	props_file_close(classlibProps);

	return (IDATA)count;
}

static IDATA
addVMSpecificDirectories(J9JavaVM *vm, UDATA *cursor, char * subdirName)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	int javaHomePathLength = (int)strlen((char*)vm->javaHome);
	int j2seRootPathLength = (NULL != vm->j2seRootDirectory) ? (int)strlen(vm->j2seRootDirectory) : 0;

	/* NOTE: You must increment NUM_RESERVED_BOOTPATH_ENTRIES if you add new classpath entries here. */

#define VM_JAR "vm.jar"

	if ((NULL != vm->j2seRootDirectory) && (J2SE_LAYOUT(vm) & J2SE_LAYOUT_VM_IN_SUBDIR)) {
		/* 3 +1s from: dir sep, !, dir sep*/
		int subDirPathLength = 1 + j2seRootPathLength + 1 + (int)strlen(subdirName) + 1;
		/* +1 for NUL-terminator */
		char *vmJarPath = NULL;
		int vmJarLen = LITERAL_STRLEN(VM_JAR);
		vmJarPath = j9mem_allocate_memory(subDirPathLength + vmJarLen + 1, J9MEM_CATEGORY_VM_JCL);
		if (NULL == vmJarPath) {
			setFatalErrorStringInDLLTableEntry(vm, "failed to allocate memory for vm jar path");
			return JNI_ENOMEM;
		}

		strcpy(vmJarPath, "!");
		strcat(vmJarPath, vm->j2seRootDirectory);
		strcat(vmJarPath, DIR_SEPARATOR_STR);
		strcat(vmJarPath, subdirName);
		strcat(vmJarPath, DIR_SEPARATOR_STR);
		strcat(vmJarPath, VM_JAR);
		jclBootstrapClassPath[(*cursor)++] = vmJarPath;
	} else {
		jclBootstrapClassPath[(*cursor)++] = VM_JAR;
	}

#define SE_SERVICE_JAR "se-service.jar"
	{
		int serviceJarPathLength = 1 + javaHomePathLength + 1 + LITERAL_STRLEN("lib") + 1 + LITERAL_STRLEN(SE_SERVICE_JAR) + 1;
		char * serviceJarPath = j9mem_allocate_memory(serviceJarPathLength, J9MEM_CATEGORY_VM_JCL);
		if (NULL == serviceJarPath) {
			setFatalErrorStringInDLLTableEntry(vm, "failed to allocate memory for service jar path");
			return JNI_ENOMEM;
		}
		strcpy(serviceJarPath, "!");
		strcat(serviceJarPath, (char*)vm->javaHome);
		strcat(serviceJarPath, DIR_SEPARATOR_STR);
		strcat(serviceJarPath, "lib");
		strcat(serviceJarPath, DIR_SEPARATOR_STR);
		strcat(serviceJarPath, SE_SERVICE_JAR);
		jclBootstrapClassPath[(*cursor)++] = serviceJarPath;
	}

#if defined(J9VM_OPT_CUDA)
#define CUDA4J_JAR "cuda4j.jar"
	if (NULL != vm->j2seRootDirectory) {
		/* Format: '!' + vm->javaHome + '/' + 'lib' + '/' + 'cuda4j.jar' + NUL-terminator */
		int cuda4jPathLength = 1 + javaHomePathLength + 1 + LITERAL_STRLEN("lib") + 1 + LITERAL_STRLEN(CUDA4J_JAR) + 1;
		char * cuda4jJarPath = j9mem_allocate_memory(cuda4jPathLength, J9MEM_CATEGORY_VM_JCL);
		if (NULL == cuda4jJarPath) {
			setFatalErrorStringInDLLTableEntry(vm, "failed to allocate memory for cuda4j jar path");
			return JNI_ENOMEM;
		}
		strcpy(cuda4jJarPath, "!");
		strcat(cuda4jJarPath, (char*)vm->javaHome);
		strcat(cuda4jJarPath, DIR_SEPARATOR_STR);
		strcat(cuda4jJarPath, "lib");
		strcat(cuda4jJarPath, DIR_SEPARATOR_STR);
		strcat(cuda4jJarPath, CUDA4J_JAR);
		jclBootstrapClassPath[(*cursor)++] = cuda4jJarPath;
	}
#undef CUDA4J_JAR
#endif /* J9VM_OPT_CUDA */

	return 0;
}
