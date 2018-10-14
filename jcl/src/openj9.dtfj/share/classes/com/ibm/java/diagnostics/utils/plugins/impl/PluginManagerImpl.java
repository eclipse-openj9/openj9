/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2018 IBM Corp. and others
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
package com.ibm.java.diagnostics.utils.plugins.impl;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.logging.Level;
import java.util.logging.Logger;
/*[IF Sidecar19-SE]*/
import java.net.URI;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.stream.Stream;
/*[ENDIF] Sidecar19-SE */

import jdk.internal.org.objectweb.asm.ClassReader;

import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.ClassInfo;
import com.ibm.java.diagnostics.utils.plugins.ClassListener;
import com.ibm.java.diagnostics.utils.plugins.Container;
import com.ibm.java.diagnostics.utils.plugins.Entry;
import com.ibm.java.diagnostics.utils.plugins.PluginConstants;
import com.ibm.java.diagnostics.utils.plugins.PluginManager;

/**
 * Singleton manager for working with plugins.
 *
 * @author adam
 */
public class PluginManagerImpl implements PluginManager {

	protected static final Logger logger = Logger.getLogger(PluginConstants.LOGGER_NAME);

	/**
	 * the cache of scanned files for plugins
	 */
	protected final Container cache = new Container(null);

	/**
	 * specify classloader paths by URI as this can be used by the File class and converted into a URL
	 */
	protected final ArrayList<File> pluginSearchPath = new ArrayList<>();

	private static PluginManagerImpl instance = null;

	/**
	 * listeners for moving over classes
	 */
	private final Set<ClassListener> listeners = new HashSet<>();

	private URL[] classpath = null;

	public static PluginManager getPluginManager() {
		if (instance == null) {
			instance = new PluginManagerImpl();
		}
		return instance;
	}

	private PluginManagerImpl() {
		super();
		refreshSearchPath();
	}

	/**
	 * Refresh the search path. This will take it's setting in order from
	 *
	 * 1. System property : com.ibm.java.diagnostics.plugins
	 * 2. Environment variable : com.ibm.java.diagnostics.plugins
	 *
	 */
	@Override
	public void refreshSearchPath() {
		synchronized (pluginSearchPath) {
			pluginSearchPath.clear();
			// the search path can be set by a system property or environment variable
			// with the system property taking precedence
			String property = System.getProperty(PluginConstants.PLUGIN_SYSTEM_PROPERTY);
			if (null == property) {
				property = System.getenv(PluginConstants.PLUGIN_SYSTEM_PROPERTY);
			}
			if ((null == property) || (property.length() == 0)) {
				// no plugin path was specified
				logger.fine("No system property called " + PluginConstants.PLUGIN_SYSTEM_PROPERTY + " was found"); //$NON-NLS-1$ //$NON-NLS-2$
				return;
			}
			logger.fine("Plugins search path = " + property); //$NON-NLS-1$
			// break the path up and add to classloader search path
			String[] parts = property.split(File.pathSeparator);
			for (String part : parts) {
				String path = stripQuotesFromPath(part);
				pluginSearchPath.add(new File(path));
			}
		}
	}

	private static String stripQuotesFromPath(String path) {
		if ((path.length() > 0) && (path.charAt(0) == '"')) {
			// strip out any initial quote
			path = path.substring(1);
		}
		if ((path.length() > 0) && (path.charAt(path.length() - 1) == '"')) {
			// strip out trailing quote
			path = path.substring(0, path.length() - 1);
		}
		return path;
	}

	/**
	 * Scan the supplied plugin path to find classes set by the plugin search path and then
	 * examine the classes to see if any of the correct interfaces and annotations are supported.
	 *
	 * This method does not support MVS on z/OS, the path needs to point to HFS locations
	 */
	@Override
	public void scanForClassFiles() throws CommandException {
		synchronized (pluginSearchPath) {
			classpath = null; //reset the URL classpath
			for (File file : pluginSearchPath) { //a path entry can be null if the URI was malformed
				logger.fine("Scanning path " + file + " in search of plugins"); //$NON-NLS-1$ //$NON-NLS-2$
				if (!file.exists()) {
					//log that the entry does not exist and skip
					logger.fine(String.format("Skipping search path: %s does not exist", file.getAbsolutePath())); //$NON-NLS-1$
					continue;
				}
				if (file.isDirectory()) {
					scanDirectory(file);
				} else {
					scanFile(file);
				}
			}

			/*[IF Sidecar19-SE]*/
			logger.fine("Scanning path jrt:/modules/openj9.dtfjview in search of plugins"); //$NON-NLS-1$
			FileSystem fs = FileSystems.getFileSystem(URI.create("jrt:/")); //$NON-NLS-1$
			Path modules = fs.getPath("modules/openj9.dtfjview"); //$NON-NLS-1$
			listClassFiles(modules);
			/*[ENDIF] Sidecar19-SE */
		}
	}

