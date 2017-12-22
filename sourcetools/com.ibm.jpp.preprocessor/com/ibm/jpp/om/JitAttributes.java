/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
package com.ibm.jpp.om;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;

/**
 * Format of JCL annotations:
 * [ATTR <Type> <Signature>]
 * where Type is one of:
 * SkipNullChecks
 * SkipBoundChecks
 * SkipCheckCasts
 * SkipDivChecks
 * SkipArrayStoreChecks
 * SkipChecksOnArrayCopies
 * SkipZeroInitializationOnNewarrays
 *
 * These Types are always applied to a method. Signature must be of the
 * form com_ibm_oti_Foo.blah(II)V.
 *
 * or [ATTR Recognized <Signature>]
 *
 * The given method is a "recognized" method (used for special,
 * non-generalizable optimizations).
 *
 * or [ATTR <Type> <Arg> <FQN/Signature>]
 * where Type is one of:
 * Trusted
 * Untrusted
 *
 * Trusted means arguments will not escape the methods. The Trusted
 * attribute may be applied to either a method or a class. If it is
 * applied to a method, Signature must be the same form as above. If it
 * is applied to a class, use the fully qualified class name, like
 * java_lang_Object. Untrusted can only be used for methods, and is used
 * to indicate exceptions to Trusted classes.
 *
 * For Trusted and Untrusted, Arg is an integer indicating for which
 * argument(s) the attribute applies. -1 means all arguments. The number
 * must be formatted such that it can be converted to a Java int using
 * Integer.valueOf().intValue().
 *
 * eg.
 * [ATTR Trusted -1 com_ibm_oti_Foo]
 * All arguments of all methods in com.ibm.oti.Foo will not escape.
 *
 * [ATTR Trusted 2 com_ibm_oti_Foo]
 * The third argument of all methods in the class will not escape.
 *
 * [ATTR Trusted -1 com_ibm_oti_Foo]
 * [ATTR Untrusted -1 com_ibm_oti_Foo.blah()V]
 * All arguments of all methods will not escape, except blah(), where
 * all arguments will escape.
 *
 * [ATTR Trusted 0 com_bim_oti_Foo.blah()V]
 * The first argument of blah() will not escape.
 *
 */
public class JitAttributes {

	static Map<String, Integer> recognizedMethods;

	static {
		/* This set of methods should be synchronized with the
		 * "enum RecognizedMethod" in codegen_common/Symbol.hpp.
		 * ie., the contents and order should be the same.
		 */
		String[] methods = {
			"unknownMethod",
			"java_lang_Double_doubleToLongBits",
			"java_lang_Double_longBitsToDouble",
			"java_lang_Float_floatToIntBits",
			"java_lang_Float_intBitsToFloat",
			"java_lang_Object_getClass",
			"java_lang_Object_hashCodeImpl",
			"java_lang_String_hashCode",
			"java_lang_System_arraycopy",
			"java_lang_System_currentTimeMillis",
			"java_lang_Thread_currentThread",
			"java_lang_Throwable_fillInStackTrace",
			"java_lang_Throwable_printStackTrace",
			"java_util_HashtableHashEnumerator_hasMoreElements",
			"com_ibm_oti_vm_VM_callerClass",
			"com_ibm_oti_vm_VM_callerClassLoader" };
		recognizedMethods = new HashMap<>();
		for (int i = 0; i < methods.length; i++) {
			recognizedMethods.put(methods[i], Integer.valueOf(i));
		}
	}

	AttributesMap map = new AttributesMap();

	public void skipNullChecksFor(String method) {
		checkMethod(method);
		map.getOrAdd(method).skipNullChecks = true;
	}

	public void skipBoundChecksFor(String method) {
		checkMethod(method);
		map.getOrAdd(method).skipBoundChecks = true;
	}

	public void skipCheckCastsFor(String method) {
		checkMethod(method);
		map.getOrAdd(method).skipCheckCasts = true;
	}

	public void skipDivChecksFor(String method) {
		checkMethod(method);
		map.getOrAdd(method).skipDivChecks = true;
	}

	public void skipArrayStoreChecksFor(String method) {
		checkMethod(method);
		map.getOrAdd(method).skipArrayStoreChecks = true;
	}

	public void skipChecksOnArrayCopiesFor(String method) {
		checkMethod(method);
		map.getOrAdd(method).skipChecksOnArrayCopies = true;
	}

	public void skipZeroInitializationOnNewarraysFor(String method) {
		checkMethod(method);
		map.getOrAdd(method).skipZeroInitializationOnNewarrays = true;
	}

	public void addTrustedMethodOrClass(String name, int argNum) {
		if (argNum < -1 || argNum >= 100) {
			throw new PreprocessorException("Argument must be between -1 and 99 inclusive");
		}
		Attributes attr = map.getOrAdd(name);
		attr.trusted = true;
		attr.argNum = argNum;
	}

