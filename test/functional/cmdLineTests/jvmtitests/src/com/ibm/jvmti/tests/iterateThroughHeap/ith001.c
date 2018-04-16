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

#include "jvmti_test.h"

static agentEnv * env;

typedef struct testIterateThroughHeapData {
	JavaVM      * vm;
	jvmtiEnv    * jvmti_env;
	JNIEnv      * jni_env;
	jint          heapCallbackOk;
	jint          stringCallbackMatches;
	jint          arrayPrimitiveMatches;
	jint          fieldPrimitiveMatches;
} testIterateThroughHeapData;
 
static jint JNICALL testIterateThroughHeap_callback(jlong class_tag, jlong size, jlong* tag_ptr, jint length, void* user_data);

jint JNICALL
ith001(agentEnv * agent_env, char * args)
{
	jvmtiError err;
	jvmtiCapabilities capabilities;
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	if (!ensureVersion(agent_env, JVMTI_VERSION_1_1)) {
		return JNI_ERR;
	}   
  
	env = agent_env; 
	
	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}						

	return JNI_OK;
}




static jint JNICALL
testIterateThroughHeap_callback(jlong class_tag, jlong size, jlong* tag_ptr, jint length, void* user_data)
{
	testIterateThroughHeapData *userData = (testIterateThroughHeapData *) user_data;        

	/*
	printf("\theapIteration:\n\t\tsize=%d ", size);
	printf(" Array size=%d\n", length);
	*/

	if (class_tag != 0xc0decafe) {
		error(env, JVMTI_ERROR_INTERNAL, "IterateThroughHeap callback called for incorrect"
		                                 " class. class_tag=0x%x\n", class_tag);
		userData->heapCallbackOk = JNI_ERR;
	} else {
		userData->heapCallbackOk = JNI_OK;
	}

	return JVMTI_VISIT_OBJECTS;
}




/**
 * TODO: the array objects we want to intercept here should be first tagged by FollowReferences
 */ 
