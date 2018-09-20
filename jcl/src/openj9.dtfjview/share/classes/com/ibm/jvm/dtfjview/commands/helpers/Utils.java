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

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.lang.reflect.Modifier;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.Stack;
import java.util.StringTokenizer;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.runtime.ManagedRuntime;

@SuppressWarnings({"rawtypes","unchecked"})
public class Utils {

	public static final String byteToAscii =
		  "................" +
		  "................" +
		  " !\"#$%&'()*+'-./" +
		  "0123456789:;<=>?" +
		  "@ABCDEFGHIJKLMNO" +
		  "PQRSTUVWXYZ[\\]^_"+
		  "`abcdefghijklmno" +
		  "pqrstuvwxyz{|}~." +
		  "................" +
		  "................" +
		  "................" +
		  "................" +
		  "................" +
		  "................" +
		  "................" +
		  "................"
		 ;
	
	public static final String byteToEbcdic =
		  "................" +
		  "................" +
		  "................" +
		  "................" +
		  "................" +
		  "................" +
		  "................" +
		  "................" +
		  ".abcdefghi......" +
		  ".jklmnopqr......" +
		  "..stuvwxyz......" +
		  "................" +
		  ".ABCDEFGHI......" +
		  ".JKLMNOPQR......" +
		  "..STUVWXYZ......"+
		  "0123456789......"
		 ;
	
	//List of keys used by HashMap object associated with each session 
	public static final String CURRENT_MEM_ADDRESS = "currentMemPtrAddress";
	public static final String CURRENT_NUM_BYTES_TO_PRINT = "currentNumBytes";
	public static final String RootCommand_OBJECT = "RootCommandObject";
	public static final String FIND_ATTRIBUTES = "FindAttributes";
	
	private static final int CLASS_MODIFIERS_MASK = 0xc1f;
	private static final int INTERFACE_MODIFIERS_MASK = 0xe0f;
	private static final int CONSTRUCTOR_MODIFIERS_MASK = 0x7;
	private static final int METHOD_MODIFIERS_MASK = 0xd3f;
	private static final int FIELD_MODIFIERS_MASK = 0xdf;

	public static final String SORT_BY_SIZE_FLAG = "-sort:size";
	public static final String SORT_BY_NAME_FLAG = "-sort:name";
	public static final String SORT_BY_ADDRESS_FLAG = "-sort:address";
	public static final String SORT_BY_COUNT_FLAG = "-sort:count";
	public static final String REVERSE_SORT_FLAG = "-sort:reverse";
	public static final String VERBOSE_FLAG = "-verbose";
	
	public static Iterator getRuntimes(Image loadedImage)
	{
		Vector runtimes = new Vector();
		Iterator itAddressSpace;
		Iterator itProcess;
		Iterator itRuntime;
		ManagedRuntime mr;
		ImageAddressSpace ias;
		ImageProcess ip;
		
		itAddressSpace = loadedImage.getAddressSpaces();
		while (itAddressSpace.hasNext()) {
			ias = (ImageAddressSpace)itAddressSpace.next();
			itProcess = ias.getProcesses();
			while (itProcess.hasNext())
			{
				ip = (ImageProcess)itProcess.next();
				itRuntime = ip.getRuntimes();
				while (itRuntime.hasNext()) {
					// this iterator can contain ManagedRuntime or CorruptData objects
					Object next = itRuntime.next();
					if (next instanceof CorruptData) {
						continue; // skip any CorruptData objects
					} else {
						mr = (ManagedRuntime)next;
						if (!runtimes.contains(mr))
							runtimes.add(mr);
					}
				}
			}
		}
		return runtimes.iterator();
	}	

	/**
	 * Format an ImagePointer for printing.
	 * Get the address and format it as a hex string with 0x at the front
	 * 
	 * @param p the ImagePointer
	 * @return p.getAddress() as a hex string
	 */
	public static String toHex(ImagePointer p)
	{
		return toHex(p.getAddress());
	}
	
	public static String toHex(long l)
	{
		return "0x" + Long.toHexString(l);
	}
	
	public static String toHex(int i)
	{
		return "0x" + Integer.toHexString(i);
	}
	
