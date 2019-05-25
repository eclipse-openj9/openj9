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
#include <ctype.h>

#include "j9.h"
#include "j9consts.h"
#include "jni.h"
#include "j2sever.h"
#include "j9protos.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "vmaccess.h"

#ifdef BREW
#define MAX_PATH_SIZE 40
#else
#define MAX_PATH_SIZE 1024
#endif

/* used for System.loadLibrary() */
#if defined(WIN32) 
#define ALT_DIR_SEPARATOR '/'
#endif

#define MAXIMUM_MESSAGE_LENGTH          128
#define J9STATIC_ONLOAD                 "JNI_OnLoad_"
#define J9STATIC_ONLOAD_LENGTH          sizeof(J9STATIC_ONLOAD)
#define J9DYNAMIC_ONLOAD                "JNI_OnLoad"
#define J9DYNAMIC_ONUNLOAD              "JNI_OnUnload"
#define J9DYNAMIC_ONUNLOAD_LENGTH       sizeof(J9DYNAMIC_ONUNLOAD)

/** Function type called by openNativeLibrary. Usually this is classLoaderRegisterLibrary
 *
 * \param userData Opaque user data passed through by caller.
 * \param classLoader The ClassLoader being used to load the library.
 * \param logicalName The logical name of the library.
 * \param physicalName The physical name of the library.
 * \param userData Opaque data passed through to the openFunction.
 * \param libraryPtr Receives the J9NativeLibrary pointer, can be NULL.
 * \param errorBuffer Buffer to receive a descriptive error string.
 * \param bufferLength Opaque data passed through to the openFunction.
 * \param flags
 *
 * \return One of the status codes indicating what happened.
 *   LOAD_OK 					If the library is already loaded by this classLoader.
 *   LOAD_OK 					If the library was loaded successfully.
 *   LOAD_ERR_ALREADY_LOADED 	If library is already loaded by another classLoader.
 *   LOAD_ERR_OUT_OF_MEMORY		If metadata allocation fails.
 *   LOAD_ERR_NOT_FOUND			If the library was not found on the libraryPath.
 *   LOAD_ERR_INVALID			If the library was not found on the libraryPath.
 *   LOAD_ERR_JNI_ONLOAD_FAILED	Library was found but OnLoad() failed.
 */
typedef UDATA (*openFunc_t) (void *userData, J9ClassLoader *classLoader, const char *logicalName, char *physicalName, J9NativeLibrary** libraryPtr, char* errorBuffer, UDATA bufferLength, UDATA flags);

/**
 * Prototypes for locally defined functions.
 */
static UDATA classLoaderRegisterLibrary(void *voidVMThread, J9ClassLoader *classLoader, const char *logicalName, char *physicalName, J9NativeLibrary** libraryPtr, char* errBuf, UDATA bufLen, UDATA flags);
static BOOLEAN isAbsolutePath (const char *path);
static J9ClassLoader* findLoadedSharedLibrary(J9VMThread* vmThread, const char* sharedLibraryName, J9NativeLibrary** libraryPtr);
static void freeSharedLibrary(J9VMThread *vmThread, J9ClassLoader* classLoader, J9NativeLibrary* library);
static UDATA    openNativeLibrary (J9JavaVM* vm, J9ClassLoader * classLoader, const char * libName, char * libraryPath, J9NativeLibrary** libraryPtr, openFunc_t openFunction, void* userData, char* errorBuffer, UDATA bufferLength);
static char * getBootLibraryPath(JavaVMInitArgs *vmInitArgs);
static void reportError(char* errorBuffer, const char* message, UDATA bufferLength);

/* Callbacks used in library management */
static UDATA closeLibraryCallback(J9VMThread* vmThread, J9NativeLibrary* library);

#ifdef J9VM_THR_PREEMPTIVE
#define ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM) (omrthread_monitor_enter((javaVM)->classLoaderBlocksMutex))
#define RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM) (omrthread_monitor_exit((javaVM)->classLoaderBlocksMutex))
#else
#define ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM)
#define RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM)
#endif


/**
 * Called when a library is being closed, this function is stored as a pointer
 * in the J9NativeLibrary structure.
 * \param vmThread The current thread.
 * \param library The native library being closed.
 * \return Zero on success, non-zero otherwise.
 */
