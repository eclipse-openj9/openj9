/*[INCLUDE-IF Sidecar17]*/
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
package com.ibm.java.lang.management.internal;

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.lang.management.MemoryManagerMXBean;
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.MemoryType;
import java.lang.management.MemoryUsage;
import java.lang.reflect.Constructor;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import javax.management.ListenerNotFoundException;
import javax.management.MBeanNotificationInfo;
import javax.management.Notification;
import javax.management.NotificationBroadcasterSupport;
import javax.management.NotificationEmitter;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;
/*[IF Sidecar19-SE]*/
import com.ibm.sharedclasses.spi.SharedClassProvider;
import java.util.ServiceLoader;
/*[ELSE]
import com.ibm.oti.shared.SharedClassStatistics;
/*[ENDIF]*/
/**
 * Runtime type for {@link MemoryMXBean}.
 * <p>
 * Implementation note. This type of bean is both dynamic and a notification
 * emitter. The dynamic behavior comes courtesy of the
 * {@link com.ibm.java.lang.management.internal.DynamicMXBeanImpl} superclass while the
 * notifying behavior uses a delegation approach to a private member that
 * implements the {@link javax.management.NotificationEmitter} interface.
 * Because multiple inheritance is not supported in Java it was a toss up which
 * behavior would be based on inheritance and which would use delegation. Every
 * other <code>*MXBeanImpl</code> class in this package inherits from the
 * abstract base class <code>DynamicMXBeanImpl</code> so that seemed to be the
 * natural approach for this class too. By choosing not to make this class a
 * subclass of {@link javax.management.NotificationBroadcasterSupport}, the
 * protected
 * <code>handleNotification(javax.management.NotificationListener, javax.management.Notification, java.lang.Object)</code>
 * method cannot be overridden for any custom notification behavior. However,
 * taking the agile mantra of <b>YAGNI </b> to heart, it was decided that the
 * default implementation of that method will suffice until new requirements
 * prove otherwise.
 * </p>
 *
 * @since 1.5
 */
public class MemoryMXBeanImpl implements MemoryMXBean, NotificationEmitter {

	/**
	 * The delegate for all notification management.
	 */
	private final NotificationBroadcasterSupport notifier = new NotificationBroadcasterSupport();

	private static final MemoryMXBeanImpl instance = new MemoryMXBeanImpl();

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

	private final List<MemoryManagerMXBean> memoryManagerList;

	private final List<MemoryPoolMXBean> managedPoolList;

	private final ObjectName objectName;
	
	/*[IF Sidecar19-SE]*/
	private final SharedClassProvider sharedClassServiceProvider;
	/*[ENDIF]*/

	/**
	 * Constructor intentionally private to prevent instantiation by others.
	 * Sets the metadata for this bean.
	 */
	protected MemoryMXBeanImpl() {
		super();
		objectName = ManagementUtils.createObjectName(ManagementFactory.MEMORY_MXBEAN_NAME);
		managedPoolList = new LinkedList<>();
		memoryManagerList = new LinkedList<>();

		createMemoryPools();
		createMemoryManagers();
		setManagedMemoryPoolsForManagers();
		/*[IF Sidecar19-SE]*/
		sharedClassServiceProvider = firstNonNull(ServiceLoader.load(SharedClassProvider.class));
		/*[ENDIF]*/
	}

	/*[IF Sidecar19-SE]*/
	private static <T> T firstNonNull(Iterable<T> iterable) {
		for (T member : iterable) {
			if (null != member) {
				return member;
			}
		}
		return null;
	}
	/*[ENDIF]*/

	private void setManagedMemoryPoolsForManagers() {
		Iterator<MemoryManagerMXBean> iterManager = memoryManagerList.iterator();
		
		while (iterManager.hasNext()) {
			MemoryManagerMXBeanImpl beanManager = (MemoryManagerMXBeanImpl) iterManager.next();

			Iterator<MemoryPoolMXBean> iterPool = managedPoolList.iterator();
			while (iterPool.hasNext()) {
				MemoryPoolMXBeanImpl beanPool = (MemoryPoolMXBeanImpl) iterPool.next();

				if (beanManager instanceof GarbageCollectorMXBean) {
					if ((MemoryType.HEAP == beanPool.getType()) && beanManager.isManagedPoolImpl(beanManager.id, beanPool.getID())) {
						beanManager.addMemoryPool(beanPool);
					}
				} else {
					if (MemoryType.NON_HEAP == beanPool.getType()) {
						beanManager.addMemoryPool(beanPool);
					}
				}
			}
		}
    }

