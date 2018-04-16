/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package jit.test.vich.utils;

public class Timer {
	private long start_time;
	private long elapsed_time;
	private long total_time = 0L;

/**
 * @return long
 */
public long delta ( ) {
	return elapsed_time;
}
public void mark ( ) {
	
	elapsed_time = System.currentTimeMillis () - start_time;
	total_time += elapsed_time;
	return;
}

public void reset ( ) { 
	start_time = System.currentTimeMillis (); 
	return; 
}
/**
 * @return long
 */
public long totalTime ( ) {
	return total_time;
}
}
