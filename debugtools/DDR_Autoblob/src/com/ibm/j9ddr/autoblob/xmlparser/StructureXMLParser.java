/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob.xmlparser;

import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.Stack;

import org.xml.sax.Attributes;
import org.xml.sax.ContentHandler;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.DefaultHandler;
import org.xml.sax.helpers.XMLReaderFactory;

import com.ibm.j9ddr.autoblob.CTypeParser;
import com.ibm.j9ddr.autoblob.datamodel.ClassType;
import com.ibm.j9ddr.autoblob.datamodel.Declaration;
import com.ibm.j9ddr.autoblob.datamodel.EnumType;
import com.ibm.j9ddr.autoblob.datamodel.EnumerationConstant;
import com.ibm.j9ddr.autoblob.datamodel.ITypeCollection;
import com.ibm.j9ddr.autoblob.datamodel.RecordType;
import com.ibm.j9ddr.autoblob.datamodel.StructType;
import com.ibm.j9ddr.autoblob.datamodel.Type;
import com.ibm.j9ddr.autoblob.datamodel.Typedef;
import com.ibm.j9ddr.autoblob.datamodel.UnionType;
import com.ibm.j9ddr.autoblob.datamodel.UserDefinedType;
import com.ibm.j9ddr.autoblob.linenumbers.ILineNumberConverter;


/**
 * Class that reads the XML produced by the EDG-based 
 * extract_structures tool.
 * 
 * @author andhall
 *
 */
public class StructureXMLParser implements ITypeCollection
{
	public static final String CLASS_TAG = "class";
	
	public static final String BASE_CLASS_TAG = "base_class";
	
	public static final String UNION_TAG = "union";
	
	public static final String TYPEDEF_TAG = "typedef";
	
	public static final String ENUM_TAG = "enum";
	
	public static final String ENUM_CONSTANT_TAG = "constant";
	
	public static final String NAMESPACE_TAG = "namespace";
	
	public static final String STRUCT_TAG = "struct";
	
	public static final String FIELD_TAG = "field";
	
	public static final String TYPES_TAG = "types";

	public static final String ENUM_CONSTANT_VALUE_ATTRIBUTE = "value";
	
	public static final String TYPE_NAME_ATTRIBUTE = "name";
	
	public static final String TYPE_LINE_ATTRIBUTE = "line";
	
	public static final String FIELD_NAME_ATTRIBUTE = "name";
	
	public static final String TYPE_COMPLETE_ATTRIBUTE = "complete";
	
	public static final String FIELD_TYPE_ATTRIBUTE = "type";
	
	public static final String FIELD_LINE_ATTRIBUTE = "line";
	
	public static final String TYPEDEF_ALIASES_ATTRIBUTE = "aliases";
	
	public static final String TYPES_LANGUAGE_ATTRIBUTE = "language";
	
	public static final String ANONYMOUS_TYPE_ATTRIBUTE = "anonymous_type";
	
	public static final String ANONYMOUS_TYPE_ID_ATTRIBUTE = "anonymous_type_id";
	
	private final XMLHandler handler;
	
	public StructureXMLParser(File xmlFile, ILineNumberConverter lineNumberConverter) throws SAXException, IOException
	{
		this.handler = new XMLHandler(lineNumberConverter);
		
		XMLReader reader = XMLReaderFactory.createXMLReader();
		
		reader.setContentHandler(handler);
		reader.setErrorHandler(handler);
		
		reader.parse(new InputSource(new FileReader(xmlFile)));

		handler.fixUpFieldTypes();
	}
	
	public Map<String, Typedef> getTypedefs()
	{
		return Collections.unmodifiableMap(handler.typedefsByName);
	}
	
	public Set<StructType> getStructures()
	{
		return Collections.unmodifiableSet(new HashSet<StructType>(handler.structsByName.values()));
	}
	
	public Set<UnionType> getUnions()
	{
		return Collections.unmodifiableSet(new HashSet<UnionType>(handler.unionsByName.values()));
	}
	
	public Set<EnumType> getEnums()
	{
		return Collections.unmodifiableSet(new HashSet<EnumType>(handler.enumsByName.values()));
	}
	
	public List<EnumType> getTopScopeAnonymousEnums()
	{
		return Collections.unmodifiableList(handler.topScopeAnonymousEnums);
	}
	
	public UserDefinedType findType(String name)
	{
		if (handler.structsByName.containsKey(name)) {
			return handler.structsByName.get(name);
		} else if (handler.enumsByName.containsKey(name)) {
			return handler.enumsByName.get(name);
		} else if (handler.unionsByName.containsKey(name)) {
			return handler.unionsByName.get(name);
		} else if (handler.typedefsByName.containsKey(name)) {
			return handler.typedefsByName.get(name);
		} else {
			return null;
		}
	}
	
