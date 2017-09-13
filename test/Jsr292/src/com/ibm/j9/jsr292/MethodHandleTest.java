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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.NavigableMap;
import java.util.TreeMap;

import junit.framework.AssertionFailedError;
import junit.framework.TestListener;
import junit.framework.TestResult;
import examples.PackageExamples;

/**
 * @author mesbah
 * This class contains tests for the MethodType API. 
 */
public class MethodHandleTest{
	
	/****************************
	 * Tests for asCollector
	 * **************************/
	
	/**
	 * Basic asCollector test. Tests by invoking a MH obtained through a findVirtual call to a class in the same package
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_SamePackage_Virtual() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"arrayToString",MethodType.methodType(String.class, String[].class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asCollector(String[].class, 5);
		
		AssertJUnit.assertEquals(MethodType.methodType(String.class,String.class,String.class,String.class,String.class,String.class),mh.type());
		AssertJUnit.assertEquals("[A,star,I am looking at,may already have,died]",(String)mh.invokeExact("A","star","I am looking at","may already have","died"));
	}
	
	/**
	 * Basic asCollector test. Tests by invoking a MH obtained through a findStatic call to a class in the same package
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_SamePackage_Static() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class,"getLengthStatic",MethodType.methodType(int.class, String[].class));
		mh = mh.asCollector(String[].class, 5);
		
		AssertJUnit.assertEquals(MethodType.methodType(int.class,String.class,String.class,String.class,String.class,String.class),mh.type());
		AssertJUnit.assertEquals(5,(int)mh.invokeExact("A","star","I am looking at","may already have","died"));
	}
	
	/**
	 * asCollector test using Arrays.deepToString
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_SamePackage_Static_Array() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(Arrays.class,"deepToString",MethodType.methodType(String.class, Object[].class));
		mh = mh.asCollector(Object[].class, 1);
		
		AssertJUnit.assertEquals("[[There, is, nothing, outside of text]]",(String)mh.invokeExact( (Object) new Object[] {"There", "is", "nothing", "outside of text"} ) );
	}
	
	/**
	 * Negative test : asCollector using wrong type on a MH obtained through a findVirtual call to a class in the same package
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_SamePackage_Virtual_WrongType() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class, String[].class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		boolean illegalArgumentExceptionThrown = false; 
		try {
			mh = mh.asCollector(int[].class, 2);
		}
		catch(IllegalArgumentException e) {
			illegalArgumentExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentExceptionThrown);
	}
	
	/**
	 * Cross package test : asCollector MH for a virtual method of a class in a different package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_CrossPackage_Virtual() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"getLength",MethodType.methodType(int.class, String[].class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		mh = mh.asCollector(String[].class, 5);
		
		AssertJUnit.assertEquals(MethodType.methodType(int.class,String.class,String.class,String.class,String.class,String.class),mh.type());
		AssertJUnit.assertEquals(5,(int)mh.invokeExact("A","star","I am looking at","may already have","died"));
	}
	
	/**
	 * Cross package test : asCollector MH for a static method of a class in a different package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_CrossPackage_Static() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"getLengthStatic",MethodType.methodType(int.class, String[].class));
		mh = mh.asCollector(String[].class, 5);
		
		AssertJUnit.assertEquals(MethodType.methodType(int.class,String.class,String.class,String.class,String.class,String.class),mh.type());
		AssertJUnit.assertEquals(5,(int)mh.invokeExact("A","star","I am looking at","may already have","died"));
	}
	
	/**
	 * Negative test : asCollector using wrong type on a MH obtained through a findVirtual call to a class in a different package
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_CrossPackage_Virtual_WrongType() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"getLength",MethodType.methodType(int.class, String[].class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		boolean illegalArgumentExceptionThrown = false; 
		try {
			mh = mh.asCollector(int[].class, 2);
		}
		catch(IllegalArgumentException e) {
			illegalArgumentExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentExceptionThrown);
	}
	
	/**
	 * Negative test :  asCollector using wrong type on a MH that has no array argument , obtained through a findVirtual call to a class in a different package
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_CrossPackage_Virtual_WrongType_nonArrayTarget() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"addPublic",MethodType.methodType(int.class, int.class, int.class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		boolean illegalArgumentExceptionThrown = false; 
		try {
			mh = mh.asCollector(int[].class, 2);
		}
		catch(IllegalArgumentException e) {
			illegalArgumentExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentExceptionThrown);
	}
	
	/**
	 * asCollector test for compatible array types - ie: MH takes Object[], asCollector it to String[]
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_CompatibleArrayTypes_StringToObject() throws Throwable {
		MethodHandle mh  = MethodHandles.lookup().findVirtual(SamePackageExample.class,"arrayToString",MethodType.methodType(String.class,Object[].class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asCollector(String[].class, 2);
		AssertJUnit.assertEquals("[a,b]",(String)mh.invokeExact("a","b"));
	}
	
	/**
	 * asCollector test for the special case of a MH with Object as the final arg - asCollector that to an array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_TypeCompatibility_Array_Object() throws Throwable {
		MethodHandle mh  = MethodHandles.lookup().findVirtual(SamePackageExample.class,"toOjectArrayString",MethodType.methodType(String.class,Object.class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asCollector(Object[].class, 6);
		AssertJUnit.assertEquals("[a,1000,true,1.1,9223372036854775807,-32768]",mh.invoke("a",1000,true,1.1,Long.MAX_VALUE,Short.MIN_VALUE)); 
	}
	
	/**
	 * asCollector test for the corner cases: 0 sized array, different array types when using 0, etc
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_ZeroSizedArray() throws Throwable{
		//test with String array
		MethodHandle mhGetLength  = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class,String[].class));
		SamePackageExample g = new SamePackageExample();
		mhGetLength = mhGetLength.bindTo(g);
		mhGetLength = mhGetLength.asCollector(String[].class, 0);
		AssertJUnit.assertEquals(0,(int)mhGetLength.invokeExact());
		
		//test with Object array
		MethodHandle mhArrayToString = MethodHandles.lookup().findVirtual(SamePackageExample.class,"arrayToString",MethodType.methodType(String.class,Object[].class));
		mhArrayToString = mhArrayToString.bindTo(g);
		mhArrayToString = mhArrayToString.asCollector(String[].class,0);
		AssertJUnit.assertEquals("[]",(String)mhArrayToString.invokeExact());
		
		//test with byte array
		MethodHandle mhByteArray = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class,byte[].class));
		mhByteArray = mhByteArray.bindTo(g);
		mhByteArray = mhByteArray.asCollector(byte[].class,0);
		AssertJUnit.assertEquals(0,(int)mhByteArray.invokeExact());
		
		//test with short array
		MethodHandle mhShortArray = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class,short[].class));
		mhShortArray = mhShortArray.bindTo(g);
		mhShortArray = mhShortArray.asCollector(short[].class,0);
		AssertJUnit.assertEquals(0,(int)mhShortArray.invokeExact());
		
		//test with boolean array
		MethodHandle mhBooleanArray = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class,boolean[].class));
		mhBooleanArray = mhBooleanArray.bindTo(g);
		mhBooleanArray = mhBooleanArray.asCollector(boolean[].class,0);
		AssertJUnit.assertEquals(0,(int)mhBooleanArray.invokeExact());
		
		//test with long array
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class,long[].class));
		mhLongArray = mhLongArray.bindTo(g);
		mhLongArray = mhLongArray.asCollector(long[].class,0);
		AssertJUnit.assertEquals(0,(int)mhLongArray.invokeExact());
		
		//test with char array
		MethodHandle mhCharArray = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class,char[].class));
		mhCharArray = mhCharArray.bindTo(g);
		mhCharArray = mhCharArray.asCollector(char[].class,0);
		AssertJUnit.assertEquals(0,(int)mhCharArray.invokeExact());
		
		//test with double array
		MethodHandle mhDoubleArray = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class,double[].class));
		mhDoubleArray = mhDoubleArray.bindTo(g);
		mhDoubleArray = mhDoubleArray.asCollector(double[].class,0);
		AssertJUnit.assertEquals(0,(int)mhDoubleArray.invokeExact());
		
		//test with float array
		MethodHandle mhFloatArray = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class,float[].class));
		mhFloatArray = mhFloatArray.bindTo(g);
		mhFloatArray = mhFloatArray.asCollector(float[].class,0);
		AssertJUnit.assertEquals(0,(int)mhFloatArray.invokeExact());
		
		//test with int array
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class,"getLength",MethodType.methodType(int.class,int[].class));
		mhIntArray = mhIntArray.bindTo(g);
		mhIntArray = mhIntArray.asCollector(int[].class,0);
		AssertJUnit.assertEquals(0,(int)mhIntArray.invokeExact());
	}
	
	
	/******************************
	 * Tests for asVarargsCollector
	 * ****************************/
	
