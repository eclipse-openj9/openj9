/*******************************************************************************
 * Copyright IBM Corp. and others 1998
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#include <string.h>
#include <stdlib.h>
#include "jcl.h"
#include "j9lib.h"
#include "omr.h"
#include "util_api.h"


#include "j9protos.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "j2sever.h"

#include "jclglob.h"
#include "jclprots.h"

#include "ut_j9jcl.h"
#include "j9jclnls.h"

#if defined(J9ZOS390)
#include "atoe.h"
#include <_Ccsid.h>
#endif



/* JCL_J2SE */
#define JCL_J2SE



typedef struct {
	int errorOccurred;
	jobject args;
	jint nCommandLineDefines;
	JNIEnv *env;
	const char **defaultValues;
	int defaultCount;
	jclass stringClass;
} CreateSystemPropertiesData;

jint propertyListAddString( JNIEnv *env, jarray array, jint arrayIndex, const char *value);
static void JNICALL systemPropertyIterator(char* key, char* value, void* userData);
jobject getPropertyList(JNIEnv *env);

#if JAVA_SPEC_VERSION >= 11
void JNICALL
Java_java_lang_System_initJCLPlatformEncoding(JNIEnv *env, jclass clazz)
{
	UDATA handle = 0;
	J9JavaVM * const vm = ((J9VMThread*)env)->javaVM;
	char dllPath[EsMaxPath] = {0};
	UDATA written = 0;
	const char *encoding = NULL;
	PORT_ACCESS_FROM_ENV(env);

#if defined(OSX)
	encoding = "UTF-8";
#else
	char property[128] = {0};
	encoding = getPlatformFileEncoding(env, property, sizeof(property), 1); /* platform encoding */
#endif /* defined(OSX) */
	/* libjava.[so|dylib] is in the jdk/lib/ directory, one level up from the default/ & compressedrefs/ directories */
	written = j9str_printf(PORTLIB, dllPath, sizeof(dllPath), "%s/../java", vm->j2seRootDirectory);
	/* Assert the number of characters written (not including the null) fit within the dllPath buffer */
	Assert_JCL_true(written < (sizeof(dllPath) - 1));
	if (0 == j9sl_open_shared_library(dllPath, &handle, J9PORT_SLOPEN_DECORATE)) {
		void (JNICALL *nativeFuncAddrJNU)(JNIEnv *env, const char *str) = NULL;
		if (0 == j9sl_lookup_name(handle, "InitializeEncoding", (UDATA*) &nativeFuncAddrJNU, "VLL")) {
			/* invoke JCL native to initialize platform encoding explicitly */
			nativeFuncAddrJNU(env, encoding);
		}
	}
}
#endif /* JAVA_SPEC_VERSION >= 11 */

/**
 * sysPropID
 *    0 - os.version
 *    1 - platform encoding
 *    2 - file.encoding
 *    3 - os.encoding
 *    4 - default value of java.io.tmpDir before any -D options
 */