	public static String toHex(short s)
	{
		// bit-wise AND is done to cancel the sign-extension that int casting does;
		//  if a short's value is -1, its hex representation is 0xffff, which will be
		//  sign-extended to 0xffffffff when cast to an int, but we want to know the hex
		//  representation before the short is cast to an int, which the bit-wise AND does
		return "0x" + Integer.toHexString(s & 0x0000ffff);
	}
	
	public static String toHex(byte b)
	{
		// bit-wise AND is done to cancel the sign-extension that int casting does;
		//  if a byte's value is -1, its hex representation is 0xff, which will be
		//  sign-extended to 0xffffffff when cast to an int, but we want to know the hex
		//  representation before the byte is cast to an int, which the bit-wise AND does
		return "0x" + Integer.toHexString(b & 0x000000ff);
	}
	
	public static String toFixedWidthHex(long l)
	{
		return Utils.padWithZeroes(Long.toHexString(l), 16);
	}
	
	public static String toFixedWidthHex(int i)
	{
		return Utils.padWithZeroes(Integer.toHexString(i), 8);
	}
	
	public static String toFixedWidthHex(short s)
	{
		// bit-wise AND is done to cancel the sign-extension that int casting does;
		//  if a short's value is -1, its hex representation is 0xffff, which will be
		//  sign-extended to 0xffffffff when cast to an int, but we want to know the hex
		//  representation before the short is cast to an int, which the bit-wise AND does
		return Utils.padWithZeroes(Integer.toHexString(s & 0x0000ffff), 4);
	}
	
	public static String toFixedWidthHex(byte b)
	{
		// bit-wise AND is done to cancel the sign-extension that int casting does;
		//  if a byte's value is -1, its hex representation is 0xff, which will be
		//  sign-extended to 0xffffffff when cast to an int, but we want to know the hex
		//  representation before the byte is cast to an int, which the bit-wise AND does
		return Utils.padWithZeroes(Integer.toHexString(b & 0x000000ff), 2);
	}
	
	public static Stack<String> constructStackFromString(String args)
	{
		String[] argsArray = args.split("\\s+");
		return constructStackFromStringArray(argsArray);
	}
	
	public static Stack<String> constructStackFromStringArray(String[] argsArray){
		Stack<String> s = new Stack<String>();
		for (int i = argsArray.length - 1; i >= 0; i--){
			s.push(argsArray[i]);
		}
		return s;
	}
	
	public static String concatArgsFromStack(Stack<String> args){
		String s = "";
		while(!args.empty()){
			s = s.concat((String)args.pop());
		}
		return s;
	}
	
	//CMVC 175488 : fixed so that this function returns an address space which contains a process
	//rather than the first address space it finds.
	public static ImageAddressSpace _extractAddressSpace(Image loadedImage)
	{
		ImageAddressSpace space = null;
		Iterator spaces = loadedImage.getAddressSpaces();
		
		Object obj = null;			//object returned through DTFJ iterators
		while (spaces.hasNext()) {
			obj = spaces.next();
			if(obj instanceof ImageAddressSpace) {
				space = (ImageAddressSpace) obj;
				Iterator procs = space.getProcesses();
				while(procs.hasNext()) {
					obj = procs.next();
					if(obj instanceof ImageProcess) {
						return space;
					}
				}
			}
		}
		//failed to find an address space which contains a process.
		return null;
	}
	
	public static Long longFromString(String value)
	{
		Long translated = null;
		
		if (null != value) {
			if (value.startsWith("0x")) {
				value = value.substring(2);
			}
			try {
				translated = Long.valueOf(value, 16);
			} catch (NumberFormatException e) {
				try {
					// parseLong does not successfully read in numbers > 2^63 e.g. "ffffffffff600000"
					// have another try going via a big integer
					BigInteger big = new BigInteger(value,16);
					translated = Long.valueOf(big.longValue());
				} catch (NumberFormatException e2) {
					// whatever the problem was it was not just to do with being > 2^63
					translated = null;
				}
			}
		}
		return translated;
	}
	
	public static Long longFromStringWithPrefix(String value)
	{
		Long translated = null;
		
		if (value.startsWith("0x"))
		{
			translated = longFromString(value);
		}
		
		return translated;
	}
	
