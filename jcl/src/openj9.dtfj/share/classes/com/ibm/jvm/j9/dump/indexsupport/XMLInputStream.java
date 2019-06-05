/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
package com.ibm.jvm.j9.dump.indexsupport;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.UnsupportedEncodingException;
import java.util.Stack;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.corereaders.ResourceReleaser;

/**
 * CMVC 154851 : class created
 * This is a SAX 'lite' input stream whose primary purpose is to clean up
 * xml being allowing to be read by the SAX parser in DTFJ. It currently performs
 * the following 
 * 
 * 1. Automatically closes any missing tags which can be as a result of jextract not properly closing tags when invalid data is encountered.
 *    
 * @author Adam Pilkington
 *
 */
public class XMLInputStream extends InputStream implements ResourceReleaser {
	private InputStream in = null;												//underlying input stream to wrap
	private static final int BUFFER_SIZE = 4096;								//size of buffer to use when reading from the wrapped stream
	private char[] buffer = new char[BUFFER_SIZE];								//buffer to read data from the underlying stream into
	private InputStreamReader reader = null;									//reader to convert incoming bytes into chars 
	private OutputStreamWriter writer = null;									//writer for outgoing chars to the same charset as they were read in under
	private int index = 0;														//index within the buffer, default to past the end of the buffer to force a read
	private int indexMax = 0;													//the maximum amount of data available in the buffer
	private boolean eos = false;												//end-of-stream marker
	private static final String CHARSET_NAME = "UTF-8";							//the charset for jextract xml files
	private int[] utf8Data = new int[3];										//utf 8 encoded string
	private int utf8Index = 0;													//current pointer within the UTF-8 index
	private int utf8MaxIndex = -1;												//the total number of items in the UTF 8 data buffer
	
	private static final int STATE_START_TAG = 1;								//currently parsing a start tag
	private static final int STATE_CLOSE_TAG = 2;								//parsing a close tag
	private static final int STATE_CONTENTS = 3;								//reading tag contents
	private static final int STATE_ATTRIBUTES = 4;								//reading tag attributes
	private static final int STATE_ATTRIBUTE_CONTENTS = 5;						//reading contents of an attribute
	private static final int STATE_INSERT_MISSING_CLOSING_TAG = 6;				//inserting a missing tag
	private static final int STATE_INSERT_MISSING_CLOSING_TAG_ROOT = 7;			//inserting a missing tag
	private int state = STATE_CONTENTS;											//current state of the stream
	private Stack tags = new Stack();											//stack of encountered tags
	private StringBuffer name = null;											//name of the tag currently being processed
	private String peekName = null;												//name of the tag peeked from the stream
	private StringBuffer missingDataBuffer = null;								//buffer holding the missing tag data
	private int missingDataIndex = 0;											//index within the buffer
	
	private Logger log = Logger.getLogger(this.getClass().getName());			//logger for this class
	private boolean outputWritten = false;										//flag to indicate if the output has been written 
	private File temp = null;
	private FileOutputStream fout = null;
	
	/**
	 * Construct a new XML input stream
	 * @param in The underlying input stream to wrap
	 */
	public XMLInputStream(InputStream in) {
		log.fine("Cleaning xml input stream");
		this.in = in;
		try {
			reader = new InputStreamReader(in, CHARSET_NAME);
			writer = new OutputStreamWriter(new OutputStream() {
				public void write(int b) throws IOException {
					utf8Data[++utf8MaxIndex] = b;
				}
			}, CHARSET_NAME);
		} catch (UnsupportedEncodingException e) {
			log.log(Level.WARNING, "Could not create an " + CHARSET_NAME + " reader",e);
		}
		if(log.isLoggable(Level.FINEST)) {
			try {
				temp = File.createTempFile("dtfj", ".txt");
				fout = new FileOutputStream(temp);
			} catch (Exception e) {
				log.warning("Could not create temporary file to log the output");
			}
		}
	}
	