static UDATA
closeLibraryCallback(J9VMThread* vmThread, J9NativeLibrary* library)
{
	PORT_ACCESS_FROM_VMC(vmThread);

	if (library->handle != 0) {
		j9sl_close_shared_library(library->handle);
		library->handle = 0;
	}
	return 0;
}

/**
 * Triggers a lifecycle event named <tt>functionName</tt> in the specified library.
 * The function must conform to either of the following prototypes:
 *
 * 		jint (JNICALL *JNI_OnLoad)(JavaVM* vm, void* reserved)
 * 		void (JNICALL *JNI_OnUnload)(JavaVM* vm, void* reserved)
 * Technically, any of the lifecycle routines, namely JNI_OnLoad, JNI_OnLoad_<library>,
 * JNI_OnUnload, and JNI_OnUnload_<library> can be invoked via this routine. 
 *
 * \param vmThread The current thread.
 * \param library The native library.
 * \param functionName The name of the function to look up and dispatch.
 * \param defaultResult The default result to return.
 *
 * \return The value returned from the lifecycle function, or defaultResult on error. In case
 * a void-returning lifecycle event is resolved and invoked, it returns 0.
 */
static IDATA
sendLifecycleEventCallback(struct J9VMThread* vmThread, struct J9NativeLibrary* library, const char * functionName, IDATA defaultResult) 
{
	PORT_ACCESS_FROM_VMC(vmThread);
	IDATA result = defaultResult;
	jint (JNICALL *JNI_Load)(JavaVM* vm, void* reserved);

	Trc_VM_sendLifecycleEventCallback_Entry(vmThread, vmThread, library->handle, functionName, defaultResult);

	if (0 == j9sl_lookup_name(library->handle, (char*)functionName, (void*)&JNI_Load, "VLL")) {
		Trc_VM_sendLifecycleEventCallback_Event1(vmThread, functionName, library->handle);

		/* Check whether the return type for the callee lifecycle event is void; return 0 to indicate 
		 * success instead of an undefined return value.
		 */
		if (0 == strncmp(functionName, J9DYNAMIC_ONUNLOAD, J9DYNAMIC_ONUNLOAD_LENGTH)) {
			JNI_Load((JavaVM*)vmThread->javaVM, NULL);
			result = 0;
		} else {
			result = JNI_Load((JavaVM*)vmThread->javaVM, NULL);
		}
	}

	Trc_VM_sendLifecycleEventCallback_Exit(vmThread, result);

	return result;
}


/**
 *
 * \param vm
 * \param classLoader
 * \param libName The logical name of the library to scan for (e.g. 'awt')
 * \param libraryPath A list of directories to examine, separated by the classpath separator.
 * \param libraryPtr The native library pointer, can be null.
 * \param openFunction Function that will be called for each entry in the libraryPath.
 * \param userData Opaque data passed through to the openFunction.
 * \param errorBuffer Buffer to receive a descriptive error string.
 * \param bufferLength Opaque data passed through to the openFunction.
 * \return
 */
