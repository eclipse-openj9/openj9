/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
import java.io.File;
import java.io.IOException;
import java.io.StringWriter;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Vector;

import com.ibm.uma.freemarker.TemplateLoader;
import com.ibm.uma.om.Artifact;
import com.ibm.uma.om.Command;
import com.ibm.uma.om.Dependency;
import com.ibm.uma.om.Include;
import com.ibm.uma.om.Library;
import com.ibm.uma.om.MakefileStub;
import com.ibm.uma.om.Module;
import com.ibm.uma.om.Object;
import com.ibm.uma.om.Objects;
import com.ibm.uma.om.SubdirArtifact;
import com.ibm.uma.om.VPath;
import com.ibm.uma.om.parser.Parser;
import com.ibm.uma.util.FileAssistant;
import com.ibm.uma.util.Logger;

import freemarker.template.Configuration;
import freemarker.template.DefaultObjectWrapper;
import freemarker.template.Template;
import freemarker.template.TemplateException;
import freemarker.template.TemplateModel;

public class UMA {
	
	public static final String UMA_TARGETS_IN_TREE = "UMA_TARGETS_IN_TREE";
	public static final String UMA_TARGETS_IN_TREE_VAR = "$("+UMA_TARGETS_IN_TREE+")";
	public static final String UMA_PATH_TO_ROOT = "UMA_PATH_TO_ROOT";
	public static final String UMA_PATH_TO_ROOT_VAR = "$("+UMA_PATH_TO_ROOT+")";
	public static final String OMR_DIR = "OMR_DIR";
	public static final String OMR_DIR_VAR = "$("+OMR_DIR+")";
	public static final String UMA_TARGET_MAKEFILE = "targets.mk";
	public static final String UMA_MAKELIB_DIR = "makelib";
	public static final String UMA_MACROS_FILE = "uma_macros.mk";
	public static final String UMA_MACROS_PATH_FROM_ROOT = UMA_MAKELIB_DIR + "/" + UMA_MACROS_FILE;
	public static final String UMA_TARGET_MAKEFILE_WITH_PATH = UMA_MAKELIB_DIR + "/" + UMA_TARGET_MAKEFILE;
	public static final String UMA_MKCONSTANTS_FILE = "mkconstants.mk";
	public static final String UMA_MKCONSTANTS_PATH_FROM_ROOT = UMA_MAKELIB_DIR + "/" + UMA_MKCONSTANTS_FILE;
	public static final String UMA_ARTIFACT_CFLAGS_LABEL = "A_CFLAGS";
	public static final String UMA_ARTIFACT_CXXFLAGS_LABEL = "A_CXXFLAGS";
	public static final String UMA_ARTIFACT_CPPFLAGS_LABEL = "A_CPPFLAGS";
	public static final String UMA_ARTIFACT_ASFLAGS_LABEL = "A_ASFLAGS";

	public static final String FREEMARKER_UMA = "uma";
	

	public static final int NUM_OBJECTS_PER_LINE=10;

	static UMA uma;
	
	public static UMA getUma() {
		return uma;
	}

	public IPlatform getPlatform() {
		return  platform;
	}
	
	IPlatform platform;
	IConfiguration configuration;
	Configuration freemarkerConfig;
	ISinglePredicateEvaluator singlePredicateEvaluator;
	Vector<Module> modules;
	Hashtable<String, Artifact> artifactMap = new Hashtable<String,Artifact>();
	Hashtable<String, Artifact> artifactLocationMap = new Hashtable<String, Artifact>();
	String rootDirectory;
	TemplateLoader templateLoader;
	String phasePrefix;
	
	static final String UMA_MAKEFILE_PHASE_PREFIX = "makefile_phase_prefix";
	
	public UMA(IConfiguration configuration, ISinglePredicateEvaluator singlePredicateEvaluator, String rootDir) throws UMAException {
		this.configuration = configuration;
		this.platform = configuration.getPlatform();
		this.singlePredicateEvaluator = singlePredicateEvaluator;
		this.rootDirectory = rootDir;
		if ( !rootDir.endsWith(File.separator) ) this.rootDirectory += File.separator;

		// ensure that the root directory exists before proceeding.
		File file = new File(this.rootDirectory);
		if ( !file.exists() ) {
			throw new UMAException("Error: Source directory " + file.getAbsolutePath() + " does not exist.");
		}

		UMA.uma = this;
		
		phasePrefix = configuration.replaceMacro(UMA_MAKEFILE_PHASE_PREFIX);
		if ( phasePrefix == null ) {
			phasePrefix = "";
		} else {
			phasePrefix += " ";
		}


		// Initialize the Freemarker configuration
		freemarkerConfig = new Configuration();
		// Specify how templates will see the data model. 
		freemarkerConfig.setObjectWrapper(new DefaultObjectWrapper());
		this.templateLoader = new TemplateLoader();
		freemarkerConfig.setTemplateLoader(this.templateLoader);
	}
	
	public IConfiguration getConfiguration() {
		return configuration;
	}
	
	public ISinglePredicateEvaluator getSinglePredicateEvaluator() {
		return singlePredicateEvaluator;
	}
		
	public Vector<Module> getModules() {
		return modules;
	}
	
