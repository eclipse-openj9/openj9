/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.oti.HookTool;

import java.io.File;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Queue;

import org.apache.xerces.parsers.DOMParser;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

/**
 * @author PBurka
 */
@SuppressWarnings("nls")
public class HookTool {

	private static final boolean FORCE = Boolean.getBoolean("hooktool.force");

	private static final boolean VERBOSE = Boolean.getBoolean("hooktool.verbose");

	private static Writer newWriter(File file) throws IOException {
		if (FORCE) {
			return new java.io.FileWriter(file);
		} else {
			return new OutputStreamWriter(new IncrementalFileOutputStream(file));
		}
	}

	private final File xmlFile;

	private File publicHeaderFile;

	private File privateHeaderFile;

	private Writer publicHeader;

	private Writer privateHeader;

	private int eventNum;

	public static void main(String[] args) {
		if (args.length != 1) {
			System.err.println("Usage: HookTool {directory}");
			System.exit(0);
		}

		long start = System.currentTimeMillis();

		File startDir = new File(args[0]);

		if (!startDir.isDirectory()) {
			System.err.println(args[0] + "is not a directory");
			System.exit(-1);
		}

		List<File> files = searchHookFiles(startDir);

		try {
			for (File file : files) {
				new HookTool(file).parseFile();
			}
		} catch (IOException e) {
			e.printStackTrace();
			System.exit(-1);
		}

		long duration = System.currentTimeMillis() - start;

		System.out.println("Finished processing " + files.size() + " HDF file(s) in " + duration + "ms");
	}

	private HookTool(File xmlFile) {
		super();
		this.xmlFile = xmlFile;
	}

	private static List<File> searchHookFiles(File startDir) {
		List<File> hookFiles = new LinkedList<>();

		if (VERBOSE) {
			System.out.println("Searching for hook description files (HDF)...");
		}

		for (Queue<File> dirsToSearch = new LinkedList<>(Collections.singletonList(startDir));;) {
			File dir = dirsToSearch.poll();

			if (dir == null) {
				break;
			}

			File[] files = dir.listFiles();

			if (files != null) {
				for (File file : files) {
					if (file.isDirectory()) {
						dirsToSearch.add(file);
					} else if (file.getName().endsWith(".hdf")) {
						if (VERBOSE) {
							System.out.println("Found " + file.getPath());
						}

						hookFiles.add(file);
					}
				}
			}
		}

		return hookFiles;
	}

	private void parseFile() throws IOException {
		if (VERBOSE) {
			System.out.println("Generating hook files for " + xmlFile.getPath());
		}

		DOMParser parser = new DOMParser();

		try {
			parser.parse(xmlFile.toURI().toURL().toExternalForm());
		} catch (SAXException se) {
			se.printStackTrace();
			System.exit(-1);
		}

		Document document = parser.getDocument();

		Element interfaceNode = document.getDocumentElement();

		Node publicHeaderNode = getNamedChild(interfaceNode, "publicHeader");
		Node privateHeaderNode = getNamedChild(interfaceNode, "privateHeader");
		Node struct = getNamedChild(interfaceNode, "struct");
		Node declarations = getNamedChild(interfaceNode, "declarations");

		createPublicHeader(getText(publicHeaderNode), declarations == null ? null : getText(declarations));
		createPrivateHeader(getText(privateHeaderNode));

		NodeList children = interfaceNode.getElementsByTagName("event");

		for (int i = 0, n = children.getLength(); i < n; ++i) {
			addEvent((Element) children.item(i));
		}

		closePublicHeader();
		closePrivateHeader(struct);
	}

	private static String getText(Node node) {
		StringBuilder text = new StringBuilder();
		NodeList children = node.getChildNodes();

		for (int i = 0, n = children.getLength(); i < n; ++i) {
			Node child = children.item(i);

			if (child.getNodeType() == Node.TEXT_NODE) {
				text.append(child.getNodeValue());
			}
		}

		return text.toString();
	}

	private static Node getNamedChild(Node parent, String name) {
		NodeList children = parent.getChildNodes();

		for (int i = 0, n = children.getLength(); i < n; ++i) {
			Node node = children.item(i);

			if (name.equals(node.getNodeName())) {
				return node;
			}
		}

		return null;
	}

