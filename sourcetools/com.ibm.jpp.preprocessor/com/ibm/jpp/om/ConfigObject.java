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
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Properties;
import java.util.Set;

import com.ibm.jpp.commandline.CommandlineLogger;
import com.ibm.jpp.commandline.CommandlineNowarnLogger;

/**
 * A config object is a preprocessor configuration used to generate a version of the
 * J9 Java Class Libraries (JCL).
 * <p>
 * A <code>ConfigObject</code> identifies all of the flags associated with said preprocessor
 * configuration and at the same time defines the output path, source paths, classpath entries,
 * and dependent jobs.  Dependent jobs are <code>ConfigObject</code> dependencies that need to
 * be preprocessed before and that inherit the options and output path of the parent.
 * <p>
 * Furthermore, <code>ConfigObject</code>s are collected in a {@link ConfigurationRegistry} which
 * guarantees name uniqueness.  Note that name uniqueness is not guaranteed across registryies.
 *
 * @see			com.ibm.jpp.om.ConfigurationRegistry
 * @see			com.ibm.jpp.om.MetaRegistry
 * @see			com.ibm.jpp.om.Src
 */
public class ConfigObject {
	private final String name;
	private final boolean isConfiguration; // If this is a configuration or a set
	private String outputPath;
	private String outputPathKeyword;

	private String testsTempOutputPath;
	private String testsOutputPath;
	private String testsTempBootOutputPath;
	private String testsBootOutputPath;

	private final Map<String, String> defaultTestsResources;
	private final Map<String, String> testsResources;
	private boolean useTestResourcesDefaults = false;

	private String baseDir;
	private final Set<String> addDependFlagSet;
	private final Set<String> removeDependFlagSet;
	private final Set<String> requiredIncludeFlagSet;
	private final Set<String> flagSet;
	private final Set<String> removeFlagSet;
	private String[] flagArray = null;
	private final List<String> optionSet; // The order matters, pairs of name/value
	private final List<String> compilerOptionSet; // The order matters, pairs of name/value
	private final List<Src> sources;
	private final List<ClassPathEntry> classPathEntries;
	private final Map<String, ConfigObject> dependJobs;
	private String JDKCompliance;
	private boolean metadata = false;
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	// prefixes and suffixes
	private String outputPathPrefix = "";
	private String outputPathSuffix = "";
	private String testsOutputPathPrefix = "";
	private String testsOutputPathSuffix = "";
	private String bootTestsOutputPathPrefix = "";
	private String bootTestsOutputPathSuffix = "";

	/* [PR 120359] New classpath entry is needed for configurations */
	private String configXMLParserError = "";
	// Registry containing this ConfigObject (aka parent registry)
	private ConfigurationRegistry registry;

	/**
	 * Class constructor.
	 *
	 * @param       name                the configuration's name
	 * @param       isConfiguration     <code>true</code> if this is a configuration, <code>false</code> if this is a set
	 */
	public ConfigObject(String name, boolean isConfiguration) {
		this(name, "", isConfiguration);
	}

	/**
	 * Class constructor.
	 *
	 * @param       name                the configuration's name
	 * @param       baseDir             the base directory for this configuration
	 * @param       isConfiguration     <code>true</code> if this is a configuration, <code>false</code> if this is a set
	 */
	public ConfigObject(String name, String baseDir, boolean isConfiguration) {
		this.name = name;
		this.baseDir = baseDir;
		this.isConfiguration = isConfiguration;

		flagSet = new HashSet<>();
		requiredIncludeFlagSet = new HashSet<>();
		removeFlagSet = new HashSet<>();
		addDependFlagSet = new HashSet<>();
		removeDependFlagSet = new HashSet<>();

		classPathEntries = new ArrayList<>();
		optionSet = new ArrayList<>();
		compilerOptionSet = new ArrayList<>();
		sources = new ArrayList<>();
		dependJobs = new HashMap<>();
		/* [PR 121295] Support for resources' prefixes and directories */
		defaultTestsResources = new HashMap<>();
		testsResources = new HashMap<>();
	}

	/**
	 * Returns whether this ConfigObject is a configuration or not.
	 *
	 * @return      <code>true</code> if this config object is a configuration, <code>false</code> otherwise
	 *
	 * @see         #isSet()
	 */
	public boolean isConfiguration() {
		return isConfiguration;
	}

	/**
	 * Returns whether this ConfigObject is a set or not.
	 *
	 * @return      <code>true</code> if this config object is a set, <code>false</code> otherwise
	 *
	 * @see         #isConfiguration()
	 */
	public boolean isSet() {
		return !isConfiguration;
	}

	/**
	 * Returns this ConfigObject's name (or label).
	 *
	 * @return      the name
	 */
	public String getName() {
		return name;
	}

	/**
	 * Returns whether or not tests can be generated for this config.
	 *
	 * @return      <code>true</code> if tests can be generated, <code>false</code> otherwise
	 */
	public boolean canGenerateTests() {
		return (registry.getTestsProject() != null && !registry.getTestsProject().equals(""));
	}

	/**
	 * Sets this ConfigObject's JDK Compiler compliance version.
	 *
	 * @param       version     the JDK version
	 */
	public void setJDKCompliance(String version) {
		JDKCompliance = version;
	}

