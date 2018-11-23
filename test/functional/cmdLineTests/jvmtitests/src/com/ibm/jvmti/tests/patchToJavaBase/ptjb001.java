/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
 package com.ibm.jvmti.tests.patchToJavaBase;

 public class ptjb001 {
     /*
      * Test that classes have been assigned to the proper modules by the jvmti callback method.
      */
     public boolean testPatchToJavaBase() {
        boolean result = true;

        /* check that BootstrapClassJavaBase is in the java.base module */
        Module java_base = ModuleLayer.boot().findModule("java.base").get();
        Class baseClazz = Class.forName(java_base, "com.ibm.jvmti.tests.patchToJavaBase.bootstrapClass.BootstrapClassJavaBase");
        if (null == baseClazz) {
            System.out.println("Error: com.ibm.jvmti.tests.patchToJavaBase.bootstrapClass.BootstrapClassJavaBase was not found in java.base");
            result = false;
        } else {
            System.out.println("Success: com.ibm.jvmti.tests.patchToJavaBase.bootstrapClass.BootstrapClassJavaBase was found in java.base");
        }

        /* check that BootstrapClassUnnamed is in the unnamed module (accessable without export) */
        try {
            String unnamedClassModule =  com.ibm.jvmti.tests.patchToJavaBase.bootstrapClass.BootstrapClassUnnamed.class.getModule().getName();
            if (null != unnamedClassModule) {
                result = false;
                System.out.println("Error: com.ibm.jvmti.tests.patchToJavaBase.bootstrapClass.BootstrapClassUnnamed was found " + unnamedClassModule + " instead of in unnamed module");
            } else {
                System.out.println("Success: com.ibm.jvmti.tests.patchToJavaBase.bootstrapClass.BootstrapClassUnnamed is in the unnamed module.");
            }
        } catch(Throwable e) {
            result = false;
            System.out.println("Error: com.ibm.jvmti.tests.patchToJavaBase.bootstrapClass.BootstrapClassUnnamed was not found in unnamed module");
            e.printStackTrace();
        }

        return result;
     }

     public String helpPatchToJavaBase() {
         return "Test loading custom class into java.base or unnamed module";
     }
 }