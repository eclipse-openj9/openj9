/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive;

import static java.util.logging.Level.FINE;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Method;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.SortedSet;
import java.util.TreeSet;
import java.util.Vector;
import java.util.logging.Logger;

import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.VMDataFactory;
import com.ibm.j9ddr.command.CommandParser;
import com.ibm.j9ddr.command.CommandReader;
import com.ibm.j9ddr.command.ConsoleCommandReader;
import com.ibm.j9ddr.command.JNICommandReader;
import com.ibm.j9ddr.corereaders.CoreReader;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.debugger.JniOutputStream;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.exceptions.JVMNotDDREnabledException;
import com.ibm.j9ddr.exceptions.JVMNotFoundException;
import com.ibm.j9ddr.exceptions.MissingDDRStructuresException;
import com.ibm.j9ddr.logging.LoggerNames;
import com.ibm.j9ddr.tools.ddrinteractive.commands.ForeachCommand;
import com.ibm.j9ddr.tools.ddrinteractive.commands.LookupSymbolCommand;
import com.ibm.j9ddr.tools.ddrinteractive.commands.NativeLibrariesCommand;
import com.ibm.j9ddr.tools.ddrinteractive.commands.NativeStacksCommand;
import com.ibm.j9ddr.tools.ddrinteractive.commands.TimeCommand;
import com.ibm.j9ddr.tools.ddrinteractive.plugins.PluginCommand;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImage;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageAddressSpace;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageProcess;

public class DDRInteractive implements Runnable
{
	private final PrintStream out;
	private final CommandReader commandReader;
	private static String path;
	private final List<Context> contexts = new LinkedList<Context>();
	private Context currentContext;
	private ICore currentCore;
	
	/*
	 * Commands that contain no VM-specific code (commands that don't need
	 * bootstrapping). Commands that need VM code should be listed in
	 * com.ibm.j9ddr.vmXX.tools.ddrinteractive.GetCommandsTask
	 */
	private final List<ICommand> nonVMCommands;
	{
		List<ICommand> localCommandList = new LinkedList<ICommand>();

		localCommandList.add(new J9XCommand());
		localCommandList.add(new ContextCommand());
		localCommandList.add(new MemoryRangesCommand());
		localCommandList.add(new FindInMemoryCommand());
		localCommandList.add(new J9HelpCommand());
		localCommandList.add(new PluginCommand());
		localCommandList.add(new NativeLibrariesCommand());
		localCommandList.add(new LookupSymbolCommand());
		localCommandList.add(new NativeStacksCommand());
		localCommandList.add(new ExtractMemoryCommand());
		localCommandList.add(new TimeCommand());
		localCommandList.add(new ForeachCommand());

		nonVMCommands = Collections.unmodifiableList(localCommandList);
	}

	/**
	 * Construct DDR from J9DDRImage, ICore or IProcess
	 * @param obj Core file. 
	 * @param out result output stream.
	 * @throws Exception
	 */
	public DDRInteractive(Object obj, PrintStream out) throws Exception
	{
		this.out = out;
		this.commandReader = new ConsoleCommandReader(out);
		DDRInteractive.path = null;
		
		if (obj instanceof J9DDRImage) {
			ICore icore = ((J9DDRImage) obj).getCore();
			locateRuntimes(icore);
			return;
		}
		if(obj instanceof IProcess) {
			addContextFromProcess((IProcess) obj);
			return;
		}
		if ((obj instanceof ICore)) {
			locateRuntimes((ICore) obj);
			return;
		}
		throw new IllegalArgumentException("The object parameter was not an instance of ICore, IProcess or J9DDRImage, it was " + obj.getClass().getName());
	}

	public DDRInteractive(IProcess proc, IVMData vmData, PrintStream out) throws Exception {
		this.out = out;
		this.commandReader = new ConsoleCommandReader(out);
		DDRInteractive.path = null;
		if (vmData != null) {
			contexts.add(new Context(proc, vmData, nonVMCommands));
		} else {
			addMissingJVMToContexts(proc);
		}
		currentContext = contexts.get(0);
	}
	
