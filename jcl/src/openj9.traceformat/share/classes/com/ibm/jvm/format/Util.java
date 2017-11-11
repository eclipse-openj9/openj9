/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.jvm.format;

import java.math.BigInteger;
import java.util.Hashtable;
import java.util.Properties;
import java.util.Vector;

/** 
 * Provides various utility functions needed by the trace formatter
 *
 * @author Jonathon Lee
 */
final public class Util {
    private     static            Properties  properties;
    private     static            Hashtable   threads;
    private     static            int         timerType;
    private     static            Hashtable   components;
    private     static            boolean     bigendian;
    final       static String[]   timerDesc= {"Sequence number    ",
                                              "Special            ",
                                              "Pentium TSC        ",
                                              "Time (UTC)         ",
                                              "MSPR               ",
                                              "MFTB               ",
                                              "Time (UTC)         ",
                                              "J9 timer(UTC)      "};

    protected   static   final    int         BYTE                 =  1; // length of a byte in bytes
    protected   static   final    int         INT                  =  4; // length of an int in bytes
    protected   static   final    int         LONG                 =  8; // length of a long in bytes
    protected   static   final    String      SUM_TAB              =  "        ";

    protected   static   final    BigInteger  MILLIS2SECONDS       =  BigInteger.valueOf(1000);
    protected   static   final    BigInteger  SECONDS2MINUTES      =  BigInteger.valueOf(60);
    protected   static   final    BigInteger  MINUTES2HOURS        =  BigInteger.valueOf(60);
    protected   static   final    BigInteger  HOURS2DAYS           =  BigInteger.valueOf(24);
    protected   static   final    BigInteger  MILLION              =  BigInteger.valueOf(1000000);

    // don't let anyone instantiate this class
    private Util()
    {
    }

    /** Initialises static variables.
     *
     *  <p>This is called each time the TraceFormatter is started, from the initStatics()
     *     method in TraceFormat.java</p>
     */
    final protected static void initStatics()
    {
        properties = null;
        threads    = null;
        components = null;
    }

    /** puts a thread ID into a hashtable telling the formatter that this thread ID
     *  is one that should be traced
     *
     * @param   threadID     a string representation of a hexidecimal thread id
     */
    final protected static void putThreadID(Long threadID)
    {
        if ( threads == null ) {
            threads = new Hashtable();
        }
        threads.put(threadID, threadID);
    }

    /** retrieve whether of not the given id is in a hashtable.  if the table is null that means
     *  that no ids were added, since the default behavior is all id this returns true
     *
     * @param   threadID     a string representation of a hexidecimal thread id
     * @return  a boolean that is true if the id was added to the table or if the table is null
     */
    final protected static boolean findThreadID(Long threadID)
    {
        if ( threads == null ) {
            return true; //defaults to all
        }
        return threads.containsKey(threadID);
    }

    /** puts a component name into a hashtable telling the formatter that this component
     *  is one that should be traced. In this case all types of entries are formatted for this
     *  component
     *
     * @param   comp     a string representation of a component in the jvm
     */
    final protected static void putComponent(String comp)
    {
        if ( components == null ) {
            components = new Hashtable();
        }
        components.put(comp.toLowerCase(), new TypeList());
    }

    /** puts a component name into a hashtable telling the formatter that this component
     *  is one that should be traced. In this case only the types of entries specified in types
     *  are formatted for this component
     *
     * @param   comp     a string representation of a component in the jvm
     * @param   types    a vector of string for the type of trace entries to format (ie, Entry, Exit, Exception, etc)
     */
    final protected static void putComponent(String comp, Vector types)
    {
        if ( components == null ) {
            components = new Hashtable();
        }
        components.put(comp.toLowerCase(), new TypeList(types));
    }


    /** determines if a particular component and type is to be formatted
     *
     * @param   comp     a string representation of a component in the jvm
     * @param   types    a string for the type of trace entry to format (ie, Entry, Exit, Exception, etc)
     * @return  a boolean that is true if this component and type was added to the table or if the table is null
     */
    final protected static boolean findComponentAndType(String comp, String type)
    {
        if ( components == null ) {
            return true; // defaults to all(all)
        }
        if (comp.equals("dg")) {
            return true;
        }
        return(components.containsKey(comp) && ((TypeList) components.get(comp)).contains(type));
    }

    /** determines the encoding based on the byte passed in.  If the byte is
     *  a \u0044 (ASCII 'D') the encoding is ASCII
     *
     * @param   info
     */
    final static void determineEncoding(byte testChar)
    {
        Debug.println("Encoding is Ascii: " + (char) testChar + "= \u0044");
        setProperty("ENCODING", "ASCII");
    }

