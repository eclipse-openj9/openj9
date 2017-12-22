/*******************************************************************************
 * Copyright (c) 2009, 2016 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.comparators;

import java.net.InetAddress;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

import static junit.framework.Assert.*;
import static com.ibm.j9ddr.view.dtfj.test.TestUtil.*;

/**
 * @author andhall
 *
 */
public class ImageComparator extends DTFJComparator
{
	public static final int ADDRESS_SPACES = 1;
	public static final int CREATION_TIME = 2;
	public static final int HOST_NAME = 4;
	public static final int INSTALLED_MEMORY = 8;
	public static final int IP_ADDRESSES = 16;
	public static final int PROCESSOR_COUNT = 32;
	public static final int PROCESSOR_SUB_TYPE = 64;
	public static final int PROCESSOR_TYPE = 128;
	public static final int SYSTEM_TYPE = 256;
	public static final int SYSTEM_SUB_TYPE = 512;
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.view.dtfj.test.DTFJComparator#testEquals(java.lang.Object, java.lang.Object, int)
	 */
	@Override
	public void testEquals(Object ddrObject, Object jextractObject, int members)
	{
		Image ddrImg = (Image)ddrObject;
		Image jextractImg = (Image)jextractObject;
		
		if ((members & ADDRESS_SPACES) != 0) {
			new ImageAddressSpaceComparator().testComparatorIteratorEquals(ddrImg.getAddressSpaces(), jextractImg.getAddressSpaces(), "getAddressSpaces", ImageAddressSpace.class);
		}
		
		if ((members & CREATION_TIME) != 0) {
			long ddrCreationTime = -1;
			long jextractCreationTime = -1;
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				ddrCreationTime = ddrImg.getCreationTime();
			} catch (DataUnavailable e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				jextractCreationTime = jextractImg.getCreationTime();
			} catch (DataUnavailable e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException == null && jextractException == null) {
				assertEquals(jextractCreationTime, ddrCreationTime);
			} else if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
			} else {
				if (ddrException != null) {
					fail("DDR threw an exception, and jextract didn't");
				} else {
					fail("jextract threw an exception, and ddr didn't");
				}
			}
		}
		
		if ((members & HOST_NAME) != 0) {
			String ddrHostname = null;
			String jextractHostname = null;
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				ddrHostname = ddrImg.getHostName();
			} catch (Exception e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				jextractHostname = jextractImg.getHostName();
			} catch (Exception e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException == null && jextractException == null) {
				assertEquals(jextractHostname, ddrHostname);
			} else if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
			} else {
				if (ddrException != null) {
					fail("DDR threw an exception, and jextract didn't");
				} else {
					fail("jextract threw an exception, and ddr didn't");
				}
			}
		}
		
		if ((members & INSTALLED_MEMORY) != 0) {
			long ddrInstalledMemory = -1;
			long jextractInstalledMemory = -1;
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				ddrInstalledMemory = ddrImg.getInstalledMemory();
			} catch (DataUnavailable e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				jextractInstalledMemory = jextractImg.getInstalledMemory();
			} catch (DataUnavailable e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException == null && jextractException == null) {
				assertEquals(jextractInstalledMemory, ddrInstalledMemory);
			} else if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
			} else {
				if (ddrException != null) {
					fail("DDR threw an exception, and jextract didn't");
				} else {
					fail("jextract threw an exception, and ddr didn't");
				}
			}
		}
		
		if ((members & IP_ADDRESSES) != 0) {
			Iterator<?> ddrIt = null;
			Iterator<?> jextractIt = null;
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				ddrIt = ddrImg.getIPAddresses();
			} catch (Exception e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				jextractIt = jextractImg.getIPAddresses();
			} catch (Exception e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException == null && jextractException == null) {
				//Slurp iterators into lists, sort lists, compare contents
				List<?> ddrList = slurpIterator(ddrIt);
				List<?> jextractList = slurpIterator(jextractIt);
				
				assertEquals("IP address lists different lengths",jextractList.size(),ddrList.size());
				
				Comparator<Object> inetAddressComp = new Comparator<Object>(){

					public int compare(Object o1, Object o2)
					{
						InetAddress i1 = (InetAddress)o1;
						InetAddress i2 = (InetAddress)o2;
						
						return i1.getHostAddress().compareTo(i2.getHostAddress());
					}};
					
				Collections.sort(ddrList,inetAddressComp);
				Collections.sort(jextractList,inetAddressComp);
				
				assertEquals(jextractList,ddrList);
			} else if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
			} else {
				if (ddrException != null) {
					fail("DDR threw an exception, and jextract didn't");
				} else {
					fail("jextract threw an exception, and ddr didn't");
				}
			}
		}
		

		if ((members & PROCESSOR_COUNT) != 0) {
			int ddrProcessorCount = -1;
			int jextractProcessorCount = -1;
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				ddrProcessorCount = ddrImg.getProcessorCount();
			} catch (DataUnavailable e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				jextractProcessorCount = jextractImg.getProcessorCount();
			} catch (DataUnavailable e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException == null && jextractException == null) {
				assertEquals(jextractProcessorCount, ddrProcessorCount);
			} else if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
			} else {
				if (ddrException != null) {
					fail("DDR threw an exception, and jextract didn't");
				} else {
					fail("jextract threw an exception, and ddr didn't");
				}
			}
		}
		
		
		if ((members & PROCESSOR_SUB_TYPE) != 0) {
			String ddrSubType = null;
			String jextractSubType = null;
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				ddrSubType = ddrImg.getProcessorSubType();
			} catch (Exception e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				jextractSubType = jextractImg.getProcessorSubType();
			} catch (Exception e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException == null && jextractException == null) {
				assertEquals(jextractSubType, ddrSubType);
			} else if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
			} else {
				if (ddrException != null) {
					fail("DDR threw an exception, and jextract didn't");
				} else {
					fail("jextract threw an exception, and ddr didn't");
				}
			}
		}
		
		if ((members & PROCESSOR_TYPE) != 0) {
			String ddrType = null;
			String jextractType = null;
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				ddrType = ddrImg.getProcessorType();
			} catch (Exception e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				jextractType = jextractImg.getProcessorType();
			} catch (Exception e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException == null && jextractException == null) {
				assertEquals(jextractType, ddrType);
			} else if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
			} else {
				if (ddrException != null) {
					fail("DDR threw an exception, and jextract didn't");
				} else {
					fail("jextract threw an exception, and ddr didn't");
				}
			}
		}
		
		if ((members & SYSTEM_TYPE) != 0) {
			String ddrType = null;
			String jextractType = null;
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				ddrType = ddrImg.getSystemType();
			} catch (Exception e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				jextractType = jextractImg.getSystemType();
			} catch (Exception e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException == null && jextractException == null) {
				assertEquals(jextractType, ddrType);
			} else if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
			} else {
				if (ddrException != null) {
					fail("DDR threw an exception, and jextract didn't");
				} else {
					fail("jextract threw an exception, and ddr didn't");
				}
			}
		}
		
		if ((members & SYSTEM_SUB_TYPE) != 0) {
			String ddrType = null;
			String jextractType = null;
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				ddrType = ddrImg.getSystemSubType();
			} catch (Exception e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				jextractType = jextractImg.getSystemSubType();
			} catch (Exception e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException == null && jextractException == null) {
				assertEquals(jextractType, ddrType);
			} else if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
			} else {
				if (ddrException != null) {
					fail("DDR threw an exception, and jextract didn't");
				} else {
					fail("jextract threw an exception, and ddr didn't");
				}
			}
		}
	}

	

}
