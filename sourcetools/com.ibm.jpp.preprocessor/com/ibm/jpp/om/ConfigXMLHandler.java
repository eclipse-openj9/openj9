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
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.StringTokenizer;

import com.ibm.jpp.xml.IXMLDocumentHandler;
import com.ibm.jpp.xml.XMLException;
import com.ibm.jpp.xml.XMLParser;

/**
 * <b>HOW TO USE XML TO SETUP CONFIGURATION AND PREPROCESS JOBS </b>
 * <p>
 * XMl files are used two different ways in the preprocessor
 *
 * <ol>
 * <li>The default set of configurations is loaded when the preprocessing
 * begins, loading in a number of configurations/sets from default.xml which
 * must be in the plugin dir (if using the plugin) or the same location as the
 * preprocess code is located. (Use the command line argument "-xmldefault
 * {xmlfilepath}" to define a different default filepath</li>
 * <li>A second xml file can be defined using the command line argument (-xml
 * {xmlfilepath} {basedirectory}). With this command, the given xml file is
 * parsed for not only configuration setup, but relative source and output paths
 * for the build. The only other command line arguments currently allowed in
 * combination with -xml is -xmldefault. All build information
 * must be found inside the xml. More than one configuration to be built can be
 * defined in the xml and configurations missing source or output dirs will be
 * ignored.</li>
 * </ol>
 *
 * <p>For more details of the command structure see CommandLineBuilder.java
 * <p>
 * <b>FUNCTIONALITY IN DEFAULT XML FILES FOR LOADING CONFIGURATIONS </b>
 * <p>
 * See the default.xml or the plugin's plugin.xml for examples
 * <ol>
 * <li>"set" and "configuration" elements must define the attribute label.
 * </li>
 * <li>Only configurations can have source/output elements</li>
 * <li>A non-unique set/config label may result in dependency problems (-xml
 * will override -xmldefault, but try to not have anything dependent on that
 * set/config)</li>
 * <li>flags attribute used to add individual flags, dependencies attr used to
 * add all of the flags in that set/config (therefore the dependency set/config
 * must be defined in -xml or -defaultxml</li>
 * <li>Precede the flag/dependency name with '-' to remove, rather than add
 * that flag/dependency from the final flagset, trying it remove flags that
 * don't exist in the flagset will not cause an error.</li>
 * <li>Options that apply to the entire set/config are added as a separate
 * element called parameter, with attributes name and value.</li>
 * <li>Other elements, such as extension point will be ignored</li>
 * <li>Use the output path attribute of configuration to set the output path,
 * note that this path will be combined with the {basedir} arg</li>
 * <li>For each source, create a separate source element, with attribute path
 * to define the source location. Add "type=simplecopy" to do a copy instead of
 * a preprocess, add an element called parameter with attributes name and value
 * to add an option to a specific source dir</li>
 * </ol>
 *
 * <pre>
 * EX:
 *   &lt;set
 *         label="xtr"
 *         flags="xJava,xIO,xThread,xNet,Deprecated,MAX13"&gt;
 *   &lt;/set&gt;
 *
 * EX:
 *   &lt;configuration
 *         label="XTREME"
 *         outputpath="pConfig XTREME/src"
 *         dependencies="xtr"&gt;
 *       &lt;source path="src"&gt;
 *           &lt;parameter name="macro:define" value="com.ibm.oti.vm.library.version=29;com.ibm.oti.jcl.build=plugin2"/&gt;
 *       &lt;/source&gt;
 *       &lt;parameter name="jxerules:outputdir" value="com/ibm/oti/util"/&gt;
 *   &lt;/configuration&gt;
 * </pre>
 */
public class ConfigXMLHandler implements IXMLDocumentHandler {

	private static final int MAJOR_VERSION = 3;

	private final List<ConfigObject> configObjects;
	private final List<ConfigObject> requiredObjects;
	private ConfigObject currentConfig;
	private final FileInputStream XMLInput;

