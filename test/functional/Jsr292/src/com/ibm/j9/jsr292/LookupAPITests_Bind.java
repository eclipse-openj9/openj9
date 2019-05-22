package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import static java.lang.invoke.MethodType.methodType;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.util.NavigableMap;
import java.util.TreeMap;

import examples.PackageExamples;

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
public class LookupAPITests_Bind {

	/*public methods ( same package )*/

	/**
	 * Lookup.bind() test using a public method of a receiver class which is in the same package as the caller. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Public_SamePackage() throws Throwable {
		/*Call the public method of the receiver*/
		SamePackageExample spe = new SamePackageExample();
		MethodHandle example = MethodHandles.lookup().bind( spe, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		
		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( 3, s );
				
		s = 0 ; 
		
		s = (int) example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals ( 3, s);
	}

	/**
	 * Lookup.bind() test using public methods of the receiver and its parent where both are in the same package as the caller.  
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Public_SamePackage_SuperclassLookup() throws Throwable {
		/*Call the overridden addPublic method of child(receiver) */
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();
		MethodHandle example = MethodHandles.lookup().bind( g, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		
		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( s, 4 );

		/*Call the addPublicSuper method implemented by parent of the receiver*/
		example = MethodHandles.lookup().bind( g, "addPublic_Super", MethodType.methodType( int.class, int.class, int.class ) );
		s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 8, s );
	}

	/**
	 * Lookup.bind() test using a public method of an inner class as the receiver belonging to the same package as the caller.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Public_SamePackage_InnerClass() throws Throwable {
		SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
		MethodHandle example = MethodHandles.lookup().bind( g, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		
		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( 3, s );
		
		s = 0 ; 
		
		s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}


	/**
	 * Lookup.bind() test using public method of an inner classes (level 2 deep) as the receiver where the 
	 * receiver is in the same package as the caller. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Public_SamePackage_InnerClass_Level2() throws Throwable {
		SamePackageExample.SamePackageInnerClass.SamePackageInnerClass_Nested_Level2 g = ((new SamePackageExample()).new SamePackageInnerClass()).new SamePackageInnerClass_Nested_Level2();
		MethodHandle example = MethodHandles.lookup().bind( g, "addPublicInner_Level2", MethodType.methodType( int.class, int.class, int.class ) );
		
		int o = (int)example.invoke( 5, 0 );
		AssertJUnit.assertEquals( 5, o );
		
		o = 0 ; 
		
		o = (int)example.invokeExact( 5, 0 );
		AssertJUnit.assertEquals( 5, o );
	}

	/**
	 * Negative test : Lookup.bind() test using a public static method of a receiver class which is in the same package as the caller. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Static_Public_SamePackage() throws Throwable {
		boolean iaeThrown = false; 
		try {
			SamePackageExample spe = new SamePackageExample();
			MethodHandles.lookup().bind( spe, "addPublicStatic", MethodType.methodType( int.class, int.class, int.class ) );
		} catch ( IllegalAccessException e ) {
			iaeThrown = true;
		}
		
		AssertJUnit.assertTrue ( iaeThrown ); 
	}
	
	/*Blocked by CMVC : 192860*/
	/**
	 * Lookup.bind() test with MethodHandle.invoke() where we initially obtain a method handle by binding a virtual method of 
	 * a receiver class that is in the same package as the caller and then bind the method handle to MethodHandle.invoke(..).
	 * @throws Throwable
	 */
	@Test(enabled = false, groups = { "level.extended"})
	public void test_Bind_Invoke() throws Throwable {
		SamePackageExample spe = new SamePackageExample();
		MethodHandle example = MethodHandles.lookup().bind( spe, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		MethodHandle bindHandle = MethodHandles.lookup().bind(example, "invoke", MethodType.methodType(int.class,int.class,int.class) );
		
		int result = (int)bindHandle.invoke( 1, 2 );
		AssertJUnit.assertEquals ( 3 , result );
	}
	
	/*Blocked by CMVC : 192860*/
	/**
	 * Lookup.bind() test with MethodHandle.invoke() where we obtain a method handle for String.toUpperCase() method and create 
	 * a second method handle by binding MethodHandle.invoke() method where the receiver is the first method handle object. 
	 * @throws Throwable
	 */
	@Test(enabled = false, groups = { "level.extended"})
	public void test_Bind_Invoke_2() throws Throwable {
		MethodHandle toUpperCase = MethodHandles.lookup().findVirtual( String.class, "toUpperCase", methodType(String.class) );
		MethodHandle bindHandle = MethodHandles.lookup().bind( toUpperCase, "invoke", methodType(String.class,String.class) );
		String result = (String)bindHandle.invoke( "abc");
		AssertJUnit.assertEquals ( "ABC", result );
	}
	
	/*Blocked by CMVC : 192860*/
	/**
	 * Lookup.bind() test with MethodHandle.invokeExact() where we initially obtain a method handle by binding a virtual method of 
	 * a receiver class that is in the same package as the caller and then bind the method handle to MethodHandle.invokeExact(..).
	 * @throws Throwable
	 */
	@Test(enabled = false, groups = { "level.extended"})
	public void test_Bind_InvokeExact() throws Throwable {
		SamePackageExample spe = new SamePackageExample();
		MethodHandle example = MethodHandles.lookup().bind( spe, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		MethodHandle bindHandle = MethodHandles.lookup().bind(example, "invokeExact", MethodType.methodType(int.class,int.class,int.class) );
		
		int result = (int)bindHandle.invokeExact( 1, 2 );
		AssertJUnit.assertEquals ( 3 , result );
	}
	
	/*Blocked by CMVC : 192860*/
	/**
	 * Lookup.bind() test with MethodHandle.invokeExact() where we obtain a method handle for String.concat() method and create 
	 * a second method handle by binding MethodHandle.invokeExact() method where the receiver is the first method handle object. 
	 */
	@Test(enabled = false, groups = { "level.extended"})
	public void test_Bind_InvokeExact_2() throws Throwable {
		MethodHandle concat = MethodHandles.lookup().findVirtual( String.class, "concat", methodType(String.class,String.class) );
		MethodHandle bindHandle = MethodHandles.lookup().bind( concat, "invoke", methodType(String.class,String.class,String.class) );

		String result = (String)bindHandle.invokeExact( "ab", "ba" );
		AssertJUnit.assertEquals ( "abba", result );
	}
	
	/*Protected methods ( same package )*/

	/**
	 * Lookup.bind() test using a protected overridden method
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Protected_SamePackage_Overridden() throws Throwable {
		SamePackageExampleSubclass g = new SamePackageExampleSubclass();
		MethodHandle example = MethodHandles.lookup().bind( g, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );

		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( 13, s );
		
		s = 0 ; 
		
		s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 13, s );
	}

	/**
	 * Lookup.bind() test using a protected overridden method that belongs to a super-class of the receiver.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Protected_SamePackage_Overridden_SubclassLookup() throws Throwable {
		SamePackageExample g = new SamePackageExample();
		MethodHandle example = MethodHandles.lookup().bind( g, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );
		
		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( 3, s );
		
		s = 0 ; 
		
		s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}

	/*private methods ( same package )*/

	/**
	 * Negative test involving a private method being looked up 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Private_SamePackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;

		try {
			SamePackageExample g = new SamePackageExample();
			MethodHandles.lookup().bind( g, "addPrivate", MethodType.methodType( int.class, int.class, int.class ) );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}

		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}

	/*public methods ( cross package )*/

	/**
	 * Lookup.bind() test with a public method in a receiver class that belongs to a different package than the caller class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Public_CrossPackage() throws Throwable {
		PackageExamples g = new PackageExamples();
		MethodHandle example = MethodHandles.lookup().bind( g, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );
		
		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( 3, s );
		
		s = 0 ; 
		
		s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}

	/**
	 * Lookup.bind() test with a public overridden method in a sub-class(receiver) where the parent class resides in a different package.
	 * @throws Throwable
	 */			
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Public_CrossPackage_Overridden_SuperclassLookup() throws Throwable {
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();
		MethodHandle example = MethodHandles.lookup().bind( g, "addPublic", MethodType.methodType( int.class, int.class, int.class ) );

		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( 5, s );
		
		s = 0 ; 
		
		s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 5, s );
	}
	
	/**
	 * Lookup.bind() test using a public method of an inner class(receiver) belonging to a different package than the caller class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Public_CrossPackage_InnerClass() throws Throwable {
		PackageExamples.CrossPackageInnerClass g = (new PackageExamples()).new CrossPackageInnerClass();
		MethodHandle example = MethodHandles.lookup().bind( g, "addPublicInner", MethodType.methodType( int.class, int.class, int.class ) );
		
		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( 3, s ) ;
		
		s = 0 ; 
		
		s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 3, s ) ;
	}
	
	/**
	 * Lookup.bind() test using a public method of a level 2 inner class(receiver) belonging to a different package than the caller class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Public_CrossPackage_Overridden_InnerClass_Nested_Level2() throws Throwable {
		PackageExamples.CrossPackageInnerClass.CrossPackageInnerClass2_Nested_Level2 g = ((new PackageExamples()).new CrossPackageInnerClass()).new CrossPackageInnerClass2_Nested_Level2();
		MethodHandle example = MethodHandles.lookup().bind( g, "addPublicInner_level2", MethodType.methodType( int.class, int.class, int.class ) );
		
		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( 3, s );
		
		s = 9 ;
		
		s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 3, s );
	}

	/*protected methods ( cross package )*/
	/**
	 * Lookup.bind() test with a protected method overridden in a child class (receiver) whose parent is in a different package than the caller class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Protected_CrossPackage_Overridden() throws Throwable {
		CrossPackageExampleSubclass g = new CrossPackageExampleSubclass();
		MethodHandle example = MethodHandles.lookup().bind( g, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );

		int s = ( int )example.invoke( 1, 2 );
		AssertJUnit.assertEquals( -1, s );
		
		s = 0 ; 
		
		s = ( int )example.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( -1, s );
	}

	/**
	 * Negative test : Lookup.bind() test with a protected method belonging to a super-class(receiver) that is on a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Protected_CrossPackage_Overridden_SubclassLookup() throws Throwable {
		boolean iaeThrown = false; 
		try {
			PackageExamples g = new PackageExamples();
			MethodHandles.lookup().bind( g, "addProtected", MethodType.methodType( int.class, int.class, int.class ) );
		} catch ( IllegalAccessException e ) {
			iaeThrown = true;
		}
		
		AssertJUnit.assertTrue ( iaeThrown );
	}

	/*private methods ( cross package )*/

	/**
	 * Lookup.bind() test with a private method belonging to a different class than the lookup class
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Private_CrossPackage() throws Throwable {
		boolean illegalAccessExceptionThrown = false;

		try {
			PackageExamples g = new PackageExamples();
			MethodHandles.lookup().bind( g, "addPrivate", MethodType.methodType( int.class, int.class, int.class ) );
		} catch( IllegalAccessException e ) {
			illegalAccessExceptionThrown = true;
		}

		AssertJUnit.assertTrue( illegalAccessExceptionThrown );
	}

	/**
	 * Lookup.bind() test using a public method of an inner class belonging to a different package than the lookup class.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_Virtual_Private_CrossPackage_InnerClass() throws Throwable {
		boolean illegalAccessExceptionThrown = false;

		try {
			SamePackageExample.SamePackageInnerClass g = (new SamePackageExample()).new SamePackageInnerClass();
			MethodHandles.lookup().bind( g, "addPrivateInner", MethodType.methodType( int.class, int.class, int.class ) );
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
	public void test_Bind_Virtual_TreeMapTest() throws Throwable {
		NavigableMap nm = new TreeMap();
		TreeMap t = new TreeMap();  
		Object o = "foobar";
		Object o2 = "foobar2";

		MethodHandle m = MethodHandles.lookup().bind( nm, "put", MethodType.fromMethodDescriptorString( "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", null ) );

		Object r = ( Object )m.invokeExact( o, o );
		AssertJUnit.assertNull( r );

		Object r2 = ( Object )m.invokeExact( o, o2 );
		AssertJUnit.assertEquals( o, r2 );
		
		Object r3 = ( Object )m.invoke( o, o );
		AssertJUnit.assertEquals( o2, r3 );

		Object r4 = ( Object )m.invoke( o, o2 );
		AssertJUnit.assertEquals( o, r4 );
	}
	
	/**
	 * Lookup.bind() test using a public method of a custom-loaded receiver class where the class loader used to load the 
	 * receiver is different than the caller. 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Bind_UnrelatedClassLoaders() throws Throwable {
		ParentCustomClassLoader unrelatedClassLoader = new ParentCustomClassLoader( LookupInTests.class.getClassLoader() );
		Object customLoadedClass = unrelatedClassLoader.loadClass( "com.ibm.j9.jsr292.CustomLoadedClass1" ).newInstance();
		
		MethodHandle mh = MethodHandles.lookup().bind( customLoadedClass, "add",  MethodType.methodType( int.class, int.class, int.class) );
		
		int s = ( int )mh.invoke( 1, 2 );
		AssertJUnit.assertEquals( 103, s );
		
		s = 0 ; 
		
		s = ( int )mh.invokeExact( 1, 2 );
		AssertJUnit.assertEquals( 103, s );
	}
}
