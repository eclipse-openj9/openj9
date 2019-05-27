/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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
package com.ibm.j9tools.om;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

import com.ibm.j9tools.om.io.BuildInfoParser;
import com.ibm.j9tools.om.io.BuildSpecParser;
import com.ibm.j9tools.om.io.FeatureParser;
import com.ibm.j9tools.om.io.FlagDefinitionParser;

/**
 * The ObjectFactory provides a convenient way of turning files into object model classes.
 *
 * @author	Gabriel Castro
 * @author	Han Xu
 */
public class ObjectFactory {
	/** The configuration directory that will be scanned for flag definitions and feature references. */
	private final ConfigDirectory configDirectory;

	private BuildInfo buildInfoCache;
	private FlagDefinitions flagDefinitionsCache;
	private Map<String, FeatureDefinition> featureDefinitionsCache;
	private Map<String, BuildSpec> buildSpecCache;

	private Map<String, BuildSpec> invalidBuildSpecs;
	private Map<String, FeatureDefinition> invalidFeatureDefinitions;

	private final Map<String, Throwable> errors = new HashMap<String, Throwable>();

	/**
	 * Creates a new ObjectFactory which will load from the specified <code>configDirectory</code>.
	 *
	 * @param 	configDirectory 	The directory from which resources will be loaded.
	 */
	public ObjectFactory(ConfigDirectory configDirectory) {
		this.configDirectory = configDirectory;

		flushCaches();
	}

	/**
	 * Flush all cached values.
	 */
	public void flushCaches() {
		buildInfoCache = null;
		flagDefinitionsCache = null;
		featureDefinitionsCache = new HashMap<String, FeatureDefinition>();
		buildSpecCache = new HashMap<String, BuildSpec>();
		invalidBuildSpecs = new HashMap<String, BuildSpec>();
		invalidFeatureDefinitions = new HashMap<String, FeatureDefinition>();
	}

	/**
	 * Retrieves this {@link ObjectFactory}'s config directory.
	 *
	 * @return	the config directory
	 */
	public ConfigDirectory getConfigDirectory() {
		return configDirectory;
	}

	/**
	 * Sets this {@link ObjectFactory}'s flag definitions.
	 *
	 * @param 	defs		the new flag definitions
	 */
	public void setFlagDefinitions(FlagDefinitions defs) {
		flagDefinitionsCache = (defs != null) ? defs : new FlagDefinitions();
		flagDefinitionsCache.setRuntime(buildInfoCache.getRuntime());

		Collection<IFlagContainer> flagContainers = new HashSet<IFlagContainer>();

		flagContainers.addAll(featureDefinitionsCache.values());
		flagContainers.addAll(invalidFeatureDefinitions.values());
		flagContainers.addAll(buildSpecCache.values());
		flagContainers.addAll(invalidBuildSpecs.values());

		resetFlagDefinitions(flagContainers);
	}

	private void resetFlagDefinitions(Collection<IFlagContainer> containers) {
		for (IFlagContainer container : containers) {
			for (Flag flag : container.getLocalFlags().values()) {
				if (flagDefinitionsCache.contains(flag.getId())) {
					flag.setDefinition(flagDefinitionsCache.getFlagDefinition(flag.getId()));
				} else {
					flag.removeDefinition();
				}
			}
		}
	}

	/**
	 * Retrieves all the flag definitions for this <code>ObjectFactory</code>.
	 *
	 * @return 	the flag definitions
	 */
	public FlagDefinitions getFlagDefinitions() {
		return flagDefinitionsCache;
	}

	/**
	 * Sets the cached build info.
	 *
	 * @param 	buildInfo	the build info to be cached
	 */
	public void setBuildInfo(BuildInfo buildInfo) {
		buildInfoCache = buildInfo;
	}

	/**
	 * Retrieves the cached build info.
	 *
	 * @return 	The newly loaded build information.
	 */
	public BuildInfo getBuildInfo() {
		return buildInfoCache;
	}

