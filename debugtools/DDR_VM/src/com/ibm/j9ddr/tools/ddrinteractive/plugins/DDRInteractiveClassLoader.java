/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

package com.ibm.j9ddr.tools.ddrinteractive.plugins;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.lang.annotation.Annotation;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.logging.Level;
import java.util.logging.Logger;

import jdk.internal.org.objectweb.asm.AnnotationVisitor;
import jdk.internal.org.objectweb.asm.Attribute;
import jdk.internal.org.objectweb.asm.ClassReader;
import jdk.internal.org.objectweb.asm.ClassVisitor;
import jdk.internal.org.objectweb.asm.FieldVisitor;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;

import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.ICommand;
import com.ibm.j9ddr.tools.ddrinteractive.annotations.DebugExtension;

/**
 * DDR Interactive classloader which is responsible for finding all classes specified by the plugins property.
 * 
 * @author apilkington
 *
 */
public class DDRInteractiveClassLoader extends ClassLoader {
	/**
	 * System property which will be used to scan for plugins.
	 */
	public static final String PLUGIN_SYSTEM_PROPERTY = "plugins";
	/**
	 * The name of the environment variable which will set the plugin search path.
	 * An explicitly set system property will override this setting.
	 */
	public static final String PLUGIN_ENV_VAR = "com.ibm.java.diagnostics.plugins";			//env var for platforms that allow .
	public static final String PLUGIN_ENV_VAR_ALT = "com_ibm_java_diagnostics_plugins";		//env var for platforms that do not allow .
	private static final String FILE_EXT_JAR = ".jar";			//jar file extension
	private static final String FILE_EXT_CLASS = ".class";		//class file extension
	private static final String VM_ALLVERSIONS = "*";		//indicator for support in all VM versions
	private Logger logger = Logger.getLogger(getClass().getName());
	Level logLevelForPluginLoadFailures = Level.WARNING; // emit warnings to DDRInteractive console
	private final String vmversion;
	private final ArrayList<PluginConfig> pluginCache = new ArrayList<PluginConfig>();		//list of plugins successfully loaded
	private final ArrayList<PluginConfig> pluginFailures = new ArrayList<PluginConfig>();	//list of plugins which failed to load
	protected static ArrayList<String> runtimeCommandClasses = new ArrayList<String>();	//classes that implement ICommand and are added after startup
	private final ArrayList<File> pluginSearchPath = new ArrayList<File>();	//specify classloader paths by URI as this can be used by the File class and converted into a URL
	
	public DDRInteractiveClassLoader(IVMData vmdata) throws DDRInteractiveCommandException {
		this(vmdata, vmdata.getClassLoader());
	}

	/**
	 * @throws DDRInteractiveCommandException if the -Dplugins property contains a bad path
	 */
	public DDRInteractiveClassLoader(IVMData vmdata, ClassLoader loader) throws DDRInteractiveCommandException {
		super(loader);
		vmversion = vmdata.getVersion();
		configureSearchPath();
		loadPlugins();
	}
	
	/**
	 * Check to see if a class has the version annotation and if so whether it matches the version of the currently
	 * running vm.
	 * 
	 * @param url
	 * @param clazz
	 */
	@SuppressWarnings("unchecked")
	private void examineClass(URL url, Class<?> clazz) {
		if(clazz.isAnnotationPresent(DebugExtension.class)) {
			Annotation a = clazz.getAnnotation(DebugExtension.class);
			if(null != a) {
				String value = ((DebugExtension) a).VMVersion();
				String[] versions = value.split(",");
				for(String version : versions) {
					if(version.equals(vmversion) || version.equals(VM_ALLVERSIONS)) {
						if(hasCommandIFace(clazz)) {
							PluginConfig config = new PluginConfig(clazz.getName(), vmversion, (Class<ICommand>)clazz, true, url);
							pluginCache.add(config);
							logger.fine("Added command " + clazz.getName());
						} else {
							logger.fine("Skipping annotated command which did not implement the ICommand interface : " + clazz.getName());
						}
					} else {
						String msg = String.format("Skipping %s as wrong VM version [allowed = %s, * : actual = %s]", clazz.getName(), vmversion, version);
						logger.fine(msg);
					}
				}
			}
		}
	}
	
