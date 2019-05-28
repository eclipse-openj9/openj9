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
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;

/**
 * J9 JCL Perprocessor builder.  The builder is responsible for all the tasks related to preprocessing
 * a group of files.  This includes managing the extensions, notifying others, verifying preprocess options,
 * handling preprocessor warnings, and actually preprocessing the sources.
 */
public class Builder {

	public static boolean isForced(Map<?, ?> options) {
		return Boolean.TRUE.equals(options.get("force"));
	}

	private Properties options = new Properties();
	private BuilderExtension[] extensions = new BuilderExtension[0];

	private Logger logger = new NullLogger();
	private ConfigurationRegistry registry;
	private ConfigObject configObject = null;
	private boolean isIncremental = false;
	private boolean enabledMetadata = false;

	private File sourceDir = null;

	/**
	 * The value is a String[] containing the relative paths of all of the build
	 * files for a given sourceDir.
	 */
	private final Map<File, String[]> buildFilesBySourceDir = new HashMap<>();
	/*[PR 118220] Incremental builder is not called when file is deleted in base library*/
	private final Map<File, List<String>> deleteFilesBySourceDir = new HashMap<>();
	private final Map<File, List<String>> buildResourcesBySourceDir = new HashMap<>();

	private int buildFileCount = 0;
	private int deleteFileCount = 0;
	private int builtFileCount = 0;
	private int buildResourcesCount = 0;
	private File outputDir = null;
	private boolean verdict = false;
	private boolean includeIfUnsure = false;
	/*[PR 117967] idea 491: Automatically create the jars required for test bootpath*/
	private boolean isTestsBootPath = false;
	private boolean noWarnIncludeIf = false;
	private boolean noWarnInvalidFlags = false;
	private boolean multipleSources = false;
	private boolean updateAllCopyrights = false;

	/**
	 * J9 JCL Preprocessor builder constructor.  Initializes the needed extensions.
	 */
	public Builder() {
		addExtension(new ExternalMessagesExtension());
		addExtension(new MacroExtension());
		addExtension(new JxeRulesExtension());
		addExtension(new EclipseMetadataExtension());
		addExtension(new JitAttributesExtension());
		addExtension(new TagExtension());
	}

	/**
	 * Sets the preprocess options.
	 *
	 * @param 		options		the preprocess options
	 */
	public void setOptions(Properties options) {
		if (options != null) {
			this.options.putAll(options);
		}
		this.options = options;
	}

	/**
	 * Returns the preprocess options for this builder.
	 *
	 * @return		the preprocess options
	 */
	public Properties getOptions() {
		return this.options;
	}

	/**
	 * Adds an extension to the builder.
	 *
	 * @param 		extension	the extension to add
	 */
	public void addExtension(BuilderExtension extension) {
		if (extension == null) {
			throw new NullPointerException();
		}

		BuilderExtension[] newExtensions = new BuilderExtension[extensions.length + 1];
		if (extensions.length > 0) {
			System.arraycopy(extensions, 0, newExtensions, 0, extensions.length);
		}
		newExtensions[newExtensions.length - 1] = extension;
		this.extensions = newExtensions;

		extension.setBuilder(this);
	}

	/**
	 * Returns the builder extensions/
	 *
	 * @return		the builder extensions
	 */
	public BuilderExtension[] getExtensions() {
		return extensions;
	}

	/**
	 * Returns the logger associated with this builder.
	 *
	 * @return		the logger
	 */
	public Logger getLogger() {
		return logger;
	}

	/**
	 * Sets this builder's logger.
	 *
	 * @param 		logger		the new logger
	 */
	public void setLogger(Logger logger) {
		this.logger = logger;
	}

	/**
	 * Sets whether the build is incremental or not.
	 *
	 * @param 		isIncremental	<code>true</code> if the build is incremental, <code>false</code> otherwise
	 */
	public void setIncremental(boolean isIncremental) {
		this.isIncremental = isIncremental;
	}

	/**
	 * Returns whether or not this builder will only do an incremental build.
	 *
	 * @return		<code>true</code> if the build is incremental, <code>false</code> otherwise
	 */
	public boolean isIncremental() {
		return this.isIncremental;
	}