	/**
	 * Returns this ConfigObject's JDK Compiler compliance version.
	 *
	 * @return      the JDK compliance version
	 */
	public String getJDKCompliance() {
		return JDKCompliance;
	}

	/**
	 * Sets whether or not preprocessor metadata will be generated for this config.
	 *
	 * @param       metadata    <code>true</code> if metadata is to be generated,
	 *                          <code>false</code> otherwise
	 */
	public void enableMetadata(boolean metadata) {
		this.metadata = metadata;
	}

	/**
	 * Returns whether or not preprocessor metadata is enabled.
	 *
	 * @return      <code>true</code> if metadata will be written, <code>false</code> otherwise
	 */
	public boolean isMetadataEnabled() {
		return metadata;
	}

	/**
	 * Adds a classpath entry to this ConfigObject.  This classpath entry will be
	 * used by the eclipse plugin to generate a project classpath.
	 *
	 * @param       path            the classpath
	 * @param       type            the type of classpath
	 * @param       isExported      <code>true</code> if this entry is contributed to dependent projects, <code>false</code> otherwise
	 */
	public void addClassPathEntry(String path, String type, boolean isExported) {
		classPathEntries.add(new ClassPathEntry(path, type, isExported));
	}

	/**
	 * Adds a classpath entry to this ConfigObject.  This classpath entry will be
	 * used by the eclipse plugin to generate a project classpath.
	 *
	 * @param       path            the classpath
	 * @param       type            the type of classpath
	 * @param       sourcePath      the classpath's source path
	 * @param       isExported      <code>true</code> if this entry is contributed to dependent projects, <code>false</code> otherwise
	 */
	public void addClassPathEntry(String path, String type, String sourcePath, boolean isExported) {
		classPathEntries.add(new ClassPathEntry(path, type, sourcePath, isExported));
	}

	/**
	 * Adds a classpath entry to this ConfigObject.  This classpath entry will be
	 * used by the eclipse plugin to generate a project classpath.
	 *
	 * @param       classpath       the classpath entry
	 */
	public void addClassPathEntry(ClassPathEntry classpath) {
		classPathEntries.add(classpath);
	}

	/**
	 * Returns this ConfigObject's classpath entries.
	 *
	 * @return      the classpath entries
	 */
	public ClassPathEntry[] getClassPathEntries() {
		return classPathEntries.toArray(new ClassPathEntry[classPathEntries.size()]);
	}

	/**
	 * Adds a flag to this ConfigObject.
	 *
	 * @param       flag        the flag to be added
	 */
	public void addFlag(String flag) {
		flagSet.add(flag);
	}

	/* [PR 117967] idea 491: Automatically create the jars required for test bootpath */
	/**
	 * Adds a flag to removeFlagSet.
	 *
	 * @param       flag        the flag to be added
	 */
	public void addFlagToRemoveFlagSet(String flag) {
		removeFlagSet.add(flag);
	}

	/**
	 * Removes a flag from this ConfigObject.
	 *
	 * @param       flag        the flag to be removed
	 */
	public void removeFlag(String flag) {
		flagSet.remove(flag);
	}

	/* [PR 117967] idea 491: Automatically create the jars required for test bootpath */
	/**
	 * Removes a flag from requiredflagset
	 *
	 * @param       flag        the flag to be removed
	 */
	public void removeRequiredFlag(String flag) {
		requiredIncludeFlagSet.remove(flag);
	}

	/**
	 * Returns the flags associated with this ConfigObject.
	 * <p>
	 * <b>Note:</b> the flagSet isn't completely correct until updateWithDependencies
	 * is called, when the removes and adding of sets happens
	 *
	 * @return      the ConfigObject flags
	 */
	public Set<String> getFlags() {
		return flagSet;
	}

	/**
	 * Returns the flags associated with this ConfigObject.
	 * <p>
	 * <b>Note:</b> the flagSet isn't completely correct until updateWithDependencies
	 * is called, when the removes and adding of sets happens
	 *
	 * @return      the ConfigObject flags
	 *
	 * @see         #getFlags()
	 */
	public String[] getFlagsAsArray() {
		if (flagArray == null || flagArray.length != flagSet.size()) {
			flagArray = new String[flagSet.size()];
			flagSet.toArray(flagArray);
		}
		return flagArray;
	}

	/**
	 * Adds a flag dependency to another configuration or set
	 *
	 * @param       name        the dependency name
	 */
	public void addFlagDependency(String name) {
		addDependFlagSet.add(name);
	}

	/**
	 * Removes a flag dependency to another configuration or set
	 *
	 * @param       name        the dependency name
	 */
	public void removeFlagDependency(String name) {
		removeDependFlagSet.add(name);
	}

	/**
	 * Adds a new required include flag to this ConfigObject.
	 *
	 * @param       newFlag     the new flag to be added.
	 */
	public void addRequiredIncludeFlag(String newFlag) {
		requiredIncludeFlagSet.add(newFlag);
	}

	/**
	 * Returns the required include flags.
	 *
	 * @return      the required include flags
	 */
	public Set<String> getRequiredIncludeFlagSet() {
		return requiredIncludeFlagSet;
	}

