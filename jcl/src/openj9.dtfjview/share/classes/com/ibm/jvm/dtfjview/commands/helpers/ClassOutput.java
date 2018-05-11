/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands.helpers;

import java.io.PrintStream;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;

public class ClassOutput {

	public static void printStaticFields(JavaClass jc, PrintStream out)
	{
		// if the class name refers to an array, return because there are no fields
		try {
			if (jc.isArray()) {
				return;
			}
		} catch (CorruptDataException cde) {
			out.print("\t  <can't determine if class is array; assuming it's not>\n\n");
		}
		
		String className;
		try {
			className = jc.getName();
		} catch (CorruptDataException cde) {
			className = null;
		}
		
		// we've found a class, so we'll print out its static fields
		boolean found = false;
		Iterator itField = jc.getDeclaredFields();
		while (itField.hasNext())
		{
			JavaField jf = (JavaField)itField.next();
			boolean isStatic;
			
			try {
				isStatic = Modifier.isStatic(jf.getModifiers());
			} catch (CorruptDataException e) {
				out.print("\t   <error while getting modifier for field \"");
				try {
					out.print(jf.getName());
				} catch (CorruptDataException d) {
					out.print(Exceptions.getCorruptDataExceptionString());
				}
				out.print("\", " + Exceptions.getCorruptDataExceptionString() + ">");
				isStatic = false;
			}
			
			if (isStatic)
			{
				if (!found)
				{
					out.print("\t  static fields for \"" + className + "\"\n");
				}
				found = true;
				printStaticFieldData(jf, out);
			}
		}
		if (found)
			out.print("\n");
		else
			out.print("\t  \"" + className + "\" has no static fields\n\n");
	}

	public static void printNonStaticFields(JavaClass jc, PrintStream out)
	{
		// if the class name refers to an array, return because there are no fields
		try {
			if (jc.isArray()) {
				return;
			}
		} catch (CorruptDataException cde) {
			out.print("\t  <can't determine if class is array; assuming it's not>\n\n");
		}
		
		String className;
		try {
			className = jc.getName();
		} catch (CorruptDataException cde) {
			className = null;
		}
		
		// we've found a class, so we'll print out its static fields
		boolean found = false;
		Iterator itField = jc.getDeclaredFields();
		while (itField.hasNext())
		{
			JavaField jf = (JavaField)itField.next();
			boolean isStatic;
			
			try {
				isStatic = Modifier.isStatic(jf.getModifiers());
			} catch (CorruptDataException e) {
				out.print("\t   <error while getting modifier for field \"");
				try {
					out.print(jf.getName());
				} catch (CorruptDataException d) {
					out.print(Exceptions.getCorruptDataExceptionString());
				}
				out.print("\", " + Exceptions.getCorruptDataExceptionString() + ">");
				isStatic = false;
			}
			
			if (!isStatic)
			{
				if (!found)
				{
					out.print("\t  non-static fields for \"" + className + "\"\n");
				}
				found = true;
				printNonStaticFieldData(null, jf, out);
			}
		}
		if (found)
			out.print("\n");
		else
			out.print("\t  \"" + className + "\" has no non-static fields\n\n");
	}
	
