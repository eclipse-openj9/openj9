/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
package com.ibm.jpp.om;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Properties;
import java.util.Set;
import java.util.TreeMap;

/**
 * This builder extension finds, loads, and writes external messages during the preprocess.
 */
public class ExternalMessagesExtension extends BuilderExtension {

	/**
	 * The relative path where the external messages should be found.
	 */
	private String messagesPath = "com/ibm/oti/util";

	/**
	 * The name of the external messages file.
	 */
	private static final String messagesFile = "ExternalMessages.properties";

	/**
	 * If this option is set, create externalMessages file.
	 */
	private boolean externalMessages = true;

	/**
	 * If this option is set, messages are inlined.
	 */
	private boolean inlineMessages = false;

	/**
	 * If this option is set, message keys are inlined.
	 */
	private boolean inlineMessageKeys = false;

	/**
	 * If this option is set, an error is thrown if messages are found.
	 */
	private boolean foundMessagesError = false;

	/**
	 * If this option is set, we substitute the name of the class used
	 * to lookup messages
	 */
	private String messageClassName = null;

	/**
	 * The external messages loaded from the output directory. This is only used
	 * if builder.isIncremental()==true.
	 */
	private Map<String, String> existingMessages = null;

	/**
	 * The external messages found during this preprocessor run.
	 */
	private final Map<String, String> messages = new TreeMap<>();

	/**
	 * Unconditionally update output files?
	 */
	private boolean force = false;

