#!/bin/sh

#
# Copyright IBM Corp. and others 2023
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
#

echo "Creating SSL certificates";

COMMON_NAME="localhost"
VALID_DAYS=365

# Generate private key
openssl genrsa -out key.pem 2048

# Generate self-signed certificate
openssl req -new -x509 -sha256 -key key.pem -out cert.pem -days $VALID_DAYS -subj "/CN=$COMMON_NAME"

# Generate another private key and self-signed certificate
openssl req -nodes -newkey rsa:2048 -keyout wrongKey.pem -x509 -days 365 -out wrongCert.pem  -subj "/CN=localhost"

# Generate another self-signed certificate
openssl req -new -x509 -sha256 -key key.pem -out nosslserverCert.pem -days $VALID_DAYS -subj "/CN=$COMMON_NAME"

echo "Certificates generated";