	/**
	 * Basic asVarargsCollector test using a virtual method in a class living in the same package (uses MethodHandle.invoke)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_SamePackage_Virtual_Invoke() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"arrayToString",MethodType.methodType(String.class, String[].class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asVarargsCollector(String[].class);
		
		AssertJUnit.assertEquals(MethodType.methodType(String.class,String[].class),mh.type());
		AssertJUnit.assertEquals("[A,star,I am looking at,may already have,died]",mh.invoke("A","star","I am looking at","may already have","died"));
	}
	
	/**
	 * Basic asVarargsCollector test using a virtual method in a class living in the same package (uses MethodHandle.invokeExact)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_SamePackage_Virtual_InvokeExact() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"arrayToString",MethodType.methodType(String.class, String[].class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asVarargsCollector(String[].class);
		
		AssertJUnit.assertEquals(MethodType.methodType(String.class,String[].class),mh.type());
		AssertJUnit.assertEquals("[A,star,I am looking at,may already have,died]", (String)mh.invokeExact(new String[]{"A","star","I am looking at","may already have","died"}));
	}
	
	/**
	 * Negative test : asVarargsCollector test using wrong method type
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_SamePackage_Virtual_WrongType() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"getLength",MethodType.methodType(int.class, String[].class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		boolean illegalArgumentExceptionThrown = false; 
		try {
			mh = mh.asVarargsCollector(int[].class);
		}
		catch(IllegalArgumentException e) {
			illegalArgumentExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentExceptionThrown);
	}
	
	/**
	 * asVarargsCollector test using Arrays.deepToString method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_SamePackage_Static_Array() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(Arrays.class,"deepToString",MethodType.methodType(String.class, Object[].class));
		mh = mh.asVarargsCollector(Object[].class);
		
		AssertJUnit.assertEquals("[[There, is, nothing, outside of text]]",(String) mh.invoke((Object) new Object[] {"There", "is", "nothing", "outside of text"} ) );
	}
	
	/**
	 * Basic asVarargsCollector test using a virtual method in a class living in a different package (uses MethodHandle.invoke)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_CrossPackage_Virtual_Invoke() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"getLength",MethodType.methodType(int.class, String[].class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		mh = mh.asVarargsCollector(String[].class);
		
		AssertJUnit.assertEquals(MethodType.methodType(int.class,String[].class),mh.type());
		AssertJUnit.assertEquals(6,mh.invoke("A","star","I am looking at","may already have","died","long ago"));
	}
	
	/**
	 * Basic asVarargsCollector test using a virtual method in a class living in a different package (uses MethodHandle.invokeExact)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_CrossPackage_Virtual_InvokeExact() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"getLength",MethodType.methodType(int.class, String[].class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		mh = mh.asVarargsCollector(String[].class);
		
		AssertJUnit.assertEquals(MethodType.methodType(int.class,String[].class),mh.type());
		AssertJUnit.assertEquals(5, (int)mh.invokeExact(new String[]{"A","star","I am looking at","may already have","died"}));
	}
	
	/**
	 * Basic asVarargsCollector test using static method living in a class of a different package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_CrossPackage_Static() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"getLengthStatic",MethodType.methodType(int.class, String[].class));
		mh = mh.asVarargsCollector(String[].class);
		
		AssertJUnit.assertEquals(MethodType.methodType(int.class,String[].class),mh.type());
		AssertJUnit.assertEquals(5,(int)mh.invoke("A","star","I am looking at","may already have","died"));
	}
	
	/**
	 * Negative test : asVarargsCollector test using wrong type and a virtual method belonging to a class in a different package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_CrossPackage_Virtual_WrongType() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"getLength",MethodType.methodType(int.class, String[].class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		
		boolean illegalArgumentExceptionThrown = false; 
		try {
			mh = mh.asVarargsCollector(int[].class);
		}
		catch(IllegalArgumentException e) {
			illegalArgumentExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentExceptionThrown);
	}
	
	/**
	 * Negative test : asVarargsCollector test using wrong method type and virtual method living in a class of a different package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_CrossPackage_Virtual_WrongType_nonArrayTarget() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"addPublic",MethodType.methodType(int.class, int.class, int.class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		boolean illegalArgumentExceptionThrown = false; 
		try {
			mh = mh.asVarargsCollector(int[].class);
		}
		catch(IllegalArgumentException e) {
			illegalArgumentExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentExceptionThrown);
	}

	/**
	 * asVarargsCollector test for compatible array types - ie: MH takes Object[], asCollector it to String[]
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_CompatibleArrayTypes_StringToObject() throws Throwable {
		MethodHandle mh  = MethodHandles.lookup().findVirtual(SamePackageExample.class,"arrayToString",MethodType.methodType(String.class,Object[].class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asVarargsCollector(String[].class);
		AssertJUnit.assertEquals("[a,b]",(String)mh.invoke("a","b"));
	}
	
	/**
	 * asVarargsCollectorTest for the special case of a MH with Object as the final arg - asCollector that to an array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asVarargsCollector_TypeCompatibility_Array_Object() throws Throwable {
		MethodHandle mh  = MethodHandles.lookup().findVirtual(SamePackageExample.class,"toOjectArrayString",MethodType.methodType(String.class,Object.class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asVarargsCollector(Object[].class);
		AssertJUnit.assertEquals("[a,1000,true,1.1,9223372036854775807,-32768]",mh.invoke("a",1000,true,1.1,Long.MAX_VALUE,Short.MIN_VALUE)); 
	}
	
	
	/******************************
	 * Tests for asSpreader
	 * ****************************/
	
