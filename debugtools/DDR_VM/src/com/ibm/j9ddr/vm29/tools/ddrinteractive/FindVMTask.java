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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.IBootstrapRunnable;
import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.j9.DataType;

/**
 * IBootstrapRunnable that copies the VM pointer into a long array
 * @author andhall
 *
 */
public class FindVMTask implements IBootstrapRunnable
{

	private final Logger logger = CommandUtils.getLogger();
	
	// Used during bootstrapping to allow the JVM to be overridden if J9RAS is damaged but
	// the JVM can be found manually.
	private final static String J9VM_ADDRESS_PROPERTY = "com.ibm.j9ddr.vmaddr";
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.IBootstrapRunnable#run(com.ibm.j9ddr.IVMData, java.lang.Object[])
	 */
	public void run(IVMData vmData, Object[] userData)
	{
		long[] passBackArray = (long[])userData[0];
		String vmAddressString = System.getProperty(J9VM_ADDRESS_PROPERTY);
		
		if( vmAddressString != null ) {
			
			long address = 0;
			try {
				if( vmAddressString.startsWith("0x") ) {
					address = Long.parseLong( vmAddressString.substring(2), 16);
				} else {
					address = Long.parseLong( vmAddressString );
				}
			} catch (NumberFormatException nfe ) {
				logger.warning("System property " + J9VM_ADDRESS_PROPERTY + " does not contain a valid pointer address, found: " + vmAddressString);
				throw nfe;
			}
			logger.warning("FindVMTask forcing J9JavaVMPointer to address from system property " + J9VM_ADDRESS_PROPERTY + " : " + vmAddressString);
			// Override the cached version from J9RASHelper as well.
			J9JavaVMPointer vm = J9JavaVMPointer.cast(address);
			J9RASHelper.setCachedVM(vm);
			passBackArray[0] = vm.getAddress();
		} else {
			try {
				passBackArray[0] = DataType.getJ9RASPointer().vm().longValue();
			} catch (CorruptDataException e) {
				throw new RuntimeException(e);
			}
		}
		logger.fine("FindVMTask passing back J9JavaVMPointer: 0x" + Long.toHexString(passBackArray[0]));
	}

}
