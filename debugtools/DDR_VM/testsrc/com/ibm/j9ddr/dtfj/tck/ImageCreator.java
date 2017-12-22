/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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
package com.ibm.j9ddr.dtfj.tck;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Iterator;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.tck.api.IImageCreator;
import com.ibm.dtfj.tck.api.IJavaRuntimeCreator;
import com.ibm.dtfj.tck.api.TCKConfiguration;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory;

/**
 * Factory used by the DTFJ TCK to create Images.
 * 
 * Expects the name of the system dump to be passed in with
 * -Dsystem.dump=location
 * 
 * @author andhall
 *
 */
public class ImageCreator implements IImageCreator, IJavaRuntimeCreator
{

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.tck.api.IImageCreator#createProcessImage(com.ibm.dtfj.tck.api.TCKConfiguration)
	 */
	public Image createProcessImage(TCKConfiguration conf) throws IOException
	{
		fixClassPath();
		String systemDump = System.getProperty("system.dump");
		
		if (systemDump == null) {
			throw new IllegalArgumentException("You need to specify a system dump with -Dsystem.dump=");
		}
		
		J9DDRImageFactory factory = new J9DDRImageFactory();
		
		Image image = factory.getImage(new File(systemDump));
		
		if (null == image) {
			throw new IOException("Cannot get DTFJ image from " + systemDump);
		}
		
		return image;
	}

	public JavaRuntime createJavaRuntime(TCKConfiguration conf)
			throws IOException
	{
		Image image = createProcessImage(conf);
		
		Iterator<?> it = image.getAddressSpaces();
		
		while (it.hasNext()) {
			Object addressSpaceObj = it.next();
			
			if (addressSpaceObj instanceof ImageAddressSpace) {
				ImageAddressSpace as = (ImageAddressSpace)addressSpaceObj;
				
				Iterator<?> processesIt = as.getProcesses();
				
				while (processesIt.hasNext()) {
					Object processObj = processesIt.next();
					
					if (processObj instanceof ImageProcess) {
						ImageProcess process = (ImageProcess)processObj;
						
						Iterator<?> runtimeIterator = process.getRuntimes();
						
						while (runtimeIterator.hasNext()) {
							Object runtimeObj = runtimeIterator.next();
							
							if (runtimeObj instanceof JavaRuntime) {
								return (JavaRuntime)runtimeObj;
							} else {
								throw new IOException("Unexpected runtime object: " + runtimeObj + ", class = " + runtimeObj.getClass());
							}
						}
					} else {
						throw new IOException("Unexpected process object: " + processObj + ", class = " + processObj.getClass());
					}
				}
			} else {
				throw new IOException("Unexpected address space object: " + addressSpaceObj + ", class = " + addressSpaceObj.getClass());
			}
		}
		
		return null;
	}
	
	@SuppressWarnings("unchecked")
	private static void fixClassPath() {
		URL ddrURL = null;
		try {
			ddrURL = new File(System.getProperty("java.home", ""), "lib/ddr/j9ddr.jar").toURI().toURL();
		} catch (MalformedURLException e) {
			// This can only happen with a typo in source code
			e.printStackTrace();
		}
		
		ClassLoader loader = ClassLoader.getSystemClassLoader();
		Class loaderClazz = loader.getClass();

		try {
			while (loaderClazz != null && !loaderClazz.getName().equals("java.net.URLClassLoader")) {
				loaderClazz = loaderClazz.getSuperclass();
			}
			
			if (loaderClazz == null) {
				throw new UnsupportedOperationException("Application class loader is not an instance of URLClassLoader.  Can not initialize J9DDR");
			}
			
			Method addURLMethod = loaderClazz.getDeclaredMethod("addURL", new Class[] {URL.class});
			addURLMethod.setAccessible(true);
			addURLMethod.invoke(loader, new Object[] {ddrURL});
		} catch (SecurityException e) {
			e.printStackTrace();
			throw new UnsupportedOperationException("Must enable suppressAccessChecks permission");
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			throw new UnsupportedOperationException("Must enable suppressAccessChecks permission");
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
	}

}
