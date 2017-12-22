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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.jvm.format;

import java.math.BigInteger;

/* class to represent and parse the data in a TraceRecord that represents a tracepoint */
public class TracePoint implements Comparable, com.ibm.jvm.trace.TracePoint
{
	private static final String CHANGE_OF_THREAD_INDICATOR = "*";
	private String componentName = null;
	private String containerComponentName = null;
	private int tracepointID = -1;
	private byte[] parameterData = null; // the binary data representing the parameters
	private BigInteger rawTimeStamp = null;
	private long threadID = -1;
	private boolean isChangeOfThread = false;
	private int tplen = -1; // the length in total of this tracepoint in its binary representation
	private String padding = null;
	private static String compNamePadding = "          ";
	private static String tpIDPadding = "    ";
	private boolean isLongTracePoint = false;
	private int longTracePointLength = 0; // if this is a long tracepoint, this number is the number of bytes in the 

	// buffer that constitute the actual tracepoint (will be the next previous
	// block).	
	private boolean isNormalTracepoint = true; //  means the same thing as isNotInternalTraceEngineTracePoint!
	private boolean isInvalid = false; // a flag that is set if the tracepoint contains any surprises.
	private boolean isLostRecord = false;
	private long lostRecordCount = 0;

	// hopefully it will be possible to write a parser that takes a look
	// at all the isInvalid tracepoints and makes a call about what was wrong with them
	// or at least gives the debugger an idea of where they are in the tracefile.	
	private long startOffsetInTraceFile = -1; //
	private long endOffsetInTraceFile = -1; //
	private boolean fragmented = false; // if true, this tracepoint was created from non-contiguous data in the binary trace file.
	private String nameOfTraceFile = null;
	private boolean isNewTimerUpperWord = false;
	private long newTimerUpperWord = -1;
	BigInteger upperWord = null;

	/** 
	 *
	 */
	public TracePoint(byte[] rawTracePoint, int length, BigInteger upperWord,
			long threadID, String fileName, long offsetOfStartOfTracePointInFile, long offsetOfEndOfTracePointInFile, boolean wasFragmented)
	{
		/* do preliminary parse on tracepoint */
		this.upperWord = upperWord;
		this.threadID = threadID;
		this.nameOfTraceFile = fileName;
		this.startOffsetInTraceFile = offsetOfStartOfTracePointInFile;
		this.endOffsetInTraceFile = offsetOfEndOfTracePointInFile;
		this.fragmented = wasFragmented;

		parseDataIntoTracepoint(rawTracePoint, length);

		if (Integer.valueOf(Util.getProperty("POINTER_SIZE")).intValue() == 4) {
			padding = "            ";
		} else {
			padding = "                    ";
		}
	}

