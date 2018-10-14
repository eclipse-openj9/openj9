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
package j9vm.test.ddrext.util.parser;

import j9vm.test.ddrext.Constants;

/**
 * This class is being used to extract info from !j9javavm <address> DDR extension output 
 * @author fkaraman
 *
 */
public class J9JavaVmOutputParser {
	private static final String FIELDSIGNATURE_MAINTHREAD = "* mainThread";
	private static final String FIELDSIGNATURE_DLLLOADTABLE = "* dllLoadTable";
	private static final String FIELDSIGNATURE_SYSTEMPROPERTIES = "* systemProperties";
	private static final String FIELDSIGNATURE_CLASSLOADINGSTACKPOOL = "* classLoadingStackPool";
	private static final String FIELDSIGNATURE_VMTHREADPOOL = "* vmThreadPool";
	private static final String FIELDSIGNATURE_CLASSLOADERSBLOCKS = "* classLoaderBlocks";
	
	
	/**
	 * This method finds the address of mainThread from !j9javavm output
	 * 
	 * @param j9javavmOutput
	 * @return address of mainThread. 
	 */
	public static String getMainThreadAddress(String j9javavmOutput) {
		return ParserUtil.getFieldAddressOrValue(FIELDSIGNATURE_MAINTHREAD, Constants.J9VMTHREAD_CMD, j9javavmOutput);
	}

	/**
	 * This method finds the address of dllLoadTable from !j9javavm output
	 * 
	 * @param j9javavmOutput
	 * @return address of dllLoadTable. 
	 */
	public static String getDllLoadTableAddress(String j9javavmOutput) 
	{
		return ParserUtil.getFieldAddressOrValue(FIELDSIGNATURE_DLLLOADTABLE, Constants.J9POOL_CMD, j9javavmOutput);	
	}
	
	/**
	 * This method finds the address of systemProperties from !j9javavm output
	 * 
	 * @param j9javavmOutput
	 * @return address of systemProperties. 
	 */
	public static String getSystemPropertiesAddress(String j9javavmOutput) 
	{
		return ParserUtil.getFieldAddressOrValue(FIELDSIGNATURE_SYSTEMPROPERTIES, Constants.J9POOL_CMD, j9javavmOutput);	
	}

	/**
	 * This method finds the address of vmThreadPool from !j9javavm output
	 * 
	 * @param j9javavmOutput
	 * @return address of vmThreadPool. 
	 */
	public static String getVmThreadPoolAddress(String j9javavmOutput) 
	{
		return ParserUtil.getFieldAddressOrValue(FIELDSIGNATURE_VMTHREADPOOL, Constants.J9POOL_CMD, j9javavmOutput);	
	}

	/**
	 * This method finds the address of classLoaderBlocks from !j9javavm output
	 * 
	 * @param j9javavmOutput
	 * @return address of classLoaderBlocks. 
	 */
	public static String getClassLoaderBlocksAddress(String j9javavmOutput) 
	{
		return ParserUtil.getFieldAddressOrValue(FIELDSIGNATURE_CLASSLOADERSBLOCKS, Constants.J9POOL_CMD, j9javavmOutput);	
	}

	/**
	 * This method finds the address of classLoadingStackPool from !j9javavm output
	 * 
	 * @param j9javavmOutput
	 * @return address of classLoadingStackPool. 
	 */
	public static String getClassLoadingStackPoolAddress(String j9javavmOutput) 
	{
		return ParserUtil.getFieldAddressOrValue(FIELDSIGNATURE_CLASSLOADINGSTACKPOOL, Constants.J9POOL_CMD, j9javavmOutput);	
	}

}
