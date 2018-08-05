/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics.data;

import java.io.Serializable;
import java.lang.management.MemoryUsage;
import java.util.List;

/**
 * Class with information about memory utilization and about GC activity
 */
public class GCInfoResponse implements Serializable {
	private static final long serialVersionUID = 1L; 
	
	private MemoryUsageResponse heapMemoryUsage;
	private MemoryUsageResponse nonHeapMemoryusage;
	
	private List<MemoryPoolResponse> pools;
	private List<MemoryManagerResponse> managers;
	
	/**
	 * Immutable class with information about particular memory region
	 */
	public static class MemoryUsageResponse implements Serializable {
		private static final long serialVersionUID = 1L;

		private final long init;
		private final long used;
		private final long committed;
		private final long max;

		/**
		 * Constructor 
		 * @param init initial size of region
		 * @param used current usage of region
		 * @param committed committed amount of region
		 * @param max maximum region size
		 */
		public MemoryUsageResponse(long init, long used, long committed, long max) {
			super();
			this.init = init;
			this.used = used;
			this.committed = committed;
			this.max = max;
		}
		
		/**
		 * Factory method for building instance right from {@link MemoryUsage}
		 * @param usage {@link MemoryUsage}
		 * @return new instance of {@link MemoryUsageResponse}
		 */
		public static MemoryUsageResponse fromMemoryUsage(MemoryUsage usage) {
			if(usage==null) {
				return null;
			}
			return new MemoryUsageResponse(usage.getInit(), usage.getUsed(), usage.getCommitted(), usage.getMax());
		}

		/**
		 * @return initial uage
		 */
		public long getInit() {
			return init;
		}

		/**
		 * @return current usage
		 */
		public long getUsed() {
			return used;
		}

		/**
		 * @return amount of committed memory
		 */
		public long getCommitted() {
			return committed;
		}

		/**
		 * @return max usage
		 */
		public long getMax() {
			return max;
		}
	}
	
	/**
	 * Immutable class with information about particular memory pool
	 */
	public static class MemoryPoolResponse implements Serializable {
		private static final long serialVersionUID = 1L;

		private final String name;
		private final MemoryUsageResponse lastCollectionUsage;
		private final MemoryUsageResponse usage;
		private final MemoryUsageResponse peakUsage;

		/**
		 * Constructor 
		 * @param name pool's name
		 * @param lastCollectionUsage pool usage after last GC
		 * @param usage current pool usage
		 * @param peakUsage pool's peak usage
		 */
		public MemoryPoolResponse(String name, MemoryUsageResponse lastCollectionUsage,
				MemoryUsageResponse usage, MemoryUsageResponse peakUsage) {
			super();
			this.name = name;
			this.lastCollectionUsage = lastCollectionUsage;
			this.usage = usage;
			this.peakUsage = peakUsage;
		}

		/**
		 * @return name of memory pool
		 */
		public String getName() {
			return name;
		}

		/**
		 * @return get pool usage after last GC
		 */
		public MemoryUsageResponse getLastCollectionUsage() {
			return lastCollectionUsage;
		}

		/**
		 * @return pool usage information
		 */
		public MemoryUsageResponse getUsage() {
			return usage;
		}

		/**
		 * @return peak usage of this pool
		 */
		public MemoryUsageResponse getPeakUsage() {
			return peakUsage;
		}
	}

	/**
	 * Information from MemoryManagerMXBean
	 *
	 */
	public static class MemoryManagerResponse implements Serializable {
		private static final long serialVersionUID = 1L;
		private final String name;
		private final long collectionCount;
		private final long collectionTime;
		private final long lastCollectionStartTime;
		private final long lastCollectionEndTime;

		/**
		 * @param name manager's name
		 * @param collectionCount count of GC occurred
		 * @param collectionTime total time spent in GC
		 * @param lastCollectionStartTime last GC start time
		 * @param lastCollectionEndTime last GC end time
		 */
		public MemoryManagerResponse(String name, long collectionCount, long collectionTime,
				long lastCollectionStartTime, long lastCollectionEndTime) {
			super();
			this.name = name;
			this.collectionCount = collectionCount;
			this.collectionTime = collectionTime;
			this.lastCollectionStartTime = lastCollectionStartTime;
			this.lastCollectionEndTime = lastCollectionEndTime;
		}

		/**
		 * @return manager's name
		 */
		public String getName() {
			return name;
		}

		/**
		 * @return count of GC occurred
		 */
		public long getCollectionCount() {
			return collectionCount;
		}

		/**
		 * @return total time spent in GC
		 */
		public long getCollectionTime() {
			return collectionTime;
		}

		/**
		 * @return last GC start time
		 */
		public long getLastCollectionStartTime() {
			return lastCollectionStartTime;
		}

		/**
		 * @return last GC end time
		 */
		public long getLastCollectionEndTime() {
			return lastCollectionEndTime;
		}
	}

	/**
	 * @return heap usage information
	 */
	public MemoryUsageResponse getHeapMemoryUsage() {
		return heapMemoryUsage;
	}

	/**
	 * set heap usage
	 * @param heapMemoryUsage heap usage
	 */
	public void setHeapMemoryUsage(MemoryUsageResponse heapMemoryUsage) {
		this.heapMemoryUsage = heapMemoryUsage;
	}

	/**
	 * @return non-heap memory usage
	 */
	public MemoryUsageResponse getNonHeapMemoryusage() {
		return nonHeapMemoryusage;
	}

	/**
	 * set non-heap memory usage
	 * @param nonHeapMemoryusage non-heap memory usage
	 */
	public void setNonHeapMemoryusage(MemoryUsageResponse nonHeapMemoryusage) {
		this.nonHeapMemoryusage = nonHeapMemoryusage;
	}

	/**
	 * @return list of memory pools
	 */
	public List<MemoryPoolResponse> getPools() {
		return pools;
	}

	/**
	 * set list of memory pools
	 * @param pools list of memory pools
	 */
	public void setPools(List<MemoryPoolResponse> pools) {
		this.pools = pools;
	}

	/**
	 * @return list of memory managers
	 */
	public List<MemoryManagerResponse> getManagers() {
		return managers;
	}

	/**
	 * save list of memory managers
	 * @param managers list of memory managers
	 */
	public void setManagers(List<MemoryManagerResponse> managers) {
		this.managers = managers;
	}
}
