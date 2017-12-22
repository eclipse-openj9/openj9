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
package com.ibm.j9.uma.platform;

import java.io.IOException;
import java.util.HashSet;
import java.util.Vector;

import com.ibm.j9.uma.configuration.ConfigurationImpl;
import com.ibm.uma.IConfiguration;
import com.ibm.uma.IPlatform;
import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;
import com.ibm.uma.om.Artifact;
import com.ibm.uma.om.Export;
import com.ibm.uma.om.Flag;
import com.ibm.uma.om.Library;
import com.ibm.uma.om.Module;
import com.ibm.uma.util.FileAssistant;

import freemarker.ext.beans.BeansWrapper;
import freemarker.ext.beans.BooleanModel;
import freemarker.template.SimpleScalar;
import freemarker.template.TemplateModel;

public abstract class PlatformImplementation implements IPlatform {

	static final String asmFilesRequiringPreprocessing = "asmFilesRequiringPreprocessing";
	static final String DLL_BASE_ADDRESS = "dllBaseAddress";
	
	ConfigurationImpl configuration;
	String majorVersion;
	String minorVersion;
	
	public PlatformImplementation( IConfiguration config ) {
		configuration = (ConfigurationImpl)config;		
		majorVersion = configuration.getBuildInfo().getMajorVersion();
		minorVersion = configuration.getBuildInfo().getMinorVersion();
		if (majorVersion == null) majorVersion = "0";
		if (minorVersion == null) minorVersion = "0";
	}
	
	public void decorateModules( Vector<Module> modules ) throws UMAException {
		// default of do nothing
	}
	
	public StringBuffer writePreprocessingDirectives(Artifact artifact) throws UMAException {
		StringBuffer buffer = new StringBuffer();
		String [] files = artifact.getData(asmFilesRequiringPreprocessing);
		if (files != null) {
			for ( String file : files ) {
				buffer.append("UMA_FILES_TO_PREPROCESS+="+file+"\n");
			}
		}
		return buffer;
	}
	
	
	public StringBuffer writeMakefilePostscript(Artifact artifact) throws UMAException{
		return writePreprocessingDirectives(artifact);
	}
	
	protected String getFlagStringForAS(Flag flag) throws UMAException {
		String flagString = "";
		if ( isType("qnx") || (isType("linux") && (!(isProcessor("ppc") && !isType("montavista")))) ) {
			if ( flag.isDefinition() ) {
				String prefix = "--defsym ";
				if ( isType("qnx") ) {
					prefix = "-Wa,--defsym,";
				}
				if (flag.getValue()!=null) {
					flagString += prefix + flag.getName() + "=" + flag.getValue();
				} else {
					flagString += prefix + flag.getName() + "=1";
				}
			}
		} else {
			if ( flag.isDefinition() ) {
				if (flag.getValue()!=null) {
					flagString += "-D" + flag.getName() + "=" + flag.getValue();
				} else {
					flagString += "-D" + flag.getName();
				}
			} else {
				flagString += flag.getName();
			}
		}
		return flagString;
	}
	