    /** Reverses the order of the bytes in the byte array
     *
     * @param   data  an array of bytes to be reversed
     */
    final static private void reverseByteOrder(byte[] data)
    {
        for ( int i = 0; i < data.length / 2 ; i++ ) {
            int temp    = data[i];
            data[i]     = data[(data.length - 1) - i];
            data[(data.length - 1) - i] = (byte) temp;
        }
    }

    /**  construct a BigInteger from an array of bytes.  BigIntgers are used to get around problems
     *   caused by the lack of unsigned primitive type.
     *
     * @param  data      an array of bytes
     * @param  offset    offset to the first byte to use to create the BigInteger
     * @param  numBytes  the length of this BigInteger in bytes
     * @return a positive BigInteger constructed from the byte array
     */
    final protected static BigInteger constructUnsignedLong(byte[] data, int offset)
    {
        return constructUnsignedLong(data, offset, Util.LONG);
    }

    final protected static BigInteger constructUnsignedLong(byte[] data, int offset, int numBytes)
    {
        byte[] bytes = new byte[numBytes];
        try {
            System.arraycopy(data, offset, bytes, 0, numBytes);
            if ( bigendian == false ) reverseByteOrder(bytes);
            BigInteger temp = new BigInteger(1, bytes); // 1 means positive
            // Debug.println("Util.constructUnsignedLong() => " + temp.toString() + " = 0x" +temp.toString(16));
            return temp;
        } catch (Exception e) {
            TraceFormat.outStream.println("******************* Exception: data.length "+
                                          data.length+ " offset " +offset+ " numBytes " +numBytes);
            return null;
        }
    }

    final protected static int constructTraceID(byte[] data, int offset)
    {

        return((((int) data[offset] << 16) & 0xff0000) | (((int) data[offset+1] << 8)  & 0xff00) | ((int) data[offset+2]  & 0xff));

        // Debug.println("Util.constructTraceID() => " + Long.toString(temp) + " = 0x" +Long.toString(temp, 16));
    }


    final protected static long constructUnsignedInt(byte[] data, int offset)
    {

        return( bigendian == false )
        ? ((  ((long) data[offset+3] << 24) & 0xff000000L)
           | (((long) data[offset+2] << 16) & 0xff0000L)
           | (((long) data[offset+1] << 8)  & 0xff00L)
           | ((long) data[offset]           & 0xffL))
        : ((   ((long) data[offset]  << 24) & 0xff000000L)
           | (((long) data[offset+1] << 16) & 0xff0000L)
           | (((long) data[offset+2] << 8)  & 0xff00L)
           | ((long) data[offset+3]         & 0xffL));
        // Debug.println("Util.constructUnsignedInt() => " + Long.toString(temp) + " = 0x" +Long.toString(temp, 16));
    }

    final protected static int constructUnsignedByte(byte[] data, int offset)
    {
        return(int) data[offset] & 0xff;

        // Debug.println("Util.constructUnsignedByte() => " + Long.toString(temp) + " = 0x" +Long.toString(temp, 16));
    }

    final static private int findNull(byte[] data, int index)
    {
        while ( index < data.length ) {
            if ( data[index] == 0 ) {
                return index;
            }
            index++;
        }
        return -1;
    }

    /**  Constructs a string from an array of bytes, encoding is as specified in the properties
     *
     */
    final protected static String constructString(byte[] data, int offset)
    {
        return constructString(data, offset, getProperty("ENCODING", "ASCII"));
    }

    /**  Constructs a string from an array of bytes, with the specified encoding (ASCII or EBCDIC?)
     *
     */
    final protected static String constructString(byte[] data, int offset, String enc)
    {
        int nullChar = findNull(data, offset);
        if ( nullChar == -1 ) {
            return "";
        }
        return constructString(data, offset, (nullChar-offset), enc);
    }

    /**  Constructs a string from an array of bytes with a count, encoding is as specified in the properties
     *
     */
    final protected static String constructString(byte[] data, int offset, int count)
    {
        return constructString(data, offset, count, getProperty("ENCODING", "ASCII"));
    }

    static boolean charIsOneOfControlCharactersToRemove(char c){
        if (c < 9) {
            /* ignore 0 - NUL
             * ignore 1 - SOH
             * ignore 2 - STX
             * ignore 3 - ETX
             * ignore 4 - EOT
             * ignore 5 - ENQ
             * ignore 6 - ACK
             * ignore 7 - BEL
             * ignore 8 - BS
             */
            return true;
        } else {
            return false;
        }        
    }

