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

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.nio.ByteOrder;

/* class to represent and parse the data in a TraceRecord that represents a tracepoint */
public class TracePointImpl implements TracePoint {
	TraceContext context;

	String componentName = null;
	String containerComponentName = null;
	int tracepointID = -1;
	private byte[] parameterData = null;
	TraceThread thread = null;
	int tracepointLength = -1;
	private String padding = null;

	private static String compNamePadding = "          ";
	private static String tpIDPadding = "    ";

	private boolean isNormalTracepoint = true;
	private boolean isInvalid = false;
	long lostRecordCount = 0;
	private long newTimerUpperWord = 0;

	long time_lowerWord;
	// long time_upperWord;
	BigInteger time_merged = BigInteger.ZERO;
	private Message message;

	byte debugData[];
	TracePointDebugInfo debugInfo;

	/** 
	 *
	 */
	public TracePointImpl(TraceContext context, ByteStream stream, TraceThread thread) {
		/* do preliminary parse on tracepoint */
		this.context = context;
		this.thread = thread;

		parseDataIntoTracepoint(stream);

		if (context.getPointerSize() == 4) {
			padding = "            ";
		} else {
			padding = "                    ";
		}
	}

	private TracePointImpl parseDataIntoTracepoint(ByteStream stream) {
		int length = stream.get() & 0xff;
		tracepointLength = length;
		boolean skipped = false;

		/* do some initial sanity checks on this tracepoint */
		if (length == 0) {
			context.error(this, "found a 0 byte length tracepoint on thread " + thread);
			isInvalid = true;
			isNormalTracepoint = false;
			return this;
		}

		/*
		 * TODO: we should implement a slide here so that if we've
		 * started looking at an invalid trace point then we just slide
		 * along the stream, byte at a time until we get something that
		 * looks like a valid trace point. Otherwise once the lengths in
		 * the data are screwed up we're likely to lose good with the
		 * bad.
		 */
		if (context.debugLevel > 0 && tracepointLength != 4) {
			debugData = new byte[length - 1];
			stream.get(debugData);
			stream = context.createByteStream(debugData);
			if (context.debugStream != null && context.debugLevel >= 5) {
				for (int i = 0; i < debugData.length; i++) {
					context.debugStream.print(debugData[i] + ", ");
				}
				context.debugStream.println();
			}

			// We've already skipped over this trace point in the main stream by reading
			// in the debug info.
			skipped = true;
		}

		byte id[] = new byte[3];
		stream.get(id);

		/*
		 * check for the long trace record trace point. this is a complex
		 * one that tells the formatter that the previous trace point
		 * (i.e. the one before this one in the trace record will have
		 * the length denoted by this special trace point. The long
		 * trace point indicator is that the length of the current
		 * trace point is 4 (i.e. UT_TRC_EXTENDED_LENGTH). data is < 0x00
		 * ^ (entryLength >> 8) ^ UT_TRC_EXTENDED_LENGTH
		 */
		if (tracepointLength == 4) {
			byte high = stream.get();
			if (id[1] == 0 && id[2] == 0) {
				length = ((high & 0xff) << 8) | (id[0] & 0xff);
				tracepointLength = length;

				/* populate the debug info now we have the actual length */
				if (context.debugLevel > 0) {
					debugData = new byte[length - 1];
					stream.get(debugData);
					stream = context.createByteStream(debugData);
					if (context.debugStream != null && context.debugLevel >= 5) {
						for (int i = 0; i < debugData.length; i++) {
							context.debugStream.print(debugData[i] + ", ");
						}
						context.debugStream.println();
					}
					skipped = true;
				}

				/* and read in the actual id for the trace point */
				stream.get(id);
			} else {
				context.error(this, "found a 4 byte length tracepoint, but it's center bytes are not null");
				isInvalid = true;
				isNormalTracepoint = false;
				return null;
			}
		}

		tracepointID = ((id[0] << 16) & 0xff0000) | ((id[1] << 8) & 0xff00) | (id[2] & 0xff);
		tracepointID &= 0x3FFF; /* work around for comp id coded into cmvc */

		/*
		 * if this trace point has a length of 8 and is the first
		 * trace point in the buffer then we check to see if it's a lost
		 * record trace point.
		 * 
		 * This trace point could well have overwritten the end of
		 * trace point spanning from the previous buffer because of the
		 * assumption by the lost record code that it is an empty buffer
		 * the first time someone tries to write a trace point to it and
		 * ignores the possibility of overflow.
		 */

		/*
		 * special lost record trace point == 0x0010nnnn8 == < 0 ^
		 * UT_TRC_LOST_COUNT_ID ^ nnn ^ 8 > where nnn is the number of
		 * lost records they should always be the first trace point in a
		 * buffer too, but we can't really check that here as we don't
		 * really know where we are in the buffer in this class
		 */
		if (tracepointLength == 8) {
			if (id[0] == 0x0 && id[1] == 0x1 && id[2] == 0x0) {
				/* get the number of lost records */
				lostRecordCount = stream.getUnsignedInt();

				/* check to see if this is an injected lost record trace point with zero records */
				if (lostRecordCount == 0) {
					lostRecordCount = -1;
				}

				isNormalTracepoint = false;
				return this;
			} else if (tracepointID == 0) {
				/* if the tracepointID == 0x000 == UT_TRC_SEQ_WRAP_ID then we have a sequence wrap tracepoint */
				isNormalTracepoint = false;

				/* this is the new upper word for all trace points in the rest of this buffer */
				long sequence = stream.getUnsignedInt();
				newTimerUpperWord = sequence;
				if (context.debugStream != null) {
					context.debug(this, 4, "Sequence wrap found, value = 0x" + Long.toString(sequence, 16));
				}
				return this;
			} else {
				context.error(this, "Special tracepoint (length is 8 bytes) but not sequence wrap or lost record, id = [" + id[0] + "," + id[1] + "," + id[2] + "]");

				/* swallow the next 4 bytes to get to the next trace point */
				if (!skipped) {
					stream.skip(4);
				}
				isInvalid = true;
				return this;
			}
		}

		/* We have a regular tracepoint. The tracepoint length should be at least 8 bytes at this point */
		if (tracepointLength < 8) {
			context.warning(this, "TracePointImpl.parseDataIntoTracepoint() detected invalid tracepoint length " + tracepointLength + " on thread " + thread);
			isInvalid = true;
			isNormalTracepoint = false;
			return this;
		}
		
		/* read in the timestamp */
		time_lowerWord = stream.getUnsignedInt();

		/* read and sanity check the component name length */
		int compNameLength = stream.getInt();
		if (compNameLength <= 0 || compNameLength > tracepointLength - 8) {
			context.warning(this, "TracePointImpl.parseDataIntoTracepoint() detected bad component name length: " + compNameLength);
			if (!skipped) {
				stream.skip(tracepointLength - 8);
			}
			isInvalid = true;
			isNormalTracepoint = false;
			return this;
		}

		/* read and sanity check the component name string */
		byte nameBytes[] = new byte[compNameLength];
		stream.get(nameBytes);
		componentName = componentIntern(nameBytes, 0, compNameLength);
		if (componentName == null || componentName.length() == 0) {
			/* this is a symptom of an incomplete data tracepoint */
			/*
			 * just refuse to process for now - probably partially
			 * overwritten during buffer wrap
			 */
			context.warning(this, "TracePointImpl.parseDataIntoTracepoint() detected bad component name");
			if (!skipped) {
				stream.skip(tracepointLength - 8);
			}
			isInvalid = true;
			isNormalTracepoint = false;
			return this;
		}

		if (componentName.equals("INTERNALTRACECOMPONENT")) {
			componentName = "dg";
		} else {
			tracepointID -= 257;
		}

		int openBracketIndex = componentName.indexOf("(");
		if (openBracketIndex >= 0) {
			/*
			 * name is of format
			 * componentName(containerComponentName)
			 */
			int closeBracketIndex = componentName.indexOf(")");
			if ((closeBracketIndex < 0) || (closeBracketIndex < openBracketIndex)) {
				/* data corrupt - just use whatever there is */
				context.debug(this, 3, "Overriding closeBracketIndex - not found in [" + componentName + "]");
				closeBracketIndex = componentName.length();
			}
			containerComponentName = componentName.substring(openBracketIndex + 1, closeBracketIndex);
			String newComponentName = componentName.substring(0, openBracketIndex);
			componentName = newComponentName;
		}

		/* add the rest of the bytes as the parmData */
		int parmDataStartsAt = 12 + (int) compNameLength;
		int parmDataLength = tracepointLength - parmDataStartsAt;

		if (parmDataLength > 0) {
			parameterData = new byte[parmDataLength];
			stream.get(parameterData);
		}

		message = context.messageFile.getMessageFromID(componentName, tracepointID);
		/*
		 * TODO: we should fix this. Maybe add app trace
		 * components/formatting data to the trace output so that we can
		 * actually validate them correctly rather than making an
		 * assumption that if we can't find the tracepoint in the dat
		 * file it must be user generated
		 */
		for (int i = 0; message.getType() == TracePoint.APP_TYPE && i < context.auxiliaryMessageFiles.size(); i++) {
			MessageFile file = (MessageFile)context.auxiliaryMessageFiles.get(i);
			if (context.debugStream != null) {
				context.debug(this, 4, "Looking in auxiliary message file " + file.toString() + " for tracepoint " + componentName + "." + tracepointID);
			}
			message = file.getMessageFromID(componentName, tracepointID);
		}
		
		if (message.getType() == TracePoint.APP_TYPE && !componentName.equals("ApplicationTrace")) {
			if (parmDataLength > 0) {
				/* This is going to output any parameter data as raw data so it takes a precision string.
				 * Add in the required length parameter
				 */
				byte newParmData[] = new byte[parmDataLength + 2];
				System.arraycopy(parameterData, 0, newParmData, 2, parameterData.length);
				parameterData = newParmData;
	
				short len = (short)parmDataLength;
				if (stream.order() == ByteOrder.LITTLE_ENDIAN) {
					parameterData[0] = (byte)(0xFF & len);
					parameterData[1] = (byte)(0xFF00 & len);
				} else {
					parameterData[0] = (byte)(0xFF00 & len);
					parameterData[1] = (byte)(0xFF & len);
				}
			} else {
				message = new Message(TracePoint.APP_TYPE, "trace point not present in dat file", tracepointID, -1, componentName, "", context);
			}
		}

		if (message != null) {
			message.addStatistic("bytes", tracepointLength);
			message.addStatistic("count", 1);
		}

		return this;
	}