	private void definePackage(String name)
	{
		//Split off the class name
		int finalSeparator = name.lastIndexOf("/");
		
		if (finalSeparator != -1) {
			String packageName = name.substring(0, finalSeparator);
			
			if (getPackage(packageName) != null) {
				return;
			}
			
			definePackage(packageName, "J9DDR", "0.1", "IBM", "J9DDR", "0.1", "IBM", null);
		}
	}
	
	/**
	 * At the bottom of this file is a main and a special constructor which is useful for running and testing 
	 * this class loader as a stand-alone application
	 */
	
	//if the required system property has been set then parse it and add the paths to the classloader search path
	private void configureSearchPath() {
		// The search path can be set by a system property or environment variable with the sys prop taking precedence
		// TODO handle quoted string, blanks in the file name
		String property = System.getProperty(PLUGIN_SYSTEM_PROPERTY);
		if(null == property) {
			property = System.getenv(PLUGIN_ENV_VAR);
			if(property == null) {
				property = System.getenv(PLUGIN_ENV_VAR_ALT);
			}
		}
		if((null == property) || (property.length() == 0)) {
			//no plugin path was specified
			logger.fine("No system property called " + PLUGIN_SYSTEM_PROPERTY + " was found");
			return;		
		}
		logger.fine("Plugins search path = " + property);
		//break the path up and add to classloader search path
		String[] parts = property.split(File.pathSeparator);
		for(int i = 0; i < parts.length; i++) {
			try {
				File file = new File(parts[i]);
				pluginSearchPath.add(file);
			} catch (Exception e) {
				logger.warning("Failed to create a URI or URL from " + parts[i]);
			}
		}
	}
	
	/**
	 * Used with asm's ClassReader - checks if a given class has a DTFJPlugin annotation and reads its name
	 * @author blazejc
	 *
	 */
	private class DTFJPluginSnifferVisitor extends ClassVisitor {
		private boolean isDebugExtension = false;
		private String className; 
		
 		public DTFJPluginSnifferVisitor() {
 			super(Opcodes.ASM4, null);
 		}
 		
		public AnnotationVisitor visitAnnotation(String desc, boolean visible) {
			String extensionClassname = DebugExtension.class.getName().replace(".", "/");
			logger.finest("Inspecting annotation " + desc + " looking for annotation " + extensionClassname);
			
			// check if the annotation on this class contains the class name of the DebugExtension annotation
			// this amounts to the annotation being present or not on the visited class
			if (desc.contains(extensionClassname)) {
				logger.finest("Found DebugExtension annotation");
				isDebugExtension = true;
			} else {
				logger.finest("Did not find DebugExtension annotation");
				isDebugExtension = false;
			}
			return null;
		}
		
		public void visit(int version, int access, String name, String signature, String superName, String[] interfaces) {
			className = name.replace("/", ".");
		}

		public void visitAttribute(Attribute attr) {}
		public void visitEnd() {}
		public FieldVisitor visitField(int arg0, String arg1, String arg2, String arg3, Object arg4) { return null; }
		public void visitInnerClass(String arg0, String arg1, String arg2, int arg3) {}
		public MethodVisitor visitMethod(int arg0, String arg1,	String arg2, String arg3, String[] arg4) { return null;	}
		public void visitOuterClass(String arg0, String arg1, String arg2) {}
		public void visitSource(String arg0, String arg1) {}
	}
	
