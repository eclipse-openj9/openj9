/*******************************************************************************
 * Copyright (c) 2007, 2011 IBM Corp. and others
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

import java.util.Map;
import java.util.Set;

/**
 * @author	Gabriel Castro
 * @since	v1.5.0
 */
public class Feature extends FeatureDefinition implements Comparable<Feature> {
	private FeatureDefinition featureDefinition;

	/**
	 * Creates a J9 feature reference with the given id.
	 *
	 * @param 	featureId	the id of the feature definition
	 */
	public Feature(String featureId) {
		this.id = featureId;
	}

	/**
	 * Creates a J9 feature reference from the given {@link FeatureDefinition}.  The feature ID
	 * is inherited from the {@link FeatureDefinition}.
	 *
	 * @param 	featureDefinition	the parent definition
	 */
	public Feature(FeatureDefinition featureDefinition) {
		this(featureDefinition.getId());
		this.setDefinition(featureDefinition);
	}

	/**
	 * Sets this feature's parent {@link FeatureDefinition}.
	 *
	 * @param 	featureDefinition	the parent definition
	 */
	public void setDefinition(FeatureDefinition featureDefinition) {
		this.featureDefinition = featureDefinition;
		this.complete = true;
	}

	/**
	 * Removes this feature's parent {@link FeatureDefinition}.
	 */
	public void removeDefinition() {
		this.featureDefinition = null;
		this.complete = false;
	}

	/**
	 * Does nothing as a {@link Property} cannot be added to a {@link Feature}, only to its
	 * parent {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#addProperty(com.ibm.j9tools.om.Property)
	 */
	@Override
	public void addProperty(Property property) {
	}

	/**
	 * Does nothing as a {@link Property} cannot be added to a {@link Feature}, only to its
	 * parent {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#addAllProperties(java.util.Set)
	 */
	@Override
	public void addAllProperties(Set<String> propertyNames) {
	}

	/**
	 * Does nothing as a {@link Flag} cannot be added to a {@link Feature}, only to its
	 * parent {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#addAllFlags(java.util.Map)
	 */
	@Override
	public void addAllFlags(Map<String, Flag> flags) {
	}

	/**
	 * Does nothing as a {@link Flag} cannot be added to a {@link Feature}, only to its
	 * parent {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#addFlag(com.ibm.j9tools.om.Flag)
	 */
	@Override
	public void addFlag(Flag flag) {
	}

	/**
	 * Does nothing as a {@link Source} cannot be added to a {@link Feature}, only to its
	 * parent {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#addSource(com.ibm.j9tools.om.Source)
	 */
	@Override
	public void addSource(Source source) {
	}

	/**
	 * Retrieves the name of the parent {@link FeatureDefinition} if one is set.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getName()
	 */
	@Override
	public String getName() {
		return (complete) ? featureDefinition.getName() : super.getName();
	}

	/**
	 * Retrieves the description of the parent {@link FeatureDefinition} if one is set.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getDescription()
	 */
	@Override
	public String getDescription() {
		return (complete) ? featureDefinition.getDescription() : super.getDescription();
	}

	/**
	 * Retrieves the given property from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getProperty(java.lang.String)
	 */
	@Override
	public Property getProperty(String propertyName) {
		return (complete) ? featureDefinition.getProperty(propertyName) : super.getProperty(propertyName);
	}

	/**
	 * Retrieves the properties from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getProperties()
	 */
	@Override
	public Map<String, Property> getProperties() {
		return (complete) ? featureDefinition.getProperties() : super.getProperties();
	}

	/**
	 * Retrieves the given flag from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getFlag(java.lang.String)
	 */
	@Override
	public Flag getFlag(String flagId) {
		return (complete) ? featureDefinition.getFlag(flagId) : super.getFlag(flagId);
	}

	/**
	 * Retrieves the flags from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getFlags()
	 */
	@Override
	public Map<String, Flag> getFlags() {
		return (complete) ? featureDefinition.getFlags() : super.getFlags();
	}

	/**
	 * Retrieves the flags with the given category from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getFlags(java.lang.String)
	 */
	@Override
	public Map<String, Flag> getFlags(String category) {
		return (complete) ? featureDefinition.getFlags(category) : super.getFlags(category);
	}

	/**
	 * Retrieves the given local flag from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getLocalFlag(java.lang.String)
	 */
	@Override
	public Flag getLocalFlag(String flagId) {
		return (complete) ? featureDefinition.getLocalFlag(flagId) : super.getLocalFlag(flagId);
	}

	/**
	 * Retrieves the local flags from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getLocalFlags()
	 */
	@Override
	public Map<String, Flag> getLocalFlags() {
		return (complete) ? featureDefinition.getLocalFlags() : super.getLocalFlags();
	}

	/**
	 * Retrieves the local flags with the given category from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getLocalFlags(java.lang.String)
	 */
	@Override
	public Map<String, Flag> getLocalFlags(String category) {
		return (complete) ? featureDefinition.getLocalFlags(category) : super.getLocalFlags(category);
	}

