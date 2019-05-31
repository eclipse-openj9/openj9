/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import j9vm.test.softmx.MemoryExhauster;

import java.lang.management.ManagementFactory;
import java.util.ArrayList;
import java.util.Iterator;

@Test(groups = { "level.extended" })
public class SoftmxLocalTest{

	private static final Logger logger = Logger.getLogger(SoftmxLocalTest.class);

	private ArrayList <String> outOfRangeErrors = new ArrayList<String>();
	private java.lang.management.MemoryMXBean mb = ManagementFactory.getMemoryMXBean();

    // cast downwards so that we can use the IBM-specific functions
    private com.ibm.lang.management.MemoryMXBean ibmBean = (com.ibm.lang.management.MemoryMXBean) mb;

    private MemoryExhauster memoryExhauster = new MemoryExhauster();

	/*
	 * Test heap size can be read through JMX
	 */
	@Test
	public void testJMXReadHeapSize(){

		long max_size = ibmBean.getMaxHeapSize();
	    long max_limit = ibmBean.getMaxHeapSizeLimit();
	    long min_size = ibmBean.getMinHeapSize();

	    logger.debug("  IBM-specific information:");
	    logger.debug(    "    Current GC mode:        " + ibmBean.getGCMode());
	    logger.debug(    "    Current max heap size:  " + max_size + " bytes");
	    logger.debug(    "    Max heap size limit:    " + max_limit + " bytes");
	    logger.debug(    "    Minimum heap size:      " + min_size + " bytes");

	    // check the heap information is reasonable
	    if (max_size < 0)
        {
	    	outOfRangeErrors.add("Negative value returned for Max Heap Size. Value was retrieved using ibm extensions test");
        }

        if (max_limit < 0)
        {
        	outOfRangeErrors.add("Negative value returned for Max Heap Size Limit, Value was retrieved using ibm extensions test");
        }

        if (min_size < 0)
        {
        	outOfRangeErrors.add("Negative value returned for Min Heap Size. Value was retrieved using ibm extensions test");
        }

        if (max_size < min_size)
        {
    		outOfRangeErrors.add("Max Heap Size returned was less then the Min Heap Size value.  Values were retrieved using ibm extensions test");
        }

   	    if (max_limit < max_size)
        {
    		outOfRangeErrors.add("Max Heap Size Limit returned was less then the Max Heap Size value.  Values were retrieved using ibm extensions test");
        }

   	    if (outOfRangeErrors.size() > 0){
   	    	Iterator errors = outOfRangeErrors.iterator();
   	    	while (errors.hasNext()){
   	    		logger.error(errors.next());
   	    	}
   	    	Assert.fail("	JMX return unreasonable heap size info!");
   	    }else{
   	    	logger.debug("	JMX return reasonable heap size info!");
   	    }

	}

	@Test
	public void testJMXSetHeapSize(){

		long max_size = ibmBean.getMaxHeapSize();
	    long max_limit = ibmBean.getMaxHeapSizeLimit();

	    if (ibmBean.isSetMaxHeapSizeSupported()){
	    	logger.debug(    "    Current max heap size:  " + max_size + " bytes");
	 	    logger.debug(    "    Max heap size limit:    " + max_limit + " bytes");
	    	if (max_size < max_limit){
	    		long reset_size = (max_size + max_limit)/2;
	    		logger.debug(    "    Reset heap size to:    " + reset_size + " bytes");
	    		ibmBean.setMaxHeapSize(reset_size);
	    		AssertJUnit.assertEquals(reset_size,ibmBean.getMaxHeapSize());
	    		logger.debug  ("    Current max heap size is reset to:  " + reset_size + " bytes");
	    	}else{
	    		Assert.fail("Warning: current maximum heap size reach maximum heap size limit,it can't be reset!");
	    	}
	    }else{
	    	Assert.fail("Warning: VM doesn't support runtime reconfiguration of the maximum heap size!");
	    }
	}

	@Test
	public void testJMXSetHeapSizeBiggerThanLimit(){

		long max_limit = ibmBean.getMaxHeapSizeLimit();
		if (ibmBean.isSetMaxHeapSizeSupported()){
			logger.debug ( "	Max heap size limit:    " + max_limit + " bytes");
			logger.debug ( "	Current max heap size:  " + ibmBean.getMaxHeapSize() + " bytes");
			logger.debug ( "	Reset max heap size to: " + (max_limit + 1024) + " bytes");

			try {
				ibmBean.setMaxHeapSize(max_limit + 1024);
				Assert.fail("	FAIL: Expected to get an Exception while trying to set max sixe bigger than max heap size limit!");
			} catch (java.lang.IllegalArgumentException e){
				logger.debug ("	PASS: get below expected exception while trying to set max sixe bigger than max heap size limit!");
				logger.debug("Expected exception", e);
			} catch (java.lang.UnsupportedOperationException  e){
				logger.warn ("	java.lang.UnsupportedOperationException: this operation is not supported!");
			} catch (java.lang.SecurityException e){
				logger.warn ("	java.lang.SecurityException: if a SecurityManager is being used and the caller does not have the ManagementPermission value of control");
			} catch (Exception e){
				logger.error("Unexpected exception ", e);
				Assert.fail("Unexpected exception " + e);
			}
		}else{
			Assert.fail("	Warning: VM doesn't support runtime reconfiguration of the maximum heap size!");
		}

	}

