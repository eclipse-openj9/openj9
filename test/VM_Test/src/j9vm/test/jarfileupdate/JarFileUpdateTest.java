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
package j9vm.test.jarfileupdate;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.net.URL;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import com.ibm.oti.shared.*;

/**
 * This class is used by JarFileUpdateTestRunner to shared cache 
 * and load a class using a custom classloader.
 * Its usage is as follows:
 * 		1. java TestCase coldcache <jarfile1>
 * 			where jarfile1 contains the class to be loaded using custom classloader.
 * 		2. java TestCase warmcache <jarfile1> <jarfile2>
 * 			where jarfile1 and jarfile2 contain different versions of the same class and are used to load class using custom classloader.
 * 		  
 * @author Ashutosh
 */
public class JarFileUpdateTest {
	public static void showUsage() {
		System.err.println("Usage:");
		System.err.println("java TestCase coldcache <absolute path to jarfile1>");
		System.err.println("java TestCase warmcache <absolute path to jarfile1> <absolute path tho jarfile2>");
		System.exit(0);
	}
	
	public static void main(String args[]) throws Exception {
		if (args.length != 2 && args.length != 3) {
			showUsage();
		} else if (args[0].equals("warmcache") && args.length != 3) {
			showUsage();
		}

		String jarfile1 = args[1];
		String jarfile2 = null;

		if (args[0].equals("warmcache")) {
			jarfile2 = args[2];
		}

		loadClass(jarfile1, "Sample");
		
		if (args[0].equals("warmcache")) {
			/* Replace original jar file with modified jar file as follows:
			 *  - rename <jarfile1> to <jarfile1.bak>
			 *  - rename <jarfile2> to <jarfile1>
			 */
			File orig = new File(jarfile1);
			File backup = new File(orig.getAbsolutePath() + ".bak");
			boolean res = orig.renameTo(backup);
			if (!res) {
				System.err.println("Error: Failed to rename original jar file to " + backup.getName());
				return;
			}
			if (orig.exists()) {
				System.err.println("Error: Original jar file has been renamed but it still exists!");
			}
			File mod = new File(jarfile2);
			res = mod.renameTo(orig);
			if (!res) {
				System.err.println("Error: Failed to rename modified jar file to " + orig.getName());
				return;
			}
			if (!orig.exists()) {
				System.err.println("Error: Modified jar file has been renamed to original jar file, but original jar file does not exist!");
			}
			if (mod.exists()) {
				System.err.println("Error: Modified jar file has been renamed but it still exists!");
			}
			/* Wait for 2 secs and update last modification time of the jar file */
			System.err.println("Jarfile has been updated");
			System.err.println("Waiting for 2 seconds before updating the jar file timestamp...");
			Thread.sleep(2000);
			orig.setLastModified(System.currentTimeMillis());
			System.err.println();
			loadClass(jarfile1, "Sample");
			
			/* Restore original and modified jar files */
			orig.renameTo(mod);
			backup.renameTo(orig);
		}
	}

	public static void loadClass(String jarLoc, String className) throws Exception {
		ClassLoader loader = new CustomClassLoader(jarLoc);
		Class<?> clazz = loader.loadClass(className);
		
		if (clazz != null) {
			System.err.println("Loaded " + className);
		} else {
			System.err.println("Failed to load " + className);
		return;
		}
		
		Method m = clazz.getMethod("foo", (Class[])null);
		if (m != null) {
			m.invoke(null, (Object[])null);
		} else {
			System.err.println("Failed to reflect on Sample.foo()");
		}
		return;
	}
	static class CustomClassLoader extends ClassLoader {
		SharedClassURLHelper helper = null;
		URL url;
		File file;
		boolean loadedFromCache;
		ZipFile zip;

		public CustomClassLoader(String jarFile) throws Exception {
			file = new File(jarFile);
			if (null != file) {
				url = file.toURI().toURL();
				System.out.println("Jar file url: " + url);
				zip = new ZipFile(file);
			}
			loadedFromCache = false;
			SharedClassHelperFactory factory = null;
			factory = Shared.getSharedClassHelperFactory();
			if (factory != null) {
				helper = factory.getURLHelper(this);
			} else {
				System.err.println("Error: Failed to get SharedClassHelperFactory");
			}
		}

		public Class<?> findClass(String name) throws ClassNotFoundException {
			byte[] b;
			Class<?> clazz = null;
			
			try {
				b = loadClassData(name);
			} catch (Exception e) {
				throw new ClassNotFoundException("Error: Failed load class data", e);
			}
			if (null != b) {
				clazz = defineClass(name, b, 0, b.length);
			} else {
				throw new ClassNotFoundException("Error: Failed to define class");
			}
			if (!loadedFromCache && (helper != null)) {
				helper.storeSharedClass(url, clazz);
			}
			return clazz;
		}

		private byte[] loadClassData(String name) throws Exception {
			String fullName = name.replace('.', '/') + ".class";
			byte[] classbytes = null;
			
			if (helper != null) {
				System.err.println("Searching class in shared class cache...");
				classbytes = helper.findSharedClass(url, name);
			}
			if (classbytes != null) {
				System.err.println("Found class in shared class cache");
				loadedFromCache = true;
				zip.close();
				return classbytes;
			} else {
				/* Open jar URL and read classfile bytes */
				System.err.println("Failed to find class in shared class cache, loading from disk");
				ZipEntry entry = zip.getEntry(fullName);
				InputStream is = zip.getInputStream(entry);
				ByteArrayOutputStream os = new ByteArrayOutputStream();
				byte[] data = new byte[1024];
				int count = 0;
				while ((count = is.read(data)) != -1) {
					os.write(data, 0, count);
				}
				is.close();
				zip.close();
				return os.toByteArray();
			}
		}
	}
}
