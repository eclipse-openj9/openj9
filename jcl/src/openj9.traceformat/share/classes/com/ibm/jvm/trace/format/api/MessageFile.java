/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
package com.ibm.jvm.trace.format.api;

import java.io.*;
import java.util.HashMap;
import java.util.Iterator;

/**
 * Acts as a template for mapping trace ids to messages.
 * 
 * @author Jonathon Lee
 */
final public class MessageFile {
	TraceContext context;

	private BufferedReader reader;

	private String formatName;

	protected float verMod = 0;

	private HashMap messages = new HashMap();

	private HashMap componentList = new HashMap();;

	private final String fromDATFile = ".definedInDatFile";
	private boolean processingDATFile = false;

	int counter = 0;

	String savedComponentName = "not_a_component";
	Component savedComponent = null;

	public HashMap getStatistics() {
		HashMap stats = new HashMap();
		
		Iterator mItr = messages.keySet().iterator();
		while (mItr.hasNext()) {
			Object mKey = mItr.next();
			Message msg = (Message)messages.get(mKey);		
			if (msg != null) {
				stats.put(mKey, msg.getStatistics());
			}
		}

		Iterator cItr = componentList.keySet().iterator();
		while (cItr.hasNext()) {
			Object cKey = cItr.next();
			Component comp = (Component)componentList.get(cKey);
			if (comp != null) {
				int id = 0;
				for (id = 0; id < comp.messageList.size(); id++) {
					Message msg = (Message)comp.messageList.get(id);
					if (msg != null) {
						stats.put(comp.getName()+"."+id, msg.getStatistics());
					}
				}
			}
		}

		return stats;
	}

	private void initialize(TraceContext context, InputStream stream) throws IOException {
		this.context = context;

		reader = new BufferedReader(new InputStreamReader(stream));
		read();
	}

	private MessageFile(TraceContext context, File file) throws IOException {
		this.formatName = file.getName();
		initialize(context, new FileInputStream(file));
	}

	private MessageFile(TraceContext context, InputStream stream) throws IOException {
		initialize(context, stream);
	}

	public float getVersion() {
		return verMod;
	}

	private void read() throws IOException {
		String headerLine = reader.readLine();
		if (headerLine == null) {
			throw new IOException("file is empty");
		}
		readHeader(headerLine);
		processingDATFile = true;
		try {
			String line = reader.readLine();
			if (verMod >= 5.0) {
				while (line != null && line.length() != 0) {
					addMessage_50(line);
					line = reader.readLine();
				}
			} else {
				while (line != null) {
					addMessage_12(line);
					line = reader.readLine();
				}
			}
		} finally {
			processingDATFile = false;
		}
	}

	private void failParse(String description, String badLine) throws IOException {
		context.error(this, "File: '" + formatName + "' bad.  Reason: " + description + "Contents:  <" + badLine + ">");
		throw new IOException(description);
	}

	private void readHeader(String messageLine) throws IOException {
		int firstSpace = messageLine.indexOf(" ");
		if (firstSpace != -1) {
			failParse("Trace format file '" + formatName + "' header malformed.", messageLine);
		}
		try {
			verMod = Float.valueOf(messageLine).floatValue();
		} catch (Exception e) {
			if (messageLine.equals("dg")) {
				failParse("Trace format file '" + formatName + "' obsolete, no version number.", "");
			} else {
				failParse("Trace format file '" + formatName + "' appears to be corrupted.", messageLine);
			}
		}
	}

	/**
	 * adds a message to the table by parsing a line of the message file. If
	 * the line has no spaces, it is determined to be a component; otherwise
	 * it contains a trace id, an entry type, and a template string
	 * (separated by spaces)
	 * 
	 * @param messageLine
	 *                a string
	 */
	protected void addMessage(String messageLine) {
		addMessage_12(messageLine);
	}

	/**
	 * adds a message to the table by parsing a line of the message file. If
	 * the line has no spaces, it is determined to be a component; otherwise
	 * it contains a trace id, an entry type, and a template string
	 * (separated by spaces)
	 * 
	 * @param messageLine
	 *                a string
	 */
	private void addMessage_50(String messageLine) {
		int firstSpace = messageLine.indexOf(" ");
		int secondSpace = messageLine.indexOf(" ", firstSpace + 1);
		int thirdSpace = messageLine.indexOf(" ", secondSpace + 1);
		int fourthSpace = messageLine.indexOf(" ", thirdSpace + 1);
		int fifthSpace = messageLine.indexOf(" ", fourthSpace + 1);
		int sixthSpace = messageLine.indexOf(" ", fifthSpace + 1);

		String componentName = null;
		int tpNumber = -1;
		String symbol = null;
		String msg = null;

		componentName = new String(messageLine.substring(0, firstSpace));

		/*
		 * version 5.1 and above contain the component and trace id
		 * together as a period separated pair: e.g. j9vm.123
		 */
		if (verMod >= 5.1F) {
			int period = componentName.indexOf('.');
			if (period == -1) {
				/* ignore */
			} else {
				String compName = componentName.substring(0, period);
				String tpNum = componentName.substring(period + 1);
				tpNumber = Integer.parseInt(tpNum);
				componentName = compName;
			}
		}

		if (!componentName.equals(savedComponentName)) {
			savedComponentName = componentName;

			savedComponent = (Component) componentList.get(componentName);
			if (savedComponent == null) {
				savedComponent = new Component(componentName);
				componentList.put(componentName, savedComponent);
				messages.put(componentName+fromDATFile, null);
				savedComponent.setBase(0);
			}
			counter = 0;
		}

		int id = counter++;
		if (tpNumber >= 0) {
			if (id != tpNumber) {
				throw new Error("Tracepoint number out of order: component = " + componentName + ", tpid = " + id + ", in file '" + formatName + "'");
			}
		}

		int type = Integer.parseInt(messageLine.substring(firstSpace + 1, secondSpace));
		int level = Integer.parseInt(messageLine.substring(thirdSpace + 1, fourthSpace));
		symbol = messageLine.substring(fifthSpace + 1, sixthSpace);
		msg = messageLine.substring(sixthSpace + 2, messageLine.length() - 1);

		Message message = new Message(type, msg, id, level, componentName, symbol, context);
		savedComponent.addMessage(message, id);
	}

