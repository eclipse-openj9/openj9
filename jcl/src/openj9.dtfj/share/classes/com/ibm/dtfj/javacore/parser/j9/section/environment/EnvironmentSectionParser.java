/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.environment;

import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.Date;

import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;
import com.ibm.dtfj.javacore.parser.j9.section.common.ICommonTypes;

/**
 * Provides parser for environment (CI) section in the javacore
 * @throws ParserException
 */
public class EnvironmentSectionParser extends SectionParser implements IEnvironmentTypes {

	private IImageProcessBuilder fImageProcessBuilder;
	private IJavaRuntimeBuilder fRuntimeBuilder;
	
	public EnvironmentSectionParser() {
		super(ENVIRONMENT_SECTION);
	}

	/**
	 * Overall controls of parsing for environment (CI) section
	 * @throws ParserException
	 */
	protected void topLevelRule() throws ParserException {
		
		fImageProcessBuilder = fImageBuilder.getCurrentAddressSpaceBuilder().getCurrentImageProcessBuilder();
		fRuntimeBuilder = fImageProcessBuilder.getCurrentJavaRuntimeBuilder();
		
		parseVersion();
		parseUserArgs();
		parseEnvironmentVars();
		parseJVMMI();
	}
	
	/**
	 * Parse the version information (1CIJAVAVERSION, and lines) 
	 * @throws ParserException
	 */
	private void parseVersion() throws ParserException {
		IAttributeValueMap results = null;
		
		// Process the version lines
		if ((results = processTagLineOptional(T_1CIJAVAVERSION)) != null) {
			int pointerSize = results.getIntValue(ICommonTypes.POINTER_SIZE);
			if (pointerSize != IBuilderData.NOT_AVAILABLE) {
				fImageProcessBuilder.setPointerSize(pointerSize);
			}
			String javaversion = results.getTokenValue(ARG_STRING);
			String checkString = "j2re 1.";
			//check that the core we are processing is from Java 5 or later
			if((javaversion.toLowerCase().startsWith(checkString)) && (javaversion.length() > (checkString.length() + 1))) {
				String number = javaversion.substring(checkString.length(), javaversion.indexOf('.', checkString.length() + 1));
				try {
					int version = Integer.parseInt(number);
					if(version <= 4) {
						throw new ParserException("Javacore files earlier than 1.5.0 are not supported");
					}
				} catch (NumberFormatException e) {
					handleError("Error determining Java version", e);
				}	
			}
			results = processTagLineOptional(T_1CIVMVERSION);
			if (results != null) {
				javaversion = buildVersionString(javaversion, results);
			}
			results = processTagLineOptional(T_1CIJITVERSION);
			if (results != null) {
				javaversion = buildVersionString(javaversion, results);
			}
			results = processTagLineOptional(T_1CIGCVERSION);
			if (results != null) {
				javaversion = buildVersionString(javaversion, results);
			}
			if (javaversion != null) {
				fRuntimeBuilder.setJavaVersion(javaversion);
			}
		} else {		
			processTagLineOptional(T_1CIVMVERSION);
			processTagLineOptional(T_1CIJITVERSION);
			processTagLineOptional(T_1CIGCVERSION);
		}
		results = processTagLineOptional(T_1CIJITMODES);
		if(results != null) {
			IParserToken token = results.getToken(JIT_MODE);
			if(token != null) {
				String value = token.getValue().trim();
				if(value.toLowerCase().startsWith("unavailable")) {
					//JIT was disabled with -Xint
					fRuntimeBuilder.setJITEnabled(false);
				} else {
					//JIT was operational but still could be disabled if -Xnojit was specified
					fRuntimeBuilder.setJITEnabled(true);
					String[] props = value.split(",");
					for(int i = 0; i < props.length; i++) {
						String item = props[i].trim();		//trim any white space
						int index = item.indexOf(' ');
						if(index == -1) {
							fRuntimeBuilder.addJITProperty(props[i], "<default value>");
						} else {
							String name = item.substring(0, index);
							String pvalue = item.substring(index + 1);
							fRuntimeBuilder.addJITProperty(name, pvalue);
						}
					}
				}
			}
		}
		processTagLineOptional(T_1CIRUNNINGAS);
		
		// 1CISTARTTIME   JVM start time: 2015/07/17 at 13:31:04:547
		if ((results = processTagLineOptional(T_1CISTARTTIME)) != null) {
			String dateTimeString = results.getTokenValue(START_TIME);
			if (dateTimeString != null) {
				SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd 'at' HH:mm:ss:SSS");
				Date startDate = sdf.parse(dateTimeString, new ParsePosition(0));
				if (startDate != null) {
					fRuntimeBuilder.setStartTime(startDate.getTime());
				}
			}
		}
		
		// 1CISTARTNANO   JVM start nanotime: 3534023113503
		if ((results = processTagLineOptional(T_1CISTARTNANO)) != null) {
			String nanoTimeString = results.getTokenValue(START_NANO);
			if (nanoTimeString != null) {
				fRuntimeBuilder.setStartTimeNanos(Long.parseLong(nanoTimeString));
			}
		}
		
		if ((results = processTagLineOptional(T_1CIPROCESSID)) != null) {
			String pidStr = results.getTokenValue(PID_STRING);
			if( pidStr != null ) {
				fImageBuilder.getCurrentAddressSpaceBuilder().getCurrentImageProcessBuilder().setID(pidStr);
			}
		}
		
		String cmdLine = null;
		if ((results = processTagLineOptional(T_1CICMDLINE)) != null) {
			cmdLine = results.getTokenValue(CMD_LINE);
			if ("[not available]".equals(cmdLine)) {
				cmdLine = null;
			} else {
				fImageProcessBuilder.setCommandLine(cmdLine);
			}
		}
		String home = null;
		if ((results = processTagLineOptional(T_1CIJAVAHOMEDIR)) != null) {;
			home = results.getTokenValue(ARG_STRING);
		}
		String dlldir = null;
		if ((results = processTagLineOptional(T_1CIJAVADLLDIR)) != null) {;
			dlldir = results.getTokenValue(ARG_STRING);
		}
		if (cmdLine != null) {
			if (dlldir != null) {
				// See if the command line starts with part of Java directory - if so add rest of path
				// Find position of first path separator
				int min1 = cmdLine.indexOf('/') + 1;
				if (min1 == 0) min1 = cmdLine.length();
				int min2 = cmdLine.indexOf('\\') + 1;
				if (min2 == 0) min2 = cmdLine.length();
				int min = Math.min(min1, min2);
				for (int i = cmdLine.length(); i > min; --i) {
					String prefix = cmdLine.substring(0, i);
					int j = dlldir.indexOf(prefix);
					if (j >= 0) {
						cmdLine = dlldir.substring(0, j) + cmdLine;
						break;
					}
				}
			}
			// Allow for spaces in the path by skipping over spaces in javadlldir
			int i = 0;
			if (dlldir != null) {
				for (i = dlldir.length(); i >= 0 ; --i) {
					if (cmdLine.startsWith(dlldir.substring(0, i))) {
						break;
					}
				}
			}
			// Look for the rest of the command line
			int sp = cmdLine.indexOf(' ', i);
			String exec;
			if (sp >= 0) {
				exec = cmdLine.substring(0, sp);
			} else {
				exec = cmdLine;
			}
			ImageModule execMod = fImageProcessBuilder.addLibrary(exec);
			fImageProcessBuilder.setExecutable(execMod);
		}
		processTagLineOptional(T_1CISYSCP);
	}

