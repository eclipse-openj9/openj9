<!--
Copyright (c) 2018, 2020 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

JITServer adds two additional *personas* to the JVM: client mode and server mode. To start the JVM in server mode, run the `jitserver` launcher present under `bin` directory (along side `java` launcher). To start the JVM in client mode, there is a new command line option: `-XX:+UseJITServer` (under bin run `./java -XX:+UseJITServer <application name>`)

In server mode, the JVM will halt after startup and begin listening for compilation requests from clients. No Java application is given to the `jitserver` launcher on the command line. Any option accepted by JVM can also be passed to the `jitserver` launcher.

```
$ jitserver
JITServer is currently a technology preview. Its use is not yet supported

JITServer is ready to accept incoming requests

```

Clients can then be started. For this example, we assume the client and server are running on the same machine. The server needs to continue running, so you will need to start clients in a new terminal.

```
$ java -XX:+UseJITServer -version
JIT: using build "Nov  6 2019 20:02:35"
JIT level: cf3ba4120
openjdk version "1.8.0_232-internal"
OpenJDK Runtime Environment (build 1.8.0_232-internal-root_2019_11_06_19_57-b00)
Eclipse OpenJ9 VM (build master-cf3ba4120, JRE 1.8.0 Linux amd64-64-Bit Compressed References 20191106_000000 (JIT enabled, AOT enabled)
OpenJ9   - cf3ba4120
OMR      - cf9d75a4a
JCL      - 03cb3a3cb4 based on jdk8u232-b09)
```
Note that `java -version` is itself a small Java application! If you have some application you'd like to test, you can substitute it in for `-version`. Run it as usual, but with the flag `-XX:+UseJITServer` before the application name.

You might have noticed that running the client without any server to connect to still appears to work. This is because the client performs required JIT compilations locally if it cannot connect to a server. To ensure that everything is really working as intended, it is a good idea to enable some logging. It's often most convenient on the server side, because log messages will not interfere with application output, but logging can be added to either the server or the client.

With verbose logging, if a client connects successfully then server output should look something like this:
```
$ jitserver -Xjit:verbose=\{JITServer\}

JITServer is currently a technology preview. Its use is not yet supported

#JITServer: JITServer Server Mode. Port: 38400. Connection Timeout 30000ms
#JITServer: Started JITServer listener thread: 0000000000226C00
JITServer is ready to accept incoming requests

#JITServer: Server received request for stream 00007FEC658EF720
#JITServer: Server allocated data for a new clientUID 11129135271614904954
#JITServer: compThreadID=0 created clientSessionData=00007FEC658EFBE0 for clientUID=11129135271614904954 seqNo=0
#JITServer: Server will process a list of 0 unloaded classes for clientUID 11129135271614904954
#JITServer: compThreadID=0 has successfully compiled java/lang/Double.longBitsToDouble(J)D
#JITServer: compThreadID=0 found clientSessionData=00007FEC658EFBE0 for clientUID=11129135271614904954 seqNo=1
#JITServer: Server will process a list of 0 unloaded classes for clientUID 11129135271614904954
#JITServer: compThreadID=0 has successfully compiled jdk/internal/reflect/Reflection.getCallerClass()Ljava/lang/Class;
#JITServer: compThreadID=0 found clientSessionData=00007FEC658EFBE0 for clientUID=11129135271614904954 seqNo=2
#JITServer: Server will process a list of 0 unloaded classes for clientUID 11129135271614904954
#JITServer: compThreadID=0 has successfully compiled java/lang/System.getEncoding(I)Ljava/lang/String;
...
```

### Configuration

#### Hostname
On the client, you can specify the server hostname or IP.
```
$ java -XX:+UseJITServerAddress=example.com
```

#### Port
By default, communication occurs on port 38400. You can change this by specifying the `-XX:JITServerPort` suboption as follows:
```
$ jitserver -XX:JITServerPort=1234
$ java -XX:+UseJITServer -XX:JITServerPort=1234 MyApplication
```

#### Timeout
If your network connection is flaky, you may want to adjust the timeout. Timeout is given in milliseconds using `-XX:JITServerTimeout` suboption. Client and server timeouts do not need to match. By default there is timeout of 30000 ms at the server and 2000 ms at the client. Typically the timeout at the server can be larger; it can afford to wait because there is nothing else to do anyway. Waiting too much at the client can be detrimental because the client has the option of compiling locally and make progress.
```
$ jitserver -XX:JITServerTimeout=5000
$ java -XX:+UseJITServer -XX:JITServerTimeout=5000 MyApplication
```

#### Encryption (TLS)
By default, communication is not encrypted. If messages sent between the client and server need to traverse some untrusted network, you may want to set up encryption. Encryption reduces performance, so consider whether it is required for your use case.

Encryption support is currently experimental. **The implementation has not yet undergone a security audit.** Additionally, only certain modes of operation are supported. If you require encryption but run into problems with the current implementation, then please consider opening an issue with your requirements.

To generate some keys for testing, try:
```
$ openssl genrsa -out key.pem 2048
$ openssl req -new -x509 -sha256 -key key.pem -out cert.pem -days 3650
```
Make sure you specify localhost for the common name, but other fields don't matter (they can be left blank). **Keys generated in this manner should NOT be used in production!**

On the server, there are two options `sslKey` and `sslCert`, which are used to specify the filenames of PEM-encoded keypair.
```
$ jitserver -XX:JITServerSSLKey=key.pem -XX:JITServerSSLCert=cert.pem
```
The client currently accepts a single option `sslRootCerts`, which is the filename of a PEM-encoded certificate chain.
```
$ java -XX:+UseJITServer -XX:JITServerSSLRootCerts=cert.pem -version
```
