/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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


#include "stdio.h"
#include "stdbool.h"
#include "stdlib.h"

typedef struct {
	int x;
	int y;
} Point;

typedef struct {
	Point st;
	Point en;
} Line;

typedef struct {
	Point v;
	int n;
} ScalarVector;

typedef struct {
	char *n;
	Point v;
} PointerVector;

typedef struct {
	char *m;
	char *n;
} PointerPointer;

typedef struct {
    int64_t a;
    double b;
} Complex;

typedef struct {
	int64_t x;
	int32_t y;
	/* there will be 32bits of padding here */
	int64_t z;
} Padding;

typedef struct {
	char *pc;
    Point *pp;
    char **ppc;
    Point **ppp;
} PointerStruct;

typedef struct {
    Line triangle[3];
    int arr[4];
} ArrayStruct;

typedef struct {
    int8_t n;
} SmallStruct;

typedef struct {
	char **pp;
	char *p;
	int n;
	void *x;
	Line *ln;
	Point pt;
	int arr[3];
	double d;
} LargeStruct;

char addTwoByte(char val1, char val2) {
	return val1 + val2;
}

char addTwoChar(char val1, char val2) {
	return val1 + val2 - 'a' + 1;
}

double addTwoDouble(double val1, double val2) {
	return val1 + val2;
}

float addTwoFloat(float val1, float val2) {
	return val1 + val2;
}

int addTwoInt(int val1, int val2) {
	return val1 + val2;
}

long addTwoLong(long val1, long val2) {
	return val1 + val2;
}

short addTwoShort(short val1, short val2) {
	return val1 + val2;
}

bool testBoolean(bool val1) {
	return !val1;
}

int returnFour(void) {
	return 4;
}

void voidWithArgs(int val1) {
	printf("voidWithArgs(%d) success\n", val1);
}

void voidNoArgs(void) {
	printf("voidNoArgs success\n");
}

double manyArgs(int val1, float val2, long val3, double val4) {
	return val3 + val1 * 10 + val2 * 0.1 + val4 * 0.001;
}

int* getIntPtr(void) {
	int *arr = (int*) malloc(sizeof(int) * 4);
	arr[0] = 4;
	arr[1] = 3;
	arr[2] = 2;
	arr[3] = 1;
	return arr;
}

bool checkIntPtr(int *arr) {
	return (4 == arr[0] && 3 == arr[1] && 2 == arr[2] && 1 == arr[3]);
}

void freeIntPtr(int *arr) {
	free(arr);
}

Point getPoint(int x, int y) {
	Point* p = (Point*) malloc(sizeof(Point));
	p->x = x;
	p->y = y;
	return *p;
}

bool checkPoint(Point p, int x, int y) {
	return (p.x == x && p.y == y);
}

Point* getPointPtr(int x, int y) {
	Point* p = (Point*) malloc(sizeof(Point));
	p->x = x;
	p->y = y;
	return p;
}

bool checkPointPtr(Point *p, int x, int y) {
	return (p->x == x && p->y == y);
}

void freePointPtr(Point *p) {
	free(p);
}

Line getLine(Point st, Point en) {
	Line* ln = (Line*) malloc(sizeof(Line));
	ln->st = st;
	ln->en = en;
	return *ln;
}

bool checkLine(Line ln, Point st, Point en) {
	return (ln.st.x == st.x && ln.st.y == st.y && ln.en.x == en.x && ln.en.y == en.y);
}

ScalarVector getScalarVector(int n, Point pt) {
	ScalarVector* s = (ScalarVector*) malloc(sizeof(ScalarVector));
	s->n = n;
	s->v = pt;
	return *s;
}

bool checkScalarVector(ScalarVector s, int n, Point pt) {
	return (s.n == n && s.v.x == pt.x && s.v.y == pt.y);
}

PointerVector getPointerVector(char *n, Point pt) {
	PointerVector* s = (PointerVector*) malloc(sizeof(PointerVector));
	s->n = n;
	s->v = pt;
	return *s;
}

bool checkPointerVector(PointerVector s, char *n, Point pt) {
	return (0 == strcmp(s.n, n) && s.v.x == pt.x && s.v.y == pt.y);
}

PointerPointer getPointerPointer(char *m, char *n) {
	PointerPointer* s = (PointerPointer*) malloc(sizeof(PointerPointer));
	s->m = m;
	s->n = n;
	return *s;
}

bool checkPointerPointer(PointerPointer s, char *m, char *n) {
	return (0 == strcmp(s.m, m) && 0 == strcmp(s.n, n));
}

Complex getComplex(int64_t n, double f) {
	Complex* s = (Complex*) malloc(sizeof(Complex));
	s->a = n;
	s->b = f;
	return *s;
}

