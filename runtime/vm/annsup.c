/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"

#include "annsup.h"
#include "vm_internal.h"

/* this file is being left in existence to provide a dummy implementation of these functions to allow to continued existence of the JIT code that uses these functions
 * these functions however are no longer supported
 */

UDATA
getAnnotationsFromAnnotationInfo(J9AnnotationInfo *annInfo, UDATA annotationType, char *memberName, U_32 memberNameLength, char *memberSignature, U_32 memberSignatureLength, J9AnnotationInfoEntry **annotations)
{
	return 0;
}


J9AnnotationInfo *
getAnnotationInfoFromClass(J9JavaVM *vm, J9Class *clazz)
{
	return NULL;
}



J9UTF8 *
annotationElementIteratorNext(J9AnnotationState *state, void **data) 
{
	return NULL;
}



J9UTF8 *
annotationElementIteratorStart(J9AnnotationState *state, J9AnnotationInfoEntry *annotation, void **data) 
{
	return NULL;
}

UDATA
getAllAnnotationsFromAnnotationInfo(J9AnnotationInfo *annInfo, J9AnnotationInfoEntry **annotations)
{
	return 0;
}


void *
elementArrayIteratorNext(J9AnnotationState *state) 
{
	return NULL;
}



void *
elementArrayIteratorStart(J9AnnotationState *state, UDATA start, U_32 count) 
{
	return NULL;
}

J9AnnotationInfoEntry *
getAnnotationDefaultsForAnnotation(J9VMThread *currentThread, J9Class *containingClass, J9AnnotationInfoEntry *annotation, UDATA flags)
{
	return NULL;
}


void *
getNamedElementFromAnnotation(J9AnnotationInfoEntry *annotation, char *name, U_32 nameLength)
{
	return NULL;
}


J9AnnotationInfoEntry *
getAnnotationFromAnnotationInfo(J9AnnotationInfo *annInfo, UDATA annotationType, char *memberName, U_32 memberNameLength, char *memberSignature, U_32 memberSignatureLength, char *annotationName, U_32 annotationNameLength)
{
	return NULL;
}


J9AnnotationInfoEntry *
getAnnotationDefaultsForNamedAnnotation(J9VMThread *currentThread, J9Class *containingClass, char *className, U_32 classNameLength, UDATA flags)
{
	return NULL;
}

