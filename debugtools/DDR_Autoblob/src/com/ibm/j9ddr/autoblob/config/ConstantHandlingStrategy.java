/*******************************************************************************
 * Copyright (c) 2010, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob.config;

import java.io.File;
import java.io.IOException;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.xml.sax.SAXException;

import com.ibm.j9ddr.autoblob.GenerateBlobC.FileTypes;
import com.ibm.j9ddr.autoblob.constants.ConstantParser;
import com.ibm.j9ddr.autoblob.constants.ICVisitor;
import com.ibm.j9ddr.autoblob.datamodel.Constant;
import com.ibm.j9ddr.autoblob.datamodel.ConstantsType;
import com.ibm.j9ddr.autoblob.datamodel.EnumType;
import com.ibm.j9ddr.autoblob.datamodel.ITypeCollection;
import com.ibm.j9ddr.autoblob.datamodel.UserDefinedType;
import com.ibm.j9ddr.autoblob.linenumbers.SourceLocation;
import com.ibm.j9ddr.autoblob.xmlparser.J9FlagsXMLParser;

/**
 * Enumerator of the ways of handling constants.
 * 
 * @author andhall
 *
 */
public abstract class ConstantHandlingStrategy
{
	/**
	 * Extracts constants (numeric #defines) from the supplied C file
	 * @param cFile File to be parsed
	 * @param fileTypes Types from this file
	 * @param allTypes All known types from this compile module
	 * @return List of pseudo-types created to hold constants
	 * @throws IOException
	 */
	public final List<UserDefinedType> loadConstants(File cFile, FileTypes fileTypes, ITypeCollection allTypes) throws IOException, SAXException
	{
		loadMacroConstants(cFile, fileTypes, allTypes);
		
		attachAnonymousEnums(fileTypes);
		
		return getConstants();
	}
	
	public abstract void loadMacroConstants(File cFile, FileTypes fileTypes, ITypeCollection allTypes) throws IOException, SAXException;
	
	public abstract void configure(Properties prop, String prefix);
	
	protected abstract List<UserDefinedType> getConstants();
	
	protected void attachAnonymousEnums(FileTypes types)
	{
		if (types == null) {
			return;
		}
		
		for (EnumType thisType : types.anonymousEnums) {
			if (thisType.shouldBeAttachedAsConstants()) {
				for (Constant thisConstant : thisType.getConstants()) {
					attachAnonymousEnumConstant(thisType, thisConstant);
				}
			}
		}
	}
	
	protected abstract void attachAnonymousEnumConstant(EnumType t, Constant c);
	
	public static ConstantHandlingStrategy getConstantHandling(String type, File j9Flags)
	{
		if (type == null) {
			return new PseudoStructureConstantHandling();
		} else {
			type = type.toLowerCase();
			if (type.equals("builder")) {
				return new BuilderConstantHandling();
			} else if (type.equals("constantsanddefines")) {
				return new ConstantsAndDefinesHandling();
			} else if (type.equals("pseudostructure")) {
				return new PseudoStructureConstantHandling();
			} else if (type.equals("j9buildflags")) {
				return new J9BuildFlagsConstantHandling(j9Flags);
			} else if (type.equals("stackwalkconstants")) {
				return new StackWalkConstantHandling();
			} else if (type.equals("maptotype")) {
				return new MapToTypeConstantHandling();
			} else {
				throw new IllegalArgumentException("Unknown constant handling: " + type);
			}
		}
	}
	
	private static String getStructureNameFromFile(File cFile, String suffix)
	{
		String name = stripPath(cFile.getName());
		
		//Chop the .h off
		name = name.substring(0, name.indexOf("."));
		
		//Capital letter on the front
		name = name.substring(0,1).toUpperCase() + name.substring(1);
		
		//Convert any _ into camel case
		int index;
		
		while ( (index = name.indexOf("_")) != -1 ) {
			if (index < name.length() - 1) {
				name = name.substring(0, index) + Character.toUpperCase(name.charAt(index + 1)) + name.substring(index + 2);
			} else {
				name = name.substring(0, index);
			}
		}
		
		//Upper-case any letters that occur after numbers
		boolean lastCharWasNumeric = false;
		
		for (int i=0; i < name.length(); i++) {
			char thisChar = name.charAt(i);
			
			if (Character.isDigit(thisChar)) {
				lastCharWasNumeric = true;
			} else if (lastCharWasNumeric) {
				name = name.substring(0, i) + Character.toUpperCase(thisChar) + name.substring(i + 1);
				lastCharWasNumeric = false;
			} else {
				lastCharWasNumeric = false;
			}
		}
		
		name = name + suffix;
		
		return name;
	}
	
