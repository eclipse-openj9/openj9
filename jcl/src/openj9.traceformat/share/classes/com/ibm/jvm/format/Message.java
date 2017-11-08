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

final public class Message
{
   private static int   ptrSize;

   private int          type;
   private String       message;
   private String       component;
   private boolean      zero = false;
   private String       symbol;

   public Message(int type, String message, String component, String symbol)
   {
      this.type      =  type;
      this.message   =  message;
      this.component =  component;
      this.symbol    =  symbol;
   }

   /** processs all the percent sign directives in a string, much like a call to printf
    *
    * @param   str   a string
    * @return  the string with percent directive replaces by their associated values
    *
    */
   protected StringBuffer processPercents(String str, byte[] buffer, int offset)
   {

      FormatSpec     fspec    = null;
      StringBuffer   result   = new StringBuffer();
      int            index    = str.indexOf("%");
                     zero     = false;
      int            dataSize;
      long           l;
      int            i;
      String         s;
      boolean        prefix0x;
      boolean        utf8;

      while ( (index != -1) && (offset < buffer.length))
      {
         utf8 = false;
         dataSize = 4;                                // Default to an integer
         prefix0x = (index > 1) ?                     // Check for "0x" prefix
                    "0x".equalsIgnoreCase(str.substring(index - 2, index)):
                    false;
         result.append(str.substring(0, index));
         str      = str.substring(index+1, str.length());
         /*
          *  Check for "-" prefix
          */
         if ( str.charAt(0) == '-' )
         {
            fspec = new FormatSpec(true);
            str = str.substring(1, str.length());
         }

         /*
          *  Zero padding ?
          */
         if (str.charAt(0) == '0')
         {
            zero = true;
            str  = str.substring(1, str.length());
         }
         /*
          *  Handle width...
          */
         if ( Character.isDigit(str.charAt(0)) )
         {
            if ( fspec == null )
            {
               fspec = new FormatSpec();
            }
            String val = readDigits(str);
            fspec.width = Integer.valueOf(val);
            str = str.substring(val.length(), str.length());
         }

         /*
          *  ...and precision
          */
         if ( str.charAt(0) == '.' )
         {
            str = str.substring(1, str.length());
            if (str.charAt(0) == '*') {
               utf8 = true;
               str = str.substring(1, str.length());
            } else {

               if ( fspec == null )
               {
                  fspec = new FormatSpec();
               }

               String   prec        = readDigits(str);
               fspec.precision      = Integer.valueOf(prec);
               str = str.substring(prec.length(), str.length());
            }
         }

         /*
          *  Check for a size modifier
          */
         char first = str.charAt(0);

         if (first == 'l')
         {
             if (str.charAt(1) == 'l')
             {
                 dataSize = Util.LONG;
                 str = str.substring(2, str.length());
             } else {
                 // Default size is INT
                 str = str.substring(1, str.length());
             }
         }
         else if (first == 'I' && str.charAt(1) == '6' && str.charAt(2) == '4')
         {
             dataSize = Util.LONG;
             str = str.substring(3, str.length());
         }
         else if (first == 'h')
         {
             dataSize = 2;
             str = str.substring(1, str.length());
         }
         else if (first == 'z')
         {
             if (ptrSize == 8) {
                 dataSize = Util.LONG;
             }
             str = str.substring(1, str.length());
         }

         /*
          *  Finally check for a type
          */
         first = str.charAt(0);
         switch (first) {

         case 'p':
             l = ptrSize == 4 ? Util.constructUnsignedInt(buffer, offset) :
                                Util.constructUnsignedLong(buffer, offset).longValue();
             offset += ptrSize;
             if (!prefix0x) {
                 result.append("0x");
             }
             result.append(format(l, fspec, 16));
             str = str.substring(1, str.length());
             break;

         case 's':
             if (utf8) {
                 int utf8Length = Util.constructUnsignedLong(buffer, offset, 2).intValue();
                 offset += 2;
                 s = Util.constructString(buffer, offset, utf8Length);
                 offset += utf8Length;
             } else {
                 s = Util.constructString(buffer, offset);
                 offset += (s.length() + 1) ;
             }

             result.append(format(s, fspec));
             str = str.substring(1, str.length());
             break;

         case 'd':
         case 'i':
             l = dataSize == 4 ? Util.constructUnsignedInt(buffer, offset) :
                                 Util.constructUnsignedLong(buffer, offset).longValue();
             offset += dataSize;
             result.append(format(l, fspec));
             str = str.substring(1, str.length());
             break;

         case 'u':
             if (dataSize == 4) {
                 l = Util.constructUnsignedInt(buffer, offset);
                 l = l & 0xffffffffL;
             } else {
                 l = Util.constructUnsignedLong(buffer, offset).longValue();
             }
             offset += dataSize;
             result.append(format(l, fspec));
             str = str.substring(1, str.length());
             break;

         case 'f': 
        	 /* CMVC 164940 All %f tracepoints were promoted to double in runtime trace code.
        	  * Affects:
        	  *  TraceFormat  com/ibm/jvm/format/Message.java
        	  *  TraceFormat  com/ibm/jvm/trace/format/api/Message.java
        	  *  runtime    ute/ut_trace.c
        	  *  TraceGen     OldTrace.java
        	  * Intentional fall through to next case.
        	  */
         case 'g':
            l = Util.constructUnsignedLong(buffer, offset).longValue();
            offset += Util.LONG;
            result.append(format(Double.longBitsToDouble(l), fspec));
            str = str.substring(1, str.length());
            break;

         case 'X':
         case 'x':
            if (dataSize == 4) {
                l = Util.constructUnsignedInt(buffer, offset);
                l = l & 0xffffffffL;
            } else {
                l = Util.constructUnsignedLong(buffer, offset).longValue();
            }
            offset += dataSize;
            if (!prefix0x) {
                result.append("0x");
            }
            result.append(format(l, fspec, 16));
            str = str.substring(1, str.length());
            break;

         case 'c':
            s = Util.constructString(buffer, offset, 1);
            offset += 1;
            result.append(format(s,fspec));
            str = str.substring(1, str.length());
            break;

         case '%':
            result.append("%");
            str = str.substring(1, str.length());
            break;

         case '+':
         case ' ':
         case '#':
            TraceFormat.outStream.println("Used a printf flag not supported " + first);
            str = str.substring(1, str.length());
            break;

         default:
            TraceFormat.outStream.println("error percent directive looked like => " + str);
            str = str.substring(1, str.length());
         }

         fspec = null;
         zero  = false;
         index = str.indexOf("%");
      }
      result.append(str);
      return result;
   }