	final public StringBuffer writeMakefileFlagsLine(Artifact artifact) throws UMAException {
		StringBuffer buffer = new StringBuffer();
		Vector<Flag> flags = artifact.getFlags();
		for ( Flag flag : flags ) {
			if ( flag.evaluate()) {
				String flagString = "";
				if ( flag.isDefinition() ) {
					if (flag.getValue()!=null) {
						flagString += "-D" + flag.getName() + "=" + flag.getValue();
					} else {
						flagString += "-D" + flag.getName();
					}
				} else {
					flagString += flag.getName();
				}
				if ( flag.isCflag() ) {
					buffer.append(UMA.UMA_ARTIFACT_CFLAGS_LABEL+"+="+flagString+"\n");
					if ( replaceMacro("uma_use_cflags_on_asflag_line") != null ) {
						buffer.append(UMA.UMA_ARTIFACT_ASFLAGS_LABEL+"+="+getFlagStringForAS(flag)+"\n");
					}
				}
				if ( flag.isCxxflag() ) {
					buffer.append(UMA.UMA_ARTIFACT_CXXFLAGS_LABEL+"+="+flagString+"\n");
				}
				if ( flag.isCppflag() ) { 
					buffer.append(UMA.UMA_ARTIFACT_CPPFLAGS_LABEL+"+="+flagString+"\n");
				}
				if ( flag.isAsmflag() ) {
					buffer.append(UMA.UMA_ARTIFACT_ASFLAGS_LABEL+"+="+getFlagStringForAS(flag)+"\n");
				}
			}
		}
		return buffer;
	}
	
	
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#writePlatformSpecificFiles(com.ibm.uma.Artifact)
	 */
	public void writePlatformSpecificFiles( Artifact artifact ) throws UMAException {
		writeExportFile(artifact);

		if ( !artifact.flagSet("requiresPrimitiveTable") && !artifact.flagSet("dumpMasterPrimitiveTable") ) {
			return;
		}
		String filename = UMA.getUma().getRootDirectory()+ artifact.getContainingModule().getFullName() + "/" + artifact.getTargetName() + "exp.c";
		Vector<Export> exports = artifact.getExportedFunctions();

		FileAssistant fa = new FileAssistant(filename);
		StringBuffer buffer = fa.getStringBuffer();

		buffer.append(configuration.getcCopyrightNotice());
		buffer.append("#include \"j9comp.h\"\n#include \"j9static.h\"\n\n");
		buffer.append("#if !defined(J9VM_SHARED_LIBRARIES_SUPPORTED) || defined(J9VM_OPT_BUNDLE_CORE_MODULES)\n");
		if ( artifact.flagSet("requiresPrimitiveTable") ) {
			if ( artifact.flagSet("dumpGenericProtosForPrimTable")) {
				for ( Export export : exports ) {
					buffer.append("extern void VMCALL " + export.getExport() + "(void);\n");
				}		
			} else {
				// "We ask the the module to give us a header file name to include with the public prototypes"
				String[] prototypeHeaderFileNames = artifact.getData("prototypeHeaderFileNames");
				if ( prototypeHeaderFileNames != null ) {
					for ( String fnm : prototypeHeaderFileNames ) {
						buffer.append( "#include \"" + fnm + "\"\n" );
					}
				}
				if ( artifact.flagSet("extraPrototypesForExportFile")) {
					String[] extraProtos = artifact.getData("extraPrototypesForExportFile");
					for ( String proto : extraProtos ) {
						buffer.append("extern void VMCALL " + proto + "(void);\n");
					}
				}
			}

			buffer.append("\nEsDefinePrimitiveTable(" + artifact.getTargetName().toUpperCase() + "PrimitiveTable)\n");

			for ( Export export : exports ) {
				String strippedExportName = stripExportName(export.getExport());
				buffer.append("\tEsPrimitiveTableEntry(\"" + strippedExportName + "\","+ strippedExportName +")\n");
			}

			buffer.append("EsEndPrimitiveTable\n"); 
		}

		if ( artifact.flagSet("dumpMasterPrimitiveTable") ) {
			buffer.append("\n");
			Vector<Library> libs = artifact.getStaticLinkLibraries();
			if (artifact.getType() == Artifact.TYPE_BUNDLE) {
				libs.addAll(artifact.getLibraries());
			}
			
			Vector<Artifact> artifacts = new Vector<Artifact>();
			for (Library lib : libs) {
				switch (lib.getType()) {
				case Library.TYPE_BUILD:
					Artifact artifact2 = UMA.getUma().getArtifact(lib.getName());
					if ( artifact2 != null && artifact2.evaluate() ) {
						artifacts.add(artifact2);
					}
					break;
				}
			}
			for ( Artifact oa : artifacts ) {
				if ( oa.evaluate() && oa.flagSet("requiresPrimitiveTable") ) {
					buffer.append("extern EsPrimitiveTable " + oa.getTargetName().toUpperCase() + "PrimitiveTable;\n");
				}
			}

			buffer.append( "\nEsDefinePrimitiveTable(J9LinkedNatives)\n");

			for ( Artifact oa : artifacts ) {
				if ( oa.evaluate() && oa.flagSet("requiresPrimitiveTable") ) {
					String startComment = "";
					String endComment = "";
					if ( !oa.flagSet("isRequired") && !oa.flagSet("isRequiredFor"+artifact.getTargetNameWithScope())) {
						startComment = "/* ";
						endComment = " */";
					}
					buffer.append( "\t"+startComment+"EsPrimitiveTableEntry(\""+ getTargetNameWithRelease(oa)+"\", "+ oa.getTargetName().toUpperCase() +"PrimitiveTable)"+endComment+"\n");
				}
			}
			buffer.append("EsEndPrimitiveTable\n");
		}

		buffer.append( "\n#endif\n");

		fa.writeToDisk();
	}