	public static String getSignatureName(String signature)
	{
		Hashtable table = new Hashtable();
		String retval = null;
		
		table.put("Z", "boolean");
		table.put("B", "byte");
		table.put("C", "char");
		table.put("S", "short");
		table.put("I", "int");
		table.put("J", "long");
		table.put("F", "float");
		table.put("D", "double");
		table.put("V", "void");
		
		if (signature.startsWith("L"))
		{
			StringTokenizer st = new StringTokenizer(signature.substring(1), ";");
			String className;
			if (st.hasMoreTokens()) {
				className = st.nextToken();
				if (className.startsWith("java/lang/"))
				{
					className = className.substring(10);
				}
				retval = className.replaceAll("/", ".");
			} else {
				// nothing or only ";"s after opening "L"
				return null;
			}
		}
		else if (signature.startsWith("["))
		{
			String currSig = new String(signature);
			String arraySuffix = "";
			
			while (currSig.startsWith("["))
			{
				arraySuffix += "[]";
				currSig = currSig.substring(1);
			}
			
			retval = Utils.getSignatureName(currSig) + arraySuffix;
		}
		else
		{
			retval = (String)table.get(signature.substring(0,1));
		}
		
		return retval;
	}
	
	public static String getMethodSignatureName(String signature)
	{
		Hashtable table = new Hashtable();
		String retval = "";
		
		table.put("Z", "boolean");
		table.put("B", "byte");
		table.put("C", "char");
		table.put("S", "short");
		table.put("I", "int");
		table.put("J", "long");
		table.put("F", "float");
		table.put("D", "double");
		
		if (signature.startsWith("("))
		{
			StringTokenizer st = new StringTokenizer(signature.substring(1), ")");
			if (st.hasMoreTokens()) {
				String parameters = st.nextToken();
				if (st.hasMoreTokens()) {
					retval = "(" + Utils.getMethodSignatureName(parameters) + ")";
				} else {
					// there is nothing between "(" and ")"
					retval = "()";
				}
			} else {
				// there is no ")" after opening "("
				retval = null;
			}
		}
		else if (signature.startsWith("L"))
		{
			StringTokenizer st = new StringTokenizer(signature.substring(1), ";");
			String className;
			if (st.hasMoreTokens()) {
				className = st.nextToken();
				if (className.startsWith("java/lang/"))
				{
					className = className.substring(10);
				}
				retval = className.replaceAll("/", ".");
				if (st.hasMoreTokens()) {
					retval += ", " + Utils.getMethodSignatureName(
								signature.substring(signature.indexOf(';') + 1)
							);
				}
			} else {
				// nothing or only ";"s after opening "L"
				retval = null;
			}
		}
		else if (signature.startsWith("["))
		{
			String currSig = new String(signature);
			String arraySuffix = "";
			
			while (currSig.startsWith("["))
			{
				arraySuffix += "[]";
				currSig = currSig.substring(1);
			}
			
			if (currSig.startsWith("L")) {
				if (currSig.indexOf(';') + 1 == currSig.length()) {
					retval = Utils.getSignatureName(currSig) + arraySuffix;
				} else {
					retval = Utils.getMethodSignatureName(
							currSig.substring(0, currSig.indexOf(';') + 1)
						);
					retval += arraySuffix + ", ";
					retval += Utils.getMethodSignatureName(
							currSig.substring(currSig.indexOf(';') + 1)
						);
				}
			} else {
				if (1 == currSig.length()) {
					retval = (String)table.get(currSig) + arraySuffix;
				} else {
					retval = (String)table.get(currSig.substring(0,1)) + arraySuffix;
					retval += ", ";
					retval += Utils.getMethodSignatureName(currSig.substring(1));
				}
			}
		}
		else
		{
			retval = (String)table.get(signature.substring(0,1));
			if (signature.length() > 1) {
				retval += ", " + Utils.getMethodSignatureName(signature.substring(1));
			}
		}
		
		return retval;
	}
	
	public static String getReturnValueName(String signature)
	{
		if (signature.startsWith("("))
		{
			StringTokenizer st = new StringTokenizer(signature, ")");
			if (st.hasMoreTokens()) {
				st.nextToken();	// we don't care what's inside the brackets
				if (st.hasMoreTokens()) {
					String retval = st.nextToken();
					if (st.hasMoreTokens()) {
						// there is more than one ")"; that's one too many
						return null;
					} else {
						return Utils.getSignatureName(retval);
					}
				} else {
					// there is no ")" after opening "(" or the ")" is followed by nothing
					return null;
				}
			}
			else {
				// nothing after opening "("
				return null;
			}
		}
		else
		{
			return null;
		}
	}
	