   /** process all the percent sign directives in a string, making them "???"
    *
    * @param   str   a string
    * @return  the string with percent directives replaced by "???"
    *
    */
   protected StringBuffer skipPercents(String str, byte[] buffer, int offset)
   {

      StringBuffer   result   = new StringBuffer();
      int            index    = str.indexOf("%");

      while ( (index != -1) && (offset < buffer.length))
      {
         result.append(str.substring(0, index));
         str      = str.substring(index+1, str.length());
         if ( str.charAt(0) == '-' ||
              str.charAt(0) == '0')
         {
            str = str.substring(1, str.length());
         }

         if ( Character.isDigit(str.charAt(0)) )
         {
            String val = readDigits(str);
            str = str.substring(val.length(), str.length());
         }

         if ( str.charAt(0) == '.' )
         {
            str = str.substring(1, str.length());
            if (str.charAt(0) == '*') {
                str = str.substring(1, str.length());
            } else {
                String   prec        = readDigits(str);
                str = str.substring(prec.length(), str.length());
            }
         }
         /*
          *  Check for a size modifier
          */

         char first = str.charAt(0);

         if (first == 'l')
         {
             str = str.substring((str.charAt(1) == 'l' ? 2 : 1), str.length());
         }
         else if (first == 'I' && str.charAt(1) == '6' && str.charAt(2) == '4')
         {
             str = str.substring(3, str.length());
         }
         else if (first == 'h')
         {
             str = str.substring(1, str.length());
         }
         else if (first == 'z')
         {
             str = str.substring(1, str.length());
         }

         /*
          *  Now look for a type
          */
         first = str.charAt(0);
         switch (first) {

             case 'd':
             case 'i':
             case 'u':
             case 'f':
             case 'X':
             case 'x':
             case 'p':
             case 's':
             case 'c':
                 str = str.substring(1, str.length());
                 result.append("???");
                 break;
             case '%':
                result.append("%");
                str = str.substring(1, str.length());
                break;
             case '+':
             case ' ':
             case '#':
                TraceFormat.outStream.println("Used a printf flag not supported " + first);
                str = str.substring(1, str.length());
                break;
             default:
                TraceFormat.outStream.println("Error: percent directive looked like => " + str);
                str = str.substring(1, str.length());
         }
         index = str.indexOf("%");
      }
      result.append(str);
      return result;
   }

