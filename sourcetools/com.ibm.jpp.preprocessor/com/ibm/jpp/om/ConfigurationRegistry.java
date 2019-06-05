/*******************************************************************************
 * Copyright (c) 1999, 2019 IBM Corp. and others
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
package com.ibm.jpp.om;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import com.ibm.jpp.xml.XMLException;

public class ConfigurationRegistry {

	/**
	 * The default configuration XML for a registry.
	 */
	public static final String DEFAULT_XML = "jpp_configuration.xml";

	private int configVersion;

	private final String name;
	private final Map<String, ConfigObject> configsByName;
	private final List<ConfigObject> configs;
	private final List<ConfigObject> localConfigs;
	private final Set<String> validFlags;
	private String baseDir;
	private String srcRoot;
	private final Map<String, String> incompleteConfigs;

	// Automated Test Metadata
	private String testsProject;
	private final List<ClassPathEntry> testsClassPaths;
	private final List<Src> testsSources;

	/**
	 * Constructor
	 *
	 * @param       baseDir         the registry's base directory
	 * @param       srcRoot         the registry's source root path
	 */
	public ConfigurationRegistry(String baseDir, String srcRoot) {
		testsClassPaths = new ArrayList<>();
		testsSources = new ArrayList<>();

		configsByName = new HashMap<>();
		configs = new ArrayList<>();
		localConfigs = new ArrayList<>();
		validFlags = new HashSet<>();
		incompleteConfigs = new HashMap<>();

		this.name = srcRoot;

		if (baseDir.endsWith(File.separator)) {
			this.baseDir = baseDir;
		} else {
			StringBuffer buffer = new StringBuffer(baseDir);
			buffer.append(File.separator);
			this.baseDir = buffer.toString();
		}

		if (srcRoot.endsWith(File.separator)) {
			this.srcRoot = srcRoot;
		} else {
			StringBuffer buffer = new StringBuffer(srcRoot);
			buffer.append(File.separator);
			this.srcRoot = buffer.toString();
		}
	}

	/**
	 * Returns the name of the registry.
	 *
	 * @return Returns the configuration registry name.
	 */
	public String getName() {
		return name;
	}

	/**
	 * Returns the name of the automated tests project associated with this
	 * registry (and all of its configurations).
	 *
	 * @return      the automated tests project name
	 */
	public String getTestsProject() {
		return testsProject;
	}

	/**
	 * Returns the {@link ConfigObject} specified by name.
	 *
	 * @param       name        the name of the desired configuration
	 * @return      the configuration
	 */
	public ConfigObject getConfiguration(String name) {
		return (name == null) ? null : configsByName.get(name);
	}

	/**
	 * Returns a {@link Set} of the registered configurations.
	 *
	 * @return      the registered configurations as a Set
	 */
	public Collection<ConfigObject> getConfigurationsAsCollection() {
		return configs;
	}

	/**
	 * Returns a {@link Set} of the names of the registered configurations.
	 *
	 * @return      the registered configuration names as a Set
	 */
	public Set<String> getConfigurationNamesAsSet() {
		return configsByName.keySet();
	}

	/**
	 * Returns an array of all configurations in this registry.  This includes all
	 * configurations inherited from required XML files.
	 *
	 * @return      the configurations
	 */
	public ConfigObject[] getConfigurations() {
		return configs.toArray(new ConfigObject[configs.size()]);
	}

	/**
	 * Returns an array of the configurations defined in the parent XML file.  This
	 * <b>does not include</b> the configurations inherited from the required XML files.
	 *
	 * @return      the local configurations
	 */
	public ConfigObject[] getLocalConfigurations() {
		return localConfigs.toArray(new ConfigObject[localConfigs.size()]);
	}

	/**
	 * Checks to see if the specified flag is valid.
	 *
	 * @param       name        the name of the flag to be check
	 * @return      <code>true</code> for a valid flag, <code>false</code> otherwise
	 */
	public boolean isValidFlag(String name) {
		return validFlags.contains(name);
	}

	/**
	 * Returns a HashSet of the Valid flags.
	 *
	 * @return      the HashSet of the valid flags.
	 */
	public Set<String> getValidFlags() {
		return validFlags;
	}

	/**
	 * Returns a list of all the configurations that are invalid as a result of
	 * improper or invalid tags in the XML file.
	 *
	 * @return      the list of the incomplete configurations
	 */
	public Map<String, String> getIncompleteConfigs() {
		return incompleteConfigs;
	}

	/**
	 * Sets the new base directory for all the configurations to be added to this
	 * registry.
	 *
	 * @param       newBaseDir      the new base dir path
	 */
	public void setBaseDir(String newBaseDir) {
		this.baseDir = (newBaseDir != null) ? newBaseDir : "";
	}

	/**
	 * Returns the base directory for all the configurations.
	 *
	 * @return      the base directory
	 */
	public String getBaseDir() {
		return baseDir;
	}

	/**
	 * Sets the new source root for all the configurations to be added to this
	 * registry.
	 *
	 * @param       newSrcRoot      the new source root path
	 */
	public void setSourceRoot(String newSrcRoot) {
		this.srcRoot = (newSrcRoot == null) ? "" : File.pathSeparator + newSrcRoot + File.separator;
	}

	/**
	 * Returns the source root for all the configurations
	 *
	 * @return      the source root path
	 */
	public String getSourceRoot() {
		return srcRoot;
	}

	/**
	 * Registers a configuration with this configuration registry.
	 * <p>
	 * NOTE: A configuration can be defined as local if it was defined in the registry's
	 * own XML file as opposed to one of the registry's required XMLs.
	 *
	 * @param       config      the new configuration
	 * @param       local       <code>true</code> if the config was declared in this registry's XML, <code>false</code> otherwise
	 * @return      <code>true</code> if the registration was successful, <code>false</code> otherwise
	 */
	public boolean registerConfiguration(ConfigObject config, boolean local) {
		config.setRegistry(this);
		if (config.checkDependencies()) {
			config.updateWithDependencies();

			if (local) {
				localConfigs.add(config);
			}
			configs.add(config);
			configsByName.put(config.getName(), config);
			validFlags.addAll(config.getFlags());
			return true;
		} else {
			return false;
		}
	}

	/**
	 * Used if a configuration is added with the same name as a previous one.
	 * It is not a recommended practice to override the an existing configuration
	 * as it may produce unexpected results.
	 * <p>
	 * A configuration can be also defined as local if it was defined in the registry's
	 * own XML file as opposed to one of the registry's required XMLs.
	 *
	 * @param       config      the new configuration
	 * @param       local       <code>true</code> if the config was declared in this registry's XML, <code>false</code> otherwise
	 * @return      <code>true</code> if the registration was successful, <code>false</code> otherwise
	 */
	public boolean overrideConfiguration(ConfigObject config, boolean local) {
		config.setRegistry(this);
		if (config.checkDependencies()) {
			config.updateWithDependencies();

			if (local) {
				localConfigs.remove(config);
				localConfigs.add(config);
			}
			configs.remove(config);
			configs.add(config);
			configsByName.put(config.getName(), config);
			validFlags.addAll(config.getFlags());
			return true;
		} else {
			return false;
		}
	}

	/**
	 * Method to load registry configurations from the specified XML file
	 *
	 * @param       filename    the location of the jpp_configuration.xml
	 *
	 * @throws      FileNotFoundException   if the file specified by filename cannot be found.
	 * @throws      XMLException    if there are errors in the XML file
	 */
	public void registerXMLSet(String filename) throws FileNotFoundException, XMLException {
		if (!filename.startsWith(File.separator) && !filename.startsWith(":", 1)) {
			StringBuffer buffer = new StringBuffer(baseDir);
			//          buffer.append(File.separator);
			buffer.append(srcRoot);
			//          buffer.append(File.separator);
			buffer.append(filename);
			filename = buffer.toString();
		}

		ConfigXMLHandler xmlHandler = new ConfigXMLHandler(filename);
		xmlHandler.setBaseDir(baseDir);
		xmlHandler.setSourceRoot(srcRoot);
		xmlHandler.parseInput();

		// Gets the automated tests variables
		testsProject = xmlHandler.getTestsProject();
		testsClassPaths.addAll(xmlHandler.getTestsClassPaths());
		testsSources.addAll(xmlHandler.getTestsSources());

		setConfigVersion(xmlHandler.getConfigVersion());

		List<ConfigObject> tempLocalConfigs = xmlHandler.getLocalConfigs();
		List<ConfigObject> tempConfigs = xmlHandler.getAllConfigs();

		for (ConfigObject currentConfig : tempConfigs) {
			boolean failed = false;

			if (currentConfig.isSet() || (currentConfig.getSourceCount() > 0 && currentConfig.getOutputPath() != null)) {
				if (getConfiguration(currentConfig.getName()) != null) {
					//Note that overriding is being done
					StringBuffer buffer = new StringBuffer("Warning: Multiple definitions of ");
					buffer.append(currentConfig.getName());
					buffer.append(" possible errors in dependencies");
					System.err.println(buffer.toString());
					failed = !overrideConfiguration(currentConfig, tempLocalConfigs.contains(currentConfig));
				} else {
					failed = !registerConfiguration(currentConfig, tempLocalConfigs.contains(currentConfig));
				}

				if (failed) {
					StringBuffer buffer = new StringBuffer("Warning dependencies could not be resolved: ");
					buffer.append(currentConfig.getName());
					System.err.println(buffer.toString());
					//                  unresolvedDependencies.add(currentConfig.getName());
				}
			} else {
				StringBuffer buffer = new StringBuffer();

				if (currentConfig.getSourceCount() == 0) {
					buffer.append("Warning ");
					buffer.append(currentConfig.getName());
					buffer.append(" does not have sources");
				}

				if (currentConfig.getSourceCount() == 0 && currentConfig.getOutputPath() == null) {
					buffer.append("\n");
				}

				if (currentConfig.getOutputPath() == null) {
					buffer.append("Warning ");
					buffer.append(currentConfig.getName());
					buffer.append(" does not have an output path");
				}

				System.err.println(buffer.toString());
				incompleteConfigs.put(currentConfig.getName(), buffer.toString());
			}
		}
	}

	/**
	 * Adds a classpath entry to this registry.  This classpath entry will be
	 * used by the eclipse plugin to generate an automated test project classpath.
	 *
	 * @param       path            the classpath
	 * @param       type            the type of classpath
	 * @param       isExported      <code>true</code> if this entry is contributed to dependent projects, <code>false</code> otherwise
	 */
	public void addClassPathEntry(String path, String type, boolean isExported) {
		testsClassPaths.add(new ClassPathEntry(path, type, isExported));
	}

	/**
	 * Adds a classpath entry to this registry.  This classpath entry will be
	 * used by the eclipse plugin to generate an automated test project classpath.
	 *
	 * @param       path            the classpath
	 * @param       type            the type of classpath
	 * @param       sourcePath      the classpath's source path
	 * @param       isExported      <code>true</code> if this entry is contributed to dependent projects, <code>false</code> otherwise
	 */
	public void addClassPathEntry(String path, String type, String sourcePath, boolean isExported) {
		testsClassPaths.add(new ClassPathEntry(path, type, sourcePath, isExported));
	}

	/**
	 * Returns this registry's automated tests' classpath entries.
	 *
	 * @return      the classpath entries
	 */
	public ClassPathEntry[] getClassPathEntries() {
		return (!testsClassPaths.isEmpty()) ? (ClassPathEntry[]) testsClassPaths.toArray(new ClassPathEntry[testsClassPaths.size()]) : new ClassPathEntry[0];
	}

	/**
	 * Returns this registry's automated tests' source entries.
	 *
	 * @return      the source entries
	 */
	public List<Src> getTestsSources() {
		return testsSources;
	}

	/**
	 * @see         java.lang.Object#toString()
	 */
	@Override
	public String toString() {
		StringBuffer buffer = new StringBuffer("\nName: ");
		buffer.append(name);

		buffer.append("\nConfigurations: ");
		buffer.append(getConfigurationNamesAsSet().toString());
		return buffer.toString();
	}

	/**
	 * @see         java.lang.Object#equals(java.lang.Object)
	 */
	@Override
	public boolean equals(Object obj) {
		if (obj instanceof ConfigurationRegistry) {
			ConfigurationRegistry objReg = (ConfigurationRegistry) obj;
			return (name.equals(objReg.getName()) && baseDir.equals(objReg.getBaseDir()) && srcRoot.equals(objReg.getSourceRoot()));
		} else {
			return false;
		}
	}

	/**
	 * @see java.lang.Object#hashCode()
	 */
	@Override
	public int hashCode() {
		return name.hashCode() + baseDir.hashCode() + srcRoot.hashCode();
	}

	private void setConfigVersion(int ver) {
		this.configVersion = ver;
	}

	public int getConfigVersion() {
		return configVersion;
	}

}
