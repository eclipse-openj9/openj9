/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.nio.BufferUnderflowException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.StringTokenizer;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.jvm.trace.format.api.TraceContext;
import com.ibm.jvm.trace.format.api.TracePoint;
import com.ibm.jvm.trace.format.api.TracePointImpl;
import com.ibm.jvm.trace.format.api.TraceThread;

public class SnapFormatCommand extends SnapBaseCommand 
{
	private TraceContext traceContext = null;
	private InputStream[] messageFiles = new InputStream[2];
	private boolean outputHeader = true;
	
	private static final String[] MESSAGEFILENAMES = {"TraceFormat.dat","J9TraceFormat.dat"};
	private static final String[] DEFAULTMESSAGEFILEPATHS = new String[] {System.getProperty("user.dir"), System.getProperty("java.home") + File.separator + "lib"} ;
	private static final PrintStream dummyOut = new PrintStream(new OutputStream() {
		@Override
		public void write(int b) throws IOException 
		{
			// do nothing
		}
	});
	
	private static abstract class TraceFilterExpression
	{
		public abstract boolean matches(TracePoint tp);
		
		public static TraceFilterExpression parseExpression(String expression)
		{
			TraceFilterExpression result = null;

			StringTokenizer toker = new StringTokenizer(expression, "()&|!,", true);
			ArrayList<String> tokens = new ArrayList<String>();
			while (toker.hasMoreTokens()) {
				String token = toker.nextToken().trim();
				if (0 != token.length()) {
					if (token.equals(",")) {
						// comma is the same as OR
						tokens.add("|");
					} else {
						tokens.add(token);
					}
				}
			}
			result = parseExpression(tokens);
			if (null == result) {
				throw new IllegalArgumentException("Failed to parse filter expression: no result");
			}
			if (!tokens.isEmpty()) {
				throw new IllegalArgumentException("Failed to parse filter expression: unparsed text");
			}
			return result;
		}

		private static TraceFilterExpression parseExpression(ArrayList<String> tokens)
		{
			TraceFilterExpression result = null;

			result = parseTerm(tokens);
			while (!tokens.isEmpty()) {
				String token = tokens.get(0);
				if (token.equals("|") || token.equals(",")) {
					tokens.remove(0);
					TraceFilterExpression lhs = result;
					TraceFilterExpression rhs = parseTerm(tokens);
					if (null == rhs) {
						throw new IllegalArgumentException("Failed to parse filter expression: missing operand to OR");
					}
					result = new TraceFilterOrExpression(lhs, rhs);
				} else {
					break;
				}
			}
			
			if (null == result) { 
				throw new IllegalArgumentException("Failed to parse filter expression: expected expression");
			}
			return result;
		}
		
		private static TraceFilterExpression parseTerm(ArrayList<String> tokens)
		{
			TraceFilterExpression result = null;

			result = parsePrimary(tokens);
			while (!tokens.isEmpty()) {
				String token = tokens.get(0);
				if (token.equals("&")) {
					tokens.remove(0);
					TraceFilterExpression lhs = result;
					TraceFilterExpression rhs = parsePrimary(tokens);
					if (null == rhs) {
						throw new IllegalArgumentException("Failed to parse filter expression: missing operand to AND");
					}
					result = new TraceFilterAndExpression(lhs, rhs);
				} else {
					break;
				}
			}
			
			if (null == result) { 
				throw new IllegalArgumentException("Failed to parse filter expression: expected expression");
			}
			return result;
		}
		
		private static TraceFilterExpression parsePrimary(ArrayList<String> tokens)
		{
			TraceFilterExpression result = null;

			if (tokens.isEmpty()) { 
				throw new IllegalArgumentException("Failed to parse filter expression: expected expression");
			}
			
			String token = tokens.get(0);
			if (token.equals("(")) {
				// subexpression
				tokens.remove(0);
				result = parseExpression(tokens);
				if (tokens.isEmpty() || !tokens.get(0).equals(")")) {
					throw new IllegalArgumentException("Failed to parse filter expression: expected closing parenthesis");
				}
				tokens.remove(0);

			} else if (token.equals("!")) {
				// not expression
				tokens.remove(0);
				result = parsePrimary(tokens);
				if (null == result) {
					throw new IllegalArgumentException("Failed to parse filter expression: missing operand to NOT");
				}
				result = new TraceFilterNotExpression(result);
				
			} else if (token.equals(")") || token.equals("&") || token.equals("|") || token.equals(",")) {
				throw new IllegalArgumentException("Failed to parse filter expression: expected expression, found " + token);
				
			} else {
				// anything else
				tokens.remove(0);
				result = new TraceFilter(token);
			}

			return result;
		}
	}
	