	public void addArtifact( Artifact artifact ) throws UMAException {
		String artifactKey = artifact.getArtifactKey();
		Artifact artifactInMap = artifactMap.get(artifactKey);
		if ( artifactInMap != null ) {
			throw new DuplicateArtifactKeyException("Duplicate artifact key found [" + artifactKey + "] in file " + artifact.getContainingFile() + " already found in file " + artifactInMap.getContainingFile() );
		}
		artifactMap.put(artifactKey, artifact);		
	}
	
	public void validateArtifactLocations() throws DuplicateArtifactKeyException, UMAException {

		// Artifacts won't know if they are in bundles until all artifacts are created
		for (Artifact artifact : getArtifacts().values()) {
			if ( artifact.evaluate() ) {
				String artifactLocationKey = platform.getLibOnDiskName(artifact);
				Artifact artifactLocationInMap = artifactLocationMap.get(artifactLocationKey);
				if ( artifactLocationInMap != null ) {
					throw new DuplicateArtifactKeyException("Duplicate artifact locations found [" + artifactLocationKey + "] in file " + artifact.getContainingFile() + " already found in file " + artifactLocationInMap.getContainingFile() );
				}
				artifactLocationMap.put(artifactLocationKey, artifact);
			}
		}		
	}
	
	public Artifact getArtifact(String artifactName) {
		return artifactMap.get(artifactName);
	}

	public Artifact getBundleArtifact(String artifactName) throws UMAException {
		for (Artifact artifact : artifactMap.values()) {
			if (artifact.isBundle()) {
				if (artifact.getTargetName().equals(artifactName)) {					
					return artifact;
				}
			}
		}
		return null;
	}
	
	public Hashtable<String, Artifact> getArtifacts() {
		return artifactMap;
	}
	

	
	public String getRootDirectory() {
		return rootDirectory;
	}
	
	public void generate() throws UMAException {

		Parser parser = new Parser(this);
		if ( !parser.parse() ) {
			return;
		}

		modules = parser.getModules();

		platform.decorateModules(modules);

		// Freemarker invocation

		try {
			Map<String, TemplateModel> map = new HashMap<String, TemplateModel>();
			map.put(FREEMARKER_UMA, new com.ibm.uma.freemarker.UMA());
			for ( String tmp : templateLoader.getTemplates() ) {
				Template template = freemarkerConfig.getTemplate(tmp);
				String filename = tmp.substring(0, tmp.lastIndexOf(".ftl"));
				StringWriter writer = new StringWriter();
				template.process(map, writer);
				new FileAssistant(this.rootDirectory+filename, writer.getBuffer()).writeToDisk();
			}
		} catch (IOException e) {
			throw new UMAException(e);

		} catch (TemplateException e) {
			throw new UMAException(e);
		}

		writeTargetInformationMacrosFile();

		// top level module is the first in the list.
		Module rootModule = null;
		for ( Module module : modules ) {
			if ( module.isTopLevel() ) {
				if ( rootModule != null ) {
					throw new UMAException("Error: more than one root module " + rootModule.getFullName() + " and " + module.getFullName());
				}
				rootModule = module;
			}
		}
		if ( rootModule == null ) {
			throw new UMAException("Error: no " + configuration.getMetadataFilename() + " found in the root directory " + getRootDirectory());
		}
		writeDirectoryMakefile(rootModule);

		Logger.getLogger().println(Logger.InformationL1Log, "Complete");
	}
	
	class LineWrapMakefileDirective {
		int itemCount = 0;
		int maxItems = NUM_OBJECTS_PER_LINE;
		String underMaxSeperator = " ";
		String overMaxSeperator = "\\\n  ";
		StringBuffer stringBuffer;
		
		LineWrapMakefileDirective(StringBuffer stringBuffer) {
			this.stringBuffer = stringBuffer;
		}
		
		LineWrapMakefileDirective(StringBuffer stringBuffer, String underMaxSeperator, String overMaxSeperator) {
			this.stringBuffer = stringBuffer;
			this.underMaxSeperator = underMaxSeperator;
			this.overMaxSeperator = overMaxSeperator;
		}
		
		void addItem(String item) {
			if (itemCount<maxItems) {
				itemCount++;
				stringBuffer.append(underMaxSeperator);
			} else {
				itemCount = 1;
				stringBuffer.append(overMaxSeperator);
			}
			stringBuffer.append(item);
		}
	}
	
