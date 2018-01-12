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
import java.util.List;
import java.util.Vector;

import com.ibm.uma.IPlatform;
import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;

public class Artifact extends PredicateList {
	
	public static final int TYPE_SHARED = 1;
	public static final int TYPE_STATIC = 2;
	public static final int TYPE_EXECUTABLE = 3;
	public static final int TYPE_SUBDIR = 4;
	public static final int TYPE_BUNDLE = 5;
	public static final int TYPE_REFERENCE = 6;
	public static final int TYPE_TARGET = 7;

	Hashtable<String, Library> allLibrariesThisArtifactDependsOn;
	boolean appendRelease = false;
	String bundle;
	boolean console = true;
	String description;
	Vector<Export> exports = new Vector<Export>();
	Vector<Flag> flags = new Vector<Flag>();
	Vector<Include> includes = new Vector<Include>();
	Vector<Library> libraries = new Vector<Library>();
	Vector<Command> commands = new Vector<Command>();
	Vector<Dependency> dependencies = new Vector<Dependency>();
	String loadgroup;
	String makefileName;
	Vector<MakefileStub> makefileStubs = new Vector<MakefileStub>();
	Module module;
	String name;
	String scope;
	boolean includeInAllTarget;
	Vector<Object> objects = new Vector<Object>();
	Hashtable<String, Option> options = new Hashtable<String,Option>();
	boolean[] phases = new boolean[UMA.getUma().getConfiguration().numberOfPhases()];
	Vector<Library> staticLinkLibraries = new Vector<Library>();
	boolean buildLocal = false;
	int type;
	Vector<VPath> vpaths = new Vector<VPath>();
	public Artifact(Module module) {
		super(module.getModulePath());
		this.module = module;
		for ( int phase=0; phase < UMA.getUma().getConfiguration().numberOfPhases(); phase ++ ) {
			phases[phase] = false;
		}
	}
	
	public Artifact copy() throws UMAException {
		Artifact copy = new Artifact(module);
		
		// Hashtable<String, Library> allLibrariesThisArtifactDependsOn; Cached when requested, no need to copy it.
		// set type
		copy.setType(getType());
		// copy description
		copy.setDescription(getDescription());
		// copy commands 
		for ( Command command : getCommands() ) {
			copy.addCommand(command);
		}
		// copy dependencies
		for ( Dependency dependency : getDependendies() ) {
			copy.addDependency(dependency);
		}
		// copy vpaths
		for ( VPath vPath : getVPaths() ) {
			copy.addVPath(vPath);
		}
		// copy options
		for ( Option option : getOptions().values()) {
			copy.addOption(option);
		}
		// copy phase information
		for ( int k=0; k<UMA.getUma().getConfiguration().numberOfPhases(); k++ ) {
			copy.setPhase(k, isInPhase(k));
		}
		// copy includes information
		for( Include include : getIncludes() ) {
			copy.addInclude(include);
		}
		// copy library information
		for ( Library library : getLibraries() ) {
			copy.addLibrary(library);
		}
		// copy static library information
		for ( Library library : getStaticLinkLibraries() ) {
			copy.addStaticLinkLibrary(library);
		}
		// copy object information
		for ( Object object : getObjects() ) {
			copy.addObject(object);
		}
		// copy makefile information
		for ( MakefileStub makefileStub : getMakefileStubs() ){
			copy.addMakefileStub(makefileStub);
		}
		// copy export information 
		for ( Export export : getExports() ) {
			copy.addExport(export);
		}
		// copy flags
		for ( Flag flag : getFlags() ) {
			copy.addFlag(flag);
		}
		// set the scope
		copy.setScope(getScope());
		// set artifact name
		copy.setName(getTargetName());
		// set makefile name
		copy.setMakefileName(getMakefileName());

		// set various flags
		copy.setBundle(getBundle());
		copy.setLoadgroup(getLoadGroup());
		copy.setBuildLocal(getBuildLocal());
		copy.setAppendRelease(appendRelease());
		copy.setConsoleApplication(isConsoleApplication());
		copy.setIncludeInAllTarget(includeInAllTarget());

		return copy;
	}
	
	public String getScope() {
		return scope;
	}
	
	
	public void setScope(String scope) {
		this.scope = scope;
		if (makefileName != null && !makefileName.startsWith(scope+"_")) {
			makefileName = scope + "_" + makefileName;
		} 
	}
	
