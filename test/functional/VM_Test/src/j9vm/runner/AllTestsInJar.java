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
package j9vm.runner;
import java.util.*;
import java.util.zip.*;
import java.io.*;

public class AllTestsInJar implements java.util.Enumeration {
	boolean entryReady;
	String testName;
	ZipFile zipFile;
	Enumeration zipFileEntries;

public boolean isValidTestName(String testName)  {
	if (!testName.startsWith("j9vm.test."))  {
		return false;
	}
	if (!testName.endsWith("Test"))  {
		return false;
	}
	return true;
}

public boolean hasMoreElements(){
	if (entryReady)  return true;
	while (!entryReady)  {
		if (!zipFileEntries.hasMoreElements())  {
			return false;
		}
		testName = ((ZipEntry)zipFileEntries.nextElement()).getName();
		if (testName.endsWith(".class"))  {
			testName = testName.substring(0, testName.length() - ".class".length());
			testName = testName.replace('/','.');
			entryReady = isValidTestName(testName);
		}
	}
	return true;
}
public Object nextElement(){
	if (!hasMoreElements())  throw new NoSuchElementException();
	entryReady = false;
	return testName;
}

public AllTestsInJar(String jarFileName) throws IOException {
	entryReady = false;
	testName = null;
	zipFile = new ZipFile(jarFileName);
	zipFileEntries = zipFile.entries();
}

public void finalize()  {
	try  {
		zipFile.close();
	} catch (Exception e)  {
	}
}

}
