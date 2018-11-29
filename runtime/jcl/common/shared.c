/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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
#include "j9.h"
#include "j9consts.h"
#include "j9jclnls.h"
#include "hashtable_api.h"
#include "jni.h"
#include "romcookie.h"
#include "ut_j9jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#if defined(J9VM_OPT_SHARED_CLASSES)
#include "SCAbstractAPI.h"
#endif
#include "omrutil.h"
#include "shchelp.h"

#if defined(J9VM_OPT_SHARED_CLASSES)

#define DEF_STRING_FARM_SIZE 4096
#define STACK_STRINGBUF_SIZE 256

/* error code returned by native methods when shared class utilities is disabled i.e. vm->sharedCacheAPI is NULL */
#define SHARED_CLASSES_UTILITIES_DISABLED	-255

/* Uncomment, rebuild and run "java -Xshareclasses" to run unit tests */
/* #define J9SHR_UNIT_TEST */

typedef struct URLhtEntry {
	const char* origPath;
	jsize origPathLen;
	jint helperID;
	UDATA cpeType;
	void* data;
} URLhtEntry;

typedef struct UTF8htEntry {
	const char* key;
	U_16 keyLen;
	const J9UTF8* cachedUTFString;
} UTF8htEntry;

typedef struct URLElements {
	const char *pathChars;
	const char *protocolChars;
	jsize pathLen;
	jsize protocolLen;
	jstring pathObj;
	jstring protocolObj;
} URLElements;


static J9ClassPathEntry* getCachedURL(JNIEnv* env, jint helperID, const char* pathChars, jsize pathLen, UDATA cpeType, U_16 cpeStatus);
static const char* copyString(J9PortLibrary* portlib, const char* toCopy, UDATA length, J9SharedStringFarm** farmRoot, const J9UTF8** makeUTF8);
UDATA urlHashFn(void* item, void *userData);
static jint createURLEntry(JNIEnv* env, jint helperID, J9ClassPathEntry** cpEntry_, char* correctedPathCopy, UDATA cpeType, U_16 cpeStatus);
UDATA utfHashEqualFn(void* left, void* right, void *userData);
static jint createROMClassCookie(JNIEnv* env, J9JavaVM* vm, J9ROMClass* romClass, jbyteArray romClassCookieBuffer);
static jint createToken(JNIEnv* env, jint helperID, J9ClassPathEntry** cpEntry_, const char* tokenChars, jsize tokenSize);
static UDATA correctURLPath(JNIEnv* env, const char* pathChars, jsize pathLen, char** correctedPathPtr, J9SharedStringFarm** jclStringFarm);
static const char* getCachedString(JNIEnv* env, const char* input, jsize length, J9SharedStringFarm** farmRoot, const J9UTF8** getUTF8);
static UDATA getCpeTypeForProtocol(char* protocol, jsize protocolLen, const char* pathChars, jsize pathLen);
static void getURLMethodIDs(JNIEnv* env);
static J9Pool* getClasspathCache(JNIEnv* env);
static J9Pool* getURLCache(JNIEnv* env);
static J9ClassPathEntry* getCachedToken(JNIEnv* env, jint helperID, const char* tokenChars, jsize tokenSize);
static jint createCPEntries(JNIEnv* env, jint helperID, jint urlCount, J9ClassPathEntry** cpEntries_, URLElements* urlArrayElements);
UDATA utfHashFn(void* item, void *userData);
static UDATA getStringChars(JNIEnv* env, const char** chars, jsize* len, jstring obj);
static UDATA getStringPair(JNIEnv* env, const char** chars1, jsize* len1, const char** chars2, jsize* len2, jstring obj1, jstring obj2);
#if defined(J9SHR_UNIT_TEST)
static void runCorrectURLUnitTests(JNIEnv* env);
static void testCorrectURLPath(JNIEnv* env, char* test, char* expected);
#endif
static jobject createDirectByteBuffer(JNIEnv* env, const void* address, UDATA dataLen);
UDATA urlHashEqualFn(void* left, void* right, void *userData);
static UDATA getPathProtocolFromURL(JNIEnv* env, jobject url, jmethodID URLgetPathID, jmethodID URLgetProtocolID, URLElements *urlElements);
static void releaseStringChars(JNIEnv* env, jstring str, const char* chars);
static void releaseStringPair(JNIEnv* env, jstring str1, const char* chars1, jstring str2, const char* chars2);
static J9Pool* getTokenCache(JNIEnv* env);


/* Pass a jclStringFarm if this string is to be copied and kept, otherwise pass a large enough char buffer in correctedPathPtr */
/* THREADING: Must be protected by jclCacheMutex */
static UDATA
correctURLPath(JNIEnv* env, const char* pathChars, jsize pathLen, char** correctedPathPtr, J9SharedStringFarm** jclStringFarm)
{
	UDATA decodeEscape, i, charValue, copiedChars, freeMem, returnVal, startOffset;
	char buffer[STACK_STRINGBUF_SIZE];
	char* bufPtr = (char*)buffer;
	char current;

	PORT_ACCESS_FROM_VMC((J9VMThread*)env);

	Trc_JCL_com_ibm_oti_shared_correctURLPath_Entry(env, (char*)pathChars);

	charValue = 0;
	decodeEscape = 0;
	copiedChars = 0;
	freeMem = FALSE;
	returnVal = 1;
	startOffset = 0;

#ifdef WIN32
	for (;pathChars[startOffset]=='/'; startOffset++);

	/* If Windows UNC (no path colon) do not remove leading spaces */
	if (pathChars[startOffset+1] != ':') {
		startOffset = 0;
	}
#endif

	if (pathLen >= STACK_STRINGBUF_SIZE) {
		if (!(bufPtr = (char*)j9mem_allocate_memory((pathLen + 1) * sizeof(char), J9MEM_CATEGORY_VM_JCL))) {
			Trc_JCL_com_ibm_oti_shared_correctURLPath_Exit0(env);
			returnVal = 0;
			goto _donePreAlloc;
		}
		freeMem = TRUE;
	}

	for (i=startOffset; i<(UDATA)pathLen; i++) {
		current = pathChars[i];

		if (current=='/') {
			if (i == (pathLen-1)) { 
				current = '\0';		/* remove trailing slash */
			} 
#ifdef WIN32
			else {
				current = '\\';		/* convert / to \\ */
			}
#endif
		}

		/* Assumes escape sequence is %nn, where nn is hex value */
		if (decodeEscape) {
			charValue += (current - '0') * (decodeEscape==2 ? 16 : 1);
			if (--decodeEscape == 0) {
				current = (char)charValue;
			} else {
				continue;
			}
		} else {
			if (current == '%') {
				decodeEscape = 2;
				charValue = 0;
				continue;
			}
		}

		/* Don't add a trailing NULL char to the new UTF8 */
		if (current) {
			bufPtr[copiedChars++] = current;
		}
	}

	if (!jclStringFarm) {
		strncpy(*correctedPathPtr, bufPtr, copiedChars);
		(*correctedPathPtr)[copiedChars]='\0';
	} else {
		if (!(*correctedPathPtr = (char*)getCachedString(env, (const char*)bufPtr, (jsize)copiedChars, jclStringFarm, NULL))) {
			Trc_JCL_com_ibm_oti_shared_correctURLPath_Exit0(env);
			returnVal = 0;
		}
	}

	if (freeMem) {
		j9mem_free_memory(bufPtr);
	}

_donePreAlloc:

	Trc_JCL_com_ibm_oti_shared_correctURLPath_Exit1(env, returnVal, (*correctedPathPtr));
	return returnVal;
}


/* THREADING: Can be called multi-threaded */
static jint
createROMClassCookie(JNIEnv* env, J9JavaVM* vm, J9ROMClass* romClass, jbyteArray romClassCookieBuffer)
{
	IDATA i = 0;
	J9ROMClassCookieSharedClass romClassCookie;
	jbyte romClassCookieSig[] = J9_ROM_CLASS_COOKIE_SIG;
	J9VMThread* vmthread = (J9VMThread*)env;

	Trc_JCL_com_ibm_oti_shared_createROMClassCookie_Entry(env, romClass);
	
	memset( &romClassCookie, 0, sizeof(J9ROMClassCookieSharedClass));
	for ( i=0; i<J9_ROM_CLASS_COOKIE_SIG_LENGTH; i++ ) {
		romClassCookie.signature[i] = romClassCookieSig[i];
	}
	romClassCookie.version = J9_ROM_CLASS_COOKIE_VERSION;
	romClassCookie.type = J9_ROM_CLASS_COOKIE_TYPE_SHARED_CLASS;
	romClassCookie.romClass = romClass;
	/* Create a magic number that we can check in romutil.c to prevent people hacking up ROMClass cookies from the java space */
	romClassCookie.magic = J9_ROM_CLASS_COOKIE_MAGIC(vmthread->javaVM, romClass);

	(*env)->SetByteArrayRegion(env, romClassCookieBuffer, (jsize)0, sizeof(J9ROMClassCookieSharedClass), (jbyte*)&romClassCookie);

	Trc_JCL_com_ibm_oti_shared_createROMClassCookie_Exit(env);
	return 0;
}


/* THREADING: Must be protected by jclCacheMutex */
static jint
createCPEntries(JNIEnv* env, jint helperID, jint urlCount, J9ClassPathEntry** cpEntries_, URLElements* urlArrayElements)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	J9ClassPathEntry* cpEntries = NULL;
	struct J9ClasspathByID* newCacheItem = NULL;
	J9Pool* cpCachePool = getClasspathCache(env);
	UDATA cpEntrySize = 0;
	IDATA i;

	PORT_ACCESS_FROM_VMC((J9VMThread*)env);

	Trc_JCL_com_ibm_oti_shared_createCPEntries_Entry(env, helperID, urlCount);
	
	cpEntrySize = urlCount * sizeof(struct J9ClassPathEntry);
	cpEntries = j9mem_allocate_memory(cpEntrySize, J9MEM_CATEGORY_VM_JCL);
	if (NULL == cpEntries) {
		Trc_JCL_com_ibm_oti_shared_createCPEntries_ExitFalse2(env);
		goto _error;
	}
	memset(cpEntries, 0, cpEntrySize);

	for (i=0; i<urlCount; i++) {
		UDATA cpeType = 0;
		char* correctPath = NULL;

		cpeType = getCpeTypeForProtocol((char*)urlArrayElements[i].protocolChars, urlArrayElements[i].protocolLen, urlArrayElements[i].pathChars, urlArrayElements[i].pathLen);
		if (CPE_TYPE_UNKNOWN == cpeType) {
			Trc_JCL_com_ibm_oti_shared_createCPEntries_ExitFalse4(env);
			goto _error;
		}
		if (!correctURLPath(env, urlArrayElements[i].pathChars, urlArrayElements[i].pathLen, &correctPath, &(vm->sharedClassConfig->jclStringFarm))) {
			Trc_JCL_com_ibm_oti_shared_createCPEntries_ExitFalse5(env);
			goto _error;
		}

		cpEntries[i].path = (U_8*)correctPath;
		cpEntries[i].extraInfo = NULL;
		cpEntries[i].pathLength = (U_32)strlen(correctPath);
		cpEntries[i].flags = 0;
		cpEntries[i].type = (U_16)cpeType;
	}

	if (!cpCachePool || !(newCacheItem = (struct J9ClasspathByID*)pool_newElement(cpCachePool))) {
		Trc_JCL_com_ibm_oti_shared_createCPEntries_ExitFalse6(env);
		goto _error;
	}
	newCacheItem->header.magic = CP_MAGIC;
	newCacheItem->header.type = CP_TYPE_CLASSPATH;
	newCacheItem->header.id = helperID;
	newCacheItem->header.jclData = cpEntries;
	newCacheItem->header.cpData = NULL;

	newCacheItem->entryCount = urlCount;
	cpEntries[0].extraInfo = (void*)newCacheItem;

	*cpEntries_ = cpEntries;

	Trc_JCL_com_ibm_oti_shared_createCPEntries_ExitTrue(env);
	return TRUE;
	