	long getLostRecordCount() {
		return lostRecordCount;
	}

	final static String internedNames[] = { "j9vm", "j9mm", "j9jit", "j9bcu", "j9jcl", "j9scar", "j9bcverify" };

	final static byte internedNamesAsBytes[][];
	static {
		internedNamesAsBytes = new byte[internedNames.length][];
		int l = internedNames.length;

		for (int i = 0; i < l; i++) {
			String current = internedNames[i];
			int strlen = current.length();
			internedNamesAsBytes[i] = new byte[strlen];
			for (int j = 0; j < strlen; j++) {
				internedNamesAsBytes[i][j] = (byte) current.charAt(j);
			}
		}
	}

	private boolean internListCompare(byte traceData[], int offset, int internedNamesIndex, int length) {
		int l = internedNamesAsBytes[internedNamesIndex].length;
		int j;
		
		/* check the lengths match */
		if (l != length) {
			return false;
		}

		for (j = 0; j < l; j++) {
			if (traceData[offset + j] != internedNamesAsBytes[internedNamesIndex][j]) {
				return false;
			}
		}

		if (j != traceData.length) {
			return false;
		}

		return true;
	}

	private String componentIntern(byte traceData[], int offset, int length) {
		int s = internedNames.length;
		for (int i = 0; i < s; i++) {
			if (internListCompare(traceData, offset, i, length) == true) {
				return internedNames[i];
			}
		}
		String t;
		try {
			t = new String(traceData, offset, length, "UTF-8");
		} catch (UnsupportedEncodingException e) {
			// UTF-8 will be supported
			t = new String(traceData, offset, length);
		}
		return t;
	}