	//  This function takes a module and creates a makefile for the directory,
	//  then recursively calls this function on all sub-modules.
	void writeDirectoryMakefile(Module module) throws UMAException {
		if ( !module.evaluate() ) return;
		
		String modulePath = new File(module.getModulePath()).getParent();
		Logger.getLogger().println(Logger.InformationL2Log, "Producing makefile(s) for " + modulePath );

		for ( Artifact artifact : module.getArtifacts() ) {
			writeMakefileForArtifact(artifact);
		}
		
		if ( module.requiresDispatchMakefile() ) {
			// There needs to be a dispatch makefile
			String makefileName = modulePath + File.separator +"makefile";
			StringBuffer buffer = new StringBuffer();
			FileAssistant fa = new FileAssistant(makefileName, buffer);
			buffer.append(configuration.getMakefileCopyrightNotice());
			String pathToRoot = determinePathToRoot(module);


			Vector<String> targets = new Vector<String>();
			Vector<String> specialTargets = new Vector<String>();
			for ( Artifact artifact : module.getAllArtifactsInTree() ) {
				if ( !artifact.evaluate() ) continue;
				if ( artifact.getType() == Artifact.TYPE_TARGET ) {
					if ( artifact.includeInAllTarget() ) {
						/* 
						 * The separation of targets into targets and specialTargets
						 * is being done to allow the 'TARGET' type artifacts to be built first.
						 * This is a weak way of saying that the whole system is dependent on 
						 * these targets without having to add the dependency to each and every
						 * artifact.
						 * 
						 * This only works when not using -j on gmake and because
						 * gmake will do the targets in the order they are on the dependency line.
						 * 
						 */
						specialTargets.add(artifact.getMakefileName());
					}
				} else {
					if (artifact.includeInAllTarget()) {
						targets.add(artifact.getMakefileName());
					}
				}
			}

			buffer.append(UMA_PATH_TO_ROOT+"=" + pathToRoot + "\n");
			buffer.append(OMR_DIR+"?=" + pathToRoot + "omr\n");
			StringBuffer targetsInTreeBuffer = new StringBuffer();
			LineWrapMakefileDirective lwmd = new LineWrapMakefileDirective(targetsInTreeBuffer);
			targetsInTreeBuffer.append(UMA_TARGETS_IN_TREE+"=");
			for ( String target : specialTargets ) {
				lwmd.addItem(target);
			}
			for ( String target : targets ) {
				lwmd.addItem(target);
			}
			buffer.append(targetsInTreeBuffer.toString());

			buffer.append("\n\ninclude " + UMA_PATH_TO_ROOT_VAR + UMA_MKCONSTANTS_PATH_FROM_ROOT + "\n");
			buffer.append("include " + UMA_PATH_TO_ROOT_VAR + UMA_MACROS_PATH_FROM_ROOT + "\n\n");

			buffer.append(phasePrefix + "all: " + UMA_TARGETS_IN_TREE_VAR + "\n\n");

			buffer.append("clean: $(addprefix clean_," + UMA_TARGETS_IN_TREE_VAR + ")\n\n");

			buffer.append("ddrgen: $(addprefix ddrgen_," + UMA_TARGETS_IN_TREE_VAR + ")\n\n");

			buffer.append(
					"static: UMA_STATIC_BUILD=1\n" +
					"	export UMA_STATIC_BUILD\n\n" +
					"static:\n" +
			"	$(MAKE) -f makefile\n\n\n");


			for ( int phase=0; phase<configuration.numberOfPhases(); phase++ ) {
				Vector<String> phaseTargets = new Vector<String>();
				Vector<String> specialPhaseTargets = new Vector<String>();
				for ( Artifact artifact : module.getAllArtifactsInTree() ) {
					if ( !artifact.evaluate() ) continue;
					if ( !artifact.isInPhase(phase) ) continue;
					if ( artifact.getType() == Artifact.TYPE_TARGET ) {
						/* 
						 * The separation of targets into phaseTargets and specialPhaseTargets
						 * is being done to allow the 'TARGET' type artifacts to be built first.
						 * This is a weak way of saying that the whole system is dependent on 
						 * these targets without having to add the dependency to each and every
						 * artifact.
						 * 
						 * This only works when not using -j on gmake and because
						 * gmake will do the targets in the order they are on the dependency line.
						 * 
						 */
						specialPhaseTargets.add(artifact.getMakefileName());
					} else {
						phaseTargets.add(artifact.getMakefileName());
					}
				}
				
				buffer.append(phasePrefix + "phase_" + configuration.phaseName(phase) + ":");
				StringBuffer targetsInPhaseBuffer = new StringBuffer();
				lwmd = new LineWrapMakefileDirective(targetsInPhaseBuffer);
				for ( String target : specialPhaseTargets ) {
					lwmd.addItem(target);
				}
				for ( String target : phaseTargets ) {
					lwmd.addItem(target);
				}
				buffer.append( targetsInPhaseBuffer.toString() + "\n" );
			}

			buffer.append( "\n" );

			for ( Artifact artifact : module.getAllArtifactsInTree() ) {
				if ( !artifact.evaluate() ) continue;
				switch (artifact.getType()) {
				case Artifact.TYPE_SUBDIR:
					// Ignore this type of artifact.  It is only used 
					// to glue the tree together.
					break;
				case Artifact.TYPE_TARGET:
					buffer.append( "clean_" + artifact.getMakefileName() + ":\n" );
					for ( Command command : artifact.getCommands() ) {
						if ( !command.evaluate() || command.getType() != Command.TYPE_CLEAN ) continue;
						buffer.append("\t" + command.getCommand() + "\n");
					}
					buffer.append( "\n" );

					buffer.append( "ddrgen_" + artifact.getMakefileName() + ":\n" );
					for ( Command command : artifact.getCommands() ) {
						if ( !command.evaluate() || command.getType() != Command.TYPE_DDRGEN ) continue;
						buffer.append("\t" + command.getCommand() + "\n");
					}
					buffer.append( "\n" );

					buffer.append( artifact.getMakefileName() + ":" );
					for ( Dependency dependency : artifact.getDependendies() ) {
						if ( !dependency.evaluate() ) continue;
						buffer.append( " " + dependency.getDependency() );
					}
					buffer.append( "\n" );
					for ( Command command : artifact.getCommands() ) {
						if ( !command.evaluate() || command.getType() != Command.TYPE_ALL ) continue;
						buffer.append("\t" + command.getCommand() + "\n");
					}
					buffer.append( "\n" );
					break;
				default:
					buffer.append( "clean_" + artifact.getMakefileName() + ":\n\t$(" + artifact.getMakefileName() + "_build) clean\n\n");
					buffer.append( "ddrgen_" + artifact.getMakefileName() + ":\n\t$(" + artifact.getMakefileName() + "_build) ddrgen\n\n");
					buffer.append( artifact.getMakefileName() + ": $(" + artifact.getMakefileName() + "_dependencies)\n\t$(" + artifact.getMakefileName() + "_build)\n\n");
				break;
				}
			}

			if ( module.isTopLevel() ) {
				platform.writeTopLevelTargets(buffer);
			}

			buffer.append(".PHONY: all clean ddrgen");
			StringBuffer phonyTargetsBuffer = new StringBuffer();
			lwmd = new LineWrapMakefileDirective(phonyTargetsBuffer);
			for ( String phonyTarget : specialTargets ) {
				lwmd.addItem(phonyTarget);
				lwmd.addItem(" clean_" + phonyTarget);
			}
			for ( String phonyTarget : targets ) {
				lwmd.addItem(phonyTarget);
				lwmd.addItem(" clean_" + phonyTarget);
			}
			buffer.append(phonyTargetsBuffer.toString()+"\n");
			fa.writeToDisk();

		}
		
		for ( Artifact artifact : module.getArtifacts() ) {
			if ( artifact.getType() == Artifact.TYPE_SUBDIR ) {
				SubdirArtifact subdirArtifact = (SubdirArtifact) artifact;
				writeDirectoryMakefile(subdirArtifact.getSubdirModule());
			}
		}
	}
	