	//******************************************************************************************************
	//******************************************************************************************************
	//******************************************************************************************************
	//******************************************************************************************************
	//**************** FROM THIS POINT ON, TO THE END OF THE CLASS, THE CODE IS IDENTICAL ******************
	//**************** TO THAT IN DTFJ_UTILS PLUGINCLASSLOADER                            ******************
	//******************************************************************************************************
	//**************** EXCEPT THAT loadPlugins() and scanForClassFiles now throw DDRInteractiveException ***
	//******************************************************************************************************
	//******************************************************************************************************

	
	private Map<String, ClassFile> classFilesOnClasspath = new HashMap<String, ClassFile>();
	
	/**
	 * Represents the information and common code we need to locate and load a class on the plugins search path
	 */
	private abstract class ClassFile {
		protected String classFilePathName;
		boolean loaded = false;
		
		/**
		 * Construct a ClassFile object given the pathname of the file.
		 * For a class file within a jar file this will be the path name within the jar file e.g. /com/ibm/....
		 * For a stand alone class file this will be the pathname of the file.
		 * @param cfpn classfile pathname
		 */
		ClassFile (String cfpn) {
			this.classFilePathName = cfpn;
		}
		
		public abstract URL toURL();
		
		/**
		 * Load the code into a buffer then pass the buffer to 
		 * loadByteCodeFromBuffer(String...) for loading
		 * 
		 * @return the class if loaded OK, otherwise null
		 */
		public Class<?> loadByteCode() {
			if (loaded) {
				logger.finest("Would load " + toURL() + " but it is already loaded (presumably because required by an earlier class)");
				return null;
			}
			InputStream in = null;
			try {
				in = getStreamForByteCode();
				byte[] byteCodeBuffer = readByteCodeFromStream(in);
				return loadByteCodeFromBuffer(toURL(), null, byteCodeBuffer, 0, byteCodeBuffer.length);
			} catch (FileNotFoundException e) {
				logger.log(Level.FINE, "Unable to find file " + toURL(), e);
			} catch (IOException e) {
				logger.log(Level.FINE, "Error reading from file " + toURL(), e);
			} finally {
				try {
					if (in != null) {
						in.close();
					}
				} catch (IOException e) {
					logger.log(Level.FINE, "Error closing file " + toURL(), e);
				}
			}
			return null;
		}
		
		public abstract InputStream getStreamForByteCode() throws FileNotFoundException, IOException;
		
		/**
		 * Read the byte code from a stream into a byte array
		 * @param in InputStream
		 * @return byte array containing byte code
		 * @throws IOException
		 */
		private byte[] readByteCodeFromStream(InputStream in) throws IOException {
			byte[] buffer = new byte[4096];
			ByteArrayOutputStream out = new ByteArrayOutputStream(4096);
			int bytesRead = 0;
			while(-1 != (bytesRead = in.read(buffer))) {
				out.write(buffer, 0, bytesRead);
			}
			return out.toByteArray();
		}
		
		/**
		 * Provide a central function for processing the byte code for a class and allows any exceptions or errors to be handled.
		 * @param url URL of the file from which the byte code came
		 * @param packageName the name of the package (if null is supplied then the name of the class is used to derive the package name)
		 * @param bytecode byte code to process
		 * @param offset where the byte code starts
		 * @param length length of code to process
		 * @return returns the class if processed without error, null otherwise
		 */
		private Class<?> loadByteCodeFromBuffer(URL u, String packageName, byte[] bytecode, int offset, int length) {
			try {
				//load the byte code to create a class
				logger.finer("About to call defineClass with byte code from " + u);
				Class<?> clazz = defineClass(null, bytecode, offset, length);
				//can also throw NoClassDefFoundError
				logger.finer("Successful call to defineClass, loaded class is " + clazz.getName());
				loaded = true;
				//define the package for the class
				if(null == packageName) {
					definePackage(clazz.getName());
				} else {
					definePackage(packageName);
				}
				return clazz;
			} catch (ClassFormatError t) {
				//catch any problems and record what caused the error but allow processing to continue
				logger.log(logLevelForPluginLoadFailures,"ClassFormatError thrown when processing byte code from " + u + " : " + t);
				PluginConfig config = new PluginConfig(u.toString(), t, u);
				pluginFailures.add(config);
			} catch (NoClassDefFoundError t) {
				//catch any problems and record what caused the error but allow processing to continue
				logger.log(logLevelForPluginLoadFailures,"NoClassDefFoundError thrown when processing byte code from " + u + " : " + t);
				PluginConfig config = new PluginConfig(u.toString(), t, u);
				pluginFailures.add(config);
			} catch (SecurityException t) {
				//catch any problems and record what caused the error but allow processing to continue
				logger.log(logLevelForPluginLoadFailures,"SecurityException thrown when processing byte code from " + u + " : " + t);
				PluginConfig config = new PluginConfig(u.toString(), t, u);
				pluginFailures.add(config);
			} catch (LinkageError t) {
				//catch any problems and record what caused the error but allow processing to continue
				logger.log(logLevelForPluginLoadFailures,"LinkageError thrown when processing byte code from " + u + " : " + t);
				PluginConfig config = new PluginConfig(u.toString(), t, u);
				pluginFailures.add(config);
			}
			return null;
		}
	}
	