	/*[IF Sidecar19-SE]*/
	void listClassFiles(Path path) {
		try (Stream<Path> paths = Files.list(path).filter(filePath -> !filePath.endsWith("module-info.class"))) { //$NON-NLS-1$
			paths.forEach(filePath -> {
				if (Files.isRegularFile(filePath)) {
					try {
						URL fileUrl = filePath.toUri().toURL();
						if (fileUrl.getFile().endsWith(".class")) { //$NON-NLS-1$
							InputStream is = Files.newInputStream(filePath);
							Entry entry = examineClassFile(is, fileUrl);
							if (entry != null) {
								for (ClassListener listener : listeners) {
									listener.scanComplete(entry);
								}
							}
						}
					} catch (Exception e) {
						// log and ignore exception
						logger.log(Level.FINE, "Error occurred scanning " + filePath, e); //$NON-NLS-1$
					}
				} else if (Files.isDirectory(filePath)) {
					listClassFiles(filePath);
				}
			});
		} catch (IOException e) {
			// log and ignore exception
			logger.log(Level.FINE, "Error occurred scanning " + path, e); //$NON-NLS-1$
		}
	}
	/*[ENDIF] Sidecar19-SE */

	/**
	 * Get an entry from the cache, using generics to describe the type expected.
	 * @param <T>
	 * @param file
	 * @return
	 */
	@SuppressWarnings("unchecked")
	public <T extends Entry> T getEntry(File file) {
		return (T) cache.getEntry(file);
	}

	private void scanDirectory(File dir) {
		logger.fine("Scanning directory " + dir.getAbsolutePath()); //$NON-NLS-1$
		File[] files = dir.listFiles();
		for (File file : files) {
			if (file.isDirectory()) {
				scanDirectory(file);
			} else {
				scanFile(file);
			}
		}
	}

	/**
	 * Returns the file extension
	 * @param file file to test
	 * @return everything after, and including, the last period in the file name or an empty string if there is no identifiable extension
	 */
	private static String getExtension(File file) {
		String name = file.getName();
		int pos = name.lastIndexOf('.');
		if ((-1 == pos) || (name.length() == (pos + 1))) {
			return ""; // no extension //$NON-NLS-1$
		} else {
			return name.substring(pos);
		}
	}

	/**
	 * Scans a file and determines if it can be loaded
	 * @param file
	 */
	private void scanFile(File file) {
		Entry entry = null;
		logger.fine("Scanning file " + file.getAbsolutePath()); //$NON-NLS-1$
		String ext = getExtension(file);
		if (ext.equals(Entry.FILE_EXT_JAR)) {
			if (!file.getName().equalsIgnoreCase("dtfj.jar")) { //$NON-NLS-1$
				// mask out dtfj.jar as this will cause potentially horrible circular class references
				entry = examineJarFile(file);
			}
		}
		if (ext.equals(Entry.FILE_EXT_CLASS)) {
			entry = examineClassFile(file);
			if (entry != null) {
				for (ClassListener listener : listeners) {
					listener.scanComplete(entry);
				}
			}
		}
	}

	private Entry examineClassFile(File file) {
		if (file.length() > Integer.MAX_VALUE) {
			logger.fine("Skipping file " + file.getAbsolutePath() + " as the file size is > Integer.MAX_VALUE"); //$NON-NLS-1$ //$NON-NLS-2$
			return null; //skip this file
		}
		//check to see if the class has been scanned or it's changed
		Entry entry = getEntry(file);
		if ((entry == null) || (entry.getData() == null) || entry.hasChanged(file)) {
			//it hasn't so scan it
			InputStream is = null;
			try {
				is = new FileInputStream(file);
				ClassInfo info = scanClassFile(is, file.toURI().toURL());
				if (entry == null) {
					entry = new Entry(info.getClassname(), file);
					cache.addEntry(entry);
				}
				entry.setData(info);
				entry.setSize(file.length());
				entry.setLastModified(file.lastModified());
			} catch (IOException e) {
				logger.log(Level.FINE, e.getMessage());
			} finally {
				try {
					if (is != null) {
						is.close();
					}
				} catch (IOException e) {
					logger.log(Level.FINE, "Error closing file " + file.getAbsolutePath(), e); //$NON-NLS-1$
				}
			}
		}
		return entry;
	}

	/*[IF Sidecar19-SE]*/
	private Entry examineClassFile(InputStream is, URL url) {
		Entry entry = null;
		try {
			ClassInfo info = scanClassFile(is, url);
			entry = new Entry(info.getClassname(), null);
			cache.addEntry(entry);
			entry.setData(info);
		} catch (IOException e) {
			logger.log(Level.FINE, e.getMessage());
		} finally {
			try {
				if (is != null) {
					is.close();
				}
			} catch (IOException e) {
				logger.log(Level.FINE, "Error closing file " + is, e); //$NON-NLS-1$
			}
		}
		return entry;
	}
	/*[ENDIF] Sidecar19-SE */