static jint JNICALL
testIterateThroughHeap_arrayPrimitiveCallback(jlong class_tag,
					      jlong size,
					      jlong* tag_ptr, 
					      jint element_count,
					      jvmtiPrimitiveType element_type,
					      const void* elements, 
					      void* user_data)
{
	int i;
	testIterateThroughHeapData *userData = (testIterateThroughHeapData *) user_data;
	jvmtiError err = JVMTI_ERROR_INTERNAL;

	jboolean testBooleanArray[] = { 1, 0, 1, 0, 1, 1, 0, 0 };
	jchar testCharArray[]     = { 'a', 'b', 'c', 'd' };
	jbyte testByteArray[]     = { 0x11, 0x22, 0x33, 0x44 };
	jshort testShortArray[]   = { 0x1000, 0x2000, 0x3000, 0x4000 };
	jint testIntArray[]       = { 0x10000, 0x20000, 0x30000, 0x40000 };
	jlong testLongArray[]     = { 0x100000, 0x200000, 0x300000, 0x400000 };
	jfloat testFloatArray[]   = { 1, 2, 3, 4, -1, -2, -3, -4 };
	jdouble testDoubleArray[] = { 1.0, 2.0, 3.0, 4.0, -1.0, -2.0, -3.0, -4.0 };


	iprintf(env, 200, 2, "\t\tArray primitive type=%d  count=%d tag=[%llx]\n", element_type, element_count, *tag_ptr);

	userData->arrayPrimitiveMatches++;

	switch (element_type) {

		case JVMTI_PRIMITIVE_TYPE_BOOLEAN:
			for (i = 0; i < element_count; i++) {
				iprintf(env, 200, 2, "0x%x ", *((jboolean *) elements + i));
				if (*((jboolean *) elements + i) != testBooleanArray[i]) {
					error(env, err, "Boolean array element mismatch at index %d\n", i);
				}
			}
			break;

		case JVMTI_PRIMITIVE_TYPE_BYTE:
			for (i = 0; i < element_count; i++) {
				iprintf(env, 200, 2, "0x%x ", *((jbyte *) elements + i));
				if (*((jbyte *) elements + i) != testByteArray[i]) {
					error(env, err, "Byte array element mismatch at index %d\n", i);
				}
			}
			break;

		case JVMTI_PRIMITIVE_TYPE_CHAR:
			for (i = 0; i < element_count; i++) {
				iprintf(env, 200, 2, "0x%x ", *((jchar *) elements + i));
				if (*((jchar *) elements + i) != testCharArray[i]) {
					error(env, err, "Char array element mismatch at index %d\n", i);
				}
			}
			break;

		case JVMTI_PRIMITIVE_TYPE_SHORT:
			for (i = 0; i < element_count; i++) {
				iprintf(env, 200, 2, "0x%x ", *((jshort *) elements + i));
				if (*((jshort *) elements + i) != testShortArray[i]) {
					error(env, err, "Short array element mismatch at index %d\n", i);
				}
			}
			break;

		case JVMTI_PRIMITIVE_TYPE_INT:
			for (i = 0; i < element_count; i++) {
				iprintf(env, 200, 2, "0x%x ", *((jint *) elements + i));
				if (*((jint *) elements + i) != testIntArray[i]) {
					error(env, err, "Int array element mismatch at index %d\n", i);
				}
			}
			break;

		case JVMTI_PRIMITIVE_TYPE_FLOAT:
			for (i = 0; i < element_count; i++) {
				iprintf(env, 200, 2, "%f ", *((jfloat *) elements + i));
				if (*((jfloat *) elements + i) != testFloatArray[i]) {
					error(env, err, "Float array element mismatch at index %d\n", i);
				}
			}
			break;

		case JVMTI_PRIMITIVE_TYPE_LONG:
			for (i = 0; i < element_count; i++) {
				iprintf(env, 200, 2, "%e ", *((jlong *) elements + i));
				if (*((jlong *) elements + i) != testLongArray[i]) {
					error(env, err, "Long array element mismatch at index %d\n", i);
				}
			}
			break;

		case JVMTI_PRIMITIVE_TYPE_DOUBLE:
			for (i = 0; i < element_count; i++) {
				iprintf(env, 200, 2, "0x%x ", *((jdouble *) elements + i));
				if (*((jdouble *) elements + i) != testDoubleArray[i]) {
					error(env, err, "Double array element mismatch at index %d\n", i);
				}
			}
			break;
		default:
			error(env, err, "Unknown primitive type %d\n", element_type);
			break;
	}


	return JVMTI_VISIT_OBJECTS;
}