static UDATA   
openNativeLibrary(J9JavaVM* vm, J9ClassLoader * classLoader, const char * libName, char * libraryPath, J9NativeLibrary** libraryPtr, openFunc_t openFunction, void* userData, char* errorBuffer, UDATA bufferLength)
{
	UDATA result;
	/* systemClassLoader always uses immediate symbol resolution, others determined by vmargs or defaults. */
	UDATA lazy = ((classLoader != vm->systemClassLoader)
				  && (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_LAZY_SYMBOL_RESOLUTION)) ? J9PORT_SLOPEN_LAZY : 0;

#if defined(J9VM_INTERP_MINIMAL_JNI)
	char *fullPath = NULL;
	char *fullPathPtr = libName;
#else
	char fullPath[MAX_PATH_SIZE + 1];
	char *fullPathPtr = fullPath;
	UDATA fullPathBufferLength = MAX_PATH_SIZE;
	char c;
	char *search;
	UDATA expectedPathLength = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_VM_openNativeLibrary(libName, libraryPath ? libraryPath : "NULL");

	/* If the alternateJitDir is set and this is the JIT DLL, modify the library path */
	if ((NULL != vm->alternateJitDir) && (0 == strcmp(libName, J9_JIT_DLL_NAME))) {
		libraryPath = vm->alternateJitDir;
	}

	/* If a library path was specified and the library name is not an absolute path,
   * search all entries in the library path for the library */

	if (libraryPath && (!isAbsolutePath(libName))) {
		char classPathSeparator = (char) j9sysinfo_get_classpathSeparator();
		J9VMSystemProperty *classpathSeparatorProperty = NULL;

		/* update with value overridden by proxy if appropriate */
		getSystemProperty(vm, "path.separator", &classpathSeparatorProperty);
		if (classpathSeparatorProperty != NULL) {
			classPathSeparator = classpathSeparatorProperty->value[0];
		}

		for (;;) {
			UDATA pathLength;

			/* Find the class path separator */
			pathLength = 0;
			search = libraryPath;
			while ((c = *search++) != '\0') {
				if (c == classPathSeparator) break;
				pathLength++;
			}

			/* Ignore empty entries in the library path */
			if (pathLength) {
				/* Set the flag J9PORT_SLOPEN_NO_LOOKUP_MSG_FOR_NOT_FOUND to remove the call to FormatMessageW()
				 * That call takes 700K of memory on windows when called the first time.
				 * The error message created will be overwritten by the following reportError() call. */
				char *dirSeparator = "";
				if (libraryPath[pathLength - 1] != DIR_SEPARATOR) {
					dirSeparator = DIR_SEPARATOR_STR;
				}

				/* expectedPathLength - %.*s%s%s - +1 includes NUL terminator */
				expectedPathLength = pathLength + strlen(dirSeparator) + strlen(libName) + 1;
				if (expectedPathLength > fullPathBufferLength) {
					if (fullPath != fullPathPtr) {
						j9mem_free_memory(fullPathPtr);
					}
					fullPathPtr = j9mem_allocate_memory(expectedPathLength, J9MEM_CATEGORY_CLASSES);
					if (NULL == fullPathPtr) {
						return J9NATIVELIB_LOAD_ERR_OUT_OF_MEMORY;
					}
					fullPathBufferLength = expectedPathLength;
				}
				j9str_printf(PORTLIB, fullPathPtr, expectedPathLength, "%.*s%s%s", pathLength, libraryPath, dirSeparator, libName);
				result = openFunction(userData, classLoader, libName, fullPathPtr, libraryPtr, errorBuffer, bufferLength, lazy | J9PORT_SLOPEN_DECORATE | J9PORT_SLOPEN_NO_LOOKUP_MSG_FOR_NOT_FOUND);
				if(result == J9NATIVELIB_LOAD_ERR_NOT_FOUND) {
					result = openFunction(userData, classLoader, libName, fullPathPtr, libraryPtr, errorBuffer, bufferLength, lazy | J9PORT_SLOPEN_NO_LOOKUP_MSG_FOR_NOT_FOUND);
					if(result != J9NATIVELIB_LOAD_ERR_NOT_FOUND) {
						goto exit;
					}
				} else {
					/* J9NATIVELIB_LOAD_ERR_NOT_FOUND indicates an error opening the library, all others are immediate failure */
					goto exit;
				}
			}

			/* Not found - move on to next library path entry.  Fail if there are no more entries to try. */
			libraryPath += pathLength;
			if (!libraryPath[0]) {
				char *msg = "Not found in com.ibm.oti.vm.bootstrap.library.path";

				if (classLoader != vm->systemClassLoader) {
					msg = "Not found in java.library.path";
				}
				reportError(errorBuffer, msg, bufferLength);
				result = J9NATIVELIB_LOAD_ERR_NOT_FOUND;
				goto exit;
			}
			libraryPath += 1;
		}
	}
#endif

	/* No library path specified, just add the extension and try that */
	/* temp fix.  Remove the second openFunc call once apps like javah have bootLibraryPaths */
	result = openFunction(userData, classLoader, libName, (char *)libName, libraryPtr, errorBuffer, bufferLength, lazy);
	if(result == J9NATIVELIB_LOAD_ERR_NOT_FOUND) {
		result = openFunction(userData, classLoader, libName, (char *)libName, libraryPtr, errorBuffer, bufferLength, lazy | J9PORT_SLOPEN_DECORATE);
	}
	
exit:
	if (fullPath != fullPathPtr) {
		j9mem_free_memory(fullPathPtr);
	}
	return result;
}

/**
 * Copies first \c bufferLength-1 characters from \c message into the \c
 * errorBuffer and null terminate the buffer.  NOP if bufferLength == 0.
 * \param errorBuffer The buffer to copy into.
 * \param message The message text to copy in.
 * \param bufferLength The number of bytes to copy including terminating null.
 */