	/**
	 * Adds a dependency on another ConfigObject, which then must be preprocessed
	 * first.
	 *
	 * @param       dependName      the name of the dependent ConfigObject
	 * @param       config          the dependent ConfigObject
	 */
	public void addDependJob(String dependName, ConfigObject config) {
		dependJobs.put(dependName, config);
	}

	/**
	 * Returns this ConfigObject's complete configuration dependencies through recursion.
	 *
	 * @return      the configuration dependencies
	 */
	public Map<String, ConfigObject> getAllDependJobs() {
		Map<String, ConfigObject> allDependencies = new HashMap<>();
		allDependencies.putAll(dependJobs);

		// Recursively get the configuration dependencies
		if (!dependJobs.isEmpty()) {
			ConfigObject[] dependConfigs = dependJobs.values().toArray(new ConfigObject[dependJobs.size()]);

			for (ConfigObject dependConfig : dependConfigs) {
				if (dependConfig != null) {
					allDependencies.putAll(dependConfig.getDependJobs());
				}
			}
		}

		return allDependencies;
	}

	/**
	 * Returns this ConfigObject's immediate configuration dependencies.
	 *
	 * @return      the configuration dependencies
	 */
	public Map<String, ConfigObject> getDependJobs() {
		return dependJobs;
	}

	/**
	 * Returns this ConfigObject's parent registry
	 *
	 * @return      the parent registry
	 */
	public ConfigurationRegistry getRegistry() {
		return registry;
	}

	/**
	 * Sets this ConfigObject's parent registry
	 *
	 * @param       registry    the registry to set
	 */
	public void setRegistry(ConfigurationRegistry registry) {
		this.registry = registry;
	}

	/**
	 * Checks whether this ConfigObject has flag dependencies that need to be
	 * resolved with {@link #updateWithDependencies()}.
	 *
	 * @return      <code>true</code> if there are flag dependencies to be resolved, <code>false</code> otherwise
	 */
	public boolean checkDependencies() {
		return (!addDependFlagSet.isEmpty() && !removeDependFlagSet.isEmpty()) ? true : registry.getConfigurationNamesAsSet().containsAll(addDependFlagSet);
	}

	/**
	 * Updates the flag dependencies in this ConfigObject.
	 */
	public void updateWithDependencies() {
		for (String configName : addDependFlagSet) {
			ConfigObject configuration = registry.getConfiguration(configName);
			flagSet.addAll(configuration.getFlags());
			requiredIncludeFlagSet.addAll(configuration.getRequiredIncludeFlagSet());
		}

		for (String configName : removeDependFlagSet) {
			ConfigObject configuration = registry.getConfiguration(configName);
			flagSet.removeAll(configuration.getFlags());
			requiredIncludeFlagSet.removeAll(configuration.getRequiredIncludeFlagSet());
		}

		flagSet.removeAll(removeFlagSet);
		requiredIncludeFlagSet.removeAll(removeFlagSet);
	}

	/**
	 * Adds global options to this ConfigObject.  These options will apply to all
	 * sources.
	 *
	 * @param       name        the option name
	 * @param       value       the option value
	 */
	public void addOption(String name, String value) {
		optionSet.add(name);
		optionSet.add(value);
	}

	/* [PR 119500] Design 955 Core.JCL : Support bootpath JCL testing */
	/**
	 * Removes a global option from this ConfigObject. These options will apply to all
	 * sources.
	 *
	 * @param       name        the option name
	 */
	public void removeOption(String name) {
		for (int i = 0; i < optionSet.size();) {
			String option = optionSet.get(i);
			if (option.equals(name)) {
				optionSet.remove(i);
				optionSet.remove(i);
			} else {
				i += 2;
			}
		}
	}

	/* [PR 118829] Desing 894: Core.JCL : Support for compiler options in preprocessor plugin */
	/**
	 * Adds compiler options to this ConfigObject.  These options will apply to
	 * config projects' compiler options
	 *
	 * @param       name        the option name
	 * @param       value       the option value
	 */
	public void addCompilerOption(String name, String value) {
		compilerOptionSet.add(name);
		compilerOptionSet.add(value);
	}

	/**
	 * Returns the global options associated with this ConfigObject.
	 *
	 * @return      the global options
	 */
	public Properties getOptions() {
		Properties options = new Properties();
		int setSize = optionSet.size();
		for (int i = 0; i < setSize; i += 2) {
			options.setProperty(optionSet.get(i), optionSet.get(i + 1));
		}
		return options;
	}

	/**
	 * Returns the compiler options associated with this ConfigObject.
	 *
	 * @return      the compiler options
	 */
	public Properties getCompilerOptions() {
		Properties coptions = new Properties();
		int setSize = compilerOptionSet.size();
		for (int i = 0; i < setSize; i += 2) {
			coptions.setProperty(compilerOptionSet.get(i), compilerOptionSet.get(i + 1));
		}
		return coptions;
	}

	/**
	 * Sets this ConfigObject's base directory.
	 *
	 * @param       newBaseDir      the new base dir
	 */
	public void setBaseDir(String newBaseDir) {
		baseDir = newBaseDir;
	}

	/**
	 * Returns this ConfigObject's base directory.
	 *
	 * @return      the base directory
	 */
	public String getbaseDir() {
		return baseDir;
	}

