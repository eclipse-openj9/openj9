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
package examples;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;

/**
 * All the fields, constructors and methods in this class are used by test cases that require fields, methods, and constructors of 
 * various behavior from a class in a different package than that of the test class. 
 * 
 * @author mesbah
 *
 */
public class PackageExamples {
	
	public static Lookup getLookup() {
		return MethodHandles.lookup();
	}
	
	public static Lookup getPublicLookup() {
		return MethodHandles.publicLookup();
	}
	
	final protected String finalProtectedMethod() { return "finalProtectedMethod"; }

	protected static String protectedMethod () { return "protectedMethod"; }
	
	/* default */ static String defaultMethod() { return "defaultMethod"; }
	
	public int nonStaticPublicField;
	public static int staticPublicField;
	
	private int nonStaticPrivateField;
	private static int staticPrivateField;
	
	protected int nonStaticProtectedField;
	
	public int addPublic(int a, int b){return a+b;}
	private int addPrivate(int a, int b){return a+b;}
	
	public static int addPublicStatic (int a,int b) {return a+b;}
	private static int addPrivateStatic (int a,int b) {return a+b;}
	
	protected static int addProtectedStatic(int a,int b) {return a+b;}
	
	public PackageExamples(){
		super();
	}
	
	public PackageExamples(int a, int b){
		this.nonStaticPublicField = a + b;
	}
	

	public class CrossPackageInnerClass{
		public int addPublicInner(int a, int b){ return a+b; }
		public Lookup getLookup() { return MethodHandles.lookup(); }
		
		public class CrossPackageInnerClass2_Nested_Level2 {
			public Lookup getLookup() {
				return MethodHandles.lookup();
			}
			public int addPublicInner_level2(int a, int b){ return a+b; }
		}
	}
	
	public int getLength(String[] o){
		return o.length;
	}
	
	public static int getLengthStatic(String[] o){
		return o.length;
	}
	
	public int addPublicVariableArity(int... n){
		int sum = 0 ; 
		for ( int i = 0 ; i < n.length ; i++ ){
			sum += n[i];
		}
		return sum;
	}
	
	public static int addPublicStaticVariableArity(int... n){
		int sum = 0 ; 
		for ( int i = 0 ; i < n.length ; i++ ){
			sum += n[i];
		}
		return sum;
	}
	
	protected int addProtected(int a, int b) {return a+b;}
}
