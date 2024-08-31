/*
 * Copyright IBM Corp. and others 2020
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package jit.test.jitserver;

import java.util.Arrays;
import java.util.Random;
import java.util.concurrent.TimeUnit;
import java.util.Scanner;
import java.util.ArrayList;
import java.util.Comparator;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.FileNotFoundException;
import java.net.Socket;

import org.testng.AssertJUnit;
import org.testng.SkipException;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups={ "level.sanity", "component.jit" })
public class JITServerTest {
	private static Logger logger = Logger.getLogger(JITServerTest.class);

	private static final int PROCESS_START_WAIT_TIME_MS = 2 * 1000;
	private static final int PROCESS_TERMINATE_WAIT_TIME_MS = 2 * 1000;
	private static final int SERVER_START_WAIT_TIME_MS = 5 * 1000;
	private static final int CLIENT_TEST_TIME_MS = 45 * 1000;
	private static final int SUCCESS_RETURN_VALUE = 0;
	private static final int DESTROY_SCC_WAIT_TIME_MS = 1500;

	private final ProcessBuilder clientBuilder;
	private final ProcessBuilder serverBuilder;
	private final boolean updatePorts;

	private static final String SERVER_PORT_ENV_VAR_NAME = "JITServerTest_SERVER_PORT";
	private static final String JITSERVER_PORT_OPTION_FORMAT_STRING = "-XX:JITServerPort=%d";
	private final String aotCacheOption = "-XX:+JITServerUseAOTCache";

	private static final String CLIENT_EXE = System.getProperty("CLIENT_EXE");
	// This handy regex pattern uses positive lookahead to match a string containing either zero or an even number of " (double quote) characters.
	// If a character is followed by this pattern it means that the character itself is not in a quoted string, otherwise it would be followed by
	// an odd number of " characters. Note that this doesn't handle ' (single quote) characters.
	private static final String QUOTES_LOOKAHEAD_PATTERN = "(?=([^\"]*\"[^\"]*\")*[^\"]*$)";
    // We want to split the client program string on whitespace, unless the space appears in a quoted string.
    private static final String SPLIT_ARGS_PATTERN = "\\s+" + QUOTES_LOOKAHEAD_PATTERN;

	JITServerTest() {
		AssertJUnit.assertEquals("Tests have only been validated on Linux. Other platforms are currently unsupported.", "Linux", System.getProperty("os.name"));

		final String SERVER_EXE = System.getProperty("SERVER_EXE");
		final String CLIENT_PROGRAM = System.getProperty("CLIENT_PROGRAM");
		// -Xjit options may already be on the command line so add extra JIT options via TR_Options instead to avoid one overriding the other.
		final String JIT_LOG_ENV_OPTION = "verbose={compileEnd|JITServer|heartbeat}";

		// Set the port if it was specified by a user
		final String userPort = System.getenv().get(SERVER_PORT_ENV_VAR_NAME);

		String portOption;
		if (userPort != null) {
			portOption = String.format(JITSERVER_PORT_OPTION_FORMAT_STRING, Integer.parseInt(userPort));
			logger.info("Using " + SERVER_PORT_ENV_VAR_NAME + "=" + userPort + " from env for server port.");
			updatePorts = false;
		} else {
			portOption = generatePortOption();
			updatePorts = true;
		}

		clientBuilder = new ProcessBuilder(stripQuotesFromEachArg(String.join(" ", CLIENT_EXE, portOption, "-XX:+UseJITServer", CLIENT_PROGRAM).split(SPLIT_ARGS_PATTERN)));
		if (CLIENT_PROGRAM.contains(aotCacheOption))
			serverBuilder = new ProcessBuilder(stripQuotesFromEachArg(String.join(" ", SERVER_EXE, portOption, "-XX:-JITServerHealthProbes", aotCacheOption).split(SPLIT_ARGS_PATTERN)));
		else
			serverBuilder = new ProcessBuilder(stripQuotesFromEachArg(String.join(" ", SERVER_EXE, portOption, "-XX:-JITServerHealthProbes").split(SPLIT_ARGS_PATTERN)));

		// Redirect stderr to stdout, one log for each of the client and server is sufficient.
		clientBuilder.redirectErrorStream(true);
		serverBuilder.redirectErrorStream(true);
		// Join our TR_Options with others if they exist, otherwise just set ours.
		clientBuilder.environment().compute("TR_Options", (k, v) -> v != null && !v.isEmpty() ? String.join(",", v, JIT_LOG_ENV_OPTION) : JIT_LOG_ENV_OPTION);
		serverBuilder.environment().compute("TR_Options", (k, v) -> v != null && !v.isEmpty() ? String.join(",", v, JIT_LOG_ENV_OPTION) : JIT_LOG_ENV_OPTION);

	}

	private void updateJITServerPort() {
		// If user provided a specific port, do not update
		if (!updatePorts)
			return;
		// The port must be the first parameter passed to the program
		int portOptionBaseLength = JITSERVER_PORT_OPTION_FORMAT_STRING.length() - 2;
		String portOptionBaseStr = JITSERVER_PORT_OPTION_FORMAT_STRING.substring(0, portOptionBaseLength);

		String firstClientArg = clientBuilder.command().get(1);
		firstClientArg = firstClientArg.length() > portOptionBaseLength ? firstClientArg.substring(0, portOptionBaseLength) : firstClientArg;
		AssertJUnit.assertEquals("-XX:JITServerPort=<port> must be the first argument passed to the client",
			firstClientArg,
			JITSERVER_PORT_OPTION_FORMAT_STRING.substring(0, portOptionBaseLength));

		String firstServerArg = serverBuilder.command().get(1);
		firstServerArg = firstServerArg.length() > portOptionBaseLength ? firstServerArg.substring(0, portOptionBaseLength) : firstServerArg;
		AssertJUnit.assertEquals("-XX:JITServerPort=<port> must be the first argument passed to the server",
			firstServerArg,
			JITSERVER_PORT_OPTION_FORMAT_STRING.substring(0, portOptionBaseLength));

		// Generate a new port and update process builders
		String newPortOption = generatePortOption();
		logger.info("Chose random port for server: " + newPortOption + ", set " + SERVER_PORT_ENV_VAR_NAME + " in your env to override.");

		clientBuilder.command().set(1, newPortOption);
		serverBuilder.command().set(1, newPortOption);
	}

	private static String generatePortOption() {
		// Most systems have a specified ephemeral ports range. We're not bothering to find the actual range, just choosing a range that is outside the reserved area, reasonably large, and well-behaved.
		// The range chosen here is within the actual ephemeral range on recent Linux systems and others (at the time of writing).
		final int EPHEMERAL_PORTS_START = 33000, EPHEMERAL_PORTS_LAST = 60000;

		// In order to run on multi-user systems we need to use a unique server port.
		// We choose a random one in the ephemeral ports range here; the user can override via an env var if they want.
		// This scheme is not perfect. We start many servers while executing this class using our random port so if someone else grabs the port
		// in the middle of our test some of our servers will fail to start.
		// The only way to avoid most of the races is to have the server choose a random port (and retry if it is busy) and for us read the port
		// from the server log. We still need to worry about tests where we stop and restart the server, because we need to keep using the same
		// port but someone may grab it in the interim.
		int randomPort = EPHEMERAL_PORTS_START + new Random().nextInt(EPHEMERAL_PORTS_LAST - EPHEMERAL_PORTS_START + 1);
		while (!isPortOpen(randomPort)) {
			String[] portInfo = infoOfProcessUsingPort(randomPort);
			if (0 != portInfo.length) {
				logger.info("Port " + randomPort + " is busy. Process info:");
				for (int i = 0; i < portInfo.length; ++i) {
					logger.info(portInfo[i]);
				}
			} else {
				logger.info("Port " + randomPort + " is busy");
			}

			randomPort = EPHEMERAL_PORTS_START + new Random().nextInt(EPHEMERAL_PORTS_LAST - EPHEMERAL_PORTS_START + 1);
		}
		return String.format(JITSERVER_PORT_OPTION_FORMAT_STRING, randomPort);
	}

	private static boolean isPortOpen(int port) {
		Socket s = null;
		try {
			// Connect to the port as a client. If this succeeds, then something else is using that port.
			s = new Socket((String)null, port);
			s.close();
			return false;
		} catch (IOException e) {
			// A client connection could not be established, so nothing is using the port.
			return true;
		} catch (SecurityException e) {
			// Access to the port is forbidden.
			return false;
		}
	}

	private static String[] infoOfProcessUsingPort(int port) {
		String[] output = {};
		if (System.getProperty("os.name").toLowerCase().contains("linux")) {
			try {
				Process proc = new ProcessBuilder("lsof", "-i", ":" + port).start();
				proc.waitFor();
				if (proc.exitValue() == 0) {
					BufferedReader stdOutput = new BufferedReader(new InputStreamReader(proc.getInputStream()));
					ArrayList<String> al = new ArrayList<String>();
					String line = null;
					while ((line = stdOutput.readLine()) != null) {
						al.add(line);
					}
					output = al.toArray(new String[0]);
				}
			} catch (IOException|InterruptedException e) {
				// Do nothing.
			}
		}
		return output;
	}

	private static String[] stripQuotesFromEachArg(String[] args) {
		for (int i = 0; i < args.length; ++i) {
			args[i] = args[i].replaceAll("\"", "");
		}
		return args;
	}

	private static void dumpProcessLog(final ProcessBuilder b) {
		File log = b.redirectOutput().file();
		try {
			final String topAndBottom = "////////////////////////////////////////////////////////////////////////";
			final String leftMargin = "//// ";
			Scanner s = new Scanner(log);
			System.err.println("Dumping the contents of log file: " + log.getAbsolutePath() + "\n" + topAndBottom);
			while (s.hasNextLine())
				System.err.println(leftMargin + s.nextLine());
			System.err.println(topAndBottom);
		}
		catch (FileNotFoundException e) {
			System.err.println("Attempted to dump the log file '" + log.getAbsolutePath() + "' but it was not found.\n" + e.getMessage());
		}
	}

	// Count the number of lines that contain a match for the given regex
	private static int countRegexLinesInFile(String regexToLookFor, File file1) throws FileNotFoundException,IOException {
		int count = 0;
		String line = null;
		String[] buffer;

		BufferedReader bfReader = new BufferedReader(new FileReader(file1));
		while((line = bfReader.readLine()) != null)   {
			if (line.split(regexToLookFor, 2).length > 1)
				count++;
		}
		logger.info("File: "+file1.getName()+" has "+count+" lines with the regex \""+regexToLookFor+"\"");
		bfReader.close();
		return count;
        }

	private static boolean checkLogFiles(String fileNamePattern, String regexToLookFor) {
		boolean foundMatch = false;
		try{
			File f = new File(System.getProperty("user.dir"));
			FilenameFilter filter = new FilenameFilter() {
				public boolean accept(File f, String name){
					return name.matches(fileNamePattern);
				}
			};

			//List all the log files matching the criteria in the order of last modified
			File[] files = f.listFiles(filter);
			Arrays.sort(files, Comparator.comparingLong(File::lastModified).reversed());

			if(files == null)
				logger.info("No files matching the pattern " + fileNamePattern + " in current directory.");

			for (int i = 0; i < files.length; i++) {
				logger.info("Checking file " + files[i] + " for string "+ regexToLookFor );
				if (countRegexLinesInFile(regexToLookFor,files[i]) > 0){
					foundMatch = true;
					break;
				}
			}
		} catch (FileNotFoundException e) {
			System.err.println("FileNotFoundException in checkLogFiles: " + e.getMessage());
		} catch (IOException e) {
                        System.err.println("IOException in checkLogFiles: " + e.getMessage());
		}
		return foundMatch;
	}

	private static boolean runSCCCommand(String cmd, String cacheName) {
		boolean found = false;
		try {
			ProcessBuilder pb = new ProcessBuilder(stripQuotesFromEachArg(String.join(" ", CLIENT_EXE, cmd).split(SPLIT_ARGS_PATTERN)));
			pb.redirectErrorStream(true);
			Process proc = pb.start();
			BufferedReader stdOutput = new BufferedReader(new InputStreamReader(proc.getInputStream()));
			String line = null;
			while ((line = stdOutput.readLine()) != null) {
				if (cmd.contains("listallcaches")){
					if (line.startsWith(cacheName)) {
						logger.info(line);
						found=true;
						break;
					}
				} else if(cmd.contains("printallstats=aot")) {
					if (line.contains("# AOT Methods")) {
						logger.info(line);
						int value = Integer.parseInt(line.replaceAll("[^0-9]", ""));
						if (value > 0)
							found = true;
						break;
					}
				} else if(cmd.contains("destroy")) {
					if (line.contains("\"" + cacheName + "\""+" has been destroyed")) {
						logger.info(line);
						found = true;
						break;
					}
				} else {
					logger.info(line);
				}
			}
			proc.waitFor();
		} catch (IOException|InterruptedException e) {
			// Do nothing.
		}
                return found;
        }

	private static boolean checkCacheExists(String cacheName) {
                String cmd = "-Xshareclasses:listallcaches";
                return (runSCCCommand(cmd, cacheName));
        }

        private static boolean checkAOTCompileExists(String cacheName) {
                String cmd = "-Xshareclasses:name="+cacheName+",printallstats=aot";
                return (runSCCCommand(cmd, cacheName));
        }

        private static boolean destroyCache(String cacheName) {
                String cmd = "-Xshareclasses:name="+cacheName+",destroy";
                return (runSCCCommand(cmd, cacheName));
        }

	private static void destroyAndCheckProcess(final Process p, final ProcessBuilder builder) throws InterruptedException {
		final int PROCESS_DESTROY_WAIT_TIME_MS = 5 * 1000;
		// On *nix systems Process.destroy() sends SIGTERM to the process and (unless the process handles it and decides to return something else) the return value will be signum+128.
		// SIGTERM is 15 on most *nix systems, but this can vary.
		final int SIGTERM_RETURN_VALUE = 15 + 128;

		p.destroy();

		int waitCount = 0;
		while (!p.waitFor(PROCESS_DESTROY_WAIT_TIME_MS, TimeUnit.MILLISECONDS) && (waitCount < 12)) {
			waitCount++;
		}

		if (p.isAlive()) {
			// The process wasn't destroyed after PROCESS_DESTROY_WAIT_TIME_MS * waitCount,
			// so have to destroy it forcibly now and fail the test case
			p.destroyForcibly().waitFor();

			final String errorText = "The process is still alive after waiting for " + (PROCESS_DESTROY_WAIT_TIME_MS * waitCount) + "ms.";
			AssertJUnit.fail(errorText);
		}

		final int exitValue = p.exitValue();

		// The process may exit normally before we can destroy it so we have to accept two possible return values.
		if ((exitValue != SUCCESS_RETURN_VALUE) && (exitValue != SIGTERM_RETURN_VALUE)) {
			final String errorText = "Expected an exit value of " + SUCCESS_RETURN_VALUE + " or " + SIGTERM_RETURN_VALUE + ", got " + exitValue + " instead.";
			System.err.println(errorText);
			dumpProcessLog(builder);
			AssertJUnit.fail(errorText);
		}
	}

	private static Process startProcess(final ProcessBuilder builder, final String name) throws IOException, InterruptedException {
		// If a previous test did not terminate the process, kill it here (e.g. if assert got triggered before server was terminated)
		final String commandLine = String.join(" ", builder.command());
		final String pkillCommandLine[] = {"pkill", "-TERM", "-f", "-x", escapeCommandLineRegexChars(commandLine)};
		if (Runtime.getRuntime().exec(pkillCommandLine).waitFor() == SUCCESS_RETURN_VALUE) {
			logger.info("Terminating the previous version of " + name);
			// if there is a process to kill, wait for it to terminate
			Thread.sleep(PROCESS_TERMINATE_WAIT_TIME_MS);
		}

		// Wrap any arguments containing whitespace in quotes for display (we have to make a copy of ProcessBuilder.command() to avoid modifying our PB's commands).
		ArrayList<String> command = new ArrayList<String>(builder.command());
		command.replaceAll(s -> s.matches("\\S+") ? s : "\"" + s + "\"");
		logger.info("Starting " + name + " with command line:\n" + String.join(" ", command));
		logger.info("With stdout/stderr redirected to:\n" + builder.redirectOutput().file().getAbsolutePath());
		final Process p = builder.start();
		// We expect these processes to be fairly long running; if they exit almost immediately abort the test.
		if (p.waitFor(PROCESS_START_WAIT_TIME_MS, TimeUnit.MILLISECONDS)) {
			final String errorText = "Failed to properly start " + name + ", it terminated prematurely with exit value: " + p.exitValue();
			System.err.println(errorText);
			dumpProcessLog(builder);
			AssertJUnit.fail(errorText);
		}
		return p;
	}

	// `pkill` expects a regex pattern as the last argument. We need to pass in a JVM command line, but we don't want any regex-like patterns
	// (e.g. certain -Xjit sub-options) on the command line to be interpreted as a regex by pkill so we need to escape any special chars.
	private static String escapeCommandLineRegexChars(final String commandLine) { return commandLine.replaceAll("[{+}|]", "\\\\$0"); }

	// "Pause" and "resume" are implemented with SIGSTOP and SIGCONT; they should work on most *nix platforms, but YMMV. Windows would need another solution.
	private static void pauseProcessWithCommandLine(final String commandLine) throws IOException, InterruptedException {
		final String pkillCommandLine[] = {"pkill", "-STOP", "-f", "-x", escapeCommandLineRegexChars(commandLine)};
		logger.debug("Running pkill with command line: " + Arrays.toString(pkillCommandLine));
		AssertJUnit.assertEquals("Unable to pause target process, pkill failed or did not match any process.", SUCCESS_RETURN_VALUE, Runtime.getRuntime().exec(pkillCommandLine).waitFor());
	}

	private static void resumeProcessWithCommandLine(final String commandLine) throws IOException, InterruptedException {
		final String pkillCommandLine[] = {"pkill", "-CONT", "-f", "-x", escapeCommandLineRegexChars(commandLine)};
		logger.debug("Running pkill with command line: " + Arrays.toString(pkillCommandLine));
		AssertJUnit.assertEquals("Unable to resume target process, pkill failed or did not match any process.", SUCCESS_RETURN_VALUE, Runtime.getRuntime().exec(pkillCommandLine).waitFor());
	}

	private static void redirectProcessOutputs(final ProcessBuilder builder, final String outputName) {
		builder.redirectOutput(new File(outputName + ".out"));
		// Add the vlog= option (or replace it if it was previously added) to TR_Options.
		// Note: This code only handles vlog appearing at the end of TR_Options.
		final String TR_Options = builder.environment().get("TR_Options").replaceFirst(",vlog=.+\\.jitverboselog\\.out$", "");
		builder.environment().put("TR_Options", TR_Options + ",vlog=" + outputName + ".jitverboselog.out");
	}

	public void testServer() throws IOException, InterruptedException {
		logger.info("running testServer: INFO and above level logging enabled");

		redirectProcessOutputs(clientBuilder, "testServer.client");
		redirectProcessOutputs(serverBuilder, "testServer.server");

		updateJITServerPort();

		final Process server = startProcess(serverBuilder, "server");

		Thread.sleep(SERVER_START_WAIT_TIME_MS);

		final Process client = startProcess(clientBuilder, "client");

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		logger.info("Stopping client...");
		destroyAndCheckProcess(client, clientBuilder);

		logger.info("Stopping server...");
		destroyAndCheckProcess(server, serverBuilder);
	}

	public void testServerGoesDown() throws IOException, InterruptedException {
		logger.info("running testServerGoesDown: INFO and above level logging enabled");

		redirectProcessOutputs(clientBuilder, "testServerGoesDown.client");
		redirectProcessOutputs(serverBuilder, "testServerGoesDown.server");

		updateJITServerPort();

		final Process server = startProcess(serverBuilder, "server");

		Thread.sleep(SERVER_START_WAIT_TIME_MS);

		final Process client = startProcess(clientBuilder, "client");

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		logger.info("Stopping server...");
		destroyAndCheckProcess(server, serverBuilder);

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		logger.info("Stopping client...");
		destroyAndCheckProcess(client, clientBuilder);
	}

	public void testServerGoesDownAnotherComesUp() throws IOException, InterruptedException {
		logger.info("running testServerGoesDownAnotherComesUp: INFO and above level logging enabled");

		redirectProcessOutputs(clientBuilder, "testServerGoesDownAnotherComesUp.client");
		redirectProcessOutputs(serverBuilder, "testServerGoesDownAnotherComesUp.server");

		updateJITServerPort();

		Process client = null;

		{
			final Process server = startProcess(serverBuilder, "server");

			Thread.sleep(SERVER_START_WAIT_TIME_MS);

			client = startProcess(clientBuilder, "client");

			logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
			Thread.sleep(CLIENT_TEST_TIME_MS);

			logger.info("Stopping server...");
			destroyAndCheckProcess(server, serverBuilder);
		}

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		redirectProcessOutputs(serverBuilder, "testServerGoesDownAnotherComesUp.secondServer");

		{
			final Process secondServer = startProcess(serverBuilder, "server");

			Thread.sleep(SERVER_START_WAIT_TIME_MS);

			logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
			Thread.sleep(CLIENT_TEST_TIME_MS);

			logger.info("Stopping client...");
			destroyAndCheckProcess(client, clientBuilder);

			logger.info("Stopping server...");
			destroyAndCheckProcess(secondServer, serverBuilder);
		}
	}

	public void testServerUnreachableForAWhile() throws IOException, InterruptedException {
		final int SERVER_UNREACHABLE_TIME_MS = 10 * 1000;

		logger.info("running testServerUnreachableForAWhile: INFO and above level logging enabled");

		redirectProcessOutputs(clientBuilder, "testServerUnreachableForAWhile.client");
		redirectProcessOutputs(serverBuilder, "testServerUnreachableForAWhile.server");

		updateJITServerPort();

		final Process server = startProcess(serverBuilder, "server");

		Thread.sleep(SERVER_START_WAIT_TIME_MS);

		final Process client = startProcess(clientBuilder, "client");

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		final String serverCommandLine = String.join(" ", serverBuilder.command());
		logger.info("Pausing server for " + SERVER_UNREACHABLE_TIME_MS + " millis.");
		pauseProcessWithCommandLine(serverCommandLine);
		Thread.sleep(SERVER_UNREACHABLE_TIME_MS);

		logger.info("Resuming server...");
		resumeProcessWithCommandLine(serverCommandLine);

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		logger.info("Stopping server...");
		destroyAndCheckProcess(server, serverBuilder);

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		logger.info("Stopping client...");
		destroyAndCheckProcess(client, clientBuilder);
	}

	public void testServerComesUpAfterClient() throws IOException, InterruptedException {
		logger.info("running testServerComesUpAfterClient: INFO and above level logging enabled");

		redirectProcessOutputs(clientBuilder, "testServerComesUpAfterClient.client");
		redirectProcessOutputs(serverBuilder, "testServerComesUpAfterClient.server");

		updateJITServerPort();

		final Process client = startProcess(clientBuilder, "client");

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		final Process server = startProcess(serverBuilder, "server");

		Thread.sleep(SERVER_START_WAIT_TIME_MS);

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		logger.info("Stopping client...");
		destroyAndCheckProcess(client, clientBuilder);

		logger.info("Stopping server...");
		destroyAndCheckProcess(server, serverBuilder);
	}

	public void testServerComesUpAfterClientAndGoesDownAgain() throws IOException, InterruptedException {
		logger.info("running testServerComesUpAfterClientAndGoesDownAgain: INFO and above level logging enabled");

		redirectProcessOutputs(clientBuilder, "testServerComesUpAfterClientAndGoesDownAgain.client");
		redirectProcessOutputs(serverBuilder, "testServerComesUpAfterClientAndGoesDownAgain.server");

		updateJITServerPort();

		final Process client = startProcess(clientBuilder, "client");

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		final Process server = startProcess(serverBuilder, "server");

		Thread.sleep(SERVER_START_WAIT_TIME_MS);

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		logger.info("Stopping server...");
		destroyAndCheckProcess(server, serverBuilder);

		logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
		Thread.sleep(CLIENT_TEST_TIME_MS);

		logger.info("Stopping client...");
		destroyAndCheckProcess(client, clientBuilder);
	}

	public void testServerAOTCache() throws IOException, InterruptedException {
                logger.info("running testServerSCC: INFO and above level logging enabled");

		// Run this tests only for the test variation with AOT Cache option specified
		if(System.getProperty("CLIENT_PROGRAM").contains(aotCacheOption)) {
			redirectProcessOutputs(clientBuilder, "testServerAOTCache.client");
			redirectProcessOutputs(serverBuilder, "testServerAOTCache.server");

			updateJITServerPort();

			if (checkCacheExists("test_jitscc"))
				destroyCache("test_jitscc");

			final Process server = startProcess(serverBuilder, "server");

			Thread.sleep(SERVER_START_WAIT_TIME_MS);

			Process client = startProcess(clientBuilder, "client");

			logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
			Thread.sleep(CLIENT_TEST_TIME_MS);

			logger.info("Check whether the cache test_jitscc exists.");
			if(checkCacheExists("test_jitscc"))
				logger.info("Check if AOT methods exists in the cache test_jitscc:" + checkAOTCompileExists("test_jitscc"));
			else
				AssertJUnit.fail("The cache test_jitscc is not created.");

			logger.info("Stopping client...");
			destroyAndCheckProcess(client, clientBuilder);

			// Destroy the SCC and then restart the client
			logger.info("Destroy the cache test_jitscc");
			destroyCache("test_jitscc");
			Thread.sleep(DESTROY_SCC_WAIT_TIME_MS);

			// Check if the SCC exists and then restart the client
			logger.info("Check whether the cache test_jitscc exists.");
			if (checkCacheExists("test_jitscc"))
				AssertJUnit.fail("The cache test_jitscc should have been destroyed.");

			logger.info("Checking if serialization of methods are logged at the server");
			if(!checkLogFiles("testServerAOTCache.server.jitverboselog.out.*" , "serialization")) {
				logger.info("No serialized methods found at the server. Stopping server...");
				destroyAndCheckProcess(server, serverBuilder);
				AssertJUnit.fail("There are no serialized methods at the server.");
			}

			logger.info("Restarting client...");
			int randomLogId = new Random().nextInt();
			redirectProcessOutputs(clientBuilder, "testServerAOTCache.client"+randomLogId);
			client = startProcess(clientBuilder, "client");

			logger.info("Waiting for " + CLIENT_TEST_TIME_MS + " millis.");
			Thread.sleep(CLIENT_TEST_TIME_MS);

			logger.info("Stopping client...");
			destroyAndCheckProcess(client, clientBuilder);

			// Destroy the SCC for cleanup
			logger.info("Destroy the cache test_jitscc");
			destroyCache("test_jitscc");
			Thread.sleep(DESTROY_SCC_WAIT_TIME_MS);

			if(!checkLogFiles("testServerAOTCache.client"+randomLogId+".jitverboselog.out.*", "remote deserialized")) {
				logger.info("No deserialized methods found at the client.");
				if(!checkLogFiles("testServerAOTCache.client"+randomLogId+".jitverboselog.out.*", "\\+.* remote")) {
					logger.info("No successful remote compilations found at the client.");
				}
				logger.info("Stopping server...");
				destroyAndCheckProcess(server, serverBuilder);
				AssertJUnit.fail("There are no deserialized methods at the client.");
			}

			logger.info("Stopping server...");
			destroyAndCheckProcess(server, serverBuilder);
		}
	}
}
