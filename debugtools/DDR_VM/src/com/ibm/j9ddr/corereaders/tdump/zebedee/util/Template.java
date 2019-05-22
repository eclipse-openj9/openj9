/*******************************************************************************
 * Copyright (c) 2006, 2019 IBM Corp. and others
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

import java.io.*;
import java.util.*;
import javax.xml.parsers.*;
import java.util.logging.*;
import org.xml.sax.*;
import org.w3c.dom.*;

/**
 * This class represents a single template as defined in a template xml file. A template
 * defines the layout of storage in memory (similar to a C struct) and the purpose of templates
 * is to define for each field, the name, offset in bytes, length, type, description etc.
 * The template classes have no dependencies on any of the other Zebedee classes and
 * so may be reused for other purposes. All it requires is that the user supplies an implementation
 * of <a href="http://java.sun.com/j2se/1.4.2/docs/api/javax/imageio/stream/ImageInputStream.html">ImageInputStream</a>
 * so that the template classes can obtain the data for the fields in each structure.
 * <p>
 * The DTD for the XML description of a template type can be found <a href="templatedtd.html">here</a> (in pretty
 * html format) or <a href="templatedtd.dtd">here</a> (in raw form). The XML is always verified and the first line
 * must contain the following DTD declaration: <b>&lt;!DOCTYPE type SYSTEM "file://template.dtd"&gt;</b>
 * (the reference to template.dtd is automatically intercepted).
 * <p>
 * Templates are created using the factory method getTemplate rather than being instantiated directly so only
 * one persistent instance is created for a given name.
 */

public class Template {

    /** The name of this template */
    private String templateName;
    /** Maps template field names to TemplateField objects */
    private HashMap fieldNameTable = new HashMap();
    /** List of fields in their original order */
    private ArrayList fields = new ArrayList();
    /** Total length of the template in bytes */
    private int length;
    /** Maps template names to instances */
    private static HashMap templateMap = new HashMap();
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

    /**
     * Create a new Template from the given template file.
     * @param templateFileName the name of the template file. This can be relative or
     * absolute and must include the ".xml" (if any). getResourceAsStream is used to locate
     * the xml file so absolute pathnames are actually relative to this class's classpath.
     * @throws FileNotFoundException if the xml file could not be found
     * @throws InvalidTemplateFile if the xml is invalid in any way
     */
    public static Template getTemplate(String templateFileName) throws FileNotFoundException, InvalidTemplateFile {
        Template template = (Template)templateMap.get(templateFileName);
        if (template == null) {
            template = new Template(templateFileName);
            templateMap.put(templateFileName, template);
        }
        return template;
    }

    /**
     * Create a new Template from the given template file.
     * @param templateFileName the name of the template file. This can be relative or
     * absolute and must include the ".xml" (if any).
     * @throws FileNotFoundException if the xml file could not be found
     * @throws InvalidTemplateFile if the xml is invalid in any way
     */
    private Template(String templateFileName) throws FileNotFoundException, InvalidTemplateFile {
        log.fine("creating new template from file: " + templateFileName);
        InputStream is;
        if (templateFileName.charAt(0) == '/') {
            is = Template.class.getResourceAsStream(templateFileName);
            if (is == null) {
                throw new FileNotFoundException("getResourceAsStream failed for " + templateFileName);
            }
        } else {
            is = new FileInputStream(templateFileName);
        }
        create(is);
    }

    /**
     * Create a new Template from the given InputStream
     * @param is InputStream containing the template definition in xml.
     * @throws InvalidTemplateFile if the xml is invalid in any way
     */
    private void create(InputStream is) throws InvalidTemplateFile {
        /* Set up the XML parser. We use DOM style. */
        DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
        factory.setValidating(true);
        DocumentBuilder builder = null;
        try {
            builder = factory.newDocumentBuilder();
        } catch (javax.xml.parsers.ParserConfigurationException e) {
            throw new Error("unexpected error: " + e);
        }
        /* This is the error handler for the parser. It just rethrows all exceptions. */
        ErrorHandler handler = new ErrorHandler() {
            public void error(SAXParseException e) throws SAXException {
                throw e;
            }
            public void fatalError(SAXParseException e) throws SAXException {
                throw e;
            }
            public void warning(SAXParseException e) throws SAXException {
                throw e;
            }
        };
        builder.setErrorHandler(handler);

        /* Create an entity resolver for the parser. We use this to intercept the DOCTYPE line
         * and go get the template.dtd ourselves. */
        EntityResolver resolver = new EntityResolver() {
            public InputSource resolveEntity (String publicId, String systemId) {
                if (systemId.equals("file://template.dtd")) {
                    InputStream is = getClass().getResourceAsStream("template.dtd");
                    if (is == null)
                        throw new Error("could not load template.dtd");
                    return new InputSource(is);
                } else {
                    return null;
                }
            }
        };
        builder.setEntityResolver(resolver);
        /* Now go and parse the XML */
        Document document = null;
        try {
            document = builder.parse(is);
        } catch (SAXParseException e) {
            throw new InvalidTemplateFile("bad xml: " + e + " line number " + e.getLineNumber());
        } catch (SAXException e) {
            throw new InvalidTemplateFile("bad xml: " + e);
        } catch (IOException e) {
            throw new Error("unexpected error: " + e);
        }
        /* Get the root element. This will be the template element, its children are
           the versions of the root item. */
        Element templateElement = document.getDocumentElement();
        Element rootItemElement = null;
        templateName = templateElement.getAttribute("name");
        log.fine("creating type " + templateName);
        for (Node node = templateElement.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (node.getNodeType() == Node.ELEMENT_NODE) {
                rootItemElement = (Element)node;
                break;
            }
        }
        /* Now go and create TemplateField objects for all the descendent elements in the tree */
        length = createTemplateFields(rootItemElement, "", 0) / 8;
    }

