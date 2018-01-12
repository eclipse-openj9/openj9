/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
/* Prototypes and types required by the JCL module */

#ifndef JCL_INTERNAL_H
#define JCL_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- getstacktrace.c ---------------- */
j9object_t
createStackTraceThrowable(J9VMThread *currentThread,  const UDATA *frames, UDATA maxFrames);



/* ---------------- mgmtthread.c ---------------- */

U_64
checkedTimeInterval(U_64 endNS, U_64 startNS);


/* ---------------- reflecthelp.c ---------------- */
void
initializeReflection(J9JavaVM *javaVM);

void
preloadReflectWrapperClasses(J9JavaVM *javaVM);

j9object_t getClassAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass);

jbyteArray getClassTypeAnnotationsAsByteArray(JNIEnv *env, jclass jlClass);

j9object_t getFieldAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9JNIFieldID *j9FieldID);

jbyteArray getFieldTypeAnnotationsAsByteArray(JNIEnv *env, jobject jlrField);

j9object_t getMethodAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9Method *ramMethod);

j9object_t getMethodParametersAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9Method *ramMethod);

j9object_t getMethodTypeAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9Method *ramMethod);

jbyteArray getMethodTypeAnnotationsAsByteArray(JNIEnv *env, jobject jlrMethod);

j9object_t getMethodDefaultAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9Method *ramMethod);

jobjectArray getMethodParametersAsArray(JNIEnv *env, jobject jlrMethod);

/**
 * The caller must have VM access. 
 * Build a java.lang.reflect.Field object for a field specified with a name and a declaring class.
 * @param[in] vmThread The current vmThread.
 * @param[in] declaringClass The declaring class. Must be non-null.
 * @param[in] fieldName The name of the field.
 * @return j9object_t a java.lang.reflect.Field
 */
j9object_t
getFieldObjHelper(J9VMThread *vmThread, jclass declaringClass, jstring fieldName);

/**
 * Build a java.lang.reflect.Field for a specified declared field.
 * @param[in] env The JNI context.
 * @param[in] declaringClass The declaring class. Must be non-null.
 * @param[in] fieldName The name of the field.
 * @return jobject a java.lang.reflect.Field
 */
jobject
getDeclaredFieldHelper(JNIEnv *env, jobject declaringClass, jstring fieldName);

/**
 * Build an array of java.lang.reflect.Field for the fields declared by a class.
 * @param[in] env The JNI context.
 * @param[in] declaringClass The declaring class.  Must be non-null.
 * @return jarray an array of java.lang.reflect.Field
 */
jarray
getDeclaredFieldsHelper(JNIEnv *env, jobject declaringClass);

/**
 * Build a java.lang.reflect.Field for a specified field.
 * @param[in] env The JNI context.
 * @param[in] cls A class. Must be non-null.
 * @param[in] fieldName The name of the field.
 * @return jobject a java.lang.reflect.Field
 */
jobject
getFieldHelper(JNIEnv *env, jobject cls, jstring fieldName);

/**
 * Build an array of java.lang.reflect.Field for the public fields of a class.
 * @param[in] env The JNI context.
 * @param[in] cls A class.  Must be non-null.
 * @return jarray an array of java.lang.reflect.Field
 */
jarray
getFieldsHelper(JNIEnv *env, jobject cls);

/**
 * Get the array class for an element class.
 * Creates the class if it isn't already created.
 *
 * @param[in] vmThread The current vmThread.
 * @param[in] elementTypeClass The element class.
 * @return the array class
 *
 * @pre has VM access
 * @todo There is a duplicate copy of this function in j9vm/j7vmi.c
 */
J9Class *
fetchArrayClass(struct J9VMThread *vmThread, J9Class *elementTypeClass);


/* ---------------- sigquit.c ---------------- */
void
J9SigQuitShutdown(J9JavaVM * vm);


/* ---------------- unsafe_mem.c ---------------- */
UDATA
initializeUnsafeMemoryTracking(J9JavaVM* vm);

void
freeUnsafeMemory(J9JavaVM* vm);


#ifdef __cplusplus
}
#endif

#endif /* JCL_INTERNAL_H */