	public void setIncludeInAllTarget(boolean includeInAllTarget) {
		this.includeInAllTarget = includeInAllTarget;
	}
	
	public boolean includeInAllTarget() {
		return includeInAllTarget;
	}

	public void addExport(Export export) {
		exports.add(export);
	}
	
	public void addFlag(Flag flag) {
		flags.add(flag);
	}
	
	public void addInclude(Include include) {
		includes.add(include);
	}
	
	public void addCommand(Command command) {
		commands.add(command);
	}
	
	public void addDependency(Dependency dependency) {
		dependencies.add(dependency);
	}
	
	public void addLibrary(Library library) {
		libraries.add(library);
	}
	
	public void addMakefileStub(MakefileStub makefileStub) {
		makefileStubs.add(makefileStub);
	}
	
	public void addObject(Object object) {
		objects.add(object);
	}
	
	public void addOption(Option option) {
		options.put(option.getName(), option);
	}
	
	public void addStaticLinkLibrary(Library library) {
		staticLinkLibraries.add(library);
	}
	
	public void addVPath(VPath vpath) {
		vpaths.add(vpath);
	}
	
	public boolean appendRelease() {
		return appendRelease;
	}
	
	@Override
	public boolean evaluate() throws UMAException {
		/* 
		 * short circuit evaluation if this artifact was specified to be always excluded.
		 */
		List<String> excludedArtifacts = UMA.getUma().getConfiguration().getExcludedArtifacts();
		if ( excludedArtifacts.contains(name)) return false;
		return super.evaluate();
	}
	
	public boolean flagSet(String flag) throws UMAException {
		Option option = options.get(flag);
		if ( option == null || !option.evaluate() ) return false;
		return true;
	}
	
	public String[] getAllLibrariesInTree() {
		String[] libs = new String[libraries.size()];
		for ( int i=0; i<libraries.size(); i++ ) {
			libs[i] = libraries.elementAt(i).getName();
		}
		return libs;
	}
	
	public Hashtable<String, Library> getAllLibrariesThisArtifactDependsOn() throws UMAException {
		if ( allLibrariesThisArtifactDependsOn == null ) {
			allLibrariesThisArtifactDependsOn = new Hashtable<String, Library>();
			for ( Library lib : libraries ) {

				/* Check to see if the library should be included */
				if (!lib.evaluate()) {
					continue;
				}
				
				if ( allLibrariesThisArtifactDependsOn.get(lib.getName()) == null ) {
					switch ( lib.getType() ) {
					case Library.TYPE_BUILD:
						Artifact artifact = UMA.getUma().getArtifact(lib.getName());
						if ( artifact == null || !artifact.evaluate()) {
							// this lib is not being built ignore it
							continue;
						}

						if ( lib.getName().equals(getArtifactKey()) ) {
							throw new UMAException("circular dependency artifact [" + getArtifactKey()+ "] defined in [" + getContainingFile() + "] depends on itself via artifact [" + artifact.getArtifactKey()+ "] defined in [" + artifact.getContainingFile() + "]" );
						}
							
						if ( artifact.isInBundle() && (!isInBundle() || !artifact.getBundle().equalsIgnoreCase(getBundle()))) {
							if ( allLibrariesThisArtifactDependsOn.get(artifact.getBundle()) == null ) {
								Library bundleLib = new Library(containingFile);
								bundleLib.setName(artifact.getBundle());
								bundleLib.setType("build");
								allLibrariesThisArtifactDependsOn.put(artifact.getBundle(), bundleLib);
							}
						} else {
							allLibrariesThisArtifactDependsOn.put(lib.getName(), lib);
						}

						Hashtable<String, Library> hasht = artifact.getAllLibrariesThisArtifactDependsOn();
						for ( String libname : hasht.keySet() ) {
							if ( allLibrariesThisArtifactDependsOn.get(libname) == null ) {
								if ( libname.equals(getArtifactKey()) ) {
									throw new UMAException("circular dependency artifact [" + getArtifactKey()+ "] defined in [" + getContainingFile() + "] depends on itself via artifact [" + artifact.getArtifactKey()+ "] defined in [" + artifact.getContainingFile() + "]" );
								}
								allLibrariesThisArtifactDependsOn.put(libname, hasht.get(libname));
							}
						}
						break;
						default:
							allLibrariesThisArtifactDependsOn.put(lib.getName(), lib);
						break;
					}
				}
			}
		}
		return allLibrariesThisArtifactDependsOn;
	}
	
