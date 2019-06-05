/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.Writer;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

@SuppressWarnings("nls")
class Test {
	private final String _id;
	private String _command;
	private final String _timeoutString;
	final boolean _debugCmdOnTimeout;
	private int _outputLimit;

	final List<TestCondition> _testConditions;
	boolean[] _matched;

	private final StringBuilder _standardOutput;
	private final StringBuilder _errorOutput;

	boolean _timedOut;

	private String _commandExecutable;
	private List<String> _commandArgs;
	private List<String> _commandInputLines;
	private static final long _JAVACORE_TIMEOUT_MILLIS = 5 * 60 * 1000; // 5 minute timeout

	/**
	 * The number of core files to capture may be specified via
	 *   -Dcmdline.corecount=N
	 * The default is 2.
	 */
	static final String CORE_COUNT_PROPERTY = "cmdline.corecount";
	static final int CORE_COUNT = Integer.getInteger(CORE_COUNT_PROPERTY, 2).intValue();
	
	/**
	 * The time in milliseconds between capturing core files (if CORE_COUNT > 1)
	 * may be specified via
	 *   -Dcmdline.coreintervalms=N
	 * The default is one minute.
	 */
	static final long CORE_SPACING_MILLIS = Long.getLong("cmdline.coreintervalms", 60 * 1000).longValue();

	/**
	 * Create a new test case with the given id.
	 * @param id
	 */
	Test(String id, String timeout, boolean debugCmdOnTimeout) {
		super();
		_id = id;
		_timeoutString = timeout;
		_debugCmdOnTimeout = debugCmdOnTimeout;
		_outputLimit = -1;
		_testConditions = new ArrayList<>();
		_matched = null;
		_standardOutput = new StringBuilder();
		_errorOutput = new StringBuilder();
		_timedOut = false;
	}

	void setOutputLimit(int outputLimit) {
		_outputLimit = outputLimit;
	}

	/**
	 * Set the command particular to this test case
	 */
	void setCommand(String command) {
		_command = command;
	}

	void setSplitCommand(String command, List<String> args, List<String> inputLines) {
		_commandExecutable = command;
		_commandArgs = args;
		_commandInputLines = inputLines;
	}

	/**
	 * Get the name of this test case
	 */
	public String getId() {
		return TestSuite.evaluateVariables(_id);
	}

	/**
	 * Adds another test condition to this test case
	 */
	void addTestCondition(TestCondition tc) {
		_testConditions.add(tc);
	}

