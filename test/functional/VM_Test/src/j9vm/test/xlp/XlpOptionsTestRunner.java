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

package j9vm.test.xlp;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import j9vm.runner.Runner;
import j9vm.test.xlphelper.XlpOption;
import j9vm.test.xlphelper.XlpUtil;

/**
 * This test checks different flavors of -Xlp option except -Xlp:codecache
 * It uses -Xlp options along with -verbose:gc to verify the working of -Xlp options.
 * 
 * Before testing any -Xlp option, it runs -verbose:sizes to check if default large page sizes is supported by the platform
 * and to get the default page size. Default page size is the first entry in the list of supported page sizes displayed by
 * -verbose:sizes output.
 * 
 * Default/preferred page size on various platforms is as follows:
 * 		On AIX, it is 64K if available.
 * 		On Linux x86 (including amd64), it is 2M if available.
 * 		On zLinux, it is 1M if available.
 * 		On z/OS, it is 1M pageable if available. 
 *  
 * All Xlp options to be tested are added to an Xlp option array list in following order:
 * 		no -Xlp option,
 * 		-Xlp,
 * 		-Xlp<size>,
 * 		-Xlp:objectheap,
 * 		-Xlp with -Xlp<size>,
 * 		-Xlp with -Xlp:objectheap,
 * 		-Xlp<size> with -Xlp:objectheap,
 * 		Multiple -Xlp<size>,
 * 		Multiple -Xlp:objectheap
 *		Multiple pagesize and [non,]pageable parameters
 * 		
 * 
 * -Xlp options tested are specific to the platform in terms of page size and page type specified.
 * 
 * It then executes command corresponding to each entry in the Xlp option array list.
 * Each command uses -verbose:gc option to generate GC logs.
 * 
 * Output of each command is analyzed as follows:
 * 
 * 		- When running without -Xlp option, verify in GC logs that JVM is using the default page size.
 * 			Note that on AIX preferred default page size for heap allocation is 64K and on Linux it is 2M.
 * 
 * 		- For other flavors of -Xlp (which includes -Xlp<size> and -Xlp:objectheap) it performs following checks:
 * 			- If default large page size is not available, then '-Xlp' and '-Xlp<size>' should fail.
 * 			- If default large page size is available, check the presence of following information in gc logs:
 * 				'pageSize': this is the page size used by port library to allocate heap.
 * 				'pageType': page type for 'pageSize'. 
 * 				'requestedPageSize': this is the page size used by GC to make request for heap allocation to port library.
 * 				'requestedPageType': page type for 'requestedPageSize'.
 * 
 *				A sample output from gc logs showing above parameter is:
 *					<attribute name="pageSize" value="0x100000" />
 *					<attribute name="pageType" value="nonpageable" />
 *					<attribute name="requestedPageSize" value="0x80000000" />
 *					<attribute name="requestedPageType" value="nonpageable" />
 *
 *			- On non-Z platforms, if default large page size is not available, verify that JVM uses default page size. 
 * 			- If the (requestedPageSize, requestedPageType) combination is not same as specified in -Xlp option,
 * 				a warning message that JVM is using a different page size than specified should be printed.
 * 
 * 		- Verify the (requestedPageSize, requestedPageType) in gc logs is same as the value of -Xlp:objectheap in -verbose:sizes output. 
 * 
 * @author Ashutosh Mehra
 */
public class XlpOptionsTestRunner extends Runner {

	private static final long ONE_KB = 1 * 1024;
	private static final long ONE_MB = 1 * 1024 * 1024;
	private static final long ONE_GB = 1 * 1024 * 1024 * 1024;
	
	private int commandIndex = 0;
	
	private ArrayList<XlpOption> xlpOptionsList = null;
	
	private boolean isDefaultLargePageSizeSupported = false;
	
	private long defaultPageSize = 4 * ONE_KB;
	private String defaultPageType = XlpUtil.XLP_PAGE_TYPE_NOT_USED;
	private String differentPageSizeWarningMsg = "(.)* Large page size (.)* is not a supported page size; using (.)* instead";
	private String unsupportedOptionMsg = "System configuration does not support option '-Xlp'";
	
	public XlpOptionsTestRunner(String className, String exeName,
			String bootClassPath, String userClassPath, String javaVersion) {
		super(className, exeName, bootClassPath, userClassPath, javaVersion);

		populateXlpOptionsList();
		
		if (osName == OSName.ZOS) {
			defaultPageType = XlpUtil.XLP_PAGE_TYPE_PAGEABLE;
		}
	}
	