_error:
	if (NULL != cpEntries) {
		j9mem_free_memory(cpEntries);
	}
	return FALSE;
}


/* THREADING: Must be protected by jclCacheMutex */
static J9ClassPathEntry*
getCachedToken(JNIEnv* env, jint helperID, const char* tokenChars, jsize tokenSize)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	pool_state aState;
	struct J9TokenByID* anElement;
	J9Pool* tokenCachePool;
	UDATA hashCompare;

	Trc_JCL_com_ibm_oti_shared_getCachedToken_Entry(env, helperID);

	if (!(tokenCachePool = getTokenCache(env))) {
		Trc_JCL_com_ibm_oti_shared_getCachedToken_ExitNull(env);
		return NULL;
	}
	hashCompare = vm->internalVMFunctions->computeHashForUTF8((U_8*)tokenChars, (U_16)tokenSize);

	anElement = (struct J9TokenByID*)pool_startDo(tokenCachePool, &aState);
	while (anElement && (!(
										helperID==anElement->header.id && 
										hashCompare==anElement->tokenHash && 
										anElement->header.jclData->pathLength==tokenSize &&
										strncmp((char*)anElement->header.jclData->path, tokenChars, tokenSize)==0
										))) {
		anElement = (struct J9TokenByID*)pool_nextDo(&aState);
	}
	if (anElement) {
		Trc_JCL_com_ibm_oti_shared_getCachedToken_ExitFound(env);
		return anElement->header.jclData;
	}
	Trc_JCL_com_ibm_oti_shared_getCachedToken_ExitNull(env);
	return NULL;
}


static void
notifyCacheFull(JNIEnv* env, jbyteArray nativeFlags)
{
	jbyte value = 1;
	
	(*env)->SetByteArrayRegion(env, nativeFlags, (jsize)0, sizeof(value)/sizeof(jbyte), &value);
}


/* THREADING: Must be protected by jclCacheMutex */
static jint
createToken(JNIEnv* env, jint helperID, J9ClassPathEntry** cpEntry_, const char* tokenChars, jsize tokenSize)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	J9Pool* tokenCachePool = getTokenCache(env);
	J9Pool* j9ClassPathEntryPool = vm->sharedClassConfig->jclJ9ClassPathEntryPool;
	J9TokenByID* tokenByID = NULL;
	J9ClassPathEntry* newEntry = NULL;

	Trc_JCL_com_ibm_oti_shared_createToken_Entry(env, helperID);
	
	if (!(tokenCachePool && (tokenByID = (J9TokenByID*)pool_newElement(tokenCachePool)))) {
		Trc_JCL_com_ibm_oti_shared_createToken_ExitFalse1(env);
		return FALSE;
	}
	if (!(newEntry = (J9ClassPathEntry*)pool_newElement(j9ClassPathEntryPool))) {
		Trc_JCL_com_ibm_oti_shared_createToken_ExitFalse2(env);
		return FALSE;
	}
	if (!(newEntry->path = (U_8*)getCachedString(env, (const char*)tokenChars, (jsize)tokenSize, &(vm->sharedClassConfig->jclStringFarm), NULL))) {
		Trc_JCL_com_ibm_oti_shared_createToken_ExitFalse3(env);
		return FALSE;
	}
	tokenByID->header.magic = CP_MAGIC;
	tokenByID->header.type = CP_TYPE_TOKEN;
	tokenByID->header.id = helperID;
	tokenByID->header.jclData = newEntry;
	tokenByID->header.cpData = NULL;
	tokenByID->tokenHash = vm->internalVMFunctions->computeHashForUTF8((U_8*)tokenChars, (U_16)tokenSize);

	newEntry->extraInfo = (void*)tokenByID;
	newEntry->pathLength = (U_32)tokenSize;
	newEntry->flags = 0;
	newEntry->type = CPE_TYPE_UNUSABLE;

	*cpEntry_ = newEntry;

	Trc_JCL_com_ibm_oti_shared_createToken_ExitTrue(env);
	return TRUE;
}


static void
getURLMethodIDs(JNIEnv* env)
{
	jclass javaNetURLClassLocalRef = NULL;
	jclass javaNetURLClass = NULL;
	jmethodID urlGetPathID = NULL;
	jmethodID urlGetProtocolID = NULL;
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;

	Trc_JCL_com_ibm_oti_shared_getURLMethodIDs_Entry(env);

	javaNetURLClass = JCL_CACHE_GET(env, CLS_java_net_URL);

	if (NULL == javaNetURLClass) {
		omrthread_monitor_enter(vm->jclCacheMutex);

		javaNetURLClass = JCL_CACHE_GET(env, CLS_java_net_URL);
		if (NULL == javaNetURLClass) {
			javaNetURLClassLocalRef = (*env)->FindClass(env, "java/net/URL");
			if (NULL == javaNetURLClassLocalRef) {
				/* exception has already been set */
				omrthread_monitor_exit(vm->jclCacheMutex);
				goto _exit;
			}

			javaNetURLClass = (*env)->NewGlobalRef(env, javaNetURLClassLocalRef);
			(*env)->DeleteLocalRef(env, javaNetURLClassLocalRef);
			if (NULL == javaNetURLClass) {
				omrthread_monitor_exit(vm->jclCacheMutex);
				vm->internalVMFunctions->throwNativeOOMError(env, J9NLS_JCL_OOM_NEW_GLOBAL_REF);
				goto _exit;
			}
			JCL_CACHE_SET(env, CLS_java_net_URL, javaNetURLClass);
		}

		omrthread_monitor_exit(vm->jclCacheMutex);
	}

	urlGetPathID = JCL_CACHE_GET(env, MID_java_net_URL_getPath);
	if (NULL == urlGetPathID) {
		urlGetPathID = (*env)->GetMethodID(env, javaNetURLClass, "getPath", "()Ljava/lang/String;");
		if (NULL == urlGetPathID) {
			/* exception has already been set */
			goto _exit;
		} else {
			JCL_CACHE_SET(env, MID_java_net_URL_getPath, urlGetPathID);
		}
	}

	urlGetProtocolID = JCL_CACHE_GET(env, MID_java_net_URL_getProtocol);
	if (NULL == urlGetProtocolID) {
		urlGetProtocolID = (*env)->GetMethodID(env, javaNetURLClass, "getProtocol", "()Ljava/lang/String;");
		if (NULL == urlGetProtocolID) {
			/* exception has already been set */
			goto _exit;
		} else {
			JCL_CACHE_SET(env, MID_java_net_URL_getProtocol, urlGetProtocolID);
		}
	}

_exit:
	if (JNI_FALSE == (*env)->ExceptionCheck(env)) {
		Trc_JCL_com_ibm_oti_shared_getURLMethodIDs_ExitSetOK_V1(env);
	} else {
		Trc_JCL_com_ibm_oti_shared_getURLMethodIDs_ExitError(env);
	}

	return;
}


/* Note:CorrectedPathCopy must be a copied string which will not disappear and partition must be also not disappear */
/* THREADING: Must be protected by jclCacheMutex */
static jint
createURLEntry(JNIEnv* env, jint helperID, J9ClassPathEntry** cpEntry_, char* correctedPathCopy, UDATA cpeType, U_16 cpeStatus)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	J9ClassPathEntry* newEntry;
	struct J9URLByID* urlByID = NULL;
	J9Pool* urlCachePool = getURLCache(env);
	J9Pool* j9ClassPathEntryPool = vm->sharedClassConfig->jclJ9ClassPathEntryPool;

	Trc_JCL_com_ibm_oti_shared_createURLEntry_Entry(env, helperID, correctedPathCopy, cpeType);

	if (!urlCachePool || !(urlByID = (J9URLByID*)pool_newElement(urlCachePool))) {
		Trc_JCL_com_ibm_oti_shared_createURLEntry_ExitFalse1(env);
		return FALSE;
	}
	if (!(newEntry = (J9ClassPathEntry*)pool_newElement(j9ClassPathEntryPool))) {
		Trc_JCL_com_ibm_oti_shared_createURLEntry_ExitFalse2(env);
		return FALSE;
	}

	newEntry->path = (U_8*)correctedPathCopy;
	newEntry->extraInfo = NULL;
	newEntry->pathLength = (U_32)strlen(correctedPathCopy);
	newEntry->flags = 0;
	newEntry->type = (U_16)cpeType;
	newEntry->status = cpeStatus;

	urlByID->header.magic = CP_MAGIC;
	urlByID->header.type = CP_TYPE_URL;
	urlByID->header.id = helperID;
	urlByID->header.jclData = newEntry;
	urlByID->header.cpData = NULL;

	newEntry->extraInfo = (void*)urlByID;

	*cpEntry_ = newEntry;

	Trc_JCL_com_ibm_oti_shared_createURLEntry_ExitTrue(env);
	return TRUE;
}


/* THREADING: Can be called multi-threaded */
static UDATA
getPathProtocolFromURL(JNIEnv* env, jobject url, jmethodID URLgetPathID, jmethodID URLgetProtocolID, URLElements* urlElements)
{
	UDATA rc = 0;
	Trc_JCL_com_ibm_oti_shared_getPathProtocolFromURL_Entry(env, url);

	urlElements->pathObj = (*env)->CallObjectMethod(env, url, URLgetPathID);
	if ((*env)->ExceptionCheck(env) || (NULL == urlElements->pathObj)) {
		(*env)->ExceptionClear(env);
		Trc_JCL_com_ibm_oti_shared_getPathProtocolFromURL_Exit1(env);
		rc = 0;
		goto _end;
	}

	urlElements->pathChars = (const char*)(*env)->GetStringUTFChars(env, urlElements->pathObj, NULL);
	if (NULL == urlElements->pathChars) {
		Trc_JCL_com_ibm_oti_shared_getPathProtocolFromURL_Exit2(env);
		rc = 0;
		goto _end;
	}
	urlElements->pathLen = (*env)->GetStringLength(env, urlElements->pathObj);

	urlElements->protocolObj = (*env)->CallObjectMethod(env, url, URLgetProtocolID);
	if ((*env)->ExceptionCheck(env) || (NULL == urlElements->protocolObj)) {
		(*env)->ExceptionClear(env);
		Trc_JCL_com_ibm_oti_shared_getPathProtocolFromURL_Exit3(env);
		rc = 0;
		goto _end;
	}

	urlElements->protocolChars = (const char*)(*env)->GetStringUTFChars(env, urlElements->protocolObj, NULL);
	if (NULL == urlElements->protocolChars) {
		Trc_JCL_com_ibm_oti_shared_getPathProtocolFromURL_Exit4(env);
		rc = 0;
		goto _end;
	}
	urlElements->protocolLen = (*env)->GetStringLength(env, urlElements->protocolObj);

	Trc_JCL_com_ibm_oti_shared_getPathProtocolFromURL_ExitOK(env);
	rc = 1;

_end:
	return rc;
}


/* THREADING: Can be called multi-threaded */
static UDATA
getCpeTypeForProtocol(char* protocol, jsize protocolLen, const char* pathChars, jsize pathLen)
{
	Trc_JCL_com_ibm_oti_shared_getCpeTypeForProtocol_Entry();

	if (!protocol) {
		Trc_JCL_com_ibm_oti_shared_getCpeTypeForProtocol_Exit0();
		return 0;
	}
	if (strncmp(protocol, "jar", 4)==0) {
		Trc_JCL_com_ibm_oti_shared_getCpeTypeForProtocol_ExitJAR();
		return CPE_TYPE_JAR;
	}
	if (strncmp(protocol, "file", 5)==0) {
		char* endsWith = (char*)(pathChars + (pathLen-4));
		if ((strncmp(endsWith, ".jar", 4)==0) || (strncmp(endsWith, ".zip", 4)==0) || strstr(pathChars,"!/") || strstr(pathChars,"!\\")) {
			Trc_JCL_com_ibm_oti_shared_getCpeTypeForProtocol_ExitJAR();
			return CPE_TYPE_JAR;
		} else {
			char JimageEndsWith[] = DIR_SEPARATOR_STR "lib" DIR_SEPARATOR_STR "modules";
			IDATA len = LITERAL_STRLEN(JimageEndsWith);

			if (pathLen >= len) {
				endsWith = (char*)(pathChars + (pathLen - len));
				if (strncmp(endsWith, JimageEndsWith, len) == 0) {
					Trc_JCL_com_ibm_oti_shared_getCpeTypeForProtocol_ExitJIMAGE();
					return CPE_TYPE_JIMAGE;
				}
			}
		}
		Trc_JCL_com_ibm_oti_shared_getCpeTypeForProtocol_ExitDIR();
		return CPE_TYPE_DIRECTORY;
	}
	Trc_JCL_com_ibm_oti_shared_getCpeTypeForProtocol_UnknownProtocol(protocolLen, protocol, pathLen, pathChars);
	Trc_JCL_com_ibm_oti_shared_getCpeTypeForProtocol_ExitUnknown();
	return CPE_TYPE_UNKNOWN;
}


