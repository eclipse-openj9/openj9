package org.openj9.test.java.lang;

/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.lang.annotation.Annotation;
import java.lang.annotation.Inherited;
import java.lang.annotation.Repeatable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Parameter;
import java.lang.ClassLoader;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Vector;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;

import static org.objectweb.asm.Opcodes.*;

import com.ibm.oti.vm.VM;
import org.openj9.test.support.resource.Support_Resources;

import org.openj9.test.java.lang.specimens.C_CSuperA_SuperDuper;
import org.openj9.test.java.lang.specimens.C_I_SupDuperSupA;
import org.openj9.test.java.lang.specimens.C_SuperA;
import org.openj9.test.java.lang.specimens.I_SupDuper_SupA;
import org.openj9.test.java.lang.specimens.InterfaceTestClasses;
import org.openj9.test.java.lang.specimens.SuperA;
import org.openj9.test.java.lang.specimens.SuperDuper;

import org.openj9.test.jdk.internal.InternalAccessor;

/**
 *
 * @requiredClass org.openj9.test.support.resource.Support_Resources
 * @requiredResource org/openj9/resources/openj9tr_compressD.txt
 * @requiredResource org/openj9/resources/openj9tr_general.jar
 * @requiredResource org/openj9/resources/openj9tr_Foo.c
 */
@Test(groups = { "level.sanity" })
public class Test_Class {

	private static final Logger logger = Logger.getLogger(Test_Class.class);

	static class StaticMember$Class {
		class Member2$A {
		}
	}

	class Member$Class {
		class Member3$B {
		}
	}