	void addDependencies( Vector<Library> libraries, Hashtable<String, LinkedHashSet<String>> buildDependencies, Artifact artifact ) throws UMAException {
		String target = artifact.getMakefileName();
		LinkedHashSet<String> deps = buildDependencies.get(target);
		if ( deps == null ) {
			deps = new LinkedHashSet<String>();
			buildDependencies.put(target, deps);
		}
		for ( Library library : libraries ) {
			if ( library.evaluate() ) {
				if ( library.getType() != Library.TYPE_BUILD )  continue;
				Artifact libArtifact = getArtifact(library.getName());
				if ( libArtifact == null ) continue;
				String libName = libArtifact.getMakefileName();
				if ( libArtifact.isInBundle() ) {
					Artifact bundle = getBundleArtifact(libArtifact.getBundle());
					if (artifact != bundle) {
						/* If we are not dealing with the bundle itself, then substitute the 
						 * bundle name for the library. */
						libName = bundle.getMakefileName();
					}					
				} 
				if ( !deps.contains(libName) && !libName.equalsIgnoreCase(target)) { 
					deps.add(libName);
				}
			}
		}
		for ( Dependency artifactDependency : artifact.getDependendies() ) {
			if ( artifactDependency.evaluate() ) {
				String dependency = artifactDependency.getDependency();
				if ( !deps.contains(dependency) && !dependency.equalsIgnoreCase(target)) { 
					deps.add(dependency);
				}
			}
		}
	}
	
