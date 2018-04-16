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
package com.ibm.jvmti.tests.followReferences;

public class fr003 
{
	public static native boolean followFromObject(Object obj, Class filter);
	public static native boolean followFromStringObject(Object obj, Class filter);
	public static native boolean followFromArrayObject(Object obj, Class filter, int primitiveType, int arraySize);
	public static native String getStringData();
	
    public static final int JVMTI_PRIMITIVE_TYPE_BOOLEAN = 90;
    public static final int JVMTI_PRIMITIVE_TYPE_BYTE = 66;
    public static final int JVMTI_PRIMITIVE_TYPE_CHAR = 67;
    public static final int JVMTI_PRIMITIVE_TYPE_SHORT = 83;
    public static final int JVMTI_PRIMITIVE_TYPE_INT = 73;
    public static final int JVMTI_PRIMITIVE_TYPE_LONG = 74;
    public static final int JVMTI_PRIMITIVE_TYPE_FLOAT = 70;
    public static final int JVMTI_PRIMITIVE_TYPE_DOUBLE = 68;

    	
	public boolean testFollowFromObject()
	{
		X1 x1 = new X1();
		
		TagManager.clearTags();
		TagManager.setObjectTag(x1, 0x1000L);
		TagManager.setObjectTag(x1.c1, 0x1001L);
						
		boolean ret = followFromObject(x1, null);
		if (ret == false)
			return ret;
				
		ret = TagManager.checkTags();
		
		return ret;
	}
	
	public String helpFollowFromObject()
	{
		return "follow references from a specific object without any class filtering";
	}
	
	
	public boolean testFollowFromObjectWithClassFilter()
	{
		X1 x1 = new X1();
	
		TagManager.clearTags();
		TagManager.setObjectTag(x1, 0x1000L);
		TagManager.setObjectTag(x1.c1, 0x1001L);
				
		boolean ret = followFromObject(x1, x1.getClass());
		if (ret == false)
			return ret;
		
		if (TagManager.isTagQueued(0x1000L) == false) {
			System.out.println("1");
			return false;
		}
		
		if (TagManager.isTagQueued(0x1001L) == true) {
			System.out.println("2");
			return false;
		}
		
		
		return ret;
	}
		
	public String helpFollowFromObjectWithClassFilter()
	{
		return "follow references from a specific object with a class filter";
	}

	
	

	
	
	public boolean testFollowArrayPrimitiveValues_Boolean()
	{
		X1 x1 = new X1();
		boolean[] array = x1.x1_boolean_array;
				
		TagManager.clearTags();
		TagManager.setObjectTag(array, 0x1000L);
				
		boolean ret = followFromArrayObject(array, array.getClass(), JVMTI_PRIMITIVE_TYPE_BOOLEAN, array.length);
		if (ret == false)
			return ret;
		
		return TagManager.checkTags();
	}
	
	public String helpFollowArrayPrimitiveValues_Boolean()
	{
		return "check boolean array primitive values with class filtering"; 
	}
	
	
	//////////////////////////////////////////////////////////////////////////
	//
	
	public boolean testFollowArrayPrimitiveValues_Byte()
	{
		X1 x1 = new X1();
		byte[] array = x1.x1_byte_array;
				
		TagManager.clearTags();
		TagManager.setObjectTag(array, 0x1001L);
				
		boolean ret = followFromArrayObject(array, array.getClass(), JVMTI_PRIMITIVE_TYPE_BYTE, array.length);
		if (ret == false)
			return ret;
		
		return TagManager.checkTags();
	}
	
	public String helpFollowArrayPrimitiveValues_Byte()
	{
		return "check byte array primitive values with class filtering";
	}

	//////////////////////////////////////////////////////////////////////////
	//
	
	public boolean testFollowArrayPrimitiveValues_Char()
	{
		X1 x1 = new X1();
		char[] array = x1.x1_char_array;
				
		TagManager.clearTags();
		TagManager.setObjectTag(array, 0x1002L);
				
		boolean ret = followFromArrayObject(array, array.getClass(), JVMTI_PRIMITIVE_TYPE_CHAR, array.length);
		if (ret == false)
			return ret;
		
		return TagManager.checkTags();
	}
	
	public String helpFollowArrayPrimitiveValues_Char()
	{
		return "check char array primitive values with class filtering";
	}

	//////////////////////////////////////////////////////////////////////////
	//
	
	public boolean testFollowArrayPrimitiveValues_Short()
	{
		X1 x1 = new X1();
		short[] array = x1.x1_short_array;
				
		TagManager.clearTags();
		TagManager.setObjectTag(array, 0x1003L);
				
		boolean ret = followFromArrayObject(array, array.getClass(), JVMTI_PRIMITIVE_TYPE_SHORT, array.length);
		if (ret == false)
			return ret;
		
		return TagManager.checkTags();
	}
	
