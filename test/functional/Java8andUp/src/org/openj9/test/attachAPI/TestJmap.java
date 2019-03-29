/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.openj9.test.attachAPI;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertNotNull;
import static org.testng.Assert.assertTrue;
import static org.testng.Assert.fail;
import static org.testng.AssertJUnit.assertTrue;

import java.io.IOException;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

import org.openj9.test.util.HelloWorld;
import org.testng.annotations.BeforeSuite;
import org.testng.annotations.Test;

import openj9.tools.attach.diagnostics.base.HeapClassInformation;

/**
 * Test the diagnostic commands which can be invoked via jcmd.
 * If there is a standalone command, exercise that.
 *
 */
@Test(groups = { "level.extended" })
public class TestJmap extends AttachApiTest {

	private static final String JMAP_COMMAND = "jmap"; //$NON-NLS-1$
	private String hwClassName = HelloWorld.class.getName();
	private static String myId;
	@Test
	public void testHeapStatsBean() {
		MemoryMXBean genericBean = ManagementFactory.getMemoryMXBean();
		if (!(genericBean instanceof com.ibm.lang.management.MemoryMXBean)) {
			fail("Incompatible JVM: getMemoryMXBean() did not return  com.ibm.lang.management.MemoryMXBean"); //$NON-NLS-1$
			return;
		}
		com.ibm.lang.management.MemoryMXBean memBean = (com.ibm.lang.management.MemoryMXBean) genericBean;	

		ArrayList<HelloWorld> hws = new ArrayList<>();
		for (int i = 0; i < 3; i++) {
			hws.add(new HelloWorld());
		}
		HeapClassInformation[] heapStats = memBean.getHeapClassStatistics();
		Map<String, HeapClassInformation> statsMap = new HashMap<>(heapStats.length);
		for (HeapClassInformation hci: heapStats) {
			log(hci.toString());
			String heapClassName = hci.getHeapClassName();
			assertFalse(statsMap.containsKey(heapClassName), "Duplicate class " + heapClassName); //$NON-NLS-1$
			statsMap.put(heapClassName, hci);
		}
		
		HeapClassInformation hci = statsMap.get(hwClassName);
		assertNotNull(hci, "HelloWorld stats missing"); //$NON-NLS-1$
		assertEquals(hci.getObjectCount(), hws.size(), "wrong number of HelloWorld objects"); //$NON-NLS-1$
		long objectSize = hci.getObjectSize();
		assertTrue(objectSize >= 16, "HelloWorld objects are too small: " + objectSize); //$NON-NLS-1$
	}
	
	@Test
	public void testHisto() throws IOException {
		ArrayList<HelloWorld> hws = new ArrayList<>();
		for (int i = 0; i < 3; i++) {
			hws.add(new HelloWorld());
		}
		List<String> jmapOutput = runCommand(Arrays.asList(myId, "-histo"));  //$NON-NLS-1$
		logJmapOutput(jmapOutput);
		Optional<String> searchResult = search(hwClassName, jmapOutput);
		assertTrue(searchResult.isPresent(), hwClassName + " missing"); //$NON-NLS-1$
		log(hws.get(0).getClass().getName()); /* force the object to stay alive */
	}

	@Test
	public void testHistoLive() throws IOException {
		ArrayList<HelloWorld> hws = new ArrayList<>();
		for (int i = 0; i < 3; i++) {
			hws.add(new HelloWorld());
		}
		List<String> jmapOutput = runCommand(Arrays.asList(myId, "-histo:live")); //$NON-NLS-1$
		logJmapOutput(jmapOutput);
		Optional<String> searchResult = search(hwClassName, jmapOutput);
		assertTrue(searchResult.isPresent(), "Class name missing"); //$NON-NLS-1$
		log(hws.get(0).getClass().getName()); /* force the object to stay alive */
		
		hws = null;
		jmapOutput = runCommand(Arrays.asList(myId, "-histo:live")); //$NON-NLS-1$
		logJmapOutput(jmapOutput);

		searchResult = search(hwClassName, jmapOutput);
		assertTrue(!searchResult.isPresent(), "Dead objects reported"); //$NON-NLS-1$

	}

	@Test
	public void testHeap() throws IOException {
		List<String> jmapOutput = runCommand(Arrays.asList("-heap", myId));  //$NON-NLS-1$
		logJmapOutput(jmapOutput);
		Optional<String> searchResult = search("Heap Memory Usage", jmapOutput); //$NON-NLS-1$
		assertTrue(searchResult.isPresent(), hwClassName + " missing"); //$NON-NLS-1$
		searchResult = search("Garbage collector mode", jmapOutput); //$NON-NLS-1$
		assertTrue(searchResult.isPresent(), hwClassName + " missing"); //$NON-NLS-1$
	}

	@BeforeSuite
	protected void setupSuite() {
		assertTrue("This test is valid only on OpenJ9", //$NON-NLS-1$
				System.getProperty("java.vm.vendor").contains("OpenJ9")); //$NON-NLS-1$//$NON-NLS-2$
		getJdkUtilityPath(JMAP_COMMAND);
		myId = TargetManager.getVmId();
		assertNotNull(myId, "Attach ID missing"); //$NON-NLS-1$
		log(myId);
	}

	private void logJmapOutput(List<String> jmapOutput) {
		StringBuilder buff = new StringBuilder("jmap output:\n"); //$NON-NLS-1$
		jmapOutput.forEach(s -> {
			buff.append(s);
			buff.append("\n"); //$NON-NLS-1$
		});
		log(buff.toString());
	}

	public static void main(String[] args) throws IOException {
		final TestJmap testObj = new TestJmap();
		testObj.setupSuite();
		// testObj.testHeapStatsBean();
		testObj.testHisto();
		testObj.testHistoLive();
		testObj.testHeap();
	}


}
