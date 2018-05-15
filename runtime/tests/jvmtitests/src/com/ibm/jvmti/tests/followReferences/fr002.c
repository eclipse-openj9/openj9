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
#include <stdlib.h>
#include <assert.h>

#include "jvmti_test.h"

static agentEnv * env;                                                    
static JNIEnv * jniEnv;

#define TAG_MASK 0xfffffff

static jlong tagPrefix = 0xf0000000;
static jint tagCount = 0;
        

#define RC \
	{ \
		if (rc != JVMTI_ERROR_NONE) {\
			abort(); \
		}\
	}


typedef struct reference {
	jlong referrerClassTag;
	jlong referrerTag;
	jlong classTag;
	jlong tag;
	jvmtiHeapReferenceKind kind;
	jint length;
	jvmtiHeapReferenceInfo info;
	struct reference * next;
} reference;
                       

typedef struct classField {
	jfieldID id;
	char *name;
	char *sig;
	jint modifiers;
	struct classField *next;
} classField;

typedef struct classDescription {
	jclass klass;
	jobject object;
	jlong tag;
	char *name;
	char *genericName;
	int fieldCount;
	struct classField *fields;
	struct classDescription *next;
} classDescription;
 
typedef struct object {
	jlong               tag;
	jobject             object;
    classDescription  * class;
	reference         * refList;
} object;
 

#define OBJECT_LIST_MAX 10000
static object ** objectList = NULL;


static const char *
heapReferenceKindString(jvmtiHeapReferenceKind refKind)
{
	static const char *referenceNames[] = {
		"",
		"CLASS",
		"FIELD",
		"ARRAY_ELEMENT",
		"CLASS_LOADER",
		"SIGNERS",
		"PROTECTION_DOMAIN",
		"INTERFACE",
		"STATIC_FIELD",
		"CONSTANT_POOL",
		"SUPERCLASS",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"JNI_GLOBAL",
		"SYSTEM_CLASS",
		"MONITOR",
		"STACK_LOCAL",
		"JNI_LOCAL",
		"THREAD",
		"OTHER",
	};

	if (refKind > JVMTI_HEAP_REFERENCE_OTHER)
		return referenceNames[0];

	return referenceNames[refKind];

}

jint JNICALL
fr002(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiCapabilities capabilities;
	jvmtiError err;                                

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}						

	return JNI_OK;
}


#define JVMTI_TEST_FR002_OBJECT_TAG		0xAAAA
#define JVMTI_TEST_FR002_CLASS_TAG		0xBBBB

typedef struct fr002_data {
	int foundJavaLangClass;
	int foundFR002Class;
} fr002_data;


unsigned int 
getTag(jlong tag)
{
	return (unsigned int) (tag);
}
    


static jboolean 
objectExists(jlong tag)
{
 	jint t = (jint)(tag & TAG_MASK);

    if (NULL != objectList[t])
		return JNI_TRUE;
	else
		return JNI_FALSE;
}

static void
addObject(object *o)
{
 	assert(!objectExists(o->tag));

	objectList[o->tag & TAG_MASK] = o;		
}

static void
addReference(reference *r)
{
	free(r);
}




static jint JNICALL
fr002HeapReferenceCallback(jvmtiHeapReferenceKind reference_kind,
						   const jvmtiHeapReferenceInfo* referrer_info,
						   jlong class_tag,
						   jlong referrer_class_tag,
						   jlong size,
						   jlong* tag_ptr,
						   jlong* referrer_tag_ptr,
						   jint length,
						   void* user_data)
{
	unsigned int t;
	reference *ref;
	object *obj;
	fr002_data * data = (fr002_data *) user_data;
 
	if (*tag_ptr == (jlong) 0) {
     	*tag_ptr = ((jlong) (tagPrefix + tagCount));
		tagCount++;
	}
 
	if (referrer_tag_ptr) {
		if (*referrer_tag_ptr == (jlong) 0) {
			*referrer_tag_ptr = ((jlong) (tagPrefix + tagCount));
			tagCount++;
		}
	}
 

	t = getTag(*tag_ptr);

    printf("\t\t[%08x:%08x]->[%08x:%08x] [%d %s] idx=%d\n",
		   (unsigned int) getTag(referrer_class_tag),
		   (referrer_tag_ptr) ? getTag(*referrer_tag_ptr) : 0,
		   getTag(class_tag), 
		   getTag(*tag_ptr),
		   reference_kind, heapReferenceKindString(reference_kind),
		   (referrer_info) ? referrer_info->field.index : -1
		   );

    ref = calloc(1, sizeof(reference));
	ref->referrerClassTag =  getTag(referrer_class_tag);
	ref->referrerTag = (referrer_tag_ptr) ? getTag(*referrer_tag_ptr) : 0;
	ref->classTag = getTag(class_tag);
	ref->tag = t;
	ref->kind = reference_kind;
	ref->length = length;
	if (referrer_info) {
		memcpy(&ref->info, referrer_info, sizeof(jvmtiHeapReferenceInfo));
	}

    addReference(ref);

	if (!objectExists(t)) {
		obj = calloc(1, sizeof(object));
		obj->tag = t;
		obj->class = NULL;
		addObject(obj);
	}

	return JVMTI_VISIT_OBJECTS;
}


