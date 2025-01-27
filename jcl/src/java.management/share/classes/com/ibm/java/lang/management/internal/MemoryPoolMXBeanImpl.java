/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2005
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.java.lang.management.internal;

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryManagerMXBean;
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.MemoryType;
import java.lang.management.MemoryUsage;
import java.lang.reflect.Constructor;
import java.util.LinkedList;
import java.util.List;

import javax.management.ObjectName;

/**
 * Runtime type for {@link java.lang.management.MemoryPoolMXBean}
 *
 * @since 1.5
 */
@SuppressWarnings("hiding")
public class MemoryPoolMXBeanImpl implements MemoryPoolMXBean {

	private static final Constructor<MemoryUsage> memUsageConstructor;

	static {
		Constructor<MemoryUsage> constructor;
		try {
			constructor = MemoryUsage.class.getConstructor(long.class, long.class, long.class, long.class);
		} catch (NoSuchMethodException | SecurityException e) {
			constructor = null;
		}
		memUsageConstructor = constructor;
	}

	private final String name;

	private final int id;

	private final MemoryType type;

	private final MemoryMXBeanImpl memBean;

	private ObjectName objectName;

	/**
	 * Sets the metadata for this bean.
	 *
	 * @param name
	 * @param type
	 * @param id
	 * @param memBean
	 */
	protected MemoryPoolMXBeanImpl(String name, MemoryType type, int id, MemoryMXBeanImpl memBean) {
		super();
		this.name = name;
		this.type = type;
		this.id = id;
		this.memBean = memBean;
	}

	/**
	 * @param constructor MemoryUsage Constructor
	 * @return a {@link java.lang.management.MemoryUsage} object that may be interrogated by the
	 *         caller to determine the details of the memory usage. Returns
	 *         <code>null</code> if the virtual machine does not support this
	 *         method.
	 * @see #getCollectionUsage()
	 */
	private native MemoryUsage getCollectionUsageImpl(int id, Class<MemoryUsage> clazz, Constructor<MemoryUsage> constructor);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public MemoryUsage getCollectionUsage() {
		if (null != memUsageConstructor) {
			return this.getCollectionUsageImpl(id, MemoryUsage.class, memUsageConstructor);
		} else {
			return null;
		}
	}

	/**
	 * @param constructor MemoryUsage Constructor
	 * @return a {@link java.lang.management.MemoryUsage} containing the usage details for the memory
	 *         pool just before the most recent collection occurred. Returns
	 *         <code>null</code> if the virtual machine does not support this method.
	 */
	private native MemoryUsage getPreCollectionUsageImpl(int id, Class<MemoryUsage> clazz, Constructor<MemoryUsage> constructor);

	/**
	 * To satisfy com.ibm.lang.management.MemoryPoolMXBean.
	 *
	 * @return a {@link java.lang.management.MemoryUsage} containing the usage details for the memory
	 *         pool just before the most recent collection occurred. Returns
	 *         <code>null</code> if the virtual machine does not support this method.
	 */
	public MemoryUsage getPreCollectionUsage() {
		return this.getPreCollectionUsageImpl(getID(), MemoryUsage.class, memUsageConstructor);
	}

