/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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
/*[IF Sidecar19-SE]*/
package com.ibm.jvm.traceformat;
/*[ELSE]
package com.ibm.jvm;
/*[ENDIF]*/

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.StringTokenizer;
import java.util.TimeZone;

import com.ibm.jvm.trace.format.api.MissingDataException;
import com.ibm.jvm.trace.format.api.TraceContext;
import com.ibm.jvm.trace.format.api.TracePoint;
import com.ibm.jvm.trace.format.api.TracePointImpl;
import com.ibm.jvm.trace.format.api.TraceThread;

/**
 * !!! WARNING !!!
 * Adding any new top level classes in this file requires modification to jcl/jcl_build.mk
 * to ensure the new classes are included within traceformat.jar for Java 8.
 */
public class TraceFormat
{
	Object options[][] = {
			{"timezoneoffset", Integer.valueOf(-1), "An offset in hours to add to the time stamps", Boolean.FALSE, Boolean.FALSE},
			
			{"tag", Integer.valueOf(-1), "A string that's prepended to the formatted trace point string to help track and compare trace from multiple JVMs", Boolean.FALSE, Boolean.FALSE},
	};

	private static Map indentMap = new HashMap();
	
	/**
	 * @param args
	 */
	public static void main(String[] args) throws Exception {
		/* Add the option parsers we want to provide */
		ProgramOption.addOption(InputFile.class);
		ProgramOption.addOption(OutputFile.class);
		ProgramOption.addOption(MessageFile.class);
		ProgramOption.addOption(FormatTimestamp.class);
		ProgramOption.addOption(Indent.class);
		ProgramOption.addOption(Summary.class);
		ProgramOption.addOption(Threads.class);
		ProgramOption.addOption(Timezone.class);
		ProgramOption.addOption(Verbose.class);
		ProgramOption.addOption(Debug.class);
		ProgramOption.addOption(Statistics.class);
		
		
		/* The trace context holds the configuration and state for the parsing */
		TraceContext context;

		/* Process the command line options and populate any unspecified options with default values */
		try {
			for (int i = 0; i < args.length; i++) {
				ProgramOption.addArgument(args[i]);
			}
	
			ProgramOption.applyDefaults();
		} catch (IllegalArgumentException e) {
			System.err.println(e.getMessage());
			return;
		}

		/*
		 * At this point we have valid data for all command line options so get on with the parsing
		 */

		/* Get the input files to process */
		List inputFiles = (List)ProgramOption.getValue("input_file");
		List messageFiles = (List)ProgramOption.getValue("datfile");
		List threads = (List)ProgramOption.getValue("threads");
		Integer timezone = (Integer)ProgramOption.getValue("timezone");
		Boolean formatTime = (Boolean)ProgramOption.getValue("format_time");
		Boolean indenting = (Boolean)ProgramOption.getValue("indent");
		Boolean summary = (Boolean)ProgramOption.getValue("summary");
		Boolean verbose = (Boolean)ProgramOption.getValue("verbose");
		Integer debugLevel = (Integer)ProgramOption.getValue("debug");
		Boolean statistics = (Boolean)ProgramOption.getValue("statistics");

		/* Parse the header on the first file */
		int blockSize = 4000;
		PrintStream error = null;
		PrintStream warning = null;
		PrintStream debug = null;
		
		if (debugLevel.intValue() > 0) {
			debug = System.err;
		}
		
		try {
			while (true) {
				try {
					ByteBuffer data;
					data = ((RandomAccessFile)inputFiles.get(0)).getChannel().map(FileChannel.MapMode.READ_ONLY, 0, blockSize);
					context = TraceContext.getContext(data, (File)messageFiles.get(0), System.out, warning, error, debug);
					break;
				} catch (BufferUnderflowException e) {
					blockSize*= 2;
				}
			}
		} catch (IOException e) {
			/* could be many things, one is a file shorter than blockSize so try a different method */
			try {
				long length = ((RandomAccessFile)inputFiles.get(0)).length();
				if (length > 0) {
					byte header[] = new byte[(int)length];
					((RandomAccessFile)inputFiles.get(0)).seek(0);
					int i = ((RandomAccessFile)inputFiles.get(0)).read(header);
					if (i == length && length < blockSize) {
							context = TraceContext.getContext(header, header.length, (File)messageFiles.get(0), System.out, warning, error, debug);
					} else {
						throw new Exception("received premature end of file: " + e.getMessage());
					}
				} else {
					throw new Exception("empty trace file");
				}
			} catch (Exception f) {
				/* this wasn't due to filesize < blocksize so print the exception message and exit */
				System.err.println("Unable to read trace header from file: "+f.getMessage());
				System.err.println("Please check that the input file is a binary trace file");
				return;
			}
		} catch (IllegalArgumentException e) {
			System.err.println("Problem reading the trace file header: "+e.getMessage());
			System.err.println("Please check that that the input file is a binary trace file");
			return;
		}

		if (verbose.booleanValue() || debugLevel.intValue() > 0) {
			/* we don't set these in the constructor otherwise we see error messages during the retry logic if the block
			 * size estimate is too small */
			context.setErrorStream(System.err);
			context.setWarningStream(System.err);
		}

		context.setDebugLevel(debugLevel.intValue());

		/* set up the thread filters */
		Iterator itr = threads.iterator();
		while (itr.hasNext()) {
			Long id = (Long)itr.next();
			context.addThreadToFilter(id);
		}
		
		context.setTimeZoneOffset(timezone.intValue());

		/* add any remaining dat files */
		for (int i = 1; i < messageFiles.size(); i++) {
			File file = (File)messageFiles.get(i);
			try {
				context.addMessageData(file);
			} catch (IOException e) {
				// Problem reading one of the trace format .dat files, issue message and exit
				System.err.println("Unable to process trace format data file: " + file.getAbsolutePath() + " (" + e.getMessage() + ")");
				return;
			}
		}

		/* Set the output file */
		PrintWriter output = (PrintWriter)ProgramOption.getValue("output_file");

		/* read in the blocks from the various files and sort them */
		long recordsInData = 0;
		long lostCountByException = 0;
		long start = System.currentTimeMillis();
		long end = 0;
		long startBlock = start;
		long recordsProcessed = 0;
		long totalBytes = 0;
		
		/* loop over the generational files and add the blocks to the context */
		for (int i = 0; i < inputFiles.size(); i++) {
			long offset = context.getHeaderSize();
			long recordSize = context.getRecordSize();
			RandomAccessFile traceFile = (RandomAccessFile)inputFiles.get(i);

			long length = traceFile.length();
			if ((length - context.getHeaderSize()) % recordSize != 0) {
				context.warning(context, "The body of the trace file is not a multiple of the record size, file either truncated or corrupt");
			}

			while (offset < length) {
				try {
					TraceThread thread = context.addData(traceFile, offset);
					indentMap.put(thread, "");
				} catch (IllegalArgumentException e) {
					context.error(context, "Bad block of trace data in input file at offset "+offset+": "+e.getMessage());
				}

				offset += recordSize;
				totalBytes+= recordSize;
				recordsInData++;
			}
		}
		
		/* output the summary information */
		output.println(context.summary());
		
		
		if (summary.booleanValue() && !statistics.booleanValue()) {
			/* we've requested only the summary so exit here */
			output.close();
			return;
		}

		if (!summary.booleanValue()) {
			/* output the section header line and the column headings for the trace point data section */
			output.println("                Trace Formatted Data " + System.getProperty("line.separator"));
			
			String columnHeader;
			if (timezone.intValue() == 0) {
				columnHeader = "Time (UTC)          ";
			} else {
				/* user specified a timezone offset, show that in the timestamp column header */
				if (timezone.intValue() > 0) {
					columnHeader = "Time (UTC +";
				} else {
					columnHeader = "Time (UTC -";
				}
				columnHeader += Math.abs(timezone.intValue()/60) + ":" + Math.abs(timezone.intValue()%60) + ")    ";
			}
			if (context.getPointerSize() == 4) {
				columnHeader += "Thread ID ";
			} else {
				columnHeader += "Thread ID         ";
			}
			columnHeader += " Tracepoint ID       Type        Tracepoint Data";
			output.println(columnHeader);
		}
		
		/* start reading tracepoints */
		itr = context.getTracepoints();

		String totalMbytes = (float)totalBytes/(float)(1024*1024) + "Mb";
		context.message(context, "Processing " + totalMbytes + " of binary trace data");
		
		TraceThread thread = null;
		String indent = "";
		while (itr.hasNext()) {
			TracePointImpl tracepoint;

			try {
				tracepoint = (TracePointImpl) itr.next();
			} catch (MissingDataException e) {
				lostCountByException += e.getMissingBytes() / context.getRecordSize();
				continue;
			}

			/* If we've only been asked for the summary we don't format the trace */
			if (!summary.booleanValue()) {
				TraceThread current = tracepoint.getThread();
				String component = tracepoint.getComponentName();
				int tpID = tracepoint.getID();
				String container = tracepoint.getContainerComponent();
				String parameters = "";
				try {
					parameters = tracepoint.getFormattedParameters();
					if (parameters == null || parameters.length() == 0) {
					context.error(context, "null parameter data for trace point "+component+"."+tpID);
					}
				} catch (BufferUnderflowException e) {
					/* This may be thrown, but there's essentially nothing we can do about it at this level so
					 * just report it
					 */
					context.error(context, "Underflow accessing parameter data for trace point "+component+"."+tpID);
				}
	
				StringBuilder formatted = new StringBuilder();
				if (formatTime.booleanValue()) {
					formatted.append(tracepoint.getFormattedTime());
				} else {
					formatted.append(tracepoint.getRawTime());
				}
				
				/* append thread id */
				formatted.append(" ").append((current != thread ? "*" : " "));
				formatted.append(context.formatPointer(current.getThreadID()));
				formatted.append(" ");
	
				/* append component and padding - add container if this is a sub component.
				 * e.g j9codertvm(j9jit).91 vs j9jit.18 */			
				String fullTracepointID = String.format((container != null ? "%s(%s).%d" : "%1$s.%3$d"), component, container, tpID);
				
				/* Left justify but include a space in the formatting as a column separator in case of very long component id's. */
				formatted.append(String.format("%-19s ", fullTracepointID));
				
				formatted.append(tracepoint.getType());
				
				if (indenting.booleanValue()) {
					indent = indentMap.get(current).toString();
	
					/* we remove the indent before appending for exit */
					if (tracepoint.getTypeAsInt() == TracePoint.EXIT_TYPE || tracepoint.getTypeAsInt() == TracePoint.EXIT_EXCPT_TYPE) {
						try {
							indent = indent.substring(2);
							indentMap.put(current, indent);
						} catch (IndexOutOfBoundsException e) {
							indent = "";
							indentMap.put(current, "");
						}
					}
	
					formatted.append(indent);
				}
	
				formatted.append(parameters.length() > 0 ? ((parameters.charAt(0) == '*' ? " " : "") + parameters) : "");
	
				if (indenting.booleanValue()) {
					/* juggle the indent for the thread */
					if (tracepoint.getTypeAsInt() == TracePoint.ENTRY_TYPE || tracepoint.getTypeAsInt() == TracePoint.ENTRY_EXCPT_TYPE) {
						indent = indent+"  ";
						indentMap.put(current, indent);
					}
				}

				if (debugLevel > 0) {
					formatted.append(" ["+((TracePointImpl)tracepoint).getDebugInfo()+"]");
				}

				thread = current;
				output.println(formatted.toString());
			}
			
			/* print percentage */
			if (context.getTotalRecords() != recordsProcessed) {
				recordsProcessed = context.getTotalRecords();
				float processedMbytes = ((float) (context.getTotalRecords() * context.getRecordSize())) / (1024 * 1024);
				if (processedMbytes % 10 == 0) {
					int percent = (int) ((processedMbytes * 100) / (totalBytes/(1024*1024)));
					if (verbose.booleanValue()) {
						end = System.currentTimeMillis();
						float MbpsBlock = (float) 10.0 / (float) ((end - startBlock) / 1000.0);
						float Mbps = (float) (processedMbytes) / (float) (((float) (end - start) / 1000.0));
						startBlock = System.currentTimeMillis();
						context.message(context, "Processed " + processedMbytes + "Mb ("+percent+"%), burst speed: "+MbpsBlock+"Mb/s, average: "+Mbps+"Mb/s");
					} else {
						context.message(context, "Processed " + processedMbytes + "Mb ("+percent+"%)");
					}
				}
			}
		}

		if (lostCountByException > 0) {
			context.warning(context, lostCountByException + " records were discarded during trace generation");
		}

		output.close();

		context.message(context, "Completed processing of " + context.getTotalTracePoints() + " tracepoints with " + context.getWarningCount() + " warnings and " + context.getErrorCount() + " errors");
		if (verbose.booleanValue()) {
			end = System.currentTimeMillis();
			float Mbps = (float) (recordsInData * context.getRecordSize()) / (float) (((float) (end - start) / 1000) * (1024 * 1024));
			context.message(context, "Total processing time " + (end - start) + "ms (" + Mbps + "Mb/s)");
		}
		
		if (statistics) {
			context.message(context, context.statistics());
		}
	}

}