    /**  Constructs a string from an array of bytes with a count, with the specified encoding (ASCII or EBCDIC?)
     *
     */
    final protected static String constructString(byte[] data, int offset, int count, String enc)
    {
        try {
            if ( (offset + count) > data.length  ){
		return "";
	    }
            StringBuffer tempString = new StringBuffer();
            String beforeRemovingControlChars = new String(data, offset, count, enc);

            if (enc.equalsIgnoreCase("ASCII")) {            
                for (int i = 0; i < beforeRemovingControlChars.length(); i++) {
                    char c = beforeRemovingControlChars.charAt(i);
                                
                    if ( charIsOneOfControlCharactersToRemove(c) ) {
                        String octalRepresentation = Integer.toOctalString(c);
                        String formattedOctalRepresentation = "\\u" + "0000".substring(octalRepresentation.length()) + octalRepresentation;
                        tempString.append( formattedOctalRepresentation );
                    } else {
                        tempString.append( c );
                    }
                }
            } else {
                /* leave other encodings to fend for themselves */
                tempString.append(beforeRemovingControlChars);
            }
            return tempString.toString();
        } catch ( Exception e ) {
	    TraceFormat.outStream.println("Util.constructString (1) ****> offset: " + offset + ", count = " + count + " data = " + data);
            TraceFormat.outStream.println("Util.constructString (2) ****> " + e);
            return "";
        }
    }

    /** sets an property that affects the behavior of the formatter
     *
     * @param   key
     * @param   val
     */
    final protected static void setProperty(String key, String val)
    {
        if ( properties == null ) {
            properties = new Properties();
        }
        properties.setProperty(key, val);
    }

    /** gets an the value property that affecting behavior of the formatter
     *
     * @param   key
     * @return   a string
     */
    final protected static String getProperty(String key)
    {
        if ( properties == null ) {
            properties = new Properties();
        }
        return properties.getProperty(key);
    }

    /** gets an the value property that affecting behavior of the formatter, if no such
     *  property is set the default value is returned
     *
     * @param   key
     * @param   val
     * @return   a string
     */
    final protected static String getProperty(String key, String defaultValue)
    {
        if ( properties == null ) {
            properties = new Properties();
        }
        return properties.getProperty(key, defaultValue);
    }

    final protected static void padBuffer(StringBuffer sb, int i, char c)
    {
        padBuffer(sb, i, c, false);
    }

    final protected static void padBuffer(StringBuffer sb, int i, char c, boolean leftJustified)
    {
        while ( sb.length() < i ) {
            if (leftJustified) {
                sb.append(c);
            } else {
                sb.insert(0, c);
            }
        }
    }


    /*  0  Sequence Counter
     *  1  Special
     *  2  RDTSC Timer
     *  3  AIX Timer
     *  4  MFSPR Timer
     *  5  MFTB Timer
     *  6  STCK Timer
     *  7  J9 Timer
     */

    final static void setTimerType(int type)
    {
        timerType = type;
    }

    final static String getTimerDescription()
    {
        return timerDesc[timerType];
    }

    final static void printDump(byte[] buffer,int length)
    {
        int i,j,k;
        StringBuffer buf1,buf2;
        for (i=0;i<((length/16)+1);i++) {
            buf1 = new StringBuffer(100);
            buf2 = new StringBuffer(100);
            for (j=0;j<4;j++) {
                for (k=0;k<4;k++) {
                    if ((k+j*4+i*16) >= length) break;
                    byte2hex(buffer[k+j*4+i*16],buf1);
                    if ( (buffer[k+j*4+i*16]) > 40 && (buffer[k+j*4+i*16]) < 128) {
                        buf2.append((char)buffer[k+j*4+i*16]);
                    }
                    else
                    {
                        buf2.append(".");
                    }
                }
                buf1.append(" ");
            }
            Util.Debug.println(Util.formatAsHexString(i * 16) + ": " + buf1 + " " + buf2);
        }
    }

    /**
     * Formats a number as a hex string, prefixing the "0x"
     * radix specifier so that it can be unambiguously 
     * interpreted as a hex string.
     * @param number the number to format
     */
    final static public String formatAsHexString(long number)
    {
    	return "0x" + Long.toHexString(number);
    }
    

    /**
     * Converts a byte to hex digit and writes to the supplied buffer
     */
    private static void byte2hex(byte b, StringBuffer buf) {
        char[] hexChars = { '0', '1', '2', '3', '4', '5', '6', '7', '8',
                        '9', 'A', 'B', 'C', 'D', 'E', 'F' };
        int high = ((b & 0xf0) >> 4);
        int low = (b & 0x0f);
        buf.append(hexChars[high]);
        buf.append(hexChars[low]);
    }