	public static String toString(String s)
	{
		if (null == s)
			return "null";
		else
			return s;
	}
	
	public static File absPath(Properties properties, String path)
	{
		File oldPwd = (File)properties.get("pwd");
		File newPwd = new File(path);

		if (!newPwd.isAbsolute())
			newPwd = new File(oldPwd, path);

		try {
			newPwd = newPwd.getCanonicalFile();
		} catch (IOException e) {
		
		}
		
		return newPwd;
	}
	
	public static String getVal(Object o)
	{
		return Utils.getVal(o, null, null);
	}
	
	// note: this method lets you pass in a null JavaObject, but it _will_ throw a
	//  NullPointerException if the JavaField is not a static field when you pass it
	//  a null JavaObject; this is the behavior of jf.get() and jf.getString()
	public static String getVal(JavaObject jo, JavaField jf)
	{
		Object o;
		String s;
		
		if (null == jf)
		{
			o = jo;
			s = null;
		}
		else {
			try {
				o = jf.get(jo);
			} catch (CorruptDataException e) {
				return "<corrupt data>";
			} catch (MemoryAccessException d) {
				return "<invalid memory address>";
			} catch (NumberFormatException nfe) {
				return "<invalid number>";
			}
			
			try {
				s = jf.getString(jo);
			} catch (CorruptDataException e) {
				s = null;
			} catch (MemoryAccessException e) {
				s = null;
			} catch (IllegalArgumentException e) {
				s = null;
			}
		}
		
		return getVal(o, s, jf);
	}
	
	public static String getVal(Object o, String str, JavaField jf)
	{
		String val = "";
		Long value = null;
		boolean object = false;

		if (null == o)
		{
			val += "null";
		}
		else
		{
			if (o instanceof Boolean) {
				val += ((Boolean) o).toString();
			} else if (o instanceof Byte) {
				byte b = ((Byte) o).byteValue();
				val += String.valueOf(b);
				value = Long.valueOf(b);
			} else if (o instanceof Character) {
				char c = ((Character) o).charValue();
				val += Utils.getPrintableWithQuotes(c);
				value = Long.valueOf(c);
			} else if (o instanceof Double) {
				double d = ((Double) o).doubleValue();
				val += String.valueOf(d);
				value = Long.valueOf(Double.doubleToRawLongBits(d));
			} else if (o instanceof Float) {
				float f = ((Float) o).floatValue();
				val += String.valueOf(f);
				value = Long.valueOf(Float.floatToRawIntBits(f));
			} else if (o instanceof Integer) {
				int i = ((Integer) o).intValue();
				val += String.valueOf(i);
				value = Long.valueOf(i);
			} else if (o instanceof Long) {
				long l = ((Long) o).longValue();
				val += String.valueOf(l);
				value = Long.valueOf(l);
			} else if (o instanceof Short) {
				short s = ((Short) o).shortValue();
				val += String.valueOf(s);
				value = Long.valueOf(s);
			} else if (o instanceof String) {
				val += (String) o;
			} else if (o instanceof JavaObject) {
				if (Utils.isNull((JavaObject) o)) {
					val += "null";
				} else {
					object = true;
				}
			} else {
				// FIXME
			}
			
			// why are we processing objects here?
			//  because we want control over the exceptions that are thrown
			if (object)
			{
				JavaObject joField = (JavaObject)o;
				JavaClass jcField;
				String jcName;
				
				try {
					jcField = joField.getJavaClass();
				} catch (CorruptDataException e) {
					jcField = null;
				}
				
				try {
					if (null != jcField) {
						jcName = jcField.getName();
					} else {
						jcName = null;
					}
				} catch (CorruptDataException e) {
					jcName = null;
				}
				
				if (null != jcName && jcName.equals("java/lang/String"))
				{
					if (null == str) {
						val += getStringVal(joField);
					} else {
						val += "\"" + Utils.getPrintable(str) + "\"";
					}
					val += " @ ";
					val += Utils.toHex(joField.getID().getAddress());
				} else {
					val += "<object>" + " @ " + Utils.toHex(joField.getID().getAddress());
				}
			}
		}

		if (null != value)
		{
			val += " (";
			val += Utils.toHex(value.longValue());
			val += ")";
		}
		
		return val;
	}
	
