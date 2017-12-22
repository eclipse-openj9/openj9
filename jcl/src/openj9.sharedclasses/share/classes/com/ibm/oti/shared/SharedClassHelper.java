/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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

/**
 * The SharedClassHelper superinterface defines functions that are common to all class helpers. 
 * <p>
 * @see SharedHelper
 * @see SharedClassHelperFactory
 */ 
public interface SharedClassHelper extends SharedHelper { 
    /* Some functions have been moved to generic SharedHelper interface */

	/**
	 * Applies the sharing filter to the SharedClassHelper.<p>
	 *
	 * If a SecurityManager is installed, this method can only be called 
     * by an object whose caller ClassLoader has shared 
     * class "read,write" permissions.<br>
	 * If a SharedClassFilter is already set, it is replaced by the new filter.<br>
	 * Passing null as the argument removes the sharing filter.
	 * 
	 * @param filter The SharedClassFilter instance or null.
	 */
	public void setSharingFilter(SharedClassFilter filter);
	
	/**
	 * Returns the sharing filter associated with this helper.
	 * <p>
	 * @return The filter instance, or null if non is associated
	 */
	public SharedClassFilter getSharingFilter();
	
}