    /**
     * Singleton accessor method.
     * 
     * @return the <code>ClassLoadingMXBeanImpl</code> singleton.
     */
	public static MemoryMXBeanImpl getInstance() {
        return instance;
    }

    /**
     * Instantiates MemoryManagerMXBean and GarbageCollectorMXBean instance(s)
     * for the current VM configuration and stores them in memoryManagerList.
     */
    private native void createMemoryManagers();

    private void createMemoryManagerHelper(String name, int internalID, boolean isGC) {
		String domain = isGC
				? ManagementFactory.GARBAGE_COLLECTOR_MXBEAN_DOMAIN_TYPE
				: ManagementFactory.MEMORY_MANAGER_MXBEAN_DOMAIN_TYPE;
		ObjectName objName = ManagementUtils.createObjectName(domain, name);

		if (isGC) {
			memoryManagerList.add(makeGCBean(objName, name, internalID));
		} else {
			memoryManagerList.add(new MemoryManagerMXBeanImpl(objName, name, internalID, this));
		}
	}

	/**
	 * Create a new GarbageCollectorMXBean.
	 *
	 * @param objName the ObjectName of the new bean
	 * @param name the collector name
	 * @param internalID an internal id number representing the collector
	 * @return the new GarbageCollectorMXBean
	 */
	protected GarbageCollectorMXBean makeGCBean(ObjectName objName, String name, int internalID) {
		return new GarbageCollectorMXBeanImpl(objName, name, internalID, this);
	}

    /**
     * Retrieves the list of memory manager beans in the system.
     * 
     * @param copy indicates whether a copy of the list should be returned
     * 
     * @return the list of <code>MemoryManagerMXBean</code> instances
     */
	public List<MemoryManagerMXBean> getMemoryManagerMXBeans(boolean copy) {
		List<MemoryManagerMXBean> list = memoryManagerList;

		return copy ? new ArrayList<>(list) : list;
	}

	/**
	 * Returns a list of all of the instances of {@link GarbageCollectorMXBean}
	 * in this virtual machine. Owing to the dynamic nature of this kind of
	 * <code>MXBean</code>, it is possible that instances may be created or
	 * destroyed between the invocation and return of this method.
	 *
	 * @return a list of all known <code>GarbageCollectorMXBean</code>s in
	 *         this virtual machine.
	 */
	public List<GarbageCollectorMXBean> getGarbageCollectorMXBeans() {
		List<GarbageCollectorMXBean> result = new LinkedList<>();
		for (MemoryManagerMXBean bean : memoryManagerList) {
			if (bean instanceof GarbageCollectorMXBean) {
				result.add((GarbageCollectorMXBean) bean);
			}
		}
		return result;
	}

    /**
     * Instantiate the MemoryPoolMXBeans representing the memory managed by
     * this manager, and store them in the managedPoolList.
     */
    private native void createMemoryPools();
    
    private void createMemoryPoolHelper(String name, int internalID, boolean isHeap) {
		managedPoolList.add(makeMemoryPoolBean(name, isHeap ? MemoryType.HEAP : MemoryType.NON_HEAP, internalID));
	}

	/**
	 * Create a new MemoryPoolMXBean.
	 * 
	 * @param name the name of the memory pool
	 * @param internalID an internal id number representing the memory pool
	 * @param type the type of the memory pool
	 * @return the new MemoryPoolMXBean
	 */
	protected MemoryPoolMXBean makeMemoryPoolBean(String name, MemoryType type, int internalID) {
		return new MemoryPoolMXBeanImpl(name, type, internalID, this);
	}

	/**
	 * Retrieves the list of memory pool beans managed by this manager.
	 *
	 * @param copy indicates whether a copy of the list should be returned
	 *
	 * @return the list of <code>MemoryPoolMXBean</code> instances
	 */
	public List<MemoryPoolMXBean> getMemoryPoolMXBeans(boolean copy) {
		List<MemoryPoolMXBean> list = managedPoolList;

		return copy ? new ArrayList<>(list) : list;
	}
    
	/**
	 * {@inheritDoc}
	 */
	@Override
	public void gc() {
		System.gc();
    }

    /**
     * @param constructor MemoryUsage Constructor
     * @return an instance of {@link MemoryUsage} which can be interrogated by
     *         the caller.
     * @see #getHeapMemoryUsage()
     */
    private native MemoryUsage getHeapMemoryUsageImpl(Class<MemoryUsage> clazz, Constructor<MemoryUsage> constructor);

	/**
	 * {@inheritDoc}
	 */
	@Override
	public MemoryUsage getHeapMemoryUsage() {
		if (null != memUsageConstructor) {
			return this.getHeapMemoryUsageImpl(MemoryUsage.class, memUsageConstructor);
		} else {
			return null;
		}
    }

