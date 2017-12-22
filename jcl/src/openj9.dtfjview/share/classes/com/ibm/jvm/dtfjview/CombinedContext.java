/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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

import static com.ibm.jvm.dtfjview.SessionProperties.VERBOSE_MODE_PROPERTY;

import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URI;
import java.net.URISyntaxException;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.java.diagnostics.utils.DTFJContext;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.commands.CommandParser;
import com.ibm.java.diagnostics.utils.commands.ICommand;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.CdCommand;
import com.ibm.jvm.dtfjview.commands.CloseCommand;
import com.ibm.jvm.dtfjview.commands.OpenCommand;
import com.ibm.jvm.dtfjview.commands.PwdCommand;
import com.ibm.jvm.dtfjview.commands.setcommands.SetLoggingCommand;
import com.ibm.jvm.dtfjview.commands.showcommands.ShowLoggingCommand;
import com.ibm.jvm.dtfjview.spi.ICombinedContext;
import com.ibm.jvm.dtfjview.spi.IOutputManager;

/**
 * Wrapper class that takes an existing DTFJ context and augments
 * it with a DDR interactive context if one exists.
 * 
 * @author adam
 *
 */
public class CombinedContext extends DTFJContext implements ICombinedContext {
	private Logger logger = Logger.getLogger(SessionProperties.LOGGER_PROPERTY);
	
	//declarations required to support DDR interactive accessed through this DTFJ context
	private static final String DDR_INTERACTIVE_CLASS = "com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive";
	private Throwable ddrStartupException = null;	//the exception which was thrown when the DDR interactive session was started
	private Object ddriObject = null;
	private Method ddriMethod = null;
	private boolean ddrAvailable = false;
	private int id = 0;				//context id to be referenced by the 'context' command
	
	{
		//global commands exist irrespective of the context type
		globalCommands.add(new OpenCommand());
		globalCommands.add(new CloseCommand());
		globalCommands.add(new SetLoggingCommand());
		globalCommands.add(new ShowLoggingCommand());
		globalCommands.add(new PwdCommand());
		globalCommands.add(new CdCommand());
	}
	
