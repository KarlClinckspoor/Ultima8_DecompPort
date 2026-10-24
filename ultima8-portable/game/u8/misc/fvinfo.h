#ifndef FVINFO_H
#define FVINFO_H

#include "u8port.h"

class Point;
class Vport;

extern "C" int16_t GetMaxShapeWidth(void *);
extern "C" int16_t GetMaxShapeDepth(void *);
extern "C" int16_t GetNumShapes(void *);
extern "C" void *GetShapeAddress(void *, int16_t);
extern "C" int16_t SkipDraw(Vport *, int16_t, int16_t, void *);
extern "C" int16_t SkipTransformDraw(Vport *, int16_t, int16_t, void *, void *);
void GetShapeInfo(char *, int16_t, int16_t &, int16_t &, int16_t &, int16_t &);
extern "C" void TransformViewport(Vport);
extern "C" void Scale(void *);
uint8_t figureDiamond(Point, Point, Point &, Point &, Point &, Point &);
void DrawDiamond(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);
void DrawFilledDiamond(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);

#endif
