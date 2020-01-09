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
package org.openj9.test.reflect;

import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.openj9.test.reflect.defendersupersends.asm.AsmLoader;
import org.openj9.test.reflect.defendersupersends.asm.AsmTestcaseGenerator;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.lang.reflect.Method;
import java.lang.reflect.Parameter;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import org.openj9.resources.reflect.B;

public class GetMethodsTests {
	/* contrived method names which have the same String.hashCode(). */
	private static final String METHOD_NAME21 = "methodName21"; //$NON-NLS-1$
	private static final String METHOD_NAME1P = "methodName1P"; //$NON-NLS-1$	
	private static final String EXAMPLE_CLASS = "org.openj9.test.reflect.ExampleClass."; //$NON-NLS-1$
	
	public static final Logger logger = Logger.getLogger(GetMethodsTests.class);
	private final static boolean isJava8 = System.getProperty("java.specification.version").equals("1.8");  //$NON-NLS-1$//$NON-NLS-2$

	private AsmLoader loader;

	@BeforeClass(groups = { "level.sanity", "j9vm_SE80" })
	public void setupTestCases() {
		loader = new AsmLoader(GetMethodsTests.class.getClassLoader());
		AsmTestcaseGenerator generator = new AsmTestcaseGenerator(loader, new String[] { "m", "w" });
		generator.getDefenderSupersendTestcase();
	}

