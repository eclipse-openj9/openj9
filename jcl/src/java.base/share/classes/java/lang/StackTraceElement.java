/*[INCLUDE-IF Sidecar16]*/
package java.lang;

import java.security.ProtectionDomain;

/*******************************************************************************
 * Copyright (c) 2002, 2018 IBM Corp. and others
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

/**
 * StackTraceElement represents a stack frame.
 * 
 * @see Throwable#getStackTrace()
 */
public final class StackTraceElement implements java.io.Serializable {
	private static final long serialVersionUID = 6992337162326171013L;
	/*[IF Sidecar19-SE]*/	
	private final String moduleName;
	private final String moduleVersion;
	private final String classLoaderName;
	/*[ENDIF]*/
	private final String declaringClass;
	private final String methodName;
	private final String fileName;
	private final int lineNumber;
	transient Object source;

/**
 * Create a StackTraceElement from the parameters.
 * 
 * @param cls The class name
 * @param method The method name
 * @param file The file name
 * @param line The line number
 */
public StackTraceElement(String cls, String method, String file, int line) {
	if (cls == null || method == null) throw new NullPointerException();
	/*[IF Sidecar19-SE]*/
	moduleName = null;
	moduleVersion = null;
	classLoaderName = null;
	/*[ENDIF]*/	
	declaringClass = cls;
	methodName = method;
	fileName = file;
	lineNumber = line;
}

/*[IF Sidecar19-SE]*/
/**
 * Create a StackTraceElement from the parameters.
 * 
 * @param module The module name
 * @param version The module version
 * @param cls The class name
 * @param method The method name
 * @param file The file name
 * @param line The line number
 * @since 9
 */
public StackTraceElement(String classLoaderName, String module, String version, String cls, String method, String file, int line) {
	if (cls == null || method == null) throw new NullPointerException();
	moduleName = module;
	moduleVersion = version;
	declaringClass = cls;
	methodName = method;
	fileName = file;
	lineNumber = line;
	this.classLoaderName = classLoaderName;
}

/**
 * Returns the name of the ClassLoader used to load the class for the method in the stack frame.  See ClassLoader.getName().
 * @return name of the Classloader or null
 * @since 9
 */
public String getClassLoaderName() {
	return classLoaderName;
}
/*[ENDIF]*/

@SuppressWarnings("unused")
private StackTraceElement() {
	/*[IF Sidecar19-SE]*/
	moduleName = null;
	moduleVersion = null;
	classLoaderName = null;
	/*[ENDIF]*/	
	declaringClass = null;
	methodName = null;
	fileName = null;
	lineNumber = 0;
} // prevent instantiation from java code - only the VM creates these

/**
 * Returns true if the specified object is another StackTraceElement instance
 * representing the same execution point as this instance.
 * 
 * @param obj the object to compare to
 * 
 */
/*[IF AnnotateOverride]*/
@Override
/*[ENDIF]*/
public boolean equals(Object obj) {
	if (!(obj instanceof StackTraceElement)) return false;
	StackTraceElement castObj = (StackTraceElement) obj;
	
	// Unknown methods are never equal to anything (not strictly to spec, but spec does not allow null method/class names)
	if ((methodName == null) || (castObj.methodName == null)) return false;
	
	if (!getMethodName().equals(castObj.getMethodName())) return false;
	if (!getClassName().equals(castObj.getClassName())) return false;
	String localFileName = getFileName();
	if (localFileName == null) {
		if (castObj.getFileName() != null) return false;
	} else {
		if (!localFileName.equals(castObj.getFileName())) return false;
	}
	if (getLineNumber() != castObj.getLineNumber()) return false;
	
	return true;
}

/*[IF Sidecar19-SE]*/
/**
 * Answers the name of the module to which the execution point represented by this stack trace element belongs.
 * 
 * @return the name of the Module or null if it is not available
 */
public String getModuleName() {
	return moduleName;
}

/**
 * Answers the version of the module to which the execution point represented by this stack trace element belongs.
 * 
 * @return the version of the Module or null if it is not available.
 */
public String getModuleVersion() {
	return moduleVersion;
}
/*[ENDIF]*/
 
/**
 * Returns the full name (i.e. including package) of the class where this
 * stack trace element is executing.
 * 
 * @return the name of the class where this stack trace element is
 *         executing.
 */
public String getClassName() {
	return (declaringClass == null) ? "<unknown class>" : declaringClass; //$NON-NLS-1$
}

/**
 * If available, returns the name of the file containing the Java code
 * source which was compiled into the class where this stack trace element
 * is executing.
 * 
 * @return the name of the Java code source file which was compiled into the
 *         class where this stack trace element is executing. If not
 *         available, a <code>null</code> value is returned.
 */
public String getFileName() {
	return fileName;
}

/**
 * Returns the source file line number for the class where this stack trace
 * element is executing.
 * 
 * @return the line number in the source file corresponding to where this
 *         stack trace element is executing.
 */
public int getLineNumber() {
	/*[PR CMVC 82268] does not return the same value passed into the constructor */
	return lineNumber;
}
 

/**
 * Returns the name of the method where this stack trace element is
 * executing.
 * 
 * @return the method in which this stack trace element is executing.
 *         Returns &lt;<code>unknown method</code>&gt; if the name of the
 *         method cannot be determined.
 */
public String getMethodName() {
	return (methodName == null) ? "<unknown method>" : methodName; //$NON-NLS-1$
}

/**
 * Returns a hash code value for this stack trace element.
 */
/*[IF AnnotateOverride]*/
@Override
/*[ENDIF]*/
public int hashCode() {
	// either both methodName and declaringClass are null, or neither are null
	if (methodName == null) return 0;	// all unknown methods hash the same
	int hashCode = methodName.hashCode() ^ declaringClass.hashCode();	// declaringClass never null if methodName is non-null
	/*[IF Sidecar19-SE]*/	
	if (null != moduleName) {
		hashCode ^= moduleName.hashCode();
	}
	if (null != moduleVersion) {
		hashCode ^= moduleVersion.hashCode();
	}
	/*[ENDIF]*/
	return hashCode;
}
 
/**
 * Returns <code>true</code> if the method name returned by
 * {@link #getMethodName()} is implemented as a native method.
 * 
 * @return true if the method is a native method
 */
public boolean isNativeMethod() {
	return lineNumber == -2;
}
 
/**
 * Returns a string representation of this stack trace element.
 */
/*[IF AnnotateOverride]*/
@Override
/*[ENDIF]*/
public String toString() {
	StringBuilder buf = new StringBuilder(80);

	appendTo(buf);
	return buf.toString();
}

/**
 * Helper method for toString and for Throwable.print output with PrintStream and PrintWriter
 */
void appendTo(Appendable buf) {
	/*[IF Sidecar19-SE]*/
	if (null != moduleName) {
		appendTo(buf, moduleName);
		appendTo(buf, "/"); //$NON-NLS-1$
	}
	/*[ENDIF] Sidecar19-SE*/
	/*[PR CMVC 90361] print "\tat " in Throwable.printStackTrace() for compatibility */
	appendTo(buf, getClassName());
	/*[PR CMVC 121726] Exception traces print '46' instead of '.' */
	appendTo(buf, "."); //$NON-NLS-1$
	appendTo(buf, getMethodName());
  
	appendTo(buf, "("); //$NON-NLS-1$
	if (isNativeMethod()) {
		appendTo(buf, "Native Method"); //$NON-NLS-1$
	} else {
		String fileName = getFileName();

		if (fileName == null) {
			appendTo(buf, "Unknown Source"); //$NON-NLS-1$
		} else {
			int lineNumber = getLineNumber();
			
			appendTo(buf, fileName);
			if (lineNumber >= 0) {
				appendTo(buf, ":"); //$NON-NLS-1$
				appendTo(buf, lineNumber);
			}
		}
	}
	appendTo(buf, ")"); //$NON-NLS-1$
	
	/* Support for -verbose:stacktrace */
	if (source != null) {
		appendTo(buf, " from "); //$NON-NLS-1$
		if (source instanceof String) {
			appendTo(buf, (String)source);			
		} else if (source instanceof ProtectionDomain) {
			ProtectionDomain pd = (ProtectionDomain)source;
			appendTo(buf, pd.getClassLoader().toString());			
			appendTo(buf, "(");			 //$NON-NLS-1$
			appendTo(buf, pd.getCodeSource().getLocation().toString());			
			appendTo(buf, ")");			 //$NON-NLS-1$
		}
	}
}

/* 
 * CMVC 97756 provide a way of printing this stack trace element without
 *            allocating memory, in particular without String concatenation.
 *            Used when printing a stack trace for an OutOfMemoryError.
 */

/**
 * Helper method for output with PrintStream and PrintWriter
 */
static void appendTo(Appendable buf, CharSequence s) {
	try {
		buf.append(s);
	} catch (java.io.IOException e) {
	}
}
@SuppressWarnings("all")
private static final String digits[]={"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"}; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$ //$NON-NLS-6$ //$NON-NLS-7$ //$NON-NLS-8$ //$NON-NLS-9$ //$NON-NLS-10$ 
/**
 * Helper method for output with PrintStream and PrintWriter
 */
static void appendTo(Appendable buf, int number) {
	int i;
	int j = 1;
	for (i = number; i >= 10; i /= 10) {
		j *= 10;
	}
	appendTo(buf, digits[i]);
	while (j >= 10) {
		number -= j * i;
		j /= 10;
		i = number / j;
		appendTo(buf, digits[i]);
	}
}
}
