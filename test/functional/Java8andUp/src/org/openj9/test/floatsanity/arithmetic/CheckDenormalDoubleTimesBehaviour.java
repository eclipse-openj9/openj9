/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package org.openj9.test.floatsanity.arithmetic;

import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class CheckDenormalDoubleTimesBehaviour {

	public static Logger logger = Logger.getLogger(CheckDenormalDoubleTimesBehaviour.class);

	@BeforeClass
	public void groupName() {
		logger.debug("Check denormal double times behaviour");
	}

	public void denormal_double_times() {
		double x[] = new double[60];
		double y[] = new double[60];
		double r[] = new double[60];

		x[0] = Double.longBitsToDouble(0x3ff0000000000000L);
		y[0] = Double.longBitsToDouble(0x8000000000009000L);
		r[0] = Double.longBitsToDouble(0x8000000000009000L);

		x[1] = Double.longBitsToDouble(0x3ff0000000000000L);
		y[1] = Double.longBitsToDouble(0x8000000000000900L);
		r[1] = Double.longBitsToDouble(0x8000000000000900L);

		x[2] = Double.longBitsToDouble(0x3ff0000000000000L);
		y[2] = Double.longBitsToDouble(0x8000000000000009L);
		r[2] = Double.longBitsToDouble(0x8000000000000009L);

		x[0] = Double.longBitsToDouble(0x000ffffffffffff8L);
		y[0] = Double.longBitsToDouble(0x3ff0000000000001L);
		r[0] = Double.longBitsToDouble(0x000ffffffffffff9L);

		x[1] = Double.longBitsToDouble(0x001fffffffffffffL);
		y[1] = Double.longBitsToDouble(0xbfe0000000000000L);
		r[1] = Double.longBitsToDouble(0x8010000000000000L);

		x[2] = Double.longBitsToDouble(0x0000000000000001L);
		y[2] = Double.longBitsToDouble(0x7ff8000000000000L);
		r[2] = Double.longBitsToDouble(0x7ff8000000000000L);

		x[3] = Double.longBitsToDouble(0x0000000044e38425L);
		y[3] = Double.longBitsToDouble(0x414dbaa640000000L);
		r[3] = Double.longBitsToDouble(0x0010000005e8e60eL);

		x[4] = Double.longBitsToDouble(0x001fffffffffffffL);
		y[4] = Double.longBitsToDouble(0x4000000000000000L);
		r[4] = Double.longBitsToDouble(0x002fffffffffffffL);

		x[5] = Double.longBitsToDouble(0x3b200000007e03f7L);
		y[5] = Double.longBitsToDouble(0x3cd0000000010000L);
		r[5] = Double.longBitsToDouble(0x38000000007F03F7L);

		x[6] = Double.longBitsToDouble(0x3ff0000000000000L);
		y[6] = Double.longBitsToDouble(0x8000000000090000L);
		r[6] = Double.longBitsToDouble(0x8000000000090000L);

		x[7] = Double.longBitsToDouble(0x0010000000000007L);
		y[7] = Double.longBitsToDouble(0x3fe9000000000000L);
		r[7] = Double.longBitsToDouble(0x000c800000000005L);

		x[8] = Double.longBitsToDouble(0x3fe9000000000000L);
		y[8] = Double.longBitsToDouble(0x10000000000007L);
		r[8] = Double.longBitsToDouble(0xc800000000005L);

		x[9] = Double.longBitsToDouble(0x10000000000007L);
		y[9] = Double.longBitsToDouble(0x3fe9200000000000L);
		r[9] = Double.longBitsToDouble(0xc900000000005L);

		x[10] = Double.longBitsToDouble(0x3fe9200000000000L);
		y[10] = Double.longBitsToDouble(0x10000000000007L);
		r[10] = Double.longBitsToDouble(0xc900000000005L);

		x[11] = Double.longBitsToDouble(0x10000000000007L);
		y[11] = Double.longBitsToDouble(0x3fe9240000000000L);
		r[11] = Double.longBitsToDouble(0xc920000000005L);

		x[12] = Double.longBitsToDouble(0x3fe9240000000000L);
		y[12] = Double.longBitsToDouble(0x10000000000007L);
		r[12] = Double.longBitsToDouble(0xc920000000005L);

		x[13] = Double.longBitsToDouble(0x10000000000007L);
		y[13] = Double.longBitsToDouble(0x3fe9248000000000L);
		r[13] = Double.longBitsToDouble(0xc924000000005L);

		x[14] = Double.longBitsToDouble(0x3fe9248000000000L);
		y[14] = Double.longBitsToDouble(0x10000000000007L);
		r[14] = Double.longBitsToDouble(0xc924000000005L);

		x[15] = Double.longBitsToDouble(0x1000000000000fL);
		y[15] = Double.longBitsToDouble(0x3fe8888800000000L);
		r[15] = Double.longBitsToDouble(0xc44440000000bL);

		x[16] = Double.longBitsToDouble(0x3fe8888800000000L);
		y[16] = Double.longBitsToDouble(0x1000000000000fL);
		r[16] = Double.longBitsToDouble(0xc44440000000bL);

		x[17] = Double.longBitsToDouble(0x1000000000000fL);
		y[17] = Double.longBitsToDouble(0x3fe4444400000000L);
		r[17] = Double.longBitsToDouble(0xa222200000009L);

		x[18] = Double.longBitsToDouble(0x3fe4444400000000L);
		y[18] = Double.longBitsToDouble(0x1000000000000fL);
		r[18] = Double.longBitsToDouble(0xa222200000009L);

		x[19] = Double.longBitsToDouble(0x1000000000000fL);
		y[19] = Double.longBitsToDouble(0x3fe8888880000000L);
		r[19] = Double.longBitsToDouble(0xc44444000000bL);

		x[20] = Double.longBitsToDouble(0x3fe8888880000000L);
		y[20] = Double.longBitsToDouble(0x1000000000000fL);
		r[20] = Double.longBitsToDouble(0xc44444000000bL);

		x[21] = Double.longBitsToDouble(0x1000000000000fL);
		y[21] = Double.longBitsToDouble(0x3fe4444440000000L);
		r[21] = Double.longBitsToDouble(0xa222220000009L);

		x[22] = Double.longBitsToDouble(0x3fe4444440000000L);
		y[22] = Double.longBitsToDouble(0x1000000000000fL);
		r[22] = Double.longBitsToDouble(0xa222220000009L);

		x[23] = Double.longBitsToDouble(0x1000000000001fL);
		y[23] = Double.longBitsToDouble(0x3fe8421084000000L);
		r[23] = Double.longBitsToDouble(0xc210842000017L);

		x[24] = Double.longBitsToDouble(0x3fe8421084000000L);
		y[24] = Double.longBitsToDouble(0x1000000000001fL);
		r[24] = Double.longBitsToDouble(0xc210842000017L);

		x[25] = Double.longBitsToDouble(0x7ffffffL);
		y[25] = Double.longBitsToDouble(0x3ff0000001000000L);
		r[25] = Double.longBitsToDouble(0x7ffffffL);

		x[26] = Double.longBitsToDouble(0x3ff0000001000000L);
		y[26] = Double.longBitsToDouble(0x7ffffffL);
		r[26] = Double.longBitsToDouble(0x7ffffffL);

		x[27] = Double.longBitsToDouble(0x1000000000001fL);
		y[27] = Double.longBitsToDouble(0x3fe4210842000000L);
		r[27] = Double.longBitsToDouble(0xa108421000013L);

		x[28] = Double.longBitsToDouble(0x3fe4210842000000L);
		y[28] = Double.longBitsToDouble(0x1000000000001fL);
		r[28] = Double.longBitsToDouble(0xa108421000013L);

		x[29] = Double.longBitsToDouble(0xfffffffL);
		y[29] = Double.longBitsToDouble(0x3ff0000000800000L);
		r[29] = Double.longBitsToDouble(0xfffffffL);

		x[30] = Double.longBitsToDouble(0x3ff0000000800000L);
		y[30] = Double.longBitsToDouble(0xfffffffL);
		r[30] = Double.longBitsToDouble(0xfffffffL);

		x[31] = Double.longBitsToDouble(0x1000000000001fL);
		y[31] = Double.longBitsToDouble(0x3fe2108421000000L);
		r[31] = Double.longBitsToDouble(0x9084210800011L);

		x[32] = Double.longBitsToDouble(0x3fe2108421000000L);
		y[32] = Double.longBitsToDouble(0x1000000000001fL);
		r[32] = Double.longBitsToDouble(0x9084210800011L);

		x[33] = Double.longBitsToDouble(0x1fffffffL);
		y[33] = Double.longBitsToDouble(0x3ff0000000400000L);
		r[33] = Double.longBitsToDouble(0x1fffffffL);

		x[34] = Double.longBitsToDouble(0x3ff0000000400000L);
		y[34] = Double.longBitsToDouble(0x1fffffffL);
		r[34] = Double.longBitsToDouble(0x1fffffffL);

		x[35] = Double.longBitsToDouble(0x3fffffffL);
		y[35] = Double.longBitsToDouble(0x3ff0000000200000L);
		r[35] = Double.longBitsToDouble(0x3fffffffL);

		x[36] = Double.longBitsToDouble(0x3ff0000000200000L);
		y[36] = Double.longBitsToDouble(0x3fffffffL);
		r[36] = Double.longBitsToDouble(0x3fffffffL);

		x[37] = Double.longBitsToDouble(0x7fffffffL);
		y[37] = Double.longBitsToDouble(0x3ff0000000100000L);
		r[37] = Double.longBitsToDouble(0x7fffffffL);

		x[38] = Double.longBitsToDouble(0x3ff0000000100000L);
		y[38] = Double.longBitsToDouble(0x7fffffffL);
		r[38] = Double.longBitsToDouble(0x7fffffffL);

		x[39] = Double.longBitsToDouble(0x1000000000001fL);
		y[39] = Double.longBitsToDouble(0x3fe8421084200000L);
		r[39] = Double.longBitsToDouble(0xc210842100017L);

		x[40] = Double.longBitsToDouble(0x3fe8421084200000L);
		y[40] = Double.longBitsToDouble(0x1000000000001fL);
		r[40] = Double.longBitsToDouble(0xc210842100017L);

		x[41] = Double.longBitsToDouble(0xffffffffL);
		y[41] = Double.longBitsToDouble(0x3ff0000000080000L);
		r[41] = Double.longBitsToDouble(0xffffffffL);

		x[42] = Double.longBitsToDouble(0x3ff0000000080000L);
		y[42] = Double.longBitsToDouble(0xffffffffL);
		r[42] = Double.longBitsToDouble(0xffffffffL);

		x[43] = Double.longBitsToDouble(0x1000000000001fL);
		y[43] = Double.longBitsToDouble(0x3fe4210842100000L);
		r[43] = Double.longBitsToDouble(0xa108421080013L);

		x[44] = Double.longBitsToDouble(0x3fe4210842100000L);
		y[44] = Double.longBitsToDouble(0x1000000000001fL);
		r[44] = Double.longBitsToDouble(0xa108421080013L);

		x[45] = Double.longBitsToDouble(0x1ffffffffL);
		y[45] = Double.longBitsToDouble(0x3ff0000000040000L);
		r[45] = Double.longBitsToDouble(0x1ffffffffL);

		x[46] = Double.longBitsToDouble(0x3ff0000000040000L);
		y[46] = Double.longBitsToDouble(0x1ffffffffL);
		r[46] = Double.longBitsToDouble(0x1ffffffffL);

		x[47] = Double.longBitsToDouble(0x1000000000001fL);
		y[47] = Double.longBitsToDouble(0x3fe2108421080000L);
		r[47] = Double.longBitsToDouble(0x9084210840011L);

		x[48] = Double.longBitsToDouble(0x3fL);
		y[48] = Double.longBitsToDouble(0x3ff8410410410410L);
		r[48] = Double.longBitsToDouble(0x5fL);

		x[49] = Double.longBitsToDouble(0x3ff8410410410410L);
		y[49] = Double.longBitsToDouble(0x3fL);
		r[49] = Double.longBitsToDouble(0x5fL);

		x[50] = Double.longBitsToDouble(0x3fL);
		y[50] = Double.longBitsToDouble(0x3ff0208208208208L);
		r[50] = Double.longBitsToDouble(0x3fL);

		x[51] = Double.longBitsToDouble(0x3ff0208208208208L);
		y[51] = Double.longBitsToDouble(0x3fL);
		r[51] = Double.longBitsToDouble(0x3fL);

		x[52] = Double.longBitsToDouble(0x7fL);
		y[52] = Double.longBitsToDouble(0x3ff0102040810204L);
		r[52] = Double.longBitsToDouble(0x7fL);

		x[53] = Double.longBitsToDouble(0x3ff0102040810204L);
		y[53] = Double.longBitsToDouble(0x7fL);
		r[53] = Double.longBitsToDouble(0x7fL);

		x[54] = Double.longBitsToDouble(0x3fe8102040810204L);
		y[54] = Double.longBitsToDouble(0x7fL);
		r[54] = Double.longBitsToDouble(0x5fL);

		x[55] = Double.longBitsToDouble(0x7fL);
		y[55] = Double.longBitsToDouble(0x3fe4081020408102L);
		r[55] = Double.longBitsToDouble(0x4fL);

		x[56] = Double.longBitsToDouble(0x3fe4081020408102L);
		y[56] = Double.longBitsToDouble(0x7fL);
		r[56] = Double.longBitsToDouble(0x4fL);

		x[57] = Double.longBitsToDouble(0x7fL);
		y[57] = Double.longBitsToDouble(0x3fe2040810204081L);
		r[57] = Double.longBitsToDouble(0x47L);

		x[58] = Double.longBitsToDouble(0x3fe2040810204081L);
		y[58] = Double.longBitsToDouble(0x7fL);
		r[58] = Double.longBitsToDouble(0x47L);

		x[59] = Double.longBitsToDouble(0x7fL);
		y[59] = Double.longBitsToDouble(0x3fe8102040810204L);
		r[59] = Double.longBitsToDouble(0x5fL);

		for (int i = 0; i < x.length; i++) {
			String operation = x[i] + " * " + y[i] + " == " + r[i];
			logger.debug("testing operation: " + operation);
			if (Double.isNaN(r[i])) {
				Assert.assertTrue(Double.isNaN(x[i] * y[i]), operation);
			} else {
				Assert.assertEquals(x[i] * y[i], r[i], 0, operation);
			}
		}

	}

}
