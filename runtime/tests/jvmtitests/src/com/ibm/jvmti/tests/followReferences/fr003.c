/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

#include "fr.h"

static agentEnv * _agentEnv;



static jstring stringData = NULL;
static jvmtiTestFollowRefsUserData userData;

jint JNICALL
fr003(agentEnv * env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiCapabilities capabilities;
	jvmtiError err;                                

	if (!ensureVersion(env, JVMTI_VERSION_1_1)) {
		return JNI_ERR;
	}   
       
	_agentEnv = env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}
	
	tagManager_initialize(env);
	
	return JNI_OK;
}


static jint JNICALL 
testFollowReferences_primitiveFieldCallback(jvmtiHeapReferenceKind reference_kind, const jvmtiHeapReferenceInfo* referrer_info, jlong class_tag, jlong* tag_ptr, jvalue value, jvmtiPrimitiveType value_type, void* user_data)
{
	
	if (*tag_ptr != 0L) {
		queueTag(*tag_ptr);
		printf("testFollowReferences_primitiveFieldCallback tag %llx:%llx\n", class_tag, *tag_ptr);
	} else {
		if (class_tag != 0L) {
			printf("testFollowReferences_primitiveFieldCallback Class tag %llx\n", class_tag);
		}
	}
	
	return JVMTI_VISIT_OBJECTS;
}





#define checkArrayElements(type, data, goodArray, passedArray) \
	do { \
		int i; \
		for (i = 0; i < data->arraySize; i++) { \
			if (((type *) elements)[i] != ((type *) data->arrayElements)[i]) { \
				error(_agentEnv, JVMTI_ERROR_INVALID_OBJECT, "Array slot %d does not match. Should be %x instead of %x", i, ((type *) data->arrayElements)[i], ((type *) elements)[i]); \
				return JVMTI_VISIT_ABORT; \
			} \
		} \
	} while(0)

#define checkArrayTag(tag) \
	do { \
		if (*tag_ptr != tag) { \
			error(_agentEnv, JVMTI_ERROR_TYPE_MISMATCH, "Expected tag [0x%llx] got [0x%llx]", tag, *tag_ptr); \
			return JVMTI_VISIT_ABORT; \
		} \
	} while(0)

static jint JNICALL 
testFollowReferences_arrayPrimitiveCallback(jlong class_tag, jlong size, jlong * tag_ptr, jint element_count, jvmtiPrimitiveType element_type, const void* elements, void* user_data)
{
	
	jvmtiTestFollowRefsUserData * data = (jvmtiTestFollowRefsUserData *) user_data;
	
	if (*tag_ptr != 0L || class_tag != 0L) {
		printf("testFollowReferences_arrayPrimitiveCallback tag %llx:%llx\n", class_tag, *tag_ptr);
	}
	
	if (*tag_ptr != 0L) {
		queueTag(*tag_ptr);		
	} else {		
		return JVMTI_VISIT_OBJECTS;
	}

	switch (data->primitiveArrayType) {
		case JVMTI_PRIMITIVE_TYPE_BOOLEAN:
			checkArrayTag(0x1000L);
			checkArrayElements(jboolean, data, data->arrayElements, elements);									
			break;
	    case JVMTI_PRIMITIVE_TYPE_BYTE:
			checkArrayTag(0x1001L);
			checkArrayElements(jbyte, data, data->arrayElements, elements);									
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_CHAR:
			checkArrayTag(0x1002L);
			checkArrayElements(jchar, data, data->arrayElements, elements);									
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_SHORT:
			checkArrayTag(0x1003L);
			checkArrayElements(jshort, data, data->arrayElements, elements);									
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_INT:
			checkArrayTag(0x1004L);
			checkArrayElements(jint, data, data->arrayElements, elements);						
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_LONG:
			checkArrayTag(0x1005L);
			checkArrayElements(jlong, data, data->arrayElements, elements);									
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_FLOAT:
			checkArrayTag(0x1006L);
			checkArrayElements(jfloat, data, data->arrayElements, elements);									
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_DOUBLE:
			checkArrayTag(0x1007L);
			checkArrayElements(jdouble, data, data->arrayElements, elements);									
	    	break;
						
		default:
			error(_agentEnv, JVMTI_ERROR_INTERNAL, "Unknown array primitive value type [%d]", data->primitiveArrayType);
			return JVMTI_VISIT_ABORT;			
	}
	
	return JVMTI_VISIT_OBJECTS;
}


