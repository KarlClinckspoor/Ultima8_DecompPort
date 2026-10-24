// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\MERGEARY.C

#include "u8port.h"

#include "cdesc.h"

// Fades dest toward src one step per call, Bresenham style.
// The blend's state, exported by CDESC.H (PAL.C's STOP_MERGE frees it) as init, error,
// delta, vector, major and iterations; prefixed so the short names meet nothing at link time.
int16_t mergeInit = 1;
char *mergeError;
char *mergeDelta;
char *mergeVector;
int16_t mergeMajor;
int16_t mergeIterations;

int16_t MERGEBYTEARRAY(char *dest, char *src, int16_t count)
{
	int16_t i;
	int16_t d;

	if (mergeInit) {
		mergeError = new (std::nothrow) char[count];
		mergeDelta = new (std::nothrow) char[count];
		mergeVector = new (std::nothrow) char[count];
		if (mergeError == 0 || mergeDelta == 0 || mergeVector == 0) {
			if (mergeError)
				delete[] mergeError;
			if (mergeDelta)
				delete[] mergeDelta;
			if (mergeVector)
				delete[] mergeVector;
			return 0;
		}
		mergeMajor = 0;
		for (i = 0; i < count; i++) {
			d = dest[i] - src[i];
			if (d < 0) {
				d = -d;
				mergeVector[i] = 1;
			} else
				mergeVector[i] = -1;
			mergeDelta[i] = d;
			if (d > mergeMajor)
				mergeMajor = d;
		}
		d = mergeMajor / 2;
		for (i = 0; i < count; i++)
			mergeError[i] = d;
		mergeIterations = mergeMajor;
		mergeInit = 0;
	}
	if (mergeIterations-- == 0) {
		delete[] mergeError;
		delete[] mergeDelta;
		delete[] mergeVector;
		mergeInit = 1;
		return 0;
	}
	for (i = 0; i < count; i++) {
		if ((mergeError[i] += mergeDelta[i]) > mergeMajor) {
			mergeError[i] -= mergeMajor;
			dest[i] += mergeVector[i];
		}
	}
	return 1;
}
