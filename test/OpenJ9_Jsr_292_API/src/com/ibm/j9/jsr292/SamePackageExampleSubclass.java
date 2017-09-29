/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *******************************************************************************/
package com.ibm.j9.jsr292;

/**
 * Subclass example used by various tests 
 * 
 * @author mesbah
 *
 */
public class SamePackageExampleSubclass extends SamePackageExample {
	
	public int nonStaticPublicField_Child;
	public static int staticPublicField_Child;
	
	private int nonStaticPrivateField_Child;
	private static int staticPrivateField_Child;
	
	protected int nonStaticProtectedField_Child;
	
	public int addPublic(int a, int b) {return a+b+1;} // Overridden addPublic that returns something different
	
	public int addPublic_Child(int a, int b) {return a+b+2;} 
	
	@Override
	protected int addProtected(int a, int b) {
		return super.addProtected(a, b)+10;
	}
	
	public SamePackageExampleSubclass() { }
	
	public SamePackageExampleSubclass(int a, int b) {
		super();
		nonStaticPublicField_Child = a + b; 
	}
}
