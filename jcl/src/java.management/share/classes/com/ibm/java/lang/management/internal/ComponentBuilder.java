/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

import java.lang.management.PlatformManagedObject;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import sun.management.spi.PlatformMBeanProvider.PlatformComponent;

/**
 * This class helps construct the list of all components;
 * it is only used by @{link DefaultPlatformMBeanProvider}
 * and {@link com.ibm.lang.management.internal.PlatformMBeanProvider}.
 *
 * @param <T> the implementation type of the bean(s)
 *            this allows most interfaces to be validated at compile time
 */
public final class ComponentBuilder<T extends PlatformManagedObject> {

	private static abstract class Component implements PlatformComponent<Object> {

		private static <T> Set<T> compactUnmodifiableSet(Set<T> set) {
			switch (set.size()) {
			case 0:
				return Collections.emptySet();

			case 1:
				return Collections.singleton(set.iterator().next());

			default:
				return Collections.unmodifiableSet(set);
			}
		}

		private final Set<Class<?>> interfaceTypes;

		private final String objectNamePattern;

		Component(String objectNamePattern, Set<Class<?>> interfaceTypes) {
			super();
			this.interfaceTypes = compactUnmodifiableSet(interfaceTypes);
			this.objectNamePattern = objectNamePattern;
		}

		@Override
		public final String getObjectNamePattern() {
			return objectNamePattern;
		}

		@Override
		public final Set<String> mbeanInterfaceNames() {
			Set<String> interfaceNames = new HashSet<>(interfaceTypes.size());

			for (Class<?> interfaceType : interfaceTypes) {
				interfaceNames.add(interfaceType.getName());
			}

			return interfaceNames;
		}

		@Override
		public final Set<Class<?>> mbeanInterfaces() {
			return interfaceTypes;
		}

	}

	private static final class Group extends Component {

		private final Map<String, Object> nameToMBeanMap;

		Group(String objectNameBase, List<? extends PlatformManagedObject> implementations,
				Set<Class<?>> interfaceTypes) {
			super(objectNameBase, interfaceTypes);

			Map<String, Object> beanMap = new HashMap<>(implementations.size());

			for (PlatformManagedObject implementation : implementations) {
				beanMap.put(implementation.getObjectName().getCanonicalName(), implementation);
			}

			this.nameToMBeanMap = Collections.unmodifiableMap(beanMap);
		}

		@Override
		public boolean isSingleton() {
			return false;
		}

		@Override
		public Map<String, Object> nameToMBeanMap() {
			return nameToMBeanMap;
		}

	}

	private static final class Missing extends Component {

		Missing(String objectNamePattern, Set<Class<?>> interfaceTypes) {
			super(objectNamePattern, interfaceTypes);
		}

		@Override
		public Map<String, Object> nameToMBeanMap() {
			return Collections.emptyMap();
		}

	}

	private static final class Singleton extends Component {

		private final Object implementation;

		Singleton(PlatformManagedObject implementation, Set<Class<?>> interfaceTypes) {
			super(implementation.getObjectName().getCanonicalName(), interfaceTypes);
			this.implementation = implementation;
		}

		@Override
		public Map<String, Object> nameToMBeanMap() {
			return Collections.singletonMap(getObjectNamePattern(), implementation);
		}

	}

	/**
	 * Create a new builder for a list of beans all matching the object name pattern.
	 *
	 * @param objectNameBase the object name base; ",name=*" will be appended to form the pattern
	 * @param implementations the implementation beans
	 * @return a new builder object
	 */
	public static <T extends PlatformManagedObject> ComponentBuilder<T> create(String objectNameBase,
			List<T> implementations) {
		return new ComponentBuilder<>(objectNameBase, implementations);
	}

	/**
	 * Create a new builder for a named, optional singleton.
	 *
	 * @param objectName the object name
	 * @param implementation the implementation or null
	 * @return a new builder object
	 */
	public static <T extends PlatformManagedObject> ComponentBuilder<T> create(String objectName, T implementation) {
		return new ComponentBuilder<>(objectName, implementation);
	}

	/**
	 * Create a new builder for a required singleton. The implementation
	 * will provide the object name.
	 *
	 * @param implementation the implementation
	 * @return a new builder object
	 */
	public static <T extends PlatformManagedObject> ComponentBuilder<T> create(T implementation) {
		return new ComponentBuilder<>(implementation);
	}

	private final List<T> implementations;

	private final Set<Class<?>> interfaceTypes;

	private final String objectNamePattern;

	private final boolean singleton;

	private ComponentBuilder(String objectNameBase, List<T> implementations) {
		super();
		this.implementations = implementations;
		this.interfaceTypes = new HashSet<>();
		this.objectNamePattern = objectNameBase + ",name=*"; //$NON-NLS-1$
		this.singleton = false;
	}

	private ComponentBuilder(String objectName, T implementation) {
		super();
		this.implementations = Collections.singletonList(implementation);
		this.interfaceTypes = new HashSet<>();
		this.objectNamePattern = objectName;
		this.singleton = true;
	}

	private ComponentBuilder(T implementation) {
		super();
		this.implementations = Collections.singletonList(implementation);
		this.interfaceTypes = new HashSet<>();
		this.objectNamePattern = implementation.getObjectName().getCanonicalName();
		this.singleton = true;
	}

	/**
	 * Record an implemented interface for the beans of this builder.
	 * @param interfaceType the interface type implemented by all beans
	 * @return this builder
	 */
	public ComponentBuilder<T> addInterface(Class<? super T> interfaceType) {
		interfaceTypes.add(interfaceType);
		return this;
	}

	/**
	 * Optionally record an implemented interface for the beans of this builder.
	 *
	 * @param interfaceType the interface type implemented by all beans (if required is true)
	 * @param required indicates whether the beans of this builder should implement the
	 * given interface type
	 * @return this builder
	 */
	public ComponentBuilder<T> addInterfaceIf(Class<?> interfaceType, boolean required) {
		if (required) {
			for (Object implementation : implementations) {
				if (!interfaceType.isInstance(implementation)) {
					throw new IllegalArgumentException();
				}
			}

			interfaceTypes.add(interfaceType);
		}

		return this;
	}

	/**
	 * Add to the given collection a component representing the beans of this builder.
	 *
	 * @param components the collection to receive the component
	 */
	public void register(Collection<? super PlatformComponent<Object>> components) {
		Component component;

		if (singleton) {
			PlatformManagedObject implementation = implementations.get(0);

			if (implementation == null) {
				component = new Missing(objectNamePattern, interfaceTypes);
			} else {
				component = new Singleton(implementation, interfaceTypes);
			}
		} else {
			component = new Group(objectNamePattern, implementations, interfaceTypes);
		}

		components.add(component);
	}

}