	private static String stripPath(String filename)
	{
		return new File(filename).getName();
	}
	
	/**
	 * The Builder constant handling strategy is:
	 * 
	 * * Any free constant is, by default, attached to the structure above it.
	 * * Unless we see a comment like this: "Constants from J9JITDataCacheConstants" - 
	 * in which case we infer a pseudo-type for the constants from the SmallTalk pool
	 *
	 */
	private static class BuilderConstantHandling extends ConstantHandlingStrategy implements ICVisitor
	{

		private static Pattern POOL_CONSTANT_COMMENT_PATTERN = Pattern.compile("Constants from (\\S*)");
		
		private static enum Mode {NORMAL, BUILDING_POOL_PSEUDO_TYPE};
		
		private ListIterator<UserDefinedType> typesIt;
		
		private List<UserDefinedType> pseudoTypes;
		
		private Mode mode;
		
		private File cFile;
		
		private UserDefinedType currentPseudoType;
		
		private UserDefinedType spareConstantsType;
		
		private int lastPseudoTypeLineNumber;
		
		@Override
		public void loadMacroConstants(File cFile, FileTypes tff, ITypeCollection allTypes) throws IOException, SAXException
		{
			//Build a list of structures ordered by line number.
			
			List<UserDefinedType> types = new LinkedList<UserDefinedType>();
			if (tff != null) {
				types.addAll(tff.typedefs);
				types.addAll(tff.structs.values());
			}
			Collections.sort(types, new TypeComparator());
			
			typesIt = types.listIterator();
			pseudoTypes = new LinkedList<UserDefinedType>();
			mode = Mode.NORMAL;
			this.cFile = cFile;
			
			ConstantParser parser = new ConstantParser();
			parser.visitCFile(cFile, this);
		}

		public void visitComment(String comment, int lineNumber)
		{
			Matcher m = POOL_CONSTANT_COMMENT_PATTERN.matcher(comment);
			
			if (m.find()) {
				String name = m.group(1);
				
				mode = Mode.BUILDING_POOL_PSEUDO_TYPE;
				currentPseudoType = new ConstantsType(name, new SourceLocation(cFile.getAbsolutePath(), lineNumber));
				pseudoTypes.add(currentPseudoType);
				lastPseudoTypeLineNumber = lineNumber;
			}
		}
		
		public void visitNumericConstant(String name, int lineNumber)
		{
			switch (mode) {
			case NORMAL:
				Constant c = new Constant(name);
				attachConstantToClosestStructure(c, lineNumber);
				break;
			case BUILDING_POOL_PSEUDO_TYPE :
				if (lineNumber - lastPseudoTypeLineNumber > 1) {
					//We've walked past the end of the this pool declaration. Back to regular mode
					mode = Mode.NORMAL;
					//Recurse to process this constant again.
					visitNumericConstant(name, lineNumber);
					return;
				} else {
					currentPseudoType.addConstant(new Constant(name));
					lastPseudoTypeLineNumber = lineNumber;
				}
				break;
			default:
				throw new RuntimeException("Unknown mode: " + mode);
			}
		}

