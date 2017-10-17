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
package com.ibm.j9.uma.configuration;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.Vector;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.j9.uma.configuration.freemarker.Features;
import com.ibm.j9.uma.configuration.freemarker.Source;
import com.ibm.j9.uma.platform.PlatformOSX;
import com.ibm.j9.uma.platform.PlatformUnix;
import com.ibm.j9.uma.platform.PlatformWindows;
import com.ibm.j9.uma.platform.PlatformZOS;
import com.ibm.j9tools.om.BuildInfo;
import com.ibm.j9tools.om.BuildSpec;
import com.ibm.j9tools.om.ConfigDirectory;
import com.ibm.j9tools.om.Flag;
import com.ibm.j9tools.om.FlagDefinitions;
import com.ibm.j9tools.om.InvalidBuildSpecException;
import com.ibm.j9tools.om.InvalidConfigurationException;
import com.ibm.j9tools.om.OMObjectException;
import com.ibm.j9tools.om.ObjectFactory;
import com.ibm.j9tools.om.Property;
import com.ibm.uma.IConfiguration;
import com.ibm.uma.IPlatform;
import com.ibm.uma.ISinglePredicateEvaluator;
import com.ibm.uma.UMABadPhaseNameException;
import com.ibm.uma.UMAException;
import com.ibm.uma.om.Artifact;
import com.ibm.uma.util.Logger;

import freemarker.template.SimpleSequence;
import freemarker.template.TemplateModel;
import freemarker.template.TemplateModelIterator;

public class ConfigurationImpl implements IConfiguration, ISinglePredicateEvaluator {
		
	String[] phaseList; 
	String[] excludeArtifacts;
	
	public int numberOfPhases() {
		return phaseList.length;
	}
	
	public String phaseName(int phase) {
		if (phase >= numberOfPhases() )	return "base phase identifier [" + phase + "]";
		return phaseList[phase];
	}
	
	public int phaseFromString( String phase ) throws UMABadPhaseNameException {
		for ( int i=0; i< phaseList.length; i++ ) {
			if ( phase.equalsIgnoreCase(phaseList[i]) ) return i;
		}
		throw new UMABadPhaseNameException("Unknown phase label: " + phase);
	}
	
	public String[] phases() {
		return phaseList;
	}	

	BuildSpec buildSpec;
	FlagDefinitions flagDefs;
	IPlatform platform;
	BuildInfo buildInfo;
	Hashtable<String, String> macroMap = new Hashtable<String, String>();
	
	public BuildInfo getBuildInfo() {
		return buildInfo;
	}

	public ConfigurationImpl(String configPath, String buildSpecId, String buildId, String buildDate, String buildTag, String jitVersionFile, String excludeArtifacts) throws UMAException {
			try {
				ConfigDirectory configDirectory = new ConfigDirectory(configPath);
				ObjectFactory factory = new ObjectFactory(configDirectory);	
				factory.initialize();
				buildSpec = factory.getBuildSpec(buildSpecId);
				if (buildSpec==null) {
					throw new UMAException("Could not find " + buildSpecId + ".spec file.");
				}
				flagDefs = factory.getFlagDefinitions();
				buildInfo = factory.getBuildInfo();
				
				macroMap.put("buildid", buildId);
				macroMap.put("build_date", buildDate);
				macroMap.put("vm_build_tag", buildTag);
				macroMap.put("gc_build_tag", buildTag);
				macroMap.put("jit_build_tag", getJITVersionTag(jitVersionFile, buildTag));
				
				this.excludeArtifacts = excludeArtifacts.split(",");
				
				processProperties(configPath);
				
				/* 
				 * define phases from properties
				 */
				String phases = macroMap.get("phase_list");
				if (phases == null) {
					throw new UMAException("phase_list is not defined in makelib/uma.properties.");
				}
				phaseList = phases.split(" ");
				
				/* 
				 * Verify that we have a mapping from JCL name to JCL artifact name
				 */
				for (String jcl : buildInfo.getJCLs()) {
					if ( macroMap.get(jcl) == null ) {
						throw new UMAException("Error: JCL [" + jcl + "] does not contain a mapping to it's artifact name in makelib/uma.properties.");
					}
				}
				
				dumpFlags();
			} catch (OMObjectException e) {
				throw new UMAException(e);
			} catch (InvalidConfigurationException e ) {
				throw new UMAException(e);
			}
	}
	
