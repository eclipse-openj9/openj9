/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;

import com.ibm.java.lang.management.internal.CompilationMXBeanImpl;
import com.ibm.tools.attach.target.AttachmentConnection;
import com.ibm.tools.attach.target.diagnostics.data.CompilationInfoResponse;

/**
 * Get information about JIT compilation 
 */
public class GetCompilerInfoCommand implements DiagnosticCommand {

	@Override
	public void execute(InputStream cmdStream, OutputStream respStream) throws Exception {
		CompilationMXBeanImpl bean = CompilationMXBeanImpl.getInstance();
		CompilationInfoResponse item = new CompilationInfoResponse(bean.getTotalCompilationTime());
		
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
