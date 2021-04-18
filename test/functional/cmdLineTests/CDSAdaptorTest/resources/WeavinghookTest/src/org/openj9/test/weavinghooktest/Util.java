/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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

package org.openj9.test.weavinghooktest;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.net.URL;

import org.osgi.framework.Bundle;

/**
 * A utility class used by Weaving Hook to redefine class bytes of a given class
 */
public class Util {
	
	/*
	 * Returns class bytes of a specified class present in the specified bundle.
	 */
	public static byte[] getClassBytes(Bundle bundle, String className) {
		byte classBytes[];
		
		try {
			String qualifiedClassName = className.replace('.', '/') + ".class";
			URL url = bundle.getEntry(qualifiedClassName);
			if (url != null) {
				InputStream in = url.openStream();
				ByteArrayOutputStream out = new ByteArrayOutputStream();
				int b;
				while ((b = in.read()) != -1) {
					out.write(b);
				}
				in.close();
				classBytes = out.toByteArray();
			} else {
				classBytes = null;
			}
		} catch (IOException e) {			
			e.printStackTrace();
			return null;
		}
		
		return classBytes;
	}
	
	// returns the index >= offset of the first occurrence of sequence in buffer
	public static int indexOf(byte[] buffer, int offset, byte[] sequence)
	{
		while (buffer.length - offset >= sequence.length) {
			for (int i = 0; i < sequence.length; i++) {
				if (buffer[offset + i] != sequence[i]) {
					break;
				}
				if (i == sequence.length - 1) {
					return offset;
				}
			}
			offset++;
		}
		return -1;
	}

	// replaces all occurrences of seq1 in original buffer with seq2
	public static byte[] replaceAll(byte[] buffer, byte[] seq1, byte[] seq2) {
		ByteArrayOutputStream out = new ByteArrayOutputStream();
		int index, offset = 0;
		while ((index = indexOf(buffer, offset, seq1)) != -1) {
			if (index - offset != 0) {
				out.write(buffer, offset, index - offset);
			}
			out.write(seq2, 0, seq2.length);
			offset = index + seq1.length;
		}
		if (offset < buffer.length) {
			out.write(buffer, offset, buffer.length - offset);
		}
		return out.toByteArray();
	}
	
	public static byte[] replaceClassNames(byte[] classBytes, String originalClassName, String redefinedClassName) {
		// replace all occurrences of the redefined class name with the original class name
		try {
			byte[] r = redefinedClassName.getBytes("UTF-8");
			byte[] o = originalClassName.getBytes("UTF-8");
			// since this is a naive search and replace that doesn't
			// take into account the structure of the class file, and
			// hence would not update length fields, the two names must
			// be of the same length
			if (r.length != o.length) {
				return null;
			}
			classBytes = Util.replaceAll(classBytes, r, o);
		} catch (UnsupportedEncodingException e) {
			e.printStackTrace();
			return null;
		}
		return classBytes;
	}
}
