/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2018 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.platform;

import java.math.BigInteger;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.LinkedHashMap;
import java.util.Map;

import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;

public class PlatformSectionParser extends SectionParser implements IPlatformTypes {
	
	private IImageAddressSpaceBuilder fImageAddressSpaceBuilder;
	private IImageProcessBuilder fImageProcessBuilder;
	
	public PlatformSectionParser() {
		super(PLATFORM_SECTION);
	}

	/**
	 * Controls parsing for host platform (XH) section in the javacore
	 * @throws ParserException
	 */
	protected void topLevelRule() throws ParserException {
		
		// get access to DTFJ AddressSpace and ImageProcess objects 
		fImageAddressSpaceBuilder = fImageBuilder.getCurrentAddressSpaceBuilder();
		if (fImageAddressSpaceBuilder != null) {
			fImageProcessBuilder = fImageAddressSpaceBuilder.getCurrentImageProcessBuilder();
		}
		
		hostInfo();
		crashInfo();
		// Windows can come before registers
		moduleInfo();
		registerInfo();
		// Linux can come after registers
		moduleInfo();
		parseEnvironmentVars();
	}

	/**
	 * Parse the OS and CPU information (2XHOSLEVEL,2XHCPUARCH and 2XHCPUS lines) 
	 * @throws ParserException
	 */
	private void hostInfo() throws ParserException {

		IAttributeValueMap results = null;
		
		// Operating system information
		results = processTagLineOptional(T_2XHHOSTNAME);
		if (results != null) {
			// store the host name and address
			String host_name = results.getTokenValue(PL_HOST_NAME);
			String host_addr = results.getTokenValue(PL_HOST_ADDR);
			if (host_name != null) {
				fImageBuilder.setHostName(host_name);
			}
			if (host_addr != null) {
				try {
					InetAddress addr = InetAddress.getByName(host_addr);
					fImageBuilder.addHostAddr(addr);
				} catch (UnknownHostException e) {
				}
			}
		}

		// Operating system information
		results = processTagLineRequired(T_2XHOSLEVEL);
		if (results != null) {
			// store the OS name and version - two strings
			String os_name = results.getTokenValue(PL_OS_NAME);
			String os_version = results.getTokenValue(PL_OS_VERSION);

			fImageBuilder.setOSType(os_name);
			fImageBuilder.setOSSubType(os_version);
		}

		// Process and ignore the 2XHCPUS line - no useful information in there
		processTagLineRequired(T_2XHCPUS);

		// Host processor information - CPU architecture
		results = processTagLineRequired(T_3XHCPUARCH);
		if (results != null) {
			String cpu_arch = results.getTokenValue(PL_CPU_ARCH);
			fImageBuilder.setcpuType(cpu_arch);

		}
		// Host processor information - CPU count
		results = processTagLineRequired(T_3XHNUMCPUS);
		if (results != null) {
			int cpu_count = results.getIntValue(PL_CPU_COUNT);
			fImageBuilder.setcpuCount(cpu_count);
		}
		processTagLineOptional(T_3XHNUMASUP);
	}

	/**
	 * Parse the exception information lines
	 * @throws ParserException
	 */
	private void crashInfo() throws ParserException {
		IAttributeValueMap results = null;
		
		// 1XHEXCPCODE line if present contains the signal information
		while((results = processTagLineOptional(T_1XHEXCPCODE)) != null) {
			int j9_signal = results.getIntValue(PL_SIGNAL);
			if (j9_signal != IBuilderData.NOT_AVAILABLE) {
				fImageProcessBuilder.setSignal(resolveGenericSignal(j9_signal));
			}
		}
		
		// 1XHERROR2 line indicates no crash information
		processTagLineOptional(T_1XHERROR2);
	}
	

	/**
	 * Parse the exception information lines
	 * @throws ParserException
	 */
	private void moduleInfo() throws ParserException {
		IAttributeValueMap results;

		// T_1XHEXCPMODULE line if present indicates registers
		String moduleName = null;
		long moduleBase = IBuilderData.NOT_AVAILABLE;
		while ((results = processTagLineOptional(T_1XHEXCPMODULE)) != null) {
			String name = results.getTokenValue(PL_MODULE_NAME);
			if (name != null) {
				moduleName = name;
			}
			long base = results.getLongValue(PL_MODULE_BASE);
			if (base != IBuilderData.NOT_AVAILABLE) {
				moduleBase = base;
			}
			if (moduleName != null && moduleBase != IBuilderData.NOT_AVAILABLE) {
				fImageProcessBuilder.addLibrary(moduleName);
				moduleName = null;
				moduleBase = IBuilderData.NOT_AVAILABLE;
			}
		}
	}
	
