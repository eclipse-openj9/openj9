/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics;

import java.io.InputStream;
import java.io.OutputStream;

import com.ibm.jvm.Dump;
import com.ibm.tools.attach.target.AttachmentConnection;
import com.ibm.tools.attach.target.Response;

/**
 * Diagnostic command to make a heap dump
 */
public class TriggerHeapDumpCommand implements DiagnosticCommand {

	@Override
	public void execute(InputStream cmdStream, OutputStream respStream) throws Exception {
		AttachmentConnection.streamSend(respStream, Response.ACK);
		String filename = AttachmentConnection.streamReceiveString(cmdStream);
		String result = Dump.heapDumpToFile(filename);
		AttachmentConnection.streamSend(respStream, result);
	}

}