class Debug extends ProgramOption {
	int debug;
	
	String getDescription() {
		return "If specified this option will cause debug data to be written to stderr. The default debug level is 1, increasing this will increase the amount of data supplied.";
	}

	String getName() {
		return "debug";
	}
	
	String getUsage() {
		return "-debug=level";
	}

	Object getValue() {
		return Integer.valueOf(debug);
	}

	void setValue(String value) throws IllegalArgumentException {
		try {
			debug = Integer.parseInt(value);
		} catch (NumberFormatException e) {
			debug = -1;
		}
		
		if (debug < 0) {
			throw new IllegalArgumentException("The value \""+value+"\" specified for verbose is not valid, must be a positive integer");
		}
	}

	void setAutomatic() {
		debug = 1;
	}

	void setDefault() {
		debug = 0;
	}
	
}

class Verbose extends ProgramOption {
	boolean verbose;
	
	String getDescription() {
		return "If specified this option will cause warning and error messages to be printed to stderr. By default a count of each is displayed";
	}

	String getName() {
		return "verbose";
	}
	
	String getUsage() {
		return "-verbose";
	}

	Object getValue() {
		return Boolean.valueOf(verbose);
	}

	void setValue(String value) throws IllegalArgumentException {
		if (value.equalsIgnoreCase("true") || value.equalsIgnoreCase("yes")) {
			verbose = true;
		} else if (value.equalsIgnoreCase("false") || value.equalsIgnoreCase("no")) {
			verbose = false;
		} else {
			throw new IllegalArgumentException("The value \""+value+"\" specified for verbose is not valid, must be true or false");
		}
	}

