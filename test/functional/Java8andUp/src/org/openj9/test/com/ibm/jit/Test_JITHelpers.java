package org.openj9.test.com.ibm.jit;

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

import java.io.FileInputStream;
import java.io.InputStream;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.lang.reflect.Field;
import com.ibm.jit.JITHelpers;
import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import org.testng.Assert;
import org.openj9.test.com.ibm.jit.Test_JITHelpersImpl;

@Test(groups = { "level.sanity" })
public class Test_JITHelpers {

	/**
	 * @tests com.ibm.jit.JITHelpers#getSuperclass(java.lang.Class)
	 */

	public static void test_getSuperclass() {
		final Class[] classes = {FileInputStream.class, JITHelpers.class, int[].class, int.class, Runnable.class, Object.class, void.class};
		final Class[] expectedSuperclasses = {InputStream.class, Object.class, Object.class, null, null, null, null};

		for (int i = 0 ; i < classes.length ; i++) {

			Class superclass = Test_JITHelpersImpl.test_getSuperclassImpl(classes[i]);

			AssertJUnit.assertTrue( "The superclass returned by JITHelpers.getSuperclass() is not equal to the expected one.\n"
					+ "\tExpected superclass: " + expectedSuperclasses[i]
					+ "\n\tReturned superclass: " + superclass,
					(superclass == expectedSuperclasses[i]));
		}

	}

	private static final com.ibm.jit.JITHelpers helpers = getJITHelpers();
	private static com.ibm.jit.JITHelpers getJITHelpers() {

		try {
			Field f = com.ibm.jit.JITHelpers.class.getDeclaredField("helpers");
			f.setAccessible(true);
			return (com.ibm.jit.JITHelpers) f.get(null);
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		} catch (NoSuchFieldException e) {
			throw new RuntimeException(e);
		}
	}

	// Used for storing bytes into a char[]. We could have used the OpenJ9 String helpers here instead however.
	private static final ByteOrder byteOrder = ByteOrder.nativeOrder();

	private static int[] edgecaseLengths = new int[]{0, 1, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33};

