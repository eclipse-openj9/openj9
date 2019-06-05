/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(OBJECTDESCRIPTION_H_)
#define OBJECTDESCRIPTION_H_

#include "j9.h"

/**
 * Object token definitions to be used by OMR components (and eventually subsume j9object_t and related).
 */
typedef J9Object* languageobjectptr_t;
typedef J9Object* omrobjectptr_t;
typedef J9IndexableObject* omrarrayptr_t;

#if defined (OMR_GC_COMPRESSED_POINTERS)
typedef U_32 fomrobject_t;
typedef U_32 fomrarray_t;
#else
typedef UDATA fomrobject_t;
typedef UDATA fomrarray_t;
#endif

#endif /* OBJECTDESCRIPTION_H_ */
