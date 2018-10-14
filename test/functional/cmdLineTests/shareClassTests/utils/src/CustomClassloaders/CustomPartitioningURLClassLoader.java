/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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
package CustomClassloaders;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.JarURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.security.CodeSource;
import java.security.SecureClassLoader;
import java.util.Hashtable;
import java.util.jar.Attributes;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.jar.Manifest;
import java.util.jar.Attributes.Name;

import com.ibm.oti.shared.HelperAlreadyDefinedException;
import com.ibm.oti.shared.Shared;
import com.ibm.oti.shared.SharedClassHelperFactory;
import com.ibm.oti.shared.SharedClassTokenHelper;
import com.ibm.oti.shared.SharedClassURLClasspathHelper;
import com.ibm.oti.shared.SharedClassURLHelper;

/**
 * @author Matthew Kilner
 */
public class CustomPartitioningURLClassLoader extends SecureClassLoader {

	URL[] urls, orgUrls;
	
	private Hashtable jarCache = new Hashtable(32);
	int loaderType;
	
	String partition = null;
	
	SharedClassURLClasspathHelper scHelper;
	
	CustomLoaderMetaDataCache[] metaDataArray;
	
	FoundAtIndex foundAtIndex = new FoundAtIndex();

	public CustomPartitioningURLClassLoader(URL[] passedUrls, ClassLoader parent){
		super(parent);
		loaderType = ClassLoaderType.CACHEDURL.ord;
		int urlLength = passedUrls.length;
		urls = new URL[urlLength];
		orgUrls = new URL[urlLength];
		for (int i=0; i < urlLength; i++) {
			try {
				urls[i] = createSearchURL(passedUrls[i]);
			} catch (MalformedURLException e) {}
			orgUrls[i] = passedUrls[i];
		}
		metaDataArray = new CustomLoaderMetaDataCache[urls.length];
		initMetaData();
		SharedClassHelperFactory schFactory = Shared.getSharedClassHelperFactory();
		if(schFactory != null){
			try{
				scHelper = schFactory.getURLClasspathHelper(this, passedUrls);
				if(null != scHelper){
					scHelper.confirmAllEntries();
				}
			} catch (HelperAlreadyDefinedException e){
				e.printStackTrace();
			}
		}
	}
	
	public CustomPartitioningURLClassLoader(URL[] passedUrls){
		super();
		loaderType = ClassLoaderType.CACHEDURL.ord;
		int urlLength = passedUrls.length;
		urls = new URL[urlLength];
		orgUrls = new URL[urlLength];
		for (int i=0; i < urlLength; i++) {
			try {
				urls[i] = createSearchURL(passedUrls[i]);
			} catch (MalformedURLException e) {}
			orgUrls[i] = passedUrls[i];
		}
		metaDataArray = new CustomLoaderMetaDataCache[urls.length];
		initMetaData();
		SharedClassHelperFactory schFactory = Shared.getSharedClassHelperFactory();
		if(schFactory != null){
			try{
				scHelper = schFactory.getURLClasspathHelper(this, passedUrls);
				if(null != scHelper){
					scHelper.confirmAllEntries();
				}
			} catch (HelperAlreadyDefinedException e){
				e.printStackTrace();
			}
		}
	}
	
	public void setPartition(String newPartition){
		partition = newPartition;
	}
	
	public boolean getHelper(){
		SharedClassHelperFactory schFactory = Shared.getSharedClassHelperFactory();
		SharedClassURLClasspathHelper newHelper = null;
		try{
			newHelper = schFactory.getURLClasspathHelper(this, orgUrls);
		} catch (HelperAlreadyDefinedException e){
			return false;
		}
		
		if(newHelper.equals(scHelper)){
			return true;
		} else {
			return false;
		}
	}
	
	public void getHelper(URL[] urls)throws HelperAlreadyDefinedException {
		SharedClassHelperFactory schFactory = Shared.getSharedClassHelperFactory();
		SharedClassURLClasspathHelper newHelper = null;
		newHelper = schFactory.getURLClasspathHelper(this, urls);
	}
	
	public void getURLHelper()throws Exception {
		SharedClassHelperFactory schFactory = Shared.getSharedClassHelperFactory();
		SharedClassURLHelper newHelper = null;
		newHelper = schFactory.getURLHelper(this);
	}
	