/* THREADING: Must be protected by jclCacheMutex */
static J9ClassPathEntry* 
getCachedURL(JNIEnv* env, jint helperID, const char* pathChars, jsize pathLen, UDATA cpeType, U_16 cpeStatus)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	struct URLhtEntry* anElement = NULL;
	J9SharedClassConfig* config = vm->sharedClassConfig;
	J9HashTable* urlHashTable = config->jclURLHashTable;
	J9ClassPathEntry* urlEntry = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JCL_com_ibm_oti_shared_getCachedURL_Entry(env, helperID, cpeType);

	if (urlHashTable) {
		struct URLhtEntry dummy;

		dummy.origPath = pathChars;
		dummy.origPathLen = pathLen;
		dummy.helperID = helperID;
		dummy.cpeType = cpeType;

		anElement = (struct URLhtEntry*)hashTableFind(urlHashTable, (void*)&dummy);
	}

	if (anElement) {
		urlEntry = (J9ClassPathEntry*)anElement->data;
	} else {
		URLhtEntry newEntry;
		const char* origPathCopy;
		char* correctedPathCopy;

		/* We must make a copy of the path
			This is kept in the URLhtEntry and must be valid for the lifetime of the JVM */
		if (!(origPathCopy = getCachedString(env, (const char*)pathChars, (jsize)pathLen, &(config->jclStringFarm), NULL))) {
			Trc_JCL_com_ibm_oti_shared_getCachedURL_FailedStringCopy(env);
			goto _error;
		}

		/* Function returns a corrected copy of pathChars into correctedPathCopy. */
		correctURLPath(env, pathChars, pathLen, &correctedPathCopy, &(config->jclStringFarm));
		if (!createURLEntry(env, helperID, &urlEntry, correctedPathCopy, cpeType, cpeStatus)) {
			goto _error;
		}

		/* The original (not corrected) path is used as a key so that the correction does not have to be performed for each lookup */
		newEntry.origPath = (const char*)origPathCopy;
		newEntry.origPathLen = pathLen;
		newEntry.helperID = helperID;
		newEntry.cpeType = cpeType;
		newEntry.data = urlEntry;

		if (!config->jclURLHashTable) {
			config->jclURLHashTable = hashTableNew(OMRPORT_FROM_J9PORT(PORTLIB), J9_GET_CALLSITE(), 500, sizeof(URLhtEntry), sizeof(char *), 0, J9MEM_CATEGORY_VM_JCL, urlHashFn, urlHashEqualFn, NULL, vm);
			if (!config->jclURLHashTable) {
				goto _error;
			}
		}
		hashTableAdd(config->jclURLHashTable, &newEntry);
	}

	Trc_JCL_com_ibm_oti_shared_getCachedURL_ExitFound(env);
	return urlEntry;

_error:
	Trc_JCL_com_ibm_oti_shared_getCachedURL_ExitNull(env);
	return NULL;
}

static UDATA
getStringChars(JNIEnv* env, const char** chars, jsize* len, jstring obj)
{
	Trc_JCL_com_ibm_oti_shared_getStringChars_Entry(env, obj);

	if ((NULL != chars) && (NULL != len) && (NULL != obj)) {
		*chars = (const char*)(*env)->GetStringUTFChars(env, obj, NULL);
		if (NULL == *chars) {
			Trc_JCL_com_ibm_oti_shared_getStringChars_Exit(env);
			return 0;
		}
		*len = (*env)->GetStringLength(env, obj);
	}
	Trc_JCL_com_ibm_oti_shared_getStringChars_ExitOK(env);
	return 1;
}

/* THREADING: Can be called multi-threaded */
static UDATA
getStringPair(JNIEnv* env, const char** chars1, jsize* len1, const char** chars2, jsize* len2, jstring obj1, jstring obj2)
{
	if (!getStringChars(env, chars1, len1, obj1)) {
		return 0;
	}
	if (!getStringChars(env, chars2, len2, obj2)) {
		return 0;
	}
	return 1;
}

static void
releaseStringChars(JNIEnv* env, jstring str, const char* chars)
{
	Trc_JCL_com_ibm_oti_shared_releaseStringChars_Entry(env);

	if ((NULL != str) && (NULL != chars)) {
		(*env)->ReleaseStringUTFChars(env, str, chars);
	}

	Trc_JCL_com_ibm_oti_shared_releaseStringChars_Exit(env);
}

/* THREADING: Can be called multi-threaded */
static void
releaseStringPair(JNIEnv* env, jstring str1, const char* chars1, jstring str2, const char* chars2)
{
	releaseStringChars(env, str1, chars1);
	releaseStringChars(env, str2, chars2);
}


/* THREADING: Must be protected by jclCacheMutex */
static const char* 
copyString(J9PortLibrary* portlib, const char* toCopy, UDATA length, J9SharedStringFarm** farmRoot, const J9UTF8** makeUTF8)
{
	J9SharedStringFarm* farm = *farmRoot;
	J9SharedStringFarm* oldfarm = farm;
	char* returnVal = NULL;
	UDATA newSize = DEF_STRING_FARM_SIZE;
	UDATA toCopyLen = (length + 1) + ((makeUTF8) ? sizeof(J9UTF8) : 0);

	PORT_ACCESS_FROM_PORT(portlib);

	Trc_JCL_com_ibm_oti_shared_copyString_Entry(toCopy, length, farmRoot);

	while (farm!=NULL && (toCopyLen > farm->bytesLeft)) {
		oldfarm = farm;
		farm = farm->next;
	}
	if (farm==NULL) {
		if (toCopyLen > (newSize - sizeof(struct J9SharedStringFarm))) {
			newSize = toCopyLen + sizeof(struct J9SharedStringFarm);
		}
		farm = j9mem_allocate_memory(newSize, J9MEM_CATEGORY_VM_JCL);
		if (!farm) {
			Trc_JCL_com_ibm_oti_shared_copyString_ExitNoFarm();
			return NULL;
		} else {
			/*Trc_SHR_StringFarm_copyString_allocatedNewBuffer(newSize, farm);*/
			memset(farm, 0, newSize);
			farm->freePtr = (char*)((UDATA)farm + sizeof(struct J9SharedStringFarm));
			farm->bytesLeft = newSize - sizeof(struct J9SharedStringFarm);
			farm->next = NULL;
			if (oldfarm) {
				oldfarm->next = farm;
			} else {
				*farmRoot = farm;
			}
		}
	}
	returnVal = farm->freePtr;
	if (makeUTF8) {
		J9UTF8* tempUTF8 = (J9UTF8*)returnVal;

		J9UTF8_SET_LENGTH(tempUTF8, (U_16)length);
		returnVal = (char*)J9UTF8_DATA(tempUTF8);
		(*makeUTF8) = (const J9UTF8*)tempUTF8;
	}
	strncpy(returnVal, toCopy, length);
	returnVal[length] = '\0';
	farm->bytesLeft -= toCopyLen;
	farm->freePtr += toCopyLen;

	Trc_JCL_com_ibm_oti_shared_copyString_Exit(returnVal);
	return returnVal;
}


/* THREADING: All hashtable access is protected by jclCacheMutex */
UDATA
urlHashFn(void* item, void *userData)
{
	URLhtEntry* itemValue = (URLhtEntry*)item;
	UDATA hashValue;
	J9InternalVMFunctions* internalFunctionTable = ((J9JavaVM*)userData)->internalVMFunctions;

	Trc_JCL_com_ibm_oti_shared_urlHashFn_Entry(item);

	hashValue = internalFunctionTable->computeHashForUTF8((U_8*)itemValue->origPath, itemValue->origPathLen);

	Trc_JCL_com_ibm_oti_shared_urlHashFn_Exit(hashValue);
	return hashValue;
}


/* THREADING: All hashtable access is protected by jclCacheMutex */
UDATA
urlHashEqualFn(void* left, void* right, void *userData)
{
	URLhtEntry* leftItem = (URLhtEntry*)left;
	URLhtEntry* rightItem = (URLhtEntry*)right;
	UDATA result;

	Trc_JCL_com_ibm_oti_shared_urlHashEqualFn_Entry(left, right);

	if (leftItem->helperID != rightItem->helperID) {
		Trc_JCL_com_ibm_oti_shared_urlHashEqualFn_Exit1();
		return 0;
	}
	if (leftItem->cpeType != rightItem->cpeType) {
		Trc_JCL_com_ibm_oti_shared_urlHashEqualFn_Exit2();
		return 0;
	}
	result = J9UTF8_DATA_EQUALS(leftItem->origPath, leftItem->origPathLen, rightItem->origPath, rightItem->origPathLen);
	Trc_JCL_com_ibm_oti_shared_urlHashEqualFn_ExitResult(result);
	return result;
}


/* THREADING: Must be protected by jclCacheMutex */
static const char*
getCachedString(JNIEnv* env, const char* input, jsize length, J9SharedStringFarm** farmRoot, const J9UTF8** getUTF8)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	J9SharedClassConfig* config = vm->sharedClassConfig;
	J9HashTable* utfHashTable = config->jclUTF8HashTable;
	const char* result = NULL;

	PORT_ACCESS_FROM_VMC((J9VMThread*)env);

	if (!input || length<=0) {
		return NULL;
	}

	Trc_JCL_com_ibm_oti_shared_getCachedString_Entry(env, length, input, getUTF8);

	if (utfHashTable) {
		UTF8htEntry searchKey;
		UTF8htEntry* searchResult;

		searchKey.key = (const char*)input;
		searchKey.keyLen = (U_16)length;

		searchResult = (UTF8htEntry*)hashTableFind(utfHashTable, (void*)&searchKey);
		if (searchResult) {
			result = searchResult->key;
			if (getUTF8) {
				if (!searchResult->cachedUTFString) {
					/* This will make another inline copy of the input string - this should only happen if the string has been copied once, but a UTF8 was not made */
					if (!copyString(vm->portLibrary, input, (UDATA)length, farmRoot, &(searchResult->cachedUTFString))) {
						Trc_JCL_com_ibm_oti_shared_getCachedString_FailedStringCopy(env);
						goto _error;
					}
				}
				*getUTF8 = searchResult->cachedUTFString;
			}
		}
	}

	if (!result) {
		UTF8htEntry newEntry;
		const J9UTF8** doMakeUTF8 = NULL;

		newEntry.keyLen = (U_16)length;
		newEntry.cachedUTFString = NULL;

		if (getUTF8) {
			doMakeUTF8 = &(newEntry.cachedUTFString);
		}

		if (!(newEntry.key = (const char*)copyString(vm->portLibrary, input, (UDATA)length, farmRoot, doMakeUTF8))) {
			Trc_JCL_com_ibm_oti_shared_getCachedString_FailedStringCopy(env);
			goto _error;
		}

		if (!config->jclUTF8HashTable) {
			config->jclUTF8HashTable = hashTableNew(OMRPORT_FROM_J9PORT(PORTLIB), J9_GET_CALLSITE(), 1000, sizeof(UTF8htEntry), sizeof(char *), 0, J9MEM_CATEGORY_VM_JCL, utfHashFn, utfHashEqualFn, NULL, vm);
			if (!config->jclUTF8HashTable) {
				Trc_JCL_com_ibm_oti_shared_getCachedString_FailedHashTableCreate(env);
				goto _error;
			}
		}
		hashTableAdd(config->jclUTF8HashTable, &newEntry);
		result = newEntry.key;
		if (getUTF8) {
			*getUTF8 = newEntry.cachedUTFString;
		}
	}

	Trc_JCL_com_ibm_oti_shared_getCachedString_ExitOK(env, result);

	return result;