    /**
     * @param constructor MemoryUsage Constructor
     * @return an instance of {@link MemoryUsage} which can be interrogated by
     *         the caller.
     * @see #getNonHeapMemoryUsage()
     */
    private native MemoryUsage getNonHeapMemoryUsageImpl(Class<MemoryUsage> clazz, Constructor<MemoryUsage> constructor);

	/**
	 * {@inheritDoc}
	 */
    @Override
    public MemoryUsage getNonHeapMemoryUsage() {
    	if (null != memUsageConstructor) {
    		return this.getNonHeapMemoryUsageImpl(MemoryUsage.class, memUsageConstructor);
    	} else {
    		return null;
    	}
    }

    /**
     * @return the number of objects awaiting finalization.
     * @see #getObjectPendingFinalizationCount()
     */
    private native int getObjectPendingFinalizationCountImpl();

	/**
	 * {@inheritDoc}
	 */
    @Override
    public int getObjectPendingFinalizationCount() {
        return this.getObjectPendingFinalizationCountImpl();
    }

    /**
     * @return <code>true</code> if verbose output is being produced ;
     *         <code>false</code> otherwise.
     * @see #isVerbose()
     */
    private native boolean isVerboseImpl();

	/**
	 * {@inheritDoc}
	 */
    @Override
    public boolean isVerbose() {
        return this.isVerboseImpl();
    }

    /**
     * @param value
     *            <code>true</code> enables verbose output ;
     *            <code>false</code> disables verbose output.
     * @see #setVerbose(boolean)
     */
    private native void setVerboseImpl(boolean value);

	/**
	 * {@inheritDoc}
	 */
    @Override
    public void setVerbose(boolean value) {
        SecurityManager security = System.getSecurityManager();
        if (security != null) {
            security.checkPermission(ManagementPermissionHelper.MPCONTROL);
        }
        this.setVerboseImpl(value);
    }

	/**
	 * @return value of -Xmx in bytes
	 */
	private native long getMaxHeapSizeLimitImpl();

	/**
	 * {@inheritDoc}
	 */
	public long getMaxHeapSizeLimit() {
		return this.getMaxHeapSizeLimitImpl();
	}

	/**
	 * @return current value of -Xsoftmx in bytes
	 */
	private native long getMaxHeapSizeImpl();

	/**
	 * {@inheritDoc}
	 */
	public long getMaxHeapSize() {
		return this.getMaxHeapSizeImpl();
	}

	/**
	 * @return value of -Xms in bytes
	 */
	private native long getMinHeapSizeImpl();

	/**
	 * {@inheritDoc}
	 */
	public long getMinHeapSize() {
		return this.getMinHeapSizeImpl();
	}

	/**
	 * @param size new value for -Xsoftmx
	 */
	private native void setMaxHeapSizeImpl(long size);

	/**
	 * {@inheritDoc}
	 */
	public void setMaxHeapSize(long size) {
		if (!this.isSetMaxHeapSizeSupported()) {
			throw new UnsupportedOperationException();
		}
		if (size < this.getMinHeapSize() || size > this.getMaxHeapSizeLimit()) {
			throw new IllegalArgumentException();
		}
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		this.setMaxHeapSizeImpl(size);
	}

	/**
	 * @return true if setMaxHeapSize is supported by this VM
	 */
	private native boolean isSetMaxHeapSizeSupportedImpl();

	/**
	 * {@inheritDoc}
	 */
	public boolean isSetMaxHeapSizeSupported() {
		return this.isSetMaxHeapSizeSupportedImpl();
	}
	
	/**
	 * {@inheritDoc}
	 */
    @Override
    public void removeNotificationListener(NotificationListener listener,
            NotificationFilter filter, Object handback)
            throws ListenerNotFoundException {
        notifier.removeNotificationListener(listener, filter, handback);
    }

	/**
	 * {@inheritDoc}
	 */
    @Override
    public void addNotificationListener(NotificationListener listener,
            NotificationFilter filter, Object handback)
            throws IllegalArgumentException {
        notifier.addNotificationListener(listener, filter, handback);
    }

	/**
	 * {@inheritDoc}
	 */
    @Override
    public void removeNotificationListener(NotificationListener listener)
            throws ListenerNotFoundException {
        notifier.removeNotificationListener(listener);
    }

