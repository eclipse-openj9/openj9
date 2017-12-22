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
package com.ibm.uma.om;
import java.util.Hashtable;
import java.util.Vector;

import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;

public class Module extends PredicateList {
	Vector<Artifact> artifacts = new Vector<Artifact>();
	Hashtable<String,Exports> exports = new Hashtable<String, Exports>();
	Hashtable<String, Objects> objects = new Hashtable<String, Objects>();
	Hashtable<String, Flags> flags = new Hashtable<String, Flags>(); 
	String modulePath;
	String fullName;
	boolean rootModule = false;
	int moduleDepth;
		
	public Module(String modulePath) {
		super(modulePath);
		this.modulePath = modulePath;
		
		String rootDir = UMA.getUma().getRootDirectory();
		String cleanedPath = modulePath.replace("\\", "/");
		String[] pathParts = cleanedPath.substring(rootDir.length()).split("/"); 
		if ( pathParts.length == 1 || pathParts[pathParts.length-2].equals("")) {
			rootModule = true;
			this.fullName = "";
			this.moduleDepth = 0;
		} else {
			this.fullName = modulePath.substring(rootDir.length(),modulePath.length()-UMA.getUma().getConfiguration().getMetadataFilename().length()-1);
			this.moduleDepth = pathParts.length-1;
		} 
		
		//System.out.println("modulePath " + modulePath + "\nfullName " + this.fullName + "\nmoduleDepth " + this.moduleDepth );
	}
	
	public void addArtifact(Artifact artifact) throws UMAException {
		if ( artifact.getType() == Artifact.TYPE_SUBDIR ) {
			artifacts.insertElementAt(artifact,0);
		} else {
			artifacts.add(artifact);
		}
	}
	
	public void addExports(String group, Exports exps) {
		if ( exports.containsKey(group) ) {
			exports.get(group).addExports(exps);
		} else {
			exports.put(group, exps);
		}
	}
	
	public void addObjects(String group, Objects objs) {
		objects.put(group, objs);
	}
	
	public void addFlags(String group, Flags f) {
		flags.put(group, f);
	}
	
	public boolean isSet(String moduleProperty) {
		return false;
	}
	
	public String getValue(String moduleProperty) {
		return "";
	}
	
	public boolean isInPhase(int phase) throws UMAException {
		for ( int j=0; j<artifacts.size(); j++ ) {
			Artifact artifact = artifacts.elementAt(j);
			if ( !artifact.evaluate()) continue;
			if ( artifact.isInPhase(phase) ) return true;
		}
		return false;
	}
	
	public boolean containsArtifact(String artifactName) throws UMAException {
		if ( artifactName == null ) return false;
		for ( Artifact artifact : artifacts ) {
			if ( !artifact.evaluate() ) continue;
			switch (artifact.getType()) {
			case Artifact.TYPE_BUNDLE:
			case Artifact.TYPE_SHARED:
			case Artifact.TYPE_EXECUTABLE:
			case Artifact.TYPE_STATIC:
				if ( artifact.getTargetName().equals(artifactName)) return true;
				break;
			case Artifact.TYPE_SUBDIR:
				SubdirArtifact subdirArtifact = (SubdirArtifact) artifact;
				if ( subdirArtifact.getSubdirModule().containsArtifact(artifactName) ) return true;
				break;
			}
			
		}
		return false;
	}
	
	public boolean containsNonStaticArtifacts() throws UMAException {
		for ( Artifact artifact : artifacts ) {
			if ( !artifact.evaluate() ) continue;
			switch (artifact.getType()) {
			case Artifact.TYPE_EXECUTABLE: //Fall-Thru
			case Artifact.TYPE_SHARED: //Fall-Thru
			case Artifact.TYPE_BUNDLE:
				return true;
			case Artifact.TYPE_SUBDIR:
				SubdirArtifact subdirArtifact = (SubdirArtifact) artifact;
				if ( subdirArtifact.getSubdirModule().containsNonStaticArtifacts() ) return true;
				break;
			}
			
		}
		return false;
	}
	
	public Vector<Artifact> getArtifacts() {
		return artifacts;
	}
	
	public Vector<Artifact> getSubdirArtifacts() throws UMAException {
		Vector<Artifact> subdirs = new Vector<Artifact>();
		boolean artifactNotAdded = true;
		int iterations = 0;
		while( artifactNotAdded ) {
			artifactNotAdded = false;
			iterations ++;
			if ( iterations > artifacts.size() ) {
				throw new UMAException("Error: dependency loop detected in tree rooted at " + this.getFullName());
			}
			for ( Artifact a : artifacts ) {
				if ( a.getType() != Artifact.TYPE_SUBDIR ) continue;
				if ( subdirs.contains(a) ) continue;
				int index = -2;
				for ( String lib : a.getAllLibrariesInTree()) {
					for ( Artifact b : artifacts ) {
						if ( b == a || b.getType() != Artifact.TYPE_SUBDIR ) continue;
						if ( b.isLibDefinedInTree(lib) ) {
							// A library used by the current subdir 'a' is defined 
							// inside of subdir 'b', thus 'a' goes before 'b'.
							// determine the index of 'b' and see if we are already
							// scheduled to go in earlier.
							int index2 = subdirs.indexOf(b);
							if ( (index2 > index  && index != -1) || index2 == -1 ) {
								index = index2;
							}
							//System.out.println( a.getTargetName() + " [" + index2 + "] depends on " + b.getTargetName());
						}
					}
				}	
				if ( index == -1 ) {
					// artifact has dependencies not yet inserted.
					artifactNotAdded = true;
				} else if ( index == -2 ) {
					// Found no dependencies, so insert a end
					subdirs.insertElementAt(a, 0);
				} else {
					// insert before any dependencies
					subdirs.insertElementAt(a, index+1);
				}
			}
		}
		return subdirs;
	}