	/**
	 * Represents the information we need to locate a class within a jar file 
	 */
	private class ClassFileWithinJarFile extends ClassFile {
		private File jarFile;
		
		public ClassFileWithinJarFile(File f, String cfpn) {
			super(cfpn);
			this.jarFile = f;
		}

		public URL toURL() {
			try {
				return new URL("jar:file:" + jarFile.getAbsolutePath() + "!/" + classFilePathName);
			} catch (MalformedURLException e) {
				logger.log(logLevelForPluginLoadFailures,"Exception thrown when constructing URL from jar file name " + jarFile.getAbsolutePath() + " and class file name " + classFilePathName);
				return null;
			} 
		}
		
		public InputStream getStreamForByteCode() throws FileNotFoundException, IOException {
			JarInputStream jin = new JarInputStream(new FileInputStream(jarFile));
			JarEntry entry = null;
			while(null != (entry = jin.getNextJarEntry())) {
				if (entry.getName().equals(classFilePathName)) {
					return jin;
				}
			}
			throw new FileNotFoundException();
		}
	}
	
	/**
	 * Represents the information we need to locate a class within a class file in the file system
	 */
	private class StandaloneClassFile extends ClassFile {
		private File classFile;
		
		public StandaloneClassFile(File f) {
			super(f.getAbsolutePath());
			this.classFile = f;
		}

		public URL toURL() {
			try {
				return classFile.toURI().toURL();
			} catch (MalformedURLException e) {
				logger.log(logLevelForPluginLoadFailures,"Exception thrown when constructing URL from class file name " + classFile.getName());
				return null;
			} 
		}
		
		public InputStream getStreamForByteCode() throws FileNotFoundException, IOException {
			return new FileInputStream(classFile);
		}
	}

	/**
	 * Scans the plugins classpath and loads any DTFJPlugins found 
	 * @throws CommandException if any location on the plugins search path does not exist
	 */
	public void loadPlugins() throws DDRInteractiveCommandException {
		scanForClassFiles();
		
		for (String str : classFilesOnClasspath.keySet()) {
			try {
				DTFJPluginSnifferVisitor sniffer = sniffClassFile(classFilesOnClasspath.get(str).getStreamForByteCode());
				if (sniffer.isDebugExtension) {
					Class<?> clazz = loadClass(str);
					examineClass(classFilesOnClasspath.get(str).toURL(), clazz);
				}
			} catch (ClassNotFoundException e) {
				logger.log(logLevelForPluginLoadFailures, "Exception while loading plugins : " + e.getMessage());
			} catch (IOException e) {
				logger.fine(e.getMessage());
			}
		}
		addRuntimeCommands();
	}
	
	/**
	 * Adds any commands that have been added at runtime rather than loaded from the classpath
	 */
	private void addRuntimeCommands() {
		for(String name : runtimeCommandClasses) {
			try {
				loadCommandClass(name, false);
			} catch (ClassNotFoundException e) {
				logger.log(logLevelForPluginLoadFailures, "Could not load a runtime command class", e);
			}
		}
	}
	
