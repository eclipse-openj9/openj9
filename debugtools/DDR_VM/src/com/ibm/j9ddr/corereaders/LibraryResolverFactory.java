/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Constructor;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.libraries.CoreFileResolver;

import static java.util.logging.Level.*;

/**
 * Static factory for ILibraryResolvers.
 * 
 * Encapsulates the knowledge of where to look for libraries.
 * 
 * @author andhall
 *
 */
public class LibraryResolverFactory
{
	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

	// This sets the library path to search for the LibraryPathResolver.
	public static final String LIBRARY_PATH_SYSTEM_PROPERTY = "com.ibm.j9ddr.library.path";
	
	/**
	 * This property sets a path that can be used to re-map library paths on the system
	 * the core was generated to paths on the local machine. For example allowing you to
	 * use a core generated on AIX with the the same build downloaded to a Windows machine
	 * from Espresso. You can get the exact build id from core files in jdmpview
	 * by running "info system"
	 *  
	 * The output of "info system" should contain a line like:
	 * Java(TM) SE Runtime Environment(build JRE 1.6.0 AIX ppc64-64 build 20101125_69326 (pap6460_26-20101129_04))
	 * where pap6460_26-20101129_04 is the build id identifying the build on Espresso or JIM.
	 * 
	 * Paths can be substituted as:
	 * -Dcom.ibm.j9ddr.path.mapping=original/path/1=new/path/1:original/path/2=new/path/2
	 * (Note that : or ; is used to separate paths according to the platforms setting of
	 * of File.pathSeparator. = is used to separate multiple pairs of mappings.)
	 */
	public static final String PATH_MAPPING_SYSTEM_PROPERTY = "com.ibm.j9ddr.path.mapping";
	private static final String MAPPING_SEPERATOR = "=";

	//This list determines library resolver search order
	public static final String RESOLVER_LIST_PROPERTY = "com.ibm.j9ddr.library.resolvers";
	private static final String RESOLVER_DEFAULT_ORDER =
			LibraryPathResolver.class.getName()
			+ ","
			+ CoreFileResolver.class.getName()
			+ ","
			+ NextToCoreResolver.class.getName()
			+ ","
			+ InPlaceResolver.class.getName()
			+ ","
			+ ZipFileResolver.class.getName();
	
	private static final List<File> libraryPath;
	private static final Map<String, String> pathMap;
	private static final List<Class<? extends ILibraryResolver>> resolverClasses;
	
