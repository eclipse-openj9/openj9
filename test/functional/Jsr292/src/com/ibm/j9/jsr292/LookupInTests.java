/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package com.ibm.j9.jsr292;
import org.testng.annotations.Test;
import org.testng.Assert;

import java.lang.invoke.MethodHandles;
import static java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.lookup;
import static java.lang.invoke.MethodHandles.publicLookup;

import com.ibm.j9.jsr292.SamePackageExample.SamePackageInnerClass;
import com.ibm.j9.jsr292.SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2;
import com.ibm.j9.jsr292.SamePackageExample.SamePackageInnerClass2;
import com.ibm.j9.jsr292.SamePackageExample.SamePackageInnerClass2.SamePackageInnerClass2_Nested_Level2;
import com.ibm.j9.jsr292.SamePackageExample.SamePackageInnerClass_Protected;
import com.ibm.j9.jsr292.SamePackageExample.SamePackageInnerClass_Static;

import examples.PackageExamples;

/**
 *This test case verifies lookup modes being permitted by the Lookup factory object for various Lookup classes (callers) 
 *using test scenarios involving Lookup classes(callers) candidates of various access restriction enforcement.  
 */
public class LookupInTests {
	
	static final int NO_ACCESS = 0;
	static final int PUBLIC_PACKAGE_MODE =  MethodHandles.Lookup.PUBLIC | MethodHandles.Lookup.PACKAGE; // 9
	static final int PUBLIC_PACKAGE_PRIVATE_MODE = MethodHandles.Lookup.PUBLIC | MethodHandles.Lookup.PRIVATE | MethodHandles.Lookup.PACKAGE; //11
	static final int PUBLIC_PACKAGE_PROTECTED_MODE =  MethodHandles.Lookup.PUBLIC |  MethodHandles.Lookup.PACKAGE | MethodHandles.Lookup.PROTECTED; //13
	static final int FULL_ACCESS_MODE = MethodHandles.Lookup.PUBLIC | MethodHandles.Lookup.PRIVATE | MethodHandles.Lookup.PROTECTED | MethodHandles.Lookup.PACKAGE; //15
	
	final Lookup localLookup = lookup();
	final Lookup localPublicLookup = publicLookup();
	final Lookup packageExamplesLookup = PackageExamples.getLookup();
	final Lookup samePackageExampleLookup = SamePackageExample.getLookup();
	final Lookup samePackageExample2Lookup = SamePackageExample2.getLookup();
	final Lookup samePackageExampleSubclassLookup = SamePackageExampleSubclass.getLookup();
	
	SamePackageExample spe = new SamePackageExample();
	SamePackageInnerClass innerClassD = spe.new SamePackageInnerClass();
	final Lookup samePackageInnerClassLookup = spe.new SamePackageInnerClass().getLookup();
	final Lookup samePackageInnerClass_Nested_Level2Lookup = innerClassD.new SamePackageInnerClass_Nested_Level2().getLookup();
	
	/******************************************
	 * Basic Sanity tests for Lookup modes
	 * ****************************************/
	
	/**
	 * Validates public access mode of a Lookup factory created by a call to MethodHandles.publicLookup() 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testPublicLookup() throws Throwable {
		assertClassAndMode(localPublicLookup, Object.class, MethodHandles.Lookup.PUBLIC);
		Assert.assertEquals(localPublicLookup.lookupModes(), MethodHandles.Lookup.PUBLIC);
		
		Lookup newLookup = localPublicLookup.in(int[].class);
		assertClassAndMode(newLookup, int[].class, localPublicLookup.lookupModes());
	}
	
	/**
	 * Validates access restriction stored in a Lookup factory object that are applied to its own lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_inObject_Self() throws Throwable {
		Lookup newLookup = localLookup.in(LookupInTests.class);
		assertClassAndMode(newLookup, LookupInTests.class, FULL_ACCESS_MODE);
	}

	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is java.lang.Object.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_inObject() throws Throwable {
		Lookup inObject = packageExamplesLookup.in(Object.class);
		assertClassAndMode(inObject, Object.class, MethodHandles.Lookup.PUBLIC);
	}
	
	/**
	 * Using an old lookup object that was created from a call to MethodHandles.publicLookup(), 
	 * validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is java.lang.Object. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_inObject_Using_publicLookup() throws Throwable {
		Lookup lookup = PackageExamples.getPublicLookup();
		Lookup inObject = lookup.in(int.class);
		
		/*NOTE:
		 * This is equivalent of the failing test case above, but this one works.
		 * The only difference is here we are using MethodHandles.publicLookup() 
		 * instead of MethodHandles.lookup() to obtain the original Lookup object.
		 * */
		assertClassAndMode(inObject, int.class, MethodHandles.Lookup.PUBLIC);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from the Lookup object of this class (LookupInTests) 
	 * where the new lookup class is junit.framework.Assert. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_in_Assert() throws Throwable {
		Lookup newLookup = localLookup.in(Assert.class);
		assertClassAndMode(newLookup, Assert.class, MethodHandles.Lookup.PUBLIC);
	}
	
