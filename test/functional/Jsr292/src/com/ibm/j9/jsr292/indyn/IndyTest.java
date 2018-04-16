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
package com.ibm.j9.jsr292.indyn;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import static org.objectweb.asm.Opcodes.ARETURN;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.util.ArrayList;
import java.util.Arrays;

public class IndyTest {

	//constant test 
	@Test(groups = { "level.extended" })
	public void test_indyn_constant() {
		String s = com.ibm.j9.jsr292.indyn.GenIndyn.indy_return_string();
		if ( !s.equals( "helloworld" ) ) Assert.fail( "Wrong string returned'" + s +"'" );
	}
	
	//asType, identity test 
	@Test(groups = { "level.extended" })
	public void test_indyn_asType_identity() {
		int i = com.ibm.j9.jsr292.indyn.GenIndyn.box_and_unbox_int( 5 );
		if ( i != 5 ) Assert.fail( "Wrong int returned'" + i +"'" );
	}
	
	//dropArguments test
	@Test(groups = { "level.extended" })
	public void test_indyn_dropArguments() {
		int a = 0;
		int b = 1;
		int c = 2;
		int d = 3;
		int e = 4;
		int f = 5;
		
		a += f * e;
		b += e * d;
		c += Math.max( a, b ) * Math.random();
		d += a / f + 14;
		e -= Math.max( c + f, b ) * Math.random() * Math.log10( e * 47.2 );
		f += ( a + b + c + d + e )/( 2 + Math.pow( Math.random(), 4 ) );
	
		int i = com.ibm.j9.jsr292.indyn.GenIndyn.return_constant_ignore_actual_args( a, b, c, d, e, f );
		if ( i != 6 ) Assert.fail( "Wrong int returned'" + i +"'" );
	}
	
	//findVirtual test
	@Test(groups = { "level.extended" })
	public void test_indyn_findVirtual() {
		String s = com.ibm.j9.jsr292.indyn.GenIndyn.call_to_string( "Foo" );
		if ( !s.equals( "Foo" ) ) Assert.fail( "Wrong string returned'" + s +"'" );
		
		s = com.ibm.j9.jsr292.indyn.GenIndyn.call_to_string( IndyTest.class );
		if ( !s.equals( "class com.ibm.j9.jsr292.indyn.IndyTest" ) ) Assert.fail( "Wrong string returned: '" + s +"'" );
	}
	
	//findVirtual test ( interface method )
	@Test(groups = { "level.extended" })
	public void test_indyn_findVirtual_interface() {
		ArrayList l = new ArrayList();
		l.add( "a" );
		int s = com.ibm.j9.jsr292.indyn.GenIndyn.call_list_size( l );
		if (  s != 1  ) Assert.fail( "Wrong list size returned'" + s +"'" );
	}
	
	//findtest
	@Test(groups = { "level.extended" })
	public void test_indyn_findStatic() {
		String s = com.ibm.j9.jsr292.indyn.GenIndyn.call_value_of( 1 );
		if ( !s.equals( "1" ) ) Assert.fail( "Wrong string returned'" + s +"'" );
		
		s = com.ibm.j9.jsr292.indyn.GenIndyn.call_value_of( 1.55 );
		if ( !s.equals( "1.55" ) ) Assert.fail( "Wrong string returned: '" + s +"'" );
	}
	
	//findSpecial test
	@Test(groups = { "level.extended" })
	public void test_indyn_findSpecial() {
		String s = com.ibm.j9.jsr292.indyn.GenIndyn.call_tostring_of( new Integer( 1 ) );
		if ( !s.equals( "1" ) ) Assert.fail( "Wrong int returned'" + s +"'" );
	}
	
	//findGetter test
	@Test(groups = { "level.extended" })
	public void test_indyn_findGetter() {
		int publicInt = com.ibm.j9.jsr292.indyn.GenIndyn.call_to_getter( new Helper() );
		if ( publicInt != 1  ) Assert.fail( "Wrong int returned'" + publicInt +"'" );
	}
	