	/**
	 * Sets the outputPath keyword to get output project names.
	 * @param outputPath  outpath value in jpp_configuration.xml
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public void setOutputPathKeyword(String outputPath) {
		if (!outputPath.startsWith(File.separator) && !outputPath.startsWith(":", 1)) {
			int index = Math.max(outputPath.indexOf("\\"), outputPath.indexOf("/"));
			if (index != -1) {
				outputPathKeyword = outputPath.substring(0, index).trim();
			} else {
				outputPathKeyword = outputPath.trim();
			}
		}
	}

	/**
	 * outputPath name without prefix and suffix
	 * @return outputPathKeyword
	 */
	/* [PR 120359] New classpath entry is needed for configurations */
	public String getOutputPathKeyword() {
		return outputPathKeyword;
	}

	/**
	 * Sets this ConfigObject's output path
	 *
	 * @param       newOutputPath       the new output path
	 */
	public void setOutputPath(String newOutputPath) {
		if (!newOutputPath.startsWith(File.separator) && !newOutputPath.startsWith(":", 1)) {
			StringBuffer buffer = new StringBuffer(baseDir);
			buffer.append(outputPathPrefix + outputPathKeyword + outputPathSuffix + "/src");
			outputPath = buffer.toString();
		} else {
			outputPath = newOutputPath;
		}
	}

	/**
	 * Sets this ConfigObject's tests output path
	 *
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public void setTestsOutputPaths() {
		if (outputPathKeyword != null) {
			testsTempOutputPath = baseDir + testsOutputPathPrefix + outputPathKeyword + testsOutputPathSuffix + "/preprocessedTests";
			testsOutputPath = baseDir + testsOutputPathPrefix + outputPathKeyword + testsOutputPathSuffix + "/src";
		} else {
			int lastIndex = Math.max(outputPath.lastIndexOf("\\"), outputPath.lastIndexOf("/"));
			String token = outputPath.substring(0, lastIndex);
			testsTempOutputPath = token + " Tests/preprocessedTests";
			testsOutputPath = token + " Tests/src";
		}
	}

	/**
	 * Sets this ConfigObject's boot tests output path
	 *
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public void setBootTestsOutputPaths() {
		if (outputPathKeyword != null) {
			testsTempBootOutputPath = baseDir + bootTestsOutputPathPrefix + outputPathKeyword + bootTestsOutputPathSuffix + "/preprocessedBootPathTests";
			testsBootOutputPath = baseDir + bootTestsOutputPathPrefix + outputPathKeyword + bootTestsOutputPathSuffix + "/src";
		} else {
			int lastIndex = Math.max(outputPath.lastIndexOf("\\"), outputPath.lastIndexOf("/"));
			String token = outputPath.substring(0, lastIndex);
			testsTempBootOutputPath = token + " Tests BootPath/preprocessedBootPathTests";
			testsBootOutputPath = token + " Tests BootPath/src";
		}
	}

	/* [PR 117967] idea 491: Automatically create the jars required for test bootpath */
	public void setOutputPathforJCLBuilder(String newOutputPath) {
		String path;
		if (!newOutputPath.startsWith(File.separator) && !newOutputPath.startsWith(":", 1)) {
			StringBuffer buffer = new StringBuffer(baseDir);
			buffer.append(newOutputPath);
			path = buffer.toString();
		} else {
			path = newOutputPath;
		}

		outputPath = path;
		testsTempOutputPath = path;
		testsTempBootOutputPath = path;
	}

	/* [PR 117967] idea 491: Automatically create the jars required for test bootpath */
	/**
	 * Returns this ConfigObject's tests boot path
	 */
	public String getTestsBootPath() {
		return testsTempBootOutputPath;
	}

	/* [PR 118220] Incremental builder is not called when file is deleted in base library */
	/**
	 * Returns this ConfigObject's tests Outputpath
	 */
	public String getTestsOutputPath() {
		return testsOutputPath;
	}

	/**
	 * Returns this ConfigObject's tests Outputpath
	 */
	/* [PR 119753] classes.txt and AutoRuns are not updated when new test class is added */
	public String getBootTestsOutputPath() {
		return testsBootOutputPath;
	}

	/**
	 * Returns this ConfigObject's outputPathPrefix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getOutputPathPrefix() {
		return outputPathPrefix;
	}

	/**
	 * Returns this ConfigObject's outputPathSuffix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getOutputPathSuffix() {
		return outputPathSuffix;
	}

	/**
	 * Returns this ConfigObject's testsOutputPathPrefix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getTestsOutputPathPrefix() {
		return testsOutputPathPrefix;
	}

	/**
	 * Returns this ConfigObject's testsOutputPathSuffix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getTestsOutputPathSuffix() {
		return testsOutputPathSuffix;
	}

	/**
	 * Returns this ConfigObject's bootTestsOutputPathPrefix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getBootTestsOutputPathPrefix() {
		return bootTestsOutputPathPrefix;
	}

	/**
	 * Returns this ConfigObject's bootTestsOutputPathSuffix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getBootTestsOutputPathSuffix() {
		return bootTestsOutputPathSuffix;
	}

	/**
	 * Sets this ConfigObject's OutputPath Prefix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public void setOutputPathPrefix(String prefix) {
		outputPathPrefix = prefix;
	}

	/**
	 * Sets this ConfigObject's OutputPath Suffix
	 */
	public void setOutputPathSuffix(String suffix) {
		outputPathSuffix = suffix;
	}

