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
import java.io.PrintStream;
import java.text.MessageFormat;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/**
 * The {@link BuildSpec} class describes the details of a J9 specification.  It defines the flags,
 * sources, and featuers which make up the specification as well as any other relevant information.
 *
 * @author 	Maciek Klimkowski
 * @author 	Gabriel Castro
 */
public class BuildSpec extends OMObject implements IFlagContainer, ISourceContainer, Comparable<BuildSpec> {
	private String id = null; /* !< build spec id */
	private String name = null; /* !< build spec name */
	private String runtime = null;
	private String cpuArchitecture = null;
	private String os = null;
	private DefaultSizes defaultSizes = null;
	private AsmBuilder asmBuilder = null;
	private JclConfiguration jclConfiguration = null;
	private int priority = -1;

	private final Map<String, Feature> features = new TreeMap<String, Feature>();
	private final Map<String, Flag> flags = new TreeMap<String, Flag>();
	private final Map<String, Owner> owners = new TreeMap<String, Owner>();
	private final Map<String, Source> sources = new TreeMap<String, Source>();
	private final Map<String, Property> properties = new TreeMap<String, Property>();

	/**
	 * Retrieves the specification ID from the given file name.  The file name must have the
	 * specification file extension {@link ConfigDirectory#BUILD_SPEC_FILE_EXTENSION}.
	 * 
	 * @param	filename	the specification file
	 * @return	the spec ID or <code>null</code> if the filename is not identified as a spec file
	 */
	public static String getIDFromFileName(String filename) {
		if (filename != null && filename.endsWith(ConfigDirectory.BUILD_SPEC_FILE_EXTENSION)) {
			return filename.substring(filename.lastIndexOf(File.separator) + 1, filename.length() - (ConfigDirectory.BUILD_SPEC_FILE_EXTENSION.length()));
		}

		return null;
	}

	/**
	 * Adds a spec owner to this build spec.
	 *
	 * @param 	owner 		one of the owners of this build spec
	 */
	public void addOwner(Owner owner) {
		owners.put(owner.getId(), owner);
	}

	/**
	 * Retrieves the owner of this build spec for the given owner ID.
	 *
	 * @param 	ownerId 	the ID of the owner
	 * @return 	Owner instance identified by ownerId
	 */
	public Owner getOwner(String ownerId) {
		return owners.get(ownerId);
	}

	public Map<String, Owner> getOwners() {
		return Collections.unmodifiableMap(owners);
	}

	/**
	 * Retrieves a set of owner IDs defined for this build spec.
	 *
	 * @return 	a Set<String> of owner ids
	 */
	public Set<String> getOwnerIds() {
		return owners.keySet();
	}

	/**
	 * Removes the given owner from this build spec.
	 *
	 * @param 	owner		the owner to be remove
	 */
	public void removeOwner(Owner owner) {
		owners.remove(owner.getId());
	}

