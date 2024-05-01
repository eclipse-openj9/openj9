/*[INCLUDE-IF Sidecar18-SE]*/
/*
 * Copyright IBM Corp. and others 2007
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
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
import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
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
	@Override
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
					// ignore
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
		// 1XHEXCPCODE line if present contains the signal information
		for (;;) {
			IAttributeValueMap results = processTagLineOptional(T_1XHEXCPCODE);

			if (results == null) {
				break;
			}

			IParserToken token = results.getToken(PL_SIGNAL);

			if (token == null) {
				continue;
			}

			String value = token.getValue();

			if (value.startsWith("0x")) { //$NON-NLS-1$
				try {
					int signal = Integer.parseUnsignedInt(value.substring(2), 16);

					if (signal != IBuilderData.NOT_AVAILABLE) {
						fImageProcessBuilder.setSignal(signal);
					}
				} catch (NumberFormatException e) {
					// ignore
				}
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
	@Override
	protected void sovOnlyRules(String startingTag) throws ParserException {
		// do nothing
	}

}