static jint JNICALL 
testFollowReferences_stringPrimitiveCallback(jlong class_tag, jlong size, jlong* tag_ptr, const jchar* value, jint value_length, void* user_data)
{
	JVMTI_ACCESS_FROM_AGENT(_agentEnv);
		
	if (*tag_ptr != 0L) {
		jvmtiError err;
		jvmtiTestFollowRefsUserData * data = (jvmtiTestFollowRefsUserData *) user_data;

		queueTag(*tag_ptr);
		queueTag(class_tag);
		
		data->stringLen = value_length;
		
		err = (*jvmti_env)->Allocate(jvmti_env, value_length * 2, (unsigned char **) &data->string);
		if (err != JVMTI_ERROR_NONE) {
			error(_agentEnv, err, "Allocate of %d bytes failed", value_length * 2);
			return JVMTI_VISIT_ABORT;		
		}
				
		memcpy(data->string, value, value_length * 2);
		
		printf("testFollowReferences_stringPrimitiveCallback  tag %llx:%llx\n", class_tag, *tag_ptr);
	}
	
	return JVMTI_VISIT_OBJECTS;
}

static jint JNICALL
testFollowReferences_heapRefCallback(jvmtiHeapReferenceKind refKind,
			      const jvmtiHeapReferenceInfo* refInfo,
			      jlong classTag,
			      jlong referrerClassTag,
			      jlong size,
			      jlong* tagPtr,
			      jlong* referrerTagPtr,
			      jint length,
			      void* userData)

{
	if (classTag != 0L || *tagPtr != 0L) {
		printf("testFollowReferences_heapRefCallback %llx:%llx\n", classTag, *tagPtr);
	}
	
	if (refKind == JVMTI_HEAP_REFERENCE_STACK_LOCAL) {
		printf("\tdepth=0x%x  method=0x%p  location=0x%llx slot=0x%x\n",
		       refInfo->stack_local.depth,
		       refInfo->stack_local.method,
		       refInfo->stack_local.location,
		       refInfo->stack_local.slot);
	}


	return JVMTI_VISIT_OBJECTS;
}

