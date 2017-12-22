/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.store.test;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

import javax.imageio.stream.ImageInputStream;

import org.junit.Test;

import com.ibm.j9ddr.tools.store.J9DDRStructureStore;
import com.ibm.j9ddr.tools.store.StructureKey;
import com.ibm.j9ddr.tools.store.StructureMismatchError;

public class J9DDRStructureStoreTest {

	@Test(expected = IllegalArgumentException.class) 
	public void nullConstuctor() {
		try {
			new J9DDRStructureStore(null, null);
		} catch (IOException e) {
			fail("Invalid exception");
		}
	}
	
	@Test
	public void testOpenNonStoreDirectory() {
		// Make a directory
		
		File dir = new File("bogusDir");
		boolean result = false;
		dir.delete();
		assertTrue("Could not set up test", dir.mkdir());

		try {
			new J9DDRStructureStore("bogusDir", "bogusSupersetFilename");
		} catch (IOException e) {
			result = true;
		}
		
		assertTrue("Should have thrown IOException when opening on non-store directory", result);
		
		assertTrue("Could not tear down test", dir.delete());
	}
	
	@Test
	public void add() {
		
		// Test
		J9DDRStructureStore store = null;
		try {
			store = new J9DDRStructureStore("newDir", "superset.dat");			
		} catch (IOException e) {
			e.printStackTrace();
			fail("Could not open store");
		}
		
		String original = "E:\\workspaces\\j9ddr\\DDR_VM\\data\\structure blobs\\j9ddr.dat";
		String newOne = "E:\\workspaces\\j9ddr\\DDR_VM\\data\\structure blobs\\newj9ddr.dat";
		
		StructureKey keyOne = new StructureKey("windows", "foo");
		StructureKey keyTwo = new StructureKey("windows", "bar");
		
//		String structureFileName = original;
//		StructureKey key = keyOne;

		String structureFileName = newOne;
		StructureKey key = keyTwo;

		try {
			store.add(key, structureFileName, true);
			store.close();
		} catch (IOException e) {
			e.printStackTrace();
			fail(e.getMessage());
		} catch (StructureMismatchError e) {
			e.printStackTrace();
			fail(e.getMessage());
		} finally {
			try {
				store.close();
			} catch (IOException e) {
				fail("Could not close store");
			}
		}
		
		// TODO: Open file and make sure it is good.
		try {
			store.close();
			store = null;
			store = new J9DDRStructureStore("newDir", "superset.dat");
			ImageInputStream is = store.get(key);
			FileInputStream fis = new FileInputStream(structureFileName);
			assertTrue("Files where not the same", compare(is, fis));
		} catch (IOException e) {
			fail();
		}
	}

	private boolean compare(ImageInputStream is1, InputStream is2) throws IOException {
		byte[] buffer = new byte[1024];
		
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		while (true) {
			int bytesRead = is1.read(buffer);
			if (bytesRead == -1) {
				break;
			}
			baos.write(buffer, 0, bytesRead);
		}
		
		byte[] stream1 = baos.toByteArray();
		
		baos = new ByteArrayOutputStream();
		while (true) {
			int bytesRead = is2.read(buffer);
			if (bytesRead == -1) {
				break;
			}
			baos.write(buffer, 0, bytesRead);
		}
		
		byte[] stream2 = baos.toByteArray();
		
		return Arrays.equals(stream1, stream2);
	}
}
