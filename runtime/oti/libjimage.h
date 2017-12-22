/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#ifndef libjimage_h
#define libjimage_h

#include "jniport.h"

#define JIMAGE_LIBRARY_NAME "jimage"

typedef void JImageFile;
typedef jlong JImageLocationRef;

typedef JImageFile * (*libJImageOpenType)(const char *name, jint *error);
typedef void (*libJImageCloseType)(JImageFile* jimage);
typedef JImageLocationRef(*libJImageFindResourceType)(JImageFile* jimage,
        const char* module_name, const char* version, const char* name, jlong* size);
typedef jlong(*libJImageGetResourceType)(JImageFile* jimage, JImageLocationRef location,
        char* buffer, jlong size);
typedef const char* (*libJImagePackageToModuleType)(JImageFile* jimage, const char* package_name);

#define JIMAGE_VERSION_NUMBER "9.0"


#define JIMAGE_MAX_PATH 4096

/* Error codes */
#define JIMAGE_NOT_FOUND 0
#define JIMAGE_BAD_MAGIC -1
#define JIMAGE_BAD_VERSION -2
#define JIMAGE_CORRUPTED -3

#endif /* libjimage_h */
