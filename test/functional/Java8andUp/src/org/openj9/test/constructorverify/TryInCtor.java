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
package org.openj9.test.constructorverify;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import org.testng.log4testng.Logger;

@Test(groups = {"level.sanity"})
public class TryInCtor {
	private static Logger logger = Logger.getLogger(TryInCtor.class);
	private static CustomClassLoader classloader = new CustomClassLoader();

	// Check that VerifyError will get thrown for a TRY (with a CATCH clause)
	// that covers opcodes whose stackmap has flagThisUninit set, even if
	// there is no invokespecial opcode.
	@Test
	public void flagThisUninitInTry() {
		byte[]  bytes;
		try {
			bytes = ClassGenerator.dumpFlagThisUninitInTry();
		} catch(Exception e) {
			logger.error("Failed to build class bytes");
			logger.error(e);
			return;
		}
		runTest(bytes, "FlagThisUninitInTry", true);
	}

	// Check that VerifyError will get thrown for a TRY (with a CATCH clause)
	// that covers only an invokespecial opcode in an <init> method when its
	// stackmap has flagThisUninit set.
	@Test
	public void onlyInvokespecialInTry() {
		byte[]  bytes;
		try {
			bytes = ClassGenerator.dumpOnlyInvokespecialInTry();
		} catch(Exception e) {
			logger.error("Failed to build class bytes");
			logger.error(e);
			return;
		}
		runTest(bytes, "OnlyInvokespecialInTry", true);

	}

	// Check that VerifyError will not get thrown for a TRY (with a CATCH)
	// in an <init> method if the TRY/CATCH only covers opcodes after an
	// invokespecial opcode has cleared the stackmap's flagThisUninit flag.
	@Test
	public void tryAfterInvokeSpecial() {
		byte[]  bytes;
		try {
			bytes = ClassGenerator.dumpTryAfterInvokespecial();
		} catch(Exception e) {
			logger.error("Failed to build class bytes");
			logger.error(e);
			return;
		}
		runTest(bytes, "TryAfterInvokespecial", false);
	}


	private void runTest(byte[] classBytes, String className, boolean shouldThrow) {
		try{
			Class<?> newClass = classloader.getClass(className, classBytes);
			Object o = newClass.newInstance();
		} catch (VerifyError e){
			logger.debug(e);
			AssertJUnit.assertTrue("Test threw unexpected VerifyError", shouldThrow);
			return;
		} catch (Throwable f) {
			logger.error("Unexpected error");
			logger.error(f);
			AssertJUnit.assertTrue("test", false);
			return; //probably not needed
		}
		AssertJUnit.assertFalse("Test did not throw expected verify error", shouldThrow);
	}
}

class CustomClassLoader extends ClassLoader {

	public Class<?> getClass(String name, byte[] bytes){
		return defineClass(name, bytes, 0, bytes.length);
	}
}
