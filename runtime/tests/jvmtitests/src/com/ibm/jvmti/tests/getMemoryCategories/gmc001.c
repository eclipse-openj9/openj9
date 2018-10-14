/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
#include <stdlib.h>
#include <string.h>

#include "ibmjvmti.h"
#include "jvmti_test.h"

static agentEnv * env;
static jvmtiExtensionFunction getMemoryCategories = NULL;

jint JNICALL
gmc001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jint extensionCount;
	jvmtiExtensionFunctionInfo* extensions;
	jvmtiError err;
	int i;
	jint rc = JNI_OK;

	env = agent_env;

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensions);
	if (JVMTI_ERROR_NONE != err) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:JVMTI extension functions could not be loaded");
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
		jint j;
		if (0 == strcmp(extensions[i].id, COM_IBM_GET_MEMORY_CATEGORIES)) {
			getMemoryCategories = extensions[i].func;
			fprintf(stderr,"gmc001: setting getMemoryCategories extension to %p\n", getMemoryCategories);
		}

		err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensions[i].id);

		if (err != JVMTI_ERROR_NONE) {
			error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Couldn't Deallocate extensions[%d].id\n", i);
			rc = JNI_ERR;
		}

		err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensions[i].short_description);

		if (err != JVMTI_ERROR_NONE) {
			error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Couldn't Deallocate extensions[%d].short_description\n", i);
			rc = JNI_ERR;
		}

		for (j = 0; j < extensions[i].param_count; j++) {
			err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensions[i].params[j].name);

			if (err != JVMTI_ERROR_NONE) {
				error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Couldn't Deallocate extensions[%d].params[%d].name\n", i, j);
				rc = JNI_ERR;
			}
		}

		err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensions[i].params);
		if (err != JVMTI_ERROR_NONE) {
			error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Couldn't Deallocate extensions[%d].param\n", i);
			rc = JNI_ERR;
		}

		err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensions[i].errors);
		if (err != JVMTI_ERROR_NONE) {
			error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Couldn't Deallocate extensions[%d].errors\n", i);
			rc = JNI_ERR;
		}
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)extensions);

	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "gmc001:Failed to Deallocate extension functions");
		return FALSE;
	}

	if (NULL == getMemoryCategories) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:getMemoryCategories extension was not found");
		return FALSE;
	}

	fprintf(stderr,"gmc001: finished initialization\n");

	return rc;
}

/**
 * Validates that a category pointer (parent, firstChild, nextSibling) is valid
 */
static jboolean
validateCategoryPointer(jvmtiMemoryCategory * value, jvmtiMemoryCategory * categories_buffer, jint written_count)
{
	if (value < categories_buffer || value > (categories_buffer + written_count)) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Category pointer out of range. Value = %p, categories_buffer = %p, top of buffer = %p", value, categories_buffer, categories_buffer + written_count);
		return JNI_FALSE;
	}

	if (((char*)value - (char*)categories_buffer) % sizeof(jvmtiMemoryCategory)) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Misaligned category pointer = %p. Categories_buffer=%p, sizeof(jvmtiMemoryCategory)=%u, remainder=%d", value, categories_buffer, (unsigned int)sizeof(jvmtiMemoryCategory), (int)((value - categories_buffer) % sizeof(jvmtiMemoryCategory)));
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

/* Checks the data returned from TI extension.
 *
 * Looks for loops in the linked lists, checks for valid values etc.
 */