	private static char[] lowercaseLatin1Char = {0x6162, 0x6364, 0x6566, 0x6768, 0x696a, 0x6b6c, 0x6d6e, 0x6f70, 0x7172, 0x7374, 0x7576, 0x7778, 0x797a,
		(char)0xe0e1, (char)0xe2e3, (char)0xe4e5, (char)0xe600};
	private static char[] uppercaseLatin1Char = {0x4142, 0x4344, 0x4546, 0x4748, 0x494a, 0x4b4c, 0x4d4e, 0x4f50, 0x5152, 0x5354, 0x5556, 0x5758, 0x595a,
		(char)0xc0c1, (char)0xc2c3, (char)0xc4c5, (char)0xc600};
	private static char[] lowercaseUTF16Char = {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
		0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6};
	private static char[] uppercaseUTF16Char = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51,
		0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6};

	private static byte[] lowercaseLatin1Byte = {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
		0x72, 0x73, 0x74, 0x75, 0x76,0x77, 0x78, 0x79, 0x7a, (byte)0xe0, (byte)0xe1, (byte)0xe2, (byte)0xe3, (byte)0xe4, (byte)0xe5, (byte)0xe6};
	private static byte[] uppercaseLatin1Byte = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51,
		0x52, 0x53, 0x54, 0x55, 0x56,0x57, 0x58, 0x59, 0x5a, (byte)0xc0, (byte)0xc1, (byte)0xc2, (byte)0xc3, (byte)0xc4, (byte)0xc5,(byte)0xc6};
	private static byte[] lowercaseUTF16BigEndianByte = {0x00, 0x61, 0x0, 0x62, 0x0, 0x63, 0x0, 0x64, 0x0, 0x65, 0x0, 0x66, 0x0, 0x67, 0x0, 0x68, 0x0,
		0x69, 0x0, 0x6a, 0x0, 0x6b, 0x0, 0x6c, 0x0, 0x6d, 0x0, 0x6e, 0x0, 0x6f, 0x0, 0x70, 0x0, 0x71, 0x0, 0x72, 0x0, 0x73, 0x0, 0x74, 0x0,
		0x75, 0x0, 0x76, 0x0, 0x77, 0x0, 0x78, 0x0, 0x79, 0x0, 0x7a, 0x0, (byte)0xe0, 0x0, (byte)0xe1, 0x0, (byte)0xe2, 0x0, (byte)0xe3,
		0x0, (byte)0xe4, 0x0, (byte)0xe5, 0x0, (byte)0xe6};
	private static byte[] uppercaseUTF16BigEndianByte = {0x0, 0x41, 0x0, 0x42, 0x0, 0x43, 0x0, 0x44, 0x0, 0x45, 0x0, 0x46, 0x0, 0x47, 0x0, 0x48, 0x0,
		0x49, 0x0, 0x4a, 0x0, 0x4b, 0x0,0x4c, 0x0, 0x4d, 0x0, 0x4e, 0x0, 0x4f, 0x0, 0x50, 0x0, 0x51, 0x0, 0x52, 0x0, 0x53, 0x0, 0x54, 0x0, 0x55, 0x0,
		0x56, 0x0, 0x57, 0x0, 0x58, 0x0, 0x59, 0x0, 0x5a, 0x0, (byte)0xc0, 0x0, (byte)0xc1, 0x0, (byte)0xc2, 0x0, (byte)0xc3, 0x0, (byte)0xc4, 0x0,
		(byte)0xc5, 0x0, (byte)0xc6};
	private static byte[] lowercaseUTF16LittleEndianByte = {0x61, 0x0, 0x62, 0x0, 0x63, 0x0, 0x64, 0x0, 0x65, 0x0, 0x66, 0x0, 0x67, 0x0, 0x68, 0x0, 0x69,
		0x0, 0x6a, 0x0, 0x6b, 0x0, 0x6c, 0x0, 0x6d, 0x0, 0x6e, 0x0, 0x6f, 0x0, 0x70, 0x0, 0x71, 0x0, 0x72, 0x0, 0x73, 0x0, 0x74, 0x0, 0x75, 0x0,
		0x76, 0x0, 0x77, 0x0, 0x78, 0x0, 0x79, 0x0, 0x7a, 0x0, (byte)0xe0, 0x0, (byte)0xe1, 0x0, (byte)0xe2, 0x0, (byte)0xe3, 0x0, (byte)0xe4,
		0x0, (byte)0xe5, 0x0, (byte)0xe6, 0x0};
	private static byte[] uppercaseUTF16LittleEndianByte = {0x41, 0x0, 0x42, 0x0, 0x43, 0x0, 0x44, 0x0, 0x45, 0x0, 0x46, 0x0, 0x47, 0x0, 0x48, 0x0, 0x49, 0x0,
		0x4a, 0x0, 0x4b, 0x0, 0x4c, 0x0, 0x4d, 0x0, 0x4e, 0x0, 0x4f, 0x0, 0x50, 0x0, 0x51, 0x0, 0x52, 0x0, 0x53, 0x0, 0x54, 0x0, 0x55, 0x0, 0x56, 0x0,
		0x57, 0x0, 0x58, 0x0, 0x59, 0x0, 0x5a, 0x0, (byte)0xc0, 0x0, (byte)0xc1, 0x0, (byte)0xc2, 0x0, (byte)0xc3, 0x0, (byte)0xc4, 0x0, (byte)0xc5,
		0x0, (byte)0xc6, 0x0};

	private static int[] indexes = new int[]{0, 1, 4, 7, 8, 9, 15, 16, 17, 31, 32};

	private static Map<Character, Character> toLowerCaseMap = new HashMap<Character, Character>() {{
		put((char)0x00, (char)0x00); put((char)0x10, (char)0x10); put((char)0x20, (char)0x20); put((char)0x30, (char)0x30);
		put((char)0x01, (char)0x01); put((char)0x11, (char)0x11); put((char)0x21, (char)0x21); put((char)0x31, (char)0x31);
		put((char)0x02, (char)0x02); put((char)0x12, (char)0x12); put((char)0x22, (char)0x22); put((char)0x32, (char)0x32);
		put((char)0x03, (char)0x03); put((char)0x13, (char)0x13); put((char)0x23, (char)0x23); put((char)0x33, (char)0x33);
		put((char)0x04, (char)0x04); put((char)0x14, (char)0x14); put((char)0x24, (char)0x24); put((char)0x34, (char)0x34);
		put((char)0x05, (char)0x05); put((char)0x15, (char)0x15); put((char)0x25, (char)0x25); put((char)0x35, (char)0x35);
		put((char)0x06, (char)0x06); put((char)0x16, (char)0x16); put((char)0x26, (char)0x26); put((char)0x36, (char)0x36);
		put((char)0x07, (char)0x07); put((char)0x17, (char)0x17); put((char)0x27, (char)0x27); put((char)0x37, (char)0x37);
		put((char)0x08, (char)0x08); put((char)0x18, (char)0x18); put((char)0x28, (char)0x28); put((char)0x38, (char)0x38);
		put((char)0x09, (char)0x09); put((char)0x19, (char)0x19); put((char)0x29, (char)0x29); put((char)0x39, (char)0x39);
		put((char)0x0A, (char)0x0A); put((char)0x1A, (char)0x1A); put((char)0x2A, (char)0x2A); put((char)0x3A, (char)0x3A);
		put((char)0x0B, (char)0x0B); put((char)0x1B, (char)0x1B); put((char)0x2B, (char)0x2B); put((char)0x3B, (char)0x3B);
		put((char)0x0C, (char)0x0C); put((char)0x1C, (char)0x1C); put((char)0x2C, (char)0x2C); put((char)0x3C, (char)0x3C);
		put((char)0x0D, (char)0x0D); put((char)0x1D, (char)0x1D); put((char)0x2D, (char)0x2D); put((char)0x3D, (char)0x3D);
		put((char)0x0E, (char)0x0E); put((char)0x1E, (char)0x1E); put((char)0x2E, (char)0x2E); put((char)0x3E, (char)0x3E);
		put((char)0x0F, (char)0x0F); put((char)0x1F, (char)0x1F); put((char)0x2F, (char)0x2F); put((char)0x3F, (char)0x3F);

		put((char)0x40, (char)0x40); put((char)0x50, (char)0x70); put((char)0x60, (char)0x60); put((char)0x70, (char)0x70);
		put((char)0x41, (char)0x61); put((char)0x51, (char)0x71); put((char)0x61, (char)0x61); put((char)0x71, (char)0x71);
		put((char)0x42, (char)0x62); put((char)0x52, (char)0x72); put((char)0x62, (char)0x62); put((char)0x72, (char)0x72);
		put((char)0x43, (char)0x63); put((char)0x53, (char)0x73); put((char)0x63, (char)0x63); put((char)0x73, (char)0x73);
		put((char)0x44, (char)0x64); put((char)0x54, (char)0x74); put((char)0x64, (char)0x64); put((char)0x74, (char)0x74);
		put((char)0x45, (char)0x65); put((char)0x55, (char)0x75); put((char)0x65, (char)0x65); put((char)0x75, (char)0x75);
		put((char)0x46, (char)0x66); put((char)0x56, (char)0x76); put((char)0x66, (char)0x66); put((char)0x76, (char)0x76);
		put((char)0x47, (char)0x67); put((char)0x57, (char)0x77); put((char)0x67, (char)0x67); put((char)0x77, (char)0x77);
		put((char)0x48, (char)0x68); put((char)0x58, (char)0x78); put((char)0x68, (char)0x68); put((char)0x78, (char)0x78);
		put((char)0x49, (char)0x69); put((char)0x59, (char)0x79); put((char)0x69, (char)0x69); put((char)0x79, (char)0x79);
		put((char)0x4A, (char)0x6A); put((char)0x5A, (char)0x7A); put((char)0x6A, (char)0x6A); put((char)0x7A, (char)0x7A);
		put((char)0x4B, (char)0x6B); put((char)0x5B, (char)0x5B); put((char)0x6B, (char)0x6B); put((char)0x7B, (char)0x7B);
		put((char)0x4C, (char)0x6C); put((char)0x5C, (char)0x5C); put((char)0x6C, (char)0x6C); put((char)0x7C, (char)0x7C);
		put((char)0x4D, (char)0x6D); put((char)0x5D, (char)0x5D); put((char)0x6D, (char)0x6D); put((char)0x7D, (char)0x7D);
		put((char)0x4E, (char)0x6E); put((char)0x5E, (char)0x5E); put((char)0x6E, (char)0x6E); put((char)0x7E, (char)0x7E);
		put((char)0x4F, (char)0x6F); put((char)0x5F, (char)0x5F); put((char)0x6F, (char)0x6F); put((char)0x7F, (char)0x7F);
		
		put((char)0x80, (char)0x80); put((char)0x90, (char)0x90); put((char)0xA0, (char)0xA0); put((char)0xB0, (char)0xB0);
		put((char)0x81, (char)0x81); put((char)0x91, (char)0x91); put((char)0xA1, (char)0xA1); put((char)0xB1, (char)0xB1);
		put((char)0x82, (char)0x82); put((char)0x92, (char)0x92); put((char)0xA2, (char)0xA2); put((char)0xB2, (char)0xB2);
		put((char)0x83, (char)0x83); put((char)0x93, (char)0x93); put((char)0xA3, (char)0xA3); put((char)0xB3, (char)0xB3);
		put((char)0x84, (char)0x84); put((char)0x94, (char)0x94); put((char)0xA4, (char)0xA4); put((char)0xB4, (char)0xB4);
		put((char)0x85, (char)0x85); put((char)0x95, (char)0x95); put((char)0xA5, (char)0xA5); put((char)0xB5, (char)0xB5);
		put((char)0x86, (char)0x86); put((char)0x96, (char)0x96); put((char)0xA6, (char)0xA6); put((char)0xB6, (char)0xB6);
		put((char)0x87, (char)0x87); put((char)0x97, (char)0x97); put((char)0xA7, (char)0xA7); put((char)0xB7, (char)0xB7);
		put((char)0x88, (char)0x88); put((char)0x98, (char)0x98); put((char)0xA8, (char)0xA8); put((char)0xB8, (char)0xB8);
		put((char)0x89, (char)0x89); put((char)0x99, (char)0x99); put((char)0xA9, (char)0xA9); put((char)0xB9, (char)0xB9);
		put((char)0x8A, (char)0x8A); put((char)0x9A, (char)0x9A); put((char)0xAA, (char)0xAA); put((char)0xBA, (char)0xBA);
		put((char)0x8B, (char)0x8B); put((char)0x9B, (char)0x9B); put((char)0xAB, (char)0xAB); put((char)0xBB, (char)0xBB);
		put((char)0x8C, (char)0x8C); put((char)0x9C, (char)0x9C); put((char)0xAC, (char)0xAC); put((char)0xBC, (char)0xBC);
		put((char)0x8D, (char)0x8D); put((char)0x9D, (char)0x9D); put((char)0xAD, (char)0xAD); put((char)0xBD, (char)0xBD);
		put((char)0x8E, (char)0x8E); put((char)0x9E, (char)0x9E); put((char)0xAE, (char)0xAE); put((char)0xBE, (char)0xBE);
		put((char)0x8F, (char)0x8F); put((char)0x9F, (char)0x9F); put((char)0xAF, (char)0xAF); put((char)0xBF, (char)0xBF);
		
		put((char)0xC0, (char)0xE0); put((char)0xD0, (char)0xF0); put((char)0xE0, (char)0xE0); put((char)0xF0, (char)0xF0);
		put((char)0xC1, (char)0xE1); put((char)0xD1, (char)0xF1); put((char)0xE1, (char)0xE1); put((char)0xF1, (char)0xF1);
		put((char)0xC2, (char)0xE2); put((char)0xD2, (char)0xF2); put((char)0xE2, (char)0xE2); put((char)0xF2, (char)0xF2);
		put((char)0xC3, (char)0xE3); put((char)0xD3, (char)0xF3); put((char)0xE3, (char)0xE3); put((char)0xF3, (char)0xF3);
		put((char)0xC4, (char)0xE4); put((char)0xD4, (char)0xF4); put((char)0xE4, (char)0xE4); put((char)0xF4, (char)0xF4);
		put((char)0xC5, (char)0xE5); put((char)0xD5, (char)0xF5); put((char)0xE5, (char)0xE5); put((char)0xF5, (char)0xF5);
		put((char)0xC6, (char)0xE6); put((char)0xD6, (char)0xF6); put((char)0xE6, (char)0xE6); put((char)0xF6, (char)0xF6);
		put((char)0xC7, (char)0xE7); put((char)0xD7, (char)0xD7); put((char)0xE7, (char)0xE7); put((char)0xF7, (char)0xF7);
		put((char)0xC8, (char)0xE8); put((char)0xD8, (char)0xF8); put((char)0xE8, (char)0xE8); put((char)0xF8, (char)0xF8);
		put((char)0xC9, (char)0xE9); put((char)0xD9, (char)0xF9); put((char)0xE9, (char)0xE9); put((char)0xF9, (char)0xF9);
		put((char)0xCA, (char)0xEA); put((char)0xDA, (char)0xFA); put((char)0xEA, (char)0xEA); put((char)0xFA, (char)0xFA);
		put((char)0xCB, (char)0xEB); put((char)0xDB, (char)0xFB); put((char)0xEB, (char)0xEB); put((char)0xFB, (char)0xFB);
		put((char)0xCC, (char)0xEC); put((char)0xDC, (char)0xFC); put((char)0xEC, (char)0xEC); put((char)0xFC, (char)0xFC);
		put((char)0xCD, (char)0xED); put((char)0xDD, (char)0xFD); put((char)0xED, (char)0xED); put((char)0xFD, (char)0xFD);
		put((char)0xCE, (char)0xEE); put((char)0xDE, (char)0xFE); put((char)0xEE, (char)0xEE); put((char)0xFE, (char)0xFE);
		put((char)0xCF, (char)0xEF); put((char)0xDF, (char)0xDF); put((char)0xEF, (char)0xEF); put((char)0xFF, (char)0xFF);
	}};

	private static Map<Character, Character> toUpperCaseMap = new HashMap<Character, Character>() {{
		put((char)0x00, (char)0x00); put((char)0x10, (char)0x10); put((char)0x20, (char)0x20); put((char)0x30, (char)0x30);
		put((char)0x01, (char)0x01); put((char)0x11, (char)0x11); put((char)0x21, (char)0x21); put((char)0x31, (char)0x31);
		put((char)0x02, (char)0x02); put((char)0x12, (char)0x12); put((char)0x22, (char)0x22); put((char)0x32, (char)0x32);
		put((char)0x03, (char)0x03); put((char)0x13, (char)0x13); put((char)0x23, (char)0x23); put((char)0x33, (char)0x33);
		put((char)0x04, (char)0x04); put((char)0x14, (char)0x14); put((char)0x24, (char)0x24); put((char)0x34, (char)0x34);
		put((char)0x05, (char)0x05); put((char)0x15, (char)0x15); put((char)0x25, (char)0x25); put((char)0x35, (char)0x35);
		put((char)0x06, (char)0x06); put((char)0x16, (char)0x16); put((char)0x26, (char)0x26); put((char)0x36, (char)0x36);
		put((char)0x07, (char)0x07); put((char)0x17, (char)0x17); put((char)0x27, (char)0x27); put((char)0x37, (char)0x37);
		put((char)0x08, (char)0x08); put((char)0x18, (char)0x18); put((char)0x28, (char)0x28); put((char)0x38, (char)0x38);
		put((char)0x09, (char)0x09); put((char)0x19, (char)0x19); put((char)0x29, (char)0x29); put((char)0x39, (char)0x39);
		put((char)0x0A, (char)0x0A); put((char)0x1A, (char)0x1A); put((char)0x2A, (char)0x2A); put((char)0x3A, (char)0x3A);
		put((char)0x0B, (char)0x0B); put((char)0x1B, (char)0x1B); put((char)0x2B, (char)0x2B); put((char)0x3B, (char)0x3B);
		put((char)0x0C, (char)0x0C); put((char)0x1C, (char)0x1C); put((char)0x2C, (char)0x2C); put((char)0x3C, (char)0x3C);
		put((char)0x0D, (char)0x0D); put((char)0x1D, (char)0x1D); put((char)0x2D, (char)0x2D); put((char)0x3D, (char)0x3D);
		put((char)0x0E, (char)0x0E); put((char)0x1E, (char)0x1E); put((char)0x2E, (char)0x2E); put((char)0x3E, (char)0x3E);
		put((char)0x0F, (char)0x0F); put((char)0x1F, (char)0x1F); put((char)0x2F, (char)0x2F); put((char)0x3F, (char)0x3F);

		put((char)0x40, (char)0x40); put((char)0x50, (char)0x50); put((char)0x60, (char)0x60); put((char)0x70, (char)0x50);
		put((char)0x41, (char)0x41); put((char)0x51, (char)0x51); put((char)0x61, (char)0x41); put((char)0x71, (char)0x51);
		put((char)0x42, (char)0x42); put((char)0x52, (char)0x52); put((char)0x62, (char)0x42); put((char)0x72, (char)0x52);
		put((char)0x43, (char)0x43); put((char)0x53, (char)0x53); put((char)0x63, (char)0x43); put((char)0x73, (char)0x53);
		put((char)0x44, (char)0x44); put((char)0x54, (char)0x54); put((char)0x64, (char)0x44); put((char)0x74, (char)0x54);
		put((char)0x45, (char)0x45); put((char)0x55, (char)0x55); put((char)0x65, (char)0x45); put((char)0x75, (char)0x55);
		put((char)0x46, (char)0x46); put((char)0x56, (char)0x56); put((char)0x66, (char)0x46); put((char)0x76, (char)0x56);
		put((char)0x47, (char)0x47); put((char)0x57, (char)0x57); put((char)0x67, (char)0x47); put((char)0x77, (char)0x57);
		put((char)0x48, (char)0x48); put((char)0x58, (char)0x58); put((char)0x68, (char)0x48); put((char)0x78, (char)0x58);
		put((char)0x49, (char)0x49); put((char)0x59, (char)0x59); put((char)0x69, (char)0x49); put((char)0x79, (char)0x59);
		put((char)0x4A, (char)0x4A); put((char)0x5A, (char)0x5A); put((char)0x6A, (char)0x4A); put((char)0x7A, (char)0x5A);
		put((char)0x4B, (char)0x4B); put((char)0x5B, (char)0x5B); put((char)0x6B, (char)0x4B); put((char)0x7B, (char)0x7B);
		put((char)0x4C, (char)0x4C); put((char)0x5C, (char)0x5C); put((char)0x6C, (char)0x4C); put((char)0x7C, (char)0x7C);
		put((char)0x4D, (char)0x4D); put((char)0x5D, (char)0x5D); put((char)0x6D, (char)0x4D); put((char)0x7D, (char)0x7D);
		put((char)0x4E, (char)0x4E); put((char)0x5E, (char)0x5E); put((char)0x6E, (char)0x4E); put((char)0x7E, (char)0x7E);
		put((char)0x4F, (char)0x4F); put((char)0x5F, (char)0x5F); put((char)0x6F, (char)0x4F); put((char)0x7F, (char)0x7F);

		put((char)0x80, (char)0x80); put((char)0x90, (char)0x90); put((char)0xA0, (char)0xA0); put((char)0xB0, (char)0xB0);
		put((char)0x81, (char)0x81); put((char)0x91, (char)0x91); put((char)0xA1, (char)0xA1); put((char)0xB1, (char)0xB1);
		put((char)0x82, (char)0x82); put((char)0x92, (char)0x92); put((char)0xA2, (char)0xA2); put((char)0xB2, (char)0xB2);
		put((char)0x83, (char)0x83); put((char)0x93, (char)0x93); put((char)0xA3, (char)0xA3); put((char)0xB3, (char)0xB3);
		put((char)0x84, (char)0x84); put((char)0x94, (char)0x94); put((char)0xA4, (char)0xA4); put((char)0xB4, (char)0xB4);
		put((char)0x85, (char)0x85); put((char)0x95, (char)0x95); put((char)0xA5, (char)0xA5); put((char)0xB5, (char)0x39C);
		put((char)0x86, (char)0x86); put((char)0x96, (char)0x96); put((char)0xA6, (char)0xA6); put((char)0xB6, (char)0xB6);
		put((char)0x87, (char)0x87); put((char)0x97, (char)0x97); put((char)0xA7, (char)0xA7); put((char)0xB7, (char)0xB7);
		put((char)0x88, (char)0x88); put((char)0x98, (char)0x98); put((char)0xA8, (char)0xA8); put((char)0xB8, (char)0xB8);
		put((char)0x89, (char)0x89); put((char)0x99, (char)0x99); put((char)0xA9, (char)0xA9); put((char)0xB9, (char)0xB9);
		put((char)0x8A, (char)0x8A); put((char)0x9A, (char)0x9A); put((char)0xAA, (char)0xAA); put((char)0xBA, (char)0xBA);
		put((char)0x8B, (char)0x8B); put((char)0x9B, (char)0x9B); put((char)0xAB, (char)0xAB); put((char)0xBB, (char)0xBB);
		put((char)0x8C, (char)0x8C); put((char)0x9C, (char)0x9C); put((char)0xAC, (char)0xAC); put((char)0xBC, (char)0xBC);
		put((char)0x8D, (char)0x8D); put((char)0x9D, (char)0x9D); put((char)0xAD, (char)0xAD); put((char)0xBD, (char)0xBD);
		put((char)0x8E, (char)0x8E); put((char)0x9E, (char)0x9E); put((char)0xAE, (char)0xAE); put((char)0xBE, (char)0xBE);
		put((char)0x8F, (char)0x8F); put((char)0x9F, (char)0x9F); put((char)0xAF, (char)0xAF); put((char)0xBF, (char)0xBF);
		
		put((char)0xC0, (char)0xC0); put((char)0xD0, (char)0xD0); put((char)0xE0, (char)0xC0); put((char)0xF0, (char)0xD0);
		put((char)0xC1, (char)0xC1); put((char)0xD1, (char)0xD1); put((char)0xE1, (char)0xC1); put((char)0xF1, (char)0xD1);
		put((char)0xC2, (char)0xC2); put((char)0xD2, (char)0xD2); put((char)0xE2, (char)0xC2); put((char)0xF2, (char)0xD2);
		put((char)0xC3, (char)0xC3); put((char)0xD3, (char)0xD3); put((char)0xE3, (char)0xC3); put((char)0xF3, (char)0xD3);
		put((char)0xC4, (char)0xC4); put((char)0xD4, (char)0xD4); put((char)0xE4, (char)0xC4); put((char)0xF4, (char)0xD4);
		put((char)0xC5, (char)0xC5); put((char)0xD5, (char)0xD5); put((char)0xE5, (char)0xC5); put((char)0xF5, (char)0xD5);
		put((char)0xC6, (char)0xC6); put((char)0xD6, (char)0xD6); put((char)0xE6, (char)0xC6); put((char)0xF6, (char)0xD6);
		put((char)0xC7, (char)0xC7); put((char)0xD7, (char)0xD7); put((char)0xE7, (char)0xC7); put((char)0xF7, (char)0xF7);
		put((char)0xC8, (char)0xC8); put((char)0xD8, (char)0xD8); put((char)0xE8, (char)0xC8); put((char)0xF8, (char)0xD8);
		put((char)0xC9, (char)0xC9); put((char)0xD9, (char)0xD9); put((char)0xE9, (char)0xC9); put((char)0xF9, (char)0xD9);
		put((char)0xCA, (char)0xCA); put((char)0xDA, (char)0xDA); put((char)0xEA, (char)0xCA); put((char)0xFA, (char)0xDA);
		put((char)0xCB, (char)0xCB); put((char)0xDB, (char)0xDB); put((char)0xEB, (char)0xCB); put((char)0xFB, (char)0xDB);
		put((char)0xCC, (char)0xCC); put((char)0xDC, (char)0xDC); put((char)0xEC, (char)0xCC); put((char)0xFC, (char)0xDC);
		put((char)0xCD, (char)0xCD); put((char)0xDD, (char)0xDD); put((char)0xED, (char)0xCD); put((char)0xFD, (char)0xDD);
		put((char)0xCE, (char)0xCE); put((char)0xDE, (char)0xDE); put((char)0xEE, (char)0xCE); put((char)0xFE, (char)0xDE);
		put((char)0xCF, (char)0xCF); put((char)0xDF, (char)0x53); put((char)0xEF, (char)0xCF); put((char)0xFF, (char)0x178);
	}};

	/**
	 * @tests com.ibm.jit.JITHelpers#(char[], char[], int)
	 */
	public static void test_toUpperIntrinsicUTF16() {
		for (Map.Entry<Character, Character> pair : toUpperCaseMap.entrySet()) {
			char[] buffer = new char[] { pair.getKey().charValue() };
			char[] actual = new char[1];
			char[] expected = new char[] { pair.getValue().charValue() };
			
			boolean result = helpers.toUpperIntrinsicUTF16(buffer, actual, 2);

			if (result) {
				Assert.assertTrue(result == (Math.abs(buffer[0] - expected[0]) == 0x20 || Math.abs(buffer[0] - expected[0]) == 0x00) && actual[0] == expected[0],
					"Failed to correctly uppercase character 0x" + Integer.toHexString(buffer[0]) + ", actual = 0x" + Integer.toHexString(actual[0]) + " expected = 0x" + Integer.toHexString(pair.getValue().charValue()));
			}
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#(char[], char[], int)
	 */
	public static void test_toLowerIntrinsicUTF16() {
		for (Map.Entry<Character, Character> pair : toLowerCaseMap.entrySet()) {
			char[] buffer = new char[] { pair.getKey().charValue() };
			char[] actual = new char[1];
			char[] expected = new char[] { pair.getValue().charValue() };
			
			boolean result = helpers.toLowerIntrinsicUTF16(buffer, actual, 2);

			if (result) {
				Assert.assertTrue(result == (Math.abs(buffer[0] - expected[0]) == 0x20 || Math.abs(buffer[0] - expected[0]) == 0x00) && actual[0] == expected[0],
					"Failed to correctly lowercase character 0x" + Integer.toHexString(buffer[0]) + ", actual = 0x" + Integer.toHexString(actual[0]) + " expected = 0x" + Integer.toHexString(pair.getValue().charValue()));
			}
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#(byte[], byte[], int)
	 */
	public static void test_toUpperIntrinsicLatin1() {
		for (Map.Entry<Character, Character> pair : toUpperCaseMap.entrySet()) {
			byte[] buffer = new byte[] { (byte)pair.getKey().charValue() };
			byte[] actual = new byte[1];
			byte[] expected = new byte[] { (byte)pair.getValue().charValue() };
			
			boolean result = helpers.toUpperIntrinsicLatin1(buffer, actual, 1);

			if (result) {
				Assert.assertTrue(result == (Math.abs(buffer[0] - expected[0]) == 0x20 || Math.abs(buffer[0] - expected[0]) == 0x00) && actual[0] == expected[0],
					"Failed to correctly uppercase character 0x" + Integer.toHexString(buffer[0] & 0xFF) + ", actual = 0x" + Integer.toHexString(actual[0] & 0xFF) + " expected = 0x" + Integer.toHexString(pair.getValue().charValue()));
			}
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#(byte[], byte[], int)
	 */
	public static void test_toLowerIntrinsicLatin1() {
		for (Map.Entry<Character, Character> pair : toLowerCaseMap.entrySet()) {
			byte[] buffer = new byte[] { (byte)pair.getKey().charValue() };
			byte[] actual = new byte[1];
			byte[] expected = new byte[] { (byte)pair.getValue().charValue() };
			
			boolean result = helpers.toLowerIntrinsicLatin1(buffer, actual, 1);

			if (result) {
				Assert.assertTrue(result == (Math.abs(buffer[0] - expected[0]) == 0x20 || Math.abs(buffer[0] - expected[0]) == 0x00) && actual[0] == expected[0],
					"Failed to correctly lowercase character 0x" + Integer.toHexString(buffer[0] & 0xFF) + ", actual = 0x" + Integer.toHexString(actual[0] & 0xFF) + " expected = 0x" + Integer.toHexString(pair.getValue().charValue()));
			}
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#(char[], char[], int)
	 */
	public static void test_toUpperIntrinsicUTF16_edgecaseLengths() {
		for (int j : edgecaseLengths){
			char[] buffer = new char[j];
			if (helpers.toUpperIntrinsicUTF16(Arrays.copyOfRange(lowercaseUTF16Char, 0, j), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(uppercaseUTF16Char, 0, j)),
					"UTF16 JITHelper upper case conversion with char arrays of " + j + " letters failed");
			}
		}

		for (int j : edgecaseLengths) {
			byte[] buffer = new byte[j * 2];
			if (helpers.toUpperIntrinsicUTF16(Arrays.copyOfRange(lowercaseUTF16BigEndianByte, 0, j * 2), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(uppercaseUTF16BigEndianByte, 0, j * 2)),
					"UTF16 JITHelper upper case conversion with big endian byte arrays of " + j + " letters failed");
			}

			if (helpers.toUpperIntrinsicUTF16(Arrays.copyOfRange(lowercaseUTF16LittleEndianByte, 0, j * 2), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(uppercaseUTF16LittleEndianByte, 0, j * 2)),
					"UTF16 JITHelper upper case conversion with little endian byte arrays of " + j + " letters failed");
			}
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toLowerIntrinsicUTF16(char[], char[], int)
	 */
	public static void test_toLowerIntrinsicUTF16_edgecaseLengths() {
		for (int j : edgecaseLengths) {
			char[] buffer = new char[j];
			if (helpers.toLowerIntrinsicUTF16(Arrays.copyOfRange(uppercaseUTF16Char, 0, j), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(lowercaseUTF16Char, 0, j)),
					"UTF16 JITHelper lower case conversion with byte arrays of " + j + " letters failed");
			}
		}

		for (int j : edgecaseLengths) {
			byte[] buffer = new byte[j * 2];
			if (helpers.toLowerIntrinsicUTF16(Arrays.copyOfRange(uppercaseUTF16BigEndianByte, 0, j * 2), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(lowercaseUTF16BigEndianByte, 0, j * 2)),
					"UTF16 JITHelper lower case conversion with big endian byte arrays of " + j + " letters failed");
			}

			if (helpers.toLowerIntrinsicUTF16(Arrays.copyOfRange(uppercaseUTF16LittleEndianByte, 0, j * 2), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(lowercaseUTF16LittleEndianByte, 0, j * 2)),
					"UTF16 JITHelper lower case conversion with little endian byte arrays of " + j + " letters failed");
			}
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toLowerIntrinsicLatin1(char[], char[], int)
	 */
	public static void test_toUpperIntrinsicLatin1_edgecaseLengths() {
		for (int j : edgecaseLengths) {
			char[] source = Arrays.copyOfRange(lowercaseLatin1Char, 0, (j + 1) / 2);
			char[] converted = Arrays.copyOfRange(uppercaseLatin1Char, 0, (j + 1) / 2);

			if (j % 2 == 1) {
				if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
					source[source.length - 1] &= 0x00FF;
					converted[source.length - 1] &= 0x00FF;
				} else {
					source[source.length - 1] &= 0xFF00;
					converted[source.length - 1] &= 0xFF00;
				}
			}

			char[] buffer = new char[(j + 1) / 2];
			if (helpers.toUpperIntrinsicLatin1(source, buffer, j)) {
				Assert.assertTrue(Arrays.equals(buffer, converted),
					"Latin 1 JITHelper upper case conversion with char arrays of " + j + " letters failed");
			}
		}

		for (int j : edgecaseLengths) {
			byte[] buffer = new byte[j];
			if (helpers.toUpperIntrinsicLatin1(Arrays.copyOfRange(lowercaseLatin1Byte, 0, j), buffer, j)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(uppercaseLatin1Byte, 0, j)),
					"Latin 1 JITHelper upper case conversion with byte arrays of " + j + " letters failed");
			}
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toLowerIntrinsicLatin1(char[], char[], int)
	 */
	public static void test_toLowerIntrinsicLatin1_edgecaseLengths() {
		for (int j : edgecaseLengths) {
			char[] source = Arrays.copyOfRange(uppercaseLatin1Char, 0, (j + 1) / 2);
			char[] converted = Arrays.copyOfRange(lowercaseLatin1Char, 0, (j + 1) / 2);

			if (j % 2 == 1) {
				if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
					source[source.length - 1] &= 0x00FF;
					converted[source.length - 1] &= 0x00FF;
				} else {
					source[source.length - 1] &= 0xFF00;
					converted[source.length - 1] &= 0xFF00;
				}
			}

			char[] buffer = new char[(j + 1) / 2];
			if (helpers.toLowerIntrinsicLatin1(source, buffer, j)) {
				Assert.assertTrue(Arrays.equals(buffer, converted),
					"Latin 1 JITHelper lower case conversion with char arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}

		for (int j : edgecaseLengths) {
			byte[] buffer = new byte[j];
			if (helpers.toLowerIntrinsicLatin1(Arrays.copyOfRange(uppercaseLatin1Byte, 0, j), buffer, j)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(lowercaseLatin1Byte, 0, j)),
					"Latin 1 JITHelper lower case conversion with byte arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#intrinsicIndexOfLatin1(Object, byte, int, int)
	 */
	public static void test_intrinsicIndexOfLatin1() {
		try {
			Field valueField = String.class.getDeclaredField("value");
			valueField.setAccessible(true);

			// Latin1 tests are valid if and only if compact strings are enabled because of the way we extract the
			// String.value field and pass it off to the intrinsicIndexOfLatin1 API. The only way to ensure
			// the extracted array is in Latin1 format is to make sure compact strings are enabled.
			Field enableCompressionField = String.class.getDeclaredField("enableCompression");
			enableCompressionField.setAccessible(true);

			if ((boolean)enableCompressionField.get(null)) {
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get(""), (byte)'a', 0, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get(""), (byte)'a', 5, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'a', 0, 1), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'a', 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'b', 0, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'b', 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'A', 0, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'b', 0, 2), 1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'b', 1, 2), 1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'B', 0, 2), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'B', 1, 2), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'a', 0, 2), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'A', 1, 2), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'a', 1, 2), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("aba"), (byte)'b', 1, 3), 1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'o', 0, 15), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'o', 7, 15), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'a', 0, 15), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'x', 0, 15), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'x', 1, 15), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'x', 7, 15), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'d', 0, 15), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'d', 2, 15), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'d', 5, 15), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'o', 0, 16), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'o', 7, 16), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'p', 0, 16), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'p', 5, 16), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'a', 0, 16), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'x', 0, 16), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'x', 1, 16), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'x', 7, 16), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'d', 0, 16), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'d', 2, 16), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'d', 5, 16), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'o', 0, 17), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'o', 7, 17), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'p', 0, 17), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'p', 5, 17), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'q', 0, 17), 16);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'q', 5, 17), 16);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'a', 0, 17), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'x', 0, 17), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'x', 1, 17), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'x', 7, 17), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'d', 0, 17), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'d', 2, 17), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'d', 5, 17), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'o', 0, 31), 28);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'o', 7, 31), 28);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'p', 0, 31), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'p', 5, 31), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'q', 0, 31), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'q', 5, 31), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'a', 0, 31), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'x', 0, 31), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'x', 1, 31), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'x', 7, 31), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'d', 0, 31), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 0, 31), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 1, 31), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 2, 31), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 3, 31), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 4, 31), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'d', 2, 31), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'d', 5, 31), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'o', 0, 32), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'o', 7, 32), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'p', 0, 32), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'p', 5, 32), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'q', 0, 32), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'q', 5, 32), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'a', 0, 32), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'x', 0, 32), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'x', 1, 32), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'x', 7, 32), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'d', 0, 32), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'d', 2, 32), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'d', 5, 32), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'o', 0, 34), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'o', 7, 34), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'p', 0, 34), 32);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'p', 5, 34), 32);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'q', 0, 34), 33);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'q', 5, 34), 33);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'a', 0, 34), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'x', 0, 34), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'x', 1, 34), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'x', 7, 34), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'d', 0, 34), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 3, 34), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 4, 34), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 0, 34), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 1, 34), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 2, 34), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'6', 3, 34), 7);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'6', 8, 34), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'d', 2, 34), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'d', 5, 34), 20);
			}
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		} catch (NoSuchFieldException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#intrinsicIndexOfUTF16(Object, char, int, int)
	 */
	public static void test_intrinsicIndexOfUTF16() {
		try {
			Field valueField = String.class.getDeclaredField("value");
			valueField.setAccessible(true);
			
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get(""), '\u0190', 0, 0), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get(""), '\u0190', 5, 0), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), '\u0190', 0, 1), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), '\u0190', 1, 1), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), 'b', 0, 1), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), 'b', 1, 1), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), 'A', 0, 1), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'b', 0, 2), 1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'b', 1, 2), 1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'B', 0, 2), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'B', 1, 2), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), '\u0190', 0, 2), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'A', 1, 2), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), '\u0190', 1, 2), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b\u0190"), 'b', 1, 3), 1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'o', 0, 15), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'o', 7, 15), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), '\u0190', 0, 15), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'x', 0, 15), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'x', 1, 15), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'x', 7, 15), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'd', 0, 15), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'd', 2, 15), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'd', 5, 15), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'o', 0, 16), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'o', 7, 16), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'p', 0, 16), 15);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'p', 5, 16), 15);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), '\u0190', 0, 16), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'x', 0, 16), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'x', 1, 16), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'x', 7, 16), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'd', 0, 16), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'd', 2, 16), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'd', 5, 16), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'o', 0, 17), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'o', 7, 17), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'p', 0, 17), 15);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'p', 5, 17), 15);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'q', 0, 17), 16);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'q', 5, 17), 16);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), '\u0190', 0, 17), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'x', 0, 17), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'x', 1, 17), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'x', 7, 17), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'd', 0, 17), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'd', 2, 17), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'd', 5, 17), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'o', 0, 31), 28);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'o', 7, 31), 28);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'p', 0, 31), 29);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'p', 5, 31), 29);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'q', 0, 31), 30);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'q', 5, 31), 30);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '\u0190', 0, 31), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'x', 0, 31), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'x', 1, 31), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'x', 7, 31), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'd', 0, 31), 17);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 0, 31), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 1, 31), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 2, 31), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 3, 31), 12);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 4, 31), 12);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'd', 2, 31), 17);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'd', 5, 31), 17);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'o', 0, 32), 29);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'o', 7, 32), 29);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'p', 0, 32), 30);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'p', 5, 32), 30);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'q', 0, 32), 31);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'q', 5, 32), 31);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), '\u0190', 0, 32), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'x', 0, 32), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'x', 1, 32), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'x', 7, 32), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'd', 0, 32), 18);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'd', 2, 32), 18);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'd', 5, 32), 18);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'o', 0, 34), 31);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'o', 7, 34), 31);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'p', 0, 34), 32);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'p', 5, 34), 32);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'q', 0, 34), 33);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'q', 5, 34), 33);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '\u0190', 0, 34), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'x', 0, 34), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'x', 1, 34), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'x', 7, 34), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'd', 0, 34), 20);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 3, 34), 12);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 4, 34), 12);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 0, 34), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 1, 34), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 2, 34), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '6', 3, 34), 7);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '6', 8, 34), 17);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'd', 2, 34), 20);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'd', 5, 34), 20);
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		} catch (NoSuchFieldException e) {
			throw new RuntimeException(e);
		}
	}
}
