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

public class JavaRuntimeVersion {

	/**
	 * example java.runtime.version: pxa6460sr11-20120403_01 (SR11)
	 */
	public static void main(String[] args) {
        String runTimeVersion =System.getProperty("java.runtime.version");

        System.out.println("java.runtime.version is:" + runTimeVersion);

        String[] properties = runTimeVersion.split("\\s|-");
        if (properties.length < 2) {
                throw new IllegalArgumentException (
                                "cannot locate platform info/time stamp from " + runTimeVersion);
        }
        String plat = properties[0];
        String time = properties[1];

        System.out.println("Plat:      " + plat);
        System.out.println("TimeStamp: " + time);

        System.out.println("JavaRuntimeVersion Test OK");
		

	}

}