	/**
	 * Adds the {@link BuildSpec} to the valid build spec cache.  If a {@link BuildSpec} with the same ID
	 * already exists in the cache it is overwritten.
	 *
	 * @param	buildSpec	the build spec to be added
	 */
	public void addBuildSpec(BuildSpec buildSpec) {
		if (invalidBuildSpecs.containsKey(buildSpec.getId())) {
			// Remove any errors associated with the invalid spec before removing it
			errors.remove("buildspec:" + buildSpec.getId()); //$NON-NLS-1$
			invalidBuildSpecs.remove(buildSpec.getId());
		}

		buildSpec.setRuntime(buildInfoCache.getRuntime());
		buildSpecCache.put(buildSpec.getId(), buildSpec);
	}

	/**
	 * Adds the {@link BuildSpec} in the given {@link InvalidBuildSpecException} to the invalid build
	 * spec cache. If a {@link BuildSpec} with the same ID already exists in the cache it is overwritten.
	 *
	 * @param	e			the exception containing the invalid build spec to be added
	 */
	public void addInvalidBuildSpec(InvalidBuildSpecException e) {
		if (e.getObject() != null) {
			errors.put("buildspec:" + e.getObject().getId(), e); //$NON-NLS-1$
			invalidBuildSpecs.put(e.getObject().getId(), e.getObject());
		}
	}

	/**
	 * Removes the given <code>BuildSpec</code> from the valid build spec cache.
	 *
	 * @param	buildSpecId		the ID of the build spec to be removed
	 * @return	the build spec removed
	 */
	public BuildSpec removeBuildSpec(String buildSpecId) {
		return buildSpecCache.remove(buildSpecId);
	}

	/**
	 * Removes the given {@link BuildSpec} from the invalid build spec cache.
	 *
	 * @param	buildSpecId		the ID of the build spec to be removed
	 * @return	the build spec removed
	 */
	public BuildSpec removeInvalidBuildSpec(String buildSpecId) {
		// Remove any errors associated with the invalid spec before removing it
		errors.remove("buildspec:" + buildSpecId); //$NON-NLS-1$
		return invalidBuildSpecs.remove(buildSpecId);
	}

	/**
	 * Adds the given {@link FeatureDefinition} to the valid feature cache.  If a {@link FeatureDefinition}
	 * with the same ID already exists in the cache it is overwritten.  Adding a feature definition
	 * will also link it to any {@link BuildSpec} that includes a {@link Feature} of the same ID.
	 *
	 * @param 	featureDefinition	the feature definition to be added
	 */
	public void addFeatureDefinition(FeatureDefinition featureDefinition) {
		for (BuildSpec buildSpec : buildSpecCache.values()) {
			if (buildSpec.hasFeature(featureDefinition.getId())) {
				buildSpec.getFeature(featureDefinition.getId()).setDefinition(featureDefinition);
			}
		}

		for (BuildSpec buildSpec : invalidBuildSpecs.values()) {
			if (buildSpec.hasFeature(featureDefinition.getId())) {
				buildSpec.getFeature(featureDefinition.getId()).setDefinition(featureDefinition);
			}
		}

		if (invalidFeatureDefinitions.containsKey(featureDefinition.getId())) {
			// Remove any errors associated with the invalid feature definition before removing it
			errors.remove("feature:" + featureDefinition.getId()); //$NON-NLS-1$
			invalidFeatureDefinitions.remove(featureDefinition.getId());
		}

		featureDefinition.setRuntime(buildInfoCache.getRuntime());
		featureDefinitionsCache.put(featureDefinition.getId(), featureDefinition);
	}

	/**Adds the {@link FeatureDefinition} in the given {@link InvalidFeatureDefinitionException} to
	 * the invalid feature cache.  If a {@link FeatureDefinition} with the same ID already exists in
	 * the cache it is overwritten.
	 *
	 * @param 	e			the exception containing the invalid feature definition to be added
	 */
	public void addInvalidFeatureDefinition(InvalidFeatureDefinitionException e) {
		if (e.getObject() != null) {
			errors.put("feature:" + e.getObject().getId(), e); //$NON-NLS-1$
			invalidFeatureDefinitions.put(e.getObject().getId(), e.getObject());
		}
	}