	/**
	 * Sets this ConfigObject's Tests OutputPath Prefix
	 */
	public void setTestsOutputPathPrefix(String prefix) {
		testsOutputPathPrefix = prefix;
	}

	/**
	 * Sets this ConfigObject's Tests OutputPath Suffix
	 */
	public void setTestsOutputPathSuffix(String suffix) {
		testsOutputPathSuffix = suffix;
	}

	/**
	 * Sets this ConfigObject's Boot Tests OutputPath Prefix
	 */
	public void setBootTestsOutputPathPrefix(String prefix) {
		bootTestsOutputPathPrefix = prefix;
	}

	/**
	 * Sets this ConfigObject's Boot Tests OutputPath Suffix
	 */
	public void setBootTestsOutputPathSuffix(String suffix) {
		bootTestsOutputPathSuffix = suffix;
	}

	/**
	 * Sets this ConfigObject's tests boot path
	 */
	public void setTestsBootPath(String bootpath) {
		testsTempBootOutputPath = bootpath;
	}

	/**
	 * Sets the global resources prefixes to this config object
	 * @param res resources prefixes
	 */
	/* [PR 121295] Support for resources' prefixes and directories */
	public void setDefaultTestsResourcesPrefixes(Map<String, String> res) {
		defaultTestsResources.putAll(res);
		if (defaultTestsResources.keySet().size() == 0) {
			defaultTestsResources.put("*", "j9ts_");
		}
	}

	/**
	 * Sets the config objects's resources prefix. It parses all resources' prefixes and removes the prefixes that are overwritten.
	 * @param dir Directory
	 * @param prefix Prefix of resources
	 */
	/* [PR 121295] Support for resources' prefixes and directories */
	public void setTestsResourcesPrefix(String dir, String prefix) {
		if (!dir.equals("*")) {
			String dirSlash = dir + "/";

			for (Iterator<String> iter = testsResources.keySet().iterator(); iter.hasNext();) {
				String nextDir = iter.next();

				if (nextDir.startsWith(dirSlash) || nextDir.equals(dir)) {
					iter.remove();
				}
			}
		} else {
			testsResources.clear();
		}
		testsResources.put(dir, prefix);
	}

	public void setUseTestResourcesDefaults(boolean def) {
		useTestResourcesDefaults = def;
	}

	public Map<String, String> getTestsResourcesPrefixes() {
		Map<String, String> prefixes = new HashMap<>();
		if (useTestResourcesDefaults) {
			prefixes.putAll(defaultTestsResources);

			for (String dir : testsResources.keySet()) {
				String dirSlash = dir + "/";

				for (Iterator<String> iter = prefixes.keySet().iterator(); iter.hasNext();) {
					String nextDir = iter.next();

					if (nextDir.startsWith(dirSlash) || nextDir.equals(dir)) {
						iter.remove();
					}
				}
			}
			prefixes.putAll(testsResources);
		} else if (testsResources.keySet().size() != 0) {
			prefixes = testsResources;
		} else {
			prefixes = defaultTestsResources;
		}
		return prefixes;
	}

	/**
	 * Returns this ConfigObject's output path.
	 *
	 * @return      the output path
	 *
	 * @see         #getOutputDir()
	 */
	public String getOutputPath() {
		return outputPath;
	}

	/**
	 * Returns this ConfigObject's output path relative to the base directory.
	 *
	 * @return      the relative output path
	 */
	public String getRelativeOutputPath() {
		int pathLength = (baseDir != null && outputPath.length() > baseDir.length()) ? baseDir.length() : 0;
		return (outputPath != null) ? outputPath.substring(pathLength) : null;
	}

	/**
	 * Returns this ConfigObject's tests output path relative to the base directory.
	 *
	 * @return      the relative tests output path
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getRelativeTestsOutputPath() {
		int pathLength = (baseDir != null && testsOutputPath.length() > baseDir.length()) ? baseDir.length() : 0;
		return (testsOutputPath != null) ? testsOutputPath.substring(pathLength) : null;
	}

	/**
	 * Returns this ConfigObject's boot tests output path relative to the base directory.
	 *
	 * @return      the relative boo
	 * ttests output path
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getRelativeBootTestsOutputPath() {
		int pathLength = (baseDir != null && testsBootOutputPath.length() > baseDir.length()) ? baseDir.length() : 0;
		return (testsBootOutputPath != null) ? testsBootOutputPath.substring(pathLength) : null;
	}

	/**
	 * Returns this ConfigObject's output directory.
	 *
	 * @return      the output dir
	 *
	 * @see         #getOutputPath()
	 * @see         #getRelativeOutputPath()
	 */
	public File getOutputDir() {
		return (outputPath != null) ? new File(outputPath) : null;
	}

	/**
	 * Adds a config source to this ConfigObject.
	 *
	 * @param       newSource   the new config source
	 */
	public void addSource(Src newSource) {
		sources.add(newSource);
	}

	/**
	 * Returns the number of sources defined for this ConfigObject.
	 *
	 * @return      the number of defined sources
	 */
	public int getSourceCount() {
		return sources.size();
	}

	/**
	 * Returns the sources defined for this ConfigObject.
	 *
	 * @return      the config sources
	 */
	public Src[] getSources() {
		return sources.toArray(new Src[sources.size()]);
	}

