/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;
import java.lang.management.MemoryManagerMXBean;
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.MemoryUsage;
import java.util.ArrayList;
import java.util.List;

import com.ibm.java.lang.management.internal.GarbageCollectorMXBeanImpl;
import com.ibm.java.lang.management.internal.MemoryMXBeanImpl;
import com.ibm.tools.attach.target.AttachmentConnection;
import com.ibm.tools.attach.target.diagnostics.data.GCInfoResponse;
import com.ibm.tools.attach.target.diagnostics.data.GCInfoResponse.MemoryManagerResponse;
import com.ibm.tools.attach.target.diagnostics.data.GCInfoResponse.MemoryPoolResponse;
import com.ibm.tools.attach.target.diagnostics.data.GCInfoResponse.MemoryUsageResponse;

/**
 * Get information about garbage collection
 * @author doki
 *
 */
public class GetGCInfoCommand implements DiagnosticCommand {

	@Override
	public void execute(InputStream cmdStream, OutputStream respStream) throws Exception {
		MemoryMXBeanImpl bean = MemoryMXBeanImpl.getInstance();
		GCInfoResponse item = new GCInfoResponse();
		MemoryUsage heap = bean.getHeapMemoryUsage();
		MemoryUsage nonHeap = bean.getNonHeapMemoryUsage(); 
		
		MemoryUsageResponse heapUsage = MemoryUsageResponse.fromMemoryUsage(heap);
		MemoryUsageResponse nonHeapUsage = MemoryUsageResponse.fromMemoryUsage(nonHeap);
		
		item.setHeapMemoryUsage(heapUsage);
		item.setNonHeapMemoryusage(nonHeapUsage);
		
		List<MemoryPoolResponse> pools = new ArrayList<>();
		for(MemoryPoolMXBean poolBean:bean.getMemoryPoolMXBeans(false)) {
			MemoryUsageResponse afterLastGc = MemoryUsageResponse.fromMemoryUsage(poolBean.getCollectionUsage());
			MemoryUsageResponse usage = MemoryUsageResponse.fromMemoryUsage(poolBean.getUsage());
			MemoryUsageResponse peakUsage = MemoryUsageResponse.fromMemoryUsage(poolBean.getPeakUsage());
			
			pools.add(new MemoryPoolResponse(poolBean.getName(), afterLastGc, usage, peakUsage));			
		}
		item.setPools(pools);
		
		List<MemoryManagerResponse> managers = new ArrayList<>();
		for(MemoryManagerMXBean managerBean : bean.getMemoryManagerMXBeans(false)) {
			String name = managerBean.getName();
			long collectionCount = 0;
			long collectionTime = 0;
			long lastCollectionStartTime = 0;
			long lastCollectionEndTime = 0;

			if(managerBean instanceof GarbageCollectorMXBeanImpl) {
				GarbageCollectorMXBeanImpl gcbean = (GarbageCollectorMXBeanImpl) managerBean;
				collectionCount = gcbean.getCollectionCount();
				collectionTime = gcbean.getCollectionTime();
				lastCollectionStartTime = gcbean.getLastCollectionStartTime();
				lastCollectionEndTime = gcbean.getLastCollectionEndTime();
			}
			
			managers.add(new MemoryManagerResponse(name, collectionCount, collectionTime, lastCollectionStartTime, lastCollectionEndTime));
		}
		item.setManagers(managers);
		
		ByteArrayOutputStream bos = new ByteArrayOutputStream();
		try(ObjectOutputStream oos = new ObjectOutputStream(bos)) {
			oos.writeObject(item);
		}

		byte data[] = bos.toByteArray();

		AttachmentConnection.streamSend(respStream, String.valueOf(data.length));
		AttachmentConnection.streamReceiveString(cmdStream); // receives ACK
		respStream.write(data);
		respStream.flush();
	}
}
