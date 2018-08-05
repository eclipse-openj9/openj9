/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;

import com.ibm.java.lang.management.internal.ClassLoadingMXBeanImpl;
import com.ibm.tools.attach.target.AttachmentConnection;
import com.ibm.tools.attach.target.diagnostics.data.ClassInfoResponse;

/**
 * Get information some statistics about loaded classes
 */
public class GetClassesInfoCommand implements DiagnosticCommand {

	@Override
	public void execute(InputStream cmdStream, OutputStream respStream) throws Exception {
		ClassLoadingMXBeanImpl bean = ClassLoadingMXBeanImpl.getInstance();
		ClassInfoResponse item = new ClassInfoResponse(bean.getLoadedClassCount(), bean.getUnloadedClassCount(), bean.getTotalLoadedClassCount());
		
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