	/**
	 * Returns this ConfigObject's source paths relative to the base directory.
	 *
	 * @return      the relative source paths
	 *
	 * @see         #getRelativeSrcDirs()
	 */
	public String[] getRelativeSrcPaths() {
		String[] returnArray = new String[sources.size()];
		for (int i = 0; i < returnArray.length; i++) {
			returnArray[i] = sources.get(i).getRelativeSrcPath();
		}
		return returnArray;
	}

	/**
	 * Returns this ConfigObject's source directories relative to the base directory.
	 *
	 * @return      the relative source dirs
	 *
	 * @see         #getRelativeSrcPaths()
	 */
	public File[] getRelativeSrcDirs() {
		File[] returnArray = new File[sources.size()];
		for (int i = 0; i < returnArray.length; i++) {
			returnArray[i] = sources.get(i).getRelativeSrcFolder();
		}
		return returnArray;
	}

	/**
	 * Returns this ConfigObject's absolute source directories.
	 *
	 * @return      the absolute source dirs
	 */
	public String[] getAbsoluteSrcPaths() {
		String[] returnArray = new String[sources.size()];
		for (int i = 0; i < returnArray.length; i++) {
			returnArray[i] = baseDir + sources.get(i).getRelativeSrcPath();
		}
		return returnArray;
	}

	/**
	 * Returns this ConfigObject's absolute source directories.
	 *
	 * @return      the absolute source dirs
	 */
	public File[] getAbsoluteSrcDirs() {
		File[] returnArray = new File[sources.size()];
		for (int i = 0; i < returnArray.length; i++) {
			returnArray[i] = new File(baseDir + sources.get(i).getRelativeSrcPath());
		}
		return returnArray;
	}

	/**
	 * Preprocesses this configuration object.  If the ConfigObject has any config dependencies,
	 * those will be preprocessed first (automatically).
	 * <p>
	 * <b>NOTE:</b> the dependencies will inherit the configuration options and the output
	 * directory of the parent.
	 *
	 * @param       incremental     <code>true</code> if this build is incremental
	 * @param       noWarn          <code>true</code> if a logger with no warnings is to be used
	 */
	public boolean build(boolean incremental, boolean noWarn) {
		return build(incremental, noWarn, new BuilderExtension[0], null);
	}

	/* [PR 121491] Use correct build method and correct commandline input values for preprocessing jobs */
	public boolean buildTests(boolean incremental, boolean noWarn) {
		return buildTests(incremental, noWarn, new BuilderExtension[0], null);
	}

	/* [PR 121584]Add option to jcl-builder to use testsBootPath JavaDoc */
	public boolean buildTestBootpath(boolean incremental, boolean noWarn, boolean useTestBootpathJavaDoc) {
		return buildTestBootpath(incremental, noWarn, useTestBootpathJavaDoc, new BuilderExtension[0], null, false);
	}

	/**
	 * Preprocesses this configuration object.  If the ConfigObject has any config dependencies,
	 * those will be preprocessed first (automatically).
	 * <p>
	 * <b>NOTE:</b> the dependencies will inherit the configuration options and the output
	 * directory of the parent.
	 *
	 * @param       incremental     <code>true</code> if this build is incremental
	 * @param       noWarn          <code>true</code> if a logger with no warnings is to be used
	 * @param       extensions      builder extensions to be added to the preprocess
	 * @param       logger          the build logger to use, if <code>null</code> one will be created
	 */
	public boolean build(boolean incremental, boolean noWarn, BuilderExtension[] extensions, Logger logger) {
		// Preprocess all dependent jobs first
		if (!dependJobs.isEmpty()) {
			for (Entry<String, ConfigObject> entry : dependJobs.entrySet()) {
				ConfigObject dependConfig = entry.getValue();

				if (dependConfig != null) {
					Properties options = this.getOptions();
					Enumeration<?> keys = options.keys();
					while (keys.hasMoreElements()) {
						String keyName = keys.nextElement().toString();
						dependConfig.addOption(keyName, options.getProperty(keyName));
					}

					// requires setting the destDir
					String tempOutputPath = dependConfig.getOutputPath();
					dependConfig.setOutputPath(this.getOutputPath());
					dependConfig.build(incremental, noWarn, extensions, logger);

					// Return the output path to its initial value
					dependConfig.setOutputPath(tempOutputPath);
				} else {
					System.err.println("\nCould not find dependent configuration: " + entry.getKey());
				}
			}
		}

		boolean result = true;
		int multipleSources = 0;
		// Preprocess this ConfigObject
		for (int i = 0; i < sources.size(); ++i) {
			Src source = sources.get(i);
			File srcPath = new File(this.baseDir + source.getRelativeSrcPath());
			if (!source.isSimpleCopy()) {
				if (srcPath.exists()) {
					Builder builder = new Builder();
					builder.setConfiguration(this);
					builder.setIncremental(incremental);
					builder.setMetadata(metadata);
					builder.setOptions(source.getOptions());

					for (BuilderExtension extension : extensions) {
						builder.addExtension(extension);
					}

					if (logger != null) {
						builder.setLogger(logger);
					} else if (noWarn) {
						builder.setLogger(new CommandlineNowarnLogger());
					} else {
						builder.setLogger(new CommandlineLogger());
					}

					// Allow the builder to be aware of other sources (if there are any).
					// In the current setup the builder only preprocesses one source at a time
					// so it's not aware of other sources. This becomes an issue when external
					// messages must be written out. This allows the ExternalMessagesExtension
					// to check if it should load the messages already in the output file.
					if (++multipleSources > 1) {
						builder.setMultipleSources(true);
					}

					builder.setSourceDir(srcPath);
					result &= builder.build();
				} else {
					System.out.println("Potential source : " + srcPath.toString() + " does not exist");
					result = false;
				}
			} else {
				String simpleOutput = source.getSimpleCopyOutput();

				SimpleCopy simpleCopy = new SimpleCopy();
				simpleCopy.setBaseDir(baseDir);
				simpleCopy.setSourcePath(srcPath);
				simpleCopy.setSimpleOutput(simpleOutput);
				simpleCopy.setOutputDir(this.getOutputDir());
				result &= simpleCopy.copy();
			}
		}
		return result;
	}