	/**
	 * @return the collection usage threshold in bytes. The default value as set
	 *         by the virtual machine will be zero.
	 * @see #getCollectionUsageThreshold()
	 */
	private native long getCollectionUsageThresholdImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getCollectionUsageThreshold() {
		if (!isCollectionUsageThresholdSupported()) {
			/*[MSG "K05EE", "VM does not support collection usage threshold."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05EE")); //$NON-NLS-1$
		}
		return this.getCollectionUsageThresholdImpl(id);
	}

	/**
	 * @return a count of the number of times that the collection usage
	 *         threshold has been surpassed.
	 * @see #getCollectionUsageThresholdCount()
	 */
	private native long getCollectionUsageThresholdCountImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getCollectionUsageThresholdCount() {
		if (!isCollectionUsageThresholdSupported()) {
			/*[MSG "K05EE", "VM does not support collection usage threshold."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05EE")); //$NON-NLS-1$
		}
		return this.getCollectionUsageThresholdCountImpl(id);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String[] getMemoryManagerNames() {
		/* get the memory managers and check which of them manage this pool */
		List<String> result = new LinkedList<>();
		for (MemoryManagerMXBean bean : memBean.getMemoryManagerMXBeans(false)) {
			for (String managedPool : bean.getMemoryPoolNames()) {
				if (managedPool.equals(name)) {
					result.add(bean.getName());
					break;
				}
			}
		}
		return result.toArray(new String[result.size()]);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public String getName() {
		return this.name;
	}

	/**
	 * @return the identifier of the associated memory pool
	 */
	int getID() {
		return this.id;
	}

	/**
	 * @param constructor MemoryUsage constructor
	 * @return a {@link java.lang.management.MemoryUsage} which can be interrogated by the caller to
	 *         determine details of the peak memory usage. A <code>null</code>
	 *         value will be returned if the memory pool no longer exists (and
	 *         the pool is therefore considered to be invalid).
	 * @see #getPeakUsage()
	 */
	private native MemoryUsage getPeakUsageImpl(int id, Class<MemoryUsage> clazz, Constructor<MemoryUsage> constructor);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public MemoryUsage getPeakUsage() {
		return this.getPeakUsageImpl(id, MemoryUsage.class, memUsageConstructor);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public MemoryType getType() {
		return this.type;
	}

	/**
	 * @param constructor MemoryUsage Constructor
	 * @return an instance of {@link java.lang.management.MemoryUsage} that can be interrogated by
	 *         the caller to determine details on the pool's current memory
	 *         usage. A <code>null</code> value will be returned if the memory
	 *         pool no longer exists (in which case it is considered to be
	 *         invalid).
	 * @see #getUsage()
	 */
	private native MemoryUsage getUsageImpl(int id, Class<MemoryUsage> clazz, Constructor<MemoryUsage> constructor);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public MemoryUsage getUsage() {
		return this.getUsageImpl(id, MemoryUsage.class, memUsageConstructor);
	}

	/**
	 * @return the usage threshold in bytes. The default value as set by the
	 *         virtual machine depends on the platform the virtual machine is
	 *         running on. will be zero.
	 * @see #getUsageThreshold()
	 */
	private native long getUsageThresholdImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getUsageThreshold() {
		if (!isUsageThresholdSupported()) {
			/*[MSG "K05EF", "VM does not support usage threshold."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05EF")); //$NON-NLS-1$
		}
		return this.getUsageThresholdImpl(id);
	}

	/**
	 * @return a count of the number of times that the usage threshold has been
	 *         surpassed.
	 * @see #getUsageThresholdCount()
	 */
	private native long getUsageThresholdCountImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public long getUsageThresholdCount() {
		if (!isUsageThresholdSupported()) {
			/*[MSG "K05EF", "VM does not support usage threshold."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05EF")); //$NON-NLS-1$
		}
		return this.getUsageThresholdCountImpl(id);
	}

	/**
	 * @return <code>true</code> if the collection usage threshold was
	 *         surpassed after the latest garbage collection run, otherwise
	 *         <code>false</code>.
	 * @see #isCollectionUsageThresholdExceeded()
	 */
	private native boolean isCollectionUsageThresholdExceededImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isCollectionUsageThresholdExceeded() {
		if (!isCollectionUsageThresholdSupported()) {
			/*[MSG "K05EE", "VM does not support collection usage threshold."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05EE")); //$NON-NLS-1$
		}
		return this.isCollectionUsageThresholdExceededImpl(id);
	}

	/**
	 * @return <code>true</code> if supported, <code>false</code> otherwise.
	 * @see #isCollectionUsageThresholdSupported()
	 */
	private native boolean isCollectionUsageThresholdSupportedImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isCollectionUsageThresholdSupported() {
		return this.isCollectionUsageThresholdSupportedImpl(id);
	}

	/**
	 * @return <code>true</code> if the usage threshold has been surpassed,
	 *         otherwise <code>false</code>.
	 * @see #isUsageThresholdExceeded()
	 */
	private native boolean isUsageThresholdExceededImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isUsageThresholdExceeded() {
		if (!isUsageThresholdSupported()) {
			/*[MSG "K05EF", "VM does not support usage threshold."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05EF")); //$NON-NLS-1$
		}
		return this.isUsageThresholdExceededImpl(id);
	}

	/**
	 * @return <code>true</code> if supported, <code>false</code> otherwise.
	 * @see #isUsageThresholdSupported()
	 */
	private native boolean isUsageThresholdSupportedImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isUsageThresholdSupported() {
		return this.isUsageThresholdSupportedImpl(id);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean isValid() {
		return true;
	}

	/**
	 * @see #resetPeakUsage()
	 */
	private native void resetPeakUsageImpl(int id);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void resetPeakUsage() {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		@SuppressWarnings("removal")
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		this.resetPeakUsageImpl(id);
	}

	/**
	 * @param threshold
	 *            the size of the new collection usage threshold expressed in
	 *            bytes.
	 * @see #setCollectionUsageThreshold(long)
	 */
	private native void setCollectionUsageThresholdImpl(int id, long threshold);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setCollectionUsageThreshold(long threshold) {
		if (!isCollectionUsageThresholdSupported()) {
			/*[MSG "K05EE", "VM does not support collection usage threshold."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05EE")); //$NON-NLS-1$
		}

		/*[IF JAVA_SPEC_VERSION < 24]*/
		@SuppressWarnings("removal")
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		if (threshold < 0) {
			/*[MSG "K05FE", "Collection usage threshold cannot be negative."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05FE")); //$NON-NLS-1$
		}

		if (exceedsMaxPoolSize(threshold)) {
			/*[MSG "K05FF", "Collection usage threshold cannot exceed maximum amount of memory for pool."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05FF")); //$NON-NLS-1$
		}
		this.setCollectionUsageThresholdImpl(id, threshold);
	}

	/**
	 * @param threshold
	 *            the size of the new usage threshold expressed in bytes.
	 * @see #setUsageThreshold(long)
	 */
	private native void setUsageThresholdImpl(int id, long threshold);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setUsageThreshold(long threshold) {
		if (!isUsageThresholdSupported()) {
			/*[MSG "K05EF", "VM does not support usage threshold."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K05EF")); //$NON-NLS-1$
		}

		/*[IF JAVA_SPEC_VERSION < 24]*/
		@SuppressWarnings("removal")
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		if (threshold < 0) {
			/*[MSG "K05F0", "Usage threshold cannot be negative."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F0")); //$NON-NLS-1$
		}

		if (exceedsMaxPoolSize(threshold)) {
			/*[MSG "K05F1", "Usage threshold cannot exceed maximum amount of memory for pool."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K05F1")); //$NON-NLS-1$
		}
		this.setUsageThresholdImpl(id, threshold);
	}

	 /**
	 * @param value
	 * @return <code>true</code> if <code>value</code> is greater than the
	 *         maximum size of the corresponding memory pool. <code>false</code>
	 *         if <code>value</code> does not exceed the maximum memory pool
	 *         size or else no memory pool maximum size has been defined.
	 */
	private boolean exceedsMaxPoolSize(long value) {
		MemoryUsage m = getUsage();
		return ((-1 != m.getMax()) && (0 != m.getMax()) && (m.getMax() < value));
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ObjectName getObjectName() {
		if (objectName == null) {
			objectName = ManagementUtils.createObjectName(ManagementFactory.MEMORY_POOL_MXBEAN_DOMAIN_TYPE, name);
		}
		return objectName;
	}

}