	private static class XMLHandler extends DefaultHandler implements ContentHandler
	{
		final Map<String, Typedef> typedefsByName = new HashMap<String, Typedef>();
		final Map<String, StructType> structsByName = new HashMap<String, StructType>();
		final Map<String, EnumType> enumsByName = new HashMap<String, EnumType>();
		final Map<String, UnionType> unionsByName = new HashMap<String, UnionType>();
		static final Map<String, Type> primitiveTypesByName = new HashMap<String, Type>();
		
		final Map<Long, StructType> anonymousStructsById = new HashMap<Long, StructType>();
		final Map<Long, EnumType> anonymousEnumsById = new HashMap<Long, EnumType>();
		final Map<Long, UnionType> anonymousUnionsById = new HashMap<Long, UnionType>();
		
		final List<EnumType> topScopeAnonymousEnums = new LinkedList<EnumType>();
		
		private static final String[] primitiveTypes = new String[]{ 
			"bool",
			"byte",
			"short",
			"int",
			"long",
			"long long",
			"unsigned byte",
			"unsigned short",
			"unsigned int",
			"unsigned long",
			"unsigned long long",
			"signed byte",
			"signed short",
			"signed int",
			"signed long",
			"signed long long",
			"float",
			"double",
			"char",
			"unsigned char",
			"signed char"
		};
		
		static {
			for (String primitiveType : primitiveTypes) {
				primitiveTypesByName.put(primitiveType, new Type(primitiveType));
			}
		}
		
		private final ILineNumberConverter converter;
		
		/* State variables */
		
		private Stack<UserDefinedType> typeStack = new Stack<UserDefinedType>();
		
		//Stack of runnables to be executed every time a tag closes. Will cope very badly with broken XML.
		private final Stack<Runnable> deferredWorkStack = new Stack<Runnable>();
		
		private static final Runnable NO_OP = new Runnable() {

			public void run() {
			}};
		
		private final Runnable POP_TYPE_STACK = new Runnable() {
			public void run()
			{
				typeStack.pop();
			}
		};
				
		private boolean isCPlusPlus;
			
		XMLHandler(ILineNumberConverter converter)
		{
			this.converter = converter;
		}
		
		//C++ has a single namespace for structs, classes, enums and typedefs - so EDG doesn't bother specifying struct J9JavaVM *
		//for example, it just puts J9JavaVM *. The DDR structure reader relies on the type prefix, so we have to add it back in
		public void fixUpFieldTypes()
		{
			if (! isCPlusPlus) {
				return;
			}
			
			fixUpFields(structsByName.values());
			fixUpFields(unionsByName.values());
		}

		private void fixUpFields(Collection<? extends RecordType> values)
		{
			for (RecordType r : values) {
				for (Declaration dec : r.getEntries()) {
					String fixedUpType = resolveCPlusPlusTypeName(dec.getType().getFullName());
					
					dec.overrideTypeString(fixedUpType);
				}
			}
		}
		
		private String resolveCPlusPlusTypeName(String typeName)
		{
			CTypeParser parser = new CTypeParser(typeName);
			
			Type t = getCPlusPlusType(parser.getCoreType(), -1);
			
			return parser.getPrefix() + t.getFullName() + parser.getSuffix();
		}

