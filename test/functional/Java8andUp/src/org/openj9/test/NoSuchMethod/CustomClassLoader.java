/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package org.openj9.test.NoSuchMethod;

import java.lang.invoke.MethodType;
import java.security.CodeSource;
import java.util.HashMap;

import org.testng.log4testng.Logger;

import java.net.URLClassLoader;
import java.net.URL;

public class CustomClassLoader extends URLClassLoader {
	
	public static Logger logger = Logger.getLogger(CustomClassLoader.class);

	private HashMap<String, byte[]> customClasses;
	private HashMap<String, CodeSource> classCodeSources;

	public CustomClassLoader() {
		this(new URL[0]);
	}

	public CustomClassLoader(URL[] classpath) {
		super(classpath);
		logger.debug(String.format("Creating classloader %s\n", classpath[0]));
		customClasses = new HashMap<String, byte[]>();
		classCodeSources = new HashMap<String, CodeSource>();
	}

	public ClassInfo buildInvoker(String name, CodeSource codeSource, ClassInfo callee, String calledMethodName,
			MethodType calledMethodType) {
		String[] interfaces = { "java/lang/Runnable" };
		ClassGenerator cg = new ClassGenerator(name, "java/lang/Object", interfaces);
		cg.addCaller("run", callee.getName(), calledMethodName, calledMethodType);
		byte[] classBytes = cg.dump();
		return new ClassInfo(defineClass(name, classBytes, 0, classBytes.length, codeSource));
	}

	public Class<?> buildInvoker(String invokerName, String calledClassName, String calledMethodName,
			MethodType calledMethodType) {
		return buildInvoker(invokerName, null, calledClassName, calledMethodName, calledMethodType);
	}

	public Class<?> buildInvoker(String invokerName, CodeSource codeSource, String calledClassName,
			String calledMethodName, MethodType calledMethodType) {
		String[] interfaces = { "java/lang/Runnable" };
		ClassGenerator cg = new ClassGenerator(invokerName, "java/lang/Object", interfaces);
		cg.addCaller("run", calledClassName, calledMethodName, calledMethodType);
		byte[] classBytes = cg.dump();
		return defineClass(invokerName, classBytes, 0, classBytes.length, codeSource);
	}

	public ClassInfo buildCallee(String name) {
		return buildCallee(name, null);
	}

	public ClassInfo buildCallee(String name, CodeSource codeSource) {
		ClassGenerator gen = new ClassGenerator(name, "java/lang/Object", null);
		byte[] classDef = gen.dump();
		Class<?> clazz = defineClass(name, classDef, 0, classDef.length, codeSource);
		return new ClassInfo(clazz);
	}

	@Override
	protected Class<?> findClass(String name) throws ClassNotFoundException {
		logger.debug(String.format("find class '%s'\n", name));
		if (customClasses.containsKey(name)) {
			logger.debug("Intercepting findClass");
			byte[] classBytes = customClasses.get(name);
			return defineClass(name, classBytes, 0, classBytes.length, classCodeSources.get(name));
		}

		logger.debug(String.format("offloading Class find for '%s'\n", name));
		return super.findClass(name);

	}

	@Deprecated
	public void addCustomClass(String name, byte[] classDef) {
		customClasses.put(name, classDef);
	}

	@Deprecated
	public void addCustomClass(String name, byte[] classDef, CodeSource codeSource) {
		customClasses.put(name, classDef);
		classCodeSources.put(name, codeSource);
	}

	@Override
	public Class<?> loadClass(String className) throws ClassNotFoundException {
		if (customClasses.containsKey(className)) {
			return findClass(className);
		}

		return super.loadClass(className);
	}

}