	/**
	 * {@inheritDoc}
	 */
    @Override
    public MBeanNotificationInfo[] getNotificationInfo() {
        // We know what kinds of notifications we can emit whereas the
        // notifier delegate does not. So, for this method, no delegating.
        // Instead respond using our own metadata.
        MBeanNotificationInfo[] notifications = new MBeanNotificationInfo[1];
        String[] notifTypes = new String[2];
        notifTypes[0] = java.lang.management.MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED;
        notifTypes[1] = java.lang.management.MemoryNotificationInfo.MEMORY_COLLECTION_THRESHOLD_EXCEEDED;
        notifications[0] = new MBeanNotificationInfo(notifTypes,
                javax.management.Notification.class.getName(),
                "Memory Notification"); //$NON-NLS-1$
        return notifications;
    }

    /*
	 * Send notifications to registered listeners. This will be called when
	 * either of the following situations occur: <ol><li> With the method
	 * {@link java.lang.management.MemoryPoolMXBean#isUsageThresholdSupported()}
	 * returning <code> true </code> , a memory pool increases its size and, in
	 * doing so, reaches or exceeds the usage threshold value. In this case the
	 * notification type will be
	 * {@link MemoryNotificationInfo#MEMORY_THRESHOLD_EXCEEDED}. <li> With the
	 * method
	 * {@link java.lang.management.MemoryPoolMXBean#isCollectionUsageThresholdSupported()}
	 * returning <code> true </code> , a garbage-collected memory pool has
	 * reached or surpassed the collection usage threshold value after a system
	 * garbage collection has taken place. In this case the notification type
	 * will be
	 * {@link MemoryNotificationInfo#MEMORY_COLLECTION_THRESHOLD_EXCEEDED}.
	 * </ol>
	 * 
	 * @param notification For this type of bean the user data will consist of a
	 * {@link CompositeData} instance that represents a
	 * {@link MemoryNotificationInfo} object.
	 */
	public final void sendNotification(Notification notification) {
		notifier.sendNotification(notification);
	}

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheSize() {
		/*[IF Sidecar19-SE]*/
		if (null != sharedClassServiceProvider) {
			return sharedClassServiceProvider.getCacheSize();
		} else {
			return 0;
		}
		/*[ELSE]
		return SharedClassStatistics.maxSizeBytes();
		/*[ENDIF]*/
	}

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheSoftmxBytes() {
		/*[IF Sidecar19-SE]*/
		if (null != sharedClassServiceProvider) {
			return sharedClassServiceProvider.getSoftmxBytes();
		} else {
			return -1;
		}
		/*[ELSE]
		return SharedClassStatistics.softmxBytes();
		/*[ENDIF]*/
	}

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheMinAotBytes() {
		/*[IF Sidecar19-SE]*/
		if (null != sharedClassServiceProvider) {
			return sharedClassServiceProvider.getMinAotBytes();
		} else {
			return -1;
		}
		/*[ELSE]
		return SharedClassStatistics.minAotBytes();
		/*[ENDIF]*/
	}

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheMaxAotBytes() {
		/*[IF Sidecar19-SE]*/
		if (null != sharedClassServiceProvider) {
			return sharedClassServiceProvider.getMaxAotBytes();
		} else {
			return -1;
		}
		/*[ELSE]
		return SharedClassStatistics.maxAotBytes();
		/*[ENDIF]*/
	}

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheMinJitDataBytes() {
		/*[IF Sidecar19-SE]*/
		if (null != sharedClassServiceProvider) {
			return sharedClassServiceProvider.getMinJitDataBytes();
		} else {
			return -1;
		}
		/*[ELSE]
		return SharedClassStatistics.minJitDataBytes();
		/*[ENDIF]*/
	}

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheMaxJitDataBytes() {
		/*[IF Sidecar19-SE]*/
		if (null != sharedClassServiceProvider) {
			return sharedClassServiceProvider.getMaxJitDataBytes();
		} else {
			return -1;
		}
		/*[ELSE]
		return SharedClassStatistics.maxJitDataBytes();
		/*[ENDIF]*/
	}

	/**
	 * @param value new value for shared cache soft max
	 */
	private native boolean setSharedClassCacheSoftmxBytesImpl(long value);

	/**
	 * @param value new value for -Xscminaot
	 */
	private native boolean setSharedClassCacheMinAotBytesImpl(long value);

	/**
	 * @param value new value for -Xscmaxaot
	 */
	private native boolean setSharedClassCacheMaxAotBytesImpl(long value);

	/**
	 * @param value new value for -Xscminjitdata
	 */
	private native boolean setSharedClassCacheMinJitDataBytesImpl(long value);

	/**
	 * @param value new value for -Xscmaxjitdata
	 */
	private native boolean setSharedClassCacheMaxJitDataBytesImpl(long value);