	void setAutomatic() {
		verbose = true;
	}

	void setDefault() {
		verbose = false;
	}
	
}


class Statistics extends ProgramOption {
	boolean statistics;
	
	String getDescription() {
		return "If specified this will cause some statistics to be reported:" +System.getProperty("line.separator")+
				"		bytes of data by component" +System.getProperty("line.separator")+
				"		total bytes" +System.getProperty("line.separator")+
				"		hit counts by trace point" +System.getProperty("line.separator")+
				"		bytes of data by trace point";
	}

	String getName() {
		return "statistics";
	}
	
	String getUsage() {
		return "-statistics";
	}

	Object getValue() {
		return Boolean.valueOf(statistics);
	}

	void setValue(String value) throws IllegalArgumentException {
		if (value.equalsIgnoreCase("true") || value.equalsIgnoreCase("yes")) {
			statistics = true;
		} else if (value.equalsIgnoreCase("false") || value.equalsIgnoreCase("no")) {
			statistics = false;
		} else {
			throw new IllegalArgumentException("The value \""+value+"\" specified for statistics is not valid, must be true or false");
		}
	}

	void setAutomatic() {
		statistics = true;
	}

	void setDefault() {
		statistics = false;
	}
	
}

class MessageFile extends ProgramOption {
	List messageFiles = new LinkedList();