	/**
	 * Removes the {@link FeatureDefinition} with the given ID from the valid feature cache.  This also removes
	 * any links to this {@link FeatureDefinition} from all {@link BuildSpec}s including it.
	 *
	 * @param 	featureId		the ID of the feature definition to be removed
	 * @return	the removed feature
	 */
	public FeatureDefinition removeFeatureDefinition(String featureId) {
		for (BuildSpec buildSpec : buildSpecCache.values()) {
			if (buildSpec.hasFeature(featureId)) {
				buildSpec.getFeature(featureId).removeDefinition();
			}
		}

		for (BuildSpec buildSpec : invalidBuildSpecs.values()) {
			if (buildSpec.hasFeature(featureId)) {
				buildSpec.getFeature(featureId).removeDefinition();
			}
		}

		return featureDefinitionsCache.remove(featureId);
	}

	/**
	 * Removes the {@link FeatureDefinition} with the given ID from the invalid feature cache.
	 *
	 * @param 	featureId		the ID of the feature definition to be removed
	 * @return	the removed feature
	 */
	public FeatureDefinition removeInvalidFeatureDefinition(String featureId) {
		// Remove any errors associated with the invalid feature definition before removing it
		errors.remove("feature:" + featureId); //$NON-NLS-1$
		return invalidFeatureDefinitions.remove(featureId);
	}

	/**
	 * Loads the {@link FlagDefinitions} found in the given {@link File}.
	 *
	 * @param 	file		the file containing flag definitions
	 * @return	the parsed flag definitions
	 *
	 * @throws InvalidFlagDefinitionsException		thrown if errors are found while parsing the flag definitions file
	 */
	public FlagDefinitions loadFlagDefinitions(File file) throws InvalidFlagDefinitionsException {
		FlagDefinitionParser parser = new FlagDefinitionParser();
		FlagDefinitions defs = null;

		try {
			defs = parser.parse(file);
		} catch (IOException e) {
			throw new InvalidFlagDefinitionsException(e, "flags"); //$NON-NLS-1$
		}

		try {
			defs.verify();
		} catch (InvalidFlagDefinitionsException e) {
			throw new InvalidFlagDefinitionsException(e.getErrors(), defs);
		}

		defs.setRuntime(buildInfoCache.getRuntime());

		return defs;
	}

	/**
	 * Loads the {@link FlagDefinitions} found in the given {@link File}.
	 *
	 * @param 	inputStream		the input stream containing flag definitions
	 * @return	the parsed flag definitions
	 *
	 * @throws InvalidFlagDefinitionsException		thrown if errors are found while parsing the flag definitions input stream
	 */
	public FlagDefinitions loadFlagDefinitions(InputStream inputStream) throws InvalidFlagDefinitionsException {
		FlagDefinitionParser parser = new FlagDefinitionParser();
		FlagDefinitions defs = null;

		try {
			defs = parser.parse(inputStream);
		} catch (IOException e) {
			throw new InvalidFlagDefinitionsException(e, "flags"); //$NON-NLS-1$
		}

		try {
			defs.verify();
		} catch (InvalidFlagDefinitionsException e) {
			throw new InvalidFlagDefinitionsException(e.getErrors(), defs);
		}

		defs.setRuntime(buildInfoCache.getRuntime());

		return defs;
	}

	/**
	 * Loads the {@link BuildInfo} found in the given {@link File}.
	 *
	 * @param 	file			the file containing the build info
	 * @return	the parsed build info
	 *
	 * @throws InvalidBuildInfoException	thrown if errors are found while parsing the build info file
	 */
	public BuildInfo loadBuildInfo(File file) throws InvalidBuildInfoException {
		BuildInfo buildInfo;

		try {
			BuildInfoParser parser = new BuildInfoParser();
			buildInfo = parser.parse(file);
		} catch (IOException e) {
			throw new InvalidBuildInfoException(e, "buildInfo"); //$NON-NLS-1$
		}

		return buildInfo;
	}