	//findSetter test
	@Test(groups = { "level.extended" })
	public void test_indyn_findSetter() {
		Helper h = new Helper();
		com.ibm.j9.jsr292.indyn.GenIndyn.call_to_setter( h,5 );
		if (  h.publicInt != 5  ) Assert.fail( "Wrong int returned'" + h.publicInt +"'" );
	}
	
	
	//findStaticGetter test
	@Test(groups = { "level.extended" })
	public void test_indyn_findStaticGetter() {
		int publicInt = com.ibm.j9.jsr292.indyn.GenIndyn.call_to_staticGetter();
		if ( publicInt != 100  ) Assert.fail( "Wrong int returned'" + publicInt +"'" );
	}
		
	//findStaticSetter test
	@Test(groups = { "level.extended" })
	public void test_indyn_findStaticSetter() {
		com.ibm.j9.jsr292.indyn.GenIndyn.call_to_staticSetter( 5 );
		if (  Helper.publicStaticInt != 5  ) Assert.fail( "Wrong int returned'" + Helper.publicStaticInt +"'" );
	}
	
	//bindTo test
	@Test(groups = { "level.extended" })
	public void test_indyn_bind_mh_to_receiver() {
		int v = com.ibm.j9.jsr292.indyn.GenIndyn.bind_mh_to_receiver();
		if (  v != 1  ) Assert.fail( "Wrong int returned'" + v +"'" );
	}
	
	//filterReturn test
	@Test(groups = { "level.extended" })
	public void test_indyn_filter_return() {
		int h = com.ibm.j9.jsr292.indyn.GenIndyn.filter_return( 1,"a" );
		if (  h != 0  ) Assert.fail( "Wrong int returned'" + h +"'" );
	}
	
	//guardWithTest test
	@Test(groups = { "level.extended" })
	public void test_indyn_guard_with_test_constant_true() {
		int h = com.ibm.j9.jsr292.indyn.GenIndyn.guard_with_test_constant_true( 5 );
		if (  h != -5  ) Assert.fail( "Wrong int returned'" + h +"'" );
	}
	
	//Test that uses an incoming argument to decide what to do 
	@Test(groups = { "level.extended" })
	public void test_indyn_guard_with_test_let_input_decide() {
		String s = com.ibm.j9.jsr292.indyn.GenIndyn.guard_with_test_let_input_decide( 9 );
		if (  !s.equals( "fizz" ) ) Assert.fail( "Wrong String returned'" + s +"'" );
		
		s = com.ibm.j9.jsr292.indyn.GenIndyn.guard_with_test_let_input_decide( 10 );
		if (  !s.equals( "buzz" ) ) Assert.fail( "Wrong String returned'" + s +"'" );
		
		s = com.ibm.j9.jsr292.indyn.GenIndyn.guard_with_test_let_input_decide( 8 );
		if (  !s.equals( "nothing" ) ) Assert.fail( "Wrong String returned'" + s +"'" );
	}
	
	//filterArguments on a dropArguments test
	@Test(groups = { "level.extended" })
	public void test_indyn_filter_arguments_on_drop_arguments() {
		String s = com.ibm.j9.jsr292.indyn.GenIndyn.filter_arguments_on_drop_arguments( "late", "lala", "L", "G" );
		if (  !s.equals( "GATE" ) ) Assert.fail( "Wrong String returned'" + s +"'" );
	}
	
	//Negative test : attempt to call an invalid method
	@Test(groups = { "level.extended" })
	public void test_indyn_call_invalid_method() {
		
		boolean bootstrapMethodErrorThrown = false; 
		
		try { 
			String s = com.ibm.j9.jsr292.indyn.GenIndyn.call_invalid_method( 1 );
		} catch ( java.lang.BootstrapMethodError e) {
			bootstrapMethodErrorThrown = true;
		}
		
		if ( bootstrapMethodErrorThrown == false ) {
			Assert.fail( "BootstrapMethodError not thrown when bootstrap method tries to create method handle for a non-existant method" );
		}
	}
	