static jint JNICALL
fr002PrimitiveFieldCallback(jvmtiHeapReferenceKind reference_kind,
							const jvmtiHeapReferenceInfo* referrer_info,
							jlong class_tag,
							jlong* tag_ptr,
							jvalue value,
							jvmtiPrimitiveType value_type,
							void* user_data)
{
	fr002_data * data = (fr002_data *) user_data;
    if (*tag_ptr == (jlong) 0) {
		abort();
	}

	printf("\t\t\t[%08x:%08x] [%d %s] idx=%d  vt=%d\n",
		   getTag(class_tag), 
		   getTag(*tag_ptr),
		   reference_kind, heapReferenceKindString(reference_kind),
		   referrer_info->field.index, value_type
		   ); 

	return JVMTI_VISIT_OBJECTS; 

}



static void
printClass(classDescription *c)
{
    classField *f;

   	printf("[%s][%s]  [%llx]\n", c->name, c->genericName, c->tag);

	f = c->fields;
	while (f) {
		printf("\t[%s][%s] [0x%08x]\n", f->name, f->sig, f->modifiers);
		f = f->next;
	}

   return; 
}


static void
printClasses(classDescription *cListHead) 
{
 	classDescription *c;
    
    printf("Printing Classes:\n");

	c = cListHead;
    while (c) {
     	printClass(c);
		c = c->next;
	}

}


static void
getClassDetails(jvmtiEnv * jvmti_env, JNIEnv * jni_env, classDescription *c)
{ 
	int i;
	jvmtiError rc ;
	jint  field_count_ptr;
	jfieldID * fields_ptr;
	classField *f;       
	char *generic_ptr;

	rc = (*jvmti_env)->GetClassSignature(jvmti_env, c->klass, &c->name, &c->genericName);
	RC;

	rc = (*jvmti_env)->GetClassFields(jvmti_env, c->klass, &field_count_ptr, &fields_ptr);
	RC;

	c->fieldCount = field_count_ptr;

	c->fields = NULL;

	for (i = 0; i < field_count_ptr; i++) {
     	if (c->fields) {
         	f->next = calloc(1, sizeof(classField));
			f = f->next;
		} else {
         	f = calloc(1, sizeof(classField));
			c->fields = f;
		}
        f->id = fields_ptr[i];

		rc = (*jvmti_env)->GetFieldName(jvmti_env, c->klass, f->id, &f->name, &f->sig, &generic_ptr);
        RC;
 
		rc = (*jvmti_env)->GetFieldModifiers(jvmti_env, c->klass, f->id, &f->modifiers);
        RC;
                                                                           
	}
}


static void
getTaggedObjects(jvmtiEnv * jvmti_env, JNIEnv * jni_env)
{
	jvmtiError rc ;
	jint tag_count = tagCount;
    jlong tags[1024*5];
	int i;
	jint count_ptr = 0;      
	jobject * object_result_ptr;
	jlong * tag_result_ptr;
	classDescription *c, *cListHead = NULL;


	printf("Getting tagged Objects\n");

	for (i = 0; i < tagCount; i++) {
     	tags[i] = (jlong) (tagPrefix + i);
	}

	rc = (*jvmti_env)->GetObjectsWithTags(jvmti_env,
										  tag_count,
										  tags,
										  &count_ptr,
										  &object_result_ptr,
										  &tag_result_ptr);

    for (i = 0; i < count_ptr; i++) {
        c = calloc(1, sizeof(classDescription));
		c->tag = tag_result_ptr[i];
		c->object = object_result_ptr[i];
		
		c->klass = (*jni_env)->GetObjectClass(jni_env, c->object);
		
		getClassDetails(jvmti_env, jni_env, c);
		if (cListHead == NULL) {
         	cListHead = c;
		} else {
         	c->next = cListHead;
			cListHead = c;
		}
	}

	
    printClasses(c);

	return;
}