_error:
	Trc_JCL_com_ibm_oti_shared_getCachedString_ExitError(env);

	return NULL;
}


/* THREADING: Must be protected by jclCacheMutex */
static J9Pool*
getClasspathCache(JNIEnv* env)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	J9Pool** cpPool = &vm->sharedClassConfig->jclClasspathCache;

	if (!*cpPool) {
		*cpPool = pool_new(sizeof(struct J9ClasspathByID),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_VM_JCL, POOL_FOR_PORT(vm->portLibrary));
	}
	return *cpPool;
}


/* THREADING: Must be protected by jclCacheMutex */
static J9Pool*
getURLCache(JNIEnv* env)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	J9Pool** urlPool = &vm->sharedClassConfig->jclURLCache;

	if (!*urlPool) {
		*urlPool = pool_new(sizeof(struct J9URLByID), 0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_VM_JCL, POOL_FOR_PORT(vm->portLibrary));
	}
	return *urlPool;
}


/* THREADING: Must be protected by jclCacheMutex */
static J9Pool*
getTokenCache(JNIEnv* env)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	J9Pool** tokenPool = &vm->sharedClassConfig->jclTokenCache;

	if (!*tokenPool) {
		*tokenPool = pool_new(sizeof(struct J9TokenByID),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_VM_JCL, POOL_FOR_PORT(vm->portLibrary));
	}
	return *tokenPool;
}


/* THREADING: All hashtable access is protected by jclCacheMutex */
UDATA
utfHashFn(void* item, void *userData)
{
	UTF8htEntry* itemValue = (UTF8htEntry*)item;
	UDATA hashValue;
	J9InternalVMFunctions* internalFunctionTable = ((J9JavaVM*)userData)->internalVMFunctions;

	Trc_JCL_com_ibm_oti_shared_utfHashFn_Entry(item);

	hashValue = internalFunctionTable->computeHashForUTF8((U_8*)itemValue->key, itemValue->keyLen);

	Trc_JCL_com_ibm_oti_shared_utfHashFn_Exit(hashValue);
	return hashValue;
}


/* THREADING: All hashtable access is protected by jclCacheMutex */
UDATA
utfHashEqualFn(void* left, void* right, void *userData)
{
	UTF8htEntry* leftItem = (UTF8htEntry*)left;
	UTF8htEntry* rightItem = (UTF8htEntry*)right;
	UDATA result;

	Trc_JCL_com_ibm_oti_shared_utfHashEqualFn_Entry(left, right);
	
	result = J9UTF8_DATA_EQUALS(leftItem->key, leftItem->keyLen, rightItem->key, rightItem->keyLen);

	Trc_JCL_com_ibm_oti_shared_utfHashEqualFn_ExitResult(result);
	return result;
}


/* THREADING: Can be called multi-threaded */
static jobject
createDirectByteBuffer(JNIEnv* env, const void* address, UDATA dataLen)
{
	jobject returnVal = NULL;

	Trc_JCL_com_ibm_oti_shared_createDirectByteBuffer_Entry(env, address, dataLen);

	returnVal = (*env)->NewDirectByteBuffer(env, (void*)address, (jlong)dataLen);
	if (returnVal) {
		jclass byteBuffer = (*env)->FindClass(env, "java/nio/ByteBuffer");
		jmethodID asReadOnly;

		if (!byteBuffer) {
			(*env)->ExceptionClear(env);
			Trc_JCL_com_ibm_oti_shared_createDirectByteBuffer_Exit1(env);
			return NULL;
		}
		asReadOnly = (*env)->GetMethodID(env, byteBuffer, "asReadOnlyBuffer", "()Ljava/nio/ByteBuffer;");
		if (!asReadOnly) {
			(*env)->ExceptionClear(env);
			Trc_JCL_com_ibm_oti_shared_createDirectByteBuffer_Exit2(env);
			return NULL;
		}
		returnVal = (*env)->CallObjectMethod(env, returnVal, asReadOnly);
		if ((*env)->ExceptionCheck(env) || !returnVal) {
			(*env)->ExceptionClear(env);
			Trc_JCL_com_ibm_oti_shared_createDirectByteBuffer_Exit3(env);
			return NULL;
		}
	} 

	Trc_JCL_com_ibm_oti_shared_createDirectByteBuffer_ExitOK(env, returnVal);
	return returnVal;
}

#endif /* J9VM_OPT_SHARED_CLASSES */


jint JNICALL 
Java_com_ibm_oti_shared_SharedClassAbstractHelper_initializeShareableClassloaderImpl(JNIEnv* env, jobject thisObj, jobject classloader) 
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9ClassLoader* nativeClassloader;
	J9JavaVM* vm;
	jint result;

	Trc_JCL_com_ibm_oti_shared_SharedClassAbstractHelper_initializeShareableClassloaderImpl_Entry(env, classloader);

	vm = ((J9VMThread *)env)->javaVM;
	vm->internalVMFunctions->internalEnterVMFromJNI((J9VMThread *)env);
	nativeClassloader = J9VMJAVALANGCLASSLOADER_VMREF((J9VMThread *)env, J9_JNI_UNWRAP_REFERENCE(classloader));

	if (NULL == nativeClassloader) {
		nativeClassloader = vm->internalVMFunctions->internalAllocateClassLoader(vm, J9_JNI_UNWRAP_REFERENCE(classloader));
		if (NULL == nativeClassloader) {
			vm->internalVMFunctions->internalExitVMToJNI((J9VMThread *)env);
			return 0;
		}
	}

	nativeClassloader->flags |= J9CLASSLOADER_SHARED_CLASSES_ENABLED;
	vm->internalVMFunctions->internalExitVMToJNI((J9VMThread *)env);

	result = sizeof(J9ROMClassCookieSharedClass);

	Trc_JCL_com_ibm_oti_shared_SharedClassAbstractHelper_initializeShareableClassloaderImpl_Exit(env, result);
	return result;
#else
	return 0;
#endif
}

jboolean JNICALL 
Java_com_ibm_oti_shared_SharedClassTokenHelperImpl_findSharedClassImpl2(JNIEnv* env, jobject thisObj, jint helperID, 
		jstring classNameObj, jobject loaderObj, jstring tokenObj, jboolean doFind, jboolean doStore, jbyteArray romClassCookie) 
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9VMThread* vmThread = ((J9VMThread*)env);
	J9JavaVM* vm = vmThread->javaVM;
	const char* nameChars = NULL;
	const char* tokenChars = NULL;
	jsize nameLen = 0;
	jsize tokenLen = 0;
	J9ROMClass* romClass = NULL;
	J9ClassPathEntry* token = NULL;
	UDATA oldState;
	omrthread_monitor_t jclCacheMutex;
	J9ClassLoader* classloader;

	Trc_JCL_com_ibm_oti_shared_SharedClassTokenHelperImpl_findSharedClassImpl_Entry(env, helperID);

	jclCacheMutex = vm->sharedClassConfig->jclCacheMutex;

	if ((helperID > 0xFFFF) || (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_ACCESS)) {
		Trc_JCL_com_ibm_oti_shared_SharedClassTokenHelperImpl_findSharedClassImpl_ExitDeny(env);
		return FALSE;
	}

	oldState = ((J9VMThread*)env)->omrVMThread->vmState;
	((J9VMThread*)env)->omrVMThread->vmState = J9VMSTATE_SHAREDCLASS_FIND;

	vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	classloader = J9VMJAVALANGCLASSLOADER_VMREF(vmThread, J9_JNI_UNWRAP_REFERENCE(loaderObj));
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	if (!getStringPair(env, &nameChars, &nameLen, &tokenChars, &tokenLen, classNameObj, tokenObj)) {
		goto _errorPostNameToken;
	}

	omrthread_monitor_enter(jclCacheMutex);

	token = getCachedToken(env, helperID, tokenChars, tokenLen);

	if (!token) {
		if (!createToken(env, helperID, &token, tokenChars, tokenLen)) {
			goto _errorWithMutex;
		}
	}

	omrthread_monitor_exit(jclCacheMutex);
	
	omrthread_monitor_enter(vm->classTableMutex);
	omrthread_monitor_enter(vm->classMemorySegments->segmentMutex);
	ALWAYS_TRIGGER_J9HOOK_VM_FIND_LOCALLY_DEFINED_CLASS(vm->hookInterface, (J9VMThread*)env, classloader, NULL,
			(const char*)nameChars, (UDATA)nameLen, token, 1, -1, NULL, !doFind, !doStore, NULL, romClass);
	omrthread_monitor_exit(vm->classMemorySegments->segmentMutex);
	omrthread_monitor_exit(vm->classTableMutex);

	releaseStringPair(env, classNameObj, nameChars, tokenObj, tokenChars);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	if (romClass) {
		createROMClassCookie(env, vm, romClass, romClassCookie);
		Trc_JCL_com_ibm_oti_shared_SharedClassTokenHelperImpl_findSharedClassImpl_ExitTrue(env);
		return TRUE;
	}

	Trc_JCL_com_ibm_oti_shared_SharedClassTokenHelperImpl_findSharedClassImpl_ExitFalse(env);
	return FALSE;

_errorWithMutex:
	omrthread_monitor_exit(jclCacheMutex);
_errorPostNameToken:	
	releaseStringPair(env, classNameObj, nameChars, tokenObj, tokenChars);
	(*env)->ExceptionClear(env);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	Trc_JCL_com_ibm_oti_shared_SharedClassTokenHelperImpl_findSharedClassImpl_ExitError(env);
#endif		/* J9VM_OPT_SHARED_CLASSES */
	return FALSE;
}

jint JNICALL 
Java_com_ibm_oti_shared_SharedClassTokenHelperImpl_storeSharedClassImpl2(JNIEnv* env, jobject thisObj, jint helperID, 
		jobject loaderObj, jstring tokenObj, jclass clazzObj, jbyteArray nativeFlags) 
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM* vm;
	J9VMThread *vmThread;
	J9ClassPathEntry* token = NULL;
	const char* tokenChars = NULL;
	jsize tokenLen = 0;
	J9ROMClass* romClass = NULL;
	J9ROMClass* newROMClass = NULL;
	J9ClassLoader* classloader;
	UDATA oldState;
	jint result;
	omrthread_monitor_t jclCacheMutex;

	Trc_JCL_com_ibm_oti_shared_SharedClassTokenHelperImpl_storeSharedClassImpl_Entry(env, helperID);

	vmThread = (J9VMThread *)env;
	vm = vmThread->javaVM;
	jclCacheMutex = vm->sharedClassConfig->jclCacheMutex;

	if ((helperID > 0xFFFF) || (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_UPDATES)) {
		Trc_JCL_com_ibm_oti_shared_SharedClassTokenHelperImpl_storeSharedClassImpl_ExitDenyUpdates(env);
		return FALSE;
	}

	oldState = vmThread->omrVMThread->vmState;
	vmThread->omrVMThread->vmState = J9VMSTATE_SHAREDCLASS_STORE;

	vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	classloader = J9VMJAVALANGCLASSLOADER_VMREF(vmThread, J9_JNI_UNWRAP_REFERENCE(loaderObj));
	romClass = J9VM_J9CLASS_FROM_JCLASS(vmThread, clazzObj)->romClass;
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	if (!getStringChars(env, &tokenChars, &tokenLen, tokenObj)) {
		goto _error;
	}

	omrthread_monitor_enter(jclCacheMutex);

	token = getCachedToken(env, helperID, tokenChars, tokenLen);

	if (!token) {
		if (!createToken(env, helperID, &token, tokenChars, tokenLen)) {
			releaseStringChars(env, tokenObj, tokenChars);
			goto _errorWithMutex;
		}
	}

	omrthread_monitor_exit(jclCacheMutex);

	if (vm->sharedClassConfig != NULL) {
		SCAbstractAPI * sharedapi = (SCAbstractAPI *)(vm->sharedClassConfig->sharedAPIObject);
		omrthread_monitor_enter(vm->classTableMutex);
		newROMClass = sharedapi->jclUpdateROMClassMetaData(vmThread, classloader, token, 1, 0, NULL, romClass);
		omrthread_monitor_exit(vm->classTableMutex);
	}

	releaseStringChars(env, tokenObj, tokenChars);

	vmThread->omrVMThread->vmState = oldState;

	result = (newROMClass != NULL);
	
	if (!result && (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		notifyCacheFull(env, nativeFlags);		
	}

	Trc_JCL_com_ibm_oti_shared_SharedClassTokenHelperImpl_storeSharedClassImpl_ExitResult(env, result);
	return result;

_errorWithMutex:
	omrthread_monitor_exit(jclCacheMutex);
_error:
	(*env)->ExceptionClear(env);

	vmThread->omrVMThread->vmState = oldState;

	Trc_JCL_com_ibm_oti_shared_SharedClassTokenHelperImpl_storeSharedClassImpl_ExitError(env);
#endif 		/* J9VM_OPT_SHARED_CLASSES */
	return FALSE;
}

