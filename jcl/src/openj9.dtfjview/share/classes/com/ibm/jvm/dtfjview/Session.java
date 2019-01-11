/*[INCLUDE-IF Sidecar18-SE]*/
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
package com.ibm.jvm.dtfjview;

import static com.ibm.jvm.dtfjview.ExitCodes.JDMPVIEW_APPEND_OVERWRITE_COEXIST;
import static com.ibm.jvm.dtfjview.ExitCodes.JDMPVIEW_FILE_ERROR;
import static com.ibm.jvm.dtfjview.ExitCodes.JDMPVIEW_INTERNAL_ERROR;
import static com.ibm.jvm.dtfjview.ExitCodes.JDMPVIEW_OUTFILE_EXISTS;
import static com.ibm.jvm.dtfjview.ExitCodes.JDMPVIEW_SYNTAX_ERROR;
import static com.ibm.jvm.dtfjview.SessionProperties.CORE_FILE_PATH_PROPERTY;
import static com.ibm.jvm.dtfjview.SessionProperties.EXTRACT_PROPERTY;
import static com.ibm.jvm.dtfjview.SessionProperties.LEGACY_ZIP_MODE_PROPERTY;
import static com.ibm.jvm.dtfjview.SessionProperties.PWD_PROPERTY;
import static com.ibm.jvm.dtfjview.SessionProperties.SESSION_PROPERTY;
import static com.ibm.jvm.dtfjview.SessionProperties.VERBOSE_MODE_PROPERTY;
import static com.ibm.jvm.dtfjview.SystemProperties.SYSPROP_FACTORY;
import static com.ibm.jvm.dtfjview.SystemProperties.SYSPROP_LAUNCHER;
import static com.ibm.jvm.dtfjview.SystemProperties.SYSPROP_NOSYSTEMEXIT;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.Set;
import java.util.logging.ConsoleHandler;
import java.util.logging.Formatter;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.logging.SimpleFormatter;

import com.ibm.dtfj.image.ImageFactory;
import com.ibm.java.diagnostics.utils.IDTFJContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.commands.CommandParser;
import com.ibm.jvm.dtfjview.spi.ICombinedContext;
import com.ibm.jvm.dtfjview.spi.IOutputChannel;
import com.ibm.jvm.dtfjview.spi.IOutputManager;
import com.ibm.jvm.dtfjview.spi.ISession;
import com.ibm.jvm.dtfjview.spi.ISessionContextManager;
import com.ibm.jvm.dtfjview.tools.ToolsRegistry;
import com.ibm.jvm.dtfjview.tools.ToolsRegistryOutputChannels;
import com.ibm.jvm.dtfjview.tools.impl.CmdFileTool;

//TODO : future ideas
// have the OPEN command process all files in a directory
// support for TERSE files
// add to ImageModule getSourceName, isSourceFromHost, spot resolution from local path
public class Session implements ISession {

	public static final String LOGGER_PROPERTY = "com.ibm.jvm.dtfjview.logger";
	private String factoryName;
	
	private IOutputManager out;
	
	private static final String PROMPT_FORMAT_DEFAULT = "> ";
	private static final String PROMPT_FORMAT_MULTICONTEXT = "CTX:%d> ";
	public static String prompt = "> ";
	
	private static final String ARG_CORE = "-core";
	private static final String ARG_ZIP = "-zip";
	private static final String ARG_VERSION = "-version";
	private static final String ARG_VERBOSE = "-verbose";
	private static final String ARG_CMDFILE = "-cmdfile";
	private static final String ARG_CHARSET = "-charset";
	private static final String ARG_OUTFILE = "-outfile";
	private static final String ARG_OVERWRITE = "-overwrite";
	private static final String ARG_APPEND = "-append";
	private static final String ARG_NOEXTRACT = "-notemp";
	private static final String ARG_LEGACYZIP = "-legacyzip";
	
	private static enum CmdLineParseStateEnum {					//controls the parsing of the command line args
		PARSE_SCAN,				//parsing is scanning the entries
		PARSE_PAIR,				//parsing a name / value pair
		PARSE_SINGLE,			//parsing a single value e.g. -version
		PARSE_CMDS,				//parsing any commands at the end of the line
		PARSE_COMPLETE			//parsing is complete
	};
	
	private CmdLineParseStateEnum parseState = CmdLineParseStateEnum.PARSE_SCAN;		//default parse state
	