	/**
	 * Parse the exception register information lines
	 * @throws ParserException
	 */
	private void registerInfo() throws ParserException {
		// T_1XHREGISTERS line if present indicates registers
		if (processTagLineOptional(T_1XHREGISTERS) != null) {
			Map<String, Number> m = new LinkedHashMap<>();
			IAttributeValueMap results;
			while ((results = processTagLineOptional(T_2XHREGISTER)) != null) {
				String name = results.getTokenValue(PL_REGISTER_NAME);
				String value = results.getTokenValue(PL_REGISTER_VALUE);
				Number n = null;
				if (value != null) {
					// normally there's no '0x' prefix, but remove it if present
					String digits = value.regionMatches(true, 0, "0x", 0, 2) ? value.substring(2) : value;
					// register values are always in hex
					BigInteger bigN = new BigInteger(digits, 16);
					int bits = bigN.bitLength();
					if (bits <= 16) {
						n = Short.valueOf((short) bigN.intValue());
					} else if (bits <= 32) {
						n = Integer.valueOf(bigN.intValue());
					} else if (bits <= 64) {
						n = Long.valueOf(bigN.longValue());
					} else {
						n = bigN;
					}
				}
				m.put(name, n);
			}
			fImageProcessBuilder.setRegisters(m);
		}
	}
	
	// J9 port library signal bit values (from j9port.h). These same values are also used in 
	// class com.ibm.jvm.j9.dump.indexsupport.NodeGPF in the DTFJ J9 project. Really this is 
	// a candidate for using a DTFJ 'Common' project at some point.
	private final static int J9PORT_SIG_FLAG_SIGSEGV 	= 4;
	private final static int J9PORT_SIG_FLAG_SIGBUS 	= 8;
	private final static int J9PORT_SIG_FLAG_SIGILL 	= 16;
	private final static int J9PORT_SIG_FLAG_SIGFPE 	= 32;
	private final static int J9PORT_SIG_FLAG_SIGTRAP 	= 64;
	private final static int J9PORT_SIG_FLAG_SIGQUIT 	= 0x400;
	private final static int J9PORT_SIG_FLAG_SIGABRT 	= 0x800;
	private final static int J9PORT_SIG_FLAG_SIGTERM 	= 0x1000;
	
	private final static int J9PORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO 	= 0x40020;
	private final static int J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO = 0x80020;
	private final static int J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW 	= 0x100020;

	/**
	 * Converts J9 signal value to generic signal number.
	 */
	private int resolveGenericSignal(int num) {

		if ((num & J9PORT_SIG_FLAG_SIGQUIT) != 0) 	return 3;
		if ((num & J9PORT_SIG_FLAG_SIGILL) != 0) 	return 4;
		if ((num & J9PORT_SIG_FLAG_SIGTRAP) != 0) 	return 5;
		if ((num & J9PORT_SIG_FLAG_SIGABRT) != 0) 	return 6;
		if ((num & J9PORT_SIG_FLAG_SIGFPE) != 0) {
			if (num == J9PORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO) 		return 35;
			if (num == J9PORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO) 	return 36;
			if (num == J9PORT_SIG_FLAG_SIGFPE_INT_OVERFLOW) 	return 37;
			return 8;
		}
		if ((num & J9PORT_SIG_FLAG_SIGBUS) != 0) 	return 10;
		if ((num & J9PORT_SIG_FLAG_SIGSEGV) != 0) 	return 11;
		if ((num & J9PORT_SIG_FLAG_SIGTERM) != 0) 	return 15;
		return num;
	}
	
	/**
	 * Parse the user args information (1XHUSERARGS and 2XHUSERARG lines) 
	 * @throws ParserException
	 */
	private void parseEnvironmentVars() throws ParserException {
		IAttributeValueMap results = null;
		
		results = processTagLineOptional(T_1XHENVVARS);
		if (results != null) {
			while ((results = processTagLineOptional(T_2XHENVVAR)) != null) {
				String env_name = results.getTokenValue(ENV_NAME);
				String env_value = results.getTokenValue(ENV_VALUE);
				if (env_name != null) {
					fImageProcessBuilder.addEnvironmentVariable(env_name, env_value);
				}
			}
		}
	}
	
	/**
	 * Empty hook for now.
	 */
	protected void sovOnlyRules(String startingTag) throws ParserException {

	}

}
