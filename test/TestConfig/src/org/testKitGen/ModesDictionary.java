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

import java.io.BufferedReader;
import java.io.IOException;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

public class ModesDictionary {
	private static String modesXml = Options.getProjectRootDir() + "/TestConfig/" + Constants.MODESXML;
	private static String ottawaCsv = Options.getProjectRootDir() + "/TestConfig/" + Constants.OTTAWACSV;
	private static Map<String, String> spec2platMap = new HashMap<String, String>();
	private static Map<String, List<String>> invalidSpecsMap = new HashMap<String, List<String>>();
	private static Map<String, String> clArgsMap = new HashMap<String, String>();

	private ModesDictionary() {
	}

	public static void parse() {
		System.out.println("Getting modes data from " + Constants.MODESXML + " and " + Constants.OTTAWACSV + "...");
		try {
			Element modes = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(modesXml)
					.getDocumentElement();
			parseMode(modes);
			parseInvalidSpec(modes);
		} catch (SAXException | IOException | ParserConfigurationException e) {
			e.printStackTrace();
			System.exit(1);
		}
	}

	public static void parseMode(Element modes) {
		NodeList modesNodes = modes.getElementsByTagName("mode");
		for (int i = 0; i < modesNodes.getLength(); i++) {
			Element mode = (Element) modesNodes.item(i);
			StringBuilder sb = new StringBuilder();
			NodeList clArgsNodes = mode.getElementsByTagName("clArg");
			for (int j = 0; j < clArgsNodes.getLength(); j++) {
				sb.append(clArgsNodes.item(j).getTextContent().trim()).append(" ");
			}
			clArgsMap.put(mode.getAttribute("number"), sb.toString().trim());
		}
	}

	public static void parseInvalidSpec(Element modes) throws IOException {
		ArrayList<String> specs = new ArrayList<String>();
		int lineNum = 0;
		BufferedReader reader = null;
		if (Options.getSpec().toLowerCase().contains("zos")) {
			reader = Files.newBufferedReader(Paths.get(ottawaCsv), Charset.forName("IBM-1047"));
		} else {
			reader = Files.newBufferedReader(Paths.get(ottawaCsv));
		}
		String line = reader.readLine();
		while (line != null) {
			String[] fields = line.split(",");
			// Since the spec line has an empty title, we cannot do string match. We assume
			// the second line is spec.
			if (lineNum++ == 1) {
				specs.addAll(Arrays.asList(fields));
			} else if (fields[0].equals("plat")) {
				for (int i = 1; i < fields.length; i++) {
					spec2platMap.put(specs.get(i), fields[i]);
				}
			} else if (fields[0].startsWith("variation:")) {
				String modeNum = fields[0].substring("variation:".length());

				// Remove string Mode if it exists
				modeNum = modeNum.replace("Mode", "");

				NodeList modesNodes = modes.getElementsByTagName("mode");

				for (int i = 0; i < modesNodes.getLength(); i++) {
					Element mode = (Element) modesNodes.item(i);
					if (mode.getAttribute("number").equals(modeNum)) {
						ArrayList<String> invalidSpecs = new ArrayList<String>();
						for (int j = 1; j < fields.length; j++) {
							if (fields[j].equals("no")) {
								invalidSpecs.add(specs.get(j));
							}
						}
						// remove INGORESPECS from invalidSpecs array
						invalidSpecs = new ArrayList<String>(invalidSpecs.stream()
								.filter(c -> !Constants.INGORESPECS.contains(c)).collect(Collectors.toList()));
						// if invalidSpecs array is empty, set it to none
						if (invalidSpecs.size() == 0) {
							invalidSpecs.add("none");
						}
						invalidSpecsMap.put(modeNum, invalidSpecs);
						break;
					}
				}
			}
			line = reader.readLine();
		}

	}

	public static String getClArgs(String mode) {
		String rt = "";
		if (clArgsMap.containsKey(mode)) {
			rt = clArgsMap.get(mode);
		} else {
			System.out.println("\nWarning: cannot find mode " + mode + " to fetch jvm options");
		}
		return rt;
	}

	public static List<String> getInvalidSpecs(String mode) {
		List<String> rt = new ArrayList<String>();
		// It is normal that certain mode cannot be found in ottawa.csv, in which case
		// it means no invalid specs
		if (invalidSpecsMap.containsKey(mode)) {
			rt = invalidSpecsMap.get(mode);
		}
		return rt;
	}

	public static String getPlat(String spec) {
		String rt = "";
		if (spec2platMap.containsKey(spec)) {
			rt = spec2platMap.get(spec);
		} else {
			System.out.println("\nWarning: cannot find spec in " + Constants.OTTAWACSV + ".");
		}
		return rt;
	}
}