	String getDescription() {
		String eol = System.getProperty("line.separator", "\n");
		return "A comma separated list of files containing the trace format strings. By default the following files are used:" + eol
			 + "  $JAVA_HOME/lib/J9TraceFormat.dat" + eol
			 + "  $JAVA_HOME/lib/OMRTraceFormat.dat" + eol
			 + "  $JAVA_HOME/lib/TraceFormat.dat";
	}

	String getName() {
		return "datfile";
	}
	
	String getUsage() {
		return "-datfile=file";
	}

	Object getValue() {
		return messageFiles;
	}
	
	void setValue(String value) throws IllegalArgumentException {
		/* This could be just the one file or could be a comma separated list */
		StringTokenizer st = new StringTokenizer(value, ",");
		String token = "";
		try {
			while (st.hasMoreTokens()) {
				token = st.nextToken();
				/* construct files from the tokens. These files must exist */
				File datFile = new File(token);
				if (!datFile.exists()) {
					throw new IllegalArgumentException("dat file \"" + token + "\" not found");
				}
				messageFiles.add(datFile);
			}
		} catch (SecurityException e) {
			throw new IllegalArgumentException("The application does not have permission to access the specified dat file, \""+token+"\"");
		}
	}
	
	void setDefault() {
		String dir = System.getProperty("java.home");
		dir = dir.concat(File.separator).concat("lib").concat(File.separator);
		
		setValue(dir + "J9TraceFormat.dat");
		setValue(dir + "OMRTraceFormat.dat");
		try {
			setValue(dir + "TraceFormat.dat");
		} catch (IllegalArgumentException e) {
			System.out.println("Warning: " + e.getMessage());
		}
	}
}

