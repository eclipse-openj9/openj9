/*[INCLUDE-IF Sidecar16]*/
/*******************************************************************************
 * Copyright (c) 2009, 2009 IBM Corp. and others
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

package com.ibm.tools.attach.target;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;

/**
 * 
 * Manage socket communications between an attacher and a target VM.
 *
 */
public final class AttachmentConnection {

	private static final String STREAM_ENCODING = "UTF8"; //$NON-NLS-1$

	/**
	 * read until null byte found
	 * @param channel input stream
	 * @param dataLimit Maximum number of bytes to receive.  0 indicates unlimited.
	 * @param requireNull true if the message must be null-terminated.  Use false if you are re-parsing an already received buffer.
	 * @return array of bytes, not including null termination
	 * @throws IOException if file closed before null found or excessive data received
	 */
	
	static byte[] streamReceiveBytes(InputStream channel, int dataLimit, boolean requireNull) throws IOException  {
		final int bufferLen = 100; /* most messages are short */
		byte [] msgBuff = new byte[bufferLen];
		ByteArrayOutputStream msg = new ByteArrayOutputStream(bufferLen);
		boolean unlimited = (0 == dataLimit);
		
		if (null == channel) {
			/*[MSG "K0575", "channel is null"]*/
			throw new IOException(com.ibm.oti.util.Msg.getString("K0575")); //$NON-NLS-1$
		}
		boolean done = false;
		while (!done && (unlimited || (dataLimit > msg.size()))) {	
			int	nRead = channel.read(msgBuff, 0, bufferLen);
			if (nRead > 0) {
				if (msgBuff[nRead-1] == 0) { /* messages are terminated by a null */
					done = true;
					nRead--; /* strip the terminating null */
				}
				msg.write(msgBuff, 0, nRead);
			} else if (requireNull) {
				/*[MSG "K0571", "input stream closed"]*/
				throw new IOException(com.ibm.oti.util.Msg.getString("K0571")); /* premature close of the socket */ //$NON-NLS-1$
			} else {
				done = true;
			}
		}
		if (!done && (msg.size() > dataLimit)) {
			/*[MSG "K0572", "Message from target exceeds maximum length of {0} bytes"]*/
			throw new IOException(com.ibm.oti.util.Msg.getString("K0572", dataLimit)); //$NON-NLS-1$
		}
		return msg.toByteArray();
	}

	/**
	 * Read unlimited-length message until null byte or EOF found. Use this only if you trust the sender.
	 * @param channel input stream
	 * @param requireNull true if the message must be null-terminated.  Use false if you are re-parsing an already received buffer.
	 * @return array of bytes, not including null termination
	 * @throws IOException if file closed before null found or excessive data received
	 */
	
	static byte[] streamReceiveBytes(InputStream channel, boolean requireNull) throws IOException  {
		return streamReceiveBytes(channel, 0, requireNull);
	}
	/**
	 * writes the data with a null termination
	 * @param channel output channel
	 * @param data data to write
	 * @throws IOException if channel not open
	 */
	public static void streamSend(OutputStream channel, String data) throws IOException {
		IPC.logMessage("streamSend "+data); //$NON-NLS-1$
		if (null == channel) {
			throw new IOException();
		}
		channel.write(data.getBytes(STREAM_ENCODING));
		channel.write(0);
		channel.flush();
	}

	/**
	 * read until null byte found
	 * @param channel input stream
	 * @param dataLimit maximum number of bytes to receive.  0 denotes unlimited.
	 * @return string with entire message
	 * @throws IOException if file closed before null found
	 */
	public static String streamReceiveString(InputStream channel, int dataLimit) throws IOException  {
		byte[] msgBuff = streamReceiveBytes(channel, dataLimit, true);
		String message = bytesToString(msgBuff);
		return message;
	}

	/**
	 * Read unlimited-length message. Use this only if you trust the sender.
	 * @param channel input stream
	 * @return string with entire message
	 * @throws IOException if file closed before null found
	 */
	public static String streamReceiveString(InputStream channel) throws IOException {
		return streamReceiveString(channel, 0);
	}

	/**
	 * Convert a byte buffer to a string with the preferred encoding
	 * @param msgBuff raw bytes
	 * @return equivalent string
	 * @throws UnsupportedEncodingException
	 */
	static String bytesToString(byte[] msgBuff) throws UnsupportedEncodingException {
		String message = new String(msgBuff, STREAM_ENCODING);
		return message;
	}

}