/*
 * store URL class and methodIDs in JCL_CACHE.
 */
void JNICALL
Java_com_ibm_oti_shared_SharedClassURLHelperImpl_init(JNIEnv *env, jclass clazz)
{
	getURLMethodIDs(env);
}

jboolean JNICALL 
Java_com_ibm_oti_shared_SharedClassURLHelperImpl_findSharedClassImpl3(JNIEnv* env, jobject thisObj, jint helperID, jstring partitionObj, 
		jstring classNameObj, jobject loaderObj, jobject urlObj, jboolean doFind, jboolean doStore, jbyteArray romClassCookie, jboolean minimizeUpdateChecks)
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9VMThread* vmThread = ((J9VMThread*)env);
	J9JavaVM* vm = vmThread->javaVM;
	const char* nameChars = NULL;
	const char* partitionChars = NULL;
	jsize nameLen = 0;
	jsize partitionLen = 0;
	jmethodID urlGetPathID = NULL;
	jmethodID urlGetProtocolID = NULL;
	J9ROMClass* romClass = NULL;
	J9ClassPathEntry* urlEntry = NULL;
	UDATA cpeType = 0;
	UDATA oldState;
	const J9UTF8* partition = NULL;
	omrthread_monitor_t jclCacheMutex;
	U_16 cpeStatus = minimizeUpdateChecks ? CPE_STATUS_IGNORE_ZIP_LOAD_STATE : 0;
	J9ClassLoader* classloader;
	URLElements urlElements = {0};
	
	Trc_JCL_com_ibm_oti_shared_SharedClassURLHelperImpl_findSharedClassImpl_Entry(env, helperID);

	jclCacheMutex = vm->sharedClassConfig->jclCacheMutex;

	if ((helperID > 0xFFFF) || (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_ACCESS)) {
		Trc_JCL_com_ibm_oti_shared_SharedClassURLHelperImpl_findSharedClassImpl_ExitDenyAccess(env);
		return FALSE;
	}

	oldState = ((J9VMThread*)env)->omrVMThread->vmState;
	((J9VMThread*)env)->omrVMThread->vmState = J9VMSTATE_SHAREDCLASS_FIND;

	vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	classloader = J9VMJAVALANGCLASSLOADER_VMREF(vmThread, J9_JNI_UNWRAP_REFERENCE(loaderObj));
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	urlGetPathID = JCL_CACHE_GET(env, MID_java_net_URL_getPath);
	if (NULL == urlGetPathID) {
		goto _error;
	}
	urlGetProtocolID = JCL_CACHE_GET(env, MID_java_net_URL_getProtocol);
	if (NULL == urlGetProtocolID) {
		goto _error;
	}

	if (!getPathProtocolFromURL(env, urlObj, urlGetPathID, urlGetProtocolID, &urlElements)) {
		goto _errorPostPathProtocol;
	}

	if (!getStringPair(env, &nameChars, &nameLen, &partitionChars, &partitionLen, classNameObj, partitionObj)) {
		goto _errorPostClassNamePartition;
	}
	if (!(cpeType = getCpeTypeForProtocol((char*)urlElements.protocolChars, urlElements.protocolLen, urlElements.pathChars, urlElements.pathLen))) {
		goto _errorPostClassNamePartition;
	}

	omrthread_monitor_enter(jclCacheMutex);

	if (!(urlEntry = getCachedURL(env, helperID, urlElements.pathChars, urlElements.pathLen, cpeType, cpeStatus))) {
		omrthread_monitor_exit(jclCacheMutex);
		goto _errorPostClassNamePartition;
	}

	if (partitionChars) {
		if (!getCachedString(env, partitionChars, partitionLen, &(vm->sharedClassConfig->jclStringFarm), &partition)) {
			omrthread_monitor_exit(jclCacheMutex);
			goto _errorPostClassNamePartition;
		}
	}

	omrthread_monitor_exit(jclCacheMutex);

	omrthread_monitor_enter(vm->classTableMutex);
	omrthread_monitor_enter(vm->classMemorySegments->segmentMutex);
	ALWAYS_TRIGGER_J9HOOK_VM_FIND_LOCALLY_DEFINED_CLASS(vm->hookInterface, (J9VMThread*)env, classloader, NULL,
			(const char*)nameChars, (UDATA)nameLen, urlEntry, 1, -1, partition, !doFind, !doStore, NULL, romClass);
	omrthread_monitor_exit(vm->classMemorySegments->segmentMutex);
	omrthread_monitor_exit(vm->classTableMutex);

	releaseStringPair(env, urlElements.pathObj, urlElements.pathChars, urlElements.protocolObj, urlElements.protocolChars);
	releaseStringPair(env, classNameObj, nameChars, partitionObj, partitionChars);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	if (romClass) {
		createROMClassCookie(env, vm, romClass, romClassCookie);
		Trc_JCL_com_ibm_oti_shared_SharedClassURLHelperImpl_findSharedClassImpl_ExitTrue(env);
		return TRUE;
	}
	Trc_JCL_com_ibm_oti_shared_SharedClassURLHelperImpl_findSharedClassImpl_ExitFalse(env);
	return FALSE;

_errorPostClassNamePartition:
	releaseStringPair(env, classNameObj, nameChars, partitionObj, partitionChars);
_errorPostPathProtocol:
	releaseStringPair(env, urlElements.pathObj, urlElements.pathChars, urlElements.protocolObj, urlElements.protocolChars);
_error:
	(*env)->ExceptionClear(env);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	Trc_JCL_com_ibm_oti_shared_SharedClassURLHelperImpl_findSharedClassImpl_ExitError(env);
#endif 		/* J9VM_OPT_SHARED_CLASSES */
	return FALSE;
}

jint JNICALL 
Java_com_ibm_oti_shared_SharedClassURLHelperImpl_storeSharedClassImpl3(JNIEnv* env, jobject thisObj, jint helperID, 
		jstring partitionObj, jobject loaderObj, jobject urlObj, jclass clazzObj, jboolean minimizeUpdateChecks, jbyteArray nativeFlags)
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9VMThread *vmThread;
	J9JavaVM* vm;
	const char* partitionChars = NULL;
	jsize partitionLen = 0;
	jmethodID urlGetPathID = NULL;
	jmethodID urlGetProtocolID = NULL;
	J9ROMClass* romClass = NULL;
	J9ROMClass* newROMClass = NULL;
	J9ClassPathEntry* urlEntry = NULL;
	UDATA cpeType = 0;
	J9ClassLoader* classloader;
	UDATA oldState;
	jint result;
	const J9UTF8* partition = NULL;
	omrthread_monitor_t jclCacheMutex;
	U_16 cpeStatus = minimizeUpdateChecks ? CPE_STATUS_IGNORE_ZIP_LOAD_STATE : 0;
	URLElements urlElements = {0};

	Trc_JCL_com_ibm_oti_shared_SharedClassURLHelperImpl_storeSharedClassImpl_Entry(env, helperID);

	vmThread = (J9VMThread *)env;
	vm = vmThread->javaVM;
	jclCacheMutex = vm->sharedClassConfig->jclCacheMutex;

	if ((helperID > 0xFFFF) || (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_UPDATES)) {
		Trc_JCL_com_ibm_oti_shared_SharedClassURLHelperImpl_storeSharedClassImpl_ExitDenyUpdates(env);
		return FALSE;
	}

	oldState = vmThread->omrVMThread->vmState;
	vmThread->omrVMThread->vmState = J9VMSTATE_SHAREDCLASS_STORE;

	vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	classloader = J9VMJAVALANGCLASSLOADER_VMREF(vmThread, J9_JNI_UNWRAP_REFERENCE(loaderObj));
	romClass = J9VM_J9CLASS_FROM_JCLASS(vmThread, clazzObj)->romClass;
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	urlGetPathID = JCL_CACHE_GET(env, MID_java_net_URL_getPath);
	if (NULL == urlGetPathID) {
		goto _error;
	}
	urlGetProtocolID = JCL_CACHE_GET(env, MID_java_net_URL_getProtocol);
	if (NULL == urlGetProtocolID) {
		goto _error;
	}

	if (!getPathProtocolFromURL(env, urlObj, urlGetPathID, urlGetProtocolID, &urlElements)) {
		goto _errorPostPathProtocol;
	}

	if (!getStringChars(env, &partitionChars, &partitionLen, partitionObj)) {
		goto _errorPostPathProtocol;
	}
	if (!(cpeType = getCpeTypeForProtocol((char*)urlElements.protocolChars, urlElements.protocolLen, urlElements.pathChars, urlElements.pathLen))) {
		goto _errorPostPartition;
	}

	omrthread_monitor_enter(jclCacheMutex);

	if (!(urlEntry = getCachedURL(env, helperID, urlElements.pathChars, urlElements.pathLen, cpeType, cpeStatus))) {
		omrthread_monitor_exit(jclCacheMutex);
		goto _errorPostPartition;
	}

	if (partitionChars) {
		if (!getCachedString(env, partitionChars, partitionLen, &(vm->sharedClassConfig->jclStringFarm), &partition)) {
			omrthread_monitor_exit(jclCacheMutex);
			goto _errorPostPartition;
		}
	}

	omrthread_monitor_exit(jclCacheMutex);

	if (vm->sharedClassConfig != NULL) {
		SCAbstractAPI * sharedapi = (SCAbstractAPI *)(vm->sharedClassConfig->sharedAPIObject);
		omrthread_monitor_enter(vm->classTableMutex);
		newROMClass = sharedapi->jclUpdateROMClassMetaData((J9VMThread*)env, classloader, urlEntry, 1, 0, partition, romClass);
		omrthread_monitor_exit(vm->classTableMutex);
	}

	releaseStringPair(env, urlElements.pathObj, urlElements.pathChars, urlElements.protocolObj, urlElements.protocolChars);
	releaseStringChars(env, partitionObj, partitionChars);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	result = (newROMClass != NULL);

	if (!result && (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		notifyCacheFull(env, nativeFlags);		
	}

	Trc_JCL_com_ibm_oti_shared_SharedClassURLHelperImpl_storeSharedClassImpl_ExitResult(env, result);
	return result;

_errorPostPartition:
	releaseStringChars(env, partitionObj, partitionChars);
_errorPostPathProtocol:
	releaseStringPair(env, urlElements.pathObj, urlElements.pathChars, urlElements.protocolObj, urlElements.protocolChars);
_error:
	(*env)->ExceptionClear(env);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	Trc_JCL_com_ibm_oti_shared_SharedClassURLHelperImpl_storeSharedClassImpl_ExitError(env);
#endif 		/* J9VM_OPT_SHARED_CLASSES */
	return FALSE;
}

/*
 * store URL class and methodIDs in JCL_CACHE.
 */
void JNICALL
Java_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_init(JNIEnv *env, jclass clazz)
{
	getURLMethodIDs(env);
}

