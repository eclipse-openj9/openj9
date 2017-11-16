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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.oti.NLSTool;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;
import java.util.regex.Pattern;

public class J9NLS implements NLSConstants {
	private static final int DIFFERENT_ORDER_FOUND = -1;
	private Properties outputProperties;
	private File palmResourceFile;
	private FileWriter palmResourceWriter;
	private Hashtable oldNLSFiles;
	private Hashtable localeHashtable;
	private Vector nlsInfos;
	private int totalMessagesProcessed = 0;
	private int totalNLSFilesFound = 0;
	private int totalHeaderFilesCreated = 0;
	private int totalHeaderFilesSkipped = 0;
	private int totalPropertiesFilesCreated = 0;
	private int totalPropertiesFilesSkipped = 0;
	private int totalPalmFilesCreated = 0;
	private int totalPalmFilesSkipped = 0;
	private int messageNumber;
	private int errorCount = 0;
	private String workingDirectory = System.getProperty("user.dir");
	private String sourceDirectory = System.getProperty("user.dir");
	private String fileSeparator = System.getProperty("file.separator");
	private String propertiesFileName = "java.properties"; // default
	private String palmResourceFileName = "java.rc"; // default
	private String htmlFileName = "properties.html"; // default
	private String locale = DEFAULT_LOCALE;
	private static boolean debugoutput = false;
	
	public static void main(String[] args) {
		J9NLS j9NLS = new J9NLS();
		j9NLS.runMain(args);
	}

	public static void dp(String s){
		if (debugoutput) {
			System.out.println(s);
		}
	}

	public J9NLS() {
	}

	private void runMain(String[] args) {
		boolean palmMode = false;
		boolean nohtml = false;
		String outFileName;

		for (int i = 0; i < args.length; i++) {
			String arg = args[i];
			if (arg.compareToIgnoreCase("-help") == 0) {
				printUsage();
				return;
			} else if (arg.compareToIgnoreCase("-palm") == 0) {
				palmMode = true;
			} else if (arg.compareToIgnoreCase("-debug") == 0) {
				debugoutput = true;
			} else if (arg.compareToIgnoreCase("-nohtml") == 0) {
				nohtml = true;
			} else if (arg.compareToIgnoreCase("-out") == 0) {
				if ((i + 1) >= args.length || args[i + 1].startsWith("-"))
					failure("Output file name is not specified.");
				else if (!(args[i + 1].endsWith(".rc") || args[i + 1].endsWith(".properties")))
					failure("Output file name is not in correct format: " + args[i + 1]);
				else
					propertiesFileName = args[++i];
			} else if (arg.compareToIgnoreCase("-html") == 0) {
				if ((i + 1) >= args.length || args[i + 1].startsWith("-"))
					failure("HTML file name is not specified.");
				else if (!(args[i + 1].endsWith(".html") || args[i + 1].endsWith(".htm"))) {
					failure("Output file name is not in correct format: " + args[i + 1]);
				} else
					htmlFileName = args[++i];
			} else if (arg.compareToIgnoreCase("-source") == 0) {
				if ((i + 1) >= args.length || args[i + 1].startsWith("-"))
					failure("Source directory not specified");
				File srcDir = new File(args[++i]);
				if (!srcDir.exists() || ! srcDir.isDirectory())
					failure("Source directory '" + srcDir + "' does not exist");

				sourceDirectory = srcDir.getAbsolutePath();
			} else {
				failure("Unrecognized option: " + arg);
			}
		}

		if (palmMode)
			outFileName = palmResourceFileName;
		else
			outFileName = propertiesFileName;

		nlsInfos = new Vector();
		searchNLSFiles();
		if (localeHashtable.size() < 1)
			failure("Cannot find any NLS files below " + sourceDirectory);

		try {
			generatePropertiesFiles(outFileName, palmMode);
			if (!nohtml)
				new NLSHtmlGenerator().generateHTML(nlsInfos, outFileName, htmlFileName);
		} catch (FileNotFoundException fnfe) {
			if (debugoutput)
				fnfe.printStackTrace();
			failure("Caught exception: " + fnfe.getMessage(),true);
		} catch (IOException e) {
			if (debugoutput)
				e.printStackTrace();
			failure("Caught exception: " + e.getMessage(),true);
		}
		summarize();
	}

