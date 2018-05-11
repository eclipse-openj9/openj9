/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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
package com.ibm.jvm.format;

import java.io.*;
import java.util.Hashtable;

/**
 * Acts as a template for mapping trace ids to messages.
 * 
 * @author Jonathon Lee
 */
final public class MessageFile extends File
{
	private BufferedReader reader;

	private String component;

	private String formatName;

	protected static float verMod;

	private static boolean first;

	private static Hashtable messages;
	
	private static final String fromDATFile = ".definedInDatFile";

	public MessageFile(String s) throws IOException
	{
		super(s);
		formatName = s;
		reader = new BufferedReader(new InputStreamReader(
					new FileInputStream(this)));
		first = true;
		read();
	}

	public float getVersion()
	{
		return verMod;
	}

	/**
	 * Initializes static variables.
	 * 
	 * <p>
	 * This is called each time the TraceFormatter is started, from the
	 * initStatics() method in TraceFormat.java
	 * </p>
	 */
	final protected static void initStatics()
	{
		verMod = 0;
		first = true;
		messages = null;
	}

	private void read() throws IOException
	{
		String line = reader.readLine();
		while (line != null) {
			addMessage(line);
			line = reader.readLine();
		}
	}

	static int counter = 0;

	static String savedComponentName = null;

	/**
	 * adds a message to the table by parsing a line of the message file. If the
	 * line has no spaces, it is determined to be a component; otherwise it
	 * contains a trace id, an entry type, and a template string (separated by
	 * spaces)
	 * 
	 * @param messageLine
	 *            a string
	 */
	protected void addMessage(String messageLine)
	{
		int firstSpace = messageLine.indexOf(" ");
		int secondSpace = messageLine.indexOf(" ", firstSpace + 1);
		int thirdSpace = messageLine.indexOf(" ", secondSpace + 1);

		String componentName = null;
		int tpNumber = -1;
		String symbol = null;
		String msg = null;

		if ((firstSpace == -1) || (secondSpace == -1) || (thirdSpace == -1)) {
			if (first) {
				first = false;
				if (messageLine.equals("dg")) {
					verMod = (float) 1.0;
				} else {
					verMod = Float.valueOf(messageLine).floatValue();
				}
				
				if (MessageFile.verMod < TraceFormat.verMod) {
					// Defect 101265 - suppress warnings
					if (!TraceFormat.SUPPRESS_VERSION_WARNINGS) {
						TraceFormat.outStream.println(formatName + " (Version "
								+ MessageFile.verMod
								+ ") does not match this trace file (Version "
								+ TraceFormat.verMod + ")");
					}
				}
				return;
			}
			component = messageLine;
			return;
		}

		if (verMod >= 5.0F) {
			int fourthSpace = messageLine.indexOf(" ", thirdSpace + 1);
			componentName = new String(messageLine.substring(0, firstSpace));
			
			/* 
			 * version 5.1 and above contain the component and trace id together as
			 * a period separated pair: e.g. j9vm.123
			 */
			if (verMod >= 5.1F) {
				int period = componentName.indexOf('.');
				if (period == -1){
					/* ignore */
				} else {
					String compName = componentName.substring(0, period);
					String tpNum = componentName.substring(period + 1);
					tpNumber = Integer.parseInt(tpNum);
					componentName = compName;
				}
			}
			
			if (MessageFile.messages == null) {
				MessageFile.messages = new Hashtable();
			}

			if (savedComponentName == null || !componentName.equals(savedComponentName)) {
				/* add a special "component came from dat file" entry to the hashtable */
				String componentCameFromDATFileEntry = componentName + fromDATFile;
				MessageFile.messages.put(componentCameFromDATFileEntry, fromDATFile);
				savedComponentName = componentName;
				counter = 0;
			}

			int id = counter++;
			if (tpNumber >= 0){
				/* could check that counter and tpNumber match, but why bother */
				id = tpNumber;
			}
			int type = Integer.parseInt(messageLine.substring(firstSpace + 1, secondSpace));

			int fifthSpace = messageLine.indexOf(" ", fourthSpace + 1);
			int sixthSpace = messageLine.indexOf(" ", fifthSpace + 1);
			symbol = messageLine.substring(fifthSpace + 1, sixthSpace);
			msg = messageLine.substring(sixthSpace + 2, messageLine.length() - 1);

			Message message = new Message(type, msg, component, symbol);
			String messageKey = componentName + "." + id;

			if (messages.containsKey(messageKey)){
				System.err.println("Duplicate messages in .dat files: " + messageKey);
			} else {
				MessageFile.messages.put(messageKey, message);
			}
		} else {
			/* old behaviour required must be a 1.1 or similar trace file */
			int id = Integer.parseInt(messageLine.substring(0, firstSpace), 16);
			int type = Integer.parseInt(messageLine.substring(firstSpace + 1,
					secondSpace));
			if (verMod < (float) 1.2) {
				symbol = messageLine.substring(secondSpace + 1, thirdSpace);
				msg = messageLine.substring(thirdSpace + 2, messageLine
						.length() - 1);
			} else {
				int fourthSpace = messageLine.indexOf(" ", thirdSpace + 1);
				int fifthSpace = messageLine.indexOf(" ", fourthSpace + 1);
				int sixthSpace = messageLine.indexOf(" ", fifthSpace + 1);
				symbol = messageLine.substring(fifthSpace + 1, sixthSpace);
				msg = messageLine.substring(sixthSpace + 2, messageLine
						.length() - 1);
			}
			// Util.Debug.println("MessageFile: id + type + msg " + id + " " +
			// type + " " + msg);
			Message message = new Message(type, msg, component, symbol);

			if (MessageFile.messages == null) {
				MessageFile.messages = new Hashtable();
			}

			MessageFile.messages.put(Integer.valueOf(id), message);
		}

	}