	public String getBundle() {
		return bundle;
	}
		
	public Module getContainingModule() {
		return module;
	}
	
	public String[] getData(String name) throws UMAException {
		Option option = options.get(name);
		if ( option == null || !option.evaluate() ) return null;
		return option.getData();
	}
	
	public String getDescription() {
		return description;
	}
	
	public Vector<Export> getExportedFunctions() throws UMAException {
		if ( type == TYPE_BUNDLE ) {
			return getBundleExportedFunctions();
		} else {
			return getArtifactExportedFunctions();
		}
	}
	
	public Vector<Export> getArtifactExportedFunctions() throws UMAException {
		Vector<Export> ef = new Vector<Export>();
		for ( Export export : exports ) {
			if ( !export.evaluate() ) continue;
			switch ( export.getType() ) {
			case Export.TYPE_FUNCTION:
				ef.add(export);
				break;
			case Export.TYPE_GROUP:
				Exports expGroup = module.getExports().get(export.getExport());
				if ( expGroup == null ) {
					throw new UMAException("Error: export group " + export.getExport() + " not found in module " + module.getModulePath() + ".");
				}
				if ( expGroup.evaluate() ) { 
					for ( Export exp : expGroup.getExports() ) {
						if ( !exp.evaluate() ) continue;
						switch ( exp.getType() ) {
						case Export.TYPE_FUNCTION:
							ef.add(exp);
							break;
						case Export.TYPE_GROUP:
							throw new UMAException("Error: export group " + export.getExport() + " refers to another group [" + exp.getExport() +"] in module " + module.getModulePath() + ".");
						}
					}
				}
			}
		}
		return ef;
	}
	
	private Export addArtifactPrefix(Artifact artifact, Export export)
	{
		String exportName = export.getExport();
		if (exportName.equals("JVM_OnLoad") || exportName.equals("JVM_OnUnload") || exportName.equals("J9VMDllMain") || exportName.equals("JNI_OnLoad") || exportName.equals("JNI_OnUnload") ) {
			Export copy = new Export(export.containingFile);
			copy.setExport(artifact.getTargetName() + "_" + exportName);
			copy.setType(export.getType());
			return copy;
		}

		return export;
	}
	
	public Vector<Export> getBundleExportedFunctions() throws UMAException {
		Vector<Export> ef = new Vector<Export>();
		Vector<Artifact> bundleArtifacts = getAllArtifactsInThisBundle();
		for ( Artifact artifact : bundleArtifacts ) {
			for ( Export export : artifact.exports ) {
				if ( !export.evaluate() ) continue;
				switch ( export.getType() ) {
				case Export.TYPE_FUNCTION:
					ef.add( addArtifactPrefix(artifact, export) );
					break;
				case Export.TYPE_GROUP:
					Exports expGroup = artifact.module.getExports().get(export.getExport());
					if ( expGroup == null ) {
						throw new UMAException("Error: export group " + export.getExport() + " not found in module " + module.getModulePath() + ".");
					}
					if ( expGroup.evaluate() ) { 
						for ( Export exp : expGroup.getExports() ) {
							if ( !exp.evaluate() ) continue;
							switch ( exp.getType() ) {
							case Export.TYPE_FUNCTION:
								ef.add( addArtifactPrefix(artifact, exp) );
								break;
							case Export.TYPE_GROUP:
								throw new UMAException("Error: export group " + export.getExport() + " refers to another group [" + exp.getExport() +"] in module " + module.getModulePath() + ".");
							}
						}
					}
				}
			}
		}
		return ef;
	}
	
	
	public Vector<Export> getExports() {
		return exports;
	}
	