	/**
	 * Loads the {@link BuildInfo} found in the given {@link InputStream}.
	 *
	 * @param 	inputStream		the input stream containing the build info
	 * @return	the parsed build info
	 *
	 * @throws InvalidBuildInfoException	thrown if errors are found while parsing the build info input stream
	 */
	public BuildInfo loadBuildInfo(InputStream inputStream) throws InvalidBuildInfoException {
		BuildInfo buildInfo;

		try {
			BuildInfoParser parser = new BuildInfoParser();
			buildInfo = parser.parse(inputStream);
		} catch (IOException e) {
			throw new InvalidBuildInfoException(e, "buildInfo"); //$NON-NLS-1$
		}

		return buildInfo;
	}

	/**
	 * Loads the {@link BuildSpec} found in the given {@link File}.
	 *
	 * @param 	file 		the spec file.
	 * @return 	the parsed build spec.
	 *
	 * @throws InvalidBuildSpecException 	thrown if errors are found while parsing the build spec file
	 */
	public BuildSpec loadBuildSpec(File file) throws InvalidBuildSpecException {
		BuildSpecParser parser = new BuildSpecParser();
		BuildSpec buildSpec = null;

		try {
			buildSpec = parser.parse(file, flagDefinitionsCache, featureDefinitionsCache);
		} catch (IOException e) {
			throw new InvalidBuildSpecException(e, file.getName().substring(0, file.getName().length() - ConfigDirectory.BUILD_SPEC_FILE_EXTENSION.length()));
		}

		try {
			buildSpec.verify(flagDefinitionsCache, buildInfoCache);
		} catch (InvalidBuildSpecException e) {
			buildSpec = e.getObject();
			String buildSpecId = (buildSpec == null) ? e.getObjectId() : buildSpec.getIdFromFile();
			errors.put("buildspec:" + buildSpecId, e); //$NON-NLS-1$

			if (buildSpec != null) {
				invalidBuildSpecs.put(buildSpec.getIdFromFile(), buildSpec);
			}

			throw e;
		}

		buildSpec.setRuntime(buildInfoCache.getRuntime());

		return buildSpec;
	}

	/**
	 * Loads the {@link BuildSpec} found in the given {@link InputStream}.
	 *
	 * @param  inputStream			the build spec input stream
	 * @return the newly loaded build spec.
	 *
	 * @throws InvalidBuildSpecException 	thrown if errors are found while parsing the build spec input stream
	 */
	public BuildSpec loadBuildSpec(InputStream inputStream, String objectId) throws InvalidBuildSpecException {
		BuildSpecParser parser = new BuildSpecParser();
		BuildSpec buildSpec = null;

		try {
			buildSpec = parser.parse(inputStream, objectId, flagDefinitionsCache, featureDefinitionsCache);
		} catch (IOException e) {
			throw new InvalidBuildSpecException(e, objectId);
		}

		try {
			buildSpec.verify(flagDefinitionsCache, buildInfoCache);
		} catch (InvalidBuildSpecException e) {
			buildSpec = e.getObject();
			String buildSpecId = (buildSpec == null) ? e.getObjectId() : buildSpec.getIdFromFile();
			errors.put("buildspec:" + buildSpecId, e); //$NON-NLS-1$

			if (buildSpec != null) {
				invalidBuildSpecs.put(buildSpec.getIdFromFile(), buildSpec);
			}

			throw e;
		}

		buildSpec.setRuntime(buildInfoCache.getRuntime());

		return buildSpec;
	}