	/**
	 * adds a message to the table by parsing a line of the message file. If
	 * the line has no spaces, it is determined to be a component; otherwise
	 * it contains a trace id, an entry type, and a template string
	 * (separated by spaces)
	 * 
	 * @param messageLine
	 *                a string
	 */
	private void addMessage_12(String messageLine) {
		int firstSpace = messageLine.indexOf(" ");
		int secondSpace = messageLine.indexOf(" ", firstSpace + 1);
		int thirdSpace = messageLine.indexOf(" ", secondSpace + 1);

		String componentName = null;
		String symbol = null;
		String msg = null;

		/* old behaviour required must be a 1.1 or similar trace file */
		int id = Integer.parseInt(messageLine.substring(0, firstSpace), 16);
		int type = Integer.parseInt(messageLine.substring(firstSpace + 1, secondSpace));
		int level = 1;
		if (verMod < (float) 1.2) {
			symbol = messageLine.substring(secondSpace + 1, thirdSpace);
			msg = messageLine.substring(thirdSpace + 2, messageLine.length() - 1);
		} else {
			int fourthSpace = messageLine.indexOf(" ", thirdSpace + 1);
			int fifthSpace = messageLine.indexOf(" ", fourthSpace + 1);
			int sixthSpace = messageLine.indexOf(" ", fifthSpace + 1);
			level = Integer.parseInt(messageLine.substring(thirdSpace + 1, fourthSpace));
			symbol = messageLine.substring(fifthSpace + 1, sixthSpace);
			msg = messageLine.substring(sixthSpace + 2, messageLine.length() - 1);
		}

		Message message = new Message(type, msg, id, level, componentName, symbol, context);

		messages.put(new Integer(id), message);
	}

	/**
	 * retrieve the message associated with a given traceID
	 * 
	 * @param id
	 *                an int that is a trace identifier
	 * @return a message that contains the type of entry, the component for
	 *         the entry and the text to be printed
	 */
	protected Message getMessageFromID(int id) {
		if (verMod >= 5.0) {
			context.error(this, "Trying to retrieve an old style message (" + "0x" + Long.toString(id) + ") from 5.0 or newer message file");
			return null;
		}

		return (Message) messages.get(new Integer(id));
	}

	/**
	 * retrieve the message associated with a given traceID and component
	 * 
	 * @param id
	 *                an int that is a trace identifier
	 * @return a message that contains the type of entry, the component for
	 *         the entry and the text to be printed
	 */
	synchronized protected Message getMessageFromID(String compName, int id) {
		Message message;

		if (verMod < 5.0) {
			context.error(this, "Trying to retrieve a new style message (" + compName + "." + id + ") from 1.1 or older message file");
			return null;
		}

		Component component = (Component) componentList.get(compName);
		if (component != null) {
			message = component.getMessageByID(id);
		} else {
			String messageKey = new String(compName + "." + id);
			message = (Message) messages.get(messageKey);
		}

		if (message == null) {
			if (componentIsFromDATFile(compName)) {
				/*
				 * this means the component exists, but the dat
				 * files in use do not contain the format
				 * specification for this tracepoint in that
				 * component. The almost certain cause is that
				 * the tracepoint was introduced into a newer
				 * level of VM than the current dat file came
				 * from.
				 */
				context.error(this, compName + "." + id + " not in dat file: corrupted trace point or dat file mismatch against source VM.");
				/*
				 * adding a message means this tracepoint will
				 * only be flagged as an error once, the
				 * formatted file will still proceed, but all
				 * new tracepoints will have catch all error
				 * message below. User should rerun with newer
				 * dat files to get full tracepoint info. Those
				 * just needing some trace info will get a
				 * complete tracefile, with errors flagged
				 * inline.
				 */
				message = new Message(TracePoint.ERROR_TYPE, "This tracepoint's format string was not available in dat file.", id, -1, compName, " ", context);
			} else if (compName.equals("ApplicationTrace")) {
				/*
				 * this is almost certainly an AppTrace
				 * tracepoint
				 */
				message = new Message(TracePoint.APP_TYPE, "%s", id, -1, compName, "%s", context);
			} else {
				/*
				 * this is either an AppTrace trace point or one for which we can't find an entry in the data file.
				 * We can't be sure that any parameter data will be a usable string so we note that it's raw data in the message
				 */
				message = new Message(TracePoint.APP_TYPE, "raw parameter data [%.*s]", id, -1, compName, "%s", context);
			}
			if (component != null && id >= 0 ) {
				component.addMessage(message, id);
			} else {
				String msg = new String(compName + "." + id);
				messages.put(msg, message);
			}
		}

		return message;
	}

	public boolean componentIsFromDATFile(String componentName) {
		if (this.messages != null && this.messages.containsKey(componentName + fromDATFile)) {
			return true;
		} else {
			return false;
		}
	}

	public static MessageFile getMessageFile(InputStream stream, TraceContext context) throws IOException {
		return new MessageFile(context, stream);
	}

	public static MessageFile getMessageFile(File file, TraceContext context) throws IOException {
		return new MessageFile(context, file);
	}
}