	/**
	 * Basic asSpreader test using a virtual method in the same packge 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_SamePackage_Virtual() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"addPublic",MethodType.methodType(int.class,int.class,int.class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asSpreader(int[].class, 2);
		AssertJUnit.assertEquals(MethodType.methodType(int.class,int[].class),mh.type());
		AssertJUnit.assertEquals(5,(int)mh.invokeExact(new int[] {3,2}));
	}
	
	/**
	 * Negative test : asSpreader test using wrong method type for a virtual method in the same package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_SamePackage_Virtual_WrongType() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"addPublic",MethodType.methodType(int.class,int.class,int.class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		
		boolean wrongMethodTypeExceptionThrown = false; 
		
		try {
			mh = mh.asSpreader(String[].class, 2);
		}
		catch(WrongMethodTypeException e) {
			wrongMethodTypeExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(wrongMethodTypeExceptionThrown);
	}
	
	/**
	 * Negative test : asSpreader test using wrong argument count for the target method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_SamePackage_Virtual_WrongArgCount() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"addPublic",MethodType.methodType(int.class,int.class,int.class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		
		boolean illegalArgumentException = false; 
		
		try {
			mh = mh.asSpreader(int[].class, 3);
		}
		catch(IllegalArgumentException e) {
			illegalArgumentException = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentException);
	}
	
	/**
	 * asSpreader test using static method of a class living in the same package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_SamePackage_Static() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class,"addPublicStatic",MethodType.methodType(int.class,int.class,int.class));
		mh = mh.asSpreader(int[].class, 2);
		AssertJUnit.assertEquals(MethodType.methodType(int.class,int[].class),mh.type());
		AssertJUnit.assertEquals(5,(int)mh.invokeExact(new int[] {3,2}));
	}
	
	/**
	 * Negative test : asSpreader test using wrong method type for a static method belonging to a class in the same package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_SamePackage_Static_WrongType() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class,"addPublicStatic",MethodType.methodType(int.class,int.class,int.class));
		mh = mh.asSpreader(int[].class, 2);
		boolean illegalArgExceptionThrown = false; 
		
		try {
			mh = mh.asSpreader(String[].class, 2);
		}
		catch(IllegalArgumentException e) {
			illegalArgExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(illegalArgExceptionThrown);
	}
	
	/**
	 * Basic asSpreader test using cross package virtual method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_CrossPackage_Virtual() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"addPublic",MethodType.methodType(int.class,int.class,int.class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		mh = mh.asSpreader(int[].class, 2);
		AssertJUnit.assertEquals(MethodType.methodType(int.class,int[].class),mh.type());
		AssertJUnit.assertEquals(5,(int)mh.invokeExact(new int[] {3,2}));
	}
	
	/**
	 * Basic asSpreader test using cross package static method 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_CrossPackage_Static() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStatic",MethodType.methodType(int.class,int.class,int.class));
		mh = mh.asSpreader(int[].class, 2);
		AssertJUnit.assertEquals(MethodType.methodType(int.class,int[].class),mh.type());
		AssertJUnit.assertEquals(5,(int)mh.invokeExact(new int[] {3,2}));
	}
	
	/**
	 * Negative test : asSpreader test using cross package virtual method with a wrong method type
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_CrossPackage_Virtual_WrongType() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"addPublic",MethodType.methodType(int.class,int.class,int.class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		boolean wrongMethodTypeExceptionThrown = false; 
		
		try {
			mh = mh.asSpreader(String[].class, 2);
		}
		catch(WrongMethodTypeException e) {
			wrongMethodTypeExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(wrongMethodTypeExceptionThrown);
	}
	
	/**
	 * Negative test : asSpreader test using a cross-package static method and wrong method type
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_CrossPackage_Static_WrongType() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStatic",MethodType.methodType(int.class,int.class,int.class));
		mh = mh.asSpreader(int[].class, 2);
		boolean illegalArgumentExceptionThrown = false; 
		
		try {
			mh = mh.asSpreader(String[].class, 2);
		}
		catch(IllegalArgumentException e) {
			illegalArgumentExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentExceptionThrown);
	}
	
	/**
	 * Negative test : asSpreader test using cross package static method and a wrong argument count
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_CrossPackage_Static_WrongArgCount() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStatic",MethodType.methodType(int.class,int.class,int.class));
		
		boolean illegalArgumentException = false; 
		
		try {
			mh = mh.asSpreader(int[].class, 3);
		}
		catch(IllegalArgumentException e) {
			illegalArgumentException = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentException);
	}
	
	/******************************
	 * Tests for MethodHandle arity
	 * ****************************/
	/**
	 * Create MethodHandle with highest possible arity and use asSpreader (253)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_ArityLimit() throws Throwable {
		MethodHandle mh = null;
		mh = MethodHandles.lookup().findVirtual(MethodHandleTest.class, "helperSpreaderMethod_253arg", helperMethodType_void_253int);
		mh = mh.asSpreader(int[].class, 253);
	}
	
	/**
	 * Create static MethodHandle with highest possible arity and use asSpreader (254)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asSpreader_Static_ArityLimit() throws Throwable {
		MethodHandle mh = null;
		mh = MethodHandles.lookup().findStatic(MethodHandleTest.class, "helperSpreaderStaticMethod_254arg", helperMethodType_void_254int);
		mh = mh.asSpreader(int[].class, 254);
	}
		
	/**
	 * Negative test: Create MethodHandle with highest possible arity + 1 (254) 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_MethodHandle_ArityLimit_TooHigh() throws Throwable {
		boolean illegalArgumentException = false;
		try {
			MethodHandles.lookup().findVirtual(MethodHandleTest.class, "helperSpreaderMethod_254arg", helperMethodType_void_254int);
		}
		catch (IllegalArgumentException e) {
			illegalArgumentException = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentException);
	}
	
	/**
	 * Negative test: Create static MethodHandle with highest possible arity + 1 (255)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_MethodHandle_Static_ArityLimit_TooHigh() throws Throwable {
		boolean illegalArgumentException = false;
		try {
			MethodHandles.lookup().findStatic(MethodHandleTest.class, "helperSpreaderStaticMethod_255arg", helperMethodType_void_255int);
		}
		catch (IllegalArgumentException e) {
			illegalArgumentException = true;
		}
		
		AssertJUnit.assertTrue(illegalArgumentException);
	}
	
	/**
	 * Create MethodHandle with highest possible arity using asCollector (253)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_ArityLimit() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(MethodHandleTest.class, "helperCollectorMethod", MethodType.methodType(void.class,int[].class));
		mh = mh.asCollector(int[].class, 253);
	}
	
	/**
	 * Negative test: Create MethodHandle with highest possible arity + 1 using asCollector (254)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_ArityLimit_TooHigh() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(MethodHandleTest.class, "helperCollectorMethod", MethodType.methodType(void.class, int[].class));
		boolean illegalArgumentException = false; 
		try {
			mh = mh.asCollector(int[].class, 254);
		} catch(IllegalArgumentException e) {
			illegalArgumentException = true;
		}
		AssertJUnit.assertTrue(illegalArgumentException);
	}
	
	/**
	 * Create MethodHandle with highest possible arity using asCollector (254)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_Static_ArityLimit() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(MethodHandleTest.class, "helperStaticCollectorMethod", MethodType.methodType(void.class, int[].class));
		mh = mh.asCollector(int[].class, 254);
	}
	
	/**
	 * Negative test: Create MethodHandle with highest possible arity + 1 using asCollector (255)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asCollector_Static_ArityLimit_TooHigh() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(MethodHandleTest.class, "helperStaticCollectorMethod", MethodType.methodType(void.class, int[].class));
		boolean illegalArgumentException = false; 
		try {
			mh = mh.asCollector(int[].class, 255);
		} catch(IllegalArgumentException e) {
			illegalArgumentException = true;
		}
		AssertJUnit.assertTrue(illegalArgumentException);
	}
	
	/**
	 * Create spreadInvoker with highest possible arity (253)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_spreadInvoker_ArityLimit() throws Throwable {
		final int args = 252;
		MethodHandle mh = MethodHandles.lookup().findVirtual(MethodHandleTest.class, "helperSpreaderMethod_" + args + "arg", helperMethodType_void_252int);
		MethodHandle invoker = MethodHandles.spreadInvoker(helperMethodType_void_252int.insertParameterTypes(0, MethodHandleTest.class), 1);
		Object[] a = new Object[args];
		for (int i = 0; i < args; i++) {
			a[i] = 0;
		}
		invoker.invoke(mh, new MethodHandleTest(), a);
	}
	
	/**
	 * Create spreadInvoker with highest possible arity 254)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_spreadInvoker_Static_ArityLimit() throws Throwable {
		final int args = 253;
		MethodHandle mh = MethodHandles.lookup().findStatic(MethodHandleTest.class, "helperSpreaderStaticMethod_" + args + "arg", helperMethodType_void_253int);
		MethodHandle invoker = MethodHandles.spreadInvoker(helperMethodType_void_253int, 0);
		Object[] a = new Object[args];
		for (int i = 0; i < args; i++) {
			a[i] = 0;
		}
		invoker.invoke(mh, a);
	}

	/**
	 * Create exactInvoker with highest possible arity (252)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_exactInvoker_ArityLimit() throws Throwable {
		final int args = 252;
		MethodHandle mh = MethodHandles.lookup().findVirtual(MethodHandleTest.class, "helperSpreaderMethod_" + args + "arg", helperMethodType_void_252int);
		MethodHandle invoker = MethodHandles.exactInvoker(helperMethodType_void_252int.insertParameterTypes(0, MethodHandleTest.class));
		invoker.invokeExact(mh, new MethodHandleTest(), 
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	
	/**
	 * Create static exactInvoker with highest possible arity (253)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_exactInvoker_Static_ArityLimit() throws Throwable {
		final int args = 253;
		MethodHandle mh = MethodHandles.lookup().findStatic(MethodHandleTest.class, "helperSpreaderStaticMethod_" + args + "arg", helperMethodType_void_253int);
		MethodHandle invoker = MethodHandles.exactInvoker(helperMethodType_void_253int);
		invoker.invokeExact(mh, 
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	
	/**
	 * Negative test: Create exactInvoker with highest possible arity + 1 (253)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_exactInvoker_ArityLimit_TooHigh() throws Throwable {
		boolean IAEThrown = false;
		try {
			MethodHandles.exactInvoker(helperMethodType_void_253int.insertParameterTypes(0, MethodHandleTest.class));
		} catch (IllegalArgumentException e) {
			IAEThrown = true;
		}
		AssertJUnit.assertTrue(IAEThrown);
	}
	
	/**
	 * Negative test: Create static exactInvoker with highest possible arity + 1 (254)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_exactInvoker_Static_ArityLimit_TooHigh() throws Throwable {
		boolean IAEThrown = false;
		try {
			MethodHandles.exactInvoker(helperMethodType_void_254int);
		} catch (IllegalArgumentException e) {
			IAEThrown = true;
		}
		AssertJUnit.assertTrue(IAEThrown);
	}
	
	/**
	 * Create invoker with highest possible arity (252)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invoker_ArityLimit() throws Throwable {
		final int args = 252;
		MethodHandle mh = MethodHandles.lookup().findVirtual(MethodHandleTest.class, "helperSpreaderMethod_" + args + "arg", helperMethodType_void_252int);
		MethodHandle invoker = MethodHandles.invoker(helperMethodType_void_252int.insertParameterTypes(0, MethodHandleTest.class));
		invoker.invoke(mh, new MethodHandleTest(), 
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	
	/**
	 * Create invoker with highest possible arity (253)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invoker_Static_ArityLimit() throws Throwable {
		final int args = 253;
		MethodHandle mh = MethodHandles.lookup().findStatic(MethodHandleTest.class, "helperSpreaderStaticMethod_" + args + "arg", helperMethodType_void_253int);
		MethodHandle invoker = MethodHandles.invoker(helperMethodType_void_253int);
		invoker.invoke(mh, 
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}

	/**
	 * Negative test: Create invoker with highest possible arity + 1 (253)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invoker_ArityLimit_TooHigh() throws Throwable {
		boolean IAEThrown = false;
		try {
			MethodHandles.invoker(helperMethodType_void_253int.insertParameterTypes(0, MethodHandleTest.class));
		} catch (IllegalArgumentException e) {
			IAEThrown = true;
		}
		AssertJUnit.assertTrue(IAEThrown);
	}
	
	/**
	 * Negative test: Create static invoker with highest possible arity + 1 (254)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invoker_Static_ArityLimit_TooHigh() throws Throwable {
		boolean IAEThrown = false;
		try {
			MethodHandles.invoker(helperMethodType_void_254int);
		} catch (IllegalArgumentException e) {
			IAEThrown = true;
		}
		AssertJUnit.assertTrue(IAEThrown);
	}
	
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_ArityLimit() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(MethodHandleTest.class, "helperSpreaderMethod_253arg", helperMethodType_void_253int);
		mh.invokeWithArguments(new MethodHandleTest(),
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_Static_ArityLimit() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(MethodHandleTest.class, "helperSpreaderStaticMethod_254arg", helperMethodType_void_254int);
		mh.invokeWithArguments(
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_VarArgs_ArityLimit() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(MethodHandleTest.class, "helperVarArgsMethod", MethodType.methodType(void.class, String.class, int[].class));
		mh.invokeWithArguments(new MethodHandleTest(), "test",
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	
	
	/******************************
	 * Tests for asFixedArity
	 * ****************************/
	