	/* (non-Javadoc)
	 * @see java.io.InputStream#read()
	 */
	public int read() throws IOException {
		if(reader == null) {		//no reader to convert the underlying bytes into chars so just delegate call
			return in.read();	
		}
		if(utf8Index <= utf8MaxIndex) {
			return utf8Data[utf8Index++];	//return from the UTF8 encoded data first before fetching new data
		}
		if(eos) {
			return -1;				//no data left to read from the underlying stream
		}
		char data = getData();		//read the next data item
		if(eos) {					//check not at end of stream
			return -1;
		}
		utf8MaxIndex = -1;			//reset the total number of items in the data buffer
		writer.write(data);			//convert character to underlying charset
		writer.flush();				//force char conversion
		utf8Index = 0;				//reset the current position in the data buffer
		if(log.isLoggable(Level.FINEST)) {		//log output received by the SAX parser
			fout.write(data);
		}
		return utf8Data[utf8Index++];
	}

	/**
	 * Get the next item of data, either from the underlying stream or the missing data buffer
	 * @return next character to return
	 * @throws IOException rethrow exceptions
	 */
	private char getData() throws IOException {
		char nextChar = readNextChar();		//get the next character to process
		if(eos) {
			return 0;					//no data left to be read from the underlying stream
		}
		switch(state) {					//how we process the data depend upon the current state
			case STATE_START_TAG :
				return doStartTag(nextChar);
			case STATE_INSERT_MISSING_CLOSING_TAG :
			case STATE_INSERT_MISSING_CLOSING_TAG_ROOT :
				return doMissingClosingTag(nextChar);
			case STATE_ATTRIBUTES :
				return doAttributes(nextChar);
			case STATE_ATTRIBUTE_CONTENTS :
				return doAttributeContents(nextChar);
			case STATE_CONTENTS :
				return doTagContents(nextChar);
			case STATE_CLOSE_TAG :
				return doCloseTag(nextChar);
			default :					//catch all to detect invalid states
				throw new IllegalStateException("An unknown state of " + state + " was encountered");
		}
	}
	
	/**
	 * This method is called when the state is STATE_CLOSE_TAG. It occurs when 
	 * a </ is detected in the stream
	 * @param data character to process
	 * @return the character to return to the stream consumer
	 * @throws IOException re-throw exceptions
	 */
	private char doCloseTag(char data) throws IOException {
		if(data == '>') {
			state = STATE_CONTENTS;
			doStackIntegrityCheck(tags.pop());
		} else {
			name.append(data);
		}
		return data;						//return the data unmodified
	}
	
	/**
	 * This method is called when the state is STATE_CONTENTS. It occurs when 
	 * the stream is moving over a tag's content
	 * @param data character to process
	 * @return the character to return to the stream consumer
	 * @throws IOException re-throw exceptions
	 */
	private char doTagContents(char data) throws IOException {
		if(data == '<') {	//found the start of another tag
			if(!tags.isEmpty()) {
				peekName = peekTagFromStream(' ');
				//CMVC 168448 : correctly handle comments and processing instructions when peeking the tag name 
				char checkInvalidTagNameChar = peekName.charAt(0);		//check the start character does not show that the tag is a comment or PI i.e. starts with ! of ?
				if(('!' == checkInvalidTagNameChar) || ('?' == checkInvalidTagNameChar)) {
					return data;
				}
				if(tags.peek().equals(peekName)) {		//if the tag to be added to the stack is the same as the top, it means the last one was not closed
					missingDataIndex = 0;			//reset data index
					missingDataBuffer = new StringBuffer();
					missingDataBuffer.append("/");
					missingDataBuffer.append(tags.peek());	//close this one off
					missingDataBuffer.append("><");
					tags.pop();								//remove the now closed tag from the stack of open tags
					state = STATE_INSERT_MISSING_CLOSING_TAG_ROOT;
					return data;
				}
			}
			state = STATE_START_TAG;
			name = new StringBuffer();
		}
		return data;						//return the data unmodified
	}
	
