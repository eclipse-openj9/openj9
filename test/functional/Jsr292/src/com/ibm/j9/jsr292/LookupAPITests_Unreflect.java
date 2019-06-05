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
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import examples.PackageExamples;

/**
 * @author mesbah
 * This class contains tests for the unreflect*() methods of the MethodHandles.Lookup API.
 */
public class LookupAPITests_Unreflect {
	
	/******************************************************************************************************
	 * unreflect tests. 
	 * Combinations : same package - public [static] and private [static] , virtual [overridden] methods
	 * ***************************************************************************************************/
	
	/**
	 * Test to unreflect a public virtual method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Public_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		Method m = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		
		MethodHandle mh = MethodHandles.lookup().unreflect( m );
		SamePackageExample g = new SamePackageExample();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	

	/**
	 * Test to unreflect a public interface method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Interface_Public_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageSingleMethodInterfaceExample" );
		Method m = clazz.getDeclaredMethod( "singleMethodAdd", int.class, int.class );
		
		MethodHandle mh = MethodHandles.lookup().unreflect( m );
		SamePackageSingleMethodInterfaceExample g = new SingleMethodInterfaceImpl();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * Test to unreflect a public overridden method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Public_SamePackage_Overridden() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExampleSubclass" );
		Method m = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		
		MethodHandle mh = MethodHandles.lookup().unreflect( m );
		SamePackageExample g = new SamePackageExampleSubclass();
		int out = ( int ) mh.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 4, out );
	}
	
	/**
	 * Test to unreflect a public static method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Public_Static_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		Method m = clazz.getDeclaredMethod( "addPublicStatic", int.class, int.class );
		
		MethodHandle mh = MethodHandles.lookup().unreflect( m );
		int out = ( int ) mh.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * Test to unreflect a private method belonging to a class in the same package as the Lookup class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Private_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		boolean illegalAccessExceptionThrown = false;
	 
		Method method = clazz.getDeclaredMethod( "addPrivate", int.class, int.class );
		
		try {
			MethodHandles.lookup().unreflect(method);
			System.out.println("IllegalAccessException is NOT thrown while trying to unreflect a private method " +
					"of a class different from the lookup class.");
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true; 
		}
		
		AssertJUnit.assertTrue ( illegalAccessExceptionThrown );
	}
	
	/**
	 * Testto unreflect a private static method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Private_Static_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		boolean illegalAccessExceptionThrown = false;
		
		Method method = clazz.getDeclaredMethod( "addPrivateStatic", int.class, int.class );
		
		try {
			MethodHandles.lookup().unreflect(method);
			System.out.println("IllegalAccessException is NOT thrown while trying to unreflect a private static method " +
					"of a class different from the lookup class.");
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true; 
		}
		
		AssertJUnit.assertTrue ( illegalAccessExceptionThrown );
	}
	
	/**
	 * Test to unreflect a protected overridden method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Protected_SamePackage_Overridden() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExampleSubclass" );
		Method m = clazz.getDeclaredMethod( "addProtected", int.class, int.class );
		
		MethodHandle mh = MethodHandles.lookup().unreflect( m );
		SamePackageExample g = new SamePackageExampleSubclass();
		int out = ( int ) mh.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 13, out );
	}
	
	/****************************************************************************************************
	 * unreflect tests. 
	 * Combinations :Cross package - public [static] and private [static] , virtual [overridden] methods
	 * **************************************************************************************************/
	
	/**
	 * Test to unreflect a public virtual method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Public_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "examples.PackageExamples" );
		Method m = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		
		MethodHandle mh = MethodHandles.lookup().unreflect( m );
		PackageExamples g = new PackageExamples();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * Test to unreflect a public overridden method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Public_CrossPackage_Overridden() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.CrossPackageExampleSubclass" );
		Method m = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		
		MethodHandle mh = MethodHandles.lookup().unreflect( m );
		PackageExamples g = new CrossPackageExampleSubclass();
		int out = ( int ) mh.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 5, out );
	}
	
	/**
	 * Test to unreflect a public static method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Public_Static_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "examples.PackageExamples" );
		Method m = clazz.getDeclaredMethod( "addPublicStatic", int.class, int.class );
		
		MethodHandle mh = MethodHandles.lookup().unreflect( m );
		int out = ( int ) mh.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 3, out );
		
	}
	
	/**
	 * Test to unreflect a private method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Private_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "examples.PackageExamples" );
		boolean illegalAccessExceptionThrown = false;
		Method method = clazz.getDeclaredMethod( "addPrivate", int.class, int.class );
		
		try {
			MethodHandles.lookup().unreflect(method);
			System.out.println("IllegalAccessException is NOT thrown while trying to unreflect a private method " +
					"of a class belonging to a different package than that of the lookup class.");
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true; 
		}
		
		AssertJUnit.assertTrue ( illegalAccessExceptionThrown );
	}
	
	/**
	 * Test to unreflect a private static method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Private_Static_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "examples.PackageExamples" );
		boolean illegalAccessExceptionThrown = false;
		Method method = clazz.getDeclaredMethod( "addPrivateStatic", int.class, int.class );
		
		try {
			MethodHandles.lookup().unreflect(method);
			System.out.println("IllegalAccessException is NOT thrown while trying to unreflect a private static method " +
					"of a class belonging to a different package than that of the lookup class.");
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true; 
		}
		
		AssertJUnit.assertTrue ( illegalAccessExceptionThrown );
	}
	
	/**
	 * Test to unreflect a protected overridden method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Protected_CrossPackage_Overridden() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.CrossPackageExampleSubclass" );
		Method m = clazz.getDeclaredMethod( "addProtected", int.class, int.class );
		
		MethodHandle mh = MethodHandles.lookup().unreflect( m );
		PackageExamples g = new CrossPackageExampleSubclass();
		int out = ( int ) mh.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( -1, out );
	}
	
	/****************************************************************************
	 * unreflectGetter, unreflectSetter tests. 
	 * Combinations : same package - public [static] and private [static] fields
	 ****************************************************************************/
	
