/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2019 IBM Corp. and others
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
package com.ibm.dtfj.utils.file;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.charset.IllegalCharsetNameException;
import java.nio.charset.UnsupportedCharsetException;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.MemoryCacheImageInputStream;

/**
 * Sniffs files attempting to determine the contents, typically from known 
 * magic numbers/values.
 * 
 * @author adam
 *
 */
public class FileSniffer {
	//ELF identifier
	private static final int ELF = 0x7F454C46;		//0x7F,E,L,F
	
	//minidump identifier
	private static final int MINIDUMP = 0x4D444D50;	//MDMP
	
	//userdump identifier
	private static final int USERDUMP_1 = 0x55534552;
	private static final int USERDUMP_2 = 0x44554D50;
	
	//DR1 and DR2 are z/OS core file identifiers
    private static final int DR1 = 0xc4d9f140;
    private static final int DR2 = 0xc4d9f240;
    
    //XCOFF identifiers
	private static final int CORE_DUMP_XX_VERSION = 0xFEEDDB2;
	private static final int CORE_DUMP_X_VERSION = 0xFEEDDB1;

	//MACHO identifiers
	private static final int MACHO_64 = 0xFEEDFACF;
	private static final int MACHO_64_REV = 0xCFFAEDFE;
    
    private static int[] coreid = new int[]{ELF, MINIDUMP, DR1, DR2};
    
    //PHD identifier
    private static final String PHD_HEADER = "portable heap dump";
    private static final int PHD_HEADER_SIZE = PHD_HEADER.length() + 2;		//UTF-8 string so need to add 2 length bytes
    
    //zip file identifier
    private static final int ZIP_ID = 0x04034b50;
    
    //the format for a core file
    public enum CoreFormatType {
		ELF,
		MINIDUMP,
		MVS,
		XCOFF,
		USERDUMP,
		MACHO,
    	UNKNOWN
    }
    
    /**
     * Determine the format of the core file.
     * 
     * @param iis stream to the core file
     * @return format
     * @throws IOException re-thrown
     */
    public static CoreFormatType getCoreFormat(ImageInputStream iis) throws IOException {
		try {
			int header = iis.readInt();
			int header2 = iis.readInt();
			if(header == MINIDUMP) {
				return CoreFormatType.MINIDUMP;
			}
			if(header == USERDUMP_1) {
				//because the userdump id spans 8 bytes need to look at the second header now
				if(header2 == USERDUMP_2) {
					return  CoreFormatType.USERDUMP;
				}
			}
			if(header == ELF && isElfCoreFile(iis)) {
				return CoreFormatType.ELF;
			}
			if((header == DR1) || (header == DR2)) {
				return CoreFormatType.MVS;
			}
			//potential AIX core file version strings are at offset 4, so use the second header
			if((header2 == CORE_DUMP_X_VERSION) || (header2 == CORE_DUMP_XX_VERSION)) {
				return CoreFormatType.XCOFF;
			}
			if((header == MACHO_64) || (header == MACHO_64_REV)) {
				if (isMachCoreFile(iis, header)) {
					return CoreFormatType.MACHO;
				}
			}
			return CoreFormatType.UNKNOWN;
		} finally {
			iis.seek(0);		//do not close the stream but reset it back
		}		
	}
    
	public static boolean isCoreFile(InputStream in, long filesize) throws IOException {
		ImageInputStream iis = new MemoryCacheImageInputStream(in);
		return isCoreFile(iis, filesize);		
	}
	
	public static boolean isCoreFile(ImageInputStream iis, long filesize) throws IOException {
		try {
			int header = iis.readInt();
			int header2 = iis.readInt();
			for(int i = 0; i < coreid.length; i++) {
				if(header == coreid[i]) {
					if( header == ELF ) {
						return isElfCoreFile(iis);
					} else {
						return true;
					}
				}
			}
			//check for a windows user dump
			if(header == USERDUMP_1) {
				//because the userdump id spans 8 bytes need to look at the second header now
				if(header2 == USERDUMP_2) {
					return true;
				}
			}
			//potential AIX core file version strings are at offset 4, so use the second header
			if((header2 == CORE_DUMP_X_VERSION) || (header2 == CORE_DUMP_XX_VERSION)) {
				return true;
			}
			if((header == MACHO_64) || (header == MACHO_64_REV)) {
				if (isMachCoreFile(iis, header)) {
					return true;
				}
			}
			return false;
		} finally {
			iis.seek(0);		//do not close the stream but reset it back
		}		
	}
	
