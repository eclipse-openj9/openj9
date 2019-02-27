package org.openj9.test.jdk.internal;

/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

import jdk.internal.misc.SharedSecrets;
import jdk.internal.misc.JavaLangAccess;

import java.lang.reflect.Executable;

/* some jdk.internal.misc classes have been moved in Java 12 to a new package jdk.internal.access. The purpose of this 
 * class is to provide access to those packages to prevent duplicating code */

public class InternalAccessor {

    private JavaLangAccess access;

    public InternalAccessor() {
        this.access = SharedSecrets.getJavaLangAccess();
    }

    /* wrapper method for IbmTypeAnnotationsApiTest */
    public byte[] getRawClassTypeAnnotations(Class<?> clazz) {
        return this.access.getRawClassTypeAnnotations(clazz);
    }

    /* wrapper method for IbmTypeAnnotationsApiTest */
    public byte[] getRawExecutableTypeAnnotations(Executable m) {
        return this.access.getRawExecutableTypeAnnotations(m);
    }

    /* wrapper method for IbmTypeAnnotationsApiTest */
    public byte[] getRawClassAnnotations(Class<?> clazz) {
        return this.access.getRawClassAnnotations(clazz);
    }

    /* wrapper method for Test_Class */
    public <E extends Enum<E>> E[] getEnumConstantsShared(Class<E> clazz) {
        return this.access.getEnumConstantsShared(clazz);
    }
}