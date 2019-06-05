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
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandleProxies;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import examples.CrossPackageSingleMethodInterfaceExample;
import examples.PackageExamples;

/**
 * @author mesbah
 * This class contains tests for the MethodHandleProxies API. 
 */
public class MethodHandleProxiesTest {
	
	/********************************
	 * Tests for asInterfaceInstance
	 ********************************/
	
	/**
	 * Tests asInterfaceInstance using a public single method interface from the same package and followed by one from a different package.
	 * Wrapper is a public virtual method from same package and cross-package respectively. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asInterfaceInstance_VirtualMethod() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findVirtual( SamePackageExample.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		mh = mh.bindTo( new SamePackageExample() );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		AssertJUnit.assertEquals( 3, interfaceInstance.singleMethodAdd( 1, 2 ) );
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findVirtual( PackageExamples.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		mh2 = mh2.bindTo( new PackageExamples() );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		AssertJUnit.assertEquals( 3, interfaceInstanceCrossPackage.singleMethodAdd( 1, 2 ) );
	}
	
	/**
	 * Tests asInterfaceInstance using a public single method interface from the same package and followed by one from a different package.
	 * Wrapper is a public static method from same package and cross-package respectively. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asInterfaceInstance_StaticMethod() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findStatic( SamePackageExample.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		AssertJUnit.assertEquals( 3, interfaceInstance.singleMethodAdd( 1, 2 ) );
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findStatic( PackageExamples.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		AssertJUnit.assertEquals( 3, interfaceInstanceCrossPackage.singleMethodAdd( 1, 2 ) );
	}
	
	/**
	 * Tests asInterfaceInstance using a public single method interface from the same package and followed by one from a different package.
	 * Wrapper is a public virtual method defined in an inner class from same package and cross-package respectively. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_asInterfaceInstance_InnerClass() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findVirtual( SamePackageExample.SamePackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample x = new SamePackageExample();
		SamePackageExample.SamePackageInnerClass i = x.new SamePackageInnerClass();
		mh = mh.bindTo( i );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		AssertJUnit.assertEquals( 3, interfaceInstance.singleMethodAdd( 1, 2 ) );
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findVirtual( PackageExamples.CrossPackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		PackageExamples y = new PackageExamples();
		mh2 = mh2.bindTo( y.new CrossPackageInnerClass() );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		AssertJUnit.assertEquals( 3, interfaceInstanceCrossPackage.singleMethodAdd( 1, 2 ) );
	}
	
	
	/******************************
	 *Tests for isWrapperInstance
	 ******************************/
	
	/**
	 * Tests isWrapperInstance using a public single method interface from the same package and followed by one from a different package.
	 * Wrapper is a public virtual method defined in an inner class from same package and cross-package respectively. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isWrapperInstance_VirtualMethod() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findVirtual( SamePackageExample.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		mh = mh.bindTo( new SamePackageExample() );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		AssertJUnit.assertTrue( MethodHandleProxies.isWrapperInstance( interfaceInstance ) );
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findVirtual( PackageExamples.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		mh2 = mh2.bindTo( new PackageExamples() );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		AssertJUnit.assertTrue( MethodHandleProxies.isWrapperInstance( interfaceInstanceCrossPackage ) );
	}
	
	/**
	 * Tests isWrapperInstance using a public single method interface from the same package and followed by one from a different package.
	 * Wrapper is a public static method from same package and cross-package respectively. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isWrapperInstance_StaticMethod() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findStatic( SamePackageExample.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		AssertJUnit.assertTrue( MethodHandleProxies.isWrapperInstance( interfaceInstance ) );
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findStatic( PackageExamples.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		AssertJUnit.assertTrue( MethodHandleProxies.isWrapperInstance( interfaceInstanceCrossPackage ) );
	}
	
	/**
	 * Tests isWrapperInstance using a public single method interface from the same package and followed by one from a different package.
	 * Wrapper is a public virtual method defined in an inner class from same package and cross-package respectively. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isWrapperInstance_InnerClass() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findVirtual( SamePackageExample.SamePackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample x = new SamePackageExample();
		SamePackageExample.SamePackageInnerClass i = x.new SamePackageInnerClass();
		mh = mh.bindTo( i );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		AssertJUnit.assertTrue( MethodHandleProxies.isWrapperInstance( interfaceInstance ) );
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findVirtual( PackageExamples.CrossPackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		PackageExamples y = new PackageExamples();
		mh2 = mh2.bindTo( y.new CrossPackageInnerClass() );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		AssertJUnit.assertTrue( MethodHandleProxies.isWrapperInstance( interfaceInstanceCrossPackage ) );
	}
	
	/**
	 * Negative test : verify isWrapperInstance returns false for non-wrapper objects. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_isWrapperInstance_NegativeTest() throws Throwable {
		//same package test
		AssertJUnit.assertFalse( MethodHandleProxies.isWrapperInstance( new SamePackageSingleMethodInterfaceExample() {
			
			@Override
			public int singleMethodAdd( int a, int b ) {
				return 0;
			}
		} ) );
		
		//cross package test
		AssertJUnit.assertFalse( MethodHandleProxies.isWrapperInstance( new PackageExamples() ) );
	}
	
	/****************************************
	 * Tests for wrapperInstanceTarget
	 * *************************************/
	
