<!--
Copyright (c) 2020, 2020 Red Hat. and others

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
# Building the snapshot+restore prototype

The snapshot+restore prototype currently builds with the JDK8 Extensions repo and the `snapshot` branch of the eclipse#openj9 and the eclipse#openj9-omr repos.
Currently, the snapshot branch only builds on Linux x86-64.  Parts of the code have been ported to other architectures - such as the ReadOnly code cache on Z -
but the complete build is not yet complete on those platforms.

# Configure:

```
bash configure --with-freemarker-jar=/root/freemarker.jar --with-boot-jdk=/usr/lib/jvm/adoptojdk-java-80 --disable-ddr
```

# Compile:

```
make images
```

Note, the exploded jdk image (basic target of `make`) doesn't work with OpenJ9 JDK8 builds.

DDR support is disabled due to changes in data structure layouts breaking its compilation - see #10586.