static void
reportError(char* errorBuffer, const char* message, UDATA bufferLength) {
	if (bufferLength > 0) {
		strncpy(errorBuffer, message, bufferLength);
		errorBuffer[bufferLength -1] = '\0';
	}
}


/**
 * Registers a library named \c libName in \c classLoader by scanning the
 * directories listed in \c libraryPath.
 * \param vmThread
 * \param classLoader 	The ClassLoader in which to load the library.
 * \param libName 		The logical name of the library to load.
 * \param libraryPath 	The delimited set of paths to scan.
 * \param libraryPtr 	The native library handle to fill in (can be null).
 * \param errorBuffer	Buffer to store descriptive error message.
 * \param bufferLength	Length of the buffer in bytes.
 * \return One of the LOAD_* constants returned by \sa openNativeLibrary()
 */
UDATA
registerNativeLibrary(J9VMThread * vmThread, J9ClassLoader * classLoader, const char * libName, char * libraryPath, J9NativeLibrary** libraryPtr, char* errorBuffer, UDATA bufferLength)
{
	return openNativeLibrary(vmThread->javaVM, classLoader, libName, libraryPath, libraryPtr, classLoaderRegisterLibrary, vmThread, errorBuffer, bufferLength);
}

/**
 * Look for the boot library path system property starting
 * at the bottom of the vmInitArgs->options array
 *
 * @return	The string corresponding to the "com.ibm.oti.vm.bootstrap.library.path"
 *          or NULL
 */
static char *
getBootLibraryPath(JavaVMInitArgs *vmInitArgs)
{
	char * bootLibraryPath = NULL;
	jint count = vmInitArgs->nOptions;

	for ( count = (vmInitArgs->nOptions - 1) ; count >= 0 ; count-- ) {
		JavaVMOption* option = &(vmInitArgs->options[count]);
		bootLibraryPath = getDefineArgument(option->optionString, "com.ibm.oti.vm.bootstrap.library.path");
		if (bootLibraryPath){
			break;
		}
	}
	return bootLibraryPath;
}

/**
 * Registers a bootstrap library named \c libName.
 * \param vmThread
 * \param libName 		The logical name of the library to load.
 * \param libraryPtr 	The native library handle to fill in (can be null).
 * \param suppressError	Non-zero to suppress error codes.
 * \return One of the LOAD_* constants returned by \sa openNativeLibrary()
 */
UDATA
registerBootstrapLibrary(J9VMThread * vmThread, const char * libName, J9NativeLibrary** libraryPtr, UDATA suppressError)
{
	char * bootLibraryPath = NULL;
	UDATA result;
	char errorBuffer[512];

	JavaVMInitArgs  *vmInitArgs = (JavaVMInitArgs  *) vmThread->javaVM->vmArgsArray->actualVMArgs;

	/* Look for the boot library path system property */
	if (vmInitArgs) {
		bootLibraryPath = getBootLibraryPath(vmInitArgs);
	}
	
	Assert_VM_mustNotHaveVMAccess(vmThread);

	result = registerNativeLibrary(vmThread, vmThread->javaVM->systemClassLoader, libName, bootLibraryPath, libraryPtr, errorBuffer, sizeof(errorBuffer));

	if (result && !suppressError) {
		PORT_ACCESS_FROM_VMC(vmThread);
		j9tty_printf(PORTLIB, "<error: unable to load %s (%s)>\n", libName, errorBuffer);
	}

	return result;
}


/**
 * Determines if the \c path is relative or absolute.
 * \param path The path to examine.
 * \return Non-zero if the path is absolute, zero if relative.
 */
static BOOLEAN
isAbsolutePath(const char *path)
{
	if (!path) {
		return FALSE;
	}

	if (path[0] == DIR_SEPARATOR) {
		return TRUE;
	}

#if defined(ALT_DIR_SEPARATOR)
	if (path[0] == ALT_DIR_SEPARATOR) {
		return TRUE;
	}
#endif /* defined(ALT_DIR_SEPARATOR) */

#if defined(WIN32) 
	if (isalpha(path[0]) && (path[1] == ':')) {
		if (path[2] == DIR_SEPARATOR) {
			return TRUE;
		}
#if defined(ALT_DIR_SEPARATOR)
		if (path[2] == ALT_DIR_SEPARATOR) {
			return TRUE;
		}
#endif /* defined(ALT_DIR_SEPARATOR) */
	}
#endif /* defined(WIN32) */

	return FALSE;
}


