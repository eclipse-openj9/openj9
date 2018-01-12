/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob.datamodel;

import java.io.PrintWriter;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import com.ibm.j9ddr.autoblob.config.Configuration;
import com.ibm.j9ddr.autoblob.linenumbers.SourceLocation;

/**
 * A user-defined type that can contain fields.
 * 
 * @author andhall
 *
 */
public abstract class RecordType extends UserDefinedType
{
	private static final int MAX_LINE_GAP_BETWEEN_DEFINITION_AND_DECLARATION = 3;

	private final List<Declaration> entries = new LinkedList<Declaration>();
	
	private SourceLocation definitionLocation;
	
	protected RecordType(String name, SourceLocation definitionLocation)
	{
		super(name, definitionLocation);
		this.definitionLocation = definitionLocation;
	}

	public List<Declaration> getEntries()
	{
		return Collections.unmodifiableList(entries);
	}

	public void addEntry(Declaration declaration)
	{
		// Catch the first entry - if it's more than 
		// a couple of lines from the declaration location,
		// then we assume declaration and definition are apart
		// - and definition is more useful to us
		
		if (entries.size() == 0) {
			SourceLocation location = declaration.getLocation();
			
			if (location != null) {
				if (definitionLocation != null) {
					if (! location.getFileName().equals(definitionLocation.getFileName())) {
						definitionLocation = location;
					} else {
						if (Math.abs(location.getLineNumber() - definitionLocation.getLineNumber()) > MAX_LINE_GAP_BETWEEN_DEFINITION_AND_DECLARATION) {
							definitionLocation = location;
						}
					}
				} else {
					definitionLocation = location;
				}
			}
		}
		
		entries.add(declaration);
	}
	
	@Override
	public boolean shouldBeInBlob()
	{
		return isComplete;
	}

	@Override
	public SourceLocation getDefinitionLocation()
	{
		return definitionLocation;
	}
	
	protected List<Declaration> getDeclaredEntries()
	{
		if (superClass != null) {
			Type working = superClass;
			while (working instanceof Typedef) {
				working = ((Typedef)working).getAliasedType();
			}
			
			RecordType rt = (RecordType) working;
			
			Set<String> superEntryNames = new HashSet<String>();
			
			for (Declaration dec : rt.getEntries()) {
				superEntryNames.add(dec.getName());
			}
			
			List<Declaration> toReturn = new LinkedList<Declaration>();
			
			for (Declaration dec : getEntries()) {
				if (! superEntryNames.contains(dec.getName())) {
					toReturn.add(dec);
				}
			}
			
			return toReturn;
		} else {
			return getEntries();
		}
	}
	
	@Override
	protected void writeFieldEntries(String name, String fieldPrefix, PrintWriter out, PrintWriter ssout)
	{
		for (Declaration dec : getDeclaredEntries()) {
			dec.writeOut(name, fieldPrefix, out, ssout);
		}
	}

	@Override
	public void applyTypeOverrides(Configuration autoblobConfiguration)
	{
		Map<String, String> overrides = autoblobConfiguration.getTypeOverrides().get(getBlobStructName());
		
		if (overrides != null) {
			applyTypeOverrides(getBlobStructName(), overrides);
		}
	}

	public void applyTypeOverrides(String name, Map<String, String> overrides)
	{
		for (Declaration entry : getEntries()) {
			String override = overrides.get(entry.getName());
			
			if (override != null) {
				entry.overrideTypeString(override);
			}
		}
		
		checkForRawSRPs(name);
	}

	private void checkForRawSRPs(String name)
	{
		for (Declaration entry : getDeclaredEntries()) {
			entry.checkForRawSRP(name);
		}
	}
	
}