	public void addUntrustedMethod(String method, int argNum) {
		checkMethod(method);
		if (argNum < -1 || argNum >= 100) {
			throw new PreprocessorException("Argument must be between -1 and 99 inclusive");
		}
		Attributes attr = map.getOrAdd(method);
		attr.untrusted = true;
		attr.argNum = argNum;
	}

	public void addRecognizedMethod(String method) {
		checkMethod(method);

		/* Convert from com_ibm_oti_Foo.blah() to com_ibm_oti_Foo_blah, to match
		 * the format of the enum values.
		 */
		StringBuffer chopped = new StringBuffer(method.substring(0, method.indexOf("(")));
		chopped.setCharAt(method.indexOf("."), '_');

		Integer integer = recognizedMethods.get(chopped.toString());
		if (integer == null) {
			throw new PreprocessorException(method + " is not a recognized method");
		}
		int value = integer.intValue();
		if (value < 0 || value >= 100) {
			throw new PreprocessorException("Not expecting recognizedMethodId < 0 or >= 100");
		}
		Attributes attr = map.getOrAdd(method);
		attr.recognized = true;
		attr.recognizedMethodId = value;
	}

	public void write(String filename) throws IOException {
		if (map.size() == 0) {
			return;
		}
		File outputFile = new File(filename);
		outputFile.mkdirs();
		try (FileOutputStream stream = new FileOutputStream(filename);
			 PrintWriter out = new PrintWriter(stream)) {
			out.print(map.toString());
		}
	}

	private static void checkMethod(String method) {
		boolean valid = true;
		int dot = method.indexOf('.');

		// if the first char is the dot, or there is more
		// than one dot, it's not a valid method name
		if (dot == 0 || method.indexOf('.', dot + 1) > 0) {
			valid = false;
		} else {
			// ensure that there is both a left bracket and a right
			// bracket, and that they appear in the right order.
			int left = method.indexOf('(', dot + 1);
			if (left == -1) {
				valid = false;
			} else {
				int right = method.indexOf(')', left + 1);
				if (right == -1) {
					valid = false;
				}
			}
		}

		if (!valid) {
			throw new PreprocessorException(method + " does not appear to be a valid method name.");
		}
	}
}

class Attributes {
	String name;

	boolean skipNullChecks = false;
	boolean skipBoundChecks = false;
	boolean skipCheckCasts = false;
	boolean skipDivChecks = false;
	boolean skipArrayStoreChecks = false;
	boolean skipChecksOnArrayCopies = false;
	boolean skipZeroInitializationOnNewarrays = false;

	boolean trusted = false;
	boolean untrusted = false;
	int argNum;
	boolean recognized = false;
	int recognizedMethodId;

	@Override
	public String toString() {
		StringBuffer ret = new StringBuffer();

		if (trusted) {
			if (argNum == -1) {
				ret.append("T");
			} else {
				ret.append("t");
				if (argNum < 10) {
					ret.append("0");
				}
				ret.append(argNum);
			}
		} else if (untrusted) {
			if (argNum == -1) {
				ret.append("U");
			} else {
				ret.append("u");
				if (argNum < 10) {
					ret.append("0");
				}
				ret.append(argNum);
			}
		}
		/* These attributes are only for methods */
		if (name.indexOf(".") != -1) {
			if (recognized) {
				ret.append("r");
				if (recognizedMethodId < 10) {
					ret.append("0");
				}
				ret.append(recognizedMethodId);
			}
			ret.append(skipNullChecks ? "y" : "n");
			ret.append(skipBoundChecks ? "y" : "n");
			ret.append(skipCheckCasts ? "y" : "n");
			ret.append(skipDivChecks ? "y" : "n");
			ret.append(skipArrayStoreChecks ? "y" : "n");
			ret.append(skipChecksOnArrayCopies ? "y" : "n");
			ret.append(skipZeroInitializationOnNewarrays ? "y" : "n");
		}

		return ret.toString();
	}
}

class AttributesMap extends HashMap<String, Attributes> {

	/**
	 * serialVersionUID
	 */
	private static final long serialVersionUID = 3257003276233683256L;

	public Attributes getOrAdd(String name) {
		Attributes attr = get(name);
		if (attr == null) {
			attr = new Attributes();
			attr.name = name;
			put(name, attr);
		}
		return attr;
	}

	@Override
	public String toString() {
		StringBuffer ret = new StringBuffer();

		for (Attributes next : values()) {
			ret.append(next.name);
			ret.append("\n");
			ret.append(next.toString());
			ret.append("\n");
		}

		return ret.toString();
	}

}
