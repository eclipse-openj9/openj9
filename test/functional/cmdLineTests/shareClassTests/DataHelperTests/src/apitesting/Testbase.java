/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package apitesting;

import java.io.File;
import java.io.FileInputStream;
import java.util.Properties;
import java.util.StringTokenizer;

import apitesting.GlobalConstants;
import apitesting.TestFailedException;

public class Testbase implements GlobalConstants {

	protected static void fail(String string) {
		System.out.println("TEST FAILED: "+string);
		throw new TestFailedException(string); 
	}

	protected static Properties getProperties(String fname) {
		Properties props = new Properties();
		try{
			FileInputStream PropertiesFile = new FileInputStream(fname);
			props.load(PropertiesFile);
			PropertiesFile.close();
		} catch (Exception e){
			e.printStackTrace();
		}
		return props;
	}

	protected static void log(String msg) {
		System.out.println(msg);
	}

	public void touchFiles(String listForTouching) {
		if (listForTouching.length()==0) return;
		StringTokenizer st = new StringTokenizer(listForTouching,",");
		try { Thread.sleep(1000); } catch (Exception e) {} // allow for filesystem with bad resolution timer
		while (st.hasMoreTokens()) {
			String touch = st.nextToken();
			File f = new File(touch);
			if (!f.exists()) throw new RuntimeException("Can't find: "+touch);
			boolean b = f.setLastModified(System.currentTimeMillis());
			log("Touched: "+f+" : "+(b?"successful":"unsuccessful"));
		}
	}

}
