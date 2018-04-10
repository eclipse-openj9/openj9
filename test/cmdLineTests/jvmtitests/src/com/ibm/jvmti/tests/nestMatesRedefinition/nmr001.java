/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

package com.ibm.jvmti.tests.nestMatesRedefinition;

public class nmr001 {
	/*
	 * Redefines a class with given class bytes
	 * 
	 * @param name class being redefined
	 * @param classBytesSize size of the new class bytes
	 * @param classBytes new class bytes
	 * 
	 * @return	jvmti error code
	 */
	public static native int redefineClass(Class name, int classBytesSize, byte[] classBytes);

	/*
	 * Every JVMTI function returns a jvmtiError code with values as defined
	 * by the specification.
	 */
	static final int JVMTI_ERROR_NONE = 0;
	static final int JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_ATTRIBUTE_CHANGED = 72;
	
	static boolean runRedefinitionTest(int expectedErrorCode, String className, byte[] originalDefinition, byte[] newDefinition) {
		int redefinitionErrorCode = 0;
		try {
			CustomClassLoader classloader = new CustomClassLoader();
			Class<?> clazz = classloader.getClass(className, originalDefinition);
			Object instance = clazz.getDeclaredConstructor().newInstance();
			redefinitionErrorCode = redefineClass(clazz, newDefinition.length, newDefinition);
		} catch (Exception e) {
			return false;
		}
		return expectedErrorCode == redefinitionErrorCode;
	}
	
	/* 
	 * Valid class definitions have the same nestmates attributes as the original
	 * definition - either no nestmates attribute, a nest host attribute with the 
	 * same nest host, or a nest members attribute with the same nest members in 
	 * the same order.
	 */

	/*
	 * Tests valid nest redefinition with a nest host attribute
	 * 
	 * @return boolean true if test passes; false otherwise
	 */
	static public boolean testRedefinitionWithNestHostAttribute() {
		try {
			byte[] originalBytes = ClassGenerator.testClassWithNestMembersAttribute();
			byte[] redefinedBytes = ClassGenerator.testClassWithNestMembersAttributeForceRedefinition();
			return runRedefinitionTest(JVMTI_ERROR_NONE, "TestClass", originalBytes, redefinedBytes);
		} catch (Exception e) {
			return false;
		}
	}
	
	/*
	 * Tests valid nest redefinition with a nest members attribute
	 * 
	 * @return boolean true if test passes; false otherwise
	 */
	static public boolean testRedefinitionWithNestMembersAttribute() {
		try {
			byte[] originalBytes = ClassGenerator.testClassWithNestHostAttribute();
			byte[] redefinedBytes = ClassGenerator.testClassWithNestHostAttributeForceRedefinition();
			return runRedefinitionTest(JVMTI_ERROR_NONE, "TestClass", originalBytes, redefinedBytes);
		} catch (Exception e) {
			return false;
		}
	}

	/* 
	 * It is disallowed to add or to remove a nest host or nest members attribute
	 * when redefining a class.
	 */
	
	/*
	 * Tests invalid nest redefinition by adding a nest host attribute
	 * 
	 * @return boolean true if test passes; false otherwise
	 */
	static public boolean testRedefinitionAddingNestHostAttribute() {
		try {
			byte[] originalBytes = ClassGenerator.testClassNoAttribute();
			byte[] redefinedBytes = ClassGenerator.testClassWithNestHostAttribute();
			return runRedefinitionTest(JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_ATTRIBUTE_CHANGED, "TestClass", originalBytes, redefinedBytes);
		} catch (Exception e) {
			return false;
		}
	}
	
	/*
	 * Tests invalid nest redefinition by adding a nest members attribute
	 * 
	 * @return boolean true if test passes; false otherwise
	 */
	static public boolean testRedefinitionAddingNestMembersAttribute() {
		try {
			byte[] originalBytes = ClassGenerator.testClassNoAttribute();
			byte[] redefinedBytes = ClassGenerator.testClassWithNestMembersAttribute();
			return runRedefinitionTest(JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_ATTRIBUTE_CHANGED, "TestClass", originalBytes, redefinedBytes);
		} catch (Exception e) {
			return false;
		}
	}
	
	/*
	 * Tests invalid nest redefinition by removing a nest host attribute
	 * 
	 * @return boolean true if test passes; false otherwise
	 */
	static public boolean testRedefinitionRemovingNestHostAttribute() {
		try {
			byte[] originalBytes = ClassGenerator.testClassWithNestHostAttribute();
			byte[] redefinedBytes = ClassGenerator.testClassNoAttribute();
			return runRedefinitionTest(JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_ATTRIBUTE_CHANGED, "TestClass", originalBytes, redefinedBytes);
		} catch (Exception e) {
			return false;
		}
	}
	
