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
package com.ibm.jvmti.tests.iterateThroughHeap;

public class ith001 
{
	ith001Sub testSub;
	
	public boolean setup(String args)
	{
		testSub = new ith001Sub();
		
		return true;
	}
	
	public boolean testHeapIteration() 
	{
		boolean ret;

		ret = testSub.testHeapIteration();
				
		return ret;
	}

	public String helpHeapIteration()
	{
		return "Tests the IterateThroughHeap() call";
	}

	
	
	public boolean testHeapIterationArrayPrimitive() 
	{
		boolean ret;

		testSub.testArrayPrimitive_tagArrays();
		
		ret = testSub.testArrayPrimitive();
				
		return ret;
	}

	public String helpHeapIterationArrayPrimitive()
	{
		return "Tests the IterateThroughHeap() call and the Array Primitive iteration";
	}

	
	
	public boolean testHeapIterationFieldPrimitive() 
	{
		boolean ret;

		ret = testSub.testFieldPrimitive();
				
		return ret;
	}
		
	public String helpHeapIterationFieldPrimitive()
	{
		return "Tests the IterateThroughHeap() call and the Field Primitive iteration";
	}
	
	
	
	public boolean testHeapIterationStringPrimitive() 
	{
		boolean ret;

		ret = testSub.testStringPrimitive();
				
		return ret;
	}
	
	public String helpHeapIterationStringPrimitive()
	{
		return "Tests the IterateThroughHeap() call and the String  Primitive iteration";
	}	
	
}