	/**
	 * The extension constructor.
	 */
	public ExternalMessagesExtension() {
		super("msg");
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#validateOptions(Properties)
	 */
	@Override
	public void validateOptions(Properties options) {
		//check for the need for externalMessages file, if false it is not created.
		if (options.containsKey("msg:externalMessages")) {
			this.externalMessages = !options.getProperty("msg:externalMessages").equalsIgnoreCase("false");
		}

		// check for the external messages output directory option
		if (options.containsKey("msg:outputdir")) {
			this.messagesPath = options.getProperty("msg:outputdir");
		}

		// check for the inline messages option
		if (options.containsKey("msg:inline")) {
			this.inlineMessages = options.getProperty("msg:inline").equalsIgnoreCase("true");
		}

		// check for the inline keys option
		if (options.containsKey("msg:inlinekeys")) {
			this.inlineMessageKeys = options.getProperty("msg:inlinekeys").equalsIgnoreCase("true");
		}

		// check for the found messages error option
		if (options.containsKey("msg:error")) {
			this.foundMessagesError = true;
		}

		// check for the substitute message class option
		if (options.containsKey("msg:messageclass")) {
			this.messageClassName = options.getProperty("msg:messageclass");
		}

		if (Builder.isForced(options)) {
			this.force = true;
		}
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#notifyBuildBegin()
	 */
	@Override
	public void notifyBuildBegin() {
		if (builder.isIncremental() || builder.hasMultipleSources()) {
			loadExistingMessages();
		}
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#notifyBuildEnd()
	 */
	@Override
	public void notifyBuildEnd() {
		if (externalMessages) {
			if (existingMessages != null && (builder.isIncremental() || builder.hasMultipleSources())) {
				// add the existing messages to the found messages (unless the
				// message was already in the found messages)
				for (Map.Entry<String, String> entry : existingMessages.entrySet()) {
					if (messages.get(entry.getKey()) == null) {
						messages.put(entry.getKey(), entry.getValue());
					}
				}
			}
			writeMessages();
		}
	}

	/**
	 * Loads the external messages from the output directory before the build.
	 * This is used for incremental builds to preserve existing messages.
	 */
	private void loadExistingMessages() {
		// load the existing external messages from the output directory
		this.existingMessages = new HashMap<>();
		File outputFile = getOutputMessageFile();

		try {
			loadProperties(this.existingMessages, outputFile);
		} catch (FileNotFoundException e) {
			// no external messages existed
		} catch (IOException e) {
			StringBuffer msgBuffer = new StringBuffer("An exception occured while loading the existing external messages from \"");
			msgBuffer.append(outputFile);
			builder.getLogger().log(msgBuffer.toString(), Logger.SEVERITY_ERROR, e);
		}
	}

	/**
	 * Writes the external messages to the output directory after the build.
	 */
	private void writeMessages() {
		if (messages.size() > 0 && !inlineMessages) {
			File outputFile = getOutputMessageFile();
			/*[PR 115710] FileNotFoundException*/
			File parent = outputFile.getParentFile();

			parent.mkdirs();

			if (parent.exists()) {
				try (OutputStream output = new PhantomOutputStream(outputFile, force);
					 PrintWriter put = new PrintWriter(output)) {
					/**
					 * We don't use java.util.Properties.store method, because we want the keys in ascending order in properties file,
					 * TreeMap is used for this reason
					 */
					put.println("#");
					put.println("#     Copyright (c) 2000, " + new GregorianCalendar().get(Calendar.YEAR) + " IBM Corp. and others");
					put.println("#");
					put.println("#     This program and the accompanying materials are made available under");
					put.println("#     the terms of the Eclipse Public License 2.0 which accompanies this");
					put.println("#     distribution and is available at https://www.eclipse.org/legal/epl-2.0/");
					put.println("#     or the Apache License, Version 2.0 which accompanies this distribution and");
					put.println("#     is available at https://www.apache.org/licenses/LICENSE-2.0.");
					put.println("#");
					put.println("#     This Source Code may also be made available under the following");
					put.println("#     Secondary Licenses when the conditions for such availability set");
					put.println("#     forth in the Eclipse Public License, v. 2.0 are satisfied: GNU");
					put.println("#     General Public License, version 2 with the GNU Classpath");
					put.println("#     Exception [1] and GNU General Public License, version 2 with the");
					put.println("#     OpenJDK Assembly Exception [2].");
					put.println("#"); 
					put.println("#     [1] https://www.gnu.org/software/classpath/license.html");
					put.println("#     [2] http://openjdk.java.net/legal/assembly-exception.html");
					put.println("#");
					put.println("#     SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception");
					put.println("#");
					put.println("# External Messages for EN locale");
					Set<Entry<String, String>> entries = messages.entrySet();
					StringBuffer buffer = new StringBuffer(200);
					for (Entry<String, String> entry : entries) {
						dumpString(buffer, entry.getKey(), true);
						buffer.append('=');
						dumpString(buffer, entry.getValue(), false);
						put.println(buffer.toString());
						buffer.setLength(0);
					}
					put.flush();
					StringBuffer msgBuffer = new StringBuffer("Messages written to \"");
					msgBuffer.append(outputFile);
					builder.getLogger().log(msgBuffer.toString(), Logger.SEVERITY_INFO);
				} catch (IOException e) {
					StringBuffer msgBuffer = new StringBuffer("An exception occured while writing the existing external messages to \"");
					msgBuffer.append(outputFile);
					builder.getLogger().log(msgBuffer.toString(), Logger.SEVERITY_ERROR, e);
				}
			}
		}
	}

	private static void dumpString(StringBuffer buffer, String string, boolean key) {
		int i = 0;
		int length = string.length();
		if (!key) {
			while (i < length && string.charAt(i) == ' ') {
				buffer.append("\\ ");
				i++;
			}
		}

		for (; i < length; i++) {
			char ch = string.charAt(i);
			switch (ch) {
			case '\t':
				buffer.append("\\t");
				break;
			case '\n':
				buffer.append("\\n");
				break;
			case '\f':
				buffer.append("\\f");
				break;
			case '\r':
				buffer.append("\\r");
				break;
			default:
				if ("\\#!=:".indexOf(ch) >= 0 || (key && ch == ' ')) {
					buffer.append('\\');
				}

				if (ch >= ' ' && ch <= '~') {
					buffer.append(ch);
				} else {
					String hex = Integer.toHexString(ch);
					int hexLength = hex.length();
					buffer.append("\\u");
					for (int j = 0; j < 4 - hexLength; j++) {
						buffer.append("0");
					}
					buffer.append(hex);
				}
				break;
			}
		}
	}

	/**
	 * Returns the File for external messages in the output directory.
	 */
	private File getOutputMessageFile() {
		return new File(new File(builder.getOutputDir(), messagesPath), messagesFile);
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#notifyConfigurePreprocessor(JavaPreprocessor)
	 */
	@Override
	public void notifyConfigurePreprocessor(JavaPreprocessor preprocessor) {
		preprocessor.setExternalMessages(this.messages);
		/*[PR 120535] inlineMessageKeys and inlineMessageKeys conflict*/
		if (this.inlineMessages) {
			preprocessor.setInlineMessages(this.inlineMessages);
		} else if (this.inlineMessageKeys) {
			preprocessor.setInlineMessageKeys(this.inlineMessageKeys);
		}
		if (messageClassName != null) {
			preprocessor.setMessageClassName(this.messageClassName);
		}
	}

	/**
	 * @see com.ibm.jpp.om.BuilderExtension#notifyBuildFileEnd(File, File,
	 *      String)
	 */
	@Override
	public void notifyBuildFileEnd(File sourceFile, File outputFile, String relativePath) {
		if (foundMessagesError && messages.size() > 0) {
			// if a message was found in this file write an error
			StringBuffer msgBuffer = new StringBuffer("A preprocessor MSG command was found in the file \"");
			msgBuffer.append(relativePath);
			msgBuffer.append("\"");
			builder.getLogger().log(msgBuffer.toString(), Logger.SEVERITY_ERROR);
			messages.clear();
		}
	}

}