	private static class TraceFilterOrExpression extends TraceFilterExpression
	{
		private TraceFilterExpression lhs;
		private TraceFilterExpression rhs;

		public TraceFilterOrExpression(TraceFilterExpression lhs, TraceFilterExpression rhs)
		{
			this.lhs = lhs;
			this.rhs = rhs;
		}
		
		public boolean matches(TracePoint tp)
		{
			return lhs.matches(tp) || rhs.matches(tp);
		}
		
		public String toString()
		{
			return "(" + lhs + "|" + rhs + ")";
		}
	}

	private static class TraceFilterAndExpression extends TraceFilterExpression
	{
		private TraceFilterExpression lhs;
		private TraceFilterExpression rhs;
		
		public TraceFilterAndExpression(TraceFilterExpression lhs, TraceFilterExpression rhs)
		{
			this.lhs = lhs;
			this.rhs = rhs;
		}
		
		public boolean matches(TracePoint tp)
		{
			if (lhs.matches(tp)) {
				return rhs.matches(tp);
			}
			return false;
		}
		
		public String toString()
		{
			return "(" + lhs + "&" + rhs + ")";
		}
	}

	private static class TraceFilterNotExpression extends TraceFilterExpression
	{
		protected TraceFilterExpression expression;
		
		public TraceFilterNotExpression(TraceFilterExpression expression)
		{
			this.expression = expression;
		}
		
		public boolean matches(TracePoint tp)
		{
			return !expression.matches(tp);
		}
		
		public String toString()
		{
			return "!" + expression;
		}
	}

	private static class TraceFilter extends TraceFilterExpression
	{
		private String component = null;
		private String type = null;
		private int idLow = -1;
		private int idHigh = -1;

		/* Format of a tracepoint specification:
		 * 	<component>[{<type>}]
		 * or
		 * 	<component>.<id>
		 * or
		 * 	<component>.<id-range>
		 */
		public TraceFilter(String spec)
		{
			int index = 0;	
			int typeIndex = spec.indexOf('{', index);
			if (-1 == typeIndex) {
				// no type, maybe an id?
				int dotIndex = spec.indexOf('.', index);
				if (-1 == dotIndex) {
					component = spec.substring(index);
				} else {
					component = spec.substring(index, dotIndex);
					int dashIndex = spec.indexOf('-', dotIndex);
					if (-1 == dashIndex) {
						idLow = Integer.parseInt(spec.substring(dotIndex + 1));
						idHigh = idLow;
					} else {
						idLow = Integer.parseInt(spec.substring(dotIndex + 1, dashIndex));
						idHigh = Integer.parseInt(spec.substring(dashIndex + 1));
					}
				}
			} else {
				component = spec.substring(index, typeIndex);
				type = spec.substring(typeIndex + 1, spec.length() - 1);
			}
			
			if ((null != component && ((0 == component.length()) || component.equalsIgnoreCase("all")))) {
				// "all" is the default
				component = null;
			}
		}
		
		public boolean matches(TracePoint tp)
		{
			boolean result = true;
			if (null != component) {
				if (component.equals(tp.getComponent())) {
					if (-1 != idLow) {
						int id = tp.getID();
						if ((id < idLow) || (id > idHigh)) {
							result = false;
						}
					}
				} else {
					result = false;
				}
			}
			if (result && (null != type)) {
				if (type.equalsIgnoreCase(tp.getType().trim())) {
					// all good
				} else {
					// may match a group?
					result = false;
					String[] groups = tp.getGroups();
					if (null != groups) {
						for (int i = 0; i < groups.length; i++) {
							if (type.equalsIgnoreCase(groups[i])) {
								result = true;
								break;
							}
						}
					}
				}
			}

			return result;
		}
		
		public String toString()
		{
			StringBuilder builder = new StringBuilder();
			if (null != component) {
				builder.append(component);
				if (-1 != idLow) {
					builder.append(".");
					builder.append(idLow);
					if (idLow != idHigh) {
						builder.append("-");
						builder.append(idHigh);
					}
				}
			} else {
				builder.append("all");
			}
			if (null != type) {
				builder.append("{");
				builder.append(type);
				builder.append("}");
			}
			return builder.toString();
		}
	}
	
	
	public SnapFormatCommand()
	{
	}	