    /** Returns the name of this template as specified by the top level element */
    public String templateName() {
        return templateName;
    }

    /** Returns the TemplateField with the given name.
     * @param name the name of the field
     * @throws NoSuchFieldException if there is no field with the given name
     */
    public TemplateField getField(String name) throws NoSuchFieldException {
        TemplateField field = (TemplateField)fieldNameTable.get(name);
        if (field == null)
            throw new NoSuchFieldException("no field with name " + name + " in template " + templateName);
        return field;
    }

    /**
     * Returns a <code>List</code> of the fields in the Template in their original order.
     * @return list the <code>List</code> of fields
     */
    public List getFields() {
        return fields;
    }

    /**
     * Returns the offset in bytes of the specified field within the template.
     * @param fieldName the name of the field
     * @return the offset of the field 
     * @throws NoSuchFieldException if there is no field with the given name
     */
    public int getOffset(String fieldName) throws NoSuchFieldException {
        return getField(fieldName).getOffset();
    }

    /** Create TemplateField objects for all our children and their children recursively */
    private int createTemplateFields(Element parentElement, String parentName, int bitOffset) {
        boolean isUnion = parentElement.hasAttribute("union");
        int maxLength = 0;
        for (Node node = parentElement.getFirstChild(); node != null; node = node.getNextSibling()) {
            if (node.getNodeType() != Node.ELEMENT_NODE)
                continue;
            Element element = (Element)node;
            if (element.getTagName().equals("pointer")) {
                // this element is for info purposes only
                continue;
            }
            String name = element.getAttribute("name");
            String fullName = parentName.length() > 0 ? parentName + "." + name : name;
            log.fine("creating field " + fullName + " at bit offset 0x" + hex(bitOffset));
            TemplateField field = new TemplateField(fullName, this, element, bitOffset);
            /* Create mappings from both the fully qualified and base name */
            fieldNameTable.put(fullName, field);
            fieldNameTable.put(name, field);
            fields.add(field);
            if (!element.getTagName().equals("array")) {
                /* Only process children if it's not an array */
                createTemplateFields(element, fullName, bitOffset);
            }
            if (isUnion) {
                if (field.bitLength() > maxLength)
                    maxLength = field.bitLength();
            } else {
                bitOffset += field.bitLength();
            }
        }
        return bitOffset + maxLength;
    }

    /** Returns the length in bytes of this template */
    public int length() {
        return length;
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }

    private static String hex(int i) {
        return Integer.toHexString(i);
    }

    private static String pad(String s, int pad) {
        for (int i = s.length(); i < pad; i++)
            s = " " + s;
        return s;
    }

    /** 
     * This method is mainly for regression testing. It prints a text version of the given
     * template files just containing the field name, offset and length. It's much easier to
     * do a diff on this format than the xml which I tend to fiddle with quite a lot!
     */
    public static void main(String[] args) {
        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(args[0]);
        } catch (Exception e) {
            System.err.println("Could not create output file " + args[0] + ": " + e);
            System.exit(1);
        }
        PrintStream out = new PrintStream(fos);
        for (int i = 1; i < args.length; i++) {
            try {
                Template template = Template.getTemplate(args[i]);
                out.println("=== " + template.templateName() + " ===");
                out.println("");
                out.println("Offset  Length  Field");
                out.println("======  ======  =====");
                out.println("");
                for (Iterator it = template.fields.iterator(); it.hasNext(); ) {
                    TemplateField field = (TemplateField)it.next();
                    String offset = pad(hex(field.getOffset()), 6);
                    String length = null;
                    if (field.isBitField()) {
                        length = pad("" + field.bitLength() + " b", 6);
                    } else {
                        length = pad("" + field.byteLength(), 6);
                    }
                    out.println(offset + "  " + length + "  " + field.getName());
                }
                if (i < args.length - 1) {
                    out.println("");
                    out.println("");
                }
            } catch (Exception e) {
                System.out.println("Unexpected exception: " + e);
                return;
            }
        }
    }
}
