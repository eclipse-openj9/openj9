/*******************************************************************************
 * Copyright (c) 1999, 2019 IBM Corp. and others
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
package com.ibm.jpp.om;

import java.io.File;
import java.util.Properties;

/**
 * Src represents a preprocessor source folder and it's associated options (such as the jxelink
 * or msg output directories).
 */
public class Src {
	private final File srcFolder;
	private final Properties options;
	private final boolean isSimple;
	private final String srcPath;
	private final String simpleCopyOutput;

	/**
	 * Class constructor.
	 * 
	 * @param 		folderString		the source directory
	 * @param 		isSimple			<code>true</code> if this is a simple copy source
	 */
	public Src(String folderString, boolean isSimple) {
		this.srcFolder = new File(folderString);
		this.options = new Properties();
		this.isSimple = isSimple;
		this.srcPath = folderString;
		this.simpleCopyOutput = "";
	}

	/**
	 * Class constructor.
	 * 
	 * @param 		folderString		the source directory
	 * @param 		simpleCopyOutput	the simple copy output path
	 */
	public Src(String folderString, String simpleCopyOutput) {
		this.srcFolder = new File(folderString);
		this.options = new Properties();
		this.isSimple = true;
		this.srcPath = folderString;
		this.simpleCopyOutput = simpleCopyOutput;
	}

	/**
	 * Class constructor.
	 * 
	 * @param 		folderString		the source directory
	 */
	public Src(String folderString) {
		this.srcFolder = new File(folderString);
		this.options = new Properties();
		this.isSimple = false;
		this.srcPath = folderString;
		this.simpleCopyOutput = "";
	}

	/**
	 * Returns this source's relative folder.
	 * 
	 * @return		the relative folder
	 */
	public File getRelativeSrcFolder() {
		return srcFolder;
	}

	/**
	 * Returns this source's relative path.
	 * 
	 * @return		the relative path
	 */
	public String getRelativeSrcPath() {
		return srcPath;
	}

	/**
	 * Returns this source's simplecopy output.
	 * 
	 * @return		the simplecopy output
	 */
	public String getSimpleCopyOutput() {
		return simpleCopyOutput;
	}

	/**
	 * Adds an option to this source.
	 * 
	 * @param 		name		the option name
	 * @param 		value		the option value
	 */
	public void addOption(String name, String value) {
		options.setProperty(name, value);
	}

	/**
	 * Returns the options associated with this source.
	 * 
	 * @return		the source options
	 */
	public Properties getOptions() {
		return options;
	}

	/**
	 * Returns whether this source is a simple copy source.
	 * 
	 * @return		<code>true</code> if the source is simple copy, <code>false</code> otherwise
	 */
	public boolean isSimpleCopy() {
		return isSimple;
	}

	/**
	 * @see java.lang.Object#toString()
	 */
	@Override
	public String toString() {
		return srcFolder.toString() + " " + options.toString();
	}
}
