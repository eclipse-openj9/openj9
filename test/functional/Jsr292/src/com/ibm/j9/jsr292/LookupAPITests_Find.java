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
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.util.NavigableMap;
import java.util.TreeMap;

import com.ibm.j9.jsr292.SamePackageExample.SamePackageInnerClass;
import com.ibm.j9.jsr292.SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2;

import examples.PackageExamples;
import examples.CrossPackageDefaultMethodInterface;
import examples.PackageExamples.CrossPackageInnerClass;

/**
 * @author mesbah
 * This class contains tests for the find*() method of MethodHandles.Lookup API. 
 */
public class LookupAPITests_Find {

	/*******************************************************************************
	 * findSetter, findGetter, findStaticSetter, findStaticGetter tests. 
	 * COMBINATIONS: same package - public, private, public static, private static, 
	 * protected, super-class, sub-class, inner class fields.
	 * *****************************************************************************/
	
	/**
	 * Basic validation that get of static with method handles works for all types
	 * validate get of static using method handles returns the same values a normal lookup
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_BasicGet() throws Throwable {
		// currently need this or static initializer does not run when we do the lookups
		new BasicStaticGetSetExample();
		
		// get the handles
		MethodHandle intHandle = MethodHandles.lookup().findStaticGetter(BasicStaticGetSetExample.class, "intField",int.class);
		MethodHandle longHandle = MethodHandles.lookup().findStaticGetter(BasicStaticGetSetExample.class, "longField",long.class);
		MethodHandle floatHandle = MethodHandles.lookup().findStaticGetter(BasicStaticGetSetExample.class, "floatField",float.class);
		MethodHandle doubleHandle = MethodHandles.lookup().findStaticGetter(BasicStaticGetSetExample.class, "doubleField",double.class);
		MethodHandle byteHandle = MethodHandles.lookup().findStaticGetter(BasicStaticGetSetExample.class, "byteField",byte.class);
		MethodHandle shortHandle = MethodHandles.lookup().findStaticGetter(BasicStaticGetSetExample.class, "shortField",short.class);
		MethodHandle booleanHandle = MethodHandles.lookup().findStaticGetter(BasicStaticGetSetExample.class, "booleanField",boolean.class);
		MethodHandle charHandle = MethodHandles.lookup().findStaticGetter(BasicStaticGetSetExample.class, "charField",char.class);
		MethodHandle objectHandle = MethodHandles.lookup().findStaticGetter(BasicStaticGetSetExample.class, "objectField",Object.class);
		
		// do the lookup 
		int intValue = (int) intHandle.invoke();
		long longValue = (long) longHandle.invoke();
		float floatValue = (float) floatHandle.invoke();
		double doubleValue = (double) doubleHandle.invoke();
		byte byteValue = (byte) byteHandle.invoke();
		short shortValue =  (short) shortHandle.invoke();
		boolean booleanValue = (boolean) booleanHandle.invoke();
		char charValue = (char) charHandle.invoke();
		Object objectValue = objectHandle.invoke();
		
		// validate we got the expected values
		AssertJUnit.assertTrue(	"Method handle int field lookup failure expected[" + BasicStaticGetSetExample.intField +  "] got [" + intValue + "]", intValue == BasicStaticGetSetExample.intField );
		AssertJUnit.assertTrue(	"Method handle long field lookup failure expected[" + BasicStaticGetSetExample.longField +  "] got [" + longValue + "]", longValue == BasicStaticGetSetExample.longField);
		AssertJUnit.assertTrue(	"Method handle float field lookup failure expected[" + BasicStaticGetSetExample.floatField +  "] got [" + floatValue + "]", floatValue == BasicStaticGetSetExample.floatField);
		AssertJUnit.assertTrue(	"Method handle double field lookup failure expected[" + BasicStaticGetSetExample.doubleField +  "] got [" + doubleValue + "]", doubleValue == BasicStaticGetSetExample.doubleField);
		AssertJUnit.assertTrue(	"Method handle byte field lookup failure expected[" + BasicStaticGetSetExample.byteField +  "] got [" + byteValue + "]", byteValue == BasicStaticGetSetExample.byteField);
		AssertJUnit.assertTrue(	"Method handle short field lookup failure expected[" + BasicStaticGetSetExample.shortField +  "] got [" + shortValue + "]", shortValue == BasicStaticGetSetExample.shortField);
		AssertJUnit.assertTrue(	"Method handle boolean field lookup failure expected[" + BasicStaticGetSetExample.booleanField +  "] got [" + booleanValue + "]", booleanValue == BasicStaticGetSetExample.booleanField);
		AssertJUnit.assertTrue(	"Method handle char field lookup failure expected[" + BasicStaticGetSetExample.charField +  "] got [" + charValue + "]", charValue == BasicStaticGetSetExample.charField);
		AssertJUnit.assertTrue(	"Method handle Object field lookup failure expected[" + BasicStaticGetSetExample.objectField +  "] got [" + objectValue + "]", objectValue == BasicStaticGetSetExample.objectField);
	}
	
	/**
	 *  constants used the the test which follows
	 */
	static int INT_WRITE_VALUE = 4321;
	static long LONG_WRITE_VALUE = 4321;
	static float FLOAT_WRITE_VALUE = (float) 4.321;
	static double DOUBLE_WRITE_VALUE = 4.321;
	static byte BYTE_WRITE_VALUE = 21;
	static short SHORT_WRITE_VALUE = 4321;
	static boolean BOOLEAN_WRITE_VALUE = false;
	static char CHAR_WRITE_VALUE = 4321;
	static Object OBJECT_WRITE_VALUE = new Object();
	/**
	 * Basic validate that set with method handles works for all types
	 * validate set of static using method handles works as expected by validating through normal lookup that
	 * expected value was set
 	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_BasicSet() throws Throwable {
		// currently need this or static initializer does not run when we do the lookups
		new BasicStaticGetSetExample();
		
		// get the handles
		MethodHandle intHandle = MethodHandles.lookup().findStaticSetter(BasicStaticGetSetExample.class, "intField",int.class);
		MethodHandle longHandle = MethodHandles.lookup().findStaticSetter(BasicStaticGetSetExample.class, "longField",long.class);
		MethodHandle floatHandle = MethodHandles.lookup().findStaticSetter(BasicStaticGetSetExample.class, "floatField",float.class);
		MethodHandle doubleHandle = MethodHandles.lookup().findStaticSetter(BasicStaticGetSetExample.class, "doubleField",double.class);
		MethodHandle byteHandle = MethodHandles.lookup().findStaticSetter(BasicStaticGetSetExample.class, "byteField",byte.class);
		MethodHandle shortHandle = MethodHandles.lookup().findStaticSetter(BasicStaticGetSetExample.class, "shortField",short.class);
		MethodHandle booleanHandle = MethodHandles.lookup().findStaticSetter(BasicStaticGetSetExample.class, "booleanField",boolean.class);
		MethodHandle charHandle = MethodHandles.lookup().findStaticSetter(BasicStaticGetSetExample.class, "charField",char.class);
		MethodHandle objectHandle = MethodHandles.lookup().findStaticSetter(BasicStaticGetSetExample.class, "objectField",Object.class);
		
		// set the values 
		intHandle.invoke(INT_WRITE_VALUE);
		longHandle.invoke(LONG_WRITE_VALUE);
		floatHandle.invoke(FLOAT_WRITE_VALUE);
		doubleHandle.invoke(DOUBLE_WRITE_VALUE);
		byteHandle.invoke(BYTE_WRITE_VALUE);
		shortHandle.invoke(SHORT_WRITE_VALUE);
		booleanHandle.invoke(BOOLEAN_WRITE_VALUE);
		charHandle.invoke(CHAR_WRITE_VALUE);
		objectHandle.invoke(OBJECT_WRITE_VALUE);
		
		// validate we set the values as expected
		AssertJUnit.assertTrue(	"Method handle int field set failure got[" + BasicStaticGetSetExample.intField +  "] expected [" + INT_WRITE_VALUE + "]", INT_WRITE_VALUE == BasicStaticGetSetExample.intField);
		AssertJUnit.assertTrue(	"Method handle long field set failure got[" + BasicStaticGetSetExample.longField +  "] expected [" + LONG_WRITE_VALUE + "]", LONG_WRITE_VALUE == BasicStaticGetSetExample.longField);
		AssertJUnit.assertTrue(	"Method handle float field set failure got[" + BasicStaticGetSetExample.floatField +  "] expected [" + FLOAT_WRITE_VALUE + "]", FLOAT_WRITE_VALUE == BasicStaticGetSetExample.floatField);
		AssertJUnit.assertTrue(	"Method handle double field set failure got[" + BasicStaticGetSetExample.doubleField +  "] expected [" + DOUBLE_WRITE_VALUE + "]", DOUBLE_WRITE_VALUE == BasicStaticGetSetExample.doubleField);
		AssertJUnit.assertTrue(	"Method handle byte field set failure got[" + BasicStaticGetSetExample.byteField +  "] expected [" + BYTE_WRITE_VALUE + "]", BYTE_WRITE_VALUE == BasicStaticGetSetExample.byteField);
		AssertJUnit.assertTrue(	"Method handle short field set failure got[" + BasicStaticGetSetExample.shortField +  "] expected [" + SHORT_WRITE_VALUE + "]", SHORT_WRITE_VALUE == BasicStaticGetSetExample.shortField);
		AssertJUnit.assertTrue(	"Method handle boolean field set failure got[" + BasicStaticGetSetExample.booleanField +  "] expected [" + BOOLEAN_WRITE_VALUE + "]", BOOLEAN_WRITE_VALUE == BasicStaticGetSetExample.booleanField);
		AssertJUnit.assertTrue(	"Method handle char field setp failure got[" + BasicStaticGetSetExample.charField +  "] expected [" + CHAR_WRITE_VALUE + "]", CHAR_WRITE_VALUE == BasicStaticGetSetExample.charField);
		AssertJUnit.assertTrue(	"Method handle Object field set failure got[" + BasicStaticGetSetExample.objectField +  "] expected [" + OBJECT_WRITE_VALUE + "]", OBJECT_WRITE_VALUE == BasicStaticGetSetExample.objectField);
	}

	
	/*public fields ( same package )*/
	