	void processProperties(String path) throws UMAException {
		try {
			Properties properties = new Properties();
			// Path is assumed to be in the shape {root}/buildspecs
			properties.load(new FileInputStream(path+"/../makelib/uma.properties"));
			for ( Object key : properties.keySet() ) {
				String property = properties.getProperty((String)key);
				macroMap.put((String)key, property);
			}
		} catch (FileNotFoundException e) {
			// No uma.properties file, no problem.
			Logger.getLogger().println(Logger.WarningLog, "Warning: uma.properties file not found.");
		} catch (IOException e) {
			throw new UMAException(e);
		}
	}
	
	public String getMetadataFilename() {
		return "module.xml";
	}
	
	public String getConfigurationName() {
		return buildSpec.getId();
	}
	
	public boolean isValidJcl(String jcl) {
		for (String validJcl : buildInfo.getJCLs()) {
			if ( jcl.equals(validJcl)) return true;
		}
		return false;
	}
	
	public String getDefaultJcl() {
		return buildSpec.getDefaultJCL().getId();
	}
	
	public Map<String, Flag> getFlags() {
		return buildSpec.getFlags();
	}
	
	public BuildSpec getBuildSpec() {
		return buildSpec;
	}
	
	private void dumpFlags() {
		for( String flagId : buildSpec.getFlags().keySet() ) {
			if ( isFlagSet(flagId) ) { 
				Logger.getLogger().println(Logger.InformationL2Log, "flag: " + flagId + " is on");
			} else {
				Logger.getLogger().println(Logger.InformationL2Log, "flag: " + flagId + " is off");
			}
		}
	}
	
	// TODO note this is a big hack until we can convert module.xmls to use this new version of the flags.
	String transformFlag(String oldFlag) {
		if (!oldFlag.startsWith("J9VM_")) { 
			return oldFlag;
		}
		
		// hack some special cases in 
		if ( oldFlag.equals("J9VM_ENV_BUILD_SHARED_LIB_VM")) return "env_buildSharedLibVM";
		if ( oldFlag.equals("J9VM_ENV_BUILD_STATIC_VM")) return "env_buildStaticVM";
		if ( oldFlag.equals("J9VM_IVE_JXE_OERELOCATOR")) return "ive_jxeOERelocator";
		
		String newFlag = oldFlag.substring(5); // trim off the J9VM_
		int pos = newFlag.indexOf("_") + 1;
		String flag = newFlag.substring(0, pos).toLowerCase();
		boolean makeUpper = false;
		while ( true ) {
			int last = pos;
			pos = newFlag.indexOf("_", last);
			if ( pos < 0 ) {
				if ( last >= newFlag.length() )	break;
				else pos = newFlag.length();
			}
			if (makeUpper) {
				flag += newFlag.substring(last, ++last).toUpperCase();
			} else { 
				makeUpper = true;
			}
			flag += newFlag.substring(last, pos).toLowerCase();
			pos += 1;
		}
		return flag;
	}
	
	public boolean isFlagValid(String flag) {
		flag = transformFlag(flag);
		if (flagDefs.getFlagDefinition(flag)==null) return false;
		return true;
	}
	
	public boolean isFlagSet(String flagId) {
		flagId = transformFlag(flagId);
		if ( isFlagValid(flagId) ) {
			Flag flag = buildSpec.getFlags().get(flagId);
			if ( flag != null ) {
				return flag.getState();
			}
		}
		return false;
	}
	
	public Set<String> getAllFlags() {
		return buildSpec.getFlags().keySet();
	}
	
	public void defineMacro(String key, String macro) {
		macroMap.put(key, macro);
	}

	public String replaceMacro(String macro) {
		Property property = buildSpec.getProperty(macro);
		if ( property == null ) {
			return macroMap.get(macro);
		}
		return property.getValue();
	}	

	public IPlatform getPlatform() throws UMAException {
		String configurationName = buildSpec.getId();
		if (platform == null) {
			if ( configurationName.startsWith("win") ) {
				platform = new PlatformWindows(this);
			} else if ( configurationName.startsWith("zos") ) {
				platform = new PlatformZOS(this);
			} else if ( configurationName.startsWith("aix")
					|| configurationName.startsWith("linux") 
					|| configurationName.startsWith("ose")
					|| configurationName.startsWith("qnx") ) {
				platform = new PlatformUnix(this);
			} else if ( configurationName.startsWith("osx") ) {
				platform = new PlatformOSX(this);
			}
			
		}
		if (platform == null) {
			throw new UMAException("No platform for " + configurationName);
		}
		return platform;
	}
	