	@Test(groups = { "level.extended" })
	public void test_indyn_call_Illegal_crosspackage_method () {
		
		boolean bootstrapMethodErrorThrown = false; 
		
		try { 
			String s = com.ibm.j9.jsr292.indyn.GenIndyn.call_Illegal_crosspackage_method( 1 );
		} catch ( java.lang.BootstrapMethodError e) {
			bootstrapMethodErrorThrown = true;
		}
		
		if ( bootstrapMethodErrorThrown == false ) {
			Assert.fail( "BootstrapMethodError not thrown when bootstrap method tries to create method handle for a cross package protected method" );
		}
	}
	
	//Negative test : invalid bootstrap method
	@Test(groups = { "level.extended" })
	public void test_indyn_invalid_bootstrap_method() {
		try {
			String s = com.ibm.j9.jsr292.indyn.GenIndyn.invalid_bootstrap(new Object());
			Assert.fail("BootstrapMethodError (in Java 8) or NoSuchMethodError (Java 9+) should have been thrown");
		} catch (BootstrapMethodError e) {
			//BootstrapMethodError should only apply to Java8
			if (false == "1.8".equals(System.getProperty("java.specification.version"))) {
				Assert.fail("[Java 9+] NoSuchMethodError not thrown when a non-existent bootstrap method is used ", e);
			}
		} catch (NoSuchMethodError e) {
			//Java9 and up throws NoSuchMethodError
			if ("1.8".equals(System.getProperty("java.specification.version"))) {
				Assert.fail("[Java 8] BootstrapMethodError not thrown when invalid bootstrap method is used ", e);
			}
		}
	}
	
	//Test creating an Object#toString() method using LDC
	@Test(groups = { "level.extended" })
	public void test_indyn_ldc_toString() throws Throwable {
		MethodHandle mhToString = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_toString();
		String result = (String) mhToString.invoke(new String( "SimpleString" ));
		if ( !result.equals( "SimpleString" ) ) {
			Assert.fail( "Wrong result from toString MethodHandle produced by LDC : " + result );
		}
	}
	
	//Test creating an String#toLowerCase() method handle using LDC
	@Test(groups = { "level.extended" })
	public void test_indyn_ldc_toLowerCase() throws Throwable {
		MethodHandle mhToLowerCase = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_toLowerCase();
		String result = (String) mhToLowerCase.invoke(new String( "SimpleString" ));
		if ( !result.equals( "simplestring" ) ) {
			Assert.fail( "Wrong result from toString MethodHandle produced by LDC : " + result );
		}
	}
	
	//Negative test : Attempting to create String#invalidMethod() method handle using LDC
	@Test(groups = { "level.extended" })
	public void test_indyn_ldc_invalidMethod() throws Throwable {
		boolean noSuchMethodErrorThrown = false;
		MethodHandle mhInvalidMethod = null;
		
		try { 
			mhInvalidMethod = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_invalid_MH();
		} catch ( java.lang.NoSuchMethodError e ) {
			noSuchMethodErrorThrown = true;
		}
		
		//Call it again
		try { 
			mhInvalidMethod = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_invalid_MH();
		} catch ( java.lang.NoSuchMethodError e ) {
			noSuchMethodErrorThrown = true;
		}
		
		if ( noSuchMethodErrorThrown == false ) {
			Assert.fail( "Attempt to load an invalid method using LDC did not throw NoSuchMethodError" );
		}
	}
	
	//Negative test : Attempting to create Helper#staticPrivateMethod() method handle using LDC
	@Test(groups = { "level.extended" })
	public void test_indyn_ldc_invalid_PrivateAccess() throws Throwable {
		boolean illegalAccessErrorThrown = false;
		
		try { 
			MethodHandle mhToString = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_illegal_private_MH();
		} catch ( java.lang.IllegalAccessError e ) {
			illegalAccessErrorThrown = true;
		}
		
		//Call it again
		try { 
			MethodHandle mhToString = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_illegal_private_MH();
		} catch ( java.lang.IllegalAccessError e ) {
			illegalAccessErrorThrown = true;
		}
		
		if ( illegalAccessErrorThrown == false ) {
			Assert.fail( "Attempt to load an invalid method using LDC did not throw IllegalAccessError" );
		}
	}
	