jint JNICALL 
Java_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_storeSharedClassImpl2(JNIEnv* env, jobject thisObj, jint helperID, 
		jstring partitionObj, jobject loaderObj, jobjectArray urlArrayObj, jint urlCount, jint cpLoadIndex, jclass clazzObj, jbyteArray nativeFlags)
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9VMThread *vmThread;
	J9JavaVM* vm;
	J9ClassPathEntry* cpEntries = NULL;
	const char* partitionChars = NULL;
	jsize partitionLen = 0;
	J9ROMClass* romClass = NULL;
	J9ROMClass* newROMClass = NULL;
	UDATA entryCount = (UDATA)urlCount;
	J9ClassLoader* classloader;
	UDATA oldState;
	jint result;
	const J9UTF8* partition = NULL;
	omrthread_monitor_t jclCacheMutex;
	URLElements* urlArrayElements = NULL;
	IDATA i = 0;
	jmethodID urlGetPathID = NULL;
	jmethodID urlGetProtocolID = NULL;

	PORT_ACCESS_FROM_VMC((J9VMThread*)env);

	Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_storeSharedClassImpl_Entry(env, helperID);

	vmThread = (J9VMThread *)env;
	vm = vmThread->javaVM;
	jclCacheMutex = vm->sharedClassConfig->jclCacheMutex;

	if ((helperID > 0xFFFF) || (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_UPDATES)) {
		Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_storeSharedClassImpl_ExitDenyUpdates(env);
		return FALSE;
	}

	oldState = vmThread->omrVMThread->vmState;
	vmThread->omrVMThread->vmState = J9VMSTATE_SHAREDCLASS_STORE;

	vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	classloader = J9VMJAVALANGCLASSLOADER_VMREF(vmThread, J9_JNI_UNWRAP_REFERENCE(loaderObj));
	romClass = J9VM_J9CLASS_FROM_JCLASS(vmThread, clazzObj)->romClass;
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	if (!getStringChars(env, &partitionChars, &partitionLen, partitionObj)) {
		goto _error;
	}

	urlGetPathID = JCL_CACHE_GET(env, MID_java_net_URL_getPath);
	if (NULL == urlGetPathID) {
		goto _error;
	}
	urlGetProtocolID = JCL_CACHE_GET(env, MID_java_net_URL_getProtocol);
	if (NULL == urlGetProtocolID) {
		goto _error;
	}

	if (NULL == classloader->classPathEntries) {
		urlArrayElements = (URLElements *)j9mem_allocate_memory(urlCount * sizeof(URLElements), J9MEM_CATEGORY_VM_JCL);
		if (NULL == urlArrayElements) {
			goto _error;
		}
		memset(urlArrayElements, 0, urlCount * sizeof(URLElements));

		for (i = 0; i < urlCount; i++) {
			jobject url = (*env)->GetObjectArrayElement(env, urlArrayObj, (jsize)i);
			if (JNI_TRUE == (*env)->ExceptionCheck(env)) {
				goto _errorFreeURLElements;
			}

			if (!getPathProtocolFromURL(env, url, urlGetPathID, urlGetProtocolID, urlArrayElements + i)) {
				goto _errorFreeURLElements;
			}
		}
	}

	omrthread_monitor_enter(jclCacheMutex);

	cpEntries = classloader->classPathEntries;

	if (!cpEntries) {
		if (!createCPEntries(env, helperID, urlCount, &cpEntries, urlArrayElements)) {
			if (cpEntries) {
				j9mem_free_memory(cpEntries);
			}
			goto _errorWithMutex;
		} else {
			classloader->classPathEntries = cpEntries;
		}
	}

	if (partitionChars) {
		if (!getCachedString(env, partitionChars, partitionLen, &(vm->sharedClassConfig->jclStringFarm), &partition)) {
			goto _errorWithMutex;
		}
	}

	omrthread_monitor_exit(jclCacheMutex);
	
	if (vm->sharedClassConfig != NULL) {
		SCAbstractAPI * sharedapi = (SCAbstractAPI *)(vm->sharedClassConfig->sharedAPIObject);
		omrthread_monitor_enter(vm->classTableMutex);
		newROMClass = sharedapi->jclUpdateROMClassMetaData((J9VMThread*)env, classloader, cpEntries, entryCount, cpLoadIndex, partition, romClass);
		omrthread_monitor_exit(vm->classTableMutex);
	}

	if (NULL != urlArrayElements) {
		for (i = 0; i < urlCount; i++) {
			/* NULL check is done in releaseStringPair(), no need to do it here */
			releaseStringPair(env, urlArrayElements[i].pathObj, urlArrayElements[i].pathChars, urlArrayElements[i].protocolObj, urlArrayElements[i].protocolChars);
		}
		j9mem_free_memory(urlArrayElements);
	}

	releaseStringChars(env, partitionObj, partitionChars);

	vmThread->omrVMThread->vmState = oldState;

	result = (newROMClass != NULL);

	if (!result && (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL)) {
		notifyCacheFull(env, nativeFlags);		
	}

	Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_storeSharedClassImpl_ExitResult(env, result);
	return result;

_errorWithMutex:
	omrthread_monitor_exit(jclCacheMutex);
_errorFreeURLElements:
	if (NULL != urlArrayElements) {
		for (i = 0; i < urlCount; i++) {
			/* NULL check is done in releaseStringPair(), so no need to do it here */
			releaseStringPair(env, urlArrayElements[i].pathObj, urlArrayElements[i].pathChars, urlArrayElements[i].protocolObj, urlArrayElements[i].protocolChars);
		}
		j9mem_free_memory(urlArrayElements);
	}
_error:
	releaseStringChars(env, partitionObj, partitionChars);
	(*env)->ExceptionClear(env);

	vmThread->omrVMThread->vmState = oldState;

	Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_storeSharedClassImpl_ExitError(env);
#endif		/* J9VM_OPT_SHARED_CLASSES */
	return FALSE;
}

jint JNICALL 
Java_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl2(JNIEnv* env, jobject thisObj, jint helperID, 
		jstring partitionObj, jstring classNameObj, jobject loaderObj, jobjectArray urlArrayObj, jboolean doFind, jboolean doStore,  
		jint urlCount, jint confirmedCount, jbyteArray romClassCookie)
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9VMThread* vmThread = ((J9VMThread*)env);
	J9JavaVM* vm = vmThread->javaVM;
	const char* nameChars = NULL;
	const char* partitionChars = NULL;
	jsize nameLen = 0;
	jsize partitionLen = 0;
	J9ROMClass* romClass = NULL;
	J9ClassPathEntry* cpEntries = NULL;
	UDATA entryCount = (UDATA)urlCount;
	IDATA indexFoundAt = 0;
	UDATA oldState;
	const J9UTF8* partition = NULL;
	omrthread_monitor_t jclCacheMutex;
	J9ClassLoader* classloader;
	URLElements* urlArrayElements = NULL;
	IDATA i = 0;
	jmethodID urlGetPathID = NULL;
	jmethodID urlGetProtocolID = NULL;
	
	PORT_ACCESS_FROM_VMC((J9VMThread*)env);

	Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_Entry(env, helperID);

	jclCacheMutex = vm->sharedClassConfig->jclCacheMutex;

	if ((helperID > 0xFFFF) || (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_ACCESS)) {
		/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683 */		
		Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_ExitDenyAccess_Event(env, helperID);
		Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_ExitDenyAccess(env);
		return -1;
	}

	oldState = ((J9VMThread*)env)->omrVMThread->vmState;
	((J9VMThread*)env)->omrVMThread->vmState = J9VMSTATE_SHAREDCLASS_FIND;

	vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	classloader = J9VMJAVALANGCLASSLOADER_VMREF(vmThread, J9_JNI_UNWRAP_REFERENCE(loaderObj));
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	if (!getStringPair(env, &nameChars, &nameLen, &partitionChars, &partitionLen, classNameObj, partitionObj)) {
		Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_ExitError2_Event(env, helperID);
		goto _errorPostClassNamePartition;
	}

	urlGetPathID = JCL_CACHE_GET(env, MID_java_net_URL_getPath);
	if (NULL == urlGetPathID) {
		goto _errorPostClassNamePartition;
	}
	urlGetProtocolID = JCL_CACHE_GET(env, MID_java_net_URL_getProtocol);
	if (NULL == urlGetProtocolID) {
		goto _errorPostClassNamePartition;
	}

	if (NULL == classloader->classPathEntries) {
		urlArrayElements = (URLElements *)j9mem_allocate_memory(urlCount * sizeof(URLElements), J9MEM_CATEGORY_VM_JCL);
		if (NULL == urlArrayElements) {
			goto _errorPostClassNamePartition;
		}
		memset(urlArrayElements, 0, urlCount * sizeof(URLElements));

		for (i = 0; i < urlCount; i++) {
			jobject url = (*env)->GetObjectArrayElement(env, urlArrayObj, (jsize)i);
			if (JNI_TRUE == (*env)->ExceptionCheck(env)) {
				goto _errorFreeURLElements;
			}

			if (!getPathProtocolFromURL(env, url, urlGetPathID, urlGetProtocolID, urlArrayElements + i)) {
				goto _errorFreeURLElements;
			}
		}
	}

	omrthread_monitor_enter(jclCacheMutex);

	cpEntries = classloader->classPathEntries;

	if (!cpEntries) {
		if (!createCPEntries(env, helperID, urlCount, &cpEntries, urlArrayElements)) {
			if (cpEntries) {
				j9mem_free_memory(cpEntries);
			}
			Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_ExitError3_Event(env, helperID);
			goto _errorWithMutex;
		} else {
			classloader->classPathEntries = cpEntries;
		}
	}

	if (partitionChars) {
		if (!getCachedString(env, partitionChars, partitionLen, &(vm->sharedClassConfig->jclStringFarm), &partition)) {
			Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_ExitError4_Event(env, helperID);
			goto _errorWithMutex;
		}
	}

	omrthread_monitor_exit(jclCacheMutex);

	omrthread_monitor_enter(vm->classTableMutex);
	omrthread_monitor_enter(vm->classMemorySegments->segmentMutex);
	ALWAYS_TRIGGER_J9HOOK_VM_FIND_LOCALLY_DEFINED_CLASS(vm->hookInterface, (J9VMThread*)env, classloader, NULL,
			(const char*)nameChars, (UDATA)nameLen, cpEntries, entryCount, confirmedCount, partition, !doFind, !doStore, &indexFoundAt, romClass);
	omrthread_monitor_exit(vm->classMemorySegments->segmentMutex);
	omrthread_monitor_exit(vm->classTableMutex);

	if (NULL != urlArrayElements) {
		for (i = 0; i < urlCount; i++) {
			/* NULL check is done in releaseStringPair(), so no need to do it here */
			releaseStringPair(env, urlArrayElements[i].pathObj, urlArrayElements[i].pathChars, urlArrayElements[i].protocolObj, urlArrayElements[i].protocolChars);
		}
		j9mem_free_memory(urlArrayElements);
	}

	releaseStringPair(env, classNameObj, nameChars, partitionObj, partitionChars);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	if (romClass) {
		createROMClassCookie(env, vm, romClass, romClassCookie);
		/* trace event is at level 1 and trace exit message is at level 2 as per CMVC 155318/157683 */
		Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_ExitResult_Event(env, indexFoundAt, helperID);		
		Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_ExitResult(env, indexFoundAt);
		return (jint)indexFoundAt;
	}
	
	/* no level 1 trace event here due to performance problem stated in CMVC 155318/157683 */
	Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_ExitNoResult(env);
	return -1;

_errorWithMutex:
	omrthread_monitor_exit(jclCacheMutex);
_errorFreeURLElements:
	if (NULL != urlArrayElements) {
		for (i = 0; i < urlCount; i++) {
			/* NULL check is done in releaseStringPair(), no need to do it here */
			releaseStringPair(env, urlArrayElements[i].pathObj, urlArrayElements[i].pathChars, urlArrayElements[i].protocolObj, urlArrayElements[i].protocolChars);
		}
		j9mem_free_memory(urlArrayElements);
	}