static jboolean
validateCategories(jvmtiMemoryCategory * categories_buffer, jint written_count)
{
	jint i;

	for (i = 0; i < written_count; i++) {
		jvmtiMemoryCategory * thisCategory = categories_buffer + i;
		size_t nameLength;

		/* Name should have more than 0 characters (will also detect bad pointers ) */
		nameLength = strlen(thisCategory->name);

		if (0 == nameLength) {
			error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:zero-length name in returned array. Index = %d", i);
			return JNI_FALSE;
		}

		/* Shallow counters should be >= 0 */
		if (thisCategory->liveBytesShallow < 0) {
			error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:liveBytesShallow negative for category %s. Index = %d", thisCategory->name, i);
			return JNI_FALSE;
		}

		if (thisCategory->liveAllocationsShallow < 0) {
			error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:liveAllocationsShallow negative for category %s. Index = %d", thisCategory->name, i);
			return JNI_FALSE;
		}

		/* Deep counters should be >= shallow counters */
		if (thisCategory->liveBytesShallow > thisCategory->liveBytesDeep) {
			error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:liveBytesShallow is larger than liveBytesDeep for category %s. Index = %d", thisCategory->name, i);
			return JNI_FALSE;
		}

		if (thisCategory->liveAllocationsShallow > thisCategory->liveAllocationsDeep) {
			error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:liveAllocationsShallow is larger than liveAllocationsDeep for category %s. Index = %d", thisCategory->name, i);
			return JNI_FALSE;
		}

		/* Check parent link */
		if (thisCategory->parent != NULL) {

			if (! validateCategoryPointer(thisCategory->parent, categories_buffer, written_count)) {
				error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Parent link for category %s  failed validation. Index = %d", thisCategory->name, i);
				return JNI_FALSE;
			}

			/* Parent cannot be itself */
			if (thisCategory->parent == thisCategory) {
				error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Category %s is its own parent. Index = %d", thisCategory->name, i);
				return JNI_FALSE;
			}
		}

		/* Validate child link */
		if (thisCategory->firstChild != NULL) {
			if (! validateCategoryPointer(thisCategory->firstChild, categories_buffer, written_count)) {
				error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:firstChild link for category %s  failed validation. Index = %d", thisCategory->name, i);
				return JNI_FALSE;
			}

			/* firstChild cannot be itself */
			if (thisCategory->firstChild == thisCategory) {
				error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Category %s is its own firstChild. Index = %d", thisCategory->name, i);
				return JNI_FALSE;
			}
		}

		/* Validate sibling list. Each node must validate and we mustn't have loops */
		if (thisCategory->nextSibling != NULL) {
			jvmtiMemoryCategory *ptr1, *ptr2;

			if (! validateCategoryPointer(thisCategory->nextSibling, categories_buffer, written_count)) {
				error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:nextSibling link for category %s  failed validation. Index = %d", thisCategory->name, i);
				return JNI_FALSE;
			}

			if (thisCategory->nextSibling == thisCategory) {
				error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Category %s is its own nextSibling. Index = %d", thisCategory->name, i);
				return JNI_FALSE;
			}

			/* Loop detection. */
			ptr1 = thisCategory;
			ptr2 = thisCategory->nextSibling;

			if (ptr2 && ptr2->nextSibling == ptr1) {
				error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:1-2-1 loop found in nextSibling chain for category %s. Index = %d", thisCategory->name, i);
				return JNI_FALSE;
			} else {
				while (ptr1 && ptr2) {
					/* ptr1 increments by 1 step, ptr2 increments by 2. If they are ever equal, we have a loop */
					ptr1 = ptr1->nextSibling;
					ptr2 = ptr2->nextSibling != NULL ? ptr2->nextSibling->nextSibling : NULL;

					if (ptr1 == ptr2) {
						error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:loop found in nextSibling chain for category %s. Index = %d", thisCategory->name, i);
						return JNI_FALSE;
					}
				}
			}
		}


	}

	return JNI_TRUE;
}