	//Negative test : Attempting to create differentpackage.CrossPackageHelper#staticProtectedMethod() method handle using LDC
	@Test(groups = { "level.extended" })
	public void test_indyn_ldc_invalid_CrossPackage_Access() throws Throwable {
		boolean illegalAccessErrorThrown = false;
		
		try { 
			MethodHandle mhToString = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_illegal_crosspackage_MH();
		} catch ( java.lang.IllegalAccessError e ) {
			illegalAccessErrorThrown = true;
		}
		
		//Call it again
		try { 
			MethodHandle mhToString = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_illegal_crosspackage_MH();
		} catch ( java.lang.IllegalAccessError e ) {
			illegalAccessErrorThrown = true;
		}
		
		if ( illegalAccessErrorThrown == false ) {
			Assert.fail( "Incorrect visibility available to class belonging to a different package in MethodHandle produced by LDC" );
		}
	}
		
	//Test creating a methodType using LDC
	@Test(groups = { "level.extended" })
	public void test_indyn_ldc_String_type() {
		MethodType mtToString = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_String_type();
		if ( !mtToString.toString().equals( "()String" )) {
			Assert.fail( "Wrong method type produced by LDC : " + mtToString);
		}
	}
	
	//Test creating a methodType using LDC
	@Test(groups = { "level.extended" })
	public void test_indyn_ldc_BootstrapMethod_type() {
		MethodType mtToString = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_bootstrapmethod_type();
		if ( !mtToString.toString().equals( "(Lookup,String,MethodType)CallSite" )) {
			Assert.fail( "Wrong method type produced by LDC : " + mtToString);
		}
	}
	
	//Negative test : Creating a MethodType with a non-existent class using LDC
	@Test(groups = { "level.extended" })
	public void test_indyn_ldc_non_existent_class_methodtype() {
		try {
			MethodType mtToString = com.ibm.j9.jsr292.indyn.GenIndyn.ldc_invalid_type();
			Assert.fail("NoClassDefFoundError not thrown when non-existent class is used to create MethodType via LDC");
		} catch (NoClassDefFoundError e) {
			/* Expected behavior */
		}
	}
	
	
	// test different BSM signatures
	@Test(groups = { "level.extended" })
	public void test_different_bsm_signatures() {
		String s1 = com.ibm.j9.jsr292.indyn.GenIndyn.bsm_signature_obj_array();
		AssertJUnit.assertNotNull(s1);	// can't validate easily as it contains a Lookup object
		String s2 = com.ibm.j9.jsr292.indyn.GenIndyn.bsm_signature_L_S_MT_obj_array();
		AssertJUnit.assertEquals(Arrays.toString(new Object[] { "hello", "world"}), s2);
		String s3 = com.ibm.j9.jsr292.indyn.GenIndyn.bsm_signature_L_S_MT_obj_array_passing_primitives();
		AssertJUnit.assertEquals(Arrays.toString(new Object[] { "hello", "world", 444, 555, 666, 777, 888}), s3);
		String s4 = com.ibm.j9.jsr292.indyn.GenIndyn.bsm_signature_L_S_MT_byte_array();
		AssertJUnit.assertEquals(Arrays.toString(new byte[] { 4,5,6,7,8 }), s4);
		long longValue = com.ibm.j9.jsr292.indyn.GenIndyn.bsm_signature_L_S_MT_sss_long_array();
		AssertJUnit.assertEquals(7 + 8, longValue);
	}
	
	// Test to ensure coverage of MethodType resolving code in Java_java_lang_invoke_MethodHandle_getCPMethodTypeAt()
	@Test(groups = { "level.extended" })
	public void test_boostrap_return_constant_MethodType() {
		MethodType expected = MethodType.methodType(String.class, String.class, String.class, int.class);
		MethodType mt = com.ibm.j9.jsr292.indyn.GenIndyn.test_boostrap_return_constant_MethodType();
		AssertJUnit.assertEquals(expected, mt);
		AssertJUnit.assertTrue(expected == mt);
	}
}
