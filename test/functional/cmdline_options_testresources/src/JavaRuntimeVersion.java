/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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

public class JavaRuntimeVersion {

	/**
	 * example java.runtime.version's:
	 *  1.8.0_272-b10
	 *  8.0.7.0 - pxa6480sr7-20200922_01(SR7)
	 *  11.0.10-internal
	 *  11.0.10+5
	 *  obsolete: pxa6460sr11-20120403_01 (SR11)
	 */
	public static void main(String[] args) {
        String runTimeVersion = System.getProperty("java.runtime.version");

        System.out.println("java.runtime.version is:" + runTimeVersion);

        String[] properties = runTimeVersion.split("\\s|-");
        String[] versionParts = properties[0].split("\\+");
        if (versionParts.length > 2) {
        	throw new IllegalArgumentException ("invalid version: " + properties[0]);
        }
        String intialVersion = versionParts[0];
        if (intialVersion.startsWith("1.8") || intialVersion.startsWith("8.")) {
        	// Java 8 can have an underscore
            if (!intialVersion.matches("[1-9][0-9\\.]+") && !intialVersion.matches("[1-9][0-9\\.]+_[1-9][0-9]*")) {
            	throw new IllegalArgumentException ("invalid version: " + intialVersion);
            }
        } else {
	        if (!intialVersion.matches("[1-9][0-9\\.]+")) {
	        	throw new IllegalArgumentException ("invalid version: " + intialVersion);
	        }
        }
        		
        if ((versionParts.length > 1) && !versionParts[1].matches("[1-9][0-9]*")) {
        	throw new IllegalArgumentException ("invalid build: " + versionParts[1]);
        }
 
        System.out.println("Version:  " + properties[0]);
        for (int i = 1; i < properties.length; i++) {
        	System.out.println("optional: " + properties[i]);
        }

        System.out.println("JavaRuntimeVersion Test OK");
	}

}
