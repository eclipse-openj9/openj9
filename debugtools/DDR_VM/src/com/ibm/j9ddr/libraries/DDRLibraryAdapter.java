/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

package com.ibm.j9ddr.libraries;

import static java.util.logging.Level.SEVERE;
import static java.util.logging.Level.WARNING;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.j9ddr.corereaders.CoreReader;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.ILibraryDependentCore;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory;

@SuppressWarnings("restriction")
public class DDRLibraryAdapter implements LibraryAdapter {
	private static final Logger logger = Logger.getLogger(LibraryCollector.LOGGER_NAME);
	private ArrayList<String> moduleNames = null;
	private final ArrayList<String> errorMessages = new ArrayList<String>();
	
	public ArrayList<String> getErrorMessages() {
		return errorMessages;
	}

	public ArrayList<String> getLibraryList(final File coreFile) {
		if(moduleNames == null) {
			constructLibraryList(coreFile);
		}
		return moduleNames;
	}
	
	public boolean isLibraryCollectionRequired(File coreFile) {
		ICore reader = getReader(coreFile.getPath());
		return (reader instanceof ILibraryDependentCore);
	}

	private ICore getReader(final String path) {
		final ICore reader;
		try {
			reader = CoreReader.readCoreFile(path);
		} catch (IOException e) {
			logger.log(SEVERE, "Could not open core file", e);
			errorMessages.add(e.getMessage());
			return null;
		}
		return reader;
	}	
	
	/**
	 * Constructs the list of libraries required using the DDR implementation of the DTFJ Image* API.
	 * This ensures that the correct classloading is used for determining which libraries to collect.
	 * @param coreFile core file to process
	 */
	@SuppressWarnings({ "unchecked" })
	private void constructLibraryList(final File coreFile) {
		moduleNames = new ArrayList<String>();
		ImageFactory factory = new J9DDRImageFactory();
		final Image image;
		final boolean isAIX;
		try {
			image = factory.getImage(coreFile);
			isAIX = image.getSystemType().toLowerCase().startsWith("aix");
		} catch (IOException e) {
			logger.log(SEVERE, "Could not open core file", e);
			errorMessages.add(e.getMessage());
			return;
		} catch (CorruptDataException e) {
			logger.log(SEVERE, "Could not determine system type", e);
			errorMessages.add(e.getMessage());
			return;		
		} catch (DataUnavailable e) {
			logger.log(SEVERE, "Could not determine system type", e);
			errorMessages.add(e.getMessage());
			return;		
		}
		for(Iterator spaces = image.getAddressSpaces(); spaces.hasNext(); ) {
			ImageAddressSpace space = (ImageAddressSpace) spaces.next(); 
			for(Iterator procs = space.getProcesses(); procs.hasNext(); ) {
				ImageProcess proc = (ImageProcess) procs.next();
				try {
					ImageModule exe = proc.getExecutable();				//add the executable to the list of libraries to be collected
					moduleNames.add(exe.getName());
					for(Iterator libraries = proc.getLibraries(); libraries.hasNext(); ) {
						ImageModule module = (ImageModule) libraries.next();
						String key = null;
						try {	//handle CDE thrown by getName(), as this is required further on this call needs to succeed
							if(isAIX) {
								key = module.getName();
								int pos = key.indexOf(".a("); 		//check on AIX if module is the .a file or library
								if((pos != -1) && (key.lastIndexOf(')') == key.length() - 1)) {
									key = key.substring(0, pos + 2);
								}
							} else {
								key = module.getName();
							}
							logger.fine("Module : " + key);
							if(!moduleNames.contains(key)) {			//don't store duplicate libraries
								moduleNames.add(key);
							}
						} catch (Exception e) {
							logger.log(WARNING, "Error getting module name", e);
						}
					}
				} catch (DataUnavailable e) {
					logger.log(WARNING, "Error getting library list", e);
					errorMessages.add(e.getMessage());
				} catch (com.ibm.dtfj.image.CorruptDataException e) {
					logger.log(WARNING, "Error getting library list", e);
					errorMessages.add(e.getMessage());
				}
			}
		}
	}
	
}