	/**
	 * Retrieves the given local source from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getLocalSource(java.lang.String)
	 */
	@Override
	public Source getLocalSource(String sourceId) {
		return (complete) ? featureDefinition.getLocalSource(sourceId) : super.getLocalSource(sourceId);
	}

	/**
	 * Retrieves the local sources from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getLocalSources()
	 */
	@Override
	public Map<String, Source> getLocalSources() {
		return (complete) ? featureDefinition.getLocalSources() : super.getLocalSources();
	}

	/**
	 * Retrieves the given source from the parent {@link FeatureDefinition} if one is set,
	 * otherwise it returns null;
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#getSource(java.lang.String)
	 */
	@Override
	public Source getSource(String sourceId) {
		return (complete) ? featureDefinition.getSource(sourceId) : super.getSource(sourceId);
	}

	/**
	 * @see com.ibm.j9tools.om.FeatureDefinition#getSources()
	 */
	@Override
	public Map<String, Source> getSources() {
		return (complete) ? featureDefinition.getSources() : super.getSources();
	}

	/**
	 * Determines if the parent {@link FeatureDefinition} has the given property.  If the definition is not
	 * set it returns null.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#hasProperty(java.lang.String)
	 */
	@Override
	public boolean hasProperty(String propertyName) {
		return (complete) ? featureDefinition.hasProperty(propertyName) : super.hasProperty(propertyName);
	}

	/**
	 * Determines if the parent {@link FeatureDefinition} has the given flag.  If the definition is not
	 * set it returns null.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#hasFlag(java.lang.String)
	 */
	@Override
	public boolean hasFlag(String flagId) {
		return (complete) ? featureDefinition.hasFlag(flagId) : super.hasFlag(flagId);
	}

	/**
	 * Determines if the parent {@link FeatureDefinition} has the given local flag.  If the definition is not
	 * set it returns null.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#hasLocalFlag(java.lang.String)
	 */
	@Override
	public boolean hasLocalFlag(String flagId) {
		return (complete) ? featureDefinition.hasLocalFlag(flagId) : super.hasLocalFlag(flagId);
	}

	/**
	 * Determines if the parent {@link FeatureDefinition} has the given source.  If the definition is not
	 * set it returns null.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#hasSource(java.lang.String)
	 */
	@Override
	public boolean hasSource(String sourceId) {
		return (complete) ? featureDefinition.hasSource(sourceId) : super.hasSource(sourceId);
	}

	/**
	 * Determines if the parent {@link FeatureDefinition} has the given local source.  If the definition is not
	 * set it returns null.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#hasLocalSource(java.lang.String)
	 */
	@Override
	public boolean hasLocalSource(String sourceId) {
		return (complete) ? featureDefinition.hasLocalSource(sourceId) : super.hasLocalSource(sourceId);
	}

	/**
	 * Does nothing as a {@link Property} cannot be removed from a {@link Feature}, only from
	 * a {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#removeProperty(com.ibm.j9tools.om.Property)
	 */
	@Override
	public void removeProperty(Property property) {
	}

	/**
	 * Does nothing as a {@link Property} cannot be removed from a {@link Feature}, only from
	 * a {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#removeProperty(java.lang.String)
	 */
	@Override
	public void removeProperty(String propertyName) {
	}

	/**
	 * Does nothing as a {@link Flag} cannot be removed from a {@link Feature}, only from
	 * a {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#removeFlag(com.ibm.j9tools.om.Flag)
	 */
	@Override
	public void removeFlag(Flag flag) {
	}

	/**
	 * Does nothing as a {@link Flag} cannot be removed from a {@link Feature}, only from
	 * a {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#removeFlag(java.lang.String)
	 */
	@Override
	public void removeFlag(String flagId) {
	}

	/**
	 * Does nothing as a {@link Source} cannot be removed from a {@link Feature}, only from
	 * a {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#removeSource(java.lang.String)
	 */
	@Override
	public void removeSource(String sourceId) {
	}

	/**
	 * Does nothing as a {@link Source} cannot be removed from a {@link Feature}, only from
	 * a {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#setDescription(java.lang.String)
	 */
	@Override
	public void setDescription(String description) {
	}

	/**
	 * Does nothing as the name of a {@link Feature} cannot be changed.  The name is
	 * inherited from it's {@link FeatureDefinition}.
	 *
	 * @see com.ibm.j9tools.om.FeatureDefinition#setName(java.lang.String)
	 */
	@Override
	public void setName(String name) {
	}

	/**
	 * @see java.lang.Comparable#compareTo(java.lang.Object)
	 */
	public int compareTo(Feature o) {
		return id.compareToIgnoreCase(o.id);
	}

	/**
	 * @see java.lang.Object#equals(java.lang.Object)
	 */
	@Override
	public boolean equals(Object o) {
		return (o instanceof Feature) ? id.equals(((Feature) o).id) : false;
	}

}