		private void attachConstantToClosestStructure(Constant c, int lineNumber)
		{
			//Find a structure to attach this to.
			UserDefinedType working = null;
			
			if (typesIt.hasPrevious()) {
				working = typesIt.previous();
			} else if (typesIt.hasNext()) {
				working = typesIt.next();
			} else {
				addSpareConstant(c);
				return;
			}
			
			while ((working.getDefinitionLocation().getLineNumber() < lineNumber) && typesIt.hasNext()) {
				working = typesIt.next();
			}
			
			if (working.getDefinitionLocation().getLineNumber() > lineNumber) {
				if (typesIt.hasPrevious()) {
					typesIt.previous();
					
					if (typesIt.hasPrevious()) {
						working = typesIt.previous();
					} else {
						addSpareConstant(c);
						return;
					}
				} else {
					addSpareConstant(c);
					return;
				}
			}
			working.addConstant(c);
		}

		private void addSpareConstant(Constant c)
		{
			if (spareConstantsType == null) {
				spareConstantsType = new ConstantsType(getStructureNameFromFile(cFile, "Constants"), null);
				pseudoTypes.add(spareConstantsType);
			}
			spareConstantsType.addConstant(c);
		}

		@Override
		public void configure(Properties prop, String prefix)
		{
			//Do nothing
		}

		public void visitNoValueConstant(String name, boolean defined)
		{
			//Do nothing
		}

		@Override
		protected void attachAnonymousEnumConstant(EnumType t, Constant c)
		{
			attachConstantToClosestStructure(c, t.getDefinitionLocation().getLineNumber());
		}

		@Override
		protected List<UserDefinedType> getConstants()
		{
			return pseudoTypes;
		}

		
	}
	
	private static class ConstantsAndDefinesHandling extends BuilderConstantHandling implements ICVisitor
	{
		private UserDefinedType defines;
		private String nameOverride;
		private static final String BASE_NAME_PROPERTY = "baseName";
		
		private List<UserDefinedType> toReturn = new LinkedList<UserDefinedType>();
		
		@Override
		public void configure(Properties prop, String prefix)
		{
			nameOverride = prop.getProperty(prefix + "." + BASE_NAME_PROPERTY);
		}

		@Override
		public void loadMacroConstants(File cFile, FileTypes tff, ITypeCollection allTypes)
				throws IOException, SAXException
		{
			defines = new ConstantsType(getStructureName(cFile), null);
			super.loadMacroConstants(cFile, tff, allTypes);
			
			toReturn.addAll(super.getConstants());
			toReturn.add(defines);
		}

		private String getStructureName(File cFile)
		{
			if (null != nameOverride) {
				return nameOverride;
			}
			
			return getStructureNameFromFile(cFile, "Flags");
		}
		
		@Override
		protected List<UserDefinedType> getConstants()
		{
			return Collections.unmodifiableList(toReturn);
		}
		
		public void visitNoValueConstant(String name, boolean defined)
		{
			if (null != defines) {
				Constant c = new Constant(name, "#if defined(" + name + ")");
				defines.addConstant(c);
			}
		}
	}
	
	private static class PseudoStructureConstantHandling extends ConstantHandlingStrategy implements ICVisitor
	{
		private UserDefinedType structure;
		
		private String nameOverride;
		
		@Override
		public void loadMacroConstants(File cFile, FileTypes tff, ITypeCollection allTypes) throws IOException
		{
			ConstantParser parser = new ConstantParser();
			
			//Much of the GC code expects a map-to-type behaviour where the constants from Something.hpp are mapped to MM_Something.
			//We replicate that behaviour here by default
			
			String gcStructureName = getGCStructureName(cFile);
			UserDefinedType gcStructure = tff != null ? tff.findType(gcStructureName) : null;
			
			if (gcStructure != null) {
				structure = gcStructure;
			} else {
				structure = new ConstantsType(getStructureName(cFile),null);
			}
			
			parser.visitCFile(cFile, this);
		}
		
		private String getGCStructureName(File cFile)
		{
			String name = cFile.getName();
			
			int index = name.indexOf('.');
			
			if (index != -1) {
				name = name.substring(0, index);
			}
			
			return "MM_" + name;
		}

		private String getStructureName(File cFile)
		{
			if (nameOverride != null) {
				return nameOverride;
			}
			
			return getStructureNameFromFile(cFile, "Constants");
		}


		public void visitComment(String comment, int lineNumber)
		{
			//Do nothing
		}

