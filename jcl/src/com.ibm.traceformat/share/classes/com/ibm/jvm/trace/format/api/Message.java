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
package com.ibm.jvm.trace.format.api;

import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.nio.BufferUnderflowException;
import java.util.ArrayList;
import java.util.Properties;

final public class Message {
	TraceContext context;

	private int ptrSize;

	private int type;
	private String message;
	private String component;
	private boolean zero = false;
	private Arg parameterTypes[];
	private Arg nonConstantParameterTypes[];
	private int id;
	private int level;
	private Properties statistics = new Properties();
	
	public Message(int type, String message, int id, int level, String component, String symbol, TraceContext context) {
		this.context = context;
		this.type = type;
		this.message = message;
		this.component = component;
		this.ptrSize = context.getPointerSize();
		this.parameterTypes = parseMessageTemplate(null);
		this.nonConstantParameterTypes = filterTypes(parameterTypes);
		this.id = id;
		this.level = level;
		this.statistics.put("component", component);
	}
	
	private Message(String message, int pointerSize) {
		this.message = message;
		this.ptrSize = pointerSize;
	}
	
	public void addStatistic(String key, long value) {
		long total = 0;
		if (statistics.containsKey(key)) {
			total = ((Long)statistics.get(key)).longValue();
		}
		total+= value;
		statistics.put(key, Long.valueOf(total));
	}
	
	public Properties getStatistics() {
		return statistics;
	}
	
	public static Arg[] getArgumentDetails(String template, int bytesPerPointer, StringBuffer templateErrors) {
		Message msg = new Message(template, bytesPerPointer);
		Arg[] parameters = msg.parseMessageTemplate(templateErrors);
		return msg.filterTypes(parameters);
	}

	private Arg[] filterTypes(Arg types[]) {
		int totalElems = types.length;
		ArrayList result = new ArrayList(totalElems);
		for (int i = 0; i < totalElems; i++) {
			if (!(types[i] instanceof FixedStringArg)) {
				result.add(types[i]);
			}
		}
		return wrangleToArgArray(result);
	}

	static private Arg[] wrangleToArgArray(ArrayList v) {
		int resSize = v.size();
		Arg res[] = new Arg[resSize];
		for (int i = 0; i < resSize; i++) {
			res[i] = (Arg) v.get(i);
		}
		return res;
	}

	final protected static void padBuffer(StringBuilder sb, int i, char c) {
		padBuffer(sb, i, c, false);
	}

	final protected static void padBuffer(StringBuilder sb, int i, char c, boolean leftJustified) {
		while (sb.length() < i) {
			if (leftJustified) {
				sb.append(c);
			} else {
				sb.insert(0, c);
			}
		}
	}

	/**
	 * process all the percent sign directives in a string, much like a call
	 * to printf
	 * 
	 * @param str
	 *                a string
	 * @return the string with percent directive replaces by their
	 *         associated values
	 * 
	 */
	protected StringBuilder processPercents(String str, byte[] buffer, int offset) {
		int totalElems = parameterTypes.length;
		StringBuilder result = new StringBuilder();
		ByteStream dataStream = context.createByteStream(buffer, offset);
		int item = 0;
		try {
			for (item = 0; item < totalElems; item++) {
				parameterTypes[item].format(dataStream, result);
			}
		} catch (BufferUnderflowException e) {
			context.warning(this, "BufferUnderflowException processing "+component+"."+id);
		}

		for (; item < totalElems; item++) {
			if (!(parameterTypes[item] instanceof FixedStringArg)) {
				result.append("<missing in trace>");
			} else {
				parameterTypes[item].format(dataStream, result);
			}
		}
		return result;
	}

	/**
	 * process all the percent sign directives in a string, making them
	 * "???"
	 * 
	 * @param str
	 *                a string
	 * @return the string with percent directives replaced by "???"
	 * 
	 */
	protected StringBuilder skipPercents(String str, byte[] buffer, int offset) {
		int totalElems = parameterTypes.length;
		StringBuilder result = new StringBuilder();
		ByteStream dataStream = context.createByteStream(buffer, offset);
		for (int i = 0; i < totalElems; i++) {
			if (!(parameterTypes[i] instanceof FixedStringArg)) {
				result.append("???");
			} else {
				parameterTypes[i].format(dataStream, result);
			}
		}
		return result;
	}

