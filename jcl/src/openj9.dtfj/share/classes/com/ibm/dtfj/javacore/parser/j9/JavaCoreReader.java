/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.io.SequenceInputStream;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.IllegalCharsetNameException;
import java.nio.charset.UnsupportedCharsetException;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.javacore.builder.IImageBuilderFactory;
import com.ibm.dtfj.javacore.parser.framework.parser.IErrorListener;
import com.ibm.dtfj.javacore.parser.framework.parser.IParserController;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.j9.registered.RegisteredComponents;

public class JavaCoreReader {

	private RegisteredComponents fComponents;
	private IImageBuilderFactory fImageBuilderFactory;
	
	public JavaCoreReader(IImageBuilderFactory imageBuilderFactory) {
		fImageBuilderFactory = imageBuilderFactory;
		fComponents = new RegisteredComponents();
	}


	
	/**
	 * 
	 * @param input
	 * 
	 * @throws UnsupportedSourceException
	 * @throws ParserException
	 * @throws IOException
	 */
	public Image generateImage(InputStream input) throws IOException {
		try {
			byte[] head = new byte[256];
			input.read(head);
			ByteArrayInputStream headByteStream = new ByteArrayInputStream(head);
			Charset cs = getJavaCoreCodePage(headByteStream);
			SequenceInputStream stream = new SequenceInputStream(headByteStream, input);
			// Use default charset if none found
			// Charset.defaultCharset is 5.0, so not usable for 1.4
			Reader reader = cs != null ? new InputStreamReader(stream, cs) : new InputStreamReader(stream);
			List frameworkSections = new DTFJComponentLoader().loadSections();
			IParserController parserController = new ParserController(frameworkSections, fImageBuilderFactory);
			parserController.addErrorListener(new IErrorListener() {
				private Logger logger = Logger.getLogger(ImageFactory.DTFJ_LOGGER_NAME);
				
				public void handleEvent(String msg) {
					//map errors onto Level.FINE so that DTFJ is silent unless explicitly changed
					logger.fine(msg);
				}
				
			});
			J9TagManager tagManager = J9TagManager.getCurrent();
			return parserController.parse(fComponents.getScannerManager(reader, tagManager));
		} catch (ParserException e) {
			IOException e1 = new IOException("Error parsing Javacore");
			e1.initCause(e);
			throw e1;
		}
	}

	private Charset getJavaCoreCodePage(ByteArrayInputStream input)	throws IOException {
		input.mark(256);
		Charset cs = null;
		try {
			byte[] headBytes = new byte[256];
			input.read(headBytes);
			ByteBuffer headByteBuffer = ByteBuffer.wrap(headBytes);
			String[] estimates = new String[] { 
				"IBM1047", // EBCDIC
				"UTF-16BE", // Multibyte big endian
				"UTF-16LE", // Multibyte Windows
				System.getProperty("file.encoding"),
				"IBM850", // DOS/ASCII/CP1252/ISO-8859-1/CP437 lookalike.
			};
			try {
				for (int i = 0; i < estimates.length && cs == null; i++) {
					cs = attemptCharset(headByteBuffer, Charset.forName(estimates[i]));
				}
			} catch (UnsupportedCharsetException e) {
				/* This JVM doesn't support the charset we've requested, so skip it */
			}
		} catch (JavacoreFileEncodingException e) {
			/* As it isn't possible to report the error through the API
			 * we have no choice but to silently consume it here. The 
			 * API will continue in a best effort basis.
			 */
			Logger.getLogger(com.ibm.dtfj.image.ImageFactory.DTFJ_LOGGER_NAME).log(Level.INFO,e.getMessage());
		} finally {
			input.reset();
		}
		return cs;
	}

	private class JavacoreFileEncodingException extends Exception {
		public JavacoreFileEncodingException(String string) {
			super(string);
		}
		public JavacoreFileEncodingException(String string,Exception exception) {
			super(string,exception);
		}
	}

	private Charset attemptCharset(ByteBuffer headByteBuffer, Charset trialCharset) throws JavacoreFileEncodingException {
		final String sectionEyeCatcher = "0SECTION";
		final String charsetEyeCatcher = "1TICHARSET";
		headByteBuffer.rewind();
		String head = trialCharset.decode(headByteBuffer).toString();
		
		/* If we can find the section eyecatcher, this encoding is mostly good */
		if (head.indexOf(sectionEyeCatcher) >= 0) {
			int idx = head.indexOf(charsetEyeCatcher);
			
			/* The charset eyecatcher is much newer, so may not be present */
			if (idx >= 0) {
				idx += charsetEyeCatcher.length();
				String javacoreCharset = head.substring(idx).trim();
				javacoreCharset = javacoreCharset.split("\\s+")[0];
				try {
					Charset trueCharset;
					try {
						trueCharset = Charset.forName(javacoreCharset);
					} catch (UnsupportedCharsetException c) {
						// Re-attempt to catch a common windows case where we get 1252 (or similar)
						// at it should be "cp1252".
						trueCharset = Charset.forName("cp"+javacoreCharset);
					}
					/*
					 * Do a quick sanity check for obvious cases where a javacore
					 * has been translated from e.g. EBCDIC to ASCII before getting
					 * to DTFJ.
					 */
					ByteBuffer sanityTrial = trialCharset.encode(sectionEyeCatcher);
					ByteBuffer sanityTrue = trueCharset.encode(sectionEyeCatcher);
					if (sanityTrial.equals(sanityTrue)) {
						return trueCharset;
					} else {
						throw new JavacoreFileEncodingException(
							"Ignoring Javacore encoding '" + javacoreCharset + "' hinted at by '" + 
							charsetEyeCatcher + "' eye catcher due to suspected change of encoding.\n" +
							"Eye catcher was readable using encoding '"	+ trialCharset.displayName() + "'."
						);
					}
				} catch (IllegalCharsetNameException e) {
					/*
					 * Keep going - it's possible that though the eyecatcher is
					 * legible in an attempted decoding, the real charset itself may
					 * only be readable in that same charset.
					 */
				} catch (UnsupportedCharsetException e) {
					/*
					 * Ah. Found a valid charset, this JVM can't deal with it
					 * though. We know we could interpret the eyecatcher at least
					 * though, so some characters of the trial charset map to
					 * meaningful code points. The best we can do will be if we use
					 * the trial charset, which to reach here must be supported.
					 */
					throw new JavacoreFileEncodingException(
							"Unable to use Javacore encoding '" + javacoreCharset + "' hinted at by '" + 
							charsetEyeCatcher + "' eye catcher as the JVM does not support this encoding.", e
						);			
				}
			} else {
				/* Couldn't find the charset eyecatcher, but the section eyecatcher worked */
				return trialCharset;
			}
		}
		return null;
	}
}