	public static class ClassTest{
		private int privField = 1;
		public int pubField = 2;
		private Object cValue = null;
		public Object ack = new Object();
		private int privMethod(){
			return 1;
		}
		public int pubMethod(){
			return 2;
		}
		public Object cValue(){
			return cValue;
		}
		public ClassTest(){
		}
		private ClassTest(Object o){
		}
	}

public static class SubClassTest extends ClassTest{
	}

/**
 * @tests java.lang.Class#forName(java.lang.String)
 */
@Test
public void test_forName() {
	// Test for method java.lang.Class java.lang.Class.forName(java.lang.String)
	try {
		AssertJUnit.assertTrue(
			"Class for name failed for java.lang.Object",
			Class.forName("java.lang.Object") == java.lang.Object.class);
	} catch (Exception e) {
		AssertJUnit.assertTrue(
			"Unexpected exception " + e + " in Class.forName(java.lang.Object)",
			false);
	}
	try {
		AssertJUnit.assertTrue(
			"Class for name failed for [[Ljava.lang.Object;",
			Class.forName("[[Ljava.lang.Object;") == java.lang.Object[][].class);
	} catch (Exception e) {
		AssertJUnit.assertTrue(
			"Unexpected exception " + e + " in Class.forName([[Ljava.lang.Object;)",
			false);
	}
	try {
		AssertJUnit.assertTrue(
			"Class for name failed for [I",
			Class.forName("[I") == int[].class);
	} catch (Exception e) {
		AssertJUnit.assertTrue(
			"Unexpected exception " + e + " in Class.forName([I)",
			false);
	}

	try {
		AssertJUnit.assertTrue(
			"Class for name worked (but should not have) for int",
			Class.forName("int") == null || Class.forName("int") != null);
	} catch (Exception e) {
		// Exception ok here.
	}

	boolean exception = false;
	try {
		Class.forName("int");
	} catch (ClassNotFoundException e) {
		exception = true;
	}
	AssertJUnit.assertTrue("Found class int", exception);

	exception = false;
	try {
		Class.forName("byte");
	} catch (ClassNotFoundException e) {
		exception = true;
	}
	AssertJUnit.assertTrue("Found class byte", exception);

	exception = false;
	try {
		Class.forName("char");
	} catch (ClassNotFoundException e) {
		exception = true;
	}
	AssertJUnit.assertTrue("Found class char", exception);

	exception = false;
	try {
		Class.forName("void");
	} catch (ClassNotFoundException e) {
		exception = true;
	}
	AssertJUnit.assertTrue("Found class void", exception);

	exception = false;
	try {
		Class.forName("short");
	} catch (ClassNotFoundException e) {
		exception = true;
	}
	AssertJUnit.assertTrue("Found class short", exception);

	exception = false;
	try {
		Class.forName("long");
	} catch (ClassNotFoundException e) {
		exception = true;
	}
	AssertJUnit.assertTrue("Found class long", exception);

	exception = false;
	try {
		Class.forName("boolean");
	} catch (ClassNotFoundException e) {
		exception = true;
	}
	AssertJUnit.assertTrue("Found class boolean", exception);

	exception = false;
	try {
		Class.forName("float");
	} catch (ClassNotFoundException e) {
		exception = true;
	}
	AssertJUnit.assertTrue("Found class float", exception);

	exception = false;
	try {
		Class.forName("double");
	} catch (ClassNotFoundException e) {
		exception = true;
	}
	AssertJUnit.assertTrue("Found class double", exception);
}

public interface IA {
	void method_L(String name);
	void method_M(String name);
}
public interface IB {
	void method_L(String name);
	void method_M(String name);
}
public interface IC extends IA, IB {
	void method_L(String name);
}
public abstract class CD implements IA, IC {
	public abstract void method_N(String name);
}
public abstract class CE implements IA {
}
public abstract class CF extends CE implements IC {
	public abstract void method_N(String name);
}

/**
 * @tests java.lang.Class#getMethods()
 */
@Test
public void test_getMethods_subtest1() {
	String[]	expected_IC = new String[] {
			"IC.method_L(java.lang.String)",
			"IA.method_M(java.lang.String)",
			"IB.method_M(java.lang.String)"};
	Method[] methodNames_IC = IC.class.getMethods();
	AssertJUnit.assertEquals("Returned incorrect number of methods for IC", expected_IC.length,  methodNames_IC.length);
	for (int i = 0; i < expected_IC.length; i++) {
		boolean match = false;
		for (int j = 0; j < methodNames_IC.length; j++) {
			if (methodNames_IC[j].toString().indexOf(expected_IC[i]) != -1) {
				match = true;
			}
		}
		AssertJUnit.assertTrue("Expected method " + expected_IC[i] + " not found.", match);
	}

	/* as of Java 9, IC.method_L() masks IA.method_L() */
	String[]	expected_CD = new String[] {
			"CD.method_N(java.lang.String)",
			"IA.method_M(java.lang.String)",
			"IC.method_L(java.lang.String)",
			"IB.method_M(java.lang.String)"};
	Method[] methodNames_CD = CD.class.getMethods();
	AssertJUnit.assertEquals("Returned incorrect number of methods for CD", (expected_CD.length + Object.class.getMethods().length), methodNames_CD.length);
	for (int i = 0; i < expected_CD.length; i++) {
		boolean match = false;
		for (int j = 0; j < methodNames_CD.length; j++) {
			if (methodNames_CD[j].toString().indexOf(expected_CD[i]) != -1) {
				match = true;
			}
		}
		AssertJUnit.assertTrue("Expected method " + expected_CD[i] + " not found.", match);
	}

	/* as of Java 9, IC.method_L() masks IA.method_L() */
	String[]	expected_CF = new String[] {
			"IA.method_M(java.lang.String)",
			"CF.method_N(java.lang.String)",
			"IC.method_L(java.lang.String)",
			"IB.method_M(java.lang.String)"};
	Method[] methodNames_CF = CF.class.getMethods();
	AssertJUnit.assertEquals("Returned incorrect number of methods for CF", expected_CF.length + Object.class.getMethods().length, methodNames_CF.length);
	for (int i = 0; i < expected_CF.length; i++) {
		boolean match = false;
		for (int j = 0; j < methodNames_CF.length; j++) {
			if (methodNames_CF[j].toString().indexOf(expected_CF[i]) != -1) {
				match = true;
			}
		}
		AssertJUnit.assertTrue("Expected method " + expected_CF[i] + " not found.", match);
	}
}
@Test
public void test_getMethods_subtest2() {

	java.security.PrivilegedAction action = new java.security.PrivilegedAction() {
		public Object run() {
			try {
				File resources = Support_Resources.createTempFolder();
				String[]	expected_E = new String[] {
						"org.openj9.resources.classinheritance.E.method_N(java.lang.String)",
						"org.openj9.resources.classinheritance.E.method_M(java.lang.String)",
						"org.openj9.resources.classinheritance.A.method_K(java.lang.String)",
						"org.openj9.resources.classinheritance.E.method_L(java.lang.String)",
						"org.openj9.resources.classinheritance.E.method_O(java.lang.String)",
						"org.openj9.resources.classinheritance.B.method_K(java.lang.String)"};

				Support_Resources.copyFile(resources, null, "openj9tr_general.jar");
				File file = new File(resources.toString() + "/openj9tr_general.jar");
				java.net.URL url = new java.net.URL("file:" + file.getPath());
				ClassLoader loader = new java.net.URLClassLoader(new java.net.URL[]{url}, null);

				Class cls_E = Class.forName("org.openj9.resources.classinheritance.E", false, loader);
				Method[] methodNames_E = cls_E.getMethods();
				AssertJUnit.assertEquals("Returned incorrect number of methods for cls_E", expected_E.length + Object.class.getMethods().length, methodNames_E.length);
				for (int i = 0; i < expected_E.length; i++) {
					boolean match = false;
					for (int j = 0; j < methodNames_E.length; j++) {
						if (methodNames_E[j].toString().indexOf(expected_E[i]) != -1) {
							match = true;
						}
					}
					AssertJUnit.assertTrue("Expected method " + expected_E[i] + " not found.", match);
				}
			} catch (MalformedURLException e) {
				logger.error("Unexpected exception: " + e);
			} catch (ClassNotFoundException e) {
				logger.error("Unexpected exception: " + e);
			}

			return null;
		}
	};
	java.security.AccessController.doPrivileged(action);
}

static class ParentClass {
	public static void methodPublicStatic() {}
	public void methodPublicInstance() {}
	static void methodPkgPrivateStatic() {}
	void methodPkgPrivateInstance() {}
	private static void methodPrivateStatic() {}
	private void methodPrivateInstance() {}
}

static class ChildClass extends ParentClass {
	public static void methodPublicStatic() {}
	public void methodPublicInstance() {}
	static void methodPkgPrivateStatic() {}
	void methodPkgPrivateInstance() {}
	private static void methodPrivateStatic() {}
	private void methodPrivateInstance() {}
}

static interface SuperInterface {
	void methodPublicInterface(); 
	public static void methodPublicStatic() {};
}

static interface SubInterface extends SuperInterface {
	void methodPublicInterface(); 
	public static void methodPublicStatic() {};
	default void defaultMethodPublicInterface() {}
}

static class ClassImplInterface implements SubInterface {
	public void methodPublicInterface() {}
	public static void methodPublicStatic() {};
	public void defaultMethodPublicInterface() {}
}

@Test
public void test_getMethods_subtest3() {
	final Method[] methods = ChildClass.class.getMethods();
	for (Method method : methods) {
		final Class<?> declaringClass = method.getDeclaringClass();
		if (declaringClass.equals(ParentClass.class)) {
			Assert.fail("Should not return Method " + method.getName() + " declared in class " + declaringClass.getName());
		}
	}
}
@Test
public void test_getMethods_subtest4() {
	final Method[] methods = SubInterface.class.getMethods();
	for (Method method : methods) {
		final Class<?> declaringClass = method.getDeclaringClass();
		if (declaringClass.equals(SuperInterface.class)) {
			Assert.fail("Should not return Method " + method.getName() + " declared in class " + declaringClass.getName());
		}
	}
}
@Test
public void test_getMethods_subtest5() {
	final Method[] methods = ClassImplInterface.class.getMethods();
	for (Method method : methods) {
		final Class<?> declaringClass = method.getDeclaringClass();
		if (declaringClass.equals(SuperInterface.class) || declaringClass.equals(SubInterface.class)) {
			Assert.fail("Should not return Method " + method.getName() + " declared in class " + declaringClass.getName());
		}
	}
}

static String[] concatenateObjectMethods(String methodList[]) {
	final String[] jlobjectMethods = new String[] {
			objectClass+".equals(java.lang.Object)boolean",
			objectClass+".getClass()java.lang.Class",
			objectClass+".hashCode()int",
			objectClass+".notify()void",
			objectClass+".notifyAll()void",
			objectClass+".toString()java.lang.String",
			objectClass+".wait()void",
			objectClass+".wait(long)void",
			objectClass+".wait(long, int)void"
		};

	String result[] = new String[methodList.length + jlobjectMethods.length];
	System.arraycopy(methodList, 0, result, 0, methodList.length);
	System.arraycopy(jlobjectMethods, 0, result, methodList.length, jlobjectMethods.length);
	return result;
}
@Test
public void testInterfaceMethodInheritance() {
	Class testClasses[] = new Class[] {InterfaceTestClasses.I1.class, InterfaceTestClasses.I2.class, InterfaceTestClasses.I3.class,
			InterfaceTestClasses.SuperDuperClass.class, InterfaceTestClasses.SuperClass.class, InterfaceTestClasses.SubClass.class};
	HashMap<Class, String[]> expectedMethods = new HashMap<>();
	
	/*
	 * As of Java 9, method declarations in subinterfaces hide declarations of the same method in superinterfaces.
	 * This means that the following methods are not inherited by implementers of M3:
	 * InterfaceTestClasses$I1.m1
	 * InterfaceTestClasses$I1.m2
	 * InterfaceTestClasses$I1.m3
	 * InterfaceTestClasses$I2.m1
	 * InterfaceTestClasses$I2.m2
	 * InterfaceTestClasses$I2.m3
	 */
	final String[] interfaceMethodList = concatenateObjectMethods(new String [] {
			specimenPackage+".InterfaceTestClasses$I3.m1()void",
			specimenPackage+".InterfaceTestClasses$I3.m2()void",
			specimenPackage+".InterfaceTestClasses$I3.m3()void"
	});
	
	expectedMethods.put(
			testClasses[0],
			new String [] {
					specimenPackage+".InterfaceTestClasses$I1.m1()void",
					specimenPackage+".InterfaceTestClasses$I1.m2()void",
					specimenPackage+".InterfaceTestClasses$I1.m3()void"
			}
			);
	expectedMethods.put(
			testClasses[1],
			new String [] {
					specimenPackage+".InterfaceTestClasses$I2.m1()void",
					specimenPackage+".InterfaceTestClasses$I2.m2()void",
					specimenPackage+".InterfaceTestClasses$I2.m3()void"
			}
			);
	expectedMethods.put(
			testClasses[2],
			new String [] {
					specimenPackage+".InterfaceTestClasses$I3.m1()void",
					specimenPackage+".InterfaceTestClasses$I3.m2()void",
					specimenPackage+".InterfaceTestClasses$I3.m3()void"
			}
			);
	expectedMethods.put(
			testClasses[3],
			interfaceMethodList
			);
	expectedMethods.put(
			testClasses[4],
			interfaceMethodList);
	expectedMethods.put(
			testClasses[5],
			interfaceMethodList);
	compareMethods(testClasses, expectedMethods);
}

@Test
public void testDefaultMethodInheritance() {
	Class testClasses[] = new Class[] {C_CSuperA_SuperDuper.class, C_I_SupDuperSupA.class, C_SuperA.class, I_SupDuper_SupA.class, SuperA.class, SuperDuper.class};
	HashMap<Class, String[]> expectedMethods = new HashMap<>();
	/*
	 * As of Java 9, method declarations in subinterfaces hide declarations of the same method in superinterfaces.
	 * This means that the following methods are now hidden in implementers of SuperA:
	 * SuperDuper.abstractInSuperA_abstractInSuperDuper()
	 * SuperDuper.abstractInSuperA_defaultInSuperDuper()
	 */
	final String[] interfaceMethodList = concatenateObjectMethods(new String [] {
			specimenPackage+".SuperA.abstractInSuperA_abstractInSuperDuper()void",
			specimenPackage+".SuperA.abstractInSuperA_defaultInSuperDuper()void",
			specimenPackage+".SuperA.defaultInSuperA_abstractInSuperDuper()void",
			specimenPackage+".SuperA.defaultInSuperA_defaultInSuperDuper()void"});
	expectedMethods.put(
			testClasses[0],
			interfaceMethodList);
	expectedMethods.put(
			testClasses[1],
			interfaceMethodList);
	expectedMethods.put(
			testClasses[2],
			concatenateObjectMethods(new String [] {
					specimenPackage+".SuperA.abstractInSuperA_abstractInSuperDuper()void",
					specimenPackage+".SuperA.abstractInSuperA_defaultInSuperDuper()void",
					specimenPackage+".SuperA.defaultInSuperA_abstractInSuperDuper()void",
					specimenPackage+".SuperA.defaultInSuperA_defaultInSuperDuper()void"}));
	expectedMethods.put(
			testClasses[3],
			new String [] {
					specimenPackage+".SuperA.abstractInSuperA_abstractInSuperDuper()void",
					specimenPackage+".SuperA.abstractInSuperA_defaultInSuperDuper()void",
					specimenPackage+".SuperA.defaultInSuperA_abstractInSuperDuper()void",
					specimenPackage+".SuperA.defaultInSuperA_defaultInSuperDuper()void",
					specimenPackage+".SuperDuper.abstractInSuperA_abstractInSuperDuper()void",
					specimenPackage+".SuperDuper.abstractInSuperA_defaultInSuperDuper()void",

			}
			);
	expectedMethods.put(
			testClasses[4],
			new String [] {
					specimenPackage+".SuperA.abstractInSuperA_abstractInSuperDuper()void",
					specimenPackage+".SuperA.abstractInSuperA_defaultInSuperDuper()void",
					specimenPackage+".SuperA.defaultInSuperA_abstractInSuperDuper()void",
					specimenPackage+".SuperA.defaultInSuperA_defaultInSuperDuper()void",

			}
			);
	expectedMethods.put(
			testClasses[5],
			new String [] {
					specimenPackage+".SuperDuper.abstractInSuperA_abstractInSuperDuper()void",
					specimenPackage+".SuperDuper.abstractInSuperA_defaultInSuperDuper()void",
					specimenPackage+".SuperDuper.defaultInSuperA_abstractInSuperDuper()void",
					specimenPackage+".SuperDuper.defaultInSuperA_defaultInSuperDuper()void",

			}
			);
	compareMethods(testClasses, expectedMethods);
}

private void compareMethods(@SuppressWarnings("rawtypes") Class[] testClasses,
		@SuppressWarnings("rawtypes") HashMap<Class, String[]> expectedMethods) {
	for (@SuppressWarnings("rawtypes") Class c: testClasses) {
		Method methodList[] = c.getMethods();
		String actualNames[] = new String[methodList.length];
		for (int i = 0; i < methodList.length; ++i) {
			actualNames[i] = methToString(methodList[i]);
		}
		Arrays.sort(actualNames);
		String expectedNames[] = expectedMethods.get(c);
		Arrays.sort(expectedNames);
		for (int i = 0; i < methodList.length; ++i) {
			AssertJUnit.assertEquals("Wrong method for "+c.getSimpleName(), expectedNames[i],  actualNames[i]);
		}
	}
}

private static String methToString(Method m) {
    StringBuffer methName = new StringBuffer(m.getDeclaringClass().getName());
    methName.append('.');
    methName.append(m.getName());
    methName.append('(');
    String separator = "";
    for (Parameter p: m.getParameters()) {
            methName.append(separator);
            methName.append(p.getType().getName());
            separator = ", ";
    }
    methName.append(')');
    methName.append(m.getReturnType().getName());
    return methName.toString();
}

/**
 * @tests java.lang.Class#getClasses()
 */
@Test
public void test_getClasses() {
	// Test for method java.lang.Class [] java.lang.Class.getClasses()
	Class[] classes = Test_Class.class.getClasses();
	if (classes.length != 15) {
		for (int i=0; i<classes.length; i++)
			logger.debug("classes[" + i + "]: " + classes[i]);
		Assert.fail("Incorrect class array returned: expected 15 but returned " + classes.length);
	}
}

/**
 * @tests java.lang.Class#getClasses()
 */
@Test
public void test_getClasses2() {
	final java.security.Permission privCheckPermission = new java.security.BasicPermission("Privilege check") {
	};
	class MyCombiner implements java.security.DomainCombiner {
		boolean combine;
		public java.security.ProtectionDomain[] combine(java.security.ProtectionDomain[] executionDomains, java.security.ProtectionDomain[] parentDomains) {
			combine = true;
			return new java.security.ProtectionDomain[0];
		}
		public boolean isPriviledged() {
			combine = false;
			try {
				java.security.AccessController.checkPermission(privCheckPermission);
			} catch (SecurityException e) {
			}
			return !combine;
		}
	};
	final MyCombiner combiner = new MyCombiner();
	class SecurityManagerCheck extends SecurityManager {
		String reason;
		Class checkClass;
		int checkType;
		int checkPermission = 0;
		int checkMemberAccess = 0;
		int checkPackageAccess = 0;

		public void setExpected(String reason, Class cls, int type) {
			this.reason = reason;
			checkClass = cls;
			checkType = type;
			checkPermission = 0;
			checkMemberAccess = 0;
			checkPackageAccess = 0;
		}
		public void checkPermission(java.security.Permission perm) {
			if (combiner.isPriviledged())
				return;
			checkPermission++;
		}
		public void checkPackageAccess(String packageName) {
			if (packageName.startsWith("java.") || packageName.startsWith("org.openj9.test.java.lang")) {
				return;
			}
			if (combiner.isPriviledged())
				return;
			checkPackageAccess++;
			String name = checkClass.getName();
			int index = name.lastIndexOf('.');
			String checkPackage = name.substring(0, index);
			AssertJUnit.assertTrue(reason + " unexpected package: " + packageName, packageName.equals(checkPackage));
		}
		public void assertProperCalls() {
			int declared = checkType == Member.DECLARED ? 1 : 0;
			AssertJUnit.assertTrue(reason + " unexpected checkPermission count: " + checkPermission, checkPermission == declared);
			AssertJUnit.assertTrue(reason + " unexpected checkPackageAccess count: " + checkPackageAccess, checkPackageAccess == 1);
		}
	}

	final SecurityManagerCheck sm = new SecurityManagerCheck();
	System.setSecurityManager(sm);

	java.security.AccessControlContext acc = new java.security.AccessControlContext(new java.security.ProtectionDomain[0]);
	java.security.AccessControlContext acc2 = new java.security.AccessControlContext(acc, combiner);

	java.security.PrivilegedAction action = new java.security.PrivilegedAction() {
		public Object run() {
			File resources = Support_Resources.createTempFolder();
			try {
				Support_Resources.copyFile(resources, null, "openj9tr_general.jar");
				File file = new File(resources.toString() + "/openj9tr_general.jar");
				java.net.URL url = new java.net.URL("file:" + file.getPath());
				ClassLoader loader = new java.net.URLClassLoader(new java.net.URL[]{url}, null);
				Class cls = Class.forName("org.openj9.resources.security.SecurityTestSub", false, loader);
				// must preload junit.framework.Assert before installing SecurityManager
				// otherwise loading it inside checkPackageAccess() is recursive
				AssertJUnit.assertTrue("preload assertions", true);
				try {
					sm.setExpected("getClasses", cls, java.lang.reflect.Member.PUBLIC);
					cls.getClasses();
					sm.assertProperCalls();

					sm.setExpected("getDeclaredClasses", cls, java.lang.reflect.Member.DECLARED);
					cls.getDeclaredClasses();
					sm.assertProperCalls();

					sm.setExpected("getConstructor", cls, java.lang.reflect.Member.PUBLIC);
					cls.getConstructor(new Class[0]);
					sm.assertProperCalls();

					sm.setExpected("getConstructors", cls, java.lang.reflect.Member.PUBLIC);
					cls.getConstructors();
					sm.assertProperCalls();

					sm.setExpected("getDeclaredConstructor", cls, java.lang.reflect.Member.DECLARED);
					cls.getDeclaredConstructor(new Class[0]);
					sm.assertProperCalls();

					sm.setExpected("getDeclaredConstructors", cls, java.lang.reflect.Member.DECLARED);
					cls.getDeclaredConstructors();
					sm.assertProperCalls();

					sm.setExpected("getField", cls, java.lang.reflect.Member.PUBLIC);
					cls.getField("publicField");
					sm.assertProperCalls();

					sm.setExpected("getFields", cls, java.lang.reflect.Member.PUBLIC);
					cls.getFields();
					sm.assertProperCalls();

					sm.setExpected("getDeclaredField", cls, java.lang.reflect.Member.DECLARED);
					cls.getDeclaredField("publicField");
					sm.assertProperCalls();

					sm.setExpected("getDeclaredFields", cls, java.lang.reflect.Member.DECLARED);
					cls.getDeclaredFields();
					sm.assertProperCalls();

					sm.setExpected("getDeclaredMethod", cls, java.lang.reflect.Member.DECLARED);
					cls.getDeclaredMethod("publicMethod", new Class[0]);
					sm.assertProperCalls();

					sm.setExpected("getDeclaredMethods", cls, java.lang.reflect.Member.DECLARED);
					cls.getDeclaredMethods();
					sm.assertProperCalls();

					sm.setExpected("getMethod", cls, java.lang.reflect.Member.PUBLIC);
					cls.getMethod("publicMethod", new Class[0]);
					sm.assertProperCalls();

					sm.setExpected("getMethods", cls, java.lang.reflect.Member.PUBLIC);
					cls.getMethods();
					sm.assertProperCalls();

					sm.setExpected("newInstance", cls, java.lang.reflect.Member.PUBLIC);
					cls.newInstance();
					sm.assertProperCalls();
				} finally {
					System.setSecurityManager(null);
				}
			} catch (Exception e) {
				if (e instanceof RuntimeException)
					throw (RuntimeException)e;
				Assert.fail("unexpected exception: " + e);
			}
			return null;
		}
	};
	java.security.AccessController.doPrivileged(action, acc2);
}

/**
 * @tests java.lang.Class#getComponentType()
 */
@Test
public void test_getComponentType() {
	// Test for method java.lang.Class java.lang.Class.getComponentType()
	AssertJUnit.assertTrue( "int array does not have int component type" ,
		int[].class.getComponentType() == int.class);
	AssertJUnit.assertTrue( "Object array does not have Object component type" ,
		Object[].class.getComponentType() == Object.class);
	AssertJUnit.assertTrue( "Object has non-null component type" ,
		Object.class.getComponentType() == null);
}

/**
 * @tests java.lang.Class#getConstructor(java.lang.Class[])
 */
@Test
public void test_getConstructor() {
	// Test for method java.lang.reflect.Constructor java.lang.Class.getConstructor(java.lang.Class [])
		try {
		ClassTest.class.getConstructor(new Class[0]);
		try {
			ClassTest.class.getConstructor(new Class[] {Object.class });
		} catch (NoSuchMethodException e) {
			//Correct - constructor with obj is private
			return;
		}
		AssertJUnit.assertTrue("Found private Constructor", false);
	} catch (NoSuchMethodException e) {
		AssertJUnit.assertTrue("Exception during getConstructoy test", false);
	}
}

/**
 * @tests java.lang.Class#getConstructors()
 */
@Test
public void test_getConstructors() {
	// Test for method java.lang.reflect.Constructor [] java.lang.Class.getConstructors()
	try {
		java.lang.reflect.Constructor[] c = ClassTest.class.getConstructors();
		AssertJUnit.assertTrue("Incorrect number of constructors returned", c.length == 1);
	}
	catch (Exception e) {
		AssertJUnit.assertTrue("Exception during getDeclaredConstructor test:" + e.toString(), false);
	}
}

/**
 * @tests java.lang.Class#getDeclaredClasses()
 */
@Test
public void test_getDeclaredClasses() {
	int len = 65;
	// Test for method java.lang.Class [] java.lang.Class.getDeclaredClasses()
	Class[] declaredClasses = Test_Class.class.getDeclaredClasses();
	if (declaredClasses.length != len) {
		for (int i=0; i<declaredClasses.length; i++)
			logger.debug("declared[" + i + "]: " + declaredClasses[i]);
		Assert.fail("Incorrect class array returned: expected 65 but returned " + declaredClasses.length);
	}
}

/**
 * @tests java.lang.Class#getDeclaredConstructor(java.lang.Class[])
 */
@Test
public void test_getDeclaredConstructor$Ljava_lang_Class() {
	// Test for method java.lang.reflect.Constructor java.lang.Class.getDeclaredConstructor(java.lang.Class [])
	try {
		java.lang.reflect.Constructor c = ClassTest.class.getDeclaredConstructor(new Class[0]);
		AssertJUnit.assertTrue("Incorrect constructor returned", ((ClassTest)(c.newInstance(new Object[0]))).cValue() == null);
		c = ClassTest.class.getDeclaredConstructor(new Class[] {Object.class });
	} catch (NoSuchMethodException e) {
		AssertJUnit.assertTrue("Exception during getDeclaredConstructor test: " + e.toString(), false);
	}
	catch (Exception e) {
		AssertJUnit.assertTrue("Exception during getDeclaredConstructor test:" + e.toString(), false);
	}
}

/**
 * @tests java.lang.Class#getDeclaredConstructors()
 */
@Test
public void test_getDeclaredConstructors() {
	// Test for method java.lang.reflect.Constructor [] java.lang.Class.getDeclaredConstructors()
	try {
		java.lang.reflect.Constructor[] c = ClassTest.class.getDeclaredConstructors();
		AssertJUnit.assertTrue("Incorrect number of constructors returned", c.length == 2);
	}
	catch (Exception e) {
		AssertJUnit.assertTrue("Exception during getDeclaredConstructor test:" + e.toString(), false);
	}
}

/**
 * @tests java.lang.Class#getDeclaredField(java.lang.String)
 */
@Test
public void test_getDeclaredFieldLjava_lang_String() {
	// Test for method java.lang.reflect.Field java.lang.Class.getDeclaredField(java.lang.String)
	try {
		java.lang.reflect.Field f = ClassTest.class.getDeclaredField("pubField");
		AssertJUnit.assertTrue("Returned incorrect field", f.getInt(new ClassTest()) == 2);
	} catch (Exception e) {
		AssertJUnit.assertTrue("Exception getting fields: " + e.toString(), false);
	}

/*
	// disable this test until JCL level is updated to contain the fix for
	// 93772: 7u85-b11: 6642881: Improve performance of Class.getClassLoader().
	// This JCL workitem added (and registerFieldsToFilter) field j.l.Class.classLoader.
	try {
		java.lang.reflect.Field f = Class.class.getDeclaredField("classLoader");
		fail("java.lang.Class.classLoader shoud NOT be accessible via reflection");
	} catch (NoSuchFieldException e) {
	}
*/

	try {
		java.lang.reflect.Field f = System.class.getDeclaredField("security");
		Assert.fail("java.lang.System.security shoud NOT be accessible via reflection");
	} catch (NoSuchFieldException e) {
	}

	try {
		java.lang.reflect.Field f = jdk.internal.reflect.Reflection.class.getDeclaredField("fieldFilterMap");
		Assert.fail("jdk.internal.reflect.Reflection.fieldFilterMap shoud NOT be accessible via reflection");
	} catch (NoSuchFieldException e) {
	}

	try {
		java.lang.reflect.Field f = jdk.internal.reflect.Reflection.class.getDeclaredField("methodFilterMap");
		Assert.fail("jdk.internal.reflect.Reflection.methodFilterMap shoud NOT be accessible via reflection");
	} catch (NoSuchFieldException e) {
	}
}

/**
 * @tests java.lang.Class#getDeclaredFields()
 */
@Test
public void test_getDeclaredFields() {
	// Test for method java.lang.reflect.Field [] java.lang.Class.getDeclaredFields()
	try {
		java.lang.reflect.Field[] f = ClassTest.class.getDeclaredFields();
		AssertJUnit.assertTrue("Returned incorrect number of fields", f.length == 4);
		f = SubClassTest.class.getDeclaredFields();
		AssertJUnit.assertTrue("Returned incorrect number of fields", f.length == 0); //Declared fields do not include inherited
	} catch (Exception e) {
		AssertJUnit.assertTrue("Exception getting fields", false);
	}
}

/**
 * @tests java.lang.Class#getDeclaredMethod(java.lang.String, java.lang.Class[])
 */
@Test
public void test_getDeclaredMethodLjava_lang_String$Ljava_lang_Class() {
	// Test for method java.lang.reflect.Method java.lang.Class.getDeclaredMethod(java.lang.String, java.lang.Class [])
	try {
		java.lang.reflect.Method m = ClassTest.class.getDeclaredMethod("pubMethod", new Class[0]);
		AssertJUnit.assertTrue("Returned incorrect method", ((Integer)(m.invoke(new ClassTest(), new Class[0]))).intValue() == 2);
		m = ClassTest.class.getDeclaredMethod("privMethod", new Class[0]);
		try{
		m.invoke(new ClassTest(), new Class[0]); //Invoking private non-sub, non-package
		} catch (IllegalAccessException e){
			//Correct
			return;
		}
	} catch (Exception e) {
		AssertJUnit.assertTrue("Exception getting methods: " + e.toString(), false);
	}
}

/**
 * @tests java.lang.Class#getDeclaredMethods()
 */
@Test
public void test_getDeclaredMethods() {
	// Test for method java.lang.reflect.Method [] java.lang.Class.getDeclaredMethods()
	try {
		java.lang.reflect.Method[] m = ClassTest.class.getDeclaredMethods();
		AssertJUnit.assertTrue("Returned incorrect number of methods", m.length == 3);
		m = SubClassTest.class.getDeclaredMethods();
		AssertJUnit.assertTrue("Returned incorrect number of methods", m.length == 0);
	} catch (Exception e) {
		AssertJUnit.assertTrue("Exception getting methods: " + e.toString(), false);
	}
}

/**
 * @tests java.lang.Class#getDeclaringClass()
 */
@Test
public void test_getDeclaringClass() {
	// Test for method java.lang.Class java.lang.Class.getDeclaringClass()
	AssertJUnit.assertTrue(ClassTest.class.getDeclaringClass().equals(Test_Class.class));
}

/**
 * @tests java.lang.Class#getField(java.lang.String)
 */
@Test
public void test_getField() {
	// Test for method java.lang.reflect.Field java.lang.Class.getField(java.lang.String)
	try {
		java.lang.reflect.Field f = ClassTest.class.getField("pubField");
		AssertJUnit.assertTrue("Returned incorrect field", f.getInt(new ClassTest()) == 2);
		try{
		f = ClassTest.class.getField("privField");
		} catch (NoSuchFieldException e) {
		//Correct
		return;
		}
		AssertJUnit.assertTrue("Private field access failed to throw exceprion", false);
	} catch (Exception e) {
		AssertJUnit.assertTrue("Exception getting fields: " + e.toString(), false);
	}
}

/**
 * @tests java.lang.Class#getFields()
 */
@Test
public void test_getFields() {
	// Test for method java.lang.reflect.Field [] java.lang.Class.getFields()
	try {
		java.lang.reflect.Field[] f = ClassTest.class.getFields();
		AssertJUnit.assertTrue("Incorrect number of fields returned: " + f.length, f.length == 2);
		f = SubClassTest.class.getFields();
		AssertJUnit.assertTrue("Incorrect number of fields returned: " + f.length, f.length == 2);//Check inheritance of pub fields
	} catch (Exception e) {
		AssertJUnit.assertTrue("Exception getting fields: " + e.toString(), false);
	}
}

/**
 * @tests java.lang.Class#getInterfaces()
 */
@Test
public void test_getInterfaces() {
	// Test for method java.lang.Class [] java.lang.Class.getInterfaces()
	Class[] interfaces;
	List interfaceList;
	interfaces = java.lang.Object.class.getInterfaces();
	AssertJUnit.assertTrue(
		"Incorrect interface list for Object",
		interfaces.length == 0);
	interfaceList = Arrays.asList(java.util.Vector.class.getInterfaces());
	AssertJUnit.assertTrue(
		"Incorrect interface list for Vector",
		interfaceList.contains(java.lang.Cloneable.class)
		&& interfaceList.contains(java.io.Serializable.class)
		&& interfaceList.contains(java.util.List.class));
}

/**
 * @tests java.lang.Class#getMethod(java.lang.String, java.lang.Class[])
 */
@Test
public void test_getMethod() {
	// Test for method java.lang.reflect.Method java.lang.Class.getMethod(java.lang.String, java.lang.Class [])
	try {
		java.lang.reflect.Method m = ClassTest.class.getMethod("pubMethod", new Class[0]);
		AssertJUnit.assertTrue("Returned incorrect method", ((Integer) (m.invoke(new ClassTest(), new Class[0]))).intValue() == 2);
		try {
			m = ClassTest.class.getMethod("privMethod", new Class[0]);
		} catch (NoSuchMethodException e) {
			//Correct
			return;
		}
		AssertJUnit.assertTrue("Failed to throw exception accessing private method", false);
	} catch (Exception e) {
		AssertJUnit.assertTrue("Exception getting methods: " + e.toString(), false);
	}
}

/**
 * @tests java.lang.Class#getDeclaredMethod(java.lang.String, java.lang.Class[])
 */
@Test
public void test_getDeclaredMethod() {
	// Test for method java.lang.reflect.Method java.lang.Class.getDeclaredMethod(java.lang.String, java.lang.Class [])
	try {
		java.lang.reflect.Method m1 = ClassTest.class.getDeclaredMethod("pubMethod", new Class[0]);
		AssertJUnit.assertTrue("Returned incorrect method", ((Integer) (m1.invoke(new ClassTest(), new Object[0]))).intValue() == 2);
		java.lang.reflect.Method m2 = ClassTest.class.getDeclaredMethod("privMethod", new Class[0]);
		m2.setAccessible(true);
		AssertJUnit.assertTrue("Returned incorrect method", ((Integer) (m2.invoke(new ClassTest(), new Object[0]))).intValue() == 1);
	} catch (Exception e) {
		AssertJUnit.assertTrue("Exception getting methods: " + e.toString(), false);
	}
}

static public class A {
	public void method(C param) {
	}
}
static public class B extends A {
}
static public class C {
}

/**
 * @tests java.lang.Class#getMethod(java.lang.String, java.lang.Class[])
 */
@Test
public void test_getMethod2() {
	final String baseName = getClass().getName();
	URL location = getClass().getProtectionDomain().getCodeSource().getLocation();
	URLClassLoader loader1 = new URLClassLoader(new URL[]{location}, null) {
		public Class loadClass(String name, boolean resolve) throws ClassNotFoundException {
			if (name.equals(baseName + "$B")) throw new ClassNotFoundException();
			return super.loadClass(name, resolve);
		}
	};
	URLClassLoader loader2 = new URLClassLoader(new URL[]{location}, loader1) {
		public Class loadClass(String name, boolean resolve) throws ClassNotFoundException {
			if (name.equals(baseName + "$C")) throw new ClassNotFoundException();
			return super.loadClass(name, resolve);
		}
	};
	try {
		Class Cclass = Class.forName(baseName + "$C", false, loader1);
		Class Bclass = Class.forName(baseName + "$B", false, loader2);
		Bclass.getMethod("method", new Class[]{Cclass});
	} catch (ClassNotFoundException e) {
		Assert.fail("unexpected: " + e);
	} catch (NoSuchMethodException e) {
		Assert.fail("unexpected: " + e);
	}
}

/**
 * @tests java.lang.Class#getMethods()
 */
@Test
public void test_getMethods() {
	// Test for method java.lang.reflect.Method [] java.lang.Class.getMethods()
	try {
		java.lang.reflect.Method[] m = ClassTest.class.getMethods();
		AssertJUnit.assertTrue("Returned incorrect number of methods: "  + m.length, m.length == 2 + Object.class.getMethods().length);
		m = SubClassTest.class.getMethods();
		AssertJUnit.assertTrue("Returned incorrect number of sub-class methods: " + m.length, m.length == 2 + Object.class.getMethods().length); //Number of inherited methods
	} catch (Exception e) {
		AssertJUnit.assertTrue("Exception getting methods: " + e.toString(), false);
	}
}

/**
 * @tests java.lang.Class#getModifiers()
 */
@Test
public void test_getModifiers_accessModifiers() {
	// Test for method int java.lang.Class.getModifiers()

	class defaultClass { };

	final int publicFlag = 0x0001;
	final int privateFlag = 0x0002;
	final int protectedFlag = 0x0004;
	AssertJUnit.assertTrue( "default class is public" , (defaultClass.class.getModifiers()&publicFlag) == 0);
	AssertJUnit.assertTrue( "default class is protected" , (defaultClass.class.getModifiers()&protectedFlag) == 0);
	AssertJUnit.assertTrue( "default class is private" , (defaultClass.class.getModifiers()&privateFlag) == 0);
	AssertJUnit.assertTrue( "public class is not public" , (Object.class.getModifiers()&publicFlag) != 0);
	AssertJUnit.assertTrue( "public class is protected" , (Object.class.getModifiers()&protectedFlag) == 0);
	AssertJUnit.assertTrue( "public class is private" , (Object.class.getModifiers()&privateFlag) == 0);
}

/**
 * @tests java.lang.Class#getModifiers()
 *
 * Verify modifiers for different class types, e.g. inner classes.
 */
@Test
public void test_getModifiers_classTypes() {
	boolean failed = false;

	Class<?>[] classes = new Class<?>[] {
		DefaultClazz.class,
		ProtectedClazz.class,
		PrivateClazz.class,
		innerClassFromMethod(),
		PublicClazz.class,
		FinalClazz.class,
		StaticClazz.class,
		FinalStaticClazz.class,
		PublicStaticClazz.class,
		PublicFinalClazz.class,
		PublicFinalStaticClazz.class,
		staticInnerAnonymousClazz,
		Array.newInstance(Helper.innerClassDifferentModifiers, 0).getClass(),
		AbstractClazz.class,
		Helper.syntheticClass,
		AnnotationClazz.class,
		EnumClazz.class,
	};
	int[] modifiers = new int[] {
		0x0000, //
		0x0004, // protected
		0x0002, // private
		0x0000, //
		0x0001, // public
		0x0010, // final
		0x0008, // static
		0x0018, // final static
		0x0009, // public static
		0x0011, // public final
		0x0019, // public final static
		0x0000, //
		0x0410, // static
		0x0400, // abstract
		0x1001, // public synthetic
		0x2608, // static abstract interface annotation
		0x4018, // static final enum
	};

	StringBuilder errors = new StringBuilder();
	for (int i = 0; i < classes.length; i++) {
		if (classes[i].getModifiers() != modifiers[i]) {
			errors.append("Incorrect modifiers for ").append(classes[i].getName()).append(". ");
			errors.append("Expected <").append(modifiers[i]).append(">, ");
			errors.append("Actual <").append(classes[i].getModifiers()).append(">.");
			errors.append(System.lineSeparator());
			failed = true;
		}
	}

	if (failed) {
		Assert.fail("One or more tests failed:" + System.lineSeparator() + errors);
	}
}

/**
 * @tests java.lang.Class#getName()
 */
@Test
public void test_getName() {
	// Test for method java.lang.String java.lang.Class.getName()
	String className = null;
	try {
		className = Class.forName("java.lang.Object").getName();
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find java.lang.Object", false);
		return;
	}
	AssertJUnit.assertTrue( "Class getName printed wrong value:" + className,
		className.equals("java.lang.Object"));
	AssertJUnit.assertTrue( "Class getName printed wrong value:" + int.class.getName(),
		int.class.getName().equals("int"));
	try {
		className = Class.forName("[I").getName();
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find class [I", false);
		return;
	}
	AssertJUnit.assertTrue( "Class getName printed wrong value:" + className,
		className.equals("[I"));

	try {
		className = Class.forName("[Ljava.lang.Object;").getName();
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find [Ljava.lang.Object;", false);
		return;
	}

	AssertJUnit.assertTrue( "Class getName printed wrong value:" + className,
		className.equals("[Ljava.lang.Object;"));
}

/**
 * @tests java.lang.Class#getResource(java.lang.String)
 */
@Test
public void test_getResource() {
	// Test for method java.net.URL java.lang.Class.getResource(java.lang.String)
	System.setSecurityManager(new SecurityManager());
	try {
		java.net.URL res = Object.class.getResource("Object.class");
		AssertJUnit.assertTrue("Object.class should not be found", res == null);
	} finally {
		System.setSecurityManager(null);
	}
	
	String name = "/org/openj9/resources/openj9tr_Foo.c";
	
	// find resource from object
	AssertJUnit.assertTrue("directory of this class can be found",
			Test_Class.class.getResource(name) != null);
	
	// find resource from array of objects
	AssertJUnit.assertTrue("directory of array of this class can be found",
			Test_Class[].class.getResource(name) != null);
}

/**
 * @tests java.lang.Class#getResourceAsStream(java.lang.String)
 */
@Test
public void test_getResourceAsStream() {
	// Test for method java.io.InputStream java.lang.Class.getResourceAsStream(java.lang.String)
	String name = Support_Resources.RESOURCE_PACKAGE + "openj9tr_compressD.txt";
	Class clazz = null;
	try {
		clazz = Class.forName("org.openj9.test.java.lang.Test_Class");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class org.openj9.test.java.lang.Test_Class", false);
	}
	AssertJUnit.assertTrue("the file " + name + " can not be found in this directory",
		 clazz.getResourceAsStream(name) != null);

	System.setSecurityManager(new SecurityManager());
	try {
		InputStream res = Object.class.getResourceAsStream("Object.class");
		AssertJUnit.assertTrue("Object.class should not be found", res == null);
	} finally {
		System.setSecurityManager(null);
	}

	name = "org/openj9/resources/openj9tr_Foo.c";
	AssertJUnit.assertTrue("the file " + name + " should not be found in this directory",
		clazz.getResourceAsStream(name) == null);
	AssertJUnit.assertTrue("the file " + name + " can be found in the root directory",
		clazz.getResourceAsStream("/" + name) != null);
	// find resource from array of objects
	AssertJUnit.assertTrue("the file " + name + " can be found in the root directory where the class is an array",
			Test_Class[].class.getResourceAsStream("/" + name) != null);

	try {
		clazz = Class.forName("java.lang.Object");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class java.lang.Object", false);
	}
	InputStream str = clazz.getResourceAsStream("Class.class");
	AssertJUnit.assertTrue(
		"java.lang.Object couldn't find its class with getResource...",
		str != null);
	try {
		AssertJUnit.assertTrue("Cannot read single byte", str.read() != -1);
		AssertJUnit.assertTrue("Cannot read multiple bytes", str.read(new byte[5]) == 5);
		str.close();
	} catch (IOException e) {
		AssertJUnit.assertTrue("Exception while closing resource stream 1.", false);
	}

	InputStream str2 = getClass().getResourceAsStream(Support_Resources.RESOURCE_PACKAGE + "openj9tr_compressD.txt");
	AssertJUnit.assertTrue("Can't find resource", str2 != null);
	try {
		AssertJUnit.assertTrue("Cannot read single byte", str2.read() != -1);
		AssertJUnit.assertTrue("Cannot read multiple bytes", str2.read(new byte[5]) == 5);
		str2.close();
	} catch (IOException e) {
		AssertJUnit.assertTrue("Exception while closing resource stream 2.", false);
	}

}

/**
 * @tests java.lang.Class#getSuperclass()
 */
@Test
public void test_getSuperclass() {
	// Test for method java.lang.Class java.lang.Class.getSuperclass()

	AssertJUnit.assertTrue("Object has a superclass???",
		Object.class.getSuperclass() == null);
	AssertJUnit.assertTrue("Normal class has bogus superclass",
		java.io.FileInputStream.class.getSuperclass()
			== java.io.InputStream.class);
	AssertJUnit.assertTrue("Array class has bogus superclass",
		java.io.FileInputStream[].class.getSuperclass()
			== Object.class);
	AssertJUnit.assertTrue("Base class has a superclass",
		int.class.getSuperclass()
			== null);
	AssertJUnit.assertTrue("Interface class has a superclass",
		Cloneable.class.getSuperclass()
			== null);
}

/**
 * @tests java.lang.Class#isArray()
 */
@Test
public void test_isArray() {
	// Test for method boolean java.lang.Class.isArray()
	AssertJUnit.assertTrue( "Non-array type claims to be." , !int.class.isArray());
	Class clazz = null;
	try {
		clazz = Class.forName("[I");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class [I", false);
	}
	AssertJUnit.assertTrue( "int Array type claims not to be." , clazz.isArray());

	try {
		clazz = Class.forName("[Ljava.lang.Object;");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class [Ljava.lang.Object;", false);
	}

	AssertJUnit.assertTrue( "Object Array type claims not to be." , clazz.isArray());

	try {
		clazz = Class.forName("java.lang.Object");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class java.lang.Object", false);
	}
	AssertJUnit.assertTrue( "Non-array Object type claims to be." , !clazz.isArray());
}

/**
 * @tests java.lang.Class#isAssignableFrom(java.lang.Class)
 */
@Test
public void test_isAssignableFrom() {
	// Test for method boolean java.lang.Class.isAssignableFrom(java.lang.Class)
	Class clazz1 = null;
	Class clazz2 = null;
	try {
		clazz1 = Class.forName("java.lang.Object");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class java.lang.Object", false);
	}
	try {
		clazz2 = Class.forName("java.lang.Class");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class java.lang.Class", false);
	}
	AssertJUnit.assertTrue("returned false for superclass", clazz1.isAssignableFrom(clazz2));

	try {
		clazz1 = Class.forName("org.openj9.test.java.lang.Test_Class$ClassTest");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class org.openj9.test.java.lang.Test_Class$ClassTest", false);
	}
	AssertJUnit.assertTrue("returned false for same class", clazz1.isAssignableFrom(clazz1));

	try {
		clazz1 = Class.forName("java.lang.Runnable");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class java.lang.Runnable", false);
	}
	try {
		clazz2 = Class.forName("java.lang.Thread");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class java.lang.Thread", false);
	}
	AssertJUnit.assertTrue("returned false for implemented interface", clazz1.isAssignableFrom(clazz2));
}

/**
 * @tests java.lang.Class#isInterface()
 */
@Test
public void test_isInterface() {
	// Test for method boolean java.lang.Class.isInterface()
	AssertJUnit.assertTrue( "Prim type claims to be interface." , !int.class.isInterface());
	Class clazz = null;
	try {
		clazz = Class.forName("[I");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class [I", false);
	}
	AssertJUnit.assertTrue( "Prim Array type claims to be interface." , !clazz.isInterface());

	try {
		clazz = Class.forName("java.lang.Runnable");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class java.lang.Runnable", false);
	}
	AssertJUnit.assertTrue( "Interface type claims not to be interface." , clazz.isInterface());

	try {
		clazz = Class.forName("java.lang.Object");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class java.lang.Object", false);
	}
	AssertJUnit.assertTrue( "Object type claims to be interface." , !clazz.isInterface());

	try {
		clazz = Class.forName("[Ljava.lang.Object;");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class [Ljava.lang.Object;", false);
	}
	AssertJUnit.assertTrue( "Array type claims to be interface." , !clazz.isInterface());
}

/**
 * @tests java.lang.Class#isPrimitive()
 */
@Test
public void test_isPrimitive() {
	// Test for method boolean java.lang.Class.isPrimitive()
	AssertJUnit.assertTrue( "Interface type claims to be primitive." , !Runnable.class.isPrimitive());
	AssertJUnit.assertTrue( "Object type claims to be primitive." , !Object.class.isPrimitive());
	AssertJUnit.assertTrue( "Prim Array type claims to be primitive." , !int[].class.isPrimitive());
	AssertJUnit.assertTrue( "Array type claims to be primitive." , !Object[].class.isPrimitive());
	AssertJUnit.assertTrue( "Prim type claims not to be primitive." , int.class.isPrimitive());
	AssertJUnit.assertTrue( "Object type claims to be primitive." , !Object.class.isPrimitive());
}

/**
 * @tests java.lang.Class#newInstance()
 */
@Test
public void test_newInstance() {
	// Test for method java.lang.Object java.lang.Class.newInstance()

	Class clazz = null;
	try {
		try {
			clazz = Class.forName("java.lang.Object");
		} catch (ClassNotFoundException e) {
			AssertJUnit.assertTrue("Should be able to find the class java.lang.Object", false);
		}
		AssertJUnit.assertTrue(
			"new object instance was null",
			clazz.newInstance() != null);
	} catch (Exception e) {
		AssertJUnit.assertTrue(
			"Unexpected exception " + e + " in newInstance()",
			false);
	}
	try {
		try {
			clazz = Class.forName("java.lang.Throwable");
		} catch (ClassNotFoundException e) {
			AssertJUnit.assertTrue("Should be able to find the class java.lang.Throwable", false);
		}
		AssertJUnit.assertTrue(
			"new Throwable instance was not a throwable",
			clazz.newInstance().getClass()
				== clazz);
	} catch (Exception e) {
		AssertJUnit.assertTrue(
			"Unexpected exception " + e + " in newInstance()",
			false);
	}
	int r = 0;
	try {
		try {
			clazz = Class.forName("java.lang.Integer");
		} catch (ClassNotFoundException e) {
			AssertJUnit.assertTrue("Should be able to find the class java.lang.Integer", false);
		}
		AssertJUnit.assertTrue(
			"Allowed to do newInstance, when no default constructor",
			clazz.newInstance() != null
				|| clazz.newInstance() == null);
	} catch (Exception e) {
		r = 1;
	}
	AssertJUnit.assertTrue("Exception for instantiating a newInstance with no default constructor is not thrown",r ==1);
	// There needs to be considerably more testing here.
	//Assert.fail( "Test Test_Class.test_24_newInstance not written");
}

/**
 * @tests java.lang.Class#toString()
 */
@Test
public void test_toString() {
	// Test for method java.lang.String java.lang.Class.toString()
	AssertJUnit.assertTrue( "Class toString printed wrong value:" + int.class.toString(),
		int.class.toString().equals("int"));
	Class clazz = null;
	try {
		clazz = Class.forName("[I");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class [I", false);
	}
	AssertJUnit.assertTrue( "Class toString printed wrong value:" + clazz.toString(),
		clazz.toString().equals("class [I"));

	try {
		clazz = Class.forName("java.lang.Object");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class java.lang.Object", false);
	}
	AssertJUnit.assertTrue( "Class toString printed wrong value:" + clazz.toString(),
		clazz.toString().equals("class java.lang.Object"));

	try {
		clazz = Class.forName("[Ljava.lang.Object;");
	} catch (ClassNotFoundException e) {
		AssertJUnit.assertTrue("Should be able to find the class [Ljava.lang.Object;", false);
	}
	AssertJUnit.assertTrue( "Class toString printed wrong value:" + clazz.toString(),
		clazz.toString().equals("class [Ljava.lang.Object;"));
}

/**
 * @tests java.lang.Class#toGenericString()
 */
@Test
@SuppressWarnings("rawtypes")
public void test_toGenericString() {
	// Test for method java.lang.String java.lang.Class.toGenericString()
	AssertJUnit.assertEquals("class org.openj9.test.java.lang.DefaultClass", DefaultClass.class.toGenericString());
	AssertJUnit.assertEquals("public class org.openj9.test.java.lang.PublicStrictFPClass", PublicStrictFPClass.class.toGenericString());
	AssertJUnit.assertEquals("public final class org.openj9.test.java.lang.PublicFinalClass", PublicFinalClass.class.toGenericString());
	AssertJUnit.assertEquals("public final enum org.openj9.test.java.lang.PublicEnum", PublicEnum.class.toGenericString());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.PublicFinalClass[]", PublicFinalClass[].class.toGenericString());
	AssertJUnit.assertEquals("class org.openj9.test.java.lang.GenericClass<E>", GenericClass.class.toGenericString());
	AssertJUnit.assertEquals("class org.openj9.test.java.lang.GenericsClass<first,second>", GenericsClass.class.toGenericString());
	AssertJUnit.assertEquals("public class org.openj9.test.java.lang.InnerClasses$InnerClass", InnerClasses.InnerClass.class.toGenericString());
	AssertJUnit.assertEquals("public static class org.openj9.test.java.lang.InnerClasses$InnerStaticClass", InnerClasses.InnerStaticClass.class.toGenericString());
	AssertJUnit.assertEquals("protected class org.openj9.test.java.lang.InnerClasses$ProtectedClass", InnerClasses.getProtectedClass().toGenericString());
	AssertJUnit.assertEquals("private class org.openj9.test.java.lang.InnerClasses$PrivateClass", InnerClasses.getPrivateClass().toGenericString());
	AssertJUnit.assertEquals("abstract interface org.openj9.test.java.lang.Interface", Interface.class.toGenericString());
	AssertJUnit.assertEquals("public abstract @interface org.openj9.test.java.lang.AnnotationType", AnnotationType.class.toGenericString());
	AssertJUnit.assertEquals("char[]", char[].class.toGenericString());
	Class[] classes = getTestClasses();
	AssertJUnit.assertEquals("public class org.openj9.test.java.lang.Test_Class", classes[0].toGenericString());
	AssertJUnit.assertEquals("static class org.openj9.test.java.lang.Test_Class$StaticMember$Class", classes[1].toGenericString());
	AssertJUnit.assertEquals("class org.openj9.test.java.lang.Test_Class$Member$Class", classes[2].toGenericString());
	AssertJUnit.assertEquals("class org.openj9.test.java.lang.Test_Class$1Local$Class", classes[3].toGenericString());
	AssertJUnit.assertEquals("class org.openj9.test.java.lang.Test_Class$6", classes[4].toGenericString());
	AssertJUnit.assertEquals("int", classes[5].toGenericString());
	AssertJUnit.assertEquals("class org.openj9.test.java.lang.Test_Class$1Local$Class$Member6$E", classes[6].toGenericString());
	AssertJUnit.assertEquals("class org.openj9.test.java.lang.Test_Class$StaticMember$Class$Member2$A", classes[7].toGenericString());
	AssertJUnit.assertEquals("class org.openj9.test.java.lang.Test_Class$Member$Class$Member3$B", classes[8].toGenericString());
}

/**
 * @tests java.lang.Class - getTypeName
 */
@Test
public void test_getTypeName() {
	String typeName = Test_Class.class.getTypeName();
	String LANG_TEST_CLASS = "org.openj9.test.java.lang.Test_Class";
	AssertJUnit.assertEquals("Wrong name for ordinary class", LANG_TEST_CLASS, typeName);
	typeName = SubClassTest.class.getTypeName();
	AssertJUnit.assertEquals("Wrong name for inner class", LANG_TEST_CLASS+"$SubClassTest", typeName);
	typeName = org.openj9.test.java.lang.Test_Class[].class.getTypeName();
	AssertJUnit.assertEquals("Wrong name for array class", LANG_TEST_CLASS+"[]", typeName);
	typeName = org.openj9.test.java.lang.Test_Class[][].class.getTypeName();
	AssertJUnit.assertEquals("Wrong name for 2-D array class", LANG_TEST_CLASS+"[][]", typeName);

	AssertJUnit.assertEquals("org.openj9.test.java.lang.DefaultClass", DefaultClass.class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.PublicStrictFPClass", PublicStrictFPClass.class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.PublicFinalClass", PublicFinalClass.class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.PublicEnum", PublicEnum.class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.PublicFinalClass[]", PublicFinalClass[].class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.GenericClass", GenericClass.class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.GenericsClass", GenericsClass.class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.InnerClasses$InnerClass", InnerClasses.InnerClass.class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.InnerClasses$InnerStaticClass", InnerClasses.InnerStaticClass.class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.InnerClasses$ProtectedClass", InnerClasses.getProtectedClass().getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.InnerClasses$PrivateClass", InnerClasses.getPrivateClass().getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.Interface", Interface.class.getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.AnnotationType", AnnotationType.class.getTypeName());
	AssertJUnit.assertEquals("char[]", char[].class.getTypeName());
	Class[] classes = getTestClasses();
	AssertJUnit.assertEquals("org.openj9.test.java.lang.Test_Class", classes[0].getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.Test_Class$StaticMember$Class", classes[1].getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.Test_Class$Member$Class", classes[2].getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.Test_Class$1Local$Class", classes[3].getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.Test_Class$6", classes[4].getTypeName());
	AssertJUnit.assertEquals("int", classes[5].getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.Test_Class$1Local$Class$Member6$E", classes[6].getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.Test_Class$StaticMember$Class$Member2$A", classes[7].getTypeName());
	AssertJUnit.assertEquals("org.openj9.test.java.lang.Test_Class$Member$Class$Member3$B", classes[8].getTypeName());
}

/**
 * @tests java.lang.Class#getEnclosingConstructor()
 */
@Test
public void test_getEnclosingConstructor(){
	java.lang.reflect.Constructor constructor = null;
	try {
		constructor = TestClasses.class.getDeclaredConstructor(new Class[0]);
	} catch (NoSuchMethodException e) {
		Assert.fail("unexpected: " + e);
	}
	Class[] classes = new TestClasses().getConstructorClasses();
	for (int i=0; i<classes.length; i++) {
		AssertJUnit.assertTrue(i + ": " + classes[i] + " should not have enclosing method: " + classes[i].getEnclosingMethod(), classes[i].getEnclosingMethod() == null);
		if (i == 3 || i == 4) {
			AssertJUnit.assertTrue(i + ": " + classes[i] + " should not have enclosing constructor: " + classes[i].getEnclosingConstructor(), constructor.equals(classes[i].getEnclosingConstructor()));
		} else {
			AssertJUnit.assertTrue(i + ": " + classes[i] + " should not have enclosing constructor: " + classes[i].getEnclosingConstructor(), classes[i].getEnclosingConstructor() == null);
		}
		Class arrayClass = java.lang.reflect.Array.newInstance(classes[i], 0).getClass();
		AssertJUnit.assertTrue(i + ": " + arrayClass + " should not have enclosing constructor: " + arrayClass.getEnclosingConstructor(), arrayClass.getEnclosingConstructor() == null);
	}
}

/**
 * @tests java.lang.Class#getEnclosingMethod()
 */
@Test
public void test_getEnclosingMethod(){
	java.lang.reflect.Method method = null;
	try {
		method = getClass().getDeclaredMethod("getTestClasses", new Class[0]);
	} catch (NoSuchMethodException e) {
		Assert.fail("unexpected: " + e);
	}
	Class[] classes = getTestClasses();
	for (int i=0; i<classes.length; i++) {
		AssertJUnit.assertTrue(i + ": " + classes[i] + " should not have enclosing constructor: " + classes[i].getEnclosingConstructor(), classes[i].getEnclosingConstructor() == null);
		if (i == 3 || i == 4) {
			AssertJUnit.assertTrue(i + ": " + classes[i] + " should not have enclosing method: " + classes[i].getEnclosingMethod(), classes[i].getEnclosingMethod().equals(method));
		} else {
			AssertJUnit.assertTrue(i + ": " + classes[i] + " should not have enclosing method: " + classes[i].getEnclosingMethod(), classes[i].getEnclosingMethod() == null);
		}
		Class arrayClass = java.lang.reflect.Array.newInstance(classes[i], 0).getClass();
		AssertJUnit.assertTrue(i + ": " + arrayClass + " should not have enclosing method: " + arrayClass.getEnclosingMethod(), arrayClass.getEnclosingMethod() == null);
	}
}

interface TestInterface {
	default void testFunc1() {
		class LocalClass {
			public void print() {
				System.out.println("This is within a localClass within testFunc1");
			}
		};
		new LocalClass().print();
	};

	void testFunc2();

	static void testFunc3() {
		class LocalClass {
			public void print() {
				System.out.println("This is a localClass within testFunc3");
			}
		};
	}
}

/**
 * @tests java.lang.Class#getEnclosingMethod()
 */
@Test
public void test_getEnclosingMethod2() throws Throwable {
	// Test scenario #1: enclosing method is an interface default method
	Method enclosingMethodTestFunc1 = TestInterface.class.getDeclaredMethod("testFunc1", new Class[0]);
	Method method = Class.forName("org.openj9.test.java.lang.Test_Class$TestInterface$1LocalClass").getEnclosingMethod();
	AssertJUnit.assertTrue("org.openj9.test.java.lang.Test_Class$TestInterface$1LocalClass enclosing method expected: <" + enclosingMethodTestFunc1 + ">, received: <"  + method + ">",
		enclosingMethodTestFunc1.equals(method));

	// Test scenario #2: enclosing method is an interface static method
	Method enclosingMethodTestFunc3 = TestInterface.class.getDeclaredMethod("testFunc3", new Class[0]);
	method = Class.forName("org.openj9.test.java.lang.Test_Class$TestInterface$2LocalClass").getEnclosingMethod();
	AssertJUnit.assertTrue("org.openj9.test.java.lang.Test_Class$TestInterface$2LocalClass enclosing method expected: <" + enclosingMethodTestFunc3 + ">, received: <"  + method + ">",
		enclosingMethodTestFunc3.equals(method));

	// Test scenario #3: enclosing method is an interface default method, enclosing constructor should be null
	Constructor<?> construtor = Class.forName("org.openj9.test.java.lang.Test_Class$TestInterface$1LocalClass").getEnclosingConstructor();
	AssertJUnit.assertTrue("org.openj9.test.java.lang.Test_Class$TestInterface$1LocalClass enclosing constructor expected: <null>, received: <"  + construtor + ">",
		null == construtor);

	// Test scenario #4: enclosing method is an interface static method, enclosing constructor should be null
	construtor = Class.forName("org.openj9.test.java.lang.Test_Class$TestInterface$2LocalClass").getEnclosingConstructor();
	AssertJUnit.assertTrue("org.openj9.test.java.lang.Test_Class$TestInterface$2LocalClass enclosing constructor expected: <null>, received: <"  + construtor + ">",
		null == construtor);
}

/**
 * @tests java.lang.Class#getEnclosingClass()
 */
@Test
public void test_getEnclosingClass(){
	Class[] classes = getTestClasses();
	for (int i=0; i<classes.length; i++) {
		if (i == 6 || i == 7 || i == 8) {
			int result = -1;
			if (i == 6) result = 3;
			else if (i == 7) result = 1;
			else if (i == 8) result = 2;
			AssertJUnit.assertTrue(i + ": " + classes[i] + " should not have enclosing class: " + classes[i].getEnclosingClass(), classes[i].getEnclosingClass() == classes[result]);
		} else if (i == 1 || i == 2 || i == 3 || i == 4) {
			AssertJUnit.assertTrue(i + ": " + classes[i] + " should not have enclosing class: " + classes[i].getEnclosingClass(), classes[i].getEnclosingClass() == getClass());
		} else {
			AssertJUnit.assertTrue(i + ": " + classes[i] + " should not have enclosing class: " + classes[i].getEnclosingClass(), classes[i].getEnclosingClass() == null);
		}
		Class arrayClass = java.lang.reflect.Array.newInstance(classes[i], 0).getClass();
		AssertJUnit.assertTrue(i + ": " + arrayClass + " should not have enclosing class: " + arrayClass.getEnclosingClass(), arrayClass.getEnclosingClass() == null);
	}
}

/**
 * @tests java.lang.Class#getSimpleName()
 */
@Test
public void test_getSimpleName(){
	String[] results = new String[] {
			"Test_Class", 					// 0
			"StaticMember$Class",			// 1
			"Member$Class",					// 2
			"Local$Class",					// 3
			"",								// 4
			"int",							// 5
			"Member6$E",					// 6
			"Member2$A",					// 7
			"Member3$B",					// 8
	};
	Class[] classes = getTestClasses();
	for (int i=0; i<classes.length; i++) {
		String simpleName = classes[i].getSimpleName();
		AssertJUnit.assertTrue(i + ") unexpected simple name " + simpleName, simpleName != null && simpleName.equals(results[i]));
		Class arrayClass = java.lang.reflect.Array.newInstance(classes[i], 0).getClass();
		simpleName = arrayClass.getSimpleName();
		AssertJUnit.assertTrue(i + ") unexpected simple name " + simpleName + "[]", simpleName != null && simpleName.equals(results[i] + "[]"));
	}
}

/**
 * @tests java.lang.Class#getCanonicalName()
 */
@Test
public void test_getCanonicalName(){
	String packageName = getClass().getName();
	packageName = packageName.substring(0, packageName.lastIndexOf('.') + 1);
	System.out.println(packageName);
	String[] results = new String[] {
			"Test_Class", 						// 0
			"Test_Class.StaticMember$Class",	// 1
			"Test_Class.Member$Class",			// 2
			null,								// 3
			null,								// 4
			"int",								// 5
			null,								// 6
			"Test_Class.StaticMember$Class.Member2$A",	// 7
			"Test_Class.Member$Class.Member3$B",		// 8
	};
	Class[] classes = getTestClasses();
	for (int i=0; i<classes.length; i++) {
		String canonicalName = classes[i].getCanonicalName();
		String result = results[i];
		if (result != null && i != 5) {
			result = packageName + result;
		}
		AssertJUnit.assertTrue(i + ") unexpected canonial name " + canonicalName, canonicalName == null ?
				canonicalName == results[i] : canonicalName.equals(result));
		Class arrayClass = java.lang.reflect.Array.newInstance(classes[i], 0).getClass();
		canonicalName = arrayClass.getCanonicalName();
		AssertJUnit.assertTrue(i + ") unexpected canonial name " + canonicalName, canonicalName == null ?
				canonicalName == results[i] : canonicalName.equals(result + "[]"));
	}
}

/**
 * @tests java.lang.Class#isAnonymousClass()
 */
@Test
public void test_isAnonymousClass(){
	Class[] classes = getTestClasses();
	for (int i=0; i<classes.length; i++) {
		if (i == 4) {
			AssertJUnit.assertTrue(i + ") Should be anonymous: " + classes[i], classes[i].isAnonymousClass());
		} else {
			AssertJUnit.assertFalse(i + ") Should not be anonymous: " + classes[i], classes[i].isAnonymousClass());
		}
		Class arrayClass = java.lang.reflect.Array.newInstance(classes[i], 0).getClass();
		AssertJUnit.assertFalse(i + ") should not be anonymous: " + arrayClass, arrayClass.isAnonymousClass());
	}
}

/**
 * @tests java.lang.Class#isLocalClass()
 */
@Test
public void test_isLocalClass(){
	Class[] classes = getTestClasses();
	for (int i=0; i<classes.length; i++) {
		if (i == 3) {
			AssertJUnit.assertTrue(i + ") Should be local: " + classes[i], classes[i].isLocalClass());
		} else {
			AssertJUnit.assertFalse(i + ") Should not be local: " + classes[i], classes[i].isLocalClass());
		}
		Class arrayClass = java.lang.reflect.Array.newInstance(classes[i], 0).getClass();
		AssertJUnit.assertFalse(i + ") should not be local: " + arrayClass, arrayClass.isLocalClass());
	}
}

/**
 * @tests java.lang.Class#isMemberClass()
 */
@Test
public void test_isMemberClass(){
	Class[] classes = getTestClasses();
	for (int i=0; i<classes.length; i++) {
		if (i == 1 || i == 2 || i == 6 || i == 7 || i == 8) {
			AssertJUnit.assertTrue(i + ") Should be member: " + classes[i], classes[i].isMemberClass());
		} else {
			AssertJUnit.assertFalse(i + ") Should not be member: " + classes[i], classes[i].isMemberClass());
		}
		Class arrayClass = java.lang.reflect.Array.newInstance(classes[i], 0).getClass();
		AssertJUnit.assertFalse(i + ") should not be member: " + arrayClass, arrayClass.isMemberClass());
	}
}

/**
 * @tests java.lang.Class#getConstructor(java.lang.Class[])
 */
@Test
public void test_reflectConstructor() {
	final String loadName = getClass().getName();
	class TestLoader extends ClassLoader {
		int count = 0;
		public TestLoader() {
			super();
		}
		public Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
			// Must throw an exception before calling findLoadedClass()
			if (className.equals(loadName)) {
				count++;
				if (count > 1) throw new ClassNotFoundException("Already loaded");
			}
			return super.loadClass(className, resolve);
		}
	}
	
	ClassLoader loader = new TestLoader();
	try {
		Class testClass = Class.forName(loadName, false, loader);
		Constructor c = testClass.getConstructor(new Class[0]);
		// run enough iterations so the Constructor is optimized to bytecodes
		for (int i=0; i<500; i++) {
			try {
				c.newInstance(new Object[0]);
			} catch (Error e) {
				Assert.fail("Failure at " + i + ": " + e);
			}
		}
	} catch (Throwable e) {
		Assert.fail("unexpected: " + e);
	}
}

/**
 * @tests java.lang.Class#getMethod(java.lang.String, java.lang.Class[])
 */
public void dummyReflectHelper() {
}

/**
 * @tests java.lang.Class#getMethod(java.lang.String, java.lang.Class[])
 */
@Test
public void test_reflectMethod() {
	final String loadName = getClass().getName();
	class TestLoader extends ClassLoader {
		int count = 0;
		public TestLoader() {
			super();
		}
		public Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
			// Must throw an exception before calling findLoadedClass()
			if (className.equals(loadName)) {
				count++;
				if (count > 1) throw new ClassNotFoundException("Already loaded");
			}
			return super.loadClass(className, resolve);
		}
	}
	
	ClassLoader loader = new TestLoader();
	try {
		Class testClass = Class.forName(loadName, false, loader);
		Object testInstance = testClass.newInstance();
		Method m = testClass.getMethod("dummyReflectHelper", new Class[0]);
		// run enough iterations so the Method is optimized to bytecodes
		for (int i=0; i<500; i++) {
			try {
				m.invoke(testInstance, new Object[0]);
			} catch (Error e) {
				Assert.fail("Failure at " + i + ": " + e);
			}
		}
	} catch (Throwable e) {
		Assert.fail("unexpected: " + e);
	}
}

public Vector dummyReflectFieldObject = new Vector();
public int dummyReflectFieldInt = 2;
public byte dummyReflectFieldByte = 3;
public short dummyReflectFieldShort = 4;
public long dummyReflectFieldLong = 5;
public char dummyReflectFieldChar = 6;
public boolean dummyReflectFieldBoolean = true;
public float dummyReflectFieldFloat = 8;
public double dummyReflectFieldDouble = 9;

/**
 * @tests java.lang.Class#getField(java.lang.String)
 */
@Test
public void test_reflectField() {
	final String loadName = getClass().getName();
	class TestLoader extends ClassLoader {
		int count = 0;
		public TestLoader() {
			super();
		}
		public Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
			// Must throw an exception before calling findLoadedClass()
			if (className.equals(loadName)) {
				count++;
				if (count > 1) throw new ClassNotFoundException("Already loaded");
			}
			return super.loadClass(className, resolve);
		}
	}
	
	ClassLoader loader = new TestLoader();
	try {
		Class testClass = Class.forName(loadName, false, loader);
		Object testInstance = testClass.newInstance();
		Field fObject = testClass.getField("dummyReflectFieldObject");
		Field fInt = testClass.getField("dummyReflectFieldInt");
		Field fByte = testClass.getField("dummyReflectFieldByte");
		Field fShort = testClass.getField("dummyReflectFieldShort");
		Field fLong = testClass.getField("dummyReflectFieldLong");
		Field fChar = testClass.getField("dummyReflectFieldChar");
		Field fBoolean = testClass.getField("dummyReflectFieldBoolean");
		Field fFloat = testClass.getField("dummyReflectFieldFloat");
		Field fDouble = testClass.getField("dummyReflectFieldDouble");
		// run enough iterations so the Field could be optimized
		for (int i=0; i<500; i++) {
			try {
				((Vector)fObject.get(testInstance)).size();
				AssertJUnit.assertTrue("Int at " + i, fInt.getInt(testInstance) == 2);
				AssertJUnit.assertTrue("Byte at " + i, fByte.getByte(testInstance) == 3);
				AssertJUnit.assertTrue("Short at " + i, fShort.getShort(testInstance) == 4);
				AssertJUnit.assertTrue("Long at " + i, fLong.getLong(testInstance) == 5);
				AssertJUnit.assertTrue("Char at " + i, fChar.getChar(testInstance) == 6);
				AssertJUnit.assertTrue("Boolean at " + i, fBoolean.getBoolean(testInstance));
				AssertJUnit.assertTrue("Float at " + i, fFloat.getFloat(testInstance) == 8);
				AssertJUnit.assertTrue("Double at " + i, fDouble.getDouble(testInstance) == 9);
			} catch (Error e) {
				Assert.fail("Failure at " + i + ": " + e);
			}
		}
	} catch (Throwable e) {
		e.printStackTrace();
		Assert.fail("unexpected: " + e);
	}
}

interface IM2 {
	public void m1();
}
interface IM1 {
	public void m1();
}
interface IM0 extends IM1, IM2 {
}

/**
 * @tests java.lang.Class#getMethods()
 */
@Test
public void test_getMethods_subtest0() {
	Method[] methods = IM0.class.getMethods();
	AssertJUnit.assertEquals("wrong length for IM0: ", 2, methods.length);
	AssertJUnit.assertTrue("wrong class 1", methods[0].getDeclaringClass() == IM1.class);
	AssertJUnit.assertEquals("wrong name 1", "m1", methods[0].getName());
	AssertJUnit.assertTrue("wrong class 2", methods[1].getDeclaringClass() == IM2.class);
	AssertJUnit.assertEquals("wrong name 2", "m1", methods[1].getName());
}

class IF2 {
	public int f1;
}
class IF1 extends IF2 {
	public int f1;
}

/**
 * @tests java.lang.Class#getFields()
 */
@Test
public void test_getFields_subtest0() {
	Field[] fields = IF1.class.getFields();
	AssertJUnit.assertTrue("wrong length: " + fields.length, fields.length == 2);
	AssertJUnit.assertTrue("wrong class 1", fields[0].getDeclaringClass() == IF1.class);
	AssertJUnit.assertTrue("wrong name 1", fields[0].getName().equals("f1"));
	AssertJUnit.assertTrue("wrong class 2", fields[1].getDeclaringClass() == IF2.class);
	AssertJUnit.assertTrue("wrong name 2", fields[1].getName().equals("f1"));
}

enum Enum1 {ONE{}}

@Test
public void test_isEnum() {
	AssertJUnit.assertTrue("Enum1 is not enum", Enum1.class.isEnum());
	AssertJUnit.assertTrue("enum value class isEnum()", !Enum1.ONE.getClass().isEnum());
}

interface FakeAnnotation extends Annotation {
}

@interface RealAnnotation {
}

@Test
public void test_isAnnotation() {
	AssertJUnit.assertTrue("RealAnnotation is not annotation", RealAnnotation.class.isAnnotation());
	AssertJUnit.assertTrue("FakeAnnotation is annotation", !FakeAnnotation.class.isAnnotation());
}

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@Repeatable(ContainerAnn.class)
@interface RepeatableAnn { String value(); }

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@Repeatable(ContainerContainerAnn.class)
@interface ContainerAnn { RepeatableAnn[] value(); }

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@interface ContainerContainerAnn { ContainerAnn[] value(); }

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@interface RetroAnn { String value(); }

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@interface RetroAnn2 { String value(); }

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@interface RetroAnn3{ String value(); }

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@interface RetroAnn4{ String value(); }

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@interface RetroAnn5{ String value(); }

@Inherited
@Retention(RetentionPolicy.RUNTIME)
@interface RetroAnn6{ String value(); }

class AnnoBlank {
}

@ContainerContainerAnn({@ContainerAnn({@RepeatableAnn("Nested-1"), @RepeatableAnn("Nested-2")}),@ContainerAnn({@RepeatableAnn("Nested-3"), @RepeatableAnn("Nested-4")})})
class AnnoNested {

}

@RepeatableAnn("GrandParent-1")
@RetroAnn4("GrandParent-2")
@RepeatableAnn("GrandParent-3")
@RetroAnn5("GrandParent-4")
@RepeatableAnn("GrandParent-5")
@RetroAnn6("GrandParent-6")
class AnnoGrandParent {
}

@RetroAnn2("Parent-7")
@RepeatableAnn("Parent-1")
@RepeatableAnn("Parent-2")
@RepeatableAnn("Parent-3")
@RepeatableAnn("Parent-4")
@RepeatableAnn("Parent-5")
@RetroAnn("Parent-6")
class AnnoParent extends AnnoGrandParent{
}

@RepeatableAnn("Child-1")
@RepeatableAnn("Child-2")
@RetroAnn3("Child-5")
@RepeatableAnn("Child-3")
@RepeatableAnn("Child-4")
class AnnoChild extends AnnoParent {
}

@RetroAnn4("GrandParentNR-1")
@RetroAnn5("GrandParentNR-2")
@RepeatableAnn("GrandParentNR-3")
@RetroAnn6("GrandParentNR-4")
@RetroAnn3("GrandParentNR-5")
class AnnoGrandParentNR {
}

@RepeatableAnn("ParentNR-1")
@RetroAnn2("ParentNR-2")
@RetroAnn("ParentNR-3")
class AnnoParentNR extends AnnoGrandParentNR {
}

@RetroAnn3("ChildNR-1")
@RepeatableAnn("ChildNR-2")
class AnnoChildNR extends AnnoParentNR {
}

@RetroAnn("GrandParentR-1")
@RepeatableAnn("GrandParentR-2")
@RepeatableAnn("GrandParentR-3")
@RetroAnn2("GrandParentR-4")
class AnnoGrandParentR {
}

@RepeatableAnn("ParentR-1")
@RetroAnn3("ParentR-2")
@RetroAnn4("ParentR-3")
class AnnoParentR extends AnnoGrandParentR {
}

@RetroAnn("ChildR-1")
@RetroAnn5("ChildR-2")
@RepeatableAnn("ChildR-3")
class AnnoChildR extends AnnoParentR {
}

private String anno2String(Annotation annotation){
	if (annotation == null) {
		return "null";
	}
	return annotation.toString();
}

private String[] AnnoExpectedValues = {
	/* 00 */ "null",
	/* 01 */ "@org.openj9.test.java.lang.Test_Class$ContainerContainerAnn(value={@org.openj9.test.java.lang.Test_Class$ContainerAnn(value={@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Nested-1\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Nested-2\")}), @org.openj9.test.java.lang.Test_Class$ContainerAnn(value={@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Nested-3\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Nested-4\")})})",
	/* 02 */ "@org.openj9.test.java.lang.Test_Class$ContainerAnn(value={@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParent-1\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParent-3\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParent-5\")})",
	/* 03 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn4(value=\"GrandParent-2\")",
	/* 04 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn5(value=\"GrandParent-4\")",
	/* 05 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn6(value=\"GrandParent-6\")",
	/* 06 */ "@org.openj9.test.java.lang.Test_Class$ContainerAnn(value={@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-1\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-2\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-3\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-4\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-5\")})",
	/* 07 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn2(value=\"Parent-7\")",
	/* 08 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn(value=\"Parent-6\")",
	/* 09 */ "@org.openj9.test.java.lang.Test_Class$ContainerAnn(value={@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Child-1\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Child-2\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Child-3\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Child-4\")})",
	/* 10 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn3(value=\"Child-5\")",
	/* 11 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn4(value=\"GrandParentNR-1\")",
	/* 12 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn5(value=\"GrandParentNR-2\")",
	/* 13 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParentNR-3\")",
	/* 14 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn6(value=\"GrandParentNR-4\")",
	/* 15 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn3(value=\"GrandParentNR-5\")",
	/* 16 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"ParentNR-1\")",
	/* 17 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn2(value=\"ParentNR-2\")",
	/* 18 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn(value=\"ParentNR-3\")",
	/* 19 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn3(value=\"ChildNR-1\")",
	/* 20 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"ChildNR-2\")",
	/* 21 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn(value=\"GrandParentR-1\")",
	/* 22 */ "@org.openj9.test.java.lang.Test_Class$ContainerAnn(value={@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParentR-2\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParentR-3\")})",
	/* 23 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn2(value=\"GrandParentR-4\")",
	/* 24 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"ParentR-1\")",
	/* 25 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn3(value=\"ParentR-2\")",
	/* 26 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn4(value=\"ParentR-3\")",
	/* 27 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn(value=\"ChildR-1\")",
	/* 28 */ "@org.openj9.test.java.lang.Test_Class$RetroAnn5(value=\"ChildR-2\")",
	/* 29 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"ChildR-3\")",
	/* 30 */ "@org.openj9.test.java.lang.Test_Class$ContainerAnn(value={@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Nested-1\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Nested-2\")})",
	/* 31 */ "@org.openj9.test.java.lang.Test_Class$ContainerAnn(value={@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Nested-3\"), @org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Nested-4\")})",
	/* 32 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParent-1\")",
	/* 33 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParent-3\")",
	/* 34 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParent-5\")",
	/* 35 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-1\")",
	/* 36 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-2\")",
	/* 37 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-3\")",
	/* 38 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-4\")",
	/* 39 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Parent-5\")",
	/* 40 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Child-1\")",
	/* 41 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Child-2\")",
	/* 42 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Child-3\")",
	/* 43 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"Child-4\")",
	/* 44 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParentR-2\")",
	/* 45 */ "@org.openj9.test.java.lang.Test_Class$RepeatableAnn(value=\"GrandParentR-3\")",

};

private void innerTest_getAnnotations(Class clazz, boolean isDeclared, int ... expected) {
	Annotation[] ann = (isDeclared) ? clazz.getDeclaredAnnotations() : clazz.getAnnotations();
	AssertJUnit.assertTrue(clazz.getSimpleName() + ", Incorrect length of annotation list. Expected: " + expected.length + " Actual: " + ann.length, ann.length == expected.length);
	for (int i = 0; i < expected.length; i++) {
		AssertJUnit.assertTrue(clazz.getSimpleName() + ", Incorrect value in annotation list. Position: " + i + ", Expected value: " + AnnoExpectedValues[expected[i]] + ", Actual Value: " + anno2String(ann[i]), anno2String(ann[i]).equals(AnnoExpectedValues[expected[i]]));
	}
}

@Test
public void test_getAnnotations() {

	innerTest_getAnnotations(AnnoBlank.class, false);
	innerTest_getAnnotations(AnnoNested.class, false, 1);
	innerTest_getAnnotations(AnnoGrandParent.class, false, 2, 3, 4, 5);
	innerTest_getAnnotations(AnnoParent.class, false, 6, 3, 4, 5, 7, 8);
	innerTest_getAnnotations(AnnoChild.class, false, 9, 3, 4, 5, 7, 8, 10);
	innerTest_getAnnotations(AnnoGrandParentNR.class, false, 11, 12, 13, 14, 15);
	innerTest_getAnnotations(AnnoParentNR.class, false, 11, 12, 16, 14, 15, 17, 18);
	innerTest_getAnnotations(AnnoChildNR.class, false, 11, 12, 20, 14, 19, 17 ,18);
	innerTest_getAnnotations(AnnoGrandParentR.class, false, 21, 22, 23);
	innerTest_getAnnotations(AnnoParentR.class, false, 21, 22, 23, 24, 25, 26);
	innerTest_getAnnotations(AnnoChildR.class, false, 27, 22, 23, 29, 25, 26, 28);
}

@Test
public void test_getDeclaredAnnotations() {

	innerTest_getAnnotations(AnnoBlank.class, true);
	innerTest_getAnnotations(AnnoNested.class, true, 1);
	innerTest_getAnnotations(AnnoGrandParent.class, true, 2, 3, 4, 5);
	innerTest_getAnnotations(AnnoParent.class, true, 7, 6, 8);
	innerTest_getAnnotations(AnnoChild.class, true, 9, 10);
	innerTest_getAnnotations(AnnoGrandParentNR.class, true, 11, 12, 13, 14, 15);
	innerTest_getAnnotations(AnnoParentNR.class, true, 16,17, 18);
	innerTest_getAnnotations(AnnoChildNR.class, true, 19, 20);
	innerTest_getAnnotations(AnnoGrandParentR.class, true, 21, 22, 23);
	innerTest_getAnnotations(AnnoParentR.class, true, 24, 25, 26);
	innerTest_getAnnotations(AnnoChildR.class, true, 27, 28, 29);
}

private void innerTest_getAnnotation(Class clazz, Class annoClazz, boolean isDeclared, int expected) {
	Annotation ann = (isDeclared) ? clazz.getDeclaredAnnotation(annoClazz) : clazz.getAnnotation(annoClazz);
	AssertJUnit.assertTrue(clazz.getSimpleName() + ", Incorrect value for: " + annoClazz.getSimpleName() + ". Expected value: " + AnnoExpectedValues[expected] + ", Actual Value: " + anno2String(ann), anno2String(ann).equals(AnnoExpectedValues[expected]));
}

private void innerTest_getAnnotationNPE(Class clazz, boolean isDeclared, boolean isByType) {
	boolean nullException = false;

	try {
		if (isByType) {
			Annotation[] ann = (isDeclared) ? clazz.getDeclaredAnnotationsByType(null) : clazz.getAnnotationsByType(null);
		} else {
			Annotation ann = (isDeclared) ? clazz.getDeclaredAnnotation(null) : clazz.getAnnotation(null);
		}
	} catch (NullPointerException e) {
		nullException = true;
	}
	AssertJUnit.assertTrue("NullPointerException was not thrown", nullException);
}

private void innerTest_getAnnotationGeneral(Class clazz, boolean isDeclared, int ... expected){
	Class[] annoClazz = {
			RepeatableAnn.class,
			ContainerAnn.class,
			ContainerContainerAnn.class,
			RetroAnn.class,
			RetroAnn2.class,
			RetroAnn3.class,
			RetroAnn4.class,
			RetroAnn5.class,
			RetroAnn6.class,
	};

	AssertJUnit.assertTrue("Invalid number of expected results.", expected.length == annoClazz.length);

	for (int i = 0; i < annoClazz.length; i++ ) {
		innerTest_getAnnotation(clazz, annoClazz[i], isDeclared, expected[i]);
	}
}

@Test
public void test_getAnnotation() {
	innerTest_getAnnotationNPE(AnnoBlank.class, false, false);
	innerTest_getAnnotationGeneral(AnnoBlank.class, false, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoNested.class, false, 0, 0, 1, 0, 0, 0, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoGrandParent.class, false, 0, 2, 0, 0, 0, 0, 3, 4, 5);
	innerTest_getAnnotationGeneral(AnnoParent.class, false, 0, 6, 0, 8, 7, 0, 3, 4, 5);
	innerTest_getAnnotationGeneral(AnnoChild.class, false, 0, 9, 0, 8, 7, 10, 3, 4, 5);
	innerTest_getAnnotationGeneral(AnnoGrandParentNR.class, false, 13, 0, 0, 0, 0, 15, 11, 12, 14);
	innerTest_getAnnotationGeneral(AnnoParentNR.class, false, 16, 0, 0, 18, 17, 15, 11, 12, 14);
	innerTest_getAnnotationGeneral(AnnoChildNR.class, false, 20, 0, 0, 18, 17, 19, 11, 12, 14);
	innerTest_getAnnotationGeneral(AnnoGrandParentR.class, false, 0, 22, 0, 21, 23, 0, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoParentR.class, false, 24, 22, 0, 21, 23, 25, 26, 0, 0);
	innerTest_getAnnotationGeneral(AnnoChildR.class, false, 29, 22, 0, 27, 23, 25, 26, 28, 0);

}

@Test
public void test_getDeclaredAnnotation() {
	innerTest_getAnnotationNPE(AnnoBlank.class, true, false);
	innerTest_getAnnotationGeneral(AnnoBlank.class, true, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoNested.class, true, 0, 0, 1, 0, 0, 0, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoGrandParent.class, true, 0, 2, 0, 0, 0, 0, 3, 4, 5);
	innerTest_getAnnotationGeneral(AnnoParent.class, true, 0, 6, 0, 8, 7, 0, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoChild.class, true, 0, 9, 0, 0, 0, 10, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoGrandParentNR.class, true, 13, 0, 0, 0, 0, 15, 11, 12, 14);
	innerTest_getAnnotationGeneral(AnnoParentNR.class, true, 16, 0, 0, 18, 17, 0, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoChildNR.class, true, 20, 0, 0, 0, 0, 19, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoGrandParentR.class, true, 0, 22, 0, 21, 23, 0, 0, 0, 0);
	innerTest_getAnnotationGeneral(AnnoParentR.class, true, 24, 0, 0, 0, 0, 25, 26, 0, 0);
	innerTest_getAnnotationGeneral(AnnoChildR.class, true, 29, 0, 0, 27, 0, 0, 0, 28, 0);
}

private <A extends Annotation> void innerTest_getAnnotationsByType(Class clazz, Class<A> annoClazz, boolean isDeclared, int ... expected) {
	Annotation[] ann = (isDeclared) ? clazz.getDeclaredAnnotationsByType(annoClazz) : clazz.getAnnotationsByType(annoClazz);
	AssertJUnit.assertTrue( clazz.getSimpleName() + ", Incorrect length of annotation list. Expected: " + expected.length + " Actual: " + ann.length, ann.length == expected.length);
	for (int i = 0; i < expected.length; i++) {
		AssertJUnit.assertTrue( clazz.getSimpleName() + ", Incorrect value in annotation list. Position: " + i + ", Expected value: " + AnnoExpectedValues[expected[i]] + ", Actual Value: " + anno2String(ann[i]), anno2String(ann[i]).equals(AnnoExpectedValues[expected[i]]));
	}
}

@Test
public void test_getAnnotationsByType() {

	innerTest_getAnnotationNPE(AnnoBlank.class, false, true);

	innerTest_getAnnotationsByType(AnnoBlank.class, RepeatableAnn.class, false);

	innerTest_getAnnotationsByType(AnnoNested.class, RepeatableAnn.class, false);
	innerTest_getAnnotationsByType(AnnoNested.class, ContainerAnn.class, false, 30, 31);
	innerTest_getAnnotationsByType(AnnoNested.class, ContainerContainerAnn.class, false, 1);

	innerTest_getAnnotationsByType(AnnoGrandParent.class, RepeatableAnn.class, false, 32, 33, 34);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, ContainerAnn.class, false, 2);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, ContainerContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn2.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn3.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn4.class, false, 3);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn5.class, false, 4);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn6.class, false, 5);

	innerTest_getAnnotationsByType(AnnoParent.class, RepeatableAnn.class, false, 35, 36, 37, 38, 39);
	innerTest_getAnnotationsByType(AnnoParent.class, ContainerAnn.class, false, 6);
	innerTest_getAnnotationsByType(AnnoParent.class, ContainerContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn.class, false, 8);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn2.class, false, 7);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn3.class, false);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn4.class, false, 3);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn5.class, false, 4);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn6.class, false, 5);

	innerTest_getAnnotationsByType(AnnoChild.class, RepeatableAnn.class, false, 40, 41, 42, 43);
	innerTest_getAnnotationsByType(AnnoChild.class, ContainerAnn.class, false, 9);
	innerTest_getAnnotationsByType(AnnoChild.class, ContainerContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn.class, false, 8);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn2.class, false, 7);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn3.class, false, 10);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn4.class, false, 3);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn5.class, false, 4);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn6.class, false, 5);

	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RepeatableAnn.class, false, 13);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, ContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, ContainerContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn2.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn3.class, false, 15);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn4.class, false, 11);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn5.class, false, 12);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn6.class, false, 14);

	innerTest_getAnnotationsByType(AnnoParentNR.class, RepeatableAnn.class, false, 16);
	innerTest_getAnnotationsByType(AnnoParentNR.class, ContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoParentNR.class, ContainerContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn.class, false, 18);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn2.class, false, 17);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn3.class, false, 15);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn4.class, false, 11);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn5.class, false, 12);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn6.class, false, 14);

	innerTest_getAnnotationsByType(AnnoChildNR.class, RepeatableAnn.class, false, 20);
	innerTest_getAnnotationsByType(AnnoChildNR.class, ContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoChildNR.class, ContainerContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn.class, false, 18);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn2.class, false, 17);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn3.class, false, 19);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn4.class, false, 11);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn5.class, false, 12);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn6.class, false, 14);

	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RepeatableAnn.class, false, 44, 45);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, ContainerAnn.class, false, 22);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, ContainerContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn.class, false, 21);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn2.class, false, 23);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn3.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn4.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn5.class, false);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn6.class, false);

	innerTest_getAnnotationsByType(AnnoParentR.class, RepeatableAnn.class, false, 24);
	innerTest_getAnnotationsByType(AnnoParentR.class, ContainerAnn.class, false, 22);
	innerTest_getAnnotationsByType(AnnoParentR.class, ContainerContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn.class, false, 21);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn2.class, false, 23);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn3.class, false, 25);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn4.class, false, 26);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn5.class, false);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn6.class, false);

	innerTest_getAnnotationsByType(AnnoChildR.class, RepeatableAnn.class, false, 29);
	innerTest_getAnnotationsByType(AnnoChildR.class, ContainerAnn.class, false, 22);
	innerTest_getAnnotationsByType(AnnoChildR.class, ContainerContainerAnn.class, false);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn.class, false, 27);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn2.class, false, 23);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn3.class, false, 25);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn4.class, false, 26);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn5.class, false, 28);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn6.class, false);

}