	protected void setPointerSize(int size) {
		ptrSize = size;
	}

	static private String readDigits(String str) {
		char[] digits = new char[10];
		int index = 0;

		while (Character.isDigit(str.charAt(index))) {
			if (index < digits.length) {
				digits[index] = str.charAt(index);
				index++;
			}
		}
		return new String(digits, 0, index);
	}

//	private StringBuilder format(BigInteger val, FormatSpec spec) {
//		StringBuilder str = new StringBuilder(val.toString());
//		if (spec != null) {
//			if (spec.precision != null) {
//				padBuffer(str, spec.precision.intValue(), '0');
//			}
//			if (spec.width != null) {
//				padBuffer(str, spec.width.intValue(), ' ', spec.leftJustified);
//			}
//		}
//		return str;
//	}
//
//	private StringBuilder format(String str, FormatSpec spec) {
//		StringBuilder result = new StringBuilder();
//		if (spec != null) {
//			if (spec.precision != null) {
//				int x = spec.precision.intValue();
//
//				if (x < str.length()) {
//
//					str = str.substring(0, x);
//				}
//			}
//			result.append(str);
//			if (spec.width != null) {
//				padBuffer(result, spec.width.intValue(), ' ', spec.leftJustified);
//			}
//		} else {
//			result.append(str);
//		}
//		return result;
//	}

	/**
	 * retrieve the text of the message, parsing any percent directives and
	 * replacing them with data from the buffer
	 * 
	 * @param buffer
	 *                an array of bytes
	 * @param offset
	 *                an int
	 * @return a string that is the message from the file with any percent
	 *         directives replaces by data from the buffer
	 */
	public String getMessage(byte[] buffer, int offset, int end) {
		String prefix = "";

		if (offset < end) {
			// If this is preformatted application trace, skip the
			// first 4 bytes
			if (component == "ApplicationTrace") {
				offset += 4;
			}
			return prefix + processPercents(message, buffer, offset).toString();
		} else {
			return prefix + skipPercents(message, buffer, offset).toString();
		}
	}

	/**
	 * returns the component this message corresponds to, as determined by
	 * the message file
	 * 
	 * @return an String that is component from which this message came from
	 */
	protected String getComponent() {
		return component;
	}

	/**
	 * returns the level of the component so it's possible to distinguish between the
	 * default always on set and user enabled trace points
	 * 
	 * @return the level of the trace point, -1 if not applicable
	 */
	protected int getLevel() {
		return level;
	}

	/**
	 * returns the type of entry this message corresponds to, as determined
	 * by the message file and TGConstants
	 * 
	 * @return an int that is one of the type defined in TGConstants
	 * @see TGConstants
	 */
	protected int getType() {
		return type;
	}

	protected String getFormattingTemplate() {
		return message;
	}

	/**
	 * parse the raw message into an array of the appropriate wrapped types.
	 * 
	 * @param buffer
	 *                an array of bytes that is the raw data to format
	 * @param offset
	 *                an int representing the offset into buffer to start.
	 * @return an array of wrapped types representing the parsed raw data.
	 * 
	 */
	protected Object[] parseMessage(byte[] buffer, int offset) {
		int totalElems = nonConstantParameterTypes.length;
		if (totalElems == 0 || buffer == null) {
			return new Object[0];
		}

		Object result[] = new Object[totalElems];
		ByteStream dataStream = context.createByteStream(buffer, offset);
		for (int i = 0; i < totalElems; i++) {
			result[i] = nonConstantParameterTypes[i].getValue(dataStream);
		}
		return result;
	}

