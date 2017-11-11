/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.jvm.trace;

import java.util.*;

/**
 * @author Simon Rowland
 *
 */
public interface TraceThread {
	/**
	 * @return a TracePoint Iterator that can be used to walk each TracePoint on the
	 * current TraceThread in chronological order. Note that the Iterator consumes data
	 * as it walks, and as such each TraceThread can be Iterated over once only. 
	 * Subsequent attempts to Iterate will return an empty Iterator, as will an attempt
	 * to iterate over an unpopulated TraceThread.
	 */
	public Iterator getChronologicalTracePointIterator();
	/**
	 * @return The name of the TraceThread, or null if the Thread was unnamed.
	 */
	public String getThreadName();
	/**
	 * @return the ID of the thread that generated the current TraceThread's data.
	 */
	public long getThreadID();
}
