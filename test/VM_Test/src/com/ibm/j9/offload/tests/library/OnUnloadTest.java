package com.ibm.j9.offload.tests.library;
/*******************************************************************************
 * Copyright (c) 2008, 2012 IBM Corp. and others
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
import java.net.URLClassLoader;
import java.net.URL;

/**
 * This test class simply calls a native that uses j9tty_printf to output
 * 
 * 			**** OUTPUT FROM NATIVE ****
 * 
 * so that we can validate that output is displayed when quarantines are run
 * out of process
 */
public class OnUnloadTest {

	
	public static final String NATIVE_LIBRARY_NAME = "j9offjnitest26";
	public static native int setPrintfOnUnload();
	public static native int getOnLoadCalled();
	
	public static void main(String[] args){
		try {
			URL[] urls = new URL[1];
			String extraSlash = "";
			if (args[0].charAt(0) != '/'){
				extraSlash = "/";
			}
			String urlString = "file://" + extraSlash + args[0].replace('\\','/');
			urls[0] = new URL(urlString);
			ClassLoader loader = URLClassLoader.newInstance(urls,null);
			Class theClass = loader.loadClass("com.ibm.j9.offload.tests.library.LibraryNatives");
			theClass.newInstance();
			theClass = null;
			loader = null;
			
			loader = URLClassLoader.newInstance(urls,null);
			theClass = loader.loadClass("com.ibm.j9.offload.tests.library.LibraryNatives");
			theClass.newInstance();
			theClass = null;
			loader = null;

			/* provide enough time to ensure output comes out */
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
			}
		} catch (Exception e){
			System.out.println("Unexpected exception:" + e);
		}
	}
}