	/**
	 * This method is called when the state is STATE_ATTRIBUTE_CONTENTS. It occurs when 
	 * the stream is moving over a tag's attribute values
	 * @param data character to process
	 * @return the character to return to the stream consumer
	 * @throws IOException re-throw exceptions
	 */
	private char doAttributeContents(char data) throws IOException {
		if((data == '"') || (data == '\'')) {
			state = STATE_ATTRIBUTES;		//finished reading attribute
		}
		return data;						//return the data unmodified
	}
	
	/**
	 * This method is called when the state is STATE_INSERT_MISSING_CLOSING_TAG. It occurs when 
	 * a missing closing tag has been detected and one is being inserted into the stream.
	 * @param data character to process
	 * @return the character to return to the stream consumer
	 * @throws IOException re-throw exceptions
	 */
	private char doMissingClosingTag(char data) throws IOException {
		if(missingDataIndex == missingDataBuffer.length()) {	//inserted all required tags so switch back to normal processing
			switch(state) {
				case STATE_INSERT_MISSING_CLOSING_TAG :			//missing closing tag was detected when another closing tag was detected
					state = STATE_CLOSE_TAG;
					break;
				case STATE_INSERT_MISSING_CLOSING_TAG_ROOT :	//missing closing tag was detected when the same tag was added again and so are now processing attributes
					state = STATE_START_TAG;
					name = new StringBuffer();
					break;
				default :
			}
						
		}
		return data;							//return the data unmodified
	}
	
	/**
	 * This method is called when the state is STATE_ATTRIBUTES. It occurs when 
	 * the stream is moving over the attributes of the current tag. One important
	 * function of this method is to detect self closing tags i.e. <name />
	 * @param data character to process
	 * @return the character to return to the stream consumer
	 * @throws IOException re-throw exceptions
	 */
	private char doAttributes(char data) throws IOException {
		switch(data) {
			case '/' :	//this is a self closing tag
				state = STATE_CONTENTS;
				doStackIntegrityCheck(tags.pop());	//pop tag from the stack
				break;
			case '>' : //end of the tag
				state = STATE_CONTENTS;
				break;
			case '"' :
			case '\'' :
				state = STATE_ATTRIBUTE_CONTENTS;	//reading attribute contents
				break;
			default :
				//ignore attributes
				break;
		}
		return data;						//return the data unmodified
	}

	/**
	 * This method is called when the state is STATE_START_TAG. It occurs when 
	 * a < character is the next character to process.
	 * @param data character to process
	 * @return the character to return to the stream consumer
	 * @throws IOException re-throw exceptions
	 */
	private char doStartTag(char data) throws IOException {
		String tag = null;							//tag currently being processed
		switch(data) {
			case ' ' :								//end of the tag name
				tag = name.toString();
				tags.push(tag);
				state = STATE_ATTRIBUTES;
				break;
			case '/' : 								//this is actually a closing tag
				peekName = peekTagFromStream('>');
				if(peekName.equals(tags.peek())) {	//tags match so carry on processing
					state = STATE_CLOSE_TAG;
				} else {
					missingDataIndex = 0;			//reset data index
					missingDataBuffer = new StringBuffer();
					do {
						tag = (String) tags.pop();
						missingDataBuffer.append(tag);
						missingDataBuffer.append("></");
					} while(!tags.peek().equals(peekName));
					state = STATE_INSERT_MISSING_CLOSING_TAG;
				}
				break;
			case '?' : //this is a processing instruction so ignore it
			case '!' : //this is a comment so ignore it
				state = STATE_CONTENTS;
				break;
			default :	//store the data as part of the name
				name.append(data);
				break;
		}
		return data;						//return the data unmodified
	}
	
