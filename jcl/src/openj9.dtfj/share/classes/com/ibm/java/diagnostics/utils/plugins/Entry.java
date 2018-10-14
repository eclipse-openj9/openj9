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
package com.ibm.java.diagnostics.utils.plugins;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Describes an entry in a file system. It could be a class file or a container.
 * 
 * @author adam
 */
public class Entry {

	protected static final Logger logger = Logger.getLogger(PluginConstants.LOGGER_NAME);

	/**
	 * jar file extension
	 */
	public static final String FILE_EXT_JAR = ".jar"; //$NON-NLS-1$

	/**
	 * class file extension
	 */
	public static final String FILE_EXT_CLASS = ".class"; //$NON-NLS-1$

	private final String name;
	private long lastModified = -1;
	private long size = -1;
	private Container parent = null;
	private URL url = null; // URL for this entry
	protected File file = null; // file that this entry corresponds to in the file system
	protected Object data = null; // data associated with this entry

	public Entry(String name) {
		super();
		this.name = name;
	}

	public Entry(String name, File file) {
		this(name);
		this.file = file;
		if (file != null) {
			size = file.length();
			lastModified = file.lastModified();
		}
	}

	public Entry(File file) {
		this((file != null) ? file.getName() : "root", file); //$NON-NLS-1$
	}

	public long getLastModified() {
		return lastModified;
	}

	public void setLastModified(long lastModified) {
		this.lastModified = lastModified;
	}

	public long getSize() {
		return size;
	}

	public void setSize(long size) {
		this.size = size;
	}

	public String getName() {
		return name;
	}

	public void setParent(Container parent) {
		this.parent = parent;
	}

	public Container getParent() {
		return parent;
	}

	public URL toURL() {
		if (url == null) {
			if (file != null) {
				try {
					url = file.toURI().toURL();
				} catch (MalformedURLException e) {
					logger.log(Level.FINE, "Exception thrown when constructing URL from file name " + file.getAbsolutePath()); //$NON-NLS-1$
				}
			} else if (parent != null && parent.getName().endsWith(FILE_EXT_JAR)) {
				String jarPath = parent.getFile().getAbsolutePath();
				try {
					url = new URL("jar:file:" + jarPath + "!/" + name); //$NON-NLS-1$ //$NON-NLS-2$
				} catch (MalformedURLException e) {
					logger.log(Level.FINE, "Exception thrown when constructing URL from jar file name " //$NON-NLS-1$
							+ jarPath + " and class file name " + name); //$NON-NLS-1$
				}
			}
		}
		return url;
	}

	public File getFile() {
		return file;
	}

	public boolean hasChanged(Entry previous) {
		if (file != null) {
			if (previous.file == null) {
				return false;
			}
		} else {
			if (!name.equals(previous.name)) {
				return false;
			}
		}
		return (size != previous.size) || (lastModified != previous.lastModified);
	}

	public boolean hasChanged(File previous) {
		if (file == null) {
			return true; //always assumed to have changed if the file has not been set
		}
		if (file.equals(previous)) {
			return (size != previous.length()) || (lastModified != previous.lastModified());
		}
		return true;
	}

	@SuppressWarnings("unchecked")
	public <T> T getData() {
		return (T) data;
	}

	public void setData(Object data) {
		this.data = data;
	}

	@Override
	public boolean equals(Object o) {
		if ((o == null) || !(o instanceof Entry)) {
			return false;
		}
		Entry compareTo = (Entry) o;
		return name.equals(compareTo.name);
	}

	@Override
	public int hashCode() {
		return name.hashCode();
	}

	@Override
	public String toString() {
		if (file == null) {
			return name;
		} else {
			return name + " loaded from " + file.getAbsolutePath(); //$NON-NLS-1$
		}
	}

}
