/* SPDX-License-Identifier: MIT */

// Fixed-point math routines

#include "asm/fixpoint.hpp"

#include <math.h>

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

uint8_t fixPrecision;

uint8_t fix_Precision() {
	return fixPrecision;
}

double fix_PrecisionFactor() {
	return pow(2.0, fixPrecision);
}

static double fix2double(int32_t i, int32_t q) {
	return i / pow(2.0, q);
}

static int32_t double2fix(double d, int32_t q) {
	if (isnan(d))
		return 0;
	if (isinf(d))
		return d < 0 ? INT32_MIN : INT32_MAX;
	return (int32_t)round(d * pow(2.0, q));
}

static double turn2rad(double t) {
	return t * (M_PI * 2);
}

static double rad2turn(double r) {
	return r / (M_PI * 2);
}

int32_t fix_Sin(int32_t i, int32_t q) {
	return double2fix(sin(turn2rad(fix2double(i, q))), q);
}

int32_t fix_Cos(int32_t i, int32_t q) {
	return double2fix(cos(turn2rad(fix2double(i, q))), q);
}

int32_t fix_Tan(int32_t i, int32_t q) {
	return double2fix(tan(turn2rad(fix2double(i, q))), q);
}

int32_t fix_ASin(int32_t i, int32_t q) {
	return double2fix(rad2turn(asin(fix2double(i, q))), q);
}

int32_t fix_ACos(int32_t i, int32_t q) {
	return double2fix(rad2turn(acos(fix2double(i, q))), q);
}

int32_t fix_ATan(int32_t i, int32_t q) {
	return double2fix(rad2turn(atan(fix2double(i, q))), q);
}

int32_t fix_ATan2(int32_t i, int32_t j, int32_t q) {
	return double2fix(rad2turn(atan2(fix2double(i, q), fix2double(j, q))), q);
}

int32_t fix_Mul(int32_t i, int32_t j, int32_t q) {
	return double2fix(fix2double(i, q) * fix2double(j, q), q);
}

int32_t fix_Div(int32_t i, int32_t j, int32_t q) {
	return double2fix(fix2double(i, q) / fix2double(j, q), q);
}

int32_t fix_Mod(int32_t i, int32_t j, int32_t q) {
	return double2fix(fmod(fix2double(i, q), fix2double(j, q)), q);
}

int32_t fix_Pow(int32_t i, int32_t j, int32_t q) {
	return double2fix(pow(fix2double(i, q), fix2double(j, q)), q);
}

int32_t fix_Log(int32_t i, int32_t j, int32_t q) {
	return double2fix(log(fix2double(i, q)) / log(fix2double(j, q)), q);
}

int32_t fix_Round(int32_t i, int32_t q) {
	return double2fix(round(fix2double(i, q)), q);
}

int32_t fix_Ceil(int32_t i, int32_t q) {
	return double2fix(ceil(fix2double(i, q)), q);
}

int32_t fix_Floor(int32_t i, int32_t q) {
	return double2fix(floor(fix2double(i, q)), q);
}