	private void generatePropertiesFiles(String outFileName, boolean palmMode) throws IOException, FileNotFoundException {
		String moduleName, headerName;
		FileOutputStream out = null;
		Hashtable headerHashtable = new Hashtable();
		Vector defaultNLSFiles = (Vector) localeHashtable.get(DEFAULT_LOCALE);
		StringBuffer buffer = new StringBuffer();

		// Create the output directory if it doesnt exist
		File outputDir = new File(workingDirectory + fileSeparator + "nls");
		if (!outputDir.exists()) {
				dp("Output directory does not exist, creating it");
				outputDir.mkdir();
		}

		for (Enumeration locales = localeHashtable.keys(); locales.hasMoreElements();) {
			locale = (String) locales.nextElement();
			Vector nlsFiles = (Vector) localeHashtable.get(locale);
			dp("Processing " + nlsFiles.size() + " NLS files for " + getLocaleName(locale)
					+ " locale");

			if (!palmMode) {
				outputProperties = new Properties();
			}

			for (int i = 0; i < nlsFiles.size(); i++) {
				NLSInfo nlsInfo = null;
				boolean newNLSInfo = false;
				messageNumber = 0;

				File nlsFile = (File) nlsFiles.elementAt(i);
				String nlsPath = nlsFile.getPath();
				Properties nlsProperties = new Properties();
				java.io.FileInputStream strm = new java.io.FileInputStream(nlsPath);
				nlsProperties.load(strm);
				strm.close();

				dp("Processing " + nlsPath);

				validateKeys(nlsPath, nlsProperties);

				moduleName = (String) nlsProperties.get(MODULE_KEY);
				if (moduleName.length() != DEFAULT_MODULE_LENGTH)
					failure("Module name must be exactly " + DEFAULT_MODULE_LENGTH + " characters long: "
							+ moduleName);

				String headerPath = workingDirectory + fileSeparator + "nls" + fileSeparator;
				headerName = (String) nlsProperties.get(HEADER_KEY);

				nlsInfo = findNLSInfo(nlsInfos, moduleName, headerName);
				if (nlsInfo.getModule() == null)
					newNLSInfo = true;
				nlsInfo.setModule(moduleName);
				nlsInfo.setPath(nlsPath.substring((sourceDirectory + fileSeparator).length()));
				nlsInfo.setHeaderName(headerName);
				nlsInfo.addLocale(locale);

				HeaderBuffer headerBuffer = new HeaderBuffer(headerName, nlsFile.getName());
				headerBuffer.writeHeader(headerName, moduleName);

				compareWithOld(nlsFile, nlsProperties);
				compareWithDefault(nlsFile, nlsProperties, defaultNLSFiles);

				Iterator keyIterator = getOrderedKeys(nlsProperties, nlsFile).iterator();
				Vector msgInfos = new Vector();
				while (keyIterator.hasNext()) {
					if (messageNumber >= MAX_NUMBER_OF_MESSAGES_PER_MODULE)
						failure(moduleName + " has more messages than the max limit of "
								+ MAX_NUMBER_OF_MESSAGES_PER_MODULE);
					String key = (String) keyIterator.next();
					if (nlsProperties.containsKey(key)) {
						String formattedMsgNumber = formatMsgNumber(messageNumber);
						String msg = (String) nlsProperties.get(key);
						if ((!msg.equals("")) || (locale!="")){
							if (palmMode) {
								byte[] utf8Msg = msg.getBytes("UTF-8");
								buffer.append("HEX \"" + moduleName + "\" ID " + formattedMsgNumber
										+ " " + toHexString(utf8Msg) + "\n");
							} else {
								outputProperties.setProperty(moduleName + formattedMsgNumber, msg);
							}
						}
						msgInfos
						.addElement(createMsgInfo(moduleName + formattedMsgNumber, key, msg, nlsProperties));

						if (msg.equals("")) {
							headerBuffer.append("#define " + key + "__PREFIX \"" + moduleName + formattedMsgNumber
									+ "\"\n");
						} else {
							headerBuffer.appendMsg(moduleName, key);
						}
						messageNumber++;
					}
				}

				nlsInfo.setMsgInfo(msgInfos);
				dp(messageNumber + " messages processed from " + nlsPath);
				totalMessagesProcessed += messageNumber;
				headerBuffer.append("\n#endif\n");

				if (newNLSInfo)
					nlsInfos.addElement(nlsInfo);

				if (DEFAULT_LOCALE.equals(locale)) {
					/* only output the header once (for the default locale) */
					generateHeaderFile(headerHashtable, nlsFile, headerPath, headerBuffer);
				}
			}

			//check if file already exists on the file system
			//if found, compare it with the generated file; if the same, do not overwrite it
			//if the file does not exists, write it to disk
			if (palmMode) {
				palmResourceFile = new File(getNameWithLocale(outFileName, locale));
				if (differentFromCopyOnDisk(getNameWithLocale(outFileName, locale), buffer)) {
					palmResourceWriter = new FileWriter(palmResourceFile);
					palmResourceWriter.write(buffer.toString());
					palmResourceWriter.close();
					dp("** Generated " + palmResourceFileName + "\n");
					totalPalmFilesCreated++;
				} else {
					dp("** Skipped writing [same as on file system]: " + palmResourceFile);
					totalPalmFilesSkipped++;
				}
			} else {
					String fileName = getNameWithLocale(outFileName, locale);
					if (propertiesDifferentFromCopyOnDisk(fileName)) {
						out = new FileOutputStream(fileName);	
						outputProperties.store(out, null);
						out.flush();
						dp("** Generated " + fileName + "\n");
						totalPropertiesFilesCreated++;
					} else {
						dp("** Skipped writing [same as on file system]: " + fileName);
						totalPropertiesFilesSkipped++;
					}
			}
			
			if (null != out) {
				out.close();
			}
		}
	}

