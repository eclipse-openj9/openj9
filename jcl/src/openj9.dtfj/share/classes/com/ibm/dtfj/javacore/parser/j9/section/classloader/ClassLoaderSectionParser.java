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
package com.ibm.dtfj.javacore.parser.j9.section.classloader;

import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IBuilderData;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.builder.IJavaRuntimeBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.framework.scanner.IGeneralTokenTypes;
import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.SectionParser;

/**
 * 
 *
 */
public class ClassLoaderSectionParser extends SectionParser implements IClassLoaderTypes{

	private IJavaRuntimeBuilder fRuntimeBuilder;
	private IImageProcessBuilder fImageProcessBuilder;

	public ClassLoaderSectionParser() {
		super(CLASSLOADER_SECTION);
	}
	
	/**
	 * 
	 */
	protected void topLevelRule() throws ParserException {
		fImageProcessBuilder = fImageBuilder.getCurrentAddressSpaceBuilder().getCurrentImageProcessBuilder();
		fRuntimeBuilder = fImageProcessBuilder.getCurrentJavaRuntimeBuilder();
		classLoaderSummaries();
		classLoaderLibraries();
		classLoaders();
	}
	
	/**
	 * 
	 * @throws ParserException
	 */
	private void classLoaderSummaries() throws ParserException {
		/*
		 * 1CLTEXTCLLOS and 1CLTEXTCLLSS contain no meaningful data. Process and ignore.
		 */
		processTagLineRequired(T_1CLTEXTCLLOS);
		processTagLineRequired(T_1CLTEXTCLLSS);	
		IAttributeValueMap results = null;
		if ((results = processTagLineRequired(T_2CLTEXTCLLOADER)) != null) {
			processClassLoaderStats(results);
			while ((results = processTagLineOptional(T_2CLTEXTCLLOADER)) != null) {
				processClassLoaderStats(results);
			}
		}
	}
	
	/**
	 * Loaded libraries data may not be found in Sovereign VMs, therefore make
	 * it optional, even though it is a standard tag in later J9 VMs.
	 * @throws ParserException
	 */
	private void classLoaderLibraries() throws ParserException {
		if (matchOptional(T_1CLTEXTCLLIB)) {
			consume();
			/*
			 * Consume whatever other data is in the same line, as it's just header data.
			 */
			IParserToken token = lookAhead(1);
			if (token != null && token.getType().equals(IGeneralTokenTypes.UNPARSED_STRING)) {
				consume();
			}
			IAttributeValueMap results = null;
			if ((results = processTagLineRequired(T_2CLTEXTCLLIB)) != null) {
				processLibraryLoader(results);
				while ((results = processTagLineOptional(T_2CLTEXTCLLIB)) != null) {
					processLibraryLoader(results);
				}
			}
		}
	}

	/**
	 * 
	 * @throws ParserException
	 */
	private void classLoaders() throws ParserException {
		processTagLineRequired(T_1CLTEXTCLLOD);
		IAttributeValueMap results = null;
		if ((results = processTagLineRequired(T_2CLTEXTCLLOAD)) != null) {
			processClassLoaders(results);
			while ((results = processTagLineOptional(T_2CLTEXTCLLOAD)) != null) {
				processClassLoaders(results);
			}
		}
	}
	
	/**
	 * 
	 * @param results
	 * @throws ParserException
	 */
	private void processClassLoaderStats(IAttributeValueMap results) throws ParserException {
		String cl_name = results.getTokenValue(CL_ATT__NAME);
		long cl_address = results.getLongValue(CL_ATT_ADDRESS);
		long cl_object_address = results.getLongValue(CL_ATT_SHADOW_ADDRESS);
		if (cl_object_address == IBuilderData.NOT_AVAILABLE) cl_object_address = cl_address;
		try {
			fRuntimeBuilder.addClassLoader(cl_name, cl_address, cl_object_address);
		} catch (BuilderFailureException e) {
			handleError("Failed to add class loader: " + cl_name + " " + cl_address + " ", e);
		}
		
		// listing a parent class loader may be optional, therefore do a check
		// that parent class loader attributes are present before catching
		// any possible exceptions.
		cl_name = results.getTokenValue(CL_ATT_PARENT_NAME);
		cl_address = results.getLongValue(CL_ATT_PARENT_ADDRESS);
		if (cl_name != null || cl_address != IBuilderData.NOT_AVAILABLE) {
			if (cl_address == 0 && ClassLoaderPatternMatchers.none.reset(cl_name).matches()) {
				// Don't register class loader of type *none* and address 0 as it is not a real class loader
			} else {
				try {
					fRuntimeBuilder.addClassLoader(cl_name, cl_address, cl_address);
				} catch (BuilderFailureException e) {
					handleError("Failed to add class loader: " + cl_name + " " + cl_address + " ", e);
				}
			}
		}

		/*
		 * T_3CLNMBRLOADEDLIB  and T_3CLNMBRLOADEDCL not in DTFJ, process and ignore
		 */
		processTagLineOptional(T_3CLNMBRLOADEDLIB);
		processTagLineOptional(T_3CLNMBRLOADEDCL);
	}
	
	/**
	 * Appears to be information only present in J9VMs.
	 * @param results
	 * @throws ParserException
	 */
	private void processLibraryLoader(IAttributeValueMap results) throws ParserException {
		String cl_name = results.getTokenValue(CL_ATT__NAME);
		long cl_address = results.getLongValue(CL_ATT_ADDRESS);
		try {
			fRuntimeBuilder.addClassLoader(cl_name, cl_address, cl_address);
		} catch (BuilderFailureException e) {
			handleError("Failed to add class loader: " + cl_name + " " + cl_address + " ", e);
		}
		
		results = null;
		if ((results = processTagLineRequired(T_3CLTEXTLIB)) != null) {
			fImageProcessBuilder.addLibrary(results.getTokenValue(CL_ATT_LIB_NAME));
			while ((results = processTagLineOptional(T_3CLTEXTLIB)) != null) {
				fImageProcessBuilder.addLibrary(results.getTokenValue(CL_ATT_LIB_NAME));
			}
		}
	}
	
	/**
	 * 
	 * Grammar subsection 2CLTEXTCLLOAD
	 * Although classes must be associated with classloaders in DTFJ, if a class loader
	 * is not built due to corruption of data, yet a class list still exists for it,
	 * the parser still has to process the class data from the buffer any way, 
	 * even though it may not be added to
	 * DTFJ by the builder.
	 */
	private void processClassLoaders(IAttributeValueMap results) throws ParserException{
		JavaClassLoader loader = null;
		String cl_name = results.getTokenValue(CL_ATT__NAME);
		long cl_address = results.getLongValue(CL_ATT_ADDRESS);
		try {
			loader = fRuntimeBuilder.addClassLoader(cl_name, cl_address, cl_address);
		} catch (BuilderFailureException e) {
			handleError("Failed to add class loader: " + cl_name + " " + cl_address + " ", e);
		}
		while((results = processTagLineOptional(T_3CLTEXTCLASS)) != null) {
			String className = results.getTokenValue(CLASS_ATT_NAME);
			long classID = results.getLongValue(CLASS_ATT_ADDRESS);
			try {
				fRuntimeBuilder.addClass(loader, className, classID, IBuilderData.NOT_AVAILABLE, null);
			} catch (BuilderFailureException e) {
				handleError("Failed to add class: " + className + " " + classID + " ", e);
			}
		}
	}
	
	/**
	 * Empty hook
	 */
	protected void sovOnlyRules(String startingTag) throws ParserException {	
	}
}