	public Vector<Flag> getFlags() throws UMAException {
		Vector<Flag> processedFlags = flags;
		if (flags.size() > 0) {
			processedFlags = new Vector<Flag>();
			for (Flag f : flags) {
				if (f == null) {
					continue;
				}
				//System.out.println("Flag: " + (f == null ? "null" : f.getName()));
				switch(f.type) {
				case Flag.SINGLE:
					processedFlags.add(f);
					break;
				case Flag.GROUP:
					//System.out.println("Group Flag: " + f.getName() + "  group:"+ f.groupName);
					Flags group = module.getFlags().get(f.groupName);
					if (group == null) {
						throw new UMAException("Error: flags group " + f.groupName + " not found in module " + module.getModulePath() + ".");
					}
					for (Flag groupFlag : group.getFlags()) {
						//System.out.println("Group Flag: " + groupFlag.getName() + "  group:"+ f.groupName);
						switch(groupFlag.type) {
						case Flag.SINGLE:
							processedFlags.add(groupFlag);
							break;
						case Flag.GROUP:
							throw new UMAException("Error: flags group " + groupFlag.groupName + " inside group in  " + module.getModulePath() + ".");
						}
					}
					break;
				}
			}
		}
		return processedFlags;
	}
	
	public Vector<Include> getIncludes() {
		return includes;
	}
	
	public Vector<Command> getCommands() {
		return commands;
	}
	
	public Vector<Dependency> getDependendies() {
		return dependencies;
	}
	
	
	public String getArtifactKey() {
		String artifactKey = getTargetName();
		if ( getScope() != null ) {
			artifactKey = getScope() + "_" + artifactKey;
		}
		return artifactKey;
	}
	
	public Hashtable<String, Artifact> getAllArtifactsInThisBundleInHashtable() throws UMAException {
		Hashtable<String, Artifact> table = new Hashtable<String, Artifact>();
		for ( Artifact artifact : getAllArtifactsInThisBundle() ) {
			table.put(artifact.getArtifactKey(), artifact);
		}
		return table;
	}
	
	public Vector<Artifact> getAllArtifactsInThisBundle() throws UMAException {
		Vector<Artifact> artifactsInThisBundle = new Vector<Artifact>();
		for ( Module module : UMA.getUma().getModules() ) {
			if (!module.evaluate()) continue;
			for ( Artifact artifact : module.getArtifacts() ) {
				if (!artifact.evaluate()) continue;
				String bundle = artifact.getBundle();
				if (bundle != null && bundle.equalsIgnoreCase(getTargetName())) {
					artifactsInThisBundle.add(artifact);
				}
			}
		}
		return artifactsInThisBundle;
	}
	
	public Boolean isInBundle() throws UMAException {
		if ( getBundle() != null ) {
			Artifact bundleArtifact = UMA.getUma().getBundleArtifact(getBundle());
			if ( bundleArtifact != null && bundleArtifact.evaluate() ) {
				return true;
			}
		}
		return false;
	}
	
	public boolean isBundle() {
		return type == TYPE_BUNDLE;
	}
	
	public Vector<Library> getLibraries() throws UMAException {
		if ( type == TYPE_BUNDLE ) {
			//TODO cache this result instead calculating it everytime
			Vector<Library> bundleLibraries = new Vector<Library>(libraries);
			Vector<Artifact> bundleArtifacts = getAllArtifactsInThisBundle();
			for ( Artifact artifact : bundleArtifacts ) {
				Library artifactLib = new Library(containingFile);
				artifactLib.setName(artifact.getTargetName());
				artifactLib.setType("build");
				bundleLibraries.add(artifactLib);
				bundleLibraries.addAll(artifact.getLibraries());
			}
			return bundleLibraries;
		}
		return libraries;
	}
	
	public String getLoadGroup() {
		return loadgroup;
	}
	
	public String getMakefileName() {
		return makefileName;
	}
	
	public String getMakefileFileName() throws UMAException {
		if (module.requiresDispatchMakefile()) return getMakefileName() + ".mk";
		return "makefile";
	}
	
	public Vector<MakefileStub> getMakefileStubs() {
		return makefileStubs;
	}
	public Vector<Object> getObjects() {
		return objects;
	}
	
	public Vector<VPath> getVPaths() {
		return vpaths;
	}
	
	public Hashtable<String, Option> getOptions() {
		return options;
	}
	
	public Vector<Library> getStaticLinkLibraries() throws UMAException {
		Vector<Library> combinedStaticLibraries = new Vector<Library>(staticLinkLibraries);
		for ( Library library : libraries ) {
			if ( !library.evaluate() ) continue;
			if ( library.getType() == Library.TYPE_BUILD ) {
				Artifact libArtifact = UMA.getUma().getArtifact(library.getName());
				if ( libArtifact == null || !libArtifact.evaluate() ) continue;
				if ( libArtifact.isInBundle() ) {
					// need to augment static libraries with libraries that make
					// up this bundle
					Library refLibrary = new Library("static-link-created");
					refLibrary.setName(libArtifact.getTargetName());
					combinedStaticLibraries.add(refLibrary);
				}
			}
			
		}
		return combinedStaticLibraries;
	}
	
