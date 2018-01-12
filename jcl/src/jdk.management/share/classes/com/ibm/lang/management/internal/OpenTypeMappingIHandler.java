/*[INCLUDE-IF Sidecar17 & !Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2005, 2017 IBM Corp. and others
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
package com.ibm.lang.management.internal;

import java.io.IOException;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.security.AccessController;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.util.function.Predicate;

import javax.management.Attribute;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.ListenerNotFoundException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;
import javax.management.MBeanServerConnection;
import javax.management.NotificationEmitter;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.RuntimeMBeanException;

import com.ibm.java.lang.management.internal.ManagementUtils;

/**
 * Concrete instance of the {@link InvocationHandler} interface that is used to
 * handle method invocations on MXBeans that have been obtained using the proxy
 * method.
 */
public final class OpenTypeMappingIHandler implements InvocationHandler {

	private static enum InvokeType {
		ATTRIBUTE_GETTER, ATTRIBUTE_SETTER, NOTIFICATION_OP, OPERATION
	}

	/**
	 * Obtain a string array of all of the argument types (in the correct order)
	 * for the method on this handler's target object. The signature types will
	 * all be open types.
	 *
	 * @param method
	 *            a {@link Method} instance that describes an operation on the
	 *            target object.
	 * @return an array of strings with each element holding the fully qualified
	 *         name of the corresponding argument to the method. The order of
	 *         the array elements corresponds exactly with the order of the
	 *         method arguments.
	 */
	private static String[] getOperationSignature(Method method) {
		Class<?>[] types = method.getParameterTypes();
		String[] result = new String[types.length];

		for (int i = 0; i < types.length; ++i) {
			result[i] = types[i].getName();
		}

		return result;
	}

	private final MBeanServerConnection connection;

	private final MBeanInfo info;

	private final ObjectName mxBeanObjectName;

	/**
	 *
	 * @param connection
	 *            the MBeanServerConnection to forward to.
	 * @param mxBeanType
	 *            the fully qualified name of an <code>MXBean</code>.
	 * @param mxBeanObjectName
	 *            the name of a platform MXBean within connection to forward to
	 * @throws IOException
	 *            if a problem occurs communicating with the server
	 */
	public OpenTypeMappingIHandler(MBeanServerConnection connection, Class<?> mxBeanType, ObjectName mxBeanObjectName)
			throws IOException {
		super();
		this.connection = connection;
		this.mxBeanObjectName = mxBeanObjectName;
		checkBeanIsRegistered();

		try {
			this.info = connection.getMBeanInfo(mxBeanObjectName);
		} catch (InstanceNotFoundException | IntrospectionException | ReflectionException e) {
			// According to the server, the bean is registered: it should be able to provide information about it.
			throw new IllegalArgumentException(e);
		}
	}

