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

/**
 * An interface supported by any object that contains flags.
 */
public interface IFlagContainer {

	/**
	 * Adds a flag to this flag container. Typically this only affect local flags.
	 * 
	 * @param 	flag 		An initialized instance of a Flag
	 */
	public void addFlag(Flag flag);

	/**
	 * Removes the given flag from this container. Typically this only affect local flags.
	 * 
	 * @param 	flag		the flag to be removed
	 */
	public void removeFlag(Flag flag);
	
	/**
	 * Removes the flag with the given ID from this container. Typically this only affect local flags.
	 * 
	 * @param 	flagId		the flag ID
	 */
	public void removeFlag(String flagId);

	/**
	 * Checks for existence of a flag. Not to be confused with the value of a flag.
	 * 
	 * @param 	flagId 		ID of the flag to check for
	 * @return 	True if a flag is defined for this container, false otherwise
	 */
	public boolean hasFlag(String flagId);
	
	/**
	 * Checks for existence of a local flag. Not to be confused with the value of a flag.
	 * 
	 * @param 	flagId 		ID of the local flag to check for
	 * @return 	True if a flag is local to this container, false otherwise
	 */
	public boolean hasLocalFlag(String flagId);
	
	/**
	 * Retrieves the requested flag.
	 * 
	 * @param 	flagId		the ID of the requested flag
	 * @return	the requested flag
	 */
	public Flag getFlag(String flagId);
	
	/**
	 * Retrieves the requested local flag.  This would not return any flags
	 * inherited from another {@link IFlagContainer}.
	 * 
	 * @param 	flagId		the ID of the requested flag
	 * @return	the requested flag
	 */
	public Flag getLocalFlag(String flagId);
	
	/**
	 * Retrieves all this container's flags.
	 * 
	 * @return	a <code>Map</code> of the container's flags, keyed by ID
	 */
	public Map<String, Flag> getFlags();
	
	/**
	 * Retrieves all this container's flags for the given category.
	 * 
	 * @param 	category	the category for which to retrieve flags
	 * @return	a <code>Map</code> of the container's flags, keyed by ID
	 */
	public Map<String, Flag> getFlags(String category);
	
	/**
	 * Retrieves all the flags that are local to this container
	 * 
	 * @return	a <code>Map</code> of the container's flags, keyed by ID
	 */
	public Map<String, Flag> getLocalFlags();
	
	/**
	 * Retrieves all the flags that are local to this container for the given category.
	 * 
	 * @param 	category	the category for which to retrieve flags
	 * @return	a <code>Map</code> of the container's flags, keyed by ID
	 */
	public Map<String, Flag> getLocalFlags(String category);
}
