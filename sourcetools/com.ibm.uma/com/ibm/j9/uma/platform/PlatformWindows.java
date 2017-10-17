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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.j9.uma.platform;

import java.io.IOException;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.Hashtable;
import java.util.Vector;

import com.ibm.uma.IConfiguration;
import com.ibm.uma.UMA;
import com.ibm.uma.UMAException;
import com.ibm.uma.om.Artifact;
import com.ibm.uma.om.Export;
import com.ibm.uma.om.Library;
import com.ibm.uma.util.FileAssistant;

public class PlatformWindows extends PlatformImplementation {
	
	static final String DEFAULT_RC_PRODUCT_NAME="IBM SDK, Java(tm) 2 Technology Edition";
	
	public PlatformWindows( IConfiguration buildSpec ) {
		super(buildSpec);
	}
	
	@Override
	protected void addPlatformSpecificLibraryLocationInformation(Artifact artifact, StringBuffer buffer) throws UMAException {
		super.addPlatformSpecificLibraryLocationInformation(artifact, buffer);
		buffer.append(artifact.getTargetNameWithScope() + "_pdb=" + getPdbName(artifact)+ "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_exp=" + getExpName(artifact) + "\n");
	}
	
	@Override
	protected void addPlatformSpecificLibraryLocationInformationForStatic(Artifact artifact, StringBuffer buffer) throws UMAException {
		buffer.append(artifact.getTargetNameWithScope() + "_depend=" + getLibName(artifact) + "\n");
		buffer.append("ifeq ($(UMA_TARGET_TYPE),EXE)\n");
		buffer.append(artifact.getTargetNameWithScope() + "_link=" + getLibLinkName(artifact) + "\n");
		buffer.append("endif\n");
		buffer.append(artifact.getTargetNameWithScope() + "_ondisk=" + getLibName(artifact) + "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_path=" + UMA.UMA_PATH_TO_ROOT_VAR + artifact.getTargetPath(this)+ "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_pdb=" + getPdbName(artifact)+ "\n");
		buffer.append(artifact.getTargetNameWithScope() + "_exp=" + getExpName(artifact) + "\n");
	}

	protected String getExpName(Artifact artifact) throws UMAException {
		return getLibNameStart(artifact) + ".exp";
	}