	public static void printFields(JavaObject jo, JavaClass jc, JavaRuntime jr, PrintStream out)
	{
		boolean array;
		
		try {
			array = jo.isArray();
		} catch (CorruptDataException e) {
			out.print("\t   <cannot determine if above object is array (" +
					Exceptions.getCorruptDataExceptionString() + "); "
					+ "we will assume it is not an array>\n");
			array = false;
		}

		if (array)
		{
			String componentType;
			int arraySize;
			
			try {
				componentType = jc.getComponentType().getName();
			} catch (CorruptDataException e) {
				out.print("\t   <cannot determine what type of array this is (" +
						Exceptions.getCorruptDataExceptionString() + ")>\n");
				return;
			}

			try {
				arraySize = jo.getArraySize();
			} catch (CorruptDataException e) {
				out.print("\t   <cannot determine the size of the array (" +
						Exceptions.getCorruptDataExceptionString() + ")>\n");
				return;
			}
			
			
			
			Object dst = null;

			if (componentType.equals("boolean")) {
				dst = new boolean[arraySize];
			} else if (componentType.equals("byte")) {
				dst = new byte[arraySize];
			} else if (componentType.equals("char")) {
				dst = new char[arraySize];
			} else if (componentType.equals("short")) {
				dst = new short[arraySize];
			} else if (componentType.equals("int")) {
				dst = new int[arraySize];
			} else if (componentType.equals("long")) {
				dst = new long[arraySize];
			} else if (componentType.equals("float")) {
				dst = new float[arraySize];
			} else if (componentType.equals("double")) {
				dst = new double[arraySize];
			} else {
				dst = new JavaObject[arraySize];
			}
			
			try {
				jo.arraycopy(0, dst, 0, arraySize);
			} catch (CorruptDataException e) {
				out.print("\t   <cannot copy data from the array (" +
					Exceptions.getCorruptDataExceptionString() + ")>\n");
				return;
			} catch (MemoryAccessException e) {
				out.print("\t   <cannot copy data from the array (" +
					Exceptions.getMemoryAccessExceptionString() + ")>\n");
				return;
			} catch (UnsupportedOperationException e) { 
				// Some artefacts e.g. phd do not support arraycopy so just put out what we can
				out.print("\t    This is an array of size " + arraySize + " elements\n");
				return;
			}
			
			for (int i = 0; i < arraySize; i++)
			{
				out.print("\t   " + i + ":\t");
				if (componentType.equals("boolean")) {
					out.print(Utils.getVal(Boolean.valueOf(((boolean[]) dst)[i])));
				} else if (componentType.equals("byte")) {
					out.print(Utils.getVal(Byte.valueOf(((byte[]) dst)[i])));
				} else if (componentType.equals("char")) {
					out.print(Utils.getVal(Character.valueOf(((char[]) dst)[i])));
				} else if (componentType.equals("short")) {
					out.print(Utils.getVal(Short.valueOf(((short[]) dst)[i])));
				} else if (componentType.equals("int")) {
					out.print(Utils.getVal(Integer.valueOf(((int[]) dst)[i])));
				} else if (componentType.equals("long")) {
					out.print(Utils.getVal(Long.valueOf(((long[]) dst)[i])));
				} else if (componentType.equals("float")) {
					out.print(Utils.getVal(Float.valueOf(((float[]) dst)[i])));
				} else if (componentType.equals("double")) {
					out.print(Utils.getVal(Double.valueOf(((double[]) dst)[i])));
				} else {
					out.print(Utils.getVal(((JavaObject[]) dst)[i]));
				}
				out.print("\n");
			}
		}
		else
		{	
			
			JavaClass initialJC = jc;
			List<JavaClass> classList = new ArrayList<JavaClass>(); 
			while (jc != null){
				
				classList.add(jc);
				try {
					jc = jc.getSuperclass();
				} catch (CorruptDataException d) {
					jc = null;
				}
			}
			for (int i = (classList.size()-1); i >=0 ; i--){
				
				jc = classList.get(i);
				Iterator itField = jc.getDeclaredFields();
				
				if (itField.hasNext()){
					if (jc.equals(initialJC)){
						out.print("\t   declared fields:\n");
					} else {
						out.print("\t   fields inherited from \"");
						try {
							out.print(jc.getName() + "\":\n");
						} catch (CorruptDataException d) {
							out.print(Exceptions.getCorruptDataExceptionString());
						}
					}
				}
				
				while (itField.hasNext())
				{
					JavaField jf = (JavaField)itField.next();
					boolean isStatic;
					
					try {
						isStatic = Modifier.isStatic(jf.getModifiers());
					} catch (CorruptDataException e) {
						out.print("\t   <error while getting modifier for field \"");
						try {
							out.print(jf.getName());
						} catch (CorruptDataException d) {
							out.print(Exceptions.getCorruptDataExceptionString());
						}
						out.print("\", " + Exceptions.getCorruptDataExceptionString() + ">");
						isStatic = true;
					}
					
					if (!isStatic)
					{
						printNonStaticFieldData(jo, jf, out);
					}
				}
			}
		}
		out.print("\n");
	}
	
