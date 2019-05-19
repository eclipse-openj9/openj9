/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.HashMap;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.java.diagnostics.utils.IDTFJContext;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand.ArtifactType;

/**
 * Class which attempts to handle exceptions thrown by the DTFJ API in a consistent manner.
 * It is context aware and knows which DTFJ calls will not be possible when analyzing certain 
 * artifacts. e.g. it understands that thread information is not available from a PHD file.
 * 
 * @author adam
 *
 */
public class ExceptionHandler {
	static final int UNKNOWN = 0;
	private static final int WORKS = 1;
	private static final int CAN_FAIL = 2;
	private static final int FAILS = 3;
	
	private static HashMap<String, DTFJClass> methodTable = new HashMap<String, DTFJClass>();
	private static IDTFJContext ctx;		//current DTFJ context		
	private static final Logger logger = Logger.getLogger("com.ibm.jvm.dtfjview.logger.command");
	
	static {
		init();
	}
	
	
	/**
	 * Handle an exception thrown by the DTFJ API.
	 * 
	 * @param cmd the command current executing the API. This is required to provide access to context information.
	 * @param cause exception to be handled
	 * @return textual data to be written out
	 */
	public static String handleException(BaseJdmpviewCommand cmd, Throwable cause) {
		ctx = cmd.ctx;
		StringBuffer sb = new StringBuffer();
		// Try and determine the artifact type
		StackTraceElement[] myStack = cause.getStackTrace();
		ArtifactType artifactType = cmd.getArtifactType();
		String artifactTypeName = null;
		switch (artifactType) {
			case core:
				artifactTypeName = "core";
				break;
			case javacore:
				artifactTypeName = "javacore";
				break;
			case phd:
				artifactTypeName = "phd";
				break;
			default:
				artifactTypeName = "Unknown artifact type";
				break;
		}

		// Let's find out which DTFJ Class/Method was being used
		DTFJMethod dMethod = getDTFJMethod(myStack, cmd);
		if (cause instanceof DataUnavailable) {
			if (dMethod == null) {
				sb.append("Could not determine DTFJ class/method that caused this exception: ");
				sb.append(cause.getLocalizedMessage());
			} else if (dMethod.isSupported(artifactType) == WORKS) {
				sb.append(dMethod.getClassName());
				sb.append(".");
				sb.append(dMethod.getMethodname());
				sb.append(" should have worked for a ");
				sb.append(artifactTypeName);
				sb.append(" but returned: ");
				sb.append(cause.getLocalizedMessage());
			} else if (dMethod.isSupported(artifactType) == CAN_FAIL) {
				sb.append(dMethod.getClassName());
				sb.append(".");
				sb.append(dMethod.getMethodname());
				sb.append(" can fail for a ");
				sb.append(artifactTypeName);
				sb.append(" which is why it returned: ");
				sb.append(cause.getLocalizedMessage());
			} else if (dMethod.isSupported(artifactType) == FAILS) {
				sb.append(dMethod.getClassName());
				sb.append(".");
				sb.append(dMethod.getMethodname());
				sb.append(" is not supported for a ");
				sb.append(artifactTypeName);
				String s = cause.getLocalizedMessage();
				if (s != null) {
					sb.append(" causing: ");
					sb.append(s);
				}
			}

		} else if (cause instanceof CorruptDataException) {
			CorruptData corruptData = ((CorruptDataException) cause)
					.getCorruptData();
			ImagePointer ip = corruptData.getAddress();
			if (ip == null) {
				sb.append("CorruptData found in dump at unknown address executing ");
				if (dMethod == null) {
					sb.append("an unknown class/method that caused this exception: ");
					sb.append(cause.getLocalizedMessage());
				} else {
					sb.append(dMethod.getClassName());
					sb.append(".");
					sb.append(dMethod.getMethodname());
				}
			} else {
				String addr = cmd.toHexStringAddr(ip.getAddress());
				sb.append("CorruptData found in dump at address: ");
				sb.append(addr);
				sb.append(" causing: ");
				sb.append(cause.getLocalizedMessage());
				if (dMethod != null) {
					sb.append(" executing ");
					sb.append(dMethod.getClassName());
					sb.append(".");
					sb.append(dMethod.getMethodname());
				}
			}
		} else if (cause instanceof MemoryAccessException) {
			sb.append(cause.getLocalizedMessage());
		} else if (cause instanceof IllegalArgumentException) {
			sb.append(cause.getLocalizedMessage());
		} else {
			// If we are here then something bad has really happened
			// This is just debug code to help determine other problems this
			// method has to deal with
			sb.append("==========> Unexpected exception "
					+ cause.getClass().getName() + " thrown: "
					+ cause.getLocalizedMessage());
			StringWriter st = new StringWriter();
			cause.printStackTrace(new PrintWriter(st));
			sb.append(st.toString());
		}
		return sb.toString();
	}
	