@Test
public void test_getDeclaredAnnotationsByType() {

	innerTest_getAnnotationNPE(AnnoBlank.class, true, true);

	innerTest_getAnnotationsByType(AnnoBlank.class, RepeatableAnn.class, true);

	innerTest_getAnnotationsByType(AnnoNested.class, RepeatableAnn.class, true);
	innerTest_getAnnotationsByType(AnnoNested.class, ContainerAnn.class, true, 30, 31);
	innerTest_getAnnotationsByType(AnnoNested.class, ContainerContainerAnn.class, true, 1);

	innerTest_getAnnotationsByType(AnnoGrandParent.class, RepeatableAnn.class, true, 32, 33, 34);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, ContainerAnn.class, true, 2);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, ContainerContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn2.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn3.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn4.class, true, 3);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn5.class, true, 4);
	innerTest_getAnnotationsByType(AnnoGrandParent.class, RetroAnn6.class, true, 5);

	innerTest_getAnnotationsByType(AnnoParent.class, RepeatableAnn.class, true, 35, 36, 37, 38, 39);
	innerTest_getAnnotationsByType(AnnoParent.class, ContainerAnn.class, true, 6);
	innerTest_getAnnotationsByType(AnnoParent.class, ContainerContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn.class, true, 8);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn2.class, true, 7);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn3.class, true);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn4.class, true);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn5.class, true);
	innerTest_getAnnotationsByType(AnnoParent.class, RetroAnn6.class, true);

	innerTest_getAnnotationsByType(AnnoChild.class, RepeatableAnn.class, true, 40, 41, 42, 43);
	innerTest_getAnnotationsByType(AnnoChild.class, ContainerAnn.class, true, 9);
	innerTest_getAnnotationsByType(AnnoChild.class, ContainerContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn.class, true);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn2.class, true);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn3.class, true, 10);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn4.class, true);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn5.class, true);
	innerTest_getAnnotationsByType(AnnoChild.class, RetroAnn6.class, true);

	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RepeatableAnn.class, true, 13);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, ContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, ContainerContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn2.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn3.class, true, 15);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn4.class, true, 11);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn5.class, true, 12);
	innerTest_getAnnotationsByType(AnnoGrandParentNR.class, RetroAnn6.class, true, 14);

	innerTest_getAnnotationsByType(AnnoParentNR.class, RepeatableAnn.class, true, 16);
	innerTest_getAnnotationsByType(AnnoParentNR.class, ContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoParentNR.class, ContainerContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn.class, true, 18);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn2.class, true, 17);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn3.class, true);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn4.class, true);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn5.class, true);
	innerTest_getAnnotationsByType(AnnoParentNR.class, RetroAnn6.class, true);

	innerTest_getAnnotationsByType(AnnoChildNR.class, RepeatableAnn.class, true, 20);
	innerTest_getAnnotationsByType(AnnoChildNR.class, ContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoChildNR.class, ContainerContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn.class, true);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn2.class, true);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn3.class, true, 19);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn4.class, true);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn5.class, true);
	innerTest_getAnnotationsByType(AnnoChildNR.class, RetroAnn6.class, true);

	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RepeatableAnn.class, true, 44, 45);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, ContainerAnn.class, true, 22);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, ContainerContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn.class, true, 21);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn2.class, true, 23);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn3.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn4.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn5.class, true);
	innerTest_getAnnotationsByType(AnnoGrandParentR.class, RetroAnn6.class, true);

	innerTest_getAnnotationsByType(AnnoParentR.class, RepeatableAnn.class, true, 24);
	innerTest_getAnnotationsByType(AnnoParentR.class, ContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoParentR.class, ContainerContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn.class, true);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn2.class, true);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn3.class, true, 25);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn4.class, true, 26);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn5.class, true);
	innerTest_getAnnotationsByType(AnnoParentR.class, RetroAnn6.class, true);

	innerTest_getAnnotationsByType(AnnoChildR.class, RepeatableAnn.class, true, 29);
	innerTest_getAnnotationsByType(AnnoChildR.class, ContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoChildR.class, ContainerContainerAnn.class, true);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn.class, true, 27);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn2.class, true);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn3.class, true);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn4.class, true);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn5.class, true, 28);
	innerTest_getAnnotationsByType(AnnoChildR.class, RetroAnn6.class, true);

}

