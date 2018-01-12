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
package com.ibm.dtfj.javacore.parser.j9;

import java.io.IOException;
import java.util.Iterator;
import java.util.List;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.javacore.builder.BuilderFailureException;
import com.ibm.dtfj.javacore.builder.IImageAddressSpaceBuilder;
import com.ibm.dtfj.javacore.builder.IImageBuilder;
import com.ibm.dtfj.javacore.builder.IImageBuilderFactory;
import com.ibm.dtfj.javacore.builder.IImageProcessBuilder;
import com.ibm.dtfj.javacore.parser.framework.parser.IErrorListener;
import com.ibm.dtfj.javacore.parser.framework.parser.ILookAheadBuffer;
import com.ibm.dtfj.javacore.parser.framework.parser.IParserController;
import com.ibm.dtfj.javacore.parser.framework.parser.ISectionParser;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.framework.scanner.IScannerManager;
import com.ibm.dtfj.javacore.parser.framework.scanner.ScannerException;
import com.ibm.dtfj.javacore.parser.j9.section.common.ICommonTypes;

public class ParserController implements IParserController {
	
	
	private List fFramework;
	private IErrorListener fListener;
	private IImageBuilderFactory fImageBuilderFactory;
	private static final String DEFAULT_IMAGE_BUILDER = "default_image_builder";
	private static final String DEFAULT_IMAGE_ADDRESS_SPACE_BUILDER = "default_image_address_space_builder";
	private static final String DEFAULT_IMAGE_PROCESS_BUILDER = "default_image_process_builder";
	private static final String DEFAULT_JAVA_RUNTIME_BUILDER = "default_java_runtime_builder";

	
	
	/**
	 * 
	 * @param framework
	 */
	public ParserController(List framework, IImageBuilderFactory imageBuilderFactory) throws ParserException {
		if (imageBuilderFactory == null) {
			throw new ParserException("Must pass a valid image builder factory");
		}
		fFramework = framework;
		fImageBuilderFactory = imageBuilderFactory;
	}
	
	
	/**
	 * Support for one image builder parsing javacore data for only one runtime.
	 * Multiple runtime support will require overriding this method in a subclass.
	 */
	public Image parse(IScannerManager scannerManager) throws ParserException {
		ILookAheadBuffer lookAhead = scannerManager.getLookAheadBuffer();
		// For error reporting
		StringBuffer sb = new StringBuffer();
		try {
			for (int i = 1; i <= 2 && i <= lookAhead.maxDepth(); ++i) {
				IParserToken token = lookAhead.lookAhead(i);
				if (token != null) {
					sb.append(token.getValue());
				}
			}
		} catch (IOException e) {
		} catch (IndexOutOfBoundsException e) {
		} catch (ScannerException e) {
			throw new ParserException(e);
		}
		// Don't get too much data
		sb.setLength(Math.min(300, sb.length()));
		String first = sb.toString();

		IImageBuilder imageBuilder = null;
		try {
			imageBuilder = generateImageBuilder();
		} catch (BuilderFailureException e) {
			throw new ParserException(e);
		}
	
		boolean anyMatched = false;
		
		try {
			lookAhead.init();
			for (Iterator it = fFramework.iterator(); it.hasNext();) {
				processUnknownData(lookAhead);
				ISectionParser sectionParser = (ISectionParser) it.next();
				sectionParser.readIntoDTFJ(lookAhead, imageBuilder);

				/*
				 * Retrieve the error log.
				 */
				Iterator errors = sectionParser.getErrors();
				if ((fListener != null) && errors.hasNext()) {
					while(errors.hasNext()) {
						fListener.handleEvent(errors.next().toString());
					}
				}
				anyMatched |= sectionParser.anyMatched();
			}
		} catch (IOException e) {
			/*
			 * If IO exception encountered, parser may have reached a non-recoverable state, so for now no
			 * point in continuing the parsing process.
			 */
			e.printStackTrace();
		} catch (RuntimeException e) {
			/*
			 * For internal tracing purposes (Eclipse catches runtime exceptions,
			 * and they get hard to track even with the eclipse error log view).
			 */
			e.printStackTrace();
			throw e;
		} catch (ScannerException e) {
			throw new ParserException(e);
		}	
		
		if (!anyMatched) {
			throw new ParserException("Not a javacore file. First line: "+first);
		}
		return imageBuilder.getImage();
	}

	
	
	/**
	 * Will consume unknown data until a recognised section tag is reached. Common tags are also consumed.
	 * 
	 * @param lookAheadBuffer
	 * @throws ScannerException 
	 */
	private void processUnknownData(ILookAheadBuffer lookAheadBuffer) throws IOException, ScannerException {
		J9TagManager tagManager = J9TagManager.getCurrent();
		boolean stop = false;
		while (!lookAheadBuffer.allConsumed() && !stop) {
			IParserToken token = lookAheadBuffer.lookAhead(1);
			if (token != null) {
				String type = token.getType();
				if (!tagManager.hasTag(type) || tagManager.isTagInSection(type, ICommonTypes.COMMON)) {
					lookAheadBuffer.consume();
				}
				else {
					stop = true;
				}
			}
			else {
				lookAheadBuffer.consume();
			}
		}
	}
	
	
	/**
	 * 
	 * 
	 * @throws BuilderFailureException
	 */
	private IImageBuilder generateImageBuilder() throws BuilderFailureException {
		IImageBuilder imageBuilder = fImageBuilderFactory.generateImageBuilder(DEFAULT_IMAGE_BUILDER);
		IImageAddressSpaceBuilder addressSpace = imageBuilder.generateAddressSpaceBuilder(DEFAULT_IMAGE_ADDRESS_SPACE_BUILDER);
		IImageProcessBuilder processBuilder = addressSpace.generateImageProcessBuilder(DEFAULT_IMAGE_PROCESS_BUILDER);
		processBuilder.generateJavaRuntimeBuilder(DEFAULT_JAVA_RUNTIME_BUILDER);
		return imageBuilder;
	}

	

	
	/**
	 *
	 * @see com.ibm.dtfj.javacore.parser.framework.parser.IParserController#addErrorListener(com.ibm.dtfj.javacore.parser.framework.parser.IErrorListener)
	 */
	public void addErrorListener(IErrorListener listener) {
		fListener = listener;
	}
	




}
