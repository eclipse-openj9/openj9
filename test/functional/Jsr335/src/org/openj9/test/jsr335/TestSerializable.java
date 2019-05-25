/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package org.openj9.test.jsr335;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.concurrent.Callable;

@Test(groups = { "level.sanity" })
public class TestSerializable {
	private static Callable<String> staticPrivateLambda = null;
	protected static Callable<String> staticProtectedLambda = null;
	public static Callable<String> staticPublicLambda = null;
	private Callable<String> privateLambda = null;
	protected Callable<String> protectedLambda = null;
	public Callable<String> publicLambda = null;
	private static String mutatedString;
	
	/**
	 * Basic test that creates a lambda and reserializes it
	 */
	@Test
	public void testBasicSerializable() throws Exception{
		Callable<String> foo = (Callable<String> & Serializable) () -> {
			return "Hello, world";
		};
		
		AssertJUnit.assertEquals(foo.call(), reserialize(foo).call());
	}
	
	/**
	 * Puts a serialized lambda inside another and reserialize both
	 */
	@Test
	public void testNested() throws Exception {
		Callable<String> lamba = (Callable<String> & Serializable) () -> {
			Callable<String> lambda2 = (Callable<String> & Serializable) () -> {
				return "nestedLambda";
			};
			return reserialize(lambda2).call();
		};
		AssertJUnit.assertEquals("nestedLambda", reserialize(lamba).call());
	}
	
	/** Test static reserializing
	 */
	@Test
	public void testPublicStaticSerializable() throws Exception {
		staticPublicLambda = (Callable<String> & Serializable) () -> {
			return "staticPublicLambda";
		};
		
		AssertJUnit.assertEquals(staticPublicLambda.call(), reserialize(staticPublicLambda).call());
	}
	
	@Test
	public void testProtectedStaticSerializable() throws Exception {
		staticProtectedLambda = (Callable<String> & Serializable) () -> {
			return "staticProtectedLambda";
		};
		
		AssertJUnit.assertEquals(staticProtectedLambda.call(), reserialize(staticProtectedLambda).call());
	}
	
	@Test
	public void testPrivateStaticSerializable() throws Exception {
		staticPrivateLambda = (Callable<String> & Serializable) () -> {
			return "staticPrivateLambda";
		};
		
		AssertJUnit.assertEquals(staticPrivateLambda.call(), reserialize(staticPrivateLambda).call());
	}
	
	/** Test member reserializing
	 */
	@Test
	public void testPrivateSerializable() throws Exception {
		privateLambda = (Callable<String> & Serializable) () -> {
			return "privateLambda";
		};
		
		AssertJUnit.assertEquals(privateLambda.call(), reserialize(privateLambda).call());
	}
	
	@Test
	public void testProtectedSerializable() throws Exception {
		protectedLambda = (Callable<String> & Serializable) () -> {
			return "protectedLambda";
		};
		
		AssertJUnit.assertEquals(protectedLambda.call(), reserialize(protectedLambda).call());
	}
	
	@Test
	public void testPublicSerializable() throws Exception {
		publicLambda = (Callable<String> & Serializable) () -> {
			return "publicLambda";
		};
		
		AssertJUnit.assertEquals(publicLambda.call(), reserialize(publicLambda).call());
	}
	
	/** Test reserializing various primitive types */
	@Test
	public void testLocalLongLambda() throws Exception {
		J_Method lamba = (J_Method & Serializable) () -> {
			return (long)42;
		};
		AssertJUnit.assertEquals((long)42, reserialize(lamba).call());
	}
	
	@Test
	public void testLocalIntLambda() throws Exception {
		I_Method lamba = (I_Method & Serializable) () -> {
			return 42;
		};
		AssertJUnit.assertEquals(42, reserialize(lamba).call());
	}
	
	@Test
	public void testLocalFloatLambda() throws Exception {
		F_Method lamba = (F_Method & Serializable) () -> {
			return (float)3.14;
		};
		AssertJUnit.assertEquals((float)3.14, reserialize(lamba).call());
	}
	
