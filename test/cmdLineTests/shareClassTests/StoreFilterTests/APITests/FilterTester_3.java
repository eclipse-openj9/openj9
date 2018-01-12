/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package APITests;

import java.lang.reflect.Method;
import java.net.URL;

import CustomClassloaders.CustomURLClassLoader;
import Utilities.URLClassPathCreator;

import com.ibm.oti.shared.CannotSetClasspathException;
import com.ibm.oti.shared.HelperAlreadyDefinedException;
import com.ibm.oti.shared.Shared;
import com.ibm.oti.shared.SharedClassHelperFactory;
import com.ibm.oti.shared.SharedClassURLClasspathHelper;

public class FilterTester_3  extends FilterTesterUtils{
	int counter = 0;
	public static void main(String[] args) {
		FilterTester_3 test = new FilterTester_3();
		test.runTests();
	}
	
	public void runTests(){
		boolean result = test3();
			
		if (result) {
			System.out.println("TEST PASSED");
		} else {
			System.out.println("TEST FAILED");
		}
			
	}
	
	public boolean test3(){
		System.out.println("\nTest " + ++counter + ": Store Filter = C_Dummy$Data. Verify shared classpath helper methods.");
		
		String testClassName = "sharedclasses.storefilter.resources.A_Main";
		
		URLClassPathCreator pathCreator = new URLClassPathCreator("./Resources/resources.jar");
		
		CustomURLClassLoader customURLCL = new CustomURLClassLoader(pathCreator.createURLClassPath());
		
		SharedClassHelperFactory schFactory = Shared.getSharedClassHelperFactory();
		SharedClassURLClasspathHelper newHelper;
		
		try {
			newHelper = schFactory.getURLClasspathHelper(customURLCL, pathCreator.createURLClassPath(), new StoreFilter("C_Dummy$Data"));
			if (null == newHelper) {
				System.out.println("\t->newHelper is null.");
				return false;
			}			
		} catch (HelperAlreadyDefinedException e) {
			System.out.println("\t->HelperAlreadyDefinedException is thrown.");
			return false;
		}
		
		try {
			Class c = Class.forName(testClassName, true, customURLCL);
			Method meth = c.getDeclaredMethod("run", new Class[0]);
			invokeMethod(c.newInstance(), "run", new Class[0]);
			
			/* Set the filter to null, 
			 * so there will be no filter to prevent some class names to be looked in shared cache.
			 * See SharedClassFilter#acceptFind 
			 */
			newHelper.setSharingFilter(null);
			
			if (!customURLCL.isClassInSharedCache("sharedclasses.storefilter.resources.A_Main$Data")) {
				System.out.println("\nTEST FAILED - sharedclasses.storefilter.resources.A_Main$Data is not in shared cache.");
				return false;
			}
			
			if (!customURLCL.isClassInSharedCache("sharedclasses.storefilter.resources.B_Dummy$Data")) {
				System.out.println("\nTEST FAILED - sharedclasses.storefilter.resources.B_Dummy$Data is not in shared cache.");
				return false;
			}

			if (customURLCL.isClassInSharedCache("sharedclasses.storefilter.resources.C_Dummy$Data")) {
				System.out.println("\nTEST FAILED - sharedclasses.storefilter.resources.C_Dummy$Data is in shared cache.");
				return false;
			}

			if (!customURLCL.isClassInSharedCache("sharedclasses.storefilter.resources.D_Dummy$Data")) {
				System.out.println("\nTEST FAILED - sharedclasses.storefilter.resources.D_Dummy$Data is not in shared cache.");
				return false;
			}
			
			if (!customURLCL.isClassInSharedCache("sharedclasses.storefilter.resources.A_Main")) {
				System.out.println("\nTEST FAILED - sharedclasses.storefilter.resources.A_Main is not in shared cache.");
				return false;
			}
			
			if (!customURLCL.isClassInSharedCache("sharedclasses.storefilter.resources.B_Dummy")) {
				System.out.println("\nTEST FAILED - sharedclasses.storefilter.resources.B_Dummy is not in shared cache.");
				return false;
			}

			if (!customURLCL.isClassInSharedCache("sharedclasses.storefilter.resources.C_Dummy")) {
				System.out.println("\nTEST FAILED - sharedclasses.storefilter.resources.C_Dummy is not in shared cache.");
				return false;
			}

			if (!customURLCL.isClassInSharedCache("sharedclasses.storefilter.resources.D_Dummy")) {
				System.out.println("\nTEST FAILED - sharedclasses.storefilter.resources.D_Dummy is not in shared cache.");
				return false;
			}
			
			newHelper.setSharingFilter(new StoreFilter("C_Dummy"));
			invokeMethod(c.newInstance(), "run", new Class[0]);
			/* Set the filter to null, 
			 * so there will be no filter to prevent some class names to be looked in shared cache.
			 * See SharedClassFilter#acceptFind 
			 */
			newHelper.setSharingFilter(null);
			
			if (null != newHelper.getSharingFilter()) {
				System.out.println("\nTEST FAILED - Shared Filter is not null");
				return false;
			}
			
			if (newHelper.getClassLoader() != customURLCL) {
				System.out.println("\nTEST FAILED - Wrong classloader in shared classpath helper");
				return false;
			}
			
			newHelper.confirmAllEntries();
			try {
				newHelper.setClasspath(new URL[0]);
				System.out.println("\nTEST FAILED - Shared classpath helper can set new classpath after confirming all entries. ");
				return false;
			} catch (CannotSetClasspathException e) {
				/* Expected */
			}
		} catch (Exception e2) {
			e2.printStackTrace();
			return false;
		}
		return true;
	}
}