/**
 * @tests java.lang.Class#getEnumConstants()
 */
enum first_enum{enum1_value1, enum1_value2}
enum second_enum{enum2_value1, enum2_value2}
@Test
public void test_getEnumConstants2(){
	first_enum[] enum1Constants = first_enum.class.getEnumConstants();
	AssertJUnit.assertTrue("Failed to get enum constants for the class : " + first_enum.class.getName(), enum1Constants != null);
	AssertJUnit.assertTrue("Incorrect number of enum constants are returned", enum1Constants.length == 2);
	AssertJUnit.assertTrue("Expected first value is \"enum1_value1\", got \"" + enum1Constants[0] + "\"", enum1Constants[0].toString().equals("enum1_value1"));
	AssertJUnit.assertTrue("Expected second value is \"enum1_value2\", got \"" + enum1Constants[1] + "\"", enum1Constants[1].toString().equals("enum1_value2"));

	/*Change a value in the returned results array before we call getEnumConstants second time to check the values*/
	enum1Constants[0] = null;

	second_enum[] enum2Constants = second_enum.class.getEnumConstants();
	AssertJUnit.assertTrue("Failed to get enum constants for the class : " + second_enum.class.getName(), enum2Constants != null);
	AssertJUnit.assertTrue("Incorrect number of enum constants are returned", enum2Constants.length == 2);
	AssertJUnit.assertTrue("Expected first value is \"enum2_value1\", got \"" + enum2Constants[0] + "\"", enum2Constants[0].toString().equals("enum2_value1"));
	AssertJUnit.assertTrue("Expected second value is \"enum2_value2\", got \"" + enum2Constants[1] + "\"", enum2Constants[1].toString().equals("enum2_value2"));

	/*Check whether it return the same correct values on the second call for getEnumConstants*/
	first_enum[] enum1_2_Constants = first_enum.class.getEnumConstants();
	AssertJUnit.assertTrue("Failed to get enum constants for the class : " + first_enum.class.getName(), enum1_2_Constants != null);
	AssertJUnit.assertTrue("Incorrect number of enum constants are returned", enum1_2_Constants.length == 2);
	AssertJUnit.assertTrue("Expected first value is \"enum1_value1\", got \"" + enum1_2_Constants[0] + "\"", enum1_2_Constants[0].toString().equals("enum1_value1"));
	AssertJUnit.assertTrue("Expected second value is \"enum1_value2\", got \"" + enum1_2_Constants[1] + "\"", enum1_2_Constants[1].toString().equals("enum1_value2"));
	/*Check whether each call to getEnumConstants() returns non-identical array results. Each call should return new array object.*/
	AssertJUnit.assertFalse("Returned the same array of results for two calls of getEnumConstants", enum1_2_Constants == enum1Constants);
	/*Modify one array of results after we call getEnumConstants second time and ensure the subsequent array of results is not affected*/
	enum1Constants[1] = null;
	AssertJUnit.assertFalse("Modifying one return value (array of enum) of getEnumConstants call affected the other return value of getEnumConstants call", enum1_2_Constants[1] == null);
	AssertJUnit.assertFalse("Modifying one return value (array of enum) of getEnumConstants call affected the other return value of getEnumConstants call", enum1_2_Constants[0] == null);
	AssertJUnit.assertTrue("Modifying one return value (array of enum) of getEnumConstants call affected the other return value of getEnumConstants call", enum1_2_Constants[1].toString().equals("enum1_value2") );
}

