/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;
import java.util.Properties;
import java.util.stream.Collectors;

import org.openj9.test.util.HelloWorld;
import org.testng.annotations.AfterTest;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.BeforeSuite;
import org.testng.annotations.Test;

import com.ibm.tools.attach.attacher.OpenJ9VirtualMachine;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;

/**
 * Test the jmap utility.
 */
@Test(groups = { "level.extended" })
public class TestJmap extends AttachApiTest {

	private static final String HELLOWORLD_CLASSNAME = HelloWorld.class.getName();
	private static final String JMAP_COMMAND = "jmap"; //$NON-NLS-1$
	private static String myId;
	
	/* test objects */
	private ArrayList<HelloWorld> hws;
	private int[][] primitiveArray;
	private long[] primitiveVector;
	private String[][] objectArray;
	private Integer[] objectVector;
	private String[] expectedClasses;

	@Test
	public void testHisto() throws IOException {
		List<String> jmapOutput = runCommand(Arrays.asList(myId, "-histo")); //$NON-NLS-1$
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
		InputStream result = myVm.heapHisto("-all"); //$NON-NLS-1$
		checkForProperties(result);
		result = myVm.heapHisto("-live"); //$NON-NLS-1$
		checkForProperties(result);
		result = myVm.heapHisto();
		checkForProperties(result);
		vm.detach();
		int terminationStatus = tgtMgr.terminateTarget();
		log("Target terminated with status" + Integer.toString(terminationStatus)); //$NON-NLS-1$
	}

	@BeforeSuite
	protected void setupSuite() {
		getJdkUtilityPath(JMAP_COMMAND);
		myId = TargetManager.getVmId();
		assertNotNull(myId, "Attach ID missing"); //$NON-NLS-1$
		log(myId);
		
		/* make test objects */
		hws = new ArrayList<>();
		for (int i = 0; i < 3; i++) {
			hws.add(new HelloWorld());
		}
		objectArray = new String[][] {{"a", "b"}, {"c", "d"}}; //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$//$NON-NLS-4$
		objectVector = new Integer[] {Integer.valueOf(10), Integer.valueOf(20)};
		primitiveArray = new int[][] {{1, 2}, {3, 4}};
		primitiveVector = new long[] {5, 6};
		expectedClasses = new String[] {HELLOWORLD_CLASSNAME, 
				primitiveArray.getClass().getName(),
				primitiveVector.getClass().getName(),
				objectArray.getClass().getName(),
				objectVector.getClass().getName(),
		};
	}

	private void checkHeapStats(List<String> heapStats) {
		for (String className : expectedClasses) {
			Optional<String> result = searchSubstring(className, heapStats);
			assertTrue(result.isPresent(), "stats missing for " + className); //$NON-NLS-1$
		}
		Optional<String> result = searchSubstring(HELLOWORLD_CLASSNAME, heapStats);
		assertTrue(result.isPresent(), HELLOWORLD_CLASSNAME + " missing"); //$NON-NLS-1$
		String stringResult = result.get();
		assertTrue(stringResult.matches(".*\\b3\\b.*"), //$NON-NLS-1$
				"wrong number of HelloWorld objects: " + stringResult); //$NON-NLS-1$
	}

	private void checkForProperties(InputStream result) throws IOException {
		try (BufferedReader jpsOutReader = new BufferedReader(new InputStreamReader(result))) {
			List<String> outputLines = jpsOutReader.lines().collect(Collectors.toList());
			String propertiesName = Properties.class.getName();
			if (!searchSubstring(propertiesName, outputLines).isPresent()) {
				log(propertiesName + " not found in the following data: "); //$NON-NLS-1$
				for (String outputLine : outputLines) {
					log(outputLine);
				}
				log("----------------------------------------"); //$NON-NLS-1$

				fail("Missing class: " + propertiesName); //$NON-NLS-1$
			}
		}
	}

	@BeforeMethod
	protected void setUp(Method testMethod) {
		testName = testMethod.getName();
		log("starting " + testName);		 //$NON-NLS-1$
	}
	
	@AfterTest
	private void preserveHelloWorld() {
		/* force the objects to stay alive */
		for (HelloWorld instance : hws) {
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

}