jstring JNICALL
Java_java_lang_System_getSysPropBeforePropertiesInitialized(JNIEnv *env, jclass clazz, jint sysPropID)
{
	char *envSpace = NULL;
	const char *sysPropValue = NULL;
	/* The sysPropValue points to following property which has to be declared at top level. */
	char property[128] = {0};
	jstring result = NULL;
	PORT_ACCESS_FROM_ENV(env);
	VMI_ACCESS_FROM_ENV(env);
	JavaVMInitArgs *vmInitArgs = (*VMI)->GetInitArgs(VMI);

	switch (sysPropID) {
	case 0: /* os.version */
		/* Same logic as vmprops.c:initializeSystemProperties(vm) - j9sysinfo_get_OS_version() */
		sysPropValue = j9sysinfo_get_OS_version();
		if (NULL != sysPropValue) {
#if defined(WIN32)
			char *cursor = strchr(sysPropValue, ' ');
			if (NULL != cursor) {
				*cursor = '\0';
			}
#endif /* defined(WIN32) */
		} else {
			sysPropValue = "unknown";
		}
		break;

	case 1: /* platform encoding: ibm.system.encoding, sun.jnu.encoding, native.encoding when file.encoding is NULL */
#if defined(OSX)
		sysPropValue = "UTF-8";
#else
		sysPropValue = getPlatformFileEncoding(env, property, sizeof(property), sysPropID);
#endif /* defined(OSX) */
		break;

	case 2: /* file.encoding */
		sysPropValue = getDefinedArgumentFromJavaVMInitArgs(vmInitArgs, "file.encoding");
		if (NULL == sysPropValue) {
#if JAVA_SPEC_VERSION < 18
			sysPropValue = getPlatformFileEncoding(env, property, sizeof(property), sysPropID);
#else /* JAVA_SPEC_VERSION < 18 */
			sysPropValue = "UTF-8";
		} else {
			if (0 == strcmp("COMPAT", sysPropValue)) {
				sysPropValue = getPlatformFileEncoding(env, property, sizeof(property), sysPropID);
			}
#endif /* JAVA_SPEC_VERSION < 18 */
		}
#if defined(J9ZOS390)
		if (__CSNameType(sysPropValue) == _CSTYPE_ASCII) {
			__ccsid_t ccsid;
			ccsid = __toCcsid(sysPropValue);
			atoe_setFileTaggingCcsid(&ccsid);
		}
#endif /* defined(J9ZOS390) */
		break;

	case 3: /* os.encoding */
		sysPropValue = getDefinedArgumentFromJavaVMInitArgs(vmInitArgs, "os.encoding");
		if (NULL == sysPropValue) {
#if defined(J9ZOS390) || defined(J9ZTPF)
			sysPropValue = "ISO8859_1";
#elif defined(WIN32) /* defined(J9ZOS390) || defined(J9ZTPF) */
			sysPropValue = "UTF-8";
#endif /* defined(J9ZOS390) || defined(J9ZTPF) */
		}
		break;

	case 4: /* default value of java.io.tmpDir before any -D options */
		sysPropValue = getTmpDir(env, &envSpace);
		break;

#if defined(J9ZOS390) && (JAVA_SPEC_VERSION >= 21)
	case 5: /* com.ibm.autocvt setting on z/OS */
		sysPropValue = getDefinedArgumentFromJavaVMInitArgs(vmInitArgs, "com.ibm.autocvt");
		if (NULL == sysPropValue) {
			/* As part of better handling of JEP400 constraints on z/OS, the com.ibm.autocvt property
			 * determines whether file I/O considers file tagging. If not explicitly specified,
			 * the property defaults to true, unless file.encoding is set.
			 */
			const char *fileEncodingValue = getDefinedArgumentFromJavaVMInitArgs(vmInitArgs, "file.encoding");
			sysPropValue = (NULL == fileEncodingValue) ? "true" : "false";
		}
		break;
#endif /* defined(J9ZOS390) && (JAVA_SPEC_VERSION >= 21) */

	default:
		break;
	}
	if (NULL != sysPropValue) {
		result = (*env)->NewStringUTF(env, sysPropValue);
	}
	if (NULL != envSpace) {
		jclmem_free_memory(env, envSpace);
	}

	return result;
}

jobject JNICALL Java_java_lang_System_getPropertyList(JNIEnv *env, jclass clazz)
{
	return getPropertyList(env);
}