	/**
	 * Scan the supplied plugin path to find commands which are written.
	 * This method does not support MVS on z/OS, the path needs to point to HFS locations 
	 * @param urls
	 * 
	 * @throws CommandException if any location on the plugins search path does not exist
	 */
	private void scanForClassFiles() throws DDRInteractiveCommandException {
		for(File file : pluginSearchPath) {	//a path entry can be null if the URI was malformed
			logger.fine("Scanning path " + file + " in search of DDR plugins");
			if(!file.exists()) {
				//log that the entry does not exist and skip
				logger.fine(String.format("Abandoning scan of DDR plugins search path: %s does not exist", file.getAbsolutePath()));
				throw new DDRInteractiveCommandException(file.getAbsolutePath() + " was specified on the plugins search path but it does not exist");
			} else {
				if(file.isDirectory()) {
					scanDirectory(file);
				} else {
					scanFile(file);
				}
			}
		}
	}
	
	private void scanDirectory(File dir) {
		logger.fine("Scanning directory " + dir.getAbsolutePath());
		File[] files = dir.listFiles();
		for(File file : files) {
			if(file.isDirectory()) {
				scanDirectory(file);
			} else {
				scanFile(file);
			}
		}
	}
	
	/**
	 * Returns the file extension
	 * @param file file to test
	 * @return everything after, and including, the last period in the file name or an empty string if there is no identifiable extension
	 */
	private static String getExtension(File file) {
		String name = file.getName();
		int pos = name.lastIndexOf('.');
		if((-1 == pos) || (name.length() == (pos + 1))) {
			return "";	//no extension
		} else {
			return name.substring(pos);
		}
	}
	
	/**
	 * Scans a file and determines if it can be loaded
	 * @param file
	 */
	private void scanFile(File file) {
		logger.fine("Scanning file " + file.getAbsolutePath());
		String ext = getExtension(file);
		if(ext.equals(FILE_EXT_JAR)) {
			examineJarFile(file);
			return;
		}
		if(ext.equals(FILE_EXT_CLASS)) {
			examineClassFile(file);
			return;
		}
		//if get this far, ignore the file as not having a recognised extension
	}
	
	private void examineClassFile(File file) {
		logger.fine("Found class file " + file.getAbsolutePath());
		if(file.length() > Integer.MAX_VALUE) {
			logger.fine("Skipping file " + file.getAbsolutePath() + " as the file size is > Integer.MAX_VALUE");
			return;		//skip this file
		}
		
		InputStream is = null;
		try {
			is = new FileInputStream(file);
			DTFJPluginSnifferVisitor sniffer = sniffClassFile(is);
			classFilesOnClasspath.put(sniffer.className, new StandaloneClassFile(file));
		} catch (IOException e) {
			logger.log(logLevelForPluginLoadFailures, e.getMessage());
		} finally {
			try {
				if (is != null) {
					is.close();
				}
			} catch (IOException e) {
				logger.log(logLevelForPluginLoadFailures, "Error closing file " + file.getAbsolutePath(), e);
			}
		}
	}
	
	private DTFJPluginSnifferVisitor sniffClassFile(InputStream in) throws IOException {
		DTFJPluginSnifferVisitor sniffer = new DTFJPluginSnifferVisitor();
		ClassReader cr = new ClassReader(in);
		cr.accept(sniffer, 0);
		return sniffer;
	}
	