static jint JNICALL
testIterateThroughHeap_primitiveFieldCallback(jvmtiHeapReferenceKind reference_kind,
					      const jvmtiHeapReferenceInfo* referrer_info,
					      jlong class_tag,
					      jlong* tag_ptr,
					      jvalue value, 
					      jvmtiPrimitiveType value_type,
					      void* user_data)
{
	jvmtiError err = JVMTI_ERROR_INTERNAL;
	testIterateThroughHeapData *userData = (testIterateThroughHeapData *) user_data;


	/* We should be called only for the object that instantiates the values we are 
	 * looking for. This is done by calling the Iteration API with the right class 
	 * parameter filter */

	userData->fieldPrimitiveMatches++;

	switch (value_type) {
		case JVMTI_PRIMITIVE_TYPE_BOOLEAN:
			if (value.z != 1) {
				/* TODO: error */
				error(env, err, "Invalid Boolean primitive value.\n");
			}
			break;

		case JVMTI_PRIMITIVE_TYPE_BYTE:
			if (value.b != 0x44) {
				/* TODO: error */
				error(env, err, "Invalid Byte primitive value [0x%x].\n", value.b);
			}        		
			break;

		case JVMTI_PRIMITIVE_TYPE_CHAR:
			if (value.c != 0x55) {
				/* TODO: error */
				error(env, err, "Invalid Char primitive value [0x%x].\n", value.c);
			}  
			break;

		case JVMTI_PRIMITIVE_TYPE_SHORT:
			if (value.s != 0x5566) {
				/* TODO: error */
				error(env, err, "Invalid Short primitive value.\n");
			}  
			break;

		case JVMTI_PRIMITIVE_TYPE_INT:
			if (value.i != 0x66667777) {
				/* TODO: error */
				error(env, err, "Invalid Int primitive value.\n");
			}  
			break;

		case JVMTI_PRIMITIVE_TYPE_FLOAT:
			if (value.f != -10) {
				/* TODO: error */
				error(env, err, "Invalid Float primitive value.\n");
			} 
			break;

		case JVMTI_PRIMITIVE_TYPE_LONG:
			if (value.j != 0xc0fec0dedeadbeefLL) {
				/* TODO: error */
				error(env, err, "Invalid Long primitive value.\n");
			}        	
			break;

		case JVMTI_PRIMITIVE_TYPE_DOUBLE:
			if (value.d != -20.0) {
				/* TODO: error */
				error(env, err, "Invalid Double primitive value.\n");
			}
			break;
			
		default:
			error(env, JVMTI_ERROR_INTERNAL, "Unknown primitive type [%d]\n", value_type);
			break;			
	}

	return JVMTI_VISIT_OBJECTS;
}

static jint JNICALL
testIterateThroughHeap_stringPrimitiveCallback(jlong class_tag, 
					       jlong size, 
					       jlong* tag_ptr, 
					       const jchar* value, 
					       jint value_length, 
					       void* user_data)
{
	testIterateThroughHeapData *userData = (testIterateThroughHeapData *) user_data;
	char testString[] = "testStringContent";
	char *passedString = (char *) value;
	int i;
	int len = (int)strlen(testString);
	
	if (value_length != len) {
		return JVMTI_VISIT_OBJECTS;
	}
	
	for (i = 0; i < len; i++) {
		if (value[i] != (jchar) (testString[i])) {
			return JVMTI_VISIT_OBJECTS;
		}
	}
	
	userData->stringCallbackMatches++;
	
	return JVMTI_VISIT_OBJECTS;
}



