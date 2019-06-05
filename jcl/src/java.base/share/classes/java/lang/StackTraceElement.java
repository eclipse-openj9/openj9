/*[INCLUDE-IF Sidecar16]*/
package java.lang;

import com.ibm.oti.util.Util;

/*******************************************************************************
 * Copyright (c) 2002, 2019 IBM Corp. and others
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
 * @param classLoaderName The name for the ClassLoader
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
	Util.printStackTraceElement(this, source, buf, false);
	return buf.toString();
}

}
