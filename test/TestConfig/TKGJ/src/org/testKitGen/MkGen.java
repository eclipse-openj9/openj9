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

package org.testKitGen;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

public class MkGen {
	private File playlistXML;
	private List<String> currentdirs;
	private List<String> subdirs;
	private String makeFile;
	private List<TestInfo> testInfoArr;
	private String include;
	private Map<String, ArrayList<String>> groupTargets;

	public MkGen(File playlistXML, String absolutedir, ArrayList<String> currentdirs, List<String> subdirs) {
		this.playlistXML = playlistXML;
		this.currentdirs = currentdirs;
		this.subdirs = subdirs;
		this.makeFile = absolutedir + "/" + Constants.TESTMK;
		this.testInfoArr = new ArrayList<TestInfo>();
		this.groupTargets = new HashMap<String, ArrayList<String>>();
	}

	public void start() {
		System.out.println();
		System.out.println("Generating make file " + makeFile);

		try {
			writeVars();
		} catch (IOException e) {
			e.printStackTrace();
			System.exit(1);
		}

		if (playlistXML != null) {
			try {
				processPlaylist();
				writeTargets();
			} catch (SAXException | IOException | ParserConfigurationException e) {
				e.printStackTrace();
				System.exit(1);
			}
		}
	}

	private void writeVars() throws IOException {
		FileWriter f = new FileWriter(makeFile);
		String realtiveRoot = "";
		int subdirlevel = currentdirs.size();
		if (subdirlevel == 0) {
			realtiveRoot = ".";
		} else {
			for (int i = 1; i <= subdirlevel; i++) {
				realtiveRoot = realtiveRoot + ((i == subdirlevel) ? ".." : "..$(D)");
			}
		}

		f.write(Constants.HEADERCOMMENTS + "\n");
		f.write("D=/\n\n");
		f.write("ifndef TEST_ROOT\n");

		// TODO: remove win condition after testing
		if (Options.getSpec().contains("win")) {
			String projectRootDir = Options.getProjectRootDir().replace("C:", "/cygdrive/c")
					.replace("c:", "/cygdrive/c").replace('\\', '/');
			f.write("\tTEST_ROOT := " + projectRootDir + "\n");
		} else {
			f.write("\tTEST_ROOT := " + Options.getProjectRootDir() + "\n");

		}

		f.write("endif\n\n");
		f.write("SUBDIRS = " + String.join(" ", subdirs) + "\n\n");
		f.write("include $(TEST_ROOT)$(D)TestConfig$(D)" + Constants.SETTINGSMK + "\n\n");
		f.close();
	}

	private void processPlaylist() throws SAXException, IOException, ParserConfigurationException {
		Document xml = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(playlistXML);
		NodeList childNodes = xml.getDocumentElement().getChildNodes();

		for (int i = 0; i < childNodes.getLength(); i++) {
			Node currentNode = childNodes.item(i);

			if (currentNode.getNodeType() == Node.ELEMENT_NODE) {
				Element currentElement = ((Element) currentNode);
				if (currentElement.getNodeName().equals("include")) {
					include = currentElement.getTextContent();
				} else if (currentElement.getNodeName().equals("test")) {
					TestInfo testInfo = new TestInfo(currentElement);
					testInfo.parseInfo();
					if (testInfo.isValid()) {
						testInfoArr.add(testInfo);
					}
				}
			}
		}
	}

