package com.ibm.j9.offload.tests.mapping;

/*******************************************************************************
 * Copyright (c) 2009, 2012 IBM Corp. and others
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

/**
 * this is a generic main that can be used to load a class and print out it's toString value
 * the test uses this by writing the mapping file and setting appropriate classpath values on the command line
 * and then using this class to load a class from the mapped/unmapped file as appropriate and the
 * toString value tells us which version we have loaded
 */
public class LoadClass {

	public static void main(String[] args) {
		Class theClass;
		try {
			theClass = Class.forName(args[0]);
		} catch (ClassNotFoundException e){
			System.out.println("NOT FOUND");
			System.out.flush();
			return;
		} catch (Throwable t){
			return;
		}
		System.out.println("!FOUND!");
		System.out.flush();
		try {
			Object theObject = theClass.newInstance();
			System.out.println(theObject.toString());
			System.out.flush();
		} catch (InstantiationException e){
			
		} catch (IllegalAccessException e){
		
		}
	}
}