	@Test
	public void testJMXSetHeapSizeSmallerThanMin(){

		long min_size = ibmBean.getMinHeapSize();
		if (ibmBean.isSetMaxHeapSizeSupported()){
			logger.debug ( "	Current min heap size:  " + min_size + " bytes");
			logger.debug ( "	Reset max heap size to: " + (min_size - 1024) + " bytes");

			try {
				ibmBean.setMaxHeapSize(min_size - 1024);
				Assert.fail("	FAIL: Expected to get an exception while trying to set max sixe smaller than min heap size!");
			} catch (java.lang.IllegalArgumentException e){
				logger.debug ("	PASS: get below expected exception while trying to set max sixe bigger than max heap size limit!");
				logger.debug("Expected exception", e);
			} catch (java.lang.UnsupportedOperationException  e){
				logger.warn ("	java.lang.UnsupportedOperationException: this operation is not supported!");
			} catch (java.lang.SecurityException e){
				logger.warn ("	java.lang.SecurityException: if a SecurityManager is being used and the caller does not have the ManagementPermission value of control");
			} catch (Exception e){
				logger.error("Unexpected exception ", e);
				Assert.fail("Unexpected exception " + e);
			}
		}else{
			Assert.fail("	Warning: VM doesn't support runtime reconfiguration of the maximum heap size!");
		}

	}

	/*
	 * force memory used by the heap to 80% of current max heap size, then rest max heap size to 50% of original size, trigger gc to shrink heap, verify the heap shrinks to
	 * the reset value within 5 minutes. After that try to use 120% of reset max heap size, expect to get OutOfMemoryError.
	 */
	@Test
	public void testSoftmxHasEffects(){

		long current_max_size =  ibmBean.getMaxHeapSize();

		boolean exhausted = memoryExhauster.usePercentageOfHeap(0.8);

		if ( exhausted == false ) {
			Assert.fail("Catch OutOfMemoryError before reaching 80% of current max heap size.");
		}

		logger.debug( "	Now we have used approximately 80% of current max heap size: " +  ibmBean.getHeapMemoryUsage().getUsed() + " bytes");

		long new_max_size = (long) (current_max_size * 0.5);
		logger.debug("	Reset maximum heap size to 50% of original size: " + new_max_size);
		ibmBean.setMaxHeapSize(new_max_size);

		logger.debug("	Force Aggressive GC. Waiting for heap shrink..." );
		TestNatives.setAggressiveGCPolicy();

		long startTime = System.currentTimeMillis();
		boolean isShrink = false;

		//waiting for maximum 5 min (300000ms)
		while((System.currentTimeMillis() - startTime) < 300000 ){
			if (ibmBean.getHeapMemoryUsage().getCommitted() <= new_max_size){
				logger.debug( "	PASS: Heap shrink to " + ibmBean.getHeapMemoryUsage().getCommitted() + " bytes"
						+ " in " + (System.currentTimeMillis() - startTime) + " mSeconds");
				isShrink = true;
				break;
			}else{
				try {
					Thread.sleep(2000);
					logger.debug( "	Current committed memory:  " + ibmBean.getHeapMemoryUsage().getCommitted() + " bytes");
					logger.debug("	Force Aggressive GC. Waiting for heap shrink..." );
					TestNatives.setAggressiveGCPolicy();
				} catch (InterruptedException e) {
					e.printStackTrace();
					Assert.fail("	FAIL: Catch InterruptedException");
				}
			}
		}

		if(!isShrink){
			logger.debug(    "	Generate Java core dump after forcing GC.");
			com.ibm.jvm.Dump.JavaDump();
			com.ibm.jvm.Dump.HeapDump();
			com.ibm.jvm.Dump.SystemDump();
		}

		AssertJUnit.assertTrue("	FAIL: Heap can't shrink to the reset max heap size within 5 minutes!",isShrink);

		//test heap can't expand beyond current max heap size

		boolean OOMNotReached = true;

		try {
			OOMNotReached = memoryExhauster.usePercentageOfHeap(1.2);
		} catch ( OutOfMemoryError e ) {
			logger.debug("SoftmxLocalTest : OOM Reached");
			e.printStackTrace();
			OOMNotReached = false;
		}

		if ( OOMNotReached ){
			logger.warn(" FAIL: Heap can expand beyond reset max heap size! Generate Java core dump.");
			com.ibm.jvm.Dump.JavaDump();
			com.ibm.jvm.Dump.HeapDump();
			com.ibm.jvm.Dump.SystemDump();
		} else {
			logger.debug("	PASS: Heap can't expand beyond reset max heap size.");
		}
	}
}