	/**
	 * Construct DDR from DTFJ contexts, this prevents both the DTFJ view such as jdmpview
	 * from scanning a core file, followed by a second scan by DDR interactive. It also
	 * ensures that a tool can know which contexts point to the same VM. Uses reflection
	 * to avoid a binding to the DTFJ jars which allows DDR to be invoked independently 
	 * 
	 * @param obj  
	 * @param out result output stream.
	 * @throws Exception
	 */
	public DDRInteractive(List<Object> dtfjcontexts, PrintStream out) throws Exception
	{
		this.out = out;
		this.commandReader = new ConsoleCommandReader(out);
		DDRInteractive.path = null;
		
		for(Object dtfjctx : dtfjcontexts) {
			if(hasInterface(dtfjctx.getClass(), "com.ibm.java.diagnostics.utils.IDTFJContext")) {
				addDDRContextFromDTFJ(dtfjctx);
			}
		}
		if(contexts.size() > 0) {
			currentContext = contexts.get(0);
		}
	}
	
	//see if a class implements a named interface
	private boolean hasInterface(Class<?> clazz, String ifaceName) {
		Class<?>[] ifaces = clazz.getInterfaces();
		for(Class<?> iface : ifaces) {
			if(iface.getName().equals(ifaceName)) {
				return true;
			}
		}
		Class<?> superclass = clazz.getSuperclass();
		if(superclass == null) {
			return false;		//hit the top of the class hierarchy
		} else {
			return hasInterface(superclass, ifaceName);		//check the superclass
		}
	}
	
	//finds the named method in the class or it's hierarchy
	private Method findMethod(Class<?> clazz, String name, Class<?>[] parameterTypes) throws NoSuchMethodException {
		try {
			return clazz.getDeclaredMethod(name, parameterTypes);
		} catch (NoSuchMethodException e) {
			if(clazz.getSuperclass() == null) {
				throw e;
			} else {
				return findMethod(clazz.getSuperclass(), name, parameterTypes);
			}
		}
		
	}
	
	private void addDDRContextFromDTFJ(Object dtfjctx) throws Exception {
		Method getAddressSpace = findMethod(dtfjctx.getClass(), "getAddressSpace", (Class<?>[])null);
		Method getProcess = findMethod(dtfjctx.getClass(), "getProcess", (Class<?>[])null);
		Method getRuntime = findMethod(dtfjctx.getClass(), "getRuntime", (Class<?>[])null);
		
		Object addressSpace = getAddressSpace.invoke(dtfjctx, (Object[])null);
		Object process = getProcess.invoke(dtfjctx, (Object[])null);
		Object runtime = getRuntime.invoke(dtfjctx, (Object[])null);
		
		if(addressSpace == null) {
			throw new IOException("Cannot create a context without an associated address space");
		}
		if(!(addressSpace instanceof J9DDRImageAddressSpace)) {
			throw new UnsupportedOperationException("The supplied DTFJ context is not backed by a DDR implementation");
		}
		IAddressSpace space = ((J9DDRImageAddressSpace) addressSpace).getIAddressSpace();
		IProcess proc = null;
		if(process != null) {
			proc = ((J9DDRImageProcess) process).getIProcess();
		}
		if(proc == null) {
			ASNoProcess as = new ASNoProcess(space);
			addMissingJVMToContexts(as);
			return;
		}
		if(runtime == null) {
			addMissingJVMToContexts(proc);
			return;
		}
		//DDR will cache the VM data, so if the DTFJ context has already found it then this should be in the cache
		IVMData vmData = VMDataFactory.getVMData(proc);

		if (vmData != null) {
			contexts.add(new Context(proc, vmData, nonVMCommands));
		} else {
			addMissingJVMToContexts(proc);
		}
	}
	
	/**
	 * Adds a DDR context from an existing IProcess
	 * 
	 * @param proc
	 * @throws IOException 
	 */
	private void addContextFromProcess(IProcess proc) throws IOException {
		//DDR will cache the VM data, so if the DTFJ context has already found it then this should be in the cache
		IVMData vmData = VMDataFactory.getVMData(proc);

		if (vmData != null) {
			contexts.add(new Context(proc, vmData, nonVMCommands));
		} else {
			addMissingJVMToContexts(proc);
		}
	}

	
	/**
	 * Start DDR. Read command from standard input.
	 * @param path path to core file.
	 * @param out result output stream.
	 * @throws Exception
	 */
	public DDRInteractive(String path, PrintStream out) throws Exception
	{
		this(path, new ConsoleCommandReader(out), out);
	}

	/**
	 * Start DDR
	 * @param path core file location on disk.
	 * @param commandReader user input.
	 * @param out result output stream.
	 * @throws Exception
	 */
	public DDRInteractive(String path, CommandReader commandReader,	PrintStream out) throws Exception
	{
		this.commandReader = commandReader;
		this.out = out;
		DDRInteractive.path = path;
		locateRuntimes(path);
	}