_errorPostClassNamePartition:
	releaseStringPair(env, classNameObj, nameChars, partitionObj, partitionChars);
	(*env)->ExceptionClear(env);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	/*
	 * There is no level 1 trace event added here because a level 1 trace event would have already been
	 * printed before the code flow comes here.
	 */
	Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_findSharedClassImpl_ExitError(env);
#endif 		/* J9VM_OPT_SHARED_CLASSES */

	return -1;
}


jlong JNICALL 
Java_com_ibm_oti_shared_SharedClassStatistics_maxSizeBytesImpl(JNIEnv* env, jobject thisObj)
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM* vm;
	jlong result = 0;

	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_maxSizeBytesImpl_Entry(env);

	vm = ((J9VMThread*)env)->javaVM;
	if (vm->sharedClassConfig) {
		result = (jlong)vm->sharedClassConfig->getTotalUsableCacheBytes(vm);
	}

	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_maxSizeBytesImpl_Exit(env, result);

	return result;
#else
	return 0;
#endif
}

jlong JNICALL
Java_com_ibm_oti_shared_SharedClassStatistics_softmxBytesImpl(JNIEnv* env, jobject thisObj)
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM* vm = NULL;
	jlong result = -1;

	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_softmxBytesImpl_Entry(env);

	vm = ((J9VMThread*)env)->javaVM;
	if (NULL != vm->sharedClassConfig) {
		U_32 softmx = 0;
		
		vm->sharedClassConfig->getMinMaxBytes(vm, &softmx, NULL, NULL, NULL, NULL);
		if ((U_32)-1 != softmx) {
			result = (jlong)softmx;
		}
	}

	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_softmxBytesImpl_Exit(env, result);
	return result;
#else
	return -1;
#endif
}

jlong JNICALL
Java_com_ibm_oti_shared_SharedClassStatistics_minAotBytesImpl(JNIEnv* env, jobject thisObj)
{
	I_32 ret = -1;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	
	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_minAotBytesImpl_Entry(env);

	if (NULL != javaVM->sharedClassConfig) {
		javaVM->sharedClassConfig->getMinMaxBytes(javaVM, NULL, &ret, NULL, NULL, NULL);
	}
	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_minAotBytesImpl_Exit(env, ret);
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return (jlong)ret;
}

jlong JNICALL
Java_com_ibm_oti_shared_SharedClassStatistics_maxAotBytesImpl(JNIEnv* env, jobject thisObj)
{
	I_32 ret = -1;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	
	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_maxAotBytesImpl_Entry(env);

	if (NULL != javaVM->sharedClassConfig) {
		javaVM->sharedClassConfig->getMinMaxBytes(javaVM, NULL, NULL, &ret, NULL, NULL);
	}
	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_maxAotBytesImpl_Exit(env, ret);
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return (jlong)ret;
}

jlong JNICALL
Java_com_ibm_oti_shared_SharedClassStatistics_minJitDataBytesImpl(JNIEnv* env, jobject thisObj)
{
	I_32 ret = -1;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	
	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_minJitDataBytesImpl_Entry(env);

	if (NULL != javaVM->sharedClassConfig) {
		javaVM->sharedClassConfig->getMinMaxBytes(javaVM, NULL, NULL, NULL, &ret, NULL);
	}
	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_minJitDataBytesImpl_Exit(env, ret);
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return (jlong)ret;
}

jlong JNICALL
Java_com_ibm_oti_shared_SharedClassStatistics_maxJitDataBytesImpl(JNIEnv* env, jobject thisObj)
{
	I_32 ret = -1;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_maxJitDataBytesImpl_Entry(env);
	if (NULL != javaVM->sharedClassConfig) {
		javaVM->sharedClassConfig->getMinMaxBytes(javaVM, NULL, NULL, NULL, NULL, &ret);
	}
	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_maxJitDataBytesImpl_Exit(env, ret);
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return (jlong)ret;
}

jlong JNICALL 
Java_com_ibm_oti_shared_SharedClassStatistics_freeSpaceBytesImpl(JNIEnv* env, jobject thisObj)
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM* vm;
	jlong result = 0;

	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_freeSpaceBytesImpl_Entry(env);

	vm = ((J9VMThread*)env)->javaVM;
	if (vm->sharedClassConfig) {
		result = (jlong)vm->sharedClassConfig->getFreeSpaceBytes(vm);
	}

	Trc_JCL_com_ibm_oti_shared_SharedClassStatistics_freeSpaceBytesImpl_Exit(env, result);
	return result;
#else
	return 0;
#endif
}


void JNICALL 
Java_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_notifyClasspathChange2(JNIEnv* env, jobject thisObj, jobject classLoaderObj)
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9VMThread* vmThread = ((J9VMThread*)env);
	J9JavaVM* vm = vmThread->javaVM;
	J9ClassLoader* classloader;

	PORT_ACCESS_FROM_VMC((J9VMThread*)env);

	Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_notifyClasspathChange_Entry(env);

	vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	classloader = J9VMJAVALANGCLASSLOADER_VMREF(vmThread, J9_JNI_UNWRAP_REFERENCE(classLoaderObj));
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	omrthread_monitor_enter(vm->sharedClassConfig->jclCacheMutex);

	/* Remove all cached classpaths for this helper ID */
	if (NULL != classloader->classPathEntries) {
		J9Pool* cpCachePool = vm->sharedClassConfig->jclClasspathCache;
		J9GenericByID* cachePoolItem = (J9GenericByID *)classloader->classPathEntries->extraInfo;
		
		if (NULL != cachePoolItem->cpData) {
			vm->sharedClassConfig->freeClasspathData(vm, cachePoolItem->cpData);
		}
		pool_removeElement(cpCachePool, (void *)cachePoolItem);
	
		/* Free the classPathEntries before setting it to null */
		j9mem_free_memory(classloader->classPathEntries);
		classloader->classPathEntries = NULL;		
	}

	/* Tell the cache code to reset its local classpath cache array as this has likely been invalidated by the change */
	vm->sharedClassConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_DO_RESET_CLASSPATH_CACHE;

	omrthread_monitor_exit(vm->sharedClassConfig->jclCacheMutex);

	Trc_JCL_com_ibm_oti_shared_SharedClassURLClasspathHelperImpl_notifyClasspathChange_Exit(env);

#endif		/* J9VM_OPT_SHARED_CLASSES */
}


jobject JNICALL
Java_com_ibm_oti_shared_SharedDataHelperImpl_findSharedDataImpl(JNIEnv* env, jobject thisObj, jint helperID, jstring tokenObj) 
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM* vm;
	const char* tokenChars = NULL;
	jsize tokenLen = 0;
	UDATA oldState;
	jobject returnVal = NULL;
	J9SharedClassConfig* config;
	J9SharedDataDescriptor existingData;
	IDATA numElem = 0;

	Trc_JCL_com_ibm_oti_shared_SharedDataHelperImpl_findSharedDataImpl_Entry(env, helperID);

	vm = ((J9VMThread*)env)->javaVM;
	config = vm->sharedClassConfig;

	if ((helperID > 0xFFFF) || (config->runtimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_ACCESS)) {
		Trc_JCL_com_ibm_oti_shared_SharedDataHelperImpl_findSharedDataImpl_ExitDeny(env);
		return NULL;
	}

	oldState = ((J9VMThread*)env)->omrVMThread->vmState;
	((J9VMThread*)env)->omrVMThread->vmState = J9VMSTATE_SHAREDDATA_FIND;

	if (!getStringChars(env, &tokenChars, &tokenLen, tokenObj)) {
		goto _error;
	}

	omrthread_monitor_enter(config->jclCacheMutex);

	numElem = config->findSharedData((J9VMThread*)env, (const char*)tokenChars, (UDATA)tokenLen, J9SHR_DATA_TYPE_JCL, FALSE, &existingData, NULL);

	releaseStringChars(env, tokenObj, tokenChars);

	if (numElem == 1) {
		if (!(returnVal = createDirectByteBuffer(env, existingData.address, existingData.length))) {
			goto _errorWithMutex;
		}
	} else if (numElem > 1) {
		Trc_JCL_com_ibm_oti_shared_SharedDataHelperImpl_findSharedDataImpl_MultipleDataForKeyError(env);
	}

	omrthread_monitor_exit(config->jclCacheMutex);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	Trc_JCL_com_ibm_oti_shared_SharedDataHelperImpl_findSharedDataImpl_Exit(env, returnVal);
	return returnVal;

_errorWithMutex:
	omrthread_monitor_exit(config->jclCacheMutex);
_error:
	(*env)->ExceptionClear(env);

	((J9VMThread*)env)->omrVMThread->vmState = oldState;

	Trc_JCL_com_ibm_oti_shared_SharedDataHelperImpl_findSharedDataImpl_ExitError(env);
#endif		/* J9VM_OPT_SHARED_CLASSES */
	return NULL;
}


jobject JNICALL
Java_com_ibm_oti_shared_SharedDataHelperImpl_storeSharedDataImpl(JNIEnv* env, jobject thisObj, jobject loaderObj, jint helperID, jstring tokenObj, jobject byteBufferInput) 
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM* vm;
	J9VMThread* vmThread;
	const char* tokenChars = NULL;
	jsize tokenLen = 0;
	UDATA oldState;
	void* inputData = NULL;
	UDATA inputDataLen = 0;
	const void* cachedData = NULL;
	jobject returnVal = NULL;
	J9ClassLoader* classloader;
	J9SharedClassConfig* config;

	Trc_JCL_com_ibm_oti_shared_SharedDataHelperImpl_storeSharedDataImpl_Entry(env, helperID);

	vmThread = (J9VMThread*)env;
	vm = vmThread->javaVM;
	config = vm->sharedClassConfig;

	if ((helperID > 0xFFFF) || (config->runtimeFlags & J9SHR_RUNTIMEFLAG_DENY_CACHE_UPDATES)) {
		Trc_JCL_com_ibm_oti_shared_SharedDataHelperImpl_storeSharedDataImpl_ExitDenyUpdates(env);
		return NULL;
	}

	oldState = vmThread->omrVMThread->vmState;
	vmThread->omrVMThread->vmState = J9VMSTATE_SHAREDDATA_STORE;

	vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
	classloader = J9VMJAVALANGCLASSLOADER_VMREF(vmThread, J9_JNI_UNWRAP_REFERENCE(loaderObj));
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	if (!getStringChars(env, &tokenChars, &tokenLen, tokenObj)) {
		goto _error;
	}
	
	if (byteBufferInput==NULL) { /* caller wants to mark data stored against this token as stale */
		J9SharedDataDescriptor descriptor;

		descriptor.address = NULL;
		descriptor.length = 0;
		descriptor.type = J9SHR_DATA_TYPE_JCL;
		descriptor.flags = 0;
		/* cachedData = */
			config->storeSharedData(vmThread, (const char*)tokenChars, (UDATA)tokenLen, &descriptor);
	} else {
		inputData = (*env)->GetDirectBufferAddress(env, byteBufferInput);
		inputDataLen = (UDATA)((*env)->GetDirectBufferCapacity(env, byteBufferInput));

		if (inputData && (classloader->flags & J9CLASSLOADER_SHARED_CLASSES_ENABLED)) {
			J9SharedDataDescriptor descriptor;

			descriptor.address = inputData;
			descriptor.length = inputDataLen;
			descriptor.type = J9SHR_DATA_TYPE_JCL;
			descriptor.flags = 0;
			cachedData = config->storeSharedData(vmThread, (const char*)tokenChars, (UDATA)tokenLen, &descriptor);
		}
	}

	releaseStringChars(env, tokenObj, tokenChars);

	if (cachedData) {
		if (!(returnVal = createDirectByteBuffer(env, cachedData, inputDataLen))) {
			goto _error;
		}
	}

	vmThread->omrVMThread->vmState = oldState;

	Trc_JCL_com_ibm_oti_shared_SharedDataHelperImpl_storeSharedDataImpl_Exit(env, returnVal);
	return returnVal;

