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
package com.ibm.jvmti.tests.redefineClasses;

import com.ibm.jvmti.tests.util.Util;

public class rc001 {
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);
	
	public boolean setup(String args) {
		return true;
	}
	
	public boolean testRedefineToSelf()	{
		int pre, post;

		rc001_testReplaceSelfWithSelf t_pre = new rc001_testReplaceSelfWithSelf();
		pre = t_pre.meth1();
				
		boolean redefined = Util.redefineClass(getClass(), rc001_testReplaceSelfWithSelf.class, rc001_testReplaceSelfWithSelf.class);
		if (!redefined) {
			return false;
		}

		rc001_testReplaceSelfWithSelf t_post = new rc001_testReplaceSelfWithSelf();
		post = t_post.meth1();
		
		System.out.println("Pre  Replace: " + pre);
		System.out.println("Post Replace: " + post);
		
		if (pre != 0) {
			return false;
		}
		
		if (pre != post) {
			return false;
		}
		
		return true;
	}
	
	public String helpRedefineToSelf() {
		return "Tests class redefinition where the redefined class is exactly the same as the currently loaded one. ie no changes have been done.";
	}

	public boolean testRedefineMethodBody() {
		int pre, post;

		rc001_testReplaceMethodBody_O1 t_pre = new rc001_testReplaceMethodBody_O1();
		pre = t_pre.meth1();

		boolean redefined = Util.redefineClass(getClass(), rc001_testReplaceMethodBody_O1.class, rc001_testReplaceMethodBody_R1.class);
		if (!redefined) {
			return false;
		}

		rc001_testReplaceMethodBody_O1 t_post = new rc001_testReplaceMethodBody_O1();
		post = t_post.meth1();
				
		System.out.println("Pre  Replace: " + pre);
		System.out.println("Post Replace: " + post);
		
		if (pre != 0) {
			return false;
		}
		
		if (pre == post) {
			return false;
		}
		
		if (post != 100) {
			return false;
		}
		
		return true;		
	}
	
	public String helpRedefineMethodBody() {
		return "Tests modification of the method body code.";
	}


	public boolean testVTableFixups() {
		int pre, post;

		rc001_testVTableFixups_SubB t_pre = new rc001_testVTableFixups_SubB();
		pre = t_pre.subclassMethodB();

		System.out.println("Replacement 1 Starting");
		boolean redefined = Util.redefineClass(getClass(), rc001_testVTableFixups_O1.class, rc001_testVTableFixups_R1.class);
		if (!redefined) {
			return false;
		}
		System.out.println("Replacement 1 DONE");

		rc001_testVTableFixups_SubB t_post = new rc001_testVTableFixups_SubB();
		post = t_post.subclassMethodB();
				
				
		System.out.println("Pre  Replace: " + pre + " old: " + t_pre.subclassMethodB());
		System.out.println("Post Replace: " + post);
		
		if (pre != 102) {
			return false;
		}
		
		if (pre == post) {
			return false;
		}
		
		if (post != 202) {
			return false;
		}
		
		// Hit it again, to make sure that the jit vtable has been fixed correctly 
		rc001_testVTableFixups_SubB t_post2 = new rc001_testVTableFixups_SubB();
		post = t_post2.subclassMethodB();
		if (post != 202) {			
			return false;
		}
		
		System.out.println("Replacement 2 Starting");
		redefined = Util.redefineClass(getClass(), rc001_testVTableFixups_O1.class, rc001_testVTableFixups_R1.class);
		if (!redefined) {
			return false;
		}
		System.out.println("Replacement 2 DONE");

		rc001_testVTableFixups_SubB t_post3 = new rc001_testVTableFixups_SubB();
		post = t_post3.subclassMethodB();
		if (post != 202) {			
			return false;
		}
		
		System.out.println("Post Replace: " + post);
		return true;		
	}
	
	public String helpVTableFixups() {
		return "Tests whether the subclass vtables have been fixed correctly after a superclass has been replaced.";
	}


	public boolean testStaticFixups() {
		
		// **************** FIRST ROUND *******************

		rc001_testStaticFixups_O1 t1_pre = new rc001_testStaticFixups_O1();
		int preInt = (int) t1_pre.getStaticInt();
		String preVersion = t1_pre.getClassVersion();

		System.out.println("Replacement 1 Starting");
		boolean redefined = Util.redefineClass(getClass(), rc001_testStaticFixups_O1.class, rc001_testStaticFixups_R1.class);
		if (!redefined) {
			return false;
		}
		System.out.println("Replacement 1 DONE");

		rc001_testStaticFixups_O1 t1_post = new rc001_testStaticFixups_O1();
		int postInt = t1_post.getStaticInt();
		String postVersion = t1_pre.getClassVersion();
			
				
		System.out.println("Pre  Replace: " + preInt + " old: " + t1_pre.getStaticInt());
		System.out.println("Post Replace: " + postInt);
		
		if (preInt != 100) {
			return false;
		}
				
		if (postInt == 200) {
			return false;
		}

		// *************************************************************************************************
		// After successful redefine, the static address resolved in the new class 
		// should be exactly the same as in the old class. The remapping magic happens in findFieldInClass()		
		if (preInt != postInt) {
			return false;
		}
		
		if (preVersion != "original")
			return false;
		
		if (postVersion != "redefined")
			return false;
		
		if (preVersion == postVersion)
			return false;

		
		System.out.println("Replacement 1 Verified");

		// **************** SECOND ROUND *******************

		rc001_testStaticFixups_O1 t2_pre = new rc001_testStaticFixups_O1();
		int preInt2 = (int) t2_pre.getStaticInt();
	//	String preVersion2 = t2_pre.getClassVersion();

		System.out.println("Replacement 2 Starting");
		redefined = Util.redefineClass(getClass(), rc001_testStaticFixups_O1.class, rc001_testStaticFixups_R2.class);
		if (!redefined) {
			return false;
		}
		System.out.println("Replacement 2 DONE");

		rc001_testStaticFixups_O1 t2_post = new rc001_testStaticFixups_O1();
		int postInt2 = t2_post.getStaticInt();
	//	String postVersion2 = t2_pre.getClassVersion();
			
				
		System.out.println("Pre  Replace: " + preInt2 + " old: " + t2_pre.getStaticInt());
		System.out.println("Post Replace: " + postInt2);
		
		if (preInt2 != 100) {
			return false;
		}
				
		if (postInt2 == 200 || postInt2 == 300) {
			return false;
		}

		System.out.println("Replacement 2 Verified");
		
		System.out.println("t1_pre: " + t1_pre.getStaticInt());
		System.out.println("t2_pre: " + t2_pre.getStaticInt());
	
		System.out.println("t1_post: " + t1_post.getStaticInt());
		System.out.println("t2_post: " + t2_post.getStaticInt());
				
		return true;		
	}
	
	public String helpStaticFixups() {
		return "Tests whether the statics in the new version of the class have been fixed to resolve to the statics in the old version";
	}
}
