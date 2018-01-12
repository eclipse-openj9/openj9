/*[INCLUDE-IF Sidecar16]*/

package com.ibm.oti.vm;

import java.util.jar.*;
import java.io.IOException;
import java.lang.reflect.*;

/*******************************************************************************
 * Copyright (c) 2002, 2014 IBM Corp. and others
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

public class JarRunner {

	public static void main(String args[]) throws Exception {
		
		//Manifest from the jarfile
		Manifest manifest = getManifest(args[0]);
		if (null==manifest) {
			/*[PR 98078]*/
			/*[MSG "K0222", "No Manifest found in jar file: {0}"]*/
			System.err.println(com.ibm.oti.util.Msg.getString("K0222", args[0])); //$NON-NLS-1$
			return;
		}
		
		// Main class name from the jarFile
		String mainClass = JarRunner.mainClassName(manifest);
		if (mainClass == null) {
/*[MSG "K01c6", "No Main-Class specified in manifest: {0}"]*/
			System.err.println(com.ibm.oti.util.Msg.getString("K01c6", args[0])); //$NON-NLS-1$
			return;
		}
		
		// Get the main method from the mainClass
		Class runnable = Class.forName(mainClass, true, ClassLoader.getSystemClassLoader());
		Class mainParams[] = new Class[1];
		mainParams[0] = args.getClass();
		Method mainMethod =  runnable.getMethod("main", mainParams); //$NON-NLS-1$
		
		// Run the main method	
		Object params[] = new Object[1];
		String margs[] = new String[args.length - 1];
		System.arraycopy(args, 1, margs, 0, (args.length - 1));
		params[0] = margs;
		mainMethod.invoke(null, params);
	}
	
	private static String mainClassName(Manifest manifest) throws IOException {
		Attributes mainAttrib = manifest.getMainAttributes();
		String name = mainAttrib.getValue(Attributes.Name.MAIN_CLASS);
		/*[PR 93573]*/
		if (name != null) name = name.replace('/', '.');
		return name;
	}
	
	private static Manifest getManifest(String jarFileName) throws IOException {
		/*[PR 98078]*/
		JarFile jar = new JarFile(jarFileName);
		return jar.getManifest();
	}	
}

