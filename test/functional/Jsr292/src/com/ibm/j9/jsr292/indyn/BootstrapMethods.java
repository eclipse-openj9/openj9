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
package com.ibm.j9.jsr292.indyn;

import static java.lang.invoke.MethodHandles.catchException;
import static java.lang.invoke.MethodHandles.dropArguments;
import static java.lang.invoke.MethodHandles.foldArguments;
import static java.lang.invoke.MethodHandles.guardWithTest;
import static java.lang.invoke.MethodHandles.identity;
import static java.lang.invoke.MethodHandles.lookup;
import static java.lang.invoke.MethodHandles.permuteArguments;
import static java.lang.invoke.MethodHandles.throwException;
import static java.lang.invoke.MethodType.methodType;

import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.MutableCallSite;
import java.lang.invoke.SwitchPoint;
import java.util.Arrays;
import java.util.List;

import differentpackage.CrossPackageHelper;

public class BootstrapMethods {
	
	//Bootstrap method for constant MH test
	public static CallSite bootstrap_constant_string( Lookup lookup, String name, MethodType type, String s1, String s2 ) {
		String combined = s1 + s2;
		return new ConstantCallSite( MethodHandles.constant( String.class, combined ) );
	}
	
	//Bootstrap method for MHs.identify  and MH.asType test 
	public static CallSite bootstrap_box_and_unbox_int( Lookup lookup, String name, MethodType type ) {
		MethodHandle identity = MethodHandles.identity( int.class );
		return new ConstantCallSite( identity.asType( methodType( int.class, Integer.class ) ).asType( identity.type() ) );
	}
	
	//Bootstrap method for MHs.dropArguments test 
	public static CallSite bootstrap_return_constant_ignore_actual_args( Lookup lookup, String name, MethodType type ) {
		MethodHandle constant = MethodHandles.constant( int.class, 6 );
		return new ConstantCallSite( MethodHandles.dropArguments( constant, 0, int.class, int.class, int.class, int.class, int.class, int.class ) );
	}
	
	//Bootstrap method for findVirtual MH test 
	public static CallSite bootstrap_call_to_string( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findVirtual( Object.class, "toString", methodType( String.class ) );
		return new MutableCallSite( mh );
	}
	
	//Bootstrap method for findVirtual MH test [interface method]
	public static CallSite bootstrap_call_list_size( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findVirtual( List.class, "size", methodType( int.class ) );
		return new ConstantCallSite( mh );
	}
	
	//Bootstrap method for findStatic MH test
	public static CallSite bootstrap_call_value_of( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findStatic( String.class, "valueOf", MethodType.methodType( String.class,Object.class ) );
		return new ConstantCallSite( mh );
	}
	
	public static CallSite bootstrap_call_tostring_of( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findVirtual( Integer.class, "toString", MethodType.methodType( String.class ));
		return new ConstantCallSite( mh );
	}
	
	//Bootstrap method for findGetter MH test 
	public static CallSite bootstrap_call_to_getter( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findGetter( Helper.class, "publicInt", int.class );
		return new ConstantCallSite( mh );
	}
	
	//Bootstrap method for findSetter MH test 
	public static CallSite bootstrap_call_to_setter( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findSetter( Helper.class, "publicInt", int.class  );
		return new ConstantCallSite( mh );
	}
	
	//Bootstrap method for findStaticGetter MH test 
	public static CallSite bootstrap_call_to_staticGetter( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findStaticGetter( Helper.class, "publicStaticInt", int.class );
		return new ConstantCallSite( mh );
	}
	
	//Bootstrap method for findStaticSetter MH test 
	public static CallSite bootstrap_call_to_staticSetter( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findStaticSetter( Helper.class, "publicStaticInt", int.class );
		return new ConstantCallSite( mh );
	}
	
	//Bootstrap method for bindTo MH test 
	public static CallSite bootstrap_bind_mh_to_receiver( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findGetter( Helper.class, "publicInt", int.class  );
		mh = mh.bindTo( new Helper() );
		return new ConstantCallSite( mh );
	}
	
	//Bootstrap method for filterReturn MH test 
	public static CallSite bootstrap_filter_return( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle handle = MethodHandles.lookup().findStatic( Helper.class, "addStaticPublic", methodType( void.class,int.class,String.class ) );
		MethodHandle filter = MethodHandles.constant( int.class, 0 );
		MethodHandle filtered = MethodHandles.filterReturnValue( handle, filter );
		return new ConstantCallSite( filtered );
	}
	
