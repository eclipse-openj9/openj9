package com.ibm.weavinghooktest;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.net.URL;

import org.osgi.framework.Bundle;

/**
 * A utility class used by Weaving Hook to redefine class bytes of a given class
 * 
 * @author administrator
 */
public class Util {
	
	/*
	 * Returns class bytes of a specified class present in the specified bundle.
	 */
	public static byte[] getClassBytes(Bundle bundle, String className) {
		byte classBytes[];
		
		try {
			String qualifiedClassName = className.replace('.', '/') + ".class";
			URL url = bundle.getEntry(qualifiedClassName);
			if (url != null) {
				InputStream in = url.openStream();
				ByteArrayOutputStream out = new ByteArrayOutputStream();
				int b;
				while ((b = in.read()) != -1) {
					out.write(b);
				}
				in.close();
				classBytes = out.toByteArray();
			} else {
				classBytes = null;
			}
		} catch (IOException e) {			
			e.printStackTrace();
			return null;
		}
		
		return classBytes;
	}
	
	// returns the index >= offset of the first occurrence of sequence in buffer
	public static int indexOf(byte[] buffer, int offset, byte[] sequence)
	{
		while (buffer.length - offset >= sequence.length) {
			for (int i = 0; i < sequence.length; i++) {
				if (buffer[offset + i] != sequence[i]) {
					break;
				}
				if (i == sequence.length - 1) {
					return offset;
				}
			}
			offset++;
		}
		return -1;
	}

	// replaces all occurrences of seq1 in original buffer with seq2
	public static byte[] replaceAll(byte[] buffer, byte[] seq1, byte[] seq2) {
		ByteArrayOutputStream out = new ByteArrayOutputStream();
		int index, offset = 0;
		while ((index = indexOf(buffer, offset, seq1)) != -1) {
			if (index - offset != 0) {
				out.write(buffer, offset, index - offset);
			}
			out.write(seq2, 0, seq2.length);
			offset = index + seq1.length;
		}
		if (offset < buffer.length) {
			out.write(buffer, offset, buffer.length - offset);
		}
		return out.toByteArray();
	}
	
	public static byte[] replaceClassNames(byte[] classBytes, String originalClassName, String redefinedClassName) {
		// replace all occurrences of the redefined class name with the original class name
		try {
			byte[] r = redefinedClassName.getBytes("UTF-8");
			byte[] o = originalClassName.getBytes("UTF-8");
			// since this is a naive search and replace that doesn't
			// take into account the structure of the class file, and
			// hence would not update length fields, the two names must
			// be of the same length
			if (r.length != o.length) {
				return null;
			}
			classBytes = Util.replaceAll(classBytes, r, o);
		} catch (UnsupportedEncodingException e) {
			e.printStackTrace();
			return null;
		}
		return classBytes;
	}
}
