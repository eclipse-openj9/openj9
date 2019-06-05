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

import java.io.PrintStream;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/**
 * The Feature class defines the elements of a J9 Build Spec Feature.  It encompasses flags and sources
 * which are common among multiple build specifications and should be re-used.
 * 
 * @author 	Gabriel Castro
 */
public class FeatureDefinition extends OMObject implements IFlagContainer, ISourceContainer {
	protected String id;
	protected String name;
	protected String description;
	private String runtime;

	protected Map<String, Property> properties;
	protected Map<String, Flag> flags;
	protected Map<String, Source> sources;

	protected boolean complete = false;

	/**
	 * Retrieves the specification ID from the given file name.  The file name must have the
	 * specification file extension {@link ConfigDirectory#BUILD_SPEC_FILE_EXTENSION}.
	 * 
	 * @param	filename	the specification file
	 * @return	the spec ID or <code>null</code> if the filename is not identified as a spec file
	 */
	public static String getIDFromFileName(String filename) {
		if (filename != null && filename.endsWith(ConfigDirectory.FEATURE_FILE_EXTENSION)) {
			return filename.substring(0, filename.length() - (ConfigDirectory.FEATURE_FILE_EXTENSION.length()));
		}

		return null;
	}

	/**
	 * Default constructor
	 */
	public FeatureDefinition() {
		this.properties = new TreeMap<String, Property>();
		this.flags = new TreeMap<String, Flag>();
		this.sources = new TreeMap<String, Source>();
	}

	/**
	 * Constructor
	 * 
	 * @param	featureId		the feature ID 
	 */
	public FeatureDefinition(String featureId) {
		this();
		this.id = featureId;
	}

	/**
	 * Retrieves this feature's ID.
	 * 
	 * @return	the feature ID
	 */
	public String getId() {
		return id;
	}

	/**
	 * Sets this feature's ID.
	 * 
	 * @param 	id			the feature ID
	 */
	public void setId(String id) {
		this.id = id;
	}

	/**
	 * Retrieves this feature's description.
	 * 
	 * @return	the feature description
	 */
	public String getDescription() {
		return description;
	}

	/**
	 * Sets this feature's description.
	 * 
	 * @param 	description		the description
	 */
	public void setDescription(String description) {
		this.description = description;
	}

	/**
	 * Retrieves this feature's name.
	 * 
	 * @return	the feature name
	 */
	public String getName() {
		return name;
	}

	/**
	 * Sets this feature's name.
	 * 
	 * @param 	name		the feature name
	 */
	public void setName(String name) {
		this.name = name;
		complete = true;
	}

	/**
	 * Sets the runtime to which this feature definition belongs.
	 * 
	 * @param runtime		the parent runtime
	 */
	public void setRuntime(String runtime) {
		this.runtime = runtime;
	}

	/**
	 * Returns the runtime to which this feature definition belongs.
	 * 
	 * @return the runtime
	 */
	public String getRuntime() {
		return runtime;
	}

	/**
	 * Adds the given {@link Property} to this {@link FeatureDefinition}.
	 * 
	 * @param 	property		new property.
	 */
	public void addProperty(Property property) {
		properties.put(property.getName(), property);
	}

	/**
	 * Adds the given properties to this {@link FeatureDefinition}.
	 * 
	 * @param 	propertyNames		new property names
	 */
	public void addAllProperties(Set<String> propertyNames) {
		for (String name : propertyNames) {
			properties.put(name, new Property(name, null));
		}
	}

	/**
	 * Removes the given property from this {@link FeatureDefinition}.
	 * 
	 * @param 	property		the property to be removed
	 */
	public void removeProperty(Property property) {
		properties.remove(property.getName());
	}

	/**
	 * Removes the property with the given name from this {@link FeatureDefinition}.
	 * 
	 * @param 	propertyName	the name of the property to be removed.
	 */
	public void removeProperty(String propertyName) {
		properties.remove(propertyName);
	}

	/**
	 * Determines if this {@link FeatureDefinition} has a property with the given name.
	 * 
	 * @param 	propertyName	the name of the property to check for
	 * @return	<code>true</code> if this definitions has a property with the given name, <code>false</code> otherwise
	 */
	public boolean hasProperty(String propertyName) {
		return properties.containsKey(propertyName);
	}

	/**
	 * Retrieves the property with the given name.
	 * 
	 * @param 	propertyName	the name of the property to be retrieved
	 * @return	the property retrieves or null if no property with that name exists
	 */
	public Property getProperty(String propertyName) {
		return properties.get(propertyName);
	}

