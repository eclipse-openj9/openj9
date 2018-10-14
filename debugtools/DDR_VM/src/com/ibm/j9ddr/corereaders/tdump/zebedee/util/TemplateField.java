/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.tdump.zebedee.util;

import javax.imageio.stream.ImageInputStream;
import java.io.*;
import java.util.logging.*;
import org.w3c.dom.*;

/**
 * This class represents one field of a template.
 */
public class TemplateField {
    /** Our parent template */
    private Template template;
    /** The XML element that describes this field */
    private Element element;
    /** The offset in bits from the start of the template */
    private int offset;
    /** Is this a bit field? */
    private boolean isBitField;
    /** The length of this field in bits */
    int bitLength = -1;
    /** The name of the field (fully qualified) */
    private String name;
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

    /** Creates a new TemplateField */
    TemplateField(String name, Template template, Element element, int offset) {
        this.template = template;
        this.element = element;
        this.offset = offset;
        this.name = name;
        /* If the offset or length is not a multiple of 8 then we must be a bit field */
        if ((offset & 7) != 0 || (bitLength() & 7) != 0)
            isBitField = true;
    }

    /** Returns true if this is a bit field (ie the number of bits is not a multiple of 8) */
    public boolean isBitField() {
        return isBitField;
    }

    /**
     * Returns the length in bytes of this field
     */
    public int byteLength() {
        return bitLength() / 8;
    }

    /**
     * Returns the length in bits of this field
     */
    public int bitLength() {
        if (bitLength != -1)
            return bitLength;
        if (element == null)
            return 0;
        if (element.getTagName().equals("array")) {
            Element child = (Element)element.getElementsByTagName("item").item(0);
            bitLength = Integer.parseInt(element.getAttribute("length")) *
                Integer.parseInt(child.getAttribute("length"));
        } else if (element.getTagName().equals("item")) {
            bitLength = Integer.parseInt(element.getAttribute("length"));
        } else {
            throw new Error("unknown element tag: " + element.getTagName());
        }
        return bitLength;
    }

    /** Returns the name of the field */
    public String getName() {
        return name;
    }

    /**
     * Returns the field in long form. The field must be either of type "int" or of type "pointer".
     * For integers if the native size is less than 64 bits then a widening conversion is performed.
     * @param inputStream the ImageInputStream to read from
     * @param address the address of the start of the template (not of the field!)
     * @return the field converted to a long
     * @throws IOException if any errors occurred on reading from the stream
     */
    public long readLong(ImageInputStream inputStream, long address) throws IOException {
        inputStream.seek(address + (offset / 8));
        long result = 0;
        if (isBitField) {
            if (bitLength() > 64)
                throw new Error("request for long value for field " + name + " which has length of " + bitLength() + " bits");
            inputStream.setBitOffset(offset & 7);
            result = inputStream.readBits(bitLength());
            int shift = 64 - bitLength();
            result <<= shift;
            result >>= shift;
        } else {
            if (byteLength() == 1)
                result = inputStream.readByte();
            else if (byteLength() == 4)
                result = isPointer() ? inputStream.readUnsignedInt() : inputStream.readInt();
            else if (byteLength() == 8) {
                result = inputStream.readLong();
            } else if (byteLength() > 0 && byteLength() < 8) {
                for (int i = 0; i < byteLength(); i++) {
                    result <<= 8;
                    result |= inputStream.readUnsignedByte();
                }
                if (!isPointer()) {
                    int shift = 64 - (byteLength() * 8);
                    result <<= shift;
                    result >>= shift;
                }
            } else {
                throw new Error("request for long value for field " + name + " which has length of " + byteLength());
            }
        }
        log.fine("for field " + name + " of type " + element.getTagName() + " read value of 0x" + hex(result));
        return result;
    }

    /** 
     * Returns true is this field is a pointer.
     */
    public boolean isPointer() {
        return element.getElementsByTagName("pointer").getLength() != 0;
    }

    /**
     * Returns the offset (in bytes) of the field within the template.
     * @return the offset of the field (rounded down for bit fields)
     */
    public int getOffset() {
        return offset / 8;
    }

    /**
     * Returns the offset (in bits) of the field within the template.
     * @return the offset of the field 
     */
    public int getBitOffset() {
        return offset;
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }

    private static String hex(int i) {
        return Integer.toHexString(i);
    }
}
