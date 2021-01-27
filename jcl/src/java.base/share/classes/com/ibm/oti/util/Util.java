/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
package com.ibm.oti.util;

/*******************************************************************************
 * Copyright (c) 1998, 2021 IBM Corp. and others
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

import java.io.IOException;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.UTFDataFormatException;
import java.io.UnsupportedEncodingException;
import java.net.URL;
import java.security.CodeSource;
import java.security.ProtectionDomain;
import java.util.Optional;
import java.util.Set;

/*[IF JAVA_SPEC_VERSION >= 11]*/
import java.lang.module.ResolvedModule;
import jdk.internal.module.ModuleReferenceImpl;
/*[ENDIF] JAVA_SPEC_VERSION >= 11*/

import com.ibm.oti.vm.VM;
import com.ibm.oti.vm.VMLangAccess;

public final class Util {

	private static final String defaultEncoding;
	private static final VMLangAccess vmLangAccess;

	static {
		vmLangAccess = VM.getVMLangAccess();
		String encoding = vmLangAccess.internalGetProperties().getProperty("os.encoding"); //$NON-NLS-1$
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

/**
 * This class contains a utility method which checks if one class loader is in
 * the ancestry of another.
 * 
 * @param currentLoader classloader whose ancestry is being checked
 * @param requestedLoader classloader who may be an ancestor of currentLoader
 * @return true if currentClassLoader is the same or a child of the requestLoader
 */
public static boolean doesClassLoaderDescendFrom(ClassLoader currentLoader, ClassLoader requestedLoader) {
	if (requestedLoader == null) {
		/* Bootstrap loader is parent of everyone */
		return true;
	}
	if (currentLoader != requestedLoader) {
		while (currentLoader != null) {
			if (currentLoader == requestedLoader) {
				return true;
			}
			currentLoader = currentLoader.getParent();
		}
		return false;
	}
	return true;
}

/**
 * Provide a way of printing text without
 * allocating memory, in particular without String concatenation.
 * Used when printing a stack trace for an OutOfMemoryError.
 * @param buf Buffer to receive string
 * @param s New data for string
 */
public static void appendTo(Appendable buf, CharSequence s) {
	try {
		buf.append(s);
	} catch (java.io.IOException e) {
		/* ignore */
	}
}

@SuppressWarnings("all")
private static final String digitChars[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"}; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$ //$NON-NLS-6$ //$NON-NLS-7$ //$NON-NLS-8$ //$NON-NLS-9$ //$NON-NLS-10$ 
/**
 * Helper method for output with PrintStream and PrintWriter
 * @param buf Buffer to receive string
 * @param number number to be converted to a sequence of chars
 */
public static void appendTo(Appendable buf, int number) {
	int j = 1;
	int i = 0;
	for (i = number; i >= 10; i /= 10) {
		j *= 10;
	}
	Util.appendTo(buf, digitChars[i]);
	while (j >= 10) {
		number -= j * i;
		j /= 10;
		i = number / j;
		appendTo(buf, digitChars[i]);
	}
}

/**
 * Helper method to print text without constructing new String objects
 * @param buf destination for new text
 * @param s text to append
 * @param indents number of tabs to prepend
 */
public static void appendTo(Appendable buf, CharSequence s, int indents) {
	for (int i=0; i<indents; i++) {
		appendTo(buf, "\t"); //$NON-NLS-1$
	}
	appendTo(buf, s);
}

/**
 * Helper method to print text without constructing new String objects
 * @param buf destination for new text
 */
public static void appendLnTo(Appendable buf) {		
	if (buf instanceof PrintStream) {
		((PrintStream)buf).println();
	} else if (buf instanceof PrintWriter) {
		((PrintWriter)buf).println();
	} else {
		appendTo(buf, "\n"); //$NON-NLS-1$
	}
}

/**
 * Print a stack frame without allocating memory directly.
 * @param e StackTraceElement to add
 * @param elementSource location of the source file.  May be null.
 * @param buf Receiver for the text
 * @param includeExtendedInfo Whether or not to include extended info, such as classloader name or module version
 */
public static void printStackTraceElement(StackTraceElement e, Object elementSource, Appendable buf, boolean includeExtendedInfo) {
	/*[IF JAVA_SPEC_VERSION >= 11]*/
	boolean classLoaderNameIncluded = false;
	final String classLoaderName = e.getClassLoaderName();
	/**
	 * The classloader name will only be included in the trace if the classloader isn't a built-in classloader,
	 * or if it is the "app" classloader and the calling class is Throwable/StackFrame.
	 */
	if ((null != classLoaderName)
			&& (!classLoaderName.isEmpty())
			&& (includeExtendedInfo || includeClassLoaderName(e))
	) {
		classLoaderNameIncluded = true;
		appendTo(buf, classLoaderName);
		appendTo(buf, "/"); //$NON-NLS-1$
	}

	final String modName = e.getModuleName();
	if ((null == modName) || (modName.isEmpty())) {
		/* If the classloader name was included but there is no module name, note the empty module in the trace */
		if (classLoaderNameIncluded) {
			appendTo(buf, "/"); //$NON-NLS-1$
		}
	} else {
		/* Append the module name if it exists */
		appendTo(buf, modName);
		if (includeExtendedInfo || includeModuleVersion(e)) {
			String mVer = e.getModuleVersion();
			if ((null != mVer) && (!mVer.isEmpty())) {
				/* Append the module version if it exists */
				appendTo(buf, "@"); //$NON-NLS-1$
				appendTo(buf, mVer);
			}
		}
		appendTo(buf, "/"); //$NON-NLS-1$
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 11*/

	appendTo(buf, e.getClassName());
	appendTo(buf, "."); //$NON-NLS-1$
	appendTo(buf, e.getMethodName());

	appendTo(buf, "("); //$NON-NLS-1$
	if (e.isNativeMethod()) {
		appendTo(buf, "Native Method"); //$NON-NLS-1$
	} else {
		String fileName = e.getFileName();

		if (fileName == null) {
			appendTo(buf, "Unknown Source"); //$NON-NLS-1$
		} else {
			int lineNumber = e.getLineNumber();

			appendTo(buf, fileName);
			if (lineNumber >= 0) {
				appendTo(buf, ":"); //$NON-NLS-1$
				appendTo(buf, lineNumber);
			}
		}
	}
	appendTo(buf, ")"); //$NON-NLS-1$

	/* Support for -verbose:stacktrace */
	if (elementSource != null) {
		appendTo(buf, " from "); //$NON-NLS-1$
		if (elementSource instanceof String) {
			appendTo(buf, (String)elementSource);
		} else if (elementSource instanceof ProtectionDomain) {
			ProtectionDomain pd = (ProtectionDomain)elementSource;
			appendTo(buf, String.valueOf(pd.getClassLoader()));
			CodeSource cs = pd.getCodeSource();
			if (cs != null) {
				URL location = cs.getLocation();
				if (location != null) {
					appendTo(buf, "("); //$NON-NLS-1$
					appendTo(buf, location.toString());
					appendTo(buf, ")"); //$NON-NLS-1$
				}
			}
		}
	}
}

/*[IF JAVA_SPEC_VERSION >= 11]*/
/**
 * Helper method for printStackTraceElement to check if the classloader name
 * should be included in the stack trace for the provided StackTraceElement.
 *
 * @param element The StackTraceElement to check
 * @return true if the classloader name should be included, false otherwise
 */
private static boolean includeClassLoaderName(StackTraceElement element) {
	return vmLangAccess.getIncludeClassLoaderName(element);
}

/**
 * Helper method for printStackTraceElement to check if the module version
 * should be included in the stack trace for the provided StackTraceElement.
 *
 * The module version should not be included for modules that are non-upgradeable.
 *
 * @param element The StackTraceElement to check
 * @return true if the module version should be included, false otherwise
 */
private static boolean includeModuleVersion(StackTraceElement element) {
	boolean includeModuleVersion = vmLangAccess.getIncludeModuleVersion(element);

	if (includeModuleVersion) {
		/* includeModuleVersion should only be set to true if the module is not a non-upgradeable module */
		includeModuleVersion = !NonUpgradeableModules.moduleNames.contains(element.getModuleName());
	}

	return includeModuleVersion;
}

/**
 * NonUpgradeableModules statically determines the set of non-upgradeable modules
 * by checking the module names that are recorded in the hashes of the java.base
 * module. If a module name is contained within this set, then the module is a
 * non-upgradeable module. Non-upgradeable modules should not have their module
 * version displayed in a stack trace if the calling class is Throwable or StackFrame.
 */
private static class NonUpgradeableModules {
	static Set<String> moduleNames;

	static {
		Optional<ResolvedModule> javaBaseModule = ModuleLayer.boot().configuration().findModule("java.base"); //$NON-NLS-1$

		if (javaBaseModule.isPresent()) {
			ResolvedModule resolvedJavaBaseModule = javaBaseModule.get();
			ModuleReferenceImpl javaBaseModuleRef = (ModuleReferenceImpl) resolvedJavaBaseModule.reference();
			moduleNames = javaBaseModuleRef.recordedHashes().names();
		}
	}
}
/*[ENDIF] JAVA_SPEC_VERSION >= 11*/

}
