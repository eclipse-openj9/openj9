# Copyright (c) 2019, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

# To use this docker file:
# 
# 1. Make sure you have an sdk under /your/path/to/j2sdk-image and execute the following command
#   `docker build -f \
#    buildenv/docker/jdk<version>/<platform>/rhel<version>/jitaas/run/Dockerfile \
#    -v /your/path/to/j2sdk-image:/root/j2sdk-image \
#    -t=openj9-jitaas-run .`
#

FROM registry.access.redhat.com/ubi7/ubi:latest

WORKDIR /root

ENTRYPOINT ["/root/j2sdk-image/jre/bin/java", "-XX:StartAsJITServer", "-Xjit:verbose={JITServer}"]

EXPOSE 38400