		@Override
		public void startElement(String uri, String localName, String qName, Attributes atts) throws SAXException
		{
			//Every tag must add an entry to deferredWorkStack (even if it's NO_OP)
			if (qName.equals(TYPES_TAG)) {
				String language = atts.getValue(TYPES_LANGUAGE_ATTRIBUTE);
				
				isCPlusPlus = language.equals("C++");
				
				deferredWorkStack.push(NO_OP);
			} else if (qName.equals(STRUCT_TAG) || qName.equals(CLASS_TAG)) {
				String name = atts.getValue(TYPE_NAME_ATTRIBUTE);
				int line = getLine(atts);
				boolean isComplete = isComplete(atts);
				name = cleanName(name);
				
				final UserDefinedType type;
				if (qName.equals(STRUCT_TAG)) {
					type = getStruct(name, atts, line);
				} else {
					type = getClass(name, line);
				}
				
				type.setComplete(isComplete);
				
				typeStack.push(type);
				deferredWorkStack.push(POP_TYPE_STACK);
			} else if (qName.equals(UNION_TAG)) {
				String name = atts.getValue(TYPE_NAME_ATTRIBUTE);
				int line = getLine(atts);
				name = cleanName(name);
				
				final UserDefinedType type = getUnion(name, atts, line);
				
				typeStack.push(type);
				deferredWorkStack.push(POP_TYPE_STACK);
			} else if (qName.equals(BASE_CLASS_TAG)) {
				String name = atts.getValue(TYPE_NAME_ATTRIBUTE);
				
				RecordType subclass = (RecordType)typeStack.peek();
				Type superClass = getType(name,atts,0);
				
				if (! (superClass instanceof UserDefinedType)) {
					throw new SAXException("Superclass: " + name + " of type " + superClass.getClass().getName() + " isn't a UserDefinedType");
				}
				
				subclass.setSuperClass((UserDefinedType) superClass);
				deferredWorkStack.push(NO_OP);
			} else if (qName.equals(TYPEDEF_TAG)) {
				final String name = atts.getValue(TYPE_NAME_ATTRIBUTE);
				final int line = getLine(atts);
				String aliasedStr = atts.getValue(TYPEDEF_ALIASES_ATTRIBUTE);
				
				Type aliasedType = getType(aliasedStr, atts, line);
				
				if (aliasedType == null) {
					throw new SAXException("Couldn't match aliased type: " + aliasedStr + " for typedef " + name);
				}
				
				Typedef typedef = new Typedef(name, aliasedType, converter.convert(line));
				
				typedefsByName.put(name, typedef);
				deferredWorkStack.push(NO_OP);
			} else  if (qName.equals(ENUM_TAG)) {
				String name = atts.getValue(TYPE_NAME_ATTRIBUTE);
				int line = getLine(atts);
				
				name = cleanName(name);
				
				final EnumType type = getEnum(name, atts, line);
				
				if (type.isAnonymous()) {
					//Anonymous enums need to have their constants exposed somewhere
					if (typeStack.isEmpty()) {
						topScopeAnonymousEnums.add(type);
					} else {
						//Enum declared inside another type
						UserDefinedType t = typeStack.peek();
						t.attachInnerEnum((EnumType)type);
					}
					
				}
				
				typeStack.push(type);
				deferredWorkStack.push(POP_TYPE_STACK);
			} else if (qName.equals(FIELD_TAG)) {
				final String name = atts.getValue(FIELD_NAME_ATTRIBUTE);
				String typeName = atts.getValue(FIELD_TYPE_ATTRIBUTE);
				final int line = getLine(atts);
				
				Type type = typeName != null ? new Type(typeName) : getAnonymousType(atts, line);
				
				RecordType currentStruct = (RecordType) typeStack.peek();
				currentStruct.addEntry(new Declaration(name, type, converter.convert(line)));
				
				deferredWorkStack.push(NO_OP);
			} else if (qName.equals(ENUM_CONSTANT_TAG)) {
				String value = atts.getValue(ENUM_CONSTANT_VALUE_ATTRIBUTE);
				
				EnumType parent = (EnumType)typeStack.peek();
				
				parent.addConstant(new EnumerationConstant(parent, value));
				deferredWorkStack.push(NO_OP);
			} else {
				deferredWorkStack.push(NO_OP);
			}
		}

		private UserDefinedType getAnonymousType(Attributes atts, int line)
		{
			String type = atts.getValue(ANONYMOUS_TYPE_ATTRIBUTE);
			
			long id = getAnonymousTypeId(atts);
			
			if (type.equals("struct")) {
				return getAnonymousStruct(id, line);
			} else if (type.equals("union")) {
				return getAnonymousUnion(id, line);
			} else if (type.equals("enum")) {
				return getAnonymousEnum(id, line);
			} else {
				throw new UnsupportedOperationException("Unknown anonymous type group: " + type);
			}
		}

		private boolean isComplete(Attributes atts)
		{
			String completeString = atts.getValue(TYPE_COMPLETE_ATTRIBUTE);
			
			if (completeString == null) {
				return false;
			} else {
				return Boolean.parseBoolean(completeString);
			}
		}

		private EnumType getEnum(String name, Attributes atts, int line)
		{
			if (name == null) {
				return getAnonymousEnum(getAnonymousTypeId(atts), line);
			}
			
			EnumType type = enumsByName.get(name);
			
			if (type == null) {
				type = new EnumType(name, converter.convert(line));
				enumsByName.put(name, type);
			}
			return type;
		}

		private long getAnonymousTypeId(Attributes atts)
		{
			String working = atts.getValue(ANONYMOUS_TYPE_ID_ATTRIBUTE);
			
			if (working.startsWith("0x")) {
				working = working.substring(2);
			}
			
			return Long.parseLong(working, 16);
		}

		private UnionType getUnion(String name, Attributes atts, int line)
		{
			if (name == null) {
				return getAnonymousUnion(getAnonymousTypeId(atts), line);
			}
			
			UnionType type = unionsByName.get(name);
			
			if (type == null) {
				type = new UnionType(name, converter.convert(line));
				unionsByName.put(name, type);
			}
			
			return type;
		}

		private StructType getClass(String name, int line)
		{
			StructType type = structsByName.get(name);
			
			if (type == null) {
				type = new ClassType(name, converter.convert(line));
				structsByName.put(name, type);
			}
			
			return type;
		}

