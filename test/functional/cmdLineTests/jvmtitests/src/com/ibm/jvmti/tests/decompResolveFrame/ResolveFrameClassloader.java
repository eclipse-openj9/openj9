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
package com.ibm.jvmti.tests.decompResolveFrame;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;


public class ResolveFrameClassloader extends ClassLoader 
{
    private static final int BUFFER_SIZE = 8192;
    private String classFilter = null;
    
    
    static public native boolean checkFrame();
    
    public ResolveFrameClassloader(String classFilter)
    {
    	this.classFilter = classFilter;
    }
    
    protected synchronized Class loadClass(String className, boolean resolve) throws ClassNotFoundException 
    {
        log("Loading class: " + className + ", resolve: " + resolve);

        // 1. is this class already loaded?
        Class cls = findLoadedClass(className);
        if (cls != null) {
        	log("\tAlready loaded");
            return cls;
        }
          
        if (className.equals(classFilter)) {       
        	checkFrame();
        } 
        
        String clsFile = className.replace('.', '/') + ".class";

        
        byte[] classBytes = null;
        try {
            InputStream in = getResourceAsStream(clsFile);
            byte[] buffer = new byte[BUFFER_SIZE];
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            int n = -1;
            while ((n = in.read(buffer, 0, BUFFER_SIZE)) != -1) {
                out.write(buffer, 0, n);
            }
            classBytes = out.toByteArray();
        }
        catch (IOException e) {
            log("ERROR loading class file: " + e);
        }

                
        if (classBytes == null) {
            throw new ClassNotFoundException("Cannot load class: " + className);
        }

        try {
            cls = defineClass(className, classBytes, 0, classBytes.length);
            if (resolve) {
                resolveClass(cls);
            }
        }
        catch (SecurityException e) { 
            cls = super.loadClass(className, resolve);
        }
 
        return cls;
    }

    private static void log(String s) {
        System.out.println(s);
    }
}