	/**
	 * Sets whether or not preprocessor metadata will be generated.
	 *
	 * @param 		enabledMetadata		<code>true</code> if metadata is to be generated,
	 * 									<code>false</code> otherwise
	 */
	public void setMetadata(boolean enabledMetadata) {
		this.enabledMetadata = enabledMetadata;
	}

	/**
	 * Returns whether or not preprocessor metadata is enabled.
	 *
	 * @return		<code>true</code> if metadata will be written, <code>false</code> otherwise
	 */
	public boolean isMetadataEnabled() {
		return this.enabledMetadata;
	}

	/**
	 * Sets whether or not the preprocessor should include files that do not
	 * have a INCLUDE-IF tag.
	 *
	 * @param 		include		<code>true</code> if files with no INCLUDE-IF should
	 * 							be included, <code>false</code> otherwise
	 */
	public void setIncludeIfUnsure(boolean include) {
		this.includeIfUnsure = include;
	}

	/*[PR 117967] idea 491: Automatically create the jars required for test bootpath*/
	/**
	 * Sets whether or not the preprocessor is running to generate Tests Boot Path project
	 *
	 * @param 	isTestsBoot		<code>true</code> if preprocessor is running to generate Tests Boot Path project,
	 * 							<code>false</code> otherwise
	 */
	public void setIsTestsBoot(boolean isTestsBoot) {
		this.isTestsBootPath = isTestsBoot;
	}

	/*[PR 117967] idea 491: Automatically create the jars required for test bootpath*/
	/**
	 * Sets whether or not the preprocessor should give warningsor errors about the files that do not
	 * have a INCLUDE-IF tag.
	 *
	 * @param 		warning		<code>true</code> if files with no INCLUDE-IF should
	 * 							be marked with warning or error, <code>false</code> otherwise
	 */
	public void setNoWarnIncludeIf(boolean warning) {
		this.noWarnIncludeIf = warning;
	}

	/**
	 * Sets the configuration to preprocess.
	 *
	 * @param 		config		the configuration to preprocess
	 */
	public void setConfiguration(ConfigObject config) {
		if (config.isSet()) {
			System.err.println("Warning: Builder is using " + config + ", a set, not a configuration.");
		}
		this.configObject = config;
		this.registry = config.getRegistry();
		this.outputDir = config.getOutputDir();
	}

	/**
	 * Returns this builder's output directory.
	 *
	 * @return		the output directory
	 */
	public File getOutputDir() {
		return this.outputDir;
	}

	/**
	 * Sets this builder's output directory.
	 *
	 * @param 		outputDir	the new output directory
	 */
	public void setOutputDir(File outputDir) {
		if (outputDir == null) {
			throw new NullPointerException();
		}
		this.outputDir = outputDir;
	}

	/**
	 * Returns this builder's configuration source directories.
	 *
	 * @return		the config's source dirs
	 */
	public File getSourceDir() {
		return this.sourceDir;
	}

	/**
	 * Sets the proprocess job's source directory.
	 *
	 * @param 		sourceDir	the source directory to preprocess
	 */
	public void setSourceDir(File sourceDir) {
		if (sourceDir == null) {
			throw new NullPointerException();
		} else {
			this.sourceDir = sourceDir;
		}
	}

	/**
	 * Set builder aware of other sources (to be used by the ExternalMessagesExtension).
	 *
	 * @param 		multipleSources		<code>true</code> if there are other sources, <code>false</code> otherwise
	 */
	public void setMultipleSources(boolean multipleSources) {
		this.multipleSources = multipleSources;
	}

	/**
	 * Returns whether or not the configuration that setup this builder has multiple sources.
	 *
	 * @return		<code>true</code> if there are other sources, <code>false</code> otherwise
	 */
	public boolean hasMultipleSources() {
		return multipleSources;
	}