	/**
	 * asFixedArity test to ensure MethodHandles.lookup().find* methods returned varargs handles for methods with the ACC_VARAGS bit
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asFixedArity_Using_VariableArity_JavaMethod_Virtual_SamePackage() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"takeVariableArityObject", MethodType.methodType(void.class, Object[].class));
		SamePackageExample g = new SamePackageExample(); 
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		mh = mh.asFixedArity();
		
		//Ensure after calling asFixedArity, it is no longer a varargs handles 
		AssertJUnit.assertFalse(mh.isVarargsCollector());
		
		AssertJUnit.assertEquals(null,mh.invoke(g,new Object[]{new Object[]{"abc",true,12}}));
	}
	
	/**
	 * Basic asFixedArity test using a same-package static method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asFixedArity_SamePackage_Static() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "getLengthStatic", MethodType.methodType(int.class,String[].class));
		mh = mh.asVarargsCollector(String[].class);
		MethodHandle mhFix = mh.asFixedArity();
		
		AssertJUnit.assertFalse(mhFix.isVarargsCollector());
		AssertJUnit.assertEquals(5,mhFix.invoke(new String[]{"a","b","c","d","e"}));
	}
	
	/**
	 * Negative test : asFixedArity test using variable input
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asFixedArity_SamePackage_Static_VarialbeInput() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "getLengthStatic", MethodType.methodType(int.class,String[].class));
		mh = mh.asVarargsCollector(String[].class);
		
		MethodHandle mhFix = mh.asFixedArity();
		
		AssertJUnit.assertFalse(mhFix.isVarargsCollector());
		
		boolean wmtThrown = false;
		
		try {
			AssertJUnit.assertEquals(5,mhFix.invoke("a","b","c","d","e"));
		}
		catch(WrongMethodTypeException e) {
			wmtThrown = true;
		}
		
		AssertJUnit.assertTrue(wmtThrown);
	}
	
	
	/****************************************
	 * Tests for invokeWithArguments(List<?>)
	 * **************************************/
	
	public static String helper_invokeWithArguments_List_Different_Array_Type(Object a, Object b, Object c) {
		return "" + a + ":" + b + ":" + c;
	}
	