	Arg[] parseMessageTemplate(StringBuffer templateErrors) {
		ArrayList result = new ArrayList();
		String str = message;
		int index = str.indexOf("%");
		int dataSize;
		boolean utf8;
		boolean prefix0x;
		String errors = "";

		while (index != -1) {
			FormatSpec fspec = null;
			if (index != 0) {
				result.add(new FixedStringArg(str.substring(0, index)));
			}
			// Check for "0x" prefix
			prefix0x = (index > 1) ? "0x".equalsIgnoreCase(str.substring(index - 2, index)) : false;
			utf8 = false;
			dataSize = 4; // Default to an integer
			str = str.substring(index + 1, str.length());
			/*
			 * Check for "-" prefix
			 */
			if (str.charAt(0) == '-') {
				fspec = new FormatSpec(true);
				str = str.substring(1, str.length());
			}

			/*
			 * Zero padding ?
			 */
			if (str.charAt(0) == '0') {
				if (fspec == null) {
					/* '-' overrides zero padding in printf */
					fspec = new FormatSpec();
					fspec.setZeroPad(true);
				}
				str = str.substring(1, str.length());
			}
			/*
			 * Handle width...
			 */
			if (Character.isDigit(str.charAt(0))) {
				if (fspec == null) {
					fspec = new FormatSpec();
				}
				String val = readDigits(str);
				fspec.width = Integer.valueOf(val);
				str = str.substring(val.length(), str.length());
			}

			/*
			 * ...and precision
			 */
			if (str.charAt(0) == '.') {
				str = str.substring(1, str.length());
				if (str.charAt(0) == '*') {
					utf8 = true;
					str = str.substring(1, str.length());
				} else {
					if (fspec == null) {
						fspec = new FormatSpec();
					}
					String prec = readDigits(str);
					fspec.precision = Integer.valueOf(prec);
					str = str.substring(prec.length(), str.length());
				}
			}

			/*
			 * Check for a size modifier
			 */
			char first = str.charAt(0);

			if (first == 'l') {
				if (str.charAt(1) == 'l') {
					dataSize = TraceContext.LONG;
					str = str.substring(2, str.length());
				} else {
					// Default size is INT
					str = str.substring(1, str.length());
				}
			} else if (first == 'I' && str.charAt(1) == '6' && str.charAt(2) == '4') {
				dataSize = TraceContext.LONG;
				str = str.substring(3, str.length());
			} else if (first == 'h') {
				dataSize = 2;
				str = str.substring(1, str.length());
			} else if (first == 'z') {
				dataSize = TraceContext.SIZE_T;
				str = str.substring(1, str.length());
			}

			/*
			 * Finally check for a type
			 */
			first = str.charAt(0);
			switch (first) {

			case 'p':
				if (!prefix0x) {
					result.add(new FixedStringArg("0x"));
				}
				str = str.substring(1, str.length());
				result.add(new PointerArg(ptrSize));
				break;

			case 's':
				if (utf8) {
					result.add(new UTF8StringArg());
				} else {
					result.add(new StringArg());
				}
				str = str.substring(1, str.length());
				break;

			case 'd':
			case 'i':
				str = str.substring(1, str.length());
				if (dataSize == 4 || (dataSize == -1 && ptrSize == 4)) {
					result.add(new I32_Arg(fspec, dataSize));
				} else {
					result.add(new I64_Arg(fspec, dataSize));
				}
				break;

			case 'u':
				str = str.substring(1, str.length());
				if (dataSize == 4 || (dataSize == -1 && ptrSize == 4)) {
					result.add(new U32_Arg(fspec, dataSize));
				} else {
					result.add(new U64_Arg(fspec, dataSize));
				}
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
				str = str.substring(1, str.length());
				result.add(new Double_Arg(fspec, ptrSize));
				break;

			case 'X':
			case 'x':
				if (fspec == null) {
					fspec = new FormatSpec();
				}
				fspec.setRadix(16);
				if (!prefix0x) {
					fspec.addOxPrefix();
				}
				str = str.substring(1, str.length());

				if (dataSize == 4 || (dataSize == -1 && ptrSize == 4)) {
					result.add(new U32_Arg(fspec, dataSize));
				} else {
					result.add(new U64_Arg(fspec, dataSize));
				}
				break;

			case 'c':
				str = str.substring(1, str.length());
				result.add(new CharArg());
				break;

			case '%':
				str = str.substring(1, str.length());
				result.add(new FixedStringArg("%"));
				break;

			case '+':
			case ' ':
			case '#':
				if (templateErrors != null) {
					templateErrors.append("Used a printf flag not supported " + first);
				}
				if (context != null) {
					context.error(this, "Used a printf flag not supported " + first);
				}
				str = str.substring(1, str.length());
				break;

			default:
				if (templateErrors != null) {
					templateErrors.append("error percent directive looked like => " + str);
				}
				if (context != null) {
					context.error(this, "error percent directive looked like => " + str);
				}
				str = str.substring(1, str.length());
			}

			index = str.indexOf("%");
			fspec = null;
		}

		// add trailing fixed text if any exists
		if (str.length() > 0) {
			result.add(new FixedStringArg(str));
		}

		int resSize = result.size();
		Arg res[] = new Arg[resSize];
		for (int j = 0; j < resSize; j++) {
			res[j] = (Arg) result.get(j);
		}
		return res;
	}