	/**
	 * Retrieves all the properties from this {@link FeatureDefinition}.
	 * 
	 * @return	the properties
	 */
	public Map<String, Property> getProperties() {
		return Collections.unmodifiableMap(properties);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#addFlag(com.ibm.j9tools.om.Flag)
	 */
	public void addFlag(Flag flag) {
		flags.put(flag.getId(), flag);
	}

	/**
	 * Adds the given {@link Flag} map to this {@link FeatureDefinition}.
	 * 
	 * @param 	flags		the flags to be added
	 */
	public void addAllFlags(Map<String, Flag> flags) {
		this.flags.putAll(flags);
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
	 * @see com.ibm.j9tools.om.IFlagContainer#hasFlag(java.lang.String)
	 */
	public boolean hasFlag(String flagId) {
		return hasLocalFlag(flagId);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#hasFlag(java.lang.String)
	 */
	public boolean hasLocalFlag(String flagId) {
		return flags.containsKey(flagId);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#getFlag(java.lang.String)
	 */
	public Flag getFlag(String flagId) {
		return getLocalFlag(flagId);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#getFlag(java.lang.String)
	 */
	public Flag getLocalFlag(String flagId) {
		return flags.get(flagId);
	}

	/**
	 * @see com.ibm.j9tools.om.IFlagContainer#getFlags()
	 */
	public Map<String, Flag> getFlags() {
		return getLocalFlags();
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
	 * @see com.ibm.j9tools.om.IFlagContainer#getFlags()
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
	 * Adds the given source to this feature.
	 * 
	 * @param	source		the source to be added
	 */
	public void addSource(Source source) {
		sources.put(source.getId(), source);
	}

	/**
	 * Adds the given sources to this {@link FeatureDefinition}.
	 * 
	 * @param 	sourceNames		new source names
	 */
	public void addAllSources(Set<String> sourceNames) {
		for (String name : sourceNames) {
			sources.put(name, new Source(name));
		}
	}

	/**
	 * Removes the given source from this feature.
	 * 
	 * @param 	source		the source to be removed
	 */
	public void removeSource(Source source) {
		sources.remove(source.getId());
	}

	/**
	 * Removes the source with the given ID from this feature.
	 * 
	 * @param 	sourceId	the source ID 
	 */
	public void removeSource(String sourceId) {
		sources.remove(sourceId);
	}

	/**
	 * Determines if the given source is defined in this feature.
	 * 
	 * @param 	sourceId	the source ID
	 * @return	<code>true</code> if this feature contains the source, <code>false</code> otherwise
	 */
	public boolean hasSource(String sourceId) {
		return hasLocalSource(sourceId);
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
	 * @see com.ibm.j9tools.om.ISourceContainer#getSource(java.lang.String)
	 */
	public Source getSource(String sourceId) {
		return getLocalSource(sourceId);
	}

	/**
	 * Retrieves the sources with the given ID.
	 * 
	 * @param 	sourceId	the source ID
	 * @return	the source with the given ID or null if the source in not part of this feature
	 */
	public Source getLocalSource(String sourceId) {
		return sources.get(sourceId);
	}

	/**
	 * Retrieves all the sources for this feature.
	 * 
	 * @return	the {@link Map} of sources
	 */
	public Map<String, Source> getSources() {
		return getLocalSources();
	}

	/**
	 * @see com.ibm.j9tools.om.ISourceContainer#getLocalSources()
	 */
	public Map<String, Source> getLocalSources() {
		return Collections.unmodifiableMap(sources);
	}

	/**
	 * Determines if this {@link FeatureDefinition} is fully defined.
	 * 
	 * @return	returns true if all elements are defined
	 */
	public boolean isComplete() {
		return complete;
	}

	/**
	 * Verifies the validity of this feature by checking its sources and flags.  Sources are verified by a
	 * {@link SourceVerifier} and flags by a {@link FlagVerifier}.
	 * 
	 * @throws InvalidFeatureDefinitionException	thrown when source of flag errors are found
	 */
	public void verify(FlagDefinitions flagDefinitions, Set<String> sources) throws InvalidFeatureDefinitionException {
		Collection<Throwable> errors = new HashSet<Throwable>();

		SourceVerifier sourceVerifier = new SourceVerifier(this, sources);
		errors.addAll(sourceVerifier.verify());

		FlagVerifier flagVerifier = new FlagVerifier(this, flagDefinitions);
		errors.addAll(flagVerifier.verify());

		if (errors.size() > 0) {
			throw new InvalidFeatureDefinitionException(errors, this);
		}
	}

	/**
	 * Debug helper used to dump this feature's member variables and lists
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

		out.println(indent + "Feature "); //$NON-NLS-1$
		out.println(indent + " +--- id:              " + this.getId()); //$NON-NLS-1$
		out.println(indent + " |--- name:            " + this.getName()); //$NON-NLS-1$
		out.println(indent + " |--- description:     " + this.getDescription()); //$NON-NLS-1$

		out.println(indent + " |"); //$NON-NLS-1$
		out.println(indent + " |--- Flags        "); //$NON-NLS-1$
		for (String flagId : flags.keySet()) {
			Flag f = flags.get(flagId);
			out.println(indent + " |    |-- Flag"); //$NON-NLS-1$
			out.println(indent + " |    |     +-- id:      " + f.getId()); //$NON-NLS-1$
			out.println(indent + " |    |     +-- state:   " + f.getState()); //$NON-NLS-1$
		}

		out.println(indent + " |"); //$NON-NLS-1$
		out.println(indent + " |--- Sources      "); //$NON-NLS-1$
		for (String sourceId : sources.keySet()) {
			Source s = sources.get(sourceId);
			out.println(indent + " |    |-- Source"); //$NON-NLS-1$
			out.println(indent + " |    |     +-- id:      " + s.getId()); //$NON-NLS-1$
		}
	}

}