	/**
	 * Get the DTFJ method which caused the exception by examining the call stack for the
	 * exception.
	 * 
	 * @param myStack stack from an exception
	 * @return the DTFJ method which caused the exception or null if it could not determined
	 */
	private static DTFJMethod getDTFJMethod(StackTraceElement[] myStack, BaseJdmpviewCommand cmd) {
		// Only search through the top 3 items on the stack ...
		// In reality it will normally be the first item on the stack
		for (int s = 0; s < 3; s++) {

			String className = myStack[s].getClassName();
			String methodName = myStack[s].getMethodName();
			// System.out.println(DEBUG_PREFIX + "Getting DTFJMethod for: "
			// + className + "." + methodName);

			try {
				Class<?> c = Class.forName(className, true, cmd.getClass().getClassLoader());
				do {
					// System.out.println(DEBUG_PREFIX + "className: "
					// + c.getName());
					DTFJMethod method = checkInterfaces(c, methodName);
					if (method != null) {
						return method;
					}
				} while ((c = c.getSuperclass()) != null);

			} catch (Exception e) {
				logger.log(Level.FINE, "Error examining stack trace to determine failing method.", e);
				//fall through and allow it to examin the next stack frame
			}
		}
		return null;
	}

	/**
	 * Find the DTFJ interface for a specified class
	 * 
	 * @param c class to check
	 * @param methodName method name to find in the DTFJ interface
	 * @return the located method or null if no match is found
	 */
	private static DTFJMethod checkInterfaces(Class<?> c, String methodName) {
		Class<?>[] interfaces = c.getInterfaces();
		// System.out.println(DEBUG_PREFIX + "Num interfaces: "
		// + interfaces.length);

		// Go through the interfaces of this class and determine
		// which
		// DTFJ interface it is
		for (int i = 0; i < interfaces.length; i++) {
			String interfaceName = interfaces[i].getName();
			int index = interfaceName.lastIndexOf('.');
			String name = interfaceName.substring(++index);
			// System.out.println(DEBUG_PREFIX + "Interface found: "
			// + interfaceName + " " + name);
			Object oc = methodTable.get(name);
			DTFJMethod om;
			if (oc != null) {
				DTFJClass classDefinition = (DTFJClass) oc;
				om = classDefinition.getMethod(methodName);
				if (om != null) {
					return om;
				}
			}
			Class<?> ic;
			try {
				//now check any possible super-interfaces for this one
				ic = Class.forName(interfaceName, true, ctx.getAddressSpace()
						.getClass().getClassLoader());
				om = checkInterfaces(ic, methodName);
				if (om != null) {
					return om;
				}
			} catch (ClassNotFoundException e) {
				logger.log(Level.FINE, "Error checking interfaces.", e);
				//fall through and allow it to check the next interface
			}
		}
		return null;
	}