	private void createPublicHeader(String name, String declarations) throws IOException {
		publicHeaderFile = new File(xmlFile.getParentFile(), name);

		String publicGateMacro = headerGateMacro(publicHeaderFile);

		publicHeader = newWriter(publicHeaderFile);
		publicHeader.write("/* Auto-generated public header file */\n");
		publicHeader.write("#ifndef " + publicGateMacro + "\n");
		publicHeader.write("#define " + publicGateMacro + "\n");
		publicHeader.write("#include \"j9hookable.h\"\n");

		if (declarations != null) {
			publicHeader.write("\n/* Begin declarations block */\n");
			publicHeader.write(declarations);
			publicHeader.write("\n/* End declarations block */\n");
		}

		publicHeader.write("\n");
	}

	private void createPrivateHeader(String name) throws IOException {
		privateHeaderFile = new File(xmlFile.getParentFile(), name);

		String privateGateMacro = headerGateMacro(privateHeaderFile);

		privateHeader = newWriter(privateHeaderFile);
		privateHeader.write("/* Auto-generated private header file */\n\n");
		privateHeader.write("/* This file should be included by the IMPLEMENTOR of the hook interface\n");
		privateHeader.write(" * It is not required by USERS of the hook interface\n");
		privateHeader.write(" */\n\n");
		privateHeader.write("#ifndef " + privateGateMacro + "\n");
		privateHeader.write("#define " + privateGateMacro + "\n");
		privateHeader.write("#include \"" + publicHeaderFile.getName() + "\"\n");
		privateHeader.write("\n");
	}

	private void addEvent(Element eventNode) throws IOException {
		// parse the elements of the event
		Node name = getNamedChild(eventNode, "name");
		Node description = getNamedChild(eventNode, "description");
		Node condition = getNamedChild(eventNode, "condition");
		Node struct = getNamedChild(eventNode, "struct");
		Node once = getNamedChild(eventNode, "once");
		Node sampling_node = getNamedChild(eventNode, "trace-sampling");
		Node reverse = getNamedChild(eventNode, "reverse");
		NodeList data = eventNode.getElementsByTagName("data");
		int sampling = 0;

		eventNum += 1;

		writeEventToPublicHeader(name, description, condition, struct, data, reverse);
		if (null != sampling_node) {
			sampling = Integer.parseInt(((Element)sampling_node).getAttribute("intervals"));
		}
		writeEventToPrivateHeader(name, condition, once, sampling, struct, data);
	}

	/**
	 * @param name
	 * @param struct
	 * @param data
	 * @throws IOException
	 */
	private void writeEventToPrivateHeader(Node name, Node condition, Node once, int sampling, Node struct, NodeList data) throws IOException {
		/* private header elements */
		String conditionText = condition != null ? getText(condition) : null;
		if (conditionText != null) {
			privateHeader.write("#if " + conditionText + "\n");
		}

		String nameText = getText(name);
		privateHeader.write("#define ALWAYS_TRIGGER_" + nameText + "(hookInterface");
		for (int i = 0, n = data.getLength(); i < n; ++i) {
			Element datum = (Element) data.item(i);
			privateHeader.write(", arg_" + datum.getAttribute("name"));
		}
		privateHeader.write(") do { \\\n");
		privateHeader.write("\t\tstruct " + getText(struct) + " eventData; \\\n");
		for (int i = 0, n = data.getLength(); i < n; ++i) {
			Element datum = (Element) data.item(i);
			privateHeader.write("\t\teventData." + datum.getAttribute("name") + " = (arg_" + datum.getAttribute("name") + "); \\\n");
		}
		privateHeader.write("\t\t(*J9_HOOK_INTERFACE(hookInterface))->J9HookDispatch(J9_HOOK_INTERFACE(hookInterface), " + nameText + (once == null ? "" : " | J9HOOK_TAG_ONCE") + (sampling == 0 ? "" : (" | " + (sampling << 16))) + ", &eventData); \\\n");
		for (int i = 0, n = data.getLength(); i < n; ++i) {
			Element datum = (Element) data.item(i);
			if (booleanValue(datum.getAttribute("return"), false)) {
				privateHeader.write("\t\t(arg_" + datum.getAttribute("name") + ") = eventData." + datum.getAttribute("name") + "; /* return argument */ \\\n");
			}
		}
		privateHeader.write("\t} while (0)\n\n");

		privateHeader.write("#define TRIGGER_" + nameText + "(hookInterface");
		for (int i = 0, n = data.getLength(); i < n; ++i) {
			Element datum = (Element) data.item(i);
			privateHeader.write(", arg_" + datum.getAttribute("name"));
		}
		privateHeader.write(") do { \\\n");
		if (once == null) {
			privateHeader.write("\tif (J9_EVENT_IS_HOOKED(hookInterface, " + nameText + ")) { \\\n");
		} else {
			privateHeader.write("\t\t/* always trigger this 'report once' event, so that it will be disabled after this point */ \\\n");
		}
		privateHeader.write("\t\tALWAYS_TRIGGER_" + nameText + "(hookInterface");
		for (int i = 0, n = data.getLength(); i < n; ++i) {
			Element datum = (Element) data.item(i);
			privateHeader.write(", arg_" + datum.getAttribute("name"));
		}
		privateHeader.write("); \\\n\t");
		if (once == null) {
			privateHeader.write("} ");
		}
		privateHeader.write("} while (0)\n");

		if (conditionText != null) {
			privateHeader.write("#else /* " + conditionText + " */\n");
			privateHeader.write("#define TRIGGER_" + nameText + "(hookInterface");
			for (int i = 0, n = data.getLength(); i < n; ++i) {
				Element datum = (Element) data.item(i);
				privateHeader.write(", arg_" + datum.getAttribute("name"));
			}
			privateHeader.write(")\n");
			privateHeader.write("#endif /* " + conditionText + " */\n");
		}

		privateHeader.write("\n");
	}

