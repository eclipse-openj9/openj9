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
package org.openj9.test.floatsanity;

/* Provides double constants for testing trig functions.  All the constants
	in this class should be verified by CheckDoubleAssumptions! */

public class TD	{

	public static final	double PPI = Math.PI;
	public static final	double NPI = -PPI;
	public static final	double P2PI	= 2.0*Math.PI;
	public static final	double N2PI	= -P2PI;
	public static final	double PHPI	= Math.PI/2.0;
	public static final	double NHPI	= -PHPI;
	public static final	double P3HPI = 3.0*Math.PI/2.0;
	public static final	double N3HPI = -P3HPI;
	public static final	double PQPI	= Math.PI/4.0;
	public static final	double NQPI	= -PQPI;
	public static final	double P3QPI= 3.0*Math.PI/4.0;
	public static final	double N3QPI	= -P3QPI;

	public static final	double p0deg   = D.PZERO;  
	public static final	double p15deg  = 15.0; 
	public static final	double p30deg  = 30.0; 
	public static final	double p45deg  = 45.0; 
	public static final	double p60deg  = 60.0; 
	public static final	double p75deg  = 75.0;
	public static final	double p90deg  = 90.0; 
	public static final	double p105deg = 105.0;
	public static final	double p120deg = 120.0;
	public static final	double p135deg = 135.0;
	public static final	double p150deg = 150.0;
	public static final	double p165deg = 165.0;
	public static final	double p180deg = 180.0;
	public static final	double p195deg = 195.0;
	public static final	double p210deg = 210.0;
	public static final	double p225deg = 225.0;
	public static final	double p240deg = 240.0;
	public static final	double p255deg = 255.0;
	public static final	double p270deg = 270.0;
	public static final	double p285deg = 285.0;
	public static final	double p300deg = 300.0;
	public static final	double p315deg = 315.0;
	public static final	double p330deg = 330.0;
	public static final	double p345deg = 345.0;
	public static final	double p360deg = 360.0;

	public static final	double n0deg   = D.NZERO;
	public static final	double n15deg  = -p15deg;
	public static final	double n30deg  = -p30deg;
	public static final	double n45deg  = -p45deg;
	public static final	double n60deg  = -p60deg;
	public static final	double n75deg  = -p75deg;
	public static final	double n90deg  = -p90deg;
	public static final	double n105deg = -p105deg;
	public static final	double n120deg = -p120deg;
	public static final	double n135deg = -p135deg;
	public static final	double n150deg = -p150deg;
	public static final	double n165deg = -p165deg;
	public static final	double n180deg = -p180deg;

	public static final	double[] degArray = {
		p0deg, p15deg, p30deg, p45deg, p60deg, p75deg, p90deg,
		p105deg, p120deg, p135deg, p150deg,	p165deg, p180deg,
		p195deg, p210deg, p225deg, p240deg,	p255deg, p270deg,
		p285deg, p300deg, p315deg, p330deg,	p345deg, p360deg
	};
	public static final	double[] ndegArray = {
		n0deg, n15deg, n30deg, n45deg, n60deg, n75deg, n90deg,
		n105deg, n120deg, n135deg, n150deg,	n165deg, n180deg
	};        	

	public static final	double p0rad   = D.PZERO;
	public static final	double p15rad  = p15deg	 * P2PI	/ p360deg;
	public static final	double p30rad  = p30deg	 * P2PI	/ p360deg;
	public static final	double p45rad  = p45deg	 * P2PI	/ p360deg;
	public static final	double p60rad  = p60deg	 * P2PI	/ p360deg;
	public static final	double p75rad  = p75deg	 * P2PI	/ p360deg;
	public static final	double p90rad  = PHPI;
	public static final	double p105rad = p105deg * P2PI	/ p360deg;
	public static final	double p120rad = p120deg * P2PI	/ p360deg;
	public static final	double p135rad = p135deg * P2PI	/ p360deg;
	public static final	double p150rad = p150deg * P2PI	/ p360deg;
	public static final	double p165rad = p165deg * P2PI	/ p360deg;
	public static final	double p180rad = PPI;
	public static final	double p195rad = p195deg * P2PI	/ p360deg;
	public static final	double p210rad = p210deg * P2PI	/ p360deg;
	public static final	double p225rad = p225deg * P2PI	/ p360deg;
	public static final	double p240rad = p240deg * P2PI	/ p360deg;
	public static final	double p255rad = p255deg * P2PI	/ p360deg;
	public static final	double p270rad = P3HPI;
	public static final	double p285rad = p285deg * P2PI	/ p360deg;
	public static final	double p300rad = p300deg * P2PI	/ p360deg;
	public static final	double p315rad = p315deg * P2PI	/ p360deg;
	public static final	double p330rad = p330deg * P2PI	/ p360deg;
	public static final	double p345rad = p345deg * P2PI	/ p360deg;
	public static final	double p360rad = P2PI;