	private MsgInfo createMsgInfo(String macro, String key, String msg, Properties nlsProperties) {
		MsgInfo msgInfo = new MsgInfo();
		msgInfo.setMacro(macro);
		msgInfo.setKey(key);
		msgInfo.setMsg(msg);
		msgInfo.setId(messageNumber);
		msgInfo.setExplanation(nlsProperties.getProperty(key + SUFFIX_EXPLANATION));
		msgInfo.setSystemAction(nlsProperties.getProperty(key + SUFFIX_SYSTEM_ACTION));
		msgInfo.setUserResponse(nlsProperties.getProperty(key + SUFFIX_USER_RESPONSE));
		return msgInfo;
	}

	/**
	 * @param nlsPath
	 * @param nlsProperties
	 */
	private void validateKeys(String nlsPath, Properties nlsProperties) {
		if (!nlsProperties.containsKey(MODULE_KEY))
			failure("Module name not found in " + nlsPath + " file");

		if (!nlsProperties.containsKey(HEADER_KEY))
			failure("Header file name not found in " + nlsPath + " file");

		for (Enumeration keys = nlsProperties.keys(); keys.hasMoreElements();) {
			String key = (String) keys.nextElement();
			
			if (isSpecialKey(key)) {
				String entry = key.substring(0, key.indexOf('.'));

				if (MODULE_KEY.equals(key)) {
					// no validation of this key
				} else if (HEADER_KEY.equals(key)) {
					// no validation of this key
				} else {
					if (!nlsProperties.containsKey(entry)) {
						failure(key + " has no corresponding NLS entry", false);
					}
					if (
						key.endsWith(SUFFIX_EXPLANATION)
						|| key.endsWith(SUFFIX_SYSTEM_ACTION)
						|| key.endsWith(SUFFIX_USER_RESPONSE)
						|| key.endsWith(SUFFIX_LINK)
						|| Pattern.matches(".*\\.sample_input_[1-9][0-9]*", key))
					{
						// key is valid
					} else {
						failure(key + " is an unrecognized special key. ", false);
						
					}
				}
			}
		}

	}