	public String helpFollowArrayPrimitiveValues_Short()
	{
		return "check short array primitive values with class filtering";
	}

	//////////////////////////////////////////////////////////////////////////
	//
	
	
	public boolean testFollowArrayPrimitiveValues_Int()
	{
		X1 x1 = new X1();
		int[] array = x1.x1_int_array;
		
		TagManager.clearTags();
		TagManager.setObjectTag(array, 0x1004L);
		
				
		boolean ret = followFromArrayObject(array, array.getClass(), JVMTI_PRIMITIVE_TYPE_INT, array.length);
		if (ret == false)
			return ret;
		
		return TagManager.checkTags();
	}
	
	public String helpFollowArrayPrimitiveValues_Int()
	{
		return "check integer array primitive values with class filtering"; 
	}
	
	//////////////////////////////////////////////////////////////////////////
	//
	
	public boolean testFollowArrayPrimitiveValues_Long()
	{
		X1 x1 = new X1();
		long[] array = x1.x1_long_array;
				
		TagManager.clearTags();
		TagManager.setObjectTag(array, 0x1005L);
				
		boolean ret = followFromArrayObject(array, array.getClass(), JVMTI_PRIMITIVE_TYPE_LONG, array.length);
		if (ret == false)
			return ret;
		
		return TagManager.checkTags();
	}
	
	public String helpFollowArrayPrimitiveValues_Long()
	{
		return "check long array primitive values with class filtering";
	}

	//////////////////////////////////////////////////////////////////////////
	//
	
	public boolean testFollowArrayPrimitiveValues_Float()
	{
		X1 x1 = new X1();
		float[] array = x1.x1_float_array;
				
		TagManager.clearTags();
		TagManager.setObjectTag(array, 0x1006L);
				
		boolean ret = followFromArrayObject(array, array.getClass(), JVMTI_PRIMITIVE_TYPE_FLOAT, array.length);
		if (ret == false)
			return ret;
		
		return TagManager.checkTags();
	}
	
	public String helpFollowArrayPrimitiveValues_Float()
	{
		return "check float array primitive values with class filtering";
	}
	
	//////////////////////////////////////////////////////////////////////////
	//
	
	public boolean testFollowArrayPrimitiveValues_Double()
	{
		X1 x1 = new X1();
		double[] array = x1.x1_double_array;
				
		TagManager.clearTags();
		TagManager.setObjectTag(array, 0x1007L);
				
		boolean ret = followFromArrayObject(array, array.getClass(), JVMTI_PRIMITIVE_TYPE_DOUBLE, array.length);
		if (ret == false)
			return ret;
		
		return TagManager.checkTags();
	}
	
	public String helpFollowArrayPrimitiveValues_Double()
	{
		return "check double array primitive values with class filtering";
	}

	//////////////////////////////////////////////////////////////////////////
	//
	public boolean testStringPrimitive()
	{
		X2 x2 = new X2();
		String s = x2.x2_string;
		
		TagManager.clearTags();
		TagManager.setObjectTag(s, 0x2000L);
		TagManager.setObjectTag(s.getClass(), 0x2001L);
		
		boolean ret = followFromStringObject(s, s.getClass());
		if (ret == false)
			return ret;
		
		String str = getStringData();
		
		if (!str.equals(x2.x2_string)) {
			System.out.println("Returned string ["+str+"] does not match expected ["+x2.x2_string+"]");
			return false;
		}
		
		
		return TagManager.checkTags();
		
	}
	
	public String helpStringPrimitive()
	{
		return "check correct return of a string";
	}
	
}



class X1 
{
	public X2 x2;
	public C1 c1;
	public int x1_int;
	
	public byte foo[] = {1 , 1, 1, 1, 1};
	
	public boolean x1_boolean_array[] = {true, false, true, false};
	public byte    x1_byte_array[]    = {0x10, 0x20, 0x30, 0x40};
	public char    x1_char_array[]    = {'a', 'b', 'c', 'd'};
	public short   x1_short_array[]   = {0x100, 0x200, 0x300, 0x400};
	public int 	   x1_int_array[]     = {0, 10000, 20000, 40000};
	public long    x1_long_array[]    = {0, 10000000L, 20000000L, 40000000L};
	public float   x1_float_array[]   = {0, 10000, 20000, 40000};
	public double  x1_double_array[]  = {0, 10000.0, 20000.0, 40000.0};
		
	public X1()
	{
		x2 = new X2();
		c1 = new C1();
	}
	
}

class X2 {
	public int x2_int = 1000;	
	public long x2_long = 2000;
	String x2_string = "test string";
	
}