class Threads extends ProgramOption {
	List threads = new LinkedList();

	String getDescription() {
		return "A comma separated list of thread ids to include in the formatted trace output.";
	}

	String getName() {
		return "threads";
	}
	
	String getUsage() {
		return "-threads=id[,id]";
	}

	Object getValue() {
		return threads;
	}
	
	void setValue(String value) throws IllegalArgumentException {
		/* This could be just the one file or could be a comma separated list */
		StringTokenizer st = new StringTokenizer(value, ",");
		String token = "";
		try {
			while (st.hasMoreTokens()) {
				token = st.nextToken();
				Long id;
				
				/* construct Long's from the id's */
				if (token.startsWith("0x")) {
					id = Long.valueOf(token.substring(2), 16);
				} else {
					id = Long.valueOf(token);
				}
				threads.add(id);
			}
		} catch (NumberFormatException e) {
			throw new IllegalArgumentException("The specified thread id, \""+token+"\" is not valid. Id's must be a number");
		}
	}
	
	void setDefault() {
	}
}

class Timezone extends ProgramOption {
	Integer timezone;
	
	String getDescription() {
		return "Specifies the offset from UTC/GMT to apply when formatting timestamps, as +|-HH:MM";
	}

	String getName() {
		return "timezone";
	}

	String getUsage() {
		return "-timezone=+|-HH:MM";
	}

	Object getValue() {
		return timezone;
	}

	void setValue(String value) throws IllegalArgumentException {
		// Validate and convert input time zone string from +|-HH:MM format to +/- integer minutes
		try {
			if (value.length() != 6) { // initial validation, string length must be 6
				throw new NumberFormatException();
			}
 			int hours = Integer.parseInt(value.substring(1,3));
			int minutes = Integer.parseInt(value.substring(4,6));
 			if ((hours > 12) || (minutes > 60))  {
 				throw new NumberFormatException();
 			}
			timezone = (hours * 60) + minutes;
			if (value.substring(0,1).equals("-")) {
				timezone = -timezone;	
			}
		} catch (NumberFormatException e) {
			throw new IllegalArgumentException("The specified timezone offset \""+value+"\" is not valid. Format is +|-HH:MM");
		}
	}

	void setAutomatic() {
		/* if specified with no value, default to the current timezone. Convert from millis to minutes */
		timezone = Integer.valueOf((TimeZone.getDefault().getRawOffset() + TimeZone.getDefault().getDSTSavings())/(1000 * 60));
	}
	
	void setDefault() {
		/* default value is false */
		timezone = Integer.valueOf(0);
	}
}

class Indent extends ProgramOption {
	boolean indent;
	