	void expandLibraryDependencies(Artifact artifact, StringBuffer libLocations) throws UMAException {
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE: // FALL-THRU
		case Artifact.TYPE_STATIC: // FALL-THRU
		case Artifact.TYPE_SHARED: 
			libLocations.append(artifact.getTargetNameWithScope() + "_deps=" + artifact.getTargetName());
			Hashtable<String, Library> libs = artifact.getAllLibrariesThisArtifactDependsOn();
			for ( Library lib : libs.values() ) {
				if ( lib.evaluate() ) {
					switch ( lib.getType() ) {
					case Library.TYPE_MACRO:
						String mlibs = platform.replaceMacro(lib.getName());
						if (mlibs != null) {
							libLocations.append(" " + mlibs);
						}
						continue;
					}
					libLocations.append(" " + lib.getName());
				}
			}
			
			/* for bundles emit a dependency on element in the bundle */
			if (artifact.getType() == Artifact.TYPE_BUNDLE) {
				String bundleName = artifact.getTargetName();
				for (Artifact artifactCursor : artifactMap.values()) {					
					if (!artifactCursor.evaluate()) {
						continue;
					}
					if (!artifactCursor.isInBundle()) {
						continue;
					}
					if (!artifactCursor.getBundle().equalsIgnoreCase(bundleName)) {
						continue;
					}
					libLocations.append(" " + artifactCursor.getArtifactKey());
				}
			}
			libLocations.append("\n\n");
			break;
		}
	}
	

	
	void writeTargetInformationMacrosFile() throws UMAException {
		/*
		 *  Buffer for information about which libraries are static vs. shared 
		 *  and their locations on disk and references on the target and link lines.
		 */
		StringBuffer libLocations = new StringBuffer();
		/* 
		 * Buffer for target rules.
		 */
		StringBuffer targetRules = new StringBuffer();

		/*
		 * A couple of hashtable to gather information an artifacts build dependencies.
		 */
		Hashtable<String, LinkedHashSet<String>> buildDependencies = new Hashtable<String,LinkedHashSet<String>>();
		Hashtable<String, LinkedHashSet<String>> staticBuildDependencies = new Hashtable<String,LinkedHashSet<String>>();

		for ( Module module : modules) {
			if ( !module.evaluate() ) continue;
			// determine build dependencies
			for (Artifact artifact : module.getArtifacts()) {
				if ( !artifact.evaluate() ) continue;
				switch (artifact.getType()) {
				case Artifact.TYPE_BUNDLE: // Fall Through
				case Artifact.TYPE_SHARED:
					libLocations.append(artifact.getTargetNameWithScope()+"_alllib="+artifact.getTargetName()+"\n");
					libLocations.append(artifact.getTargetNameWithScope()+"_sharedlib="+artifact.getTargetName()+"\n");
					platform.addLibraryLocationInformation(artifact, libLocations);
					expandLibraryDependencies(artifact, libLocations);
					break;
				case Artifact.TYPE_STATIC:
					libLocations.append(artifact.getTargetNameWithScope()+"_alllib="+artifact.getTargetName()+"\n");
					libLocations.append(artifact.getTargetNameWithScope()+"_staticlib="+artifact.getTargetName()+"\n");
					platform.addLibraryLocationInformation(artifact, libLocations);
					expandLibraryDependencies(artifact, libLocations);
					break;
				case Artifact.TYPE_EXECUTABLE:
					break;
				}

				switch (artifact.getType()) {
				case Artifact.TYPE_BUNDLE: // Fall Through
				case Artifact.TYPE_SHARED: // Fall Through
				case Artifact.TYPE_STATIC: // Fall Through
				case Artifact.TYPE_EXECUTABLE: // Fall Through
					targetRules.append(
							artifact.getMakefileName() + "_build" + "=+$(MAKE) $(UMA_WINDOWS_PARRALLEL_HACK) -C " + UMA_PATH_TO_ROOT_VAR + artifact.getContainingModule().getFullName() + " -f " + artifact.getMakefileFileName() + "\n");
					break;
				}

				addDependencies( artifact.getLibraries(), buildDependencies, artifact);
				addDependencies( artifact.getStaticLinkLibraries(), staticBuildDependencies, artifact);
			}
		}

		/*
		 * Write uma macros helper makefile.  This file contains several variable definitions that allow
		 * easy reference to information about the artifacts. e.g., 
		 */
		File makelibDirectory = new File(UMA.getUma().getRootDirectory() + UMA_MAKELIB_DIR);
		makelibDirectory.mkdir();
		FileAssistant fa = new FileAssistant(UMA.getUma().getRootDirectory() + UMA_MACROS_PATH_FROM_ROOT );
		StringBuffer buffer = fa.getStringBuffer();
		buffer.append("# UMA Macro Helper Definition Makefile\n" );
		buffer.append(configuration.getMakefileCopyrightNotice());
		buffer.append(libLocations.toString()+"\n\n\n");

		for (String target : buildDependencies.keySet()) {
			buffer.append( "\n");
			for ( String dependency : buildDependencies.get(target) ) {
				buffer.append(target + "_dependencies+=$(findstring " + dependency + ","+ UMA_TARGETS_IN_TREE_VAR +")\n");
			}
			buffer.append("\n");
		}
		buffer.append("\n\nifdef UMA_STATIC_BUILD\n\n");
		for (String target : staticBuildDependencies.keySet()) {
			buffer.append( "\n");
			for ( String dependency : staticBuildDependencies.get(target) ) {
				buffer.append(target + "_dependencies+=$(findstring " + dependency + ","+ UMA_TARGETS_IN_TREE_VAR +")\n");
			}
			buffer.append("\n");
		}
		buffer.append("\n\nendif\n\n");

		buffer.append( targetRules.toString() );

		fa.writeToDisk();
	}
	
	void writeMakefileForArtifact(Artifact artifact) throws UMAException {
		if ( !artifact.evaluate() ||
				artifact.getType() == Artifact.TYPE_SUBDIR ||
				artifact.getType() == Artifact.TYPE_REFERENCE ||
				artifact.getType() == Artifact.TYPE_TARGET) return;

		Module module = artifact.getContainingModule();
		String modulePath = new File(module.getModulePath()).getParent();
		String makefileName = artifact.getMakefileFileName();
		makefileName =  modulePath + File.separator + makefileName;
		
		platform.writePlatformSpecificFiles(artifact);

		FileAssistant fa = new FileAssistant(makefileName);
		StringBuffer buffer = fa.getStringBuffer();
			buffer.append("# Makefile for '" + artifact.getMakefileName() + "'\n" );
			buffer.append(configuration.getMakefileCopyrightNotice());
			String pathToRoot = determinePathToRoot(module);
			buffer.append(UMA_PATH_TO_ROOT+"=" + pathToRoot + "\n");
			buffer.append(OMR_DIR+"?=" + pathToRoot + "omr\n");
			switch ( artifact.getType() ) {
			case Artifact.TYPE_BUNDLE: // fall-thru
			case Artifact.TYPE_SHARED:
				buffer.append("UMA_TARGET_TYPE=DLL\n");
				break;
			case Artifact.TYPE_EXECUTABLE:
				buffer.append("UMA_TARGET_TYPE=EXE\n");
				break;
			case Artifact.TYPE_STATIC:				
				buffer.append("UMA_TARGET_TYPE=LIB\n");
				break;
			}
			buffer.append("UMA_TARGET_NAME=" + artifact.getTargetNameWithScope() + "\n");
			
			/* On OSX, <LIB_NAME_DYLIB>.dylib should match the -install_name <LIB_NAME_INSTALL>. 
			 * Otherwise, the linker is unable to find the required library. Currently, LIB_NAME_DYLIB
			 * doesn't match LIB_NAME_INSTALL. LIB_NAME_DYLIB has major and minor build versions
			 * appended whereas LIB_NAME_INSTALL doesn't contain version info. UMA_LIB_NAME will contain
			 * LIB_NAME_INSTALL with major and minor build versions.
			 */
			buffer.append("UMA_LIB_NAME=" + platform.getTargetNameWithRelease(artifact) + "\n");
			
			if ( artifact.getTargetPath() != null ) {
				buffer.append("UMA_TARGET_PATH=" + UMA_PATH_TO_ROOT_VAR + artifact.getTargetPath()+"\n");
			}
			if ( artifact.flagSet("isCPlusPlus") ) {
				buffer.append("UMA_IS_C_PLUS_PLUS=1\n");
			}
			platform.addArtifactSpecificMakefileInformation(artifact, buffer);

			buffer.append("\ninclude "+UMA_PATH_TO_ROOT_VAR + UMA_MKCONSTANTS_PATH_FROM_ROOT + "\n\n");
			writeMakefileIncludeLine(buffer, module, artifact);
			writeMakefileLibraryLine(buffer, module, artifact);
			buffer.append(platform.writeMakefileFlagsLine(artifact).toString());
			writeMakefileObjectLine(buffer, module, artifact);
			buffer.append(platform.writeMakefileExtras(artifact).toString());
			writeMakefileStubs(buffer, module, artifact);
			writeMakefileVPathInformation(buffer, module, artifact);
			buffer.append(platform.writeMakefilePostscript(artifact).toString());
			if (artifact.getCommands().isEmpty()) {
				buffer.append("\ninclude "+UMA_PATH_TO_ROOT_VAR+UMA_TARGET_MAKEFILE_WITH_PATH+"\n");
			} else {
				/* Support custom recipes for building a target.
				 * 
				 * If module.xml specifies <commands /> for this artifact, we will use the commands
				 * to build the on-disk artifact, instead of the rules in targets.mk.
				 */ 
				
				/* uma_macros.mk defines the names of the ondisk artifact and its dependencies */
				buffer.append("include " + UMA_PATH_TO_ROOT_VAR + UMA_MACROS_PATH_FROM_ROOT + "\n\n");
				
				/* Defines the default target.
				 * 
				 * $($(target)_ondisk): $($(target)_dependencies)
				 * 		commands with type="all"
				 */ 
				buffer.append("$(" + artifact.getTargetNameWithScope() + "_ondisk): ");
				buffer.append("$(" + artifact.getTargetNameWithScope() + "_dependencies)\n");
				for (Command command : artifact.getCommands()) {
					if (!command.evaluate() || command.getType() != Command.TYPE_ALL)
						continue;
					buffer.append("\t" + command.getCommand() + "\n");
				}
				buffer.append("\n");

				/* Defines the clean target.
				 * 
				 * clean:
				 * 		commands with type="clean"
				 */
				buffer.append("clean:\n");
				for (Command command : artifact.getCommands()) {
					if (!command.evaluate() || command.getType() != Command.TYPE_CLEAN)
						continue;
					buffer.append("\t" + command.getCommand() + "\n");
				}
				buffer.append("\n");
				
				/* Defines the ddrgen target.
				 * 
				 * ddrgen:
				 * 		commands with type="ddrgen"
				 */
				buffer.append("ddrgen:\n");
				for (Command command : artifact.getCommands()) {
					if (!command.evaluate() || command.getType() != Command.TYPE_DDRGEN)
						continue;
					buffer.append("\t" + command.getCommand() + "\n");
				}
				buffer.append("\n");
			}
			fa.writeToDisk();
	}

	void writeMakefileVPathInformation( StringBuffer buffer, Module module, Artifact artifact ) throws UMAException {
		for ( VPath vpath : artifact.getVPaths() ) {
			if ( !vpath.evaluate() ) continue;
			if ( vpath.getPath() == null ) continue;
			// vpath for a any pattern
			buffer.append("vpath " + vpath.getPattern() + " ");
			if ( vpath.getType() == VPath.TYPE_ROOT_PATH ) buffer.append( UMA_PATH_TO_ROOT_VAR );
			buffer.append(vpath.getPath() + "\n");
			if ( vpath.isAugmentObjects() ) {
				buffer.append("UMA_OBJECTS+=" + vpath.getObjectFile() + "\n");
			}
			if ( vpath.isAugmentIncludes() ) {
				buffer.append("UMA_INCLUDES+=");
				if ( vpath.getType() == VPath.TYPE_ROOT_PATH ) buffer.append( UMA_PATH_TO_ROOT_VAR );
				buffer.append(vpath.getPath() + "\n");
			}

		}
	}

	void writeMakefileIncludeLine( StringBuffer buffer, Module module, Artifact artifact ) throws UMAException {
		Vector<Include> includes = artifact.getIncludes();

		buffer.append("UMA_INCLUDES=. " + configuration.getAdditionalIncludesForArtifact(artifact));

		for ( Include include : includes ) {
			if (include.evaluate()) {
				if ( include.getType() == VPath.TYPE_ROOT_PATH ) buffer.append( UMA_PATH_TO_ROOT_VAR );
				buffer.append(include.getPath() + " ");
			}
		}
		buffer.append("\n");
	}

	/**
	 * @param artifact	artifact being built
	 * @param type The type of library to find.
	 * @param forStatic Whether these libraries are for an UMA_STATIC_BUILD
	 * @param libArtifact artifact being linked in
	 * @throws UMAException
	 */
	private String getLibraryDependenciesOfType( Artifact artifact, int type, boolean forStatic, Artifact libArtifact ) throws UMAException {

		String libName = libArtifact.getTargetNameWithScope();

		if ( type == Library.TYPE_BUILD ) {
			/* static versions use the library name without any tweaks */
			if (forStatic) {
				return libName + " ";
			}

			/* unbundled artifacts don't require special treatment */
			if (!libArtifact.isInBundle()) {
				return libName + " ";
			}
		}

		StringBuilder stringBuilder = new StringBuilder();

		/* executables/external DLL's should link in the *actual* code, not the bundle */
		if (!artifact.isInBundle() && (artifact.getType() == Artifact.TYPE_EXECUTABLE) ) {

			if ( type == Library.TYPE_BUILD ) {
				stringBuilder.append(libName + " ");
			}

			for (Library inheritedLib : libArtifact.getLibraries()) {
				if (!inheritedLib.evaluate()) {
					continue;
				}

				Artifact inheritedLibArtifact = getArtifact(inheritedLib.getName());
				if ( inheritedLib.getType() == Library.TYPE_BUILD) {
					/* Do not include the library if its artifact is not included */
					if (null == inheritedLibArtifact || !inheritedLibArtifact.evaluate()) {
						continue;
					}
				}

				if (inheritedLib.getType() == type) {
					/* macro libraries will have their name mangled */
					if (type == Library.TYPE_MACRO) {
						if (null != inheritedLibArtifact && !inheritedLibArtifact.evaluate()) {
							continue;
						}
						String libs  = platform.replaceMacro(inheritedLib.getName());
						if ( libs != null ) {
							stringBuilder.append(libs + " ");
						}
					} else {
						stringBuilder.append(inheritedLib.getName() + " ");
					}
				}


			}
			return stringBuilder.toString();
		}

		if ( type == Library.TYPE_BUILD ) {
			/* if we are building the bundle then used unmodified names */
			Artifact bundle = getBundleArtifact(libArtifact.getBundle());
			if (bundle == artifact) {
				return stringBuilder.toString();
			}

			/* otherwise the library is in a bundle and we should substitute the bundle name (ugh) */
			stringBuilder.append(bundle.getArtifactKey() + " ");
			return stringBuilder.toString();
		}

		return "";
	}

	/**
	 * @param artifact artifact being built
	 * @param libraries The list of libraries which we are through.
	 * @param type The type of library to find.
	 * @param forStatic Whether these libraries are for an UMA_STATIC_BUILD
	 * @throws UMAException
	 */
	String getLibrariesOfType( Artifact artifact, Vector<Library> libraries, int type, boolean forStatic ) throws UMAException {
		if ( libraries.size() < 1 )
			return "";

		StringBuilder stringBuilder = new StringBuilder();

		for ( Library library : libraries ) {
			if ( library.evaluate()) {
				if ( library.getType() == Library.TYPE_BUILD ) {
					/* If the library is an artifact, we want to include its dependencies as
					 * well as itself.
					 */
					Artifact libArtifact = getArtifact(library.getName());
					if ( libArtifact != null ) { 
						if ( libArtifact.evaluate() ) {
							stringBuilder.append(getLibraryDependenciesOfType(artifact, type, forStatic, libArtifact));
						}
					} else {
						Logger.getLogger().println(Logger.WarningLog, "Warning: unknown library reference " + library.getName() + " in: " + artifact.getContainingFile());
					}
				} else if (library.getType() == type) {
					/* macro libraries will have their name mangled */
					if (type == Library.TYPE_MACRO) {
						String libs  = platform.replaceMacro(library.getName());
						if ( libs != null ) {
							stringBuilder.append(libs + " ");
						}
					} else {
						stringBuilder.append(library.getName() + " ");
					}
				}
			}
		}
		return stringBuilder.toString();
	}

	void writeMakefileLibraryLine( StringBuffer buffer, Module module, Artifact artifact ) throws UMAException {
		if ( artifact.getType() != Artifact.TYPE_BUNDLE && artifact.getType() != Artifact.TYPE_SHARED && artifact.getType() != Artifact.TYPE_EXECUTABLE) return;
		Vector<Library> libraries = artifact.getLibraries();

		String libs = getLibrariesOfType(artifact, libraries, Library.TYPE_BUILD, false);
		libs = libs + getLibrariesOfType(artifact, libraries, Library.TYPE_SYSTEM, false);
		libs = libs + getLibrariesOfType(artifact, libraries, Library.TYPE_MACRO, false);
		if (libs.length() != 0) {
			buffer.append("UMA_LIBRARIES := " + libs + "\n");
		}

		libs = getLibrariesOfType(artifact, libraries, Library.TYPE_EXTERNAL, false);
		if (libs.length() != 0) {
			buffer.append("UMA_EXTERNAL_LIBRARIES := " + libs + "\n");
		}

		/* Static libraries will have their dependencies compiled in */
		libraries = artifact.getStaticLinkLibraries();
		libs = getLibrariesOfType(artifact, libraries, Library.TYPE_BUILD, true);
		if (libs.length() != 0) {
			buffer.append("UMA_STATIC_LIBRARIES := " + libs + "\n");
		}
	}

		void writeMakefileObjectLine( StringBuffer buffer, Module module, Artifact artifact ) throws UMAException {
			// Add all objects into a set, eliminating duplicates.
			HashSet<String> objtable = new HashSet<String>();
			Vector<Object> objects = artifact.getObjects();
			for ( Object object : objects ) {
				if ( object.evaluate() ) {
					switch (object.getType()) {
					case Object.SINGLE:
						if ( !objtable.contains(object.getName()) ) {
							objtable.add(object.getName());
						}
						break;
					case Object.GROUP:
						Vector<String> newGroups = new Vector<String>();
						newGroups.add(object.getName());
						while ( newGroups != null ) {
							Vector<String> groups = newGroups;
							newGroups = null;
							for ( String groupName : groups ) {
								Objects objs = module.getObjects().get(groupName);
								if ( objs == null ) {
									throw new UMAException("Error: bad object group name [" +groupName+"] in " + module.getModulePath() +"." );
								}
								if ( objs.evaluate() ) {
									for ( Object obj : objs.getObjects()) {
										if ( obj.evaluate() ) {
											switch (obj.getType()) {
											case Object.SINGLE:
												if ( !objtable.contains(obj.getName()) ) {
													objtable.add(obj.getName());
												}
												break;
											case Object.GROUP:
												if ( newGroups == null ) {
													newGroups = new Vector<String>();
												}
												newGroups.add(obj.getName());
												break;
											}
										}
									}
								}
							}
						}
						break;
					} // switch
				}
			}

			StringBuffer objectBuffer = new StringBuffer();
			if ( objtable.size() >= 1 ) {
				produceObjectLine(objectBuffer, objtable, "UMA_OBJECTS");
			}
			HashSet<String> targetSpecificObjects = new HashSet<String>();
			produceObjectLine(objectBuffer, targetSpecificObjects, "UMA_OBJECTS+");
			
			objtable = new HashSet<String>();
			platform.addTargetSpecificObjectsForStaticLink(objtable, artifact);
			if ( objtable.size() >= 1) {
				produceObjectLine(objectBuffer, objtable, "UMA_STATIC_OBJECTS");
			}
			if ( objectBuffer.length() > 0 ) {
				buffer.append(objectBuffer.toString());
			}
		}
		
		void produceObjectLine( StringBuffer objectBuffer, HashSet<String> objtable, String tag) {
			objectBuffer.append(tag + "=");
			int objectsWritten = 0; 
			for ( String obj : objtable ) {
				objectBuffer.append(" " + obj);
				objectsWritten ++;
				if ( objectsWritten % UMA.NUM_OBJECTS_PER_LINE == 0 ) {
					objectBuffer.append("\\\n");
				}
			}
			objectBuffer.append("\n");
		}
		
		void writeMakefileStubs( StringBuffer buffer, Module module, Artifact artifact ) throws UMAException {
			Vector<MakefileStub> makefileStubs = artifact.getMakefileStubs();
			if ( !artifact.appendRelease() ) {
				buffer.append("UMA_DO_NOT_APPEND_RELEASE_TO_DLL=1\n");
			}
			for ( MakefileStub stub : makefileStubs ) {
				if ( stub.evaluate() ) { 
					String[] stubs = stub.toString().split("\\\\n");
					for (int k=0; k<stubs.length; k++) {
						buffer.append(stubs[k] + "\n");
					}
				}
			}
		}
		
		String determinePathToRoot( Module module ) {
			String pathToRoot = "";
			int depth = module.getModuleDepth();
			for ( int i=0; i<depth; i++ ) {
				pathToRoot = pathToRoot + "../";
			}
			if (pathToRoot.equals("")) return "./";
			return pathToRoot;
		}
}
