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

#if defined(J9ZOS390) && (JAVA_SPEC_VERSION >= 21)
	case 4: /* com.ibm.autocvt setting on z/OS */
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

#if JAVA_SPEC_VERSION < 17
jobjectArray JNICALL
Java_java_lang_System_getPropertyList(JNIEnv *env, jclass clazz)
{
	return ((J9VMThread *)env)->javaVM->internalVMFunctions->getSystemPropertyList(env);
}
#endif /* JAVA_SPEC_VERSION < 17 */

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
