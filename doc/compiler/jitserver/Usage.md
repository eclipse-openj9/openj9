<!--
Copyright IBM Corp. and others 2018

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
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

# Getting started with JITServer

This document explains how to use JITServer technology with any Java application and describes some basic options, such as network configuration, logging and encryption.

1. [Running applications with JITServer](#running-applications-with-jitserver)
2. [Network Configuration](#network-configuration)
3. [JITServer-specific options](#jitserver-specific-options)
4. [Logging](#logging)

## Running applications with JITServer

JITServer adds two additional *personas* to the JVM: **client** mode and **server** mode.
In **client** mode the JVM will execute the given Java application as usual, but it will delegate JIT compilations to a **server** process.

In **server** mode, the JVM will halt after startup and begin listening for compilation requests from clients. To start the **server**, run the `jitserver` launcher present under `bin` directory (alongside `java` launcher). No Java application is given to the `jitserver` launcher on the command line. Any option accepted by JVM can also be passed to the `jitserver` launcher. One **server** is capable of serving multiple clients.

Begin by launching the **server**:

```
$ jitserver -XX:+JITServerLogConnections
JITServer is currently a technology preview. Its use is not yet supported

JITServer is ready to accept incoming requests

```

> **Note**: we passed an option `-XX:+JITServerLogConnections` to the server to enable logging of client connection events. Refer to [this](#connection-logging) section for more information.

To start the JVM in **client** mode, provide the -XX:+UseJITServer command line option to the java launcher. In the following example we assume the **client** and the **server** are run on the same machine. Since the **server** needs to continue running, we need to start the **client** in a new terminal.

Open a new terminal window and launch a simple program as **client**:

```
$ java -XX:+UseJITServer -version
openjdk version "11.0.12-internal" 2021-07-20
OpenJDK Runtime Environment (build 11.0.12-internal+0-adhoc.root.openj9-openjdk-jdk11)
Eclipse OpenJ9 VM (build master-1b998a73d, JRE 11 Linux amd64-64-Bit Compressed References 20210708_000000 (JIT enabled, AOT enabled)
OpenJ9   - 1b998a73d
OMR      - 5567b92f2
JCL      - 6a9be9a140 based on jdk-11.0.12+6)
```

> **Note**: `java -version` is itself a small Java application! If you have some application you'd like to test, you can substitute it in for `-version`. Run it as usual, but with the flag `-XX:+UseJITServer` before the application name.

If you go back to the **server**, you will see that it printed out that a client connected to it, which is how we can confirm that JITServer is working.

If at any time during the **client**'s lifetime the **server** goes down or becomes unaccessible,
the **client** will automatically switch to local compilations and will continue running like a regular JVM until the **server** becomes available again.

## Network Configuration

In the above example we assumed that both client and server are run
on the same machine. However, in real life scenarios they will likely be running
on different machines connected by a network. For example, they could be containers running on two different nodes in a cloud cluster.
JITServer provides command line options to configure network parameters, such as server address, ports, and encryption.

### Hostname/IP Address

On the client, you can specify server's hostname or IP address with
`-XX:JITServerAddress` option.

```
$ java -XX:JITServerAddress=example.com

-or-

$ java -XX:JITServerAddress=127.0.0.1
```

By default, the server address is set to `localhost`, i.e. server and client are on the same machine.

### Port

By default, communication occurs on port `38400`. You can change this by specifying the `-XX:JITServerPort` suboption as follows:

```
$ jitserver -XX:JITServerPort=1234 &
$ java -XX:+UseJITServer -XX:JITServerPort=1234 MyApplication
```

### Timeout

If your network connection is flaky, you may want to adjust the timeout. Timeout is given in milliseconds using `-XX:JITServerTimeout` suboption. Client and server timeouts do not need to match. By default there is timeout of 30000 ms at the server and 10000 ms at the client. Typically the timeout at the server can be larger; it can afford to wait because there is nothing else to do anyway. Waiting too much at the client can be detrimental because the client has the option of compiling locally and make progress.

```
$ jitserver -XX:JITServerTimeout=5000 &
$ java -XX:+UseJITServer -XX:JITServerTimeout=5000 MyApplication
```

### Encryption (TLS)

By default, network communication is not encrypted. If messages sent between the client and the server need to traverse some untrusted network, you may want to set up encryption. Encryption reduces performance, i.e. compilations take longer and consume more CPU, so consider whether it is required for your use case.

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

## JITServer-specific options

### JITServer heuristics

If a client is given the option `-Xjit:enableJITServerHeuristics`, it will attempt to
guess which compilations are "cheap" (in terms of resources required) and perform them locally, instead of making a compilation
request to a server. The reasoning here is that for cheap compilations, overhead
of network communication is larger than resources required to compile locally.
With heuristics enabled, client will try to maximize benefits of using JITServer while
minimizing overheads.

### JITServer AOT cache

The server-side option `-XX:+JITServerUseAOTCache` enables storing AOT methods
on the server. This allows the server to send AOT method bodies to clients that
do not have pre-populated Shared Class Caches, improving startup time.

The option `-XX:+JITServerUseAOTCache` can then be enabled on the client side so the
client can use the server's AOT cache.

## Logging

As mentioned previously, running the client without any server to connect to still appears to work. This is because the client performs required JIT compilations locally if it cannot connect to a server. To ensure that everything is really working as intended, it is a good idea to enable some logging. It's often most convenient on the server side, because log messages will not interfere with application output, but logging can be added to either the server or the client.

### Connection logging

Option `-XX:+JITServerLogConnections` can be enabled on both the server and the client.
It is the simplest form of JITServer logging. When enabled, connection and disconnection events will be logged to standard error or verbose log (if vlog is specified).

Sample server output:

```
$ jitserver -XX:+JITServerLogConnections
JITServer is currently a technology preview. Its use is not yet supported.

JITServer is ready to accept incoming requests
#JITServer: t=   744 A new client (clientUID=4987233166247274132) connected. Server allocated a new client session.
#JITServer: t=   805 Client (clientUID=4987233166247274132) disconnected. Client session deleted
```

Sample client output:

```
$ java -XX:+JITServerLogConnections -XX:+UseJITServer <MyJavaApp>
JITServer is currently a technology preview. Its use is not yet supported
#INFO:  StartTime: Jul 08 13:10:21 2021
#INFO:  Free Physical Memory: 10587 MB
#INFO:  CPU entitlement = 600.00
#JITServer: t=    10 Connected to a server (serverUID=1158025007443556312)
...
#JITServer: t=   506 Lost connection to the server (serverUID=1158025007443556312)
```

This option can also be enabled with a verbose option `-Xjit:verbose="{JITServerConns}"`.

### JITServer heartbeat

Server has a statistics thread running in the background, which can periodically print general information
about the state of the server. Use `-Xjit:statisticsFrequency=<period-in-ms>` option to enable heartbeat logging.
The output looks like this:

```
$ jitserver -Xjit:statisticsFrequency=3000` # print heartbeat info every 3 seconds

...
#JITServer: CurrentTime: Jul 08 13:42:22 2021
#JITServer: Compilation Queue Size: 0
#JITServer: Number of clients : 1
#JITServer: Total compilation threads : 63
#JITServer: Active compilation threads : 1
#JITServer: Physical memory available: 10560 MB
#JITServer: CpuLoad 66% (AvgUsage 11%) JvmCpu 81%
...
```

This option is useful for tracking the state of JITServer over time, particularly
for tracking things like available memory and CPU usage.

### Verbose logs

With verbose logging, if a client connects successfully then server output should look something like this:

```
$ jitserver -Xjit:verbose=\{JITServer\}

JITServer is currently a technology preview. Its use is not yet supported.
#JITServer: JITServer version: 1.26.0
#JITServer: JITServer Server Mode. Port: 38400. Connection Timeout 30000ms
#JITServer: Started JITServer listener thread: 000000000022AD00

JITServer is ready to accept incoming requests

#JITServer: Server received request for stream 00007F50BC4E7AB0
#JITServer: Server allocated data for a new clientUID 7315883206984276314
#JITServer: compThreadID=0 created clientSessionData=00007F50BC4F0880 for clientUID=7315883206984276314 seqNo=1 (isCritical=1) (criticalSeqNo=0 lastProcessedCriticalReq=0)
#JITServer: compThreadID=0 will ask for address ranges of unloaded classes and CHTable for clientUID 7315883206984276314
#JITServer: compThreadID=0 will initialize CHTable for clientUID 7315883206984276314 size=1152
#JITServer: compThreadID=0 has successfully compiled java/lang/Object.<init>()V
#JITServer: compThreadID=0 found clientSessionData=00007F50BC4F0880 for clientUID=7315883206984276314 seqNo=2 (isCritical=1) (criticalSeqNo=1 lastProcessedCriticalReq=1)
#JITServer: compThreadID=0 has successfully compiled com/ibm/jit/JITHelpers.classIsInterfaceFlag()I
#JITServer: compThreadID=0 found clientSessionData=00007F50BC4F0880 for clientUID=7315883206984276314 seqNo=3 (isCritical=1) (criticalSeqNo=2 lastProcessedCriticalReq=2)
#JITServer: compThreadID=0 has successfully compiled jdk/internal/misc/Unsafe.arrayBaseOffset(Ljava/lang/Class;)I
#JITServer: compThreadID=0 found clientSessionData=00007F50BC4F0880 for clientUID=7315883206984276314 seqNo=4 (isCritical=1) (criticalSeqNo=3 lastProcessedCriticalReq=3)
...
```

JITServer prints out information about new client connections, new networking streams, successful compilations, etc.
The same option can be used on the client to log similar information.

### JITServer version

Note that JITServer prints out version information that is different from `java -version` output.
To use JITServer, client and server must have matching versions.
To check client's version pass it `verbose=\{JITServer\}` option as well.

```
$ java -XX:+UseJITServer -Xjit:verbose=\{JITServer\} <MyJavaApp>

JITServer is currently a technology preview. Its use is not yet supported
#JITServer: JITServer version: 1.26.0
#JITServer: JITServer Client Mode. Server address: localhost port: 38400. Connection Timeout 2000ms
#JITServer: Identifier for current client JVM: 9238414254742342824
...
```