	public void getTokenHelper()throws Exception {
		SharedClassHelperFactory schFactory = Shared.getSharedClassHelperFactory();
		SharedClassTokenHelper newHelper = null;
		newHelper = schFactory.getTokenHelper(this);
	}
	
	public boolean duplicateStore(String name){
		Class clazz = null;
		try{
			clazz = this.loadClass(name);	
		} catch (ClassNotFoundException e){
			e.printStackTrace();
		}
		if (clazz != null){
			int indexFoundAt = locateClass(name);
			if(indexFoundAt != -1){
				scHelper.storeSharedClass(clazz, indexFoundAt);
				return true;
			}
		}
		return false;
	}
	
	private static boolean isDirectory(URL url) {
		String file = url.getFile();
		return (file.length() > 0 && file.charAt(file.length()-1) == '/');
	}
	
	private URL createSearchURL(URL url) throws MalformedURLException {
		if (url == null) return url;
		String protocol = url.getProtocol();

		if (isDirectory(url) || protocol.equals("jar")) {
			return url;
		} else {
				return new URL("jar", "", -1, url.toString() + "!/");
		}
	}
	
	private void initMetaData(){
		for(int loopIndex = 0; loopIndex < metaDataArray.length; loopIndex++){
			metaDataArray[loopIndex] = null;
		}
	}
	
	private void addMetaDataEntry(){
		CustomLoaderMetaDataCache[] newArray = new CustomLoaderMetaDataCache[(metaDataArray.length)];
		System.arraycopy(metaDataArray,0,newArray,0,metaDataArray.length);
		metaDataArray = newArray;
		metaDataArray[(metaDataArray.length - 1)] = null;
	}
	
	public void addUrl(URL url){
		URL searchURL = null;
		try{
			searchURL = createSearchURL(url);
		} catch (Exception e){
			e.printStackTrace();
		}
		URL[] newOrgUrls = new URL[(orgUrls.length)];
		System.arraycopy(orgUrls,0,newOrgUrls,0,orgUrls.length);
		orgUrls = newOrgUrls;
		orgUrls[(orgUrls.length - 1)] = url;
		URL[] newUrls = new URL[urls.length];
		System.arraycopy(urls,0,newUrls,0,urls.length);
		urls = newUrls;
		urls[(urls.length - 1)] = searchURL;
		scHelper.addClasspathEntry(url);
		scHelper.confirmAllEntries();
		addMetaDataEntry();
	}
	
	public URL[] getURLS(){
		return orgUrls;
	}
	
	private static byte[] getBytes(InputStream is, boolean readAvailable) throws IOException {
		if (readAvailable) {
			byte[] buf = new byte[is.available()];
			is.read(buf, 0, buf.length);
			is.close();
			return buf;
		}
		byte[] buf = new byte[4096];
		int size = is.available();
		if (size < 1024) size = 1024;
		ByteArrayOutputStream bos = new ByteArrayOutputStream(size);
		int count;
		while ((count = is.read(buf)) > 0)
			bos.write(buf, 0, count);
		return bos.toByteArray();
	}
	
	private int locateClass(String className){
		int classAtEntry = -1;
		String name = className.replace('.','/').concat(".class");		
		for(int index = 0; index < urls.length; index ++){
			URL currentUrl = urls[index];
			if(currentUrl.getProtocol().equals("jar")){
				JarEntry entry = null;
				JarFile jf = (JarFile)jarCache.get(currentUrl);
				if(jf == null){
					/* First time we have encountered this jar.
					 * Lets cache its metaData.
					 */
					try{
						URL jarFileUrl = ((JarURLConnection)currentUrl.openConnection()).getJarFileURL();
						JarURLConnection jarUrlConnection = (JarURLConnection)new URL("jar", "", jarFileUrl.toExternalForm() + "!/").openConnection();
						try{
							jf = jarUrlConnection.getJarFile();
						}catch(Exception e){
						}
						if(jf != null){
							jarCache.put(currentUrl, jf);
						}
					} catch (Exception e){
						e.printStackTrace();
					}
					if(jf != null){
						Manifest manifest = null;
						java.security.cert.Certificate[] certs = null;
						URL csUrl = currentUrl;
						CodeSource codeSource;
						try{
							manifest = jf.getManifest();
						} catch(Exception e){
							e.printStackTrace();
						}
						entry = jf.getJarEntry(name);
						if(entry != null){
							certs = entry.getCertificates();
						}
						codeSource = new CodeSource(csUrl, certs);
						CustomLoaderMetaDataCache metaData = new CustomLoaderMetaDataCache();
						metaData.manifest = manifest;
						metaData.codeSource = codeSource;
						metaDataArray[index] = metaData;
					}
				}
				if(entry == null && jf != null){
					entry = jf.getJarEntry(name);
				}
				if(entry != null){
					/* We have the first match on the class path, return the current url index */
					return index;
				}
			} else {
				String filename = currentUrl.getFile();
				String host = currentUrl.getHost();
				if (host != null && host.length() > 0) {
					filename = new StringBuffer(host.length() + filename.length() + name.length() + 2).
						append("//").append(host).append(filename).append(name).toString();
				} else {
					filename = new StringBuffer(filename.length() + name.length()).
						append(filename).append(name).toString();
				}
				File file = new File(filename);
				// Don't throw exceptions for speed
				if (file.exists()) {
					if(metaDataArray[index] == null){
						java.security.cert.Certificate[] certs = null;
						CustomLoaderMetaDataCache metaData = new CustomLoaderMetaDataCache();
						metaData.manifest = null;
						metaData.codeSource = new CodeSource(currentUrl, certs);
						metaDataArray[index] = metaData;
					}
					return index;
				}
			}			
		}
		return classAtEntry;
	}
	