    final static String getFormattedTime(BigInteger time)
    {
        //Util.Debug.println("formatting time    " + time);
        //Util.Debug.println("Util: timerType    " + timerType);
        switch (timerType) {

        case 2:
        case 4:
        case 5:
        case 7:
        {
            /*
             *  X86 with RDTSC, MFTB, MSPR, J9
             */
            if (TraceFormat.verMod >= 1.1) {
                BigInteger splitTime[];
                BigInteger nanos;
                BigInteger secondsMillis[];
                BigInteger minutesSeconds[];
                BigInteger hoursMinutes[];
                BigInteger daysHours[];

                time = time.subtract(TraceFormat.overallStartPlatform);
                splitTime = time.divideAndRemainder(TraceFormat.timeConversion);
                secondsMillis = splitTime[0].add(TraceFormat.overallStartSystem).divideAndRemainder(MILLIS2SECONDS);
                nanos = secondsMillis[1].multiply(MILLION).add(splitTime[1].multiply(MILLION).divide(TraceFormat.timeConversion));
                minutesSeconds = secondsMillis[0].divideAndRemainder(SECONDS2MINUTES);
                hoursMinutes = minutesSeconds[0].divideAndRemainder(MINUTES2HOURS);
                daysHours = hoursMinutes[0].divideAndRemainder(HOURS2DAYS);

                daysHours[1] = daysHours[1].add(TraceArgs.timeZoneOffset); /* allow user to modify timezone */
                try{
                    return "00".substring(daysHours[1].toString().length()) + daysHours[1].toString() + ":" +
                           "00".substring(hoursMinutes[1].toString().length()) + hoursMinutes[1].toString() + ":" +
                           "00".substring(minutesSeconds[1].toString().length()) + minutesSeconds[1].toString() + "." +
                           "000000000".substring(nanos.toString().length()) + nanos.toString();
                } catch(Exception e) {
                    Debug.println(daysHours[1].toString() + ":" +
                                  hoursMinutes[1].toString() + ":" +
                                  minutesSeconds[1].toString() + "." +
                                  nanos.toString());
                    return "Bad Time: " + time.toString(16);

                }
            } else {
                return "0000000000000000".substring(time.toString(16).length()) + time.toString(16);
            }
        }

        case 3:
        {
            /*
             *  Power/Power PC
             */
            long   seconds;
            long   nanos;
            long   ss;
            long   mm;
            long   hh;

            seconds = time.shiftRight(32).longValue() & 0xffffffffL;
            nanos = time.longValue() &  0xffffffffL;
            ss = seconds % 60;
            mm = (seconds / 60) % 60;
            hh = (seconds / 3600) % 24;
            try{
                return "00".substring(Long.toString(hh).length()) + Long.toString(hh) + ":" +
                       "00".substring(Long.toString(mm).length()) + Long.toString(mm) + ":" +
                       "00".substring(Long.toString(ss).length()) + Long.toString(ss) + "." +
                       "000000000".substring(Long.toString(nanos).length()) + Long.toString(nanos);
            } catch(Exception e) {
                Debug.println("hh: " + Long.toString(hh) + " mm: " +
                              Long.toString(mm) + " ss: " +
                              Long.toString(ss) + " Nanos: " +
                              Long.toString(nanos));
                return "Bad Time: " + time.toString(16);

            }
        }
        case 6:
        {
            /*
             *  System/390
             */
            long picos;
            long micros;
            long ss;
            long mm;
            long hh;

            micros = time.shiftRight(12).longValue() & 0xfffffffffffffL;
            ss = (micros / 1000000) % 60;
            mm = (micros / 60000000) % 60;
            hh = (micros / 3600000000L) % 24;
            micros = micros % 1000000;
            picos = (((time.longValue() &  0xfffL) | (micros << 12)) * 244140625) / 1000000;
            try{
                return "00".substring(Long.toString(hh).length()) + Long.toString(hh) + ":" +
                       "00".substring(Long.toString(mm).length()) + Long.toString(mm) + ":" +
                       "00".substring(Long.toString(ss).length()) + Long.toString(ss) + "." +
                       "000000000000".substring(Long.toString(picos).length()) + Long.toString(picos);
            } catch(Exception e) {
                Debug.println("hh: " + Long.toString(hh) + " mm: " +
                              Long.toString(mm) + " ss: " +
                              Long.toString(ss) + " Picos: " +
                              Long.toString(picos));
                return "Bad Time: " + time.toString(16);
            }
        }
        default:
        {
            return "0000000000000000".substring(time.toString(16).length()) + time.toString(16);
        }
        }

    }
    
