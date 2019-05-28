/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package j9vm.test.invalidclasspath;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.net.JarURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.util.jar.JarEntry;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;

import com.ibm.oti.shared.Shared;
import com.ibm.oti.shared.SharedClassHelperFactory;
import com.ibm.oti.shared.SharedClassURLClasspathHelper;

/**
 * This class is used by SetClasspathTestRunner to check behavior of com.ibm.oti.shared.SharedClassURLClasspathHelper
 * when its classpath is updated to include an invalid URL 
 * using com.ibm.oti.shared.SharedClassURLClasspathHelper.setClasspath() API.
 * 
 * This class uses a custom classloader to add class(es) to a shared cache using SharedClassURLClasspathHelper.
 * It works as follows:
 * 	- Create a custom loader 'CL' that uses SharedClassURLClasspathHelper to find/store classes.
 *  - Assign a set of jar files to be used as classpath by SharedClassURLClasspathHelper.
 *  - Load a class using 'CL'. This class gets added to shared class cache.
 *  - Perform a series of modifications in the classpath of SharedClassURLClasspathHelper by adding/removing/replacing URLs.
 *  - After each modification, attempt to load a new class. 
 *      - If the modification adds an invalid URL to classpath, then the class gets added as ORPHAN in the shared cache. 
 *      - If there no invalid URLs in the classpath, then the class gets added as ROMCLASS in the shared cache.
 *  
 *  Modifications done to the classpath are as follows:
 *  Start	: 	a:b
 *  Mod1	:	a:b:<invalidURL1>:<invalidURL2>
 *  Mod2	:	a:b:<invalidURL1>
 *  Mod3	:	a:b:<invalidURL1>:c
 *  Mod4	:	a:b:c
 *  Mod5	:	a:b:<invalidURL1>
 *  Mod6	:	a:b
 *  
 * @author Ashutosh
 */
public class SetClasspathTest {
	private static final String MSG_PREFIX = SetClasspathTest.class.getSimpleName() + "> ";
	public static final String INVALID_URL1 = "https://w3.ibm.com/";
	public static final String INVALID_URL2 = "http://www.google.com";
	
	public static void main(String args[]) throws Exception {
		URL urls1[] = new URL[] {
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource1.jar"),
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource2.jar")
		};
		
		CustomClassLoader loader = new CustomClassLoader(urls1);
		/* TestA1 should be stored as ROMCLASS in cache */
		Class.forName("TestA1", true, loader);
		
		System.err.println("\n" + MSG_PREFIX + "URL Transformation 1");
		URL urls2[] = new URL[] {
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource1.jar"),
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource2.jar"),
				new URL(INVALID_URL1),
				new URL(INVALID_URL2)
		};
		/* added two invalid URLs */
		loader.setClasspath(urls2);
		/* TestA2 should only be stored as ORPHAN in cache */
		Class.forName("TestA2", true, loader);
				
		System.err.println("\n" + MSG_PREFIX + "URL Transformation 2");
		URL urls3[] = new URL[] {
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource1.jar"),
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource2.jar"),
				new URL(INVALID_URL1)
		};
		/* removed one invalid URL */
		loader.setClasspath(urls3);
		/* TestB1 should only be stored as ORPHAN in cache, as there is still an invalid URL in the classpath */
		Class.forName("TestB1", true, loader);
		
		System.err.println("\n" + MSG_PREFIX + "URL Transformation 3");
		URL urls4[] = new URL[] {
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource1.jar"),
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource2.jar"),
				new URL(INVALID_URL1),
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource3.jar")
		};
		/* added one valid URL */
		loader.setClasspath(urls4);
		/* TestB2 should only be stored as ORPHAN in cache, as there is an one invalid URL in the classpath */
		Class.forName("TestB2", true, loader);
		/* TestC1 should only be stored as ORPHAN in cache, as there is an one invalid URL in the classpath */
		Class.forName("TestC1", true, loader);

		System.err.println("\n" + MSG_PREFIX + "URL Transformation 4");
		URL urls5[] = new URL[] {
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource1.jar"),
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource2.jar"),
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource3.jar")
		};
		/* remove invalid URL */
		loader.setClasspath(urls5);
		/* TestB3 should be stored as ROMCLASS in cache, as there is no invalid URL in the classpath.
		 * Do not load any class from InvalidClasspathResource3.jar otherwise it will get marked as 'confirmed' by the VM
		 * and further transformations to remove InvalidClasspathResource3.jar won't be allowed.
		 */
		Class.forName("TestB3", true, loader);
		
		System.err.println("\n" + MSG_PREFIX + "URL Transformation 5");
		URL urls6[] = new URL[] {
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource1.jar"),
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource2.jar"),
				new URL(INVALID_URL1)
		};
		/* replace valid URL with invalid URL */
		loader.setClasspath(urls6);
		/* TestA3 should be stored as ORPHAN in cache, as there is an invalid URL in the classpath */
		Class.forName("TestA3", true, loader);
		
		System.err.println("\n" + MSG_PREFIX + "URL Transformation 6");
		URL urls7[] = new URL[] {
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource1.jar"),
				SetClasspathTest.class.getClassLoader().getResource("InvalidClasspathResource2.jar")
		};
		/* remove invalid URL */
		loader.setClasspath(urls7);
		/* TestA4 should be stored as ROMCLASS in cache, as there is no invalid URL in the classpath */
		Class.forName("TestA4", true, loader);
	}
	