_error:
	(*env)->ExceptionClear(env);

	vmThread->omrVMThread->vmState = oldState;

	Trc_JCL_com_ibm_oti_shared_SharedDataHelperImpl_storeSharedDataImpl_ExitError(env);
#endif		/* J9VM_OPT_SHARED_CLASSES */
	return NULL;
}


jboolean JNICALL 
Java_com_ibm_oti_shared_SharedAbstractHelper_getIsVerboseImpl(JNIEnv* env, jobject thisObj) 
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM* vm;
	jboolean result = FALSE;

	Trc_JCL_com_ibm_oti_shared_SharedClassAbstractHelper_getIsVerboseImpl_Entry(env);

	vm = ((J9VMThread*)env)->javaVM;
	if (vm->sharedClassConfig != NULL) {
		result = (jboolean)J9_ARE_ALL_BITS_SET(vm->sharedClassConfig->verboseFlags, J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_HELPER);
	}

#if defined(J9SHR_UNIT_TEST)
	/* For now, run these tests using getIsVerboseImpl as a convenient hook as it will always be called on startup */
	runCorrectURLUnitTests(env);
#endif			/* J9SHR_UNIT_TEST */

	Trc_JCL_com_ibm_oti_shared_SharedClassAbstractHelper_getIsVerboseImpl_Exit(env, result);
	return result;
#else
	return FALSE;
#endif			/* J9VM_OPT_SHARED_CLASSES */
}

/*
 * store class and methodIDs in JCL_CACHE.
 */
void JNICALL
Java_com_ibm_oti_shared_SharedClassUtilities_init(JNIEnv *env, jclass clazz) {
#if defined(J9VM_OPT_SHARED_CLASSES)
	jclass javaClass;
	jmethodID mid;

	javaClass = (*env)->FindClass(env, "com/ibm/oti/shared/SharedClassCacheInfo");
	if (NULL == javaClass) {
		return;
	}
	javaClass = (*env)->NewGlobalRef(env, javaClass);
	if (!javaClass) {
		return;
	}
	JCL_CACHE_SET(env, CLS_com_ibm_oti_shared_SharedClassCacheInfo, javaClass);

	/* get methodID of SharedClassCacheInfo constructor */
	mid = (*env)->GetMethodID(env, javaClass, "<init>", "(Ljava/lang/String;ZZIIJIIZJJIJ)V");
	if (NULL == mid) {
		return;
	}
	JCL_CACHE_SET(env, MID_com_ibm_oti_shared_SharedClassCacheInfo_init, mid);

	javaClass = (*env)->FindClass(env, "java/util/ArrayList");
	if (NULL == javaClass) {
		return;
	}
	javaClass = (*env)->NewGlobalRef(env, javaClass);
	if (!javaClass) {
		return;
	}
	JCL_CACHE_SET(env, CLS_java_util_ArrayList, javaClass);

	mid = (*env)->GetMethodID(env, javaClass, "add", "(Ljava/lang/Object;)Z");
	if (NULL == mid) {
		return;
	}
	JCL_CACHE_SET(env, MID_java_util_ArrayList_add, mid);

	return;

#endif
}

/*
 * callback for j9shr_iterateSharedCache().
 * Adds a SharedClassCacheInfo object in the ArrayList.
 */
static IDATA
populateSharedCacheInfo(J9JavaVM *vm, J9SharedCacheInfo *event_data, void *user_data) {
#if defined(J9VM_OPT_SHARED_CLASSES)
	jstring cacheName;
	jclass javaClass;
	jmethodID mid;
	jobject sharedCacheInfoObject;
	jobject arrayList = (jobject)user_data;
	JNIEnv *env;

	env = (JNIEnv *)vm->internalVMFunctions->currentVMThread(vm);

	cacheName = (*env)->NewStringUTF(env, (const char *)event_data->name);
	if (NULL == cacheName) {
		return -1;
	}

	javaClass = JCL_CACHE_GET(env, CLS_com_ibm_oti_shared_SharedClassCacheInfo);
	mid = JCL_CACHE_GET(env, MID_com_ibm_oti_shared_SharedClassCacheInfo_init);

	sharedCacheInfoObject = (*env)->NewObject(env, javaClass, mid,
												cacheName,
												(jboolean)event_data->isCompatible,
												(jboolean)(J9PORT_SHR_CACHE_TYPE_PERSISTENT == (jboolean)event_data->cacheType),
												(jint)event_data->os_shmid,
												(jint)event_data->os_semid,
												(jlong)event_data->lastDetach,
												(jint)event_data->modLevel,
												(jint)event_data->addrMode,
												(jboolean)(0 < (IDATA)event_data->isCorrupt),
												(-1 == event_data->cacheSize) ? (jlong) -1 : (jlong) event_data->cacheSize,
												(-1 == event_data->freeBytes) ? (jlong) -1 : (jlong) event_data->freeBytes,
												(jint)event_data->cacheType,
												((UDATA)-1 == event_data->softMaxBytes) ? (jlong) -1 : (jlong) event_data->softMaxBytes
												);
	if (NULL == sharedCacheInfoObject) {
		return -1;
	}
	mid = JCL_CACHE_GET(env, MID_java_util_ArrayList_add);
	(*env)->CallVoidMethod(env, arrayList, mid, sharedCacheInfoObject);

	return 0;
#else
	return 0;
#endif
}

jint JNICALL
Java_com_ibm_oti_shared_SharedClassUtilities_getSharedCacheInfoImpl(JNIEnv *env, jclass clazz, jstring cacheDir, jint flags, jboolean useCommandLineValue, jobject arrayList) {
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *vm;
	const jbyte	*dir = NULL;
	I_32 result = 0;

	vm = ((J9VMThread *)env)->javaVM;

	if (NULL == vm->sharedCacheAPI) {
		result = SHARED_CLASSES_UTILITIES_DISABLED;
		goto exit;
	}

	if ((JNI_FALSE == useCommandLineValue) && (NULL != cacheDir)) {
		dir = (const jbyte *)(*env)->GetStringUTFChars(env, cacheDir, NULL);
		if (NULL == dir) {
			(*env)->ExceptionClear(env);
			vm->internalVMFunctions->throwNativeOOMError(env, 0, 0);
			result = -1;
			goto exit;
		}
	}

	result = (I_32)vm->sharedCacheAPI->iterateSharedCaches(vm, (const char *)dir, (UDATA)flags, useCommandLineValue, populateSharedCacheInfo, (void *)arrayList);
	if ((JNI_FALSE == useCommandLineValue) && (NULL != cacheDir)) {
		(*env)->ReleaseStringUTFChars(env, cacheDir, (const char *)dir);
	}

exit:
	return result;

#else
	return 0;
#endif
}

jint JNICALL
Java_com_ibm_oti_shared_SharedClassUtilities_destroySharedCacheImpl(JNIEnv *env, jclass clazz, jstring cacheDir, jint cacheType, jstring cacheName, jboolean useCommandLineValue)
{
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *vm;
	const jbyte	*dir = NULL;
	const jbyte	*name = NULL;
	I_32 result;

	vm = ((J9VMThread *)env)->javaVM;

	if (NULL == vm->sharedCacheAPI) {
		result = SHARED_CLASSES_UTILITIES_DISABLED;
		goto exit;
	}

	/* need to use cacheDir and cacheName only when useCommandLineValue is false */
	if (JNI_FALSE == useCommandLineValue) {
		if (NULL != cacheDir) {
			dir = (const jbyte *)(*env)->GetStringUTFChars(env, cacheDir, NULL);
			if (NULL == dir) {
				(*env)->ExceptionClear(env);
				vm->internalVMFunctions->throwNativeOOMError(env, 0, 0);
				result = -1;
				goto exit;
			}
		}

		if (NULL != cacheName) {
			name = (const jbyte *)(*env)->GetStringUTFChars(env, cacheName, NULL);
			if (NULL == name) {
				(*env)->ExceptionClear(env);
				vm->internalVMFunctions->throwNativeOOMError(env, 0, 0);
				result = -1;
				goto exit;
			}
		}
	}

	result = (I_32)vm->sharedCacheAPI->destroySharedCache(vm, (const char *)dir, (const char *)name, cacheType, useCommandLineValue);

exit:
	if (JNI_FALSE == useCommandLineValue) {
		if ((NULL != cacheDir) && (NULL != dir)) {
			(*env)->ReleaseStringUTFChars(env, cacheDir, (const char *)dir);
		}
		if ((NULL != cacheName) && (NULL != name)){
			(*env)->ReleaseStringUTFChars(env, cacheName, (const char *)name);
		}
	}

	return result;
#else
	return 0;
#endif
}

jboolean JNICALL
Java_com_ibm_oti_shared_Shared_isNonBootSharingEnabledImpl(JNIEnv* env, jclass clazz)
{
	jboolean ret = JNI_FALSE;
#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *vm = ((J9VMThread *)env)->javaVM;
	if (NULL != vm->sharedClassConfig) {
		if (J9_ARE_ALL_BITS_SET(vm->sharedClassConfig->runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_CACHE_NON_BOOT_CLASSES)) {
			ret = JNI_TRUE;
		}
	}
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return ret;
}


#if defined(J9SHR_UNIT_TEST)

static void
testCorrectURLPath(JNIEnv* env, char* test, char* expected)
{
	char expectedBuffer[128];
	char* expectedBufPtr = (char*)expectedBuffer;

	PORT_ACCESS_FROM_VMC((J9VMThread*)env);

	correctURLPath(env, (const char*)test, (jsize)strlen(test), &expectedBufPtr, NULL);

	j9tty_printf(PORTLIB, "Unit testing %s: Got %s: ", test, expectedBufPtr);
	if (strcmp(expected, expectedBufPtr)) {
		j9tty_printf(PORTLIB, "FAILED. Expected %s\n", expected);
	} else {
		j9tty_printf(PORTLIB, "PASSED\n");
	}
}


static void
runCorrectURLUnitTests(JNIEnv* env)
{
	testCorrectURLPath(env, "", "");
#if defined(WIN32)
	testCorrectURLPath(env, "C:/dir1", "C:\\dir1");
	testCorrectURLPath(env, "/C:/dir1", "C:\\dir1");
	testCorrectURLPath(env, "/C:/dir1/", "C:\\dir1");
	testCorrectURLPath(env, "///C:/dir1/", "C:\\dir1");
	testCorrectURLPath(env, "///C:/dir1/dir2/dir3", "C:\\dir1\\dir2\\dir3");
	testCorrectURLPath(env, "///C:/dir1/dir2/dir3/", "C:\\dir1\\dir2\\dir3");

	testCorrectURLPath(env, "C:/dir%201", "C:\\dir 1");
	testCorrectURLPath(env, "/C:/dir%201", "C:\\dir 1");
	testCorrectURLPath(env, "/C:/dir%201/", "C:\\dir 1");
	testCorrectURLPath(env, "/C:/dir%251/", "C:\\dir%1");
	testCorrectURLPath(env, "///C:/dir%201/", "C:\\dir 1");
	testCorrectURLPath(env, "///C:/dir%201/dir%202/dir%20%203", "C:\\dir 1\\dir 2\\dir  3");
	testCorrectURLPath(env, "///C:/dir%201/dir%202/dir%20%203/", "C:\\dir 1\\dir 2\\dir  3");
	testCorrectURLPath(env, "//UNCTest/subdir", "\\\\UNCTest\\subdir");
	testCorrectURLPath(env, "//UNCTest/sub%20dir", "\\\\UNCTest\\sub dir");
#else
	testCorrectURLPath(env, "/dir1", "/dir1");
	testCorrectURLPath(env, "/dir1/", "/dir1");
	testCorrectURLPath(env, "//dir1/", "//dir1");
	testCorrectURLPath(env, "/dir%201", "/dir 1");
	testCorrectURLPath(env, "/dir%251", "/dir%1");
	testCorrectURLPath(env, "/dir%201/", "/dir 1");
	testCorrectURLPath(env, "/dir%201/dir%202/dir%20%203/", "/dir 1/dir 2/dir  3");
#endif 		/* WIN32 */
}

#endif /* J9SHR_UNIT_TEST */