jstring JNICALL Java_java_lang_System_mapLibraryName(JNIEnv * env, jclass unusedClass, jstring inName)
{
	PORT_ACCESS_FROM_ENV(env);
	jboolean isCopy = FALSE;
	char *outNameUTF;
	const char *inNameUTF;
	jstring result;

	if (!inName) {
		jclass aClass;
		aClass = (*env)->FindClass(env, "java/lang/NullPointerException");
		if (0 == aClass) {
			return NULL;
		}
		(*env)->ThrowNew(env, aClass, "");
		return NULL;
	}

	inNameUTF = (const char *) (*env)->GetStringUTFChars(env, inName, &isCopy);
	if (!inNameUTF) {
		return NULL;			/* there will be an exception pending */
	}

	if (!(outNameUTF = jclmem_allocate_memory(env, strlen(inNameUTF) + 20))) /* allow for platform extras plus trailing zero */
		return NULL;
	mapLibraryToPlatformName(inNameUTF, outNameUTF);

	(*env)->ReleaseStringUTFChars(env, inName, inNameUTF);

	result = (*env)->NewStringUTF(env, outNameUTF);
	jclmem_free_memory(env, outNameUTF);
	return result;
}


void JNICALL Java_java_lang_System_setFieldImpl(JNIEnv * env, jclass cls, jstring name, jobject stream)
{
	jfieldID descriptorFID;
	const char *bytes;

	bytes = (const char *) (*env)->GetStringUTFChars(env, name, NULL);
	if (!bytes) return;
	if (!strcmp(bytes, "in"))
		descriptorFID = (*env)->GetStaticFieldID(env, cls, bytes, "Ljava/io/InputStream;");
	else
		descriptorFID = (*env)->GetStaticFieldID(env, cls, bytes, "Ljava/io/PrintStream;");
	(*env)->ReleaseStringUTFChars(env, name, bytes);
	if (!descriptorFID) return;
	(*env)->SetStaticObjectField(env, cls, descriptorFID, stream);
}


jobject createSystemPropertyList(JNIEnv *env, const char *defaultValues[], int defaultCount)
{
	VMI_ACCESS_FROM_ENV(env);

	jint i, nCommandLineDefines = 0;
	jclass stringClass;
	jarray args = NULL;
	int propertyCount;

	stringClass = (*env)->FindClass(env, "java/lang/String");
	if (!stringClass) {
/*		printf("\nFailed to find class java/lang/String");*/
		return (jobject) 0;
	}

	(*VMI)->CountSystemProperties(VMI, &propertyCount);
	if (propertyCount) {
		CreateSystemPropertiesData iteratorData;

		args = (*env)->NewObjectArray(env, defaultCount + (propertyCount * 2), stringClass, NULL);
		if (NULL == args) {
			return NULL;
		}

		iteratorData.errorOccurred = 0;
		iteratorData.args = args;
		iteratorData.nCommandLineDefines = nCommandLineDefines;
		iteratorData.env = env;
		iteratorData.defaultValues = defaultValues;
		iteratorData.defaultCount = defaultCount;

		iteratorData.stringClass = stringClass;
		(*VMI)->IterateSystemProperties(VMI, systemPropertyIterator, &iteratorData);
		if (iteratorData.errorOccurred) {
			return NULL;
		}
		nCommandLineDefines = iteratorData.nCommandLineDefines;
	}

	if (NULL == args) {
		args = (*env)->NewObjectArray(env, defaultCount, stringClass, NULL);
	}
	if (NULL == args) {
/*		printf("\nFailed to create arg array");*/
		return NULL;
	}

	for (i = 0; i < defaultCount; ++i) {
		if (defaultValues[i] == NULL) continue;
		if (-1 == propertyListAddString( env, args, nCommandLineDefines, defaultValues[i]) ) {
			return NULL;
		}
		nCommandLineDefines++;
	}

	return args;
}

/**
 * @return 0 on success, -1 on error
 */
jint propertyListAddString( JNIEnv *env, jarray array, jint arrayIndex, const char *value)
{
	/* String must be well-formed modified UTF-8 */
	jobject str = (*env)->NewStringUTF(env, value);
	if (NULL != str) {
		(*env)->SetObjectArrayElement(env, array, arrayIndex, str);
	}
	/* NewStringUTF does not throw an exception, other than OutOfMemory */
	return ((*env)->ExceptionCheck(env) == JNI_TRUE) ? -1 : 0;
}