	public static class CustomClassLoader extends ClassLoader {
		SharedClassURLClasspathHelper helper = null;
		URL urls[];
		boolean loadedFromCache;
		int foundAtIndex = -1;

		public CustomClassLoader(URL urls[]) throws Exception {
			this.urls = new URL[urls.length];
			System.arraycopy(urls, 0, this.urls, 0, urls.length);
			loadedFromCache = false;
			SharedClassHelperFactory factory = null;
			factory = Shared.getSharedClassHelperFactory();
			if (factory != null) {
				helper = factory.getURLClasspathHelper(this, urls);
			} else {
				System.err.println(MSG_PREFIX + "Error: Failed to get SharedClassHelperFactory");
			}
		}
		
		public void setClasspath(URL urls[]) throws Exception {
			this.urls = new URL[urls.length];
			System.arraycopy(urls, 0, this.urls, 0, urls.length);
			helper.setClasspath(urls);
		}
		
		public Class<?> findClass(String name) throws ClassNotFoundException {
			Class<?> clazz = null;
			byte[] classbytes = null;
			
			try {
				if (helper != null) {
					System.err.println(MSG_PREFIX + "Searching class " + name + " in shared class cache...");
					classbytes = helper.findSharedClass(name, null);
				}
				if (classbytes == null) {
					classbytes = loadClassData(name);
				}
			} catch (Exception e) {
				throw new ClassNotFoundException("Failed to load classfile bytes for class " + name, e);
			}
			if (null != classbytes) {
				clazz = defineClass(name, classbytes, 0, classbytes.length);
			} else {
				throw new ClassNotFoundException("Could not locate classfile bytes for class" + name);
			}
			if (!loadedFromCache && (helper != null)) {
				if (helper.storeSharedClass(clazz, foundAtIndex)) {
					System.err.println(MSG_PREFIX + "Successfully stored class " + name + " in shared cache");
				} else {
					System.err.println(MSG_PREFIX + "Failed to store class " + name + " in shared cache");
				}
			}
			return clazz;
		}

		private byte[] loadClassData(String name) throws Exception {
			String fullName = name.replace('.', '/') + ".class";
			byte[] classbytes = null;

			System.err.println(MSG_PREFIX + "Failed to find class " + name + " in shared class cache, loading from disk");
			
			foundAtIndex = -1;
			for (int i = 0; i < urls.length; i++) {
				/* Find and read class. Treat the url as a jar file url. */
				URLConnection urlConn = urls[i].openConnection();
				if (urlConn instanceof JarURLConnection) {
					JarURLConnection conn = (JarURLConnection)urls[i].openConnection();
					JarEntry entry = conn.getJarEntry();
					InputStream is = this.getClass().getClassLoader().getResourceAsStream(entry.getName());
					ZipInputStream zis = new ZipInputStream(is);
					ZipEntry zentry = zis.getNextEntry();
					while (zentry != null) {
						if (zentry.getName().equals(fullName)) {
							ByteArrayOutputStream os = new ByteArrayOutputStream();
							byte[] data = new byte[1024];
							int count = 0;
							while ((count = zis.read(data, 0, data.length)) != -1) {
								os.write(data, 0, count);
							}
							classbytes = os.toByteArray();
							foundAtIndex = i;
							System.err.println(MSG_PREFIX + "Found classfile bytes for class " + name + " at classpath index " + foundAtIndex);
							break;
						}
						zentry = zis.getNextEntry();
					}
					if (classbytes != null) {
						break;
					}
				}
			}
			if (classbytes == null) {
				System.err.println(MSG_PREFIX + "Failed to find classfile bytes for class " + name);
			}
			return classbytes;
		}
	}
}
