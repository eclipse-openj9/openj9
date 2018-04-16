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

public class EqualsImplementations {
	private static Logger logger = Logger.getLogger(EqualsImplementations.class);
	Timer timer;
	int field = 99;

public EqualsImplementations() {
	timer = new Timer ();
}
/**
 * @param args java.lang.String[]
 */
public boolean equalsClassMatch(EqualsImplementations o) {
	if (o == null) return false;
	return field == o.field;
} 
/**
 * @param args java.lang.String[]
 */
public boolean equalsClassMatch2(EqualsImplementations o) {
	try {
		return field == o.field;
	} catch (Exception e) {
		return false;
	}
}
/**
 * @param args java.lang.String[]
 */
public boolean equalsObject(Object obj) {
	return ((obj != null) && (obj instanceof EqualsImplementations) && (field == (((EqualsImplementations) obj).field)));
	 
} 
/**
 * @param args java.lang.String[]
 */
public boolean equalsObject2(Object obj) {
	try {
		return field == ((EqualsImplementations) obj).field;
	} catch (Exception e) {
		return false;
	}
}
/**
 * @param args java.lang.String[]
 */
@Test(groups = { "level.sanity","component.jit" })
public void testEqualsImplementations() {
	int count = 500000;
	
	timer.reset();
	for (int i = 0; i < count; i++) {
		equalsObject(this);
	}
	timer.mark();
	logger.info(count + " times equalsObject = " + Long.toString(timer.delta()));
	
	timer.reset();
	for (int i = 0; i < count; i++) {
		equalsObject2(this);
	}
	timer.mark();
	logger.info(count + " times equalsObject2 = " + Long.toString(timer.delta()));
	
	timer.reset();
	for (int i = 0; i < count; i++) {
		equalsClassMatch(this);
	}
	timer.mark();
	logger.info(count + " times equalsClassMatch = " + Long.toString(timer.delta()));
	
	timer.reset();
	for (int i = 0; i < count; i++) {
		equalsClassMatch2(this);
	}
	timer.mark();
	logger.info(count + " times equalsClassMatch2 = " + Long.toString(timer.delta()));
}
}
