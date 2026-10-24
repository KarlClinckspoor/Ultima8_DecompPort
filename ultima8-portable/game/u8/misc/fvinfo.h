#ifndef FVINFO_H
#define FVINFO_H

#include "u8port.h"

// SkipDraw and SkipTransformDraw, declared here before, are with the other
// shape drawers.
#include "skipdraw.h"

class Point;
class Vport;

int16_t GetMaxShapeWidth(void *);
int16_t GetMaxShapeDepth(void *);
int16_t GetNumShapes(void *);
void *GetShapeAddress(void *, int16_t);
void GetShapeInfo(char *, int16_t, int16_t &, int16_t &, int16_t &, int16_t &);
void TransformViewport(Vport);
void Scale(void *);
uint8_t figureDiamond(Point, Point, Point &, Point &, Point &, Point &);
void DrawDiamond(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);
void DrawFilledDiamond(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);

#endif
