/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd.util;

import java.util.*;

/**
 * This interface provides an enumeration of long values. As well as the methods provided
 * by its superclass it also offers a {@link #nextLong} method to get the value as a primitive thus
 * avoiding the overhead of an object creation. In addition it provides an optional
 * {@link #numberOfElements} method which returns the total number of elements in the enumeration.
 * If this optional method is supported then the provider will return <tt>true</tt> for the
 * {@link #hasNumberOfElements} method.
 */

public interface LongEnumeration extends Enumeration {
	/**
	 * Returns the next long value.
	 */
	public long nextLong();
	/**
	 * Returns <tt>true</tt> if this enumeration supports the {@link #numberOfElements} method.
	 */
	public boolean hasNumberOfElements();
	/**
	 * Returns the total number of elements in this enumeration (not the remaining number!). Users
	 * of this method should check that {@link #hasNumberOfElements} returns true.
	 */
	public int numberOfElements();
}

