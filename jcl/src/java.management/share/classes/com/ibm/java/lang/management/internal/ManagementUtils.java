/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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
package com.ibm.java.lang.management.internal;

import java.io.IOException;
import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.LockInfo;
import java.lang.management.MemoryNotificationInfo;
import java.lang.management.MemoryType;
import java.lang.management.MemoryUsage;
import java.lang.management.MonitorInfo;
import java.lang.management.RuntimeMXBean;
import java.lang.management.ThreadInfo;
import java.lang.reflect.Array;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Properties;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Function;
/*[IF !Sidecar19-SE]*/
import java.lang.management.ManagementFactory;
import java.lang.management.PlatformManagedObject;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Objects;
import java.util.logging.LogManager;
import com.ibm.lang.management.internal.ExtendedMemoryMXBeanImpl;
/*[ENDIF] !Sidecar19-SE*/

import javax.management.InstanceNotFoundException;
import javax.management.MBeanServerConnection;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;
import javax.management.openmbean.TabularData;
import javax.management.openmbean.TabularDataSupport;
import javax.management.openmbean.TabularType;

/**
 * Support methods for com.ibm.lang.management classes.
 * Provides utility methods to other packages (hence, specified as public),
 * such as JLM classes, but not meant to be published as an API.
 */
@SuppressWarnings("javadoc")
public final class ManagementUtils {

	/*
	 * String representation for the type NotificationEmitter that must
	 * be implemented by a bean that emits notifications.
	 */
	private static final String NOTIFICATION_EMITTER_TYPE = "javax.management.NotificationEmitter"; //$NON-NLS-1$

	/* Set to true, if we are running on a Unix or Unix like operating
	 * system; false, otherwise.
	 */
	private static final boolean isUnix;

	/**
	 * System property setting used to decide if non-fatal exceptions should be
	 * written out to console.
	 */
	public static final boolean VERBOSE_MODE;

	static {
		Properties properties = com.ibm.oti.vm.VM.getVMLangAccess().internalGetProperties();
		String thisOs = properties.getProperty("os.name"); //$NON-NLS-1$

		isUnix = "aix".equalsIgnoreCase(thisOs) //$NON-NLS-1$
				|| "linux".equalsIgnoreCase(thisOs) //$NON-NLS-1$
				|| "mac os x".equalsIgnoreCase(thisOs) //$NON-NLS-1$
				|| "z/OS".equalsIgnoreCase(thisOs); //$NON-NLS-1$

		VERBOSE_MODE = properties.getProperty("com.ibm.lang.management.verbose") != null; //$NON-NLS-1$
	}