	/**
	 * Tests wrapperInstanceTarget using single method interface and wrapper types from same package and cross-package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_wrapperInstanceTarget_VirtualMethod() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findVirtual( SamePackageExample.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		mh = mh.bindTo( new SamePackageExample() );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		MethodHandle retrievedTarget = MethodHandleProxies.wrapperInstanceTarget( interfaceInstance );
		AssertJUnit.assertEquals( 4, ( int )retrievedTarget.invokeExact( 1, 3 ) ); 
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findVirtual( PackageExamples.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		mh2 = mh2.bindTo( new PackageExamples() );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		MethodHandle retrievedTarget2 = MethodHandleProxies.wrapperInstanceTarget( interfaceInstanceCrossPackage );
		AssertJUnit.assertEquals( 4, retrievedTarget2.invoke( 1, 3 ) );
	}
	
	/**
	 * Tests wrapperInstanceTarget using single method interface and static wrapper methods from same package and cross-package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_wrapperInstanceTarget_StaticMethod() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findStatic( SamePackageExample.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		MethodHandle retrievedTarget = MethodHandleProxies.wrapperInstanceTarget( interfaceInstance );
		AssertJUnit.assertEquals( 4, retrievedTarget.invoke( 1, 3 ) );
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findStatic( PackageExamples.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		MethodHandle retrievedTarget2 = MethodHandleProxies.wrapperInstanceTarget( interfaceInstanceCrossPackage );
		AssertJUnit.assertEquals( 4, ( int )retrievedTarget2.invokeExact( 1, 3 ) );
	}

	/**
	 * Tests wrapperInstanceTarget using single method interface and inner-class wrapper types from same package and cross-package 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_wrapperInstanceTarget_InnerClass() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findVirtual( SamePackageExample.SamePackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample x = new SamePackageExample();
		SamePackageExample.SamePackageInnerClass i = x.new SamePackageInnerClass();
		mh = mh.bindTo( i );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		MethodHandle retrievedTarget = MethodHandleProxies.wrapperInstanceTarget( interfaceInstance );
		AssertJUnit.assertEquals( 4, ( int )retrievedTarget.invokeExact( 1, 3 ) );
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findVirtual( PackageExamples.CrossPackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		PackageExamples y = new PackageExamples();
		mh2 = mh2.bindTo( y.new CrossPackageInnerClass() );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		MethodHandle retrievedTarget2 = MethodHandleProxies.wrapperInstanceTarget( interfaceInstanceCrossPackage );
		AssertJUnit.assertEquals( 4, retrievedTarget2.invoke( 1, 3 ) );
	}
	
	/**
	 * Negative test for verifying target retrieval failure for wrapperInstanceTarget method in case of non-wrapper instances.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_wrapperInstanceTarget_NegativeTest() throws Throwable {
		boolean notAWrapperInstance = false; 
		try {
			MethodHandle retrievedTarget = MethodHandleProxies.wrapperInstanceTarget( new SamePackageExample() );
			AssertJUnit.assertEquals( 4, ( int )retrievedTarget.invokeExact( 1, 3 ) );
		}
		catch( IllegalArgumentException e ) {
			notAWrapperInstance = true;
		}
		
		AssertJUnit.assertTrue( notAWrapperInstance );
	}
	
	/**********************************
	 * Tests for wrapperInstanceType
	 * *******************************/
	
