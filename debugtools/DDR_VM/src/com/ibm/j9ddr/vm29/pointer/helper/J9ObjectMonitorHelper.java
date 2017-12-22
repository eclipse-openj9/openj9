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
package com.ibm.j9ddr.vm29.pointer.helper;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.pointer.generated.*;

/**
 * A helper class for functionality related to J9ObjectMonitorPointers
 * 
 *  NOTE: J9ObjectMonitorHelper is only relevant to inflated monitors, whereas ObjectMonitor handles both inflated and uninflated monitors
 */
public class J9ObjectMonitorHelper 
{
	
	/**
	 * Returns the J9ObjectPointer corresponding to the specified J9ObjectMonitorPointer
	 * 
	 * @throws CorruptDataException
	 * 
	 * @param objectMonitor the J9ObjectMonitorPointer who's object we are after
	 * 
	 * @return the J9ObjectPointer corresponding to the specified J9ObjectMonitorPointer
	 */
	public static J9ObjectPointer getObject(J9ObjectMonitorPointer objectMonitor) throws CorruptDataException {
		return J9ObjectPointer.cast(objectMonitor.monitor().userData());
	}

	/**
	 * Returns a string containing information related to this monitor.
	 * 
	 * @throws CorruptDataException
	 * 
	 * If the monitor represents:
	 * 	- an instance of java/lang/String, the value of the String is included
	 * 	- an instance of java/long/Class, the name of the class it represents is included 
	 * 
	 * @param objectMonitor the monitor who's information we are after.
	 * 
	 * @return a string containing information related to this monitor.
	 */
	public static String formatFullInteractive(J9ObjectMonitorPointer objectMonitor) throws CorruptDataException {
		String output = "";
		
		J9ObjectPointer object = J9ObjectMonitorHelper.getObject(objectMonitor);
		output = String.format("\t%s\t\"%s\"\n", object.formatShortInteractive(), J9ObjectHelper.getClassName(object));
		
		output = output + String.format("\t%s\t\n", objectMonitor.formatShortInteractive());
		output = output + String.format("\t%s\n", objectMonitor.monitor().formatShortInteractive());
		output = output + String.format("\t%s\n", objectMonitor.monitor().owner().formatShortInteractive());
		
		/* If this a java/lang/Class, print the name it represents */
		if (0 == J9ObjectHelper.getClassName(object).compareTo("java/lang/Class")) {
			
			J9ClassPointer backingClazz = ConstantPoolHelpers.J9VM_J9CLASS_FROM_HEAPCLASS(object);
			String name = J9ClassHelper.getName(backingClazz);
			output = output + String.format("\tClass name: \"" + name +"\"\n");
		
		} else if (0 == J9ObjectHelper.getClassName(object).compareTo("java/lang/String")) {
			output = output + String.format("\tString value: \"" + J9ObjectHelper.stringValue(object) +"\"\n");
		} 

		return output;
	
	}
}


