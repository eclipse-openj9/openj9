<!--
Copyright (c) 2018, 2019 IBM Corp. and others

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

JITServer adds an additional two *personas* to the `java` executable: client mode and server mode. There are two new command line options that can be used to select a persona: `-XX:StartAsJITServer` and `-XX:+UseJITServer`.

In server mode, the JVM will halt after startup and begin listening for compilation requests from clients. No Java application is given to the server on the command line.

```
$ java -XX:StartAsJITServer

JITServer ready to accept incoming requests
```

Clients can then be started. For this example, we assume the client and server are running on the same machine. The server needs to continue running, so you will need to start clients in a new terminal.

```
$ java -XX:+UseJITServer -version
openjdk version "1.8.0_192-internal"
OpenJDK Runtime Environment (build 1.8.0_192-internal-ymanton_2018_09_19_11_13-b00)
Eclipse OpenJ9 VM (build jitaas-f409446, JRE 1.8.0 Linux amd64-64-Bit Compressed References 20180920_000000 (JIT enabled, AOT enabled)
OpenJ9   - f409446
OMR      - feaf86d
JCL      - 84cb836 based on jdk8u192-b03)
```
Note that `java -version` is itself a small Java application! If you have some application you'd like to test, you can substitute it in for `-version`. Run it as usual, but with the flag `-XX:+UseJITServer` before the application name.

You might have noticed that running the client without any server to connect to still appears to work. This is because the client will fall back to running the interpreter (slow path) if it cannot connect to a server. To ensure that everything is really working as intended, it is a good idea to enable some logging. It's often most convenient on the server side, because log messages will not interfere with application output, but logging can be added to either the server or the client.

With verbose logging, if a client connects successfully then server output should look something like this:
```
$ java -XX:StartAsJITServer -Xjit:verbose=\{JITServer\}

#JITServer: JITServer in Server Mode. Port: 38400
#JITServer: Started JITServer listener thread: 0000000001B8DE00
JITServer ready to accept incoming requests

#JITServer: Server allocated data for a new clientUID 7273413081043723869
#JITServer: Server received request to compile sun/reflect/Reflection.getCallerClass @ cold
#JITServer: Server queued compilation for sun/reflect/Reflection.getCallerClass
#JITServer: Server cached clientSessionData=00007FFFDA1973E0 for clientUID=7273413081043723869 compThreadID=0
#JITServer: Server has successfully compiled sun/reflect/Reflection.getCallerClass()Ljava/lang/Class;
#JITServer: Server received request to compile java/lang/Double.longBitsToDouble @ cold
#JITServer: Server queued compilation for java/lang/Double.longBitsToDouble
#JITServer: Server cached clientSessionData=00007FFFDA1973E0 for clientUID=7273413081043723869 compThreadID=1
#JITServer: Server has successfully compiled java/lang/Double.longBitsToDouble(J)D
#JITServer: Server received request to compile java/lang/System.getEncoding @ cold
...
```

### Configuration

#### Hostname
On the client, you can specify the server hostname or IP.
```
$ java -XX:+UseJITServer:server=example.com
```

#### Port
By default, communication occurs on port 38400. You can change this by specifying the `port` suboption as follows:
```
$ java -XX:StartAsJITServer -XX:JITServerPort=1234
$ java -XX:+UseJITServer -XX:JITServerPort=1234 MyApplication
```

#### Timeout
If your network connection is flaky, you may want to specify a timeout. By default there is no timeout. Timeout is given in milliseconds. Client and server timeouts do not need to match, but there's usually no reason for them to differ.
```
$ java -XX:StartAsJITServer -XX:JITServerTimeout=5000
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
$ java -XX:StartAsJITServer -XX:JITServerSSLKey=key.pem -XX:JITServerSSLCert=cert.pem
```
The client currently accepts a single option `sslRootCerts`, which is the filename of a PEM-encoded certificate chain.
```
$ java -XX:+UseJITServer -XX:JITServerSSLRootCerts=cert.pem -version
```