	/**
	 * Performs the build.
	 */
	public boolean build() {
		//create output dir even if no file is gonna be included in preprocess
		getOutputDir().mkdirs();
		if (validateOptions()) {
			computeBuildFiles();
			notifyBuildBegin();

			PreprocessorFactory factory = newPreprocessorFactory();
			boolean force = isForced(this.options);

			//Ignore folders that do not exist (warning thrown in computeBuildFiles()
			if (sourceDir != null) {
				File metadataDir = new File(outputDir.getParentFile(), "jppmd");
				String[] buildFiles = buildFilesBySourceDir.get(sourceDir);
				getLogger().log("\nPreprocessing " + sourceDir.getAbsolutePath(), 1);
				builtFileCount = 0;

				for (String buildFile : buildFiles) {
					File sourceFile = new File(sourceDir, buildFile);
					File outputFile = new File(outputDir, buildFile);
					File metadataFile = new File(metadataDir, buildFile + ".jppmd");

					notifyBuildFileBegin(sourceFile, outputFile, buildFile);

					try (OutputStream metadataOutput = new PhantomOutputStream(metadataFile);
						 OutputStream output = new PhantomOutputStream(outputFile, force)) {

						// configure the preprocessor and let extensions do the same
						JavaPreprocessor jpp;

						if (enabledMetadata) {
							jpp = factory.newPreprocessor(metadataOutput, sourceFile, output, outputFile);
						} else {
							jpp = factory.newPreprocessor(sourceFile, output);
						}

						Calendar cal = new GregorianCalendar();
						if (!updateAllCopyrights) {
							cal.setTime(new Date(sourceFile.lastModified()));
						}
						jpp.setCopyrightYear(cal.get(Calendar.YEAR));
						jpp.addValidFlags(registry.getValidFlags());
						/*[PR 120411] Use a javadoc tag instead of TestBootpath preprocessor tag*/
						jpp.setTestBootPath(isTestsBootPath);
						notifyConfigurePreprocessor(jpp);

						// preprocess
						boolean included = false;
						try {
							included = jpp.preprocess();
							if (included) {
								builtFileCount++;
							}
							handlePreprocessorWarnings(jpp, sourceFile);
						} catch (Throwable t) {
							handlePreprocessorException(t, sourceFile);
						}

						if (!included && outputFile.exists()) {
							outputFile.delete();
						}

						if (!included && metadataFile.exists()) {
							metadataFile.delete();
						}
					} catch (Throwable t) {
						getLogger().log("Exception occured in file " + sourceFile.getAbsolutePath() + ", preprocess failed.", 3, t);
						handleBuildException(t);
					} finally {
						notifyBuildFileEnd(sourceFile, outputFile, buildFile);
					}
				}

				logger.log(builtFileCount + " of " + buildFileCount + " file(s) included in preprocess", 1);

				/*[PR 118220] Incremental builder is not called when file is deleted in base library*/
				List<String> deleteFiles = deleteFilesBySourceDir.get(sourceDir);
				if (deleteFiles != null && deleteFiles.size() != 0) {
					int deletedFilesCount = 0;
					for (String file : deleteFiles) {
						File deleteFile = new File(outputDir, file);
						if (deleteFile.exists()) {
							deletedFilesCount++;
							deleteFile.delete();
						}
					}
					getLogger().log(deletedFilesCount + " of " + deleteFileCount
							+ " file(s) deleted in preprocess from " + outputDir.getAbsolutePath(), 1);
				}
			}
			/*[PR 119753] classes.txt and AutoRuns are not updated when new test class is added */
			List<String> buildResources = buildResourcesBySourceDir.get(sourceDir);
			if (buildResources != null && buildResources.size() != 0) {
				int copiedResourcesCount = 0;
				int deletedResorucesCount = 0;
				String outputpath;
				if (isTestsBootPath) {
					outputpath = configObject.getBootTestsOutputPath();
				} else {
					outputpath = configObject.getTestsOutputPath();
				}
				for (String file : buildResources) {
					File resource_out = new File(outputpath, file);
					File resource_src = new File(sourceDir, file);
					if (resource_src.exists()) {
						copyResource(resource_src, resource_out);
						copiedResourcesCount++;
					} else {
						resource_out.delete();
						deletedResorucesCount++;
					}
				}

				getLogger().log("Total Build Resource Count : " + buildResourcesCount, 1);
				getLogger().log("  - " + copiedResourcesCount + " resource" + (copiedResourcesCount > 1 ? "s are " : " is ") + "copied to " + outputpath, 1);
				getLogger().log("  - " + deletedResorucesCount + " resource" + (deletedResorucesCount > 1 ? "s are " : " is ") + "deleted from " + outputpath, 1);
			}

			notifyBuildEnd();
		}

		if (logger.getErrorCount() == 0) {
			if (verdict) {
				getLogger().log("PREPROCESS WAS SUCCESSFUL", 1);
			}
			return true;
		} else {
			if (verdict) {
				getLogger().log("PREPROCESS WAS NOT SUCCESSFUL", 1);
			}
			return false;
		}
	}