	private NLSInfo findNLSInfo(Vector nlsInfos, String moduleName, String headerName) {
		for (Enumeration e = nlsInfos.elements(); e.hasMoreElements();) {
			NLSInfo nlsInfo = (NLSInfo) e.nextElement();
			if (nlsInfo.isSameNLS(moduleName, headerName))
				return nlsInfo;
		}
		return new NLSInfo();
	}

	private void compareWithOld(File nlsFile, Properties nlsProperties) throws IOException, FileNotFoundException {
		Vector orderedKeys = getOrderedKeys(nlsProperties, nlsFile);
		if (oldNLSFiles.containsKey(nlsFile.getName() + ".old")) {
			File oldNLSFile = (File) oldNLSFiles.get(nlsFile.getName() + ".old");
			Properties oldNLSProperties = new Properties();
			java.io.FileInputStream strm = new java.io.FileInputStream(oldNLSFile.getPath());
			oldNLSProperties.load(strm);
			strm.close();
			Vector oldOrderedKeys = getOrderedKeys(oldNLSProperties, oldNLSFile);
			dp("Comparing with " + oldNLSFile.getPath());
			int compareOrders = compareOrder(oldOrderedKeys, orderedKeys);
			if (compareOrders == DIFFERENT_ORDER_FOUND) {
				failure("Old messages in " + nlsFile.getPath() + " are different from the ones in "
						+ oldNLSFile.getPath(), false);
			} else if (compareOrders != 0)
				dp("  Found " + compareOrders + " new message(s) in " + nlsFile.getPath());
			else
				dp("  They are identical to each other.");
		}
	}

	private void compareWithDefault(File nlsFile, Properties nlsProperties, Vector defaultNLSFiles)
			throws FileNotFoundException, IOException {
		File defaultNLSFile = findDefault(nlsFile, defaultNLSFiles);
		Properties defaultNLSProperties = new Properties();
		FileInputStream strm = new java.io.FileInputStream(defaultNLSFile.getPath());
		defaultNLSProperties.load(strm);
		strm.close();
		Vector defaultOrderedKeys = getOrderedKeys(defaultNLSProperties, defaultNLSFile);
		Vector defaultMessages = getValuesForKeys(defaultOrderedKeys, defaultNLSFile);
		Vector nlsMessages = getValuesForKeys(defaultOrderedKeys, nlsFile);
		Iterator nlsKeys = defaultOrderedKeys.iterator();
		Iterator defaultIterator = defaultMessages.iterator();
		Iterator nlsIterator = nlsMessages.iterator();
		while (defaultIterator.hasNext()) {
			String key = (String)nlsKeys.next();
			String defaultMessage = (String) defaultIterator.next();
			String nlsMessage = (String) nlsIterator.next();
			if (!"".equals(defaultMessage) && null != nlsMessage) {
				if (false == formatSpecifiersAreEquivalent(defaultMessage, nlsMessage))
					failure(key + " Message \"" + nlsMessage + "\" in " + nlsFile.getPath() + " is different from \""
							+ defaultMessage + "\" in " + defaultNLSFile.getPath(), false);
			}
		}
	}

	private File findDefault(File nlsFile, Vector defaultNLSFiles) {
		String nlsFileName = nlsFile.getName();
		String defaultNLSFileName = ((nlsFileName.lastIndexOf('_') != -1) ? nlsFileName.substring(0, nlsFileName
				.indexOf('_')) : nlsFileName.substring(0, nlsFileName.lastIndexOf('.')))
				+ DEFAULT_LOCALE + nlsFileName.substring(nlsFileName.lastIndexOf('.'));
		Iterator i = defaultNLSFiles.iterator();
		while (i.hasNext()) {
			File f = (File) i.next();
			if (f != null && f.getName().equals(defaultNLSFileName)) {
				return f;
			}
		}
		return null;
	}

