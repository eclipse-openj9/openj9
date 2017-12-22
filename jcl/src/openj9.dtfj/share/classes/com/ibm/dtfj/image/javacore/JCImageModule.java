/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.javacore;

import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageSymbol;

public class JCImageModule implements ImageModule {
	
	private Vector fSections;
	private Vector fSymbols;
	private Properties fProperties;
	private final String fName;
	
	public JCImageModule(String name) {
		fName = name;
		fSections = new Vector();
		fSymbols = new Vector();
		fProperties = new Properties();
	}

	
	/**
	 * 
	 */
	public String getName() throws CorruptDataException {
		if (fName == null) {
			throw new CorruptDataException(new JCCorruptData("No name found for library", null));
		}
		return fName;
	}
	
	
	/**
	 * NON-DTFJ. Used for internal building purposes. Do NOT call outside the building process.
	 * Use the DTFJ alternative instead.
	 * 
	 */
	public String getInternalName() {
		return fName;
	}

	
	
	/**
	 * 
	 */
	public Properties getProperties() throws CorruptDataException {
		if (fProperties.size() == 0) {
			throw new CorruptDataException(new JCCorruptData("No properties available for " + fName,null));
		}
		return fProperties;
	}
	
	
	/**
	 * 
	 * @param key
	 * @param value
	 */
	public void addProperty(String key, String value) {
		fProperties.put(key, value);
	}

	
	/**
	 * 
	 */
	public Iterator getSections() {
		return fSections.iterator();
	}

	
	/**
	 * 
	 */
	public Iterator getSymbols() {
		return fSymbols.iterator();
	}

	/**
	 *
	 */
	public void addSymbol(ImageSymbol symbol) {
		if (!fSymbols.contains(symbol)) {
			fSymbols.add(symbol);
		}
	}
	
	public long getLoadAddress() throws DataUnavailable {
		throw new DataUnavailable("no load address in a javacore");
	}
}
