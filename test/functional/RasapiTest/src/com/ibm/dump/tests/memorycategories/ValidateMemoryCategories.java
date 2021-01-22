/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.memorycategories;

import java.io.File;
import java.io.IOException;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageFactory;

/**
 * Test application that reads a DTFJ image and validates that the state of its
 * JavaRuntimeMemoryCategories.
 * 
 * It uses knowledge of the memory categories that should be present to check
 * the memory categories are being calculated, stored and parsed correctly.
 * 
 * Usage:
 * 
 * java com.ibm.memorycategories.ValidateMemoryCategories <DTFJ image factory class> <name of dump>
 */
public class ValidateMemoryCategories
{

	
	public static void main(String args[]) throws Exception
	{
		if (args.length < 2) {
			System.err.println("Unexpected number of arguments. Got " + args.length);
			usage();
			return;
		}
		
		String imageFactoryClassName = args[0];
		String dumpPath = args[1];
		
		System.err.println("Loading DTFJ Image");
		
		Image image = loadImage(imageFactoryClassName, dumpPath);
		
		DTFJMemoryCategoryTest test = new DTFJMemoryCategoryTest(image);
		
		boolean success = false;
		
		try {
			success = test.runTest();
		} finally {
			if (success) {
				System.err.println("Test PASSED");
			} else {
				System.err.println("Test FAILED");
				System.err.println("Failure reasons:");
				test.printFailures();
			}
		}
		
	}

	private static Image loadImage(String imageFactoryClassName, String dumpPath) {
		Class<?> clazz = null;
		try {
			clazz = Class.forName(imageFactoryClassName);
		} catch (ClassNotFoundException e) {
			System.err.println("Cannot load supplied ImageFactory class name: " + imageFactoryClassName);
			System.exit(1);
		}
		
		if (! ImageFactory.class.isAssignableFrom(clazz)) {
			System.err.println("Supplied class: " + clazz.getName() + " does not implement the ImageFactory interface");
		}

		ImageFactory factory = null;
		try {
			factory = (ImageFactory) clazz.newInstance();
		} catch (Exception e) {
			System.err.println("Couldn't instantiate image factory from class: " + clazz.getName());
			e.printStackTrace();
			System.exit(1);
		}
		
		
		try {
			return factory.getImage(new File(dumpPath));
		} catch (IOException e) {
			System.err.println("Error creating Image from ImageFactory");
			e.printStackTrace();
			System.exit(1);
		}
		
		//Can't get here
		throw new Error("Control shouldn't reach here");
	}

	private static void usage()
	{
		System.err.println("Usage:");
		System.err.println();
		System.err.println("java " + ValidateMemoryCategories.class.getName() + " <DTFJ image factory class> <path to dump>");
	}
}