/**
 * @tests java.lang.Class#enumConstantDirectory()
 * @tests java.lang.Enum#valueOf(java.lang.Class, java.lang.String)
 */
@Test
public void test_valueOf(){
	/*
	 *Since enum values are cached in enumConstantsDirectory,
	 *valueOf should return the same object each time it is called with the same parameters.
	 */
	AssertJUnit.assertTrue("Enum.valueOf returned different object. It should return what is cached in enumConstantsDirectory", Enum.valueOf(first_enum.class, "enum1_value1") == Enum.valueOf(first_enum.class, "enum1_value1"));
	AssertJUnit.assertTrue("Enum.valueOf returned different object. It should return what is cached in enumConstantsDirectory", Enum.valueOf(first_enum.class, "enum1_value2") == Enum.valueOf(first_enum.class, "enum1_value2"));
	AssertJUnit.assertTrue("Enum.valueOf returned different object. It should return what is cached in enumConstantsDirectory", Enum.valueOf(second_enum.class, "enum2_value1") == Enum.valueOf(second_enum.class, "enum2_value1"));
	AssertJUnit.assertTrue("Enum.valueOf returned different object. It should return what is cached in enumConstantsDirectory", Enum.valueOf(second_enum.class, "enum2_value2") == Enum.valueOf(second_enum.class, "enum2_value2"));

	/*Using a non-existing enum constant name in valueOf should return IllegalArgumentException*/
	try{
		Enum.valueOf(second_enum.class, "enum1_value1");
		Assert.fail("IllegalArgumentException is expected when there is no enum constant with the specified name");
	} catch (IllegalArgumentException e) {
		/*Expected*/
	}

	/*IllegalArgumentException is expected when specified object does not represent enum type*/
	try{
		Class c = Object.class;
		Enum.valueOf(c, "enum1_value1");
		Assert.fail("IllegalArgumentException is expected when specified class object does not represent an enum type");
	} catch (IllegalArgumentException e) {
		/*Expected*/
	}

	/*NullPointerException if any of the parameters for valueOf is null*/
	try{
		Class c = null;
		Enum.valueOf(c, "enum1_value1");
		Assert.fail("NullPointerException is expected when specified class type is null");
	} catch (NullPointerException e) {
		/*Expected*/
	}

	try{
		String str = null;
		Enum.valueOf(first_enum.class, str);
		Assert.fail("NullPointerException is expected when senum constant name is null");
	} catch (NullPointerException e) {
		/*Expected*/
	}
}