		private StructType getStruct(String name, Attributes atts, int line)
		{
			if (name == null) {
				return getAnonymousStruct(getAnonymousTypeId(atts), line);
			}
			
			StructType type = structsByName.get(name);
			
			if (type == null) {
				type = new StructType(name, converter.convert(line));
				structsByName.put(name, type);
			}
			
			return type;
		}

		/* Strips struct, union, enum etc. from the front of a type name */
		private String cleanName(String name)
		{
			String working = name;
			
			if (working == null) {
				return working;
			}
			
			working = stripPrefix(working, "struct ");
			working = stripPrefix(working, "enum ");
			working = stripPrefix(working, "union ");
			
			return working;
		}

		private String stripPrefix(String name, String prefix)
		{
			String working = name.trim();
			
			if (working.startsWith(prefix)) {
				return working.substring(prefix.length());
			} else {
				return working;
			}
		}
		
		private StructType getAnonymousStruct(long id, int line)
		{
			StructType toReturn = anonymousStructsById.get(id);
			
			if (null == toReturn) {
				toReturn = new StructType(null, converter.convert(line));
				anonymousStructsById.put(id, toReturn);
			}
			
			return toReturn;
		}
		
		private EnumType getAnonymousEnum(long id, int line)
		{
			EnumType toReturn = anonymousEnumsById.get(id);
			
			if (null == toReturn) {
				toReturn = new EnumType(null, converter.convert(line));
				anonymousEnumsById.put(id, toReturn);
			}
			
			return toReturn;
		}
		
		private UnionType getAnonymousUnion(long id, int line)
		{
			UnionType toReturn = anonymousUnionsById.get(id);
			
			if (null == toReturn) {
				toReturn = new UnionType(null, converter.convert(line));
				anonymousUnionsById.put(id, toReturn);
			}
			
			return toReturn;
		}
		
		private Type getType(String name, Attributes atts, int line)
		{
			if (name == null) {
				return getAnonymousType(atts, line);
			} else {
				return isCPlusPlus ? getCPlusPlusType(name, line) : getCType(name, line);
			}
		}
		
		private Type getCPlusPlusType(String name, int line)
		{
			//One namespace for structs, enums, unions and classes
			if (name.contains("*") && !name.contains(">")) {
				//Pointer type of some description
				return new Type(name);
			} else if (name.contains(":") && !name.contains("::")) {
				//Bitfield
				return new Type(name);
			} else if (name.contains("[")) {
				return new Type(name);
			} else if (enumsByName.containsKey(name)) {
				return enumsByName.get(name);
			} else if (structsByName.containsKey(name)) {
				return structsByName.get(name);
			} else if (unionsByName.containsKey(name)) {
				return unionsByName.get(name);
			} else if (primitiveTypesByName.containsKey(name)) {
				return primitiveTypesByName.get(name);
			} else if (typedefsByName.containsKey(name)) {
				return typedefsByName.get(name);
			} else {
				return new Type(name);
			}
		}
		
		private Type getCType(String name, int line)
		{
			Type toReturn = null;
			
			if (name.contains("*")) {
				//Pointer type of some description
				toReturn = new Type(name);
			} else if (name.contains(":")) {
				//Bitfield
				toReturn = new Type(name);
			} else if (name.contains("[")) {
				toReturn = new Type(name);
			} else if (name.startsWith("enum ")) {
				name = stripPrefix(name, "enum ");
				
				toReturn = getEnum(name, null, line);
			} else if (name.startsWith("struct ")) {
				name = stripPrefix(name, "struct ");
				
				toReturn = getStruct(name,null, line);
			} else if (name.startsWith("union ")) {
				name = stripPrefix(name, "union ");
				
				toReturn = getUnion(name, null, line);
			} else if (primitiveTypesByName.containsKey(name)) {
				toReturn =  primitiveTypesByName.get(name);
			} else if (typedefsByName.containsKey(name)) {
				toReturn =  typedefsByName.get(name);
			} else if (structsByName.containsKey(name)) {
				toReturn =  structsByName.get(name);
			}

			return toReturn;
		}

		private int getLine(Attributes atts)
		{
			String lineStr = atts.getValue(TYPE_LINE_ATTRIBUTE);
			
			if (lineStr != null) {
				return Integer.parseInt(lineStr);
			} else {
				return 0;
			}
		}

		@Override
		public void endElement(String uri, String localName, String qName) throws SAXException
		{
			deferredWorkStack.pop().run();
		}
		
		
		@Override
		public void error(SAXParseException arg0) throws SAXException
		{
			throw arg0;
		}
		
		@Override
		public void warning(SAXParseException arg0) throws SAXException
		{
			System.err.println("Warning parsing XML");
			arg0.printStackTrace();
		}
		
	}

	public boolean readCPlusPlus()
	{
		return handler.isCPlusPlus;
	}
}