	String rawCopyrightNotice;
	public String getRawCopyrightNotice() throws UMAException {
		if (cCopyrightNotice == null ) {
			GregorianCalendar calendar = new GregorianCalendar();
			rawCopyrightNotice = "Copyright (c) 2000, " + calendar.get(Calendar.YEAR) + " IBM Corp. and others";
		}
		return rawCopyrightNotice;
	}
	
	
	String makefileCopyrightNotice;
	public String getMakefileCopyrightNotice() throws UMAException {
		if (makefileCopyrightNotice == null ) {
			String majorVersion = buildInfo.getMajorVersion();
			String minorVersion = buildInfo.getMinorVersion();
			if (majorVersion == null) majorVersion = "0";
			if (minorVersion == null) minorVersion = "0";
			makefileCopyrightNotice = 
				"#\n" + 
				"#     " + getRawCopyrightNotice() +
				"\n#\n" +
				"#     This program and the accompanying materials are made available under\n" +
				"#     the terms of the Eclipse Public License 2.0 which accompanies this\n" +
				"#     distribution and is available at https://www.eclipse.org/legal/epl-2.0/\n" +
				"#     or the Apache License, Version 2.0 which accompanies this distribution and\n" +
				"#     is available at https://www.apache.org/licenses/LICENSE-2.0.\n" +
				"#\n" +
				"#     This Source Code may also be made available under the following\n" +
				"#     Secondary Licenses when the conditions for such availability set\n" +
				"#     forth in the Eclipse Public License, v. 2.0 are satisfied: GNU\n" +
				"#     General Public License, version 2 with the GNU Classpath\n" +
				"#     Exception [1] and GNU General Public License, version 2 with the\n" +
				"#     OpenJDK Assembly Exception [2].\n" +
				"#\n" +
				"#     [1] https://www.gnu.org/software/classpath/license.html\n" +
				"#     [2] http://openjdk.java.net/legal/assembly-exception.html\n" +
				"#\n" +
				"#     SPDX-License-Identifier: EPL-2.0 OR Apache-2.0\n" +
				"\n# File generated in stream: " + 
				majorVersion + 
				"." + 
				minorVersion + 
				"\n#\n# Autogenerated Code\n";
		} 
		return makefileCopyrightNotice;
	}

	String cCopyrightNotice;
	public String getcCopyrightNotice() throws UMAException {
		if (cCopyrightNotice == null ) {
			cCopyrightNotice = "/*\n * " + getRawCopyrightNotice() +
			"\n * This program and the accompanying materials are made available under\n" +
			" * the terms of the Eclipse Public License 2.0 which accompanies this\n" +
			" * distribution and is available at https://www.eclipse.org/legal/epl-2.0/\n" +
			" * or the Apache License, Version 2.0 which accompanies this distribution and\n" +
			" * is available at https://www.apache.org/licenses/LICENSE-2.0.\n" +
			" *\n" +
			" * This Source Code may also be made available under the following\n" +
			" * Secondary Licenses when the conditions for such availability set\n" +
			" * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU\n" +
			" * General Public License, version 2 with the GNU Classpath\n" +
			" * Exception [1] and GNU General Public License, version 2 with the\n" +
			" * OpenJDK Assembly Exception [2].\n" +
			" *\n" +
			" * [1] https://www.gnu.org/software/classpath/license.html\n" +
			" * [2] http://openjdk.java.net/legal/assembly-exception.html\n" +
			" *\n" +
			" * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0\n" +
			" */\n";
		}
		return cCopyrightNotice;
	}
	