	/**
	 * Loads the {@link FeatureDefinition} found in the given {@link File}.
	 *
	 * @param 	file 		the feature definition file
	 * @return 	the newly loaded feature definition
	 *
	 * @throws InvalidFeatureDefinitionException 		thrown if errors are found while parsing the feature definition file
	 */
	public FeatureDefinition loadFeatureDefinition(File file) throws InvalidFeatureDefinitionException {
		FeatureParser parser = new FeatureParser();
		FeatureDefinition featureDefinition = null;

		try {
			featureDefinition = parser.parse(file, flagDefinitionsCache);
		} catch (IOException e) {
			throw new InvalidFeatureDefinitionException(e, file.getName().substring(0, file.getName().length() - ConfigDirectory.FEATURE_FILE_EXTENSION.length()));
		}

		try {
			if (buildInfoCache == null) {
				featureDefinition.verify(flagDefinitionsCache, new HashSet<String>());
			} else {
				featureDefinition.verify(flagDefinitionsCache, buildInfoCache.getSources());
			}
		} catch (InvalidFeatureDefinitionException e) {
			featureDefinition = e.getObject();
			String featureDefinitionId = (featureDefinition == null) ? e.getObjectId() : featureDefinition.getId();
			errors.put("feature:" + featureDefinitionId, e); //$NON-NLS-1$

			if (featureDefinition != null) {
				invalidFeatureDefinitions.put(featureDefinition.getId(), featureDefinition);
			}

			throw e;
		}

		featureDefinition.setRuntime(buildInfoCache.getRuntime());

		return featureDefinition;
	}

	/**
	 * Loads the {@link FeatureDefinition} found in the given {@link InputStream}.
	 *
	 * @param 	inputStream 		the feature definition input stream
	 * @return 	the newly loaded feature definition
	 *
	 * @throws InvalidFeatureDefinitionException 		thrown if errors are found while parsing the feature definition input stream
	 */
	public FeatureDefinition loadFeatureDefinition(InputStream inputStream, String objectId) throws InvalidFeatureDefinitionException {
		FeatureParser parser = new FeatureParser();
		FeatureDefinition featureDefinition = null;

		try {
			featureDefinition = parser.parse(inputStream, objectId, flagDefinitionsCache);
		} catch (IOException e) {
			throw new InvalidFeatureDefinitionException(e, objectId);
		}

		try {
			if (buildInfoCache == null) {
				featureDefinition.verify(flagDefinitionsCache, new HashSet<String>());
			} else {
				featureDefinition.verify(flagDefinitionsCache, buildInfoCache.getSources());
			}
		} catch (InvalidFeatureDefinitionException e) {
			featureDefinition = e.getObject();
			String featureDefinitionId = (featureDefinition == null) ? e.getObjectId() : featureDefinition.getId();
			errors.put("feature:" + featureDefinitionId, e); //$NON-NLS-1$

			if (featureDefinition != null) {
				invalidFeatureDefinitions.put(featureDefinition.getId(), featureDefinition);
			}

			throw e;
		}

		featureDefinition.setRuntime(buildInfoCache.getRuntime());

		return featureDefinition;
	}

	/**
	 * Retrieves the {@link FeatureDefinition} with the given <code>featureId</code> from the cache.
	 *
	 * @param 	featureId		the ID of the desired Feature
	 * @return	the Feature requested
	 *
	 * @throws InvalidFeatureDefinitionException if the feature definition contains errors
	 */
	public FeatureDefinition getFeature(String featureId) throws InvalidFeatureDefinitionException {
		if (invalidFeatureDefinitions.containsKey(featureId)) {
			throw (InvalidFeatureDefinitionException) errors.get("feature:" + featureId); //$NON-NLS-1$
		}

		return featureDefinitionsCache.get(featureId);
	}

	public BuildSpec getDefaultActiveSpec() {
		int highestPriority = -1;
		BuildSpec defaultActiveSpec = null;

		for (BuildSpec spec : buildSpecCache.values()) {
			if (spec.getPriority() > highestPriority) {
				highestPriority = spec.getPriority();
				defaultActiveSpec = spec;
			}
		}

		return defaultActiveSpec;
	}