/**
 * Registers the library called \c sharedLibrary name in the specified \c classLoader.
 *
 * \param vmThread
 * \param classLoader		The classloader in which to register the library.
 * \param logicalName		The logical name of the shared library.
 * \param physicalName		The physical name of the shared library.
 * \param libraryPtr		Memory location to store the library handle.
 * \param errBuf			Buffer to store descriptive error message.
 * \param bufLen			Length of the buffer
 * \param flags				Flags that will be passed to j9sl_open_shared_library()
 *
 * \return One of the status codes indicating what happened.
 *   LOAD_OK 					If the library is already loaded by this classLoader.
 *   LOAD_OK 					If the library was loaded successfully.
 *   LOAD_ERR_ALREADY_LOADED 	If library is already loaded by another classLoader.
 *   LOAD_ERR_OUT_OF_MEMORY		If metadata allocation fails.
 *   LOAD_ERR_NOT_FOUND			If the library was not found on the libraryPath.
 *   LOAD_ERR_INVALID			If the library was not found on the libraryPath.
 *   LOAD_ERR_JNI_ONLOAD_FAILED	Library was found but OnLoad() failed.
 *
 * \warning The caller must not have VM access when calling this function and should be at a debug safe point.
 * \warning If vmThread is NULL, the library had better not have a JNI_OnLoad.
 * \warning If vmThread is non-NULL, the vmThread should have a call-out frame built on top of stack which should be discarded after calling this function to remove any JNI refs created in
 *  the JNI_OnLoad code.
 * \note If bufLen is > 0, errorBuffer will be filled in with an error description
 */
static UDATA
classLoaderRegisterLibrary(void *voidVMThread, J9ClassLoader *classLoader, const char* logicalName, char *physicalName, J9NativeLibrary** libraryPtr, char* errBuf, UDATA bufLen, UDATA flags)
{
	J9VMThread * vmThread = voidVMThread;
	UDATA rc = 0;
	J9JavaVM* javaVM = vmThread->javaVM;
	J9NativeLibrary* newNativeLibrary = NULL;
	J9ClassLoader* aClassLoader;
	BOOLEAN loadWasIntercepted = FALSE;
	UDATA jniVersion = (UDATA) -1;
	UDATA slOpenResult = J9PORT_SL_FOUND;
	char* onloadRtnName = NULL;
	UDATA nameLength = 0;
	char* executableName = NULL;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	Trc_VM_classLoaderRegisterLibrary_Entry(vmThread, classLoader, physicalName, flags);

	Assert_VM_mustNotHaveVMAccess(vmThread);

	omrthread_monitor_enter(javaVM->nativeLibraryMonitor);

	ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);

	/*
	 * See if this library is already open in a classLoader.
	 */
	aClassLoader = findLoadedSharedLibrary(vmThread, physicalName, libraryPtr);

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	/* if another ClassLoader has loaded this library attempt to force it to be unloaded before reporting an error */
	if ( (aClassLoader != NULL) && (aClassLoader != classLoader) ) {
		javaVM->memoryManagerFunctions->forceClassLoaderUnload(vmThread, aClassLoader);
		/* try to find the loaded shared library again */
		aClassLoader = findLoadedSharedLibrary(vmThread, physicalName, libraryPtr);
	}