	public String toString() {
		StringBuffer tpFormatted = new StringBuffer();

		String threadIDString = "0x" + Long.toString(thread.getThreadID(), 16);
		tpFormatted.append(padding.substring(threadIDString.length()));
		tpFormatted.append(threadIDString);

		if (componentName.length() < compNamePadding.length()) {
			tpFormatted.append(compNamePadding.substring(componentName.length()));
		}
		tpFormatted.append(" ");
		tpFormatted.append(componentName);

		if (containerComponentName != null) {
			tpFormatted.append("(" + containerComponentName + ")");
		}

		tpFormatted.append(".");
		tpFormatted.append(tracepointID);
		if ((Integer.toString(tracepointID)).length() < tpIDPadding.length()) {
			tpFormatted.append(tpIDPadding.substring((Integer.toString(tracepointID)).length()));
		}

		return tpFormatted.toString();
	}

	public int getTPID() {
		return tracepointID;
	}

	public String getComponentName() {
		return componentName;
	}

	public String getContainerComponentName() {
		return containerComponentName;
	}

	public TraceThread getThread() {
		return thread;
	}

	public String getParameterDataFormatted() {
		Message msg = this.message;
		byte[] parmData = getParmData();
		String formattedData = null;
		if (parmData == null) {
			parmData = new byte[1];
			try {
				formattedData = msg.getMessage(parmData, 0, 0);
			} catch (Exception e) {
				formattedData = "FORMATTING PROBLEM OCCURRED WHILE PROCESSING THE RAW DATA FOR THIS TRACEPOINT - PARAMETER DATA UNAVAILABLE";
			}
		} else {
			try {
				formattedData = msg.getMessage(parmData, 0, parmData.length);
			} catch (Exception e) {
				formattedData = "FORMATTING PROBLEM OCCURRED WHILE PROCESSING THE RAW DATA FOR THIS TRACEPOINT - PARAMETER DATA MISMATCH";
			}
		}

		return formattedData;
	}