	@Test
	public void testLocalDoubleLambda() throws Exception {
		D_Method lamba = (D_Method & Serializable) () -> {
			return (double)3.14;
		};
		AssertJUnit.assertEquals((double)3.14, reserialize(lamba).call());
	}
	
	@Test
	public void testLocalBooleanLambda() throws Exception {
		Z_Method lamba_true = (Z_Method & Serializable) () -> {
			return true;
		};
		Z_Method lamba_false = (Z_Method & Serializable) () -> {
			return false;
		};
		AssertJUnit.assertEquals(true, reserialize(lamba_true).call());
		AssertJUnit.assertEquals(false, reserialize(lamba_false).call());
	}
	
	@Test
	public void testLocalShortLambda() throws Exception {
		S_Method lamba = (S_Method & Serializable) () -> {
			return (short)42;
		};
		AssertJUnit.assertEquals((short)42, reserialize(lamba).call());
	}
	
	@Test
	public void testLocalByteLambda() throws Exception {
		B_Method lamba = (B_Method & Serializable) () -> {
			return (byte)42;
		};
		AssertJUnit.assertEquals((byte)42, reserialize(lamba).call());
	}
	
	@Test
	public void testLocalCharLambda() throws Exception {
		C_Method lamba = (C_Method & Serializable) () -> {
			return 'Q';
		};
		AssertJUnit.assertEquals('Q', reserialize(lamba).call());
	}
	
	/** Test arrays */
	@Test
	public void testStringArrayLambda() throws Exception {
		Array_Method lamba = (Array_Method & Serializable) () -> {
			String stringArray[] = new String[3];
			stringArray[0] = "Goodbye";
			stringArray[1] = "Cruel";
			stringArray[2] = "World";
			return stringArray;
		};
		
		Object[] array = reserialize(lamba).call();
		AssertJUnit.assertEquals(String[].class, array.getClass());
		AssertJUnit.assertEquals(3, array.length);
		String[] sarray = (String[])array;
		AssertJUnit.assertEquals(sarray[0], "Goodbye");
		AssertJUnit.assertEquals(sarray[1], "Cruel");
		AssertJUnit.assertEquals(sarray[2], "World");
	}
	
	private static String getStaticString(){
		return new String("Hello, ").concat("World");
	}
	
	/** Test that lambdas who use generated (ie not static) strings work as expected */
	@Test
	public void testGeneratedStringLambda() throws Exception {
		mutatedString = getStaticString();
		Callable<String> lambda = (Callable<String> & Serializable)() -> {
			return mutatedString;
		};
		Callable<String> lambda2 = reserialize(lambda);
		lambda = null;
		System.gc();
		mutatedString = mutatedString.concat(getStaticString());
		
		AssertJUnit.assertEquals("Hello, WorldHello, World", lambda2.call());
	}
	
	/* Various interfaces used by tests */
	interface J_Method {
		public long call();
	}
	interface I_Method {
		public int call();
	}
	interface F_Method {
		public float call();
	}
	interface D_Method {
		public double call();
	}
	interface Z_Method {
		public boolean call();
	}
	interface S_Method {
		public short call();
	}
	interface B_Method {
		public byte call();
	}
	interface C_Method {
		public char call();
	}
	interface Array_Method {
		public Object[] call();
	}
	
	/**
	 * Reserializes the object to a bag of bytes and then loads it back and returns the result
	 * @param o The object to serialize
	 * @return The deserialized version
	 * @throws IOException
	 */
	private static <T> T reserialize(T o) throws IOException {
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		ObjectOutputStream oos = new ObjectOutputStream(baos);
		
		oos.writeObject(o);
		
		oos.close();
		
		ObjectInputStream iis = new ObjectInputStream(new ByteArrayInputStream(baos.toByteArray()));
		
		try {
			o = (T)iis.readObject();
		} catch (ClassNotFoundException e) {
			throw new IOException(e);
		}
		iis.close();
		return o;
	}

}