/**
 * @tests java.lang.Class#getEnumConstantsShared()
 */
@Test
public void test_getEnumConstantsShared(){
	first_enum[] enum1SharedConstants;
	second_enum[] enum2SharedConstants;
	first_enum[] enum1SharedConstants_2;
	second_enum[] enum2SharedConstants_2;

	InternalAccessor accessor = new InternalAccessor();
	enum1SharedConstants = accessor.getEnumConstantsShared(first_enum.class);
	enum2SharedConstants = accessor.getEnumConstantsShared(second_enum.class);
	enum1SharedConstants_2 = accessor.getEnumConstantsShared(first_enum.class);
	enum2SharedConstants_2 = accessor.getEnumConstantsShared(second_enum.class);

	/* Check the values of the first_enum */
	AssertJUnit.assertTrue("Failed to get enum constants for the class : " + first_enum.class.getName(), enum1SharedConstants != null);
	AssertJUnit.assertTrue("Incorrect number of enum constants are returned", enum1SharedConstants.length == 2);
	AssertJUnit.assertTrue("Expected first value is \"enum1_value1\", got \"" + enum1SharedConstants[0] + "\"", enum1SharedConstants[0].toString().equals("enum1_value1"));
	AssertJUnit.assertTrue("Expected second value is \"enum1_value2\", got \"" + enum1SharedConstants[1] + "\"", enum1SharedConstants[1].toString().equals("enum1_value2"));

	/* Check the values of the second_enum */
	AssertJUnit.assertTrue("Failed to get enum constants for the class : " + second_enum.class.getName(), enum2SharedConstants != null);
	AssertJUnit.assertTrue("Incorrect number of enum constants are returned", enum2SharedConstants.length == 2);
	AssertJUnit.assertTrue("Expected first value is \"enum2_value1\", got \"" + enum2SharedConstants[0] + "\"", enum2SharedConstants[0].toString().equals("enum2_value1"));
	AssertJUnit.assertTrue("Expected second value is \"enum2_value2\", got \"" + enum2SharedConstants[1] + "\"", enum2SharedConstants[1].toString().equals("enum2_value2"));

	/*Check whether calls to getEnumConstantsShared returns the same object  */
	AssertJUnit.assertTrue("Subsequent calls to getEnumConstantsShared returns different object for the class : " + first_enum.class.getName(), enum1SharedConstants == enum1SharedConstants_2);
	AssertJUnit.assertTrue("Subsequent calls to getEnumConstantsShared returns different object for the class : " + second_enum.class.getName(), enum2SharedConstants == enum2SharedConstants_2);
}