	private boolean formatSpecifiersAreEquivalent(String oldMessage, String newMessage) {
		boolean reorderable = false;
		Vector oldFormatSpec = formatSpecifiersOf(oldMessage);
		Vector newFormatSpec = formatSpecifiersOf(newMessage);
		
		if (newFormatSpec.size() != oldFormatSpec.size()) {
			return false;
		}

		/*
		 * If the format specifiers contain $ signs, then the specifiers may
		 * appear in any order within the string. Detect if $ signs are being
		 * used to decide how the list of specifiers ought to be compared.
		 */
		for (Iterator iter = oldFormatSpec.iterator(); iter.hasNext();) {
			String spec = (String) iter.next();
			if (spec.indexOf('$') != -1) {
				reorderable = true;
				break;
			}
		}
		
		if (reorderable) {
			for (Iterator iter = oldFormatSpec.iterator(); iter.hasNext();) {
				String spec = (String)iter.next();
				if (spec.indexOf('$') == -1) {
					failure("\"" + oldMessage + "\" includes a mix of ordered and unordered format sepcifiers", false);
				}
				if (!newFormatSpec.remove(spec)) {
					return false;
				}
			}
			return true;
		} else {
			return oldFormatSpec.equals(newFormatSpec);
		}
	}

	private Vector formatSpecifiersOf(String s) {
		Vector result = new Vector();
		
		if (null == s)
			return result;

		StringBuffer buf = new StringBuffer();
		int e = s.length();
		int state = 0;
		for (int i = 0; i < e; ++i) {
			char c = s.charAt(i);
			switch (state) {
			case 0:
				if ('%' == c)
					state = 1;
				break;
			case 1:
				if ('%' == c) {
					state = 0;
					break;
				}
				buf.append('%');
				state = 2;
			// FALLTHRU
			case 2:
				if ("pcsxXuidf".indexOf(c) != -1) {
					if ('p' == c)
						buf.append("zx");
					else if ("csxf".indexOf(c) != -1)
						buf.append(c);
					else
						buf.append('x');
					result.add(buf.toString());
					buf = new StringBuffer();
					state = 0;
				} else {
					buf.append(c);
				}
				break;
			default:
				failure("unknown error extracting format specifiers from " + s);
			}
		}

		return result;
	}

	private Vector getValuesForKeys(Vector keys, File nlsFile) throws FileNotFoundException, IOException {
		String nlsPath = nlsFile.getPath();
		Properties nlsProperties = new Properties();
		java.io.FileInputStream strm = new java.io.FileInputStream(nlsPath);
		nlsProperties.load(strm);
		strm.close();

		Vector values = new Vector();
		Iterator i = keys.iterator();
		while (i.hasNext()) {
			values.addElement(nlsProperties.get(i.next()));
		}
		return values;
	}

	private boolean isSpecialKey(String key) {
		return key.indexOf('.') >= 0;
	}

	private int compareOrder(Vector oldOrderedKeys, Vector orderedKeys) {
		Iterator iterator = orderedKeys.iterator();
		Iterator oldIterator = oldOrderedKeys.iterator();
		String key, oldKey;
		while (oldIterator.hasNext()) {
			if (!iterator.hasNext())
				return DIFFERENT_ORDER_FOUND;
			else {
				key = (String) iterator.next();
				oldKey = (String) oldIterator.next();
				if (!key.equals(oldKey)) {
					dp("Mismatch found: " + key + ", " + oldKey);
					return DIFFERENT_ORDER_FOUND;
				}
			}
		}
		int toReturn = 0;
		while (iterator.hasNext()) {
			iterator.next();
			toReturn++;
		}
		return toReturn;
	}

	private void generateHeaderFile(Hashtable headerHashtable, File nlsFile, String headerPath,
			HeaderBuffer headerBuffer) throws IOException,FileNotFoundException {
		if (!headerHashtable.containsKey(headerBuffer.getHeaderName())) {
		
			StringBuffer buffer = new StringBuffer();
			buffer.append(headerBuffer.toString());
			
			if (differentFromCopyOnDisk(headerPath + headerBuffer.getHeaderName(), buffer)){
				File headerFile = new File(headerPath + headerBuffer.getHeaderName());
				FileWriter headerFileWriter = new FileWriter(headerFile);
				headerFileWriter.write(headerBuffer.toString());
				headerHashtable.put(headerPath + headerBuffer.getHeaderName(), headerBuffer);
				headerFileWriter.close();
				dp("** Generated " + headerFile.getPath());
				totalHeaderFilesCreated++;
			}else{
				totalHeaderFilesSkipped++;
				dp("** Skipped writing [same as on file system]: " + headerPath + headerBuffer.getHeaderName());
			}
		} else {
			HeaderBuffer oldBuffer = (HeaderBuffer) headerHashtable.get(headerBuffer.getHeaderName());
			if (!oldBuffer.equals(headerBuffer))
				failure("Header files generated from " + oldBuffer.getNLSFileName() + " and " + nlsFile.getName()
						+ " do not match");
			headerBuffer.clear();
		}
	}