	/**
	 * Creates array list containing all Xlp options to be tested.
	 */
	protected void populateXlpOptionsList() {
		xlpOptionsList = new ArrayList<XlpOption>();
		switch(osName) {
		case AIX:
			/* No -Xlp option */
			xlpOptionsList.add(new XlpOption(null, false));
			
			/* Test '-Xlp' option. We can't determine the pageSize beforehand for -Xlp. 
			 * Moreover, we don't need pageSize for this case as command with '-Xlp' option
			 * will never generate a warning message.  
			 */
			xlpOptionsList.add(new XlpOption("-Xlp", true));
			
			/* -XX:+UseLargePages */
			xlpOptionsList.add(new XlpOption("-XX:+UseLargePages", true));

			/* Test -Xlp<size> options */
			xlpOptionsList.add(new XlpOption("-Xlp4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp16M", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			/* Value larger than or equal to 4G for pagesize on 32-bit JVM will cause JVM startup to fail */
			if (addrMode == AddrMode.BIT64) {
				xlpOptionsList.add(new XlpOption("-Xlp16G", 16 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			}
			/* An unsupported page size (as of now) */
			xlpOptionsList.add(new XlpOption("-Xlp7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* Test '-XX:LargePageSizeInBytes=<size>' options */
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=16M", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));			
			if (addrMode == AddrMode.BIT64) {
				xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=16G", 16 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			}
			
			/* Test -Xlp:objectheap: options. Note that [non]pageable parameters are just ignored. */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K,pageable", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K,nonpageable", 4 *ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=64K,pageable", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=64K,nonpageable", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=16M", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=16M,pageable", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=16M,nonpageable", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			/* Value larger than or equal to 4G for pagesize on 32-bit JVM will cause JVM startup to fail */
			if (addrMode == AddrMode.BIT64) {
				xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=16G", 16 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
				xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=16G,pageable", 16 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
				xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=16G,nonpageable", 16 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			}
			/* Unsupported page size (as of now) */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M,pageable", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M,nonpageable", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			
			/* Test -Xlp with -Xlp<size> option. In such case -Xlp is ignored */
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp4K -Xlp", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp7M -Xlp", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			/* Test -Xlp with -Xlp:objectheap: option. In such case -Xlp is ignored */
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp:objectheap:pagesize=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K -Xlp", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp:objectheap:pagesize=7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M -Xlp", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));

			/* Test '-Xlp:objectheap:pagesize=<size>' with '-Xlp:LargePageSizeInBytes=<size>' */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=64K -XX:LargePageSizeInBytes=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=4K -Xlp:objectheap:pagesize=64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* Test '-Xlp' with '-XX:LargePageSizeInBytes=<size>' */
			xlpOptionsList.add(new XlpOption("-Xlp -XX:LargePageSizeInBytes=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* TODO: Test '-Xlp' with '-XX:LargePageSizeInBytes=<size>', and '-Xlp:objectheap:pagesize=<size>' */

			/* Test -Xlp<size> with -Xlp:objectheap: option. In such cases rightmost option wins */
			xlpOptionsList.add(new XlpOption("-Xlp64K -Xlp:objectheap:pagesize=16M", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=16M -Xlp64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			xlpOptionsList.add(new XlpOption("-Xlp16M -Xlp:objectheap:pagesize=7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M -Xlp16M", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* Test '-Xlp<size>' with '-XX:LargePageSizeInBytes=<size>' */
			xlpOptionsList.add(new XlpOption("-Xlp64K -XX:LargePageSizeInBytes=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=4K -Xlp64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* TODO: Test '-Xlp<size>' with '-XX:LargePageSizeInBytes=<size>' and 'Xlp:objectheap:pagesize=<size>' */

			/* Test Multiple '-XX:LargePageSizeInBytes=<size>'. Rightmost option wins */
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=64K -XX:LargePageSizeInBytes=16M", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=16M -XX:LargePageSizeInBytes=64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));

			/* Test multiple -Xlp<size> options. In such cases rightmost option wins */
			xlpOptionsList.add(new XlpOption("-Xlp64K -Xlp16M", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp16M -Xlp64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			/* Test multiple -Xlp:objectheap: options. In such cases rightmost option wins */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=64K -Xlp:objectheap:pagesize=16M", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=16M -Xlp:objectheap:pagesize=64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));

			/* Test multiple pagesize parameters. In such cases rightmost parameter wins */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=64K,pagesize=16M", 16 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=16M,pagesize=64K", 64 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			break;
			
		case LINUX:
		case WINDOWS:
			/* No -Xlp option */
			xlpOptionsList.add(new XlpOption(null, false));
			
			/* Test '-Xlp' option. We can't determine the pageSize beforehand for -Xlp. 
			 * Moreover, we don't need pageSize for this case as command with '-Xlp' option
			 * will never generate a warning message.  
			 */
			xlpOptionsList.add(new XlpOption("-Xlp", true));
			
			/* -XX:+UseLargePages */
			xlpOptionsList.add(new XlpOption("-XX:+UseLargePages", true));

			/* Test '-Xlp<size>' options */
			xlpOptionsList.add(new XlpOption("-Xlp4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp4M", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			/* Unsupported page size (as of now) */
			xlpOptionsList.add(new XlpOption("-Xlp7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			/* Test '-XX:LargePageSizeInBytes=<size>' options */
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=4M", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* Test '-Xlp:objectheap:' options. Note that [non]pageable parameters are just ignored. */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K,pageable", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K,nonpageable", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2M,pageable", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2M,nonpageable", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4M", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4M,pageable", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4M,nonpageable", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			/* Unsupported page size (as of now) */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M,pageable", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M,nonpageable", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			
			/* Test -Xlp with -Xlp<size> option. In such case -Xlp is ignored */
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp4K -Xlp", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp7M -Xlp", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			/* Test -Xlp with -Xlp:objectheap: option. In such case -Xlp is ignored */
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp:objectheap:pagesize=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K -Xlp", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp:objectheap:pagesize=7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M -Xlp", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
		
			/* Test '-Xlp<size>' with '-XX:LargePageSizeInBytes=<size>' */
			xlpOptionsList.add(new XlpOption("-Xlp2M -XX:LargePageSizeInBytes=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=4K -Xlp2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* Test '-Xlp' with '-XX:LargePageSizeInBytes=<size>' */
			xlpOptionsList.add(new XlpOption("-Xlp -XX:LargePageSizeInBytes=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* TODO: Test '-Xlp' with '-XX:LargePageSizeInBytes=<size>', and '-Xlp:objectheap:pagesize=<size>' */
		
			/* Test -Xlp<size> with -Xlp:objectheap: option. In such cases rightmost option wins */
			xlpOptionsList.add(new XlpOption("-Xlp2M -Xlp:objectheap:pagesize=4M", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4M -Xlp2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			xlpOptionsList.add(new XlpOption("-Xlp2M -Xlp:objectheap:pagesize=7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M -Xlp2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			/* Test '-Xlp<size>' with '-XX:LargePageSizeInBytes=<size>' */
			xlpOptionsList.add(new XlpOption("-Xlp4M -XX:LargePageSizeInBytes=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=4K -Xlp4M", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* TODO: Test '-Xlp<size>' with '-XX:LargePageSizeInBytes=<size>' and 'Xlp:objectheap:pagesize=<size>' */

			/* Test Multiple '-XX:LargePageSizeInBytes=<size>'. Rightmost option wins */
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=2M -XX:LargePageSizeInBytes=4M", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=4M -XX:LargePageSizeInBytes=2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));

			/* Test multiple -Xlp<size> options. In such cases rightmost option wins */
			xlpOptionsList.add(new XlpOption("-Xlp2M -Xlp4M", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-Xlp4M -Xlp2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			
			/* Test multiple -Xlp:objectheap: options. In such cases rightmost option wins */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2M -Xlp:objectheap:pagesize=4M", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4M -Xlp:objectheap:pagesize=2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));

			/* Test multiple pagesize parameters. In such cases rightmost parameter wins */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2M,pagesize=4M", 4 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4M,pagesize=2M", 2 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));

			break;
			
		case ZOS:
			/* No -Xlp option */
			xlpOptionsList.add(new XlpOption(null, false));
			
			/* Test '-Xlp' option. We can't determine the pageSize beforehand for -Xlp. 
			 * Moreover, we don't need pageSize for this case as command with '-Xlp' option
			 * will never generate a warning message.  
			 */
			xlpOptionsList.add(new XlpOption("-Xlp", true));
			
			/* -XX:+UseLargePages */
			xlpOptionsList.add(new XlpOption("-XX:+UseLargePages", true));

			/* Test '-Xlp<size>' options */
			xlpOptionsList.add(new XlpOption("-Xlp4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, true));
			xlpOptionsList.add(new XlpOption("-Xlp1M", ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, true));
			xlpOptionsList.add(new XlpOption("-Xlp2G", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, true));
			/* Unsupported page size (as of now) */
			xlpOptionsList.add(new XlpOption("-Xlp7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, true));
			
			/* Test '-XX:LargePageSizeInBytes=<size>' options */
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=1M", ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=2G", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* Test '-Xlp:objectheap:' options */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K,pageable", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K,nonpageable", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=1M,pageable", ONE_MB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=1M,nonpageable", ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2G,pageable", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2G,nonpageable", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M,pageable", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M,nonpageable", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));
			
			/* Test '-Xlp<size>' and '-XX:LargePageSizeInBytes=<size>'. Rightmost wins */
			xlpOptionsList.add(new XlpOption("-Xlp1M -XX:LargePageSizeInBytes=2G", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=2G -Xlp1M", 1 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));

			/* Test '-Xlp<size>' with '-XX:LargePageSizeInBytes=<size>' */
			xlpOptionsList.add(new XlpOption("-Xlp2G -XX:LargePageSizeInBytes=1M", 1 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=1M -Xlp2G", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* Test '-Xlp' with '-XX:LargePageSizeInBytes=<size>' */
			xlpOptionsList.add(new XlpOption("-Xlp -XX:LargePageSizeInBytes=1M", 1 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* TODO: Test '-Xlp' with '-XX:LargePageSizeInBytes=<size>', and '-Xlp:objectheap:pagesize=<size>' */

			/* Test -Xlp with -Xlp<size> option. In such case -Xlp is ignored */
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp4K", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, true));
			xlpOptionsList.add(new XlpOption("-Xlp4K -Xlp", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, true));
			
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp7M", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, true));
			xlpOptionsList.add(new XlpOption("-Xlp7M -Xlp", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, true));
			
			/* Test -Xlp with -Xlp:objectheap: option. In such case -Xlp is ignored */
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp:objectheap:pagesize=4K,pageable", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=4K,pageable -Xlp", 4 * ONE_KB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, false));
			
			xlpOptionsList.add(new XlpOption("-Xlp -Xlp:objectheap:pagesize=7M,nonpageable", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M,nonpageable -Xlp", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));
			
			/* Test -Xlp<size> with -Xlp:objectheap: option. In such cases rightmost option wins */
			xlpOptionsList.add(new XlpOption("-Xlp1M -Xlp:objectheap:pagesize=2G,pageable", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2G,pageable -Xlp1M", ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, true));
			
			xlpOptionsList.add(new XlpOption("-Xlp2G -Xlp:objectheap:pagesize=7M,nonpageable", 7 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=7M,nonpageable -Xlp2G", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, true));
			
			/* Test '-Xlp<size>' with '-XX:LargePageSizeInBytes=<size>' */
			xlpOptionsList.add(new XlpOption("-Xlp1M -XX:LargePageSizeInBytes=2G", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=2G -Xlp1M", 1 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, true));

			/* TODO: Test '-Xlp<size>' with '-XX:LargePageSizeInBytes=<size>' and 'Xlp:objectheap:pagesize=<size>' */

			/* Test Multiple '-XX:LargePageSizeInBytes=<size>'. Rightmost option wins */
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=1M -XX:LargePageSizeInBytes=2G", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));
			xlpOptionsList.add(new XlpOption("-XX:LargePageSizeInBytes=2G -XX:LargePageSizeInBytes=1M", 1 * ONE_MB, XlpUtil.XLP_PAGE_TYPE_NOT_USED, false));

			/* Test multiple -Xlp<size> options. In such cases rightmost option wins */
			xlpOptionsList.add(new XlpOption("-Xlp1M -Xlp2G", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, true));
			xlpOptionsList.add(new XlpOption("-Xlp2G -Xlp1M", ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, true));
			
			/* Test multiple -Xlp:objectheap: options. In such cases rightmost option wins */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=1M,pageable -Xlp:objectheap:pagesize=2G,nonpageable", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2G,nonpageable -Xlp:objectheap:pagesize=1M,pageable", ONE_MB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, false));

			/* Test multiple pagesize and [non,]pageable parameters. In such cases rightmost parameter wins */
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=1M,pageable,pagesize=2G,nonpageable", 2 * ONE_GB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pagesize=2G,pagesize=1M,nonpageable,pageable", ONE_MB, XlpUtil.XLP_PAGE_TYPE_PAGEABLE, false));
			xlpOptionsList.add(new XlpOption("-Xlp:objectheap:pageable,pagesize=2G,nonpageable,pagesize=1M", ONE_MB, XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE, false));

			break;
		default:
			System.out.println("WARNING: Failed to determine underlying OS. This test needs to know underlying OS.");
			break;
		}
	}

	/* Overrides method in Runner. */
	public String getCustomCommandLineOptions() {
		String customOptions = super.getCustomCommandLineOptions();
		
		/* First command is to see if large page sizes are supported by parsing output of -verbose:sizes */
		customOptions += "-verbose:sizes ";
		if (commandIndex > 0) {
			String option = null;
			customOptions += "-verbose:gc ";
			XlpOption xlpOption = xlpOptionsList.get(commandIndex-1);
			option = xlpOption.getOption();
			/* XlpOption.option is null for 0th entry in xlpOptionsList.
			 * This corresponds to running without -Xlp option.
			 */
			if (option != null) {
				customOptions += option;
			}
		}

		return customOptions;
	}

	/* Overrides method in j9vm.runner.Runner. */
	public boolean run() {
		boolean success = false;
		/* xlpOptionsList.size()+1 is to account for first command that runs -verbose:sizes */
		for (commandIndex = 0; commandIndex < xlpOptionsList.size()+1; commandIndex++) {
			success = super.run();
			if (commandIndex != 0) {
				/* We are testing -Xlp option */
				XlpOption xlpOption = xlpOptionsList.get(commandIndex-1);
				/* Overwrite 'success' if we know the command will fail */ 
				if ((xlpOption.canFail() == true) && (isDefaultLargePageSizeSupported == false)) {
					success = true;
				}
			}
			if (success == true) {
				byte[] stdOut = inCollector.getOutputAsByteArray();
				byte[] stdErr = errCollector.getOutputAsByteArray();
				try {
					success = analyze(stdOut, stdErr);
				} catch (Exception e) {
					success = false;
					System.out.println("Unexpected Exception:");
					e.printStackTrace();
				}
			}
			if (success == false) {
				break;
			}
		}
		return success;
	}
	
	public boolean analyze(byte[] stdOut, byte[] stdErr) throws IOException {
		BufferedReader in = new BufferedReader(new InputStreamReader(
				new ByteArrayInputStream(stdErr)));
		String errorLine = null;
		boolean error = false;
		
		if (commandIndex == 0) {
			String line = null;
			/* Output corresponds to -verbose:sizes option.
			 * Parse it to check if default large page size is available or not.
			 */
			
			/* Skip any statements till -Xlp */
			do {
				line = in.readLine();
			} while((line != null) && (line.indexOf("-Xlp:objectheap:pagesize=") == -1));
			if (line != null) {
				String pageSizeString = null;
				String pageTypeString = null;
				String defaultLargePageSize = null;
				String defaultLargePageType = null;
				
				/*
				 * An example of -Xlp in -verbose:sizes on z/OS platform is:
				 *   	-Xlp:objectheap:pagesize=4K,pageable   large page size
				 *   		            available large page sizes:
				 *   	                4K pageable
				 *                      1M pageable
				 *                      2G nonpageable
				 *                      
				 * An example of -Xlp in -verbose:sizes on non-z/OS platforms is:
				 *   	-Xlp:objectheap:pagesize=4K   large page size
				 *   		            available large page sizes:
				 *   	                4K
				 *                      64K
				 *                      16M
				 *                      2G
				 *                      
				 */
				line = in.readLine();	/* skip 'available large page sizes:' */
				boolean firstEntryDone = false;
				
				do {
					line = in.readLine();
					if (line == null) {
						/* This should not happen, but in case does, break out */
						break;
					}
					line = line.trim();
					if (line.startsWith("-X")) {
						/* Some other option. End of list of supported page sizes, break out */
						break;
					}
					if (!firstEntryDone) {
						/* This is the first entry, treat it as default page size. 
						 * Note that it may get overwritten later.
						 */
						firstEntryDone = true;
						if (osName == OSName.ZOS) {
							/* Split around empty space to get page size and page type */
							pageSizeString = line.split(" ")[0];
							defaultPageType = line.split(" ")[1];
						} else {
							pageSizeString = line;
						}
						defaultPageSize = XlpUtil.pageSizeStringToLong(pageSizeString);
						if (defaultPageSize == 0) {
							/* Failed to get valid page size */
							error = true;
							errorLine = line;
							break;
						}
					} else {
						/* Overwrite default page size depending on preferences specific to each platform */
						if (osName == OSName.ZOS) {
							/* Split around empty space to get page size and page type */
							pageSizeString = line.split(" ")[0];
							pageTypeString = line.split(" ")[1];
							/* On z/OS, default large page size is 1M nonpageable */
							if ((pageSizeString.equals("1M") == true) && (pageTypeString.equals(XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE))) {
								isDefaultLargePageSizeSupported = true;
								defaultLargePageSize = pageSizeString;
								defaultLargePageType = pageTypeString;
							}
							/* On z/OS, default or preferred page size is 1M pageable for heap */
							if ((pageSizeString.equals("1M") == true) && (pageTypeString.equals(XlpUtil.XLP_PAGE_TYPE_PAGEABLE))) {
								defaultPageSize = ONE_MB;
								defaultPageType = "pageable";
							}
						} else {
							/* If there are more than one page size, that means default large page size is available */
							isDefaultLargePageSizeSupported = true;
							
							if (defaultLargePageSize == null) {
								defaultLargePageSize = line;
							}
							if (osName == OSName.AIX) {
								if (line.equals("64K")) {
									/* On AIX, default or preferred page size is 64K for heap */
									defaultPageSize = 64 * ONE_KB;
									defaultLargePageSize = line;
								}
								if (line.equals("16M")) {
									defaultLargePageSize = line;
								}
							} else {
								if (osName == OSName.LINUX)  {
									if (osArch == OSArch.X86) {
										if (line.equals("2M")) {
											/* On Linux x86, default or preferred page size is 2M for heap */
											defaultPageSize = 2 * ONE_MB;
										}
									} else if (osArch == OSArch.S390X) {
										if (line.equals("1M")) {
											/* On zLinux, default or preferred page size is 1M for heap */
											defaultPageSize = ONE_MB;
										}
									}
								}
							}
						}
					}
				} while (true);
				
				if (!error) {
					if (isDefaultLargePageSizeSupported) {
						if (osName == OSName.ZOS) {
							System.out.println("INFO: Default large page size is " + defaultLargePageSize + " " + defaultLargePageType);
						} else {
							System.out.println("INFO: Default large page size is " + defaultLargePageSize);
						}
					} else {
						System.out.println("INFO: Default large page size is not supported");
					}
				} else {
					if (errorLine != null) {
						System.out.println("Error in line: " + errorLine);
					}
					return false;
				}
			}
			return true;
		} else {
			/* Add all output statements in a array list */
			ArrayList<String> outputList = new ArrayList<String>();
			String inputLine = null;
			
			do {
				inputLine = in.readLine();
				if (inputLine != null) {
					outputList.add(inputLine);
				}
			} while(inputLine != null);
			
			XlpOption xlpOption = xlpOptionsList.get(commandIndex-1);
			String option = xlpOption.getOption();
			
			if ((xlpOption.canFail() == false) || (isDefaultLargePageSizeSupported == true)) {
				boolean pageSizeFound = false;
				boolean pageTypeFound = false;
				boolean requestedPageSizeFound = false;
				boolean requestedPageTypeFound = false;
				long requestedPageSize = 0;
				String requestedPageType = null;
				long pageSizeInVerbose = 0;
				String pageTypeInVerbose = null;
				Pattern pageSizeRegex = Pattern.compile(".*0x(([0-9]|[a-f]|[A-F])*).*");
				Matcher pageSizeMatcher = null;
				boolean isVerboseOutputPresent = false;
				
				for (String line : outputList) {
					/* If large page size is supported, -verbose:sizes should display the page size and type used by JVM for heap allocation */
					if (line.trim().startsWith("-Xlp:objectheap:pagesize=")) {
						isVerboseOutputPresent = true;
						/* Parse -Xlp:objectheap statement to get page size and type used by JVM for heap allocation */
						line = line.trim();
						/* Split around empty space and use first element to get page size and type */
						String objectHeapInfo = line.split(" ")[0].trim();
						int pageSizeBegin = objectHeapInfo.indexOf("=") + 1;
						int pageSizeEnd = 0;
						if (osName == OSName.ZOS) {
							pageSizeEnd = objectHeapInfo.indexOf(",");
							if (pageSizeEnd == -1) {
								System.out.println("ERROR: Error in parsing -Xlp:codecache statement. Did not find ','. ");
								error = true;
								errorLine = line;
								break;
							}
						} else {
							pageSizeEnd = objectHeapInfo.length();
						}
						String pageSizeString = objectHeapInfo.substring(pageSizeBegin, pageSizeEnd);
						pageSizeInVerbose = XlpUtil.pageSizeStringToLong(pageSizeString);
						if (pageSizeInVerbose == 0) {
							error = true;
							errorLine = line;
							break;
						}
						if (osName == OSName.ZOS) {
							pageTypeInVerbose = objectHeapInfo.substring(pageSizeEnd + 1);
						} else {
							pageTypeInVerbose = XlpUtil.XLP_PAGE_TYPE_NOT_USED;
						}
						continue;
					}
					
					if (line.indexOf("name=\"pageSize\"") != -1) {
						pageSizeMatcher = pageSizeRegex.matcher(line);
						if (pageSizeMatcher.matches()) {
							pageSizeFound = true;
							continue;
						} else {
							System.out.println("ERROR: Did not find valid \"pageSize\" value in verbose:gc output");
							errorLine = line;
							error = true;
							break;
						}
					}
					
					if (line.indexOf("name=\"pageType\"") != -1) {
						switch(osName) {
						case AIX:
						case LINUX:
						case WINDOWS:
							if (line.indexOf(XlpUtil.XLP_PAGE_TYPE_NOT_USED) != -1) {
								pageTypeFound = true;
							} else {
								System.out.println("ERROR: Expected pageType \"not used\" not found in verbose:gc output\n");
								errorLine = line;
								error = true;
							}
							break;
						case ZOS:
							if ((line.indexOf(XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE) != -1) || (line.indexOf(XlpUtil.XLP_PAGE_TYPE_PAGEABLE) != -1)) {
								pageTypeFound = true;
							} else {
								System.out.println("ERROR: Expected pageType \"nonpageable\" or \"pageable\" not found in verbose:gc output\n");
								errorLine = line;
								error = true;
							}
							break;
						default:
							System.out.println("ERROR: Undefined OS. This test needs to know underlying operating system");
							error = true;
							break;
						}
						if (error) {
							break;
						} else {
							continue;
						}
					}
					
					if (line.indexOf("name=\"requestedPageSize\"") != -1) {
						pageSizeMatcher = pageSizeRegex.matcher(line);
						if (pageSizeMatcher.matches()) {
							/* Group 1 matches to first capturing group which is page size value */
							String pageSizeString = pageSizeMatcher.group(1);
							requestedPageSize = Long.parseLong(pageSizeString, 16);
							requestedPageSizeFound = true;
							continue;
						} else {
							System.out.println("ERROR: Did not find valid \"requestedPageSize\" value in verbose:gc output");
							errorLine = line;
							error = true;
							break;
						}
					}
					
					if (line.indexOf("name=\"requestedPageType\"") != -1) {
						switch(osName) {
						case AIX:
						case LINUX:
						case WINDOWS:
							if (line.indexOf(XlpUtil.XLP_PAGE_TYPE_NOT_USED) != -1) {
								requestedPageType = XlpUtil.XLP_PAGE_TYPE_NOT_USED;
								requestedPageTypeFound = true;
							} else {
								System.out.println("ERROR: Expected pageType \"not used\" not found in verbose:gc output\n");
								errorLine = line;
								error = true;
							}
							break;
						case ZOS:
							if (line.indexOf(XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE) != -1) {
								requestedPageType = XlpUtil.XLP_PAGE_TYPE_NONPAGEABLE;
								requestedPageTypeFound = true;
							} else if (line.indexOf(XlpUtil.XLP_PAGE_TYPE_PAGEABLE) != -1) {
								requestedPageType = XlpUtil.XLP_PAGE_TYPE_PAGEABLE;
								requestedPageTypeFound = true;
							} else {
								System.out.println("ERROR: Expected pageType \"nonpageable\" or \"pageable\" not found in verbose:gc output\n");
								errorLine = line;
								error = true;
							}
							break;
						default:
							System.out.println("ERROR: Undefined OS. This test needs to know underlying Operating System");
							error = true;
							break;
						}
						if (error) {
							break;
						} else {
							continue;
						}
					}
				}
				
				if (!error) {
					if (pageSizeFound && pageTypeFound && requestedPageSizeFound && requestedPageTypeFound) {
						System.out.println("INFO: verbose:gc contains required information\n");
						if (isVerboseOutputPresent) {
							if ((pageSizeInVerbose == requestedPageSize) && (pageTypeInVerbose.equals(requestedPageType))) {
								System.out.println("INFO: Requested page size in GC logs matches with page size in -verbose:sizes output ");
							} else {
								System.out.println("ERROR: Requested page size in GC logs does not match with page size in -verbose:sizes output ");
								System.out.println("\t Page size and type in verbose:sizes output: " + pageSizeInVerbose + " " + pageTypeInVerbose);
								System.out.println("\t Requested page size and type in GC logs: " + requestedPageSize + " " + requestedPageType);
								return false;
							}
							if ((option == null) || ((osName != OSName.ZOS) && (isDefaultLargePageSizeSupported == false))) {
								/* (requestedPageSize, requestedPageType) should be same as (defaultPageSize, defaultPageType) if
								 * 	- we are running without -Xlp option, or
								 * 	- on non Z, default large page size is not supported and are running with -Xlp:objectheap option
								 */
								if ((defaultPageSize != requestedPageSize) || (!defaultPageType.equals(requestedPageType))) {
									if (option == null) {
										System.out.println("ERROR: Without -Xlp JVM should use default page size and type\n");
									} else {
										System.out
												.println("ERROR: -Xlp:objectheap option on OS without large page support" +
														"should use default page size and type\n"); 
									}
									System.out.println("\t Default page size and type: " + defaultPageSize + " " + defaultPageType);
									System.out.println("\t Page size and type used by JVM: " + requestedPageSize + " " + requestedPageType);
									return false;
								} else {
									if (option == null) {
										System.out.println("INFO: JVM is using default page size in absence of -Xlp option");
									} else {
										System.out.println("INFO: JVM is using default page size as OS does not support large page sizes\n");
									}
								}
							}
						}
						if (option != null) {
							/* Check if warning message should be printed */
							long optionPageSize = xlpOption.getPageSize();
							if (optionPageSize != 0) {
								String optionPageType = xlpOption.getPageType();
								if ((optionPageSize != requestedPageSize) || (!optionPageType.equals(requestedPageType))) {
									/* Warning message should have been printed */
									boolean warningMsgFound = false;
									for (String line: outputList) {
										if (Pattern.matches(differentPageSizeWarningMsg, line)) {
											warningMsgFound = true;
											System.out.println("INFO: Found warning message for using different page size than specified\n");
											break;
										}
									}
									if (!warningMsgFound) {
										/* Print error message */
										System.out.println("ERROR: Page size and type in Xlp option is not same as used by JVM, but the expected warning message is not found");
										System.out.println("\tPage size and page type in Xlp option: " + optionPageSize + " " + optionPageType);
										System.out.println("\tPage size and page type used by JVM: " + requestedPageSize + " " + requestedPageType);
										return false;
									}
								}
							}
						}
						return true;
					} else {
						System.out
								.println("ERROR: Did not find expected information in verbose:gc output");
						System.out.println("ERROR: pageSizeFound: "
								+ pageSizeFound + "pageTypeFound: "
								+ pageTypeFound + " requestedPageSizeFound: "
								+ requestedPageSizeFound
								+ " requestedPageTypeFound: "
								+ requestedPageTypeFound);
						return false;
					}
				} else {
					/* Print the output statement where error occurred */
					if (errorLine != null) {
						System.out.println("Error in line: " + errorLine);
					}
					return false;
				}
			} else {
				/* Check for messages related to unsupported page size */
				boolean unsupportedOptionMsgFound = false;
				for (String line: outputList) {
					if (line.indexOf(unsupportedOptionMsg) != -1) {
						unsupportedOptionMsgFound = true;
						System.out.println("INFO: Found message for unsupported option\n");
						break;
					}
				}
				if (!unsupportedOptionMsgFound) {
					/* Print error message */
					System.out.println("ERROR: Did not find message about unsupported -Xlp option");
					return false;
				}
				return true;
			}
		}
	}
}
