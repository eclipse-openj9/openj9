package org.openj9.test.annotationClassLoader;

/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
import org.openj9.resources.classloader.CustomSyncProvider;
import org.testng.annotations.Test;
import org.testng.Assert;

import java.net.URL;
import java.net.URLClassLoader;
import java.sql.SQLException;

import javax.sql.rowset.CachedRowSet;
import javax.sql.rowset.spi.SyncProvider;

@Test(groups = { "level.sanity" })
public class Test_ClassLoader {

	@Test
	public void test_classloaders() {

		URL url = getClass().getProtectionDomain().getCodeSource().getLocation();
		// loads Annotation2 and HelperClass1
		ClassLoader loader0 = new URLClassLoader(new URL[] { url }, null) {
			protected Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
				if (className.equals("org.openj9.test.annotationClassLoader.Annotation1"))
					throw new ClassNotFoundException(className);
				return super.loadClass(className, resolve);
			}
		};
		// loads Annotation1
		ClassLoader loader1 = new URLClassLoader(new URL[] { url }, loader0) {
			protected Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
				if (className.equals("org.openj9.test.annotationClassLoader.AnnotationClass")
						|| className.equals("org.openj9.test.annotationClassLoader.HelperClass2"))
					throw new ClassNotFoundException(className);
				return super.loadClass(className, resolve);
			}
		};
		// loads AnnotationClass and HelperClass2
		ClassLoader loader2 = new URLClassLoader(new URL[] { url }, loader1) {
			protected Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
				if (className.equals("org.openj9.test.annotationClassLoader.Annotation2"))
					throw new ClassNotFoundException(className);
				return super.loadClass(className, resolve);
			}
		};
		try {
			Class testClass = Class.forName("org.openj9.test.annotationClassLoader.AnnotationClass", false, loader2);
			((Runnable)testClass.newInstance()).run();
		} catch (ClassNotFoundException e) {
			Assert.fail("unexpected: " + e);
		} catch (IllegalAccessException e) {
			Assert.fail("unexpected: " + e);
		} catch (InstantiationException e) {
			Assert.fail("unexpected: " + e);
		}
	}

	@Test
	public void test_latestUserDefinedLoader() throws Exception {
		String customSyncProviderClassName = CustomSyncProvider.class.getName();
		CachedRowSet crs = (CachedRowSet)Class.forName("com.sun.rowset.CachedRowSetImpl").newInstance();
		crs.setSyncProvider(customSyncProviderClassName);
		SyncProvider syncProvider = crs.getSyncProvider();
	
		if (syncProvider instanceof CustomSyncProvider) {
			/* Call to CachedRowSetImpl.createCopy results in the following Java stack:
			 * 
			 *  [1] jdk.internal.loader.BuiltinClassLoader.loadClass [Bootstrap ClassLoader]
			 *  [2] java.lang.ClassLoader.loadClass [Bootstrap ClassLoader]
			 *  [3] java.lang.Class.forNameImpl [Bootstrap ClassLoader]
			 *  [4] java.lang.Class.forName [Bootstrap ClassLoader]
			 *  [5] java.io.ClassCache$FutureValue.get [Bootstrap ClassLoader]
			 *  [6] java.io.ClassCache.get [Bootstrap ClassLoader]
			 *  [7] java.io.ObjectInputStream.resolveClass [Bootstrap ClassLoader]
			 *  [8] java.io.ObjectInputStream.readNonProxyDesc [Bootstrap ClassLoader]
			 *  [9] java.io.ObjectInputStream.readClassDesc [Bootstrap ClassLoader]
			 *  [10] java.io.ObjectInputStream.readOrdinaryObject [Bootstrap ClassLoader]
			 *  [11] java.io.ObjectInputStream.readObject0 [Bootstrap ClassLoader]
			 *  [12] java.io.ObjectInputStream.defaultReadFields [Bootstrap ClassLoader]
			 *  [13] java.io.ObjectInputStream.defaultReadObject [Bootstrap ClassLoader]
			 *  [14] com.sun.rowset.CachedRowSetImpl.readObject [Platform ClassLoader]
			 *  [15] jdk.internal.reflect.NativeMethodAccessorImpl.invoke0 [Bootstrap ClassLoader]
			 *  [16] jdk.internal.reflect.NativeMethodAccessorImpl.invoke [Bootstrap ClassLoader]
			 *  [17] jdk.internal.reflect.DelegatingMethodAccessorImpl.invoke [Bootstrap ClassLoader]
			 *  [18] java.lang.reflect.Method.invoke [Bootstrap ClassLoader]
			 *  [19] java.io.ObjectStreamClass.invokeReadObject [Bootstrap ClassLoader]
			 *  [20] java.io.ObjectInputStream.readSerialData [Bootstrap ClassLoader]
			 *  [21] java.io.ObjectInputStream.readOrdinaryObject [Bootstrap ClassLoader]
			 *  [22] java.io.ObjectInputStream.readObject0 [Bootstrap ClassLoader]
			 *  [23] java.io.ObjectInputStream.readObjectImpl [Bootstrap ClassLoader]
			 *  [24] java.io.ObjectInputStream.readObject [Bootstrap ClassLoader]
			 *  [25] com.sun.rowset.CachedRowSetImpl.createCopy [Platform ClassLoader]
			 *  [26] .... [Application ClassLoader]
			 *  
			 * ObjectInputStream.resolveClass makes a call to JVM_LatestUserDefinedLoader.
			 * ClassLoader returned from JVM_LatestUserDefinedLoader is passed to Class.forName.
			 *  
			 * CustomSyncProvider is loaded by the Application ClassLoader. CachedRowSetImpl.createCopy
			 * will try to search for CustomSyncProvider using the ClassLoader returned by
			 * JVM_LatestUserDefinedLoader. If JVM_LatestUserDefinedLoader returns a Bootstrap
			 * or Platform ClassLoader, then CustomSyncProvider won't be found.
			 * CachedRowSetImpl.createCopy will catch the ClassNotFoundException and throw
			 * a SQLException.
			 *  
			 * The purpose of this test is to verify that JVM_LatestUserDefinedLoader returns
			 * a valid Application ClassLoader. If a Bootstrap or a Platform ClassLoader is
			 * returned, then SQLException will be thrown by CachedRowSetImpl.createCopy
			 * since CustomSyncProvider won't be found.
			 *  
			 * CachedRowSetImpl.createCopy may also throw SQLException for other reasons.
			 * Use a debugger to confirm if SQLException is a result of ClassNotFoundException
			 * or something else.
			 */
			CachedRowSet crsCopy = crs.createCopy();
		} else {
			Assert.fail("Wrong SyncProvider.\n"
					+ "Failed to use " + customSyncProviderClassName + ".\n"
					+ "Instead " + syncProvider + " is used.\n"
					+ "Check if cmdline option -Drowset.provider.classname=org.openj9.resources.classloader.CustomSyncProvider is set.");
		}
	}
}
