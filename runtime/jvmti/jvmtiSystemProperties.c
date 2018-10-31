/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "vmi.h"
#include "jvmtiHelpers.h"
#include "jvmti_internal.h"


jvmtiError JNICALL
jvmtiGetSystemProperties(jvmtiEnv* env,
	jint* count_ptr,
	char*** property_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	char ** propertyList;
	UDATA propertyCount;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_count = 0;
	char **rv_property = NULL;

	Trc_JVMTI_jvmtiGetSystemProperties_Entry(env);

	ENSURE_PHASE_ONLOAD_OR_LIVE(env);

	ENSURE_NON_NULL(count_ptr);
	ENSURE_NON_NULL(property_ptr);

	propertyCount = pool_numElements(vm->systemProperties);
	propertyList = j9mem_allocate_memory(propertyCount * sizeof(char **), J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (propertyList == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		char ** currentPropName = propertyList;
		pool_state walkState;

		J9VMSystemProperty* property = pool_startDo(vm->systemProperties, &walkState);
		while (property != NULL) {
			char * propName;

			propName = j9mem_allocate_memory(strlen(property->name) + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (propName == NULL) {
				while (currentPropName != propertyList) {
					--currentPropName;
					j9mem_free_memory(*currentPropName);
				}
				j9mem_free_memory(propertyList);
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				goto done;
			}
			strcpy(propName, property->name);
			*currentPropName++ = propName;
			property = pool_nextDo(&walkState);
		}

		rv_count = (jint) propertyCount;
		rv_property = propertyList;
	}

done:
	if (NULL != count_ptr) {
		*count_ptr = rv_count;
	}
	if (NULL != property_ptr) {
		*property_ptr = rv_property;
	}
	TRACE_JVMTI_RETURN(jvmtiGetSystemProperties);
}


jvmtiError JNICALL
jvmtiGetSystemProperty(jvmtiEnv* env,
	const char* property,
	char** value_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMSystemProperty * systemProperty;
	jvmtiError rc = JVMTI_ERROR_NONE;
	char * value;
	PORT_ACCESS_FROM_JAVAVM(vm);
	char *rv_value = NULL;

	Trc_JVMTI_jvmtiGetSystemProperty_Entry(env);

	ENSURE_PHASE_ONLOAD_OR_LIVE(env);

	ENSURE_NON_NULL(property);
	ENSURE_NON_NULL(value_ptr);

	if (vm->internalVMFunctions->getSystemProperty(vm, property, &systemProperty) != J9SYSPROP_ERROR_NONE) {
		JVMTI_ERROR(JVMTI_ERROR_NOT_AVAILABLE);
	}

	value = j9mem_allocate_memory(strlen(systemProperty->value) + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (value == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		strcpy(value, systemProperty->value);
		rv_value = value;
	}

done:
	if (NULL != value_ptr) {
		*value_ptr = rv_value;
	}
	TRACE_JVMTI_RETURN(jvmtiGetSystemProperty);
}


jvmtiError JNICALL
jvmtiSetSystemProperty(jvmtiEnv* env,
	const char* property,
	const char* value)
{
	jvmtiError rc;
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMSystemProperty * systemProperty;

	Trc_JVMTI_jvmtiSetSystemProperty_Entry(env);

	ENSURE_PHASE_ONLOAD(env);

	ENSURE_NON_NULL(property);

	if (vm->internalVMFunctions->getSystemProperty(vm, property, &systemProperty) != J9SYSPROP_ERROR_NONE) {
		JVMTI_ERROR(JVMTI_ERROR_NOT_AVAILABLE);
	}

	/* Note: the value == NULL case (read-only check) is handled by setSystemProperty */

	switch (vm->internalVMFunctions->setSystemProperty(vm, systemProperty, value)) {
		case J9SYSPROP_ERROR_NONE:
			rc = JVMTI_ERROR_NONE;
			break;
		case J9SYSPROP_ERROR_READ_ONLY:
			rc = JVMTI_ERROR_NOT_AVAILABLE;
			break;
		case J9SYSPROP_ERROR_OUT_OF_MEMORY:
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
			break;
		default:
			rc = JVMTI_ERROR_INTERNAL;
			break;
	}

done:
	TRACE_JVMTI_RETURN(jvmtiSetSystemProperty);
}