void testGetClassLoaderImpl(Class<?> clz) throws Throwable {
	Method m = clz.getClass().getDeclaredMethod("getClassLoaderImpl", new Class<?>[0]);
	m.setAccessible(true);
	ClassLoader cl = (ClassLoader)m.invoke(clz, new Object[0]);
	AssertJUnit.assertTrue("getClassLoaderImpl should return a non-null ClassLoader!", cl != null);
}
/**
 * @tests java.lang.Class#getClassLoader()
 */
@Test
public void test_getClassLoader() throws Throwable {
	testGetClassLoaderImpl(java.lang.ArrayStoreException.class);
	testGetClassLoaderImpl(java.lang.ClassCastException.class);
	testGetClassLoaderImpl(java.lang.IllegalMonitorStateException.class);
	testGetClassLoaderImpl(java.lang.IndexOutOfBoundsException.class);
	testGetClassLoaderImpl(java.lang.NegativeArraySizeException.class);
	testGetClassLoaderImpl(java.lang.NullPointerException.class);
	testGetClassLoaderImpl(java.lang.VirtualMachineError.class);
	testGetClassLoaderImpl(java.lang.InstantiationError.class);
	testGetClassLoaderImpl(java.lang.UnsatisfiedLinkError.class);
	testGetClassLoaderImpl(java.lang.InternalError.class);
	testGetClassLoaderImpl(java.lang.OutOfMemoryError.class);
	testGetClassLoaderImpl(java.lang.StackOverflowError.class);
	testGetClassLoaderImpl(java.lang.UnsupportedClassVersionError.class);
	testGetClassLoaderImpl(java.lang.AbstractMethodError.class);
	testGetClassLoaderImpl(java.lang.IncompatibleClassChangeError.class);
	testGetClassLoaderImpl(java.lang.IllegalAccessError.class);
	testGetClassLoaderImpl(java.lang.NoSuchMethodError.class);
	testGetClassLoaderImpl(java.lang.CloneNotSupportedException.class);
	testGetClassLoaderImpl(java.lang.InstantiationException.class);
	testGetClassLoaderImpl(java.lang.NoSuchFieldError.class);
	testGetClassLoaderImpl(java.lang.NoClassDefFoundError.class);
	testGetClassLoaderImpl(java.lang.ArrayIndexOutOfBoundsException.class);
	testGetClassLoaderImpl(java.lang.Object.class);
	testGetClassLoaderImpl(java.lang.String.class);
	testGetClassLoaderImpl(java.lang.Class.class);
	testGetClassLoaderImpl(java.lang.IllegalThreadStateException.class);
	testGetClassLoaderImpl(java.lang.Thread.class);
	testGetClassLoaderImpl(java.lang.ArithmeticException.class);
	testGetClassLoaderImpl(java.lang.ThreadGroup.class);
	testGetClassLoaderImpl(java.lang.InterruptedException.class);
	testGetClassLoaderImpl(java.lang.ClassNotFoundException.class);
	testGetClassLoaderImpl(java.lang.Throwable.class);
	testGetClassLoaderImpl(java.lang.StringIndexOutOfBoundsException.class);
	testGetClassLoaderImpl(java.lang.ThreadDeath.class);
	testGetClassLoaderImpl(java.lang.IllegalAccessException.class);
	testGetClassLoaderImpl(java.lang.IllegalArgumentException.class);
	testGetClassLoaderImpl(java.lang.reflect.Method.class);
	testGetClassLoaderImpl(java.lang.reflect.Field.class);
	testGetClassLoaderImpl(java.lang.LinkageError.class);
	testGetClassLoaderImpl(java.lang.Byte.class);
	testGetClassLoaderImpl(java.lang.Short.class);
	testGetClassLoaderImpl(java.lang.Integer.class);
	testGetClassLoaderImpl(java.lang.Long.class);
	testGetClassLoaderImpl(java.lang.Float.class);
	testGetClassLoaderImpl(java.lang.Double.class);
	testGetClassLoaderImpl(java.lang.Boolean.class);
	testGetClassLoaderImpl(java.lang.Character.class);
	testGetClassLoaderImpl(java.lang.Void.class);
	testGetClassLoaderImpl(java.lang.ClassLoader.class);
	testGetClassLoaderImpl(java.lang.NoSuchFieldException.class);
	testGetClassLoaderImpl(java.lang.NoSuchMethodException.class);
	testGetClassLoaderImpl(java.lang.reflect.Constructor.class);
	testGetClassLoaderImpl(java.lang.ClassCircularityError.class);
	testGetClassLoaderImpl(java.lang.VerifyError.class);
	testGetClassLoaderImpl(java.lang.ClassFormatError.class);
	testGetClassLoaderImpl(java.lang.StackTraceElement.class);
	testGetClassLoaderImpl(java.lang.ExceptionInInitializerError.class);
	testGetClassLoaderImpl(java.lang.reflect.InvocationTargetException.class);
	testGetClassLoaderImpl(java.lang.ref.Reference.class);
	testGetClassLoaderImpl(java.lang.ref.SoftReference.class);
	testGetClassLoaderImpl(java.lang.invoke.MethodHandle.class);
	testGetClassLoaderImpl(java.lang.invoke.VolatileCallSite.class);
	testGetClassLoaderImpl(java.lang.invoke.MutableCallSite.class);
	testGetClassLoaderImpl(java.lang.invoke.MethodType.class);
	testGetClassLoaderImpl(java.lang.invoke.WrongMethodTypeException.class);
	testGetClassLoaderImpl(java.lang.SecurityException.class);
	testGetClassLoaderImpl(java.lang.System.class);
	testGetClassLoaderImpl(java.lang.Object[].class);
	testGetClassLoaderImpl(int[].class);
	testGetClassLoaderImpl(java.lang.Integer.TYPE);
}

