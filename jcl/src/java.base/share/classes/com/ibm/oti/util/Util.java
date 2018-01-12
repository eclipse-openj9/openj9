/*[INCLUDE-IF Sidecar16]*/
package com.ibm.oti.util;

/*******************************************************************************
 * Copyright (c) 1998, 2010 IBM Corp. and others
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

import java.io.UTFDataFormatException;
import java.io.UnsupportedEncodingException;

public final class Util {

	private static final String defaultEncoding;

	static {
		String encoding = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties().getProperty("os.encoding"); //$NON-NLS-1$
		if (encoding != null) {
			try {
				"".getBytes(encoding); //$NON-NLS-1$
			} catch (java.io.UnsupportedEncodingException e) {
				encoding = null;
			}
		}
		defaultEncoding = encoding;
	}

public static byte[] getBytes(String name) {
	if (defaultEncoding != null) {
		try {
			return name.getBytes(defaultEncoding);
		} catch (java.io.UnsupportedEncodingException e) {}
	}
	return name.getBytes();
}

public static String toString(byte[] bytes) {
	if (defaultEncoding != null) {
		try {
			return new String(bytes, 0, bytes.length, defaultEncoding);
		} catch (java.io.UnsupportedEncodingException e) {}
	}
	return new String(bytes, 0, bytes.length);
}

public static String convertFromUTF8(byte[] buf, int offset, int utfSize) throws UTFDataFormatException {
	return convertUTF8WithBuf(buf, new char[utfSize], offset, utfSize);
}

public static String convertUTF8WithBuf(byte[] buf, char[] out, int offset, int utfSize) throws UTFDataFormatException {
	int count = 0, s = 0, a;
	while (count < utfSize) {
		if ((out[s] = (char)buf[offset + count++]) < '\u0080') s++;
		else if (((a = out[s]) & 0xe0) == 0xc0) {
			if (count >= utfSize)
/*[MSG "K0062", "Second byte at {0} does not match UTF8 Specification"]*/
				throw new UTFDataFormatException(com.ibm.oti.util.Msg.getString("K0062", count)); //$NON-NLS-1$
			int b = buf[count++];
			if ((b & 0xC0) != 0x80)
/*[MSG "K0062", "Second byte at {0} does not match UTF8 Specification"]*/
				throw new UTFDataFormatException(com.ibm.oti.util.Msg.getString("K0062", (count-1))); //$NON-NLS-1$
			out[s++] = (char) (((a & 0x1F) << 6) | (b & 0x3F));
		} else if ((a & 0xf0) == 0xe0) {
			if (count+1 >= utfSize)
/*[MSG "K0063", "Third byte at {0} does not match UTF8 Specification"]*/
				throw new UTFDataFormatException(com.ibm.oti.util.Msg.getString("K0063", (count+1))); //$NON-NLS-1$
			int b = buf[count++];
			int c = buf[count++];
			if (((b & 0xC0) != 0x80) || ((c & 0xC0) != 0x80))
/*[MSG "K0064", "Second or third byte at {0} does not match UTF8 Specification"]*/
				throw new UTFDataFormatException(com.ibm.oti.util.Msg.getString("K0064", (count-2))); //$NON-NLS-1$
			out[s++] = (char) (((a & 0x0F) << 12) | ((b & 0x3F) << 6) | (c & 0x3F));
		} else {
/*[MSG "K0065", "Input at {0} does not match UTF8 Specification"]*/
			throw new UTFDataFormatException(com.ibm.oti.util.Msg.getString("K0065", (count-1))); //$NON-NLS-1$
		}
	}
	return new String(out, 0, s);
}

/**
 * This class contains a utility method for converting a string to the format
 * required by the <code>application/x-www-form-urlencoded</code> MIME content type.
 * <p>
 * All characters except letters ('a'..'z', 'A'..'Z') and numbers ('0'..'9') 
 * and special characters are converted into their hexidecimal value prepended
 * by '%'.
 * <p>
 * For example: '#' -> %23
 * <p>
 *		
 * @return the string to be converted, will be identical if no characters are
 * converted
 * @param s		the converted string
 */
public static String urlEncode(String s) {
	boolean modified = false;
	StringBuilder buf = new StringBuilder();
	int start = -1;
	for (int i = 0; i < s.length(); i++) {
		char ch = s.charAt(i);
		if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
			(ch >= '0' && ch <= '9') || "_-!.~'()*,:$&+/@".indexOf(ch) > -1) //$NON-NLS-1$
		{
			if (start >= 0) {
				modified = true;
				/*[PR 117329] must convert groups of chars (Foundation 1.1 TCK) */
				convert(s.substring(start, i), buf);
				start = -1;
			}
			buf.append(ch);
		} else {
			if (start < 0) {
				start = i;
			}
		}
	}
	if (start >= 0) {
		modified = true;
		convert(s.substring(start, s.length()), buf);
	}
	return modified ? buf.toString() : s;		
}

private static void convert(String s, StringBuilder buf) {
	final String digits = "0123456789abcdef"; //$NON-NLS-1$
	byte[] bytes;
	try {
		bytes = s.getBytes("UTF8"); //$NON-NLS-1$
	} catch (UnsupportedEncodingException e) {
		// should never occur since UTF8 is required
		bytes = s.getBytes();
	}
	for (int j=0; j<bytes.length; j++) {
		buf.append('%');
		buf.append(digits.charAt((bytes[j] & 0xf0) >> 4));
		buf.append(digits.charAt(bytes[j] & 0xf));
	}
}

public static boolean startsWithDriveLetter(String path){
	// returns true if the string starts with <letter>:, example, d:
	if (!(path.charAt(1) == ':')) {
		return false;
	}
	char c = path.charAt(0);
	c = Character.toLowerCase( c );
	if (c >= 'a' && c <= 'z') {
		return true;
	}
	return false;
}

/*[PR 134399] jar:file: URLs should get canonicalized */
public static String canonicalizePath(String file) {
	int dirIndex;
	while ((dirIndex = file.indexOf("/./")) >= 0) //$NON-NLS-1$
		file = file.substring(0, dirIndex + 1) + file.substring(dirIndex + 3);
	if (file.endsWith("/.")) file = file.substring(0, file.length() - 1); //$NON-NLS-1$
	while ((dirIndex = file.indexOf("/../")) >= 0) { //$NON-NLS-1$
		if (dirIndex != 0) {
			file = file.substring(0, file.lastIndexOf('/', dirIndex - 1)) + file.substring(dirIndex + 3);
		} else
			file = file.substring(dirIndex + 3);
	}
	if (file.endsWith("/..") && file.length() > 3) //$NON-NLS-1$
		file = file.substring(0, file.lastIndexOf('/', file.length() - 4) + 1);
	
	return file;
}
}