	/*[PR 119753] classes.txt and AutoRuns are not updated when new test class is added */
	public static void copyResource(File source, File destination) {
		destination.delete();

		try {
			SimpleCopy.copyFile(source, destination);
		} catch (IOException e) {
			System.err.println("ERROR - Could not copy the file to destination");
			System.err.println("   Source: " + source.toString());
			System.err.println("   Destination: " + destination.toString());
			e.printStackTrace();
		}
	}

	/**
	 * Validates the build options.
	 */
	private boolean validateOptions() {
		boolean isValid = true;

		if (configObject == null) {
			configObject = registry.getConfiguration(options.getProperty("config"));
		}
		this.options.putAll(configObject.getOptions());

		// check for the verdict option
		if (options.containsKey("verdict")) {
			this.verdict = true;
		}

		if (options.containsKey("includeifunsure")) {
			setIncludeIfUnsure(true);
		}
		if (options.containsKey("nowarnincludeif")) {
			setNoWarnIncludeIf(true);
		}

		if (options.containsKey("nowarninvalidflags")) {
			this.noWarnInvalidFlags = true;
		}

		if (options.containsKey("updateallcopyrights")) {
			this.updateAllCopyrights = true;
		}

		// call the method for all the extensions
		String extensionName = "";
		try {
			for (BuilderExtension extension : extensions) {
				extensionName = extension.getName();
				extension.validateOptions(this.options);
			}
		} catch (BuilderConfigurationException e) {
			logger.log("A configuration exception occured", Logger.SEVERITY_FATAL, e);
			isValid = false;
		} catch (Exception e) {
			StringBuffer buffer = new StringBuffer("An exception occured while invoking validateOptions() for the extension \"");
			buffer.append(extensionName);
			buffer.append("\"");
			logger.log(buffer.toString(), Logger.SEVERITY_ERROR, e);
		}
		return isValid;
	}

	/**
	 * Notifies the extensions that the build is beginning.
	 */
	private void notifyBuildBegin() {
		// call the method for all the extensions
		String extensionName = "";
		try {
			for (BuilderExtension extension : extensions) {
				extensionName = extension.getName();
				logger.setMessageSource(extensionName);
				extension.notifyBuildBegin();
				logger.setMessageSource(null);
			}
		} catch (Exception e) {
			StringBuffer buffer = new StringBuffer("An exception occured while invoking notifyBuildBegin() for the extension \"");
			buffer.append(extensionName);
			buffer.append("\"");
			logger.log(buffer.toString(), Logger.SEVERITY_ERROR, e);
		}
	}

	/**
	 * Notifies the extensions that the build is ending.
	 */
	private void notifyBuildEnd() {
		// call the method for all the extensions
		String extensionName = "";
		try {
			for (BuilderExtension extension : extensions) {
				extensionName = extension.getName();
				logger.setMessageSource(extensionName);
				extension.notifyBuildEnd();
				logger.setMessageSource(null);
			}
		} catch (Exception e) {
			StringBuffer buffer = new StringBuffer("An exception occured while invoking notifyBuildEnd() for the extension \"");
			buffer.append(extensionName);
			buffer.append("\"");
			logger.log(buffer.toString(), Logger.SEVERITY_ERROR, e);
		}
	}

	/**
	 * Notifies the extensions that the build is beginning on the specified
	 * file.
	 */
	private void notifyBuildFileBegin(File sourceFile, File outputFile, String relativePath) {
		// call the method for all the extensions
		String extensionName = "";
		try {
			for (BuilderExtension extension : extensions) {
				extensionName = extension.getName();
				logger.setMessageSource(extensionName);
				extension.notifyBuildFileBegin(sourceFile, outputFile, relativePath);
				logger.setMessageSource(null);
			}
		} catch (Exception e) {
			StringBuffer buffer = new StringBuffer("An exception occured while invoking notifyBuildFileBegin() for the extension \"");
			buffer.append(extensionName);
			buffer.append("\"");
			logger.log(buffer.toString(), Logger.SEVERITY_ERROR, e);
		}
	}