#endif

	if (aClassLoader != NULL) {
		RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
		if (aClassLoader == classLoader) {
			/* Loading a library multiple times in a ClassLoader is NOP. */
			omrthread_monitor_exit(javaVM->nativeLibraryMonitor);
			Trc_VM_classLoaderRegisterLibrary_Noop(vmThread, physicalName);
			return J9NATIVELIB_LOAD_OK;
		} else {
			/* loading a library in multiple class loaders is not permitted */
			reportError(errBuf, "Library is already loaded in another ClassLoader", bufLen);
			omrthread_monitor_exit(javaVM->nativeLibraryMonitor);
			Trc_VM_classLoaderRegisterLibrary_OtherLoader(vmThread, physicalName);
			return J9NATIVELIB_LOAD_ERR_ALREADY_LOADED;
		}
	}

	/*
	 * Create an entry for this library.
	 */
	if (classLoader->sharedLibraries == NULL) {
		classLoader->sharedLibraries = pool_new(sizeof(J9NativeLibrary),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES,
				(j9memAlloc_fptr_t)pool_portLibAlloc, (j9memFree_fptr_t)pool_portLibFree, PORTLIB);
	}

	if (classLoader->sharedLibraries != NULL) {
		newNativeLibrary = pool_newElement(classLoader->sharedLibraries);
	}

	if (NULL == newNativeLibrary) {
		reportError(errBuf, "Failed allocating memory for native library", bufLen);
		rc = J9NATIVELIB_LOAD_ERR_OUT_OF_MEMORY;
	}

	if (0 == rc) {
		/* Initialize the structure with default values */
		javaVM->internalVMFunctions->initializeNativeLibrary(javaVM, newNativeLibrary);

		/* Copy the physical name */
		newNativeLibrary->name = j9mem_allocate_memory(strlen(physicalName) + 1, J9MEM_CATEGORY_CLASSES);
		if (NULL == newNativeLibrary->name) {
			reportError(errBuf, "Failed allocating memory for native library name", bufLen);
			rc = J9NATIVELIB_LOAD_ERR_OUT_OF_MEMORY;
		} else {
			strcpy(newNativeLibrary->name, physicalName);
		}

		/* Copy the logical name */
		newNativeLibrary->logicalName = j9mem_allocate_memory(strlen(logicalName) + 1, J9MEM_CATEGORY_CLASSES);
		if (NULL == newNativeLibrary->logicalName) {
			reportError(errBuf, "Failed allocating memory for native library logical name", bufLen);
			rc = J9NATIVELIB_LOAD_ERR_OUT_OF_MEMORY;
		} else {
			strcpy(newNativeLibrary->logicalName, logicalName);
		}

	}

	/**
	 * Call the intercept hook.
	 */
	if (J9_EVENT_IS_HOOKED(javaVM->hookInterface, J9HOOK_VM_LOAD_JNI_LIBRARY)) {
		UDATA result = J9NATIVELIB_LOAD_ERR_NOT_FOUND;
		ALWAYS_TRIGGER_J9HOOK_VM_LOAD_JNI_LIBRARY(
					javaVM->hookInterface,
					vmThread,
					classLoader,
					physicalName,
					newNativeLibrary,
					result,
					errBuf,
					bufLen,
					flags);

		switch (result) {
			case J9NATIVELIB_LOAD_OK:
				loadWasIntercepted = TRUE;
				if (libraryPtr != NULL) {
					*libraryPtr = newNativeLibrary;
				}
				break;

			case J9NATIVELIB_LOAD_ERR_NOT_FOUND:
				/* non-fatal error, try standard load */
				break;

			default:
				rc = result;
				break;
		}
	}
	newNativeLibrary->linkMode = J9NATIVELIB_LINK_MODE_UNINITIALIZED;

	/* Try linking statically */
	if ((J9NATIVELIB_LOAD_OK == rc) && !loadWasIntercepted) {
		/* Open a handle to the executable that launched this jvm instance.
		 * Certain platforms require executable name for opening a handle to it. If this cannot
		 * be found, fall back to plain old dynamic linking.
		 */
		if (0 == j9sysinfo_get_executable_name(NULL, &executableName)) {
			slOpenResult = j9sl_open_shared_library(executableName,
						&newNativeLibrary->handle,
						(flags | J9PORT_SLOPEN_OPEN_EXECUTABLE));
			if (J9PORT_SL_FOUND == slOpenResult) {
				nameLength = J9STATIC_ONLOAD_LENGTH + strlen(logicalName) + 1;
				onloadRtnName = j9mem_allocate_memory(nameLength, J9MEM_CATEGORY_CLASSES);
				if (NULL == onloadRtnName) {
					/* Close handle to the executable. */
					j9sl_close_shared_library(newNativeLibrary->handle);
					newNativeLibrary->handle = 0;
					rc = J9NATIVELIB_LOAD_ERR_OUT_OF_MEMORY;
					reportError(errBuf, "Failed allocating memory for JNI_OnLoad routine", bufLen);
					RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
					goto leave_routine;
				}

				/* Proceed to check for the JNI_OnLoad_L() routine.  If not found skip to linking 
				 * dynamically.
				 */
				j9str_printf(PORTLIB, onloadRtnName, nameLength, "%s%s", J9STATIC_ONLOAD, logicalName);
				RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
				exitVMToJNI(vmThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
				jniVersion = (*newNativeLibrary->send_lifecycle_event)(vmThread,
																	   newNativeLibrary,
																	   onloadRtnName,
																	   (UDATA)-1);
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
				enterVMFromJNI(vmThread);
				releaseVMAccess(vmThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

				/* If JNI version returned is 1.8 (or above), JNI_OnLoad_L /was/ found and 
				 * invoked, successfully. 
				 */
				if ((JNI_VERSION_1_8 <= jniVersion) && ((UDATA)-1 != jniVersion)) {
					/* Found and invoked the library initializer; linking statically. */
					newNativeLibrary->linkMode = J9NATIVELIB_LINK_MODE_STATIC;
					if (NULL != libraryPtr) {
						*libraryPtr = newNativeLibrary;
					}
				} else {
					/* Return value is not 1.8 (or above); could mean either of 2 things:
					 * 1. JNI_OnLoad_L /was/ defined, but didn't return 1.8 (or above).  Eg. a 
					 * JNI_OnLoad_L returned a lower version, say 1.6)!  Flag error, since this 
					 * should be possible only 1.8 upwards.
					 * 2. The JNI_OnLoad_L rtn was not defined at all, in which case we go to 
					 * dynamic linking.
					 */
					if (((UDATA)-1 != jniVersion) || (NULL != vmThread->currentException)) {
						char msgBuffer[MAXIMUM_MESSAGE_LENGTH];

						/* Close handle to the executable. */
						j9sl_close_shared_library(newNativeLibrary->handle);
						newNativeLibrary->handle = 0;
						rc = J9NATIVELIB_LOAD_ERR_JNI_ONLOAD_FAILED;
						j9str_printf(PORTLIB, 
									 msgBuffer, 
									 MAXIMUM_MESSAGE_LENGTH, 
									 "0x%.8x not a valid JNI version for static linking (1.8 or later required)",
									 jniVersion);
						reportError(errBuf, msgBuffer, bufLen);
						goto leave_routine;
					}
					ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
				}
			}
		} else {
			/* Report failure to obtain executable name as a trace point; proceed to dynamic linking. */
			Trc_VM_classLoaderRegisterLibrary_execNameNotFound(vmThread, 
															   j9error_last_error_number());
		}
	}

	/* Not bound statically as either:
	 * (1) Java version is not 1.8 or above, or 
	 * (2) Library couldn't be opened as a executable; that is, it is indeed a shared 
	 * library named by System.loadLibrary("library"), or 
	 * (3) JNI_OnLoad_L wasn't found inside the executable (static library not linked 
	 * into the executable).  
	 * In any case, fall back on linking dynamically. 
	 */
	if ((J9NATIVELIB_LINK_MODE_UNINITIALIZED == newNativeLibrary->linkMode) 
		&& (J9NATIVELIB_LOAD_OK == rc) 
		&& !loadWasIntercepted
	) {
		slOpenResult = j9sl_open_shared_library(physicalName, &newNativeLibrary->handle, flags);
		if (J9PORT_SL_FOUND != slOpenResult) {
			if (J9PORT_SL_NOT_FOUND == slOpenResult) {
				rc = J9NATIVELIB_LOAD_ERR_NOT_FOUND;
			} else {
				rc = J9NATIVELIB_LOAD_ERR_INVALID;
			}
			reportError(errBuf, j9error_last_error_message(), bufLen);
		} else {
			/* Linked the library successfully in dynamic mode. */
			newNativeLibrary->linkMode = J9NATIVELIB_LINK_MODE_DYNAMIC;
		}

		if (NULL != libraryPtr) {
			*libraryPtr = newNativeLibrary;
		}
#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
		if (J9NATIVELIB_LOAD_OK == rc) {
			validateLibrary(javaVM, newNativeLibrary);
		}
#endif
		RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);

		/* Call JNI_OnLoad to get the required JNI version, only if the library has not
		 * already been linked statically (as JNI_OnLoad_L would have been invoked by now). 
		 */
		if (J9NATIVELIB_LOAD_OK == rc) {
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			exitVMToJNI(vmThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
			jniVersion = (*newNativeLibrary->send_lifecycle_event)(vmThread,
																   newNativeLibrary,
																   J9DYNAMIC_ONLOAD,
																   JNI_VERSION_1_1);
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			enterVMFromJNI(vmThread);
			releaseVMAccess(vmThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

			if ((FALSE == jniVersionIsValid(jniVersion)) || (NULL != vmThread->currentException)) {
				char msgBuffer[MAXIMUM_MESSAGE_LENGTH];

				j9str_printf(PORTLIB, 
							 msgBuffer, 
							 MAXIMUM_MESSAGE_LENGTH, 
							 "0x%.8x is not a valid JNI version", 
							 jniVersion);
				if ((UDATA)-1 == jniVersion) {
					strcpy(msgBuffer, "JNI_OnLoad returned JNI_ERR");
				}
				if (NULL != vmThread->currentException) {
					strcpy(msgBuffer, "An exception was pending after running JNI_OnLoad");
				}
				rc = J9NATIVELIB_LOAD_ERR_JNI_ONLOAD_FAILED;
				reportError(errBuf, msgBuffer, bufLen);
			}
		}
	}

	if ( (rc != J9NATIVELIB_LOAD_OK) && (newNativeLibrary != NULL) ) {
		freeSharedLibrary(vmThread, classLoader, newNativeLibrary);
	} else {
		issueWriteBarrier();
		ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
		if (classLoader->librariesTail == NULL) {
			classLoader->librariesHead = classLoader->librariesTail = newNativeLibrary;
		} else {
			classLoader->librariesTail->next = newNativeLibrary;
			classLoader->librariesTail = newNativeLibrary;
		}
		RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
	}

leave_routine:
	omrthread_monitor_exit(javaVM->nativeLibraryMonitor);
	if (NULL != onloadRtnName) {
		j9mem_free_memory(onloadRtnName);
	}
	/* Don't delete executableName; system-owned string. */
	Trc_VM_classLoaderRegisterLibrary_Exit(vmThread, physicalName, rc);

	return rc;
}

/**
 * Closes a native library, and removes any metadata.
 * \param vmThread
 * \param classLoader 	The classloader which contains the library.
 * \param library		The library to close.
 */
static void
freeSharedLibrary(J9VMThread *vmThread, J9ClassLoader* classLoader, J9NativeLibrary* library)
{
	J9JavaVM* javaVM = vmThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);

	(*library->close)(vmThread, library);
	j9mem_free_memory(library->name);
	j9mem_free_memory(library->logicalName);
	pool_removeElement(classLoader->sharedLibraries, library);

	RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
}

/**
 * Scans all ClassLoaders searching for a library named \c sharedLibraryName.
 * \param vmThread
 * \param sharedLibraryName The name of the library to scan for.
 * \param libraryPtr Location to store the native library if found.
 * \return The ClassLoader which loaded the library, NULL if none found.
 */
static J9ClassLoader*
findLoadedSharedLibrary(J9VMThread* vmThread, const char* sharedLibraryName, J9NativeLibrary** libraryPtr)
{
	J9JavaVM *vm = vmThread->javaVM;
	J9ClassLoader *classLoader;
	J9ClassLoaderWalkState walkState;

	/* this walk will include any dead or unloading classloaders */
	classLoader = vm->internalVMFunctions->allClassLoadersStartDo(&walkState, vm, J9CLASSLOADERWALK_INCLUDE_DEAD);
	while (NULL != classLoader) {
		if (classLoader->sharedLibraries != NULL) {
			pool_state librariesPoolState;
			J9NativeLibrary *nativeLibrary = pool_startDo(classLoader->sharedLibraries, &librariesPoolState);

			while (nativeLibrary != NULL) {
				if (0 == strcmp(nativeLibrary->name, sharedLibraryName)) {
					if (NULL != libraryPtr) {
						*libraryPtr = nativeLibrary;
					}
					vm->internalVMFunctions->allClassLoadersEndDo(&walkState);
					return classLoader;
				}
				nativeLibrary = pool_nextDo(&librariesPoolState);
			}
		}
		classLoader = vm->internalVMFunctions->allClassLoadersNextDo(&walkState);
	}
	vm->internalVMFunctions->allClassLoadersEndDo(&walkState);

	return NULL;
}

/**
 * Initialize native library callback functions to default values.
 * \param javaVM
 * \param library The library to initialize.
 * \return Zero on success, non-zero on failure.
 */
UDATA
initializeNativeLibrary(J9JavaVM * javaVM, J9NativeLibrary* library)
{
	if (NULL==library) {
		return 1;
	}

	library->close = closeLibraryCallback;
	library->send_lifecycle_event = sendLifecycleEventCallback;
	library->bind_method = lookupJNINative;
	library->flags = 0;
	return 0;
}