	/* [PR 121491] Use correct build method and correct commandline input values for preprocessing jobs */
	public boolean buildTests(boolean incremental, boolean noWarn, BuilderExtension[] extensions, Logger logger) {
		return buildTests(incremental, noWarn, extensions, logger, false);
	}

	/**
	 * Preprocesses this configuration object's automated tests.
	 *
	 * @param       incremental     <code>true</code> if this build is incremental
	 * @param       noWarn          <code>true</code> if a logger with no warnings is to be used
	 * @param       extensions      builder extensions to be added to the preprocess
	 * @param       logger          the build logger to use, if <code>null</code> one will be created
	 * @return      <code>true</code> if the classes.txt is updated, <code>false</code> otherwise.
	 */
	public boolean buildTests(boolean incremental, boolean noWarn, BuilderExtension[] extensions, Logger logger, boolean callFromEclipse) {
		boolean result = true;
		int multipleSources = 0;
		List<Src> testsSources = this.registry.getTestsSources();
		String sourceRoot = this.registry.getSourceRoot();
		Builder builder = new Builder();
		builder.setConfiguration(this);
		builder.setIncremental(incremental);
		builder.setMetadata(metadata);
		/* [PR 116982] Preprocessor should not put INCLUDE-IF warnings on Automated Tests */
		if (this.registry.getConfigVersion() < 3) {
			builder.setNoWarnIncludeIf(true);
			builder.setIncludeIfUnsure(true);
		}
		for (BuilderExtension extension : extensions) {
			builder.addExtension(extension);
		}

		if (logger != null) {
			builder.setLogger(logger);
		} else if (noWarn) {
			builder.setLogger(new CommandlineNowarnLogger());
		} else {
			builder.setLogger(new CommandlineLogger());
		}
		if (callFromEclipse) {
			if (this.registry.getConfigVersion() < 3) {
				builder.setOutputDir(new File(testsTempOutputPath));
			} else {
				builder.setOutputDir(new File(testsOutputPath));
			}
		}

		/* Process explicit source paths for automated tests */
		if (testsSources != null) {
			for (int i = 0; i < testsSources.size(); i++) {
				File testsSrc = null;
				String srcfolder = testsSources.get(i).getRelativeSrcPath();
				if (callFromEclipse) {
					testsSrc = new File(this.baseDir + this.registry.getTestsProject(), srcfolder);
				} else {
					testsSrc = new File(this.baseDir + sourceRoot + srcfolder);
				}
				if (testsSrc.exists()) {
					// Allow the builder to be aware of other sources (if there are any).
					//                  // In the current setup the builder only preprocesses one source at a time
					//                                      // so it's not aware of other sources. This becomes an issue when external
					//                                                          // messages must be written out. This allows the ExternalMessagesExtension
					//                                                                              // to check if it should load the messages already in the output file.
					if (++multipleSources > 1) {
						builder.setMultipleSources(true);
					}
					builder.setSourceDir(testsSrc);
					result &= builder.build();
				} else {
					// this is just a warning, and does not indicate a failure
					System.out.println("Warning: Tests source directory does not exist: " + testsSrc.getAbsolutePath());
				}
			}
		}

		/* [PR 119039] Design 917: Multiple source folder support for automated tests */
		for (Src source : sources) {
			if (!source.isSimpleCopy()) {
				String srcfolder = source.getRelativeSrcPath();
				if (srcfolder.startsWith(sourceRoot)) {
					/* [PR 121491] Use correct build method and correct commandline input values for preprocessing jobs */
					File testsSrc = null;
					if (callFromEclipse) {
						srcfolder = srcfolder.substring(sourceRoot.length());
						testsSrc = new File(this.baseDir + this.registry.getTestsProject(), srcfolder);
					} else {
						testsSrc = new File(this.baseDir + source.getRelativeSrcPath());
					}
					if (testsSrc.exists()) {
						// Allow the builder to be aware of other sources (if there are any).
						// In the current setup the builder only preprocesses one source at a time
						// so it's not aware of other sources. This becomes an issue when external
						// messages must be written out. This allows the ExternalMessagesExtension
						// to check if it should load the messages already in the output file.
						if (++multipleSources > 1) {
							builder.setMultipleSources(true);
						}
						builder.setSourceDir(testsSrc);
						result &= builder.build();
					} else {
						// this is just a warning, and does not indicate a failure
						System.out.println("Warning: Tests source directory does not exist: " + testsSrc.getAbsolutePath());
					}
				}
			}
		}
		return result;
	}