	/**
	 * Basic invokeWithArgument(List<?>) test using same-package virtual method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_List_Different_Array_Type() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(
				MethodHandleTest.class, 
				"helper_invokeWithArguments_List_Different_Array_Type", 
				MethodType.methodType(String.class, Object.class,Object.class,Object.class));	
		String result = (String)mh.invokeWithArguments(Arrays.asList("A", "B", "C"));
		AssertJUnit.assertEquals("A:B:C", result);
	}
	
	/**
	 * Basic invokeWithArgument(List<?>) test using same-package virtual method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_List_SamePackage_Virtual() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addPublic", MethodType.methodType(int.class,int.class,int.class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		
		List<Integer> l = new ArrayList<Integer>();
		l.add(1);
		l.add(2);
		
		AssertJUnit.assertEquals(3,(int)mh.invokeWithArguments(l));
	}
	
	/**
	 * Negative test : invokeWithArgument(List<?>) test with mismatched number of argument passed in argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_List_SamePackage_Virtual_WrongArgCount() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addPublic", MethodType.methodType(int.class,int.class,int.class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		boolean wmtThrown = false;
		
		List<Integer> l = new ArrayList<Integer>();
		l.add(1);
		l.add(2);
		l.add(3);
		
		try {
			AssertJUnit.assertEquals(6,(int)mh.invokeWithArguments(l));
		}
		catch(WrongMethodTypeException e) {
			wmtThrown = true;
		}
		
		AssertJUnit.assertTrue(wmtThrown);
	}
	
	/**
	 *  invokeWithArgument(List<?>) test using a same-package static method 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_List_SamePackage_Static() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "addPublicStatic", MethodType.methodType(int.class,int.class,int.class));
		
		List<Integer> l = new ArrayList<Integer>();
		l.add(1);
		l.add(2);
		
		AssertJUnit.assertEquals(3,(int)mh.invokeWithArguments(l));
	}
	
	/**
	 * invokeWithArgument(List<?>) test using a cross-package virtual method 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_List_CrossPackage_Virtual() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class, "addPublic", MethodType.methodType(int.class,int.class,int.class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		
		List<Integer> l = new ArrayList<Integer>();
		l.add(1);
		l.add(2);
		
		AssertJUnit.assertEquals(3,(int)mh.invokeWithArguments(l));
	}
	
	/**
	 * invokeWithArgument(List<?>) test using a cross-package static method 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_List_CrossPackage_Static() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class, "addPublicStatic", MethodType.methodType(int.class,int.class,int.class));
		
		List<Integer> l = new ArrayList<Integer>();
		l.add(1);
		l.add(2);
		
		AssertJUnit.assertEquals(3,(int)mh.invokeWithArguments(l));
	}
	

	/****************************************
	 * Tests for invokeWithArguments(Object...)
	 * **************************************/
	
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_WMT_NULL_args()  throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findVirtual(Object.class, "toString", MethodType.methodType(String.class));
		try {
			mh.invokeWithArguments("receiver", null, null);
			Assert.fail("Should have thrown WMT");
		} catch(WrongMethodTypeException e) {
			
		}
	}
	
	/**
	 * invokeWithArguments(Object... args) test using a same-package virtual method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_VarargsObj_SamePackage_Virtual() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addPublic", MethodType.methodType(int.class,int.class,int.class));
		SamePackageExample g = new SamePackageExample();
		
		AssertJUnit.assertEquals(3,(int)mh.invokeWithArguments(g,1,2));
	}
	
	/**
	 * invokeWithArguments(Object... args) test to ensure that the varargsCollector behavior is respected by the invokeWithArguments call
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_VarargsObj_SamePackage_Virtual_VarargsMethod() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addPublicVariableArity", MethodType.methodType(int.class,Object[].class));
		SamePackageExample g = new SamePackageExample();
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		AssertJUnit.assertEquals(9,mh.invokeWithArguments(g, 1, 2, 2, 2, 2));
	}
	
	/**
	 * Negative test : invokeWithArguments(Object... args) with wrong argument type
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_VarargsObj_SamePackage_Virtual_WrongArgCount() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addPublic", MethodType.methodType(int.class,int.class,int.class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		boolean wmtThrown = false;
		
		try {
			AssertJUnit.assertEquals(6,(int)mh.invokeWithArguments(1,2,3));
		}
		catch(WrongMethodTypeException e) {
			wmtThrown = true;
		}
		
		AssertJUnit.assertTrue(wmtThrown);
	}
	
	/**
	 * invokeWithArguments(Object... args) test with same-package static method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_VarargsObj_SamePackage_Static() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "addPublicStatic", MethodType.methodType(int.class,int.class,int.class));
		
		Object [] l = new Object[2];
		l[0] = 1;
		l[1] = 2;
		
		AssertJUnit.assertEquals(3,(int)mh.invokeWithArguments(l));
	}
	
	/**
	 * invokeWithArguments(Object... args) with cross-package virtual method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_VarargsObj_CrossPackage_Virtual() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class, "addPublic", MethodType.methodType(int.class,int.class,int.class));
		PackageExamples g = new PackageExamples();
		mh = mh.bindTo(g);
		
		Object [] l = new Object[2];
		l[0] = 1;
		l[1] = 2;
		
		AssertJUnit.assertEquals(3,(int)mh.invokeWithArguments(l));
	}
	
	/**
	 * invokeWithArguments(Object... args) with cross-package static method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_VarargsObj_CrossPackage_Static() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class, "addPublicStatic", MethodType.methodType(int.class,int.class,int.class));
		
		Object [] l = new Object[2];
		l[0] = 1;
		l[1] = 2;
		
		AssertJUnit.assertEquals(3,(int)mh.invokeWithArguments(l));
	}
	
	/**
	 * invokeWithArguments(Object... args) test with Corner case of 0 length array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_VarargsObj_CrossPackage_Static_ZeroLengthArray() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "returnOne", MethodType.methodType(String.class));
		
		Object [] l = new Object[0];
		
		AssertJUnit.assertEquals("1",(String)mh.invokeWithArguments(l));
		
		MethodHandle mh2 = MethodHandles.lookup().findStatic(SamePackageExample.class, "addPublicStatic", MethodType.methodType(int.class,int.class,int.class));
		
		Object [] l2 = new Object[0];
		
		boolean wrongMethodTypeExceptionThrown = false;
		
		try {
			AssertJUnit.assertEquals(0,(int)mh2.invokeWithArguments(l2));
		}
		catch(WrongMethodTypeException e) {
			wrongMethodTypeExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(wrongMethodTypeExceptionThrown);
	}
	
	/****************************************
	 * Tests for isVarargsCollector
	 * **************************************/
	
	/**
	 * Test isVarargsCollector using a call to asVaragrsCollector
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_Virtual_SamePackage() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"arrayToString",MethodType.methodType(String.class, String[].class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asVarargsCollector(String[].class);
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
	}
	
	/**
	 * Negative test : isVarargsCollector test using asCollector call
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asCollector_Virtual_SamePackage() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"arrayToString",MethodType.methodType(String.class, String[].class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		mh = mh.asCollector(String[].class,3);
		
		AssertJUnit.assertFalse(mh.isVarargsCollector());
	}
	
	/**
	 * Basic isVarargsCollector test using a cross-package static method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_Static_CrossPackage() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"getLengthStatic",MethodType.methodType(int.class, String[].class));
		mh = mh.asVarargsCollector(String[].class);
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
	}
	
	/**
	 * Test isVarargsCollector using a asVarargsCollector type MethodHandle obtained from a call to a lookup method which resolves to a variable arity Java method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_VariableArity_JavaMethod_Virtual_SamePackage() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(SamePackageExample.class,"addPublicVariableArity",MethodType.methodType(int.class, int[].class));
				
		AssertJUnit.assertTrue(mh.isVarargsCollector());
	}
	
	/**
	 * Test isVarargsCollector using MethodHandle obtained from a call to a lookup method which resolves to a variable arity Java constructor
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_VariableArity_JavaConstructor_SamePackage() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findConstructor(SamePackageExample.class,MethodType.methodType(void.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
	}
	
	/**
	 * Test isVarargsCollector using a asVarargsCollector type MH obtained from a call to a lookup method which resolves to a variable arity Java method in a different package
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_VariableArity_JavaMethod_Virtual_CrossPackage() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findVirtual(PackageExamples.class,"addPublicVariableArity",MethodType.methodType(int.class, int[].class));

		AssertJUnit.assertTrue(mh.isVarargsCollector());
	}
	
	/**
	 * Test isVarargsCollector using a asVarargsCollector type MH obtained from a call to a lookup method which resolves to a static variable arity Java method in a different package
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_VariableArity_JavaMethod_Static_CrossPackage() throws Throwable{
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStaticVariableArity",MethodType.methodType(int.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
	}
	
	/**
	 * Test isVarargsCollector to ensure that an asVarargsCollector handle that has been asTyped() to the same type still remains asVarargsCollector type
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_asTyped() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStaticVariableArity",MethodType.methodType(int.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		mh = mh.asType(MethodType.methodType(int.class,int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
	}
	
	/**
	 * Ensure that an asVarargsCollector handle that has been asTyped() to a different type no longer remains asVarargsCollector type
	 * Change type of argument from int --> byte
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_asTyped_arg_int_to_byte() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStaticVariableArity",MethodType.methodType(int.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		mh = mh.asType(MethodType.methodType(int.class,byte.class));
		
		AssertJUnit.assertFalse(mh.isVarargsCollector());  
	}
	
	/**
	 * Ensure that an asVarargsCollector handle that has been asTyped() to a different type no longer remains asVarargsCollector type
	 * Negative test : Change return type from int --> byte : This should throw exception as return types can only be converted from small to big types
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_asTyped_RT_int_to_byte() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStaticVariableArity",MethodType.methodType(int.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		boolean wrongMethodTypeExceptionThrown = false;
		
		try {
			mh = mh.asType(MethodType.methodType(byte.class,int[].class));
		}
		catch(WrongMethodTypeException e) {
			wrongMethodTypeExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(wrongMethodTypeExceptionThrown);  
	}
	
	/**
	 * Ensure that an asVarargsCollector handle that has been asTyped() to a different type no longer remains asVarargsCollector type
	 * Change type of argument from int --> short
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_asTyped_arg_int_to_short() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStaticVariableArity",MethodType.methodType(int.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		mh = mh.asType(MethodType.methodType(int.class,short.class));
		
		AssertJUnit.assertFalse(mh.isVarargsCollector());  
	}
	
	/**
	 * Ensure that an asVarargsCollector handle that has been asTyped() to a different type no longer remains asVarargsCollector type
	 * Negative test : Change return type from int --> short : This should throw exception as return types can only be converted from small to big types
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_asTyped_RT_int_to_short() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStaticVariableArity",MethodType.methodType(int.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		boolean wrongMethodTypeExceptionThrown = false;
		
		try {
			mh = mh.asType(MethodType.methodType(short.class,int[].class));
		}
		catch(WrongMethodTypeException e) {
			wrongMethodTypeExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(wrongMethodTypeExceptionThrown);  
	}
	
	/**
	 * Ensure that an asVarargsCollector handle that has been asTyped() to a different type no longer remains asVarargsCollector type
	 * Negative test : Change type of argument from int --> long : This is an invalid conversion
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_asTyped_arg_int_to_long() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStaticVariableArity",MethodType.methodType(int.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		boolean wrongMethodTypeExceptionThrown = false; 
		
		try {
			mh = mh.asType(MethodType.methodType(int.class,long.class));
		}
		catch(WrongMethodTypeException e) {
			wrongMethodTypeExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(wrongMethodTypeExceptionThrown);
	}
	
	
	/**
	 * Ensure that an asVarargsCollector handle that has been asTyped() to a different type no longer remains asVarargsCollector type
	 * Change return type from int --> long
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_asTyped_RT_int_to_long() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStaticVariableArity",MethodType.methodType(int.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		mh = mh.asType(MethodType.methodType(long.class,int[].class));
		
		AssertJUnit.assertFalse(mh.isVarargsCollector());  
	}
	
	/**
	 * Ensure that an asVarargsCollector handle that has been asTyped() to a different type no longer remains asVarargsCollector type
	 * Negative test : Change type of argument from int --> double : This is an invalid conversion
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isVarargsCollector_Using_asVarargsCollector_asTyped_arg_int_to_double() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(PackageExamples.class,"addPublicStaticVariableArity",MethodType.methodType(int.class, int[].class));
		
		AssertJUnit.assertTrue(mh.isVarargsCollector());
		
		boolean wrongMethodTypeExceptionThrown = false; 
		
		try {
			mh = mh.asType(MethodType.methodType(int.class,double.class));
		}
		catch(WrongMethodTypeException e) {
			wrongMethodTypeExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue(wrongMethodTypeExceptionThrown);
	}
	
    /**
     * Tests bindTo by binding non-null and null objects to a MethodHandle
     * @throws Throwable
     */
    @Test(groups = { "level.extended" })
	public void testBindTo() throws Throwable {
        MethodHandle handle = SamePackageExample.getLookup().findSpecial(SamePackageExample.class, "isReceiverNull", MethodType.methodType(boolean.class), SamePackageExample.class);
        
        AssertJUnit.assertFalse((boolean)handle.bindTo(new SamePackageExample()).invokeExact());
        
        boolean npeThrownWhenNullIsPassedInBindTo = false;
        
        try{
        	boolean b = (boolean)(handle.bindTo(null).invokeExact());
        }
        catch(NullPointerException e) {
        	npeThrownWhenNullIsPassedInBindTo = true;
        }
        AssertJUnit.assertTrue(npeThrownWhenNullIsPassedInBindTo);
    }
    
    /**
     * Negative test for invoke
     * @throws Throwable
     */
    @Test(groups = { "level.extended" })
	public void test_Invoke() throws Throwable {
    	MethodHandle handle = MethodHandles.lookup().findStatic(Integer.class, "parseInt", MethodType.methodType(int.class,String.class));
    	boolean nfeHit = false;
    	boolean wmtHit = false;
    	
    	try {
    		AssertJUnit.assertEquals(5,handle.invoke("s"));
    	}
    	catch(NumberFormatException e) {
    		nfeHit = true;
    	}
    	
    	AssertJUnit.assertTrue(nfeHit);
    	
    	try {
    		AssertJUnit.assertEquals(5,handle.invoke(5));
    	}
    	catch(WrongMethodTypeException e) {
    		wmtHit = true;
    	}
    	
    	AssertJUnit.assertTrue(wmtHit);
    }
    
	/**
	 * Negative test for invokeExcat
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_InvokeExact() throws Throwable {
		MethodHandle mh  = MethodHandles.lookup().findVirtual(SamePackageExample.class,"arrayToString",MethodType.methodType(String.class,Object[].class));
		SamePackageExample g = new SamePackageExample();
		mh = mh.bindTo(g);
		boolean wmtHit = false;
		try{
			AssertJUnit.assertEquals("[a,b]",(String)mh.invokeExact("a","b"));
		}
		catch(WrongMethodTypeException e) {
			wmtHit = true;
		}
		
		AssertJUnit.assertTrue(wmtHit);
		
		wmtHit = false;
		ArrayList<String> a = new ArrayList<String>();
		MethodHandle m = MethodHandles.lookup().findVirtual(ArrayList.class, "add", MethodType.methodType(boolean.class,Object.class));
        
		try {
        	Object r = (Object)m.invokeExact(a,1);
        }catch(WrongMethodTypeException e) {
        	wmtHit = true;
        }
        
		AssertJUnit.assertTrue(wmtHit);
	}
	
	public static int countArgs(int... args) {
		return args.length;
	}

	@Test(groups = { "level.extended" })
	public void test_invokeWithArguments_variable_input() throws Throwable {
		MethodHandle MH = MethodHandles.lookup().findStatic( MethodHandleTest.class, "countArgs", MethodType.methodType( int.class, int[].class) );
		ArrayList args = new ArrayList();
		for ( int i = 0; i < 25; i++ ){
			args.add( i );
			AssertJUnit.assertEquals( i+1, MH.invokeWithArguments(args.toArray()) );
		}
	}

	private static MethodType helperMethodType_void_252int = MethodType.methodType(void.class,
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class);
	private static MethodType helperMethodType_void_253int = MethodType.methodType(void.class,
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class);
	private static MethodType helperMethodType_void_254int = MethodType.methodType(void.class,
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class);
	private static MethodType helperMethodType_void_255int = MethodType.methodType(void.class,
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, 
			int.class, int.class, int.class, int.class ,int.class);
	
	private void helperSpreaderMethod_252arg(
			int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, 
			int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19, int i20, 
			int i21, int i22, int i23, int i24, int i25, int i26, int i27, int i28, int i29, int i30, 
			int i31, int i32, int i33, int i34, int i35, int i36, int i37, int i38, int i39, int i40, 
			int i41, int i42, int i43, int i44, int i45, int i46, int i47, int i48, int i49, int i50, 
			int i51, int i52, int i53, int i54, int i55, int i56, int i57, int i58, int i59, int i60, 
			int i61, int i62, int i63, int i64, int i65, int i66, int i67, int i68, int i69, int i70, 
			int i71, int i72, int i73, int i74, int i75, int i76, int i77, int i78, int i79, int i80, 
			int i81, int i82, int i83, int i84, int i85, int i86, int i87, int i88, int i89, int i90, 
			int i91, int i92, int i93, int i94, int i95, int i96, int i97, int i98, int i99, int i100, 
			int i101, int i102, int i103, int i104, int i105, int i106, int i107, int i108, int i109, int i110, 
			int i111, int i112, int i113, int i114, int i115, int i116, int i117, int i118, int i119, int i120, 
			int i121, int i122, int i123, int i124, int i125, int i126, int i127, int i128, int i129, int i130, 
			int i131, int i132, int i133, int i134, int i135, int i136, int i137, int i138, int i139, int i140, 
			int i141, int i142, int i143, int i144, int i145, int i146, int i147, int i148, int i149, int i150, 
			int i151, int i152, int i153, int i154, int i155, int i156, int i157, int i158, int i159, int i160, 
			int i161, int i162, int i163, int i164, int i165, int i166, int i167, int i168, int i169, int i170, 
			int i171, int i172, int i173, int i174, int i175, int i176, int i177, int i178, int i179, int i180, 
			int i181, int i182, int i183, int i184, int i185, int i186, int i187, int i188, int i189, int i190, 
			int i191, int i192, int i193, int i194, int i195, int i196, int i197, int i198, int i199, int i200, 
			int i201, int i202, int i203, int i204, int i205, int i206, int i207, int i208, int i209, int i210, 
			int i211, int i212, int i213, int i214, int i215, int i216, int i217, int i218, int i219, int i220, 
			int i221, int i222, int i223, int i224, int i225, int i226, int i227, int i228, int i229, int i230, 
			int i231, int i232, int i233, int i234, int i235, int i236, int i237, int i238, int i239, int i240, 
			int i241, int i242, int i243, int i244, int i245, int i246, int i247, int i248, int i249, int i250, 
			int i251, int i252) {}
	private void helperSpreaderMethod_253arg(
			int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, 
			int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19, int i20, 
			int i21, int i22, int i23, int i24, int i25, int i26, int i27, int i28, int i29, int i30, 
			int i31, int i32, int i33, int i34, int i35, int i36, int i37, int i38, int i39, int i40, 
			int i41, int i42, int i43, int i44, int i45, int i46, int i47, int i48, int i49, int i50, 
			int i51, int i52, int i53, int i54, int i55, int i56, int i57, int i58, int i59, int i60, 
			int i61, int i62, int i63, int i64, int i65, int i66, int i67, int i68, int i69, int i70, 
			int i71, int i72, int i73, int i74, int i75, int i76, int i77, int i78, int i79, int i80, 
			int i81, int i82, int i83, int i84, int i85, int i86, int i87, int i88, int i89, int i90, 
			int i91, int i92, int i93, int i94, int i95, int i96, int i97, int i98, int i99, int i100, 
			int i101, int i102, int i103, int i104, int i105, int i106, int i107, int i108, int i109, int i110, 
			int i111, int i112, int i113, int i114, int i115, int i116, int i117, int i118, int i119, int i120, 
			int i121, int i122, int i123, int i124, int i125, int i126, int i127, int i128, int i129, int i130, 
			int i131, int i132, int i133, int i134, int i135, int i136, int i137, int i138, int i139, int i140, 
			int i141, int i142, int i143, int i144, int i145, int i146, int i147, int i148, int i149, int i150, 
			int i151, int i152, int i153, int i154, int i155, int i156, int i157, int i158, int i159, int i160, 
			int i161, int i162, int i163, int i164, int i165, int i166, int i167, int i168, int i169, int i170, 
			int i171, int i172, int i173, int i174, int i175, int i176, int i177, int i178, int i179, int i180, 
			int i181, int i182, int i183, int i184, int i185, int i186, int i187, int i188, int i189, int i190, 
			int i191, int i192, int i193, int i194, int i195, int i196, int i197, int i198, int i199, int i200, 
			int i201, int i202, int i203, int i204, int i205, int i206, int i207, int i208, int i209, int i210, 
			int i211, int i212, int i213, int i214, int i215, int i216, int i217, int i218, int i219, int i220, 
			int i221, int i222, int i223, int i224, int i225, int i226, int i227, int i228, int i229, int i230, 
			int i231, int i232, int i233, int i234, int i235, int i236, int i237, int i238, int i239, int i240, 
			int i241, int i242, int i243, int i244, int i245, int i246, int i247, int i248, int i249, int i250, 
			int i251, int i252, int i253) {}
	private void helperSpreaderMethod_254arg(
    		int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, 
    		int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19, int i20, 
    		int i21, int i22, int i23, int i24, int i25, int i26, int i27, int i28, int i29, int i30, 
    		int i31, int i32, int i33, int i34, int i35, int i36, int i37, int i38, int i39, int i40, 
    		int i41, int i42, int i43, int i44, int i45, int i46, int i47, int i48, int i49, int i50, 
    		int i51, int i52, int i53, int i54, int i55, int i56, int i57, int i58, int i59, int i60, 
    		int i61, int i62, int i63, int i64, int i65, int i66, int i67, int i68, int i69, int i70, 
    		int i71, int i72, int i73, int i74, int i75, int i76, int i77, int i78, int i79, int i80,
    		int i81, int i82, int i83, int i84, int i85, int i86, int i87, int i88, int i89, int i90, 
    		int i91, int i92, int i93, int i94, int i95, int i96, int i97, int i98, int i99, int i100, 
    		int i101, int i102, int i103, int i104, int i105, int i106, int i107, int i108, int i109, int i110, 
    		int i111, int i112, int i113, int i114, int i115, int i116, int i117, int i118, int i119, int i120, 
    		int i121, int i122, int i123, int i124, int i125, int i126, int i127, int i128, int i129, int i130, 
    		int i131, int i132, int i133, int i134, int i135, int i136, int i137, int i138, int i139, int i140, 
    		int i141, int i142, int i143, int i144, int i145, int i146, int i147, int i148, int i149, int i150, 
    		int i151, int i152, int i153, int i154, int i155, int i156, int i157, int i158, int i159, int i160, 
    		int i161, int i162, int i163, int i164, int i165, int i166, int i167, int i168, int i169, int i170, 
    		int i171, int i172, int i173, int i174, int i175, int i176, int i177, int i178, int i179, int i180, 
    		int i181, int i182, int i183, int i184, int i185, int i186, int i187, int i188, int i189, int i190, 
    		int i191, int i192, int i193, int i194, int i195, int i196, int i197, int i198, int i199, int i200, 
    		int i201, int i202, int i203, int i204, int i205, int i206, int i207, int i208, int i209, int i210, 
    		int i211, int i212, int i213, int i214, int i215, int i216, int i217, int i218, int i219, int i220, 
    		int i221, int i222, int i223, int i224, int i225, int i226, int i227, int i228, int i229, int i230, 
    		int i231, int i232, int i233, int i234, int i235, int i236, int i237, int i238, int i239, int i240, 
    		int i241, int i242, int i243, int i244, int i245, int i246, int i247, int i248, int i249, int i250, 
    		int i251, int i252, int i253, int i254) {}
    
    private static void helperSpreaderStaticMethod_253arg(
    		int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, 
    		int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19, int i20, 
    		int i21, int i22, int i23, int i24, int i25, int i26, int i27, int i28, int i29, int i30, 
    		int i31, int i32, int i33, int i34, int i35, int i36, int i37, int i38, int i39, int i40, 
    		int i41, int i42, int i43, int i44, int i45, int i46, int i47, int i48, int i49, int i50, 
    		int i51, int i52, int i53, int i54, int i55, int i56, int i57, int i58, int i59, int i60, 
    		int i61, int i62, int i63, int i64, int i65, int i66, int i67, int i68, int i69, int i70, 
    		int i71, int i72, int i73, int i74, int i75, int i76, int i77, int i78, int i79, int i80, 
    		int i81, int i82, int i83, int i84, int i85, int i86, int i87, int i88, int i89, int i90, 
    		int i91, int i92, int i93, int i94, int i95, int i96, int i97, int i98, int i99, int i100, 
    		int i101, int i102, int i103, int i104, int i105, int i106, int i107, int i108, int i109, int i110, 
    		int i111, int i112, int i113, int i114, int i115, int i116, int i117, int i118, int i119, int i120, 
    		int i121, int i122, int i123, int i124, int i125, int i126, int i127, int i128, int i129, int i130, 
    		int i131, int i132, int i133, int i134, int i135, int i136, int i137, int i138, int i139, int i140, 
    		int i141, int i142, int i143, int i144, int i145, int i146, int i147, int i148, int i149, int i150, 
    		int i151, int i152, int i153, int i154, int i155, int i156, int i157, int i158, int i159, int i160, 
    		int i161, int i162, int i163, int i164, int i165, int i166, int i167, int i168, int i169, int i170, 
    		int i171, int i172, int i173, int i174, int i175, int i176, int i177, int i178, int i179, int i180, 
    		int i181, int i182, int i183, int i184, int i185, int i186, int i187, int i188, int i189, int i190, 
    		int i191, int i192, int i193, int i194, int i195, int i196, int i197, int i198, int i199, int i200, 
    		int i201, int i202, int i203, int i204, int i205, int i206, int i207, int i208, int i209, int i210, 
    		int i211, int i212, int i213, int i214, int i215, int i216, int i217, int i218, int i219, int i220, 
    		int i221, int i222, int i223, int i224, int i225, int i226, int i227, int i228, int i229, int i230, 
    		int i231, int i232, int i233, int i234, int i235, int i236, int i237, int i238, int i239, int i240, 
    		int i241, int i242, int i243, int i244, int i245, int i246, int i247, int i248, int i249, int i250, 
    		int i251, int i252, int i253) {}
    private static void helperSpreaderStaticMethod_254arg(
    		int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, 
    		int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19, int i20, 
    		int i21, int i22, int i23, int i24, int i25, int i26, int i27, int i28, int i29, int i30,
    		int i31, int i32, int i33, int i34, int i35, int i36, int i37, int i38, int i39, int i40, 
    		int i41, int i42, int i43, int i44, int i45, int i46, int i47, int i48, int i49, int i50, 
    		int i51, int i52, int i53, int i54, int i55, int i56, int i57, int i58, int i59, int i60, 
    		int i61, int i62, int i63, int i64, int i65, int i66, int i67, int i68, int i69, int i70, 
    		int i71, int i72, int i73, int i74, int i75, int i76, int i77, int i78, int i79, int i80, 
    		int i81, int i82, int i83, int i84, int i85, int i86, int i87, int i88, int i89, int i90, 
    		int i91, int i92, int i93, int i94, int i95, int i96, int i97, int i98, int i99, int i100, 
    		int i101, int i102, int i103, int i104, int i105, int i106, int i107, int i108, int i109, int i110, 
    		int i111, int i112, int i113, int i114, int i115, int i116, int i117, int i118, int i119, int i120, 
    		int i121, int i122, int i123, int i124, int i125, int i126, int i127, int i128, int i129, int i130, 
    		int i131, int i132, int i133, int i134, int i135, int i136, int i137, int i138, int i139, int i140, 
    		int i141, int i142, int i143, int i144, int i145, int i146, int i147, int i148, int i149, int i150, 
    		int i151, int i152, int i153, int i154, int i155, int i156, int i157, int i158, int i159, int i160, 
    		int i161, int i162, int i163, int i164, int i165, int i166, int i167, int i168, int i169, int i170, 
    		int i171, int i172, int i173, int i174, int i175, int i176, int i177, int i178, int i179, int i180, 
    		int i181, int i182, int i183, int i184, int i185, int i186, int i187, int i188, int i189, int i190, 
    		int i191, int i192, int i193, int i194, int i195, int i196, int i197, int i198, int i199, int i200, 
    		int i201, int i202, int i203, int i204, int i205, int i206, int i207, int i208, int i209, int i210, 
    		int i211, int i212, int i213, int i214, int i215, int i216, int i217, int i218, int i219, int i220, 
    		int i221, int i222, int i223, int i224, int i225, int i226, int i227, int i228, int i229, int i230, 
    		int i231, int i232, int i233, int i234, int i235, int i236, int i237, int i238, int i239, int i240, 
    		int i241, int i242, int i243, int i244, int i245, int i246, int i247, int i248, int i249, int i250, 
    		int i251, int i252, int i253, int i254) {}
    private static void helperSpreaderStaticMethod_255arg(
    		int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, 
    		int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19, int i20, 
    		int i21, int i22, int i23, int i24, int i25, int i26, int i27, int i28, int i29, int i30, 
    		int i31, int i32, int i33, int i34, int i35, int i36, int i37, int i38, int i39, int i40, 
    		int i41, int i42, int i43, int i44, int i45, int i46, int i47, int i48, int i49, int i50, 
    		int i51, int i52, int i53, int i54, int i55, int i56, int i57, int i58, int i59, int i60, 
    		int i61, int i62, int i63, int i64, int i65, int i66, int i67, int i68, int i69, int i70, 
    		int i71, int i72, int i73, int i74, int i75, int i76, int i77, int i78, int i79, int i80, 
    		int i81, int i82, int i83, int i84, int i85, int i86, int i87, int i88, int i89, int i90, 
    		int i91, int i92, int i93, int i94, int i95, int i96, int i97, int i98, int i99, int i100, 
    		int i101, int i102, int i103, int i104, int i105, int i106, int i107, int i108, int i109, int i110, 
    		int i111, int i112, int i113, int i114, int i115, int i116, int i117, int i118, int i119, int i120, 
    		int i121, int i122, int i123, int i124, int i125, int i126, int i127, int i128, int i129, int i130, 
    		int i131, int i132, int i133, int i134, int i135, int i136, int i137, int i138, int i139, int i140, 
    		int i141, int i142, int i143, int i144, int i145, int i146, int i147, int i148, int i149, int i150, 
    		int i151, int i152, int i153, int i154, int i155, int i156, int i157, int i158, int i159, int i160, 
    		int i161, int i162, int i163, int i164, int i165, int i166, int i167, int i168, int i169, int i170, 
    		int i171, int i172, int i173, int i174, int i175, int i176, int i177, int i178, int i179, int i180, 
    		int i181, int i182, int i183, int i184, int i185, int i186, int i187, int i188, int i189, int i190, 
    		int i191, int i192, int i193, int i194, int i195, int i196, int i197, int i198, int i199, int i200, 
    		int i201, int i202, int i203, int i204, int i205, int i206, int i207, int i208, int i209, int i210, 
    		int i211, int i212, int i213, int i214, int i215, int i216, int i217, int i218, int i219, int i220, 
    		int i221, int i222, int i223, int i224, int i225, int i226, int i227, int i228, int i229, int i230, 
    		int i231, int i232, int i233, int i234, int i235, int i236, int i237, int i238, int i239, int i240, 
    		int i241, int i242, int i243, int i244, int i245, int i246, int i247, int i248, int i249, int i250, 
    		int i251, int i252, int i253, int i254, int i255) {}
    private void helperCollectorMethod(int[] i) {}
    private static void helperStaticCollectorMethod(int[] i) {}
    private void helperVarArgsMethod(String s, int... i) {}
}
