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

import static org.openj9.test.attachAPI.TestConstants.TARGET_VM_CLASS;
import static org.openj9.test.util.StringUtilities.searchSubstring;
import static org.testng.Assert.assertNotNull;
import static org.testng.Assert.assertTrue;
import static org.testng.Assert.fail;
import static org.testng.AssertJUnit.assertTrue;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;
import java.util.Properties;
import java.util.stream.Collectors;

import org.openj9.test.util.HelloWorld;
import org.testng.annotations.AfterTest;
import org.testng.annotations.BeforeSuite;
import org.testng.annotations.Test;

import com.ibm.tools.attach.attacher.OpenJ9VirtualMachine;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;


/**
 * Test the diagnostic commands which can be invoked via jcmd.
 *
 */
@Test(groups = { "level.extended" })
public class TestJmap extends AttachApiTest {

	private static final String JMAP_COMMAND = "jmap"; //$NON-NLS-1$
	private static String myId;
	
	/* test objects */
	private ArrayList<HelloWorld> hws;
	private int primitiveArray[][];
	private long primitiveVector[];
	private String objectArray[][];
	private Integer objectVector[];
	private Finalizable myFinalizable;

	@Test
	public void testHeapStatsBean() {
		MemoryMXBean genericBean = ManagementFactory.getMemoryMXBean();
		if (!(genericBean instanceof com.ibm.lang.management.MemoryMXBean)) {
			fail("Incompatible JVM: getMemoryMXBean() did not return " + //$NON-NLS-1$
					com.ibm.lang.management.MemoryMXBean.class.getName());
			return;
		}
		com.ibm.lang.management.MemoryMXBean memBean = (com.ibm.lang.management.MemoryMXBean) genericBean;	

		String heapStatsString = memBean.getHeapClassStatistics();
		List<String> heapStats = Arrays.asList(heapStatsString
				.split(System.lineSeparator())
				);
		checkHeapStats(heapStats);
	}

	@Test
	public void testHisto() throws IOException {
		List<String> jmapOutput = runCommand(Arrays.asList(myId, "-histo"));  //$NON-NLS-1$
		logJmapOutput(jmapOutput);
		checkHeapStats(jmapOutput);
	}

	@Test
	public void testHistoLive() throws IOException {
		List<String> jmapOutput = runCommand(Arrays.asList(myId, "-histo:live")); //$NON-NLS-1$
		logJmapOutput(jmapOutput);
		checkHeapStats(jmapOutput);
	}

	@Test
	public void testHeapHisto() throws AttachNotSupportedException, IOException {
		TargetManager tgtMgr = new TargetManager(TARGET_VM_CLASS, null);
		assertTrue(AttachApiTest.CHILD_PROCESS_DID_NOT_LAUNCH, tgtMgr.syncWithTarget());
		VirtualMachine vm = VirtualMachine.attach(tgtMgr.targetId);
		assertTrue(vm instanceof OpenJ9VirtualMachine, "Wrong attach API VirtualMachine"); //$NON-NLS-1$
		OpenJ9VirtualMachine myVm = (OpenJ9VirtualMachine) vm;
		InputStream result = myVm.heapHisto();
		checkForProperties(result);
		result = myVm.heapHisto("-live"); //$NON-NLS-1$
		checkForProperties(result);
		result = myVm.heapHisto("-all"); //$NON-NLS-1$
		checkForProperties(result);
	}

	@Test
	public void testHeap() throws IOException {
		List<String> jmapOutput = runCommand(Arrays.asList("-heap", myId));  //$NON-NLS-1$
		logJmapOutput(jmapOutput);
		String usage = "Heap Memory Usage"; //$NON-NLS-1$
		Optional<String> searchResult = searchSubstring(usage, jmapOutput);
		assertTrue(searchResult.isPresent(), usage + " missing"); //$NON-NLS-1$
		String gcmode = "Garbage collector mode"; //$NON-NLS-1$
		searchResult = searchSubstring(gcmode, jmapOutput);
		assertTrue(searchResult.isPresent(), gcmode + " missing"); //$NON-NLS-1$
	}