	private static void printStaticFieldData(JavaField jf, PrintStream out)
	{
		printFieldData(null, jf, out, true);
	}
	
	private static void printNonStaticFieldData(JavaObject jo, JavaField jf, PrintStream out)
	{
		printFieldData(jo, jf, out, false);
	}
	
	private static void printFieldData(JavaObject jo, JavaField jf, PrintStream out, boolean isStatic)
	{
		String signature;
		
		out.print("\t    ");

		try {
			String modifierString = Utils.getFieldModifierString(jf);
			out.print(modifierString);
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
		}

		try {
			signature = jf.getSignature();
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
			signature = null;
		}
		if (null != signature)
		{
			String name = Utils.getSignatureName(signature);
			if (null == name) {
				out.print("<unknown>");
			} else {
				out.print(name);
			}
		}
		
		out.print(" ");
		
		try {
			out.print(jf.getName());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
		}
		
		if (isStatic || null != jo) {
			out.print(" = ");
			out.print(Utils.getVal(jo, jf));
		}
		out.print("\n");
	}
	
	public static void printMethods(Iterator methods, PrintStream out)
	{
		while(methods.hasNext()) {
			JavaMethod jMethod = (JavaMethod)methods.next();
			try{
				out.print("Bytecode range(s): ");
				Iterator imageSections = jMethod.getBytecodeSections();
				boolean firstSectionPassed = false;
				while (imageSections.hasNext()){
					ImageSection is = (ImageSection)imageSections.next();
					long baseAddress = is.getBaseAddress().getAddress();
					long endAddress = baseAddress + is.getSize();
					if (firstSectionPassed) {
						out.print(", ");
					}
					out.print(Long.toHexString(baseAddress) + " -- " +
							Long.toHexString(endAddress));
					firstSectionPassed = true;
				}
				out.print(":  ");
				
				
				String signature;
				
				try {
					out.print(Utils.getMethodModifierString(jMethod));
				} catch (CorruptDataException e) {
					out.print(Exceptions.getCorruptDataExceptionString());
				}

				try {
					signature = jMethod.getSignature();
				} catch (CorruptDataException e) {
					out.print(Exceptions.getCorruptDataExceptionString());
					signature = null;
				}
				if (null != signature)
				{
					String name = Utils.getReturnValueName(signature);
					if (null == name) {
						out.print("<unknown>");
					} else {
						out.print(name);
					}
				}
				
				out.print(" ");
				out.print(jMethod.getName());

				if (null != signature)
				{
					String name = Utils.getMethodSignatureName(signature);
					if (null == name) {
						out.print("<unknown>");
					} else {
						out.print(name);
					}
				}
				
				out.print("\n");
			}catch (CorruptDataException cde){
				out.print("N/A (CorruptDataException occurred)");
			}
		}
	}

	public static String printRuntimeClassAndLoader(final JavaClass jc, final PrintStream out) {
		String classLoaderInfo;
		String classLoaderClass;
		String cdeInfo = "N/A (CorruptDataException occurred)";
		try {
			JavaClassLoader jClassLoader = jc.getClassLoader();
			classLoaderInfo = "0x" + Long.toHexString(jClassLoader.getObject().getID().getAddress());
			classLoaderClass = jClassLoader.getObject().getJavaClass().getName();
		} catch (CorruptDataException cde){
			classLoaderInfo = null;
			classLoaderClass = cdeInfo;
		}
		String spaces = "    ";
		out.print(spaces);
		out.print("Class ID = 0x" + Long.toHexString(jc.getID().getAddress()));
		out.print(spaces);
		out.print("Class Loader " + classLoaderClass + "(" + classLoaderInfo + ")");
		out.print("\n");
		return classLoaderInfo;
	}
}