	private final Map<String, String> args = new HashMap<String, String>();				//map for name/value pair args
	private final Set<String> singleargs = new HashSet<String>();						//commands that are single args only e.g. -verbose
	private final Set<String> pairedargs = new HashSet<String>();
 	private final Queue<String> commands = new LinkedList<String>();					//commands that could be specified on the command line
 	private final Set<String> sessioncommands = new HashSet<String>();					// commands which should execute against the session context
	private boolean isInteractiveSession = false;										//flag to indicated if running in batch or interactive mode
	private boolean isVerboseEnabled = false;											//flag for verbose logging
	private IOutputChannel defaultOutputChannel = null;
	private FileOutputChannel foc = null;
	private String charsetName = null; 
	
//	private final ArrayList<CombinedContext> contexts = new ArrayList<CombinedContext>();		//the contexts related to the runtimes from core files
	private CombinedContext ctxroot = null;												//root context for console access

	private int ctxid = 0;																//the current context in which a give command executes
	private ICombinedContext currentContext = null;
	private static final String CMD_CONTEXT = "context";
	private static final String CMD_OPEN = "open";
	private static final String CMD_CLOSE = "close";
	private static final String CMD_CD = "cd";
	private static final String CMD_PWD = "pwd";
	private boolean isZOS = false;														//flag to control some formatting aspects													
	
	private final HashMap<String, Object> variables = new HashMap<String, Object>();	//variables to propagate into contexts
	
	private final Logger logger = Logger.getLogger(LOGGER_PROPERTY);					//logger for verbose messages
	
	private final ISessionContextManager ctxmgr = new JdmpviewContextManager();
	
	{
		//define the list of supported command line args
		singleargs.add(ARG_VERBOSE);
		singleargs.add(ARG_VERSION);
		singleargs.add(ARG_OVERWRITE);
		singleargs.add(ARG_APPEND);
		singleargs.add(ARG_NOEXTRACT);
		singleargs.add(ARG_LEGACYZIP);
		pairedargs.add(ARG_CMDFILE);
		pairedargs.add(ARG_CORE);
		pairedargs.add(ARG_OUTFILE);
		pairedargs.add(ARG_CHARSET);
		pairedargs.add(ARG_ZIP);
		
		// define the list of session commands - ones which run against the session context, not the per-image context
		// set logging and show logging are also session commands but are dealt with separately
		sessioncommands.add(CMD_CLOSE);
		sessioncommands.add(CMD_OPEN);
		sessioncommands.add(CMD_CD);
		sessioncommands.add(CMD_PWD);

	}
	
	/**
	 * Factory method for creating new sessions.
	 * 
	 * @param args any session arguments
	 * @return the session
	 */
	public static ISession getInstance(String[] args) {
		return new Session(args);
	}
	