	/**
	 * @throws IOException
	 */
	private void checkBeanIsRegistered() throws IOException {
		if (!this.connection.isRegistered(this.mxBeanObjectName)) {
			/*[MSG "K05F4", "Not registered : {0}"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F4", this.mxBeanObjectName)); //$NON-NLS-1$
		}
	}

	/**
	 * Obtain the {@link MBeanAttributeInfo} meta data for the attribute of this
	 * invocation handler's target object that is returned by a call to the
	 * method called <code>methodName</code>.
	 *
	 * @param methodName
	 *            the name of the getter method on the attribute under scrutiny.
	 * @return the <code>MBeanAttributeInfo</code> that describes the
	 *         attribute.
	 */
	private MBeanAttributeInfo getAttribInfo(String methodName) {
		MBeanAttributeInfo[] attribs = info.getAttributes();

		if (methodName.startsWith("get")) { //$NON-NLS-1$
			String attribName = methodName.substring("get".length()); //$NON-NLS-1$

			for (MBeanAttributeInfo attribInfo : attribs) {
				if (attribName.equals(attribInfo.getName()) && attribInfo.isReadable()) {
					return attribInfo;
				}
			}
		} else if (methodName.startsWith("is")) { //$NON-NLS-1$
			String attribName = methodName.substring("is".length()); //$NON-NLS-1$

			for (MBeanAttributeInfo attribInfo : attribs) {
				if (attribName.equals(attribInfo.getName()) && attribInfo.isReadable() && attribInfo.isIs()) {
					return attribInfo;
				}
			}
		}

		return null;
	}

	/**
	 * For the target type associated with this invocation handler, returns the
	 * name of the attribute that will be queried or updated by the Java method
	 * described in <code>method</code>.
	 *
	 * @param method
	 *            a {@link Method} object describing a method on this handler's
	 *            target object.
	 * @return the name of the attribute that will be queried or modified by an
	 *         invocation of the method described by <code>method</code>.
	 */
	private String getAttribName(Method method) {
		String methodName = method.getName();
		String attribName;
		Predicate<MBeanAttributeInfo> attribTest;

		if (methodName.startsWith("get")) { //$NON-NLS-1$
			attribName = methodName.substring("get".length()); //$NON-NLS-1$
			attribTest = attribInfo -> attribInfo.isReadable();
		} else if (methodName.startsWith("is")) { //$NON-NLS-1$
			attribName = methodName.substring("is".length()); //$NON-NLS-1$
			attribTest = attribInfo -> attribInfo.isReadable() && attribInfo.isIs();
		} else if (methodName.startsWith("set")) { //$NON-NLS-1$
			attribName = methodName.substring("set".length()); //$NON-NLS-1$
			attribTest = attribInfo -> attribInfo.isWritable();
		} else {
			return null;
		}

		MBeanAttributeInfo[] attribs = info.getAttributes();

		for (MBeanAttributeInfo attribInfo : attribs) {
			if (attribName.equals(attribInfo.getName()) && attribTest.test(attribInfo)) {
				return attribName;
			}
		}

		return null;
	}

	/**
	 * Returns the name of the fully qualified open type of the attribute of the
	 * target type that is obtained from a call to the <code>methodName</code>
	 * method.
	 *
	 * @param methodName
	 *            the name of a getter method on an attribute of the target
	 *            type.
	 * @return the fully qualified name of the implied attribute's <i>open </i>
	 *         type.
	 */
	private String getAttrOpenType(String methodName) {
		MBeanAttributeInfo attrInfo = getAttribInfo(methodName);

		return attrInfo.getType();
	}

	/**
	 * Determine the type of invocation being made on the target object.
	 *
	 * @param methodName
	 * @return an instance of <code>InvokeType</code> corresponding to the
	 *         nature of the operation that the caller is attempting to make on
	 *         the target object.
	 */
	private InvokeType getInvokeType(String methodName) {
		if (methodName.startsWith("get")) { //$NON-NLS-1$
			String attribName = methodName.substring("get".length()); //$NON-NLS-1$
			MBeanAttributeInfo[] attribs = info.getAttributes();

			for (MBeanAttributeInfo attribInfo : attribs) {
				if (attribName.equals(attribInfo.getName()) && attribInfo.isReadable()) {
					return InvokeType.ATTRIBUTE_GETTER;
				}
			}
		} else if (methodName.startsWith("is")) { //$NON-NLS-1$
			String attribName = methodName.substring("is".length()); //$NON-NLS-1$
			MBeanAttributeInfo[] attribs = info.getAttributes();

			for (MBeanAttributeInfo attribInfo : attribs) {
				if (attribName.equals(attribInfo.getName()) && attribInfo.isReadable() && attribInfo.isIs()) {
					return InvokeType.ATTRIBUTE_GETTER;
				}
			}
		} else if (methodName.startsWith("set")) { //$NON-NLS-1$
			String attribName = methodName.substring("set".length()); //$NON-NLS-1$
			MBeanAttributeInfo[] attribs = info.getAttributes();

			for (MBeanAttributeInfo attribInfo : attribs) {
				if (attribName.equals(attribInfo.getName()) && attribInfo.isWritable()) {
					return InvokeType.ATTRIBUTE_SETTER;
				}
			}
		}

		Method[] neMethods = NotificationEmitter.class.getMethods();
		for (Method neMethod : neMethods) {
			if (methodName.equals(neMethod.getName())) {
				return InvokeType.NOTIFICATION_OP;
			}
		}

		// If not a getter or setter or a notification emitter method then
		// must be a vanilla DynamicMXBean operation.
		return InvokeType.OPERATION;
	}

	/**
	 * For the method available on this handler's target object that is
	 * described by the {@link Method} instance, return a string representation
	 * of the return type after mapping to an open type.
	 *
	 * @param method
	 *            an instance of <code>Method</code> that describes a method
	 *            on the target object.
	 * @return a string containing the open return type of the method specified
	 *         by <code>method</code>.
	 */
	private String getOperationOpenReturnType(Method method) {
		String methodName = method.getName();
		String[] methodSig = getOperationSignature(method);
		MBeanOperationInfo[] opInfos = this.info.getOperations();

		methods: for (MBeanOperationInfo opInfo : opInfos) {
			if (!methodName.equals(opInfo.getName())) {
				continue;
			}

			MBeanParameterInfo[] opParams = opInfo.getSignature();

			if (opParams.length != methodSig.length) {
				continue;
			}

			for (int i = 0; i < opParams.length; i++) {
				if (!methodSig[i].equals(opParams[i].getType())) {
					continue methods;
				}
			}

			return opInfo.getReturnType();
		}

		return null;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
		Object result = null;
		// Carry out the correct operation according to what the caller is
		// trying to do (set/get an attribute or invoke an operation. Each of
		// the below handler methods is expected to manage the conversion of
		// input args to open types and the conversion of return values from
		// an open type to a Java type.
		String methodName = method.getName();
		switch (getInvokeType(methodName)) {
		case ATTRIBUTE_GETTER:
			result = invokeAttributeGetter(method);
			break;
		case ATTRIBUTE_SETTER:
			result = invokeAttributeSetter(method, args);
			break;
		case NOTIFICATION_OP:
			result = invokeNotificationEmitterOperation(method, args);
			break;
		default:
			if ("toString".equals(methodName) && (args == null || args.length == 0)) { //$NON-NLS-1$
				result = "MXBeanProxy(" + connection + "[" + mxBeanObjectName + "])"; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
			} else if ("getObjectName".equals(methodName) && (args == null || args.length == 0)) { //$NON-NLS-1$
				result = mxBeanObjectName;
			} else {
				result = invokeOperation(method, args);
			}
			break;
		}
		return result;
	}

	/**
	 * Invoke the attribute get operation described by {@link Method} instance
	 * <code>method</code> on this handler's target object.
	 * <p>
	 * All returned values are automatically converted to their corresponding
	 * MBean open types.
	 * </p>
	 *
	 * @param method
	 *            describes the getter operation to be invoked on the target
	 *            object
	 * @return the returned value from the getter call on the target object with
	 *         any MBean open type values being automatically converted to their
	 *         Java counterparts.
	 * @throws IOException
	 * @throws ReflectionException
	 * @throws MBeanException
	 * @throws InstanceNotFoundException
	 * @throws AttributeNotFoundException
	 * @throws ClassNotFoundException
	 * @throws IllegalAccessException
	 * @throws InstantiationException
	 * @throws InvocationTargetException
	 * @throws NoSuchMethodException
	 * @throws IllegalArgumentException
	 * @throws SecurityException
	 */
	private Object invokeAttributeGetter(final Method method)
			throws AttributeNotFoundException, InstanceNotFoundException,
			MBeanException, ReflectionException, IOException,
			ClassNotFoundException, InstantiationException,
			IllegalAccessException, SecurityException,
			IllegalArgumentException, NoSuchMethodException,
			InvocationTargetException {
		Object result = null;
		String realReturnType = method.getReturnType().getName();
		String openReturnType = getAttrOpenType(method.getName());
		PrivilegedExceptionAction<Object> action = () -> connection.getAttribute(mxBeanObjectName, getAttribName(method));

		try {
			result = AccessController.doPrivileged(action);
		} catch (PrivilegedActionException e) {
			// PrivilegedActionException wraps only checked exceptions
			Throwable t = e.getCause();
			if (t instanceof AttributeNotFoundException) {
				throw (AttributeNotFoundException) t;
			} else if (t instanceof InstanceNotFoundException) {
				throw (InstanceNotFoundException) t;
			} else if (t instanceof MBeanException) {
				throw (MBeanException) t;
			} else if (t instanceof ReflectionException) {
				throw (ReflectionException) t;
			} else if (t instanceof IOException) {
				throw (IOException) t;
			} else {
				// this should not happen: getAttribute() declares only the above checked exceptions
				throw new RuntimeException(t);
			}
		} catch (RuntimeMBeanException e) {
			// RuntimeMBeanException wraps unchecked exceptions from the MBean server
			throw e.getTargetException();
		}

		if (!realReturnType.equals(openReturnType)) {
			result = ManagementUtils.convertFromOpenType(
					result,
					Class.forName(openReturnType),
					Class.forName(realReturnType));
		}

		return result;
	}

	/**
	 * Invoke the attribute set operation described by {@link Method} instance
	 * <code>method</code> on this handler's target object.
	 * <p>
	 * All argument values are automatically converted to their corresponding
	 * MBean open types.
	 * </p>
	 *
	 * @param method
	 *            describes the operation to be invoked on the target object
	 * @param args
	 *            the arguments to be used in the operation call
	 * @return the returned value from the operation call on the target object
	 *         with any MBean open type values being automatically converted to
	 *         their Java counterparts.
	 * @throws IOException
	 * @throws ReflectionException
	 * @throws MBeanException
	 * @throws InstanceNotFoundException
	 * @throws AttributeNotFoundException
	 * @throws InvalidAttributeValueException
	 */
	private Object invokeAttributeSetter(final Method method,
			final Object[] args) throws AttributeNotFoundException,
			InstanceNotFoundException, MBeanException, ReflectionException,
			IOException, InvalidAttributeValueException {
		// In Java 5.0 platform MXBeans the following applies for all
		// attribute setter methods :
		// 1. no conversion to open MBean types necessary
		// 2. all setter arguments are single value (i.e. not array or
		// collection types).
		// 3. all return null
		PrivilegedExceptionAction<Void> action = () -> {
			connection.setAttribute(mxBeanObjectName, new Attribute(getAttribName(method), args[0]));
			return null;
		};

		try {
			AccessController.doPrivileged(action);
		} catch (PrivilegedActionException e) {
			// PrivilegedActionException wraps only checked exceptions
			Throwable t = e.getCause();
			if (t instanceof InstanceNotFoundException) {
				throw (InstanceNotFoundException) t;
			} else if (t instanceof AttributeNotFoundException) {
				throw (AttributeNotFoundException) t;
			} else if (t instanceof InvalidAttributeValueException) {
				throw (InvalidAttributeValueException) t;
			} else if (t instanceof MBeanException) {
				throw (MBeanException) t;
			} else if (t instanceof ReflectionException) {
				throw (ReflectionException) t;
			} else if (t instanceof IOException) {
				throw (IOException) t;
			} else {
				// this should not happen: setAttribute() declares only the above checked exceptions
				throw new RuntimeException(t);
			}
		} catch (RuntimeMBeanException e) {
			// RuntimeMBeanException wraps unchecked exceptions from the MBean server
			throw e.getTargetException();
		}

		return null;
	}

	/**
	 * Invoke the event notification operation described by the
	 * {@link Method} instance <code>method</code> on this handler's target object.
	 * @param method
	 *            describes the operation to be invoked on the target object
	 * @param args
	 *            the arguments to be used in the operation call
	 * @return a <code>null</code> representing the <code>void</code> return
	 * from all {@link javax.management.NotificationEmitter} methods.
	 * @throws IOException
	 * @throws InstanceNotFoundException
	 * @throws ListenerNotFoundException
	 */
	private Object invokeNotificationEmitterOperation(Method method,
			final Object[] args) throws InstanceNotFoundException, IOException,
			ListenerNotFoundException {
		final String methodName = method.getName();
		Object result = null;
		if ("addNotificationListener".equals(methodName)) { //$NON-NLS-1$
			PrivilegedExceptionAction<Void> action = () -> {
				connection.addNotificationListener(
						mxBeanObjectName,
						(NotificationListener) args[0],
						(NotificationFilter) args[1],
						args[2]);
				return null;
			};

			try {
				AccessController.doPrivileged(action);
			} catch (PrivilegedActionException e) {
				// PrivilegedActionException wraps only checked exceptions
				Throwable t = e.getCause();
				if (t instanceof InstanceNotFoundException) {
					throw (InstanceNotFoundException) t;
				} else if (t instanceof IOException) {
					throw (IOException) t;
				} else {
					// this should not happen: addNotificationListener() declares only the above checked exceptions
					throw new RuntimeException(t);
				}
			} catch (RuntimeMBeanException e) {
				// RuntimeMBeanException wraps unchecked exceptions from the MBean server
				throw e.getTargetException();
			}
		} else if ("getNotificationInfo".equals(methodName)) { //$NON-NLS-1$
			result = this.info.getNotifications();
		} else if ("removeNotificationListener".equals(methodName)) { //$NON-NLS-1$
			if (args.length == 1) {
				PrivilegedExceptionAction<Void> action = () -> {
					connection.removeNotificationListener(mxBeanObjectName, (NotificationListener) args[0]);
					return null;
				};

				try {
					AccessController.doPrivileged(action);
				} catch (PrivilegedActionException e) {
					// PrivilegedActionException wraps only checked exceptions
					Throwable t = e.getCause();
					if (t instanceof InstanceNotFoundException) {
						throw (InstanceNotFoundException) t;
					} else if (t instanceof ListenerNotFoundException) {
						throw (ListenerNotFoundException) t;
					} else if (t instanceof IOException) {
						throw (IOException) t;
					} else {
						// this should not happen: removeNotificationListener() declares only the above checked exceptions
						throw new RuntimeException(t);
					}
				} catch (RuntimeMBeanException e) {
					// RuntimeMBeanException wraps unchecked exceptions from the MBean server
					throw e.getTargetException();
				}
			} else {
				PrivilegedExceptionAction<Void> action = () -> {
					connection.removeNotificationListener(
							mxBeanObjectName,
							(NotificationListener) args[0],
							(NotificationFilter) args[1],
							args[2]);
					return null;
				};

				try {
					AccessController.doPrivileged(action);
				} catch (PrivilegedActionException e) {
					// PrivilegedActionException wraps only checked exceptions
					Throwable t = e.getCause();
					if (t instanceof InstanceNotFoundException) {
						throw (InstanceNotFoundException) t;
					} else if (t instanceof ListenerNotFoundException) {
						throw (ListenerNotFoundException) t;
					} else if (t instanceof IOException) {
						throw (IOException) t;
					} else {
						// this should not happen: removeNotificationListener() declares only the above checked exceptions
						throw new RuntimeException(t);
					}
				} catch (RuntimeMBeanException e) {
					// RuntimeMBeanException wraps unchecked exceptions from the MBean server
					throw e.getTargetException();
				}
			}
		}

		return result;
	}

	/**
	 * Invoke the operation described by {@link Method} instance
	 * <code>method</code> on this handler's target object.
	 * <p>
	 * All argument values are automatically converted to their corresponding
	 * MBean open types.
	 * </p>
	 * <p>
	 * If the method return is an MBean open type value it will be automatically
	 * converted to a Java type by this method.
	 * </p>
	 *
	 * @param method
	 *            describes the operation to be invoked on the target object
	 * @param args
	 *            the arguments to be used in the operation call
	 * @return the returned value from the operation call on the target object
	 *         with any MBean open type values being automatically converted to
	 *         their Java counterparts.
	 * @throws IOException
	 * @throws ReflectionException
	 * @throws MBeanException
	 * @throws InstanceNotFoundException
	 * @throws IllegalAccessException
	 * @throws InstantiationException
	 * @throws ClassNotFoundException
	 * @throws InvocationTargetException
	 * @throws NoSuchMethodException
	 * @throws IllegalArgumentException
	 * @throws SecurityException
	 */
	private Object invokeOperation(final Method method, final Object[] args)
			throws InstanceNotFoundException, MBeanException,
			ReflectionException, IOException, ClassNotFoundException,
			InstantiationException, IllegalAccessException, SecurityException,
			IllegalArgumentException, NoSuchMethodException,
			InvocationTargetException {
		// For Java 5.0 platform MXBeans, no conversion
		// to open MBean types is necessary for any of the arguments.
		// i.e. they are all simple types.
		PrivilegedExceptionAction<Object> action = () -> connection.invoke(
				mxBeanObjectName,
				method.getName(),
				args,
				getOperationSignature(method));
		Object result = null;

		try {
			result = AccessController.doPrivileged(action);
		} catch (PrivilegedActionException e) {
			// PrivilegedActionException wraps only checked exceptions
			Throwable t = e.getCause();
			if (t instanceof InstanceNotFoundException) {
				throw (InstanceNotFoundException) t;
			} else if (t instanceof MBeanException) {
				throw (MBeanException) t;
			} else if (t instanceof ReflectionException) {
				throw (ReflectionException) t;
			} else if (t instanceof IOException) {
				throw (IOException) t;
			} else {
				// this should not happen: invoke() declares only the above checked exceptions
				throw new RuntimeException(t);
			}
		} catch (RuntimeMBeanException e) {
			// RuntimeMBeanException wraps unchecked exceptions from the MBean server
			throw e.getTargetException();
		}

		String realReturnType = method.getReturnType().getName();
		String openReturnType = getOperationOpenReturnType(method);

		if (!realReturnType.equals(openReturnType)) {
			result = ManagementUtils.convertFromOpenType(
					result,
					Class.forName(openReturnType),
					Class.forName(realReturnType));
		}

		return result;
	}

}