	/*******************************************************************************
	 * Tests involving same package, cross package, super class, subclass lookups
	 * *****************************************************************************/
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object 
	 * where the new lookup class is a different class but under the same package as the lookup class 
	 * of the original Lookup object.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_SamePackageLookup() throws Throwable {
		Lookup inObject = samePackageExampleLookup.in(SamePackageSingleMethodInterfaceExample.class);
		assertClassAndMode(inObject, SamePackageSingleMethodInterfaceExample.class, PUBLIC_PACKAGE_MODE);
	}
	
	/**
	 * Validates that, if a new lookup class differs from the old one, protected members will not be 
	 * accessible by virtue of inheritance. The test creates a new Lookup object from an old Lookup object 
	 * where the new Lookup class is a subclass of the original lookup class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_SamePackageLookup_Subclass() throws Throwable {
		Lookup inObject = samePackageExampleLookup.in(SamePackageExampleSubclass.class);
		assertClassAndMode(inObject, SamePackageExampleSubclass.class, PUBLIC_PACKAGE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is the super-class of the original Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_SamePackageLookup_Superclass() throws Throwable {
		Lookup inObject = samePackageExampleSubclassLookup.in(SamePackageExample.class);
		assertClassAndMode(inObject, SamePackageExample.class, FULL_ACCESS_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is in a different package than the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_CrossPackageLookup() throws Throwable {
		Lookup inObject = samePackageExampleLookup.in(PackageExamples.class);
		assertClassAndMode(inObject, PackageExamples.class, MethodHandles.Lookup.PUBLIC);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a subclass of the original lookup class but is residing in a 
	 * different package than the original lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_CrossPackageLookup_Subclass() throws Throwable {
		Lookup inObject = packageExamplesLookup.in(CrossPackageExampleSubclass.class);
		assertClassAndMode(inObject, CrossPackageExampleSubclass.class, MethodHandles.Lookup.PUBLIC);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is the superclass of the old lookup class but is residing in a 
	 * different package than the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_CrossPackageLookup_Superclass() throws Throwable {
		Lookup lookup = CrossPackageExampleSubclass.getLookup();
		Lookup inObject = lookup.in(PackageExamples.class);
		assertClassAndMode(inObject, PackageExamples.class, Lookup.PUBLIC);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a non-public outer class in the same Java source file as the old lookup class
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PrivateOuterClassLookup() throws Throwable {
		Lookup inObj = localLookup.in(PackageClass.class);
		assertClassAndMode(inObj, PackageClass.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a non-public outer class belonging to the same package as the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_SamePackageLookup_PrivateOuterClass() throws Throwable {
		Lookup inObj = samePackageExampleLookup.in(PackageClass.class);
		assertClassAndMode(inObj, PackageClass.class, PUBLIC_PACKAGE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a non-public outer class belonging to a different package than the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_CrossPackageLookup_PrivateOuterClass() throws Throwable {
		Lookup inObj = packageExamplesLookup.in(PackageClass.class);
		assertClassAndMode(inObj, PackageClass.class, NO_ACCESS);
	}
	
	/***************************************************
	 * Tests involving public inner classes 
	 * ************************************************/
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a public inner class under the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicInnerClassLookup() throws Throwable {
		Lookup inObj = samePackageExampleLookup.in(SamePackageInnerClass.class);
		assertClassAndMode(inObj, SamePackageInnerClass.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
		
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is the outer class of the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicOuterClassLookup() throws Throwable {
		Lookup inObj = samePackageInnerClassLookup.in(SamePackageExample.class);
		assertClassAndMode(inObj, SamePackageExample.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a public inner class under the super-class of the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicInnerClassLookup_Subclass() throws Throwable {
		Lookup inObj = samePackageExampleSubclassLookup.in(SamePackageInnerClass.class);
		assertClassAndMode(inObj, SamePackageInnerClass.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a public inner class inside a top level class belonging to the same 
	 * package as the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicInnerClassLookup_SamePackage() throws Throwable {
		Lookup inObj = samePackageExample2Lookup.in(SamePackageInnerClass.class);
		assertClassAndMode(inObj, SamePackageInnerClass.class, PUBLIC_PACKAGE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a public inner class inside a top level class belonging to a different 
	 * package than the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicInnerClassLookup_CrossPackage() throws Throwable {
		Lookup inObj = packageExamplesLookup.in(SamePackageInnerClass.class);
		assertClassAndMode(inObj, SamePackageInnerClass.class, MethodHandles.Lookup.PUBLIC);
	}
	
	/***************************************************
	 * Tests involving protected inner classes 
	 * ************************************************/
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a protected inner class under the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_ProtectedInnerClassLookup() throws Throwable {
		Lookup inObj = samePackageExampleLookup.in(SamePackageInnerClass_Protected.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Protected.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
		
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is the outer class of the original lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_ProtectedOuterClassLookup() throws Throwable {
		SamePackageExample spe = new SamePackageExample();
		Lookup lookup = spe.new SamePackageInnerClass_Protected().getLookup();
		Lookup inObj = lookup.in(SamePackageExample.class);
		assertClassAndMode(inObj, SamePackageExample.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a protected inner class under the super-class of the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_ProtectedInnerClassLookup_Subclass() throws Throwable {
		Lookup inObj = samePackageExampleSubclassLookup.in(SamePackageInnerClass_Protected.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Protected.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a protected inner class inside a top level class belonging to the same 
	 * package as the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_ProtectedInnerClassLookup_SamePackage() throws Throwable {
		Lookup inObj = samePackageExample2Lookup.in(SamePackageInnerClass_Protected.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Protected.class, PUBLIC_PACKAGE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a protected inner class inside a top level class belonging to a different 
	 * package than the old lookup class.
	 * Note: the access flag of a protected class is set to public when compiled to a class file.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_ProtectedInnerClassLookup_CrossPackage() throws Throwable {
		Lookup inObj = packageExamplesLookup.in(SamePackageInnerClass_Protected.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Protected.class, MethodHandles.Lookup.PUBLIC);
	}
	
	/***************************************************
	 * Tests involving static inner classes 
	 * ************************************************/
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a static inner class under the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_StaticInnerClassLookup() throws Throwable {
		Lookup inObj = samePackageExampleLookup.in(SamePackageInnerClass_Static.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Static.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is the outer class of the original lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_StaticdOuterClassLookup() throws Throwable {
		Lookup lookup = SamePackageExample.SamePackageInnerClass_Static.getLookup();
		Lookup inObj = lookup.in(SamePackageExample.class);
		assertClassAndMode(inObj, SamePackageExample.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a static inner class under the super-class of the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_StaticInnerClassLookup_Subclass() throws Throwable {
		Lookup inObj = samePackageExampleSubclassLookup.in(SamePackageInnerClass_Static.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Static.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a static inner class inside a top level class belonging to the same 
	 * package as the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_StaticInnerClassLookup_SamePackage() throws Throwable {
		Lookup inObj = samePackageExample2Lookup.in(SamePackageInnerClass_Static.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Static.class, PUBLIC_PACKAGE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a static inner class inside a top level class belonging to a different 
	 * package than the old lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_StaticInnerClassLookup_CrossPackage() throws Throwable {
		Lookup inObj = packageExamplesLookup.in(SamePackageInnerClass_Static.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Static.class, NO_ACCESS);
	}
	
	/***************************************************
	 * Tests involving nested public inner classes 
	 * ************************************************/
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a second level inner class under the old lookup class which is a first level inner class.
	 * Basically we validate that a nested class C.D can access private members within another nested class C.D.E.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicNestedInnerClassLookup_Level1() throws Throwable {
		Lookup inObj = samePackageInnerClassLookup.in(SamePackageInnerClass_Nested_Level2.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Nested_Level2.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a second level inner class under the old lookup class which is the top level outer class.
	 * Basically we validate that a top level class C can access private members within a nested class C.D.E.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicNestedInnerClassLookup_Level2() throws Throwable {
		Lookup inObj = samePackageExampleLookup.in(SamePackageInnerClass_Nested_Level2.class);
		assertClassAndMode(inObj, SamePackageInnerClass_Nested_Level2.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is an inner class that shares the same outer class as the old lookup class which is 
	 * another inner class. Basically we validate that a nested class C.D can access private members within another 
	 * nested class C.B.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicInnerClassLookup_ParallelInnerClasses_Level1() throws Throwable {
		Lookup inObj = samePackageInnerClassLookup.in(SamePackageInnerClass2.class);
		assertClassAndMode(inObj, SamePackageInnerClass2.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is an second level inner class that shares the same top level outer class as the old lookup class which is 
	 * another second level inner class. Basically we validate that a nested class C.D.E can access private members within another 
	 * nested class C.B.F
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicInnerClassLookup_ParallelInnerClasses_Level2() throws Throwable {
		SamePackageExample spe = new SamePackageExample();
		SamePackageInnerClass spei_level1 = spe.new SamePackageInnerClass();
		SamePackageInnerClass_Nested_Level2 spei_level2 = spei_level1.new SamePackageInnerClass_Nested_Level2();
		
		Lookup lookup = spei_level2.getLookup();
		Lookup inObj = lookup.in(SamePackageInnerClass2_Nested_Level2.class);
		assertClassAndMode(inObj, SamePackageInnerClass2_Nested_Level2.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is a first level inner class on top of the old lookup class which is the second 
	 * level inner class. Basically we validate that a nested class C.D.E can access private members within 
	 * another nested class C.D.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicNestedOuterClassLookup_Level2() throws Throwable {		
		Lookup inObj = samePackageInnerClass_Nested_Level2Lookup.in(SamePackageInnerClass.class);
		assertClassAndMode(inObj, SamePackageInnerClass.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class is the top level outer class on top of the old lookup class which is the second 
	 * level inner class. Basically we validate that a nested class C.D.E can access private members within 
	 * the top level outer class C.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_PublicNestedOuterClassLookup_Level1() throws Throwable {
		Lookup inObj = samePackageInnerClass_Nested_Level2Lookup.in(SamePackageExample.class);
		assertClassAndMode(inObj, SamePackageExample.class, PUBLIC_PACKAGE_PRIVATE_MODE);
	}
	
	
	/****************************************************
	 * Tests involving cross class loaders 
	 * ***************************************************/
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class was loaded using a custom class loader unrelated to the class loader 
	 * that loaded the original lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_UnrelatedClassLoaders() throws Throwable {
		ParentCustomClassLoader unrelatedClassLoader = new ParentCustomClassLoader(LookupInTests.class.getClassLoader());
		Object customLoadedClass = unrelatedClassLoader.loadClass("com.ibm.j9.jsr292.CustomLoadedClass1" ).newInstance();
		
		Lookup inObject = samePackageExampleLookup.in(customLoadedClass.getClass());
		assertClassAndMode(inObject, customLoadedClass.getClass(), MethodHandles.Lookup.PUBLIC);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class was loaded using a custom class loader which is the parent class loader 
	 * of the class loader  that loaded the original lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_RelatedClassLoaders_ChildLookupInParent() throws Throwable {
		ParentCustomClassLoader parentCustomCL = new ParentCustomClassLoader(LookupInTests.class.getClassLoader());
		Object customLoadedClass1 = parentCustomCL.loadClass("com.ibm.j9.jsr292.CustomLoadedClass1" ).newInstance();
		
		Assert.assertTrue(customLoadedClass1.getClass().getClassLoader() instanceof ParentCustomClassLoader);
		
		ChildCustomClassLoader childCustomCL = new ChildCustomClassLoader(parentCustomCL);
		ICustomLoadedClass customLoadedClass2 = (ICustomLoadedClass) childCustomCL.loadClass("com.ibm.j9.jsr292.CustomLoadedClass2" ).newInstance();
		 
		Assert.assertTrue(customLoadedClass2.getClass().getClassLoader() instanceof ChildCustomClassLoader);
		
		Lookup lookup = customLoadedClass2.getLookup();
	    Lookup inObject = lookup.in(customLoadedClass1.getClass());
	    
	    assertClassAndMode(inObject, customLoadedClass1.getClass(), MethodHandles.Lookup.PUBLIC);
	}
	
	/**
	 * Validates access restrictions stored in a new Lookup object created from an old Lookup object
	 * where the new lookup class was loaded using a custom class loader which is the child class loader 
	 * of the class loader that loaded the original lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testLookup_RelatedClassLoaders_ParentLookupInChild() throws Throwable {
		ParentCustomClassLoader parentCustomCL = new ParentCustomClassLoader(LookupInTests.class.getClassLoader());
		ICustomLoadedClass customLoadedClass1 = (ICustomLoadedClass) parentCustomCL.loadClass("com.ibm.j9.jsr292.CustomLoadedClass1" ).newInstance();
		
		Assert.assertTrue(customLoadedClass1.getClass().getClassLoader() instanceof ParentCustomClassLoader);
		
		ChildCustomClassLoader childCustomCL = new ChildCustomClassLoader(parentCustomCL);
		ICustomLoadedClass customLoadedClass2 = (ICustomLoadedClass) childCustomCL.loadClass("com.ibm.j9.jsr292.CustomLoadedClass2" ).newInstance();
		 
		Assert.assertTrue(customLoadedClass2.getClass().getClassLoader() instanceof ChildCustomClassLoader);
		
		Lookup lookup = customLoadedClass1.getLookup();
	    Lookup inObject = lookup.in(customLoadedClass2.getClass());
	    
	    assertClassAndMode(inObject, customLoadedClass2.getClass(), MethodHandles.Lookup.PUBLIC);
	}
	
	/**
	 *Test for MethodHandles.Lookup.toString() class where we check output depending on various access modes. 
	 */
	@Test(groups = { "level.extended" })
	public void test_Lookup_toString() {
		Lookup publicLookup = MethodHandles.publicLookup();
		Assert.assertEquals(publicLookup.toString(), "java.lang.Object/public");
		
		Lookup lookup = PackageExamples.getLookup();
		Lookup inObject = lookup.in(Object.class); 
		Assert.assertEquals(inObject.toString(), "java.lang.Object/public");
		
		lookup = MethodHandles.lookup();
		Lookup inAssertObject = lookup.in(Assert.class);
		Assert.assertEquals(inAssertObject.toString(), "org.testng.Assert/public");
	
		lookup = SamePackageExample.getLookup();
		Lookup inCrossPackageClass = lookup.in(PackageExamples.class);
		Assert.assertEquals(inCrossPackageClass.toString(), "examples.PackageExamples/public");
		
		lookup = SamePackageExample.getLookup();
		Lookup inSamePackageClass = lookup.in(SamePackageSingleMethodInterfaceExample.class);
		Assert.assertEquals(inSamePackageClass.toString(), "com.ibm.j9.jsr292.SamePackageSingleMethodInterfaceExample/package");

		lookup = MethodHandles.lookup();
		Lookup inPrivateClass = lookup.in(PackageClass.class);
		Assert.assertEquals(inPrivateClass.toString(), "com.ibm.j9.jsr292.LookupInTests$PackageClass/private");
		
		lookup = MethodHandles.publicLookup();
		Lookup inNoAccess = lookup.in(PackageClass.class);
		Assert.assertEquals(inNoAccess.toString(), "com.ibm.j9.jsr292.LookupInTests$PackageClass/noaccess");
	}
	
	/**
	 *Non-public outer class used in tests
	 */
	class PackageClass { } 
	
	/**
	 * Helper validation method. Validates the lookup class and lookup modes of the Lookup object being tested.
	 * @param lookup - Lookup object being tested 
	 * @param c - Expected Lookup class 
	 * @param mode - Expected Lookup Modes
	 */
	protected void assertClassAndMode(Lookup lookup, Class<?> c, int mode) {
		Assert.assertTrue(lookup.lookupClass().equals(c), "Lookup class mismatch. Expected: " + c + ", found: " + lookup.lookupClass());
		Assert.assertTrue(lookup.lookupModes() == mode, "Lookup mode mismatch. Expected: " + mode + ", found: " + lookup.lookupModes());
	}
}