	public byte[] getParmData() {
		return parameterData;
	}

	public BigInteger getRawTime() {
		return time_merged;
	}

	public long getNewTimerUpperWord() {
		return newTimerUpperWord;
	}

	public boolean isNormalTracepoint() {
		return isNormalTracepoint;
	}

	public String getFormattedParameters() {
		return getParameterDataFormatted();
	}

	public String getFormattedTime() {
		return context.getFormattedTime(getRawTime());
	}

	public boolean isInvalid() {
		return isInvalid;
	}

	public int getTypeAsInt() {
		return message.getType();
	}

	/* methods implementing the com.ibm.jvm.trace.TracePoint interface */
	public int getID() {
		return getTPID();
	}

	public long getTimestampMillis() {
		return 0L;
	}

	public int getMicrosecondsCount() {
		return 0;
	}

	public String getComponent() {
		return getComponentName();
	}

	public String getContainerComponent() {
		return getContainerComponentName();
	}

	public String getParameterFormattingTemplate() {
		return message.getFormattingTemplate();
	}

	public Object[] getParameters() {
		try {
			return message.parseMessage(getParmData(), 0);
		} catch (RuntimeException e) {
			RuntimeException n = new RuntimeException("Tracepoint: "+this.componentName+"."+this.tracepointID+", template: "+this.getParameterFormattingTemplate(), e);
			n.setStackTrace(e.getStackTrace());
			throw n;
		}
	}

	public String[] getGroups() {
		return null;
	}

	public int getLevel() {
		return -1;
	}

	public String getType() {
		int type = message.getType();
		return TracePoint.types[type];
	}

	public String getDebugInfo() {
		if (debugInfo != null) {
			return "Record: "+debugInfo.record+", offset: "+debugInfo.offset;
		}
		
		return "";
	}
}
