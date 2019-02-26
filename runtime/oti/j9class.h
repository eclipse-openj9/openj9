/*******************************************************************************
 * Copyright (c) 2011, 2019 IBM Corp. and others
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
#ifndef j9class_h
#define j9class_h
typedef struct J9ClassLoadingConstraint {
    struct J9ClassLoader* classLoader;
    U_8* name;
    UDATA nameLength;
    struct J9Class* clazz;
    struct J9ClassLoadingConstraint *linkNext;
    struct J9ClassLoadingConstraint *linkPrevious;
    UDATA freeName;
} J9ClassLoadingConstraint;

#define J9CLASS_FLAGS(clazz) ((UDATA)(clazz)->classDepthAndFlags)
#define J9CLASS_DEPTH(clazz) (((UDATA)(clazz)->classDepthAndFlags) & J9AccClassDepthMask)
#define J9CLASS_EXTENDED_FLAGS(clazz) ((clazz)->classFlags)
#define J9CLASS_EXTENDED_FLAGS_SET(clazz, bits) ((clazz)->classFlags |= (bits))
#define J9CLASS_EXTENDED_FLAGS_CLEAR(clazz, bits) ((clazz)->classFlags &= ~(bits))

#endif