	/**
	 * Notifies the extensions that the build is ending on the specified file.
	 */
	private void notifyBuildFileEnd(File sourceFile, File outputFile, String relativePath) {
		// call the method for all the extensions
		String extensionName = "";
		try {
			for (BuilderExtension extension : extensions) {
				extensionName = extension.getName();
				logger.setMessageSource(extensionName);
				extension.notifyBuildFileEnd(sourceFile, outputFile, relativePath);
				logger.setMessageSource(null);
			}
		} catch (Exception e) {
			StringBuffer buffer = new StringBuffer("An exception occured while invoking notifyBuildFileEnd() for the extension \"");
			buffer.append(extensionName);
			buffer.append("\"");
			logger.log(buffer.toString(), Logger.SEVERITY_ERROR, e);
		}
	}

	/**
	 * Notifies the extensions that they should configure the preprocessor.
	 */
	private void notifyConfigurePreprocessor(JavaPreprocessor preprocessor) {
		preprocessor.setIncludeIfUnsure(this.includeIfUnsure);
		preprocessor.setNoWarnIncludeIf(this.noWarnIncludeIf);

		// call the method for all the extensions
		String extensionName = "";
		try {
			for (BuilderExtension extension : extensions) {
				extensionName = extension.getName();
				logger.setMessageSource(extensionName);
				extension.notifyConfigurePreprocessor(preprocessor);
				logger.setMessageSource(null);
			}
		} catch (Exception e) {
			StringBuffer buffer = new StringBuffer("An exception occured while invoking notifyConfigurePreprocessor() for the extension \"");
			buffer.append(extensionName);
			buffer.append("\"");
			logger.log(buffer.toString(), Logger.SEVERITY_ERROR, e);
		}
	}

	/**
	 * Handles exceptions thrown while building.
	 */
	private void handleBuildException(Throwable t) {
		if (t instanceof Error) {
			logger.log("An error occured while building", Logger.SEVERITY_FATAL, t);
			throw (Error) t;
		} else {
			logger.log("An exception occured while building", Logger.SEVERITY_ERROR, t);
		}
	}

	/**
	 * Handles exceptions thrown by the preprocessor.
	 */
	private void handlePreprocessorException(Throwable t, File sourceFile) {
		if (t instanceof Error) {
			logger.log("An error occured while invoking the preprocessor", "preprocessor", Logger.SEVERITY_FATAL, sourceFile, t);
			throw (Error) t;
		} else {
			logger.log("An exception occured while invoking the preprocessor", "preprocessor", Logger.SEVERITY_ERROR, sourceFile, t);
		}
	}

	/**
	 * Handles warnings generated by the preprocessor.
	 */
	private void handlePreprocessorWarnings(JavaPreprocessor jpp, File sourceFile) {
		if (jpp.hasWarnings()) {
			for (PreprocessorWarning warning : jpp.getWarnings()) {
				int severity = warning.shouldFail() ? Logger.SEVERITY_ERROR : Logger.SEVERITY_WARNING;
				/*[PR 117967] idea 491: Automatically create the jars required for test bootpath*/
				if (warning.getMessage().startsWith("No INCLUDE-IF") && sourceFile.getAbsolutePath().endsWith(".java") && !includeIfUnsure && !isTestsBootPath) {
					severity = Logger.SEVERITY_ERROR;
				}

				if (warning.getMessage().startsWith("Ignoring copyright")) {
					severity = Logger.SEVERITY_INFO;
				}

				logger.log(warning.getMessage(), "preprocessor", severity, sourceFile, warning.getLine(), warning.getCharstart(), warning.getCharend());
			}
		}

		if (!noWarnInvalidFlags) {
			for (PreprocessorWarning warning : jpp.getInvalidFlags()) {
				logger.log(warning.getMessage(), "preprocessor", Logger.SEVERITY_ERROR, sourceFile, warning.getLine(), warning.getCharstart(), warning.getCharend());
			}
		}
	}

	/**
	 * Determines whether the specified source file should be built.
	 */
	private boolean shouldBuild(File sourceFile, File outputFile, String relativePath) {
		// call the method for all the extensions
		for (BuilderExtension extension : extensions) {
			logger.setMessageSource(extension.getName());
			boolean shouldBuild = extension.shouldBuild(sourceFile, outputFile, relativePath);
			logger.setMessageSource(null);
			if (!shouldBuild) {
				return false;
			}
		}

		return true;
	}

