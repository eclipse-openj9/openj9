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

import java.util.HashSet;
import java.util.Vector;

import com.ibm.uma.om.Artifact;
import com.ibm.uma.om.Module;

import freemarker.template.TemplateModel;

public interface IPlatform {
	
	// Allows the platform to add options to individual artifacts, modify the artifacts, 
	// and/or add or delete artifacts.
	//  e.g., Calculate and add Base DLL addresses.
	public abstract void decorateModules( Vector<Module> modules ) throws UMAException;
	
	// Per Artifact API
	public abstract void writePlatformSpecificFiles(Artifact artifact) throws UMAException;
	public abstract StringBuffer writeMakefileExtras(Artifact artifact) throws UMAException;
	public abstract StringBuffer writeMakefilePostscript(Artifact artifact) throws UMAException;
	public abstract StringBuffer writeMakefileFlagsLine(Artifact artifact) throws UMAException;

	
	// Informational API
	public abstract String getObjectExtension() throws UMAException;

	// Default location to place artifact types
	public abstract String getExecutablePath() throws UMAException;
	public abstract String getSharedLibPath() throws UMAException;
	public abstract String getStaticLibPath() throws UMAException;


	// Query API
	public abstract String replaceMacro(String macro) throws UMAException;

	// Used to determine if two or more artifacts will be written to the same file.
	public abstract String getLibOnDiskName(Artifact artifact) throws UMAException;

	// Used when writing the uma_macros.mk file. 
	public abstract void addLibraryLocationInformation(Artifact artifact, StringBuffer libLocations) throws UMAException;

	// Used to add extras to the artifact makefile.
	public abstract void addTargetSpecificObjectsForStaticLink(HashSet<String> objtable, Artifact artifact) throws UMAException;
	public abstract void addArtifactSpecificMakefileInformation(Artifact artifact, StringBuffer buffer) throws UMAException;

	// Used to add platform specific targets to the root makefile.
	public abstract void writeTopLevelTargets(StringBuffer buffer) throws UMAException;
	
	// Used to implement FreeMarker Support
	public abstract String getTargetNameWithRelease(Artifact artifact) throws UMAException;
	public abstract boolean isType(String type) throws UMAException; 
	public abstract boolean isProcessor(String processor) throws UMAException;
	public abstract String[] getProcessor() throws UMAException;
	
	// Used to extend FreeMarker Support
	public abstract TemplateModel getDataModelExtension(String prefixTag, String extensionTag) throws UMAException;	
}