		public void visitNumericConstant(String name, int lineNumber)
		{
			structure.addConstant(new Constant(name));
		}

		public void configure(Properties prop, String prefix)
		{
			nameOverride = prop.getProperty(prefix + "." + "pseudostructure");
		}

		public void visitNoValueConstant(String name, boolean defined)
		{
			
		}

		@Override
		protected void attachAnonymousEnumConstant(EnumType t, Constant c)
		{
			structure.addConstant(c);
		}

		@Override
		protected List<UserDefinedType> getConstants()
		{
			if (structure instanceof ConstantsType) {
				return Collections.<UserDefinedType>singletonList(structure);
			} else {
				return Collections.emptyList();
			}
		}
		
	}
	
	private static class J9BuildFlagsConstantHandling extends ConstantHandlingStrategy implements ICVisitor
	{
		
		private UserDefinedType j9buildflags = new ConstantsType("J9BuildFlags", null);
		private Map<String, String> cNameToIdMap;
		private File j9FlagsFile;
		
		J9BuildFlagsConstantHandling(File j9FlagsFile)
		{
			this.j9FlagsFile = j9FlagsFile;
		}
		
		@Override
		public void configure(Properties prop, String prefix)
		{
			
		}

		@Override
		public void loadMacroConstants(File cFile, FileTypes tff, ITypeCollection allTypes) throws IOException, SAXException
		{
			if (null != j9FlagsFile) {
				/* We read in the j9.flags xml file here and create a mapping here between flag ids and their equivalent cNames.
				 * We'll use this mapping again when reading the j9cfg.h macro constants so that we can use the more human-readable
				 * flag ids as constants.  
				 */
				J9FlagsXMLParser flagsParser = new J9FlagsXMLParser(j9FlagsFile);
				cNameToIdMap = flagsParser.getCNameToFlagIdMap();	
			
				ConstantParser parser = new ConstantParser();
				
				parser.visitCFile(cFile, this);
			} else {
				/* This the right exception? */
				throw new UnsupportedOperationException("no j9.flags specified");
			}
		}

		public void visitComment(String comment, int lineNumber)
		{
			//Do nothing
		}

		public void visitNoValueConstant(String name, boolean defined)
		{
			String constantName = convertBuildFlagName(name);
			
			if (constantName != null) {
				j9buildflags.addConstant(new Constant(constantName, defined ? "1" : "0"));
			}
		}
		
		/*
		 * J9VM_BUILD_DROP_TO_HURSLEY => build_dropToHursley
		 */
		private String convertBuildFlagName(String input)
		{
			String output = null;
			String cName = null;

			if (input.startsWith("J9VM_")) {
				cName = cNameToIdMap.get(input.substring(5));
				if (null != cName) {
					output = cName;
				} else {
					//couldn't find mapping, just return the input. Macro defines like J9VM_DIRECT_FUNCTION_POINTERS appear here 
					output = input;
				}
			} else {
				output = null;
			}
			
			return output;
		}

		public void visitNumericConstant(String name, int lineNumber)
		{
			//Do nothing
		}

		@Override
		protected void attachAnonymousEnumConstant(EnumType t, Constant c)
		{
			//Do nothing
		}

		@Override
		protected List<UserDefinedType> getConstants()
		{
			return Collections.<UserDefinedType>singletonList(j9buildflags);
		}
		
	}
	
	private static class StackWalkConstantHandling extends ConstantHandlingStrategy implements ICVisitor
	{
		private UserDefinedType constants;
		private UserDefinedType flags;
		private String baseName;
		private static final String BOOLEAN_CONSTANTS_PROPERTY = "booleanConstants";
		private static final String BASE_NAME_PROPERTY = "baseName";
		
		private Set<String> booleanConstants = new HashSet<String>();
		
		private List<UserDefinedType> toReturn = new LinkedList<UserDefinedType>();

