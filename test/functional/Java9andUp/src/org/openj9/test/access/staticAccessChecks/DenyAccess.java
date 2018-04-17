/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package org.openj9.test.access.staticAccessChecks;

import sun.security.internal.spec.TlsRsaPremasterSecretParameterSpec;

import org.testng.annotations.*;

import java.lang.reflect.Field;

import org.testng.*;
import jdk.internal.misc.Unsafe;

@Test(groups = { "level.extended" })
public class DenyAccess {

	public void testAccessJava8Package() {
		boolean threwIAE = false;
		try {
			TlsRsaPremasterSecretParameterSpec t = new TlsRsaPremasterSecretParameterSpec(0, 0);
		} catch (IllegalAccessError e) {
			threwIAE = true;
		}
		
		Assert.assertTrue(threwIAE, "Expected IllegalAccessError not thrown");
	}
	
	public void testAccessJava9Package() {
		boolean threwIAE = false;
		try {
			Field f = Unsafe.class.getDeclaredField("theUnsafe");
			f.setAccessible(true);
			Unsafe unsafe = (Unsafe) f.get(null);
		} catch (NoSuchFieldException | SecurityException | IllegalArgumentException | IllegalAccessException e) {
			e.printStackTrace();
		} catch (IllegalAccessError e) {
			threwIAE = true;
		}
		
		Assert.assertTrue(threwIAE, "Expected IllegalAccessError not thrown");
	}
	
}
