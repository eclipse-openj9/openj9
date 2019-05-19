/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.openj9.test.varhandle;

import static org.testng.AssertJUnit.assertEquals;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandleInfo;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.reflect.Modifier;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.stream.Collectors;

import org.testng.annotations.BeforeSuite;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.extended" })
public class TestVarHandleInfo {

	protected static Logger logger = Logger.getLogger(TestVarHandleInfo.class);
	private static final int MODIFIER_SYNTHETIC = 0x01000; /* see Table 5.5-a of the Java 8 VM spec */
	/* the JVM adds the SYNTHETIC modifier automatically */
	private static final int expectedModifiers = 
			Modifier.PUBLIC + Modifier.FINAL + Modifier.NATIVE 
			+ MODIFIER_SYNTHETIC;
	private static final String EXPECTED_MODIFIERS_STRING = Modifier.toString(expectedModifiers);
	private static final int expectedReferenceKind = MethodHandleInfo.REF_invokeVirtual;
	
	Lookup 	mylookup = MethodHandles.lookup();
	Integer instanceField; /* the target of the varhandle */
	List<String> accessModes;
	HashMap<String, MethodType> infos;

	@Test
	public void testAccessModes() throws NoSuchFieldException, IllegalAccessException {
		/* grab a VarHandle purely to check the accessModeType */
		VarHandle vh = mylookup.findVarHandle(TestVarHandleInfo.class, "instanceField", Integer.class); //$NON-NLS-1$
		for (String methName : accessModes) {
			logger.debug(methName);
			VarHandle.AccessMode am = VarHandle.AccessMode.valueFromMethodName(methName);
			
			MethodHandle mh = MethodHandles.varHandleExactInvoker(am, vh.accessModeType(am));
			checkMethodHandleInfo(mylookup.revealDirect(mh), infos.get(methName), methName);
			
			mh = MethodHandles.varHandleInvoker(am, vh.accessModeType(am));
			checkMethodHandleInfo(mylookup.revealDirect(mh), infos.get(methName), methName);
		}
	}

	private static void checkMethodHandleInfo(MethodHandleInfo info, MethodType expectedMethodType,
			String expectedMethodName) {
		assertEquals("Incorrect declaring class", VarHandle.class, info.getDeclaringClass()); //$NON-NLS-1$

		assertEquals("Incorrect methodType", expectedMethodType, info.getMethodType()); //$NON-NLS-1$

		int actualModifiers = info.getModifiers();
		assertEquals("Missing modifiers: expected \"" + EXPECTED_MODIFIERS_STRING //$NON-NLS-1$
				+ "\" (" + Integer.toHexString(expectedModifiers) //$NON-NLS-1$
				+ "), actual \"" + Modifier.toString(actualModifiers) //$NON-NLS-1$
				+ "\" (" + Integer.toHexString(actualModifiers) + ")", //$NON-NLS-1$ //$NON-NLS-2$
				expectedModifiers, actualModifiers);

		assertEquals("Incorrect name ", expectedMethodName, info.getName()); //$NON-NLS-1$

		assertEquals("Incorrect reference kind, expected " //$NON-NLS-1$
				+ MethodHandleInfo.referenceKindToString(expectedReferenceKind), expectedReferenceKind,
				info.getReferenceKind());
	}

	@BeforeSuite
	public void makeInfos() {
		infos = new HashMap<>();
		accessModes = Arrays.asList(VarHandle.AccessMode.values())
				.stream()
				.map(m -> m.methodName())
				.collect(Collectors.toList());

		MethodType mtInt = MethodType.methodType(Integer.class, TestVarHandleInfo.class);
		accessModes.stream().filter(m -> m.contains("get")).forEach(s -> infos.put(s, mtInt)); //$NON-NLS-1$

		MethodType mtIntVoid = MethodType.methodType(void.class, TestVarHandleInfo.class, Integer.class);
		accessModes.stream().filter(m -> m.contains("set")).forEach(s -> infos.put(s, mtIntVoid)); //$NON-NLS-1$

		MethodType mtIntIntBool = MethodType.methodType(boolean.class, TestVarHandleInfo.class, Integer.class, Integer.class);
		accessModes.stream().filter(m -> m.contains("ompareAndSet")).forEach(s -> infos.put(s, mtIntIntBool)); /* omit leading character as it may be upper or lower case */ //$NON-NLS-1$

		MethodType mtIntIntInt = MethodType.methodType(Integer.class, TestVarHandleInfo.class, Integer.class, Integer.class);
		accessModes.stream().filter(m -> m.contains("ompareAndExchange")).forEach(s -> infos.put(s, mtIntIntInt)); /* omit leading character as it may be upper or lower case */ //$NON-NLS-1$		

		MethodType mtIntInt = MethodType.methodType(Integer.class, TestVarHandleInfo.class, Integer.class);
		accessModes.stream().filter(m -> m.contains("getAnd")).forEach(s -> infos.put(s, mtIntInt)); /* ...set, add, etc. */ //$NON-NLS-1$					
	}
}
