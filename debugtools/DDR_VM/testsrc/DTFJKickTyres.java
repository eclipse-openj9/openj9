import java.io.File;
import java.util.Iterator;
import java.util.Properties;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageSymbol;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.dtfj.java.JavaVMOption;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory;

/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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

/**
 * @author andhall
 *
 */
public class DTFJKickTyres
{

	/**
	 * @param args
	 */
	public static void main(String[] args) throws Exception
	{
		boolean useJExtract = false;
		
		if (args.length > 1) {
			if (args[1].toLowerCase().trim().equals("jextract")) {
				useJExtract = true;
			}
		}
		
		ImageFactory factory = null;
		if (useJExtract) {
			try {
				Class<?> jxfactoryclass = Class.forName("com.ibm.dtfj.image.j9.ImageFactory");
				factory =  (ImageFactory)jxfactoryclass.newInstance();
			} catch (Exception e) {
				System.out.println("Could not create a jextract based implementation of ImageFactory");
				e.printStackTrace();
				System.exit(1);
			}
		} else {
			factory = new J9DDRImageFactory();
		}
		
		Image img = factory.getImage(new File(args[0]));
		
		Iterator<?> addressSpaceIt = img.getAddressSpaces();
		
		while (addressSpaceIt.hasNext()) {
			
			ImageAddressSpace as = (ImageAddressSpace)addressSpaceIt.next();
			
			Iterator<?> processIt = as.getProcesses();
			
			while (processIt.hasNext()) {
				ImageProcess process = (ImageProcess)processIt.next();
				
				System.err.println("Got process " + process);
				try {
					System.err.println("Command line was " + process.getCommandLine());
				} catch (Throwable t) {
					t.printStackTrace();
				}
				
				try {
					System.err.println("Executable was: " + process.getExecutable());
				} catch (Throwable t) {
					t.printStackTrace();
				}
				
				
				try {
					System.err.println("Modules were:");
					
					Iterator<?> it = process.getLibraries();
				
					if (!it.hasNext()) {
						System.err.println("No modules!");
					}
				
					while (it.hasNext()) {
						ImageModule module = (ImageModule) it.next();
						
						System.err.println("* " + module.getName());
						
						Iterator<?> symIt = module.getSymbols();
						
						while (symIt.hasNext()) {
							Object symObj = symIt.next();
							if (symObj instanceof ImageSymbol) {
								ImageSymbol sym = (ImageSymbol) symObj;
								
								if (sym.getName().toLowerCase().contains("environ")) {
									System.err.println("X sym " + sym.getName() + " = " + sym.getAddress());
								}
							}
						}
					}
				} catch (Throwable t) {
					t.printStackTrace();
				}
				
				try {
					Properties env = process.getEnvironment();
				
					System.err.println("Environment");
					for (Object key : env.keySet()) {
					System.err.println(key + " = " + env.getProperty((String)key));
					}
				} catch (Throwable t) {
					t.printStackTrace();
				}
				
				Iterator<?> runtimeIt = process.getRuntimes();
				
				while (runtimeIt.hasNext()) {
					JavaRuntime runtime = (JavaRuntime) runtimeIt.next();
					
					System.err.println("Got runtime: " + runtime);
					
					JavaVMInitArgs initArgs = runtime.getJavaVMInitArgs();
					
					Iterator<?> optionsIt = initArgs.getOptions();
					
					System.err.println("Options:");
					while (optionsIt.hasNext()) {
						Object optionObj = optionsIt.next();
						
						if (optionObj instanceof JavaVMOption) {
							JavaVMOption option = (JavaVMOption) optionObj;
							
							System.err.println("* " + option.getOptionString());
						}
					}
				}
			}
			
		}
	}

}