	public String getTargetName() {
		return name;
	}
	
	public String getTargetNameWithScope() {
		if (scope == null ) return getTargetName();
		return scope + "_" + name;
	}
	
	public String getTargetPath() {
		if (buildLocal) {
			return module.getFullName() + "/"; 
		}
		return null;
	}
	
	public boolean getBuildLocal() {
		return buildLocal;
	}
	
	public String getTargetPath(IPlatform platform) throws UMAException {
		if ( getTargetPath() != null ) return getTargetPath();
		switch ( type ) {
		case TYPE_EXECUTABLE:
			return platform.getExecutablePath();
		case TYPE_BUNDLE: // fall-thru
		case TYPE_SHARED:
			return platform.getSharedLibPath();
		case TYPE_STATIC:
			return platform.getStaticLibPath();
		}
		return "";
	}
	
	public String getTargetPathAsStatic(IPlatform platform) throws UMAException {
		if ( getTargetPath() != null ) return getTargetPath();
		return platform.getStaticLibPath();
	}
	
	public int getType() throws UMAException {
		if ( type == TYPE_SHARED && isInBundle() ) {
			type = TYPE_STATIC;
		}
		return type;
	}
	
	public boolean isConsoleApplication() {
		return console;
	}
	
	public boolean isInPhase(int phase) throws UMAException {
		return phases[phase];
	}
	
	public boolean isLibDefinedInTree( String lib ) {
		return lib.equals(name);
	}
	public boolean requires(Artifact oa) throws UMAException {
		for ( Library library : libraries ) {
			if (!library.evaluate()) continue;
			if ( library.getName().equalsIgnoreCase(oa.getTargetName()) ) {
				return true;
			}
			Artifact libArt = UMA.getUma().getArtifact(library.getName());
			if ( libArt != null && libArt.evaluate() && libArt.requires(oa) ) {
				return true;
			}
		}
		return false;		
	}
	public void setAppendRelease(boolean appendRelease) {
		this.appendRelease = appendRelease;
	}
	public void setBundle(String bundle) {
		this.bundle = bundle;
	}
	public void setConsoleApplication( boolean console ) {
		this.console = console;
	}
	
	public void setDescription(String desc) {
		description = desc;
	}
	
	public void setLoadgroup(String loadgroup) {
		this.loadgroup = loadgroup;
		if (loadgroup == null || loadgroup.equals("")) {
			this.loadgroup = getTargetName();
		}
	}
	
	public void setMakefileName(String nm) {
		makefileName = nm;
		if (scope != null && !makefileName.startsWith(scope+"_")) {
			makefileName = scope + "_" + makefileName;
		}
	}
	
	public void setName(String name) {
		this.name = name;
		// default is to set makefileName to name
		makefileName = name;
		if (scope != null && !makefileName.startsWith(scope+"_")) {
			makefileName = scope + "_" + makefileName;
		} 
	}
	
	public void setPhase(int phase, boolean value) {
		try {
			phases[phase] = value;
		} catch (ArrayIndexOutOfBoundsException e) {
			// Bad phase passed in, ignore.
		}
	}
	
	public void setBuildLocal(boolean  buildLocal) {
		this.buildLocal = buildLocal;
	}
	
	public void setType(String type) {
		if ( type.equalsIgnoreCase("shared")) {
			this.type = TYPE_SHARED;
		} else if ( type.equalsIgnoreCase("static")) {
			this.type = TYPE_STATIC;
		} else if ( type.equalsIgnoreCase("executable")) {
			this.type = TYPE_EXECUTABLE;
		} else if ( type.equalsIgnoreCase("subdir") ) {
			this.type = TYPE_SUBDIR;
		} else if ( type.equalsIgnoreCase("bundle") ) {
			this.type = TYPE_BUNDLE;
		} else if ( type.equalsIgnoreCase("reference") ) {
			this.type = TYPE_REFERENCE;
		} else if ( type.equalsIgnoreCase("target") ) {
			this.type = TYPE_TARGET;
		} 
	}
	
	private void setType(int type) {
		this.type = type;
	}
	
	
}