	private byte[] loadClassBytes(String className, int urlIndex){
		byte[] bytes = null;
		String name = className.replace('.','/').concat(".class");
		URL classLocation = urls[urlIndex];
		if(classLocation.getProtocol().equals("jar")){
			JarFile jf = (JarFile)jarCache.get(classLocation);
			JarEntry entry = jf.getJarEntry(name);
			try{
				InputStream stream = jf.getInputStream(entry);
				bytes = getBytes(stream, true);
			} catch (Exception e){
				e.printStackTrace();
			}
		} else {
			String filename = classLocation.getFile();
			String host = classLocation.getHost();
			if (host != null && host.length() > 0) {
				filename = new StringBuffer(host.length() + filename.length() + name.length() + 2).
					append("//").append(host).append(filename).append(name).toString();
			} else {
				filename = new StringBuffer(filename.length() + name.length()).
					append(filename).append(name).toString();
			}
			File file = new File(filename);
			// Don't throw exceptions for speed
			if (file.exists()) {
				try{
					FileInputStream stream = new FileInputStream(file);
					bytes = getBytes(stream, true);
				} catch(Exception e){
					e.printStackTrace();
				}
			}
		}
		return bytes;
	}
	
	public Class findClass(String name) throws ClassNotFoundException {
		Class clazz = null;
		if(scHelper != null){
			byte[] classBytes = scHelper.findSharedClass(partition, name, foundAtIndex);
			if(classBytes != null){
				if(metaDataArray[foundAtIndex.getIndex()] != null){
					checkPackage(name, foundAtIndex.getIndex());
					CustomLoaderMetaDataCache metadata = metaDataArray[foundAtIndex.getIndex()];
					CodeSource codeSource = metadata.codeSource;
					clazz = defineClass(name, classBytes, 0, classBytes.length, codeSource);
				}
			}
		}
		if(clazz == null) {
			int indexFoundAt = locateClass(name);
			if(indexFoundAt != -1){
				try{
					byte[] classBytes = loadClassBytes(name, indexFoundAt);
					checkPackage(name, indexFoundAt);
					CustomLoaderMetaDataCache metadata = metaDataArray[indexFoundAt];
					CodeSource codeSource = metadata.codeSource;
					clazz = defineClass(name, classBytes, 0, classBytes.length, codeSource);
					if(clazz != null){
						System.out.println("\n** Storing class: "+name+" on partition: "+partition);
						scHelper.storeSharedClass(partition, clazz, indexFoundAt);
					}
				} catch (Exception e){
					e.printStackTrace();
				}
			}
			if(clazz == null){
				throw new ClassNotFoundException(name);
			}
		}
		return clazz;
	}
	