	private ClassInfo scanClassFile(InputStream file, URL url) throws IOException {
		ClassScanner scanner = new ClassScanner(url, listeners);
		ClassReader cr = new ClassReader(file);
		cr.accept(scanner, 0);
		return scanner.getClassInfo();
	}

	/**
	 * Scans all classes in the specified jar file
	 * @param file
	 */
	private Entry examineJarFile(File file) {
		logger.fine("Found jar file " + file.getAbsolutePath()); //$NON-NLS-1$

		// check to see if the class has been scanned or it's changed
		Container root = getEntry(file);
		if (root == null) {
			root = new Container(file);
			cache.addEntry(root);
		}
		if ((root.getData() == null) || root.hasChanged(file)) {
			JarInputStream jin = null;
			try {
				jin = new JarInputStream(new FileInputStream(file));
				for (;;) {
					JarEntry jarentry = jin.getNextJarEntry();
					if (jarentry == null) {
						break;
					}
					String entryName = jarentry.getName();
					if (jarentry.isDirectory() || !entryName.endsWith(".class")) { //$NON-NLS-1$
						// skip directories, only interested in classes
						continue;
					}
					long entrySize = jarentry.getSize();
					if (entrySize > Integer.MAX_VALUE) {
						logger.fine("Skipping jar entry " + entryName + " as the uncompressed size is > Integer.MAX_VALUE"); //$NON-NLS-1$ //$NON-NLS-2$
						continue; //skip this entry
					}
					// getNextJarEntry correctly positions the stream for sniffing.
					// Note that sniffing does not close the stream, which is the desired behaviour.
					Entry entry = new Entry(entryName);
					ClassInfo info = scanClassFile(jin, entry.toURL());
					entry.setData(info);
					entry.setSize(entrySize);
					entry.setLastModified(jarentry.getTime());
					root.addEntry(entry);
				}
			} catch (IOException e) {
				logger.log(Level.FINE, "Error reading from file " + file.getAbsolutePath(), e); //$NON-NLS-1$
			} finally {
				if (null != jin) {
					try {
						jin.close();
					} catch (IOException e) {
						logger.log(Level.FINE, "Error closing file " + file.getAbsolutePath(), e); //$NON-NLS-1$
					}
				}
			}
			// doesn't matter what the root data is set to, it acts as a marker
			// to indicate that the jar file has been scanned
			root.setData(new Object());
		}
		for (Entry entry : root.getEntries()) {
			for (ClassListener listener : listeners) {
				listener.scanComplete(entry);
			}
		}
		return root;
	}

	@Override
	public Container getCache() {
		synchronized (pluginSearchPath) {
			return cache;
		}
	}

	@Override
	public boolean addListener(ClassListener listener) {
		synchronized (pluginSearchPath) {
			//change set operations so that additions replace an existing instance
			if (listeners.contains(listener)) {
				listeners.remove(listener);
			}
			return listeners.add(listener);
		}
	}

	@Override
	public boolean removeListener(ClassListener listener) {
		synchronized (pluginSearchPath) {
			return listeners.remove(listener);
		}
	}

	@Override
	public Set<ClassListener> getListeners() {
		return listeners;
	}

	/*
	 * (non-Javadoc)
	 * @see com.ibm.java.diagnostics.utils.pluginsng.PluginManager#getClasspath()
	 *
	 * The classpath is calculated as a mixture of single class and jar entries.
	 * All entries from the cache root are either files or jar files. The URLs
	 * need to be constructed either to point directly at the jar file or
	 * relative to the path required to pick up the single jar entry.
	 */
	@Override
	public URL[] getClasspath() {
		synchronized (pluginSearchPath) {
			// path not yet calculated?
			if (classpath == null) {
				// use an intermediate set so that duplicates are not created
				final Set<URL> urls = new HashSet<>();
				for (Entry entry : cache.getEntries()) {
					try {
						// some entries won't have an associated file
						File file = entry.getFile();
						if ((file != null) && !(entry instanceof Container)) {
							// this may be a class file, so need to work out the relative root to add
							ClassInfo info = entry.getData();
							String name = info.getClassname();
							int pos = -1;
							do {
								file = file.getParentFile();
								pos = name.indexOf('.', pos + 1);
							} while ((file != null) && (pos != -1));
						}
						if (file != null) {
							urls.add(file.toURI().toURL());
						}
					} catch (Exception e) {
						logger.log(Level.FINE, "Error setting classpath for plugin " + entry.getName(), e); //$NON-NLS-1$
					}
				}
				classpath = new URL[urls.size()];
				urls.toArray(classpath);
			}
			return classpath;
		}
	}

}