	/*
	 * CMVC 192645  : Looking up (via findSetter/ findGetter) a public static field in the super 
	 * class using a subclass lookup (same package) fails in Oracle but passes in J9.
	 * 
	 * Test looks up a static field in the super class using a subclass.  This ensures that we
	 * use the right class in the VM dispatch targets when accessing the field.
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_Static_SamePackage_Superclass() throws Throwable {
		Lookup lookup = SamePackageExampleSubclass.getLookup();
		MethodHandle mhSetter = lookup.findStaticSetter(SamePackageExampleSubclass.class, "publicStaticField_doNotDeclareInSubClasses", int.class);
		mhSetter.invokeExact( 100 );
		/* Validate the setter actually set the field, and not random memory */
		AssertJUnit.assertEquals( SamePackageExample.publicStaticField_doNotDeclareInSubClasses, 100 );
		
		MethodHandle mhGetter = lookup.findStaticGetter(SamePackageExampleSubclass.class, "publicStaticField_doNotDeclareInSubClasses", int.class);
		int out = ( int )mhGetter.invokeExact();
		
		AssertJUnit.assertEquals( out, 100 );
	}
	
	
	/**
	 * findSetter, findGetter tests using a Lookup object where the lookup class is LookupAPITests_Find (this class)
	 * and the fields being looked up are public fields belonging to a different class under the same package as the 
	 * lookup class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_SamePackage() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.class, "nonStaticPublicField", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.class, "nonStaticPublicField", int.class );
		SamePackageExample g = new SamePackageExample();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter tests using a super-class Lookup and sub-class public fields where both super and sub
	 * classes belong to the same package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_SamePackage_SuperclassLookup() throws Throwable {
		Lookup superclassLookup = SamePackageExample.getLookup();
		
		/*findSetter, findGetter using subclass fields*/
		MethodHandle mhSetter = superclassLookup.findSetter( SamePackageExampleSubclass.class, "nonStaticPublicField_Child", int.class );
		MethodHandle mhGetter = superclassLookup.findGetter( SamePackageExampleSubclass.class, "nonStaticPublicField_Child", int.class );
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
		
		/*findSetter, findGetter using super-class fields*/
		mhSetter = superclassLookup.findSetter( SamePackageExampleSubclass.class, "nonStaticPublicField", int.class );
		mhGetter = superclassLookup.findGetter( SamePackageExampleSubclass.class, "nonStaticPublicField", int.class );
		g = new SamePackageExampleSubclass();
		mhSetter.invokeExact( g, 5 );
		o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter tests using a sub-class Lookup and super-class public fields where both super and sub
	 * classes belong to the same package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_SamePackage_SubclassLookup() throws Throwable {
		Lookup subclassLookup = SamePackageExampleSubclass.getLookup();
		MethodHandle mhSetter = subclassLookup.findSetter( SamePackageExample.class, "nonStaticPublicField", int.class );
		MethodHandle mhGetter = subclassLookup.findGetter( SamePackageExample.class, "nonStaticPublicField", int.class );
		SamePackageExample g = new SamePackageExample();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );		
	}
	
	/**
	 * findSetter, findGetter test using fields of an inner classes (level 1 deep) where the 
	 * lookup class is the top outer class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_SamePackage_InnerClass_Level1() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticPublicField_Inner1", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticPublicField_Inner1", int.class );
		SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter test using fields of an inner classes (level 2 deep) where the 
	 * lookup class is the top outer class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_SamePackage_InnerClass_Level2() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPublicField_Inner2", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPublicField_Inner2", int.class );
		SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter test using public fields of a second level inner class
	 * where the lookup class is the first level inner class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_SamePackage_InnerClasse_Nested_Level12() throws Throwable {
		SamePackageExample spe = new SamePackageExample();
		SamePackageInnerClass spe_inner1 = spe.new SamePackageInnerClass();
		
		Lookup publicLookup = spe_inner1.getLookup();
		
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPublicField_Inner2", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPublicField_Inner2", int.class );
		SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter test using fields of a first level inner class
	 * where the lookup class is another first level inner class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_SamePackage_InnerClasses_Parallel_Level1() throws Throwable {
		SamePackageExample spe = new SamePackageExample();
		SamePackageInnerClass spe_inner1 = spe.new SamePackageInnerClass();
		
		Lookup publicLookup = spe_inner1.getLookup();
		
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.SamePackageInnerClass2.class, "nonStaticPublicField_Inner12", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.SamePackageInnerClass2.class, "nonStaticPublicField_Inner12", int.class );
		SamePackageExample.SamePackageInnerClass2 g = (new SamePackageExample()).new SamePackageInnerClass2();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter test using fields of a second level inner class
	 * where the lookup class is another second level inner class of the same outer class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_SamePackage_InnerClasses_Parallel_Level2() throws Throwable {
		SamePackageExample spe = new SamePackageExample();
		SamePackageInnerClass spe_inner1 = spe.new SamePackageInnerClass();
		SamePackageInnerClass_Nested_Level2 spe_inner2 = spe_inner1.new SamePackageInnerClass_Nested_Level2();
		Lookup publicLookup = spe_inner2.getLookup();
		
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.SamePackageInnerClass2.SamePackageInnerClass2_Nested_Level2.class, "nonStaticPublicField_Inner22", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.SamePackageInnerClass2.SamePackageInnerClass2_Nested_Level2.class, "nonStaticPublicField_Inner22", int.class );
		SamePackageExample.SamePackageInnerClass2.SamePackageInnerClass2_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass2()).new SamePackageInnerClass2_Nested_Level2();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/*private fields ( same package )*/ 
	
	/**
	 * Negative test :  findSetter, findGetter tests using a Lookup object where the lookup class is LookupAPITests_Find(this class)
	 * and the fields being looked up are private fields belonging to a different class under the same package as the lookup class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_SamePackage() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		boolean illegalAccessExceptionThrown = false;
		
		try { 
			publicLookup.findSetter( SamePackageExample.class, "nonStaticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findGetter( SamePackageExample.class, "nonStaticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}

	/* CMVC 196603 */
	@Test(groups = { "level.extended" })
	public void test_Getter_Public_Final_SamePackage() throws Throwable {
		MethodHandle mhGetter = MethodHandles.lookup().findGetter(SamePackageExample.class, "nonStaticFinalPublicField", int.class);
		SamePackageExample g = new SamePackageExample();
		int o = (int)mhGetter.invokeExact(g);
		AssertJUnit.assertEquals(g.nonStaticFinalPublicField, o);
	}
	
	/* CMVC 196603 */
	@Test(groups = { "level.extended" })
	public void test_Setter_Public_Final_SamePackage() throws Throwable {
		try {
			MethodHandles.lookup().findSetter(SamePackageExample.class, "nonStaticFinalPublicField", int.class);
			Assert.fail("No exception thrown finding a setter on a final field");
		} catch(IllegalAccessException e) {
			// Exception expected, test passed
		}
	}
	
	/* CMVC 196603 */
	@Test(groups = { "level.extended" })
	public void test_StaticGetter_Public_Static_Final_SamePackage() throws Throwable {
		/* Makes sure the class is loaded */
		int o = SamePackageExample.staticFinalPublicField;
		
		MethodHandle mhGetter = MethodHandles.lookup().findStaticGetter(SamePackageExample.class, "staticFinalPublicField", int.class);
		
		o = (int)mhGetter.invokeExact();
		AssertJUnit.assertEquals(SamePackageExample.staticFinalPublicField, o);
	}
	
	/* CMVC 196603 */
	@Test(groups = { "level.extended" })
	public void test_StaticSetter_Public_Static_Final_SamePackage() throws Throwable {
		try {
			MethodHandles.lookup().findStaticSetter(SamePackageExample.class, "staticFinalPublicField", int.class);
			Assert.fail("No exception thrown finding a setter on a final field");
		} catch(IllegalAccessException e) {
			// Exception expected, test passed
		}
	}
	
	/**
	 * Negative test : findSetter, findGetter tests using a super-class Lookup and sub-class private fields where both super and sub
	 * classes belong to the same package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_SamePackage_SuperclassLookup() throws Throwable {
		Lookup superclassLookup = SamePackageExample.getLookup();
		boolean illegalAccessExceptionThrown = false;
		
		try { 
			superclassLookup.findSetter( SamePackageExampleSubclass.class, "nonStaticPrivateField_Child", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		
		try {
			superclassLookup.findGetter( SamePackageExampleSubclass.class, "nonStaticPrivateField_Child", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findSetter, findGetter tests using a sub-class Lookup and super-class private fields where both super and sub
	 * classes belong to the same package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_SamePackage_SubclassLookup() throws Throwable {
		Lookup subclassLookup = SamePackageExampleSubclass.getLookup();
		boolean illegalAccessExceptionThrown = false;
		
		try { 
			subclassLookup.findSetter( SamePackageExample.class, "nonStaticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		/*findSetter access to private field of parent should be given to child*/
		AssertJUnit.assertFalse( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		
		try {
			subclassLookup.findGetter( SamePackageExample.class, "nonStaticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		/*findSetter access to private field of parent should be given to child*/
		AssertJUnit.assertFalse( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findSetter, findGetter test using private fields of an inner classes (level 1 deep)
	 * where the lookup class is the top level outer class
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_SamePackage_InnerClass_Level1() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		boolean illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticPrivateField_Inner1", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue ( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticPrivateField_Inner1", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}

		AssertJUnit.assertTrue ( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findSetter, findGetter test using private fields of an inner classes (level 2 deep) where the 
	 * lookup class is the top outer class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_SamePackage_InnerClass_Level2() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		boolean illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPrivateField_Inner2", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPrivateField_Inner2", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/*public static fields ( same package )*/
	
	/**
	 * findStaticGetter, findStaticSetter test using a Lookup whose lookup class is LookupAPITests_find (this class)
	 * and lookup fields are public static fields belonging to a different class that is under the same package as 
	 * the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_StaticGetterSetter_Public_Static_SamePackage() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		
		MethodHandle mhSetter = publicLookup.findStaticSetter( SamePackageExample.class, "staticPublicField", int.class );
		MethodHandle mhGetter = publicLookup.findStaticGetter( SamePackageExample.class, "staticPublicField", int.class );
		
		mhSetter.invokeExact( 5 );
		int o = ( int )mhGetter.invokeExact();
		AssertJUnit.assertEquals( o, 5 );		
	}
	
	/**
	 * findStaticGetter, findStaticSetter test using a Lookup whose lookup class is a super-class 
	 * and lookup fields are public static fields belonging to a subclass of the super-class under the same package as 
	 * the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_StaticGetterSetter_Public_Static_SamePackage_SuperclassLookup() throws Throwable {
		Lookup superclassLookup = SamePackageExample.getLookup();
	
		/*
		 * CMVC: 190581 
		 * 
		 * Works in J9 but fails in Oracle.
		 * Error summary: The following line of code throws java.lang.InternalError: uncaught exception in Oracle, "Caused by: java.lang.NoSuchFieldException: staticPublicField"
		 * Note: The 'staticPublicField' belongs to the parent of SamePackageExampleSubclass.   
		 **/ 
		MethodHandle mhSetter = superclassLookup.findStaticSetter( SamePackageExampleSubclass.class, "staticPublicField", int.class );
		MethodHandle mhGetter = superclassLookup.findStaticGetter( SamePackageExampleSubclass.class, "staticPublicField", int.class );
		mhSetter.invokeExact( 5 );
		int o = ( int )mhGetter.invokeExact();
		AssertJUnit.assertEquals( o, 5 );		
	}
	
	/**
	 * Negative test : findSetter, findGetter test using a Lookup whose lookup class is LookupAPITests_find (this class)
	 * and lookup fields are public static fields belonging to a different class that is under the same package as 
	 * the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_Static_SamePackage() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		boolean illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findSetter( SamePackageExample.class, "staticPublicField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		//make sure we get access exception when trying to access a static field with findGetter/findSetter
		if( illegalAccessExceptionThrown == false ) {
			Assert.fail( "Illegal access to static public field given to findSetter/findGetter" );
		}

		AssertJUnit.assertTrue( illegalAccessExceptionThrown ) ;
	}
	
	/**
	 * Negative test : findSetter, findGetter test using a Lookup whose lookup class is a super-class 
	 * and lookup fields are public static fields belonging to a subclass of the lookup class under the same package as 
	 * the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_Static_SamePackage_SuperclassLookup() throws Throwable {
		Lookup superclassLookup = SamePackageExample.getLookup();
		boolean illegalAccessExceptionThrown = false;
		
		try {
			superclassLookup.findSetter( SamePackageExampleSubclass.class, "staticPublicField_Child", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		//make sure we get access exception when trying to access a static field with findGetter/findSetter
		if( illegalAccessExceptionThrown == false ) {
			Assert.fail( "Illegal access to static public field given to findSetter/findGetter" );
		}

		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findSetter, findGetter test using a Lookup whose lookup class is a sub-class 
	 * and lookup fields are public static fields belonging to the super-class of the lookup class under the same package as 
	 * the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_Static_SamePackage_SubclassLookup() throws Throwable {
		Lookup subclassLookup = SamePackageExampleSubclass.getLookup();
		boolean illegalAccessExceptionThrown = false;
		
		try {
			subclassLookup.findSetter( SamePackageExample.class, "staticPublicField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		//make sure we get access exception when trying to access a static field with findGetter/findSetter
		if( illegalAccessExceptionThrown == false ) {
			Assert.fail( "Illegal access to static public field given to findSetter/findGetter" );
		}

		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}

	/*private static fields ( same package )*/ 
	
	/**
	 * Negative test : findGetter, findSetter test using a Lookup whose lookup class is LookupAPITests_find(this class)
	 * and lookup fields are private static fields belonging to a different class under the same package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_Static_SamePackage() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		boolean illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findSetter( SamePackageExample.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findGetter( SamePackageExample.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		//make sure we get access exception when trying to access static field with findGetter/findSetter
		if( illegalAccessExceptionThrown = false ) {
			Assert.fail( "Illegal access to static private field given to findSetter/findGetter" );
		}

		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findStaticSetter( SamePackageExample.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findStaticGetter( SamePackageExample.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findGetter, findSetter test using a Lookup whose lookup class is a super-class and 
	 * and lookup fields are private static fields belonging to a subclass of the lookup class under the same 
	 * package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_Static_SamePackage_SuperClassLookup() throws Throwable {
		Lookup superclassLookup = SamePackageExample.getLookup();
		
		boolean illegalAccessExceptionThrown = false;
		
		try {
			superclassLookup.findSetter( SamePackageExampleSubclass.class, "staticPrivateField_Child", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			superclassLookup.findGetter( SamePackageExampleSubclass.class, "staticPrivateField_Child", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		//make sure we get access exception when trying to access static field with findGetter/findSetter
		if( illegalAccessExceptionThrown = false ) {
			Assert.fail( "Illegal access to static private field given to findSetter/findGetter" );
		}

		illegalAccessExceptionThrown = false;
		
		try {
			superclassLookup.findStaticSetter( SamePackageExampleSubclass.class, "staticPrivateField_Child", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			superclassLookup.findStaticGetter( SamePackageExampleSubclass.class, "staticPrivateField_Child", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findGetter, findSetter test using a Lookup whose lookup class is a sub-class and 
	 * and lookup fields are private static fields belonging to the superclass of the lookup class under the same 
	 * package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_Static_SamePackage_SubClassLookup() throws Throwable {
		Lookup sublassLookup = SamePackageExampleSubclass.getLookup();
		
		boolean illegalAccessExceptionThrown = false;
		
		try {
			sublassLookup.findSetter( SamePackageExample.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			sublassLookup.findGetter( SamePackageExample.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		//make sure we get access exception when trying to access static field with findGetter/findSetter
		if( illegalAccessExceptionThrown = false ) {
			Assert.fail( "Illegal access to static private field given to findSetter/findGetter" );
		}

		illegalAccessExceptionThrown = false;
		
		try {
			sublassLookup.findStaticSetter( SamePackageExample.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		/*static setter access to private static fields of the parent 
		should be given to the child*/
		AssertJUnit.assertFalse( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		
		try {
			sublassLookup.findStaticGetter( SamePackageExample.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		/*static getter access to private static fields of the parent 
		should be given to the child*/
		AssertJUnit.assertFalse( illegalAccessExceptionThrown );
	}
	
	/*protected fields ( same package )*/ 
	
	/**
	 * findGetter findSetter tests using protected fields belonging to a different class under the same package
	 * as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_SamePackage() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExampleSubclass.class, "nonStaticProtectedField", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExampleSubclass.class, "nonStaticProtectedField", int.class );
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invoke( g );  
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findGetter findSetter tests using protected fields belonging to a sub-class of the lookup class under the same package
	 * as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_SamePackage_SuperclassLookup() throws Throwable {
		Lookup superclassLookup = SamePackageExample.getLookup();
		MethodHandle mhSetter = superclassLookup.findSetter( SamePackageExampleSubclass.class, "nonStaticProtectedField", int.class );
		MethodHandle mhGetter = superclassLookup.findGetter( SamePackageExampleSubclass.class, "nonStaticProtectedField", int.class );
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invoke( g );  
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findGetter findSetter tests using protected fields belonging to the super-class of the lookup class under the same package
	 * as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_SamePackage_SubclassLookup() throws Throwable {
		Lookup subclassLookup = SamePackageExampleSubclass.getLookup();
		MethodHandle mhSetter = subclassLookup.findSetter( SamePackageExample.class, "nonStaticProtectedField", int.class );
		MethodHandle mhGetter = subclassLookup.findGetter( SamePackageExample.class, "nonStaticProtectedField", int.class );
		SamePackageExample g = new SamePackageExample();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invoke( g );  
		AssertJUnit.assertEquals( o, 5 );
		
		/*superclass protected fields accessed via subclass*/
		mhSetter = subclassLookup.findSetter( SamePackageExampleSubclass.class, "nonStaticProtectedField", int.class );
		mhGetter = subclassLookup.findGetter( SamePackageExampleSubclass.class, "nonStaticProtectedField", int.class );
		SamePackageExampleSubclass g2 = new SamePackageExampleSubclass();
		mhSetter.invokeExact( g2, 5 );
		o = ( int )mhGetter.invoke( g2 );  
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter test using protected fields of an inner classes (level 1 deep)
	 * where the lookup class is the top level outer class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_SamePackage_InnerClass_Level1() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticProtectedField_Inner1", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticProtectedField_Inner1", int.class );
		SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter test using protected fields of an inner classes (level 2 deep)
	 * where the lookup class is the top level outer class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_SamePackage_InnerClass_Level2() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticProtectedField_Inner2", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticProtectedField_Inner2", int.class );
		SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	
	/******************************************************************************** 
	 * findSetter, findGetter, findStaticSetter, findStaticGetter tests. 
	 * COMBINATIONS: Cross package - public, private, public static, private static, 
	 * protected, super-class, sub-class fields
	 * ******************************************************************************/

	/*public fields ( cross package )*/ 
	
	/**
	 * findSetter, findGetter tests using public fields belonging to a different class under a 
	 * different package than the lookup class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_CrossPackage() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		MethodHandle mhSetter = publicLookup.findSetter( PackageExamples.class, "nonStaticPublicField", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( PackageExamples.class, "nonStaticPublicField", int.class );
		PackageExamples g = new PackageExamples();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invoke( g );  
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter tests using a super-class as lookup class and public fields 
	 * belonging to a sub-class under a different package than the lookup ( super ) class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_CrossPackage_SuperclassLookup() throws Throwable {
		Lookup superclassLookup = PackageExamples.getLookup();
		MethodHandle mhSetter = superclassLookup.findSetter( CrossPackageExampleSubclass.class, "nonStaticPublicField_Child", int.class );
		MethodHandle mhGetter = superclassLookup.findGetter( CrossPackageExampleSubclass.class, "nonStaticPublicField_Child", int.class );
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invoke( g );  
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter tests using a sub-class as lookup class and public fields 
	 * belonging to the super-class under a different package than the lookup ( sub ) class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_CrossPackage_SubclassLookup() throws Throwable {
		Lookup subclassLookup = CrossPackageExampleSubclass.getLookup();
		MethodHandle mhSetter = subclassLookup.findSetter( PackageExamples.class, "nonStaticPublicField", int.class );
		MethodHandle mhGetter = subclassLookup.findGetter( PackageExamples.class, "nonStaticPublicField", int.class );
		PackageExamples g = new PackageExamples();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invoke( g );  
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter test using fields of an inner classes (level 1 deep) where the 
	 * lookup class is the top outer class belonging to a different package .
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_CrossPackage_InnerClass_Level1() throws Throwable {
		Lookup publicLookup = PackageExamples.getLookup();
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticPublicField_Inner1", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticPublicField_Inner1", int.class );
		SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findSetter, findGetter test using fields of an inner classes (level 2 deep) where the 
	 * lookup class is the top outer class belonging to a different package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_CrossPackage_InnerClass_Level2() throws Throwable {
		Lookup publicLookup = PackageExamples.getLookup();
		MethodHandle mhSetter = publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPublicField_Inner2", int.class );
		MethodHandle mhGetter = publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPublicField_Inner2", int.class );
		SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2();
		mhSetter.invokeExact( g, 5 );
		int o = ( int )mhGetter.invokeExact( g );
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/*private fields ( cross package )*/
	
	/**
	 * Negative test : findSetter, findGetter tests private fields belonging to a different class under a different package 
	 * than the lookup class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_CrossPackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		Lookup publicLookup = MethodHandles.lookup();
		
		try {
			publicLookup.findSetter( PackageExamples.class, "nonStaticPrivateField", int.class );
		}
		catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findGetter( PackageExamples.class, "nonStaticPrivateField", int.class );
		}
		catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findSetter, findGetter tests using a superclass as lookup class and private fields 
	 * belonging to a sub-class under a different package than the lookup ( super ) class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_CrossPackage_SuperclassLookup() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		Lookup superclassLookup = PackageExamples.getLookup();
		
		try {
			superclassLookup.findSetter( CrossPackageExampleSubclass.class, "nonStaticPrivateField_Child", int.class );
		}
		catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			superclassLookup.findGetter( CrossPackageExampleSubclass.class, "nonStaticPrivateField_Child", int.class );
		}
		catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}

	/**
	 * Negative test : findSetter, findGetter tests using a subclass as lookup class and private fields 
	 * belonging to a superclass under a different package than the lookup ( sub ) class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_CrossPackage_SubclassLookup() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		Lookup subclassLookup = CrossPackageExampleSubclass.getLookup();
		
		try {
			subclassLookup.findSetter( PackageExamples.class, "nonStaticPrivateField", int.class );
		}
		catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			subclassLookup.findGetter( PackageExamples.class, "nonStaticPrivateField", int.class );
		}
		catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findSetter, findGetter test using private fields of an inner classes (level 1 deep)
	 * where the lookup class is the top level outer class belonging to a different package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_CrossPackage_InnerClass_Level1() throws Throwable {
		Lookup publicLookup = PackageExamples.getLookup();
		boolean illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticPrivateField_Inner1", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue ( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticPrivateField_Inner1", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}

		AssertJUnit.assertTrue ( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findSetter, findGetter test using private fields of an inner classes (level 2 deep) where the 
	 * lookup class is the top outer class belonging to a different package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_CrossPackage_InnerClass_Level2() throws Throwable {
		Lookup publicLookup = PackageExamples.getLookup();
		boolean illegalAccessExceptionThrown = false;
		try {
			publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPrivateField_Inner2", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticPrivateField_Inner2", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	
	/*public static fields ( cross package )*/
	
	/**
	 * findStaticGetter, findStaticSetter tests using public static fields of a class belonging to a different package 
	 * than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_Static_CrossPackage() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		MethodHandle mhSetter = publicLookup.findStaticSetter( PackageExamples.class, "staticPublicField", int.class );
		MethodHandle mhGetter = publicLookup.findStaticGetter( PackageExamples.class, "staticPublicField", int.class );
		mhSetter.invokeExact( 5 );
		int o = ( int ) mhGetter.invokeExact();  
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findStaticGetter, findStaticSetter tests using public static fields of a sub-class belonging to a different package 
	 * than the lookup class ( the super-class of the class containing the fields being looked up ).
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_Static_CrossPackage_SuperclassLookup() throws Throwable {
		Lookup superclassLookup = PackageExamples.getLookup();
		MethodHandle mhSetter = superclassLookup.findStaticSetter( CrossPackageExampleSubclass.class, "staticPublicField_Child", int.class );
		MethodHandle mhGetter = superclassLookup.findStaticGetter( CrossPackageExampleSubclass.class, "staticPublicField_Child", int.class );
		mhSetter.invokeExact( 5 );
		int o = ( int ) mhGetter.invokeExact();  
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/**
	 * findStaticGetter, findStaticSetter tests using public static fields of a super-class belonging to a different package 
	 * than the lookup class ( the sub-class of the class containing the fields being looked up ).
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Public_Static_CrossPackage_SubclassLookup() throws Throwable {
		Lookup subclassLookup = CrossPackageExampleSubclass.getLookup();
		MethodHandle mhSetter = subclassLookup.findStaticSetter( PackageExamples.class, "staticPublicField", int.class );
		MethodHandle mhGetter = subclassLookup.findStaticGetter( PackageExamples.class, "staticPublicField", int.class );
		mhSetter.invokeExact( 5 );
		int o = ( int ) mhGetter.invokeExact();  
		AssertJUnit.assertEquals( o, 5 );
	}
	
	/*private static fields ( cross package )*/
	
	/**
	 * Negative test : findStaticGetter, findStaticSetter tests using private static fields of a class belonging 
	 * to a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_Static_CrossPackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		Lookup publicLookup = MethodHandles.lookup();
		
		try {
			publicLookup.findStaticSetter( PackageExamples.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findStaticGetter( PackageExamples.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findStaticGetter, findStaticSetter tests using private static fields of a sub-class belonging 
	 * to a different package than the lookup class ( which is the super-class of the class containing the fields being looked up.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_Static_CrossPackage_SuperclassLookup() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		Lookup superclassLookup = PackageExamples.getLookup();
		
		try {
			superclassLookup.findStaticSetter( CrossPackageExampleSubclass.class, "staticPrivateField_Child", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			superclassLookup.findStaticGetter( CrossPackageExampleSubclass.class, "staticPrivateField_Child", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findStaticGetter, findStaticSetter tests using private static fields of a super-class belonging 
	 * to a different package than the lookup class ( which is the sub-class of the class containing the fields being looked up.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Private_Static_CrossPackage_SubclassLookup() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		Lookup subclassLookup = CrossPackageExampleSubclass.getLookup();
		
		try {
			subclassLookup.findStaticSetter( PackageExamples.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		illegalAccessExceptionThrown = false;
		
		try {
			subclassLookup.findStaticGetter( PackageExamples.class, "staticPrivateField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/*protected fields ( cross package )*/
	
	/**
	 * findSetter, findGetter tests involving protected fields of a class belonging to a different package than the Lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_CrossPackage() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		boolean illegalAccessExceptionThrown = false;
		try {
			publicLookup.findSetter( PackageExamples.class, "nonStaticProtectedField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findGetter( PackageExamples.class, "nonStaticProtectedField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findSetter, findGetter tests involving protected fields of a sub-class belonging to a different package than the Lookup ( super ) class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_CrossPackage_SuperclassLookup() throws Throwable {
		Lookup superclassLookup = PackageExamples.getLookup();
		try {
			superclassLookup.findSetter( CrossPackageExampleSubclass.class, "nonStaticProtectedField_Child", int.class );
			Assert.fail("Superclass lookup Shouldn't be able to findSetter a protected field of subclass"); 
		} catch( IllegalAccessException e ) {
			// Pass
		}
		
		try {
			superclassLookup.findGetter( CrossPackageExampleSubclass.class, "nonStaticProtectedField_Child", int.class );
			Assert.fail("Superclass lookup Shouldn't be able to findGetter a protected field of subclass");
		} catch( IllegalAccessException e ) {
			// Pass
		}
		
		/* Using superclass protected fields */
		superclassLookup.findSetter(CrossPackageExampleSubclass.class, "nonStaticProtectedField", int.class);
		superclassLookup.findGetter(CrossPackageExampleSubclass.class, "nonStaticProtectedField", int.class);
	}
	
	/**
	 * Negative test : findSetter, findGetter tests involving protected fields of a super-class belonging to a different package than the Lookup ( sub ) class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_CrossPackage_SubclassLookup() throws Throwable {
		Lookup subclassLookup = CrossPackageExampleSubclass.getLookup();
		boolean illegalAccessExceptionThrown = false;
		try {
			subclassLookup.findSetter( PackageExamples.class, "nonStaticProtectedField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		/*Subclass has access to protected field of super-class even if it belongs to a different package*/
		AssertJUnit.assertFalse( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		
		try {
			subclassLookup.findGetter( PackageExamples.class, "nonStaticProtectedField", int.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		/*Subclass has access to protected field of super-class even if it belongs to a different package*/
		AssertJUnit.assertFalse( illegalAccessExceptionThrown );
	}

	/**
	 * Negative test : findSetter, findGetter test using protected fields of an inner classes (level 1 deep)
	 * where the lookup class is the top level outer class belonging to a different package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_CrossPackage_InnerClass_Level1() throws Throwable {
		Lookup publicLookup = PackageExamples.getLookup();
		boolean illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticProtectedField_Inner1", int.class );
		} catch (IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		try {
			publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.class, "nonStaticProtectedField_Inner1", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative test : findSetter, findGetter test using protected fields of an inner classes (level 2 deep)
	 * where the lookup class is the top level outer class belonging to a different package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GetterSetter_Protected_CrossPackage_InnerClass_Level2() throws Throwable {
		Lookup publicLookup = PackageExamples.getLookup();
		boolean illegalAccessExceptionThrown = false;
		
		try {
			publicLookup.findSetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticProtectedField_Inner2", int.class );
		} catch (IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
		
		illegalAccessExceptionThrown = false;
		try {
			publicLookup.findGetter( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "nonStaticProtectedField_Inner2", int.class );
		} catch ( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	
	/********************************************************************
	 * findVirtual tests 
	 * COMBINATIONS: Same package and cross package, public [overridden], 
	 * protected [overridden], private, super-class, sub-class methods
	 * ******************************************************************/
	
	/*public methods ( same package )*/
	
	/**
	 * findVirtual test using a public method of a class belonging to the same package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_SamePackage() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findVirtual( SamePackageExample.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample g = new SamePackageExample();
		int s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}
	
	/**
	 * findVirtual test using public method of a sub-class of the lookup class belonging to the same package as the lookup class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_SamePackage_SuperclassLookup() throws Throwable {
		/*Call the overridden addPublic of child */
		MethodHandle example = SamePackageExample.getLookup().findVirtual( SamePackageExampleSubclass.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();
		int s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( s, 4 );
		
		/*Call the addPublicSuper implemented by parent*/
		example = SamePackageExample.getLookup().findVirtual( SamePackageExampleSubclass.class, "addPublic_Super", MethodType.methodType( int.class, int.class, int.class ) );
		g = new SamePackageExampleSubclass();
		s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 8, s );
		
		/*Call the addPublic_Child implemented by parent*/
		example = SamePackageExample.getLookup().findVirtual( SamePackageExampleSubclass.class, "addPublic_Child", MethodType.methodType( int.class, int.class, int.class ) );
		g = new SamePackageExampleSubclass();
		s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 5, s );
	}
	
	/**
	 * findVirtual test using public method of the super-class of the lookup class belonging to the same package as the lookup class. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_SamePackage_SubclassLookup() throws Throwable {
		MethodHandle example = SamePackageExampleSubclass.getLookup().findVirtual( SamePackageExample.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample g = new SamePackageExample();
		
		/*Should execute the superclass version of addPublic*/
		int s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}
	
	/**
	 * findVirtual test using a public method of an inner class belonging to the same package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_SamePackage_InnerClass() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findVirtual( SamePackageExample.SamePackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
		int s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}
	

	/**
	 * findVirtual test using public method of an inner classes (level 2 deep) where the 
	 * lookup class is a different class under the same package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_SamePackage_InnerClass_Level2() throws Throwable {
		Lookup publicLookup = MethodHandles.lookup();
		MethodHandle example = publicLookup.findVirtual( SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2.class, "addPublicInner_Level2", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2();
		int o = (int)example.invokeExact( g, 5, 0 );
		AssertJUnit.assertEquals( 5, o );
	}
	
	/**
	 * findVirtual test using a public overridden method 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_SamePackage_Overridden() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findVirtual( SamePackageExampleSubclass.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample g = new SamePackageExampleSubclass();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 4, s );
	}
	
	/**
	 * findVirtual test using a public overridden method belonging to a subclass of the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_SamePackage_Overridden_SuperclassLookup() throws Throwable {
		MethodHandle example = SamePackageExample.getLookup().findVirtual( SamePackageExampleSubclass.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 4, s );
	}
	
	/**
	 * findVirtual test using a public overridden method belonging to the super-class that the lookup class extends.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_SamePackage_Overridden_SubclassLookup() throws Throwable {
		MethodHandle example = SamePackageExampleSubclass.getLookup().findVirtual( SamePackageExample.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample g = new SamePackageExample();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}
	
	/**
	 * findVirtual test using a public overridden method belonging to a second level inner class 
	 * which is a child of a first level inner class (the lookup class) under the same outer class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_SamePackage_Overridden_InnerClass_Nested_Level2() throws Throwable {
		SamePackageExample spe = new SamePackageExample();
		SamePackageInnerClass spe_inner1 = spe.new SamePackageInnerClass();
		Lookup publicLookup = spe_inner1.getLookup();
		
		MethodHandle example = publicLookup.findVirtual(SamePackageExample.SamePackageInnerClass2.SamePackageInnerClass2_Nested_Level2_SubOf_Inner1.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
	
		SamePackageExample.SamePackageInnerClass g = ((new SamePackageExample()).new SamePackageInnerClass2()).new SamePackageInnerClass2_Nested_Level2_SubOf_Inner1();
		
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 23, s );
	}
	
	/*Protected methods ( same package )*/
	
	/**
	 * findVirtual test using a protected overridden method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Protected_SamePackage_Overridden() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findVirtual( SamePackageExampleSubclass.class, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample g = new SamePackageExampleSubclass();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 13, s );
	}
	
	/**
	 * findVirtual test using a protected overridden method that belongs to a subclass of the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Protected_SamePackage_Overridden_SuperclassLookup() throws Throwable {
		MethodHandle example = SamePackageExample.getLookup().findVirtual( SamePackageExampleSubclass.class, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 13, s );
	}
	
	/**
	 * findVirtual test using a protected overridden method that belongs to the super-class which the lookup class extends.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Protected_SamePackage_Overridden_SubclassLookup() throws Throwable {
		MethodHandle example = SamePackageExampleSubclass.getLookup().findVirtual( SamePackageExample.class, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample g = new SamePackageExample();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}
	
	/*private methods ( same package )*/
	
	/**
	 * Negative test involving a private method being looked up 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Private_SamePackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		
		try {
			MethodHandle example = MethodHandles.lookup().findVirtual( SamePackageExample.class, "addPrivate", MethodType.methodType( int.class, int.class, int.class ) );
			SamePackageExample g = new SamePackageExample();
			example.invoke( g, 1, 2 );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}

	/*public methods ( cross package )*/
	
	/**
	 * findVirtual test with a public method in a class that belongs to a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_CrossPackage() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findVirtual( PackageExamples.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		PackageExamples g = new PackageExamples();
		int s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}
	
	/**
	 * findVirtual test using a public method of an inner class belonging to a different package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_CrossPackage_InnerClass() throws Throwable {
		MethodHandle example = PackageExamples.getLookup().findVirtual( SamePackageExample.SamePackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
		int s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, s ) ;
	}
	
	/**
	 * findVirtual test with a public method in a subclass on a different package than the lookup ( super ) class
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_CrossPackage_SuperclassLookup() throws Throwable {
		MethodHandle example = PackageExamples.getLookup().findVirtual( CrossPackageExampleSubclass.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();
		int s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 5, s );
	}
	
	/**
	 * findVirtual test with a public method in a super-class on a different package than the lookup ( sub ) class
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_CrossPackage_SubclassLookup() throws Throwable {
		MethodHandle example = CrossPackageExampleSubclass.getLookup().findVirtual( PackageExamples.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		PackageExamples g = new PackageExamples();
		int s = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, s ) ;
	}
	
	/**
	 * findVirtual test with a public overridden method in a class on a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_CrossPackage_Overridden() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findVirtual( CrossPackageExampleSubclass.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 5, s );
	}
	
	/**
	 * findVirtual test with a public overridden method in a sub-class of the lookup class that is on a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_CrossPackage_Overridden_SuperclassLookup() throws Throwable {
		MethodHandle example = PackageExamples.getLookup().findVirtual( CrossPackageExampleSubclass.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 5, s );
	}
	
	/**
	 * findVirtual test with a public overridden method in a super-class of the lookup class that is on a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_CrossPackage_Overridden_SubclassLookup() throws Throwable {
		MethodHandle example = CrossPackageExampleSubclass.getLookup().findVirtual( PackageExamples.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		PackageExamples g = new CrossPackageExampleSubclass();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 5, s );
	}
	
	/**
	 * findVirtual test using a public overridden method belonging to a second level inner class 
	 * which is a child of a first level inner class under the same outer class. The lookup class 
	 * is a first level inner class in a different package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Public_CrossPackage_Overridden_InnerClass_Nested_Level2() throws Throwable {
		PackageExamples pe = new PackageExamples();
		CrossPackageInnerClass cp_inner1 = pe.new CrossPackageInnerClass();
		Lookup publicLookup = cp_inner1.getLookup();
		
		MethodHandle example = publicLookup.findVirtual(SamePackageExample.SamePackageInnerClass2.SamePackageInnerClass2_Nested_Level2_SubOf_Inner1.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
	
		SamePackageExample.SamePackageInnerClass g = ((new SamePackageExample()).new SamePackageInnerClass2()).new SamePackageInnerClass2_Nested_Level2_SubOf_Inner1();
		
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( 23, s );
	}
	
	/*protected methods ( cross package )*/
	/**
	 * findVirtual test with a protected overridden method belonging to a class that is on a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Protected_CrossPackage_Overridden() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findVirtual( CrossPackageExampleSubclass.class, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );
		PackageExamples g = new CrossPackageExampleSubclass();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( -1, s );
	}
	
	/**
	 * findVirtual test with a protected overridden method belonging to a sub-class of the lookup class that is on a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Protected_CrossPackage_Overridden_SuperclassLookup() throws Throwable {
		MethodHandle example = PackageExamples.getLookup().findVirtual( CrossPackageExampleSubclass.class, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();
		int s = ( int )example.invoke( g, 1, 2 );
		AssertJUnit.assertEquals( -1, s );
	}
	
	/**
	 * findVirtual test with a protected overridden method belonging to a super-class of the lookup class that is on a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Protected_CrossPackage_Overridden_SubclassLookup() throws Throwable {
		MethodHandle example = CrossPackageExampleSubclass.getLookup().findVirtual( PackageExamples.class, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );
		PackageExamples g = new PackageExamples();
		
		try {
			int s = (int)example.invoke(g, 1, 2);
			Assert.fail("Should have thrown ClassCastException");
		} catch(ClassCastException e) {
		}
		CrossPackageExampleSubclass cpes = new CrossPackageExampleSubclass();
		int s = (int)example.invoke(cpes, 1, 2);
		AssertJUnit.assertEquals(cpes.addProtected(1, 2), s);
	}
	
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Protected_Final_CrossPackage_SubclassLookup() throws Throwable {
		MethodHandle handle = CrossPackageExampleSubclass.getLookup().findVirtual(PackageExamples.class, "finalProtectedMethod", MethodType.methodType(String.class));
		String result = (String)handle.invokeExact(new CrossPackageExampleSubclass());
		AssertJUnit.assertEquals(new CrossPackageExampleSubclass().call_finalProtectedMethod(), result);
	}

	/*private methods ( cross package )*/
	
	/**
	 * findVirtual test with a private method belonging to a different class than the lookup class
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Private_CrossPackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		
		try {
			MethodHandle example = MethodHandles.lookup().findVirtual( PackageExamples.class, "addPrivate", MethodType.methodType( int.class, int.class, int.class ) );
			PackageExamples g = new PackageExamples();
			example.invokeExact( g, 1, 2 );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * findVirtual test using a public method of an inner class belonging to the same package as the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_Private_CrossPackage_InnerClass() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		
		try {
			MethodHandle example = PackageExamples.getLookup().findVirtual( SamePackageExample.SamePackageInnerClass.class, "addPrivateInner", MethodType.methodType( int.class, int.class, int.class ) );
			SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
			int s = ( int )example.invokeExact( g, 1, 2 );
			AssertJUnit.assertEquals( 3, s ) ;
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}

		
	/** 
	 * This test ensures that we use the correct ITable and ITable index.  
	 * ITable indexes are only valid against the class they have been looked up in
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_TreeMapTest() throws Throwable {
		TreeMap t = new TreeMap();  
        Object o = "foobar";
        Object o2 = "foobar2";
        
        /*
         * Works in J9 but fails in Oracle 
         * Error summary: "java.lang.invoke.WrongMethodTypeException: (Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object; 
         *  cannot be called as (Ljava/util/NavigableMap;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"
         *  
         *  Note: Oracle has agreed our behavior is correct.
		 *
         * */
        MethodHandle m = MethodHandles.lookup().findVirtual( NavigableMap.class, "put", MethodType.fromMethodDescriptorString( "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", null ) );
         
        Object r = ( Object )m.invokeExact( ( NavigableMap )t, o, o );
        AssertJUnit.assertNull( r );
        
        Object r2 = ( Object )m.invokeExact( ( NavigableMap )t, o, o2 );
        AssertJUnit.assertEquals( o, r2 );
	}
	
	/**************************************************************************************** 
	 * findStatic tests. 
	 * Combinations : same package, cross package; public static and private static methods 
	 * ***************************************************************************************/
	
	/**
	 * findStatic test using public static methods ( same package )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindStatic_Public_SamePackage() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findStatic( SamePackageExample.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );	
		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( 3, s ) ;
	}

	/**
	 * findStatic test using private static methods ( same package ) 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindStatic_Private_SamePackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		
		try {
			MethodHandle example = MethodHandles.lookup().findStatic( SamePackageExample.class, "addPrivateStatic", MethodType.methodType( int.class, int.class, int.class ) );
			example.invokeExact( 1, 2 );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * findStatic test using protected static methods ( same package )
	 * The test is used to verify that the method is inaccessible to the enclosing lookup class only with public mode.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindStatic_Protected_PublicMode_SamePackage() throws Throwable {
		try {
			SamePackageExample.publicLookupObjectSamePackage.findStatic(SamePackageExample.class, "addProtectedStatic", MethodType.methodType(int.class, int.class, int.class));
			Assert.fail("The protected method is inaccessible to the lookup class only with public mode");
		} catch(IllegalAccessException e) {
			/* Success */
		}
	}
	
	/**
	 * findStatic test using public static methods ( cross package )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindStatic_Public_CrossPackage() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findStatic( PackageExamples.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		int s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 3, s ) ;
	}
	
	/**
	 * findStatic test using private static methods ( cross package )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindStatic_Private_CrossPackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		try {
			MethodHandle example = MethodHandles.lookup().findStatic( PackageExamples.class, "addPrivateStatic", MethodType.methodType( int.class, int.class, int.class ) );
			example.invokeExact( 1, 2 );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/********************************************************************************************
	 * findSpecial tests.
	 * Combinations : same package, cross package; public [static] and private [static] methods 
	 * & super-class / sub-class invocations
	 * ******************************************************************************************/
	
	/**
	 * findSpecial test using a public method of a different class than the class the resultant 
	 * method handle will be bound to.
	 * @throws Throwable
	 */	
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Public_SamePackage() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		try {
			MethodHandles.lookup().findSpecial(SamePackageExample.class, "addPublic", mt, LookupAPITests_Find.class);
			Assert.fail("Lookupclass (class com.ibm.j9.jsr292.LookupAPITests_Find) must be the same as or subclass of the current class (class com.ibm.j9.jsr292.SamePackageExample)");
		} catch(IllegalAccessException e){
			// Pass
		}
		try {
			SamePackageExample.getLookup().findSpecial(SamePackageExample.class, "addPublic", mt, LookupAPITests_Find.class);
			Assert.fail("Private access required");
		} catch(IllegalAccessException e){
			// Pass
		}
		MethodHandle example = SamePackageExample.getLookup().findSpecial(SamePackageExample.class, "addPublic", mt, SamePackageExample.class);
		SamePackageExample g = new SamePackageExample();
		int out = (int)example.invokeExact(g, 1, 2);
		AssertJUnit.assertEquals(g.addPublic(1, 2), out);
	}
	
	/**
	 * findSpecial test using a public method of an inner class where the class the resultant 
	 * method handle will be bound to is the outer class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Public_SamePackage_InnerClass() throws Throwable {
		try {
			MethodHandles.lookup().findSpecial(SamePackageExample.SamePackageInnerClass.class, "addPublicInner", MethodType.methodType(int.class, int.class, int.class), LookupAPITests_Find.class);
			Assert.fail("Should be: 'Lookupclass (class com.ibm.j9.jsr292.LookupAPITests_Find) must be the same as or subclass of the current class (class com.ibm.j9.jsr292.SamePackageExample$SamePackageInnerClass)'");
		} catch (IllegalAccessException e) {
			// pass
		}
	}

	/**
	 * findSpecial test using a protected overridden method of the super-class of the the class the resultant 
	 * method handle will be bound to.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_ProtectedOverridden_SamePackage_SubclassLookup() throws Throwable {
		boolean iaeThrown = false;
		try {
			SamePackageExampleSubclass.getLookup().findSpecial( SamePackageExample.class, "addProtected", MethodType.methodType( int.class, int.class, int.class ), SamePackageExampleSubclass.class );
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/**
	 * findSpecial test using a public overridden method belonging to a second level inner class 
	 * which is a child of a first level inner class (the lookup class) under the same outer class.
	 * The resultant method handle is bound to LookupAPITests_Find (this class).
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Public_SamePackage_Overridden_InnerClass_Nested_Level2() throws Throwable {
		SamePackageExample spe = new SamePackageExample();
		SamePackageInnerClass spe_inner1 = spe.new SamePackageInnerClass();
		Lookup publicLookup = spe_inner1.getLookup();
		
		boolean iaeThrown = false;
		try {
			publicLookup.findSpecial(SamePackageExample.SamePackageInnerClass2.SamePackageInnerClass2_Nested_Level2_SubOf_Inner1.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ), LookupAPITests_Find.class );
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Public_SamePackage_UnrelatedClassLoaders() throws Throwable {
		ParentCustomClassLoader unrelatedClassLoader = new ParentCustomClassLoader( LookupAPITests_Find.class.getClassLoader() );
		Object customLoadedClass = unrelatedClassLoader.loadClass( "com.ibm.j9.jsr292.CustomLoadedClass1" ).newInstance();
		
		Lookup lookup = SamePackageExample.getLookup();
		boolean iaeThrown = false;
		try {
			lookup.findSpecial( customLoadedClass.getClass(), "add", MethodType.methodType( int.class, int.class, int.class ), LookupAPITests_Find.class );
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	
	}
	
	/**
	 * Negative test : findSpecial test using a private method of a different class than the class 
	 * the resultant method handle will be bound to. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Private_SamePackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		try {
			MethodHandles.lookup().findSpecial( SamePackageExample.class, "addPrivate", MethodType.methodType( int.class, int.class, int.class ), LookupAPITests_Find.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * findSpecial test using a private method of an inner class where the class the resultant 
	 * method handle will be bound to is the outer class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Private_SamePackage_InnerClass() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		try {
			MethodHandles.lookup().findSpecial( SamePackageExample.SamePackageInnerClass.class, "addPrivateInner", MethodType.methodType( int.class, int.class, int.class ), LookupAPITests_Find.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * findSpecial test using a public method belonging to a class which is in a different package than the 
	 * class to which the resultant method handle will be bound to.
	 * @throws Throwable
	 */
	
	/*
	 * 
	 * CMVC 188705
	 * 
	 * Works in J9 but fails in Oracle.
	 * Error Summary : "java.lang.IllegalAccessException: caller class must be a subclass below the method: examples.PackageExamples.addPublic(int,int)int, 
	 *  from class com.ibm.j9.jsr292.LookupAPITests_Find"
	 *  
	 * */
	@Test(enabled = false, groups = { "level.extended"})
	public void test_FindSpecial_Public_CrossPackage() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findSpecial( PackageExamples.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ), LookupAPITests_Find.class );
		LookupAPITests_Find g = new LookupAPITests_Find();
		int out = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * findSpecial test using a protected overridden method belonging to a super-class which is in a different package than the 
	 * sub-class to which the resultant method handle will be bound to.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Public_CrossPackage_SubclassLookup() throws Throwable {
		MethodHandle example = CrossPackageExampleSubclass.getLookup().findSpecial( PackageExamples.class, "addProtected", MethodType.methodType( int.class, int.class, int.class ), CrossPackageExampleSubclass.class );
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();
		int out = ( int )example.invokeExact( g, 1, 2 );
		AssertJUnit.assertEquals( 3, out );
	}
	
	/*Blocked by CMVC : 190587*/
	/**
	 * findSpecial test using a public method of an inner class where the class the resultant 
	 * method handle will be bound to is in a different package.
	 * @throws Throwable
	 */
	@Test(enabled = false, groups = { "level.extended"})
	public void test_FindSpecial_Public_CrossPackage_InnerClass() throws Throwable {
		MethodHandle example = PackageExamples.getLookup().findSpecial( SamePackageExample.SamePackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ), PackageExamples.class );
		PackageExamples g = new PackageExamples();
		
		/*
		 * CMVC 190587
		 * 
		 * Behavior mismatch between J9 and Oracle. 
		 * J9 : WrongMethodTypeException thrown here
		 * Oracle : CMVC 188705 (output 2) 
		 * 
		 * */
		System.out.println(example);
		int out = ( int )example.invokeExact( g, 1, 2 ); 
		AssertJUnit.assertEquals( 3, out );
	}
	
	/**
	 * findSpecial test using a default method of an interface where the class (the resultant 
	 * method handle will be bound to) is in a different package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Default_CrossPackage_Interface() throws Throwable {
		MethodHandle example = MethodHandles.lookup().findSpecial(CrossPackageDefaultMethodInterface.class, "addDefault", MethodType.methodType(int.class, int.class, int.class), CrossPackageDefaultMethodInterface.class);
	}
	
	/**
	 * Negative findSpecial test using a public method of a class where the class (the resultant 
	 * method handle will be bound to) is in a different package.
	 * Note: when the lookup class is not equal to the caller class, the declaring class
	 * must be an interface which the caller class can be assigned to.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Public_CrossPackage_Not_Interface() throws Throwable {
		try {
			MethodHandles.lookup().findSpecial(PackageExamples.class, "addPublic", MethodType.methodType(int.class, int.class, int.class), PackageExamples.class);
			Assert.fail("No exception thrown finding a public method of a class in a different package");
		} catch(IllegalAccessException e) {
			// Exception expected, test passed
		}
	}
	
	/**
	 * Negative findSpecial test involving a private method 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Private_CrossPackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		try {
			MethodHandles.lookup().findSpecial( PackageExamples.class, "addPrivate", MethodType.methodType( int.class, int.class, int.class ), LookupAPITests_Find.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
	
	/**
	 * Negative findSpecial test involving a private method of an inner class
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_Private_CrossPackage_InnerClass() throws Throwable {
		boolean illegalAccessExceptionThrown = false;
		try {
			PackageExamples.getLookup().findSpecial( SamePackageExample.SamePackageInnerClass.class, "addPrivateInner", MethodType.methodType( int.class, int.class, int.class ), PackageExamples.class );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}
		
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_toStringTest() throws Throwable {
		boolean iaeThrown = false;
		try {
			MethodHandles.lookup().findSpecial( SamePackageExample.class, "toString", MethodType.methodType( String.class ), SamePackageExample.class );
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
	    boolean iaeThrown = false;
		try {
			MethodHandles.publicLookup().findSpecial(LookupAPITests_Find.class, "toString", MethodType.methodType(String.class), LookupAPITests_Find.class);
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
	    boolean iaeThrown = false;
	    try {
			MethodHandles.lookup().findSpecial(SamePackageExample.class, "toString", MethodType.methodType(String.class), SamePackageExample.class);
		} catch (IllegalAccessException e) {
			iaeThrown = true;
		}
		assert(iaeThrown);
	}
	
	/**
	 * Test that &#60;clinit&#62; of the defining class is called when a static <code>DirectHandle</code> is invoked (not when created) 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindStatic_clinit() throws Throwable {
		Find_InvokeTracker.reset();
		MethodHandles.Lookup l = MethodHandles.lookup();
		
		/* 
		 * First test:
		 *  - findStatic should not invoke <clinit>
		 *  - first mh.invoke should invoke <clinit>
		 */
		MethodHandle mh = l.findStatic(Find_StaticMethodHelper.class, "method", MethodType.methodType(void.class));
		Find_InvokeTracker.calledFind = true;
		AssertJUnit.assertTrue(Find_InvokeTracker.stateFound());
		mh.invoke();
		AssertJUnit.assertTrue(Find_InvokeTracker.stateFirstInvoke());
		
		/*
		 * Second test:
		 *  - second mh.invoke should not invoke <clinit>
		 */
		Find_InvokeTracker.reset();
		mh.invoke();
		AssertJUnit.assertTrue(Find_InvokeTracker.stateSecondInvoke());
	}
	
	/**
	 * Test that &#60;clinit&#62; of the defining class is called when a <code>ConstructorHandle</code> is invoked (not when created)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindConstructor_clinit() throws Throwable {
		Find_InvokeTracker.reset();
		MethodHandles.Lookup l = MethodHandles.lookup();
		
		/* 
		 * First test:
		 *  - findConstructor should not invoke <clinit>
		 *  - first mh.invoke should invoke <clinit>
		 */
		MethodHandle mh = l.findConstructor(Find_ConstructorHelper.class, MethodType.methodType(void.class));
		Find_InvokeTracker.calledFind = true;
		AssertJUnit.assertTrue(Find_InvokeTracker.stateFound());
		mh.invoke();
		AssertJUnit.assertTrue(Find_InvokeTracker.stateFirstInvoke());
		
		/*
		 * Second test:
		 *  - second mh.invoke should not invoke <clinit>
		 */
		Find_InvokeTracker.reset();
		mh.invoke();
		AssertJUnit.assertTrue(Find_InvokeTracker.stateSecondInvoke());
	}
	
	/**
	 * Test that &#60;clinit&#62; of the defining class is called when a <code>StaticFieldGetterHandle</code> is invoked (not when created) 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_StaticGetter_clinit() throws Throwable {
		Find_InvokeTracker.reset();
		MethodHandles.Lookup l = MethodHandles.lookup();
		
		/* 
		 * First test:
		 *  - findStaticGetter should not invoke <clinit>
		 *  - first mh.invoke should invoke <clinit>
		 */
		MethodHandle mh = l.findStaticGetter(Find_StaticGetterHelper.class, "staticField", String.class);
		Find_InvokeTracker.calledFind = true;
		AssertJUnit.assertTrue(Find_InvokeTracker.stateFound());
		String s = (String) mh.invoke();
		AssertJUnit.assertTrue(Find_InvokeTracker.stateInitialized());
		Find_InvokeTracker.calledInvoke = s.equals("TEST");
		AssertJUnit.assertTrue(Find_InvokeTracker.stateFirstInvoke());
		
		/*
		 * Second test:
		 *  - second mh.invoke should not invoke <clinit>
		 */
		Find_InvokeTracker.reset();
		String t = (String) mh.invoke();
		Find_InvokeTracker.calledInvoke = t.equals("TEST");
		AssertJUnit.assertTrue(Find_InvokeTracker.stateSecondInvoke());
	}
	
	/**
	 * Test that &#60;clinit&#62; of the defining class is called when a <code>StaticFieldSetterHandle</code> is invoked (not when created) 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_StaticSetter_clinit() throws Throwable {
		Find_InvokeTracker.reset();
		MethodHandles.Lookup l = MethodHandles.lookup();
		
		/* 
		 * First test:
		 *  - findStaticSetter should not invoke <clinit>
		 *  - first mh.invoke should invoke <clinit>
		 */
		MethodHandle mh = l.findStaticSetter(Find_StaticSetterHelper.class, "staticField", String.class);
		Find_InvokeTracker.calledFind = true;
		AssertJUnit.assertTrue(Find_InvokeTracker.stateFound());
		mh.invoke("TEST");
		AssertJUnit.assertTrue(Find_InvokeTracker.stateInitialized());
		Find_InvokeTracker.calledInvoke = Find_StaticSetterHelper.staticField.equals("TEST");
		AssertJUnit.assertTrue(Find_InvokeTracker.stateFirstInvoke());
		
		/*
		 * Second test:
		 *  - second mh.invoke should not invoke <clinit>
		 */
		Find_InvokeTracker.reset();
		mh.invoke("TEST2");
		Find_InvokeTracker.calledInvoke = Find_StaticSetterHelper.staticField.equals("TEST2");
		AssertJUnit.assertTrue(Find_InvokeTracker.stateSecondInvoke());
	}
	
	/**
	 * Negative test: Looking up &lt;init&gt; or &lt;clinit&gt; using Lookup.findSpecial should fail with NoSuchMethodException
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindSpecial_InitCheck() throws Throwable {
		boolean NoSuchMethodExceptionThrown = false;
	    try {
	    	MethodHandles.lookup().findSpecial(String.class, "<init>", MethodType.methodType(String.class), this.getClass());
	    } catch (NoSuchMethodException e) {
	    	NoSuchMethodExceptionThrown = true;
		}
	    AssertJUnit.assertTrue(NoSuchMethodExceptionThrown);
	    
	    NoSuchMethodExceptionThrown = false;
	    try {
	    	MethodHandles.lookup().findSpecial(String.class, "<clinit>", MethodType.methodType(String.class), this.getClass());
	    } catch (NoSuchMethodException e) {
	    	NoSuchMethodExceptionThrown = true;
		}
	    AssertJUnit.assertTrue(NoSuchMethodExceptionThrown);
	}

	/**
	 * Negative test: Looking up &lt;init&gt; or &lt;clinit&gt; using Lookup.findVirtual should fail with NoSuchMethodException
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindVirtual_InitCheck() throws Throwable {
		boolean NoSuchMethodExceptionThrown = false;
	    try {
	    	MethodHandles.lookup().findVirtual(String.class, "<init>", MethodType.methodType(String.class));
	    } catch (NoSuchMethodException e) {
	    	NoSuchMethodExceptionThrown = true;
		}
	    AssertJUnit.assertTrue(NoSuchMethodExceptionThrown);
	    
	    NoSuchMethodExceptionThrown = false;
	    try {
	    	MethodHandles.lookup().findVirtual(String.class, "<clinit>", MethodType.methodType(String.class));
	    } catch (NoSuchMethodException e) {
	    	NoSuchMethodExceptionThrown = true;
		}
	    AssertJUnit.assertTrue(NoSuchMethodExceptionThrown);
	}

	/**
	 * Negative test: Looking up &lt;init&gt; or &lt;clinit&gt; using Lookup.findStatic should fail with NoSuchMethodException
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindStatic_InitCheck() throws Throwable {
		boolean NoSuchMethodExceptionThrown = false;
	    try {
	    	MethodHandles.lookup().findStatic(String.class, "<init>", MethodType.methodType(String.class));
	    } catch (NoSuchMethodException e) {
	    	NoSuchMethodExceptionThrown = true;
		}
	    AssertJUnit.assertTrue(NoSuchMethodExceptionThrown);
	    
	    NoSuchMethodExceptionThrown = false;
	    try {
	    	MethodHandles.lookup().findStatic(String.class, "<clinit>", MethodType.methodType(String.class));
	    } catch (NoSuchMethodException e) {
	    	NoSuchMethodExceptionThrown = true;
		}
	    AssertJUnit.assertTrue(NoSuchMethodExceptionThrown);
	}
	
	/**
	 * Test getVirtual on a concrete method inherited from Object returns a valid method handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_findVirtual_on_interface_concreteMethods() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(TestInterface.class,
				"hashCode", MethodType.methodType(int.class));
		TestInterface receiver = new TestInterface() {
			@Override
			public void test() throws Throwable {
				// empty
			}
		};
		@SuppressWarnings("unused")
		int result = (int)mh.invokeExact(receiver);
	}

	/**
	 * Test getVirtual on a default method returns a valid method handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_findVirtual_on_interface_defaultMethods() throws Throwable {
		MethodHandle mh = 
				MethodHandles.lookup().findVirtual(TestInterfaceWithDefaultMethod.class,
				"test", MethodType.methodType(Integer.class));
		TestInterfaceWithDefaultMethod receiver = new TestInterfaceWithDefaultMethod() {};
		Integer result = (Integer) mh.invokeExact(receiver);
		AssertJUnit.assertEquals("Wrong result from default method",
				TestInterfaceWithDefaultMethod.FORTY_TWO, result);
	}

}

class Find_StaticMethodHelper {
	static {
		Find_InvokeTracker.calledClinit = Find_InvokeTracker.stateFound();
	}
	public static void method() {
		Find_InvokeTracker.calledInvoke = true;
	}
}

class Find_ConstructorHelper {
	static {
		Find_InvokeTracker.calledClinit = Find_InvokeTracker.stateFound();
	}
	public Find_ConstructorHelper() {
		Find_InvokeTracker.calledInvoke = true;
	}
}

class Find_StaticGetterHelper {
	static {
		Find_InvokeTracker.calledClinit = true;
	}
	public static String staticField = "TEST";
}

class Find_StaticSetterHelper {
	static {
		Find_InvokeTracker.calledClinit = true;
	}
	public static String staticField = "";
}

class Find_InvokeTracker {
	public static boolean calledFind = false;
	public static boolean calledClinit = false;
	public static boolean calledInvoke = false;
	
	public static void reset() {
		calledFind = false;
		calledClinit = false;
		calledInvoke = false;
	}
	
	public static boolean stateFound() {
		return calledFind && !calledClinit && !calledInvoke;
	}
	
	public static boolean stateInitialized() {
		return calledFind && calledClinit && !calledInvoke;
	}
	
	public static boolean stateFirstInvoke() {
		return calledFind && calledClinit && calledInvoke;
	}
	
	public static boolean stateSecondInvoke() {
		return !calledFind && !calledClinit && calledInvoke;
	}
}