	String getDescription() {
		return "Specifies whether to indent the trace point text to illustrate the call flow for the thread.";
	}

	String getName() {
		return "indent";
	}

	String getUsage() {
		return "-indent";
	}

	Object getValue() {
		return Boolean.valueOf(indent);
	}

	void setValue(String value) throws IllegalArgumentException {
		if (value.equalsIgnoreCase("true") || value.equalsIgnoreCase("yes")) {
			indent = true;
		} else if (value.equalsIgnoreCase("false") || value.equalsIgnoreCase("no")) {
			indent = false;
		} else {
			throw new IllegalArgumentException("The value \""+value+"\" specified for indent is not valid, must be true or false");
		}
	}

	void setAutomatic() {
		/* if specified with no value, default to true */
		indent = true;
	}
	
	void setDefault() {
		/* default value is false */
		indent = false;
	}
}

class Summary extends ProgramOption {
	boolean summary;
	
	String getDescription() {
		return "Instructs the formatter to produce only summary information about the data";
	}

	String getName() {
		return "summary";
	}

	String getUsage() {
		return "-summary";
	}

	Object getValue() {
		return Boolean.valueOf(summary);
	}

	void setValue(String value) throws IllegalArgumentException {
		if (value.equalsIgnoreCase("true") || value.equalsIgnoreCase("yes")) {
			summary = true;
		} else if (value.equalsIgnoreCase("false") || value.equalsIgnoreCase("no")) {
			summary = false;
		} else {
			throw new IllegalArgumentException("The value \""+value+"\" specified for summary is not valid, must be true or false");
		}
	}

	void setAutomatic() {
		/* if specified with no value, default to true */
		summary = true;
	}
	
	void setDefault() {
		/* default value is false */
		summary = false;
	}
}


class InputFile extends ProgramOption {
	List inputFiles = new LinkedList();

	String getDescription() {
		return "The binary trace file generated by IBM JVMs. For multi-generational trace files specify the file pattern instead. This option is required";
	}

	String getName() {
		return "input_file";
	}

	Object getValue() {
		return inputFiles;
	}
	
	boolean required() {
		return true;
	}
	
	void setDefault() {
		/* pulls the first Anonymous argument out of the list and uses that */
		if (ProgramOption.AnonymousArgs.isEmpty()) {
			throw new IllegalArgumentException("An input file must be specified");
		}

		setValue((String)ProgramOption.AnonymousArgs.get(0));
	}

	void setValue(String value) throws IllegalArgumentException {
		try {
			inputFiles.add(new RandomAccessFile(value, "r"));
		} catch (FileNotFoundException e) {
			int i = 0;
			String generation = value;

			/* if this was a generational file pattern then replace the token and try again */
			if (value.indexOf('#') != -1) {
				
				/* TODO: we've had to remove record level sorting due to the problems with NTP updates.
				 * Until we've got sorting for the generational files this needs to be disabled
				 */
				throw new IllegalArgumentException("Support for generational files has been removed until file level sorting is implemented");

//				try {
//					while (true) {
//						generation = value.replace('#', Character.forDigit(i, 10));
//						setValue(generation);
//						i++;
//					}
//				} catch (IllegalArgumentException r) {
//					if (i == 0) {
//						/* we failed to find any generational files */
//						throw new IllegalArgumentException("A generational input file pattern was specified (contained '#'), but the file \""+generation+"\" was not be found");
//					}
//				}
			} else {
				throw new IllegalArgumentException("The file \""+value+"\" specified as the input file could not be found");
			}
		}
	}
}

class FormatTimestamp extends ProgramOption {
	boolean formatTimestamp;
	
	String getDescription() {
		return "Specifies whether to format the tracepoint time stamps. Default is yes.";
	}

	String getName() {
		return "format_time";
	}

	String getUsage() {
		return "-format_time=yes|no";
	}

	Object getValue() {
		return Boolean.valueOf(formatTimestamp);
	}

	void setValue(String value) throws IllegalArgumentException {
		if (value.equalsIgnoreCase("true") || value.equalsIgnoreCase("yes")) {
			formatTimestamp = true;
		} else if (value.equalsIgnoreCase("false") || value.equalsIgnoreCase("no")) {
			formatTimestamp = false;
		} else {
			throw new IllegalArgumentException("The value \""+value+"\" specified for format timestamp is not valid, must be true or false");
		}
	}

