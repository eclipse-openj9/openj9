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
package com.ibm.jvm.dtfjview.commands;

import static com.ibm.jvm.dtfjview.ExitCodes.JDMPVIEW_SYNTAX_ERROR;
import static com.ibm.jvm.dtfjview.SessionProperties.CORE_FILE_PATH_PROPERTY;
import static com.ibm.jvm.dtfjview.SessionProperties.EXTRACT_PROPERTY;
import static com.ibm.jvm.dtfjview.SessionProperties.IMAGE_FACTORY_PROPERTY;
import static com.ibm.jvm.dtfjview.SessionProperties.LEGACY_ZIP_MODE_PROPERTY;
import static com.ibm.jvm.dtfjview.SessionProperties.VERBOSE_MODE_PROPERTY;
import static com.ibm.jvm.dtfjview.SystemProperties.SYSPROP_FACTORY;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Iterator;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.utils.file.FileManager;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.IDTFJContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.jvm.dtfjview.CombinedContext;
import com.ibm.jvm.dtfjview.JdmpviewContextManager;
import com.ibm.jvm.dtfjview.Session;
import com.ibm.jvm.dtfjview.SessionProperties;
import com.ibm.jvm.dtfjview.Version;
import com.ibm.jvm.dtfjview.spi.ISession;

public class OpenCommand extends BaseJdmpviewCommand {
	private static final String CMD_NAME = "open";
	private static final String defaultFactoryName = "com.ibm.dtfj.image.j9.ImageFactory";
	
	private String factoryName = System.getProperty(SYSPROP_FACTORY, defaultFactoryName);
	private int apiLevelMajor = 0;					//DTFJ API level from the ImageAddressFactory
	private int apiLevelMinor = 0;
	private ImageFactory factory = null;
	
	{
		addCommand(CMD_NAME, "[path to core or zip]","opens the specified core or zip file");
		factory = getFactory();
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		switch(args.length) {
			case 0 :		//print out the version information for the factory
				out.println(Version.getAllVersionInfo(factory));
				return;
			case 1:
			case 2:
				imagesFromCommandLine(args);
				break;
			default :
				out.println("The open command requires at least one and at most two parameters. See 'help open'.");
				return;
		}
	}

	/**
	 * Creates a DTFJ image from the supplied core or zip file
	 */
	private void imagesFromCommandLine(String args[]) {
		apiLevelMajor = factory.getDTFJMajorVersion();
		apiLevelMinor = factory.getDTFJMinorVersion();

		out.println(Version.getAllVersionInfo(factory));
		out.println("Loading image from DTFJ...\n");
		
		try {
			Image[] images = new Image[1];		//default to a single image
			if (ctx.hasPropertyBeenSet(VERBOSE_MODE_PROPERTY)) {
				// If we are using DDR this will enable warnings.
				// (If not it's irrelevant.)
				Logger.getLogger("j9ddr.view.dtfj").setLevel(Level.WARNING);
			}
			//at this point the existence of either -core or -zip has been checked
			long current = System.currentTimeMillis();
			File file1 = new File(args[0]);
			if(args.length == 1) {
				if (FileManager.isArchive(file1) && !ctx.hasPropertyBeenSet(LEGACY_ZIP_MODE_PROPERTY) ) {
					images = factory.getImagesFromArchive(file1, ctx.hasPropertyBeenSet(EXTRACT_PROPERTY));
				} else {
					images[0] = factory.getImage(file1);
				}
			} else {
				File file2 = new File(args[1]);
				images[0] = factory.getImage(file1, file2);
			}
			logger.fine(String.format("Time taken to load image %d ms", System.currentTimeMillis() - current));
			for(Image image : images) {
				createContexts(image, args[0]);
			}
		} catch (IOException e) {
			out.println("Could not load dump file and/or could not load XML file: " + e.getMessage());
			if (ctx.hasPropertyBeenSet(VERBOSE_MODE_PROPERTY)) {
				e.printStackTrace();
			}
		}
	}
	