	static {
		String specifiedPath = AccessController.doPrivileged(new PrivilegedAction<String>() {

			public String run()
			{
				return System.getProperty(LIBRARY_PATH_SYSTEM_PROPERTY);
			}
		});
		
		String mappingPath = AccessController.doPrivileged(new PrivilegedAction<String>() {

			public String run()
			{
				return System.getProperty(PATH_MAPPING_SYSTEM_PROPERTY);
			}
		});

		String resolvers = AccessController.doPrivileged(new PrivilegedAction<String>() {

			public String run()
			{
				return System.getProperty(RESOLVER_LIST_PROPERTY, RESOLVER_DEFAULT_ORDER);
			}
		});
		
		final String pathSeperator = File.pathSeparator;
		
		List<File> localPath = new LinkedList<File>();
		
		if (specifiedPath != null) {
			logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "Library search path set as: {0} by property {1}",new Object[]{specifiedPath,LIBRARY_PATH_SYSTEM_PROPERTY});
			String pathSections[] = specifiedPath.split(pathSeperator);
			
			for (String path : pathSections) {
				localPath.add(new File(path));
			}
		} else {
			logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "No library search path set. Falling back on defaults");
		}
		
		Map<String, String> mPaths = new HashMap<String, String>();
		
		if (mappingPath != null) {
			logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "Library path mappings paths set as: {0} by property {1}",new Object[]{specifiedPath,PATH_MAPPING_SYSTEM_PROPERTY});
			String pathSections[] = mappingPath.split(pathSeperator);
			
			for (String replacePath : pathSections) {
				String[] target_subst = replacePath.split(MAPPING_SEPERATOR); 
				mPaths.put(target_subst[0], target_subst[1]);
				logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "Mapping libraries paths containing {0} to {1}",target_subst);
			}
		} else {
			logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "No library path mappings set. Falling back on defaults");
		}
		
		List<Class<? extends ILibraryResolver>> classes = new LinkedList<Class<? extends ILibraryResolver>>();
		logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "Library resolver search order: {0}",new Object[]{resolvers});
		for( String resolver: resolvers.split(",") ) {
			try {
				classes.add( Class.forName(resolver).asSubclass(ILibraryResolver.class));
			} catch (ClassNotFoundException e) {
				logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "Failed to find Library resolver class: {0}",new Object[]{resolver});
			}
		}
		
		libraryPath = Collections.unmodifiableList(localPath);
		pathMap = Collections.unmodifiableMap(mPaths);
		resolverClasses = Collections.unmodifiableList(classes);
	}
	
	/**
	 * Static factory for ILibraryResolvers.
	 * @param stream Core file whose libraries need resolving
	 * @return ILibraryResolver
	 */
	public static ILibraryResolver getResolverForCoreFile(ImageInputStream stream)
	{
		List<ILibraryResolver> resolvers = new LinkedList<ILibraryResolver>();
		
		//This list determines library path search order
		for( Class<? extends ILibraryResolver> resolverClass : resolverClasses ) {

			try {
				// We only have one argument we can pass, the coreFile we were passed.
				// If the LibraryResolver can take that as an argument to it's constructor
				// we will pass it. Otherwise we use the no-args constructor.
				Constructor<? extends ILibraryResolver> constructor = resolverClass.getConstructor(ImageInputStream.class);
				resolvers.add(constructor.newInstance(stream));
			} catch (NoSuchMethodException e) {
				logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "Skipping Library resolver class: {0} as it does not support an ImageInputStream constructor",new Object[]{resolverClass.getName()});
			} catch (Exception e) {
				logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "Failed to create instance of Library resolver class: {0}",new Object[]{resolverClass.getName()});
			}

		}
		return new DelegatingLibraryPathResolver(resolvers);
	}
	
	/**
	 * Static factory for ILibraryResolvers.
	 * @param stream Core file whose libraries need resolving
	 * @return ILibraryResolver
	 */
	public static ILibraryResolver getResolverForCoreFile(File file)
	{
		List<ILibraryResolver> resolvers = new LinkedList<ILibraryResolver>();
		
		//This list determines library path search order
		for( Class<? extends ILibraryResolver> resolverClass : resolverClasses ) {

			try {
				// We only have one argument we can pass, the coreFile we were passed.
				// If the LibraryResolver can take that as an argument to it's constructor
				// we will pass it. Otherwise we use the no-args constructor.
				Constructor<? extends ILibraryResolver> constructor = resolverClass.getConstructor(File.class);
				resolvers.add(constructor.newInstance(file));
			} catch (NoSuchMethodException e) {
				logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "Skipping Library resolver class: {0} as it does not support a File constructor",new Object[]{resolverClass.getName()});
			} catch (Exception e) {
				logger.logp(FINE, "LibraryResolverFactory", "<clinit>", "Failed to create instance of Library resolver class: {0}",new Object[]{resolverClass.getName()});
			}

		}
		return new DelegatingLibraryPathResolver(resolvers);
	}
	
	private static String stripPath(String moduleName)
	{
		File file = new File(moduleName);
		
		return file.getName();
	}
	
	private static String mapPath(String moduleName)
	{
		if( pathMap.isEmpty() ) {
			return moduleName;			
		}
		for( Entry<String, String> entry: pathMap.entrySet()) {
			moduleName = moduleName.replace(entry.getKey(), entry.getValue());
		}
		return moduleName;
	}

	private final static class DelegatingLibraryPathResolver extends BaseResolver implements ILibraryResolver
	{
		private final List<ILibraryResolver> resolvers;
		
		public DelegatingLibraryPathResolver(List<ILibraryResolver> resolvers)
		{
			this.resolvers = resolvers;
		}
		
		public LibraryDataSource getLibrary(String fileName, boolean silent)
				throws FileNotFoundException
		{
			for (ILibraryResolver resolver : resolvers) {
				try {
					return resolver.getLibrary(fileName);
				} catch (FileNotFoundException e) {
					//Do nothing
				}
			}
			
			if (! silent) {
				logger.logp(FINER,"LibraryResolverFactory","getLibrary","Couldn't find shared library " + fileName + ". Some data may be unavailable.");
			}
			
			throw new FileNotFoundException();
		}

		@Override
		public void dispose() {
			for (ILibraryResolver resolver : resolvers) {
				resolver.dispose();
			}
		}
		
		
	}
	
	/**
	 * Searches through the library path. If any entry is a file, we try to open it as a zipfile and 
	 * look through the contents.
	 * 
	 * @author andhall
	 */
	private final static class LibraryPathResolver extends BaseResolver implements ILibraryResolver
	{

		@SuppressWarnings("unused")
		public LibraryPathResolver(File coreFile) {}
		
		public LibraryDataSource getLibrary(String fileName, boolean resolver)
				throws FileNotFoundException
		{
			if( libraryPath.isEmpty() ) {
				throw new FileNotFoundException("No library paths set.");
			}
			
			String strippedModuleName = stripPath(fileName);
			
			logger.logp(FINER,"LibraryResolverFactory$LibraryPathResolver","getLibrary","Looking for {0} in library path.", strippedModuleName);
			
			for (File path : libraryPath) {
				if (path.isDirectory()) {
					logger.logp(FINEST,"LibraryResolverFactory$LibraryPathResolver","getLibrary","Looking in directory {0}.", path);
					File withDirectories = new File(path, fileName);
					File withoutDirectories = new File(path, strippedModuleName);
					
					if (withDirectories.isFile()) {
						logger.logp(FINER,"LibraryResolverFactory$LibraryPathResolver","getLibrary","Found {0} in directory {1}.", new Object[]{strippedModuleName,path});
						return new LibraryDataSource(withDirectories);
					}
					
					if (withoutDirectories.isFile()) {
						logger.logp(FINER,"LibraryResolverFactory$LibraryPathResolver","getLibrary","Found {0} in directory {1}.", new Object[]{strippedModuleName,path});
						return new LibraryDataSource(withoutDirectories);
					}
				} else {
					logger.logp(FINEST,"LibraryResolverFactory$LibraryPathResolver","getLibrary","Looking in file {0}.", path);
					File libraryFile = readArchive(path, fileName, strippedModuleName);
					
					if (libraryFile != null) {
						logger.logp(FINER,"LibraryResolverFactory$LibraryPathResolver","getLibrary","Found {0} in zip file {1}.", new Object[]{strippedModuleName,path});
						return new LibraryDataSource(libraryFile);
					}
				}
			}
			
			logger.logp(FINER,"LibraryResolverFactory$LibraryPathResolver","getLibrary","{0} not found in library path.", strippedModuleName);
			
			throw new FileNotFoundException();
		}

		private File readArchive(File path, String filePath, String strippedModuleName)
		{
			try {
				ZipFile file = new ZipFile(path);
				
				try {
					Enumeration<? extends ZipEntry> entries = file.entries();
					
					ZipEntry bestMatch = null;
					
					while (entries.hasMoreElements()) {
						ZipEntry thisEntry = entries.nextElement();
						
						if (thisEntry.getName().equals(filePath)) {
							bestMatch = thisEntry;
							break;
						}
						
						if (stripPath(thisEntry.getName()).equals(strippedModuleName)) {
							bestMatch = thisEntry;
							//Not an exact match - keep looking
						}
						
					}
					
					if (bestMatch != null) {
						return extractEntry(bestMatch, strippedModuleName, file);
					}
					
					return null;
				} finally {
					file.close();
				}
			} catch (Exception e) {
				logger.logp(Level.WARNING,"LibraryResolverFactory$LibraryPathResolver", "readArchive", "Problems reading " + path + " as a zip file", e);				
				return null;
			}
		}

		private File extractEntry(ZipEntry thisEntry, String moduleName, ZipFile file) throws FileNotFoundException
		{
			try {
				File libraryFile = File.createTempFile(moduleName, ".j9ddrlib");
				
				InputStream in = file.getInputStream(thisEntry);
				
				try {
					OutputStream out = new FileOutputStream(libraryFile);
					
					try {
						//Set ShutdownHook to clean up.
						libraryFile.deleteOnExit();
					
						byte[] buffer = new byte[4096];
						int read;
						
						while ((read = in.read(buffer)) != -1) {
							out.write(buffer,0,read);
						}
						
						return libraryFile;
					} finally {
						try {
							out.close();
						} catch (IOException e) {
							//Do nothing
						}
					}
					
				} finally {
					try {
						in.close();
					} catch (IOException e) {
						//Do nothing
					}
				}
			} catch (IOException e) {
				logger.logp(Level.WARNING, "LibraryResolverFactory$LibraryPathResolver", "extractEntry", "IOException trying to extract " + thisEntry.getName() + " from " + file.getName(), e);
				return null;
			}
		}
		
	}
	
	/**
	 * Looks for the library next to the core file.
	 *
	 */
	private final static class NextToCoreResolver extends BaseResolver implements ILibraryResolver
	{
		private final File coreDirectory;
		
		@SuppressWarnings("unused")
		public NextToCoreResolver(File coreFile)
		{
			coreDirectory = coreFile.getAbsoluteFile().getParentFile();
		}
		
		private boolean isAbsolutePath(String name) {
			if((null == name) || (name.length() == 0)) {
				return false;
			}
			if(name.length() == 1) {
				return (name.charAt(0) == '/');
			} else {
				return (name.charAt(0) == '/') || (name.charAt(1) == ':');
			}
		}
		
		//recursively searches the directory tree for a relative path
		private File findFile(File dir, String[] parts, int matchDepth) {
			File[] files = dir.listFiles();
			File current = null;
			for(int i = 0; files != null && i < files.length && (null == current); i++) {
				File file = files[i];
				if(file.getName().equals(parts[matchDepth])) {
					if(++matchDepth == parts.length) {
						return file;
					} else {
						if(file.isDirectory()) {
							current = findFile(file, parts, matchDepth);
						}
					}
				} else {
					if(file.isDirectory()) {
						current = findFile(file, parts, 0);
					}
				}
			}
			return current;		//couldn't find it
		}
		
		public LibraryDataSource getLibrary(String fileName, boolean silent)
				throws FileNotFoundException
		{
			String strippedModuleName = stripPath(fileName);
			String separator = null;
			if(fileName.indexOf('/') == -1) {
				if(fileName.indexOf('\\') != -1) {
					separator = "\\\\";
				}
			} else {
				separator = "/";
			}
			String canonicalName = (separator == null) ? fileName : normalise(fileName, separator);
			if(!isAbsolutePath(canonicalName)) {
				//if a relative path is passed then need to find it below the directory for the core file
				if(separator != null) {
					String[] parts = canonicalName.split(separator);
					File file = findFile(coreDirectory, parts, 0);
					if(file != null) {
						String abspath = file.getAbsolutePath();
						canonicalName = abspath.substring(coreDirectory.getAbsolutePath().length() + 1, abspath.length());
					}
				}
			}
			//Try with and without the extra directories in between
			File withoutDirectories = new File(coreDirectory,strippedModuleName);
			File withDirectories = new File(coreDirectory,canonicalName);
			final String fileNameWithCase = withDirectories.getName();

			File[] pathsToCheck = new File[] {withDirectories, withoutDirectories};
			for( final File checkPath : pathsToCheck) {
				if (checkPath.getParentFile() != null && checkPath.getParentFile().isDirectory())  {
					logger.logp(FINER,"LibraryResolverFactory$NextToCoreResolver","getLibrary","Trying {0}.", withoutDirectories);
					// Make this case sensitive. This should return the original name which will
					// still possess mixed case on Windows.
					File[] matches = checkPath.getParentFile().listFiles(new FilenameFilter() {

						public boolean accept(File dir, String name) {
							if( name.equals( fileNameWithCase ) ) {
								File f = new File(dir, name);
								if( f.isFile() ) {
								return true;
							}
							}
							if( name.equalsIgnoreCase( fileNameWithCase ) ) {
								logger.logp(SEVERE,"LibraryResolverFactory$NextToCoreResolver","getLibrary","Found {0} but not {1} in {2}. Case sensitive files may been copied to a case insensitive file system causing {1} to be lost or overwritten by {0}.", new String[] {name, fileNameWithCase, checkPath.getParent()});
							}
							return false;
						}
					});
					if( matches.length == 1 ) {
						// Matches should only have one element if successful.
						logger.logp(FINER,"LibraryResolverFactory$NextToCoreResolver","getLibrary","Found {0}.", checkPath);
						return new LibraryDataSource(matches[0]);
					}
				} 
			}
			throw new FileNotFoundException("Can't find " + strippedModuleName + " in directory " + coreDirectory.getAbsolutePath());
		}
	}
	
	/**
	 * Looks for the library on disk where the core file says it is. Will only work on the machine
	 * that took the dump unless a mapping path has been set to map the paths correctly.
	 *
	 */
	private final static class InPlaceResolver extends BaseResolver implements ILibraryResolver
	{
	
		@SuppressWarnings("unused")
		public InPlaceResolver(File coreFile) {}
		
		public LibraryDataSource getLibrary(String fileName, boolean silent)
				throws FileNotFoundException
		{
			File library = new File(mapPath(fileName));
		
			logger.logp(FINER,"LibraryResolverFactory$InPlaceResolver","getLibrary","Looking for {0}.", library);
			
			if (library.isFile()) {
				logger.logp(FINER,"LibraryResolverFactory$InPlaceResolver","getLibrary","Found {0}.", library);
				return new LibraryDataSource(library);
			} else {
				logger.logp(FINER,"LibraryResolverFactory$InPlaceResolver","getLibrary","Can't find " +  fileName + " on disk");
				throw new FileNotFoundException("Can't find " + fileName + " on disk");
			}
		}
		
	}
	
	private static abstract class BaseResolver implements ILibraryResolver 
	{
		/*
		 * File names are normalised because on Windows if the file name is created with a .. in it then that is 
		 * persisted in a subsequently created File object. File.equals will then fail if comparisons are made
		 * with files created from names with and without .. even if they point to the same file. The normalisation
		 * resolves any relative sections of the path.
		 */
		protected static String normalise(String name, String separator) {
			//escape Windows separator for regex
			String[] parts = name.split(separator);
			if(1 >= parts.length) {
				// cannot split so return originally passed name
				return name;
			}
			int reader = parts.length - 1;
			int marker = reader;
			String output = new String();
			for(; reader >= 0; marker--, reader--) {
				if(parts[marker].equals("..")) {
					//parent reference, skip to next path section
					reader--;
					continue;
				}
				if(parts[marker].equals(".")) {
					//self reference, ignore
					continue;
				}
				output = separator + parts[reader] + output;
			}
			if(output.startsWith(separator)) {
				output = output.substring(separator.length());
			}
			return output;
		}
		
		
		public LibraryDataSource getLibrary(String fileName) throws FileNotFoundException
		{
			return getLibrary(fileName, false);
		}
		
		public void dispose() {
			// default of a no-op
		}
	}
}