	public abstract class Arg {
		boolean zeroPad = false;
		int sizeof;
		String signature;

		Arg(int size) {
			sizeof = size;
		}

		abstract Object getValue(ByteStream msg);

		abstract void format(ByteStream msg, StringBuilder out);
		
		public int sizeof() {
			return sizeof;
		}
		
		public String signature() {
			return signature;
		}

		void zeroPad(boolean pad) {
			zeroPad = pad;
		}
	}

	public class CharArg extends Arg {
		CharArg() {
			super(1);
			signature = "char";
		}

		Object getValue(ByteStream msg) {
			return Character.valueOf((char)msg.get());
		}

		void format(ByteStream msg, StringBuilder out) {
			Character val = (Character)this.getValue(msg);
			out.append(val.charValue());
		}
	}

	public class PointerArg extends Arg {
		PointerArg(int size) {
			super(size);
			signature = "const void *";
		}

		Object getValue(ByteStream msg) {
			return new Long(ptrSize == 4 ? msg.getUnsignedInt() : msg.getLong());
		}

		void format(ByteStream msg, StringBuilder out) {
			Long val = (Long)this.getValue(msg);
			out.append(Long.toHexString(val.longValue()));
		}
	}

	public class FixedStringArg extends Arg {
		private String val;

		FixedStringArg(String fixedString) {
			/* variable size */
			super(-1);
			signature = "const char *";
			val = fixedString;
		}

		Object getValue(ByteStream msg) {
			return null;
		}

		void format(ByteStream msg, StringBuilder out) {
			out.append(val);
		}

	}

	public class StringArg extends Arg {
		StringArg() {
			/* variable size */
			super(-1);
			signature = "const char *";
		}

		Object getValue(ByteStream msg) {
			return msg.getASCIIString();
		}

		void format(ByteStream msg, StringBuilder out) {
			String val = this.getValue(msg).toString();
			out.append(val);
		}
	}

	public class UTF8StringArg extends Arg {
		UTF8StringArg() {
			/* variable size */
			super(-1);
			signature = "const char *";
		}

		Object getValue(ByteStream msg) {
			try {
				return msg.getUTF8String();
			} catch (UnsupportedEncodingException e) {
				context.error(this, "UTF-8 is reported as an invalid encoding!");
				return "see error output for details";
			}
		}

		void format(ByteStream msg, StringBuilder out) {
			String val = this.getValue(msg).toString();
			out.append(val);
		}
	}

	StringBuilder formatz(long val, FormatSpec spec) {
		StringBuilder str = new StringBuilder();
		if (spec != null) {
			if (spec.needsOxPrefix()) {
				str.append("0x");
			}
		}
		String s = Long.toString(val, spec != null ? spec.getRadix() : 10);
		str.append(s);
		if (spec != null) {
			if (spec.precision != null) {
				padBuffer(str, spec.precision.intValue(), '0');
			}
			if (spec.width != null) {
				padBuffer(str, spec.width.intValue(), (spec.needsZeroPad() && !spec.leftJustified) ? '0' : ' ', spec.leftJustified);
			}
		}
		return str;
	}

	public class I32_Arg extends Arg {
		private FormatSpec fspec;

		I32_Arg(FormatSpec fspec, int size) {
			super(size);
			if (size == TraceContext.SIZE_T) {
				signature = "IDATA";
				this.sizeof = ptrSize; 
			} else {
				signature = "I_32";
			}
			this.fspec = fspec;
		}

		Object getValue(ByteStream msg) {
			return Long.valueOf(msg.getInt());
		}