   protected static void setPointerSize()
   {
       ptrSize   =  Integer.valueOf(Util.getProperty("POINTER_SIZE")).intValue();
   }

   private String readDigits(String str)
   {

      char[]   digits   = new char[10];
      int      index    = 0;

      while ( Character.isDigit(str.charAt(index)) )
      {

         if ( index < digits.length )
         {
            digits[index] = str.charAt(index);
            index++;
         }
      }
      return new String(digits, 0, index);
   }


   private StringBuffer format(BigInteger val, FormatSpec spec)
   {
      StringBuffer str = new StringBuffer(val.toString());
      if ( spec != null )
      {
         if ( spec.precision != null )
         {
            Util.padBuffer(str, spec.precision.intValue(), '0');
         }
         if ( spec.width != null )
         {
            Util.padBuffer(str, spec.width.intValue(), ' ', spec.leftJustified);
         }
      }
      return str;
   }

   private StringBuffer format(long val, FormatSpec spec)
   {
      return format(val, spec, 10);
   }

   private StringBuffer format(long val, FormatSpec spec, int radix)
   {
      StringBuffer str = new StringBuffer(Long.toString(val, radix));
      if ( spec != null )
      {
         if ( spec.precision != null )
         {
            Util.padBuffer(str, spec.precision.intValue(), '0');
         }
         if ( spec.width != null )
         {
            Util.padBuffer(str,
                           spec.width.intValue(),
                           (zero && !spec.leftJustified) ? '0' : ' ',
                           spec.leftJustified);
         }
      }
      return str;
   }

   private StringBuffer format(float val, FormatSpec spec)
   {
      StringBuffer str = new StringBuffer(Float.toString(val));
      if ( spec != null )
      {
         if ( spec.width != null )
         {
            Util.padBuffer(str, spec.width.intValue(), ' ', spec.leftJustified);
         }
      }
      return str;
   }

   private StringBuffer format(double val, FormatSpec spec)
   {
      StringBuffer str = new StringBuffer(Double.toString(val));
      if ( spec != null )
      {
         if ( spec.width != null )
         {
            Util.padBuffer(str, spec.width.intValue(), ' ', spec.leftJustified);
         }
      }
      return str;
   }
   private StringBuffer format(String str, FormatSpec spec)
   {
      StringBuffer result = new StringBuffer();
      if ( spec != null )
      {
         if ( spec.precision != null )
         {
            int x = spec.precision.intValue();

            if ( x < str.length() )
            {

               str = str.substring(0, x);
            }
         }
         result.append(str);
         if ( spec.width != null )
         {
            Util.padBuffer(result, spec.width.intValue(), ' ', spec.leftJustified);
         }
      }
      else
      {
         result.append(str);
      }
      return result;
   }


   /** retrieve the text of the message, parsing any percent directives and replacing them with data from the buffer
    *
    * @param   buffer      an array of bytes
    * @param   offset      an int
    * @return  a string that is the message from the file with any percent directives replaces by data from the buffer
    */
   public String getMessage(byte[] buffer, int offset, int end)
   {
      String prefix = (  TraceArgs.symbolic ) ? symbol+" " : "";

      if ( offset < end )
      {
          // If this is preformatted application trace, skip the first 4 bytes
          if (component == "ApplicationTrace") {
              offset += 4;
          }
         return prefix + processPercents(message, buffer, offset).toString();
      } else {
        return prefix + skipPercents(message, buffer, offset).toString();
      }
   }

   /** returns the component this message corresponds to, as
    * determined by the message file
    *
    * @return  an String that is component from which this message came from
    */
   protected String getComponent()
   {
      return component;
   }

   /** returns the type of entry this message corresponds
    *  to, as determined by the message file and TGConstants
    *
    * @return  an int that is one of the type defined in TGConstants
    * @see     TGConstants
    */
   protected int getType()
   {
      return type;
   }

   protected String getFormattingTemplate(){
	   return message;
   }
   
   class FormatSpec
   {
      protected Integer width;         // this are object types to have
      protected Integer precision;     // a null value mean not set.
      protected boolean leftJustified = false;

      public FormatSpec(boolean left)
      {
         this.leftJustified = left;
      }

      public FormatSpec()
      {
         this(false);
      }

      public String toString()
      {
         return "FormatSpec w: "+ width +" p:"+ precision + " just: "+ leftJustified;
      }
   }

}