	private int configVersion;
	private String baseDir;
	private String srcRoot;
	private Src currentSource;
	private boolean inSource = false;
	private boolean inGlobals = false;
	private boolean inAutomatedTests = false;
	private String testsProject = "";
	private final List<ClassPathEntry> testsClassPaths;
	private final List<Src> testsSources;
	private final List<ClassPathEntry> classPaths;
	private final Map<String, String> defaultTestsResources;
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	private String outputPathPrefix = "";
	private String outputPathSuffix = "";
	private String testsOutputPathPrefix = "";
	private String testsOutputPathSuffix = " Tests";
	private String bootTestsOutputPathPrefix = "";
	private String bootTestsOutputPathSuffix = " Tests BootPath";

	/**
	 * Constructs a ConfigXMLHandler...
	 *
	 * @param       filename    the file to be parsed
	 *
	 * @throws      FileNotFoundException if the file cannot be located
	 */
	public ConfigXMLHandler(String filename) throws FileNotFoundException {
		this.XMLInput = new FileInputStream(filename);
		srcRoot = "";
		configObjects = new ArrayList<>();
		requiredObjects = new ArrayList<>();
		testsClassPaths = new ArrayList<>();
		classPaths = new ArrayList<>();
		testsSources = new ArrayList<>();
		defaultTestsResources = new HashMap<>();
	}

	public int getConfigVersion() {
		return configVersion;
	}

	/**
	 * @param       baseDir     the base directory
	 */
	public void setBaseDir(String baseDir) {
		this.baseDir = baseDir;
	}

	/**
	 * @param       srcRoot     the source root path
	 */
	public void setSourceRoot(String srcRoot) {
		this.srcRoot = srcRoot;
	}

	/**
	 * Returns only the configurations from the required XMLs.  This is the opposite of
	 * {@link #getLocalConfigs()}.
	 *
	 * @return      only the configurations from required XMLs
	 */
	public List<ConfigObject> getRequiredConfigs() {
		return requiredObjects;
	}

	/**
	 * Returns only the configurations defined in the parent XML (and not in
	 * the requires XMLs).
	 *
	 * @return      only the configurations defined in the parent XML
	 */
	public List<ConfigObject> getLocalConfigs() {
		return configObjects;
	}

	/**
	 * Returns all of the configurations.  This combines local and requires configs.
	 *
	 * @return      all of the configurations
	 *
	 * @see         #getLocalConfigs()
	 * @see         #getRequiredConfigs()
	 */
	public List<ConfigObject> getAllConfigs() {
		List<ConfigObject> temp = new ArrayList<>(requiredObjects);
		temp.addAll(configObjects);
		return temp;
	}

	/**
	 * Returns the name of the automated tests project.
	 *
	 * @return      the name of the automated tests project
	 */
	public String getTestsProject() {
		return testsProject;
	}

	/**
	 * Returns outputPathPrefix. Default value is "".
	 *
	 * @return      outputPathPrefix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getOutputPathPrefix() {
		return outputPathPrefix;
	}

	/**
	 * Returns outputPathSuffix. Default value is "".
	 *
	 * @return      outputPathSuffix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getOutputPathSuffix() {
		return outputPathSuffix;
	}

	/**
	 * Returns testsOutputPathPrefix. Default value is "".
	 *
	 * @return      testsOutputPathPrefix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getTestsOutputPathPrefix() {
		return testsOutputPathPrefix;
	}

	/**
	 * Returns testsOutputPathSuffix. Default value is "".
	 *
	 * @return      testsOutputPathSuffix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getTestsOutputPathSuffix() {
		return testsOutputPathSuffix;
	}

	/**
	 * Returns bootTestsOutputPathPrefix. Default value is "".
	 *
	 * @return      bootTestsOutputPathPrefix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getBootTestsOutputPathPrefix() {
		return bootTestsOutputPathPrefix;
	}

	/**
	 * Returns bootTestsOutputPathSuffix. Default value is "".
	 *
	 * @return      bootTestsOutputPathSuffix
	 */
	/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
	public String getBootTestsOutputPathSuffix() {
		return bootTestsOutputPathSuffix;
	}

	/**
	 * Returns the automated tests classpath entries.
	 *
	 * @return      the automated tests classpath entries
	 */
	public List<ClassPathEntry> getTestsClassPaths() {
		return testsClassPaths;
	}

	/**
 *   * Returns the automated tests source entries.
 *       *
 *           * @return      the automated tests source entries
 *               */
	public List<Src> getTestsSources() {
		return testsSources;
	}