	void setAutomatic() {
		/* if specified with no value, default to true */
		formatTimestamp = true;
	}
	
	void setDefault() {
		/* by default we format timestamps */
		formatTimestamp = true;
	}
}

class OutputFile extends ProgramOption {
	PrintWriter outputStream;
	
	String getDescription() {
		return "The output file containing the formatted trace data. The default is to append \".fmt\" to the input file name";
	}

	String getName() {
		return "output_file";
	}

	Object getValue() {
		return outputStream;
	}

	void setDefault() {
		/* locate the input file in the program options and append .trc to the name */
		if (ProgramOption.AnonymousArgs.isEmpty()) {
			throw new IllegalArgumentException("An input file must be specified before the output file when using anonymous arguments to specify input/output files");
		}

		if (ProgramOption.AnonymousArgs.size() == 1) {
			setValue(ProgramOption.AnonymousArgs.get(0).toString()+".fmt");
		} else if (ProgramOption.AnonymousArgs.size() == 2){
			setValue(ProgramOption.AnonymousArgs.get(1).toString());
		} else {
			StringBuffer sb = new StringBuffer("Usage error: too many positional arguments:"+System.getProperty("line.separator"));
			for (int i = 2; i < ProgramOption.AnonymousArgs.size(); i++) {
				sb.append(ProgramOption.AnonymousArgs.get(i)).append(System.getProperty("line.separator"));
			}
			sb.append(System.getProperty("line.separator")+help());
			throw new IllegalArgumentException(sb.toString());
		}
		
	}

	void setValue(String value) throws IllegalArgumentException {
		try {
			// Get the default encoding for input/output files, default to
			// UTF-8 if it isn't set.
			final String encoding = System.getProperty("file.encoding", "UTF-8");
			try {
				outputStream = new PrintWriter(new File(value), encoding);
			} catch (UnsupportedEncodingException e) {
				// Default to default encoding, although if UTF-8 or file.encoding
				// isn't supported the JVM is unlikely to make it this far.
				System.err.println("Unable to set encoding of output file to UTF-8. Some formatted data may not be readable.");
				outputStream = new PrintWriter(new File(value));
			}
		} catch (FileNotFoundException e) {
			throw new IllegalArgumentException("Unable to create file \""+value+"\" for writing");
		}
		System.out.println("Writing formatted trace output to file " + value);
	}
}

/**
 * A base class describing the common parts of command line options. Options are added to Configuration once processed.
 * Permissible options that are not specified on the command line are added with defaults once the entire command line is parsed.
 * @author hickeng
 *
 */
abstract class ProgramOption {
	/* The configuration as specified by the user, merged with defaults */
	final static HashMap Configuration = new HashMap();
	final static List AnonymousArgs = new LinkedList();
	
	/* The set of available options */
	final static HashMap options = new HashMap();
	static HashMap defaultOptions = null;
	
	abstract String getDescription();
	abstract String getName();
	abstract Object getValue();

	/* static accessor for the options recorded in the configuration map */
	static Object getValue(String optionName) {
		return ((ProgramOption)Configuration.get(optionName)).getValue();
	}

	/* default implementation for anonymous arguments */
	String getUsage() {
		return getName();
	}
	
	/* This method takes the string value specified for the option on the command line and
	 * parses it into whatever internal representation is required.
	 */
	abstract void setValue(String value) throws IllegalArgumentException;

	/* If an option is explicitly specified without a value this method is invoked. Subclasses should
	 * override this method if an option has an automatic value, e.g. -timezone applies the
	 * users current timezone. 
	 */
	void setAutomatic() {
		throw new IllegalArgumentException("Value must be specified for "+getName());
	}
	

	/* If an option is allowed but isn't specified on the command line the it is still added to the
	 * options set, but with it's default value. If a subclass describes an optional argument then this
	 * method should be overridden with a default value.
	 */
	void setDefault() {
		throw new IllegalArgumentException("No default value available for "+getName());
	}

