/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package j9vm.test.softmx;

import java.lang.management.ManagementFactory;
import java.util.ArrayList;
import java.util.List;

import org.testng.log4testng.Logger;

public class MemoryExhauster {

	private static final Logger logger = Logger.getLogger(MemoryExhauster.class);

	private java.lang.management.MemoryMXBean mb = ManagementFactory.getMemoryMXBean();

	// cast downwards so that we can use the IBM-specific functions
	private com.ibm.lang.management.MemoryMXBean ibmMemoryMBean = (com.ibm.lang.management.MemoryMXBean) mb;

	// Size of the objects used for forcing OOM. Should be sized so that in the face of OOM, the VM
	// can still terminate gracefully.
	private static final int OBJECT_SIZE = 20*1024*1024;

	// maximum size we can force the heap size up to in MB
	private static final int MAX_OBJECTS = 2000;

	/**
	 * This method is used to allocate objects into heap and fill the heap upto the provided percentage
	 * @param percentage - percentage value in double indicating how much heap should be filled up
	 * @return true if allocation was successful, false otherwise.
	 */
	public boolean usePercentageOfHeap( double percentage ) {
		long original_softmx_value =  ibmMemoryMBean.getMaxHeapSize();

		logger.debug( "	Current max heap size:  " + original_softmx_value + " bytes");

		Object[] myObjects = new Object[MAX_OBJECTS];

		logger.debug("	Starting Object allocation until used memory reaches " + percentage*100 + "% of current max heap size.");

		int i = 0;
		try {
			while ( ibmMemoryMBean.getHeapMemoryUsage().getCommitted() < ((long) ( original_softmx_value * percentage ))){
				try {
					myObjects[i] = new byte[OBJECT_SIZE];
					i++;
				} catch (OutOfMemoryError e){
					// at this point we stop
					logger.debug("Catch OutOfMemoryError at object allocation:" + i);
					logger.debug("Catch OutOfMemoryError before reaching " + percentage*100 + "% of current max heap size.");
					return false;
				}
			}
		} catch ( OutOfMemoryError e ) {
			logger.debug("Catch OutOfMemoryError reached in MemoryExhauster");
			return false;
		}

		logger.debug( "	Now we have used approximately " + percentage*100 + "% of current max heap size: " +  ibmMemoryMBean.getHeapMemoryUsage().getCommitted() + " bytes");
		return true;
	}

	/**
	 * This method is used to simply exhaust the heap memory complete in order to
	 * receive an OOM exception which will then generate the javacore for us to investigate.
	 */
	public boolean exhaustHeap() {
		try {
			List<Object> tempList = new ArrayList<Object>();
			while (true) {
				tempList.add(new byte[1024 * 1024]);
			}
		} catch (OutOfMemoryError OME) {
			logger.debug("Expected exception: MemoryExhauster received OutOfMemoryError");
			return true;
		}
	}
}