	/**
	 * Parses the configuration XML file.
	 *
	 * @throws      XMLException    if there are errors in the XML file
	 */
	public void parseInput() throws XMLException {
		System.out.println("Reading preprocess instructions from xml...");
		new XMLParser().parse(XMLInput, this);
	}

	/**
	 * Insures that the base directory is not null.
	 */
	@Override
	public void xmlStartDocument() {
		if (baseDir == null) {
			baseDir = "";
		}
	}

	/**
	 * Resolves any configuration dependencies of the parsed configuration. One
	 * configuration may be dependant upon other configurations meaning that and
	 * preprocessor flags held by the by the depended upon configurations are
	 * inherited.
	 */
	@Override
	public void xmlEndDocument() {
		try {
			XMLInput.close();
		} catch (IOException e) {
			System.err.println("Could not close ConfigXMLHandler InputStream");
			e.printStackTrace();
		}
	}

	/**
	 * A series of if statements to identify the significance of the parsed
	 * element and act accordingly.
	 *
	 * @param       elementName     the XML element name
	 * @param       attributes      the element's attributes
	 *
	 * @throws      XMLException    if there are errors in the XML file
	 */
	@Override
	public void xmlStartElement(String elementName, Map<String, String> attributes) throws XMLException {
		if (elementName.equals("configurationreg")) {
			String version = attributes.get("version");
			if (attributes.get("version") == null) {
				throw new XMLException("No JPP Configuration XML version defined");
			} else {
				int majorVersion = Integer.parseInt(version.substring(0, 1));
				if (majorVersion > MAJOR_VERSION) {
					throw new XMLException("Perprocessor can't handle JPP Configuration XML v" + majorVersion + ".x");
				}
				configVersion = majorVersion;
			}
		}

		if (elementName.equals("automatedtests")) {
			if (attributes.get("project") != null) {
				inAutomatedTests = true;
				testsProject = attributes.get("project");
			}
		} else if (elementName.equals("require") && attributes.get("name") != null) {
			try {
				ConfigurationRegistry requiredRegistry = MetaRegistry.getRegistry(baseDir, attributes.get("name").toString(), ConfigurationRegistry.DEFAULT_XML);
				requiredObjects.addAll(requiredRegistry.getConfigurationsAsCollection());
			} catch (FileNotFoundException e) {
				System.out.println("Could not find the XML configuration file: " + ConfigurationRegistry.DEFAULT_XML + "\n");
			}
		} else if (elementName.equals("set")) {
			// Sets are groups of flags that are typically not used in a
			// preprocessing job but insted act as dependencies for defined
			// configurations.

			currentConfig = new ConfigObject(attributes.get("label"), false);
			if (attributes.get("flags") != null) {
				StringTokenizer t = new StringTokenizer(attributes.get("flags"), ",");

				while (t.hasMoreTokens()) {
					String currentString = t.nextToken().trim();
					/*
					 * You can specify that you wish a flag removed from the configuration or set with a preceding '-' character.
					 * For example if the final flag set (after dependency resolution) was: A, B, C and -A the '-A' would signify
					 * the removal of flag 'A' and only flags B and C would remain.
					 */
					if (currentString.charAt(0) == '-') {
						currentConfig.addFlagToRemoveFlagSet(currentString.substring(1));
					} else if (currentString.charAt(0) == '*') {
						/*
						 * The '*' symbol prefixing any added flags is used when a required flag is needed. If a required flag is
						 * added to a configuration then for a java file to be accepted in preprocessing it must include at least
						 * the required flag and possibly others.
						 */
						currentConfig.addRequiredIncludeFlag(currentString.substring(1));
						currentConfig.addFlag(currentString.substring(1));
					} else {
						currentConfig.addFlag(currentString);
					}
				}
			}

			/*
			 * Dependencies are other configurations or that can be incorporated into the current configuration (or set). For
			 * example if there existed a configuration called 'depX' with the flags Y and Z and the configuration 'configA'
			 * included flag A and the dependency on 'depX' then the final flag set of configA would be A, Y and Z.
			 */
			if (attributes.get("dependencies") != null) {
				StringTokenizer t = new StringTokenizer(attributes.get("dependencies").toString(), ",");

				while (t.hasMoreTokens()) {
					String currentString = t.nextToken().trim();

					// Dependencies can be specified for removal just as flags
					if (currentString.charAt(0) == '-') {
						currentConfig.removeFlagDependency(currentString.substring(1));
					} else {
						currentConfig.addFlagDependency(currentString);
					}
				}
			}

			configObjects.add(currentConfig);
		} else if (elementName.equals("configuration")) {
			if (attributes.get("name") != null) {
				currentConfig = new ConfigObject(attributes.get("name").toString(), baseDir, true);
			} else {
				currentConfig = new ConfigObject(attributes.get("label").toString(), baseDir, true);
				if (attributes.get("flags") != null) {
					StringTokenizer t = new StringTokenizer(attributes.get("flags").toString(), ",");
					while (t.hasMoreTokens()) {
						String currentString = t.nextToken().trim();
						if (currentString.charAt(0) == '-') {
							currentConfig.addFlagToRemoveFlagSet(currentString.substring(1));
						} else if (currentString.charAt(0) == '*') {
							currentConfig.addRequiredIncludeFlag(currentString.substring(1));
							currentConfig.addFlag(currentString.substring(1));
						} else {
							currentConfig.addFlag(currentString);
						}
					}
				}
				if (attributes.get("dependencies") != null) {
					StringTokenizer t = new StringTokenizer(attributes.get("dependencies").toString(), ",");
					while (t.hasMoreTokens()) {
						String currentString = t.nextToken().trim();
						if (currentString.charAt(0) == '-') {
							currentConfig.removeFlagDependency(currentString.substring(1));
						} else {
							currentConfig.addFlagDependency(currentString);
						}
					}
				}
				/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
				// add global suffixes and prefixes
				currentConfig.setOutputPathPrefix(outputPathPrefix);
				currentConfig.setOutputPathSuffix(outputPathSuffix);
				currentConfig.setTestsOutputPathPrefix(testsOutputPathPrefix);
				currentConfig.setTestsOutputPathSuffix(testsOutputPathSuffix);
				currentConfig.setBootTestsOutputPathPrefix(bootTestsOutputPathPrefix);
				currentConfig.setBootTestsOutputPathSuffix(bootTestsOutputPathSuffix);
				currentConfig.setDefaultTestsResourcesPrefixes(defaultTestsResources);
			}

			if (attributes.get("jdkcompliance") != null) {
				currentConfig.setJDKCompliance(attributes.get("jdkcompliance").toString());
			}

			if (attributes.get("outputpath") != null) {
				currentConfig.setOutputPathKeyword(attributes.get("outputpath").toString());
				currentConfig.setOutputPath(attributes.get("outputpath").toString());
				currentConfig.setTestsOutputPaths();
				currentConfig.setBootTestsOutputPaths();
			}

			if (attributes.get("jppmetadata") != null) {
				currentConfig.enableMetadata(attributes.get("jppmetadata").equals("true"));
			}

			configObjects.add(currentConfig);
		} else if (elementName.equals("source")) {
			String path = attributes.get("path").toString();
			if (!inAutomatedTests) {
				path = (path.startsWith("/")) ? path : srcRoot + path;
			}

			if (attributes.get("type") != null && attributes.get("outputpath") != null) {
				currentSource = new Src(path, attributes.get("outputpath").toString());
			} else if (attributes.get("type") != null) {
				currentSource = new Src(path, attributes.get("type").toString().equals("simplecopy"));
			} else {
				currentSource = new Src(path);
			}

			inSource = true;
		} else if (elementName.equals("parameter")) {
			if (inSource) {
				currentSource.addOption(attributes.get("name").toString(), attributes.get("value").toString());
				/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
			} else if (inGlobals) {
				if (attributes.get("name").toString().equals("outputPathPrefix")) {
					outputPathPrefix = attributes.get("value").toString();
				} else if (attributes.get("name").toString().equals("outputPathSuffix")) {
					outputPathSuffix = attributes.get("value").toString();
				} else if (attributes.get("name").toString().equals("testsOutputPathPrefix")) {
					testsOutputPathPrefix = attributes.get("value").toString();
				} else if (attributes.get("name").toString().equals("testsOutputPathSuffix")) {
					testsOutputPathSuffix = attributes.get("value").toString();
				} else if (attributes.get("name").toString().equals("bootTestsOutputPathPrefix")) {
					bootTestsOutputPathPrefix = attributes.get("value").toString();
				} else if (attributes.get("name").toString().equals("bootTestsOutputPathSuffix")) {
					bootTestsOutputPathSuffix = attributes.get("value").toString();
				} else {
					System.out.println(attributes.get("name").toString() + " is not valid global variable.");
				}
			} else {
				/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
				// if prefixes and suffixes are defined in configuration, then overwrite global values.
				if (attributes.get("name").toString().equals(outputPathPrefix)) {
					currentConfig.setOutputPathPrefix(outputPathPrefix);
				} else if (attributes.get("name").toString().equals(outputPathSuffix)) {
					currentConfig.setOutputPathSuffix(outputPathSuffix);
				} else if (attributes.get("name").toString().equals(testsOutputPathPrefix)) {
					currentConfig.setTestsOutputPathPrefix(testsOutputPathPrefix);
				} else if (attributes.get("name").toString().equals(testsOutputPathSuffix)) {
					currentConfig.setTestsOutputPathSuffix(testsOutputPathSuffix);
				} else if (attributes.get("name").toString().equals(bootTestsOutputPathPrefix)) {
					currentConfig.setBootTestsOutputPathPrefix(bootTestsOutputPathPrefix);
				} else if (attributes.get("name").toString().equals(bootTestsOutputPathSuffix)) {
					currentConfig.setBootTestsOutputPathSuffix(bootTestsOutputPathSuffix);
				} else {
					currentConfig.addOption(attributes.get("name").toString(), attributes.get("value").toString());
				}
			}
			/* [PR 118829] Desing 894: Core.JCL : Support for compiler options in preprocessor plugin */
		} else if (elementName.equals("coption")) {
			currentConfig.addCompilerOption(attributes.get("name").toString(), attributes.get("value").toString());
		} else if (elementName.equals("classpathentry")) {
			boolean exported = (attributes.get("exported") != null) && Boolean.parseBoolean(attributes.get("exported"));
			/* [PR 120359] New classpath entry is needed for configurations */
			if (attributes.get("kind") != null) {
				if (attributes.get("sourcepath") != null) {
					classPaths.add(new ClassPathEntry(attributes.get("path").toString(), attributes.get("kind").toString(), attributes.get("sourcepath").toString(), exported));
				} else {
					classPaths.add(new ClassPathEntry(attributes.get("path").toString(), attributes.get("kind").toString(), exported));
				}
			} else {
				String registry = "this";
				if (attributes.containsKey("registry")) {
					registry = attributes.get("registry").toString();
				}
				String configName = attributes.get("configName").toString();
				String project = attributes.get("project").toString();
				ConfigObject foundConfig = null;
				String path = null;
				if (registry.equals("this") || registry.equals(srcRoot.substring(0, srcRoot.length() - 1))) {
					for (ConfigObject config : configObjects) {
						if (config.isConfiguration() && config.getName().equals(configName)) {
							foundConfig = config;
							break;
						}
					}
				} else {
					try {
						// the following two lines are risky in some cases,
						// for example, if current registry needs an config from another registry and another registry needs a
						// config from current registry,
						// this may cause a loop and you might see overflow exception on the window,
						// This part might be removed or can have better solution,
						// Be aware that we dont point to another registry for now, and really low possibility in the future to
						// point a registry from an other registry.
						ConfigurationRegistry configReg = MetaRegistry.getRegistry(baseDir, registry, ConfigurationRegistry.DEFAULT_XML);
						foundConfig = configReg.getConfiguration(configName);
					} catch (FileNotFoundException e) {
						if (currentConfig != null) {
							currentConfig.setXMLParserError("XML PARSER ERROR : Config : " + currentConfig.getName()
									+ ", Element : \"classpathentry\", Reason : registry could not be found : " + baseDir
									+ (registry.equals("this") ? srcRoot : (registry + File.separator)) + ConfigurationRegistry.DEFAULT_XML + "\n");
						}
					}
				}
				if (foundConfig != null) {
					if (project.equals("config")) {
						path = "/" + foundConfig.getOutputPathPrefix() + foundConfig.getOutputPathKeyword() + foundConfig.getOutputPathSuffix();
					} else if (project.equals("tests")) {
						path = "/" + foundConfig.getTestsOutputPathPrefix() + foundConfig.getOutputPathKeyword() + foundConfig.getTestsOutputPathSuffix();
					} else if (project.equals("boottests")) {
						path = "/" + foundConfig.getBootTestsOutputPathPrefix() + foundConfig.getOutputPathKeyword() + foundConfig.getBootTestsOutputPathSuffix();
					} else {
						if (currentConfig != null) {
							currentConfig.setXMLParserError("XML PARSER ERROR : Attribute \"project\" can have the value \"config\", \"tests\" or \"boottests\"\n");
						}
					}
					classPaths.add(new ClassPathEntry(path, "src", exported));
				} else {
					if (currentConfig != null) {
						currentConfig.setXMLParserError("XML PARSER ERROR : Config : " + currentConfig.getName()
								+ ", Element : \"classpathentry\", Reason : pointed config in classpathentry element could not be found. Pointed Config Name : " + configName
								+ "\n");
					}
				}
			}
		} else if (elementName.equals("dependjob")) {
			String dependJobName = attributes.get("config").toString();
			String dependJobRegistry = attributes.get("registry").toString();
			ConfigurationRegistry dependRegistry = null;

			try {
				dependRegistry = MetaRegistry.getRegistry(baseDir, dependJobRegistry, ConfigurationRegistry.DEFAULT_XML);
			} catch (FileNotFoundException e) {
				System.out.println("Could not find the XML configuration file: /" + dependJobRegistry + "/" + ConfigurationRegistry.DEFAULT_XML + "\n");
			} finally {
				ConfigObject dependJob = (dependRegistry != null) ? dependRegistry.getConfiguration(dependJobName) : null;
				currentConfig.addDependJob(dependJobRegistry + " - " + dependJobName, dependJob);
			}
		} else if (elementName.equals("globalParameters")) {
			inGlobals = true;
			/* [PR 121295] Support for resources' prefixes and directories */
		} else if (elementName.equals("testsResources")) {
			if (attributes.containsKey("defaults")) {
				String defaults = attributes.get("defaults");
				if (defaults.equals("true") && !inAutomatedTests) {
					currentConfig.setUseTestResourcesDefaults(true);
				}
			} else {
				String dir = attributes.get("directory");
				String prefix = attributes.get("prefix");

				if (inAutomatedTests) {
					String dirSlash = dir + "/";

					for (Iterator<String> iter = defaultTestsResources.keySet().iterator(); iter.hasNext();) {
						String nextDir = iter.next();

						if (nextDir.startsWith(dirSlash) || nextDir.equals(dir)) {
							iter.remove();
						}
					}
					defaultTestsResources.put(dir, prefix);
				} else {
					currentConfig.setTestsResourcesPrefix(dir, prefix);
				}
			}
		}
	}

	/**
	 * Defines what action need be taken at the end of the certain elements.
	 *
	 * @param       elementName     the XML element name
	 */
	@Override
	public void xmlEndElement(String elementName) {
		if (elementName.equals("automatedtests")) {
			testsClassPaths.addAll(classPaths);
			classPaths.clear();
			inAutomatedTests = false;
		} else if (elementName.equals("configuration") || elementName.equals("set")) {
			for (ClassPathEntry classPath : classPaths) {
				currentConfig.addClassPathEntry(classPath);
			}
			classPaths.clear();
			currentConfig = null;
		} else if (elementName.equals("source")) {
			if (inAutomatedTests) {
				testsSources.add(currentSource);
			} else {
				currentConfig.addSource(currentSource);
			}
			currentSource = null;
			inSource = false;
			/* [PR 119756] config project prefix and postfix support in jpp_configuration.xml file */
		} else if (elementName.equals("globalParameters")) {
			inGlobals = false;
		}
	}

	/**
	 * Unused section of the interface
	 *
	 * @param       chars
	 */
	@Override
	public void xmlCharacters(String chars) {
		// System.out.println("xmlChars: " + chars);
	}

}