	/**
	 * Options should be:
	 * 	-f ouputFile
	 * 	-t vmthread
	 * 	-d .dat file path
	 */
	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {
		
		String fileName = null;
		String userDatPath = null;
		String userThreadId = null;
		String messageFilePath = null;
		TraceFilterExpression specFilter = null;
		
		/* Usage:
		 * 	!snapformat [<filename>]
		 * 		- format trace buffers to a specified file or stdout
		 * 	!snapformat [-f <filename>] [-d <datfile_directory>] [-t <j9vmthread id>] 
		 * 		- format trace buffers for all threads or just the specified thread to a file or
		 * 		  stdout using the .dat files in the specified dir
		 * 
		 */
		
		if (args.length == 0 ){
			fileName = null; // Write formatted trace to the console.
		} else if( args.length == 1 ) {
			fileName = args[0];
		} else {
			/* Walk the arguments and look for flag/value pairs. */
			for( int i = 0; i < args.length - 1; ) {
				String flag = args[i++];
				String value = args[i++];
				if( "-f".equals(flag)) {
					fileName = value;
				} else if( "-d".equals(flag)) {
					userDatPath = value;
				} else if( "-t".equals(flag)) {
					userThreadId = value;
					outputHeader = false;
				} else if( "-s".equals(flag)) {
					specFilter = TraceFilterExpression.parseExpression(value);
					outputHeader = false;
				}
			}
		}
	
		/* Look for message files, search order is:
		 * - User specified location to this command.
		 * - as resources located under same class loader as TraceContext
		 * - current working directory
		 * - jre/lib for the current jre 
		 */
		boolean foundDatFiles = false;
		if( userDatPath != null ) {
			int i = 0;
			for( String name : MESSAGEFILENAMES ) {
				File f = new File(userDatPath + File.separator + name);
				if( !f.exists() ) {
					out.printf("Error locating .dat file %s on user path %s\n", name, userDatPath);
					return;
				} else {
					try {
						messageFiles[i++] = new FileInputStream(f);
						foundDatFiles = true;
					} catch (FileNotFoundException e) {
						// We've done f.exists(), this should be fine.
						// foundDatFiles will be false if not.
					}
				}
			}
		} else {
			int i = 0;
			String[] messageFileStrings = new String[MESSAGEFILENAMES.length];
			for( String name : MESSAGEFILENAMES ) {
				InputStream s = TraceContext.class.getResourceAsStream('/' + name);
				if ( null != s ) {
					messageFileStrings[i] = TraceContext.class.getResource('/' + name).toString();
					messageFiles[i++] = s;
					foundDatFiles = true;
				}
			}
			if( foundDatFiles ) {
				out.printf("Formatting trace using format dat files from %s and %s\n", messageFileStrings[0], messageFileStrings[1]);
			} else {
				for( String path: DEFAULTMESSAGEFILEPATHS ) {
					for( String name : MESSAGEFILENAMES ) {
						File f = new File(path + File.separator + name);
						if( f.exists() ) {
							try {
								messageFiles[i++] = new FileInputStream(f);
								foundDatFiles = true;
							} catch (FileNotFoundException e) {
								// We've done f.exists(), this should be fine.
								// foundDatFiles will be false if not.
							}
						}
					}
					if( foundDatFiles ) {
						messageFilePath = path;
						out.printf("Formatting trace using format dat files from %s and %s from %s\n", MESSAGEFILENAMES[0], MESSAGEFILENAMES[1], messageFilePath);
						break;
					}
				}
			}
			if( !foundDatFiles ) {
				out.printf("Unable to find %s and %s in %s or %s\n", MESSAGEFILENAMES[0], MESSAGEFILENAMES[1], DEFAULTMESSAGEFILEPATHS[0], DEFAULTMESSAGEFILEPATHS[1]);
				return;
			}
		}
		
		if (outputHeader) {
			extractTraceData(context, out);
		} else {
			extractTraceData(context, dummyOut);
		}
		
		if( traceContext == null ) {
			out.println("Unable to create trace context, command failed.");
			return;
		}
		
		long threadId = 0;
		if( userThreadId != null ) {
			boolean is64BitPlatform = (context.process.bytesPerPointer() == 8) ? true : false;
			threadId = CommandUtils.parsePointer(userThreadId, is64BitPlatform);
		}
		
		/* Create the stream to write the trace to.
		 * The specified output file or "out" if we
		 * are writing to the console.
		 * (Do this last so we have less cases where we
		 * need to exit for an error and close the printstream.)
		 */
		PrintStream traceOut = out;
		PrintStream filePrintStream = null;
		if( fileName != null ) {
			try {
				filePrintStream = new PrintStream(fileName);
				traceOut = filePrintStream;
			} catch (FileNotFoundException e) {
				out.printf("Unable to write formatted trace to file %s\n", fileName);
			}
		}
	
		if (outputHeader) {
			traceOut.println(traceContext.summary());
		}
		
		try {
			Iterator<TracePoint> tpIterator = null;
			if( userThreadId != null ) {
				Iterator<TraceThread> threadsIterator = (Iterator<TraceThread>)traceContext.getThreads();
				boolean foundThread = false;
				while (threadsIterator.hasNext()) {
					TraceThread thread = threadsIterator.next();
					if( thread.getThreadID() == threadId ) {
						foundThread = true;
						tpIterator = (Iterator<TracePoint>)thread.getIterator();
					}
				}
				if( !foundThread ) {
					out.printf("Unable to find thread %s in trace data\n", userThreadId);
				}
			} else {
				tpIterator = (Iterator<TracePoint>)traceContext.getTracepoints();	
			}
			
			if( tpIterator != null ) {
				int tpCount = printTracePoints(traceOut, tpIterator, specFilter);
				out.printf("Completed processing of %d tracepoints with %d warnings and %d errors\n", traceContext.getTotalTracePoints(), traceContext.getWarningCount(), traceContext.getErrorCount() );
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
		
		if( filePrintStream != null ) {
			out.println("Snap trace written to: " + fileName);
			filePrintStream.close();
		}
		
	}

	private int printTracePoints(PrintStream traceOut, Iterator<TracePoint> tpIterator, TraceFilterExpression specFilter) {
		int tpCount = 0;
		TraceThread thread = null;
		while (tpIterator.hasNext()) {
			TracePoint tp = tpIterator.next();
			TracePointImpl tracepoint = null;
			if( tp instanceof TracePointImpl ) {
				tracepoint = (TracePointImpl) tp;
			}
			if ((tracepoint != null) && ((specFilter == null) || specFilter.matches(tracepoint))) {
				TraceThread current = tracepoint.getThread();
				String component = tracepoint.getComponentName();
				int tpID = tracepoint.getID();
				String container = tracepoint.getContainerComponent();
				tpCount++;

				String parameters = "";
				try {
					parameters = tracepoint.getFormattedParameters();
					if (parameters == null || parameters.length() == 0) {
					traceContext.error(traceContext, "null parameter data for trace point "+component+"."+tpID);
					}
				} catch (BufferUnderflowException e) {
					/* This may be thrown, but there's essentially nothing we can do about it at this level so
					 * just report it
					 */
					traceContext.error(traceContext, "Underflow accessing parameter data for trace point "+component+"."+tpID);
				}
				
				StringBuilder formatted = new StringBuilder();
				
				formatted.append(tracepoint.getFormattedTime());
				

				/* append thread id */
				formatted.append(" ").append((current != thread ? "*" : " "));
				formatted.append(String.format("0x%X", current.getThreadID()));
				formatted.append(" ");

				/* append component and padding */
				StringBuilder fullTracepointID = new StringBuilder(component);
				fullTracepointID.append(container != null ? "(" + container + ")" : "").append(".").append(tpID);
				formatted.append(String.format("%-20s",fullTracepointID.toString()));
				formatted.append(" ");

				formatted.append(tracepoint.getType());

				formatted.append(parameters.length() > 0 ? ((parameters.charAt(0) == '*' ? " " : "") + parameters) : "");

				thread = current;
				traceOut.println(formatted.toString());
			}
		}
		return tpCount;
	}
	
	@Override
	protected void writeHeaderBytesToTrace(Context context, byte[] headerBytes, PrintStream out) 
	{

		try {
			traceContext = TraceContext.getContext(headerBytes, headerBytes.length, messageFiles[0] );
			traceContext.addMessageData(messageFiles[1]);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

	}

	@Override
	protected void writeBytesToTrace(Context context, long address,
			int bufferSize, PrintStream out) {
		
		if( traceContext == null ) {
			out.println("Error - trace buffer passed before context created.");
			return;
		}
		
		byte[] data = new byte[bufferSize];
		try {
			context.process.getBytesAt(address, data);
		} catch (CorruptDataException e) {
			// Although we got a CDE some data may have been copied to the buffer.
			// This appears to happen on z/OS when some of the buffer space is in a page of
			// uninitialized memory. (See defect 185780.) In that case the missing data is
			// all 0's anyway.
			out.println("Problem reading " + bufferSize + " bytes from 0x" +  Long.toHexString(address) + ". Trace file may contain partial or damaged data.");
		}
		traceContext.addData(data);

	}

}