/**
 * @tests {@link java.lang.Class#cast(Object)}
 */
@Test
public void test_cast(){
	Object.class.cast(null);
	int.class.cast(null);
	Cloneable.class.cast(new int[0]);
	try {
		String.class.cast(new Object());
		Assert.fail("Should have thrown ClassCastException");
	} catch (ClassCastException e) {
	}
	try {
		int[].class.cast(new Object());
		Assert.fail("Should have thrown ClassCastException");
	} catch (ClassCastException e) {
	}
	try {
		Comparator.class.cast("hello");
		Assert.fail("Should have thrown ClassCastException");
	} catch (ClassCastException e) {
	}
}

/**
 * Sets up the fixture, for example, open a network connection.
 * This method is called before a test is executed.
 */
@BeforeMethod
protected void setUp() {

}

/**
 * Tears down the fixture, for example, close a network connection.
 * This method is called after a test is executed.
 */
@AfterMethod
protected void tearDown() {
}

protected void doneSuite(){
}

private Class[] getTestClasses() {
	class Local$Class {
		class Member6$E {
		}
	}
	Class anonymousClass = new Cloneable() {}.getClass();
	Class[] classes = new Class[] {
			getClass(), 									// 0
			StaticMember$Class.class,						// 1
			Member$Class.class,								// 2
			Local$Class.class,								// 3
			anonymousClass,									// 4
			Integer.TYPE,									// 5
			Local$Class.Member6$E.class,					// 6
			StaticMember$Class.Member2$A.class,				// 7
			Member$Class.Member3$B.class,					// 8
	};
	return classes;
}

static class TestClasses {
	private Class[] constructorClasses;
	TestClasses() {
		class Local$Class {
			class Member7$F {
			}
		}
		Class anonymousClass = new Cloneable() {}.getClass();
		constructorClasses = new Class[] {
				getClass(), 									// 0
				StaticMember$Class.class,						// 1
				Member$Class.class,								// 2
				Local$Class.class,								// 3
				anonymousClass,									// 4
				Integer.TYPE,									// 5
				Local$Class.Member7$F.class,					// 6
				StaticMember$Class.Member2$A.class,				// 7
				Member$Class.Member3$B.class,					// 8
		};
	}
	Class[] getConstructorClasses() {
		return constructorClasses;
	}
}

/* Helper classes for test_getModifiers_classTypes */
// Access modifiers
class DefaultClazz {}
protected class ProtectedClazz {}
private class PrivateClazz {}

// Combinations of public, final and static
public class PublicClazz {}
final class FinalClazz {}
static class StaticClazz {}
public final class PublicFinalClazz {}
public static class PublicStaticClazz {}
final static class FinalStaticClazz {}
public final static class PublicFinalStaticClazz {}
static Object staticInnerAnonymousObject = new Object() {};
static Class<?> staticInnerAnonymousClazz = staticInnerAnonymousObject.getClass();
private static final String specimenPackage = "org.openj9.test.java.lang.specimens";
private static final String objectClass = "java.lang.Object";

// Other modifiers
interface InterfaceClazz {}
abstract class AbstractClazz {}
@interface AnnotationClazz {}
enum EnumClazz {}

static Class<?> innerClassFromMethod() {
    class InMethodClazz {}
    return InMethodClazz.class;
}

static class Helper {
	private static final String CLASS_SYNTHETIC = "SyntheticClass";
	private static final String OUTER_CLASS = "OuterClass";
	private static final String INNER_CLASS_DIFFERENT_MODIFIERS = "InnerClassDifferentModifers";

	static ClassLoader cl;
	static Class<?> syntheticClass;
	private static Class<?> outerClass;
	static Class<?> innerClassDifferentModifiers;

	static {
		cl = new ClassLoader() {
            public Class<?> findClass(String name) throws ClassNotFoundException {
                if (CLASS_SYNTHETIC.equals(name)) {
                    byte[] classFile = getClassSynthetic();
                    return defineClass(CLASS_SYNTHETIC, classFile, 0, classFile.length);
                } else if (INNER_CLASS_DIFFERENT_MODIFIERS.equals(name)) {
                	byte[] classFile = getInnerClassDifferentModifiers();
                    return defineClass(OUTER_CLASS + "$" + INNER_CLASS_DIFFERENT_MODIFIERS, classFile, 0, classFile.length);
                } else if (OUTER_CLASS.equals(name)) {
                	byte[] classFile = getOuterClass();
                    return defineClass(OUTER_CLASS, classFile, 0, classFile.length);
                }
                throw new ClassNotFoundException(name);
            }

            /*
             * interface A {
    		 *     void f();
    		 * }
             */
            private byte[] getClassSynthetic() {
            	ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
                MethodVisitor mv;

                cw.visit(49, ACC_PUBLIC | ACC_SUPER | ACC_SYNTHETIC, CLASS_SYNTHETIC, null, "java/lang/Object", null);
                {
                    mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
                    mv.visitCode();
                    mv.visitVarInsn(ALOAD, 0);
                    mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
                    mv.visitInsn(RETURN);
                    mv.visitMaxs(0, 0);
                    mv.visitEnd();
                }
                return cw.toByteArray();
            }

            /*
             * Returns an inner class of SyntheticClass. The accessFlags have the public flag, but the memberAccessFlags don't.
             */
            private byte[] getInnerClassDifferentModifiers() {
            	ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
                MethodVisitor mv;

                cw.visit(49, ACC_PUBLIC | ACC_SUPER, OUTER_CLASS + "$" + INNER_CLASS_DIFFERENT_MODIFIERS, null, "java/lang/Object", null);

                cw.visitInnerClass(OUTER_CLASS + "$" + INNER_CLASS_DIFFERENT_MODIFIERS, OUTER_CLASS, INNER_CLASS_DIFFERENT_MODIFIERS, ACC_STATIC);

                {
                    mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
                    mv.visitCode();
                    mv.visitVarInsn(ALOAD, 0);
                    mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
                    mv.visitInsn(RETURN);
                    mv.visitMaxs(0, 0);
                    mv.visitEnd();
                }
                return cw.toByteArray();
            }

            /*
             * Returns an outer class for InnerClassDifferentModifers.
             */
            private byte[] getOuterClass() {
            	ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
                MethodVisitor mv;

                cw.visit(49, ACC_PUBLIC | ACC_SUPER, OUTER_CLASS, null, "java/lang/Object", null);

                cw.visitInnerClass(OUTER_CLASS + "$" + INNER_CLASS_DIFFERENT_MODIFIERS, OUTER_CLASS, INNER_CLASS_DIFFERENT_MODIFIERS, ACC_PUBLIC | ACC_STATIC);

                {
                    mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
                    mv.visitCode();
                    mv.visitVarInsn(ALOAD, 0);
                    mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
                    mv.visitInsn(RETURN);
                    mv.visitMaxs(0, 0);
                    mv.visitEnd();
                }
                return cw.toByteArray();
            }
        };
		try {
			syntheticClass = cl.loadClass(CLASS_SYNTHETIC);
			outerClass = cl.loadClass(OUTER_CLASS);
			innerClassDifferentModifiers = cl.loadClass(INNER_CLASS_DIFFERENT_MODIFIERS);
		} catch (ClassNotFoundException e) {
			System.exit(-1);
		}
	}
}
}
