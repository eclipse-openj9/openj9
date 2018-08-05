/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;
import com.ibm.lang.management.ExtendedThreadInfo;
import com.ibm.lang.management.internal.ExtendedThreadMXBeanImpl;
import com.ibm.tools.attach.target.AttachmentConnection;
import com.ibm.tools.attach.target.Response;
import com.ibm.tools.attach.target.diagnostics.data.ThreadInfoResponse;

/**
 * Command for VM to send a thread dump to an Attach API client
 */
public class GetThreadDumpCommand implements DiagnosticCommand {

	@Override
	public void execute(InputStream cmdStream, OutputStream respStream) throws Exception {
		AttachmentConnection.streamSend(respStream, Response.ACK);
		boolean lockedMonitors = Boolean.parseBoolean(AttachmentConnection.streamReceiveString(cmdStream));
		AttachmentConnection.streamSend(respStream, Response.ACK);
		boolean lockedSynchronizers = Boolean.parseBoolean(AttachmentConnection.streamReceiveString(cmdStream));

		ByteArrayOutputStream bos = new ByteArrayOutputStream();
		ExtendedThreadInfo[] dump = ExtendedThreadMXBeanImpl.getInstance().dumpAllExtendedThreads(lockedMonitors, lockedSynchronizers);
		ThreadInfoResponse [] items = new ThreadInfoResponse[dump.length];
		for(int i=0;i<dump.length;i++) {
			items[i] = ThreadInfoResponse.fromThreadInfo(dump[i]);
		}

		try(ObjectOutputStream oos = new ObjectOutputStream(bos)) {
			oos.writeObject(items);
		}
		
		byte data[] = bos.toByteArray();

		AttachmentConnection.streamSend(respStream, String.valueOf(data.length));
		AttachmentConnection.streamReceiveString(cmdStream); // receives ACK
		respStream.write(data);
		respStream.flush();
	}
}