	public Vector<Artifact> getStaticArtifacts() throws UMAException {
		Vector<Artifact> statics = new Vector<Artifact>();
		for ( Artifact a : artifacts ) {
			if ( a.getType() == Artifact.TYPE_STATIC) {
				statics.add(a);
			}
		}
		return statics;
	}

	public Vector<Artifact> getSharedArtifacts() throws UMAException {
		Vector<Artifact> shareds = new Vector<Artifact>();
		for ( Artifact a : artifacts ) {
			if ( a.getType() == Artifact.TYPE_SHARED || a.getType() == Artifact.TYPE_BUNDLE) {
				shareds.add(a);
			}
		}
		return shareds;
	}

	public Vector<Artifact> getExecutableArtifacts() throws UMAException {
		Vector<Artifact> executables = new Vector<Artifact>();
		for ( Artifact a : artifacts ) {
			if ( a.getType() == Artifact.TYPE_EXECUTABLE) {
				executables.add(a);
			}
		}
		return executables;
	}

	public Hashtable<String,Exports> getExports() {
		return exports;
	}
	
	public Hashtable<String,Objects> getObjects() {
		return objects;
	}
	
	public Hashtable<String,Flags> getFlags() {
		return flags;
	}
	
	public String getModulePath() {
		return modulePath;
	}
	
	public String getFullName() {
		return fullName;
	}
	
	public String getLogicalFullName() {
		if (rootModule) return "root";
		return "root/" + fullName;
	}
	
	public int getModuleDepth() {
		return moduleDepth;
	}
	
	public String moduleNameAtDepth(int depth) {
		String[] moduleNames = getLogicalFullName().split("/");
		return moduleNames[depth];
	}
	
	public boolean isTopLevel() {
		if ( rootModule ) return true;
		return false;
	}
	
	public String getTopLevelName() {
		return moduleNameAtDepth(0);
	}
	
	public String[] getParents() {
		int modDepth = getModuleDepth();
		if ( modDepth == 0 ) return null;
		String[] parents = new String[modDepth];
		String parent = getTopLevelName();
		parents[0] = parent;
		for ( int depth = 1; depth < modDepth; depth++ ) {
			parent = parent + "/" + moduleNameAtDepth(depth);
			parents[depth] = parent;
		}
		return parents;
	}
	
	public boolean containsEnabledArtifacts() throws UMAException {
		for ( Artifact artifact : artifacts ) {
			if ( !artifact.evaluate() ) continue;
			if ( artifact.getType() == Artifact.TYPE_SUBDIR ) {
				SubdirArtifact subdirArtifact = (SubdirArtifact) artifact;
				if ( subdirArtifact.getSubdirModule().containsEnabledArtifacts() )	return true;
			} else {
				return true;
			}
		}
		return false;
	
	}
	
	public boolean requiresDispatchMakefile() throws UMAException {
		int numEnabledArtifacts = 0;
		for ( Artifact artifact : artifacts ) {
			if ( !artifact.evaluate() ) continue;
			if ( artifact.getType() == Artifact.TYPE_SUBDIR ) {
				SubdirArtifact subdirArtifact = (SubdirArtifact) artifact;
				if ( subdirArtifact.getSubdirModule().containsEnabledArtifacts() )	return true;
			} else {
				numEnabledArtifacts ++;
			}
		}
		if ( numEnabledArtifacts > 1 ) return true;
		return false;
	}
	
	public Vector<Artifact> getAllArtifactsInTree() throws UMAException {
		Vector<Artifact> allArtifacts = new Vector<Artifact>();
		for ( Artifact artifact : artifacts ) {
			if ( !artifact.evaluate() ) continue;
			if (artifact.getType() == Artifact.TYPE_REFERENCE) continue;
			if (artifact.getType() == Artifact.TYPE_SUBDIR) {
				Module subdirModule = ((SubdirArtifact)artifact).getSubdirModule();
				allArtifacts.addAll(subdirModule.getAllArtifactsInTree());
			} else {
				allArtifacts.add(artifact);
			}
		}
		return allArtifacts;
	}
	
	public String[] getAllLibrariesInTree() {
		Vector<String> allLibs = new Vector<String>();
		for ( Artifact artifact : artifacts ) {
			for ( String lib : artifact.getAllLibrariesInTree() ) {
				allLibs.add(lib);
			}
		}
		String[] libs = new String[allLibs.size()];
		allLibs.toArray(libs);
		return libs;
	}
	
	public boolean isLibDefinedInTree( String lib ) {
		for ( Artifact artifact : artifacts ) {
			if ( artifact.isLibDefinedInTree(lib) ) return true;
		}
		return false;
	}
}