	/*
	 * Tests invalid nest redefinition by removing a nest members attribute
	 * 
	 * @return boolean true if test passes; false otherwise
	 */
	static public boolean testRedefinitionRemovingNestMembersAttribute() {
		try {
			byte[] originalBytes = ClassGenerator.testClassWithNestMembersAttribute();
			byte[] redefinedBytes = ClassGenerator.testClassNoAttribute();
			return runRedefinitionTest(JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_ATTRIBUTE_CHANGED, "TestClass", originalBytes, redefinedBytes);
		} catch (Exception e) {
			return false;
		}
	}

	/*
	 * It is disallowed to alter the contents of a nest host or a nest members
	 * attribute.
	 */

	/*
	 * Tests invalid nest redefinition by altering a nest host attribute
	 * 
	 * @return boolean true if test passes; false otherwise
	 */
	static public boolean testRedefinitionAlteringNestHostAttribute() {
		try {
			byte[] originalBytes = ClassGenerator.testClassWithNestHostAttribute();
			byte[] redefinedBytes = ClassGenerator.testClassWithAlteredNestHostAttribute();
			return runRedefinitionTest(JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_ATTRIBUTE_CHANGED, "TestClass", originalBytes, redefinedBytes);
		} catch (Exception e) {
			return false;
		}
	}
	
	/*
	 * Tests invalid nest redefinition by altering a nest members attribute
	 * 
	 * @return boolean true if test passes; false otherwise
	 */
	static public boolean testRedefinitionAlteringNestMembersLength() {
		try {
			byte[] originalBytes = ClassGenerator.testClassWithNestMembersAttribute();
			byte[] redefinedBytes = ClassGenerator.testClassWithAlteredNestMembersAttributeLength();
			return runRedefinitionTest(JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_ATTRIBUTE_CHANGED, "TestClass", originalBytes, redefinedBytes);
		} catch (Exception e) {
			return false;
		}
	}
	
	/*
	 * Tests invalid nest redefinition by altering a nest members attribute
	 * 
	 * @return boolean true if test passes; false otherwise
	 */
	static public boolean testRedefinitionAlteringNestMembersContent() {
		try {
			byte[] originalBytes = ClassGenerator.testClassWithNestMembersAttribute();
			byte[] redefinedBytes = ClassGenerator.testClassWithAlteredNestMembersAttributeData();
			return runRedefinitionTest(JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_ATTRIBUTE_CHANGED, "TestClass", originalBytes, redefinedBytes);
		} catch (Exception e) {
			return false;
		}
	}

	public String helpRedefinitionNoNestAttributes() {
		return "testRedefinitionNoNestAttributes tests valid redefinition with no nest attributes.";
	}
	public String helpRedefinitionWithNestHostAttribute() {
		return "testRedefinitionWithNestHostAttribute tests valid redefinition with a nest host attribute.";
	}
	public String helpRedefinitionWithNestMembersAttribute() {
		return "testRedefinitionWithNestMembersAttribute tests valid redefinition with a nest members attribute.";
	}
	public String helpRedefinitionAddingNestHostAttribute() {
		return "testRedefinitionAddingNestHostAttribute tests invalid redefinition by adding a nest host attribute.";
	}
	public String helpRedefinitionAddingNestMembersAttribute() {
		return "testRedefinitionAddingNestMembersAttribute tests invalid redefinition by adding a nest members attribute.";
	}
	public String helpRedefinitionRemovingNestHostAttribute() {
		return "testRedefinitionRemovingNestHostAttribute tests invalid redefinition by removing a nest host attribute.";
	}
	public String helpRedefinitionRemovingNestMembersAttribute() {
		return "testRedefinitionRemovingNestHostAttribute tests invalid redefinition by removing a nest members attribute.";
	}
	public String helpRedefinitionAlteringNestHostAttribute() {
		return "testRedefinitionAlteringNestHostAttribute tests invalid redefinition by altering a nest host attribute.";
	}
	public String helpRedefinitionAlteringNestMembersLength() {
		return "testRedefinitionAlteringNestMembersLength tests invalid redefinition by altering nest members attribute length (# of members)";
	}
	public String helpRedefinitionAlteringNestMembersContent() {
		return "testRedefinitionAlteringNestMembersContent tests invalid redefinition by altering the contents of the nest members attribute.";
	}
}
