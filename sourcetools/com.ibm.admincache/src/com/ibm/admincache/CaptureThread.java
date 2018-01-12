/*******************************************************************************
 * Copyright (c) 2006, 2017 IBM Corp. and others
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


/*
 * Created on Jan 18, 2005 by Ron Servant
 * Modified on Apr 5th->22nd, 2005 by Mike Fulton
 */
package com.ibm.admincache;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.OutputStream;

/**
 * @author Ron Servant
 * @author Mike Fulton
 * @author Mark Stoodley
 */
class CaptureThread extends Thread {
   // reader is the buffered reader used to read data from the stream
   private BufferedReader reader;
   private BufferedWriter writer;

   // record is the currently active record written/read by the producer and consumer
   private Record record;
   
   // verbose is true if a PrintStream is provided to write data to, 
   // false otherwise
   private boolean verbose;
   
   CaptureThread(InputStream in, OutputStream out) {
      reader = new BufferedReader(new InputStreamReader(in));
      writer = new BufferedWriter(new OutputStreamWriter(out));
      verbose = true;
   }
   CaptureThread(InputStream in) {
      reader = new BufferedReader(new InputStreamReader(in));
      writer = null;
      verbose = false;
   }

   // run simply reads a line at a time and writes the record.
   // when the last line of data is read, a special NULL record is 
   // written out so that the consumer knows there is no more data
   public void run() {
      try {
         String read;
         while ((read = reader.readLine()) != null) {
            writeRecord(read);
            String data = readRecord();
            processRecord(data);
         }
      } catch (IOException ioe) {
      }
      processRecord(null);
      writeRecord(null);
   }

   protected void processRecord(String data) {
	   if (data != null && verbose) {
		   if (data.startsWith("Could not create the Java virtual machine"))
       			return;
		   
		   boolean ignore = false;
		   int start = data.indexOf("JVMSHRC");
		   if (start >= 0) {
			   int codeStart = start + "JVMSHRC".length();
			   int code = Integer.parseInt(data.substring(codeStart,codeStart+3));
			   char messageType = data.charAt(codeStart+3);
			   final char informationalMessage = 'I';
			   final char errorMessage = 'E';
			   if (code == 96 && messageType == informationalMessage) {
				   SharedCacheConfig.setFullCacheMessageFound(true);
				   ignore = true;
			   }
			   if (SharedCacheConfig.populating()) {
				   if (code == 23 && messageType == errorMessage)
					   ignore = true;
				   if (code == 256 && messageType == informationalMessage)
					   ignore = true;
			   }
			   
			   if (messageType == 'E' && !ignore)
				   SharedCacheConfig.setCommandError(true);
		   }
		   
		   if (!ignore) {
			   try {
				   writer.write(data);
				   writer.write(System.getProperty("line.separator"));
				   writer.flush(); 
               }
			   catch (IOException ioe) {
			   }
		   }
	   }
   }

   // see if the record is not NULL. If so, the previous record has 
   // not been read yet, so return false (record not written).
   // If the record is NULL (i.e. the previous record has been processed)
   // then create a new record object and return true (record written)
   private synchronized boolean tryToWriteRecord(String data) {
      if (record != null) {
         return false;
      } else {
         record = new Record(data);
         return true;
      }
   }

   // write a record out by repeatedly trying to write the record,
   // waiting until the reader has read it
   public void writeRecord(String data) {
      while (!tryToWriteRecord(data)) {
      }
   }

   // see if the record is NULL. If it is, there is no
   // record to grab, so return a NULL
   // record so the caller knows to retry.
   // If the record is not NULL (i.e. there is data to get),
   // then grab the record into a temp, clear the record to NULL
   // and return.
   private synchronized Record tryToReadRecord() {
      if (record == null) {
         return null;
      } else {
         Record temp = record;
         record = null;
         return temp;
      }
   }

   // read the record by repeatedly trying to read it until
   // a record is found. The last record will have an underlying
   // String that is NULL, which the caller of reader() is looking
   // for to stop reading.
   public String readRecord() {
      Record record;
      while ((record = tryToReadRecord()) == null) {
      }
      return record.getData();
   }

}
