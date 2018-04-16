/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.invoker.util;

import java.io.*;
import java.util.Hashtable;

import org.objectweb.asm.*;
import org.testng.log4testng.Logger;

public abstract class ClassGenerator implements Opcodes {

	public static Logger logger = Logger.getLogger(ClassGenerator.class);

	/* Name of the keys in hashtable to get class characteristics */
	public static final String USE_HELPER_CLASS = "USE_HELPER_CLASS";
	public static final String EXTENDS_HELPER_CLASS = "EXTENDS_HELPER_CLASS";
	public static final String IMPLEMENTS_INTERFACE = "IMPLEMENTS_INTERFACE";
	public static final String SHARED_INVOKERS = "SHARED_INVOKERS";
	public static final String SHARED_INVOKERS_TARGET = "SHARED_INVOKERS_TARGET";

	public static final String HELPER_METHOD_TYPE = "HELPER_METHOD_TYPE";

	public static final String INTF_HAS_DEFAULT_METHOD = "INTF_HAS_DEFAULT_METHOD";
	public static final String INTF_HAS_STATIC_METHOD = "INTF_HAS_STATIC_METHOD";

	public static final String VERSION = "VERSION";

	/* Default value of the keys if not present in hashtable */
	public static final String DUMMY_CLASS_NAME = "Dummy";
	public static final String HELPER_CLASS_NAME = "Helper";
	public static final String HELPER_METHOD_NAME = "foo";
	public static final String HELPER_METHOD_SIG = "()Ljava/lang/String;";
	public static final String INTERFACE_NAME = "Intf";
	public static final String INTF_METHOD_NAME = HELPER_METHOD_NAME;
	public static final String INTF_METHOD_SIG = HELPER_METHOD_SIG;
	public static final String SHARED_INVOKER_METHOD_SIG = HELPER_METHOD_SIG;

	protected byte data[];
	protected String classDirectory;
	protected String packageName; /* of the form org/openj9/test/invoker */
	protected String className; /* of the form org/openj9/test/invoker/Dummy */
	protected Hashtable<String, Object> characteristics;

	public ClassGenerator(String directory, String pkgName) {
		this.classDirectory = directory;
		this.packageName = pkgName;
		this.characteristics = null;
	}

	public ClassGenerator(String directory, String pkgName, Hashtable<String, Object> characteristics) {
		this(directory, pkgName);
		this.characteristics = characteristics;
	}

	public void setCharacteristics(Hashtable<String, Object> characteristics) {
		this.characteristics = characteristics;
	}

	/*
	 * Creates .class file on disk for the given class. 
	 * The .class file is generated in a directory named 'testName/package',
	 * e.g. if testName is testStaticInterfaceV1 and className is org/openj9/test/invoker/I,
	 * I.class is generated in testStaticInterfaceV1/org/openj9/test/invoker
	 */
	public void writeToFile() throws IOException {
		FileOutputStream fos = null;
		BufferedOutputStream bos = null;
		/* className is of the form org/openj9/test/invoker/Dummy[_V<N>] where <N>=1,2,3... */
		String simpleName = className.substring(className.lastIndexOf('/') + 1);
		if ((characteristics != null) && (characteristics.get(VERSION) != null)) {
			simpleName = simpleName + "_" + (String)characteristics.get(VERSION) + ".class";
		} else {
			simpleName = simpleName + ".class";
		}

		String pkgName = className.substring(0, className.lastIndexOf('/') + 1);

		logger.debug("Class file " + simpleName + " is created in dir " + classDirectory + "/" + pkgName);
		try {
			File dir = new File(classDirectory + File.separator + pkgName);
			if (dir.exists() || dir.mkdirs()) {
				fos = new FileOutputStream(new File(dir, simpleName));
				bos = new BufferedOutputStream(fos);
				bos.write(data);
				bos.close();
			}
		} catch (IOException e) {
			/*
			 * Ignore any exception in writing class file as it is only for
			 * debugging purpose and not required for running the test
			 */
			logger.debug("Failed to write class file at " + classDirectory + "/" + pkgName + "/" + simpleName);
		}
	}

	abstract public byte[] getClassData() throws Exception;
}