	/**
	 * Removes the owner with the given ID from this build spec.
	 *
	 * @param 	ownerId		the ID of the owner to be removed
	 */
	public void removeOwner(String ownerId) {
		owners.remove(ownerId);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#addFlag(com.ibm.j9tools.om.Flag)
	 */
	public void addFlag(Flag flag) {
		flags.put(flag.getId(), flag);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#getFlag(java.lang.String)
	 */
	public Flag getFlag(String flagId) {
		if (flags.containsKey(flagId)) {
			return flags.get(flagId);
		}

		for (Feature feature : features.values()) {
			if (feature.hasFlag(flagId)) {
				return feature.getFlag(flagId);
			}
		}

		return null;
	}

	/**
	 * Returns a local Flag
	 * @param 	flagId	ID of the Flag that needs to be returned
	 * @return	Flag	The Flag that is wanted, or <code>null</code> if the Flag does not exist
	 */
	public Flag getLocalFlag(String flagId) {
		return flags.get(flagId);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#getFlags()
	 */
	public Map<String, Flag> getFlags() {
		Map<String, Flag> allFlags = new TreeMap<String, Flag>();
		allFlags.putAll(flags);

		for (Feature feature : features.values()) {
			allFlags.putAll(feature.getFlags());
		}

		return Collections.unmodifiableMap(allFlags);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#getFlags(java.lang.String)
	 */
	public Map<String, Flag> getFlags(String category) {
		Map<String, Flag> allFlags = new TreeMap<String, Flag>();

		for (Flag flag : getFlags().values()) {
			if (flag.getCategory().equals(category)) {
				allFlags.put(flag.getId(), flag);
			}
		}

		return Collections.unmodifiableMap(allFlags);
	}

	/**
	 * Retrieves only the flags that were defined in this build spec.  This does not include
	 * any flags defined in this J9 specification's features.
	 *
	 * @return	a {@link Map} of the local flags
	 */
	public Map<String, Flag> getLocalFlags() {
		return Collections.unmodifiableMap(flags);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#getLocalFlags(java.lang.String)
	 */
	public Map<String, Flag> getLocalFlags(String category) {
		Map<String, Flag> allFlags = new TreeMap<String, Flag>();

		for (Flag flag : flags.values()) {
			if (flag.getCategory().equals(category)) {
				allFlags.put(flag.getId(), flag);
			}
		}

		return Collections.unmodifiableMap(allFlags);
	}

	/**
	 * Checks for existence of a flag. Not to be confused with the value of a flag.
	 *
	 * @param 	flagId 		ID of the flag to check for
	 * @return	<code>true</code> if the flag is included for this spec, <code>false</code> otherwise
	 */
	public boolean hasFlag(String flagId) {
		if (flags.containsKey(flagId)) {
			return true;
		}

		// Check if one of the features has the flag
		for (Feature feature : features.values()) {
			if (feature.hasFlag(flagId)) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Checks for the existence of a local flag.
	 *
	 * @param 	flagId 		ID of the flag to check for
	 * @return	<code>true</code> if the flag is included for this spec, <code>false</code> otherwise
	 */
	public boolean hasLocalFlag(String flagId) {
		return flags.containsKey(flagId);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#removeFlag(com.ibm.j9tools.om.Flag)
	 */
	public void removeFlag(Flag flag) {
		flags.remove(flag.getId());
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#removeFlag(java.lang.String)
	 */
	public void removeFlag(String flagId) {
		flags.remove(flagId);
	}

	/**
	 * Adds a fully initialized feature to the build spec. Non-conflicting flags are removed from the local
	 * list since they're contributed to the build spec by the feature.  Sources also found in the feature
	 * are removed from the build spec.
	 *
	 * @param 	feature 	loaded instance of Feature
	 */
	public void addFeature(Feature feature) {
		addFeature(feature, false);
	}

	/**
	 * Adds a fully initialized feature to the build spec. Non-conflicting flags are removed from the local
	 * list since they're contributed to the build spec by the feature.  Sources also found in the feature
	 * are removed from the build spec.
	 *
	 * @param 	feature 	loaded instance of Feature
	 * @param 	overwrite	when <code>true</code> overwrites all flags regardless of conflict
	 */
	public void addFeature(Feature feature, boolean overwrite) {
		features.put(feature.getId(), feature);

		// Remove sources already found in the in feature
		sources.values().removeAll(feature.getSources().values());

		for (Flag featureFlag : feature.getFlags().values()) {
			Flag localFlag = flags.get(featureFlag.getId());

			if (overwrite || (localFlag != null && localFlag.equals(featureFlag))) {
				flags.remove(featureFlag.getId());
			}
		}
	}

	/**
	 * Removes the given feature from this build spec.
	 *
	 * @param 	feature		the feature to be removed
	 */
	public Feature removeFeature(Feature feature) {
		return features.remove(feature.getId());
	}

	/**
	 * Removes the feature with the given ID from this build spec.
	 *
	 * @param 	featureId	the ID of the feature to be removed
	 */
	public Feature removeFeature(String featureId) {
		return features.remove(featureId);
	}

	/**
	 * Removes the given feature from this build spec and returns its inherited sources
	 * and flags to the build spec.
	 *
	 * @param 	feature		the feature to be removed
	 */
	public void unbundleFeature(Feature feature) {
		Feature localFeature = features.remove(feature.getId());

		if (localFeature != null) {
			for (Source source : localFeature.getSources().values()) {
				addSource(source);
			}

			for (Flag flag : localFeature.getFlags().values()) {
				addFlag(flag);
			}
		}
	}

	/**
	 * Removes the feature with the given ID from this build spec and returns its inherited sources
	 * and flags to the build spec.
	 *
	 * @param 	featureId	the ID of the feature to be removed
	 */
	public void unbundleFeature(String featureId) {
		Feature localFeature = features.remove(featureId);

		if (localFeature != null) {
			for (Source source : localFeature.getSources().values()) {
				addSource(source);
			}

			for (Flag flag : localFeature.getFlags().values()) {
				addFlag(flag);
			}
		}
	}

	/**
	 * Retrieves a set of features defined for this build spec
	 *
	 * @return 	a Collection<Feature>.
	 */
	public Map<String, Feature> getFeatures() {
		return Collections.unmodifiableMap(features);
	}

	/**
	 * Retrieves a Feature identified by the featureId parameter
	 *
	 * @param 	featureId	Feature to Retrieves from the build spec
	 * @return 	a Feature
	 */
	public Feature getFeature(String featureId) {
		return features.get(featureId);
	}

	/**
	 * Checks for existence of a feature.
	 *
	 * @param 	featureId	ID of the feature to check for
	 * @return	<code>true</code> if the feature is included for this spec, <code>false</code> otherwise
	 */
	public boolean hasFeature(String featureId) {
		return features.containsKey(featureId);
	}

	/**
	 * Retrieves the set of sources defined for in build spec.
	 *
	 * @return	a {@link Map} of all the sources
	 */
	public Map<String, Source> getSources() {
		Map<String, Source> allSources = new TreeMap<String, Source>();
		allSources.putAll(sources);

		for (Feature feature : features.values()) {
			allSources.putAll(feature.getSources());
		}

		return Collections.unmodifiableMap(allSources);
	}

	/**
	 * Retrieves all the local sources for this build spec.  This does not include sources
	 * defined in included features.
	 *
	 * @return	a {@link Map} of the local sources
	 */
	public Map<String, Source> getLocalSources() {
		return Collections.unmodifiableMap(sources);
	}

	/**
	 * Retrieves a source module definition
	 *
	 * @param 	sourceId 	The ID of the source module
	 * @return 	the requested source
	 */
	public Source getSource(String sourceId) {
		if (sources.containsKey(sourceId)) {
			return sources.get(sourceId);
		}

		for (Feature feature : features.values()) {
			if (feature.hasSource(sourceId)) {
				return feature.getSource(sourceId);
			}
		}

		return null;
	}

	/**
	 * Retrieves a source module definition that is local to this container
	 *
	 * @param 	sourceId 	The ID of the source module
	 * @return 	the requested source
	 */
	public Source getLocalSource(String sourceId) {
		return sources.get(sourceId);
	}

	/**
	 * Adds the given source to this build spec.
	 *
	 * @param 	source		the source to be added
	 */
	public void addSource(Source source) {
		sources.put(source.getId(), source);
	}

	/**
	 * Removes the given source from this build spec.
	 *
	 * @param 	source		the source to be removed
	 */
	public void removeSource(Source source) {
		sources.remove(source.getId());
	}

	/**
	 * Removes the source with the given source ID from this build spec.
	 *
	 * @param 	sourceId	the ID of the source
	 */
	public void removeSource(String sourceId) {
		sources.remove(sourceId);
	}

	/**
	 * Checks for existence of a source.
	 *
	 * @param 	sourceId	ID of the source to check for
	 * @return	<code>true</code> if the source is included for this spec, <code>false</code> otherwise
	 */
	public boolean hasSource(String sourceId) {
		if (sources.containsKey(sourceId)) {
			return true;
		}

		// Check if one of the features has the flag
		for (Feature feature : features.values()) {
			if (feature.hasSource(sourceId)) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Checks for existence of a local source.
	 *
	 * @param 	sourceId	ID of the source to check for
	 * @return	<code>true</code> if the source is included for this container, <code>false</code> otherwise
	 */
	public boolean hasLocalSource(String sourceId) {
		return sources.containsKey(sourceId);
	}

	/**
	 * Retrieves the set of properties defined for this build spec.
	 *
	 * @return a Collection<Property>
	 */
	public Map<String, Property> getProperties() {
		return Collections.unmodifiableMap(properties);
	}

	/**
	 * Retrieves a Property matching given ID
	 *
	 * @param 	propertyId 		The ID of the property
	 * @return 	a Property
	 */
	public Property getProperty(String propertyId) {
		return properties.get(propertyId);
	}

	/**
	 * Adds the given property to this build spec.
	 *
	 * @param 	property	the property to be added
	 */
	public void addProperty(Property property) {
		properties.put(property.getName(), property);
	}

	/**
	 * Removes the given property from this build spec.
	 *
	 * @param 	property	the property to be removed
	 */
	public void removeProperty(Property property) {
		properties.remove(property.getName());
	}

	/**
	 * Removes the property with the given ID from this build spec.
	 *
	 * @param 	propertyId	the property ID
	 */
	public void removeProperty(String propertyId) {
		properties.remove(propertyId);
	}

	/**
	 * Retrieves the assembly builder defined in this build spec.
	 *
	 * @return	the name assembly builder
	 */
	public AsmBuilder getAsmBuilder() {
		return asmBuilder;
	}

	/**
	 * Sets the name of the assembly builder for this build spec.
	 *
	 * @param 	asmBuilder		the assembly builder
	 */
	public void setAsmBuilder(AsmBuilder asmBuilder) {
		this.asmBuilder = asmBuilder;
	}

	/**
	 * Retrieves the name of the CPU architecture defined for this build spec.
	 *
	 * @return	the name of the CPU architecture
	 */
	public String getCpuArchitecture() {
		return cpuArchitecture;
	}

	/**
	 * Sets the name of the CPU architecture for this build spec.
	 * @param cpuArchitecture
	 */
	public void setCpuArchitecture(String cpuArchitecture) {
		this.cpuArchitecture = cpuArchitecture;
	}

	/**
	 * Retries the name of the default JCL configuration for this build spec.
	 *
	 * @return	the name of the default JCL configuration
	 */
	public JclConfiguration getDefaultJCL() {
		return jclConfiguration;
	}

	/**
	 * Sets the name of the default JCL configuration for this build spec.
	 *
	 * @param 	jclConfiguration		the default JCL configuration
	 */
	public void setDefaultJCL(JclConfiguration jclConfiguration) {
		this.jclConfiguration = jclConfiguration;
	}

	/**
	 * Retrieves the default size for this build spec.
	 *
	 * @return	the default size
	 */
	public DefaultSizes getDefaultSizes() {
		return defaultSizes;
	}

	/**
	 * Sets the default size for this build spec.
	 *
	 * @param 	defaultSizes	the default size
	 */
	public void setDefaultSizes(DefaultSizes defaultSizes) {
		this.defaultSizes = defaultSizes;
	}

	/**
	 * Retrieves this J9 specification's ID.
	 *
	 * @return	the ID
	 */
	public String getId() {
		return id;
	}

	public String getIdFromFile() {
		return getIDFromFileName(getLocation().getFileName());
	}

	/**
	 * Sets this J9 specification's ID.
	 *
	 * @param	id			the build spec ID
	 */
	public void setId(String id) {
		this.id = id;
	}

	/**
	 * Retrieves this J9 specification's name.
	 *
	 * @return	the name
	 */
	public String getName() {
		return name;
	}

	/**
	 * Retrieves this J9 specification's unique name.  If the runtime is available the unique name
	 * consists of the runtime and the specification id; otherwise, only the ID is returned.
	 * 
	 * @return the unique specification name
	 */
	public String getUniqueName() {
		if (runtime != null) {
			return runtime + ":" + id; //$NON-NLS-1$
		}

		return getId();
	}

	/**
	 * Sets this J9 specification's name.
	 *
	 * @param	name		the name
	 */
	public void setName(String name) {
		this.name = name;
	}

	/**
	 * Returns the runtime to which this build spec belongs
	 * 
	 * @return	the runtime name
	 */
	public String getRuntime() {
		return runtime;
	}

	/**
	 * Sets the runtime to which this build spec belongs.
	 * 
	 * @param runtime	the new runtime
	 */
	protected void setRuntime(String runtime) {
		if (runtime != null) {
			this.runtime = runtime;
		}
	}

	/**
	 * Retrieves the operating system for this build spec.
	 *
	 * @return	the operating system name
	 */
	public String getOs() {
		return os;
	}

	/**
	 * Sets the name of the operating system for this build spec.
	 *
	 * @param 	os			operating system name
	 */
	public void setOs(String os) {
		this.os = os;
	}

	/**
	 * Retrieves the priority for this build spec.
	 *
	 * @return	the priority value
	 */
	public int getPriority() {
		return priority;
	}

	/**
	 * Sets the priority for this build spec.
	 *
	 * @param 	priority	the priority value
	 */
	public void setPriority(int priority) {
		this.priority = priority;
	}

	/**
	 * Verifies the validity of this {@link BuildSpec}'s assembly builders, JCL profiles, sources and flags.
	 *
	 * For ASM builders, JCL profiles, and sources validity implies those being defined in the runtime's
	 * {@link BuildInfo}.  Source verification is done by a {@link SourceVerifier}.
	 *
	 * Flag verification is done by a {@link FlagVerifier}.
	 *
	 * @param 	flagDefinitions		the runtime's flag definitions
	 * @param 	buildInfo			the runtime's build information
	 *
	 * @throws InvalidBuildSpecException	thrown when ASMBuilder, JCL, source, or flag errors are found
	 */
	public void verify(FlagDefinitions flagDefinitions, BuildInfo buildInfo) throws InvalidBuildSpecException {
		Collection<Throwable> errors = new HashSet<Throwable>();

		Map<String, Property> featureProperties = new HashMap<String, Property>();
		Map<String, Flag> featureFlags = new HashMap<String, Flag>();
		Map<String, Source> featureSources = new HashMap<String, Source>();

		String idFromFile = getIDFromFileName(getLocation().getFileName());

		if (!id.equalsIgnoreCase(idFromFile)) {
			errors.add(new InvalidSpecIDException(Messages.getString("BuildSpec.specIDAndFilenameNotEqual"), this)); //$NON-NLS-1$
		}

		if (buildInfo == null || !buildInfo.getASMBuilders().contains(asmBuilder.getId())) {
			errors.add(new InvalidAsmBuilderException(MessageFormat.format(Messages.getString("BuildSpec.invalidASM"), new Object[] { asmBuilder.getId() }), asmBuilder)); //$NON-NLS-1$
		}

		if (buildInfo == null || !buildInfo.getJCLs().contains(jclConfiguration.getId())) {
			errors.add(new InvalidJCLException(MessageFormat.format(Messages.getString("BuildSpec.invalidJCL"), new Object[] { jclConfiguration.getId() }), jclConfiguration)); //$NON-NLS-1$
		}

		if (buildInfo == null || (buildInfo.validateDefaultSizes() && !buildInfo.getDefaultSizes().containsKey(defaultSizes.getId()))) {
			errors.add(new InvalidDefaultSizesException(MessageFormat.format(Messages.getString("BuildSpec.invalidDefaultSize"), new Object[] { defaultSizes.getId() }), defaultSizes)); //$NON-NLS-1$
		}

		for (Feature feature : features.values()) {
			if (feature.isComplete()) {
				featureProperties.putAll(feature.getProperties());
				featureFlags.putAll(feature.getFlags());
				featureSources.putAll(feature.getSources());
			} else {
				errors.add(new InvalidFeatureException(MessageFormat.format(Messages.getString("BuildSpec.invalidFeature"), new Object[] { feature.getId() }), feature)); //$NON-NLS-1$
			}
		}

		// Check that properties defined in features are set properly
		for (Property property : featureProperties.values()) {
			if (!properties.containsKey(property.getName())) {
				errors.add(new MissingPropertyException(property, this.getLocation().getFileName()));
			}
		}

		// Check for local flags that are already defined in feature
		for (Flag flag : getLocalFlags().values()) {
			if (featureFlags.containsKey(flag.getId())) {
				errors.add(new InvalidFlagException(MessageFormat.format(Messages.getString("BuildSpec.invalidFlag"), new Object[] { flag.getId() }), flag)); //$NON-NLS-1$
			}
		}

		// Check for local sources that are already defined in feature
		for (Source source : getLocalSources().values()) {
			if (featureSources.containsKey(source.getId())) {
				errors.add(new InvalidSourceException(MessageFormat.format(Messages.getString("BuildSpec.invalidSource"), new Object[] { source.getId() }), source)); //$NON-NLS-1$
			}
		}

		// Verify sources for existence in Build Info definition
		SourceVerifier sourceVerifier = (buildInfo == null) ? new SourceVerifier(this, new HashSet<String>()) : new SourceVerifier(this, buildInfo.getSources());
		errors.addAll(sourceVerifier.verify());

		// Verify the validity of all flags
		FlagVerifier flagVerifier = new FlagVerifier(this, flagDefinitions);
		errors.addAll(flagVerifier.verify());

		if (errors.size() > 0) {
			throw new InvalidBuildSpecException(errors, this);
		}
	}

	/**
	 * Debug helper used to dump this spec's member variables and lists
	 *
	 * @param 	out 			output stream to print to
	 * @param 	prefix 			prefix to prepend to each line
	 * @param 	indentLevel		number of spaces to append to the prefix
	 */
	public void dump(PrintStream out, String prefix, int indentLevel) {
		StringBuffer indent = new StringBuffer(prefix);
		for (int i = 0; i < indentLevel; i++) {
			indent.append(' ');
		}

		out.println(indent + "Spec: " + this.getId()); //$NON-NLS-1$
		out.println(indent + " |--- name:            " + this.getName()); //$NON-NLS-1$
		out.println(indent + " |--- asmBuilderName:  " + this.getAsmBuilder().getId()); //$NON-NLS-1$
		out.println(indent + " |--- cpuArchitecture: " + this.getCpuArchitecture()); //$NON-NLS-1$
		out.println(indent + " |--- os:              " + this.getOs()); //$NON-NLS-1$
		out.println(indent + " |--- defaultJCL:      " + this.getDefaultJCL().getId()); //$NON-NLS-1$
		out.println(indent + " |--- defaultSizes:    " + this.getDefaultSizes()); //$NON-NLS-1$
		out.println(indent + " |--- priority:        " + this.getPriority()); //$NON-NLS-1$

		out.println(indent + " |"); //$NON-NLS-1$
		out.println(indent + " |--- Features        "); //$NON-NLS-1$
		for (Feature f : getFeatures().values()) {
			out.println(indent + " |    |-- Feature"); //$NON-NLS-1$
			out.println(indent + " |    |     |- id:          " + f.getId()); //$NON-NLS-1$
			out.println(indent + " |    |     +- is complete: " + f.isComplete()); //$NON-NLS-1$
		}

		out.println(indent + " |"); //$NON-NLS-1$
		out.println(indent + " |--- Flags        "); //$NON-NLS-1$
		for (Flag f : getFlags().values()) {
			out.println(indent + " |    |-- Flag"); //$NON-NLS-1$
			out.println(indent + " |    |     +-- id:      " + f.getId()); //$NON-NLS-1$
			out.println(indent + " |    |     +-- state:   " + f.getState()); //$NON-NLS-1$
		}

		out.println(indent + " |"); //$NON-NLS-1$
		out.println(indent + " |--- Sources        "); //$NON-NLS-1$
		for (Source s : getSources().values()) {
			out.println(indent + " |    |-- source"); //$NON-NLS-1$
			out.println(indent + " |    |     +-- id:      " + s.getId()); //$NON-NLS-1$
		}

	}

	/**
	 * Compares this build spec to another build spec for ordering.  This ordering
	 * is based on the build spec ID and its runtime (if one is set).
	 *
	 * @see java.lang.Comparable#compareTo(java.lang.Object)
	 */
	public int compareTo(BuildSpec buildSpec) {
		int idCompareResult = id.compareTo(buildSpec.id);

		if (idCompareResult == 0 && runtime != null && buildSpec.getRuntime() != null) {
			return runtime.compareTo(buildSpec.getRuntime());
		}

		return idCompareResult;
	}

	/**
	 * @see java.lang.Object#toString()
	 */
	@Override
	public String toString() {
		StringBuilder sb = new StringBuilder();
		if (runtime != null) {
			sb.append(runtime);
			sb.append(":"); //$NON-NLS-1$
		}
		sb.append(id);

		return sb.toString();
	}

	/**
	 * @see java.lang.Object#equals(java.lang.Object)
	 */
	@Override
	public boolean equals(Object o) {
		if (o instanceof BuildSpec) {
			BuildSpec spec = (BuildSpec) o;
			if (runtime != null && spec.getRuntime() != null) {
				return runtime.equals(spec.getRuntime()) && name.equals(spec.getName()) && id.equals(spec.getId());
			}

			return name.equals(spec.getName()) && id.equals(spec.getId());
		}

		return false;
	}

}