		void format(ByteStream msg, StringBuilder out) {
			Long val = (Long)this.getValue(msg);
			out.append(formatz(val.longValue(), fspec));
		}
	}

	public class I64_Arg extends Arg {
		private FormatSpec fspec;

		I64_Arg(FormatSpec fspec, int size) {
			super(size);
			if (size == TraceContext.SIZE_T) {
				signature = "IDATA";
				this.sizeof = ptrSize;
			} else {
				signature = "I_64";
			}
			this.fspec = fspec;
		}

		Object getValue(ByteStream msg) {
			return Long.valueOf(msg.getLong());
		}

		void format(ByteStream msg, StringBuilder out) {
			Long val = (Long)this.getValue(msg);
			out.append(formatz(val.longValue(), fspec));
		}
	}

	public class U32_Arg extends Arg {
		private FormatSpec fspec;

		U32_Arg(FormatSpec fspec, int size) {
			super(size);
			if (size == TraceContext.SIZE_T) {
				signature = "UDATA";
				this.sizeof = ptrSize;
			} else {
				signature = "U_32";
			}
			this.fspec = fspec;
		}

		Object getValue(ByteStream msg) {
			return Long.valueOf(msg.getUnsignedInt());
		}

		void format(ByteStream msg, StringBuilder out) {
			Long val = (Long)this.getValue(msg);
			out.append(formatz(val.longValue(), fspec));
		}
	}

	public class U64_Arg extends Arg {
		private FormatSpec fspec;

		U64_Arg(FormatSpec fspec, int size) {
			super(size);
			if (size == TraceContext.SIZE_T) {
				signature = "UDATA";
				this.sizeof = ptrSize;
			} else {
				signature = "U_64";
			}
			this.fspec = fspec;
		}

		Object getValue(ByteStream msg) {
			// TODO: fix unsigned (and in PointerArg as well)
			return Long.valueOf(msg.getLong());
		}

		void format(ByteStream msg, StringBuilder out) {
			Long val = (Long)this.getValue(msg);
			out.append(formatz(val.longValue(), fspec));
		}
	}

	public class Float_Arg extends Arg {
		private FormatSpec fspec;

		Float_Arg(FormatSpec fspec, int size) {
			super(size);
			signature = "float";
			this.fspec = fspec;
		}

		Object getValue(ByteStream msg) {
			return Float.valueOf(msg.getFloat());
		}

		void format(ByteStream msg, StringBuilder out) {
			Float val = (Float)this.getValue(msg);

			StringBuilder str = new StringBuilder(val.toString());
			if (fspec != null && fspec.width != null) {
				padBuffer(str, fspec.width.intValue(), ' ', fspec.leftJustified);
			}
			out.append(str);
		}
	}

	public class Double_Arg extends Arg {
		private FormatSpec fspec;

		Double_Arg(FormatSpec fspec, int size) {
			super(size);
			signature = "double";
			this.fspec = fspec;
		}

		Object getValue(ByteStream msg) {
			return Double.valueOf(msg.getDouble());
		}

		void format(ByteStream msg, StringBuilder out) {
			Double val = (Double)this.getValue(msg);

			StringBuilder str = new StringBuilder(val.toString());
			if (fspec != null && fspec.width != null) {
				padBuffer(str, fspec.width.intValue(), ' ', fspec.leftJustified);
			}
			out.append(str);
		}
	}

	class FormatSpec {
		protected Integer width; // this are object types to have
		protected Integer precision; // a null value mean not set.
		protected boolean leftJustified = false;
		private int radix = -1; // -1 is default
		private boolean addOxPrefix = false;
		private boolean zeroPad = false;

		public FormatSpec() {
			this(false);
		}

		public FormatSpec(boolean left) {
			this.leftJustified = left;
		}

		public void setRadix(int rdx) {
			radix = rdx;
		}

		public int getRadix() {
			return radix;
		}

		public void addOxPrefix() {
			addOxPrefix = true;
		}

		public boolean needsOxPrefix() {
			return addOxPrefix;
		}
		
		public void setZeroPad(boolean pad) {
			zeroPad = pad;
		}

		public boolean needsZeroPad() {
			return zeroPad;
		}

		public String toString() {
			return "FormatSpec w: " + width + " p:" + precision + " just: " + leftJustified;
		}
	}
}