	protected Hashtable searchNLSFiles() {
		dp("Searching for nls files...");
		Vector dirToSearch = new Vector();
		oldNLSFiles = new Hashtable();
		localeHashtable = new Hashtable();
		dirToSearch.addElement(new File(sourceDirectory + fileSeparator + "nls" + fileSeparator));
		String files[];

		while (dirToSearch.size() > 0) {
			File dir = (File) dirToSearch.elementAt(0);
			dirToSearch.remove(0);
			if (!dir.exists() || ! dir.isDirectory()) {
				failure("Failed to open directory - " + dir);
			}
			files = dir.list();
			for (int i = 0; i < files.length; i++) {
				File file = new File(dir, files[i]);
				if (file.isDirectory())
					dirToSearch.insertElementAt(file, 0);
				else if (file.getName().endsWith(".nls")) {
					dp("Found " + file.getPath());
					String locale = getLocale(file.getName());
					if (!localeHashtable.containsKey(locale)) {
						dp("Found a new locale: " + getLocaleName(locale));
						Vector temp = new Vector();
						temp.add(file);
						localeHashtable.put(locale, temp);
					} else {
						((Vector) localeHashtable.get(locale)).add(file);
					}
					totalNLSFilesFound++;
				} else if (file.getName().endsWith(".nls.old")) {
					dp("Found " + file.getPath());
					oldNLSFiles.put(file.getName(), file);
				}
			}
		}
		dp("");

		return localeHashtable;
	}

	private String getKey(String line) {
		if (line.indexOf('=') != -1)
			return line.substring(0, line.indexOf('='));
		else
			return null;
	}

	protected Vector getOrderedKeys(Properties nlsProperties, File nlsFile) throws IOException {
		Vector keys = new Vector();
		FileReader nlsFileReader = new FileReader(nlsFile);
		BufferedReader bufferedNLSReader = new BufferedReader(nlsFileReader);
		String line;
		while ((line = bufferedNLSReader.readLine()) != null) {
			String key = getKey(line);
			if (key != null && !isSpecialKey(key)) {
				if (keys.contains(key))
					failure("Duplicate keys are found in " + nlsFile.getPath() + ": " + key, false);
				if (nlsProperties.containsKey(key))
					keys.add(key);
			}
		}
		bufferedNLSReader.close();

		return keys;
	}

	private String getNameWithLocale(String fileName, String locale) {
		return workingDirectory + fileSeparator + fileName.substring(0, propertiesFileName.lastIndexOf('.')) + locale
				+ fileName.substring(propertiesFileName.lastIndexOf('.'));
	}

	protected String formatMsgNumber(int msgNumber) {
		String currentMsgNumber = String.valueOf(msgNumber);
		while (currentMsgNumber.length() < DEFAULT_MESSAGE_NUMBER_LENGTH)
			currentMsgNumber = "0" + currentMsgNumber;
		return currentMsgNumber;
	}

	private String getLocale(String nlsFileName) {
		if (nlsFileName.lastIndexOf('_') != -1)
			return nlsFileName.substring(nlsFileName.indexOf('_'), nlsFileName.lastIndexOf("."));
		else
			return DEFAULT_LOCALE;
	}

	private String getLocaleName(String locale) {
		if (locale.equals(DEFAULT_LOCALE))
			return "\'default\'";
		else
			return "\'" + locale.substring(1) + "\'";
	}