	/**
	 * Reads the next character from the appropriate data source. This can be the buffer associated with the underlying
	 * input stream or an alternative source applicable to the current state.
	 * @return
	 */
	private char readNextChar() throws IOException {
		switch (state) {
			case STATE_INSERT_MISSING_CLOSING_TAG :
			case STATE_INSERT_MISSING_CLOSING_TAG_ROOT :
				return missingDataBuffer.charAt(missingDataIndex++);
			default :
				return readFromBuffer();
		}
	}
	
	/**
	 * Allows the process to peek ahead in the stream and see what the next closing tag is.
	 * It handles peeks beyond the end of the current buffer contents.
	 * @param the delimiter which indicates the end of the tag name
	 * @return the name of the tag found
	 * @throws IOException re-throw exceptions.
	 */
	private String peekTagFromStream(char delimeter) throws IOException {
		try {
		int peek = index;													//peek index is initialized to the current index
		String tag = (String)tags.peek();
		int unreadDataLength = availableBufferDataLength();					//see how much data is left in the buffer
		if(tag.length() > unreadDataLength) {								//not enough data in the buffer
			System.arraycopy(buffer, index, buffer, 0, unreadDataLength);	//move the outstanding data to the front
			putDataIntoBuffer(unreadDataLength);							//refill the buffer starting from the end of the preserved content
			peek = 0;														//reset the peek to start at the beginning of the buffer
		}
		while((peek < indexMax) && ((char)buffer[peek] != delimeter)) {		//scan for the specified delimeter
			peek++;
		}
		return new String(buffer, index, peek - index);
		} catch (Exception e) {
			log.log(Level.SEVERE, "Failed to peek the next tag from the stream", e);
			return "";
		}
	}
	
	/**
	 * Performs an integrity check to make sure that the item being popped from the stack
	 * corresponds to the tag which is currently being processed
	 * @param popped
	 */
	private void doStackIntegrityCheck(Object popped) {
		String currentTag = name.toString();
		if(!currentTag.equals(popped)) {
			log.severe("Tag mismatch : processing " + currentTag + " but stack top is " + popped);
		}
	}
	
	/**
	 * How much unread data is left in the buffer
	 * @return number of remaining bytes
	 */
	private int availableBufferDataLength() {
		return indexMax - index;
	}
	
	/**
	 * Reads data from the buffer. When the buffer is exhausted then data is read from the underlying
	 * input stream.
	 * @return the next item in the buffer
	 * @throws IOException re-throw exceptions
	 */
	private char readFromBuffer() throws IOException {
		if(index == indexMax) {							//need to read some more data into the buffer
			putDataIntoBuffer(0);
			if(eos) {
				return 0;
			}
		}
		return buffer[index++];							
	}

	/**
	 * Puts data into the buffer from the underlying data stream. It allows the preservation of the first part of the
	 * buffer so that new data can be appended
	 * @param startIndex the index at which point to start reading the data into the buffer
	 * @throws IOException
	 */
	private void putDataIntoBuffer(int startIndex) throws IOException {
		int count = reader.read(buffer, startIndex, BUFFER_SIZE - startIndex);
		if(count == -1) {
			eos = true;
			indexMax = -1;
		} else {
			indexMax = startIndex + count;
			index = 0;									//reset the index
		}
	}
	
	/* (non-Javadoc)
	 * @see java.io.InputStream#close()
	 */
	public void close() throws IOException {
		in.close();
		if(log.isLoggable(Level.FINEST) && !outputWritten) {
			fout.close();
			log.finest("Output sent to file " + temp.getAbsolutePath() + "\n");
			outputWritten = true;
		}
	}

	/* (non-Javadoc)
	 * @see java.io.InputStream#markSupported()
	 */
	public boolean markSupported() {
		return false;									//marking is not supported by this input stream
	}

	public void releaseResources() {
		try {
			close();
		} catch (IOException e) {
			Logger.getLogger(com.ibm.dtfj.image.ImageFactory.DTFJ_LOGGER_NAME).log(Level.WARNING,e.getMessage(),e);
		}
	}
}
