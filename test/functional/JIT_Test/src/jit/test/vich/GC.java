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

package jit.test.vich;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jit.test.vich.utils.Timer;

public class GC {

private static Logger logger = Logger.getLogger(GC.class);
Timer timer;

public GC() {
	timer = new Timer ();
}

public int hashContents (Object o[]) {
	
	
	int hash, loop;
	
	hash = 0; 
	for (loop = 0; loop < o.length; loop++) {
		hash = hash + o[loop].hashCode();
	}	 

	return hash;
}
@Test(groups = { "level.sanity","component.jit" })
public void testGC() {
	Object o[];
	int i, loop;
	o = new Object[50000];
	int pad = 0;
	for (loop = 1; loop <= 10; loop++) {
		timer.reset();
		for (i = 0; i < o.length; i++) {
			o[i] = new Object[loop];
		}
		for (i = 0; i < o.length; i += 2) {
			o[i] = new Object[loop + pad];
		}
		pad = 1 - pad;
		timer.mark();
		hashContents(o); // defeat optimization 
		logger.info("Pass " + Integer.toString(loop) + ":GC 100000 * new Object (), every 2nd object trash = "+ Long.toString(timer.delta()));
	}
	return;
}
}
