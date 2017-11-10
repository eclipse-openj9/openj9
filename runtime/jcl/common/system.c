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
jstring getEncoding(JNIEnv *env, jint encodingType);
static void JNICALL systemPropertyIterator(char* key, char* value, void* userData);
char* getDefinedEncoding(JNIEnv *env, char *defArg);
jobject getPropertyList(JNIEnv *env);

jstring JNICALL Java_java_lang_System_getEncoding(JNIEnv *env, jclass clazz, jint encodingType)
{
	return getEncoding(env, encodingType);
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


char* getDefinedEncoding(JNIEnv *env, char *defArg)
{
	VMI_ACCESS_FROM_ENV(env);

	JavaVMInitArgs  *vmInitArgs = (*VMI)->GetInitArgs(VMI);
	int len = (int)strlen(defArg);

	if (vmInitArgs) {
		jint optionIndex;
		JavaVMOption *option = vmInitArgs->options;

		for (optionIndex=0; optionIndex < vmInitArgs->nOptions; optionIndex++) {
			char *optionValue = option->optionString;
			if (strncmp(defArg, optionValue, len) == 0)
				return &optionValue[len];
			option++;
		}
	}
	return NULL;
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
	jobject propertyList;
#define PROPERTY_COUNT 135
	char *propertyKey= NULL;
	const char * language;
	const char * region;
	const char * variant;
	const char *strings[PROPERTY_COUNT];
#define USERNAME_LENGTH 128
	char username[USERNAME_LENGTH];
	char *usernameAlloc = NULL;
	IDATA result;

	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	OMR_VM *omrVM = javaVM->omrVM;

	/* Change the allocation value PROPERTY_COUNT above as you add/remove properties, * then follow the 
		propIndex++ convention and consume 2 * slots for each property. 2 * number of property keys is the * correct allocation.

		Also note the call to addSystemProperties below, which may add some configuration-specific properties.  Be sure to leave
		enough room in the property list for all possibilities.
	*/

	if (J9_GC_POLICY_METRONOME == (omrVM->gcPolicy)) {
		strings[propIndex++] = "com.ibm.jvm.realtime";
		strings[propIndex++] = "soft";
	}

#if defined(J9VM_OPT_SHARED_CLASSES)
	strings[propIndex++] = "com.ibm.oti.shared.enabled";
	if (((J9VMThread *) env)->javaVM->sharedClassConfig != NULL) {
		strings[propIndex++] = "true";
	} else {
		strings[propIndex++] = "false";
	}
#endif

#if defined(JCL_J2SE)
	strings[propIndex++] = "ibm.signalhandling.sigchain";
	if (javaVM->sigFlags & J9_SIG_NO_SIG_CHAIN) {
		strings[propIndex++] = "false";
	} else {
		strings[propIndex++] = "true";
	}
	strings[propIndex++] = "ibm.signalhandling.sigint";
	if (javaVM->sigFlags & J9_SIG_NO_SIG_INT) {
		strings[propIndex++] = "false";
	} else {
		strings[propIndex++] = "true";
	}

	/* The JCLs use ibm.signalhandling.rs to determine if they should prevent the registration of signal handlers for what 
	 * 	we consider to be asynchronous signals.
	 * The JCLs do not install handlers for any synchronous signals */
	strings[propIndex++] = "ibm.signalhandling.rs";
	if (javaVM->sigFlags & J9_SIG_XRS_ASYNC) {
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
	if ( (strcmp(language, "nn")== 0) && (strcmp(region, "NO") == 0) ){
		variant = "NY";
	}
	if ( (strcmp(language, "nn") == 0) || (strcmp(language, "nb") == 0) ) {
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
	result = j9sysinfo_get_username(username, USERNAME_LENGTH);
	if (!result) {
		strings[propIndex++] = username;
	} else {
		if (result > 0) {
			usernameAlloc = jclmem_allocate_memory(env, result);
			if (usernameAlloc) {
				result = j9sysinfo_get_username(usernameAlloc, result);
			}
		}
		strings[propIndex++] = !usernameAlloc || result ? "unknown" : usernameAlloc;
	}

#undef USERNAME_LENGTH

	propertyList = getPlatformPropertyList(env, strings, propIndex);
	if (usernameAlloc) {
		jclmem_free_memory(env, usernameAlloc);
	}
	return propertyList;
}


/**
 * encodingType
 *    0 - initialize the locale
 *    1 - platform encoding
 *    2 - file.encoding
 *    3 - os.encoding
 */
jstring getEncoding(JNIEnv *env, jint encodingType)
{
	char* encoding, property[128];

	switch(encodingType) {
		case 0:		/* initialize the locale */
			getPlatformFileEncoding(env, NULL, 0, encodingType);
			return NULL;

		case 1: 		/* platform encoding */
			encoding = getPlatformFileEncoding(env, property, sizeof(property), encodingType);
			break;

		case 2:		/* file.encoding */
			if (!(encoding = getDefinedEncoding(env, "-Dfile.encoding=")))
			{
				encoding = getPlatformFileEncoding(env, property, sizeof(property), encodingType);
			}
#if defined(J9ZOS390)
			if (__CSNameType(encoding) == _CSTYPE_ASCII) {
				__ccsid_t ccsid;
				ccsid = __toCcsid(encoding);
				atoe_setFileTaggingCcsid(&ccsid);
			}       
#endif
			break;

		case 3:		/* os.encoding */
			if (!(encoding = getDefinedEncoding(env, "-Dos.encoding="))) {
#if defined(J9ZOS390) || defined(J9ZTPF)
				encoding = "ISO8859_1";
#elif defined(WIN32)
				encoding = "UTF8";
#else
				return NULL;
#endif
			 } 
			break;

		default:
			return NULL;
	}

	return (*env)->NewStringUTF(env, encoding);
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
		
		if ((J2SE_VERSION(vm) >= J2SE_19) && (J2SE_SHAPE(vm) >= J2SE_SHAPE_B165)) {
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
#if defined(J9VM_RAS_EYECATCHERS)
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
#endif /* J9VM_RAS_EYECATCHERS */
}
