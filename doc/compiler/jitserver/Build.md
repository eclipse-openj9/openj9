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

There are currently 3 supported build procedures. Select the one that best suits your needs.

- [Building the whole VM](#vm). The complete build process (Most correct and recommended). Produces a complete JVM as output (including the JIT).
- [Building only the JIT](#jit). Choose this if you have an existing SDK (with JITServer enabled) and have no need to modify the VM or JCL. This only builds OMR and everything in the `runtime/compiler` directory. Output artifact is `libj9jit29.so`. (Fastest way to get JITServer built)
- [Building in Docker](#docker). Choose this if you don't want to deal with build dependencies. Takes the longest.

For building the whole VM and building only the JIT, you will have to go through the [Prepare your system](#prerequisites) step.
For building in Docker, you can skip [Prepare your system](#prerequisites).

If you run into issues during this process, please document any solutions here.

## PREREQUISITES
- The following example is for Ubuntu18:

```
apt-get update \
 && apt-get install -qq -y --no-install-recommends \
	  autoconf \
	  automake \
	  build-essential \
	  ca-certificates \
	  ccache \
	  cmake \
	  cpio \
	  file \
	  git \
	  libasound2-dev \
	  libcups2-dev \
	  libdwarf-dev \
	  libelf-dev \
	  libfontconfig1-dev \
	  libfreetype6-dev \
	  libnuma-dev \
	  libx11-dev \
	  libxext-dev \
	  libxrender-dev \
	  libxt-dev \
	  libxtst-dev \
	  libssl-dev \
	  libtool \
	  make \
	  nasm \
	  pkg-config \
	  software-properties-common \
	  ssh \
	  unzip \
	  wget \
	  zip \
 && rm -rf /var/lib/apt/lists/*

apt-get update \
 && add-apt-repository ppa:ubuntu-toolchain-r/test -y \
 && apt-get update \
 && apt-get install -qq -y --no-install-recommends gcc-7 g++-7 \
 && rm -rf /var/lib/apt/lists/*

ln -sf /usr/bin/g++ /usr/bin/c++ \
 && ln -sf /usr/bin/g++-7 /usr/bin/g++ \
 && ln -sf /usr/bin/gcc /usr/bin/cc \
 && ln -sf /usr/bin/gcc-7 /usr/bin/gcc

cd /root \
 && wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tar.gz \
 && tar -xzf freemarker.tar.gz freemarker-2.3.8/lib/freemarker.jar --strip=2 \
 && rm -f freemarker.tar.gz

cd /root \
 && wget https://api.adoptopenjdk.net/openjdk8-openj9/releases/x64_linux/latest/binary -O bootjdk8.tar.gz \
 && tar -xzf bootjdk8.tar.gz \
 && rm -f bootjdk8.tar.gz \
 && mv $(ls | grep -i jdk) bootjdk8


export JAVA_HOME=/root/bootjdk8
export PATH="$JAVA_HOME/bin:$PATH"
```

## VM

The `JITServer` feature is not enabled by default on the `master` branch at the moment. **`--enable-jitserver` option needs to be passed in the configure step`**

```
git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8.git

cd openj9-openjdk-jdk8

bash get_source.sh -openj9-repo=https://github.com/eclipse/openj9.git -omr-repo=https://github.com/eclipse/openj9-omr.git

bash configure --with-freemarker-jar=/root/freemarker.jar --with-boot-jdk=/root/bootjdk8 --enable-jitserver

make all
```
Depending on where you want to fetch OpenJ9 sources from, you could use your own repository as shown below:
```
bash get_source.sh -openj9-repo=https://github.com/<Your GitHub UserID>/openj9.git -omr-repo=https://github.com/eclipse/openj9-omr.git
```
See https://www.eclipse.org/openj9/oj9_build.html for more detail.

You can test the success of the build process by starting the JVM in server and client mode.

```
cd build/linux-x86_64-normal-server-release/images/jdk
```
Run:

To start the JVM in server mode:
```
$./bin/jitserver
JITServer is currently a technology preview. Its use is not yet supported

JITServer is ready to accept incoming requests
```
To start the JVM in client mode:
```
$./bin/java -XX:+UseJITServer -version
JIT: using build "Nov 6 2019 20:02:35"
JIT level: cf3ba4120
openjdk version "1.8.0_232-internal"
OpenJDK Runtime Environment (build 1.8.0_232-internal-root_2019_11_06_19_57-b00)
Eclipse OpenJ9 VM (build master-cf3ba4120, JRE 1.8.0 Linux amd64-64-Bit Compressed References 20191106_000000(JIT enabled, AOT enabled)
OpenJ9   - cf3ba4120
OMR      - cf9d75a4a
JCL      - 03cb3a3cb4 based on jdk8u232-b09)
```
## JIT

If you already have an SDK enabled with JITServer and just want to rebuild the JIT library, then **`export J9VM_OPT_JITSERVER=1` needs to be added to the build environment**.

```
export JAVA_HOME=/your/sdk
export J9SRC="$JAVA_HOME"/jre/lib/amd64/compressedrefs
export JIT_SRCBASE=/your/openj9/runtime
export OMR_SRCBASE=/your/omr

export J9VM_OPT_JITSERVER=1

ln -s "$OMR_SRCBASE" "$JIT_SRCBASE"/omr
cp "$J9SRC"/compiler/env/ut_j9jit.* "$JIT_SRCBASE"/compiler/env/

cd "$JIT_SRCBASE"/runtime/compiler
make -f compiler.mk -C "$JIT_SRCBASE"/compiler -j$(nproc) J9SRC="$JAVA_HOME"/jre/lib/amd64/compressedrefs/ JIT_SRCBASE=.. BUILD_CONFIG=debug J9_VERSION=29 JIT_OBJBASE=../objs
cp "$JIT_SRCBASE"/compiler/../objs/libj9jit29.so "$J9SRC"
```

The only new addition to the build process for JITServer is to protobuf schema files. These are supposed to be build automatically, but it keeps getting broken by upstream changes to openJ9. So, if you get errors about missing files named like `compile.pb.h`, try building the make target `proto` by adding the word `proto` to the end of the make command above.

## DOCKER

Follow the [Docker.md](Docker.md) section `build/Dockerfile` for building JITServer in Docker.