	private static String getStringVal(JavaObject jo)
	{
		JavaClass jc;
		
		try {
			jc = jo.getJavaClass();
		} catch (CorruptDataException e) {
			return "<cannot get String class from String object (" +
				Exceptions.getCorruptDataExceptionString() + ")>";
		}
		
		Iterator itJavaField = jc.getDeclaredFields();
		JavaField jf = null;
		while (itJavaField.hasNext())
		{
			jf = (JavaField)itJavaField.next();
			try {
				if (jf.getSignature().equals("[C") && !Modifier.isStatic(jf.getModifiers()))
					break;
			} catch (CorruptDataException e) {
				// if we have an exception, do nothing and go onto the next field 
			}
		}
		if (jf == null) {
			// empty field iterator (occurs e.g. when reading PHD heapdumps), can't get the char array
			return "<cannot get char array out of String>";
		}
		
		JavaObject charArray = null;
		try {
			charArray = (JavaObject)jf.get(jo);
		} catch (CorruptDataException e) {
			return "<cannot get char array out of String (" +
				Exceptions.getCorruptDataExceptionString() + ")>";
		} catch (MemoryAccessException e) {
			return "<cannot get char array out of String (" +
				Exceptions.getMemoryAccessExceptionString() + ")>";
		}
		
		int arraySize;
		try {
			arraySize = charArray.getArraySize();
		} catch (CorruptDataException e) {
			return "<cannot determine the size of the array (" +
					Exceptions.getCorruptDataExceptionString() + ")>";
		}
		
		char[] dst = new char[arraySize];
		
		try {
			charArray.arraycopy(0, dst, 0, arraySize);
		} catch (CorruptDataException e) {
			return "<cannot copy data from the array (" +
				Exceptions.getCorruptDataExceptionString() + ")>";
		} catch (MemoryAccessException e) {
			return "<cannot copy data from the array (" +
				Exceptions.getMemoryAccessExceptionString() + ")>";
		}
		
		return "\"" + Utils.getPrintable(new String(dst)) + "\"";
	}
	

	public static String getPrintable(char c)
	{
		// for a good set of test cases for this code, try using it to print the
		//  static char arrays of the String class; the memory addresses of these arrays
		//  can be found by using the command "x/j java/lang/String" and looking at the
		//  static fields printed at the top of the output
		
		switch (c)
		{
		// the following 8 cases were taken from Section 3.10.6 of the
		//  Java Language Specification (2nd Edition), which can be found at
		//  http://java.sun.com/docs/books/jls/second_edition/html/lexical.doc.html#101089
		case '\b':
			return "\\b";
		case '\t':
			return "\\t";
		case '\n':
			return "\\n";
		case '\f':
			return "\\f";
		case '\r':
			return "\\r";
		case '\"':
			return "\\\"";
		case '\'':
			return "\\'";
		case '\\':
			return "\\\\";
		default:
			int i = (int)c;
			if (i <= 255) {
				if (i >= 32 && i <= 126) {
					// if the character is easily printable (has an integer value between
					//  32 and 126), then just print it
					// note: this was determined by looking at http://www.lookuptables.com/
					return String.valueOf(c);
				} else {
					// if the character is not easily printable, print an octal escape;
					//  this is done according to the Section 3.10.6 of the Java
					//  Language Specification (see above for reference)
					return "\\" + Utils.padWithZeroes(Integer.toOctalString(i), 3);
				}
			} else {
				// if we have a character that is not between Unicode values
				//  \u0000 and \u00ff, then print an escaped form of its Unicode value;
				//  the format was determined from Section 3.3 of the
				//  Java Language Specification (2nd Edition), which can be found at
				//  http://java.sun.com/docs/books/jls/second_edition/html/lexical.doc.html#100850
				return "\\u" + Utils.padWithZeroes(Integer.toHexString(i), 4);
			}
		}
	}
	
	public static String getPrintableWithQuotes(char c)
	{
		return "\'" + Utils.getPrintable(c) + "\'";
	}
	
