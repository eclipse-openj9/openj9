/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.uma;

import java.util.List;
import java.util.Set;

import com.ibm.uma.om.Artifact;

import freemarker.template.TemplateModel;
import freemarker.template.TemplateModelIterator;


public interface IConfiguration {

	/**
	 * Obtain the filename used to store the module meta data.
	 * 
	 * @return 	String instance containing the meta data filename.
	 */
	public abstract String getMetadataFilename();
	
	/**
	 * These methods allow for the grouping of targets
	 * into separate targets called 'phases'
	 */

	/**
	 * Query the number of phases defined for this configuration.
	 * 
	 * @return the number of phases
	 */
	public abstract int numberOfPhases();
	
	/**
	 * Converts the phase number into a text string. Phases a numbered from 0.
	 * 
	 * @param phase
	 * @return a string representing the phase numbered phase.
	 */
	public abstract String phaseName(int phase);
	
	/**
	 * Converts a text string into the phase number. Phases are numbered from 0.
	 * 
	 * @param phase
	 * @return an integer representing the phase number.
	 * @throws UMABadPhaseNameException - when the string phase does not 
	 * map to a valid phase name.
	 */
	public abstract int phaseFromString( String phase ) throws UMABadPhaseNameException;
	
	/**
	 * Obtain a list of all the phase names.  Phase 0 corresponds with
	 * the 0 entry in the list.
	 * 
	 * @return A list of all the phase names in proper order.
	 */
	public abstract String[] phases();
		
	/**
	 * Obtain the name of the configuration.
	 * 
	 * @return 	String instance containing the configuration name
	 */
	public abstract String getConfigurationName();
	
	/**
	 * Determine if a flag name is valid.
	 * 
	 * @param flag
	 * @return true - when the flag name is valid, false otherwise.
	 */
	public abstract boolean isFlagValid(String flag);
	
	/** 
	 * Determine if a flag is enabled or disabled.
	 *  
	 * @param flag
	 * @return true - when the flag is enabled or set, and false otherwise.
	 * @note will return false if the false is not valid. 
	 */
	public abstract boolean isFlagSet(String flag);
	
	/**
	 * Obtain a set of all known flags.
	 * 
	 * @return a set of all known flag names.
	 */
	public abstract Set<String> getAllFlags();
	
	/**
	 * Obtain a set of all excluded artifacts.
	 * 
	 * @return a set of all excluded artifact names.
	 */
	public abstract List<String> getExcludedArtifacts();

	/**
	 * Substitute one string for another.
	 * 
	 * @param macro
	 * @return null if the macro is not known or a string that maps to the macro string.
	 */
	public abstract String replaceMacro(String macro);
	
	/** 
	 * Define the string that would be returned when replaceMacro() is called.
	 * @param macro
	 * @param substitution
	 */
	public void defineMacro(String macro, String substitution);

	/** 
	 * Obtain the platform.
	 * 
	 * @return a reference to the platform used for this configuration.
	 * @throws UMAException
	 */
	public abstract IPlatform getPlatform() throws UMAException;
	
	/** 
	 * Obtain a string containing a copyright notice to be included in
	 * all makefiles.
	 * @return a string containing a copyright notice format for use in makefiles.
	 * @throws UMAException
	 */
	public String getMakefileCopyrightNotice() throws UMAException;
	
	/**
	 * Allows the configuration implementation to extend the UMA data model
	 * supplied to the Freemarker engine.
	 * 
	 * @param prefixTag a string representing the tree segments parsed so far
	 * @param extensionTag a string representing the next tag in the tree
	 * @return a TemplateModel that implements the extensionTag
	 * @throws UMAException
	 */
	public abstract TemplateModel getDataModelExtension(String prefixTag, String extensionTag) throws UMAException;
	
	/**
	 * Allows the configuration to list all properties available under the 
	 * uma.spec.properties tag.
	 * 
	 * @return a Freemarker iterator of all the properties available.
	 * @throws UMAException
	 */
	public abstract TemplateModelIterator getPropertiesIterator() throws UMAException;

	/** 
	 * Obtain additional includes for an artifact.
	 * 
	 * @return a string to append to the UMA_INCLUDES line
	 * @throws UMAException
	 */
	public abstract String getAdditionalIncludesForArtifact(Artifact artifact) throws UMAException;
	
}