	public String getStream() {
		return getMajorVersion() + getMinorVersion();
	}
	
	public String getMajorVersion() {
		return majorVersion;
	}
	
	public String getMinorVersion() {
		return minorVersion;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getObjectExtension()
	 */
	public final String getObjectExtension() {
		return "$(UMA_DOT_O)";
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getExecutablePath()
	 */
	public String getExecutablePath() {
		return "";
	}
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getSharedLibPath()
	 */
	public String getSharedLibPath() {
		return "";
	}
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getStaticLibPath()
	 */
	public String getStaticLibPath() {
		return "lib/";
	}
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getExecutableSuffix()
	 */
	public String getExecutableSuffix() {
		return "$(UMA_DOT_EXE)";
	}
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getStaticLibPrefix()
	 */
	public abstract String getStaticLibPrefix();
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getStaticLibSuffix()
	 */
	public abstract String getStaticLibSuffix();
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getSharedLibPrefix()
	 */
	public abstract String getSharedLibPrefix();
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getSharedLibSuffix()
	 */
	public abstract String getSharedLibSuffix();
	
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getFlagDefinitionPrefix()
	 */
	public String getFlagDefinitionPrefix() {
		return "-D";
	}
		
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getSharedLibName(com.ibm.uma.Artifact)
	 */
	public String getLibName(Artifact artifact) throws UMAException {
		String libname = UMA.UMA_PATH_TO_ROOT_VAR;
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE:
		case Artifact.TYPE_SHARED:
			libname += artifact.getTargetPath(this) + getSharedLibPrefix() + artifact.getTargetNameWithScope();
			if ( artifact.appendRelease() ) {
				 libname += getStream();
			} 
			libname += getSharedLibSuffix();
			break;
		case Artifact.TYPE_STATIC:
			libname += artifact.getTargetPathAsStatic(this) + getStaticLibPrefix() + artifact.getTargetName() + getStaticLibSuffix();
			break;
		}
		return libname;
	}
	
	/* 
	 * Returns the lib name for a static build
	 */
	public String getLibNameForStatic(Artifact artifact) throws UMAException {
		String libname = UMA.UMA_PATH_TO_ROOT_VAR;
		boolean isSharedOrBundle = false;
		
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE:
		case Artifact.TYPE_SHARED:
			isSharedOrBundle = true;
			//falling through intentionally
		case Artifact.TYPE_STATIC:
			libname += artifact.getTargetPathAsStatic(this) + getStaticLibPrefix() + artifact.getTargetName();
			if ( artifact.appendRelease() && isSharedOrBundle ) {
				libname += getStream(); 
			}
			libname += getStaticLibSuffix();
			break;
		}
		return libname;
	}
	