	/**
	 * @param args
	 */
	public static void main(String[] args) throws Exception
	{
		if (args.length > 0) {
			DDRInteractive app = new DDRInteractive(args[0], new PrintStream(System.out));
			app.startDDR();
		}
	}

	/*
	 * Entry point used by GDB/DBX/Windbg to start a DDR session within native debuggers
	 */
	public static DDRInteractive instantiateDDR()
	{
		DDRInteractive app = null;
		OutputStream outputStream = new JniOutputStream();
		PrintStream printStream = new PrintStream(outputStream);
		CommandReader commandReader = new JNICommandReader(printStream);

		try {
			app = new DDRInteractive(null, commandReader, printStream);
		} catch (Exception e) {
			e.printStackTrace();
			// put the stack trace to the log
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_INTERACTIVE_CONTEXT);
			StringWriter sw = new StringWriter();
			PrintWriter pw = new PrintWriter(sw);
			e.printStackTrace(pw);
			logger.logp(FINE, null, null, sw.toString());
		}

		return app;
	}

	/*
	 * Used by native debuggers to retrieve a list of known commands for command aliasing purposes 
	 */
	public Object[] getCommandNames()
	{
		Vector<String> commandNames = currentContext.getCommandNames();
		
		return commandNames.toArray();
	}
	
	/**
	 * Get the current context that is being operated on. This is used by external applications such as 
	 * Eclipse which are providing their own command parsers
	 * @return the current context
	 */
	public Context getCurrentContext()
	{
		return currentContext;
	}

	public void startDDR() throws Exception
	{
		showContexts();

		out.print("Run !j9help to see a list of commands\n");
		commandReader.processInput(this);
	}

	/**
	 * Used by external clients to set the input stream.
	 * @param in the new stream to read commands from
	 */
	public void setInputStream(InputStream in)
	{
		commandReader.setInputStream(in);
	}

	/**
	 * Have the underlying command reader process a given line
	 * @param line command line to process
	 * @throws Exception re-throw any exceptions
	 */
	public void processLine(String line) throws Exception
	{
		commandReader.processLine(this, line);
	}

	public void showContexts()
	{
		try {
			commandReader.processLine(this, "!context");
		} catch (Exception e) {
			throw new Error(e);
		}
	}

	/**
	 * Execute DDR inside a thread that was started by a debugger (!startddr).
	 */
	public void run()
	{
		try {
			startDDR();
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
	
	public void execute(CommandParser command) {
		currentContext.execute(command, out);
	}

	public void execute(String command, String[] arguments)
	{
		currentContext.execute(command, arguments, out);
	}

	private void locateRuntimes(String path) throws Exception
	{
		ICore reader = CoreReader.readCoreFile(path);

		if (null == reader) {
			throw new RuntimeException("Cannot find Core Reader for file: " + path);
		}
		locateRuntimes(reader);
	}

	private void locateRuntimes(ICore reader)
	{
		currentCore = reader;
		Collection<? extends IAddressSpace> spaces = reader.getAddressSpaces();
		
		for (IAddressSpace thisSpace : spaces) {
			boolean hasCtxBeenAddedForAS = false;			//indicates if a context has been allocated for the address space
			
			Collection<? extends IProcess> processes = thisSpace.getProcesses();

			for (IProcess thisProcess : processes) {
				hasCtxBeenAddedForAS = true;
				try {
					IVMData vmData = VMDataFactory.getVMData(thisProcess);

					if (vmData != null) {
						contexts.add(new Context(thisProcess, vmData, nonVMCommands));
					} else {
						addMissingJVMToContexts(thisProcess);
					}
				} catch (JVMNotDDREnabledException e) {
					addMissingJVMToContexts(thisProcess);
				} catch (JVMNotFoundException e) {
					addMissingJVMToContexts(thisProcess);
				} catch (MissingDDRStructuresException e) {
					addMissingJVMToContexts(thisProcess);
				} catch (IOException e) {
					System.err.println("Problem searching for VMData in process " + thisProcess);
					e.printStackTrace();
					// put the stack trace to the log
					Logger logger = Logger.getLogger(LoggerNames.LOGGER_INTERACTIVE_CONTEXT);
					StringWriter sw = new StringWriter();
					PrintWriter pw = new PrintWriter(sw);
					e.printStackTrace(pw);
					logger.logp(FINE, null, null, sw.toString());
				}
			}
			if(!hasCtxBeenAddedForAS) {
				ASNoProcess as = new ASNoProcess(thisSpace);
				addMissingJVMToContexts(as);
			}
		}

		if (contexts.size() == 0) {
			throw new RuntimeException("Couldn't find any address spaces in this dump");
		} else {
			currentContext = contexts.get(0);		//default to the first address space
			for(Context ctx : contexts) {			//but if there is one with a JVM in it, set it to that
				if(!(ctx.vmData instanceof MissingVMData)) {
					currentContext = ctx;
					break;
				}
			}
		}
	}

	private void addMissingJVMToContexts(IProcess thisProcess) {
		IVMData novm = new MissingVMData();
		Context ctx = new Context(thisProcess, novm, nonVMCommands);
		contexts.add(ctx);
	}
	
	@SuppressWarnings("rawtypes")
	public Collection getStructuresForCurrentContext()
	{
		return currentContext.vmData.getStructures();
	}

	/* TODO: move to own file */
	public class J9HelpCommand extends Command
	{
		public J9HelpCommand()
		{
			addCommand("j9help", "", "Prints the help table" );
			addCommand("exit", "", "Exits from DDR Interactive");
			addCommand("quit", "", "Quits DDR Interactive: a synonym for \"exit\"");
		}

		public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
		{
			SortedSet<String> helpTable = new TreeSet<String>();

			for (ICommand thisCommand : nonVMCommands) {
				for (String thisEntry : thisCommand.getCommandDescriptions()) {
					if(!thisEntry.endsWith("\n")) {		//ensure that cmds are separate lines
						thisEntry += "\n";
					}
					helpTable.add(thisEntry);
				}
			}

			for (ICommand thisCommand : context.commands) {
				for (String thisEntry : thisCommand.getCommandDescriptions()) {
					if(!thisEntry.endsWith("\n")) {
						thisEntry += "\n";
					}
					helpTable.add(thisEntry);
				}
			}

			for (String entry : helpTable) {
				out.print(entry);
			}
		}
	}

	/* TODO: move to own file */
	public class ContextCommand extends Command
	{
		public ContextCommand()
		{
			addCommand("context", "[ctx#]", "Lists or selects context (VM) to work with");
		}

		public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
		{
			if (args.length == 0) {
				printContextList(false);
			} else if (args.length == 1) {
				if(args[0].equalsIgnoreCase("short")) {
					printContextList(true);
				} else {
					switchContext(args);
				}
			} else {
				throw new DDRInteractiveCommandException("Unexpected arguments. Usage: !context to list contexts, !context <id> to select a context.");
			}
		}

		private void switchContext(String[] args) throws DDRInteractiveCommandException
		{
			try {
				int index = Integer.parseInt(args[0]);

				if (index < 0 || index >= contexts.size()) {
					throw new DDRInteractiveCommandException("Context ID out of range. Range should be between 0 and " + (contexts.size() - 1) + " inclusive");
				}

				currentContext = contexts.get(index);

				out.println("Switched to " + currentContext);
			} catch (NumberFormatException ex) {
				throw new DDRInteractiveCommandException("Couldn't parse context ID", ex);
			}

		}

		private void printContextList(boolean shortFormat)
		{
			int count = 0;

			for (Context thisContext : contexts) {
				if (thisContext == currentContext) {
					out.print("*");
				} else {
					out.print(" ");
				}

				out.print(count);
				out.print(" : ");
				out.println(thisContext.toString(shortFormat));
				count++;
			}
		}

	}

	/**
	 * This function returns the path DDR interactive was invoked against.
	 * This allows commands to obtain the file name. ( A single file could
	 * contain multiple address spaces/processes so this cannot be usefully
	 * stored on IProcess or IContext. )
	 * 
	 * External tools may create more than one instance of DDRInteractive at the same time. This is a static
	 * property so allow it to be changed by external tools.
	 * 
	 * The path can set at startup or via the com.ibm.j9ddr.tools.ddrinteractive.filepath system property
	 * 
	 * @return the path to the file DDRInteractive was launched against or null if a path wasn't specified.
	 */
	public static String getPath() {
		return System.getProperty("com.ibm.j9ddr.tools.ddrinteractive.filepath", path);
	}

	/**
	 * Close the core file and libraries opened by the core reader, and release any resources
	 */
	public void close() throws Exception
	{
		if (currentCore != null) {
			currentCore.close();
		}
		VMDataFactory.clearCache();
	}
}
