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
package com.ibm.jvmti.tests.followReferences;

public class fr004 
{

	public static native boolean followObjectPrimitiveFields(Object obj, Class filter);
	
	public boolean testFollowPrimitiveFields()
	{
		C1 c1 = new C1();
		C2 c2 = new C2();
		C3 c3 = new C3();
		
		TagManager.clearTags();
		TagManager.setObjectTag(c1, 0x1000L);
		TagManager.setObjectTag(c1.getClass(), 0x1001L);
		TagManager.setObjectTag(c2, 0x2000L);
		TagManager.setObjectTag(c2.getClass(), 0x2001L);		
		TagManager.setObjectTag(c3, 0x3000L);
		TagManager.setObjectTag(c3.getClass(), 0x3001L);
						
		boolean ret = followObjectPrimitiveFields(null, c3.getClass());
		if (ret == false)
			return ret;
			
		/* hold a ref to c3 such that we dont get GC'ed before heap iteration */	
		int foo = c3.c3_int;
		
		if (TagManager.isTagQueued(0x3001L) == false) {
			System.out.println("3001 missing");
			return false;
		}
		
		if (TagManager.isTagQueued(0x3000L) == false) {
			System.out.println("3000 missing");
			return false;
		}
		
		return ret;
	}
	
	public String helpFollowPrimitiveFields()
	{
		return "follow references from a specific object without any class filtering";
	}
	
}


class C1 {
	public int c1_int = 100;
	public static int c1_static_int = 101;
}

class C2 extends C1 {
	public int c2_int = 200;
	public static int c2_static_int = 201;
}

class C3 extends C2 {
	public int c3_int = 300;
	public static int c3_static_int = 301;
}