	/**
	 * Retrieves the {@link BuildSpec} with the given <code>buildSpecId</code> from the cache.
	 *
	 * @param 	buildSpecId		the ID of the desired BuildSpec
	 * @return	the BuildSpec requested
	 *
	 * @throws InvalidBuildSpecException
	 */
	public BuildSpec getBuildSpec(String buildSpecId) throws InvalidBuildSpecException {
		if (invalidBuildSpecs.containsKey(buildSpecId)) {
			throw (InvalidBuildSpecException) errors.get("buildspec:" + buildSpecId); //$NON-NLS-1$
		}

		return buildSpecCache.get(buildSpecId);
	}

	/**
	 * Retrieves a list of all the valid {@link BuildSpec}s in the factory's config directory.
	 *
	 * @return	a list of all the valid build specs
	 */
	public Collection<BuildSpec> getBuildSpecs() {
		return buildSpecCache.values();
	}

	public Collection<BuildSpec> getAllBuildSpecs() {
		Map<String, BuildSpec> temp = new TreeMap<String, BuildSpec>();
		temp.putAll(buildSpecCache);

		for (String specID : invalidBuildSpecs.keySet()) {
			if (invalidBuildSpecs.get(specID) != null) {
				temp.put(specID, invalidBuildSpecs.get(specID));
			}
		}

		return temp.values();
	}

	/**
	 * Retrieves the list of all valid {@link BuildSpec} IDs in this factory's config directory.
	 *
	 * @return	a list of all valid build spec IDs
	 */
	public Set<String> getBuildSpecIds() {
		return buildSpecCache.keySet();
	}

	/**
	 * Retrieves the map of all valid {@link BuildSpec}s in this factory's config directory.
	 *
	 * @return	a map of all valid build specs
	 */
	public Map<String, BuildSpec> getBuildSpecsMap() {
		return Collections.unmodifiableMap(buildSpecCache);
	}

	/**
	 * Returns a <code>Set</code> containing all the OS types for the specs in this <code>ObjectFactory</code>.
	 *
	 * @return	the OS types for all the build specs
	 */
	public Set<String> getBuildSpecOSes() {
		Set<String> buildSpecOSes = new TreeSet<String>();

		for (BuildSpec spec : buildSpecCache.values()) {
			buildSpecOSes.add(spec.getOs());
		}

		return buildSpecOSes;
	}

	/**
	 * Retrieves the valid build specs for the given OS.
	 *
	 * @param 	os			the OS for which to return build specs
	 * @return	the build specs of the given OS
	 */
	public Set<BuildSpec> getBuildSpecsByOS(String os) {
		Set<BuildSpec> buildSpecs = new TreeSet<BuildSpec>();

		for (BuildSpec buildSpec : buildSpecCache.values()) {
			if (os.equals(buildSpec.getOs())) {
				buildSpecs.add(buildSpec);
			}
		}

		return buildSpecs;
	}

	/**
	 * Retrieves the list of J9 Specifications that have the given feature. Returns an empty set
	 * if the feature ID is null or an empty string.
	 * 
	 * @param 	featureID		the ID of the feature to be searched for
	 * @return	the set of specs with the given feature, or an empty set if none exist or the
	 * 			feature ID is not valid
	 */
	public Set<BuildSpec> getBuildSpecsByFeature(String featureID) {
		Set<BuildSpec> buildSpecs = new TreeSet<BuildSpec>();

		if (featureID != null && featureID.length() > 0) {
			for (BuildSpec spec : buildSpecCache.values()) {
				if (spec.getFeatures().keySet().contains(featureID)) {
					buildSpecs.add(spec);
				}
			}
		}

		return buildSpecs;
	}

	/**
	 * Retrieves the list of J9 Specification IDs that have the given feature. Returns an empty set
	 * if the feature ID is null or an empty string.
	 * 
	 * @param 	featureID		the ID of the feature to be searched for
	 * @return	the set of spec IDs with the given feature, or an empty set if none exist or the
	 * 			feature ID is not valid
	 */
	public Set<String> getBuildSpecIDsByFeature(String featureID) {
		Set<String> buildSpecs = new TreeSet<String>();

		if (featureID != null && featureID.length() > 0) {
			for (BuildSpec spec : buildSpecCache.values()) {
				if (spec.getFeatures().keySet().contains(featureID)) {
					buildSpecs.add(spec.getId());
				}
			}
		}

		return buildSpecs;
	}