	/**
	 * Does the required variable substitutions and runs the test case.
	 *
	 * @param executable - The executable command string
	 * @param defaultTimeout - The default timeout for this test suite. This will be overridden by
	 * the instance variable _timeout if it is something >= 0.
	 * @param variables - The variables to do substitutions of
	 * @return true if the testcase passed, false if it failed
	 */
	public boolean run(long defaultTimeout) {
		// clear any previously captured output
		destroy();

		_matched = new boolean[_testConditions.size()];
		long timer;
		Process proc = null;
		_timedOut = false;
		try {
			/**
			 * Set up a built-in variable "Q" to represent a double-quotes(").
			 * The command comes with $Q$ initially set up by tester to quote the classpath with white spaces.
			 * $Q$ in the command string will be replaced with a double-quotes(") by the parser of
			 * test framework before passing it over to Tokenizer for further processing.
			 * @see Tokenizer
			 */
			TestSuite.putVariable("Q", "\"");
			String exeToDebug = null;
			String fullCommand = null;
			File userDir = new File(System.getProperty("user.dir"));
			Stopwatch testTimer = new Stopwatch().start();
			if (null != _command) {
				fullCommand = TestSuite.evaluateVariables(_command);
				System.out.println("Running command: " + fullCommand);

				/**
				 * According to the test framework, a command string is passed over to exec(String command,...) of Runtime for test execution.
				 * However, the method is unable to recognize a command string if white spaces occur in the classpath of the command string.
				 * To solve the issue, exec(String command,...) is replaced with exec(String[] cmdarray,...), in which case it requires that
				 * a command string be replaced with a command array with all arguments split up in the array.
				 * Meanwhile, a path of .jar file with white spaces should be treated as a single argument in the command array.
				 * Thus, a new class called Tokenizer is created to address the issue of classpath when splitting up a command string.
				 * NOTE: The reason why StreamTokenizer was discarded is that it wrongly interpreted escape characters (e.g. \b, \n, \t)
				 * in the classpath into a single character rather than two characters.
				 * @see Tokenizer
				 */
				String[] cmdArray = Tokenizer.tokenize(fullCommand);
				exeToDebug = cmdArray[0];
				proc = Runtime.getRuntime().exec(cmdArray, TestSuite.getEnvironmentVariableList(), userDir);
			} else {
				// Use a buffer to build the command line from the _commandExecutable and the _commandArgs
				StringBuilder buffer = new StringBuilder(TestSuite.evaluateVariables(_commandExecutable));
				for (String arg : _commandArgs) {
					buffer.append(' ');
					buffer.append(TestSuite.evaluateVariables(arg));
				}
				// Get the fullCommand string from the buffer
				fullCommand = buffer.toString();
				System.out.println("Running command: " + fullCommand);
				String[] cmdArray = Tokenizer.tokenize(fullCommand);
				exeToDebug = cmdArray[0];
				// now start the program
				proc = Runtime.getRuntime().exec(cmdArray, null, userDir);
			}
			testTimer.stop();
			timer = testTimer.getTimeSpent();
			System.out.println("Time spent starting: " + timer + " milliseconds");
			StreamMatcher stdout = new StreamMatcher(proc.getInputStream(), _standardOutput, " [OUT] ");
			StreamMatcher stderr = new StreamMatcher(proc.getErrorStream(), _errorOutput, " [ERR] ");
			stdout.start();
			stderr.start();

			// Feed our commands to stdin of the process. We do this *after* starting
			// the threads that consume stdout/stderr to avoid deadlock.
			try (PrintStream stdin = new PrintStream(proc.getOutputStream())) {
				if (_commandInputLines != null && _command == null) {
					// fix up the command inputs and feed them in (with newlines between)
					for (String arg : _commandInputLines) {
						String inputLine = TestSuite.evaluateVariables(arg);
						stdin.println(inputLine);
					}
				}
			} // closes input so the underlying program will get EOF

			long _timeout = defaultTimeout;
			try {
				_timeout = 1000 * Integer.parseInt(TestSuite.evaluateVariables(_timeoutString).trim());
			} catch (Exception e) {
				/* Expected exception, failing to parse _timeout makes timeout have the value of defaultTimeout */
			}

			new ProcessKiller(proc, _timeout, exeToDebug).start();

			stdout.join(_timeout * 1000);
			stderr.join(_timeout * 1000);

			if (stdout.isAlive()) {
				TestSuite.printErrorMessage("stdout timed out");
			}
			if (stderr.isAlive()) {
				TestSuite.printErrorMessage("stderr timed out");
			}

			proc.waitFor();

			testTimer.stop();
			timer = testTimer.getTimeSpent();
			System.out.println("Time spent executing: " + timer + " milliseconds");
		} catch (Exception e) {
			if (proc != null) {
				proc.destroy();
			}

			TestSuite.printErrorMessage("Error during test case: " + _id);
			TestSuite.printStackTrace(e);
		}

		if (proc != null) {
			Integer retval = Integer.valueOf(proc.exitValue());
			for (int i = 0; i < _testConditions.size(); i++) {
				TestCondition tc = _testConditions.get(i);
				if (tc instanceof ReturnValue) {
					_matched[i] = ((ReturnValue) tc).match(retval);
				}
			}
		}

		if (proc != null && _timedOut) {
			return false;
		}

		for (int i = 0; i < _testConditions.size(); i++) {
			int badType = _matched[i] ? TestCondition.FAILURE : TestCondition.REQUIRED;

			if (_testConditions.get(i).getType() == badType) {
				return false;
			}
		}

		for (int i = 0; i < _testConditions.size(); i++) {
			if (_matched[i] && _testConditions.get(i).getType() == TestCondition.SUCCESS) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Prints the stdout and stderr output from the test case, as well as the reason the test case
	 * failed.
	 */
	public void dumpVerboseOutput(boolean result) {
		final String standardOutputString;
		synchronized (_standardOutput) {
			standardOutputString = _standardOutput.toString();
			// Clean up, if this function is called multiple times it will only output the new output
			_standardOutput.setLength(0);
		}
		final String errorOutputString;
		synchronized (_errorOutput) {
			errorOutputString = _errorOutput.toString();
			_errorOutput.setLength(0);
		}
		if (_outputLimit == -1) {
			System.out.println("Output from test:");
			System.out.print(standardOutputString);
			System.out.print(errorOutputString);
		} else {
			String outputStr = standardOutputString + errorOutputString;
			String[] lines = outputStr.split("\n");
			List<Integer> matchIndexes = new ArrayList<>();
			/* If test case passed, then all the success and required conditions are met */
			if (result) {
				for (int i = 0; i < _testConditions.size(); i++) {
					TestCondition tc = _testConditions.get(i);
					if (!(tc instanceof Output)) {
						continue;
					}
					switch (tc.getType()) {
					case TestCondition.REQUIRED:
					case TestCondition.SUCCESS:
						Output output = (Output) tc;
						for (int j = 0; j < lines.length; j++) {
							if (output.match(lines[j])) {
								/* Log the line number where first match is found */
								matchIndexes.add(Integer.valueOf(j));
								break;
							}
						}
						break;
					default:
						break;
					}
				}
			} else {
				/* If test case failed, then at least one of the following happened.
				 *
				 * 1. One of the required condition is not found.
				 * 2. None of the success conditions are found.
				 * 3. At least one failure condition is found.
				 *
				 * In the first two cases, just print the the beginning and end of the output
				 * depending on user specified outputLimit option.
				 * For every occurrence of the third case, print the output where failure condition is found.
				 */
				for (int i = 0; i < _testConditions.size(); i++) {
					TestCondition tc = _testConditions.get(i);
					if (!(tc instanceof Output)) {
						continue;
					}
					if (_matched[i] || (tc.getType() == TestCondition.FAILURE)) {
						Output output = (Output) tc;
						for (int j = 0; j < lines.length; j++) {
							if (output.match(outputStr)) {
								/* Log the line number where first match is found */
								matchIndexes.add(Integer.valueOf(j));
								break;
							}
						}
					}
				}

				/* No failure conditions is found, either case 1 or 2 above happened
				 * In this case, just print the beginning and end of the output.
				 */
				if (0 == matchIndexes.size()) {
					matchIndexes.add(Integer.valueOf(0)); /* First line */
					matchIndexes.add(Integer.valueOf(lines.length - 1)); /* Last line */
				}
			}

			Collections.sort(matchIndexes);

			int lineLimitPerCond = _outputLimit / matchIndexes.size();
			int end = 0;
			int lastPrintedLine = 0;
			for (int i = 0; i < matchIndexes.size(); i++) {
				Integer currentIndex = matchIndexes.get(i);
				int start = currentIndex.intValue() - (lineLimitPerCond / 2);
				start = start <= lastPrintedLine ? ((lastPrintedLine == 0) ? 0 : lastPrintedLine + 1) : start;
				end = Math.min(start + lineLimitPerCond, lines.length) - 1;

				if ((start > lastPrintedLine) && (lastPrintedLine != lines.length - 1)) {
					System.out.println();
					System.out.println("\t..........................................................");
					System.out.println("\t  ............ " + (start - lastPrintedLine) + " lines of output is removed ............");
					System.out.println("\t..........................................................");
					System.out.println();
				}

				lastPrintedLine = end;
				for (int j = start; j <= end; j++) {
					System.out.println(lines[j]);
				}
			}

			if (end < lines.length - 1) {
				System.out.println();
				System.out.println("\t...........................................................");
				System.out.println("\t  ............ " + (lines.length - 1 - end) + " lines of output is removed ............");
				System.out.println("\t...........................................................");
				System.out.println();
			}
		}

		if (!_timedOut) {
			for (int i = 0; i < _testConditions.size(); i++) {
				TestCondition tc = _testConditions.get(i);
				StringBuilder sb = new StringBuilder(">> ");
				if (tc.getType() == TestCondition.FAILURE) {
					sb.append("Failure");
				} else if (tc.getType() == TestCondition.REQUIRED) {
					sb.append("Required");
				} else if (tc.getType() == TestCondition.SUCCESS) {
					sb.append("Success");
				} else {
					sb.append("<Unknown type>");
				}
				sb.append(" condition was ");
				sb.append(_matched[i] ? "" : "not ");
				sb.append("found: [");
				sb.append(tc.toString());
				sb.append("]");
				System.out.println(sb.toString());
			}
		}
	}

	public static int getUnixPID(Process process) throws Exception {
		Class<?> cl = process.getClass();
		if (!cl.getName().equals("java.lang.UNIXProcess")) {
			return 0;
		}
		Field field = cl.getDeclaredField("pid");
		field.setAccessible(true);
		Object pidObject = field.get(process);
		return ((Integer) pidObject).intValue();
	}

	private final class StreamMatcher extends Thread {
		private final BufferedReader _br;
		private final StringBuilder _caughtOutput;
		private final String _prefix;

		StreamMatcher(InputStream in, StringBuilder caughtOutput, String prefix) {
			super();
			_br = new BufferedReader(new InputStreamReader(in));
			_caughtOutput = caughtOutput;
			_prefix = prefix;
		}

		@Override
		public void run() {
			int numO = 0;
			for (TestCondition tc : _testConditions) {
				if (tc instanceof Output) {
					numO++;
				}
			}
			final int[] outputIndices = new int[numO];
			numO = 0;
			for (int i = 0; i < _testConditions.size(); i++) {
				if (_testConditions.get(i) instanceof Output) {
					outputIndices[numO++] = i;
				}
			}
			try {
				String textRead;
				while ((textRead = _br.readLine()) != null) {
					synchronized (_caughtOutput) {
						_caughtOutput.append(_prefix).append(textRead).append('\n');
					}
					for (int index : outputIndices) {
						synchronized (_matched) {
							if (_matched[index]) {
								continue;
							}
							Output output = (Output) _testConditions.get(index);
							if (!output.isJavaUtilPattern()) {
								_matched[index] |= output.match(textRead);
							}
						}
					}
				}
				_br.close();

				/*If Java.Util.Pattern is used for regex's need to process the whole output file*/
				String fulloutputstr;
				synchronized (_caughtOutput) {
					fulloutputstr = _caughtOutput.toString();
				}
				for (int index : outputIndices) {
					synchronized (_matched) {
						if (_matched[index]) {
							continue;
						}
						Output output = (Output) _testConditions.get(index);
						if (output.isJavaUtilPattern()) {
							_matched[index] |= output.match(fulloutputstr);
						}
					}
				}
			} catch (IOException e) {
				// This exception is normal, it happens when the ProcessKiller destroys a process
			} catch (Exception e) {
				TestSuite.printStackTrace(e);
			}
		}
	}

	/* Used by process killer to read in stdout & stderr from exec'd processes. */
	private static final class StreamReader extends Thread {
		private final BufferedReader _br;
		private final StringBuilder _caughtOutput;
		private final String _prefix;
		private final boolean _printASAP;

		StreamReader(InputStream in, String prefix, boolean printASAP) {
			super();
			_br = new BufferedReader(new InputStreamReader(in));
			_caughtOutput = new StringBuilder();
			_prefix = prefix;
			_printASAP = printASAP;
			start();
		}

		@Override
		public void run() {
			try {
				String textRead;
				while ((textRead = _br.readLine()) != null) {
					if (_printASAP) {
						System.out.println(_prefix + textRead);
					} else {
						_caughtOutput.append(_prefix).append(textRead).append('\n');
					}
				}
				_br.close();
			} catch (IOException e) {
				// This exception is normal, it happens when the ProcessKiller destroys a process.
			} catch (Exception e) {
				e.printStackTrace();
			}
		}

		public String getString() {
			return _caughtOutput.toString();
		}
	}

	private final class ProcessKiller extends Thread {
		private final Process _proc;
		private final long _procTimeout;
		private final String _exeToDebug;

		public ProcessKiller(Process proc, long timeout, String exeToDebug) {
			super();
			_proc = proc;
			_procTimeout = timeout;
			_exeToDebug = exeToDebug;
			setDaemon(true);
		}

		@Override
		public synchronized void run() {
			final long endTime = System.currentTimeMillis() + _procTimeout;
			while (_proc.isAlive()) {
				long currTimeout = endTime - System.currentTimeMillis();
				if (currTimeout > 0) {
					try {
						_proc.waitFor(currTimeout, TimeUnit.MILLISECONDS);
					} catch (InterruptedException e) {
						// ignore
					}
				} else {
					_timedOut = true;
					TestSuite.printErrorMessage("ProcessKiller detected a timeout after " + _procTimeout + " milliseconds!");
					if (_debugCmdOnTimeout) {
						captureCoreForProcess();
					}
					killTimedOutProcess();
					// Dump content from stderr and stdout as soon as possible after a timeout.
					dumpVerboseOutput(false);
					break;
				}
			}
		}

		private synchronized void captureCoreForProcess() {
			if (CORE_COUNT <= 0) {
				System.out.printf("INFO: Not capturing core files because %s=%d.\n",
						CORE_COUNT_PROPERTY, Integer.valueOf(CORE_COUNT));
				return;
			}

			try {
				// Make sure we are on linux, otherwise there is no gdb.
				int pid = getUnixPID(_proc);

				if (0 == pid) {
					System.out.print("INFO: getUnixPID() has failed indicating this is not a UNIX System.");
					System.out.println("'Debug on timeout' is currently only supported on Linux.");
					return;
				}

				String osName = System.getProperty("os.name", "<unknown>");

				if (osName.toLowerCase().indexOf("linux") < 0) {
					System.out.print("INFO: The current OS is '" + osName + "'. ");
					System.out.println("'Debug on timeout' is currently only supported on Linux.");
					return;
				}

				// The path to gdb is hard coded because using '/home/j9build/bin/gdb' from the path doesn't always work.
				// For example on j9s10z1.torolab.ibm.com it caused problems capturing useful debug information.
				String gdbExe = "/usr/bin/gdb";
				File gdbExeFile = new File(gdbExe);

				if (false == gdbExeFile.exists()) {
					System.out.println("INFO: Cannot find '" + gdbExe + "' using 'gdb' from the path.");
					gdbExe = "gdb";
				}

				File commandFile = File.createTempFile("debugger", ".txt");

				commandFile.deleteOnExit();

				// Capture all but the last core, without terminating the process.
				for (int i = CORE_COUNT; i > 1; --i) {
					captureCoreForProcess(gdbExe, pid, commandFile, false);
					Thread.sleep(CORE_SPACING_MILLIS);
				}

				// Capture the final core and then terminate the process.
				captureCoreForProcess(gdbExe, pid, commandFile, true);
			} catch (Exception e) {
				e.printStackTrace();
			}
		}

		private void captureCoreForProcess(String gdbexe, int pid,
				File commandFile, boolean terminate)
				throws InterruptedException, IOException {
			// For gdb the commands must be streamed to the debugger
			// using a file. Using STDIN will not work because commands
			// received before the debugger is fully started are ignored.
			try (Writer writer = new PrintWriter(commandFile)) {
				writer.write("info shared\n");
				writer.write("info registers\n");
				writer.write("info thread\n");
				writer.write("thread apply all where full\n");
				writer.write("generate-core-file\n");

				if (!terminate) {
					writer.write("detach inferior\n");
				}

				writer.write("quit\n");
			}

			// Setup the command.
			String[] gdbCmd = new String[] {
					gdbexe,
					"-batch",
					"-x",
					commandFile.getCanonicalFile().toString(),
					_exeToDebug,
					String.valueOf(pid) 
			};

			StringBuilder debugCmd = new StringBuilder("executing");

			for (String arg : gdbCmd) {
				debugCmd.append(' ').append(arg);
			}

			TestSuite.printErrorMessage(debugCmd.toString());

			Process proc = Runtime.getRuntime().exec(gdbCmd);

			proc.getOutputStream().close();
			StreamReader stdout = new StreamReader(proc.getInputStream(), "GDB OUT ", true);
			StreamReader stderr = new StreamReader(proc.getErrorStream(), "GDB ERR ", false);

			/*
			 * Wait for a few minutes for gdb to grab the core on a busy system.
			 */
			stdout.join(_JAVACORE_TIMEOUT_MILLIS);
			stderr.join(_JAVACORE_TIMEOUT_MILLIS);

			/* Call destroy to ensure the process is really dead. At
			 * this point stdout&err are closed, or _JAVACORE_TIMEOUT_MILLIS
			 * has expired. Calling destroy has no effect if the process
			 * has exited cleanly.
			 */
			proc.destroy();
			int rc = proc.waitFor();

			if (rc != 0) {
				System.out.println("INFO: Running '" + gdbexe + "' failed with rc = " + rc);
				// Print the error stream only if gdb failed
				System.out.println(stderr.getString());
			}
		}

		private synchronized void killTimedOutProcess() {
			// If we can send a -QUIT signal to the process, send one
			try {
				int pid = getUnixPID(_proc);
				if (0 != pid) {
					TestSuite.printErrorMessage("executing kill -ABRT " + pid);
					Process proc = Runtime.getRuntime().exec("kill -ABRT " + pid);
					// waiting for kill
					proc.waitFor();
					TestSuite.printErrorMessage("kill -ABRT signal sent");

					// Waiting for the process to quit
					long startTime = System.currentTimeMillis();
					while ((System.currentTimeMillis() - startTime) < _JAVACORE_TIMEOUT_MILLIS) {
						try {
							_proc.exitValue();
						} catch (IllegalThreadStateException e) {
							// process not done yet
							Thread.sleep(100);
							continue;
						}
						TestSuite.printErrorMessage("ABRT completed");
						return;
					}
					TestSuite.printErrorMessage("ABRT timed out");
					/* When ABRT times out, try to kill the process with kill -9 to make sure it doesn't stop the rest */
					TestSuite.printErrorMessage("executing kill -9 " + pid);
					Process procKill9 = Runtime.getRuntime().exec("kill -9 " + pid);
					procKill9.waitFor();
					TestSuite.printErrorMessage("kill -9 signal sent");
				}
			} catch (IOException e) {
				// FIXME
			} catch (Exception e) {
				// FIXME
			}
		}
	}

	public void destroy() {
		synchronized (_standardOutput) {
			_standardOutput.setLength(0);
		}
		synchronized (_errorOutput) {
			_errorOutput.setLength(0);
		}
	}
}