	/**
	 * Scans all classes in the specified jar file
	 * @param file
	 */
	private void examineJarFile(File file) {
		logger.fine("Found jar file " + file.getAbsolutePath());

		JarInputStream jin = null;
		try {
			jin = new JarInputStream(new FileInputStream(file));
			JarEntry entry = null;
			while(null != (entry = jin.getNextJarEntry())) {
				if(entry.isDirectory() || !entry.getName().endsWith(".class")) {
					//skip directories, only interested in classes
					continue;
				}
				if(entry.getSize() > Integer.MAX_VALUE) {
					logger.fine("Skipping jar entry " + entry.getName() + " as the uncompressed size is > Integer.MAX_VALUE");
					continue;		//skip this entry
				}
				
				ClassFile classFile = new ClassFileWithinJarFile(file, entry.getName());
				InputStream in = classFile.getStreamForByteCode();
				DTFJPluginSnifferVisitor sniffer = sniffClassFile(in);
				in.close();
				classFilesOnClasspath.put(sniffer.className, classFile);
			}
		} catch (IOException e) {
			logger.log(logLevelForPluginLoadFailures, "Error reading from file " + file.getAbsolutePath(), e);
		} finally {
			if(null != jin) {
				try {
					jin.close();		
				} catch (IOException e) {
					logger.log(logLevelForPluginLoadFailures, "Error closing file " + file.getAbsolutePath(), e);
				}
			}
		}
	}
	
	/**
	 * Check to see if a class implements the command interface
	 * @param clazz class to test
	 * @return true if the interface is implemented, false if not
	 */
	protected boolean hasCommandIFace(Class<?> clazz) {
		return ICommand.class.isAssignableFrom(clazz);
	}
	
	public ArrayList<PluginConfig> getPlugins() {
		return pluginCache;
	}
	
	public ArrayList<PluginConfig> getPluginFailures() {
		return pluginFailures;
	}

	/** 
	 * Searches for and loads a class from the plugins search path.
	 * @return the loaded class or null
	 */
	public Class<?> findClass(String className) throws ClassNotFoundException {
		logger.finest("Entered findClass with class name of " + className);
		if (classFilesOnClasspath.containsKey(className)) {	
			logger.finest("Found match for with class " + className);
			return classFilesOnClasspath.get(className).loadByteCode();
		}
		
		throw new ClassNotFoundException("Unable to load class " + className);
	}

	/**
	 * Allows commands to a running session after startup.  Commands will not be visible until a refresh is called and the
	 * plugin cache is re-read by the current context.
	 * @param className class which implements the ICommand interface
	 * @param resolveClass flag to resolve the class
	 * @return the loaded class
	 * @throws ClassNotFoundException
	 */
	private synchronized Class<?> loadCommandClass(String className, boolean resolveClass) throws ClassNotFoundException {		
		Class<?> clazz = loadClass(className, resolveClass);
		try {
			URL url = new File("RuntimeLoad").toURI().toURL();
			examineClass(url, clazz);
		} catch (MalformedURLException e) {
			logger.log(logLevelForPluginLoadFailures,"Exception thrown when forming URL from file \"RuntimeLoad\" : " + e);
		}
		return clazz;
	}
	
	public void addCommandClass(String className) {
		runtimeCommandClasses.add(className);
	}
	
	public void removeCommandClass(String className) {
		if(!runtimeCommandClasses.remove(className)) {
			logger.fine(String.format("Ignored call to remove command %s as it was not previously added"));
		}
	}
}

// Following lines are a main and a special constructor to make it possible to test this class loader
// as a standalone application

//public static void main (String args[]) throws Exception {
//	DDRInteractiveClassLoader loader = new DDRInteractiveClassLoader();
//}
//
//public DDRInteractiveClassLoader() {
//	vmversion = "any old rubbish";
//	logger.setLevel(Level.FINEST);
//	Handler handler = new ConsoleHandler();
//	handler.setFormatter(new Formatter(){ 
//		public String format(LogRecord logRecord) {
//			return logRecord.getLevel() + " " + formatMessage(logRecord) + "\n";
//		} // log message is a single line with level then text
//	});
//	logger.addHandler(handler);
//	handler.setLevel(Level.FINEST);
//	// FINE to see what is happening at the file level
//	// FINER to see what is happening at the class level
//	// FINEST to see the detail of loading a class
//	configureSearchPath();
//	reload();
//}

