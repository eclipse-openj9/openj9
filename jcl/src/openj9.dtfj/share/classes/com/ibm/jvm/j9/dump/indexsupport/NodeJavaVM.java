/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.j9.dump.indexsupport;

import java.util.Iterator;

import org.xml.sax.Attributes;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.j9.ImageAddressSpace;
import com.ibm.dtfj.image.j9.ImageProcess;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.j9.JavaObject;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.j9.JavaRuntime;

/**
 * @author jmdisher
 * 
 * Example:
 * <javavm id="0x805fd90">
 */
public class NodeJavaVM extends NodeAbstract
{
	private JavaRuntime _runtime;
	
	public NodeJavaVM(XMLIndexReader parent, ImageProcess process, ImageAddressSpace addressSpace, String vmVersion, Attributes attributes)
	{
		long id = _longFromString(attributes.getValue("id"));
		
		ImagePointer vmPointer = addressSpace.getPointer(id);
		_runtime = new JavaRuntime(process, vmPointer, vmVersion);
		if (process != null) {
			process.addRuntime(_runtime);
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.indexsupport.IParserNode#nodeToPushAfterStarting(java.lang.String, java.lang.String, java.lang.String, org.xml.sax.Attributes)
	 */
	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		IParserNode child = null;
		
		if (qName.equals("systemProperties")) {
			child = new NodeSystemProperties(_runtime, attributes);
		} else if (qName.equals("class")) {
			child = new NodeClassInVM(_runtime, attributes);
		} else if (qName.equals("arrayclass")) {
			child = new NodeArrayClass(_runtime, attributes);
		} else if (qName.equals("classloader")) {
			child = new NodeClassLoader(_runtime, attributes);
		} else if (qName.equals("monitor")) {
			child = new NodeMonitor(_runtime, attributes);
		} else if (qName.equals("vmthread")) {
			child = new NodeVMThread(_runtime, attributes);
		} else if (qName.equals("trace")) {
			child = new NodeTrace(_runtime, attributes);
		} else if (qName.equals("heap")) {
			child = new NodeHeap(_runtime, attributes);
		} else if (qName.equals("javavminitargs")) {
			child = new NodeJavaVMInitArgs(_runtime, attributes);
		} else if (qName.equals("rootobject")) {
			child = new NodeRootObject(_runtime, attributes);
		} else if (qName.equals("rootclass")) {
			child = new NodeRootClass(_runtime, attributes);
		} else {
			child = super.nodeToPushAfterStarting(uri, localName, qName, attributes);
		}
		return child;
	}
	
	public void didFinishParsing()
	{
		Iterator classLoaders = _runtime.getJavaClassLoaders();
		while (classLoaders.hasNext()) {
			Object potentialClassLoader = classLoaders.next();
			if (potentialClassLoader instanceof JavaClassLoader) {
				JavaClassLoader loader = (JavaClassLoader) potentialClassLoader;
				
				//sets the associated object
				try {
					Object potentialLoaderObject = loader.getObject();
					if (null != potentialLoaderObject && potentialLoaderObject instanceof com.ibm.dtfj.java.j9.JavaObject) {
						JavaObject loaderObject = (com.ibm.dtfj.java.j9.JavaObject)potentialLoaderObject;
						loaderObject.setAssociatedClassLoader(loader);
						_runtime.addSpecialObject(loaderObject);
						Iterator classes = loader.getDefinedClasses();
						while (classes.hasNext()) {
							Object potentialClass = classes.next();
							if (potentialClass instanceof JavaClass) {
								JavaClass theClass = (JavaClass)potentialClass;
								
								try {
									Object potentialClassObject = theClass.getObject();
									if (null != potentialClassObject && potentialClassObject instanceof com.ibm.dtfj.java.j9.JavaObject) {
										JavaObject classObject = (com.ibm.dtfj.java.j9.JavaObject)potentialClassObject; 
										classObject.setAssociatedClass(theClass);
										_runtime.addSpecialObject(classObject);
									}
								} catch (CorruptDataException e) {
									//no reason to skip the other classloaders/classes just because an exception is thrown on one class
								} catch (IllegalArgumentException e) {
									//CMVC 173262 : JavaClass.getObject() uses JavaHeap.getObjectAtAddress() which can throw this exception
								}
							}
						}
					}
				} catch (CorruptDataException e) {
					//associated object not set
				}
			}
		}

		Iterator monitors = _runtime.getMonitors();
		while (monitors.hasNext()) {
			Object potentialMonitor = monitors.next();
			if (potentialMonitor instanceof JavaMonitor) {
				JavaMonitor monitor = (JavaMonitor) potentialMonitor;
				
				//sets the associated object
				Object potentialMonitorObject = monitor.getObject();
				if (null != potentialMonitorObject && potentialMonitorObject instanceof com.ibm.dtfj.java.j9.JavaObject) {
					JavaObject monitorObject = (com.ibm.dtfj.java.j9.JavaObject)potentialMonitorObject; 
					monitorObject.setAssociatedMonitor(monitor);
					_runtime.addSpecialObject(monitorObject);
				}
			}
		}

		Iterator threads = _runtime.getThreads();
		while (threads.hasNext()) {
			Object potentialThread = threads.next();
			if (potentialThread instanceof JavaThread) {
				JavaThread thread = (JavaThread) potentialThread;
				//sets the associated object
				try {
					Object potentialThreadObject = thread.getObject();
					if (null != potentialThreadObject && potentialThreadObject instanceof com.ibm.dtfj.java.j9.JavaObject) {
						JavaObject threadObject = (com.ibm.dtfj.java.j9.JavaObject)potentialThreadObject;
						threadObject.setAssociatedThread(thread);
						_runtime.addSpecialObject(threadObject);
					}
				} catch (CorruptDataException e) {
					//associated object not set
				}
			}
		}
	}
}