	//Bootstrap method for guardWithTest MH test using a constant true condition
	public static CallSite bootstrap_guard_with_test_constant_true ( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle constant_true = MethodHandles.constant( boolean.class, true );
		
		MethodHandle trueTarget = lookup.findStatic( Helper.class, "negativeInt", methodType(int.class, int.class) );
		MethodHandle falseTarget = MethodHandles.identity( int.class );

		// if (true_gwt()) { negativeInt(5) } else { identity(5) }
		MethodHandle true_gwt = MethodHandles.guardWithTest( constant_true, trueTarget, falseTarget );
			 
		return new ConstantCallSite( true_gwt );
	}
	
	//Bootstrap method for guardWithTest MH test where decision is made based on an incoming argument
	public static CallSite bootstrap_guard_with_test_let_input_decide( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh_isDivisibleByThree = lookup.findStatic( Helper.class, "isDivisibleByThree", methodType( boolean.class, int.class ) );
		MethodHandle mh_isDivisibleByFive = lookup.findStatic( Helper.class, "isDivisibleByFive", methodType( boolean.class, int.class ) );
		
		MethodHandle mh_returnFizz = lookup.findStatic( Helper.class, "returnFizz", methodType( String.class, int.class ) );
		MethodHandle mh_returnBuzz = lookup.findStatic( Helper.class, "returnBuzz", methodType( String.class, int.class ) );
		MethodHandle mh_returnNothing = lookup.findStatic( Helper.class, "returnNothing", methodType( String.class, int.class ) );

		// if input % 3 , return "fizz", else if input % 5 , return "buzz" , else return "nothing"
		MethodHandle otherGWT = MethodHandles.guardWithTest( mh_isDivisibleByFive, mh_returnBuzz, mh_returnNothing );
		MethodHandle result = MethodHandles.guardWithTest( mh_isDivisibleByThree, mh_returnFizz, otherGWT );
		return new ConstantCallSite( result );
	}
	
	//Bootstrap method for invokedynamic test where the target MH is a filterArguments() on a dropArguments() 
	//adapter such that the filters can be discarded once the they have all been inlined.
	public static CallSite bootstrap_filter_arguments_on_drop_arguments( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle repFirst = MethodHandles.lookup().findVirtual(String.class, "replaceFirst", methodType(String.class, String.class, String.class));
		
		MethodHandle mh0 = MethodHandles.dropArguments(repFirst, 1, Arrays.asList(new Class<?>[] {String.class}));
		MethodHandle upcase = MethodHandles.lookup().findVirtual(String.class, "toUpperCase", methodType(String.class));
		
		MethodHandle f0 = MethodHandles.filterArguments(mh0, 0, upcase);
		 
		return new ConstantCallSite( f0 );
	}
	
	//For Negative test : Bootstrap method that attempts to return a nonexistent method
	public static CallSite bootstrap_call_invalid_method( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findStatic( String.class, "valueO", MethodType.methodType( String.class,Object.class ) );
		return new ConstantCallSite( mh );
	}
	
	//For Negative test : Bootstrap method that attempts to return a method handle to a protected cross package method
	public static CallSite bootstrap_call_Illegal_crosspackage_method( Lookup lookup, String name, MethodType type ) throws Throwable {
		MethodHandle mh = lookup.findStatic( CrossPackageHelper.class, "staticProtectedMethod", MethodType.methodType( String.class ) );
		return new ConstantCallSite( mh );
	}
	
	public static CallSite bootstrap_objects( Object... args ) {
		return new ConstantCallSite(MethodHandles.constant(String.class, Arrays.toString(args)));
	}

	public static CallSite bootstrap_objects(Lookup l, String name, MethodType type, Object... args ) {
		return new ConstantCallSite(MethodHandles.constant(String.class, Arrays.toString(args)));
	}
	
	public static CallSite bootstrap_byte_array(Lookup l, String name, MethodType type, byte... args ) {
		return new ConstantCallSite(MethodHandles.constant(String.class, Arrays.toString(args)));
	}
	public static CallSite bootstrap_sss_long_array(Lookup l, String name, MethodType type, short s0, short s1, short s2, long... args ) {
		long value = 0;
		for (int i = 0; i < args.length; i++) {
			value += args[i];
		}
		return new ConstantCallSite(MethodHandles.constant(long.class, value));
	}
	