	/* Check that an elf file is definitely an elf core file.
	 * (It could also be a library, executable etc...)
	 */
	private static boolean isElfCoreFile(ImageInputStream iis) throws IOException {
		// First 16 bytes are e_ident, after that
		// e_type is a 16 bit value that tells us what type of elf file
		// this is.
		// #define ET_NONE         0               /* No file type */
		// #define ET_REL          1               /* Relocatable file */
		// #define ET_EXEC         2               /* Executable file */
		// #define ET_DYN          3               /* Shared object file */
		///#define ET_CORE         4               /* Core file */
		// Core files will have 4 in that space.
		// Before that byte 6 tells us if we are big or little endian, which we also need to know
		// as e_type is two bytes long.
		boolean isCore = false;
		ByteOrder originalOrder = iis.getByteOrder(); 
		iis.seek(5);
		byte order = iis.readByte();
		iis.seek(16);
		short type = 0;
		if( order == 1 ) {
			iis.setByteOrder(ByteOrder.LITTLE_ENDIAN);
			type = iis.readShort();
		} else if( order == 2 ) {
			iis.setByteOrder(ByteOrder.BIG_ENDIAN);
			type = iis.readShort();
		} else {
			// Invalid byte, not an Elf file. Don't set type.
		}
		if( type == 4 ) {
			isCore = true;
		}	 
		iis.setByteOrder(originalOrder);
		return isCore;
	}

	/* Check if file is a core file. The file header (found in
	 * /usr/include/mach-o/loader.h) has 'filetype' as the 4th member,
	 * and '#define MH_CORE 0x4' as the constant for core file type.
	*/
	private static boolean isMachCoreFile(ImageInputStream iis, int header) throws IOException {
		ByteOrder originalOrder = iis.getByteOrder();
		if (header == MACHO_64_REV) {
			iis.setByteOrder(iis.getByteOrder() == ByteOrder.BIG_ENDIAN ? ByteOrder.LITTLE_ENDIAN : ByteOrder.BIG_ENDIAN);
		}
		iis.seek(12);
		int filetype = iis.readInt();
		iis.setByteOrder(originalOrder);
		return filetype == 4;
	}
	
	public static boolean isJavaCoreFile(InputStream in, long filesize) throws IOException {
		ImageInputStream iis = new MemoryCacheImageInputStream(in);
		return isJavaCoreFile(iis, filesize);
	}
	
	public static boolean isJavaCoreFile(ImageInputStream iis, long filesize) throws IOException {
		try {
			byte[] headBytes = new byte[256];
			iis.read(headBytes);
			ByteBuffer headByteBuffer = ByteBuffer.wrap(headBytes);
			String[] estimates = new String[] { 
				"IBM1047", // EBCDIC
				"UTF-16BE", // Multibyte big endian
				"UTF-16LE", // Multibyte Windows
				System.getProperty("file.encoding"),
				"IBM850", // DOS/ASCII/CP1252/ISO-8859-1/CP437 lookalike.
			};
			try {
				Charset cs = null;
				for (int i = 0; i < estimates.length && cs == null; i++) {
					cs = attemptCharset(headByteBuffer, Charset.forName(estimates[i]));
				}
				return (cs != null);
			} catch (UnsupportedCharsetException e) {
				/* This JVM doesn't support the charset we've requested, so skip it */
				return false;
			}
		} finally {
			iis.seek(0);		//do not close the stream but reset it back
		}		
	}
	