	public static String getPrintable(String s)
	{
		String retval = "";
		
		for (int i = 0; i < s.length(); i++)
		{
			retval += Utils.getPrintable(s.charAt(i));
		}
		
		return retval;
	}
	
	public static String padWithZeroes(String unpadded, int desiredLength)
	{
		String output = new String(unpadded);
		for (int i = unpadded.length(); i < desiredLength; i++)
		{
			output = "0" + output;
		}
		return output;
	}

	public static Iterator getAddressSpaceSectionInfo(Image loadedImage) {
		Iterator itAddressSpace = loadedImage.getAddressSpaces();
		Vector vSections = new Vector();
		while(itAddressSpace.hasNext()){
			ImageAddressSpace imageAddressSpace = (ImageAddressSpace)itAddressSpace.next();
			Iterator iSections = imageAddressSpace.getImageSections();
			while(iSections.hasNext()){
				vSections.add(iSections.next());
			}
		}
		return vSections.iterator();
	}
	
	public static String getFieldModifierString(JavaField jf) throws CorruptDataException{
		int modifiers = jf.getModifiers();
		int typeMask = FIELD_MODIFIERS_MASK;
		return getModifierString(modifiers, typeMask);
	}
	
	public static String getMethodModifierString(JavaMethod jm) throws CorruptDataException{
		int modifiers = jm.getModifiers();
		int typeMask = ("<init>".equals(jm.getName()))?CONSTRUCTOR_MODIFIERS_MASK:METHOD_MODIFIERS_MASK;
		return getModifierString(modifiers, typeMask);
	}
	
	public static String getClassModifierString(JavaClass jc) throws CorruptDataException{
		int modifiers = jc.getModifiers();
		int typeMask = Modifier.isInterface(modifiers)?INTERFACE_MODIFIERS_MASK|Modifier.INTERFACE:CLASS_MODIFIERS_MASK;
		return getModifierString(modifiers, typeMask);
	}
	
	private static String getModifierString(int modifiers, int typeMask){
		if (modifiers == JavaClass.MODIFIERS_UNAVAILABLE) {
			return "No modifiers available";
		}
		modifiers = modifiers & typeMask;
		String retval = "";
		if(Modifier.isPublic(modifiers)) retval += "public ";
		if(Modifier.isPrivate(modifiers)) retval += "private ";
		if(Modifier.isProtected(modifiers))  retval += "protected ";
		if(Modifier.isStatic(modifiers)) retval += "static ";
		if(Modifier.isAbstract(modifiers)) retval += "abstract ";
		if(Modifier.isFinal(modifiers)) retval += "final ";
		if(Modifier.isSynchronized(modifiers)) retval += "synchronized ";
		if(Modifier.isVolatile(modifiers)) retval += "volatile ";
		if(Modifier.isTransient(modifiers)) retval += "transient ";
		if(Modifier.isNative(modifiers)) retval += "native ";
		if(Modifier.isInterface(modifiers)) retval += "interface ";
		if(Modifier.isStrict(modifiers)) retval += "strict ";
		return retval;
	}
	
	public static boolean isNull(JavaObject jo)
	{
		return jo.getID().getAddress() == 0;
	}
	
	public static String padWithSpaces(String unpadded, int desiredLength)
	{
		String output = new String(unpadded);
		for (int i = unpadded.length(); i < desiredLength; i++)
		{
			output += " ";
		}
		return output;
	}
	
	public static String prePadWithSpaces(String unpadded, int desiredLength)
	{
		String output = "";
		for (int i = unpadded.length(); i < desiredLength; i++)
		{
			output += " ";
		}
		output += unpadded;
		return output;
	}
	
	public static JavaClass[] getClassGivenName(String className, JavaRuntime jr, PrintStream out)
	{
		Iterator itJavaClassLoader = jr.getJavaClassLoaders();
		List classes = new ArrayList();
		
		while (itJavaClassLoader.hasNext() )
		{
			Object nextCl = itJavaClassLoader.next();
			if( !(nextCl instanceof JavaClassLoader) ) {
				continue;
			}
			JavaClassLoader jcl = (JavaClassLoader)nextCl;
			Iterator itJavaClass = jcl.getDefinedClasses();
			while (itJavaClass.hasNext() )
			{
				Object next = itJavaClass.next();
				if( !(next instanceof JavaClass) ) {
					continue;
				}
				JavaClass jc = (JavaClass)next;
				String currClassName;

				try {
					currClassName = jc.getName();
					if (currClassName.equals(className))
					{
						classes.add(jc);
					}
				} catch (CorruptDataException e) {
					out.print("\t  <error getting class name while traversing classes: ");
					out.print(Exceptions.getCorruptDataExceptionString());
					out.print(">\n");
					currClassName = null;
				}
			}
		}

		if( classes.size() > 0 ) {
			return (JavaClass[])classes.toArray(new JavaClass[0]);
		} else {
			return null;
		}
	}
	