	/**
	 * @param name
	 * @param description
	 * @param struct
	 * @param data
	 * @throws IOException
	 */
	private void writeEventToPublicHeader(Node name, Node description, Node condition, Node struct, NodeList data, Node reverse) throws IOException {
		/* public header elements */
		String nameText = getText(name);
		String conditionText = condition != null ? getText(condition) : null;
		String structText = getText(struct);
		String example = "Example usage:\n"
				+ "\t(*hookable)->J9HookRegisterWithCallSite(hookable, " + nameText + ", eventOccurred, OMR_GET_CALLSITE(), NULL);\n"
				+ "\n"
				+ "\tstatic void\n"
				+ "\teventOccurred(J9HookInterface** hookInterface, UDATA eventNum, void* voidData, void* userData)\n"
				+ "\t{\n"
				+ "\t\t" + structText + "* eventData = voidData;\n"
				+ "\t\t. . .\n"
				+ "\t}\n";

		writeComment(publicHeader, nameText + "\n" + getText(description) + "\n\n" + example);

		if (conditionText != null) {
			publicHeader.write("#if " + conditionText + "\n");
		}

		if (reverse == null) {
			publicHeader.write("#define " + nameText + " " + eventNum + "\n");
		} else {
			publicHeader.write("#define " + nameText + " (" + eventNum + " | J9HOOK_TAG_REVERSE_ORDER)\n");
		}

		publicHeader.write("typedef struct " + structText + " {\n");
		for (int i = 0, n = data.getLength(); i < n; ++i) {
			Element datum = (Element) data.item(i);
			publicHeader.write("\t" + datum.getAttribute("type") + " " + datum.getAttribute("name") + ";\n");
		}
		publicHeader.write("} " + structText + ";\n");

		if (conditionText != null) {
			publicHeader.write("#endif /* " + conditionText + "*/ \n");
		}

		publicHeader.write("\n");
	}

	private void closePrivateHeader(Node struct) throws IOException {
		String structText = getText(struct);

		int eventCount = eventNum + 1;

		privateHeader.write("typedef struct " + structText + " {\n");
		privateHeader.write("\tstruct J9CommonHookInterface common;\n");
		privateHeader.write("\tU_8 flags[" + eventCount + "];\n");
		privateHeader.write("\tstruct OMREventInfo4Dump infos4Dump[" + eventCount + "];\n");
		privateHeader.write("\tJ9HookRecord* hooks[" + eventCount + "];\n");
		privateHeader.write("} " + structText + ";\n");
		privateHeader.write("\n");
		privateHeader.write("#endif /* " + headerGateMacro(privateHeaderFile) + " */\n");
		privateHeader.close();
	}

	private void closePublicHeader() throws IOException {
		publicHeader.write("#endif /* " + headerGateMacro(publicHeaderFile) + " */\n");
		publicHeader.close();
	}

	private static String headerGateMacro(File file) {
		return file.getName().toUpperCase().replace('.', '_');
	}

	private static void writeComment(Writer writer, String comment) throws IOException {
		writer.write("/* ");
		writer.write(comment);
		writer.write(" */\n");
	}

	private static boolean booleanValue(String val, boolean defaultValue) {
		switch (val.toLowerCase()) {
		case "1":
		case "true":
		case "yes":
			return true;
		case "0":
		case "false":
		case "no":
			return false;
		default:
			return defaultValue;
		}
	}

}