	/**
	 * unreflectSetter/getter test using a public virtual method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_SetterGetter_Public_SamePackage() throws Throwable {
		Field nonStaticPublicField = SamePackageExample.class.getDeclaredField( "nonStaticPublicField" );
		SamePackageExample g = new SamePackageExample();
		MethodHandle mhSetter = MethodHandles.lookup().unreflectSetter( nonStaticPublicField );
		mhSetter.invokeExact( g, 100 );
		
		MethodHandle mhGetter = MethodHandles.lookup().unreflectGetter( nonStaticPublicField );
		int out = ( int )mhGetter.invokeExact( g );
		
		AssertJUnit.assertEquals( 100, out );
	}
	
	/* CMVC 196603 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_Getter_Public_Final_SamePackage() throws Throwable {
		SamePackageExample g = new SamePackageExample();
		
		Field f = SamePackageExample.class.getDeclaredField("nonStaticFinalPublicField");
		MethodHandle mh = MethodHandles.lookup().unreflectGetter(f);
		
		int o = (int)mh.invokeExact(g);
		AssertJUnit.assertEquals(g.nonStaticFinalPublicField, o);
	}
	
	/* CMVC 196603 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_Setter_Public_Static_SamePackage() throws Throwable {
		try {
			Field f = SamePackageExample.class.getDeclaredField("nonStaticFinalPublicField");
			MethodHandles.lookup().unreflectSetter(f);
			Assert.fail("No exception thrown finding a setter on a final field");
		} catch(IllegalAccessException e) {
			// Exception expected, test passed
		}
	}
	
	/* CMVC 196603 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_Getter_Public_Static_Final_SamePackage() throws Throwable {
		/* Makes sure the class is loaded */
		int o = SamePackageExample.staticFinalPublicField;
		Field f = SamePackageExample.class.getDeclaredField("staticFinalPublicField");
		MethodHandle mh = MethodHandles.lookup().unreflectGetter(f);
		