	private String buildVersionString(String javaversion,
			IAttributeValueMap results) {
		String extra = results.getTokenValue(ARG_STRING);
		if (extra != null) {
			if (javaversion != null) {
				javaversion = javaversion + "\n" + extra;
			} else {
				javaversion = extra;
			}
		}
		return javaversion;
	}
	
	/**
	 * Parse the user args information (1CIUSERARGS and 2CIUSERARG lines) 
	 * @throws ParserException
	 */
	private void parseUserArgs() throws ParserException {
		IAttributeValueMap results = null;
		
		if ((results = processTagLineRequired(T_1CIUSERARGS)) != null) {
			boolean added = false;
			while((results = processTagLineOptional(T_2CIUSERARG)) != null) {
				if (!added) {
					// Delay creating init args until we find some args
					added = true;
					try {
						fRuntimeBuilder.addVMInitArgs();
					} catch (BuilderFailureException e) {
						handleError("Failed to add JavaVMInitArgs to builder: ", e);
					}
				}
				String argString = results.getTokenValue(ARG_STRING);
				long extraInfo = results.getLongValue(ARG_EXTRA);
				try {
					if (extraInfo == IBuilderData.NOT_AVAILABLE) {
						if (argString != null) {
							fRuntimeBuilder.addVMOption(argString);
						}
					} else {
						fRuntimeBuilder.addVMOption(argString, extraInfo);
					}
				} catch (BuilderFailureException e) {
					handleError("Failed to add VM option to builder: " + argString + " ", e);
				}
			}
		}
	}

	/**
	 * Parse (and ignore) the JVMMI information (1CIJVMMI lines) 
	 * @throws ParserException
	 */
	private void parseJVMMI() throws ParserException {
		
		// Process and ignore lines
		processTagLineOptional(T_1CIJVMMI);
		processTagLineOptional(T_2CIJVMMIOFF);
	}
		
	/**
	 * Parse the user args information (1CIUSERARGS and 2CIUSERARG lines) 
	 * @throws ParserException
	 */
	private void parseEnvironmentVars() throws ParserException {
		IAttributeValueMap results = null;
		
		results = processTagLineOptional(T_1CIENVVARS);
		if (results != null) {
			while ((results = processTagLineOptional(T_2CIENVVAR)) != null) {
				String env_name = results.getTokenValue(ENV_NAME);
				String env_value = results.getTokenValue(ENV_VALUE);
				if (env_name != null && env_value != null) {
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