    static class Debug {
        static java.io.PrintStream out = TraceFormat.outStream;

        static void println(Object o)
        {
            if ( TraceArgs.debug ) {
                out.println(o);
            }
        }
        static void print(Object o)
        {
        	if ( TraceArgs.debug ) {
        		out.print(o);
        	}
        }
    }

    /** keeps track of the types to format for a given component
     *
     * @author Jonathon Lee
     * @version 1.0, Date: 7/7/1999
     */
    final static class TypeList {
        Hashtable types;

        TypeList()
        {

        }

        TypeList(Vector typeNames)
        {
            types = new Hashtable();
            for ( int i = 0; i < typeNames.size(); i++ ) {
                String t = ((String) typeNames.elementAt(i)).toLowerCase();
                types.put(t, t);
            }
        }

        final boolean contains(String type)
        {
            if ( types == null ) {
                return true; //defaults to all
            }
            return types.containsKey(type);
        }
    }

    final protected static int convertEndian(int in)
    {
       return ( (in >>> 24) | (in << 24 ) |
                (( in << 8 ) & 0x00FF0000) | ((in  >> 8 ) & 0x0000FF00));
    }

    final protected static String convertAndCheckEyecatcher(int eyecatcher)
    {
        String eyeCatcherString;
        // convert back !!!!!!!!!!!!!!!!!
        eyecatcher = bigendian ? eyecatcher : Util.convertEndian(eyecatcher);
        switch (eyecatcher) {
        case 0x48545455: // allow for endian
            eyeCatcherString = "UTTH (in ASCII)";
            break;
        case 0x55545448:
            eyeCatcherString = "UTTH (in ASCII)";
            break;
        case 0x55545453:
            eyeCatcherString = "UTTS (in ASCII)";
            break;
        case 0x55545353:
            eyeCatcherString = "UTSS (in ASCII)";
            break;
        case 0x5554534F:
            eyeCatcherString = "UTSO (in ASCII)";
            break;
        case 0x55545441:
            eyeCatcherString = "UTTA (in ASCII)";
            break;
        case 0x55545052:
            eyeCatcherString = "UTPR (in ASCII)";
            break;
        case 0xe4e3e3c8:
            eyeCatcherString = "UTTH (in EBCDIC)";
            break;
        case 0xe4e3e3e2:
            eyeCatcherString = "UTTS (in EBCDIC)";
            break;
        case 0xe4e3e2e2:
            eyeCatcherString = "UTSS (in EBCDIC)";
            break;
        case 0xe4e3e2d6:
            eyeCatcherString = "UTSO (in EBCDIC)";
            break;
        case 0xe4e3e3c1:
            eyeCatcherString = "UTTA (in EBCDIC)";
            break;
        case 0xe4e3d7d9:
            eyeCatcherString = "UTPR (in EBCDIC)";
            break;
        case 0x48544744: // allow for endian
            eyeCatcherString = "DGTH (in ASCII)";
            break;
        case 0x44475448:
            eyeCatcherString = "DGTH (in ASCII)";
            break;
        case 0x44475453:
            eyeCatcherString = "DGTS (in ASCII)";
            break;
        case 0x44475353:
            eyeCatcherString = "DGSS (in ASCII)";
            break;
        case 0x4447534F:
            eyeCatcherString = "DGSO (in ASCII)";
            break;
        case 0x44475441:
            eyeCatcherString = "DGTA (in ASCII)";
            break;
        case 0x44475052:
            eyeCatcherString = "DGPR (in ASCII)";
            break;
        case 0xc4c7e3c8:
            eyeCatcherString = "DGTH (in EBCDIC)";
            break;
        case 0xc4c7e3e2:
            eyeCatcherString = "DGTS (in EBCDIC)";
            break;
        case 0xc4c7e2e2:
            eyeCatcherString = "DGSS (in EBCDIC)";
            break;
        case 0xc4c7e2d6:
            eyeCatcherString = "DGSO (in EBCDIC)";
            break;
        case 0xc4c7e3c1:
            eyeCatcherString = "DGTA (in EBCDIC)";
            break;
        case 0xc4c7d7d9:
            eyeCatcherString = "DGPR (in EBCDIC)";
            break;

        default:
            eyeCatcherString = "eyecatcher is bad " + Util.formatAsHexString(eyecatcher) + " *********************************";
        }

        return eyeCatcherString;
    }

    final protected static void setBigEndian(boolean endian)
    {
        bigendian = endian;
        return ;
    }

}