	private TracePoint parseDataIntoTracepoint(byte[] rawTracePoint, int length)
	{
		/* do some initial sanity checks on this tracepoint */
		if (length == 0) {
			isInvalid = true;
			isNormalTracepoint = false;
			return this;
		}
		if (rawTracePoint == null) {
			TraceFormat.outStream
					.println("TracePoint passed null data array - can't extract tracepoint from it. "
							+ from());
			isInvalid = true;
			isNormalTracepoint = false;
			return this;
		}

		tplen = length;
		if (tplen == 0) {
			TraceFormat.outStream
					.println("Tracepoint extracted zero from first byte of trace data - returning without creating tracepoint "
							+ from());
			isInvalid = true;
			isNormalTracepoint = false;
			return this;
		}

		if (rawTracePoint.length < length) {
			Util.Debug
					.println("Partial buffer passed in - most likely partly overwritten during a buffer wrap");
			isInvalid = true;
			isNormalTracepoint = false;
			return this;
		}

		tracepointID = Util.constructTraceID(rawTracePoint, 1);
		tracepointID &= 0x3FFF; /* work around for comp id coded into cmvc */

		/* special lost record tracepoint == 0x0010nnnn8 == < 0 ^ UT_TRC_LOST_COUNT_ID ^ nnn ^ 8 > where nnn is 
		 * the number of lost records they should always be the first tracepoint in a buffer too, but we can't 
		 * really check that here as we don't really know where we are in the buffer in this class */
		if (rawTracePoint[0] == 0x0 && rawTracePoint[1] == 0x0
				&& rawTracePoint[2] == 0x1 && rawTracePoint[3] == 0x0
				&& tplen == 8) {
			Util.Debug
					.println("Lost Record TracePoint detected, should have been handled by TraceRecord50 "
							+ from());
			/* get the number of lost records */
			isNormalTracepoint = false;
			isLostRecord = true;
			lostRecordCount = Util.constructUnsignedInt(rawTracePoint, 4);
			return this;
		}

		/* if the tracepointID == 0x000 == UT_TRC_SEQ_WRAP_ID then we have a sequence wrap tracepoint */
		if (tracepointID == 0) {
			isNormalTracepoint = false;
			if (tplen != 8) {
				Util.Debug
						.println("Possible inconsistency in buffer - tracePointID 0 detected but length = "
								+ tplen);
				Util.Debug
						.println("Ignoring this tracepoint for now - inspect tracefile for further details "
								+ from());
				isInvalid = true;
				return this;
			}
			/* act on the sequence wrap */
			long sequenceWrap = Util.constructUnsignedInt(rawTracePoint, 4); /* this is the new upper word for all 
			 tracepoints in the rest of this buffer */
			isNormalTracepoint = false; /* although this is an internal tracepoint, it needs to end up in all the arrays as its location is important! */
			isNewTimerUpperWord = true;
			newTimerUpperWord = sequenceWrap;
			Util.Debug.println("Sequence wrap found, value = " + sequenceWrap
					+ " " + from());
			return this;
		}

		/* the next type of special entry to check for is the long trace record tracepoint
		 * this is a complex one that tells the formatter that the previous tracepoint (i.e. the one before
		 * this one in the trace record will have the length denoted by this special tracepoint.
		 * The long tracepoint indicator is that the length of the current tracepoint is 4 (i.e.
		 * UT_TRC_EXTENDED_LENGTH).
		 * data is < 0x00 ^ (entryLength >> 8) ^ UT_TRC_EXTENDED_LENGTH */
		if (tplen == 4) {
			if (rawTracePoint[1] == 0 && rawTracePoint[2] == 0) {
				Util.Debug.println("Long entry data");
				int longEntryID = Util.constructUnsignedByte(rawTracePoint, 3); /* the top half of the length short */
				int longEntryLength = Util.constructUnsignedByte(rawTracePoint,
						0); /* the bottom half of the length short */
				longTracePointLength = longEntryLength | (longEntryID << 8);
				Util.Debug.println(" longEntryID = " + longEntryID
						+ " length = " + longTracePointLength + " " + from());

				isLongTracePoint = true;
				componentName = "longtracepoint id";
				isNormalTracepoint = false;
				return this;
			} else {
				Util.Debug
						.println("Found a 4 byte length tracepoint that doesn't seem to contain a long tracepoint marker"
								+ from());
				Util.Debug
						.println("Incorrect long tracepoint found - bytes 1 and 2 aren;t zero (as they should be):");
				Util.Debug.println("    byte 1 == "
						+ Byte.toString(rawTracePoint[1]) + " byte 2 == "
						+ Byte.toString(rawTracePoint[2]));
				isInvalid = true;
				isNormalTracepoint = false;
				return null;
			}
		}

		/* else we have a bona fide tracepoint ! */

		/* read in the timestamp */
		BigInteger tempLower = Util.constructUnsignedLong(rawTracePoint, 4,
				Util.INT);
		rawTimeStamp = upperWord.shiftLeft(32).or(tempLower);

		/* read and sanity check the component name length */
		long compNameLength = Util.constructUnsignedInt(rawTracePoint, 8);
		if (compNameLength <= 0 || compNameLength > tplen) {
			Util.Debug.println("TracePoint.parseDataIntoTracepoint() detected bad component name length: " + from());
			isInvalid = true;
			isNormalTracepoint = false;
			return this;
		}

		/* read and sanity check the component name string */
		componentName = Util.constructString(rawTracePoint, 12, (int)compNameLength);
		if (componentName == null || componentName.length() == 0) {
			/* this is a symptom of an incomplete data tracepoint */
			/* just refuse to process for now - probably partially overwritten during buffer wrap */
			Util.Debug.println("TracePoint.parseDataIntoTracepoint() detected bad component name: " + from());
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
		if (openBracketIndex >= 0){
			/* name is of format componentName(containerComponentName) */
			int closeBracketIndex = componentName.indexOf(")");
			if ((closeBracketIndex < 0) || (closeBracketIndex < openBracketIndex)){
				/* data corrupt - just use whatever there is */
				Util.Debug.println("Overriding closeBracketIndex - not found in [" + componentName + "]");
				closeBracketIndex = componentName.length();
			}
			containerComponentName = componentName.substring(openBracketIndex + 1, closeBracketIndex);
			String newComponentName = componentName.substring(0, openBracketIndex);
			componentName = newComponentName;
		}
		
		/* add the rest of the bytes as the parmData */
		int parmDataStartsAt = 12 + (int) compNameLength;
		int parmDataLength = tplen - parmDataStartsAt;

		if (parmDataLength > 0) {
			parameterData = new byte[parmDataLength];
			System.arraycopy(rawTracePoint, parmDataStartsAt, parameterData, 0,
					parmDataLength);
		}
		return this;
	}

	public boolean isLongTracePoint()
	{
		return isLongTracePoint;
	}

	public int longTracePointLength()
	{
		if (isLongTracePoint) {
			return longTracePointLength;
		} else {
			Util.Debug
					.println("longTracePointLength called on a completed tracepoint");
			return 0;
		}
	}

	public String toString()
	{
		StringBuffer tpFormatted = new StringBuffer();

		String threadIDString = Util.formatAsHexString(threadID);
		/* If it's a change of thread, add on the asterisk and pad a bit less */
		if (isChangeOfThread) {
			tpFormatted.append(padding.substring(Math.min(threadIDString.length() + 1, padding.length())));
			tpFormatted.append(CHANGE_OF_THREAD_INDICATOR);
		} else {
			/* Otherwise, just pad */
			tpFormatted.append(padding.substring(Math.min(threadIDString.length(), padding.length())));
		}
		tpFormatted.append(threadIDString);

		String userVMID = TraceFormat.getUserVMIdentifier();
		if (userVMID != null) {
			tpFormatted.append(" " + userVMID);
		}

		if (isLostRecord) {
			tpFormatted.append(" "+lostRecordCount+" trace buffers of data were discarded for this thread");
			return tpFormatted.toString();
		}
		if (componentName.length() < compNamePadding.length()) {
			tpFormatted.append(compNamePadding
					.substring(componentName.length()));
		}
		tpFormatted.append(" ");
		tpFormatted.append(componentName);
		
		if (containerComponentName != null){
			tpFormatted.append("(" + containerComponentName + ")");
		}
		
		tpFormatted.append(".");
		tpFormatted.append(tracepointID);
		if ((Integer.toString(tracepointID)).length() < tpIDPadding.length()) {
			tpFormatted.append(tpIDPadding.substring((Integer
					.toString(tracepointID)).length()));
		}

		return tpFormatted.toString();
	}

	public String getFileName()
	{
		return nameOfTraceFile;
	}

	public long getOffsetInTraceFile()
	{
		return startOffsetInTraceFile;
	}

	public int getTPID()
	{
		return tracepointID;
	}

	public String getComponentName()
	{
		return componentName;
	}
	
	public String getContainerComponentName(){
		return containerComponentName;
	}

	public long getThreadID()
	{
		return threadID;
	}

	public String getParameterDataFormatted()
	{
		Message msg = MessageFile.getMessageFromID(getComponentName(), getTPID());
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
				formattedData = msg.getMessage(parmData, 0,
						parmData.length);
			} catch (Exception e) {
				formattedData = "FORMATTING PROBLEM OCCURRED WHILE PROCESSING THE RAW DATA FOR THIS TRACEPOINT - PARAMETER DATA UNAVAILABLE";
			}
		}
		
