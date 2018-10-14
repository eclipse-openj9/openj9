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
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Level;

/**
 * Represents a class file container in the file system, this 
 * could be a directory or a jar file.
 * 
 * @author adam
 */
public class Container extends Entry {

	private final List<Entry> entries = new ArrayList<>();

	public Container(File file) {
		super(file);
	}

	@Override
	public URL toURL() {
		try {
			return file.toURI().toURL();
		} catch (MalformedURLException e) {
			logger.log(Level.FINE,"Exception thrown when constructing URL from class file name " + file.getName()); //$NON-NLS-1$
			return null;
		}
	}

	public List<Entry> getEntries() {
		return entries;
	}

	public void addEntry(Entry entry) {
		entries.add(entry);
		entry.setParent(this);
	}

	@SuppressWarnings("unchecked")
	public <T extends Entry> T getEntry(File fileToScan) {
		return (T) scanEntries(this, fileToScan);
	}

	private static Entry scanEntries(Container root, File file) {
		for (Entry entry : root.entries) {
			if (entry.getFile() == null) {
				// entry does not have a file set so could just be a container of other entries
				if (entry instanceof Container) {
					return scanEntries((Container) entry, file);
				}
			} else {
				// file has been set, so compare files
				if (entry.getFile().equals(file)) {
					return entry;
				}
			}
		}
		return null;
	}

	/**
	 * Recursively scan all nodes in the tree looking for the specified entry.
	 * 
	 * @param entry entry to be searched for
	 * @return
	 */
	public Entry getEntry(Entry entry) {
		return scanEntries(this, entry);
	}

	public Entry getEntry(String name) {
		Entry searchFor = new Entry(name);
		return scanEntries(this, searchFor);
	}

	private static Entry scanEntries(Container root, Entry searchFor) {
		for (Entry entry : root.entries) {
			if (entry instanceof Container) {
				return scanEntries((Container) entry, searchFor);
			}
			if (entry.equals(searchFor)) {
				return entry;
			}
		}
		return null;
	}

}