	/* This method  is used only to generate the help and simply controls whether or not the option name
	 * has []'s around it. If this method returns false then setDefault should be overridden.
	 * This returns false by default as it's the most common use.
	 */
	boolean required() {
		return false;
	}

	/* This function adds the class for a ProgramOption as a permitted option */
	static void addOption(Class option) {
		try {
			ProgramOption tmp = (ProgramOption)option.newInstance();
			options.put(tmp.getName(), option);
		} catch (Exception e) {}
	}

	/* This function takes an argument from the command line, checks there's a class to process it
	 * and stores it's value 
	 */
	static void addArgument(String arg) throws IllegalArgumentException {
		String key = arg;
		String value = null;
		
		/* strip the leading '-' if present */
		if (arg.charAt(0) == '-') {
			key = arg.substring(1);
		} else {
			/* this argument isn't a key value pair so just stash it and return */
			AnonymousArgs.add(arg);
			return; 
		}


		/* fast path to check if help is specified */
		if (key.equals("help")) {
			throw new IllegalArgumentException(help());
		}

		/* construct the array of default options before we process anything */
		if (defaultOptions == null) {
			defaultOptions = (HashMap)options.clone();
		}

		/* split into key/value pair */
		if (key.indexOf('=') != -1) {
			key = key.substring(0, key.indexOf('='));
			value = arg.substring(arg.indexOf('=') + 1);
		}
		
		if (options.containsKey(key)) {
			/* Find the option */
			Class type = (Class)options.get(key);
			try {
				ProgramOption option = (ProgramOption)type.newInstance();
				if (value != null) {
					option.setValue(value);
				} else {
					option.setAutomatic();
				}

				Configuration.put(option.getName(), option);
			} catch (IllegalArgumentException e) {
				throw e;
			} catch (Exception e) {
				System.err.println("Error while constructing parser for command line option \""+key+"\"");
			}

			/* we've successfully recorded a user specified option so remove it from the default set */
			defaultOptions.remove(key);

			return;
		}

		throw new IllegalArgumentException("Usage error: unknown argument, \""+arg+"\""+System.getProperty("line.separator")+System.getProperty("line.separator")+help());
	}


	/* For all options that weren't specified in any way on the command line, add their default values. If
	 * a required option was omitted this will throw an exception.
	 */
	static void applyDefaults() {
		if (defaultOptions == null) {
			defaultOptions = (HashMap)options.clone();
		}

		Iterator itr = defaultOptions.values().iterator();
		while (itr.hasNext()) {
			Class type = (Class)itr.next();
			try {
				ProgramOption option = (ProgramOption)type.newInstance();
				option.setDefault();
				Configuration.put(option.getName(), option);
			} catch (IllegalArgumentException e) {
				throw e;
			} catch (Exception e) {
				System.err.println("Error while constructing default values");
			}
		}
	}

	public static String help() {
		StringBuffer help = new StringBuffer("Usage: \n");
/*[IF Sidecar18-SE-OpenJ9]
		StringBuffer synopsis = new StringBuffer("traceformat");
/*[ELSE]*/
		StringBuffer synopsis = new StringBuffer("com.ibm.jvm.TraceFormat");
/*[ENDIF] Sidecar18-SE-OpenJ9 */
		StringBuffer sb = new StringBuffer("Option details:"+System.getProperty("line.separator")+System.getProperty("line.separator"));

		Iterator itr = options.values().iterator();
		while (itr.hasNext()) {
			Class type = (Class)itr.next();
			try {
				ProgramOption option = (ProgramOption)type.newInstance();
				String name = option.getName();
				String usage = option.getUsage();

				/* construct the short description */
				if (option.required()) {
					synopsis.append(" "+usage);
				} else {
					synopsis.append(" ["+usage+"]");
				}
				
				/* construct the option description */
				sb.append(name+ ": "+option.getDescription()+System.getProperty("line.separator")+System.getProperty("line.separator"));
			} catch (Exception e) {
				System.err.println("Error while constructing default values");
			}
		}
		
		return help.append(synopsis).append(System.getProperty("line.separator")+System.getProperty("line.separator")).append(sb).toString();
	}
}