	private static Charset attemptCharset(ByteBuffer headByteBuffer, Charset trialCharset) throws IOException {
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
					Charset trueCharset = Charset.forName(javacoreCharset);
					/*
					 * Check if trueCharset would have encoded the sectionEyeCatcher the
					 * same way.
					 */
					ByteBuffer sanityTrial = trialCharset.encode(sectionEyeCatcher);
					ByteBuffer sanityTrue = trueCharset.encode(sectionEyeCatcher);
					if (sanityTrial.equals(sanityTrue)) {
						// Encoding matches, the trueCharset from 1TICHARSET will be the best one to return.
						return trueCharset;
					}
				/*
				* Ignore exceptions, if the trueCharset from 1TICHARSET is illegal or unknown we
				* will return the trialCharset that read 0SECTION correctly.
				*/
				} catch (IllegalCharsetNameException e) {
				} catch (UnsupportedCharsetException e) {
				}
			}
			return trialCharset;
		}
		/* Failed to find section eyecatcher after decoding in this charset. */
		return null;
	}
	
	public static boolean isPHDFile(InputStream in, long filesize) throws IOException {
		ImageInputStream iis = new MemoryCacheImageInputStream(in);
		return isPHDFile(iis, filesize);	
	}
	
	public static boolean isPHDFile(ImageInputStream iis, long filesize) throws IOException {
		if(filesize < PHD_HEADER_SIZE) {
			return false;
		}
		try {
			String header = readUTF(iis, PHD_HEADER_SIZE);
			return header.equals(PHD_HEADER);
		} finally {
			iis.reset();		//do not close the stream but reset it back
		}		
	}
	
	private static String readUTF(ImageInputStream iis, int maxlen) throws IOException {
		int length = iis.readUnsignedShort();
		if((length > maxlen) && (maxlen > 0)) {
			length = maxlen;	//ensure that only maxlen bytes are read
		}
		byte[] buf = new byte[length];
		iis.readFully(buf);
		return new String(buf, "UTF-8");//AJ
	}
	
	/**
	 * Checks to see if the input stream is for a zip file, assumes that the stream is correctly positioned.
	 * This method will reset the stream if possible to it's starting position. The reason for using this
	 * method over just trying java.util.zip.ZipFile(File) and catching errors is that on some platforms
	 * the zip support is provided by native libraries. It is possible to cause the native library to 
	 * crash when speculatively passing files into it.
	 * 
	 * @param in stream to analyze
	 * @return true if it is a zip file
	 * @throws IOException 
	 */
	/*
	 * zip format
	 * 
	 A.  Local file header:

        local file header signature     4 bytes  (0x04034b50)
        version needed to extract       2 bytes
        general purpose bit flag        2 bytes
        compression method              2 bytes
        last mod file time              2 bytes
        last mod file date              2 bytes
	 */
	public static boolean isZipFile(ImageInputStream iis) throws IOException {
		ByteOrder byteOrder = iis.getByteOrder();
		try {
			iis.setByteOrder(ByteOrder.LITTLE_ENDIAN);		//zip file format is in little endian format
			iis.mark();
			int sig = iis.readInt();
			if(sig != ZIP_ID) {
				return false;		//header doesn't match
			}
			iis.seek(10);						//date time starts at the 10th byte
			//validate the time stamp for the first local header entry
			int time = iis.readUnsignedShort();	//read an MS-DOS date time format
			int seconds = (time & 0x1F) / 2;
			if((seconds < 0) || (seconds > 60)) {
				return false;
			}
			int minutes = (time & 0x7E0) >> 5;
			if((minutes < 0) || (minutes > 60)) {
				return false;
			}
			int hours = (time & 0xF800) >> 11;
			if((hours < 0) || (hours > 23)) {
				return false;
			}
			//validate the date stamp for the first local header entry
			int date = iis.readUnsignedShort();	//read an MS-DOS date time format
			int day = date & 0x1F;
			if((day < 1) || (day > 31)) {
				return false;
			}
			int month = (date & 0x1E0) >> 5;
			if((month < 1) || (month > 12)) {
				return false;
			}
			int year = ((date & 0xFE00) >> 9) + 1980;
			if((year < 1980) || (year > 2500)) {
				return false;
			}
		} finally {
			iis.setByteOrder(byteOrder);		//restore the byte order
			iis.reset();						//reset the stream position
		}
		return true;
	}
	
	public static boolean isZipFile(File file) throws IOException {
		FileImageInputStream fis = new FileImageInputStream(file);
		boolean result = isZipFile(fis);
		fis.close();
		return result;
	}
}
