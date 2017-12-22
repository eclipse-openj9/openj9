/*[INCLUDE-IF Sidecar18-SE]*/
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
package com.ibm.dtfj.corereaders.zos.util;

/**
 * This class provides a low level interface to some of the z/OS C library. This approach was
 * inspired by the 
 * <a href="https://www.eclipse.org/articles/Article-SWT-Design-1/SWT-Design-1.html">SWT design</a>
 * in that we have a one-to-one mapping between JNI calls and C library calls: no extra work is
 * done in the native functions. This is much easier to maintain because this library should
 * rarely need recompiling.
 * <p>
 * Please refer to the C library documentation for more details about arguments to these calls.
 * <p>
 * @depend - - - com.ibm.dtfj.corereaders.zos.util.CharConversion
 */

public class Clib {

    /**
     * This method is a wrapper around fopen.
     * @param filename the name of the file to open as a null-terminated ebcdic byte array
     * @param mode the open mode which must be a null-terminated ebcdic byte array. See the
     * C library fopen documentation for more details
     * @return the FILE stream pointer from fopen cast to a long
     */
    public static long fopen(byte[] filename, byte[]mode) {
        return rawFopen(filename, mode);
    }

    private static native long rawFopen(byte[] filename, byte[]mode);

    /**
     * This method is a slightly friendlier version of the native fopen call (the
     * parameters are Strings rather than byte arrays otherwise everything is the same).
     */
    public static long fopen(String filename, String mode) {
        return fopen(CharConversion.nullTerminate(CharConversion.getEbcdicBytes(filename)), CharConversion.nullTerminate(CharConversion.getEbcdicBytes(mode)));
    }

    /**
     * This method is a wrapper around fgetpos. Here we make use of the knowledge that a C
     * fpos_t on z/OS is actually an array of 8 ints.
     * @param fp the stream obtained from an earlier call to fopen
     * @param pos this must be an array of 8 ints. The position indicator is stored in
     * this array. The same array contents may be passed to a subsequent fsetpos call.
     * @return the return code from the fgetpos call (zero if successful)
     */
    public static int fgetpos(long fp, int[] pos) {
        assert pos.length == 8 : pos.length;
        assert fp != 0;
        return rawFgetpos(fp, pos);
    }

    private static native int rawFgetpos(long fp, int[] pos);

    /**
     * This method is a wrapper around fsetpos.
     * @param fp the stream obtained from an earlier call to fopen
     * @param pos this must be an array of 8 ints whose contents were obtained in a previous
     * call to fgetpos.
     * @return the return code from the fsetpos call (zero if successful)
     */
    public static int fsetpos(long fp, int[] pos) {
        assert pos.length == 8 : pos.length;
        assert fp != 0;
        return rawFsetpos(fp, pos);
    }

    private static native int rawFsetpos(long fp, int[] pos);

    /**
     * This method is a wrapper around fseek. (Note: this hasn't been tested for seeks
     * over 2GB yet).
     * @param fp the stream obtained from an earlier call to fopen
     * @param offset the offset to seek to
     * @param whence specifies where to seek from
     * @return the return code from the fseek call (zero if successful)
     */
    public static int fseek(long fp, long offset, int whence) {
        assert fp != 0;
        return rawFseek(fp, offset, whence);
    }
        
    private static native int rawFseek(long fp, long offset, int whence);

    /** fseek whence argument to indicate that offset is from the beginning of the file */
    public static final int SEEK_SET = 0;

    /** fseek whence argument to indicate that offset is from the current position of the
     *  file pointer */
    public static final int SEEK_CUR = 1;

    /** fseek whence argument to indicate that offset is from the end of the file */
    public static final int SEEK_END = 2;

    /**
     * This method is a wrapper around fread.
     * @param buf the buffer to contain the read data
     * @param size the size of each element
     * @param count the number of elements to read
     * @param fp the stream obtained from an earlier call to fopen
     * @return the number of elements read
     */
    public static int fread(byte[] buf, int size, int count, long fp) {
        assert size * count <= buf.length;
        assert fp != 0;
        return rawFread(buf, size, count, fp);
    }

    private static native int rawFread(byte[] buf, int size, int count, long fp);

    /**
     * This method is a wrapper around perror.
     * @param prefix the message prefix as a null-terminated ebcdic byte array
     * @return the FILE stream pointer from fopen cast to a long
     */
    public static void perror(byte[] prefix) {
        rawPerror(prefix);
    }

    private static native void rawPerror(byte[] prefix);

    /**
     * Friendlier version of the native perror call that accepts a String parameter.
     */
    public static void perror(String prefix) {
        perror(CharConversion.nullTerminate(CharConversion.getEbcdicBytes(prefix)));
    }

    static String hex(int n) {
        return Integer.toHexString(n);
    }
}