		@Override
		public void configure(Properties prop, String prefix)
		{
			String booleanConstantsString = prop.getProperty(prefix + "." + BOOLEAN_CONSTANTS_PROPERTY);
			
			if (booleanConstantsString != null) {
				String components[] = booleanConstantsString.split(",");
				
				for (String thisComponent : components) {
					booleanConstants.add(thisComponent.trim());
				}
			}
			
			String baseNameString = prop.getProperty(prefix + "." + BASE_NAME_PROPERTY);
			
			if (baseNameString == null) {
				throw new IllegalArgumentException("Missing " + BASE_NAME_PROPERTY + " value for " + prefix);
			}
			
			baseName = baseNameString;
		}

		@Override
		public void loadMacroConstants(File cFile, FileTypes tff, ITypeCollection allTypes)
				throws IOException
		{
			ConstantParser parser = new ConstantParser();
			
			constants = new ConstantsType(baseName + "Constants", null);
			flags = new ConstantsType(baseName + "Flags", null);
			
			parser.visitCFile(cFile, this);
			
			toReturn.add(constants);
			toReturn.add(flags);
		}
		
		@Override
		protected List<UserDefinedType> getConstants()
		{
			return Collections.unmodifiableList(toReturn);
		}

		public void visitComment(String comment, int lineNumber)
		{
			//Do nothing
		}

		public void visitNoValueConstant(String name, boolean defined)
		{
			if (booleanConstants.contains(name)) {
				flags.addConstant(new Constant(name, "#if defined(" + name + ")"));
			} else {
				//This is normally an integer field that isn't defined on this platform.
				flags.addConstant(new Constant(name + "_DEFINED", "#if defined(" + name + ")"));
				if (! defined) {
					//Add a placeholder value (allows depending on the undefined value to compile). Shouldn't actually be used.
					constants.addConstant(new Constant(name, "0"));
				} else {
					System.err.println("Unexpected undefined StackWalk constant: " + name + ". Not included in blob.");
				}
			}
		}

		public void visitNumericConstant(String name, int lineNumber)
		{
			constants.addConstant(new Constant(name));
			flags.addConstant(new Constant(name + "_DEFINED", "#if defined(" + name + ")"));
		}

		@Override
		protected void attachAnonymousEnumConstant(EnumType t, Constant c)
		{
			//Do nothing
		}
		
	}
	
	private static class MapToTypeConstantHandling extends ConstantHandlingStrategy implements ICVisitor
	{
		private String typeName;
		
		private UserDefinedType type;
		
		@Override
		public void configure(Properties prop, String prefix)
		{
			String property = prefix + ".maptotype";
			typeName = prop.getProperty(property);
			
			if (null == typeName) {
				throw new IllegalArgumentException("Type to map to not specified ( " + property + " missing)");
			}
		}

		@Override
		public void loadMacroConstants(File cFile, FileTypes tff, ITypeCollection allTypes)
				throws IOException
		{
			//Find the type to map to
			type = allTypes.findType(typeName);
			
			if (null == type) {
				throw new Error("Didn't find map-to-type: " + typeName);
			}
			
			ConstantParser parser = new ConstantParser();
			
			parser.visitCFile(cFile, this);
		}

		public void visitComment(String comment, int lineNumber)
		{
			//Do nothing
		}

		public void visitNoValueConstant(String name, boolean defined)
		{
			//Do nothing
		}

		public void visitNumericConstant(String name, int lineNumber)
		{
			type.addConstant(new Constant(name));
		}

		@Override
		protected void attachAnonymousEnumConstant(EnumType t, Constant c)
		{
			type.addConstant(c);
		}

		@Override
		protected List<UserDefinedType> getConstants()
		{
			return Collections.singletonList(type);
		}
		
	}

	private static final class TypeComparator implements Comparator<UserDefinedType>
	{
		public int compare(UserDefinedType one, UserDefinedType two)
		{
			SourceLocation locOne = one.getDefinitionLocation();
			SourceLocation locTwo = two.getDefinitionLocation();
			
			if (locOne == null && locTwo == null) {
				return 0;
			} else if (locOne != null && locTwo == null) {
				return 1;
			} else if (locOne == null && locTwo != null) {
				return -1;
			} else {
				return locOne.getLineNumber() - locTwo.getLineNumber();
			}
		}
	}
}
