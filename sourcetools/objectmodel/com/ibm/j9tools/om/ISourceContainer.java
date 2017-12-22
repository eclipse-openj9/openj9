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

public interface ISourceContainer {
	public String getId();
	
	/**
	 * Retrieves the set of sources defined for in container.
	 * 
	 * @return	a {@link Map} of all the sources
	 */
	public Map<String, Source> getSources();

	/**
	 * Retrieves all the local sources for this container.  This does not include sources
	 * defined in included features.
	 * 
	 * @return	a {@link Map} of the local sources
	 */
	public Map<String, Source> getLocalSources();

	/**
	 * Retrieves a source module definition
	 * 
	 * @param 	sourceId 	The ID of the source module
	 * @return 	the requested source
	 */
	public Source getSource(String sourceId);

	/**
	 * Retrieves a source module definition that is local to this container
	 * 
	 * @param 	sourceId 	The ID of the source module
	 * @return 	the requested source
	 */
	public Source getLocalSource(String sourceId);

	/**
	 * Adds the given source to this container.
	 * 
	 * @param 	source		the source to be added
	 */
	public void addSource(Source source);

	/**
	 * Removes the given source from this container.
	 * 
	 * @param 	source		the source to be removed
	 */
	public void removeSource(Source source);

	/**
	 * Removes the source with the given source ID from this container.
	 * 
	 * @param	sourceId	the ID of the source to be removed
	 */
	public void removeSource(String sourceId);
	
	/**
	 * Checks for existence of a source.
	 * 
	 * @param 	sourceId	ID of the source to check for
	 * @return	<code>true</code> if the source is included for this container, <code>false</code> otherwise
	 */
	public boolean hasSource(String sourceId);
	
	/**
	 * Checks for existence of a local source.
	 * 
	 * @param 	sourceId	ID of the source to check for
	 * @return	<code>true</code> if the source is included for this container, <code>false</code> otherwise
	 */
	public boolean hasLocalSource(String sourceId);
}