bool checkComplex(Complex s, int64_t n, double f) {
	return (s.a == n && s.b == f);
}

Padding getPadding(int64_t a, int32_t b, int64_t c) {
	Padding* s = (Padding*) malloc(sizeof(Padding));
	s->x = a;
	s->y = b;
	s->z = c;
	return *s;
}

bool checkPadding(Padding s, int64_t a, int32_t b, int64_t c) {
	return (s.x == a && s.y == b && s.z == c);
}

PointerStruct getPointerStruct(char *pc, Point *pp, char **ppc, Point **ppp) {
	PointerStruct* s = (PointerStruct*) malloc(sizeof(PointerStruct));
	s->pc = pc;
	s->pp = pp;
	s->ppc = ppc;
	s->ppp = ppp;
	return *s;
}

bool checkPointerStruct(PointerStruct s, char *pc, Point pp, char *ppc, Point ppp) {
	return (0 == strcmp(s.pc, pc) && 0 == strcmp(*(s.ppc), ppc) && s.pp->x == pp.x && s.pp->y == pp.y && (*s.ppp)->x == ppp.x && (*s.ppp)->y == ppp.y);
}

ArrayStruct getArrayStruct(Line ln0, Line ln1, Line ln2, int a0, int a1, int a2, int a3) {
	ArrayStruct* s = (ArrayStruct*) malloc(sizeof(ArrayStruct));
	s->triangle[0] = ln0;
	s->triangle[1] = ln1;
	s->triangle[2] = ln2;

	s->arr[0] = a0;
	s->arr[1] = a1;
	s->arr[2] = a2;
	s->arr[3] = a3;
	return *s;
}

bool checkArrayStruct(ArrayStruct s, Line ln0, Line ln1, Line ln2, int a0, int a1, int a2, int a3) {
	return (s.triangle[0].st.x == ln0.st.x && s.triangle[0].st.y == ln0.st.y  && s.triangle[0].en.x == ln0.en.x  && s.triangle[0].en.y == ln0.en.y
		&& s.triangle[1].st.x == ln1.st.x && s.triangle[1].st.y == ln1.st.y && s.triangle[1].en.x == ln1.en.x && s.triangle[1].en.y == ln1.en.y
		&& s.triangle[2].st.x == ln2.st.x && s.triangle[2].st.y == ln2.st.y && s.triangle[2].en.x == ln2.en.x && s.triangle[2].en.y == ln2.en.y
		&& s.arr[0] == a0 && s.arr[1] == a1 && s.arr[2] == a2 && s.arr[3] == a3 );
}

SmallStruct getSmallStruct(int8_t n) {
	SmallStruct* s = (SmallStruct*) malloc(sizeof(SmallStruct));
	s->n = n;
	return *s;
}

bool checkSmallStruct(SmallStruct s, int8_t n) {
	return (s.n == n);
}

LargeStruct getLargeStruct(char *p, int n, Line *ln, Point pt, int a0, int a1, int a2, double d) {
	LargeStruct* s = (LargeStruct*) malloc(sizeof(LargeStruct));
	s->pp = (char**) malloc(sizeof(char*));
	*(s->pp) = p;
	s->p = p;
	s->n = n;
	s->x = (void *) p;
	s->ln = ln;
	s->pt = pt;
	s->arr[0] = a0;
	s->arr[1] = a1;
	s->arr[2] = a2;
	s->d = d;
	return *s;
}

bool checkLargeStruct(LargeStruct s, char *p, int n, Line ln, Point pt, int a0, int a1, int a2, double d) {
	return (0 == strcmp(*(s.pp), p) && 0 == strcmp(s.p, p) && s.n == n && s.x == p
		&& s.ln->st.x == ln.st.x && s.ln->st.y == ln.st.y && s.ln->en.x == ln.en.x && s.ln->en.y == ln.en.y
		&& s.pt.x == pt.x && s.pt.y == pt.y && s.arr[0] == a0 && s.arr[1] == a1 && s.arr[2] == a2 && s.d == d);
}

Point* getStructArray(void) {
	Point* p1 = (Point*) malloc(sizeof(Point));
	Point* p2 = (Point*) malloc(sizeof(Point));
	p1->x = 2;
	p1->y = 3;
	p2->x = 4;
	p2->y = 6;

	Point* arr = (Point*) malloc(sizeof(Point) * 2);
	arr[0] = *p1;
	arr[1] = *p2;
	return arr;
}

bool checkStructArray(Point *arr) {
	return (2 == arr[0].x && 3 == arr[0].y && 4 == arr[1].x && 6 == arr[1].y);
}

void freeStructArray(Point *arr) {
	free(arr);
}