jobject getPropertyList(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	int propIndex = 0;
	jobject propertyList = NULL;
#define PROPERTY_COUNT 137
	char *propertyKey = NULL;
	const char * language = NULL;
	const char * region = NULL;
	const char * variant = NULL;
	const char *strings[PROPERTY_COUNT] = {0};
#define USERNAME_LENGTH 128
	char username[USERNAME_LENGTH] = {0};
	char *usernameAlloc = NULL;
	/* buffer to hold the size of the maximum direct byte buffer allocations */
	char maxDirectMemBuff[24] = {0};
	IDATA result = 0;

	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = javaVM->internalVMFunctions;

	/* Change the allocation value PROPERTY_COUNT above as you add/remove properties,
	 * then follow the propIndex++ convention and consume 2 * slots for each property. 2 * number of property keys is the
	 * correct allocation.
	 * Also note the call to addSystemProperties below, which may add some configuration-specific properties.  Be sure to leave
	 * enough room in the property list for all possibilities.
	 */

	if (J9_GC_POLICY_METRONOME == (javaVM->omrVM->gcPolicy)) {
		strings[propIndex++] = "com.ibm.jvm.realtime";
		strings[propIndex++] = "soft";
	}

#if defined(J9VM_OPT_SHARED_CLASSES)
	strings[propIndex++] = "com.ibm.oti.shared.enabled";
	if ((NULL != javaVM->sharedClassConfig)
		&& J9_ARE_ALL_BITS_SET(javaVM->sharedClassConfig->runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_CACHE_NON_BOOT_CLASSES)
	) {
		strings[propIndex++] = "true";
	} else {
		strings[propIndex++] = "false";
	}
#endif

#if defined(JCL_J2SE)
	strings[propIndex++] = "ibm.signalhandling.sigchain";
	if (J9_ARE_ANY_BITS_SET(javaVM->sigFlags, J9_SIG_NO_SIG_CHAIN)) {
		strings[propIndex++] = "false";
	} else {
		strings[propIndex++] = "true";
	}
	strings[propIndex++] = "ibm.signalhandling.sigint";
	if (J9_ARE_ANY_BITS_SET(javaVM->sigFlags, J9_SIG_NO_SIG_INT)) {
		strings[propIndex++] = "false";
	} else {
		strings[propIndex++] = "true";
	}

	/* The JCLs use ibm.signalhandling.rs to determine if they should prevent the registration of signal handlers for what
	 * 	we consider to be asynchronous signals.
	 * The JCLs do not install handlers for any synchronous signals */
	strings[propIndex++] = "ibm.signalhandling.rs";
	if (J9_ARE_ALL_BITS_SET(javaVM->sigFlags, J9_SIG_XRS_ASYNC)) {
		strings[propIndex++] = "true";
	} else {
		strings[propIndex++] = "false";
	}
#endif

	strings[propIndex++] = "com.ibm.vm.bitmode";
#ifdef J9VM_ENV_DATA64
	strings[propIndex++] = "64";
#else
	strings[propIndex++] = "32";
#endif

	strings[propIndex++] = "com.ibm.cpu.endian";
#ifdef J9VM_ENV_LITTLE_ENDIAN
	strings[propIndex++] = "little";
#else
	strings[propIndex++] = "big";
#endif

	strings[propIndex++] = "sun.cpu.endian";
#ifdef J9VM_ENV_LITTLE_ENDIAN
	strings[propIndex++] = "little";
#else
	strings[propIndex++] = "big";
#endif

/*	Don't set this property as the class library will look here first and when
	there is a security manager you will get a security exception. The code
	looks in this package by default, see URLConnection.getContentHandler()
	strings[propIndex++] = "java.content.handler.pkgs";
	strings[propIndex++] = "com.ibm.oti.www.content";
*/

	/*[PR 95709]*/

	/* Get the language, region and variant */
	language = j9nls_get_language();
	region = j9nls_get_region();
	variant = j9nls_get_variant();

	/* CMVC 144405 : Norwegian Bokmal and Nynorsk need special consideration */
	if ((0 == strcmp(language, "nn")) && (0 == strcmp(region, "NO"))) {
		variant = "NY";
	}
	if ((0 == strcmp(language, "nn")) || (0 == strcmp(language, "nb"))) {
		language = "no";
	}

	strings[propIndex++] = "user.language";
	strings[propIndex++] = language;

	propertyKey = "user.country";
	strings[propIndex++] = propertyKey;
	strings[propIndex++] = region;

	/* Get the variant */
	strings[propIndex++] = "user.variant";
	strings[propIndex++] = variant;

	/* Get the User name */
	strings[propIndex++] = "user.name";
	strings[propIndex] = "unknown";
#if defined(J9VM_OPT_CRIU_SUPPORT)
	/* Skip j9sysinfo_get_username if a checkpoint can be taken.
	 * https://github.com/eclipse-openj9/openj9/issues/15800
	 */
	result = -1;
	if (!vmFuncs->isCheckpointAllowed(javaVM))
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	{
		result = j9sysinfo_get_username(username, USERNAME_LENGTH);
		if (0 == result) {
			strings[propIndex] = username;
		} else if (result > 0) {
			usernameAlloc = jclmem_allocate_memory(env, result);
			if (NULL != usernameAlloc) {
				result = j9sysinfo_get_username(usernameAlloc, result);
				if (0 == result) {
					strings[propIndex] = usernameAlloc;
				} else {
					/* free the memory, try j9sysinfo_get_env later */
					jclmem_free_memory(env, usernameAlloc);
					usernameAlloc = NULL;
				}
			} else {
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto failed;
			}
		}
	}
#if defined(LINUX) || defined(OSX)
	if (0 != result) {
		result = j9sysinfo_get_env("USER", username, USERNAME_LENGTH);
		if (0 == result) {
			strings[propIndex] = username;
		} else if (result > 0) {
			usernameAlloc = jclmem_allocate_memory(env, result);
			if (NULL != usernameAlloc) {
				result = j9sysinfo_get_env("USER", usernameAlloc, result);
				if (0 == result) {
					if (strlen(usernameAlloc) > 0) {
						strings[propIndex] = usernameAlloc;
					}
					/* keep it as "unknown" if the env value is empty */
				}
				/* usernameAlloc to be freed before this method returns */
			} else {
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto failed;
			}
		}
	}
#endif /* defined(LINUX) || defined(OSX) */
	propIndex += 1;
#undef USERNAME_LENGTH

#if defined(OPENJ9_BUILD) && JAVA_SPEC_VERSION == 8
	/* Set the maximum direct byte buffer allocation property if it has not been set manually */
	if ((~(UDATA)0) == javaVM->directByteBufferMemoryMax) {
		UDATA heapSize = javaVM->memoryManagerFunctions->j9gc_get_maximum_heap_size(javaVM);
		/* allow up to 7/8 of the heap to be direct byte buffers */
		javaVM->directByteBufferMemoryMax = heapSize - (heapSize / 8);
	}
#endif /* defined(OPENJ9_BUILD) && JAVA_SPEC_VERSION == 8 */
#if !defined(OPENJ9_BUILD)
	/* Don't set a default value for IBM Java 8. */
	if ((~(UDATA)0) != javaVM->directByteBufferMemoryMax)
#endif /* !defined(OPENJ9_BUILD) */
	{
		strings[propIndex] = "sun.nio.MaxDirectMemorySize";
		propIndex += 1;
		if ((~(UDATA)0) == javaVM->directByteBufferMemoryMax) {
			strcpy(maxDirectMemBuff, "-1");
		} else {
			j9str_printf(PORTLIB, maxDirectMemBuff, sizeof(maxDirectMemBuff), "%zu", javaVM->directByteBufferMemoryMax);
		}
		strings[propIndex] = maxDirectMemBuff;
		propIndex += 1;
	}

	propertyList = getPlatformPropertyList(env, strings, propIndex);

failed:
	if (NULL != usernameAlloc) {
		jclmem_free_memory(env, usernameAlloc);
	}
	return propertyList;
}