		o = (int)mh.invokeExact();
		AssertJUnit.assertEquals(SamePackageExample.staticFinalPublicField, o);
	}
	
	/* CMVC 196603 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_Setter_Public_Static_Final_SamePackage() throws Throwable {
		try {
			Field f = SamePackageExample.class.getDeclaredField("staticFinalPublicField");
			MethodHandles.lookup().unreflectSetter(f);
			Assert.fail("No exception thrown finding a setter on a final field");
		} catch(IllegalAccessException e) {
			// Exception expected, test passed
		}
	}
	
	/**
	 * Negative test : unreflectSetter/getter test using a private method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_SetterGetter_Private_SamePackage() throws Throwable {
		SamePackageExample g = new SamePackageExample();
		Field nonStaticPrivateField = SamePackageExample.class.getDeclaredField( "nonStaticPrivateField" );
		MethodHandle mhSetter = null;
		boolean illegalAccessExceptionThrown = false;
		try {
			mhSetter = MethodHandles.lookup().unreflectSetter( nonStaticPrivateField );
		}
		catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		nonStaticPrivateField.setAccessible( true );
		
		mhSetter = MethodHandles.lookup().unreflectSetter( nonStaticPrivateField );
		
		mhSetter.invokeExact( g, 100 );
		
		MethodHandle mhGetter = MethodHandles.lookup().unreflectGetter( nonStaticPrivateField );
		int out = ( int )mhGetter.invokeExact( g );
		
		AssertJUnit.assertEquals( 100, out );
	}
	
	/**
	 * unreflectSetter/getter test using a public static method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_SetterGetter_Public_Static_SamePackage() throws Throwable {
		Field staticPublicField = SamePackageExample.class.getDeclaredField( "staticPublicField" );
		MethodHandle mhSetter = MethodHandles.lookup().unreflectSetter( staticPublicField );
		mhSetter.invokeExact( 100 );
		
		MethodHandle mhGetter = MethodHandles.lookup().unreflectGetter( staticPublicField );
		int out = ( int )mhGetter.invokeExact();
		
		AssertJUnit.assertEquals( 100, out );
	}
	
	/*
	 * Test looks up a static field in the super class using a subclass.  This ensures that we
	 * use the right class in the VM dispatch targets when accessing the field.
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_SetterGetter_Public_Static_SamePackage_Superclass() throws Throwable {
		Field staticPublicField = SamePackageExampleSubclass.class.getField( "publicStaticField_doNotDeclareInSubClasses" );
		MethodHandle mhSetter = MethodHandles.lookup().unreflectSetter( staticPublicField );
		mhSetter.invokeExact( 100 );
		/* Validate the setter actually set the field, and not random memory */
		AssertJUnit.assertEquals( SamePackageExample.publicStaticField_doNotDeclareInSubClasses, 100 );
		
		MethodHandle mhGetter = MethodHandles.lookup().unreflectGetter( staticPublicField );
		int out = ( int )mhGetter.invokeExact();
		
		AssertJUnit.assertEquals( out, 100 );
	}
	
	
	/**
	 * Negative test: unreflectSetter/getter test using a private static method belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_SetterGetter_Private_Static_SamePackage() throws Throwable {
		 
		Field staticPrivateField = SamePackageExample.class.getDeclaredField( "staticPrivateField" );
		MethodHandle mhSetter = null;
		boolean illegalAccessExceptionThrown = false;
		try {
			mhSetter = MethodHandles.lookup().unreflectSetter( staticPrivateField );
		}
		catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		staticPrivateField.setAccessible( true );
		
		mhSetter = MethodHandles.lookup().unreflectSetter( staticPrivateField );
		mhSetter.invokeExact( 100 );
		
		MethodHandle mhGetter = MethodHandles.lookup().unreflectGetter( staticPrivateField );
		int out = ( int )mhGetter.invokeExact();
		
		AssertJUnit.assertEquals( 100, out );
	}
	
	/****************************************************************************** 
	 * unreflectGetter unreflectSetter tests. 
	 * Combinations : cross package - public [static] and private [static] fields
	 ******************************************************************************/
	
	/**
	 * unreflectSetter/getter test using a public virtual method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_SetterGetter_Public_CrossPackage() throws Throwable {
		Field nonStaticPublicField = PackageExamples.class.getDeclaredField( "nonStaticPublicField" );
		PackageExamples g = new PackageExamples();
		MethodHandle mhSetter = MethodHandles.lookup().unreflectSetter( nonStaticPublicField );
		mhSetter.invokeExact( g, 100 );
		
		MethodHandle mhGetter = MethodHandles.lookup().unreflectGetter( nonStaticPublicField );
		int out = ( int )mhGetter.invokeExact( g );
		
		AssertJUnit.assertEquals( 100, out );
	}
	
	/**
	 * Negative test : unreflectSetter/getter test using a private method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_SetterGetter_Private_CrossPackage() throws Throwable {
		PackageExamples g = new PackageExamples();
		Field nonStaticPrivateField = PackageExamples.class.getDeclaredField( "nonStaticPrivateField" );
		MethodHandle mhSetter = null;
		boolean illegalAccessExceptionThrown = false;
		try {
			mhSetter = MethodHandles.lookup().unreflectSetter( nonStaticPrivateField );
		}
		catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		nonStaticPrivateField.setAccessible( true );
		
		mhSetter = MethodHandles.lookup().unreflectSetter( nonStaticPrivateField );
		mhSetter.invokeExact( g, 100 );
		
		MethodHandle mhGetter = MethodHandles.lookup().unreflectGetter( nonStaticPrivateField );
		int out = ( int )mhGetter.invokeExact( g );
		
		AssertJUnit.assertEquals( 100, out );
	}
	
	/**
	 * unreflectSetter/getter test using a public static method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_SetterGetter_Public_Static_CrossPackage() throws Throwable {
		Field staticPublicField = PackageExamples.class.getDeclaredField( "staticPublicField" );
		MethodHandle mhSetter = MethodHandles.lookup().unreflectSetter( staticPublicField );
		mhSetter.invokeExact( 100 );
		
		MethodHandle mhGetter = MethodHandles.lookup().unreflectGetter( staticPublicField );
		int out = ( int )mhGetter.invokeExact();
		
		AssertJUnit.assertEquals( 100, out );
	}
	
	/**
	 * Negative test : unreflectSetter/getter test using a private static method belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_SetterGetter_Private_Static_CrossPackage() throws Throwable {
		 
		Field staticPrivateField = PackageExamples.class.getDeclaredField( "staticPrivateField" );
		MethodHandle mhSetter = null;
		boolean illegalAccessExceptionThrown = false;
		try {
			mhSetter = MethodHandles.lookup().unreflectSetter( staticPrivateField );
		}
		catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		staticPrivateField.setAccessible( true );
		
		mhSetter = MethodHandles.lookup().unreflectSetter( staticPrivateField );
		mhSetter.invokeExact( 100 );
		
		MethodHandle mhGetter = MethodHandles.lookup().unreflectGetter( staticPrivateField );
		int out = ( int )mhGetter.invokeExact();
		
		AssertJUnit.assertEquals( 100, out );
	}

	/***********************************************
	 * unreflectConstructor tests
	 * Combinations : Same package , cross package
	 ***********************************************/
	
	/**
	 * Test to unreflect a constructor belonging to the same package as the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Constructor_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		Constructor c = clazz.getDeclaredConstructor( int.class, int.class );
		MethodHandle mh = MethodHandles.lookup().unreflectConstructor( c );
		SamePackageExample example = ( SamePackageExample )mh.invoke( 1, 2 );
		AssertJUnit.assertEquals( example.nonStaticPublicField, 3 );
	}
	
	/**
	 * Test to unreflect a constructor belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Constructor_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "examples.PackageExamples" );
		Constructor c = clazz.getDeclaredConstructor( int.class, int.class );
		MethodHandle mh = MethodHandles.lookup().unreflectConstructor( c );
		PackageExamples example = ( PackageExamples )mh.invoke( 1, 2 );
		AssertJUnit.assertEquals( example.nonStaticPublicField, 3 );
	}
	
	/***************************************************************************************************
	 * unreflectSpecial tests. 
	 * Combinations :Same package - public [static] and private [static] , virtual [overridden] methods
	 ***************************************************************************************************/
	
	/**
	 * unreflectSpecial test using a public virtual method belonging to the same package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Special_Public_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		Method m = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		boolean iaeThrown = false;
		try {
			MethodHandles.lookup().unreflectSpecial( m, SamePackageExample.class ); 
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/**
	 * unreflectSpecial test using a public overridden method belonging to the same package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Special_Public_SamePackage_Overriddden() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExampleSubclass" );
		Method m = clazz.getDeclaredMethod( "addProtected", int.class, int.class );
		boolean iaeThrown = false;
		try {
			MethodHandles.lookup().unreflectSpecial( m, SamePackageExampleSubclass.class ); 
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/**
	 * Negative test : unreflectSpecial test using a private method belonging to the same package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Special_Private_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		Method m = clazz.getDeclaredMethod( "addPrivate", int.class, int.class );
		boolean illegalAccessExceptionThrown = false;
		
		try {
			MethodHandles.lookup().unreflectSpecial( m, SamePackageExample.class ); 
		}
		catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		m.setAccessible( true );
		illegalAccessExceptionThrown = false;
		try {
			MethodHandles.lookup().unreflectSpecial( m, SamePackageExample.class ); 
		} catch (IllegalAccessException e) {
			illegalAccessExceptionThrown = true;
		}
		assert(illegalAccessExceptionThrown);
	}
	
	/**
	 * unreflectSpecial test using a public static method belonging to the same package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Special_Public_Static_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		Method m = clazz.getDeclaredMethod( "addPublicStatic", int.class, int.class );
		boolean illegalAccessExceptionThrown = false;
		
		try {
			MethodHandles.lookup().unreflectSpecial( m, SamePackageExample.class );
		}
		catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : unreflectSpecial test using a private static method belonging to the same package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Special_Private_Static_SamePackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		Method m = clazz.getDeclaredMethod( "addPrivateStatic", int.class, int.class );
		boolean illegalAccessExceptionThrown = false;
		
		try {
			MethodHandles.lookup().unreflectSpecial( m, SamePackageExample.class ); 
		}
		catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/*******************************************************************************
	 * unreflectSpecial tests. 
	 * Combinations: Cross package - public, private, virtual [overridden] methods
	 *******************************************************************************/
	
	/**
	 * unreflectSpecial test using a public method belonging to a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Special_Public_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "examples.PackageExamples" );
		Method m = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		boolean iaeThrown = false;
		try {
			MethodHandles.lookup().unreflectSpecial( m, PackageExamples.class ); 
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/* CMVC: 195406 - JSR292: Segfault executing unreflected JLO.getClass() */
	@Test(groups = { "level.extended" })
	public void test_unreflecting_nonVtableMethods() throws Throwable {
		Method reflectedGetClass = Object.class.getMethod("getClass");
		MethodHandle mhGetClass = MethodHandles.lookup().unreflect(reflectedGetClass);
		
		AssertJUnit.assertEquals(this.getClass(), mhGetClass.invoke(this));
	}
	
	/**
	 * Negative : unreflectSpecial test using a private method belonging to a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Special_Private_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "examples.PackageExamples" );
		Method m = clazz.getDeclaredMethod( "addPrivate", int.class, int.class );
		boolean illegalAccessExceptionThrown = false;
		
		try {
			MethodHandles.lookup().unreflectSpecial( m, PackageExamples.class ); 
		}
		catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		m.setAccessible( true );
		illegalAccessExceptionThrown = false;
		try {
			MethodHandles.lookup().unreflectSpecial( m, PackageExamples.class );
		} catch (IllegalAccessException e) {
			illegalAccessExceptionThrown = true;
		}
		assert(illegalAccessExceptionThrown);
	}
	
	/**
	 * unreflectSpecial test using a public overridden method belonging to a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Special_Public_CrossPackage_Overriddden() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.CrossPackageExampleSubclass" );
		Method m = clazz.getDeclaredMethod( "addProtected", int.class, int.class );
		boolean iaeThrown = false;
		try {
			MethodHandles.lookup().unreflectSpecial( m, CrossPackageExampleSubclass.class ); 
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/**
	 * Private access required to create special MH. Try with public lookup object.
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_WeakenedLookup() throws Throwable {
		Method m = LookupAPITests_Find.class.getMethod("toString", null);
		boolean iaeThrown = false;
		try {
			MethodHandles.publicLookup().unreflectSpecial(m, LookupAPITests_Find.class);
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/**
	 * Private access required to create special MH. Try with public lookup object with accessible method.
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_WeakenedLookupAccessibleMethod() throws Throwable {
		Method m = LookupAPITests_Find.class.getMethod("toString", null);
		m.setAccessible(true);
		boolean iaeThrown = false;
		try {
			MethodHandles.publicLookup().unreflectSpecial(m, LookupAPITests_Find.class);
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/**
	 * Special MH lookup requires that the accessClass is identical to the callerClass (specialToken). Try with different classes.
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_NeqCallerClassAccessClass() throws Throwable {
		Method m = SamePackageExample.class.getMethod("toString", null);
	    boolean iaeThrown = false;
	    try {
			MethodHandles.lookup().unreflectSpecial(m, SamePackageExample.class);
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/**
	 * Special MH lookup requires that the accessClass is identical to the callerClass (specialToken). Try with different classes and with accessible method.
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_NeqCallerClassAccessClassAccessibleMethod() throws Throwable {
		Method m = SamePackageExample.class.getMethod("toString", null);
		m.setAccessible(true);
	    boolean iaeThrown = false;
	    try {
			MethodHandles.lookup().unreflectSpecial(m, SamePackageExample.class);
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/*********************************************************************
	 *unreflect tests involving inheritance.
	 *Combinations : parent / child lookups within the same package 
	 *********************************************************************/
	
	/**
	 * unreflect a subclass method using a super class lookup, parent child in same package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_SamePackage_SubClass() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExampleSubclass" );
		Method subclassMethod = clazz.getDeclaredMethod( "addPublic_Child", int.class, int.class );
		
		Lookup parentLookup = SamePackageExample.getLookup();
		MethodHandle mh = parentLookup.unreflect( subclassMethod );
		
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 5, out );
	}
	
	/**
	 * unreflect a subclass overridden method using a superclass lookup, parent child in same package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_SamePackage_SubClass_Overridden() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExampleSubclass" );
		Method subclassOverriddenMethod = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		
		Lookup parentLookup = SamePackageExample.getLookup();
		MethodHandle mh = parentLookup.unreflect( subclassOverriddenMethod );
		
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();    //SamePackageExample g = new SamePackageExampleSubClass()
		int out = ( int ) mh.invokeExact( g, 1, 2 ); 						//TODO:- WMT thrown here if 'g' is of the parent type, like above
		AssertJUnit.assertEquals( 4, out );
	}

	/**
	 * unreflect a super class method using a subclass lookup, parent child in same package
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_SamePackage_SuperClass() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		Method parentMethod = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		
		Lookup childLookup = SamePackageExampleSubclass.getLookup();
		MethodHandle mh = childLookup.unreflect( parentMethod );
		
		SamePackageExample g = new SamePackageExample();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	
	/*********************************************************************
	 *unreflect tests involving inheritance.
	 *Combinations : parent / child lookups , cross-package
	 *********************************************************************/
	
	/**
	 * unreflect a subclass method using a super class lookup, parent child in different packages.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_CrossPackage_SubClass() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.CrossPackageExampleSubclass" );
		Method subclassMethod = clazz.getDeclaredMethod( "addPublic_Child", int.class, int.class );
		
		Lookup parentLookup = PackageExamples.getLookup();
		MethodHandle mh = parentLookup.unreflect( subclassMethod );
		
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 5, out );
	}
	
	/**
	 * unreflect a subclass overridden method using a superclass lookup, parent child in different packages.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_CrossPackage_SubClass_Overridden() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.CrossPackageExampleSubclass" );
		Method subclassOverriddenMethod = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		
		Lookup parentLookup = PackageExamples.getLookup();
		MethodHandle mh = parentLookup.unreflect( subclassOverriddenMethod );
		
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();  //PackageExamples g = new SamePackageExampleSubClass()
		int out = ( int ) mh.invokeExact( g, 1, 2 ); 						//TODO:- WMT thrown here if 'g' is of the parent type, like above
		AssertJUnit.assertEquals( 5, out );
	}

	/**
	 * unreflect a super class method using a subclass lookup, parent child in different packages.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_CrossPackage_SuperClass() throws Throwable {
		Class clazz = Class.forName( "examples.PackageExamples" );
		Method parentMethod = clazz.getDeclaredMethod( "addPublic", int.class, int.class );
		
		Lookup childLookup = CrossPackageExampleSubclass.getLookup();
		MethodHandle mh = childLookup.unreflect( parentMethod );
		
		PackageExamples g = new PackageExamples();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	
	/*********************************************************************
	 *unreflectConstructor tests involving inheritance.
	 *Combinations : parent / child lookups within the same package 
	 *********************************************************************/
	
	/**
	 * unreflect subclass constructor using super class lookup, parent child in same package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_UnreflectConstructor_SamePackage_SubClass() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExampleSubclass" );
		Constructor c = clazz.getDeclaredConstructor( int.class, int.class );
		
		Lookup parentLookup = SamePackageExample.getLookup();
		MethodHandle mh = parentLookup.unreflectConstructor( c );
				
		SamePackageExampleSubclass g = ( SamePackageExampleSubclass )mh.invoke( 1, 2 );
		AssertJUnit.assertEquals( g.nonStaticPublicField_Child, 3 );
	}

	/**
	 * unreflect a super class constructor using a subclass lookup, parent child in same package
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_UnreflectConstructor_SamePackage_SuperClass() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample" );
		Constructor c = clazz.getDeclaredConstructor( int.class, int.class );
		
		Lookup childLookup = SamePackageExampleSubclass.getLookup();
		MethodHandle mh = childLookup.unreflectConstructor( c );
				
		SamePackageExample g = ( SamePackageExample )mh.invoke( 1, 2 );
		AssertJUnit.assertEquals( g.nonStaticPublicField, 3 );
	}
	
	
	/*********************************************************************
	 *unreflectConstructor tests involving inheritance.
	 *Combinations : parent / child lookups , cross-package
	 *********************************************************************/
	
	/**
	 * unreflect subclass constructor using super class lookup, parent child in different packages.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_UnreflectConstructor_CrossPackage_SubClass() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.CrossPackageExampleSubclass" );
		Constructor c = clazz.getDeclaredConstructor( int.class, int.class );
		
		Lookup parentLookup = PackageExamples.getLookup();
		MethodHandle mh = parentLookup.unreflectConstructor( c );
				
		CrossPackageExampleSubclass g = ( CrossPackageExampleSubclass )mh.invoke( 1, 2 );
		AssertJUnit.assertEquals( g.nonStaticPublicField_Child, 3 );
	}

	/**
	 * unreflect a super class constructor using a subclass lookup, parent child in different packages.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_UnreflectConstructor_CrossPackage_SuperClass() throws Throwable {
		Class clazz = Class.forName( "examples.PackageExamples" );
		Constructor c = clazz.getDeclaredConstructor( int.class, int.class );
		
		Lookup childLookup = CrossPackageExampleSubclass.getLookup();
		MethodHandle mh = childLookup.unreflectConstructor( c );
				
		PackageExamples g = ( PackageExamples )mh.invoke( 1, 2 );
		AssertJUnit.assertEquals( g.nonStaticPublicField, 3 );
	}
	
	/*****************************************************************************************************
	 * unreflect tests involving inner classes.
	 * Combination : Different levels of inner/outer class public/protected methods unreflected using 
	 *               different levels of outer/inner public/protected classes, same package/cross package.
	 *****************************************************************************************************
	
	/**
	 * unreflect a public method from a level 1 inner class using the top level outer class lookup.
	 * Basically we unreflect a method from class C.D where the lookup class used is C.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_InnerClass_Level1() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass" );
		Method innerclassMethod_Level1 = clazz.getDeclaredMethod( "addPublicInner", int.class, int.class );
		
		Lookup outerclassLookup = SamePackageExample.getLookup();
		MethodHandle mh = outerclassLookup.unreflect( innerclassMethod_Level1 );
		
		SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * unreflect a public method from a level 2 inner class using the top level outer class lookup.
	 * Basically we unreflect a method from class C.D.E where the lookup class used is C.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_InnerClass_Level2() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass$SamePackageInnerClass_Nested_Level2" );
		Method innerclassMethod_Level2 = clazz.getDeclaredMethod( "addPublicInner_Level2", int.class, int.class );
		
		Lookup outerclassLookup = SamePackageExample.getLookup();
		MethodHandle mh = outerclassLookup.unreflect( innerclassMethod_Level2 );
		
		SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}

	/**
	 * unreflect a protected method from a level 1 protected inner class using the top level outer class lookup.
	 * Basically we unreflect a method from protected inner class C.D where the lookup class used is C.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Protected_InnerClass_Level1() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass_Protected" );
		Method innerclassMethod_Level1 = clazz.getDeclaredMethod( "addProtectedInner", int.class, int.class ); 
		
		Lookup outerclassLookup = SamePackageExample.getLookup();
		MethodHandle mh = outerclassLookup.unreflect( innerclassMethod_Level1 );
		
		SamePackageExample.SamePackageInnerClass_Protected g = (new SamePackageExample()).new SamePackageInnerClass_Protected();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * Negative test : unreflect a protected method from a level 1 protected inner class using a top level outer class lookup,
	 * where the lookup class is in a different package.
	 * Basically we unreflect a protected method from protected inner class package1.C.D where the lookup class used is package2.F.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Protected_InnerClass_Level1_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass_Protected" );
		Method innerclassMethod_Level1 = clazz.getDeclaredMethod( "addProtectedInner", int.class, int.class ); 
		
		Lookup outerclassLookup = PackageExamples.getLookup();
		
		boolean illegalAccessExceptionThrown = false; 
		
		try {
			outerclassLookup.unreflect( innerclassMethod_Level1 );
			System.out.println("IllegalAccessException is NOT thrown while trying to unreflect a protected method " +
					"of a protected inner class belonging to a different package than the lookup class");
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * unreflect a protected method from a level 2 protected inner class using the top level outer class lookup.
	 * Basically we unreflect a method from protected inner class C.D.E where the lookup class used is C.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Protected_InnerClass_Level2() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass_Protected$SamePackageInnerClass_Nested_Level2" );
		Method innerclassMethod_Level2 = clazz.getDeclaredMethod( "addProtectedInner_Level2", int.class, int.class );
		
		Lookup outerclassLookup = SamePackageExample.getLookup();
		MethodHandle mh = outerclassLookup.unreflect( innerclassMethod_Level2 );
		
		SamePackageExample.SamePackageInnerClass_Protected.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass_Protected()).new SamePackageInnerClass_Nested_Level2();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * Negative test : unreflect a protected method from a level 2 protected inner class using the top level outer class lookup
	 * belonging to a different package.
	 * Basically we unreflect a method from protected inner class package1.C.D.E where the lookup class used is package2.F.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Protected_InnerClass_Level2_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass_Protected$SamePackageInnerClass_Nested_Level2" );
		Method innerclassMethod_Level2 = clazz.getDeclaredMethod( "addProtectedInner_Level2", int.class, int.class );
		
		Lookup outerclassLookup = PackageExamples.getLookup();
		
		boolean illegalAccessExceptionThrown = false; 
		
		try {
			outerclassLookup.unreflect( innerclassMethod_Level2 );
			System.out.println("IllegalAccessException is NOT thrown while trying to unreflect a protected method " +
					"of a second level protected inner class belonging to a different package than the lookup class");
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue ( illegalAccessExceptionThrown );
	}
	
	/**
	 * unreflect a method from a level 2 inner class using a level 1 inner class lookup.
	 * Basically we unreflect a method in class C.D.E where the lookup class is C.D.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_InnerClass_Nested_SameHierarchy() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass$SamePackageInnerClass_Nested_Level2" );
		Method innerclassMethod_Level2 = clazz.getDeclaredMethod( "addPublicInner_Level2", int.class, int.class );
		
		Lookup level1InnerClassLookup = (new SamePackageExample()).new SamePackageInnerClass().getLookup();
		MethodHandle mh = level1InnerClassLookup.unreflect( innerclassMethod_Level2 );
		
		SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * unreflect a method from a level 2 inner class using a lookup class which is a different level 1 inner class
	 * under the same top level outer class.
	 * Basically we unreflect a method in class C.D.E where the lookup class is C.F.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_InnerClass_Nested_DifferentHierarchy() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass$SamePackageInnerClass_Nested_Level2" );
		Method innerclassMethod_Level2 = clazz.getDeclaredMethod( "addPublicInner_Level2", int.class, int.class );
		
		Lookup level1InnerClassLookup = (new SamePackageExample()).new SamePackageInnerClass2().getLookup();
		MethodHandle mh = level1InnerClassLookup.unreflect( innerclassMethod_Level2 );
		
		SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * Negative test : unreflect a protected method from a level 2 protected inner class using a lookup class which is a 
	 * level 1 public inner class under a top level outer class belonging to a different package than the lookup class.
	 * Basically we unreflect a method in class package1.C.D.E where the lookup class is package2.F.G.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Protected_InnerClass_Nested_DifferentHierarchy_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass_Protected$SamePackageInnerClass_Nested_Level2" );
		Method innerclassMethod_Level2 = clazz.getDeclaredMethod( "addProtectedInner_Level2", int.class, int.class );
		
		Lookup level1InnerClassLookup_crossPackage = (new PackageExamples()).new CrossPackageInnerClass().getLookup();
		
		boolean illegalAccessExceptionThrown = false; 
		
		try {
			level1InnerClassLookup_crossPackage.unreflect( innerclassMethod_Level2 );
			System.out.println("IllegalAccessException NOT thrown while attempting to access " +
					"a protected method inside a level 2 protected inner class in a different package");
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * unreflect a method from a level 1 inner class using a lookup class which is a level 2 inner class
	 * under the same top level outer class.
	 * Basically we unreflect a method in class C.D where the lookup class is C.D.E
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Public_OuterClass_Nested_SametHierarchy() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass" );
		Method innerclassMethod_Level1 = clazz.getDeclaredMethod( "addPublicInner", int.class, int.class );
		
		Lookup level2InnerClassLookup = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2().getLookup();
		MethodHandle mh = level2InnerClassLookup.unreflect( innerclassMethod_Level1 );
		
		SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
		int out = ( int ) mh.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}

	/**
	 * Negative test : unreflect a protected method from a level 1 protected inner class using a lookup class which is a level 2 public inner class
	 * under a top level outer class belonging to a different package than the lookup class.
	 * Basically we unreflect a protected method in class package1.C.D where the lookup class is package2.F.G.H.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "unchecked", "rawtypes" })
	public void test_Unreflect_Protected_OuterClass_Nested_DifferentHierarchy_CrossPackage() throws Throwable {
		Class clazz = Class.forName( "com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass_Protected" );
		Method innerclassMethod_Level1 = clazz.getDeclaredMethod( "addProtectedInner", int.class, int.class );
		
		Lookup level2InnerClassLookup = ((new PackageExamples()).new CrossPackageInnerClass()).new CrossPackageInnerClass2_Nested_Level2().getLookup();
		
		boolean illegalAccessException = false; 
		
		try {
			level2InnerClassLookup.unreflect( innerclassMethod_Level1 );
			System.out.println("IllegalAccessExcetpion NOT thrown while attempting to unreflect a protected inner class " +
					"method from a different class");
		} catch ( IllegalAccessException e ) {
			illegalAccessException = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessException );
	}
	
	
	/****************************************************
	 * unreflect Tests involving custom-loaded classes. 
	 * ***************************************************/
	
	/**
	 * unreflect a static method belonging to a class that has been dynamically loaded using a custom class loader
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Static_UnrelatedClassLoaders() throws Throwable {
		ParentCustomClassLoader unrelatedClassLoader = new ParentCustomClassLoader( LookupAPITests_Unreflect.class.getClassLoader() );
		Class customLoadedClass = unrelatedClassLoader.loadClass( "com.ibm.j9.jsr292.CustomLoadedClass1" );
		
		Method method = customLoadedClass.getDeclaredMethod( "addStatic", int.class, int.class );
		MethodHandle mh = MethodHandles.lookup().unreflect( method );
		
		int out = ( int ) mh.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 103, out );
	}
	
	/**
	 * unreflect a static method belonging to a class that has been dynamically loaded using a custom class loader A
	 * using a lookup class which is also dynamically loaded but using a custom class loader B which is a child of A.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Static_RelatedClassLoaders_ChildLookupInParent() throws Throwable {
		ParentCustomClassLoader parentCustomCL = new ParentCustomClassLoader( LookupAPITests_Unreflect.class.getClassLoader() );
		Class customLoadedClass1 = parentCustomCL.loadClass( "com.ibm.j9.jsr292.CustomLoadedClass1" );
		
		ChildCustomClassLoader childCustomCL = new ChildCustomClassLoader( parentCustomCL );
		ICustomLoadedClass customLoadedClass2 = (ICustomLoadedClass) childCustomCL.loadClass( "com.ibm.j9.jsr292.CustomLoadedClass2" ).newInstance();
		
		Lookup childLookup = customLoadedClass2.getLookup();
	    
		Method method = customLoadedClass1.getDeclaredMethod( "addStatic", int.class, int.class );
		MethodHandle mh = childLookup.unreflect( method );
		
		int out = ( int ) mh.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 103, out );
	}
	
	/**
	 * unreflect a static method belonging to a class that has been dynamically loaded using a custom class loader A
	 * using a lookup class which is also dynamically loaded but using a custom class loader B which is the parent of A.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes", "unchecked" })
	public void test_Unreflect_Static_RelatedClassLoaders_ParentLookupInChild() throws Throwable {
		ParentCustomClassLoader parentCustomCL = new ParentCustomClassLoader( LookupAPITests_Unreflect.class.getClassLoader() );
		ICustomLoadedClass customLoadedClass1 = (ICustomLoadedClass) parentCustomCL.loadClass( "com.ibm.j9.jsr292.CustomLoadedClass1" ).newInstance();
		
		ChildCustomClassLoader childCustomCL = new ChildCustomClassLoader( parentCustomCL );
		Class customLoadedClass2 = childCustomCL.loadClass( "com.ibm.j9.jsr292.CustomLoadedClass2" );
		
		Lookup parentLookup = customLoadedClass1.getLookup();
	    
		Method method = customLoadedClass2.getDeclaredMethod( "addStatic", int.class, int.class );
		MethodHandle mh = parentLookup.unreflect( method );
		
		int out = ( int ) mh.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 4, out );
	}
	
	/**
	 * Test that &#60;clinit&#62; of the defining class is called when a <code>DirectHandle</code> is invoked (not when unreflected)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unreflect_clinit() throws Throwable {
		Unreflect_InvokeTracker.reset();
		Method m = Unreflect_StaticMethodHelper.class.getMethod("method");
		MethodHandles.Lookup l = MethodHandles.lookup();
		MethodHandle mh = l.unreflect(m);
		Unreflect_InvokeTracker.calledUnreflect = true;
		AssertJUnit.assertTrue(Unreflect_InvokeTracker.stateUnreflected());
		mh.invoke();
		AssertJUnit.assertTrue(Unreflect_InvokeTracker.stateInvoked());
	}
	
	/**
	 * Test that &#60;clinit&#62; of the defining class is called when a <code>ConstructorHandle</code> is invoked (not when unreflected)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	@SuppressWarnings({ "rawtypes" })
	public void test_UnreflectConstructor_clinit() throws Throwable {
		Unreflect_InvokeTracker.reset();
		Constructor c = Unreflect_ConstructorHelper.class.getConstructor();
		MethodHandles.Lookup l = MethodHandles.lookup();
		MethodHandle mh = l.unreflectConstructor(c);
		Unreflect_InvokeTracker.calledUnreflect = true;
		AssertJUnit.assertTrue(Unreflect_InvokeTracker.stateUnreflected());
		mh.invoke();
		AssertJUnit.assertTrue(Unreflect_InvokeTracker.stateInvoked());
	}
	
	/**
	 * Test that &#60;clinit&#62; of the defining class is called when a <code>StaticFieldGetterHandle</code> is invoked (not when unreflected)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_UnreflectGetter_clinit() throws Throwable {
		Unreflect_InvokeTracker.reset();
		Field m = Unreflect_StaticGetterHelper.class.getField("staticField");
		MethodHandles.Lookup l = MethodHandles.lookup();
		MethodHandle mh = l.unreflectGetter(m);
		Unreflect_InvokeTracker.calledUnreflect = true;
		AssertJUnit.assertTrue(Unreflect_InvokeTracker.stateUnreflected());
		String s = (String) mh.invoke();
		AssertJUnit.assertTrue(Unreflect_InvokeTracker.stateInitialized());
		AssertJUnit.assertEquals("TEST", s);
	}
	
	/**
	 * Test that &#60;clinit&#62; of the defining class is called when a <code>StaticFieldSetterHandle</code> is invoked (not when unreflected)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_UnreflectSetter_clinit() throws Throwable {
		Unreflect_InvokeTracker.reset();
		Field m = Unreflect_StaticSetterHelper.class.getField("staticField");
		MethodHandles.Lookup l = MethodHandles.lookup();
		MethodHandle mh = l.unreflectSetter(m);
		Unreflect_InvokeTracker.calledUnreflect = true;
		AssertJUnit.assertTrue(Unreflect_InvokeTracker.stateUnreflected());
		mh.invoke("TEST");
		AssertJUnit.assertTrue(Unreflect_InvokeTracker.stateInitialized());
		AssertJUnit.assertEquals("TEST", Unreflect_StaticSetterHelper.staticField);
	}
}

class Unreflect_StaticMethodHelper {
	static {
		Unreflect_InvokeTracker.calledClinit = Unreflect_InvokeTracker.stateUnreflected();
	}
	public static void method() {
		Unreflect_InvokeTracker.calledInvoke = Unreflect_InvokeTracker.stateInitialized();
	}
}

class Unreflect_ConstructorHelper {
	static {
		Unreflect_InvokeTracker.calledClinit = Unreflect_InvokeTracker.stateUnreflected();
	}
	public Unreflect_ConstructorHelper() {
		Unreflect_InvokeTracker.calledInvoke = Unreflect_InvokeTracker.stateInitialized();
	}
}

class Unreflect_StaticGetterHelper {
	static {
		Unreflect_InvokeTracker.calledClinit = true;
	}
	public static String staticField = "TEST";
}

class Unreflect_StaticSetterHelper {
	static {
		Unreflect_InvokeTracker.calledClinit = true;
	}
	public static String staticField = "";
}

class Unreflect_InvokeTracker {
	public static boolean calledUnreflect = false;
	public static boolean calledClinit = false;
	public static boolean calledInvoke = false;
	
	public static void reset() {
		calledUnreflect = false;
		calledClinit = false;
		calledInvoke = false;
	}
	
	public static boolean stateUnreflected() {
		return calledUnreflect && !calledClinit && !calledInvoke;
	}
	
	public static boolean stateInitialized() {
		return calledUnreflect && calledClinit && !calledInvoke;
	}
	
	public static boolean stateInvoked() {
		return calledUnreflect && calledClinit && calledInvoke;
	}
}