	public boolean evaluateSinglePredicate(String predicate) throws UMAException {
		if (predicate.startsWith("spec.")) {
			if (predicate.startsWith("flags.", "spec.".length())) {
				String flag = predicate.substring("spec.flags.".length());
				if ( !isFlagValid(flag)) {
					throw new UMAException("Error: Invalid flag used: " + predicate);
				}
				return isFlagSet(flag);
			} else if (predicate.startsWith("jcl.", "spec.".length())) {
				String defaultJcl = predicate.substring("spec.jcl.".length());
				if ( !isValidJcl(defaultJcl)) {
					throw new UMAException("Error: Invalid flag used: " + predicate);
				}

				return defaultJcl.matches(getDefaultJcl());
			} else {
				String spec = predicate.substring("spec.".length());
				// must be a platform identifier
				if (getConfigurationName().equalsIgnoreCase(spec)) {
					return true;
				} else if (getConfigurationName().matches(spec)) {
					return true;
				} else {
					return false;
				}
			}
		} 
		
		throw new UMAException("Error: Unknown predicate condition " + predicate );
	}
	
	public TemplateModel getDataModelExtension(String prefixTag, String extensionTag) throws UMAException {
		if ( prefixTag.equals(com.ibm.uma.UMA.FREEMARKER_UMA) ) {
			if ( extensionTag.equals("buildinfo") ) {
				return new com.ibm.j9.uma.configuration.freemarker.BuildInfo(this); 
			}
		} else if (prefixTag.startsWith("uma.spec.flags.") ) {
			return new com.ibm.j9.uma.configuration.freemarker.Flag( prefixTag.substring("uma.spec.flags.".length()), extensionTag, this);
		} else if (prefixTag.equals("uma.spec")) {
			if (extensionTag.equalsIgnoreCase("owners")) {
				return new SimpleSequence(buildSpec.getOwnerIds());
			} else if (extensionTag.equalsIgnoreCase("features")) {
				return new Features(this);
			} else if (extensionTag.equalsIgnoreCase("source")) {
				return new Source(this);
			} 
			return new com.ibm.j9.uma.configuration.freemarker.SimpleSpec(extensionTag, this);
		}
		return null;
	}
	
	public TemplateModelIterator getPropertiesIterator() throws UMAException {
		return new com.ibm.j9.uma.configuration.freemarker.PropertiesIterator(this);
	}
	
	protected String getJITVersionTag(String jitVersionFileName, String buildTag) {
		/* Looking to parse out the version from a line that looks like:
		 * #define TR_LEVEL_NAME "dev_20130104_30951" 
		 * */
		Pattern pattern = Pattern.compile("#define TR_LEVEL_NAME \"(.*)\"");

		String jitVersionString = "jitVersionCouldNotBeParsed";
		if ( jitVersionFileName.equals("") ) {
				System.out.print("jitVersionFile not set. Proceeding as a developer build.\n");
				jitVersionString = buildTag;
		} else {
			try {
				File jitVersionFile = new File(jitVersionFileName);
				BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(jitVersionFile)));
				String line = reader.readLine();
				while ( line != null ) {
					Matcher matcher = pattern.matcher(line);			
					if (matcher.matches()) {
						jitVersionString = matcher.group(1);
						break;
					}
					line = reader.readLine();
				}
				reader.close();
			} catch (FileNotFoundException e) {
				jitVersionString = "jitVersionFileNotFound";
				System.out.println("jitVersionFileNotFound: " + jitVersionFileName);
				System.exit(-1);
			} catch (IOException e) {
				jitVersionString = "jitVersionFileReadError";
				System.out.println("jitVersionFileReadError: " + jitVersionFileName);
				System.exit(-1);
			}
		}
		return jitVersionString;
	}

	public List<String> getExcludedArtifacts() {
		Vector<String> listOfExcludeArtifacts = new Vector<String>();
		
		for (String excludeArtifact : excludeArtifacts ) {
			listOfExcludeArtifacts.add(excludeArtifact);
		}
		return listOfExcludeArtifacts;
	}

	public String getAdditionalIncludesForArtifact(Artifact artifact)
			throws UMAException {
		if ((buildInfo.getMinorVersion().equalsIgnoreCase("4") || buildInfo.getMinorVersion().equalsIgnoreCase("6")) 
			&& buildInfo.getMajorVersion().equalsIgnoreCase("2")) {
			return  "$(UMA_PATH_TO_ROOT)include $(UMA_PATH_TO_ROOT)oti ";
		}
		return "";
	}

	public void verify() throws UMAException {
		try {
			buildSpec.verify(flagDefs, buildInfo);
		} catch (InvalidBuildSpecException e) {
			throw new UMAException(e);
		}
	}

}