	/*
	 * The methods below which are commented out have been left in for reference but
	 * as they throw neither DataUnavailable or CorruptData are not handled.
	 */
	private static void init() {

		DTFJClass c1 = new DTFJClass("JavaClass");
		// c1.addMethod("equals", WORKS, WORKS, WORKS);
		c1.addMethod("getClassLoader", WORKS, WORKS, WORKS);
		c1.addMethod("getComponentType", WORKS, FAILS, WORKS);
		c1.addMethod("getConstantPoolReference", WORKS, FAILS, WORKS);
		c1.addMethod("getDeclaredFields", WORKS, FAILS, WORKS);
		c1.addMethod("getDeclaredMethods", WORKS, FAILS, WORKS);
		// c1.addMethod("getID", WORKS, WORKS, WORKS);
		c1.addMethod("getInterfaces", WORKS, FAILS, WORKS);
		c1.addMethod("getModifiers", WORKS, FAILS, WORKS);
		c1.addMethod("getName", WORKS, WORKS, WORKS);
		c1.addMethod("getObject", WORKS, WORKS, WORKS);
		c1.addMethod("getReferences", WORKS, FAILS, WORKS);
		c1.addMethod("getSuperclass", WORKS, FAILS, WORKS);
		// c1.addMethod("hashCode", WORKS, WORKS, WORKS);
		// c1.addMethod("isArray", WORKS, WORKS, WORKS);
		methodTable.put("JavaClass", c1);

		DTFJClass c2 = new DTFJClass("JavaClassLoader");
		// c2.addMethod("equals", WORKS, WORKS, WORKS);
		c2.addMethod("findClass", WORKS, WORKS, WORKS);
		c2.addMethod("getCachedClasses", WORKS, WORKS, WORKS);
		c2.addMethod("getDefinedClasses", WORKS, WORKS, WORKS);
		c2.addMethod("getObject", WORKS, WORKS, WORKS);
		// c2.addMethod("hashcode", WORKS, WORKS, WORKS);
		methodTable.put("JavaClassLoader", c2);

		DTFJClass c3 = new DTFJClass("JavaField");
		// c3.addMethod("equals", WORKS, WORKS, WORKS);
		c3.addMethod("get", WORKS, FAILS, FAILS);
		c3.addMethod("getBoolean", WORKS, FAILS, FAILS);
		c3.addMethod("getByte", WORKS, FAILS, FAILS);
		c3.addMethod("getChar", WORKS, FAILS, FAILS);
		c3.addMethod("getDouble", WORKS, FAILS, FAILS);
		c3.addMethod("getFloat", WORKS, FAILS, FAILS);
		c3.addMethod("getInt", WORKS, FAILS, FAILS);
		c3.addMethod("getLong", WORKS, FAILS, FAILS);
		c3.addMethod("getShort", WORKS, FAILS, FAILS);
		c3.addMethod("getString", WORKS, FAILS, FAILS);
		// c3.addMethod("hashcode", WORKS, WORKS, WORKS);
		methodTable.put("JavaField", c3);

		DTFJClass c4 = new DTFJClass("JavaHeap");
		// c4.addMethod("equals", WORKS, WORKS, WORKS);
		c3.addMethod("getName", WORKS, FAILS, WORKS);
		c3.addMethod("getObject", WORKS, FAILS, WORKS);
		c3.addMethod("getSections", WORKS, WORKS, WORKS);
		// c4.addMethod("hashcode", WORKS, WORKS, WORKS);
		methodTable.put("JavaHeap", c4);

		DTFJClass c5 = new DTFJClass("JavaLocation");
		// c5.addMethod("equals", WORKS, WORKS, WORKS);
		c5.addMethod("getAddress", WORKS, FAILS, FAILS);
		c5.addMethod("getCompilationLevel", WORKS, FAILS, FAILS);
		c5.addMethod("getFilename", WORKS, FAILS, FAILS);
		c5.addMethod("getLineNumber", CAN_FAIL, FAILS, FAILS);
		c5.addMethod("getMethod", WORKS, FAILS, FAILS);
		// c5.addMethod("hashcode", WORKS, WORKS, WORKS);
		// c5.addMethod("toString", WORKS, WORKS, WORKS);
		methodTable.put("JavaLocation", c5);

		DTFJClass c6 = new DTFJClass("JavaMember");
		// c6.addMethod("equals", WORKS, WORKS, WORKS);
		c6.addMethod("getDeclaringClass", WORKS, FAILS, FAILS);
		c6.addMethod("getModifiers", WORKS, FAILS, FAILS);
		c6.addMethod("getName", WORKS, FAILS, FAILS);
		c6.addMethod("getSignature", WORKS, FAILS, FAILS);
		// c6.addMethod("hashcode", WORKS, WORKS, WORKS);
		methodTable.put("JavaMember", c6);

		DTFJClass c7 = new DTFJClass("JavaMethod");
		// c7.addMethod("equals", WORKS, WORKS, WORKS);
		c7.addMethod("getBytecodeSections", WORKS, FAILS, FAILS);
		c7.addMethod("getCompiledSections", WORKS, FAILS, FAILS);
		// c7.addMethod("hashcode", WORKS, WORKS, WORKS);
		methodTable.put("JavaMethod", c7);

		DTFJClass c8 = new DTFJClass("JavaMonitor");
		// c8.addMethod("equals", WORKS, WORKS, WORKS);
		c8.addMethod("getEnterWaiters", WORKS, WORKS, FAILS);
		// c8.addMethod("getID", WORKS, WORKS, WORKS);
		c8.addMethod("getName", WORKS, WORKS, FAILS);
		c8.addMethod("getNotifyWaiters", WORKS, WORKS, FAILS);
		// c8.addMethod("getObject", WORKS, WORKS, WORKS);
		c8.addMethod("getOwner", WORKS, WORKS, FAILS);
		// c8.addMethod("hashcode", WORKS, WORKS, WORKS);
		methodTable.put("JavaMonitor", c8);

		DTFJClass c9 = new DTFJClass("JavaObject");
		c9.addMethod("arrayCopy", WORKS, FAILS, WORKS);
		// c9.addMethod("equals", WORKS, WORKS, WORKS);
		c9.addMethod("getArraySize", WORKS, FAILS, WORKS);
		c9.addMethod("getHashcode", WORKS, FAILS, WORKS);
		c9.addMethod("getHeap", WORKS, FAILS, WORKS);
		// c9.addMethod("getID", WORKS, WORKS, WORKS);
		c9.addMethod("getJavaClass", WORKS, FAILS, WORKS);
		c9.addMethod("getPersistentHashcode", WORKS, FAILS, WORKS);
		c9.addMethod("getReferences", WORKS, FAILS, WORKS);
		c9.addMethod("getSections", WORKS, FAILS, WORKS);
		c9.addMethod("getSize", WORKS, FAILS, WORKS);
		// c9.addMethod("hashcode", WORKS, WORKS, WORKS);
		c9.addMethod("isArray", WORKS, FAILS, WORKS);
		methodTable.put("JavaObject", c9);

		DTFJClass c10 = new DTFJClass("JavaReference");
		// c10.addMethod("getDescription", WORKS, WORKS, WORKS);
		c10.addMethod("getReachability", WORKS, FAILS, WORKS);
		c10.addMethod("getReferenceType", WORKS, FAILS, WORKS);
		c10.addMethod("getRootType", WORKS, FAILS, WORKS);
		c10.addMethod("getSource", WORKS, FAILS, WORKS);
		c10.addMethod("getTarget", WORKS, FAILS, WORKS);
		c10.addMethod("isClassReference", WORKS, FAILS, WORKS);
		c10.addMethod("isObjectReference", WORKS, FAILS, WORKS);
		methodTable.put("JavaReference", c10);

		DTFJClass c11 = new DTFJClass("JavaRuntime");
		// c11.addMethod("equals", WORKS, WORKS, WORKS);
		// c11.addMethod("getCompiledMethods", WORKS, WORKS, WORKS);
		// c11.addMethod("getHeapRoots", WORKS, WORKS, WORKS);
		// c11.addMethod("getHeaps", WORKS, WORKS, WORKS);
		// c11.addMethod("getJavaClassLoaders", WORKS, WORKS, WORKS);
		c11.addMethod("getJavaVM", WORKS, FAILS, WORKS);
		c11.addMethod("getJavaVMInitArgs", WORKS, WORKS, FAILS);
		// c11.addMethod("getMonitors", WORKS, WORKS, WORKS);
		c11.addMethod("getObjectAtAddress", WORKS, FAILS, WORKS);
		// c11.addMethod("getThreads", WORKS, WORKS, WORKS);
		c11.addMethod("getTraceBuffer", FAILS, FAILS, FAILS);
		// c11.addMethod("hashcode", WORKS, WORKS, WORKS);
		methodTable.put("JavaRuntime", c11);

		DTFJClass c12 = new DTFJClass("JavaStackFrame");
		// c12.addMethod("equals", WORKS, WORKS, WORKS);
		c12.addMethod("getBasePointer", WORKS, WORKS, FAILS);
		// c12.addMethod("getHeapRoots", WORKS, WORKS, WORKS);
		c12.addMethod("getLocation", WORKS, WORKS, FAILS);
		// c12.addMethod("hashcode", WORKS, WORKS, WORKS);
		methodTable.put("JavaStackFrame", c12);

		DTFJClass c13 = new DTFJClass("JavaThread");
		// c13.addMethod("equals", WORKS, WORKS, WORKS);
		c13.addMethod("getImageThread", WORKS, WORKS, FAILS);
		c13.addMethod("getJNIEnv", WORKS, WORKS, FAILS);
		c13.addMethod("getName", WORKS, WORKS, FAILS);
		c13.addMethod("getObject", WORKS, WORKS, FAILS);
		c13.addMethod("getPriority", WORKS, WORKS, FAILS);
		// c13.addMethod("getStackFrames", WORKS, WORKS, WORKS);
		// c13.addMethod("getStackSections", WORKS, WORKS, WORKS);
		c13.addMethod("getState", WORKS, WORKS, FAILS);
		// c13.addMethod("hashcode", WORKS, WORKS, WORKS);
		methodTable.put("JavaThread", c13);

		DTFJClass c14 = new DTFJClass("JavaVMInitArgs");
		c14.addMethod("getIgnoreUnrecognized", WORKS, WORKS, FAILS);
		c14.addMethod("getOptions", WORKS, WORKS, FAILS);
		c14.addMethod("getVersion", WORKS, FAILS, FAILS);
		methodTable.put("JavaVMInitArgs", c14);

		DTFJClass c15 = new DTFJClass("JavaVMOptions");
		c15.addMethod("getExtraInfo", WORKS, WORKS, FAILS);
		c15.addMethod("getOptionString", WORKS, WORKS, FAILS);
		methodTable.put("JavaVMOptions", c15);

		DTFJClass c16 = new DTFJClass("Image");
		// c16.addMethod("getAddressSpaces", WORKS, WORKS, WORKS);
		c16.addMethod("getCreationTime", CAN_FAIL, WORKS, WORKS);
		c16.addMethod("getHostName", WORKS, FAILS, FAILS);
		c16.addMethod("getInstalledMemory", WORKS, FAILS, FAILS);
		c16.addMethod("getIPAddresses", WORKS, WORKS, FAILS);
		c16.addMethod("getProcessorCount", WORKS, WORKS, FAILS);
		c16.addMethod("getProcessorSubType", WORKS, FAILS, FAILS);
		c16.addMethod("getProcessorType", WORKS, FAILS, FAILS);
		c16.addMethod("getSystemSubType", WORKS, WORKS, FAILS);
		c16.addMethod("getSystemType", WORKS, WORKS, FAILS);
		methodTable.put("Image", c16);

		DTFJClass c17 = new DTFJClass("ImageModule");
		c17.addMethod("getName", WORKS, WORKS, FAILS);
		c17.addMethod("getProperties", WORKS, WORKS, FAILS);
		c17.addMethod("getLoadAddress", WORKS, FAILS, FAILS);
		c17.addMethod("getExecutable", WORKS, FAILS, FAILS);
		// c17.addMethod("getSections", WORKS, WORKS, WORKS);
		// c17.addMethod("getSymbols", WORKS, WORKS, WORKS);
		methodTable.put("ImageModule", c17);

		DTFJClass c18 = new DTFJClass("ImagePointer");
		// c18.addMethod("add", WORKS, WORKS, WORKS);
		// c18.addMethod("equals", WORKS, WORKS, WORKS);
		// c18.addMethod("getAddress", WORKS, WORKS, WORKS);
		// c18.addMethod("getAddressSpace", WORKS, WORKS, WORKS);
		c18.addMethod("getByteAt", WORKS, FAILS, FAILS);
		c18.addMethod("getDoubleAt", WORKS, FAILS, FAILS);
		c18.addMethod("getFloatAt", WORKS, FAILS, FAILS);
		c18.addMethod("getIntAt", WORKS, FAILS, FAILS);
		c18.addMethod("getLongAt", WORKS, FAILS, FAILS);
		c18.addMethod("getPointerAt", WORKS, FAILS, FAILS);
		c18.addMethod("getShortAt", WORKS, FAILS, FAILS);
		// c18.addMethod("hashCode", WORKS, WORKS, WORKS);
		c18.addMethod("isExecutable", WORKS, FAILS, FAILS);
		c18.addMethod("isReadOnly", WORKS, FAILS, FAILS);
		c18.addMethod("isShared", WORKS, FAILS, FAILS);
		methodTable.put("ImagePointer", c18);

		DTFJClass c19 = new DTFJClass("ImageProcess");
		c19.addMethod("getCommandLine", WORKS, WORKS, FAILS);
		c19.addMethod("getCurrentThread", WORKS, FAILS, FAILS);
		c19.addMethod("getEnvironment", WORKS, WORKS, FAILS);
		c19.addMethod("getExecutable", WORKS, FAILS, FAILS);
		c19.addMethod("getID", WORKS, FAILS, FAILS);
		c19.addMethod("getLibraries", WORKS, FAILS, FAILS);
		// c19.addMethod("getPointerSize", WORKS, WORKS, WORKS);
		// c19.addMethod("getRuntimes", WORKS, WORKS, WORKS);
		c19.addMethod("getSignalName", WORKS, WORKS, FAILS);
		c19.addMethod("getSignalNumber", WORKS, WORKS, FAILS);
		// c19.addMethod("getThreads", WORKS, WORKS, WORKS);
		methodTable.put("ImageProcess", c19);

		DTFJClass c20 = new DTFJClass("ImageRegister");
		// c20.addMethod("getName", WORKS, WORKS, WORKS);
		c20.addMethod("getValue", WORKS, FAILS, FAILS);
		methodTable.put("ImageRegister", c20);

		DTFJClass c21 = new DTFJClass("ImageSection");
		// c21.addMethod("getBaseAddress", WORKS, WORKS, WORKS);
		// c21.addMethod("getName", WORKS, WORKS, WORKS);
		// c21.addMethod("getSize", WORKS, WORKS, WORKS);
		c21.addMethod("isExecutable", WORKS, FAILS, FAILS);
		c21.addMethod("isReadOnly", WORKS, FAILS, FAILS);
		c21.addMethod("isShared", WORKS, FAILS, FAILS);
		methodTable.put("ImageSection", c21);

		DTFJClass c22 = new DTFJClass("ImageStackFrame");
		c22.addMethod("getBasePointer", WORKS, WORKS, FAILS);
		c22.addMethod("getProcedureAddress", WORKS, WORKS, FAILS);
		c22.addMethod("getProcedureName", WORKS, WORKS, FAILS);
		methodTable.put("ImageStackFrame", c22);

		DTFJClass c23 = new DTFJClass("ImageThread");
		c23.addMethod("getID", WORKS, WORKS, FAILS);
		// c23.addMethod("getProperties", WORKS, WORKS, WORKS);
		// c23.addMethod("getRegisters", WORKS, WORKS, WORKS);
		c23.addMethod("getStackFrames", WORKS, WORKS, FAILS);
		// c23.addMethod("getStackSections", WORKS, WORKS, WORKS);
		methodTable.put("ImageThread", c23);

	}

}

class DTFJClass {

	private String className;
	private HashMap<String, DTFJMethod> methods = new HashMap<String, DTFJMethod>();

	DTFJClass(String name) {
		this.className = name;
	}

	void addMethod(String mName, int core, int javacore, int phd) {
		DTFJMethod method = new DTFJMethod(className, mName, core, javacore,
				phd);
		methods.put(mName, method);
	}

	DTFJMethod getMethod(String methodName) {
		return (DTFJMethod) methods.get(methodName);
	}
}

class DTFJMethod {

	private String className;
	private String methodName;
	private int coreSupported;
	private int javacoreSupported;
	private int phdSupported;

	DTFJMethod(String cName, String mName, int core, int javacore, int phd) {
		this.className = cName;
		this.methodName = mName;
		this.coreSupported = core;
		this.javacoreSupported = javacore;
		this.phdSupported = phd;
	}

	int isSupported(ArtifactType type) {
		switch (type) {
		case core:
			return coreSupported;
		case phd:
			return phdSupported;
		case javacore:
			return javacoreSupported;
		case unknown:
		default:
			return ExceptionHandler.UNKNOWN;
		}
	}

	String getClassName() {
		return className;
	}

	String getMethodname() {
		return methodName;
	}

}