	public CombinedContext(int major, int minor, Image image, ImageAddressSpace space, ImageProcess proc, JavaRuntime rt, int id) {
		super(major, minor, image, space, proc, rt);
		this.id = id;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ICombinedContext#getId()
	 */
	public int getID() {
		return id;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ICombinedContext#setId(int)
	 */
	public void setID(int id) {
		this.id = id;
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ICombinedContext#execute(java.lang.String, java.lang.String[], java.io.PrintStream)
	 */
	public void execute(String cmdline, String[] arguments, PrintStream out) {
		try {
			CommandParser parser = new CommandParser(cmdline, arguments);
			execute(parser, out);
		} catch (ParseException e) {
			out.println("Error parsing command: " + cmdline + " (" + e.getMessage() + ")");
			logger.log(Level.FINE, "Error parsing command " + cmdline, e);
		}
	}

	/*
	 * Commands / plugins that inherit from the BaseJdmpviewCommand class need to have
	 * their initialisation methods fired, which validates the context type and sets
	 * up various instance level fields. Commands that directly implement the DTFJ
	 * plugin interfaces will be ignored.
	 */
	private boolean initJdmpviewCommand(CommandParser parser, PrintStream out) throws CommandException {	
		for (ICommand command : commands) {
			if(command.recognises(parser.getCommand(), this)) {
				try {
					/*
					 * This work-around is needed because the BaseJdmpviewCommand is loaded in both the
					 * CombinedContext classloader and in the classloader which is running the command. This
					 * means that a simple instanceof check will not work
					 * i.e. (command instanceof BaseJdmpviewCommand) fails as it's comparing the base command from
					 * two different classloaders. Solution is to use the same classloader as the command
					 * to get the BaseJdmpviewCommand class and then use reflection to invoke it.
					 */
					Class<?> base = command.getClass().getClassLoader().loadClass(BaseJdmpviewCommand.class.getName());
					if(base.isAssignableFrom(command.getClass())) {
						Method method = base.getMethod("initCommand", new Class<?>[]{String.class, String[].class, IContext.class, PrintStream.class});
						Object result = method.invoke(command, parser.getCommand(), parser.getArguments(), this, out);
						return (Boolean) result;
					}
				} catch (Exception e) {
					out.println("Error initialising command : " + parser.getOriginalLine() + " (" + e.getMessage() + ")");
					logger.log(Level.FINE, "Error initialising command : " + parser.getOriginalLine(), e);
				}
			}
		}
		return false;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ICombinedContext#execute(java.lang.String, java.io.PrintStream)
	 */
	public void execute(String cmdline, PrintStream out) {
		try {
			CommandParser parser = new CommandParser(cmdline);
			execute(parser, out);
		} catch (ParseException e) {
			out.println("Error parsing command: " + cmdline + " (" + e.getMessage() + ")");
			logger.log(Level.FINE, "Error parsing command " + cmdline, e);
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ICombinedContext#execute(com.ibm.java.diagnostics.utils.commands.CommandParser, java.io.PrintStream)
	 */
	public void execute(CommandParser command, PrintStream out) {
		if(isDDRCommand(command.getOriginalLine())) {
			executeDDRInteractiveCommand(command.getOriginalLine(), out);
		} else {
			try {
				if(!initJdmpviewCommand(command, out)) {
					super.execute(command, out);
				}
			} catch (Exception e) {
				out.println("Error executing command: " + e.getMessage());
				logger.log(Level.FINE, "Error executing command : " + command.getOriginalLine(), e);
			}
		}
	}

	private boolean isDDRCommand(String cmdline) {
		return ((cmdline != null) && (cmdline.length() > 0) && (cmdline.charAt(0) == '!'));
	}
	
	/*
	 * Jazz 26694 : integration of DDR interactive with jdmpview.
	 * A DDR interactive session is started when a core file is opened - if the core is DDR
	 * enabled then ! commands are available for executing debug extensions.
	 * 
	 */	
	
	/**
	 * Starts a DDR interactive session using the loaded image. If the core file is not DDR enabled
	 * this is recorded and future pling commands will not be available. In order to get access to the
	 * DDR classes it uses the Image classloader which, if the core file is DDR enabled, can see the 
	 * required classes.
	 * @param image Image created from the core file
	 */
	public void startDDRInteractiveSession(Image image, PrintStream out) {
		ddrAvailable = false;
		//late bind / reflect to the DDR -interactive main class so as not to provide a compile time link
		Class<?> ddriClass = null;
		try {
			//ddriClass = Class.forName(DDR_INTERACTIVE_CLASS, true, image.getClass().getClassLoader());
			ddriClass = Class.forName(DDR_INTERACTIVE_CLASS, true, image.getClass().getClassLoader());
		} catch (ClassNotFoundException e) {
			logger.fine("DDR is not enabled for " + image.getSource() + " (context " + id + "). It may be pre-DDR or not a core file e.g. javacore or PHD");
			return;
		}
		Constructor<?> constructor = null;
		try {
			constructor = ddriClass.getConstructor(new Class[]{List.class, PrintStream.class});
		} catch (Exception e) {
			logger.log(Level.FINE, "Error getting DDR Interactive constructor", e);
			return;
		}
		try {
			ArrayList<Object> contexts = new ArrayList<Object>();
			contexts.add((DTFJContext)this);
			ddriObject = constructor.newInstance(contexts, out);
			ddriMethod = ddriObject.getClass().getMethod("processLine", new Class[]{String.class});
			ddrAvailable = true;
			if (hasPropertyBeenSet(VERBOSE_MODE_PROPERTY)) {
				out.println("DTFJ DDR is enabled for this core dump");
			}
		} catch(InvocationTargetException e) {
			if((e.getCause() != null) && (e.getCause() instanceof UnsupportedOperationException)) {
				//this is thrown if DDR is not supported for this DTFJ Image e.g. it's pre-DDR, or a javacore etc.
				logger.fine("DDR is not enabled for " + image.getSource() + " (context " + id + "). It may be pre-DDR or not a core file e.g. javacore or PHD");
			} else {
				//for an invocation exception show the cause not the reflection exception
				ddrStartupException = e.getCause();
				logger.log(Level.FINE, "Error creating DDR Interactive instance", e.getCause());
			}
		} catch (Exception e) {
			ddrStartupException = e;
			logger.log(Level.FINE, "Error creating DDR Interactive instance", e);
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ICombinedContext#getDDRIObject()
	 */
	public Object getDDRIObject() {
		return ddriObject;
	}
	
	/**
	 * Pass the specified command to the DDR interactive command parser.
	 * @param line command to process
	 */
	private void executeDDRInteractiveCommand(String line, PrintStream out) {
		if(!ddrAvailable) {
			out.println("DDR is not enabled for this core file, '!' commands are disabled");
			return;
		}
		try {
			ddriMethod.invoke(ddriObject, new Object[]{line});
		} catch (Exception e) {
			out.println("Error executing DDR command " + line + " : " + e.getMessage());
			e.printStackTrace(out);		//these are debugger commands so show the stack trace
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ICombinedContext#getDDRStartupException()
	 */
	public Throwable getDDRStartupException() {
		return ddrStartupException;
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ICombinedContext#displayContext(com.ibm.jvm.dtfjview.Output, boolean)
	 */
	public void displayContext(IOutputManager out, boolean shortFormat) {
		out.print(id + " : ");
		if(ddrAvailable) {
			out.clearBuffer();
			try {
				out.setBuffering(true);
				//this command returns a list of all the contexts from DDR
				executeDDRInteractiveCommand("!context short", out.getPrintStream());
			} finally {
				out.setBuffering(false);
			}
			int index = out.getBuffer().indexOf(':');
			if(index != -1) {
				out.print(out.getBuffer().substring(index + 1).trim());		//skip the * and id at the front put there by DDR interactive
			}
			out.print(" : ");
			out.print(getJREVersion(shortFormat));
		} else {
			showDTFJContexts(out, shortFormat);
		}
		out.print("\n");
	}
	
	private void showDTFJContexts(IOutputManager out, boolean shortFormat) {
		if(getRuntime() == null) {
			out.print("<no Java runtime>");
		} else {
			out.print(getJREVersion(shortFormat));
		}
	}
	
	private String getJREVersion(boolean shortFormat) {
		StringBuilder builder = new StringBuilder();
		try {
			if(getRuntime() == null) {
				builder.append("No JRE");
			} else {
				try {
				String version = getRuntime().getVersion();
				if(shortFormat) {
					int index = version.indexOf("\n");
					String firstLine = null;
					if(index == -1) {
						if(version.length() == 0) {
							firstLine = "[cannot determine JRE level]";
						} else {
							firstLine = version;
						}
					} else {
						firstLine = version.substring(0, index).trim();
					}
					builder.append(firstLine);
				} else {
					String[] parts = version.split("\n");
					for(String part : parts) {
						builder.append("\n\t\t" + part.trim());
					}
				}
				} catch (CorruptDataException c ) {
					// Check if the Runtime was broken or we just have no version string.
					// Will throw another CDE if there is no runtime.
					if( getRuntime().getJavaVM() != null ) {
						builder.append("JRE level unknown");
					}
				}
			}
		} catch (CorruptDataException e) {
			builder.append("<corrupt Java runtime>");
		} catch (Exception e) {
			builder.append("<error : " + e.getMessage() + ">");
		}
		return builder.toString();
	}
	
	
	/* (non-Javadoc)
	 * @see com.ibm.jvm.dtfjview.spi.ICombinedContext#setAsCurrent()
	 */
	public void setAsCurrent() {
		URI uri = getImage().getSource();
		if((uri.getFragment() != null) && (uri.getFragment().length() != 0)) {
			//need to strip any fragments from the URI which point to the core in the zip file
			try {
				uri = new URI(uri.getScheme() + "://" + uri.getRawPath());		//need to use getRawPath so that spaces in Windows paths/files are correctly handled
			} catch (URISyntaxException e) {
				// this should not be possible as we are constructing the URI from another URI object and so will always be valid
			}
		}
		File file = new File(uri);
		System.setProperty("com.ibm.j9ddr.tools.ddrinteractive.filepath", file.getAbsolutePath());
	}

	public boolean isDDRAvailable() {
		return ddrAvailable;
	}

	
	
}
