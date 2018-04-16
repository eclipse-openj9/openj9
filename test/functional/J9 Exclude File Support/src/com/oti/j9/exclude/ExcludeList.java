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
package com.oti.j9.exclude;

import java.util.Vector;
import java.io.*;

public final class ExcludeList {

	private static final char SEPARATOR_CHAR = ',';

	private String _fID;
	private Vector _fExcludes = new Vector();

	public ExcludeList(String id)     {_fID = id;}
	public void addExclude(Exclude e) {_fExcludes.addElement(e);}

	public Vector elements ()    {return _fExcludes;}

	// Helper: split a string into an array of substrings around a given separator
	public static String[] substrings(String input, char separator)
	{
		Vector tags = new Vector();
		int substringStart = 0;
		int substringEnd = input.indexOf(separator, substringStart);
		String chunk;

		while (substringEnd > 0) {
			chunk = input.substring(substringStart, substringEnd);
			if (chunk.length() > 0)
				tags.addElement(chunk);

			substringStart = substringEnd + 1;
			substringEnd = input.indexOf(separator, substringStart);
		}

		chunk = input.substring(substringStart, input.length());
		if (chunk.length() > 0)
			tags.addElement(chunk);

		String tagArray[] = new String[tags.size()];
		tags.copyInto(tagArray);
		return tagArray;
	}


	// Read the exclude list from the provided location
	public static ExcludeList readFrom(String uri, String platformIDString)
	{
		InputStream uriStream = null;
		try {
			uriStream = new FileInputStream(uri);
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
		return readFrom( uriStream, platformIDString );
	}

	// Read the exclude list from the provided stream
	public static ExcludeList readFrom(InputStream uriStream, String platformIDString)
	{
		XMLInterface xmlParser = new XMLInterface();

		if (!xmlParser.parse(uriStream, substrings(platformIDString,SEPARATOR_CHAR) )) {
			System.out.println("Exiting...");
			return null;
		}

		return xmlParser.getExcludeList();
	}

	public static void main(String[] args) {
//		try {
//			FileInputStream file = new FileInputStream(args[0]);
//			ExcludeList excludes = ExcludeList.readFrom(args[0], args[1]);
//			excludes.explain(System.out);
//		} catch (IOException e) {
//			e.printStackTrace();
//		}
	}

	// Dump a human-readable report
	public void explain(PrintStream outputDevice)
	{
		Vector e = elements();
		int numberOfExcludes = e.size();

		outputDevice.println();
		if (numberOfExcludes == 0) {
			outputDevice.println("No tests are excluded.");
			outputDevice.println();
			return;
		}

		outputDevice.println("Current Date: " + DateUtil.printDate(System.currentTimeMillis()));
		outputDevice.println(numberOfExcludes + " tests are excluded as follows:");
		for (int i=0; i < numberOfExcludes; i++) {
			Exclude exclude = (Exclude) e.elementAt(i);
			outputDevice.println("  ["+ exclude.getID() + "] expiry: " +
				DateUtil.printDate(exclude.getExpiry()) + " :: " + exclude.getReason());
		}
		outputDevice.println();
	}

	// Answer the Exclude for testName, or null
	public Exclude getExclude(String testName)
	{
		//check if either the classname or specific testname is excluded
		int dotindex = testName.lastIndexOf('.');
		String className = "";
		if(dotindex!=-1)
			className = testName.substring(0, dotindex);
		Vector e = elements();
		int numberOfExcludes = e.size();
		for (int i=0; i < numberOfExcludes; i++) {
			Exclude exclude = (Exclude) e.elementAt(i);
			String id = exclude.getID();
			if (testName.equals(id) || className.equals(id) || testName.endsWith(id)) {
				if (exclude.getExpiry() == 0 || System.currentTimeMillis() < exclude.getExpiry())
					return exclude;
			}
		}
		return null;
	}

	// Answer true if testName should be run
	public boolean shouldRun(String testName)
	{
		Exclude exclude = getExclude( testName );
		if ( exclude == null ) return true;
		return false;
	}




}