static void
printObject(object * o)
{
 	printf("[%llx] [%s]\n", o->tag, o->class->name);
}

static void
resolveObject(object * o)
{
	jvmtiError rc ;
    jlong tags[1];
	jint count_ptr = 0;      
	jobject * object_result_ptr;
	jlong * tag_result_ptr;
	JVMTI_ACCESS_FROM_AGENT(env);
	JNIEnv * jni_env = env->jni_env;
    
	o->class = calloc(1, sizeof(struct classDescription));

	tags[0] = o->tag;

	rc = (*jvmti_env)->GetObjectsWithTags(jvmti_env,
										  1,
										  tags,
										  &count_ptr,
										  &object_result_ptr,
										  &tag_result_ptr);
    if (rc != JVMTI_ERROR_NONE) {
		error(env, rc, "GetObjectsWithTags");
		abort();
	}

    o->object = object_result_ptr[0];

 	o->class->klass = (*jni_env)->GetObjectClass(jni_env, o->object);
    if (o->class->klass == NULL) {
		printf("GetObjectClass failed\n");
     	abort();
	}
		

	rc = (*jvmti_env)->GetClassSignature(jvmti_env, o->class->klass, &o->class->name, &o->class->genericName);
	if (rc != JVMTI_ERROR_NONE) {
		error(env, rc, "GetClassSignature");
		abort();
	}



}

static void
printWalkedObjects(void)
{
    object *o;
    int i;


	for (i = 0; i < OBJECT_LIST_MAX; i++) {
		o = objectList[i];
     	if (o == NULL) 
			continue;
		resolveObject(o);
		printObject(o);
	}

}


jboolean JNICALL
Java_com_ibm_jvmti_tests_followReferences_fr002_followRefs(JNIEnv * jni_env, jclass boo, jclass clazz, 
														   jobject object, jobject tail)
{
	jvmtiError rc;
	jvmtiEnv * jvmti_env = env->jvmtiEnv;
	fr002_data data;
	jvmtiHeapCallbacks callbacks;
/*
 	jfieldID   *idlist;
	jint        n_fields; 
*/
	
	env->jni_env = jni_env;
	data.foundJavaLangClass = 0;
	data.foundFR002Class = 0;


    objectList = calloc(OBJECT_LIST_MAX, sizeof(struct object *));



	/*
 	rc = (*jvmti_env)->SetTag(jvmti_env, clazz, JVMTI_TEST_FR002_CLASS_TAG);
	if (rc != JVMTI_ERROR_NONE) {
		error(env, rc, "Failed to SetTag [JVMTI_TEST_FR002_JAVALANGCLASS_TAG]");
		return JNI_FALSE;
	}
 
	rc = (*jvmti_env)->SetTag(jvmti_env, object, JVMTI_TEST_FR002_OBJECT_TAG);
	if (rc != JVMTI_ERROR_NONE) {
		error(env, rc, "Failed to SetTag [JVMTI_TEST_FR002_JAVALANGCLASS_TAG]");
		return JNI_FALSE;
	}
 

	rc = (*jvmti_env)->GetClassFields(jvmti_env, clazz, &n_fields, &idlist);
	if (rc != JVMTI_ERROR_NONE) {
		error(env, rc, "Failed to GetClassFields");
		return JNI_FALSE;
	}
      */


	/* Test Follow References Callback */
	memset(&callbacks, 0x00, sizeof(jvmtiHeapCallbacks));
	callbacks.heap_reference_callback = fr002HeapReferenceCallback;
	callbacks.primitive_field_callback = fr002PrimitiveFieldCallback;
	rc = (*jvmti_env)->FollowReferences(jvmti_env, 0, NULL, NULL, &callbacks, &data); 
	if (rc != JVMTI_ERROR_NONE) {
		error(env, rc, "Failed to FollowReferences");
		return JNI_FALSE;
	}
 
 
#if 1
    getTaggedObjects(jvmti_env, jni_env);
#endif

    printWalkedObjects();



 	/* Did we find fr002 Class ? */
	if (data.foundFR002Class == 0) {
		error(env, JVMTI_ERROR_NONE, "klass tagged with JVMTI_TEST_FR002_CLASS_TAG not found");
		return JNI_FALSE;
	}
 
	return JNI_TRUE;
}


