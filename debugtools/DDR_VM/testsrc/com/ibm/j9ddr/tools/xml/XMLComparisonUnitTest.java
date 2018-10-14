/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.xml;

import java.util.Iterator;

import org.junit.Assert;
import org.junit.Test;


/**
 * Abstract class from which unit test which compare XML DOMs for equivalence 
 * 
 * @author Adam Pilkington
 *
 */
public abstract class XMLComparisonUnitTest {
	
	/**
	 * The path to the XPath config file which controls the testing
	 * @return
	 */
	public abstract String getConfigPath();
	
	/**
	 * The two XML files to compare
	 * @return
	 */
	public abstract String[] getFilesToCompare();

	@Test
	public void compareXML() {
		XMLDOMComparator comp = new XMLDOMComparator();
		String[] files = getFilesToCompare();
		String[] args = new String[]{"-ddr", files[0], "-jx", files[1], "-f", getConfigPath()};
		comp.parseArgs(args);
		boolean result = comp.compareXML();
		for(Iterator<String> messages = comp.getMessages(); messages.hasNext(); ) {
			System.out.println(messages.next());
		}
		Assert.assertTrue(result);
	}
}