	/* [PR 117967] idea 491: Automatically create the jars required for test bootpath */
	public boolean buildTestBootpath(boolean incremental, boolean noWarn, boolean useTestBootpathJavaDoc, BuilderExtension[] extensions, Logger logger, boolean callFromEclipse) {
		boolean result = true;
		int multipleSources = 0;
		String sourceRoot = this.registry.getSourceRoot();
		/* [PR 119039] Design 917: Multiple source folder support for automated tests */
		for (int i = 0; i < sources.size(); i++) {
			if (!sources.get(i).isSimpleCopy()) {
				String srcfolder = sources.get(i).getRelativeSrcPath();
				if (srcfolder.startsWith(sourceRoot)) {
					/* [PR 121491] Use correct build method and correct commandline input values for preprocessing jobs */
					File testsSrc = null;
					if (callFromEclipse) {
						srcfolder = srcfolder.substring(sourceRoot.length());
						testsSrc = new File(this.baseDir + this.registry.getTestsProject() + File.separator + srcfolder);
					} else {
						testsSrc = new File(this.baseDir + sources.get(i).getRelativeSrcPath());
					}
					if (testsSrc.exists()) {
						Builder builder = new Builder();
						builder.setConfiguration(this);
						builder.setIncremental(incremental);
						builder.setMetadata(metadata);
						/* [PR 116982] Preprocessor should not put INCLUDE-IF warnings on Automated Tests */
						builder.setNoWarnIncludeIf(true);
						/* [PR 121584]Add option to jcl-builder to use testsBootPath JavaDoc */
						builder.setIncludeIfUnsure(useTestBootpathJavaDoc);
						builder.setIsTestsBoot(true);

						for (BuilderExtension extension : extensions) {
							builder.addExtension(extension);
						}

						if (logger != null) {
							builder.setLogger(logger);
						} else if (noWarn) {
							builder.setLogger(new CommandlineNowarnLogger());
						} else {
							builder.setLogger(new CommandlineLogger());
						}

						// Allow the builder to be aware of other sources (if there are any).
						// In the current setup the builder only preprocesses one source at a time
						// so it's not aware of other sources. This becomes an issue when external
						// messages must be written out. This allows the ExternalMessagesExtension
						// to check if it should load the messages already in the output file.
						if (++multipleSources > 1) {
							builder.setMultipleSources(true);
						}

						builder.setSourceDir(testsSrc);
						if (callFromEclipse) {
							builder.setOutputDir(new File(testsTempBootOutputPath));
						}
						result &= builder.build();
					} else {
						// this is just a warning, and does not indicate a failure
						System.out.println("Warning: Tests source directory does not exist: " + testsSrc.getAbsolutePath());
					}
				}
			}
		}
		return result;
	}

	/**
	 * Returns a string representation of the config object with build info.
	 *
	 * @return      the string representation of the config object
	 *
	 * @see         java.lang.Object#toString()
	 */
	@Override
	public String toString() {
		return this.toString(true);
	}

	/**
	 * Returns a string representation of the config object with build info.
	 * Build info includes the sources for the configuration and the output
	 * directory.
	 *
	 * @param       hasBuildInfo        display build information
	 * @return      the string representation of the config object
	 *
	 * @see         java.lang.Object#toString()
	 */
	public String toString(boolean hasBuildInfo) {
		StringBuffer buffer = new StringBuffer("\nName: ");
		buffer.append(name);

		if (isConfiguration) {
			buffer.append(" (Configuration)");
			buffer.append("\nFlags: ");
			buffer.append(flagSet.toString());
			buffer.append("\nOptions: ");
			buffer.append(getOptions().toString());
			if (hasBuildInfo) {
				buffer.append("\nSources: ");
				buffer.append(sources.toString());
				buffer.append("\nOutput: ");
				buffer.append(getRelativeOutputPath());
			}
		} else {
			buffer.append(" (Set)");
			buffer.append("\nFlags: ");
			buffer.append(flagSet.toString());
		}
		return buffer.toString();
	}

	/**
	 * Indicates whether or not this ConfigObject is equal to a given object.  The only
	 * criteria used for comparison is the config name.
	 *
	 * @return      <code>true</code> if the config objects have the same name, <code>false</code> otherwise
	 *
	 * @see         java.lang.Object#equals(java.lang.Object)
	 */
	@Override
	public boolean equals(Object obj) {
		if (obj instanceof ConfigObject) {
			ConfigObject tempConfig = (ConfigObject) obj;

			if (name.equals(tempConfig.getName())) {
				if (registry == null || registry.equals(tempConfig.getRegistry())) {
					return true;
				}
			}
		}

		return false;
	}

	/**
	 * @see java.lang.Object#hashCode()
	 */
	@Override
	public int hashCode() {
		if (registry == null) {
			return name.hashCode();
		}

		return name.hashCode() + registry.hashCode();
	}

	/* [PR 120359] New classpath entry is needed for configurations */
	public void setXMLParserError(String error) {
		configXMLParserError += error;
	}

	public String getXMLParserError() {
		return configXMLParserError;
	}
}
