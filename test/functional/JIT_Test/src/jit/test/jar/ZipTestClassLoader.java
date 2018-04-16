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
package jit.test.jar;

import java.io.*;
import java.util.*;
import java.util.zip.*;
import org.testng.Assert;
import org.testng.log4testng.Logger;

/**
 * @author jistead
 *
 * To change this generated comment edit the template variable "typecomment":
 * Window>Preferences>Java>Templates.
 * To enable and disable the creation of type comments go to
 * Window>Preferences>Java>Code Generation.
 */
public class ZipTestClassLoader {
	private static Logger logger = Logger.getLogger(ZipTestClassLoader.class);
	
	protected class ZipRecord {
		protected ZipFile file;
		protected HashMap entries; // (String className,ZipEntry class)
		protected int classCount;

		protected ZipRecord(ZipFile f, int zipEntryCount) {
			entries =
				(zipEntryCount == -1
					? new HashMap()
					: new HashMap(zipEntryCount / 4));
			file = f;
			populate();
		}

		protected ZipRecord(ZipFile f) {
			this(f, -1);
		}

		// store the classes from the zipfile for faster loading later
		protected void populate() {
			classCount = 0;

			Enumeration zipFileEnum = file.entries();
			ZipEntry zipEntry = null;
			String zipEntryName = null;
			String className = null;

			while (zipFileEnum.hasMoreElements()) {
				zipEntry = (ZipEntry) zipFileEnum.nextElement();
				zipEntryName = zipEntry.getName();

				// if the entry is not a class, we don't load it
				if (!zipEntryName.endsWith(".class"))
					continue;
				className =
					zipEntryName.substring(0, zipEntryName.length() - 6);
				className = className.replace('/', '.');

				// if the classname already exists, we don't load it twice
				if (zipRecords.containsKey(className))
					continue;
				zipRecords.put(className, this);
				entries.put(className, zipEntry);
				classCount++;
			}
		}
	}

	protected class SurrogateClassLoader extends ClassLoader {
		protected int instanceNumber;
		protected int age;

		public SurrogateClassLoader() {
			super();
			age = 0;
		}

		protected Class findClass(String className)
			throws ClassNotFoundException {
			ZipRecord tree = (ZipRecord) zipRecords.get(className);
			if (tree == null)
				throw new ClassNotFoundException();

			ZipEntry entry = (ZipEntry) tree.entries.get(className);
			if (entry == null)
				throw new ClassNotFoundException();

			Class clazz = defineClass(tree.file, entry, className);
			if (clazz == null)
				throw new ClassNotFoundException();
			else
				return clazz;
		}

		protected Class defineClass(
			ZipFile file,
			ZipEntry entry,
			String className) {

			// is the class already define? (if it's loaded, then it must be defined)		
			Class clazz = findLoadedClass(className);
			if (clazz != null) {
				logger.debug("\t" + className);
				return (clazz);
			}

			int uncompressedSize = (int) entry.getSize();
			byte classFileData[] = new byte[uncompressedSize];
			try {
				InputStream inStream = file.getInputStream(entry);
				if ( null == inStream ) {
					return null;
				}
				int bytesRead = 0;
				while(bytesRead < classFileData.length){
				   int lastRead = inStream.read(
				  classFileData,
				  bytesRead,
				  uncompressedSize-bytesRead);
				   if(lastRead < 0) return (null);
				   bytesRead += lastRead;
				}
			} catch (IOException e) {
				return (null);
			}

			try {
				clazz =
					defineClass(
						null, // pass null for className, otherwise "java." packages cause a SecurityException
						classFileData,
						0,
						classFileData.length);

				if (clazz == null) {
					Map map = (Map) errMsgs.get(NO_CLASS_NO_ERROR_ERROR_TYPE);
					if (map == null) {
						map = new HashMap();
						errMsgs.put(NO_CLASS_NO_ERROR_ERROR_TYPE, map);
					}
					map.put(className, className);
				} else if (clazz.getClassLoader() == surrogate) {
					logger.debug("\t" + className);
				} else {
					Map map =
						(Map) errMsgs.get(PARENT_LOADER_PREEMPTION_ERROR_TYPE);
					if (map == null) {
						map = new HashMap();
						errMsgs.put(PARENT_LOADER_PREEMPTION_ERROR_TYPE, map);
					}
					map.put(className, className);
				}
			} catch (LinkageError e) {
				String errType = e.getClass().getName();
				Map map = (Map) errMsgs.get(errType);
				Map classList = null;
				if (map == null) {
					map = new HashMap();
					errMsgs.put(errType, map);
					classList = new HashMap();
					map.put(e.getMessage(), classList);
				} else {
					classList = (Map) map.get(e.getMessage());
					if (classList == null) {
						classList = new HashMap();
						map.put(e.getMessage(), classList);
					}
				}
				classList.put(className, className);
			}

			return (clazz);
		}