	/**
	 * retrieve the message associated with a given traceID
	 * 
	 * @param id
	 *            an int that is a trace identifier
	 * @return a message that contains the type of entry, the component for the
	 *         entry and the text to be printed
	 */
	static protected Message getMessageFromID(int id)
	{
		if (verMod >= 5.0) {
			TraceFormat.outStream
					.println("Trying to retrieve an old style message ("
							+ Util.formatAsHexString(id)
							+ ") from 5.0 or newer message file");
			return null;
		}
		if (messages == null) {
			messages = new Hashtable();
		}

		return (Message) messages.get(Integer.valueOf(id));
	}

	/**
	 * retrieve the message associated with a given traceID and component
	 * 
	 * @param id
	 *            an int that is a trace identifier
	 * @return a message that contains the type of entry, the component for the
	 *         entry and the text to be printed
	 */
	static protected Message getMessageFromID(String compName, int id)
	{
		if (verMod < 5.0) {
			TraceFormat.outStream
					.println("Trying to retrieve a new style message ("
							+ compName + "." + id
							+ ") from 1.1 or older message file");
			return null;
		}
		if (messages == null) {
			messages = new Hashtable();
		}

		String messageKey = new String(compName + "." + id);
		Message message = (Message) messages.get(messageKey);

		if (message == null) {
			if (componentIsFromDATFile(compName)){
				/* this means the component exists, but the dat files in use do not contain the 
				 * format specification for this tracepoint in that component. The almost certain
				 * cause is that the tracepoint was introduced into a newer level of VM than the
				 * current dat file came from.
				 */
				TraceFormat.outStream.println(
						"Error: " + compName + "." + id + " not in dat file: dat files old or from incorrect VM."
				);
				/* adding a message means this tracepoint will only be flagged as an error once, the formatted file
				 * will still proceed, but all new tracepoints will have catch all error message below. User should
				 * rerun with newer dat files to get full tracepoint info. Those just needing some trace info will
				 * get a complete tracefile, with errors flagged inline.
				 */ 
				message = new Message(TraceRecord.ERROR_TYPE ,"This tracepoint's format string was not available in dat file.", compName, " ") ;
			} else {
				/* this is almost certainly an AppTrace tracepoint */
				message = new Message(TraceRecord.APP_TYPE, "%s", compName, "%s");
			}
			messages.put(messageKey, message);
		}

		return message;
	}
	
	static public boolean componentIsFromDATFile(String componentName){
		String componentCameFromDATFileEntry = componentName + fromDATFile;
		if (MessageFile.messages != null && MessageFile.messages.containsKey(componentCameFromDATFileEntry)){
			return true;
		} else {
			return false;
		}
	}
}