static void JNICALL
systemPropertyIterator(char* key, char* value, void* userData)
{
	CreateSystemPropertiesData * iteratorData = userData;
	jobject args = iteratorData->args;
	JNIEnv *env = iteratorData->env;
	const char **defaultValues = iteratorData->defaultValues;
	int defaultCount = iteratorData->defaultCount;
	jint i;

	/* CMVC 95717: if an error has already occurred get out of here */
	if ( iteratorData->errorOccurred ) {
		return;
	}

	if (0 == strcmp("com.ibm.oti.shared.enabled", key)) {
		/* JAZZ103 85641: Prevent com.ibm.oti.shared.enabled from being overwritten by a command line option */
		return;
	}

#if JAVA_SPEC_VERSION >= 21
	if (0 == strcmp("java.compiler", key)) {
		PORT_ACCESS_FROM_ENV(env);
		if ((0 == strcmp("jitc", value)) || (0 == strcmp(J9_JIT_DLL_NAME, value))) {
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JCL_JAVA_COMPILER_WARNING_XJIT);
		} else {
			j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JCL_JAVA_COMPILER_WARNING_XINT);
		}
		return;
	}
#endif /* JAVA_SPEC_VERSION >= 21 */

	/* check for overridden system properties, use linear scan for now */
	for (i=0; i < defaultCount; i+=2) {
		if (defaultValues[i] && !strcmp(key, defaultValues[i])) {
			defaultValues[i] = NULL;
			defaultValues[i+1] = NULL;
			break;
		}
	}

	/* First do the key */
	if( propertyListAddString( env, args, iteratorData->nCommandLineDefines++, key) ) {
		iteratorData->errorOccurred = 1;
		return;
	}

	/* Then the value */
	if( propertyListAddString( env, args, iteratorData->nCommandLineDefines++, value) ) {
		iteratorData->errorOccurred = 1;
		return;
	}

	Trc_JCL_systemPropertyIterator(env, key, value);
}