	private void summarize() {
		System.out.println("\n************************************************\n");
		System.out.println("\tTotal # of locales processed:\t\t" + localeHashtable.size());
		System.out.println("\tTotal # of NLS Files processed:\t\t" + totalNLSFilesFound);
		System.out.println("\tTotal # of messages processed:\t\t" + totalMessagesProcessed + "\n");
		System.out.println("\tTotal # of header files generated:\t" + totalHeaderFilesCreated);
		if (totalHeaderFilesSkipped > 0) {
			System.out.println("\tTotal # of unchanged header files:\t" + totalHeaderFilesSkipped);
		}
		System.out.println("\tTotal # of properties files generated:\t" + totalPropertiesFilesCreated);
		if (totalPropertiesFilesSkipped > 0) {
			System.out.println("\tTotal # of unchanged properties files:\t" + totalPropertiesFilesSkipped);
		}
		if (totalPalmFilesCreated > 0) {
			System.out.println("\tTotal # of palm files generated:\t" + totalPalmFilesCreated);
		}
		if (totalPalmFilesSkipped > 0) {
			System.out.println("\tTotal # of unchanged palm files:\t" + totalPalmFilesSkipped);
		}
		System.out.println("\tTotal # of errors:\t\t\t" + errorCount);
		System.out.println("\n************************************************\n");
	
		if (errorCount > 0) {
			System.exit(1);
		}
	}

	private void printUsage() {
		System.out.println("J9 NLS Tool 1.2");
		System.out.println("Usage: J9NLS [options]\n");
		System.out.println("[options]");
		System.out.println("    -help                prints this message");
		System.out.println("    -out xxx             generates output named as xxx");
		System.out.println("    -source              path to the source directory");
		System.out.println("    -[no]html xxx        [do not] generates HTML file, which contains output results, named as xxx");
		System.out.println("    -palm                generates output as Palm resources\n");
		System.out.println("    -debug               generates informational output\n");
		System.out.println("Defaults are:");
		System.out.println("    -out " + propertiesFileName + "-html" + htmlFileName);
		System.out.println("    -source . ");
	}

	private void failure(String msg) {
		failure(msg, true);
	}
	
	private void failure(String msg, boolean fatal) {
		System.out.println("\n* Failure occured during the process *");
		System.out.println("Error: " + msg);
		errorCount++;
		if (fatal) {
			summarize();
		}
	}

	private class HeaderBuffer {
		StringBuffer buffer;
		String headerName;
		String NLSFileName;

		public HeaderBuffer(String headerName, String NLSFileName) {
			this.headerName = headerName;
			this.NLSFileName = NLSFileName;
			buffer = new StringBuffer("");
		}

		public String toString() {
			return buffer.toString();
		}

		public boolean equals(Object arg) {
			HeaderBuffer other = (HeaderBuffer) arg;
			if (null != other) {
				return other.buffer.toString().equals(buffer.toString());
			}
			return super.equals(arg);
		}

		public void append(String s) {
			buffer.append(s);
		}

		public void clear() {
			buffer.delete(0, buffer.length());
		}

		public String getHeaderName() {
			return headerName;
		}

		public String getNLSFileName() {
			return NLSFileName;
		}

		private String getAscii(String moduleName) {
			String asciiModuleName = "0x";
			for (int i = 0; i < moduleName.length(); i++)
				asciiModuleName += Integer.toHexString(moduleName.charAt(i));
			return asciiModuleName;
		}

		public void appendMsg(String moduleName, String key) {
			String defModule = key + "__MODULE";
			String defID = key + "__ID";
			append("#define " + defModule + " " + getAscii(moduleName) + "\n");
			append("#define " + defID + " " + messageNumber + "\n");
			append("#define " + key + " " + defModule + ", " + defID + "\n");
		}

