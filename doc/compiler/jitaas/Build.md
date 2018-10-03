<!--
Copyright (c) 2018, 2018 IBM Corp. and others

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

There are currently 3 supported build procedures. Select the one that best suits your needs.

- [Building only the JIT](#jit). Choose this if you have no need to modify the VM or JCL. This only builds OMR and everything in the `runtime/compiler` directory. Output artifact is `libj9jit29.so`.
- [Building the whole VM](#vm). The complete build process. Takes much longer but produces a complete JVM as output (including the JIT).
- [Building in Docker](#docker). Choose this if you don't want to deal with build dependencies. Takes the longest.

Either way, you will need protobuf installed.

If you run into issues during this process, please document any solutions here.

## JIT

```
$ cd runtime/compiler
$ make -f compiler.mk -C . -j$(nproc) J9SRC=$JAVA_HOME/jre/lib/amd64/compressedrefs/ JIT_SRCBASE=.. BUILD_CONFIG=debug J9_VERSION=29 JIT_OBJBASE=../objs

```

The only new addition to the build process for JITaaS is to protbuf schema files. These are supposed to be build automatically, but it keeps getting broken by upstream changes to openJ9. So, if you get errors about missing files named like `compile.pb.h`, try building the make target `proto` by adding the word `proto` to the end of the make command above.

## VM

See https://www.eclipse.org/openj9/oj9_build.html. The only difference is you need to check out the JITaaS code instead of upstream OpenJ9.


## DOCKER

Follow the [README.md](buildenv/docker/jitaas/jdk8/x86_64/ubuntu18/README.md) section `build/Dockerfile` for building JITaaS in Docker.