void JNICALL
Java_java_lang_System_startSNMPAgent(JNIEnv *env, jclass jlClass)
{
	J9VMThread * currentThread = (J9VMThread*)env;
	J9JavaVM * vm = currentThread->javaVM;

	if (J9_ARE_ALL_BITS_SET(vm->jclFlags, J9_JCL_FLAG_COM_SUN_MANAGEMENT_PROP)) {
		jclass smAClass = NULL;
		jmethodID startAgent = NULL;

		if (J2SE_VERSION(vm) >= J2SE_V11) {
			smAClass = (*env)->FindClass(env, "jdk/internal/agent/Agent");
		} else {
			smAClass = (*env)->FindClass(env, "sun/management/Agent");
		}
		if (NULL != smAClass) {
			startAgent = (*env)->GetStaticMethodID(env, smAClass, "startAgent", "()V");
			if (NULL != startAgent) {
				(*env)->CallStaticVoidMethod(env, smAClass, startAgent);
			}
		} else {
			(*env)->ExceptionClear(env);
		}
	}
}

void JNICALL
Java_java_lang_System_rasInitializeVersion(JNIEnv * env, jclass unusedClass, jstring javaRuntimeVersion)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	jboolean isCopy = JNI_FALSE;
	const char *utfRuntimeVersion = NULL;

	if (NULL != javaRuntimeVersion) {
		utfRuntimeVersion = (*env)->GetStringUTFChars(env, javaRuntimeVersion, &isCopy);
	}

	javaVM->internalVMFunctions->rasSetServiceLevel(javaVM, utfRuntimeVersion);

	if (NULL != utfRuntimeVersion) {
		(*env)->ReleaseStringUTFChars(env, javaRuntimeVersion, utfRuntimeVersion);
	}
}
