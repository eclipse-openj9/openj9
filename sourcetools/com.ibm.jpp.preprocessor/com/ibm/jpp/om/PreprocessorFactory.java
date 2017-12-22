/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class PreprocessorFactory {
	private String[] flags = new String[0];
	private Set<String> requiredIncludeFlags = new HashSet<>();
	private List<String> foundFlags = new ArrayList<>();
	private Set<String> fixedPRs = new HashSet<>();

	/**
	 * Creates a new JavaPreprocessor object and initializes it with the factory
	 * options.
	 *
	 * @return      new preprocessor
	 */
	public JavaPreprocessor newPreprocessor(File inputFile, OutputStream output) {
		return this.newPreprocessor(null, inputFile, output, null);
	}

	/**
	 * Creates a new JavaPreprocessor object, initializing it with the factory
	 * options and enabling metadata generation.
	 *
	 * @return      new preprocessor
	 */
	public JavaPreprocessor newPreprocessor(OutputStream metadata, File inputFile, OutputStream output, File outputFile) {
		JavaPreprocessor jpp = new JavaPreprocessor(metadata, inputFile, output, outputFile);
		jpp.setFlags(flags);
		jpp.setRequiredIncludeFlags(requiredIncludeFlags);
		jpp.setFoundFlags(foundFlags);
		jpp.setFixedPRs(fixedPRs);
		return jpp;
	}

	/**
	 * Returns the fixedPRs.
	 *
	 * @return Set
	 */
	public Set<String> getFixedPRs() {
		return fixedPRs;
	}

	/**
	 * Returns the flags.
	 *
	 * @return String[]
	 */
	public String[] getFlags() {
		return flags;
	}

	/**
	 * Returns the foundFlags.
	 *
	 * @return the list of found flags
	 */
	public List<String> getFoundFlags() {
		return foundFlags;
	}

	/**
	 * Sets the fixedPRs.
	 *
	 * @param fixedPRs The fixedPRs to set
	 */
	public void setFixedPRs(Set<String> fixedPRs) {
		this.fixedPRs = fixedPRs;
	}

	/**
	 * Sets the flags.
	 *
	 * @param flags The flags to set
	 */
	public void setFlags(String[] flags) {
		this.flags = flags;
	}

	/**
	 * Sets the required include flags.
	 */
	public void setRequiredIncludeFlags(Set<String> requiredIncludeFlags) {
		this.requiredIncludeFlags = requiredIncludeFlags;
	}

	/**
	 * Sets the foundFlags.
	 *
	 * @param foundFlags The foundFlags to set
	 */
	public void setFoundFlags(List<String> foundFlags) {
		this.foundFlags = foundFlags;
	}
}
