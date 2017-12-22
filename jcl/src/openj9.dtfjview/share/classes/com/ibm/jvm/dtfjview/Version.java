/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview;

import com.ibm.dtfj.image.ImageFactory;

public class Version {

	/*
	 * Major version numbers:
	 * 1 = legacy (DDR debug extensions available from 1.1)
	 * 2 = batch mode, plugin support and multiple vm support
	 * 3 = zip support
	 * 4 = SPI for external applications
	 */
	private static int majorVersion = 4;
	private static int minorVersion = 29;	// minor version is the VM stream
	private static int buildVersion = 5;	// build version - historically the tag from RAS_Auto-Build/projects.psf - now just a number
	
	private static String name = "DTFJView";
	
	public static String getVersion()
	{
		return Integer.toString(majorVersion) + "." +
			Integer.toString(minorVersion) + "." +
			Integer.toString(buildVersion);
	}
	
	public static String getName()
	{
		return name;
	}
	
	public static String getAllVersionInfo(ImageFactory factory)
	{
		String DTFJViewVersion = getName() + " version " + getVersion();
		String DTFJAPIVersion = ImageFactory.DTFJ_MAJOR_VERSION	+ "." + ImageFactory.DTFJ_MINOR_VERSION + "."
			+ factory.getDTFJModificationLevel();

		return DTFJViewVersion + ", using DTFJ version " + DTFJAPIVersion;
	}
}