	public static JavaClass getClassGivenAddress(long address, JavaRuntime jr)
	{
		Iterator itJavaClassLoader = jr.getJavaClassLoaders();
		
		while (itJavaClassLoader.hasNext() )
		{
			Object nextCl = itJavaClassLoader.next();
			if( !(nextCl instanceof JavaClassLoader) ) {
				continue;
			}
			JavaClassLoader jcl = (JavaClassLoader)nextCl;
			Iterator itJavaClass = jcl.getDefinedClasses();
			while (itJavaClass.hasNext() )
			{
				
				Object next = itJavaClass.next();
				if( !(next instanceof JavaClass) ) {
					continue;
				}
				if( next instanceof JavaClass ) {
					JavaClass jc = (JavaClass)next;

					long currClassAddress = jc.getID().getAddress();
					if (currClassAddress == address )
					{
						return jc;
					}
				}
			}
		}

		return null;
	}
	
	/* Find the JavaThread that owns a blocking object by finding the java/lang/Thread
	 * JavaObject of the owner and then looking through the list of JavaThreads to
	 * see who that belongs to. 
	 */
	public static JavaThread getParkBlockerOwner(JavaObject blocker, JavaRuntime r) throws CorruptDataException, MemoryAccessException {
		JavaObject ownerObj = getParkBlockerOwnerObject(blocker, r);
		if( ownerObj == null ) {
			return null;
		}
		Iterator threads = r.getThreads();
		while( threads.hasNext() ) {
			Object lObj = threads.next();
			if( lObj instanceof JavaThread ) {
				JavaThread thread = (JavaThread) lObj;
				if( ownerObj.equals(thread.getObject())) {
					return thread;
				}
			}
		}			
		return null;
	}
	
	/* Given an object that a parked thread has as it's blocker find the java/lang/Thread
	 * object for it's owner. (Not the dtfj JavaThread!)
	 */
	public static JavaObject getParkBlockerOwnerObject(JavaObject blocker, JavaRuntime r) throws CorruptDataException, MemoryAccessException {
		if( blocker == null ) {
			return null;
		}
		JavaClass blockingClass = blocker.getJavaClass();
		while( blockingClass != null && !("java/util/concurrent/locks/AbstractOwnableSynchronizer".equals(blockingClass.getName()) ) ) {
			blockingClass = blockingClass.getSuperclass();
		}
		if( blockingClass != null ) {
			Iterator fields = blockingClass.getDeclaredFields();
			while( fields.hasNext() ) {
				Object f = fields.next();
				if( f instanceof JavaField ) {
					JavaField field = (JavaField) f;
					if( "exclusiveOwnerThread".equals(field.getName())) {
						JavaObject ownerObj = (JavaObject) field.get(blocker);
						return ownerObj;
					}
				}
			}
		}
		return null;
	}
	
	public static String getThreadNameFromObject(JavaObject lockOwnerObj, JavaRuntime rt, PrintStream out) throws CorruptDataException, MemoryAccessException {
		if( lockOwnerObj == null ) {
			return null;
		}
		JavaClass[] threadClasses = Utils.getClassGivenName("java/lang/Thread", rt, out);
		// We might have got no classes or more than one. Both of which should be pretty hard to manage.
		if( threadClasses.length == 1 ) {
			Iterator fields = threadClasses[0].getDeclaredFields();
			while( fields.hasNext() ) {
				Object o = fields.next();
				if( !(o instanceof JavaField) ) {
					continue;
				}
				JavaField f = (JavaField)o;
				if( "name".equalsIgnoreCase(f.getName())) {
					return f.getString(lockOwnerObj);
				}
			}
		}
		return null;
	}
}