		public void writeHeader(String headerName, String moduleName) {
			String header = headerName.replace('.', '_');
			append("/*\n");
			append(" * Copyright (c) 1991, 2017 IBM Corp. and others\n");
			append(" *\n");
			append(" * This program and the accompanying materials are made available under\n");
			append(" * the terms of the Eclipse Public License 2.0 which accompanies this\n");
			append(" * distribution and is available at https://www.eclipse.org/legal/epl-2.0/\n");
			append(" * or the Apache License, Version 2.0 which accompanies this distribution and\n");
			append(" * is available at https://www.apache.org/licenses/LICENSE-2.0.\n");
			append(" *\n");
			append(" * This Source Code may also be made available under the following\n");
			append(" * Secondary Licenses when the conditions for such availability set\n");
			append(" * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU\n");
			append(" * General Public License, version 2 with the GNU Classpath\n");
			append(" * Exception [1] and GNU General Public License, version 2 with the\n");
			append(" * OpenJDK Assembly Exception [2].\n");
			append(" *\n");
			append(" * [1] https://www.gnu.org/software/classpath/license.html\n");
			append(" * [2] http://openjdk.java.net/legal/assembly-exception.html\n");
			append(" *\n");
			append(" * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0\n");
			append(" */\n\n");
			append("#ifndef " + header + "\n");
			append("#define " + header + "\n\n");
			append("#include \"j9port.h\"\n");
			append("/* " + getAscii(moduleName) + " = " + moduleName + " */\n\n");
		}
	}

	private String toHexString(byte[] b) {
		final char[] hexChar = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
		StringBuffer stringBuffer = new StringBuffer();
		for (int i = 0; i < b.length; i++) {
			stringBuffer.append("0x");
			stringBuffer.append(hexChar[(b[i] & 0xf0) >>> 4]);
			stringBuffer.append(hexChar[b[i] & 0x0f]);
			stringBuffer.append(' ');
		}
		return stringBuffer.toString();
	}
	
	/**
	 * Checks if a file with given name exists on disk. Returns true if it does 
	 * not exist. If the file exists, compares it with generated buffer and 
	 * returns true if they are equals. Also it deletes old file from the file 
	 * system.  
	 * 
	 * @param fileName
	 * @param buffer
	 * @return boolean TRUE|FALSE
	 * @throws FileNotFoundException
	 * @throws IOException
	 */
	public static boolean differentFromCopyOnDisk(String fileName, StringBuffer buffer) throws  FileNotFoundException, IOException{
		File fileOnDisk = new File (fileName);
		if (!fileOnDisk.exists()){
			return true;
		}
				
		StringBuffer fileBuffer = new StringBuffer();
		FileReader fr = new FileReader(fileOnDisk);
		char []charArray = new char[1024];
		
		int numRead = -1;
		while ( (numRead = fr.read(charArray)) != -1 ) {
			fileBuffer.append(charArray, 0, numRead);
		}
				
		if ( buffer.toString().equals(fileBuffer.toString()) ) {
			fr.close();
			return false;
		}
		
		fr.close();
		
		//delete file here, will write new file later
		fileOnDisk.delete();
		
		return true;
	}
	
	/**
	 * Compares given properties file with the generated properties.
	 * Returns true if they do not match, otherwise returns false.
	 * It also deletes the file from the file system, if it exists.
	 * 
	 * @param fileName
	 * @return boolean TRUE|FALSE
	 * @throws FileNotFoundException
	 * @throws IOException
	 */
	private boolean propertiesDifferentFromCopyOnDisk(String fileName) throws FileNotFoundException, IOException{
		File fileOnDisk = new File(fileName);
		if (!fileOnDisk.exists()) {
			return true;
		}
		
		//load the file 
		Properties  oldProperties = new Properties();
		FileInputStream in = new FileInputStream(fileOnDisk);
		oldProperties.load(in);
		in.close();
		
		boolean isDifferent = false;
		if (!oldProperties.keySet().equals(outputProperties.keySet())){
			isDifferent = true;
		}else{
			//compare messages
			Iterator keyIterator  = oldProperties.keySet().iterator();
			while (keyIterator.hasNext()){
				if (!outputProperties.containsValue(oldProperties.getProperty(String.valueOf(keyIterator.next())))){
					isDifferent = true;
					break;
				}
			}
		}
		
		if (isDifferent) {
			//delete old file here, will generate new file later
			fileOnDisk.delete();
		}
		
		return isDifferent;
	}
}
