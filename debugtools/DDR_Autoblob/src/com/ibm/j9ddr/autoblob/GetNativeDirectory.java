/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob;

/**
 * 
 * Application that prints the name of the directory containing the native
 * component of the autoblob system.
 * 
 * We build the natives for the 32 bit version of each platform. (So we have a lin_x86 directory that
 * is used for 32 & 64 bit Linux).
 * 
 */
public class GetNativeDirectory
{
	public static void main(String[] args)
	{
		System.out.println(getPlatform());
	}

	public static String getPlatform()
	{
		String osShortName = getOSShortName();
		String osArch = getArchName();

		return osShortName + "_" + osArch;
	}

	public static String getOSShortName()
	{
		// get the osName and make it lowercase
		String osName = System.getProperty("os.name").toLowerCase();

		// set the shortname to the osName if the current system is AIX this is all that is needed
		String osShortName = osName;

		// if we are on z/OS remove the slash
		if (osName.equals("z/os")) {
			osShortName = "zos";
		}

		// if we are on a Windows machine use win as the shortname
		if (osName.length() >= 6 && osName.substring(0, 6).equals("window")) {
			osShortName = "win";
		}

		return osShortName;
	}

	public static String getArchName()
	{
		// get the architecture name (ppc, amd etc), os name (z/OS,
		// linux, etc) and the architecture type (31, 32, 64)
		String osArch = System.getProperty("os.arch");

		// if the current system is a 390 machine use 390 as the osArch
		if (osArch.length() >= 4 && osArch.substring(0, 4).equals("s390")) {
			osArch = "390";
		}

		// if the current system is AMD64 use x86 as the osArch
		else if (osArch.equals("amd64")) {
			osArch = "x86";
		}

		// if the current system is PPC64 use ppc as the osArch
		else if (osArch.equals("ppc64")) {
			osArch = "ppc";
		}

		// if the current system is i?86 where ? is a digit use x86
		else if (osArch.length() == 4 && osArch.charAt(0) == 'i'
				&& Character.isDigit(osArch.charAt(1))
				&& osArch.substring(2).equals("86")) {
			osArch = "x86";
		}

		return osArch;
	}

}