	@Test(groups = { "level.sanity", "j9vm_SE80" })
	public void testGetMethods() {
		try {
			HashMap<String, String[]> methodLists = makeMethodLists();
			for (String className : methodLists.keySet()) {
				@SuppressWarnings("rawtypes") Class testClass = loader.loadClass(className);
				logger.debug("Checking methods in " + className);
				String actualMethodNames[] = getSortedMethodList(testClass);
				List<String> expectedNames = Arrays.asList(methodLists.get(className));
				Iterator<String> expectedNameIterator = expectedNames.iterator();
				logger.debug(concatenateStrings("expected methods:", expectedNames)); //$NON-NLS-1$
				logger.debug(concatenateStrings("actual methods:", Arrays.asList(actualMethodNames))); //$NON-NLS-1$
				for (String actualname : actualMethodNames) {
					AssertJUnit.assertTrue("too many methods for " + className, expectedNameIterator.hasNext()); //$NON-NLS-1$
					String expectedName = expectedNameIterator.next();
					AssertJUnit.assertEquals("wrong methods list for " + className, expectedName, actualname); //$NON-NLS-1$
				}
				AssertJUnit.assertFalse("too few methods for " + className, expectedNameIterator.hasNext()); //$NON-NLS-1$
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("unexpected exception: " + e);
		}

	}

	private static String concatenateStrings(String message, Iterable<String> stringList) {
		StringBuilder nameBuffer = new StringBuilder(message);
		for (String s: stringList) {
			nameBuffer.append("\n\t"); //$NON-NLS-1$
			nameBuffer.append(s);
		}
		return nameBuffer.toString();
	}

	public static String[] getSortedMethodList(@SuppressWarnings("rawtypes") Class testClass) {
		Method[] methodList = testClass.getMethods();
		String methodStrings[] = new String[methodList.length];
		for (int i = 0; i < methodList.length; ++i) {
			methodStrings[i] = methToString(methodList[i]);
		}
		Arrays.sort(methodStrings);
		return methodStrings;
	}

	private static String methToString(Method m) {
		StringBuffer methName = new StringBuffer(m.getDeclaringClass().getName());
		methName.append('.');
		methName.append(m.getName());
		methName.append('(');
		String separator = "";
		for (Parameter p : m.getParameters()) {
			methName.append(separator);
			methName.append(p.getType().getName());
			separator = ", ";
		}
		methName.append(')');
		methName.append(m.getReturnType().getName());
		return methName.toString();
	}

	static HashMap<String, String[]> makeMethodLists() {
		/* 
		 * Class.getMethods() erroneously included methods in superinterfaces which are
		 * overridden in subinterfaces.  This has been corrected in Java 9.
		 */
		String[] methodList_C_I_SupDuperSupA = isJava8? new String[] {
				"org.openj9.test.reflect.SuperA.abstractInSuperA_abstractInSuperDuper()void",
				"org.openj9.test.reflect.SuperA.abstractInSuperA_defaultInSuperDuper()void",
				"org.openj9.test.reflect.SuperA.defaultInSuperA_abstractInSuperDuper()void",
				"org.openj9.test.reflect.SuperA.defaultInSuperA_defaultInSuperDuper()void",
				/* 
				 * include these because of a known issue in Java 8:
				 * JDK-8029459 (reflect) getMethods returns methods that are not members of the class
				 */
				"org.openj9.test.reflect.SuperDuper.abstractInSuperA_abstractInSuperDuper()void",
				"org.openj9.test.reflect.SuperDuper.abstractInSuperA_defaultInSuperDuper()void" 
				}:
			new String[] {
					"org.openj9.test.reflect.SuperA.abstractInSuperA_abstractInSuperDuper()void",
					"org.openj9.test.reflect.SuperA.abstractInSuperA_defaultInSuperDuper()void",
					"org.openj9.test.reflect.SuperA.defaultInSuperA_abstractInSuperDuper()void",
					"org.openj9.test.reflect.SuperA.defaultInSuperA_defaultInSuperDuper()void"};
		HashMap<String, String[]> methodLists = new HashMap<>();
		
		methodLists.put("org.openj9.test.reflect.C_CSuperA_SuperDuper", concatenateObjectMethods(methodList_C_I_SupDuperSupA));
		methodLists.put("org.openj9.test.reflect.C_I_SupDuperSupA", concatenateObjectMethods(methodList_C_I_SupDuperSupA));
		methodLists.put("org.openj9.test.reflect.C_SuperA", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.SuperA.abstractInSuperA_abstractInSuperDuper()void",
				"org.openj9.test.reflect.SuperA.abstractInSuperA_defaultInSuperDuper()void",
				"org.openj9.test.reflect.SuperA.defaultInSuperA_abstractInSuperDuper()void",
				"org.openj9.test.reflect.SuperA.defaultInSuperA_defaultInSuperDuper()void" }));
		methodLists.put("org.openj9.test.reflect.I_SupDuper_SupA", methodList_C_I_SupDuperSupA);
		methodLists.put("org.openj9.test.reflect.SuperA", new String[] {
				"org.openj9.test.reflect.SuperA.abstractInSuperA_abstractInSuperDuper()void",
				"org.openj9.test.reflect.SuperA.abstractInSuperA_defaultInSuperDuper()void",
				"org.openj9.test.reflect.SuperA.defaultInSuperA_abstractInSuperDuper()void",
				"org.openj9.test.reflect.SuperA.defaultInSuperA_defaultInSuperDuper()void" });
		methodLists.put("org.openj9.test.reflect.SuperDuper", new String[] {
				"org.openj9.test.reflect.SuperDuper.abstractInSuperA_abstractInSuperDuper()void",
				"org.openj9.test.reflect.SuperDuper.abstractInSuperA_defaultInSuperDuper()void",
				"org.openj9.test.reflect.SuperDuper.defaultInSuperA_abstractInSuperDuper()void",
				"org.openj9.test.reflect.SuperDuper.defaultInSuperA_defaultInSuperDuper()void" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.A", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.A.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.A.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.A.w()char" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.B", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.B.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.B.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.B.w()char" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.C", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.C.lookup()java.lang.invoke.MethodHandles$Lookup" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.D", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.D.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.D.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.D.w()char" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.E", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.A.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.A.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.E.lookup()java.lang.invoke.MethodHandles$Lookup" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.F", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.F.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.F.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.F.w()char" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.G", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.G.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.G.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.G.w()char" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.H", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.H.lookup()java.lang.invoke.MethodHandles$Lookup" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.K", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.A.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.A.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.B.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.B.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.K.lookup()java.lang.invoke.MethodHandles$Lookup" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.L", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.L.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.L.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.L.w()char" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.M", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.B.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.B.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.M.lookup()java.lang.invoke.MethodHandles$Lookup" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.N", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.B.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.B.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.N.lookup()java.lang.invoke.MethodHandles$Lookup" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.O", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.O.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.O.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.O.w()char" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.O1", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.A.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.A.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.O1.lookup()java.lang.invoke.MethodHandles$Lookup" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.O2", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.F.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.F.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.O2.lookup()java.lang.invoke.MethodHandles$Lookup" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.O3", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.G.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.G.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.O3.lookup()java.lang.invoke.MethodHandles$Lookup" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.O4", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.O4.lookup()java.lang.invoke.MethodHandles$Lookup" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.P", new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.P.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.P.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.P.w()char" });
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.S", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.P.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.P.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.S.lookup()java.lang.invoke.MethodHandles$Lookup" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.T_K", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.T_K.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.T_K.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.T_K.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.T_L", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.T_L.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.T_L.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.T_L.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.U", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.U.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.U.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.U.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.V_K", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.V_K.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.V_K.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.V_K.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.V_L", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.V_L.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.V_L.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.V_L.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.V_M", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.V_M.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.V_M.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.V_M.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.W_L", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.W_L.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.W_L.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.W_L.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.W_M", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.W_M.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.W_M.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.W_M.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.W_O", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.W_O.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.W_O.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.W_O.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.X_L", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.X_L.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.X_L.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.X_L.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.X_M", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.X_M.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.X_M.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.X_M.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.X_N", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.X_N.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.X_N.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.X_N.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.Y_M", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.Y_M.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.Y_M.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.Y_M.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.Y_N", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.Y_N.lookup()java.lang.invoke.MethodHandles$Lookup",
				"org.openj9.test.reflect.defendersupersends.asm.Y_N.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.Y_N.w()char" }));
		methodLists.put("org.openj9.test.reflect.defendersupersends.asm.Z", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.defendersupersends.asm.D.m()char",
				"org.openj9.test.reflect.defendersupersends.asm.D.w()char",
				"org.openj9.test.reflect.defendersupersends.asm.Z.lookup()java.lang.invoke.MethodHandles$Lookup" }));
		methodLists.put("org.openj9.test.reflect.CImplements_I_J", concatenateObjectMethods(new String[] {
				"org.openj9.test.reflect.I.m()void",
				"org.openj9.test.reflect.J.m()void" }));
		methodLists.put("org.openj9.test.reflect.I", new String[] { "org.openj9.test.reflect.I.m()void" });
		methodLists.put("org.openj9.test.reflect.J", new String[] { "org.openj9.test.reflect.J.m()void" });
		methodLists.put("org.openj9.test.reflect.ExampleClass",  //$NON-NLS-1$
				concatenateObjectMethods(new String[] {
				EXAMPLE_CLASS+METHOD_NAME1P+"()boolean", //$NON-NLS-1$
				EXAMPLE_CLASS+METHOD_NAME1P+"(java.lang.String)boolean", //$NON-NLS-1$
				EXAMPLE_CLASS+METHOD_NAME21+"()boolean", //$NON-NLS-1$
				EXAMPLE_CLASS+METHOD_NAME21+"(java.lang.String)boolean", //$NON-NLS-1$
				EXAMPLE_CLASS+METHOD_NAME1P+"(java.lang.Integer)void", //$NON-NLS-1$
				EXAMPLE_CLASS+METHOD_NAME21+"(java.lang.Integer)void" //$NON-NLS-1$
		}));

		return methodLists;
	}

	private static final String objectClass = "java.lang.Object";

	static String[] concatenateObjectMethods(String methodList[]) {
		final String[] jlobjectMethods = new String[] {
				objectClass + ".equals(java.lang.Object)boolean",
				objectClass + ".getClass()java.lang.Class",
				objectClass + ".hashCode()int",
				objectClass + ".notify()void",
				objectClass + ".notifyAll()void",
				objectClass + ".toString()java.lang.String",
				objectClass + ".wait()void",
				objectClass + ".wait(long)void",
				objectClass + ".wait(long, int)void" };

		String result[] = new String[methodList.length + jlobjectMethods.length];
		System.arraycopy(methodList, 0, result, 0, methodList.length);
		System.arraycopy(jlobjectMethods, 0, result, methodList.length, jlobjectMethods.length);
		Arrays.sort(result);
		return result;
	}

	@Test(groups = { "level.sanity" })
	public void testGetMethodsAfterNewInstance() throws NoSuchMethodException {
		try {
			B.class.newInstance();
		} catch (ExceptionInInitializerError e) {
		} catch (Exception e) {
		} finally {
			Method method = B.class.getMethod("m");
			Method[] methods = B.class.getMethods();
			int expectedMethodsListSize = 10;
			int declaredMethodsListSize = methods.length;
			Assert.assertEquals(expectedMethodsListSize, declaredMethodsListSize, "wrong methods list size");
			Assert.assertTrue(Arrays.asList(methods).contains(method));
		}
	}

}
