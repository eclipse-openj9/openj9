/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Collection;

import com.ibm.j9ddr.IBootstrapRunnable;
import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.J9DDRClassLoader;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;

public class VMData implements IVMData {
	private static final String version = "29";

	public VMData() {
		super();
	}

	public void bootstrapRelative(String relativeClassname, Object... userData) throws ClassNotFoundException
	{
		Class<?> clazz = getClassLoader().loadClassRelativeToStream(relativeClassname, true);
		
		bootstrapClass(clazz, userData);
	}
	
	public void bootstrap(String binaryName, Object... userData) throws ClassNotFoundException 
	{
		// load classname in the correct class loader
		Class<?> clazz = getClassLoader().loadClass(binaryName);
		
		bootstrapClass(clazz, userData);
	}
	
	public J9DDRClassLoader getClassLoader()
	{
		return (J9DDRClassLoader) VMData.class.getClassLoader();
	}
	
	private void bootstrapClass(Class<?> clazz, Object[] userData)
	{
		try {
			IBootstrapRunnable runnable = (IBootstrapRunnable) clazz.newInstance();
			Method runMenthod = clazz.getMethod("run", IVMData.class, Object[].class);
			runMenthod.invoke(runnable, this, userData);
			return; 
		} catch (InvocationTargetException e) {
			Throwable cause = e.getTargetException();
			if (cause instanceof Error) {
				throw (Error) cause;
			} else if (cause instanceof RuntimeException) {
				throw (RuntimeException) cause;
			}
		} catch (Exception e) {
			throw new IllegalArgumentException(e);
		}
	}

	public Collection<StructureDescriptor> getStructures()
	{
		ClassLoader loader = VMData.class.getClassLoader();
		
		if (loader instanceof J9DDRClassLoader) {
			return ((J9DDRClassLoader)loader).getStructures();
		} else {
			throw new UnsupportedOperationException("This VMData wasn't loaded through a J9DDRClassLoader. Loaded by: " + loader + ", loader class: " + loader.getClass().getName());
		}
	}

	public String getVersion() {
		return version;
	}
	
	
}