	/**
	 * Retrieves the valid {@link FeatureDefinition}s in the factory's config directory.
	 *
	 * @return	the valid feature definitions
	 */
	public Collection<FeatureDefinition> getFeatureDefinitions() {
		return featureDefinitionsCache.values();
	}

	/**
	 * Retrieves the set of valid {@link FeatureDefinition} IDs the factory's config directory.
	 *
	 * @return	the valid feature definition IDs
	 */
	public Set<String> getFeatureDefinitionIds() {
		return featureDefinitionsCache.keySet();
	}

	/**
	 * Retrieves the map of valid {@link FeatureDefinition}s the factory's config directory.
	 *
	 * @return	the valid feature definitions
	 */
	public Map<String, FeatureDefinition> getFeatureDefinitionsMap() {
		return Collections.unmodifiableMap(featureDefinitionsCache);
	}

	/**
	 * Loads a runtime's build-info, flag definitions as well as all the valid build specs and features
	 * contained in this <code>ObjectFactory</code>'s config directory.
	 *
	 * Errors found when loading the build-info and flag definitions are considered fatal and will result
	 * on an {@link InvalidFactoryException} being thrown (even if errors are delayed).  Errors found
	 * when loading features or build specs will result in that element not being loaded into the factory.
	 *
	 * @param delayErrors		delay throwing exceptions unless error is critical (build-info or flags invalid)
	 * 
	 * @throws InvalidFactoryException		thrown if errors are found when initializing factory elements
	 */
	public void initialize(boolean delayErrors) throws InvalidFactoryException {
		boolean notDelayable = false;

		// Load basic information
		try {
			buildInfoCache = loadBuildInfo(configDirectory.getBuildInfoFile());
		} catch (InvalidBuildInfoException e) {
			buildInfoCache = e.getObject();
			errors.put("buildInfo", e); //$NON-NLS-1$
			notDelayable = true;
		}

		// Can't parse much more if we don't even have a partial build-info
		if (buildInfoCache != null) {
			// Load flag definitions
			try {
				flagDefinitionsCache = loadFlagDefinitions(configDirectory.getFlagDefinitionsFile());
			} catch (InvalidFlagDefinitionsException e) {
				flagDefinitionsCache = (e.getObject() == null) ? new FlagDefinitions() : e.getObject();
				errors.put("flags", e); //$NON-NLS-1$
				notDelayable = true;
			}

			// Load features
			for (File file : configDirectory.getFeatureFiles()) {
				FeatureDefinition featureDefinition;

				try {
					featureDefinition = loadFeatureDefinition(file);
					featureDefinitionsCache.put(featureDefinition.getId(), featureDefinition);
				} catch (InvalidFeatureDefinitionException e) {
					errors.put("feature:" + e.getObjectId(), e); //$NON-NLS-1$
				}
			}

			// Load build specs
			for (File file : configDirectory.getBuildSpecFiles()) {
				BuildSpec buildSpec;

				try {
					buildSpec = loadBuildSpec(file);
					buildSpecCache.put(buildSpec.getId(), buildSpec);
				} catch (InvalidBuildSpecException e) {
					errors.put("buildspec:" + e.getObjectId(), e); //$NON-NLS-1$
				}
			}
		}

		if (errors.size() > 0 && (!delayErrors || notDelayable)) {
			throw new InvalidFactoryException(errors.values(), this);
		}
	}

	/**
	 * Initializes the factory without delaying errors found.
	 *
	 * @see #initialize(boolean)
	 *
	 * @throws InvalidFactoryException		thrown if errors are found when initializing factory elements
	 */
	public void initialize() throws InvalidFactoryException {
		initialize(false);
	}