	private void checkPackage(String name, int urlIndex){
		int index = name.lastIndexOf('.');
		if(index != -1){
			String packageString = name.substring(0, index);
			Manifest manifest = metaDataArray[urlIndex].manifest;
			CodeSource codeSource = metaDataArray[urlIndex].codeSource;
			synchronized(this){
				Package packageInst = getPackage(packageString);
				if(packageInst == null){
					if (manifest != null){
						definePackage(packageString, manifest, codeSource.getLocation());
					} else {
						definePackage(packageString, null, null, null, null, null, null, null);
					}
				} else {
					boolean exception = false;
					if (manifest != null) {
						String dirName = packageString.replace('.', '/') + "/";
						if (isSealed(manifest, dirName))
							exception = !packageInst.isSealed(codeSource.getLocation());
					} else
						exception = packageInst.isSealed();
					if (exception)
						throw new SecurityException(com.ibm.oti.util.Msg.getString("Package exception"));
				}	
			}
		}		
	}
	
	private boolean isSealed(Manifest manifest, String dirName) {
		Attributes mainAttributes = manifest.getMainAttributes();
		String value = mainAttributes.getValue(Attributes.Name.SEALED);
		boolean sealed = value != null &&
			value.toLowerCase().equals ("true");
		Attributes attributes = manifest.getAttributes(dirName);
		if (attributes != null) {
			value = attributes.getValue(Attributes.Name.SEALED);
			if (value != null)
				sealed = value.toLowerCase().equals("true");
		}
		return sealed;
	}
	
	protected Package definePackage(String name, Manifest man, URL url) throws IllegalArgumentException {
		String path = name.replace('.','/').concat("/");
		String[] attrs = new String[7];
		URL sealedAtURL = null;
		
		Attributes attr = man.getAttributes(path);
		Attributes mainAttr = man.getMainAttributes();
		
		if(attr != null){
			attrs[0] = attr.getValue(Name.SPECIFICATION_TITLE);
			attrs[1] = attr.getValue(Name.SPECIFICATION_VERSION);
			attrs[2] = attr.getValue(Name.SPECIFICATION_VENDOR);
			attrs[3] = attr.getValue(Name.IMPLEMENTATION_TITLE);
			attrs[4] = attr.getValue(Name.IMPLEMENTATION_VERSION);
			attrs[5] = attr.getValue(Name.IMPLEMENTATION_VENDOR);
			attrs[6] = attr.getValue(Name.SEALED);
		}
		
		if(mainAttr != null){
			if (attrs[0] == null) {
				attrs[0] = mainAttr.getValue(Name.SPECIFICATION_TITLE);
			}
			if (attrs[1] == null) {
				attrs[1] = mainAttr.getValue(Name.SPECIFICATION_VERSION);
			}
			if (attrs[2] == null) {
				attrs[2] = mainAttr.getValue(Name.SPECIFICATION_VENDOR);
			}
			if (attrs[3] == null) {
				attrs[3] = mainAttr.getValue(Name.IMPLEMENTATION_TITLE);
			}
			if (attrs[4] == null) {
				attrs[4] = mainAttr.getValue(Name.IMPLEMENTATION_VERSION);
			}
			if (attrs[5] == null) {
				attrs[5] = mainAttr.getValue(Name.IMPLEMENTATION_VENDOR);
			}
			if (attrs[6] == null) {
				attrs[6] = mainAttr.getValue(Name.SEALED);
			}
		}
		if ("true".equalsIgnoreCase(attrs[6])){
			sealedAtURL = url;
		}
		return definePackage(name, attrs[0], attrs[1], attrs[2], attrs[3], attrs[4], attrs[5], sealedAtURL);
	}
	
	public boolean isClassInSharedCache(String className){
		byte[] sharedClass = null;
		System.out.println("\n** Checking for class: "+className+" on partition: "+partition);
		if (scHelper!=null) {
			sharedClass = scHelper.findSharedClass(partition, className, foundAtIndex);
			if (sharedClass !=null){
				return true;
			} else {
				return false;
			}
		}
		return false;
	}
	
	public Class getClassFromCache(String name){
		Class clazz = null;
		byte[] classBytes = scHelper.findSharedClass(partition, name, foundAtIndex);
		if(classBytes != null){
			CodeSource cs = null;
			clazz = defineClass(name, classBytes, 0, classBytes.length, cs);
		}
		return clazz;
	}
	
	public static class FoundAtIndex implements SharedClassURLClasspathHelper.IndexHolder {

		int indexFoundAt = -1;
		
		public void setIndex(int index) {
			indexFoundAt = index; 
		}
		
		public int getIndex(){
			return indexFoundAt;
		}
		
		public void reset(){
			indexFoundAt = -1;
		}

	}
}
