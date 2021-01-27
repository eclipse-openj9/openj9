/*[INCLUDE-IF Sidecar16]*/
package java.lang;

import com.ibm.oti.util.Util;

/*******************************************************************************
 * Copyright (c) 2002, 2021 IBM Corp. and others
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
	/*[IF JAVA_SPEC_VERSION >= 11]*/
	private final String moduleName;
	private final String moduleVersion;
	private final String classLoaderName;
	private transient boolean includeClassLoaderName;
	private transient boolean includeModuleVersion;
	/*[ENDIF] JAVA_SPEC_VERSION >= 11*/
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
	/*[IF JAVA_SPEC_VERSION >= 11]*/
	moduleName = null;
	moduleVersion = null;
	classLoaderName = null;
	/**
	 * includeClassLoaderName and includeModuleVersion are initialized to
	 * true for publicly constructed StackTraceElements, as this information
	 * is to be included when the element is printed.
	 */
	includeClassLoaderName = true;
	includeModuleVersion = true;
	/*[ENDIF] JAVA_SPEC_VERSION >= 11*/
	declaringClass = cls;
	methodName = method;
	fileName = file;
	lineNumber = line;
}

/*[IF JAVA_SPEC_VERSION >= 11]*/
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
	/**
	 * includeClassLoaderName and includeModuleVersion are initialized to
	 * true for publicly constructed StackTraceElements, as this information
	 * is to be included when the element is printed.
	 */
	includeClassLoaderName = true;
	includeModuleVersion = true;
}

/**
 * Returns the name of the ClassLoader used to load the class for the method in the stack frame.  See ClassLoader.getName().
 * @return name of the Classloader or null
 * @since 9
 */
public String getClassLoaderName() {
	return classLoaderName;
}
/*[ENDIF] JAVA_SPEC_VERSION >= 11*/

@SuppressWarnings("unused")
private StackTraceElement() {
	/*[IF JAVA_SPEC_VERSION >= 11]*/
	moduleName = null;
	moduleVersion = null;
	classLoaderName = null;
	/**
	 * includeClassLoaderName and includeModuleVersion are initialized to
	 * false for internally constructed StackTraceElements, as these fields
	 * are to be set natively.
	 */
	includeClassLoaderName = false;
	includeModuleVersion = false;
	/*[ENDIF] JAVA_SPEC_VERSION >= 11*/
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
@Override
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

/*[IF JAVA_SPEC_VERSION >= 11]*/
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

/**
 * Returns whether the classloader name should be included in the stack trace for the provided StackTraceElement.
 *
 * @return true if the classloader name should be included, false otherwise.
 */
boolean getIncludeClassLoaderName() {
	return includeClassLoaderName;
}

/**
 * Returns whether the module version should be included in the stack trace for the provided StackTraceElement.
 *
 * @return true if the module version should be included, false otherwise.
 */
boolean getIncludeModuleVersion() {
	return includeModuleVersion;
}

/**
 * Set the includeClassLoaderName and includeModuleVersion fields for this StackTraceElement.
 *
 * @param classLoader the classloader for the StackTraceElement.
 */
void setIncludeInfoFlags(ClassLoader classLoader) {
	/**
	 * If the classloader is one of the Platform or Bootstrap built-in classloaders,
	 * don't include its name or module version in the stack trace. If it is the
	 * Application/System built-in classloader, don't include the class name, but
	 * include the module version.
	 */
	if ((null == classLoader)
		|| (ClassLoader.getPlatformClassLoader() == classLoader) // VM: Extension ClassLoader
		|| (ClassLoader.bootstrapClassLoader == classLoader) // VM: System ClassLoader
	) {
		includeClassLoaderName = false;
		includeModuleVersion = false;
	} else if ((ClassLoader.getSystemClassLoader() == classLoader)) { // VM: Application ClassLoader
		includeClassLoaderName = false;
	}
}

/**
 * Disable including the classloader name and the module version for this StackTraceElement.
 */
void disableIncludeInfoFlags() {
	includeClassLoaderName = false;
	includeModuleVersion = false;
}
/*[ENDIF] JAVA_SPEC_VERSION >= 11*/

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
@Override
public int hashCode() {
	// either both methodName and declaringClass are null, or neither are null
	if (methodName == null) return 0;	// all unknown methods hash the same
	int hashCode = methodName.hashCode() ^ declaringClass.hashCode();	// declaringClass never null if methodName is non-null
	/*[IF JAVA_SPEC_VERSION >= 11]*/
	if (null != moduleName) {
		hashCode ^= moduleName.hashCode();
	}
	if (null != moduleVersion) {
		hashCode ^= moduleVersion.hashCode();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 11*/
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
@Override
public String toString() {
	StringBuilder buf = new StringBuilder(80);
	boolean includeExtendedInfo = false;
	/*[IF JAVA_SPEC_VERSION >= 11]*/
	/**
	 * includeExtendedInfo is essentially a check to see if this StackTraceElement
	 * was created using a public constructor. Both includeClassLoaderName and
	 * includeModuleVersion are initialized to true in the public constructors,
	 * since these pieces of information are to be included in the stack trace.
	 *
	 * It's also possible that both of these flags are set to true natively,
	 * in which case we still want to include extended info when printing the
	 * stack trace.
	 */
	includeExtendedInfo = includeClassLoaderName && includeModuleVersion;
	/*[ENDIF] JAVA_SPEC_VERSION >= 11*/
	Util.printStackTraceElement(this, source, buf, includeExtendedInfo);
	return buf.toString();
}

}