	public String getFullPathTarget(Artifact artifact) throws UMAException {
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE:
		case Artifact.TYPE_SHARED:
		case Artifact.TYPE_STATIC:
			return getLibName(artifact);
		case Artifact.TYPE_EXECUTABLE:
			return UMA.UMA_PATH_TO_ROOT_VAR + artifact.getTargetPath(this) + artifact.getTargetName() + getExecutableSuffix();
		}
		return "";
	}
	
	public String getLibOnDiskName(Artifact artifact) throws UMAException {
		return UMA.UMA_PATH_TO_ROOT_VAR + getFilenameWithPathFromRoot(artifact, false);	
	}
	
	public String getLibOnDiskNameForStatic(Artifact artifact) throws UMAException {
		return UMA.UMA_PATH_TO_ROOT_VAR + getFilenameWithPathFromRoot(artifact, true);	
	}
	
	/**
	 * Get the filename of an Artifact
	 * 
	 * @param artifact	The artifact
	 * @param forStaticBuild	true if we want the name for a static build, false otherwise
	 * @return	The filename of the artifact
	 * @throws UMAException
	 */
	public String getFilename(Artifact artifact, boolean forStaticBuild) throws UMAException {
		String filename = "";
		boolean isSharedOrBundle = false;
		
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE:
		case Artifact.TYPE_SHARED:
			isSharedOrBundle = true;
			if ( !forStaticBuild && !artifact.isInBundle()) {
				filename += getSharedLibPrefix() + artifact.getTargetName();
				if ( artifact.appendRelease() ) {
					filename += getStream();
				} 
				filename += getSharedLibSuffix();
				break;
			}
			//fall through intentionally if we're getting the name for a static build
		case Artifact.TYPE_STATIC:
			filename += getStaticLibPrefix() + artifact.getTargetName();
			if ( artifact.appendRelease() && isSharedOrBundle ) {
				 filename += getStream(); 
			} 
			filename += getStaticLibSuffix();
			break;
		case Artifact.TYPE_EXECUTABLE:
			filename += artifact.getTargetName() + getExecutableSuffix();
			break;
		default:
			filename += artifact.getTargetName();
		}
		
		return filename;
	}
	
	public String getFilenameWithPathFromRoot(Artifact artifact, boolean forStaticBuild) throws UMAException {
		switch (artifact.getType()) {
		case Artifact.TYPE_STATIC:
			return artifact.getTargetPathAsStatic(this) + getFilename(artifact, forStaticBuild);
		case Artifact.TYPE_BUNDLE: // fall thru
		case Artifact.TYPE_SHARED: // fall thru
			return (forStaticBuild ? artifact.getTargetPathAsStatic(this) : artifact.getTargetPath(this)) + getFilename(artifact, forStaticBuild);
		case Artifact.TYPE_EXECUTABLE: 
			return getExecutablePath() + getFilename(artifact, forStaticBuild);
		}
		return getFilename(artifact, forStaticBuild);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getStaticLibLinkName(com.ibm.uma.Artifact)
	 */
	public String getLibLinkName(Artifact artifact) throws UMAException {
		return getLibName(artifact); 
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#getTargetNameWithRelease(com.ibm.uma.Artifact)
	 */
	public String getTargetNameWithRelease(Artifact artifact) {
		if ( artifact.appendRelease() ) {
			return artifact.getTargetName() + getStream();
		}
		return artifact.getTargetName();
	}
	
	protected String getDefaultJclDll() throws UMAException {
		return replaceMacro(configuration.getBuildSpec().getDefaultJCL().getId()) + getStream();
	}

	
	protected String getArtifactForJitCodegenHost() throws UMAException {
		return "j9tr" + getTRHostForPath();
	}

	final public String getSocketLibraryFiles() throws UMAException {
		String socketlibs = replaceMacro("uma_socketLibraries");
		if (socketlibs == null || socketlibs.equalsIgnoreCase("")) return null;
		return socketlibs.replace(',', ' ');
	}

	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#replaceMacro(java.lang.String)
	 */
	public String replaceMacro(String macro) throws UMAException {
		if ( macro.equalsIgnoreCase("defaultJclDll")) {
			return getDefaultJclDll();
		} else if ( macro.equalsIgnoreCase("jit_codegen_host") ) {
			return getArtifactForJitCodegenHost();
		} else if ( macro.equalsIgnoreCase("socket") ){
			return getSocketLibraryFiles();
		}
		return configuration.replaceMacro(macro);
	}
	
	protected void addPlatformSpecificLibraryLocationInformation(Artifact artifact, StringBuffer buffer ) throws UMAException {
		buffer.append(artifact.getTargetNameWithScope() + "_depend=" + getLibName(artifact) + "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_link=" + getLibLinkName(artifact) + "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_ondisk=" + getLibOnDiskName(artifact) + "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_path=" + UMA.UMA_PATH_TO_ROOT_VAR + artifact.getTargetPath(this)+ "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_filename=" + getFilename(artifact, false) + "\n");
	}

	protected void addPlatformSpecificLibraryLocationInformationForStatic(Artifact artifact, StringBuffer buffer ) throws UMAException {
		buffer.append(artifact.getTargetNameWithScope() + "_depend=" + getLibNameForStatic(artifact) + "\n");

		buffer.append("ifeq ($(UMA_TARGET_TYPE),EXE)\n");  
		buffer.append(artifact.getTargetNameWithScope() + "_link=" + getLibLinkName(artifact) + "\n");
		buffer.append("endif\n");
		
		buffer.append(artifact.getTargetNameWithScope() + "_ondisk=" + getLibOnDiskNameForStatic(artifact) + "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_path=" + UMA.UMA_PATH_TO_ROOT_VAR + artifact.getTargetPath(this)+ "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_filename=" + getFilename(artifact, true) + "\n");
	}

	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#addLibraryLocationInformation(com.ibm.uma.Artifact, java.lang.StringBuffer)
	 */
	public void addLibraryLocationInformation(Artifact artifact, StringBuffer libLocations) throws UMAException {
		switch (artifact.getType()) {
		case Artifact.TYPE_SHARED: // FALL THROUGH
		case Artifact.TYPE_BUNDLE: // FALL THROUGH
		case Artifact.TYPE_STATIC:
			libLocations.append("#  " + artifact.getTargetNameWithScope() + "\n");
			libLocations.append("ifndef UMA_STATIC_BUILD\n");
			addPlatformSpecificLibraryLocationInformation(artifact, libLocations);
			libLocations.append("else\n");
			addPlatformSpecificLibraryLocationInformationForStatic(artifact, libLocations);
			libLocations.append("endif\n");
			break;
		}
	}
	
	public void addTargetSpecificObjectsForStaticLink(HashSet<String> objtable, Artifact artifact) throws UMAException {
		switch (artifact.getType()) {
		case Artifact.TYPE_SHARED: // Fall-Thru
		case Artifact.TYPE_BUNDLE: // Fall-Thru
		case Artifact.TYPE_EXECUTABLE: 
			if ( artifact.flagSet("requiresPrimitiveTable") || artifact.flagSet("dumpMasterPrimitiveTable") ) {
				objtable.add(artifact.getTargetNameWithScope() + "exp" + getObjectExtension());
			}
			break;
		case Artifact.TYPE_STATIC:
			if (artifact.isInBundle() && ( artifact.flagSet("requiresPrimitiveTable") || artifact.flagSet("dumpMasterPrimitiveTable") )) {
				objtable.add(artifact.getTargetNameWithScope() + "exp" + getObjectExtension());
			}
			break;
		}
	}
	
	final String getTRHostForPath() throws UMAException {
		String[] processors = getProcessor();
		if ( processors.length < 1 ) return "";
		String trHost = processors[0];
		if ( trHost.equalsIgnoreCase("ppc")) return "power";
		if ( trHost.equalsIgnoreCase("sparc")) return "";
		return trHost;
	}
	
	protected void writeStaticLibraryTargetNameDefinition(StringBuffer buffer) throws IOException {
		buffer.append(
				"ifeq ($(UMA_TARGET_TYPE),LIB)\n" +
				"  UMA_LIBTARGET:=$($(UMA_TARGET_NAME)_ondisk)\n" +
				"endif\n");
	}
	protected void writeSharedLibraryTargetNameDefinition(StringBuffer buffer) throws IOException {
		buffer.append(
				"ifeq ($(UMA_TARGET_TYPE),DLL)\n" +
				"  UMA_DLLFILENAME:=$($(UMA_TARGET_NAME)_filename)\n" +
				"  UMA_DLLTARGET:=$($(UMA_TARGET_NAME)_ondisk)\n" +
				"endif\n"
		);
	}
	protected void writeExecutableTargetNameDefinition(StringBuffer buffer)throws IOException {
		buffer.append(
				"ifeq ($(UMA_TARGET_TYPE),EXE)\n" +
				"  UMA_EXETARGET:=$(UMA_TARGET_PATH)$(UMA_TARGET_NAME)" + getExecutableSuffix() + "\n" +
				"endif\n" 
		);
	}
	
	protected String addSystemLib( String lib ) {
		return lib;
	}
	
	protected void writeExportFile(Artifact artifact) throws UMAException {
		// nothing by default
	}
		
	/* (non-Javadoc)
	 * @see com.ibm.uma.platform.IPlatform#writeTopLevelTargets(java.io.FileWriter)
	 */
	public void writeTopLevelTargets(StringBuffer buffer) throws UMAException {
		// no default
	}
	
	public void addArtifactSpecificMakefileInformation(Artifact artifact, StringBuffer buffer) throws UMAException {
		// global default do nothing
	}
	
	protected StringBuffer writeConsoleInformation(Artifact artifact) throws UMAException {
		StringBuffer buffer = new StringBuffer();
		if ( artifact.getType() != Artifact.TYPE_EXECUTABLE ) return buffer;
		if ( artifact.isConsoleApplication() )  {
			if ( !artifact.flagSet("doesNotRequireCMAIN") ) {
				buffer.append( 
						"vpath cmain.c " + UMA.UMA_PATH_TO_ROOT_VAR + "makelib\n" +
						"UMA_OBJECTS+=cmain" + getObjectExtension() + "\n"
				);
			}
		} else {
			buffer.append("UMA_NO_CONSOLE=1\n");
		}
		return buffer;
	}

	public StringBuffer writeMakefileExtras(Artifact artifact) throws UMAException {
		StringBuffer buffer = writeConsoleInformation(artifact);

		if ( artifact.isInBundle() ) {
			buffer.append(
					"J9_REDEF_FUNCTIONS=JVM_OnLoad JVM_OnUnload JNI_OnLoad JNI_OnUnload J9VMDllMain\n" +
					"J9_REDEF_FUNCTIONS:=$(foreach func,$(J9_REDEF_FUNCTIONS),-D$(func)=$(UMA_TARGET_NAME)_$(func))\n" +
					"CFLAGS+=$(J9_REDEF_FUNCTIONS)\n" +
					"CXXFLAGS+=$(J9_REDEF_FUNCTIONS)\n" +
					"CFLAGS+=-DUMA_BUNDLE_NAME=" + artifact.getBundle() + "\n" +
					"CXXFLAGS+=-DUMA_BUNDLE_NAME=" + artifact.getBundle() + "\n" +
					"UMA_OBJECTS += $(UMA_STATIC_OBJECTS)\n" 
			);
		}

		if ( artifact.flagSet("requiresPrimitiveTable") ) {
			buffer.append(
					"ifdef UMA_STATIC_BUILD\n" +
					"  J9_REDEF_FUNCTIONS=JVM_OnLoad JVM_OnUnload JNI_OnLoad JNI_OnUnload J9VMDllMain\n" +
					"  J9_REDEF_FUNCTIONS:=$(foreach func,$(J9_REDEF_FUNCTIONS),-D$(func)=$(UMA_TARGET_NAME)_$(func))\n" +
					"  CFLAGS+=$(J9_REDEF_FUNCTIONS)\n" +
					"  CXXFLAGS+=$(J9_REDEF_FUNCTIONS)\n" +
					"endif\n\n"
			);
		}
		return buffer;
	}
	
	protected String stripExportName(String exportName) {
		// Transform a export names with windowisms
		// e.g., _someFunction@8 => someFunction
		String strippedExportName = exportName;
		if ( strippedExportName.startsWith("_") ) {
			int index = strippedExportName.indexOf("@");
			if ( index > 0 ) {
				if ( strippedExportName.substring(index+1).matches("[0-9]++")) {
					strippedExportName = strippedExportName.substring(1, index);	
				}
			}
			
		}
		
		return strippedExportName;
	}
	
	protected String getBuildId() throws UMAException {
		String buildId = replaceMacro("buildid");
		if ( buildId.equalsIgnoreCase("") ) {
			buildId = "0";
		}
		return buildId;
	}
	
	final public boolean isType(String type) throws UMAException {
		String typeProperty = replaceMacro("uma_type");
		String []types = typeProperty.split(",");
		for ( String aType : types ) {
			if ( aType.equalsIgnoreCase(type) ) return true; 
		}
		return false;
	}
	
	final public boolean isProcessor(String processor) throws UMAException {
		String []processors = getProcessor();
		for ( String proc : processors ) {
			if ( processor.equalsIgnoreCase(proc) ) return true; 
		}
		return false;
	}
	
	final public String[] getProcessor() throws UMAException {
		String processorProperty = replaceMacro("uma_processor");
		if ( processorProperty == null ) {
			throw new UMAException("uma_processor property is not defined for spec: " + UMA.getUma().getConfiguration().getConfigurationName());
		}
		return processorProperty.split(",");
	}

	static String UMA_SPEC_FLAGS_PREFIX = "uma.spec.flags.";
	static String UMA_CNAME_TAG = "cname";
	static String UMA_CNAME_DEFINED_TAG = "cname_defined";

	public TemplateModel getDataModelExtension(String prefixTag, String extensionTag) throws UMAException {
		if ( prefixTag.startsWith(UMA_SPEC_FLAGS_PREFIX) ) {
			String flagName = prefixTag.substring(UMA_SPEC_FLAGS_PREFIX.length());
			if ( extensionTag.equals(UMA_CNAME_DEFINED_TAG)) {
				boolean cname_defined = true;
				if ( flagName.startsWith("module_") || flagName.startsWith("uma_") || flagName.startsWith("test_") || flagName.startsWith("graph_")) {
					cname_defined = false;
				}
				return new BooleanModel(cname_defined, new BeansWrapper());				
			} else if ( extensionTag.equals(UMA_CNAME_TAG) ) {
				String cname = "J9VM_";
				String[] knownNames = new String[5];
				knownNames[0] = "JIT";
				knownNames[1] = "JNI";
				knownNames[2] = "INL";
				knownNames[3] = "AOT";
				knownNames[4] = "Arg0EA";
				
				boolean addUnderscore = false;
				int index = 0;
				while ( index < flagName.length() ) {
					if ( addUnderscore ) {
							cname += "_";
							addUnderscore = false;
					}
					boolean foundKnownName = false;
					for ( String knownName : knownNames ) {
						String sub = flagName.substring(index);
						if ( sub.startsWith(knownName) ) {
							cname += knownName.toUpperCase();
							index += knownName.length();
							addUnderscore = true;
							foundKnownName = true;
							break;
						}
					}
					if ( !foundKnownName ) {
						char ch = flagName.charAt(index);
						if ( !Character.isUpperCase(ch) ) {
							if ( index < flagName.length()-1 && Character.isUpperCase(flagName.charAt(index+1)) ) {
								addUnderscore = true;
							}
						}
						cname += Character.toUpperCase(ch);
						index ++;
					}
				}
				return new SimpleScalar(cname);
			}
		}
		return null;
	}
}