	/*[PR 118220] Incremental builder is not called when file is deleted in base library*/
	/**
	 * Returns the deleted Files
	 */
	/*[PR 119753] classes.txt and AutoRuns are not updated when new test class is added */
	private List<String> getDeletedFiles(File sourceDir) {
		// call the method for all the extensions
		for (BuilderExtension extension : extensions) {
			logger.setMessageSource(extension.getName());
			List<String> elements = extension.getDeleteFiles(sourceDir);
			logger.setMessageSource(null);
			if (elements != null) {
				return elements;
			}
		}

		return null;
	}

	/*[PR 119753] classes.txt and AutoRuns are not updated when new test class is added */
	private List<String> getBuildResources(File sourceDir) {
		// call the method for all the extensions
		for (BuilderExtension extension : extensions) {
			logger.setMessageSource(extension.getName());
			List<String> elements = extension.getBuildResources(sourceDir);
			logger.setMessageSource(null);
			if (elements != null) {
				return elements;
			}
		}
		return null;
	}

	/**
	 * Creates a new PreprocessorFactory object.
	 */
	private PreprocessorFactory newPreprocessorFactory() {
		PreprocessorFactory factory = new PreprocessorFactory();
		/*[PR 117967] idea 491: Automatically create the jars required for test bootpath*/
		factory.setFlags(this.configObject.getFlagsAsArray());
		factory.setRequiredIncludeFlags(this.configObject.getRequiredIncludeFlagSet());
		return factory;
	}

	/**
	 * Recursively searches the given root directory to find all files. The file
	 * paths are returned, relative to the root directory.
	 */
	private List<String> getFiles(File rootDirectory) {
		List<String> fileList = new ArrayList<>();
		File[] files = rootDirectory.listFiles();

		if (files == null) {
			StringBuffer msg = new StringBuffer("Error reading the source directory \"");
			msg.append(rootDirectory.getAbsolutePath());
			msg.append("\" - No Files copied");
			getLogger().log(msg.toString(), 2);
			verdict = false;
		} else {
			getFiles(files, "", fileList);
		}

		return fileList;
	}

	/**
	 * This is a helper function to getFiles(File);
	 */
	private static void getFiles(File[] files, String relativePath, List<String> fileList) {
		for (File file : files) {
			if (file.isFile()) {
				fileList.add(relativePath + file.getName());
			} else {
				String childRelativePath = relativePath + file.getName() + File.separator;
				getFiles(file.listFiles(), childRelativePath, fileList);
			}
		}
	}

	private void computeBuildFiles() {
		if (sourceDir.exists()) {
			List<String> allFiles = getFiles(sourceDir);
			List<String> buildFiles = new ArrayList<>(allFiles.size());
			for (int j = 0; j < allFiles.size(); j++) {
				String currentFile = allFiles.get(j).toString();
				if (shouldBuild(sourceDir, outputDir, currentFile)) {
					buildFiles.add(currentFile);
				}
			}

			String[] buildFilesArray = buildFiles.toArray(new String[buildFiles.size()]);
			buildFilesBySourceDir.put(sourceDir, buildFilesArray);
			buildFileCount += buildFilesArray.length;
			/*[PR 118220] Incremental builder is not called when file is deleted in base library*/
			/*[PR 119753] classes.txt and AutoRuns are not updated when new test class is added */
			List<String> deleteFiles = getDeletedFiles(sourceDir);
			if (deleteFiles != null && deleteFiles.size() != 0) {
				deleteFileCount = deleteFiles.size();
				deleteFilesBySourceDir.put(sourceDir, deleteFiles);
			}

			List<String> buildResources = getBuildResources(sourceDir);
			if (buildResources != null && buildResources.size() != 0) {
				buildResourcesCount = buildResources.size();
				buildResourcesBySourceDir.put(sourceDir, buildResources);
			}
		} else {
			logger.log("Error: Source directory does not exist: " + sourceDir.getAbsolutePath(), Logger.SEVERITY_ERROR, new NullPointerException());
			sourceDir = null;
		}
	}

	/**
	 * Returns the number of files preprocessed.
	 *
	 * @return		the number of files preprocessed
	 */
	public int getBuildFileCount() {
		return buildFileCount;
	}

}