	public static CallSite boostrap_return_constant_MethodType(Lookup l, String name, MethodType type, MethodType result) {
		return new MutableCallSite(MethodHandles.constant(MethodType.class, result));
	}
	public static CallSite gwtBootstrap(Lookup ignored, String name, MethodType type) throws Throwable {
		Lookup lookup = lookup();
		MethodHandle double_string = lookup.findStatic(BootstrapMethods.class, "dup", methodType(String.class, String.class));
		MethodHandle double_integer = lookup.findStatic(BootstrapMethods.class, "dup", methodType(String.class, Integer.class));
		MethodHandle double_object = lookup.findStatic(BootstrapMethods.class, "dup", methodType(String.class, Object.class));
		MethodHandle isInteger = lookup.findStatic(BootstrapMethods.class, "isInteger", methodType(boolean.class, Object.class));
		MethodHandle isString = lookup.findStatic(BootstrapMethods.class, "isString", methodType(boolean.class, Object.class));

		MethodHandle handle = guardWithTest(
				isString, 
				double_string.asType(methodType(String.class, Object.class)),
				double_object);
		handle = guardWithTest(
				isInteger,
				double_integer.asType(methodType(String.class, Object.class)),
				handle);
		return new MutableCallSite(handle);
	}

	static boolean isString(Object o) {
		return o instanceof String;
	}
	static boolean isInteger(Object o) {
		return o instanceof Integer;
	}
	
	static String dup(String s) {
		return s+s;
	}

	static String dup(Integer i) {
		return "" + (i + i);
	}

	static String dup(Object o) {
		return "DoesNotUnderStand: " + o.getClass() + " message: double";
	}

	// (int, int, String)   --> (String, int, int)  --> (String)
	public static CallSite permuteBootstrap(Lookup ignored, String name, MethodType type) throws Throwable {
		Lookup lookup = lookup();
		MethodHandle object_toString = lookup.findVirtual(Object.class, "toString", methodType(String.class));
		MethodHandle drop = dropArguments(object_toString, 1, int.class, int.class);
		MethodHandle permute = permuteArguments(
				drop, 
				methodType(String.class, int.class, int.class, Object.class), 
				new int[] {2, 0, 1});
		return new MutableCallSite(permute);
	}

	static int add(int i, int j) {
		return i + j;
	}
	static int throwRuntimeException(int i, int j) {
		throw new RuntimeException("add fallback");	
	}

	public static CallSite switchpointBootstrap(Lookup ignored, String name, MethodType type) throws Throwable {
		Lookup lookup = lookup();
		MethodHandle add = lookup.findStatic(BootstrapMethods.class, "add", methodType(int.class, int.class, int.class));
		MethodHandle fallback = lookup.findStatic(BootstrapMethods.class, "throwRuntimeException", methodType(int.class, int.class, int.class));
		MethodHandle handle = new SwitchPoint().guardWithTest(add, fallback);	
		return new MutableCallSite(handle);
	}

	public static CallSite mcsBootstrap(Lookup ignored, String name, MethodType type) throws Throwable {
		Lookup lookup = lookup();
		MethodHandle add = lookup.findStatic(BootstrapMethods.class, "add", methodType(int.class, int.class, int.class));
		MutableCallSite mcs = new MutableCallSite(add);
		return new MutableCallSite(mcs.dynamicInvoker());
	}

	public static CallSite catchBootstrap(Lookup ignored, String name, MethodType type) throws Throwable {
		Lookup lookup = lookup();
		MethodHandle add = lookup.findStatic(BootstrapMethods.class, "add", methodType(int.class, int.class, int.class));
		MethodHandle catchHandle = catchException(
				add, 
				RuntimeException.class, 
				throwException(add.type().returnType(), RuntimeException.class));
		return new MutableCallSite(catchHandle);
	}

	public static CallSite foldBootstrap(Lookup ignored, String name, MethodType type) throws Throwable {
		Lookup lookup = lookup();
		MethodHandle add = lookup.findStatic(BootstrapMethods.class, "add", methodType(int.class, int.class, int.class));
		MethodHandle fold = foldArguments(add, identity(int.class));
		fold = foldArguments(add, fold);
		return new MutableCallSite(fold);
	}
	
	public static CallSite fibBootstrap(Lookup ignored, String name, MethodType type) throws Throwable {
		Lookup lookup = lookup();
		MethodHandle fib = lookup.findStatic(Class.forName("com.ibm.j9.jsr292.indyn.ComplexIndy"), "fibIndy", methodType(int.class, int.class));
		return new MutableCallSite(fib);
	}
}