	@Test
	public void testFinalizer() throws IOException {
		List<String> jmapOutput = runCommand(Arrays.asList("-finalizerinfo", myId));  //$NON-NLS-1$
		log(myFinalizable.toString()); /* ensure the object is alive */
		myFinalizable = null; /* make it finalizable */
		logJmapOutput(jmapOutput);
		Optional<String> searchResult = searchSubstring("Approximate number of finalizable objects:", jmapOutput); //$NON-NLS-1$
		assertTrue(searchResult.isPresent(), "Finalizer info missing missing"); //$NON-NLS-1$
	}

	@BeforeSuite
	protected void setupSuite() {
		assertTrue("This test is valid only on OpenJ9", //$NON-NLS-1$
				System.getProperty("java.vm.vendor").contains("OpenJ9")); //$NON-NLS-1$//$NON-NLS-2$
		getJdkUtilityPath(JMAP_COMMAND);
		myId = TargetManager.getVmId();
		assertNotNull(myId, "Attach ID missing"); //$NON-NLS-1$
		log(myId);
		
		/* make test objects */
		hws = new ArrayList<>();
		for (int i = 0; i < 3; i++) {
			hws.add(new HelloWorld());
		}
		objectArray = new String[][] {{"a", "b"}, {"c", "d"}};   //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$//$NON-NLS-4$
		objectVector = new Integer[] {Integer.valueOf(10), Integer.valueOf(20)};
		primitiveArray = new int[][] {{1, 2}, {3, 4}};
		primitiveVector = new long[] {5, 6};
		myFinalizable = new Finalizable();
	}

	private void checkHeapStats(List<String> heapStats) {
		String hwClassName = HelloWorld.class.getName();
		String expectedClasses[] = new String[] {hwClassName, 
				primitiveArray.getClass().getName(),
				primitiveVector.getClass().getName(),
				objectArray.getClass().getName(),
				objectVector.getClass().getName(),
		};
		for (String className: expectedClasses) {
			Optional<String> result = searchSubstring(className, heapStats);
			assertTrue(result.isPresent(), "stats missing for " + className); //$NON-NLS-1$
		}
		Optional<String> result = searchSubstring(hwClassName, heapStats);
		String stringResult = result.get();
		assertTrue(stringResult.matches(".*\\b3\\b.*"), "wrong number of HelloWorld objects: "+stringResult);  //$NON-NLS-1$//$NON-NLS-2$
	}

	private void checkForProperties(InputStream result) throws IOException {
		try (BufferedReader jpsOutReader = new BufferedReader(new InputStreamReader(result))) {
			List<String> outputLines = jpsOutReader.lines().collect(Collectors.toList());
			String propertiesName = Properties.class.getName();
			assertTrue(searchSubstring(propertiesName, outputLines).isPresent(), "Missing class: " + propertiesName); //$NON-NLS-1$
		}
	}

	@AfterTest
	private void preserveHelloWorld() {
		/* force the objects to stay alive */
		for (HelloWorld instance: hws) {
			log(instance.getClass().getName());
		}
		log(Integer.toString(objectArray.hashCode()));
		log(Integer.toString(objectVector.hashCode()));
		log(Integer.toString(primitiveArray.hashCode()));
		log(Integer.toString(primitiveVector.hashCode()));
	}

	private void logJmapOutput(List<String> jmapOutput) {
		StringBuilder buff = new StringBuilder("jmap output:\n"); //$NON-NLS-1$
		jmapOutput.forEach(s -> {
			buff.append(s);
			buff.append("\n"); //$NON-NLS-1$
		});
		log(buff.toString());
	}

	static class Finalizable {
		@Override
		protected void finalize() throws Throwable {
			super.finalize();
		}
	}

}