	@Override
	public String getLibName(Artifact artifact) throws UMAException {
		String libname = "";
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE:
		case Artifact.TYPE_SHARED:
		case Artifact.TYPE_STATIC:
			libname = UMA.UMA_PATH_TO_ROOT_VAR + artifact.getTargetPathAsStatic(this) + getStaticLibPrefix() + artifact.getTargetNameWithScope() + getStaticLibSuffix();
			break;
		}
		return libname;
	}
	
	protected String getLibNameStart(Artifact artifact) throws UMAException {
		String expname = "";
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE:
		case Artifact.TYPE_SHARED:
		case Artifact.TYPE_STATIC:
			expname = UMA.UMA_PATH_TO_ROOT_VAR + artifact.getTargetPathAsStatic(this) + artifact.getTargetNameWithScope();
			break;
		}
		return expname;
	}
	
	protected String getPdbName(Artifact artifact) throws UMAException {
		return getLibNameStart(artifact) + ".pdb";
	}
	
	@Override
	public String getSharedLibPath() {
		return "";
	}

	@Override
	public String getSharedLibPrefix() {
		return "";
	}
	@Override
	public String getSharedLibSuffix() {
		return ".dll";
	}
	
	@Override
	public String getStaticLibPrefix() {
		return "";
	}
	
	@Override
	public String getStaticLibSuffix() {
		return ".lib";
	}
	
		
	final public boolean requiresDLLResourceFile() throws UMAException {
		if ( replaceMacro("uma_does_not_require_resource_files") != null ) return false; 
		return true;
	}
	
	final public boolean requiresEXEResourceFile() throws UMAException {
		if ( replaceMacro("uma_does_not_require_resource_files") != null ) return false; 
		return true;
	}
	
	final public boolean requiresEXEManifestFile() throws UMAException {
		if ( replaceMacro("uma_does_not_require_manifest_files") != null ) return false; 
		return true;
	}
	
	// TODO: move to template
	void writeExecutableManifestFile(Artifact artifact) throws UMAException {
		if ( artifact.getType() != Artifact.TYPE_EXECUTABLE ) return;
		String filename = UMA.getUma().getRootDirectory() + artifact.getContainingModule().getFullName() + "/" + artifact.getTargetNameWithScope() + ".exe.manifest";
		FileAssistant fa = new FileAssistant(filename);
		StringBuffer buffer = fa.getStringBuffer();
		
		buffer.append(
				"<?xml version='1.0' encoding='UTF-8' standalone='yes'?>\n" +
				"<assembly xmlns='urn:schemas-microsoft-com:asm.v1' manifestVersion='1.0'>\n" +
				"  <trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\">\n" +
				"    <security>\n" +
				"      <requestedPrivileges>\n" +
				"        <requestedExecutionLevel level='asInvoker' uiAccess='false' />\n" +
				"      </requestedPrivileges>\n" +
				"    </security>\n" +
				"  </trustInfo>\n" +
				"</assembly>\n"
				);
		fa.writeToDisk();
	}
	
	// TODO: move to template
	void writeExecutableResourceFile(Artifact artifact) throws UMAException {
		if ( artifact.getType() != Artifact.TYPE_EXECUTABLE ) return;
		GregorianCalendar calendar = new GregorianCalendar();
		String filename = UMA.getUma().getRootDirectory() + artifact.getContainingModule().getFullName() + "/" + artifact.getTargetNameWithScope() + ".rc";
		FileAssistant fa = new FileAssistant(filename);
		StringBuffer buffer = fa.getStringBuffer();
		
		String productName = replaceMacro("rc_productName");
		if (productName == null ) {
			productName = DEFAULT_RC_PRODUCT_NAME;
		}
		
		int buildIdLow = Integer.parseInt(getBuildId()) & 0xffff;
		int buildIdHigh = Integer.parseInt(getBuildId()) >> 16;
		
		buffer.append(
				"\n" +
		"#include <windows.h>\n");
		writeResourceFileDefines(buffer);
		buffer.append(
				"#include \"j9cfg.h\"\n" +
				"#include \"j9version.h\"\n" +
				"\n" +
				"VS_VERSION_INFO VERSIONINFO\n" +
				" FILEVERSION " + getMajorVersion() + "," + getMinorVersion()+ ","+ buildIdHigh +"," + buildIdLow + "\n" +
				" PRODUCTVERSION " + getMajorVersion() + "," + getMinorVersion()+ ","+ buildIdHigh +"," + buildIdLow + "\n" +
				" FILEFLAGSMASK 0x3fL\n" +
				" FILEFLAGS 0x0L\n" +
				" FILEOS VOS_NT_WINDOWS32\n" +
				" FILETYPE VFT_APP\n" +
				" FILESUBTYPE 0x0L\n" +
				"BEGIN\n" +
				"	BLOCK \"StringFileInfo\"\n" +
				"	BEGIN\n" +
				"		BLOCK \"040904b0\"\n" +
				"		BEGIN\n" +
				"			VALUE \"CompanyName\", \"International Business Machines Corporation\\0\"\n" +
				"			VALUE \"FileDescription\", \"WebSphere Studio Device Developer\\0\"\n" +
				"			VALUE \"FileVersion\", \"R" + getMajorVersion() + "." + getMinorVersion()+ " (\" EsBuildVersionString \")\\0\"\n" +
				"			VALUE \"InternalName\", \"" + getTargetNameWithRelease(artifact) + "\\0\"\n" +
				"			VALUE \"LegalCopyright\", \" Copyright (c) 1991, " + calendar.get(Calendar.YEAR) + " IBM Corp. and others\\0\"\n" +
				"			VALUE \"OriginalFilename\", \"" + getTargetNameWithRelease(artifact) + ".dll\\0\"\n" +
				"			VALUE \"ProductName\", \"" + productName + "\\0\"\n" +
				"			VALUE \"ProductVersion\", \"R" + getMajorVersion() + "." + getMinorVersion()+ "\\0\"\n" +
				"		END\n" +
				"	END\n" +
				"	BLOCK \"VarFileInfo\"\n" +
				"	BEGIN\n" +
				"		VALUE \"Translation\", 0x0409, 1200\n" +
				"	END\n" +
				"END\n"
		);

		if ( replaceMacro("uma.includeIcon").equalsIgnoreCase("true") ) {
			buffer.append(
					"\n" +
					"100	ICON	DISCARDABLE	\"..\\include\\j9.ico\"\n" 
			);
		}

		if ( isType("ce") ) writeExecutableResourceFileExtraCommands(buffer);

		fa.writeToDisk();

	}
	
	protected void writeExecutableResourceFileExtraCommands(StringBuffer buffer) {
		// Used by WinCE Platforms
		buffer.append(
				"\n" +
				"#define IDM_MAIN_MENUITEM1              401\n" +
				"#define IDM_MAIN_MENUITEM2              402\n" +
				"#define IDS_MAIN_MENUITEM1              410\n" +
				"#define IDS_MAIN_MENUITEM2              411\n" +
				"#define IDM_MAIN_MENU                   450\n" +
				"\n" +
				"#define IDM_EXIT                        205\n" +
				"#define IDM_COPY                        207\n" +
				"#define IDM_PASTE                       208\n" +
				"\n" +
				"#if !defined(WCEOLE_ENABLE_DIALOGEX)\n" +
				"#define DIALOGEX DIALOG DISCARDABLE\n" +
				"#endif\n" +
				"\n" +
				"#include <commctrl.h>\n" +
				"#define SHMENUBAR RCDATA\n" +
				"#if defined(J9POCKETPC)\n" +
				"#include <aygshell.h>\n" +
				"#define AFXCE_IDR_SCRATCH_SHMENU 28700\n" +
				"#else\n" +
				"#define I_IMAGENONE (-2)\n" +
				"#define NOMENU 0xFFFF\n" +
				"#define IDS_SHNEW 1\n" +
				"#define IDM_SHAREDNEW 10\n" +
				"#define IDM_SHAREDNEWDEFAULT 11\n" +
				"#endif\n" +
				"#define AFXCE_IDD_SAVEMODIFIEDDLG 28701\n" +
				"\n" +
				"IDM_MAIN_MENU RCDATA DISCARDABLE\n" +
				"BEGIN\n" +
				"	IDM_MAIN_MENU, 2,\n" +
				"	I_IMAGENONE, IDM_MAIN_MENUITEM1, TBSTATE_ENABLED,\n" +
				"	TBSTYLE_DROPDOWN | TBSTYLE_AUTOSIZE, IDS_MAIN_MENUITEM1, 0, 0,\n" +
				"	I_IMAGENONE, IDM_MAIN_MENUITEM2, TBSTATE_ENABLED,\n" +
				"	TBSTYLE_DROPDOWN | TBSTYLE_AUTOSIZE, IDS_MAIN_MENUITEM2, 0, 1,\n" +
				"END\n" +
				"\n" +
				"#if defined(J9POCKETPC)\n" +
				"// Command bar Pocket PC - no shortcuts\n" +
				"IDM_MAIN_MENU MENU DISCARDABLE\n" +
				"BEGIN\n" +
				"	POPUP \"File\"\n" +
				"	BEGIN\n" +
				"		MENUITEM \"Close\", IDM_EXIT\n" +
				"	END\n" +
				"	POPUP \"Edit\"\n" +
				"	BEGIN\n" +
				"		MENUITEM \"Copy\", IDM_COPY\n" +
				"		MENUITEM \"Paste\", IDM_PASTE\n" +
				"	END\n" +
				"END\n" +
				"STRINGTABLE DISCARDABLE\n" +
				"BEGIN\n" +
				"	IDS_MAIN_MENUITEM1 \"File\"\n" +
				"	IDS_MAIN_MENUITEM2 \"Edit\"\n" +
				"END\n" +
				"#else\n" +
				"// Command bar HPC and others - shortcuts\n" +
				"IDM_MAIN_MENU MENU DISCARDABLE\n" +
				"BEGIN\n" +
				"	POPUP \"&File\"\n" +
				"	BEGIN\n" +
				"		MENUITEM \"&Close\", IDM_EXIT\n" +
				"	END\n" +
				"	POPUP \"&Edit\"\n" +
				"	BEGIN\n" +
				"		MENUITEM \"&Copy\", IDM_COPY\n" +
				"		MENUITEM \"&Paste\", IDM_PASTE\n" +
				"	END\n" +
				"END\n" +
				"STRINGTABLE DISCARDABLE\n" +
				"BEGIN\n" +
				"	IDS_MAIN_MENUITEM1 \"&File\"\n" +
				"	IDS_MAIN_MENUITEM2 \"&Edit\"\n" +
				"END\n" +
				"#endif\n"
			);
	}

	// TODO: convert to template
	@Override
	protected void writeExportFile(Artifact artifact) throws UMAException {
		if ( artifact.getType() != Artifact.TYPE_SHARED && artifact.getType() != Artifact.TYPE_BUNDLE ) {
			return;
		}

		String filename = UMA.getUma().getRootDirectory() + artifact.getContainingModule().getFullName() + "/" + artifact.getTargetNameWithScope() + ".def";
		FileAssistant fa = new FileAssistant(filename);
		StringBuffer buffer = fa.getStringBuffer();

		buffer.append("LIBRARY " + getTargetNameWithRelease(artifact).toUpperCase()+ "\n");
		if ( !isProcessor("amd64") && !isType("ce") ) {
			buffer.append("\n\nSECTIONS\n\t.data\tREAD WRITE\n\t.text\tEXECUTE READ\n\n");
		}
		Vector<Export> exports = artifact.getArtifactExportedFunctions();
		StringBuffer exportsBuffer = new StringBuffer();
		exportsBuffer.append("EXPORTS\n");
		boolean exportDefined = false;
		for ( Export export : exports ) {
			if ( !export.evaluate() ) continue;
			exportDefined = true;
			String exportString = export.getExport();
			if ( isProcessor("amd64") ) {
				exportString = stripExportName(exportString);
			} 
			exportsBuffer.append("\t" + exportString + "\n");
		}
		if ( exportDefined ) {
			buffer.append(exportsBuffer.toString() + "\n");
		}
		fa.writeToDisk();

	}
	
	@Override
	public void writePlatformSpecificFiles(Artifact artifact) throws UMAException {
		super.writePlatformSpecificFiles(artifact);
		/* Generate two files for each shared artifact: .def and .rc */
		writeResourceFile(artifact);
	}
	
	void writeRebaseTargets(StringBuffer buffer) throws IOException, UMAException {
		// determine the total number of loadgroups and which ones have duplicates
		Hashtable<String, Vector<Artifact>> loadtable = new Hashtable<String, Vector<Artifact>>();
		for( Artifact artifact : UMA.getUma().getArtifacts().values() ) {
			if ( (artifact.getType() == Artifact.TYPE_BUNDLE || artifact.getType() == Artifact.TYPE_SHARED) && 
					artifact.getLoadGroup() != null ) {
				Vector<Artifact> artifacts = loadtable.get(artifact.getLoadGroup());
				if ( artifacts == null ) {
					artifacts = new Vector<Artifact>();
					loadtable.put(artifact.getLoadGroup(), artifacts);
				}
				artifacts.add(artifact);
			}
		}
		Vector<String> groupNames = new Vector<String>();
		Vector<String> singles = new Vector<String>();
		boolean topGroupExists = false;
		for( String group : loadtable.keySet() ) {
			Vector<Artifact> artifacts = loadtable.get(group);
			if (group.equalsIgnoreCase("top")) {
				topGroupExists = true;
				groupNames.add(group);
			} else {
				if (artifacts.size()>1) {
					groupNames.add(group);
				} else {
					singles.add(group);
				}
			}
		}
		buffer.append(
				"rebase: rebase.txt" );
		
		for ( String group : groupNames ) {
			buffer.append(" rebase_" + group + ".txt");
		}
		
		String rebase_base = configuration.replaceMacro("uma_rebase_base");
		if ( rebase_base == null || rebase_base.equalsIgnoreCase("") ) {
			rebase_base = "0x7FFB0000";
		}
		

		buffer.append(
				"\n" +
				"	-rm -f rebase.log\n" +
				"	rebase -q -b " + rebase_base + " -d -R . -l rebase.log");
		if (topGroupExists) {
			buffer.append( " -G rebase_top.txt");
		}
		buffer.append(" -G rebase.txt");

		/* 
		 * In jvm.26 JIT makefiles are used directly, to allow the use the same UMA tool in jvm.24 and beyond
		 * uma needs to detect whether JIT makefiles are in use.  The absence of "j9jit" in the groups list means 
		 * that JIT makefiles are being used.
		 * 
		 * See CMVC 187776 for what happens when JIT makefiles are used and -O rebase_jit.txt is not present.  
		 * (preview there will be problems instantiating the heap)
		 * 
		 * When JIT makefiles are not used and -O rebase_jit.txt is present, compile will fail due to not being able to open 
		 * the rebase_jit.txt file. 
		 */
		boolean jitGroupDetected = false;
		for ( String group : groupNames ) {
			if (group.equalsIgnoreCase("top")) continue;
			buffer.append( " -O rebase_" + group + ".txt");
			if ( group.equalsIgnoreCase("j9jit") ) { 
				jitGroupDetected = true;
			}
		}
		if ( !jitGroupDetected ) {
			// this file is created as rebase_jit.txt.ftl in the VM windows code base.
			buffer.append(" -O rebase_jit.txt");
		}
		
		buffer.append("\n\n");
		
		for( String group : groupNames ) {
			buffer.append( 
					"rebase_"+group+".txt: makefile\n" +
					"	-rm -f rebase_"+group+".txt\n"
			);
			Vector<Artifact> artifacts = loadtable.get(group);
			for ( Artifact artifact : artifacts ) {
				buffer.append("	echo $(" + artifact.getTargetNameWithScope() +"_ondisk) >> rebase_"+group+".txt\n");
			}
			buffer.append("\n");
		}
		
		buffer.append(
				"rebase.txt: makefile\n" +
				"	-rm -f rebase.txt\n"
		);
		for( String group : singles ) {
			Vector<Artifact> artifacts = loadtable.get(group);
			for ( Artifact artifact : artifacts ) {
				buffer.append("	echo $(" + artifact.getTargetNameWithScope() +"_ondisk) >> rebase.txt\n");
			}
		}
		buffer.append(
				"\n" +
				".PHONY: rebase\n" +
				"\n"
		);
	}

	// TODO: move to template file 
	void writeResourceFile(Artifact artifact) throws UMAException {
		
		switch (artifact.getType()) {
		case Artifact.TYPE_BUNDLE: //Fall-Thru
		case Artifact.TYPE_SHARED:
			if ( requiresDLLResourceFile() ) {
				writeSharedLibResourceFile(artifact);
			}
			break;
		case Artifact.TYPE_EXECUTABLE:
			if ( requiresEXEResourceFile() ) {
				writeExecutableResourceFile(artifact);
			}
			if ( requiresEXEManifestFile() ) {
				writeExecutableManifestFile(artifact);
			}
			break;
		}
	}
	
	final protected void writeResourceFileDefines(StringBuffer buffer) throws UMAException {
		String ceOsType = replaceMacro("uma_ce_os_type");
		if ( ceOsType!=null && ceOsType.equalsIgnoreCase("Palm-size PC 2.11") ) {
			buffer.append("#define VS_VERSION_INFO 1\n" +
					"#define VOS_NT_WINDOWS32 0x00040004L\n" +
					"#define VFT_APP 0x00000001L\n" +
					"#define VFT_DLL 0x00000002L\n");
		} else {
			buffer.append("#include <winver.h>\n");
		}
	}

	// TODO: move to template file
	void writeSharedLibResourceFile(Artifact artifact) throws UMAException {
		if ( artifact.getType() != Artifact.TYPE_SHARED && artifact.getType() != Artifact.TYPE_BUNDLE ) return;
		GregorianCalendar calendar = new GregorianCalendar();
		String filename = UMA.getUma().getRootDirectory() + artifact.getContainingModule().getFullName() + "/" + artifact.getTargetNameWithScope() + ".rc";
		FileAssistant fa = new FileAssistant(filename);
		StringBuffer buffer = fa.getStringBuffer();

		String productName = replaceMacro("rc_productName");
		if (productName == null ) {
			productName = DEFAULT_RC_PRODUCT_NAME;
		}
		int buildIdLow = Integer.parseInt(getBuildId()) & 0xffff;
		int buildIdHigh = Integer.parseInt(getBuildId()) >> 16;
		
		buffer.append(
				"\n" +
		"#include <windows.h>\n" );
		writeResourceFileDefines(buffer);
		buffer.append(
				"#include \"j9cfg.h\"\n" +
				"#include \"j9version.h\"\n" +
				"\n" +
				"VS_VERSION_INFO VERSIONINFO\n" +
				" FILEVERSION " + getMajorVersion() + "," + getMinorVersion()+ ","+ buildIdHigh +"," + buildIdLow + "\n" +
				" PRODUCTVERSION " + getMajorVersion() + "," + getMinorVersion()+ ","+ buildIdHigh +"," + buildIdLow + "\n" +
				" FILEFLAGSMASK 0x3fL\n" +
				" FILEFLAGS 0x0L\n" +
				" FILEOS VOS_NT_WINDOWS32\n" +
				" FILETYPE VFT_DLL\n" +
				" FILESUBTYPE 0x0L\n" +
				"BEGIN\n" +
				"	BLOCK \"StringFileInfo\"\n" +
				"	BEGIN\n" +
				"		BLOCK \"040904b0\"\n" +
				"		BEGIN\n" +
				"			VALUE \"CompanyName\", \"International Business Machines Corporation\\0\"\n" +
				"			VALUE \"FileDescription\", \"J9 Virtual Machine Runtime\\0\"\n" +
				"			VALUE \"FileVersion\", \"R" + getMajorVersion() + "." + getMinorVersion()+ " (\" EsBuildVersionString \")\\0\"\n" +
				"			VALUE \"InternalName\", \"" + getTargetNameWithRelease(artifact) + "\\0\"\n" +
				"			VALUE \"LegalCopyright\", \" Copyright (c) 1991, " + calendar.get(Calendar.YEAR) + " IBM Corp. and others\\0\"\n" +
				"			VALUE \"OriginalFilename\", \"" + getTargetNameWithRelease(artifact) + ".dll\\0\"\n" +
				"			VALUE \"ProductName\", \"" + productName + "\\0\"\n" +
				"			VALUE \"ProductVersion\", \"R" + getMajorVersion() + "." + getMinorVersion()+ "\\0\"\n" +
				"		END\n" +
				"	END\n" +
				"	BLOCK \"VarFileInfo\"\n" +
				"	BEGIN\n" +
				"		VALUE \"Translation\", 0x0409, 1200\n" +
				"	END\n" +
				"END\n"
		);
		fa.writeToDisk();

	}
	
	@Override
	public void writeTopLevelTargets(StringBuffer buffer) throws UMAException {
		super.writeTopLevelTargets(buffer);

		if ( configuration.isFlagSet("build_openj9") ) {
			// No rebase in openj9 builds
			// ReBase.exe has been removed since Windows SDK 8. It could be replaced with "editbin /REBASE":
			//	e.g.
			//		editbin /REBASE:BASE=0x7FFFF750000,DOWN /DYNAMICBASE:NO /NOLOGO <list of *.ddl files>
			buffer.append("\n");
			buffer.append("export UMA_SINGLE_REBASE=1\n");
			buffer.append("\n\n");
		} else {
			try {
				// Add the rebase targets
				writeRebaseTargets(buffer);
				writeTweakedPhaseTargets(buffer);
			} catch (IOException e) {
				throw new UMAException(e);
			}
		}
	}

	/**
	 * Write phase targets that 
	 * @param buffer
	 */
	private void writeTweakedPhaseTargets(StringBuffer buffer) {
		UMA uma = UMA.getUma();
		IConfiguration config = uma.getConfiguration();
		
		buffer.append("\n");
		buffer.append("export UMA_SINGLE_REBASE\n\n");

		for (int i=0; i < config.numberOfPhases(); i++) {
			String phaseName = config.phaseName(i);
			buffer.append("phase_" + phaseName + ": UMA_SINGLE_REBASE=1\n");
			buffer.append("phase_" + phaseName + ": rebase\n");
			buffer.append("\n");
		}

		buffer.append("\n\n");		
		buffer.append("all: UMA_SINGLE_REBASE=1\n");		
		buffer.append("all: rebase\n");		
		buffer.append("\n\n");		
	}

	protected StringBuffer writeDllLinkComment(Artifact artifact) throws UMAException {
		StringBuffer buffer = new StringBuffer();
		if ( !((artifact.getType() == Artifact.TYPE_SHARED) || (artifact.getType() == Artifact.TYPE_BUNDLE)) ) return buffer;
		buffer.append("UMA_DLL_COMMENT=/comment:\"");		
		String[] nonIBMCopyright = artifact.getData("nonIBMCopyright");
		if (nonIBMCopyright != null) {
			for (String text : nonIBMCopyright) {
				buffer.append( text + " ");
			}
		} else {
			GregorianCalendar calendar = new GregorianCalendar();
			buffer.append("IBM J9 Virtual Machine $($(UMA_TARGET_NAME)_filename). Copyright (c) 1993, " + calendar.get(Calendar.YEAR) + " IBM Corp. and others");
		}
		buffer.append("\"\n");
		return buffer;
	}
	
	final protected void writeFixupFileVPath(Artifact artifact, StringBuffer buffer) throws UMAException {
		if ( (replaceMacro("uma_do_not_write_fixup_file_vpath")== null) &&
				artifact.flagSet("requiresLockFixups386") ) { 
			buffer.append( 
					"vpath j9dll.c " + UMA.UMA_PATH_TO_ROOT_VAR + "makelib\n" +
					"UMA_OBJECTS+=j9dll" + getObjectExtension() + "\n"
			);
		}
	}
	
	@Override
	public StringBuffer writeMakefileExtras(Artifact artifact) throws UMAException {
		StringBuffer buffer = super.writeMakefileExtras(artifact);
		buffer.append(writeDllLinkComment(artifact));
		writeFixupFileVPath(artifact, buffer);
		return buffer;
	}
	
	@Override
	public void addArtifactSpecificMakefileInformation(Artifact artifact, StringBuffer buffer) throws UMAException {
		super.addArtifactSpecificMakefileInformation(artifact, buffer);
		//add-in delay load information
		Vector<Library> delayLoadLibraries = new Vector<Library>();
		for (Library library : artifact.getLibraries()) {
			if (!library.evaluate() || !library.getDelayLoad()) continue;
			delayLoadLibraries.add(library);
		}
		if ( delayLoadLibraries.size() > 0 ) {
			buffer.append("UMA_DELAYLOAD_LIBS=");
			for ( Library library : delayLoadLibraries) {
				buffer.append(" " + library.getName().replaceFirst(".lib", ".dll"));
			}
			buffer.append("\n");
		}
		
	}
}