	private Session(String[] args) {
		sessionInit(args);
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISession#getOutput()
	 */
	public IOutputManager getOutputManager() {
		return out;
	}
	
	public String getCharset() {
		return charsetName;
	}

	private void sessionInit(String[] cmdlineargs) {
		out = new Output();
		defaultOutputChannel = new ConsoleOutputChannel();
		out.addChannel(defaultOutputChannel);
		variables.put(PWD_PROPERTY, new File(System.getProperty("user.dir")));
		variables.put(LOGGER_PROPERTY, Logger.getLogger(LOGGER_PROPERTY));
		parseCommandLineArgs(cmdlineargs);					//parse command line arguments
		variables.put(EXTRACT_PROPERTY, !hasFlagBeenSet(ARG_NOEXTRACT));
		variables.put(LEGACY_ZIP_MODE_PROPERTY, hasFlagBeenSet(ARG_LEGACYZIP));
		isVerboseEnabled = hasFlagBeenSet(ARG_VERBOSE);
		charsetName = args.get(ARG_CHARSET);

		ToolsRegistry.initialize(this);
		ToolsRegistryOutputChannels.addChannel(defaultOutputChannel);

		if(isVerboseEnabled) {
			enableConsoleLogging(logger);
			enableConsoleLogging(Logger.getLogger(ImageFactory.DTFJ_LOGGER_NAME));
//			enableConsoleLogging(Logger.getLogger(PluginConstants.LOGGER_NAME));
			variables.put(VERBOSE_MODE_PROPERTY, "true");
		}
		logCommandLineArgs();
		processCommandFile();								//process the command file if one has been set
		processOutfile();
		isInteractiveSession = (args.get(ARG_CMDFILE) == null) && (commands.isEmpty());		//interactive if no commands have been supplied
		//create a console context which will allow other commands such as the open command to function
		ctxroot = new CombinedContext(0, 0, null, null, null, null, -1);
		initializeContext(ctxroot);
		currentContext = ctxroot;			//default to the root context at startup
	
		// Set system property to switch off additional DDR processing for Node.JS
		System.setProperty("com.ibm.j9ddr.noextrasearchfornode","true");
		
		if(args.containsKey(ARG_CORE) || args.containsKey(ARG_ZIP) || args.containsKey(ARG_VERSION)) {
			//file to open has been specified on the command line
			// Note that although -version is not really an "open" action we pass it to the "open" action because
			// that knows how to create the dtfj factory and thereby get the version info
			imageFromCommandLine();
		}
	}
	
	//initializes the supplied context with its starting parameters and property values
	private void initializeContext(IDTFJContext ctx) {
		ctx.refresh();
		ctx.getProperties().put(SESSION_PROPERTY, this);
		for(String key : variables.keySet()) {
			ctx.getProperties().put(key, variables.get(key));	//populate the context properties
		}
	}
	
	private void setPrompt() {
		if(ctxmgr.hasMultipleContexts()) {
			prompt = String.format(PROMPT_FORMAT_MULTICONTEXT, ctxid);
		} else {
			prompt = String.format(PROMPT_FORMAT_DEFAULT);
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISession#getCurrentContext()
	 */
	public ICombinedContext getCurrentContext() {
		return currentContext;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISession#getCtxManager()
	 */
	public ISessionContextManager getContextManager() {
		return ctxmgr;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISession#setContext(int)
	 */
	public void setContext(int id) throws CommandException {
		ICombinedContext switchTo = ctxmgr.getContext(id);
		if(switchTo == null) {
			throw new CommandException("The specified context ID of " + id + " is not valid. Execute \"" + CMD_CONTEXT + "\" to see the list of valid IDs.");
		}
		setContext(switchTo);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISession#setContext(com.ibm.jvm.dtfjview.CombinedContext)
	 */
	public void setContext(ICombinedContext switchTo) throws CommandException {
		if(switchTo != null) {
			ctxid = switchTo.getID();
			currentContext = switchTo;
			switchTo.setAsCurrent();
			setPrompt();			//the prompt may reflect the currently selected context
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISession#showContexts(boolean)
	 */
	public void showContexts(boolean shortFormat) {
		out.println("Available contexts (* = currently selected context) : ");
		Map<URI, ArrayList<ICombinedContext>> contexts = ctxmgr.getContexts();
		if(contexts.size() == 0) {
			out.println("\n\tWARNING : no contexts were found");
			return;
		}
		URI currentSource = null;
		for(URI source : contexts.keySet()) {
			if(!source.equals(currentSource)) {
				out.println("\nSource : " + source.getScheme() + "://" + source.getPath() + (source.getFragment() != null ? "#" + source.getFragment() : ""));
				currentSource = source;
			}
			for(ICombinedContext context : contexts.get(source)) {
				if(context == currentContext) {
				 out.print("\t*");	
				} else {
					out.print("\t ");
				}
				context.displayContext(out, shortFormat);
			}
		}
		out.print("\n");
	}
		
	//stops the current processing and exits with and optional code
	private void exit(String msg, int exitCode) {
		if(System.getProperty(SYSPROP_NOSYSTEMEXIT) == null) {
			//terminate the process
			if((msg != null) && (msg.length() > 0)) {
				out.println(msg);
			}
			if((exitCode != 0) && (exitCode != JDMPVIEW_INTERNAL_ERROR)) {
				printHelp();
			}
			System.exit(exitCode);
		} else {
			//raise a runtime exception
			throw new JdmpviewInitException(exitCode, msg);
		}
	}
	
	//checks to see if the specified flag has been set, this will be a single arg command line parameter
	private boolean hasFlagBeenSet(String name) {
		if(singleargs.contains(name)) {
			String value = args.get(name);
			if(value == null) {
				return false;		//not supplied
			} else {
				return Boolean.parseBoolean(value);
			}
		} else {
			//this is the default of not being set
			return false;
		}
	}
		
	//state machine which parses all the command line arguments. It validates the correct use of single and 
	//paired arguments. It will regard the first item without a - as the start of any jdmpview cmds to run
	private void parseCommandLineArgs(String[] cmdargs) {
		if(cmdargs.length == 0) {
			//check at least one argument was supplied
			exit("No parameters were supplied", JDMPVIEW_SYNTAX_ERROR);
		}
		
		// Quotation marks are removed by the shell and that can cause filenames containing spaces to be split into several arguments.
		// This is rather crude, but if there is a quotation mark or an apostrophe in the parameter, quote it with the other (apostrophe or quotation mark)
		// If there isn't any, just surround it with "s.
		// If there are both present this is just stupid, so exit.
		for (int i = 0; i < cmdargs.length; i++) {
			if (cmdargs[i].contains("'")) {
				if (!cmdargs[i].contains("\"")) {
					cmdargs[i] = "\"" + cmdargs[i] + "\"";
				} else {
					exit("Ambiguous parameter " + cmdargs[i] + " - mixed ' and \" encountered", JDMPVIEW_SYNTAX_ERROR);
				}
			} else if (cmdargs[i].contains("\"")) {
				if (!cmdargs[i].contains("'")) {
					cmdargs[i] = "'" + cmdargs[i] + "'";
				} else {
					exit("Ambiguous parameter " + cmdargs[i] + " - mixed ' and \" encountered", JDMPVIEW_SYNTAX_ERROR);
				}
			} else if (cmdargs[i].matches(".*\\s.+")){
				cmdargs[i] = "\"" + cmdargs[i] + "\"";
			}
		}
		
		StringBuilder line = new StringBuilder();
		int index = 0;
		while((index < cmdargs.length) && (parseState != CmdLineParseStateEnum.PARSE_COMPLETE)) {
			String cmdarg = cmdargs[index].toLowerCase();
			switch(parseState) {
				case PARSE_SCAN :							//parser is ready to process the first item
					if(cmdarg.charAt(0) == '-') {
						checkArgIsRecognised(cmdargs[index]);
						if(singleargs.contains(cmdarg)) {
							parseState = CmdLineParseStateEnum.PARSE_SINGLE;
						} else {
							parseState = CmdLineParseStateEnum.PARSE_PAIR;
						}
					} else {
						if(index == 0) {
							//cannot go straight in with commands to run without specifying the core file
							exit("A core file or zip file needs to be specified", JDMPVIEW_SYNTAX_ERROR);
						} else {
							//now parsing commands at the end of the input line
							parseState = CmdLineParseStateEnum.PARSE_CMDS;
						}
					}
					break;
				case PARSE_SINGLE :							//processing a single / flag parameter
					args.put(cmdarg, "true");
					index++;
					parseState = (index == cmdargs.length ? CmdLineParseStateEnum.PARSE_COMPLETE : CmdLineParseStateEnum.PARSE_SCAN);
					break;
				case PARSE_PAIR :							//processing a name value pair
					index++;								//move to the value to set
					//check that there is a value to store
					if((index < cmdargs.length) && (cmdargs[index].length() > 0) && (cmdargs[index].charAt(0) != '-')) {
						args.put(cmdarg, cmdargs[index]);
					} else {
						exit("The parameter " + cmdarg + " requires a value to be set", JDMPVIEW_SYNTAX_ERROR);
					}
					index++;								//move to next input arg
					parseState = (index == cmdargs.length ? CmdLineParseStateEnum.PARSE_COMPLETE : CmdLineParseStateEnum.PARSE_SCAN);
					break;
				case PARSE_CMDS :							//parsing all the commands at the end
					if(cmdarg.charAt(0) == '-') {
						exit("The startup option " + cmdarg + " occurred in an unexpected position", JDMPVIEW_SYNTAX_ERROR);
					}
					line.append(cmdargs[index]);			//preserve case for commands as things like heap name are case sensitive
					index++;
					if(index < cmdargs.length) {
						line.append(" ");
					} else {
						commands.add(line.toString());
						parseState = CmdLineParseStateEnum.PARSE_COMPLETE;
					}
					break;
				default :
					exit("Internal error occurred an unknown state was reached whilst processing the command line args", JDMPVIEW_INTERNAL_ERROR);
					break;
			}
		}
		if(parseState != CmdLineParseStateEnum.PARSE_COMPLETE) {
			//did not complete the command line parsing correctly, so exit with status
			exit("Internal error occurred whilst processing the command line args", JDMPVIEW_INTERNAL_ERROR);
		}
	}
	
	//checks that a specified command line arg is recognised
	private void checkArgIsRecognised(String cmdarg) {
		if(pairedargs.contains(cmdarg) || singleargs.contains(cmdarg)){
			return;
		}
		exit("The command " + cmdarg + " was not recognised", JDMPVIEW_SYNTAX_ERROR);
	}
	
	//if verbose is specified write out how the command line args have been interpreted
	private void logCommandLineArgs() {
		if(!isVerboseEnabled) {
			return;		//only log when verbose is turned on
		}
		out.println("Startup parameters: ");
		for(String key : args.keySet()) {
			out.print("    " + key);
			if(args.get(key) == null) {
				out.println(" : not set");
			} else {
				out.println(" = " + args.get(key));
			}
		}
		out.println("Batch mode commands: " + commands.toString());
	}
	
	//print some help for the user
	private void printHelp() {
		String launcher = System.getProperty(SYSPROP_LAUNCHER, "dtfjview");
		
		out.print(
				"Usage: \"" + launcher + " -core <core_file> [-verbose]\" or \n" +
				"       \"" + launcher + " -zip <zip_file> [-verbose]\" or \n" +
				"       \"" + launcher + " -version\"\n\n" +
				"To analyze dumps from DDR-enabled JVMs, " + launcher + " only requires a core file.\n\n" + 
				"The default ImageFactory is " + factoryName + ". To change the ImageFactory use: \n" +
				"\t -J-D" + SYSPROP_FACTORY + "=<classname> \n\n");	
		
		out.print(launcher + " can also be used in 'batch mode' whereby commands can be issued without entering an interactive prompt.\n" +
				"This processing is controlled via the following command line options :\n"+
				"        -cmdfile <path to command file> : will read and sequentially execute\n" +
				"                                          a series of " + launcher + " commands in the specified file\n" +
				"        -charset <character set name>   : specifies the character set for the commands in -cmdfile.\n" +
				"        -outfile <path to output file>  : file to store any output generated by commands\n" +
				"        -overwrite                      : specifies that the file specified by -outfile should be overwritten\n" +
				"        -append                         : specifies that the file specified by -outfile should be appended.\n\n" +
				"It is possible to execute a single command without specifying a command file by appending the command to the end\n " +
				"of the command line which executes " + launcher + "\n" +
				"e.g. " + launcher + " -core mycore.dmp info class\n\n" +
				"Finally, some commands which take the * parameter will need to have that specified on the command line as ALL\n" +
				"This is to ensure correct processing by the OS " + 
				"e.g. " + launcher + " -core mycore.dmp info thread ALL\n"
			);
	}
	
	//if the -outfile param has been specified do some checking on the supplied file
	private void processOutfile() {
		if(!args.containsKey(ARG_OUTFILE)) {
			return;		//option was not specified at startup
		}
		String name = stripQuotesFromFilename(args.get(ARG_OUTFILE));
		File file = new File(name);
		boolean append = hasFlagBeenSet(ARG_APPEND);
		boolean overwrite = hasFlagBeenSet(ARG_OVERWRITE);
		if(append && overwrite) {
			exit("Option " + ARG_APPEND + " and option " + ARG_OVERWRITE + " can not be used at the same time.", JDMPVIEW_APPEND_OVERWRITE_COEXIST);
		}
		if(file.exists()) {
			if(isVerboseEnabled) {
				out.println("Specified output file " + args.get(ARG_OUTFILE) + " already exists, checking " + ARG_OVERWRITE + " flag and " + ARG_APPEND + " flag");
			}
			if(!overwrite && !append) {
				exit("The output file " + file.getAbsolutePath() + " exists and neither " + ARG_OVERWRITE + " option nor " + ARG_APPEND + " option was specified", JDMPVIEW_OUTFILE_EXISTS);
			}
			if(!file.isFile()) {
				exit("The output file " + file.getAbsolutePath() + " exists but is not a file", JDMPVIEW_FILE_ERROR);
			}
			if(overwrite) {
				boolean hasBeenDeleted = file.delete();
				if(!hasBeenDeleted) {
					exit("The output file " + file.getAbsolutePath() + " exists, and is a file, but could not be deleted", JDMPVIEW_FILE_ERROR);
				}
			}
		}
		try {
			FileWriter writer = null;
			
			if(file.exists() && append) {
				writer = new FileWriter(file, true);
			} else {
				writer = new FileWriter(file);
			}
			
			foc = new FileOutputChannel(writer, file);
			out.addChannel(foc);
			ToolsRegistryOutputChannels.addChannel(foc);
		} catch (IOException e) {
			logException("Error creating output file", e);
			exit("Unexpected error creating the output file " + file.getAbsolutePath(), JDMPVIEW_INTERNAL_ERROR);
		}
	}
	
	private String stripQuotesFromFilename(String name) {
		if((name.length() > 0) && (name.charAt(0) == '"')) {
			//strip out any initial quote
			name = name.substring(1);
		}
		if((name.length() > 0) && (name.charAt(name.length() - 1) == '"')) {
			//strip out trailing quote
			name = name.substring(0, name.length() - 1);
		}
		return name;
	}
	
	//if the -cmdfile option has been specified then process it and place them on the internal queue
	private void processCommandFile() {
		if (!args.containsKey(ARG_CMDFILE)) {
			return;		//nothing to load as the command file was not specified
		}
		List<String> cmds = parseCommandsFromFile();
		if (cmds != null) {
			commands.addAll(cmds);
		}
	}
	
	//read any commands from the specified file, there is no validation of the commands at this point
	private List<String> parseCommandsFromFile() {
		String name = stripQuotesFromFilename(args.get(ARG_CMDFILE));
		File file = new File(name);
		if (!file.exists() || !file.isFile()) {
			exit("The specified command file " + file.getAbsolutePath() + " does not exist or is not a file", JDMPVIEW_FILE_ERROR);
		}
		if (file.length() > Integer.MAX_VALUE) {
			exit("The specified command file " + file.getAbsolutePath() + " is too large to be read", JDMPVIEW_FILE_ERROR);
		}
		String charset = null;
		if (args.containsKey(ARG_CHARSET)) {
			charset = args.get(ARG_CHARSET);
		}
		try {
			return CmdFileTool.parseCmdFile(file, charset);
		} catch (UnsupportedEncodingException e) {
			exit("The supplied charset " + charset + " for reading the command file is not supported", JDMPVIEW_SYNTAX_ERROR);
			return null;			//this line will never execute
		} catch (IOException e) {
			logException("Error reading from command file " + file.getAbsolutePath(), e);
			exit("Error reading from command file " + file.getAbsolutePath(), JDMPVIEW_FILE_ERROR);
			return null;
		}
	}
	
	/**
	 * Creates a DTFJ image from the supplied core or zip file
	 */
	private void imageFromCommandLine() {
		// if -version, output the version and exit
		if (hasFlagBeenSet(ARG_VERSION)) {
			ctxroot.execute("open", out.getPrintStream());
			exit(null, 0);
		}		

	    String coreFilePath = null;
		if (hasFlagBeenSet(ARG_VERBOSE)) {
			// If we are using DDR this will enable warnings.
			// (If not it's irrelevant.)
			Logger.getLogger("j9ddr.view.dtfj").setLevel(Level.WARNING);
		}
		//at this point the existence of either -core or -zip has been checked
		long current = System.currentTimeMillis();
		if(args.containsKey(ARG_CORE)) {
			if(args.containsKey(ARG_ZIP)) {
				exit(ARG_ZIP + " and " + ARG_CORE + " cannot be specified at the same time", JDMPVIEW_SYNTAX_ERROR);
			} else {
				coreFilePath = args.get(ARG_CORE);
				ctxroot.execute("open " + coreFilePath , out.getPrintStream());
			}
		} else {
			coreFilePath = args.get(ARG_ZIP);	//if -core has been set it is handled in the first part of this if statement
			ctxroot.execute("open " + coreFilePath , out.getPrintStream());
		}
		logger.fine(String.format("Time taken to load image %d ms", System.currentTimeMillis() - current));
		//Store the core file/zip file path in the variables. We need it for creating default filenames for heapdumps
		variables.put(CORE_FILE_PATH_PROPERTY, coreFilePath);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISession#findAndSetContextWithJVM()
	 */
	public void findAndSetContextWithJVM() {
		try {
			//try and set the starting context as the first one with a JVM in it
			Collection<ArrayList<ICombinedContext>> sources = ctxmgr.getContexts().values();
			if(sources.size() == 0) {
				//there are no sources or contexts available so set to the root context
				currentContext = ctxroot;			//failed to find a context with a runtime in so set to the root
				return;
			}
			ICombinedContext defaultContext = null;	//this is what the context will be set to if none of the remaining ones contain a jvm
			for(ArrayList<ICombinedContext> contexts : sources) {
				for(ICombinedContext context : contexts) {
					if(defaultContext == null) {
						defaultContext = context;
					}
					if(context.getRuntime() != null) {
						setContext(context);
						return;
					}	
				}
			}
			setContext(defaultContext);
		} catch (CommandException e) {
			out.print("ERROR : Unable to set current context");
		}
	}
	
	//start point for running the session
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISession#run()
	 */
	public void run() {
		//first check that we have a context to run against in case there was a fatal error during startup
		if(currentContext == null) {
			out.println("No current context - exiting");
			return;
		}
		try {
			if(isInteractiveSession) {
				runInteractive();
			} else {
				runBatch();
			}
		} catch (Exception e) {
			//something has gone wrong outside of one of the commands and so is likely to be a fatal system error
			out.println("Unexpected system error, terminating application. Run with -verbose for more information");
			//this exception is logged at fine because we always want it visible with -verbose
			logger.log(Level.FINE, "Failed to start the session", e);
		}
		//remove all contexts - this will close all open images
		try {
			ctxmgr.removeAllContexts();
		} catch (Exception e) {
			logger.log(Level.FINE, "Error closing contexts: ", e);
			out.println("Error closing contexts: " + e.getMessage());
		}
	}
	
	//run in batch and exit after the last command is executed
	private void runBatch() {
		findAndSetContextWithJVM();
		showContexts(true);
		String line = null;
		PrintStream printStream = ToolsRegistryOutputChannels.newPrintStream();
		while((line = commands.poll()) != null) {
			printStream.println(prompt + line);
			try {
				ToolsRegistry.recordAndExecute(line);
			} catch (com.ibm.jvm.dtfjview.tools.CommandException e) {
				out.println(e.getMessage());
			}
		}	
		out.close();
	}
	
	//run in interactive mode with a prompt etc.
	private void runInteractive(){
		BufferedReader stdin = new BufferedReader(new InputStreamReader(System.in));
		String input = "";
		String quit = "";
		
		out.println("For a list of commands, type \"help\"; " +
				"for how to use \"help\", type \"help help\"");

		while (quit == null || !quit.equals("on"))
		{
			if(ctxmgr.hasChanged()) {
				//a change means that a context has been added or closed
				if(currentContext instanceof CombinedContext) {
					int id = ((ICombinedContext)currentContext).getID();
					if(ctxmgr.getContext(id) == null) {
						//the context that was previously the current one has been closed
						findAndSetContextWithJVM();
					}
				} else {
					findAndSetContextWithJVM();
				}
				showContexts(true);
				setPrompt();
			}
			out.printPrompt(prompt);
			
			try {
				input = stdin.readLine();
			} catch (IOException e) {
				out.print("IOException encountered while reading input; exiting program...");
				break;
			}
			
			if (null == input)
			{
				out.print("End of input stream has been reached; exiting program...");
				break;
			}
			try {
				ToolsRegistry.recordAndExecute(input);
			} catch (com.ibm.jvm.dtfjview.tools.CommandException e) {
				out.println(e.getMessage());
			}
			quit = (String)currentContext.getProperties().get("quit");
		}
		
		// "out" MUST be closed here, otherwise the file deletions in the below
		//  code block will not work
		out.close();
		
	}
	
	/**
	 * Execute a command line.
	 * <p>
	 * @param line	The command line to be executed.
	 * @param redirector	The PrintStream.
	 */
	public void execute(String line, PrintStream redirector) {
		out.removeChannel(defaultOutputChannel);
		if (foc != null) {
			out.removeChannel(foc);
		}
		IOutputChannel temp = new OutputChannelRedirector(redirector);
		out.addChannel(temp);
		execute(line);
		out.removeChannel(temp);
		if (foc != null) {
			out.addChannel(foc);
		}
		out.addChannel(defaultOutputChannel);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ISession#execute(java.lang.String)
	 */
	public void execute(String line) {
		String trimmedInput = line.trim();
		
		if (!trimmedInput.equals(""))
		{
			try {
				CommandParser parser = new CommandParser(line);
				String cmd = parser.getCommand();

				// There is special processing in what follows for "set logging" and "show logging"
				// We cannot just pass the logic through to a secondary handler like we can with "info"
				// because when we see "set" or "show" we need to distinguish here and now which context to pass
				// - "set logging" requires the session context 
				// - "set heapdump" requires the per-image context.
				// For both "set logging" and "show logging" remove the word "logging" from the arguments and make it 
				// part of the command.
				
				if(cmd.equals(CMD_CONTEXT)) {
					switchContext(trimmedInput.toLowerCase());
				} else if (cmd.equals("set") && parser.getArguments().length > 0 && parser.getArguments()[0].equals("logging")) {
					String[] oldargs = parser.getArguments();
					String[] newargs = new String[oldargs.length-1];
					System.arraycopy(oldargs, 1, newargs, 0, newargs.length); // remove the first argument "logging"
					ctxroot.execute("set logging", newargs, out.getPrintStream());
				} else if (cmd.equals("show") && parser.getArguments().length > 0 && parser.getArguments()[0].equals("logging")) {
					String[] oldargs = parser.getArguments();
					String[] newargs = new String[oldargs.length-1]; // remove the first argument "logging"
					System.arraycopy(oldargs, 1, newargs, 0, newargs.length);
					ctxroot.execute("show logging", newargs, out.getPrintStream());
				} else if(sessioncommands.contains(cmd)) { 
					// if command is in the remaining list of session commands
					// at the time of writing this is quit, help, open, close, pwd, cd 
					ctxroot.execute(parser, out.getPrintStream()); 
				} else { 
					// not a session command, execute against the per-image context
					currentContext.execute(parser, out.getPrintStream());
				}
			} catch (Exception e) {
				//ensure that errors are logged to the correct output stream
				logger.log(Level.FINEST, "An error occurred while processing the command: " + line, e);
				out.print("An error occurred while processing the command: " + line);
			}
		}
	}
	
	//switch contexts either by sequential ID or by ASID for z/OS cores
	private void switchContext(String input) {
		String[] args = input.trim().split(" ");
		try {
			switch(args.length) {
				case 1 :
					//no ID was specified so show the list of contexts
					showContexts(false);
					return;
				case 2 :
					//ID on it's own
					int radix = args[1].startsWith("0x") ? 16 : 10;
					int id = 0;
					if(radix == 16) {
						id = Integer.parseInt(args[1].substring(2), radix);
					} else {
						id = Integer.parseInt(args[1], radix);	
					}
					if((radix == 16) && isZOS) {		//if context is in hex and on z/OS then see if this matches an ASID
						if(switchContextByASID(args[1])) {
							return;
						} else {
							out.println("The ASID " + args[1] + " was not recognised, context is unchanged");
						}
					} else {
						setContext(id);				//assume that the context is the sequential context number
					}
					break;
				case 3 :
					if(args[1].equalsIgnoreCase("asid")) {
						if(isZOS) {
							if(!switchContextByASID(args[2])) {
								out.println("The specified context ASID : " + args[2] + " is not a valid ASID");
							}	
						} else {
							out.println("Switching context by ASID is not supported for this core file");
						}
					} else {
						out.println("Invalid command parameters, valid parameters are [ID|asid ID]");
					}
					break;
				default :
					out.println("Invalid command parameters, valid parameters are [ID|asid ID]");
					return;
			}
		} catch (NumberFormatException e) {
			out.println("The specified context ID : " + input + " is not a valid ID");
		} catch (Exception e) {
			out.println(e.getMessage());
		}
	} 
	
	private boolean switchContextByASID(String asid) throws Exception {
		int radix = asid.startsWith("0x") ? 16 : 10;
		if((radix == 16) && isZOS) {
			//if context is in hex and on z/OS then see if this matches an ASID
			for(ArrayList<ICombinedContext> contexts : ctxmgr.getContexts().values()) {
				for(ICombinedContext context : contexts) {
					if(context.getAddressSpace().getID().equalsIgnoreCase(asid)) {
						setContext(context);
						return true;
					}
				}
			}
		}
		return false;
	}
	
	/**
	 * Enables console logger for the supplied logger. Typically used to surface
	 * log commands generated by internal components
	 * 
	 * @param log the logger to enable
	 */
	private void enableConsoleLogging(Logger log) {
		ConsoleHandler handler = new ConsoleHandler();
		handler.setLevel(Level.FINE);
		Formatter formatter = new SimpleFormatter();
		handler.setFormatter(formatter);
		log.addHandler(handler);
		log.setLevel(Level.FINE);
		if(isVerboseEnabled) {
			out.println("Console logging is now enabled for " + log.getName());
		}
	}
	
	/**
	 * Logs an exception to the DTFJView logger or creates an anonymous logger if this
	 * one is not available.
	 * @param message message to log
	 * @param e exception to log
	 */
	private void logException(String message, Exception e) {
		Object obj = variables.get(LOGGER_PROPERTY);
		Logger log = null;
		if((null == obj) || !(obj instanceof Logger)) {
			//no logger to use, so create an anonymous one
			log = Logger.getAnonymousLogger();
			log.fine("Logger not configured or wrong property type, creating an anonymous one");
		} else {
			log = (Logger)obj;
		}
		log.log(Level.FINE, message, e);
	}
}