jboolean JNICALL
Java_com_ibm_jvmti_tests_iterateThroughHeap_ith001Sub_testHeapIteration(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError err;
	jvmtiHeapCallbacks callbacks;
	testIterateThroughHeapData userData;
	JVMTI_ACCESS_FROM_AGENT(env);
	
	/* user data to be passed through to the callbacks */
	userData.jvmti_env = jvmti_env;
	userData.jni_env = jni_env;
	userData.heapCallbackOk = JNI_ERR;

	/* Drop a tag on this class object so we are able to recognize it within the callback */
	err = (*jvmti_env)->SetTag(jvmti_env, clazz, 0xc0decafe);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to SetTag");
		return JNI_FALSE;
	} 

	/* Test Heap Iteration Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.heap_iteration_callback = testIterateThroughHeap_callback;
	err = (*jvmti_env)->IterateThroughHeap(jvmti_env, JVMTI_HEAP_FILTER_TAGGED, clazz, &callbacks, &userData); 
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to IterateThroughHeap");
		return JNI_FALSE;
	}

	if (userData.heapCallbackOk == JNI_ERR) {
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_iterateThroughHeap_ith001Sub_testArrayPrimitive(JNIEnv * jni_env, jobject obj)
{
	jvmtiError err;
	jvmtiHeapCallbacks callbacks;
	testIterateThroughHeapData userData;
	JVMTI_ACCESS_FROM_AGENT(env);


	/* user data to be passed through to the callbacks */
	userData.jvmti_env = jvmti_env;
	userData.jni_env = jni_env;

	userData.arrayPrimitiveMatches = 0;

	/* Test Array Primitive Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.array_primitive_value_callback = testIterateThroughHeap_arrayPrimitiveCallback;
	err = (*jvmti_env)->IterateThroughHeap(jvmti_env, JVMTI_HEAP_FILTER_UNTAGGED, NULL, &callbacks, &userData); 
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to IterateThroughHeap (getting array primitive values)");
		return JNI_FALSE;
	}

	if (userData.arrayPrimitiveMatches != 8) {
		error(env, err, "Retrieved an incorrect number [%d] of array primitive elements", userData.arrayPrimitiveMatches);
		return JNI_FALSE;
	}


	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_iterateThroughHeap_ith001Sub_testFieldPrimitive(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError err;
	jvmtiHeapCallbacks callbacks;
	testIterateThroughHeapData userData;
	JVMTI_ACCESS_FROM_AGENT(env);
	

	/* user data to be passed through to the callbacks */
	userData.jvmti_env = jvmti_env;
	userData.jni_env = jni_env;

	/* Drop a tag on this class object so we are able to recognize it within the callback */
	err = (*jvmti_env)->SetTag(jvmti_env, clazz, 0xc0decafe);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to SetTag");
		return JNI_FALSE;
	} 
	
	userData.fieldPrimitiveMatches = 0;

	/* Test Primitive Field Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.primitive_field_callback = testIterateThroughHeap_primitiveFieldCallback;
	err = (*jvmti_env)->IterateThroughHeap(jvmti_env, JVMTI_HEAP_FILTER_TAGGED, clazz, &callbacks, &userData); 
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to IterateThroughHeap (getting primitive field values)");
		return JNI_FALSE;
	}

    if (userData.fieldPrimitiveMatches == -1) {
		error(env, err, "Retrieved an incorrect number [%d] of field primitive elements", userData.fieldPrimitiveMatches);
		return JNI_FALSE;
	}

	return JNI_TRUE;
}
  
jboolean JNICALL
Java_com_ibm_jvmti_tests_iterateThroughHeap_ith001Sub_testStringPrimitive(JNIEnv * jni_env, jclass clazz)
{
	jvmtiError err;
	jvmtiHeapCallbacks callbacks;
	testIterateThroughHeapData userData;
	JVMTI_ACCESS_FROM_AGENT(env);
	
	/* user data to be passed through to the callbacks */
	userData.jvmti_env = jvmti_env;
	userData.jni_env = jni_env;
	userData.stringCallbackMatches = 0;

	/* Drop a tag on this class object so we are able to recognize it within the callback */
	err = (*jvmti_env)->SetTag(jvmti_env, clazz, 0xc0decafe);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to SetTag");
		return JNI_FALSE;
	} 
 
	/* Test String Primitive Value Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.string_primitive_value_callback = testIterateThroughHeap_stringPrimitiveCallback;
	err = (*jvmti_env)->IterateThroughHeap(jvmti_env, 0, NULL, &callbacks, &userData); 
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to IterateThroughHeap (getting string primitive values)");
		return JNI_FALSE;
	}
		                           
	if (userData.stringCallbackMatches == 0) {
		error(env, JVMTI_ERROR_NOT_FOUND, "Failed to find com.ibm.jvmti.tests.iterateThroughHeap.uth001Sub->testString containing 'testStringContent'");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}
 
jboolean JNICALL
Java_com_ibm_jvmti_tests_iterateThroughHeap_ith001Sub_tagObject(JNIEnv * jni_env, jobject o, jlong tag, jobject taggedObject)
{
	jvmtiError err;
	JVMTI_ACCESS_FROM_AGENT(env);
	
	err = (*jvmti_env)->SetTag(jvmti_env, taggedObject, tag);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to SetTag");
		return JNI_FALSE;
	} 

	return JNI_TRUE;
}