/*
 * Class:     com_ibm_jvmti_tests_getMemoryCategories_gmc001
 * Method:    check
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_ibm_jvmti_tests_getMemoryCategories_gmc001_check (JNIEnv * jni_env, jobject obj)
{
	jint total_categories;
	jint written_count;
	jvmtiMemoryCategory * categories_buffer = NULL;
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;
	jboolean rc = JNI_TRUE;

	if (NULL == getMemoryCategories) {
		error(env, err, "gmc001:getMemoryCategories NULL function pointer (should have been set in initializer.");
		return JNI_FALSE;
	}

	/* Negative path: try wrong version */
	err = getMemoryCategories(jvmti_env, COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1 + 1, 0, NULL, NULL, &total_categories);

	if (JVMTI_ERROR_UNSUPPORTED_VERSION != err) {
		error(env, err, "gmc001:getMemoryCategories failed wrong version test. Expected JVMTI_ERROR_UNSUPPORTED_VERSION.");
		return JNI_FALSE;
	}

	/* Negative path: try non-zero max_categories with NULL categories_buffer */
	err = getMemoryCategories(jvmti_env, COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1, 1, NULL, NULL, &total_categories);

	if (JVMTI_ERROR_ILLEGAL_ARGUMENT != err) {
		error(env, err, "gmc001:getMemoryCategories failed max_categories > 0, categories_buffer = NULL test. Expected JVMTI_ERROR_ILLEGAL_ARGUMENT.");
		return JNI_FALSE;
	}

	/* Negative path: try all output pointers NULL */
	err = getMemoryCategories(jvmti_env, COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1, 0, NULL, NULL, NULL);

	if (JVMTI_ERROR_ILLEGAL_ARGUMENT != err) {
		error(env, err, "gmc001:getMemoryCategories failed all output pointers NULL test. Expected JVMTI_ERROR_ILLEGAL_ARGUMENT.");
		return JNI_FALSE;
	}

	/* Positive path: query total number of categories */

	total_categories = -1;
	err = getMemoryCategories(jvmti_env, COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1, 0, NULL, NULL, & total_categories);

	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "gmc001:getMemoryCategories failed poll total number of categories test. Expected JVMTI_ERROR_NONE");
		return JNI_FALSE;
	}

	/* We need > 1 category for the undersized buffer test to work below */
	if (total_categories <= 1) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Expected more categories. Got %d", total_categories);
		return JNI_FALSE;
	}

	fprintf(stderr, "Total categories = %d\n", total_categories);

	/* Allocate our buffer */
	err = (*jvmti_env)->Allocate(jvmti_env, sizeof(jvmtiMemoryCategory) * total_categories, (unsigned char**)&categories_buffer);

	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "gmc001:getMemoryCategories Allocate failed (native OOM?). Expected JVMTI_ERROR_NONE");
		rc = JNI_FALSE;
		goto end;
	}

	fprintf(stderr, "Allocated categories_buffer=%p\n", categories_buffer);

	/* Negative path: get categories with an undersized buffer */
	written_count = -1;
	err = getMemoryCategories(jvmti_env, COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1, total_categories - 1, categories_buffer, &written_count, NULL);

	if (JVMTI_ERROR_OUT_OF_MEMORY != err) {
		error(env, err, "gmc001:getMemoryCategories buffer undersized test failed. Expected JVMTI_ERROR_OUT_OF_MEMORY");
		rc = JNI_FALSE;
		goto end;
	}

	if (-1 == written_count) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Expected written_count to be assigned, even on negative path.");
		rc = JNI_FALSE;
		goto end;
	}

	rc = validateCategories(categories_buffer, written_count);

	if (! rc) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Memory category validate failed after overflowed buffer");
		rc = JNI_FALSE;
		goto end;
	}

	/* Negative path: get categories with a NULL written_count_ptr */
	err = getMemoryCategories(jvmti_env, COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1, total_categories, categories_buffer, NULL, NULL);

	if (JVMTI_ERROR_ILLEGAL_ARGUMENT != err) {
		error(env, err, "gmc001:getMemoryCategories failed NULL written_count_ptr test. Expected JVMTI_ERROR_ILLEGAL_ARGUMENT.");
		return JNI_FALSE;
	}

	/* Positive path: get and validate categories */
	err = getMemoryCategories(jvmti_env, COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1, total_categories, categories_buffer, &written_count, NULL);

	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "gmc001:getMemoryCategories positive path failed");
		rc = JNI_FALSE;
		goto end;
	}

	rc = validateCategories(categories_buffer, written_count);

	if (! rc) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Memory category validate failed in positive path");
		rc = JNI_FALSE;
		goto end;
	}

	/* Positive path: get categories with an oversized buffer */
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)categories_buffer);
	err = (*jvmti_env)->Allocate(jvmti_env, sizeof(jvmtiMemoryCategory) * (total_categories + 1), (unsigned char**)&categories_buffer);

	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "gmc001:getMemoryCategories Allocate failed (native OOM?). Expected JVMTI_ERROR_NONE");
		rc = JNI_FALSE;
		goto end;
	}

	fprintf(stderr, "Allocated categories_buffer=%p\n", categories_buffer);

	err = getMemoryCategories(jvmti_env, COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1, total_categories, categories_buffer, &written_count, NULL);

	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "gmc001:getMemoryCategories positive path failed");
		rc = JNI_FALSE;
		goto end;
	}

	if (written_count > total_categories) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc100: written_count was higher that total categories with oversized buffer. Should be impossible. written_count=%d, total_categories=%d\n", written_count, total_categories);
		rc = FALSE;
		goto end;
	}

	rc = validateCategories(categories_buffer, written_count);

	if (! rc) {
		error(env, JVMTI_ERROR_NOT_FOUND, "gmc001:Memory category validate failed in positive path");
		rc = JNI_FALSE;
		goto end;
	}


end:
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)categories_buffer);
	return rc;
}
