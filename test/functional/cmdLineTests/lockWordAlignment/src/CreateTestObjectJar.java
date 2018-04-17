/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

import java.lang.IllegalArgumentException;
import java.io.File;

import static org.objectweb.asm.Opcodes.ACC_PRIVATE;

import java.io.FileOutputStream;
import java.io.ByteArrayInputStream;
import java.util.zip.ZipOutputStream;
import java.util.zip.ZipEntry;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.FieldVisitor;
import org.objectweb.asm.ClassReader;

public class CreateTestObjectJar {

	/* Length of buffer used to write JAR file. It can be set to any value */
	private final static int bufLen = 1024;

	private static void printUsage() {
		System.out.println("CreateTestObjectJar Usage:");
		System.out.println();
		System.out.println("CreateTestObjectJar <dir> <testType>");
		System.out.println();
		System.out.println("dir      : Directory where the test object jar file will be written to");
		System.out.println("testType : Must be one of the following: 'i', 'ii', 'iii', or 'd'");
		System.out.println();
	}

	public static void main(String[] args) throws Throwable {

		File testDir = new File(args[0]);
		String testType = args[1];

		String jarFileName = null;

		ClassReader cr = new ClassReader("java/lang/Object");
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		FieldVisitor fv = null;

		cr.accept(cw, 0);

		if (!testDir.isDirectory()) {
			printUsage();
			throw new IllegalArgumentException("Invalid directory: " + testDir.getPath());
		}

		/* Modify the Object class according with the test type */
		if (testType.equalsIgnoreCase("d")) {
			fv = cw.visitField(ACC_PRIVATE, "d", "D", null, null);
			jarFileName = "object_d.jar";
		} else if (testType.equalsIgnoreCase("i")) {
			fv = cw.visitField(ACC_PRIVATE, "i", "I", null, null);
			jarFileName = "object_i.jar";
		} else if (testType.equalsIgnoreCase("ii")) {
			fv = cw.visitField(ACC_PRIVATE, "i",  "I", null, null);
			fv = cw.visitField(ACC_PRIVATE, "ii", "I", null, null);
			jarFileName = "object_ii.jar";
		} else if (testType.equalsIgnoreCase("iii")) {
			fv = cw.visitField(ACC_PRIVATE, "i",   "I", null, null);
			fv = cw.visitField(ACC_PRIVATE, "ii",  "I", null, null);
			fv = cw.visitField(ACC_PRIVATE, "iii", "I", null, null);
			jarFileName = "object_iii.jar";
		} else {
			printUsage();
			throw new IllegalArgumentException("Invalid test type: " + testType);
		}

		if (null != fv) {
			fv.visitEnd();
		}

		/* Write the final jar file with the modified Object class in it */
		if (null != jarFileName) {
			ZipOutputStream outJar = new ZipOutputStream(new FileOutputStream(testDir.getPath() + "/" + jarFileName));
			outJar.putNextEntry(new ZipEntry("java/lang/Object.class"));
			ByteArrayInputStream inJar = new ByteArrayInputStream(cw.toByteArray());
			int len;
			byte[] buf = new byte[bufLen];
			while ((len = inJar.read(buf)) > 0) {
				outJar.write(buf, 0, len);
			}
			outJar.closeEntry();
			outJar.close();
		}

	}

}