		return formattedData;
	}

	public byte[] getParmData()
	{
		return parameterData;
	}

	public BigInteger getRawTimeStamp()
	{
		return rawTimeStamp;
	}

	void setRawTimeStamp(BigInteger timestamp) {
		rawTimeStamp = timestamp;
	}

	public boolean isTimerUpperWord()
	{
		return isNewTimerUpperWord;
	}

	public long getNewTimerUpperWord()
	{
		if (isNewTimerUpperWord) {
			return newTimerUpperWord;
		} else {
			TraceFormat.outStream
					.println("an attempt to get a new timer upper word has been made on a tracepoint that doesn't seem to be a timer wrap!");
			TraceFormat.outStream.println(" tracepoint = " + tracepointID + "."
					+ componentName);
			return -1;
		}
	}

	public boolean setTracePointsUpperWord(BigInteger upperWord)
	{
		if (upperWord == null) {
			TraceFormat.outStream
					.println(" attempt made to set the upper word of tracepoint to a null - original tracepoint: "
							+ from());
			return false;
		}
		if (rawTimeStamp == null) {
			TraceFormat.outStream
					.println(" attempt made to set the upper word on a tracepoint whose timestamp is null - original tracepoint: "
							+ from());
			return false;
		}
		BigInteger temp = upperWord.shiftLeft(32).or(rawTimeStamp);
		rawTimeStamp = temp;
		return true;
	}

	public boolean isNormalTracepoint()
	{
		return isNormalTracepoint;
	}

	public boolean isLostRecord()
	{
		return isLostRecord;
	}

	public int compareTo(Object other)
	{
		if (rawTimeStamp == null) {
			TraceFormat.outStream
					.println("TracePoint compareTo called on TracePoint with no rawTimeStamp");
			return 0;
		} else if (((TracePoint) other).getRawTimeStamp() == null) {
			TraceFormat.outStream
					.println("TracePoint compareTo called with a TracePoint with no rawTimeStamp");
			return 0;
		}
		return rawTimeStamp.compareTo(((TracePoint) other).getRawTimeStamp());
	}

	public String getFormattedParameters()
	{
		return getParameterDataFormatted();
	}

	public String getFormattedTime()
	{
		return Util.getFormattedTime(getRawTimeStamp());
	}
	public void setIsChangeOfThread(boolean isChangeOfThread)
	{
		this.isChangeOfThread = isChangeOfThread;
	}
	public boolean isInvalid(){
		return isInvalid;
	}
	public int getTypeAsInt(){
		Message msg = MessageFile.getMessageFromID(getComponentName(), getTPID());
		return msg.getType();
	}
	
	/* methods implementing the com.ibm.jvm.trace.TracePoint interface */
	public int getID(){
		return getTPID();
	}
	public long getTimestampMillis(){
		return 0L;
	}
	public int getMicrosecondsCount(){
		return 0;
	}
	public String getComponent(){
		return getComponentName();
	}
	public String getContainerComponent(){
		return getContainerComponentName();
	}
	public String getParameterFormattingTemplate(){
		Message msg = MessageFile.getMessageFromID(getComponentName(), getTPID());
		return msg.getFormattingTemplate();
	}
	public Object[] getParameters(){
		return null;
	}
	public String[] getGroups(){
		return null;
	}
	public int getLevel(){
		return -1;
	}
	public String getType(){
		Message msg = MessageFile.getMessageFromID(getComponentName(), getTPID());
		int type = msg.getType();
		return TraceRecord.types[type];
	}
	public String from()
	{
		StringBuffer sb = new StringBuffer();
		
		sb.append("from: 0x");
		sb.append(Long.toHexString(startOffsetInTraceFile));
		sb.append("->0x");
		sb.append(Long.toHexString(endOffsetInTraceFile));
		sb.append(":" );
		
		if (fragmented){
			sb.append("F");
			if (startOffsetInTraceFile == TraceRecord50.INTERNAL_WRAP_SPLIT_TP){
				sb.append("I");
			} else if (startOffsetInTraceFile == TraceRecord50.EXTERNAL_WRAP_SPLIT_TP){
				sb.append("E");
			} else {
				sb.append("X");
			}
		} 
		sb.append(":" );
		sb.append(nameOfTraceFile);
		
		return sb.toString();
		
	}
	
}
