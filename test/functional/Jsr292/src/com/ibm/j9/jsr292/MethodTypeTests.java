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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.io.*;
import java.lang.invoke.MethodType;
import java.util.ArrayList;
import java.util.List;

import static java.lang.invoke.MethodType.*;

/**
 * @author mesbah
 * This class contains tests for the MethodType API. 
 */
public class MethodTypeTests {
	
	/**Test MethodType.appendParameterTypes( Class<?>... ptypesToInsert )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AppendParameterTypes_Default()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, byte.class, short.class );
		mType = mType.appendParameterTypes( String.class );
		AssertJUnit.assertEquals( mType.parameterCount(), 3 );
		List<Class<?>> pList = mType.parameterList();
		
		AssertJUnit.assertEquals( pList.get( 0 ), byte.class );
		AssertJUnit.assertEquals( pList.get( 1 ), short.class );
		AssertJUnit.assertEquals( pList.get( 2 ), String.class );
	}
	
	/**Test MethodType.appendParameterTypes( List<Class<?>> ptypesToInsert )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AppendParameterTypes_List()throws Throwable {
		MethodType mType = MethodType.methodType( short.class, int.class );
		
		List<Class<?>> ptypesToInsert = new ArrayList<Class<?>>();
		ptypesToInsert.add( double.class );
		ptypesToInsert.add( byte.class );
		
		ptypesToInsert.add( long.class );
		ptypesToInsert.add( short.class );
		
		ptypesToInsert.add( float.class );
		ptypesToInsert.add( String.class );
		ptypesToInsert.add( boolean.class );
		ptypesToInsert.add( char.class );
		ptypesToInsert.add( Object.class );
		
		mType = mType.appendParameterTypes( ptypesToInsert );
		
		AssertJUnit.assertEquals( mType.parameterList().get( 0 ), int.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 1 ), double.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 2 ), byte.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 5 ), float.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 8 ), char.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 9 ), Object.class );
	}
	
	/**Test MethodType.changeParameterType( int num, Class<?> nptype )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_ChangeParameterType()throws Throwable {
		MethodType mType = MethodType.methodType( boolean.class, int.class, float.class, double.class, byte.class, char.class, short.class, long.class, boolean.class );
		mType = mType.changeParameterType( 0, String.class );
		mType = mType.changeParameterType( 2, long.class );
		mType = mType.changeParameterType( 4, boolean.class );
		
		AssertJUnit.assertEquals( mType.parameterList().get( 1 ), float.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 2 ), long.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 4 ), boolean.class );
	}
	
	/**Test MethodType.changeReturnType( Class<?> nrtype )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_ChangeReturnType()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, String.class );

		mType = mType.changeReturnType( String.class );
		AssertJUnit.assertEquals( mType.returnType(), String.class );
		
		mType = mType.changeReturnType( byte.class );
		AssertJUnit.assertEquals( mType.returnType(), byte.class );
		
		mType = mType.changeReturnType( float.class );
		AssertJUnit.assertEquals( mType.returnType(), float.class );
		
		mType = mType.changeReturnType( char.class );
		AssertJUnit.assertEquals( mType.returnType(), char.class );
		
		mType = mType.changeReturnType( double.class );
		AssertJUnit.assertEquals( mType.returnType(), double.class );
		
		mType = mType.changeReturnType( long.class );
		AssertJUnit.assertEquals( mType.returnType(), long.class );
		
		mType = mType.changeReturnType( short.class );
		AssertJUnit.assertEquals( mType.returnType(), short.class );
		
		mType = mType.changeReturnType( int.class );
		AssertJUnit.assertEquals( mType.returnType(), int.class );
		
		mType = mType.changeReturnType( boolean.class );
		AssertJUnit.assertEquals( mType.returnType(), boolean.class );
	}
	
	/**Test MethodType.dropParameterTypes( int start, int end )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_DropParameterTypes()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, byte.class, Catch.class, double.class, char.class, int.class, float.class );
		
		mType = mType.dropParameterTypes( 0, 1 );
		AssertJUnit.assertEquals( mType.parameterCount(), 5 );
		AssertJUnit.assertEquals( mType.parameterArray()[0], Catch.class );
		
		mType = mType.dropParameterTypes( 1, 3 );
		AssertJUnit.assertEquals( mType.parameterList().get( 1 ), int.class );
		
		mType = mType.dropParameterTypes( 0, mType.parameterCount());
		AssertJUnit.assertEquals( mType.parameterCount(), 0 );
	}
	
	static void dropParameterTypesIOOBEHelper(MethodType mt, int start, int end) {
		boolean indexOutOfBounds = false;
		try {
			mt.dropParameterTypes(start, end);
		} catch(IndexOutOfBoundsException t) { 
			indexOutOfBounds = true;
		}
		AssertJUnit.assertTrue("drop("+ start + ", " + end +") should throw IOOBE", indexOutOfBounds);
	}
	
	static void dropParameterTypesNoOpHelper(MethodType mt, int start, int end) {
		MethodType drop = mt.dropParameterTypes(start, end);
		AssertJUnit.assertEquals("drop("+ start + ", " + end +") should return the original MT", mt, drop);
	}
	
	/**
	 * Test MethodType.dropParameterType(start, end) edge cases.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_DropParameterTypesEdgeCases() throws Throwable {
		MethodType mt0 = methodType(Object.class);
		dropParameterTypesNoOpHelper(mt0, 0, 0);
		dropParameterTypesIOOBEHelper(mt0, 1, 0);
		dropParameterTypesIOOBEHelper(mt0, -1, 0);
		dropParameterTypesIOOBEHelper(mt0, 0, 1);
		
		MethodType mt = methodType(Object.class, Object.class);
		dropParameterTypesNoOpHelper(mt, 0, 0);
		dropParameterTypesNoOpHelper(mt, 1, 1);
		dropParameterTypesIOOBEHelper(mt, 2, 2);
		dropParameterTypesIOOBEHelper(mt, 1, 2);
		
		MethodType mt1 = methodType(Object.class, Object.class);
		mt1 = mt1.dropParameterTypes(0, 1);
		AssertJUnit.assertEquals("drop(0, 1) should drop first parameter", mt1, methodType(Object.class));
	}
	
	/**Test MethodType.erase()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Erase()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, float.class, double.class, byte.class, char.class, short.class, long.class, boolean.class );
		AssertJUnit.assertEquals( mType.wrap().erase().toString(), "(Object,Object,Object,Object,Object,Object,Object)Object" );
		AssertJUnit.assertEquals( mType.erase().toString(), "(float,double,byte,char,short,long,boolean)int" );
	}
	
	/** Test MethodType.fromMethodDescriptorString( String methodDescriptor, ClassLoader loader )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FromMethodDescriptorString()throws Throwable {
		MethodType mType = MethodType.fromMethodDescriptorString( "(FDBCSJZ)I", null );
		
		AssertJUnit.assertEquals( mType.parameterCount(), 7 );
		AssertJUnit.assertEquals( mType.returnType(), int.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 0 ), float.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 6 ), boolean.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 5 ), long.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 1 ), double.class );
		
		mType = MethodType.fromMethodDescriptorString( "(Lcom/ibm/j9/jsr292/Catch;Ljava/lang/String;)Ljava/lang/Integer;", null );
		
		AssertJUnit.assertEquals( mType.returnType(), Integer.class );
		AssertJUnit.assertEquals( mType.parameterArray()[0], Catch.class );
		AssertJUnit.assertEquals( mType.parameterArray()[1], String.class );
		
		mType = MethodType.fromMethodDescriptorString( "([Lcom/ibm/j9/jsr292/Catch;[Ljava/lang/Double;)[I", null );
		
		AssertJUnit.assertEquals( mType.returnType(), int[].class );
		AssertJUnit.assertEquals( mType.parameterArray()[0], Catch[].class );
		AssertJUnit.assertEquals( mType.parameterArray()[1], Double[].class );
	}
	
	/**Test MethodType.generic() 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Generic()throws Throwable {  
		MethodType mType = MethodType.fromMethodDescriptorString( "(FDBCSJZ)I", null );
		AssertJUnit.assertEquals( mType.generic().toString(), "(Object,Object,Object,Object,Object,Object,Object)Object" );
		
		mType = MethodType.fromMethodDescriptorString( "([Lcom/ibm/j9/jsr292/Catch;[Ljava/lang/Double;)[I", null );
		AssertJUnit.assertEquals( mType.generic().toString(), "(Object,Object)Object" );
	}
 
	/**Test MethodType.genericMethodType( int objectArgCount )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GenericMethodType()throws Throwable {
		MethodType mType = MethodType.fromMethodDescriptorString( "(II)I", null );
		MethodType mType2 = MethodType.genericMethodType( 2 );
		AssertJUnit.assertEquals( mType.generic(), mType2 );
	}
	
	/**Test MethodType.genericMethodType( int objectArgCount, boolean finalArray )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_GenericMethodType_FinalArrayOption()throws Throwable {
		MethodType mType = MethodType.fromMethodDescriptorString( "(II)I", null );
		MethodType mType2 = MethodType.genericMethodType( 2, false );
		AssertJUnit.assertEquals( mType.generic(), mType2 );	
		
		mType2 = MethodType.genericMethodType( 2, true );
		mType = mType.generic().insertParameterTypes( 2, Object[].class );
		
		AssertJUnit.assertEquals( mType2.toString(), "(Object,Object,Object[])Object" );
		AssertJUnit.assertEquals( mType, mType2 );
	}
	
	/**Test MethodType.hasPrimitives()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_HasPrimitive()throws Throwable {
		MethodType mType = MethodType.fromMethodDescriptorString( "(II)I", null );
		AssertJUnit.assertTrue( mType.hasPrimitives());
		mType = mType.wrap();
		AssertJUnit.assertFalse( mType.hasPrimitives());
		
		AssertJUnit.assertTrue( MethodType.methodType( double.class, String.class ).hasPrimitives());
		AssertJUnit.assertTrue( MethodType.methodType( Integer.class, byte.class ).hasPrimitives());
		AssertJUnit.assertTrue( MethodType.methodType( Float.class, long.class ).hasPrimitives());
		AssertJUnit.assertTrue( MethodType.methodType( Catch.class, short.class ).hasPrimitives());
		AssertJUnit.assertTrue( MethodType.methodType( float.class ).hasPrimitives());
		AssertJUnit.assertTrue( MethodType.methodType( boolean.class ).hasPrimitives());
		AssertJUnit.assertTrue( MethodType.methodType( char.class, Float.class ).hasPrimitives());
	}
	 
	/**Test MethodType.hasWrappers()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_HasWrapper()throws Throwable {
		MethodType mType = MethodType.fromMethodDescriptorString( "(II)I", null );
		AssertJUnit.assertFalse( mType.hasWrappers());
		mType = mType.wrap();
		AssertJUnit.assertTrue( mType.hasWrappers());
		
		AssertJUnit.assertTrue( MethodType.methodType( Integer.class ).hasWrappers());
		AssertJUnit.assertTrue( MethodType.methodType( int.class, Double.class ).hasWrappers());
		AssertJUnit.assertTrue( MethodType.methodType( int.class, Float.class ).hasWrappers());
		AssertJUnit.assertTrue( MethodType.methodType( Boolean.class, int.class ).hasWrappers());
		AssertJUnit.assertTrue( MethodType.methodType( Character.class ).hasWrappers());
		AssertJUnit.assertTrue( MethodType.methodType( Long.class ).hasWrappers());
		AssertJUnit.assertTrue( MethodType.methodType( Short.class ).hasWrappers());
		AssertJUnit.assertTrue( MethodType.methodType( Byte.class, int.class ).hasWrappers());
}
	
	/**Test MethodType.insertParameterTypes( int num, Class<?>... ptypesToInsert )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_InsertParameterTypes()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, char.class, Integer.class );
		mType = mType.insertParameterTypes( 1, double.class );
		AssertJUnit.assertEquals( mType.parameterCount(), 3 );
		AssertJUnit.assertEquals( mType.parameterList().get( 1 ), double.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 2 ), Integer.class );
		
		mType = mType.insertParameterTypes( 2, Catch.class, org.testng.AssertJUnit.class );
		
		AssertJUnit.assertEquals( mType.parameterList().get( 2 ), Catch.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 3 ), org.testng.AssertJUnit.class );
	}
	
	/**Test MethodType.insertParameterTypes( int num, List<Class<?>> ptypesToInsert )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_InsertParameterTypes_List()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, float.class, String.class );
		List<Class<?>> p = new ArrayList<Class<?>>();
		p.add( boolean.class );
		p.add( boolean.class );
		mType = mType.insertParameterTypes( 1, p );

		AssertJUnit.assertEquals( mType.parameterCount(), 4 );
		AssertJUnit.assertEquals( mType.parameterList().get( 0 ), float.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 1 ), boolean.class );
		AssertJUnit.assertEquals( mType.parameterList().get( 3 ), String.class );
	}
	
	/**Test MethodType.methodType( Class<?> rtype )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_MethodType_Default()throws Throwable {
		MethodType mType = MethodType.methodType( int.class );
		AssertJUnit.assertEquals( mType.returnType(), int.class );
	}
	
	
	/**Test MethodType.methodType( Class<?> rtype, Class<?> ptype0 )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_MethodType_OneParam()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, float.class );
		AssertJUnit.assertEquals( mType.toString(), "(float)int" );
	}
	
	/**Test MethodType.methodType( Class<?> rtype, Class<?>[] ptypes )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_MethodType_TypeArray()throws Throwable {
		Class<?> [] p = new Class<?>[2];
		p[0] = char.class;
		p[1] = short.class;
		MethodType mType = MethodType.methodType( double.class, p );
		AssertJUnit.assertEquals( mType.toString(), "(char,short)double" );
	}
	
	/**Test MethodType.methodType( Class<?> rtype, Class<?> ptype0, Class<?>... ptypes )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_MethodType_Prepend()throws Throwable {
		MethodType mType = MethodType.methodType( float.class, byte.class, int.class, char.class );
		AssertJUnit.assertEquals( mType.toString(), "(byte,int,char)float" );
	}
	
	/**Test MethodType.methodType( Class<?> rtype, List<Class<?>> ptypes )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_MethodType_List()throws Throwable {
		List<Class<?>> p = new ArrayList<Class<?>>();
		p.add( double.class );
		p.add( long.class );
		MethodType mType = MethodType.methodType( boolean.class, p );
		AssertJUnit.assertEquals( mType.toString(), "(double,long)boolean" );
	}
	
	/**Test MethodType.methodType( Class<?> rtype, MethodType ptypes )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_MethodType_MethodType()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, MethodType.methodType( int.class, int.class, int.class ) );
		AssertJUnit.assertEquals( mType.toString(), "(int,int)int" );
	}
	
	/**Test MethodType.insertParameterTypes( int position, Class<?>... types ) 
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_ParameterArray()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, int.class, float.class, double.class, byte.class, char.class, short.class, long.class, boolean.class );
		Class<?>[] paramArray = mType.parameterArray();
		AssertJUnit.assertEquals( paramArray[0], int.class );
		AssertJUnit.assertEquals( paramArray[1], float.class );
		AssertJUnit.assertEquals( paramArray[2], double.class );
		AssertJUnit.assertEquals( paramArray[3], byte.class );
		AssertJUnit.assertEquals( paramArray[4], char.class );
		AssertJUnit.assertEquals( paramArray[5], short.class );
		AssertJUnit.assertEquals( paramArray[6], long.class );
		AssertJUnit.assertEquals( paramArray[7], boolean.class );
	}
	
	/**Test MethodType.parameterCount()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_ParameterCount()throws Throwable {
		MethodType mType = MethodType.methodType( int.class );
		mType = mType.insertParameterTypes( 0, int.class );
		mType = mType.insertParameterTypes( 1, int.class );
		AssertJUnit.assertEquals( mType.parameterCount(), 2 );
	}
	
	/**Test MethodType.parameterList()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_ParameterList()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, int.class, float.class, double.class, byte.class, char.class, short.class, long.class, boolean.class );
		List<Class<?>> paramList = mType.parameterList();
		AssertJUnit.assertEquals( paramList.get( 0 ), int.class );
		AssertJUnit.assertEquals( paramList.get( 1 ), float.class );
		AssertJUnit.assertEquals( paramList.get( 2 ), double.class );
		AssertJUnit.assertEquals( paramList.get( 3 ), byte.class );
		AssertJUnit.assertEquals( paramList.get( 4 ), char.class );
		AssertJUnit.assertEquals( paramList.get( 5 ), short.class );
		AssertJUnit.assertEquals( paramList.get( 6 ), long.class );
		AssertJUnit.assertEquals( paramList.get( 7 ), boolean.class );
	}
	
	/**Test MethodType.parameterType( int num )
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_ParameterType()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, int.class, float.class, double.class, byte.class, char.class, short.class, long.class, boolean.class );
		AssertJUnit.assertEquals( mType.parameterType( 0 ), int.class );
		AssertJUnit.assertEquals( mType.parameterType( 1 ), float.class );
		AssertJUnit.assertEquals( mType.parameterType( 2 ), double.class );
		AssertJUnit.assertEquals( mType.parameterType( 3 ), byte.class );
		AssertJUnit.assertEquals( mType.parameterType( 4 ), char.class );
		AssertJUnit.assertEquals( mType.parameterType( 5 ), short.class );
		AssertJUnit.assertEquals( mType.parameterType( 6 ), long.class );
		AssertJUnit.assertEquals( mType.parameterType( 7 ), boolean.class );
	}
	
	/**Test MethodType.returnType()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_ReturnType()throws Throwable {
		MethodType mType = MethodType.methodType( int.class );
		AssertJUnit.assertEquals( mType.returnType(), int.class );
		
		mType = MethodType.methodType( boolean.class, int.class );
		AssertJUnit.assertEquals( mType.returnType(), boolean.class );
		
		mType = MethodType.methodType( byte.class, int.class );
		AssertJUnit.assertEquals( mType.returnType(), byte.class );
		
		mType = MethodType.methodType( short.class, int.class );
		AssertJUnit.assertEquals( mType.returnType(), short.class );
		
		mType = MethodType.methodType( long.class, int.class );
		AssertJUnit.assertEquals( mType.returnType(), long.class );
		
		mType = MethodType.methodType( double.class, int.class );
		AssertJUnit.assertEquals( mType.returnType(), double.class );
		
		mType = MethodType.methodType( float.class, int.class );
		AssertJUnit.assertEquals( mType.returnType(), float.class );
		
		mType = MethodType.methodType( char.class, int.class );
		AssertJUnit.assertEquals( mType.returnType(), char.class );
	}
	
	/**Test MethodType.toMethodDescriptorString()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_ToMethodDescriptorString()throws Throwable {
		MethodType mType = MethodType.methodType( int.class, int.class, float.class, double.class, byte.class, char.class, short.class, long.class, boolean.class );
		String descriptor = mType.toMethodDescriptorString();
		AssertJUnit.assertEquals( descriptor, "(IFDBCSJZ)I" );
		
		mType = MethodType.methodType( Integer.class, Catch.class, String.class );
		descriptor = mType.toMethodDescriptorString();
		AssertJUnit.assertEquals( descriptor, "(Lcom/ibm/j9/jsr292/Catch;Ljava/lang/String;)Ljava/lang/Integer;" );
		
		mType = MethodType.methodType( int[].class, Catch[].class, Double[].class );
		descriptor = mType.toMethodDescriptorString();
		AssertJUnit.assertEquals( descriptor, "([Lcom/ibm/j9/jsr292/Catch;[Ljava/lang/Double;)[I" );
	}
	
	/**Test MethodType.unwrap()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Unwrap()throws Throwable {
		MethodType mType = MethodType.methodType( Integer.class );
		mType = mType.unwrap();
		AssertJUnit.assertEquals( mType.toString(), "()int" );
		
		mType = MethodType.methodType( Character.class );
		AssertJUnit.assertEquals( mType.unwrap().toString(), "()char" );
		
		mType = MethodType.methodType( Double.class );
		AssertJUnit.assertEquals( mType.unwrap().toString(), "()double" );
		
		mType = MethodType.methodType( Byte.class );
		AssertJUnit.assertEquals( mType.unwrap().toString(), "()byte" );
		
		mType = MethodType.methodType( Short.class );
		AssertJUnit.assertEquals( mType.unwrap().toString(), "()short" );
		
		mType = MethodType.methodType( Long.class );
		AssertJUnit.assertEquals( mType.unwrap().toString(), "()long" );
		
		mType = MethodType.methodType( Float.class );
		AssertJUnit.assertEquals( mType.unwrap().toString(), "()float" );
		
		mType = MethodType.methodType( Boolean.class );
		AssertJUnit.assertEquals( mType.unwrap().toString(), "()boolean" );
	}
	
	/**Test MethodType.wrap()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_Wrap()throws Throwable {
		MethodType mType = MethodType.methodType( int.class );
		AssertJUnit.assertEquals( mType.wrap().toString(), "()Integer" );
		
		mType = MethodType.methodType( char.class );
		AssertJUnit.assertEquals( mType.wrap().toString(), "()Character" );
		
		mType = MethodType.methodType( double.class );
		AssertJUnit.assertEquals( mType.wrap().toString(), "()Double" );
		
		mType = MethodType.methodType( byte.class );
		AssertJUnit.assertEquals( mType.wrap().toString(), "()Byte" );
		
		mType = MethodType.methodType( short.class );
		AssertJUnit.assertEquals( mType.wrap().toString(), "()Short" );
		
		mType = MethodType.methodType( long.class );
		AssertJUnit.assertEquals( mType.wrap().toString(), "()Long" );
		
		mType = MethodType.methodType( float.class );
		AssertJUnit.assertEquals( mType.wrap().toString(), "()Float" );
		
		mType = MethodType.methodType( boolean.class );
		AssertJUnit.assertEquals( mType.wrap().toString(), "()Boolean" );
		
		mType = MethodType.fromMethodDescriptorString( "(II)I", null );
		mType = mType.wrap();
		AssertJUnit.assertFalse( mType.hasPrimitives() );
	}
	
	/**
	 * Ensure that MethodTypes serialization works. Runs with the SecurityManager enabled.
	 */
	@Test(groups = { "level.extended" })
	public void test_SerializeGenericMethodType() throws IOException, ClassNotFoundException {
		Class<?> returnType = String.class;
		Class<?> paramType = Class.class;
		MethodType mt = MethodType.methodType(returnType, paramType);
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		ObjectOutputStream oos = new ObjectOutputStream(baos);
		oos.writeObject(mt);
		oos.close();
		ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(baos.toByteArray()));
		System.setSecurityManager(new SecurityManager());
		try {
			MethodType newMT = (MethodType) ois.readObject();
			
			// Validate MethodType constructed from serialized data
			AssertJUnit.assertEquals(returnType, newMT.returnType());
			AssertJUnit.assertEquals(paramType, newMT.parameterType(0));
		} finally {
			ois.close();
			System.setSecurityManager(null);
		}
	}
}