		protected Class loadClass(String className, boolean resolveClass)
			throws ClassNotFoundException {
			Class c = findLoadedClass(className);
			try {
				if (c == null)
					c = findClass(className);
			} catch (ClassNotFoundException ex) {
				c = ClassLoader.getSystemClassLoader().loadClass(className);
			}

			if (resolveClass)
				resolveClass(c);

			return c;
		}

		public Class loadClass(String className)
			throws ClassNotFoundException {
			return loadClass(className, false);
		}
	}

	public static final int DEFAULT_SURROGATE_LIFESPAN = 10;
	public static final boolean DEFAULT_FORCE_GC_ON_SURROGATE_DEATH = false;
	public static final String PARENT_LOADER_PREEMPTION_ERROR_TYPE =
		"PARENT_LOADER_PREEMPTION";
	public static final String NO_CLASS_NO_ERROR_ERROR_TYPE =
		"NO_CLASS_NO_ERROR";

	protected SurrogateClassLoader surrogate;
	protected int surrogateLifespan;
	protected boolean forceGcOnSurrogateDeath;
	protected HashMap errMsgs;

	// zipRecords gives us constant ZipRecord fetch-by-className's, and
	// orderedZipRecords gives us insertion-order iteration. 
	protected HashMap zipRecords; // {(String className, ZipRecord zipRecord)}
	protected LinkedList orderedZipRecords; // {ZipRecord}

	// zipFiles gives us constant ZipFile fetch-by-fileName's, and
	// orderedZipFiles gives us insertion-order iteration. 
	protected LinkedList orderedZipFiles; // {ZipFile}
	protected HashMap zipFiles; // {(String fileName, ZipFile file)}

	public ZipTestClassLoader() {
		this(
			DEFAULT_SURROGATE_LIFESPAN,
			DEFAULT_FORCE_GC_ON_SURROGATE_DEATH);
	}

	public ZipTestClassLoader(
		int lifespan,
		boolean forceGC) {
		super();
		forceGcOnSurrogateDeath = forceGC;
		surrogateLifespan = lifespan;
		reinit();
	}

	private void reinit() {
		zipFiles = new HashMap();
		orderedZipFiles = new LinkedList();
		orderedZipRecords = new LinkedList();
		zipRecords = new HashMap();
		surrogate = new SurrogateClassLoader();
	}

	public LinkedList getZipFiles() {
		return new LinkedList(orderedZipFiles);
	}

	public boolean addZipFile(ZipFile z) {
		String name = z.getName();
		logger.debug("\nAdding " + name + "...  ");
		Object o = zipFiles.put(name, z);

		// the map already contained a mapping for key:name
		// so we revert to the old value:o
		if (o != null) {
			zipFiles.put(name, o);
			logger.error("failed: duplicate zip file.");
			return (false);
		}

		orderedZipFiles.add(z);
		
		logger.debug("completed.");

		// unroll the zipfile into a ZipRecord, and keep a reference.
		
		logger.debug("Indexing " + name);
		
		int size = z.size();
		
		logger.debug(" (" + String.valueOf(size) + " files)...  ");

		ZipRecord record = new ZipRecord(z, size);
		orderedZipRecords.add(record);

		logger.debug(
			"completed (found "
				+ String.valueOf(record.classCount)
				+ " new classes).");

		return (true);
	}