	public static final	double n0rad   = D.NZERO;
	public static final	double n15rad  = -p15rad;
	public static final	double n30rad  = -p30rad;
	public static final	double n45rad  = -p45rad;
	public static final	double n60rad  = -p60rad;
	public static final	double n75rad  = -p75rad;
	public static final	double n90rad  = NHPI;
	public static final	double n105rad = -p105rad;
	public static final	double n120rad = -p120rad;
	public static final	double n135rad = -p135rad;
	public static final	double n150rad = -p150rad;
	public static final	double n165rad = -p165rad;
	public static final	double n180rad = NPI;

	public static final	double[] radArray = {
		p0rad, p15rad, p30rad, p45rad, p60rad, p75rad, p90rad,
		p105rad, p120rad, p135rad, p150rad,	p165rad, p180rad,
		p195rad, p210rad, p225rad, p240rad,	p255rad, p270rad,
		p285rad, p300rad, p315rad, p330rad,	p345rad, p360rad
	};
	public static final	double[] nradArray = {
		n0rad, n15rad, n30rad, n45rad, n60rad, n75rad, n90rad,
		n105rad, n120rad, n135rad, n150rad,	n165rad, n180rad
	};

	public static final	double[] radArray2 = {
		p0rad, p15rad, p30rad, p45rad, p60rad, p75rad,
		p90rad,	p105rad, p120rad, p135rad, p150rad,	p165rad,
		n180rad, n165rad, n150rad, n135rad,	n120rad, n105rad,
		n90rad,	n75rad,	n60rad,	n45rad,	n30rad,	n15rad
	};

	public static final	double pS0	 = D.PZERO;
	public static final	double pS15	 = 0.25881904510252074;
	public static final	double pS30	 = 0.5;
	public static final	double pS45	 = 0.7071067811865475;
	public static final	double pS60	 = 0.8660254037844386;
	public static final	double pS75	 = 0.9659258262890683;
	public static final	double pS90	 = D.pOne;

	public static final	double nS0	 = D.NZERO;
	public static final	double nS15	 = -0.25881904510252074;
	public static final	double nS30	 = -0.5;
	public static final	double nS45	 = -0.7071067811865475;
	public static final	double nS60	 = -0.8660254037844386;
	public static final	double nS75	 = -0.9659258262890683;
	public static final	double nS90	 = D.nOne;

	public static final double[] sinArray = {
		TD.pS0, TD.pS15, TD.pS30, TD.pS45, TD.pS60, TD.pS75,
		TD.pS90, TD.pS75, TD.pS60, TD.pS45, TD.pS30, TD.pS15,
		TD.nS0, TD.nS15, TD.nS30, TD.nS45, TD.nS60, TD.nS75,
		TD.nS90, TD.nS75, TD.nS60, TD.nS45, TD.nS30, TD.nS15
	};
	public static final double[] cosArray = {
		TD.pS90, TD.pS75, TD.pS60, TD.pS45, TD.pS30, TD.pS15,
		TD.nS0, TD.nS15, TD.nS30, TD.nS45, TD.nS60, TD.nS75,
		TD.nS90, TD.nS75, TD.nS60, TD.nS45, TD.nS30, TD.nS15,
		TD.pS0, TD.pS15, TD.pS30, TD.pS45, TD.pS60, TD.pS75
	};

	public static final	double pT0	 = D.PZERO;
	public static final	double pT15	 = pS15	/ pS75;
	public static final	double pT30	 = pS30	/ pS60;
	public static final	double pT45	 = D.pOne;
	public static final	double pT60	 = pS60	/ pS30;
	public static final	double pT75	 = pS75	/ pS15;
	public static final	double pT90	 = D.PINF;

	public static final double[] tanArray = {
		TD.pT0, TD.pT15, TD.pT30, TD.pT45, TD.pT60, TD.pT75, TD.pT90
	};
	
	public static final double[] pAxisArray = {
		D.PZERO, D.PMIN, D.p0_0001, D.pOne, D.pTwo, D.p200, D.PMAX, D.PINF
	};
	public static final double[] nAxisArray = {
		D.NZERO, D.NMIN, D.n0_0001, D.nOne, D.nTwo, D.n200, D.NMAX, D.NINF
	};

}