static void *
getArrayElements(JNIEnv * jni_env, jvmtiPrimitiveType primitiveType, jint primitiveArraySize, jobject array)
{
	void *arrayElements = NULL;

	switch (primitiveType) {
		case JVMTI_PRIMITIVE_TYPE_BOOLEAN:
			arrayElements = (*jni_env)->GetBooleanArrayElements(jni_env, array, NULL);
			break;
	    case JVMTI_PRIMITIVE_TYPE_BYTE:
	    	arrayElements = (*jni_env)->GetByteArrayElements(jni_env, array, NULL);
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_CHAR:
	    	arrayElements = (*jni_env)->GetCharArrayElements(jni_env, array, NULL);
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_SHORT:
	    	arrayElements = (*jni_env)->GetShortArrayElements(jni_env, array, NULL);
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_INT:
	    	arrayElements = (*jni_env)->GetIntArrayElements(jni_env, array, NULL);
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_LONG:
	    	arrayElements = (*jni_env)->GetLongArrayElements(jni_env, array, NULL);
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_FLOAT:
	    	arrayElements = (*jni_env)->GetFloatArrayElements(jni_env, array, NULL);
	    	break;
	    case JVMTI_PRIMITIVE_TYPE_DOUBLE:
	    	arrayElements = (*jni_env)->GetDoubleArrayElements(jni_env, array, NULL);
	    	break;
	    default:
	    	error(_agentEnv, JVMTI_ERROR_INTERNAL, "Unknown array primitive value type [%d]", primitiveType);
	    	break;
	}

	return arrayElements;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_followReferences_fr003_followFromObject(JNIEnv * jni_env, jclass klass, jobject initialObject, jclass filterClass)
{
	jvmtiError err;	
	jvmtiHeapCallbacks callbacks;	
	JVMTI_ACCESS_FROM_AGENT(_agentEnv);
	jint errorCount;
	
	memset(&userData, 0x00, sizeof(jvmtiTestFollowRefsUserData));
	
	errorCount = getErrorCount();
	
	/* Test Follow References  Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.heap_reference_callback = testFollowReferences_heapRefCallback;
	callbacks.primitive_field_callback = testFollowReferences_primitiveFieldCallback;
	err = (*jvmti_env)->FollowReferences(jvmti_env, 0, filterClass, initialObject, &callbacks, (void *) &userData);
	if (err != JVMTI_ERROR_NONE) {
		error(_agentEnv, err, "FollowReferences failed");
		return JNI_FALSE;		
	}

	if (errorCount != getErrorCount()) {
		return JNI_FALSE;
	} 
	
	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_followReferences_fr003_followFromArrayObject(JNIEnv * jni_env, jclass klass, jobject initialObject, jclass filterClass, jint primitiveType, jint primitiveArraySize)
{
	jvmtiError err;	
	jvmtiHeapCallbacks callbacks;	
	JVMTI_ACCESS_FROM_AGENT(_agentEnv);
	jint errorCount;
	
	memset(&userData, 0x00, sizeof(jvmtiTestFollowRefsUserData));
	
	errorCount = getErrorCount();
	
	userData.primitiveArrayType = primitiveType;
	userData.arrayObject = initialObject;
	userData.arraySize = primitiveArraySize;
	userData.arrayElements = getArrayElements(jni_env, primitiveType, primitiveArraySize, initialObject);
			
	/* Test Follow References  Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.heap_reference_callback = testFollowReferences_heapRefCallback;	
	callbacks.array_primitive_value_callback = testFollowReferences_arrayPrimitiveCallback;
	err = (*jvmti_env)->FollowReferences(jvmti_env, 0, filterClass, initialObject, &callbacks, &userData);
	if (err != JVMTI_ERROR_NONE) {
		error(_agentEnv, err, "FollowReferences failed");
		return JNI_FALSE;		
	}
		
	if (errorCount != getErrorCount()) {
		return JNI_FALSE;
	} 
	
	return JNI_TRUE;
}



jstring JNICALL
Java_com_ibm_jvmti_tests_followReferences_fr003_getStringData(JNIEnv * jni_env, jclass klass)
{
	stringData = (*jni_env)->NewString(jni_env, userData.string, userData.stringLen);
	if (NULL == stringData) {
		error(_agentEnv, JVMTI_ERROR_INTERNAL, "Unable to create a return string");
		return NULL;
	}
	
	return stringData;	
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_followReferences_fr003_followFromStringObject(JNIEnv * jni_env, jclass klass, jobject initialObject, jclass filterClass)
{
	jvmtiError err;
	jvmtiHeapCallbacks callbacks;	
	JVMTI_ACCESS_FROM_AGENT(_agentEnv);
	jint errorCount;
	jint filterTag = JVMTI_HEAP_FILTER_UNTAGGED | JVMTI_HEAP_FILTER_CLASS_UNTAGGED;
	
	errorCount = getErrorCount();
	stringData = NULL;
	
	/* Test Follow References  Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	memset(&userData, 0x00, sizeof(jvmtiTestFollowRefsUserData));
	callbacks.string_primitive_value_callback = testFollowReferences_stringPrimitiveCallback;
	err = (*jvmti_env)->FollowReferences(jvmti_env, filterTag, filterClass, initialObject, &callbacks, (void *) &userData);
	if (err != JVMTI_ERROR_NONE) {
		error(_agentEnv, err, "FollowReferences failed");
		goto fail;		
	}

	if (errorCount != getErrorCount()) {
		goto fail;
	} 

	
	return JNI_TRUE;
	
fail:

	if (userData.string) {
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) userData.string);
		if (err != JVMTI_ERROR_NONE) {
			error(_agentEnv, err, "Deallocate failed");
		}
	}
	
	return JNI_FALSE;
}