	/**
	 * {@inheritDoc}
	 */
	public boolean setSharedClassCacheSoftmxBytes(long value) {
		if (value < 0) {
			throw new IllegalArgumentException();
		}
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		return this.setSharedClassCacheSoftmxBytesImpl(value);
	}

	/**
	 * {@inheritDoc}
	 */
	public boolean setSharedClassCacheMinAotBytes(long value) {
		if (value < 0) {
			throw new IllegalArgumentException();
		}
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		return this.setSharedClassCacheMinAotBytesImpl(value);
	}

	/**
	 * {@inheritDoc}
	 */
	public boolean setSharedClassCacheMaxAotBytes(long value) {
		if (value < 0) {
			throw new IllegalArgumentException();
		}
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		return this.setSharedClassCacheMaxAotBytesImpl(value);
	}

	/**
	 * {@inheritDoc}
	 */
	public boolean setSharedClassCacheMinJitDataBytes(long value) {
		if (value < 0) {
			throw new IllegalArgumentException();
		}
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		return this.setSharedClassCacheMinJitDataBytesImpl(value);
	}

	/**
	 * {@inheritDoc}
	 */
	public boolean setSharedClassCacheMaxJitDataBytes(long value) {
		if (value < 0) {
			throw new IllegalArgumentException();
		}
		SecurityManager security = System.getSecurityManager();
		if (security != null) {
			security.checkPermission(ManagementPermissionHelper.MPCONTROL);
		}
		return this.setSharedClassCacheMaxJitDataBytesImpl(value);
	}

	/**
	 * Returns the bytes that is not stored in into the shared classes cache due to the current setting of softmx in shared classes.
	 */
	private native long getSharedClassCacheSoftmxUnstoredBytesImpl();

	/**
	 * Returns the bytes that is not stored in into the shared classes cache due to the current setting of maximum shared classes cache space allowed for AOT data.
	 */
	private native long getSharedClassCacheMaxAotUnstoredBytesImpl();

	/**
	 * Returns the bytes that is not stored in into the shared classes cache due to the current setting of maximum shared classes cache space allowed for JIT data.
	 */
	private native long getSharedClassCacheMaxJitDataUnstoredBytesImpl();

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheSoftmxUnstoredBytes() {
		return getSharedClassCacheSoftmxUnstoredBytesImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheMaxAotUnstoredBytes() {
		return getSharedClassCacheMaxAotUnstoredBytesImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheMaxJitDataUnstoredBytes() {
		return getSharedClassCacheMaxJitDataUnstoredBytesImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	public long getSharedClassCacheFreeSpace() {
		/*[IF Sidecar19-SE]*/
		if (null != sharedClassServiceProvider) {
			return sharedClassServiceProvider.getFreeSpace();
		} else {
			return 0;
		}
		/*[ELSE]
		return SharedClassStatistics.freeSpaceBytes();
		/*[ENDIF]*/
	}
	
	/**
	 * {@inheritDoc}
	 */
	public String getGCMode() {
		return getGCModeImpl();
	}

	private native String getGCModeImpl();

	/**
     * Returns the amount of CPU time spent in the GC by the master thread, in milliseconds.
     * 
     * @return CPU time used in milliseconds
     * @see #getGCMasterThreadCpuUsed()
     */
	private native long getGCMasterThreadCpuUsedImpl();

	/**
	 * {@inheritDoc}
	 */
	public long getGCMasterThreadCpuUsed() {
		return getGCMasterThreadCpuUsedImpl();
	}

	/**
     * Returns the amount of CPU time spent in the GC by all slave threads, in milliseconds.
     * 
     * @return CPU time used in milliseconds
     * @see #getGCSlaveThreadsCpuUsed()
     */
	private native long getGCSlaveThreadsCpuUsedImpl();

	/**
	 * {@inheritDoc}
	 */
	public long getGCSlaveThreadsCpuUsed() {
		return getGCSlaveThreadsCpuUsedImpl();
	}

	/**
	 * Returns the maximum number of GC worker threads.
	 * 
	 * @return maximum number of GC worker threads
	 * @see #getMaximumGCThreads()
	 */
	private native int getMaximumGCThreadsImpl();

	/**
	 * {@inheritDoc}
	 */
	public int getMaximumGCThreads() {
		return getMaximumGCThreadsImpl();
	}

	/**
	 * Returns the number of GC slave threads that participated in the most recent collection.
	 * 
	 * @return number of active GC worker threads
	 * @see #getCurrentGCThreads()
	 */
	private native int getCurrentGCThreadsImpl();

	/**
	 * {@inheritDoc}
	 */
	public int getCurrentGCThreads() {
		return getCurrentGCThreadsImpl();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public ObjectName getObjectName() {
		return objectName;
	}

}