	public void processZipFiles(String classFilter) {
		errMsgs = new HashMap();

		// iterate zip files in order
		Iterator classIter = orderedZipRecords.iterator();
		int totalError = 0;
		int fileCount = 0;
		int totalClassCount = 0;
		while (classIter.hasNext()) {
			ZipRecord record = (ZipRecord) classIter.next();
			ZipFile zipFile = record.file;
			logger.debug(
				"\nLoading "
					+ zipFile.getName()
					+ " ("
					+ String.valueOf(record.classCount)
					+ " classes)...  ");

			// iterate entries in the zipfile marked as viable classes
			String className = null;
			ZipEntry zipEntry = null;
			Map.Entry classPair = null;
			Iterator entryIter = record.entries.entrySet().iterator();
			int error = 0;
			int classCount = 0;

			while (entryIter.hasNext()) {
				classPair = (Map.Entry) entryIter.next();
				zipEntry = (ZipEntry) classPair.getValue();
				className = (String) classPair.getKey();

				// load the class only if it's name contains classFilter 
				if (className.indexOf(classFilter) < 0)
					continue;

				Class clazz = defineClass(zipFile, zipEntry, className);

				if (clazz != null) {
					classCount++;
					surrogate.age++;
				} else
					error++;
			}

			StringBuffer buff = new StringBuffer();
			buff.append(classCount);
			buff.append("/");
			buff.append(record.classCount);
			buff.append(" classes loaded in ");
			buff.append(zipFile.getName());
			buff.append(", ");
			buff.append(error);
			buff.append(" errors reported.");
			logger.debug(buff.toString());
			totalError += error;
			totalClassCount += classCount;
			fileCount++;
		}

		StringBuffer buff = new StringBuffer("\n");
		buff.append(totalClassCount);
		buff.append(" classes loaded in ");
		buff.append(fileCount);
		buff.append(" zip files, ");
		buff.append(totalError);
		buff.append(" total errors reported.");
		logger.debug(buff.toString());
		Assert.assertEquals(totalError, 0);

		printErrorMessages();
	}

	protected void printErrorMessages() {
		logger.debug("");

		Iterator errTypes = errMsgs.entrySet().iterator();
		Map.Entry errType = null;
		String key = null;
		Map classNames = null;

		while (errTypes.hasNext()) {
			errType = (Map.Entry) errTypes.next();
			key = (String) errType.getKey();

			if (key.equals(PARENT_LOADER_PREEMPTION_ERROR_TYPE)) {
				classNames = (Map) errType.getValue();
				if (classNames.isEmpty())
					continue;

				logger.error("Classes loaded by unknown ClassLoaders:");
				printClassNames("\t", classNames.keySet().iterator());
			} else if (key.equals(NO_CLASS_NO_ERROR_ERROR_TYPE)) {
				classNames = (Map) errType.getValue();
				if (classNames.isEmpty())
					continue;

				logger.error(
					"Classes that failed to load for unknown reasons:");
				printClassNames("\t", classNames.keySet().iterator());
			} else {
				logger.error(key);
				Iterator causes =
					((Map) errType.getValue()).entrySet().iterator();
				while (causes.hasNext()) {
					Map.Entry cause = (Map.Entry) causes.next();
					logger.error("\t" + cause.getKey() + ":");
					printClassNames(
						"\t\t",
						((Map) cause.getValue()).keySet().iterator());
				}
			}

			logger.debug("");
		}
	}

	private void printClassNames(String prefix, Iterator classNames) {
		while (classNames.hasNext()) {
			logger.error(prefix + (String) classNames.next());
		}
	}

	public void closeZipFiles() {
		Iterator zipFileIterator = zipFiles.values().iterator();

		while (zipFileIterator.hasNext()) {
			ZipFile zipFile = (ZipFile) zipFileIterator.next();
			try {
				zipFile.close();
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
		reinit();
	}

	protected Class defineClass(
		ZipFile file,
		ZipEntry entry,
		String className) {
		if (surrogate.age > surrogateLifespan) {
			surrogate = new SurrogateClassLoader();
		}

		return surrogate.defineClass(file, entry, className);
	}
}