	/**
	 * Tests wrapperInstanceType using public virtual wrappers and single method interfaces from same package and cross-package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_wrapperInstanceType_VirtualMethod() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findVirtual( SamePackageExample.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		mh = mh.bindTo( new SamePackageExample() );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		Class<?> retrievedTargetType = MethodHandleProxies.wrapperInstanceType( interfaceInstance );
		AssertJUnit.assertEquals( SamePackageSingleMethodInterfaceExample.class, retrievedTargetType ); 
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findVirtual( PackageExamples.class, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		mh2 = mh2.bindTo( new PackageExamples() );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		Class<?> retrievedTargetType2 = MethodHandleProxies.wrapperInstanceType( interfaceInstanceCrossPackage );
		AssertJUnit.assertEquals( CrossPackageSingleMethodInterfaceExample.class, retrievedTargetType2 ); 
	}
	
	/**
	 * Tests wrapperInstanceType using public static wrappers and single method interfaces from same package and cross-package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_wrapperInstanceType_StaticMethod() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findStatic( SamePackageExample.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		Class<?> retrievedTargetType = MethodHandleProxies.wrapperInstanceType( interfaceInstance );
		AssertJUnit.assertEquals( SamePackageSingleMethodInterfaceExample.class, retrievedTargetType ); 
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findStatic( PackageExamples.class, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		Class<?> retrievedTargetType2 = MethodHandleProxies.wrapperInstanceType( interfaceInstanceCrossPackage );
		AssertJUnit.assertEquals( CrossPackageSingleMethodInterfaceExample.class, retrievedTargetType2 ); 
	}
	
	/**
	 * Tests wrapperInstanceType using inner-class wrappers and single method interfaces from same package and cross-package.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_wrapperInstanceType_InnerClass() throws Throwable {
		//same package test
		MethodHandle mh = MethodHandles.lookup().findVirtual( SamePackageExample.SamePackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		SamePackageExample x = new SamePackageExample();
		SamePackageExample.SamePackageInnerClass i = x.new SamePackageInnerClass();
		mh = mh.bindTo( i );
		SamePackageSingleMethodInterfaceExample interfaceInstance = MethodHandleProxies.asInterfaceInstance( SamePackageSingleMethodInterfaceExample.class, mh );
		Class<?> retrievedTargetType = MethodHandleProxies.wrapperInstanceType( interfaceInstance );
		AssertJUnit.assertEquals( SamePackageSingleMethodInterfaceExample.class, retrievedTargetType ); 
		
		//cross package test
		MethodHandle mh2 = MethodHandles.lookup().findVirtual( PackageExamples.CrossPackageInnerClass.class, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		PackageExamples y = new PackageExamples();
		mh2 = mh2.bindTo( y.new CrossPackageInnerClass() );
		CrossPackageSingleMethodInterfaceExample interfaceInstanceCrossPackage = MethodHandleProxies.asInterfaceInstance( CrossPackageSingleMethodInterfaceExample.class, mh2 );
		Class<?> retrievedTargetType2 = MethodHandleProxies.wrapperInstanceType( interfaceInstanceCrossPackage );
		AssertJUnit.assertEquals( CrossPackageSingleMethodInterfaceExample.class, retrievedTargetType2 ); 
	}
	
	/**
	 * Negative test for wrapperInstanceType using non-wrapper types
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_wrapperInstanceType_NegativeTest() throws Throwable {
        boolean notAWrapperInstance = false; 
		try {
			Class<?> retrievedTargetType = MethodHandleProxies.wrapperInstanceType( new PackageExamples() );
			AssertJUnit.assertEquals( SamePackageSingleMethodInterfaceExample.class, retrievedTargetType );
		}
		catch( IllegalArgumentException e ) {
			notAWrapperInstance = true;
		}
		AssertJUnit.assertTrue( notAWrapperInstance );
	}
}
