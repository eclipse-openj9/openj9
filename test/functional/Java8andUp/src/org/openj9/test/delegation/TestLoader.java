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
package org.openj9.test.delegation;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.Enumeration;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.zip.ZipEntry;

import org.testng.Assert;
import org.testng.log4testng.Logger;

public abstract class TestLoader extends ClassLoader {

	public static final Logger logger = Logger.getLogger(TestLoader.class);

	private static final int MAXFILESIZE = 100000;
	private static String[] classPathDirs;
	boolean delegateTestClassB = false;
	TestLoader otherLoader = null;
	final String tcA = "org.openj9.test.delegation.TestClassA", tcB = "org.openj9.test.delegation.TestClassB";
	String delegatedClass = null;
	String name = null;
	private static boolean isParallelCapable = registerAsParallelCapable();

	public abstract boolean isParallelCapable();

	private String undelegatedClass;

	public TestLoader(String string) {
		name = string;
	}

	@Override
	public Class<?> loadClass(String className) throws ClassNotFoundException {
		byte[] classFileBytes = null;
		InputStream classFileFile;
		Class clazz = null;
		String pathToClass = className.replace(".", "/") + ".class";
		try {
			tlog("entered class loader for class " + className);
			try {
				java.lang.Thread.sleep(100);
			} catch (InterruptedException e1) {
				e1.printStackTrace();
			}
			tlog("waking");
			if (className.equals(delegatedClass)) {
				tlog("delegating " + className);
				clazz = otherLoader.loadClass(className);
				tlog("delegated " + className);
			} else if (className.equals(undelegatedClass))
				synchronized (this) {
					tlog("reading " + className);
					int i = 0;
					while (i < classPathDirs.length) {
						JarFile classJar = null;
						String cpDir = classPathDirs[i];
						++i;
						if (cpDir.endsWith(".jar")) {
							try {
								classJar = new JarFile(cpDir);
							} catch (IOException e) {
								continue;
							}
						}
						try {
							int bytesRead = 0;
							int totalBytesRead = 0;
							classFileBytes = new byte[MAXFILESIZE];
							if (null == classJar) {
								String classFileName = cpDir + "/" + pathToClass;
								classFileFile = new java.io.FileInputStream(classFileName);
							} else {
								ZipEntry ze = classJar.getEntry(pathToClass);
								if (null == ze) {
									continue;
								}
								classFileFile = classJar.getInputStream(ze);
							}
							do {
								bytesRead = classFileFile.read(classFileBytes, bytesRead, MAXFILESIZE - bytesRead);
								if (bytesRead > 0) {
									totalBytesRead += bytesRead;
								}
							} while (bytesRead >= 0);

							tlog("defining " + className);
							clazz = defineClass(className, classFileBytes, 0, totalBytesRead);
							break;
						} catch (NoClassDefFoundError e) {
							e.printStackTrace();
							clazz = null;
							break;
						} catch (FileNotFoundException e) {
							if (i >= classPathDirs.length) {
								logger.error("FileNotFoundException reading " + className);
								clazz = null;
							}
						} catch (IOException e) {
							clazz = null;
							break;
						}
					}
				}
			else {
				clazz = super.loadClass(className);
			}
			return clazz;
		} finally {
			tlog("exiting loader");
		}
	}

	private void tlog(String string) {
		log(name + ": " + string);
	}

	public static void log(String string) {
		Long id = java.lang.Thread.currentThread().getId();
		logger.debug(id.toString() + ": " + string);
	}

	public boolean isDelegateTestClassB() {
		return delegateTestClassB;
	}

	public void setDelegateTestClassB(boolean delegateTestClassB) {
		this.delegateTestClassB = delegateTestClassB;
		delegatedClass = delegateTestClassB ? tcB : tcA;
		undelegatedClass = !delegateTestClassB ? tcB : tcA;
	}

	public TestLoader getOtherLoader() {
		return otherLoader;
	}

	public void setOtherLoader(TestLoader otherLoader) {
		this.otherLoader = otherLoader;
	}

	public String getDelegatedClass() {
		return delegatedClass;
	}

	public void setName(String name) {
		this.name = name;
	}

	public static void setPathDirs(String[] dirs) {
		classPathDirs = dirs;

	}

	public static void error(String msg) {
		Assert.fail("TEST_STATUS: TEST_FAILED " + msg);
	}
}
