package org.openj9.test.support.resource;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Enumeration;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

import org.openj9.test.support.Support_ExtendedTestEnvironment;
import org.testng.log4testng.Logger;

/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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
 *
 * @requiredClass com.ibm.support.Support_Configuration
 */
public class Support_Resources {
	private static final Logger logger = Logger.getLogger(Support_Resources.class);

	private static int counter = 0;
	public static final String RESOURCE_PACKAGE = "/org/openj9/resources/";
	public static final String RESOURCE_PACKAGE_NAME = "org.openj9.resources";
	public static InputStream getStream(String name) {
		return Support_Resources.class.getResourceAsStream(RESOURCE_PACKAGE + name);
	}
	public static String getURL(String name){
		String folder = null;
		String fileName = name;
		File resources = createTempFolder();
		int index = name.lastIndexOf("/");
		if(index!=-1){
			folder = name.substring(0,index);
			name = name.substring(index+1);
		}
		copyFile(resources,folder,name);
		URL url = null;
		String resPath = resources.toString();
		if(resPath.charAt(0) == '/' || resPath.charAt(0) == '\\')
			resPath = resPath.substring(1);
		try {
			url = new URL("file:/" + resPath +"/" + fileName);
		} catch (MalformedURLException e) {
			// TODO Auto-generated catch block
			logger.error("Unexpected exception: " + e);
		}
		return url.toString();
	}

	public static File createTempFolder(){

		File folder = null;
		try {
		folder = File.createTempFile("openj9tr_resources","",null);
		folder.delete();
		folder.mkdirs();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			logger.error("Unexpected exception: " + e);
		}
		folder.deleteOnExit();
		return folder;
	}

	public static File copyFile(File root, String folder, String file){
		return copyFile(root, folder, file, file);
	}

	public static File copyFile(File root, String folder, String src, String dest){
		File f;
		if(folder != null){
			f = new File(root.toString() + "/" + folder);
			if(!f.exists()){
				f.mkdirs();
				f.deleteOnExit();
			}
		}else
			f = root;

		File destFile = new File(f.toString() + "/" + dest);

		InputStream in = Support_Resources.getStream(folder==null? src : folder + "/" + src);
		try {
			copyLocalFileto(destFile, in);
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			logger.error("Unexpected exception: " + e);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			logger.error("Unexpected exception: " + e);
		}
		return destFile;
	}

	public static File createTempFile(String suffix) throws IOException {
		return File.createTempFile("openj9tr_", suffix, null);
	}

	public static void copyLocalFileto(File dest, InputStream in) throws FileNotFoundException, IOException{
		if(!dest.exists()){
			FileOutputStream out = new FileOutputStream(dest);
			int result;
			byte[] buf = new byte[4096];
			while ((result = in.read(buf)) != -1)
				out.write(buf, 0, result);
			in.close();
			out.close();
			dest.deleteOnExit();
		}
	}

	public static File getExternalLocalFile(String url) throws IOException, MalformedURLException
	{
		File resources = createTempFolder();
		InputStream in = new URL(url).openStream();
		File temp = new File(resources.toString() + "/local.tmp");
		copyLocalFileto(temp, in);
		return temp;
	}

	/**
	 * Copy a file from a specific jar to the current directory. If the file has
	 * the parent folder in the jar, It will create all the necessary parent
	 * directories. After system exits, all the file and directory will delete.
	 *
	 * @param jarFile	the jar file
	 * @param folder	the parent folder in the jar, can be null or like com.ibm.tenant
	 * @param fromFile	the target file
	 * @param toFile	the destination file. If it is null, it will use the target file name
	 * @throws IOException
	 */
	public static void copyFromExternalJarTo(File jarFile, String folder,
			String fromFile, String toFile) throws IOException {
		String dest = "";
		if (toFile == null) {
			toFile = fromFile;
		}
		String packageName = folder.replace(".", File.separator);
		String fullName = packageName.endsWith(File.separator) ? (packageName + fromFile)
				: (packageName + File.separator + fromFile);
		if (folder != null) {
			File dir = new File(jarFile.getParentFile(), packageName);
			if (!dir.exists()) {
				dir.mkdirs();
				dir.deleteOnExit();
			}
			dest = dir.getPath() + File.separator;
		}

		dest += fromFile;
		JarFile jf = new JarFile(jarFile);
		for (Enumeration<JarEntry> e = jf.entries(); e.hasMoreElements();) {
			JarEntry je = e.nextElement();
			if (je.getName().replace("/", File.separator).equals(fullName)) {
				InputStream in = jf.getInputStream(je);

				if (!toFile.equals(fromFile)) {
					dest = dest.substring(0, dest.lastIndexOf(fromFile)) + toFile;
				}

				File destFile = new File(dest);
				// Rewrite the dest file
				if(destFile.exists()){
					destFile.delete();
				}
				copyLocalFileto(destFile, in);
				break;
			}
		}
		jf.close();
	}

}