	private ImageFactory getFactory() {
		if(factory != null) {
			return factory;
		}
		try {
			Class<?> factoryClass = Class.forName(factoryName);
			factory = (ImageFactory)factoryClass.newInstance();
		} catch (ClassNotFoundException e) {
			out.println("ClassNotFoundException while getting ImageFactory: " + e.getMessage());
			out.println("Use -D" + SYSPROP_FACTORY + "=<classname> to change the ImageFactory");
			System.exit(JDMPVIEW_SYNTAX_ERROR);
		} catch (InstantiationException e) {
			out.println("InstantiationException while getting ImageFactory: " + e.getMessage());
			System.exit(JDMPVIEW_SYNTAX_ERROR);
		} catch (IllegalAccessException e) {
			out.println("IllegalAccessException while getting ImageFactory: " + e.getMessage());
			System.exit(JDMPVIEW_SYNTAX_ERROR);
		}
		return factory;
	}
	
	
	private void createContexts(Image loadedImage, String coreFilePath) {
		if(loadedImage == null) {
			//cannot create any contexts as an image has not been obtained
			return;
		}
		boolean hasContexts = false;
		Iterator<?> spaces = loadedImage.getAddressSpaces();
		while(spaces.hasNext()) {
			Object o = spaces.next();
			if(o instanceof ImageAddressSpace) {
				ImageAddressSpace space = (ImageAddressSpace) o;
				Iterator<?> procs = space.getProcesses();
				if(procs.hasNext()) {
					while(procs.hasNext()) {
						o = procs.next();
						if(o instanceof ImageProcess) {
							ImageProcess proc = (ImageProcess) o;
							Iterator<?> runtimes = proc.getRuntimes();
							if(runtimes.hasNext()) {
								while(runtimes.hasNext()) {
									o = runtimes.next();
									if(o instanceof JavaRuntime) {
										createCombinedContext(loadedImage, apiLevelMajor, apiLevelMinor, space, proc, (JavaRuntime)o, coreFilePath);
										hasContexts = true;
									} else if (o instanceof CorruptData) {
										logger.fine("CorruptData encountered in ImageProcess.getRuntimes(): " + ((CorruptData)o).toString());
										createCombinedContext(loadedImage, apiLevelMajor, apiLevelMinor, space, proc, null, coreFilePath);
										hasContexts = true;
									} else {
										logger.fine("Unexpected class encountered in ImageProcess.getRuntimes()");
										createCombinedContext(loadedImage, apiLevelMajor, apiLevelMinor, space, proc, null, coreFilePath);
										hasContexts = true;
									}
								}
							} else {
								//there are no runtimes so create a context for this process
								createCombinedContext(loadedImage, apiLevelMajor, apiLevelMinor, space, proc, null, coreFilePath);
								hasContexts = true;
							}
						}
					}
				} else {
					//context with only an address space
					createCombinedContext(loadedImage, apiLevelMajor, apiLevelMinor, space, null, null, coreFilePath);
					hasContexts = true;
				}
			} else {
				//need a representation of a corrupt context
				logger.fine("Skipping corrupt ImageAddress space");
			}
		}
		if(!hasContexts) {
			if(ctx.hasPropertyBeenSet(VERBOSE_MODE_PROPERTY)) {
				out.println("Warning : no contexts were found, is this a valid core file ?");
			}
		}
	}
	
	private void createCombinedContext(final Image loadedImage, final int major, final int minor, final ImageAddressSpace space, final ImageProcess proc, final JavaRuntime rt, String coreFilePath) {
		//take the DTFJ context and attempt to combine it with a DDR interactive one
		Object obj = ctx.getProperties().get(SessionProperties.SESSION_PROPERTY);
		if(obj == null) {
			logger.fine("Could not create a new context as the session property has not been set");
			return;
		}
		if(!(obj instanceof Session)) {
			logger.fine("Could not create a new context as the session type was not recognised [" + obj.getClass().getName() + "]");
			return;
		}
		JdmpviewContextManager mgr = (JdmpviewContextManager)((ISession)obj).getContextManager();
		CombinedContext cc = (CombinedContext)mgr.createContext(loadedImage, major, minor, space, proc, rt);
		cc.startDDRInteractiveSession(loadedImage, out);
		cc.getProperties().put(CORE_FILE_PATH_PROPERTY, coreFilePath);
		cc.getProperties().put(IMAGE_FACTORY_PROPERTY, getFactory());
		if (ctx.hasPropertyBeenSet(VERBOSE_MODE_PROPERTY)) {
			cc.getProperties().put(VERBOSE_MODE_PROPERTY, "true");
		}
		
		try {
			boolean hasLibError = true;		//flag to indicate if native libs are required but not present
			String os = cc.getImage().getSystemType().toLowerCase();
			if(os.contains("linux") || (os.contains("aix"))) {
				if(cc.getProcess() != null) {
					Iterator<?> modules = cc.getProcess().getLibraries();
					if(modules.hasNext()) {
						obj = modules.next();
						if(obj instanceof ImageModule) {
							hasLibError = false;		//there is at least one native lib available
						}
					}
				}
			} else {
				hasLibError = false;
			}
			if(hasLibError) {
				out.println("Warning : native libraries are not available for " + coreFilePath);
			}
		} catch (DataUnavailable e) {
			logger.log(Level.FINE, "Warning : native libraries are not available for " + coreFilePath);
		}
		catch (Exception e) {
			logger.log(Level.FINE, "Error determining if native libraries are required for " + coreFilePath, e);
		}
	}
	
	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("Opens a core file or zip file and loads it into jdmpview ready for further analysis. Any resultant contexts are added to the " +
				" list of currently available contexts");
	}

}

