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
package org.openj9.test.jsr335.interfacePrivateMethod;

/*The class LoadInterfaceContainingPrivateMethod is used as the classloader to load our classes*/
public class LoadInterfaceContainingPrivateMethod extends ClassLoader {

	/* The method loadClass takes as parameter the className of the class to be loaded and a boolean value representing whether the class 
	 * will be initialized or not. It returns as result the object of the class just created.  
	 */
	/**
	 * The method loadClass is used to load the specified classes
	 * 
	 * @param className String
	 * 			name of the class to be loaded
	 * @param resolveClass boolean
	 * 			boolean value signifying whether to resolve class or not
	 * @return the newly defined class
	 */
	@Override
	protected synchronized Class<?> loadClass(String className, boolean resolveClass) throws ClassNotFoundException {
		if (className.equals("ITest")) {
			return loadModifiedclass();
		} else if (className.equals("ImplClass")) {
			return loadImplClass();
		} else if (className.equals("ITestStaticInvokeInterface")) {
			return loadModifiedInterfaceForStaticInvokeInterface();
		} else if (className.equals("ITestNonStaticInvokeInterface")) {
			return loadModifiedInterfaceForNonStaticInvokeInterface();
		} else if (className.equals("ImplClassForStaticInvokeInterface")) {
 			return loadImplClassForStaticInvokeInterface();
		} else if (className.equals("ImplClassForNonStaticInvokeInterface")) {
			return loadImplClassForNonStaticInvokeInterface();
		} else if (className.equals("ITestChild")) {
			return loadModifiedInterfaceForITestChildInterface();
		} else if (className.equals("ImplClassToTestReflectionAndMethodHandles")) {
			return loadImplClassToTestReflectionAndMethodHandles();
		} else if (className.equals("D")) {
				return ClassD();
		} else if (className.equals("E")) {
			return ClassE();
		} else if (className.equals("G")) {
			return ClassG();
		} else if (className.equals("F")) {
			return ClassF();
		} else if (className.equals("C")) {
			return ClassC();
		} else if (className.equals("H")) {
			return ClassH();
		} 
		return super.loadClass(className, resolveClass);
	}
	
	private Class<?> ClassC() {
		String[] implementedInterfaces = {"org/openj9/test/jsr335/interfacePrivateMethod/interfaces/I"};
		byte[] loadModifiedClass = GenerateImplClass.dumpWithDefaultImplementation("C", implementedInterfaces);
		return defineClass("C", loadModifiedClass, 0, loadModifiedClass.length);
	}
	
	private Class<?> ClassH() {
		String[] implementedInterfaces = {"org/openj9/test/jsr335/interfacePrivateMethod/interfaces/I", "org/openj9/test/jsr335/interfacePrivateMethod/interfaces/L"};
		byte[] loadModifiedClass = GenerateImplClass.dumpWithDefaultImplementation("H", implementedInterfaces);
		return defineClass("H", loadModifiedClass, 0, loadModifiedClass.length);
	}
	
	public Class<?> ClassF() {
		String[] implementedInterfaces = {"org/openj9/test/jsr335/interfacePrivateMethod/interfaces/I", "org/openj9/test/jsr335/interfacePrivateMethod/interfaces/N"};
		byte[] loadModifiedClass = GenerateImplClass.dump("F", implementedInterfaces);
		return defineClass("F", loadModifiedClass, 0, loadModifiedClass.length);
	}
	
	private Class<?> ClassG() {
		String[] implementedInterfaces = {"org/openj9/test/jsr335/interfacePrivateMethod/interfaces/O"};
		byte[] loadModifiedClass = GenerateImplClass.dump("G", implementedInterfaces);
		return defineClass("G", loadModifiedClass, 0, loadModifiedClass.length);
	}

	public Class<?> ClassE() {
		String[] implementedInterfaces = {"org/openj9/test/jsr335/interfacePrivateMethod/interfaces/I", "org/openj9/test/jsr335/interfacePrivateMethod/interfaces/L"};
		byte[] loadModifiedClass = GenerateImplClass.dump("E", implementedInterfaces);
		return defineClass("E", loadModifiedClass, 0, loadModifiedClass.length);
	}

	private Class<?> ClassD() {
		String[] implementedInterfaces = {"org/openj9/test/jsr335/interfacePrivateMethod/interfaces/I"};
		byte[] loadModifiedClass = GenerateImplClass.dump("D", implementedInterfaces);
		return defineClass("D", loadModifiedClass, 0, loadModifiedClass.length);
	}

	public Class<?> loadModifiedclass() {
		byte[] loadModifiedClass = GenerateInterfaceFromASM.makeClassFile();
		return defineClass("ITest", loadModifiedClass, 0, loadModifiedClass.length);
	} 
	
	public Class<?> loadImplClass() {
		String[] implementedInterfaces = {"ITest"};
		byte[] loadModifiedClass = GenerateImplClass.dump("ImplClass", implementedInterfaces);
		return defineClass("ImplClass", loadModifiedClass, 0, loadModifiedClass.length);
	} 
	
	public Class<?> loadModifiedInterfaceForStaticInvokeInterface() {
		byte[] loadModifiedClass = GenerateInterfaceFromASMForStaticInvokeInterface.makeClassFile();
		return defineClass("ITestStaticInvokeInterface", loadModifiedClass, 0, loadModifiedClass.length);
	} 
	
	public Class<?> loadModifiedInterfaceForNonStaticInvokeInterface() {
		byte[] loadModifiedClass = GenerateInterfaceFromASMForNonStaticInvokeInterface.makeClassFile();
		return defineClass("ITestNonStaticInvokeInterface", loadModifiedClass, 0, loadModifiedClass.length);
	} 

	public Class<?> loadImplClassForStaticInvokeInterface() {
		String[] implementedInterfaces = {"ITestStaticInvokeInterface"};
		byte[] loadModifiedClass = GenerateImplClass.dump("ImplClassForStaticInvokeInterface", implementedInterfaces);
		return defineClass("ImplClassForStaticInvokeInterface", loadModifiedClass, 0, loadModifiedClass.length);
	} 
	
	public Class<?> loadImplClassForNonStaticInvokeInterface() {
		String[] implementedInterfaces = {"ITestNonStaticInvokeInterface"};
		byte[] loadModifiedClass = GenerateImplClass.dump("ImplClassForNonStaticInvokeInterface", implementedInterfaces);
		return defineClass("ImplClassForNonStaticInvokeInterface", loadModifiedClass, 0, loadModifiedClass.length);
	} 
	
	public Class<?> loadModifiedInterfaceForITestChildInterface() {
		byte[] loadModifiedClass = GenerateInterfaceFromASMForITestChild.makeInterface();
		return defineClass("ITestChild", loadModifiedClass, 0, loadModifiedClass.length);
	} 
	
	public Class<?> loadImplClassToTestReflectionAndMethodHandles() {
		String[] implementedInterfaces = {"ITestChild"};
		byte[] loadModifiedClass = GenerateImplClass.dump("ImplClassToTestReflectionAndMethodHandles", implementedInterfaces);
		return defineClass("ImplClassToTestReflectionAndMethodHandles", loadModifiedClass, 0, loadModifiedClass.length);
	} 
	
}