	/**
	 * Throws an {@link IllegalArgumentException} if the {@link CompositeData}
	 * argument <code>cd</code> contains attributes that are not of the exact
	 * types specified in the <code>expectedTypes</code> argument. The
	 * attribute types of <code>cd</code> must also match the order of types
	 * in <code>expectedTypes</code>.
	 *
	 * @param cd
	 *            a <code>CompositeData</code> object
	 * @param expectedNames
	 *            an array of expected attribute names
	 * @param expectedTypes
	 *            an array of type names
	 */
	public static void verifyFieldTypes(CompositeData cd, String[] expectedNames, String[] expectedTypes) {
		Object[] allVals = cd.getAll(expectedNames);
		// Check that the number of elements match
		if (allVals.length != expectedTypes.length) {
			/*[MSG "K05E8", "CompositeData does not contain the expected number of attributes."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E8")); //$NON-NLS-1$
		}

		// Type of corresponding elements must be the same
		for (int i = 0; i < allVals.length; ++i) {
			Object actualVal = allVals[i];
			// It is permissible that a value in the CompositeData object is
			// null in which case we cannot test its type. Move on.
			if (actualVal == null) {
				continue;
			}
			String actualType = actualVal.getClass().getName();
			String expectedType = expectedTypes[i];
			if (actualType.equals(expectedType)) {
				continue;
			}
			// Handle CompositeData and CompositeDataSupport
			if (expectedType.equals(CompositeData.class.getName())) {
				if (actualVal instanceof CompositeData) {
					continue;
				}
			} else if (expectedType.equals(TabularData.class.getName())) {
				if (actualVal instanceof TabularData) {
					continue;
				}
			}
			/*[MSG "K05E9", "CompositeData contains an attribute of unexpected type. Expected {0}, found {1}."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05E9", //$NON-NLS-1$
					expectedType, actualType));
		}
	}

	/**
	 * Throws an {@link IllegalArgumentException} if the {@link CompositeData}
	 * argument <code>cd</code> does not have any of the attributes named in
	 * the <code>expected</code> array of strings.
	 *
	 * @param cd
	 *            a <code>CompositeData</code> object
	 * @param expected
	 *            an array of attribute names expected in <code>cd</code>.
	 */
	public static void verifyFieldNames(CompositeData cd, String[] expected) {
		for (String expect : expected) {
			if (!cd.containsKey(expect)) {
				/*[MSG "K05EA", "CompositeData object does not contain expected key : {0}."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05EA", expect)); //$NON-NLS-1$
			}
		}
	}

	/**
	 * Throws an {@link IllegalArgumentException} if the {@link CompositeData}
	 * argument <code>cd</code> does not have the number of attributes
	 * specified in <code>i</code>.
	 *
	 * @param cd
	 *            a <code>CompositeData</code> object
	 * @param i
	 *            the number of expected attributes in <code>cd</code>
	 */
	public static void verifyFieldNumber(CompositeData cd, int i) {
		if (cd == null) {
			/*[MSG "K05EB", "Null CompositeData"]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K05EB")); //$NON-NLS-1$
		}
		if (cd.values().size() != i) {
			/*[MSG "K05EC", "CompositeData object does not have the expected number of attributes"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05EC")); //$NON-NLS-1$
		}
	}


	/**
	 * Convenience method to converts an array of <code>String</code> to a
	 * <code>List&lt;String&gt;</code>.
	 *
	 * @param data
	 *            an array of <code>String</code>
	 * @return a new <code>List&lt;String&gt;</code>
	 */
	public static List<String> convertStringArrayToList(String[] data) {
		List<String> result = new ArrayList<>(data.length);

		for (String string : data) {
			result.add(string);
		}

		return result;
	}

	/**
	 * Receives an instance of a {@link TabularData} whose data is wrapping a
	 * <code>Map</code> and returns a new instance of <code>Map</code>
	 * containing the input information.
	 *
	 * @param data
	 *            an instance of <code>TabularData</code> that may be mapped
	 *            to a <code>Map</code>.
	 * @return a new {@link Map} containing the information originally wrapped
	 *         in the <code>data</code> input.
	 * @throws IllegalArgumentException
	 *             if <code>data</code> has a <code>CompositeType</code>
	 *             that does not contain exactly two items (i.e. a key and a
	 *             value).
	 */
	public static Object convertTabularDataToMap(TabularData data) {
		// Bail out early on null input.
		if (data == null) {
			return null;
		}

		Set<String> cdKeySet = data.getTabularType().getRowType().keySet();

		// The key set for the CompositeData instances comprising each row
		// must contain only two elements.
		if (cdKeySet.size() != 2) {
			/*[MSG "K05ED", "TabularData's row type is not a CompositeType with two items."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05ED")); //$NON-NLS-1$
		}

		String[] keys = cdKeySet.toArray(new String[2]);

		@SuppressWarnings("unchecked")
		Collection<CompositeData> rows = (Collection<CompositeData>) data.values();
		Map<Object, Object> result = new HashMap<>(rows.size());

		for (CompositeData rowCD : rows) {
			result.put(rowCD.get(keys[0]), rowCD.get(keys[1]));
		}

		return result;
	}

	/**
	 * Return a new instance of type <code>T</code> from the supplied
	 * {@link CompositeData} object whose type maps to <code>T</code>.
	 *
	 * @param <T>
	 *            the type of object wrapped by the <code>CompositeData</code>.
	 * @param data
	 *            an instance of <code>CompositeData</code> that maps to an
	 *            instance of <code>T</code>
	 * @param realClass
	 *            the {@link Class} object for type <code>T</code>
	 * @return a new instance of <code>T</code>
	 * @throws NoSuchMethodException
	 * @throws SecurityException
	 * @throws InvocationTargetException
	 * @throws IllegalAccessException
	 * @throws IllegalArgumentException
	 */
	private static <T> T convertFromCompositeData(CompositeData data, Class<T> realClass) throws SecurityException,
			NoSuchMethodException, IllegalArgumentException, IllegalAccessException, InvocationTargetException {
		// Bail out early on null input.
		if (data == null) {
			return null;
		}

		// See if the realClass has a static 'from' method that takes a
		// CompositeData and returns a new instance of T.
		Method forMethod = realClass.getMethod("from", CompositeData.class); //$NON-NLS-1$

		return realClass.cast(forMethod.invoke(null, data));
	}

	/**
	 * Receive data of the type specified in <code>openClass</code> and return
	 * it in an instance of the type specified in <code>realClass</code>.
	 *
	 * @param <T>
	 *
	 * @param data
	 *            an instance of the type named <code>openTypeName</code>
	 * @param openClass
	 * @param realClass
	 * @return a new instance of the type <code>realTypeName</code> containing
	 *         all the state in the input <code>data</code> object.
	 * @throws ClassNotFoundException
	 * @throws IllegalAccessException
	 * @throws InstantiationException
	 * @throws InvocationTargetException
	 * @throws NoSuchMethodException
	 * @throws IllegalArgumentException
	 * @throws SecurityException
	 */
	@SuppressWarnings("unchecked")
	public static <T> T convertFromOpenType(Object data, Class<?> openClass,
			Class<T> realClass) throws ClassNotFoundException,
			InstantiationException, IllegalAccessException, SecurityException,
			IllegalArgumentException, NoSuchMethodException,
			InvocationTargetException {
		// Bail out early on null input.
		if (data == null) {
			return null;
		}

		T result = null;

		if (openClass.isArray() && realClass.isArray()) {
			Class<?> openElementClass = openClass.getComponentType();
			Class<?> realElementClass = realClass.getComponentType();
			Object[] dataArray = (Object[]) data;
			int length = dataArray.length;

			result = (T) Array.newInstance(realElementClass, length);

			for (int i = 0; i < length; ++i) {
				Array.set(result, i, convertFromOpenType(dataArray[i], openElementClass, realElementClass));
			}
		} else if (openClass.equals(CompositeData.class)) {
			result = ManagementUtils.convertFromCompositeData((CompositeData) data, realClass);
		} else if (openClass.equals(TabularData.class)) {
			if (realClass.equals(Map.class)) {
				result = (T) ManagementUtils.convertTabularDataToMap((TabularData) data);
			}
		} else if (openClass.equals(String[].class)) {
			if (realClass.equals(List.class)) {
				result = (T) ManagementUtils.convertStringArrayToList((String[]) data);
			}
		} else if (openClass.equals(String.class)) {
			if (realClass.equals(MemoryType.class)) {
				result = (T) ManagementUtils.convertStringToMemoryType((String) data);
			}
		}

		return result;
	}

	/**
	 * Convenience method that receives a string representation of a
	 * <code>MemoryType</code> instance and returns the actual
	 * <code>MemoryType</code> that corresponds to that value.
	 *
	 * @param data
	 *            a string
	 * @return if <code>data</code> can be used to obtain an instance of
	 *         <code>MemoryType</code> then a <code>MemoryType</code>,
	 *         otherwise <code>null</code>.
	 */
	private static MemoryType convertStringToMemoryType(String data) {
		MemoryType result = null;

		try {
			result = MemoryType.valueOf(data);
		} catch (IllegalArgumentException e) {
			if (ManagementUtils.VERBOSE_MODE) {
				e.printStackTrace(System.err);
			}
		}

		return result;
	}

	/**
	 * @param propsMap
	 *            a <code>Map&lt;String, String%gt;</code> of the system
	 *            properties.
	 * @return the system properties (e.g. as obtained from
	 *         {@link RuntimeMXBean#getSystemProperties()}) wrapped in a
	 *         {@link TabularData}.
	 */
	public static TabularData toSystemPropertiesTabularData(Map<String, String> propsMap) {
		// Bail out early on null input.
		if (propsMap == null) {
			return null;
		}

		TabularData result = null;
		try {
			// Obtain the row type for the TabularType
			String[] rtItemNames = { "key", "value" }; //$NON-NLS-1$ //$NON-NLS-2$
			String[] rtItemDescs = { "key", "value" }; //$NON-NLS-1$ //$NON-NLS-2$
			OpenType<?>[] rtItemTypes = { SimpleType.STRING, SimpleType.STRING };

			CompositeType rowType = new CompositeType(
					propsMap.getClass().getName(),
					propsMap.getClass().getName(),
					rtItemNames,
					rtItemDescs,
					rtItemTypes);

			// Obtain the required TabularType
			TabularType sysPropsType = new TabularType(
					propsMap.getClass().getName(),
					propsMap.getClass().getName(),
					rowType,
					new String[] { "key" }); //$NON-NLS-1$

			// Create an empty TabularData
			result = new TabularDataSupport(sysPropsType);

			// Take each entry out of the input propsMap, put it into a new
			// instance of CompositeData and put into the TabularType
			for (Entry<String, String> entry : propsMap.entrySet()) {
				String propKey = entry.getKey();
				String propVal = entry.getValue();

				result.put(new CompositeDataSupport(rowType, rtItemNames, new String[] { propKey, propVal }));
			}
		} catch (OpenDataException e) {
			if (ManagementUtils.VERBOSE_MODE) {
				e.printStackTrace(System.err);
			}
			result = null;
		}

		return result;
	}

	/**
	 * Convenience method that sets out to return the {@link Class} object for
	 * the specified type named <code>name</code>. Unlike the
	 * {@link Class#forName(java.lang.String)} method, this will work even for
	 * primitive types.
	 *
	 * @param name
	 *            the name of a Java type
	 * @return the <code>Class</code> object for the type <code>name</code>
	 * @throws ClassNotFoundException
	 *             if <code>name</code> does not correspond to any known type
	 *             (including primitive types).
	 */
	public static Class<?> getClassMaybePrimitive(String name) throws ClassNotFoundException {
		int i = name.lastIndexOf('.');

		if (i != -1) {
			SecurityManager sm = System.getSecurityManager();
			if (sm != null) {
				sm.checkPackageAccess(name.substring(0, i));
			}
		}

		Class<?> result = null;

		try {
			result = Class.forName(name);
		} catch (ClassNotFoundException e) {
			if (name.equals(boolean.class.getName())) {
				result = boolean.class;
			} else if (name.equals(char.class.getName())) {
				result = char.class;
			} else if (name.equals(byte.class.getName())) {
				result = byte.class;
			} else if (name.equals(short.class.getName())) {
				result = short.class;
			} else if (name.equals(int.class.getName())) {
				result = int.class;
			} else if (name.equals(long.class.getName())) {
				result = long.class;
			} else if (name.equals(float.class.getName())) {
				result = float.class;
			} else if (name.equals(double.class.getName())) {
				result = double.class;
			} else if (name.equals(void.class.getName())) {
				result = void.class;
			} else {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
				// Rethrow the original ClassNotFoundException
				throw e;
			}
		}

		return result;
	}

	/**
	 * Convenience method to determine if the <code>wrapper</code>
	 * <code>Class</code>
	 * object is really the wrapper class for the
	 * <code>primitive</code> <code>Class</code> object.
	 *
	 * @param wrapper
	 * @param primitive
	 * @return <code>true</code> if the <code>wrapper</code> class is the
	 *         wrapper class for <code>primitive</code>. Otherwise
	 *         <code>false</code>.
	 */
	public static boolean isWrapperClass(Class<?> wrapper, Class<?> primitive) {
		boolean result = true;
		if (primitive.equals(boolean.class) && !wrapper.equals(Boolean.class)) {
			result = false;
		} else if (primitive.equals(char.class) && !wrapper.equals(Character.class)) {
			result = false;
		} else if (primitive.equals(byte.class) && !wrapper.equals(Byte.class)) {
			result = false;
		} else if (primitive.equals(short.class) && !wrapper.equals(Short.class)) {
			result = false;
		} else if (primitive.equals(int.class) && !wrapper.equals(Integer.class)) {
			result = false;
		} else if (primitive.equals(long.class) && !wrapper.equals(Long.class)) {
			result = false;
		} else if (primitive.equals(float.class) && !wrapper.equals(Float.class)) {
			result = false;
		} else if (primitive.equals(double.class) && !wrapper.equals(Double.class)) {
			result = false;
		}

		return result;
	}

	/**
	 * Query the server via the given connection whether mxbeanObjectName
	 * is the name of a bean that is a notification emitter.
	 *
	 * @param connection
	 * @param mxbeanObjectName
	 * @return
	 * @throws IOException
	 * @throws IllegalArgumentException
	 */
	public static boolean isANotificationEmitter(MBeanServerConnection connection, ObjectName mxbeanObjectName)
			throws IOException, IllegalArgumentException {
		boolean result = false;

		try {
			result = connection.isInstanceOf(mxbeanObjectName, NOTIFICATION_EMITTER_TYPE);
		} catch (InstanceNotFoundException e) {
			// a non-existent bean is not an emitter
			result = false;
		}

		return result;
	}

	/**
	 * Helper function that tells whether we are running on Unix or not.
	 * @return Returns true if we are running on Unix; false, otherwise.
	 */
	public static boolean isRunningOnUnix() {
		return isUnix;
	}

	/**
	 * Convenience method to create an ObjectName from the specified string.
	 *
	 * @param name
	 * @return an ObjectName corresponding to the specified string.
	 */
	public static ObjectName createObjectName(String name) {
		try {
			return ObjectName.getInstance(name);
		} catch (MalformedObjectNameException e) {
			if (ManagementUtils.VERBOSE_MODE) {
				e.printStackTrace();
			}
			return null;
		}
	}

	/**
	 * Convenience method to create an ObjectName with the specified domain and name property.
	 *
	 * @param domain
	 * @param name
	 * @return an ObjectName with the specified domain and name property.
	 */
	public static ObjectName createObjectName(String domain, String name) {
		try {
			return ObjectName.getInstance(domain + ",name=" + name); //$NON-NLS-1$
		} catch (MalformedObjectNameException e) {
			if (ManagementUtils.VERBOSE_MODE) {
				e.printStackTrace();
			}
			return null;
		}
	}

	/**
	 * The prefix for all <code>ObjectName</code> strings which represent a
	 * {@link GarbageCollectorMXBean}. The unique <code>ObjectName</code> for
	 * a <code>GarbageCollectorMXBean</code> can be formed by adding
	 * &quot;,name=<i>collector name</i>&quot; to this constant.
	 */
	public static final String BUFFERPOOL_MXBEAN_DOMAIN_TYPE = "java.nio:type=BufferPool"; //$NON-NLS-1$

	/*[IF !Sidecar19-SE]*/

	/**
	 * @param mxbeanName
	 * @param mxbeanInterface
	 */
	public static ObjectName checkNamedMXBean(String mxbeanName, Class<?> mxbeanInterface) {
		if (mxbeanName == null) {
			/*[MSG "K0603", "name cannot be null"]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K0603")); //$NON-NLS-1$
		}

		if (mxbeanInterface == null) {
			/*[MSG "K0614", "class cannot be null"]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K0614")); //$NON-NLS-1$
		}

		ObjectName mxbeanObjectName = Metadata.makeObjectName(mxbeanName);
		ObjectName[] validObjectNames = Metadata.objectNamesByInterface.get(mxbeanInterface);

		if (validObjectNames != null) {
			for (ObjectName objectName : validObjectNames) {
				if (objectName.apply(mxbeanObjectName)) {
					return mxbeanObjectName;
				}
			}

			String domain = mxbeanObjectName.getDomain();
			String type = mxbeanObjectName.getKeyProperty("type"); //$NON-NLS-1$

			for (ObjectName objectName : validObjectNames) {
				if (domain.equals(objectName.getDomain())) {
					if (Objects.equals(type, objectName.getKeyProperty("type"))) { //$NON-NLS-1$
						return mxbeanObjectName;
					}
				}
			}
		}

		/*[MSG "K0605", "{0} is not an instance of interface {1}"]*/
		throw new IllegalArgumentException(
				com.ibm.oti.util.Msg.getString("K0605", mxbeanName, mxbeanInterface.getName())); //$NON-NLS-1$
	}

	public static void checkSupported(Class<?> mxbeanInterface) {
		if (!Metadata.managementInterfaces.contains(mxbeanInterface)) {
			/*[MSG "K0601", "{0} is not a valid MXBean interface."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0601", mxbeanInterface.getName())); //$NON-NLS-1$
		}
	}

	public static void checkSupportedSingleton(Class<?> mxbeanInterface) {
		checkSupported(mxbeanInterface);

		if (Metadata.isMultiInstanceBeanInterface(mxbeanInterface)) {
			/*[MSG "K0602", "{0} can have zero or more than one instances"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0602", mxbeanInterface.getName())); //$NON-NLS-1$
		}
	}

	public static Set<PlatformManagedObject> getAllAvailableMXBeans() {
		Set<PlatformManagedObject> allBeans = new HashSet<>();

		for (List<? extends PlatformManagedObject> beans : Metadata.beansByInterface.values()) {
			allBeans.addAll(beans);
		}

		return allBeans;
	}

	public static Set<Class<? extends PlatformManagedObject>> getPlatformManagementInterfaces() {
		return Collections.unmodifiableSet(Metadata.managementInterfaces);
	}

	public static <T extends PlatformManagedObject> T getPlatformMXBean(Class<T> mxbeanInterface) {
		checkSupportedSingleton(mxbeanInterface);

		List<PlatformManagedObject> beans = Metadata.beansByInterface.get(mxbeanInterface);

		if (beans != null && beans.size() == 1) {
			PlatformManagedObject bean = beans.get(0);

			if (mxbeanInterface.isAssignableFrom(bean.getClass())) {
				return mxbeanInterface.cast(bean);
			}
		}

		return null;
	}

	public static <T extends PlatformManagedObject> List<T> getPlatformMXBeans(Class<T> mxbeanInterface)
			throws IllegalArgumentException {
		checkSupported(mxbeanInterface);

		List<T> matchedBeans = new LinkedList<>();
		List<PlatformManagedObject> beans = Metadata.beansByInterface.get(mxbeanInterface);

		if (beans != null) {
			for (PlatformManagedObject bean : beans) {
				matchedBeans.add(mxbeanInterface.cast(bean));
			}
		}

		return matchedBeans;
	}

	/**
	 * This class registers all components; it is only used
	 * by the static constructor of the Metadata class.
	 *
	 * @param <T> the implementation type of the bean(s), which allows
	 *            most interfaces to be validated at compile time
	 */
	private static final class Component<T extends PlatformManagedObject> {

		private static final String GUEST_OPERATING_SYSTEM_MXBEAN_NAME = "com.ibm.virtualization.management:type=GuestOS"; //$NON-NLS-1$

		private static final String HYPERVISOR_MXBEAN_NAME = "com.ibm.virtualization.management:type=Hypervisor"; //$NON-NLS-1$

		private static final String JVM_CPU_MONITOR_MXBEAN_NAME = "com.ibm.lang.management:type=JvmCpuMonitor"; //$NON-NLS-1$
		private static final String OPENJ9_DIAGNOSTICS_MXBEAN_NAME = "openj9.lang.management:type=OpenJ9Diagnostics"; //$NON-NLS-1$

		static void registerAll() {
			// register standard singleton beans
			create(ManagementFactory.CLASS_LOADING_MXBEAN_NAME, ClassLoadingMXBeanImpl.getInstance())
				.addInterface(java.lang.management.ClassLoadingMXBean.class)
				.validateAndRegister();

			create(ManagementFactory.MEMORY_MXBEAN_NAME, ExtendedMemoryMXBeanImpl.getInstance())
				.addInterface(com.ibm.lang.management.MemoryMXBean.class)
				.addInterface(java.lang.management.MemoryMXBean.class)
				.validateAndRegister();

			create(ManagementFactory.OPERATING_SYSTEM_MXBEAN_NAME, com.ibm.lang.management.internal.ExtendedOperatingSystemMXBeanImpl.getInstance())
				.addInterface(com.sun.management.OperatingSystemMXBean.class)
				.addInterface(com.ibm.lang.management.OperatingSystemMXBean.class)
				.addInterface(java.lang.management.OperatingSystemMXBean.class)
				.addInterfaceIf(com.ibm.lang.management.UnixOperatingSystemMXBean.class, isRunningOnUnix())
				.addInterfaceIf(com.sun.management.UnixOperatingSystemMXBean.class, isRunningOnUnix())
				.validateAndRegister();

			create(LogManager.LOGGING_MXBEAN_NAME, LoggingMXBeanImpl.getInstance())
				.addInterface(java.lang.management.PlatformLoggingMXBean.class)
				.addInterface(java.util.logging.LoggingMXBean.class)
				.validateAndRegister();

			create(ManagementFactory.RUNTIME_MXBEAN_NAME, com.ibm.lang.management.internal.ExtendedRuntimeMXBeanImpl.getInstance())
				.addInterface(com.ibm.lang.management.RuntimeMXBean.class)
				.addInterface(java.lang.management.RuntimeMXBean.class)
				.validateAndRegister();

			create(ManagementFactory.THREAD_MXBEAN_NAME, com.ibm.lang.management.internal.ExtendedThreadMXBeanImpl.getInstance())
				.addInterface(com.ibm.lang.management.ThreadMXBean.class)
				.addInterface(java.lang.management.ThreadMXBean.class)
				.validateAndRegister();

			// register OpenJ9-specific singleton beans
			create(GUEST_OPERATING_SYSTEM_MXBEAN_NAME, com.ibm.virtualization.management.internal.GuestOS.getInstance())
				.addInterface(com.ibm.virtualization.management.GuestOSMXBean.class)
				.validateAndRegister();

			create(HYPERVISOR_MXBEAN_NAME, com.ibm.virtualization.management.internal.HypervisorMXBeanImpl.getInstance())
				.addInterface(com.ibm.virtualization.management.HypervisorMXBean.class)
				.validateAndRegister();

			create(JVM_CPU_MONITOR_MXBEAN_NAME, com.ibm.lang.management.internal.JvmCpuMonitor.getInstance())
				.addInterface(com.ibm.lang.management.JvmCpuMonitorMXBean.class)
				.validateAndRegister();

			create(OPENJ9_DIAGNOSTICS_MXBEAN_NAME, openj9.lang.management.internal.OpenJ9DiagnosticsMXBeanImpl.getInstance())
				.addInterface(openj9.lang.management.OpenJ9DiagnosticsMXBean.class)
				.validateAndRegister();

			// register standard optional beans
			create(ManagementFactory.COMPILATION_MXBEAN_NAME, CompilationMXBeanImpl.getInstance())
				.addInterface(java.lang.management.CompilationMXBean.class)
				.validateAndRegister();

			// register beans with zero or more instances
			create(BUFFERPOOL_MXBEAN_DOMAIN_TYPE, BufferPoolMXBeanImpl.getBufferPoolMXBeans())
				.addInterface(java.lang.management.BufferPoolMXBean.class)
				.validateAndRegister();

			create(ManagementFactory.GARBAGE_COLLECTOR_MXBEAN_DOMAIN_TYPE, ExtendedMemoryMXBeanImpl.getInstance().getGarbageCollectorMXBeans())
				.addInterfaceIf(com.ibm.lang.management.GarbageCollectorMXBean.class, true)
				.addInterfaceIf(com.sun.management.GarbageCollectorMXBean.class, true)
				.addInterface(java.lang.management.GarbageCollectorMXBean.class)
				.addInterface(java.lang.management.MemoryManagerMXBean.class)
				.validateAndRegister();

			create(ManagementFactory.MEMORY_MANAGER_MXBEAN_DOMAIN_TYPE,
					// exclude garbage collector beans handled above
					excluding(ExtendedMemoryMXBeanImpl.getInstance().getMemoryManagerMXBeans(false), java.lang.management.GarbageCollectorMXBean.class))
				.addInterface(java.lang.management.MemoryManagerMXBean.class)
				.validateAndRegister();

			create(ManagementFactory.MEMORY_POOL_MXBEAN_DOMAIN_TYPE, ExtendedMemoryMXBeanImpl.getInstance().getMemoryPoolMXBeans(false))
				.addInterfaceIf(com.ibm.lang.management.MemoryPoolMXBean.class, true)
				.addInterface(java.lang.management.MemoryPoolMXBean.class)
				.validateAndRegister();
		}

		private static <T> void addNew(Set<T> values, T value) {
			if (!values.add(value)) {
				throw new IllegalArgumentException();
			}
		}

		private static void checkNames(Collection<? extends PlatformManagedObject> beans, ObjectName pattern) {
			for (PlatformManagedObject bean : beans) {
				ObjectName objectName = bean.getObjectName();

				if (!pattern.apply(objectName)) {
					/*[MSG "K0615", "name '{0}' does not match pattern '{1}'"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0615", //$NON-NLS-1$
							objectName.getCanonicalName(), pattern));
				}

				if (objectName.isPattern()) {
					/*[MSG "K0616", "name '{0}' must not be a pattern"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0616", //$NON-NLS-1$
							objectName.getCanonicalName()));
				}
			}
		}

		private static <T extends PlatformManagedObject> Component<T> create(String objectNamePattern, T bean) {
			return new Component<>(objectNamePattern, bean);
		}

		private static <T extends PlatformManagedObject> Component<T> create(String objectNameBase, Collection<? extends T> beans) {
			return new Component<>(objectNameBase + ",name=*", beans); //$NON-NLS-1$
		}

		private static <T> List<T> excluding(List<T> objects, Class<?> excludedType) {
			List<T> filtered = new ArrayList<>();

			for (T object : objects) {
				if (!excludedType.isInstance(object)) {
					filtered.add(object);
				}
			}

			return filtered;
		}

		private static <K, V> void putNew(Map<K, V> map, K key, V value) {
			if (map.containsKey(key)) {
				throw new IllegalArgumentException();
			}

			map.put(key, value);
		}

		private final Map<ObjectName, PlatformManagedObject> beansByName;

		private final Set<Class<?>> interfaceTypes;

		private final boolean isSingleton;

		private Component(String objectName, T bean) {
			super();

			ObjectName name = Metadata.makeObjectName(objectName);

			if (name.isPattern()) {
				throw new IllegalArgumentException(objectName);
			}

			if (bean == null) {
				this.beansByName = Collections.emptyMap();
			} else {
				checkNames(Collections.singleton(bean), name);
				this.beansByName = Collections.singletonMap(name, bean);
			}
			this.interfaceTypes = new HashSet<>();
			this.isSingleton = true;
		}

		private Component(String objectNamePattern, Collection<? extends T> beans) {
			super();

			ObjectName pattern = Metadata.makeObjectName(objectNamePattern);

			if (!pattern.isPattern()) {
				throw new IllegalArgumentException(objectNamePattern);
			}

			checkNames(beans, pattern);

			this.beansByName = new HashMap<>(beans.size());
			this.interfaceTypes = new HashSet<>();
			this.isSingleton = true;

			for (PlatformManagedObject bean : beans) {
				putNew(beansByName, bean.getObjectName(), bean);
			}
		}

		private Component<T> addInterface(Class<? super T> interfaceType) {
			addNew(interfaceTypes, interfaceType);

			return this;
		}

		private Component<T> addInterfaceIf(Class<?> interfaceType, boolean required) {
			if (required) {
				for (Object implementation : beansByName.values()) {
					if (!interfaceType.isInstance(implementation)) {
						throw new IllegalArgumentException();
					}
				}

				addNew(interfaceTypes, interfaceType);
			}

			return this;
		}

		private void validateAndRegister() {
			Collection<PlatformManagedObject> allBeans = beansByName.values();

			// all beans should implement all the provided interfaces
			for (PlatformManagedObject bean : allBeans) {
				for (Class<?> interfaceType : interfaceTypes) {
					if (!interfaceType.isInstance(bean)) {
						throw new IllegalArgumentException();
					}
				}
			}

			Map<Class<?>, List<PlatformManagedObject>> beansByInterface = Metadata.beansByInterface;

			for (PlatformManagedObject bean : allBeans) {
				for (Class<?> interfaceType : interfaceTypes) {
					List<PlatformManagedObject> beans = beansByInterface.get(interfaceType);

					if (beans == null) {
						beans = new ArrayList<>(2);
						beansByInterface.put(interfaceType, beans);
					}

					beans.add(bean);
				}
			}

			Set<Class<? extends PlatformManagedObject>> managementInterfaces = Metadata.managementInterfaces;

			for (Class<?> interfaceType : interfaceTypes) {
				if (PlatformManagedObject.class.isAssignableFrom(interfaceType)) {
					@SuppressWarnings("unchecked")
					Class<? extends PlatformManagedObject> managementInterface = (Class<? extends PlatformManagedObject>) interfaceType;

					managementInterfaces.add(managementInterface);
				}
			}

			if (!isSingleton) {
				Metadata.multiInstanceBeanInterfaces.addAll(interfaceTypes);
			}

			Map<Class<?>, ObjectName[]> objectNamesByInterface = Metadata.objectNamesByInterface;
			Set<ObjectName> beanNames = beansByName.keySet();
			int beanNameCount = beanNames.size();

			for (Class<?> interfaceType : interfaceTypes) {
				ObjectName[] nameArray = objectNamesByInterface.get(interfaceType);
				int oldLength;

				if (nameArray == null) {
					oldLength = 0;
					nameArray = new ObjectName[beanNameCount];
				} else {
					oldLength = nameArray.length;
					System.arraycopy(nameArray, 0, nameArray = new ObjectName[oldLength + beanNameCount], 0, oldLength);
				}

				for (ObjectName beanName : beanNames) {
					nameArray[oldLength++] = beanName;
				}

				objectNamesByInterface.put(interfaceType, nameArray);
			}
		}

	}

	private static final class Metadata {

		static final Map<Class<?>, List<PlatformManagedObject>> beansByInterface;

		static final Set<Class<? extends PlatformManagedObject>> managementInterfaces;

		static final Set<Class<?>> multiInstanceBeanInterfaces;

		static final Map<Class<?>, ObjectName[]> objectNamesByInterface;

		static {
			beansByInterface = new HashMap<>();
			managementInterfaces = new HashSet<>();
			multiInstanceBeanInterfaces = new HashSet<>();
			objectNamesByInterface = new HashMap<>();

			Component.registerAll();
		}

		static ObjectName makeObjectName(String mxbeanName) {
			try {
				return ObjectName.getInstance(mxbeanName);
			} catch (MalformedObjectNameException e) {
				/*[MSG "K0604", "{0} is not a valid ObjectName format."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0604", mxbeanName), e); //$NON-NLS-1$
			}
		}

		static boolean isMultiInstanceBeanInterface(Class<?> mxbeanInterface) {
			return multiInstanceBeanInterfaces.contains(mxbeanInterface);
		}

	}

	/*[ENDIF] !Sidecar19-SE*/

}
