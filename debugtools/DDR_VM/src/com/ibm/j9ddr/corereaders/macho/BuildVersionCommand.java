/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
/*******************************************************************************
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights
 * Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *******************************************************************************/
package com.ibm.j9ddr.corereaders.macho;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import javax.imageio.stream.ImageInputStream;

public class BuildVersionCommand extends LoadCommand
{

	// version numbers are encoded as nibbles in an int in xxxx.yy.zz format
	public static class Version
	{
		int major;
		int minor;
		int patch;

		public Version(int encoding)
		{
			for (int i = 8; i > 4; i--) {
				major = major * 10 + ((encoding >>> (i * 4)) & 0xf);
			}
			for (int i = 4; i > 2; i--) {
				minor = minor * 10 + ((encoding >>> (i * 4)) & 0xf);
			}
			for (int i = 2; i > 0; i--) {
				patch = patch * 10 + ((encoding >>> (i * 4)) & 0xf);
			}
		}
	}

	public static class BuildToolVersion
	{
		public static final int TOOL_CLANG = 1;
		public static final int TOOL_SWIFT = 2;
		public static final int TOOL_LD	= 3;
		int toolType;
		int version;
	}

	int platform;
	int minOs;
	int sdk;
	int numTools;
	Version minOsVersion;
	Version sdkVersion;
	List<BuildToolVersion> tools;
	
	public BuildVersionCommand() {}

	public BuildVersionCommand readCommand(ImageInputStream stream, long streamSegmentOffset) throws IOException
	{
		super.readCommand(stream, streamSegmentOffset);
		minOs = stream.readInt();
		sdk = stream.readInt();
		minOsVersion = new Version(minOs);
		sdkVersion = new Version(sdk);
		numTools = stream.readInt();
		tools = new ArrayList<>(numTools);
		for (int i = 0; i < numTools; i++) {
			BuildToolVersion tool = new BuildToolVersion();
			tool.toolType = stream.readInt();
			tool.version = stream.readInt();
			tools.add(tool);
		}
		return this;
	}
}
