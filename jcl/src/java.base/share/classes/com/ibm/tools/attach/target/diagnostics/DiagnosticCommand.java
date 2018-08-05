/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics;

import java.io.InputStream;
import java.io.OutputStream;

/**
 * Attach API diagnostic command
 * OpenJ9 specific extension
 */
public interface DiagnosticCommand {
	
	/**
	 * Execute specific command
	 * @param cmdStream stream for additional argument
	 * @param respStream stream to send response
	 * @throws Exception in case of error some subtype of {@link Exception} could be thrown
	 */
	void execute(InputStream cmdStream, OutputStream respStream) throws Exception;
}