	private void writeSingleTest(TestInfo testInfo, FileWriter f) throws IOException {
		for (Variation var : testInfo.getVars()) {
			// Generate make target
			String name = var.getSubTestName();

			// TODO: skip test if it is not valid
			String condition_platform = null;
			if (!var.isValid()) {
				condition_platform = name + "_INVALID_PLATFORM_CHECK";
				f.write(condition_platform + "=$(filter " + Options.getSpec() + ", $(SPEC))\n");
			}

			if (!testInfo.getCapabilities().isEmpty()) {
				List<String> capabilityReqs_HashKeys = new ArrayList<>(testInfo.getCapabilities().keySet());
				Collections.sort(capabilityReqs_HashKeys);
				for (String capa_key : capabilityReqs_HashKeys) {
					String condition_capsReqs = name + "_" + capa_key + "_CHECK";
					f.write(condition_capsReqs + "=$(" + capa_key + ")\n");
				}
			}

			String jvmtestroot = "$(JVM_TEST_ROOT)$(D)" + String.join("$(D)", currentdirs);
			f.write(name + ": TEST_RESROOT=" + jvmtestroot + "\n");

			// TODO: keep for now to match result with perl testkitgen, remove after test
			if (testInfo.getIterations() != Integer.parseInt(Options.getIterations())) {
				f.write(name + ": TEST_ITERATIONS=" + testInfo.getIterations() + "\n");
			}

			f.write(name + ": JVM_OPTIONS?=" + testInfo.getAotOptions() + "$(RESERVED_OPTIONS) "
					+ (var.getJvmOptions().isEmpty() ? "" : (var.getJvmOptions() + " ")) + "$(EXTRA_OPTIONS)\n");

			f.write(name + ": TEST_GROUP=" + testInfo.getLevelStr() + "\n");
			String indent = "\t";
			f.write(name + ":\n");
			f.write(indent + "@echo \"\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
			f.write(indent
					+ "@echo \"===============================================\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
			f.write(indent + "@echo \"Running test $@ ...\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
			f.write(indent
					+ "@echo \"===============================================\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
			f.write(indent + "@perl '-MTime::HiRes=gettimeofday' -e 'print \"" + name
					+ " Start Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"' | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");

			if (testInfo.getDisabledReasons() != null) {
				// This line is also the key words to match runningDisabled
				f.write(indent
						+ "@echo \"Test is disabled due to:\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				for (String dReason : testInfo.getDisabledReasons()) {
					f.write(indent + "@echo \"" + dReason + "\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				}
			}

			if (condition_platform != null) {
				f.write("ifeq ($(" + condition_platform + "),)\n");
			}
			List<String> capKeys = new ArrayList<String>(testInfo.getCapabilities().keySet());
			if (!capKeys.isEmpty()) {
				Collections.sort(capKeys);
				for (String cKey : capKeys) {
					String condition_capsReqs = name + "_" + cKey + "_CHECK";
					f.write("ifeq ($(" + condition_capsReqs + "), " + testInfo.getCapabilities().get(cKey) + ")\n");
				}
			}

			f.write(indent + "$(TEST_SETUP);\n");

			f.write(indent + "@echo \"variation: " + var.getVariation()
					+ "\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
			f.write(indent
					+ "@echo \"JVM_OPTIONS: $(JVM_OPTIONS)\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");

			f.write(indent + "{ ");
			for (int k = 1; k <= testInfo.getIterations(); k++) {
				f.write("itercnt=" + k + "; \\\n" + indent + "$(MKTREE) $(REPORTDIR); \\\n" + indent
						+ "$(CD) $(REPORTDIR); \\\n");
				f.write(indent + testInfo.getCommand() + ";");
				if (k != testInfo.getIterations()) {
					f.write(" \\\n" + indent);
				}
			}
			f.write(" } 2>&1 | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");

			f.write(indent + "$(TEST_TEARDOWN);\n");

			if (!capKeys.isEmpty()) {
				Collections.sort(capKeys, Collections.reverseOrder());
				for (String cKey : capKeys) {
					f.write("else\n");
					f.write(indent + "@echo \"Skipped due to capabilities (" + cKey + ":"
							+ testInfo.getCapabilities().get(cKey)
							+ ") => $(TEST_SKIP_STATUS)\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
					f.write("endif\n");
				}
			}
			if (condition_platform != null) {
				f.write("else\n");
				if (testInfo.getPlatformRequirements() != null) {
					f.write(indent
							+ "@echo \"Skipped due to jvm options ($(JVM_OPTIONS)) and/or platform requirements ("
							+ testInfo.getPlatformRequirements()
							+ ") => $(TEST_SKIP_STATUS)\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				} else {
					f.write(indent
							+ "@echo \"Skipped due to jvm options ($(JVM_OPTIONS)) => $(TEST_SKIP_STATUS)\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				}
				f.write("endif\n");
			}
			f.write(indent + "@perl '-MTime::HiRes=gettimeofday' -e 'print \"" + name
					+ " Finish Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"' | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q)\n");

			f.write("\n.PHONY: " + name + "\n\n");

			if (testInfo.getDisabledReasons() != null) {
				String echoName = "echo.disabled." + name;
				f.write(echoName + ":\n");
				f.write(indent + "@echo \"\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				f.write(indent
						+ "@echo \"===============================================\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				f.write(indent + "@echo \"Running test " + name
						+ " ...\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				f.write(indent
						+ "@echo \"===============================================\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				f.write(indent + "@perl '-MTime::HiRes=gettimeofday' -e 'print \"" + name
						+ " Start Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"' | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				f.write(indent + "@echo \"" + name
						+ "_DISABLED\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				f.write(indent + "@echo \"Disabled Reason:\"\n");

				for (String dReason : testInfo.getDisabledReasons()) {
					f.write(indent + "@echo \"" + dReason + "\" | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q);\n");
				}

				f.write(indent + "@perl '-MTime::HiRes=gettimeofday' -e 'print \"" + name
						+ " Finish Time: \" . localtime() . \" Epoch Time (ms): \" . int (gettimeofday * 1000) . \"\\n\"' | tee -a $(Q)$(TESTOUTPUT)$(D)TestTargetResult$(Q)\n");
				f.write("\n.PHONY: " + echoName + "\n\n");
			}
			updateGroupTargets(testInfo, name);
		}

		String testCaseName = testInfo.getTestCaseName();
		f.write(testCaseName + ":");
		for (Variation var : testInfo.getVars()) {
			f.write(" \\\n" + var.getSubTestName());
		}
		f.write("\n\n.PHONY: " + testCaseName + "\n\n");
	}

	private void updateGroupTargets(TestInfo testInfo, String name) {
		for (int i = 0; i < testInfo.getGroups().size(); i++) {
			String eachGroup = testInfo.getGroups().get(i);
			for (int j = 0; j < testInfo.getLevels().size(); j++) {
				String eachLevel = testInfo.getLevels().get(j);
				for (int k = 0; k < testInfo.getTypes().size(); k++) {
					String eachType = testInfo.getTypes().get(k);
					String lgtKey = eachLevel + "." + eachGroup + "." + eachType;
					if (testInfo.getDisabledReasons() == null) {
						groupTargets.putIfAbsent(lgtKey, new ArrayList<String>());
						groupTargets.get(lgtKey).add(name);
					} else {
						String dlgtKey = "disabled." + lgtKey;
						String echodlgtKey = "echo." + dlgtKey;
						String echoName = "echo.disabled." + name;
						groupTargets.putIfAbsent(dlgtKey, new ArrayList<String>());
						groupTargets.get(dlgtKey).add(name);
						groupTargets.putIfAbsent(echodlgtKey, new ArrayList<String>());
						groupTargets.get(echodlgtKey).add(echoName);
					}
				}
			}
		}
	}

	private void writeTargets() throws IOException {
		FileWriter f = new FileWriter(makeFile, true);
		if (include != null) {
			f.write("-include " + include + "\n\n");
		}

		f.write("include $(TEST_ROOT)$(D)TestConfig$(D)" + Constants.DEPENDMK + "\n\n");

		for (TestInfo testInfo : testInfoArr) {
			writeSingleTest(testInfo, f);
		}

		List<String> allDisHead = Arrays.asList("", "disabled.", "echo.disabled.");

		Constants.ALLLEVELS.sort(null);
		Constants.ALLGROUPS.sort(null);
		Constants.ALLTYPES.sort(null);

		for (String eachDisHead : allDisHead) {
			for (String eachLevel : Constants.ALLLEVELS) {
				for (String eachGroup : Constants.ALLGROUPS) {
					for (String eachType : Constants.ALLTYPES) {
						String hlgtKey = eachDisHead + eachLevel + "." + eachGroup + "." + eachType;

						groupTargets.putIfAbsent(hlgtKey, new ArrayList<String>());
						List<String> groupTests = groupTargets.get(hlgtKey);
						int size = groupTests.size();

						f.write(hlgtKey + ":");
						for (String groupTest : groupTests) {
							f.write(" \\\n" + groupTest);
						}

						Counter.add(eachDisHead + eachLevel, size);
						Counter.add(eachDisHead + eachGroup, size);
						Counter.add(eachDisHead + eachType, size);
						Counter.add(eachDisHead + eachLevel + "." + eachGroup, size);
						Counter.add(eachDisHead + eachLevel + "." + eachType, size);
						Counter.add(eachDisHead + eachGroup + "." + eachType, size);
						Counter.add(hlgtKey, size);
						// The all contain normal and echo.disabled.
						if (eachDisHead.isEmpty()) {
							f.write(" \\\necho.disabled." + hlgtKey);
							Counter.add("all", size);
						} else if (eachDisHead.equals("echo.disabled.")) {
							// Normal target contains echo.disabled target
							Counter.add(eachLevel, size);
							Counter.add(eachGroup, size);
							Counter.add(eachType, size);
							Counter.add(eachLevel + "." + eachGroup, size);
							Counter.add(eachLevel + "." + eachType, size);
							Counter.add(eachGroup + "." + eachType, size);
							Counter.add(eachLevel + "." + eachGroup + "." + eachType, size);
							Counter.add("all", size);
						}
						f.write("\n\n.PHONY: " + hlgtKey + "\n\n");
					}
				}
			}
		}

		f.close();
	}
}