	/**
	 * Verifies the flag definitions, features, and build specs for validity.  This is to be used
	 * when modifying elements on a object factory that has already been initialized.
	 *
	 * @throws 	InvalidFactoryException		thrown if errors are found when verifying factory elements
	 */
	public void verify() throws InvalidFactoryException {
		Set<Throwable> exceptions = new HashSet<Throwable>();

		Set<BuildSpec> brokenSpecs = new HashSet<BuildSpec>();
		Set<BuildSpec> cleanSpecs = new HashSet<BuildSpec>();
		Set<FeatureDefinition> brokenFeatures = new HashSet<FeatureDefinition>();
		Set<FeatureDefinition> cleanFeatures = new HashSet<FeatureDefinition>();

		// Verify flag definitions
		try {
			flagDefinitionsCache.verify();
		} catch (InvalidFlagDefinitionsException e) {
			exceptions.add(e);
		}

		// Check features that were broken
		// If they have been fixed then add them back
		for (FeatureDefinition featureDefinition : invalidFeatureDefinitions.values()) {
			try {
				if (buildInfoCache == null) {
					featureDefinition.verify(flagDefinitionsCache, new HashSet<String>());
				} else {
					featureDefinition.verify(flagDefinitionsCache, buildInfoCache.getSources());
				}

				cleanFeatures.add(featureDefinition);
			} catch (InvalidFeatureDefinitionException e) {
				exceptions.add(e);
			}
		}

		// If the broken features has been fixed remove it from the broken list
		for (FeatureDefinition featureDefinition : cleanFeatures) {
			invalidFeatureDefinitions.remove(featureDefinition.getId());
			addFeatureDefinition(featureDefinition);
		}

		// Verify feature Definitions
		for (FeatureDefinition featureDefinition : featureDefinitionsCache.values()) {
			try {
				if (buildInfoCache == null) {
					featureDefinition.verify(flagDefinitionsCache, new HashSet<String>());
				} else {
					featureDefinition.verify(flagDefinitionsCache, buildInfoCache.getSources());
				}
			} catch (InvalidFeatureDefinitionException e) {
				String featureDefinitionId = (featureDefinition == null) ? e.getObjectId() : featureDefinition.getId();
				errors.put("feature:" + featureDefinitionId, e); //$NON-NLS-1$
				brokenFeatures.add(featureDefinition);
				exceptions.add(e);
			}
		}

		// If a feature is broken then add it to the broken list
		for (FeatureDefinition featureDefinition : brokenFeatures) {
			removeFeatureDefinition(featureDefinition.getId());
			invalidFeatureDefinitions.put(featureDefinition.getId(), featureDefinition);
		}

		// Check build specs that were broken
		// If they have been fixed then add them back
		for (BuildSpec buildSpec : invalidBuildSpecs.values()) {
			try {
				buildSpec.verify(flagDefinitionsCache, buildInfoCache);
				cleanSpecs.add(buildSpec);
			} catch (InvalidBuildSpecException e) {
				exceptions.add(e);
			}
		}

		// If the broken spec has been fixed remove it from the broken list
		for (BuildSpec buildSpec : cleanSpecs) {
			invalidBuildSpecs.remove(buildSpec.getId());
			addBuildSpec(buildSpec);
		}

		// Verify build specs
		for (BuildSpec buildSpec : buildSpecCache.values()) {
			try {
				buildSpec.verify(flagDefinitionsCache, buildInfoCache);
			} catch (InvalidBuildSpecException e) {
				String buildSpecId = (buildSpec == null) ? e.getObjectId() : buildSpec.getIdFromFile();
				errors.put("buildspec:" + buildSpecId, e); //$NON-NLS-1$
				brokenSpecs.add(buildSpec);
				exceptions.add(e);
			}
		}

		// If a build spec is broken then add it to the broken list
		for (BuildSpec buildSpec : brokenSpecs) {
			removeBuildSpec(buildSpec.getId());
			invalidBuildSpecs.put(buildSpec.getId(), buildSpec);
		}

		if (exceptions.size() > 0) {
			throw new InvalidFactoryException(exceptions, this);
		}
	}
}
