#ifndef CAMERA_H
#define CAMERA_H

#include "u8port.h"

#include "u8process.h"
#include "basecam.h"
#include "dlist.h"

class BaseFile;
class FastItem;
class Item;
class Vport;
class Zbuffer;

// The game view. _h and _v are the screen position of the view's corner,
// _x, _y, _z the world point it is centred on.
class Camera : public BaseCamera
{
public:
	Camera(void);
	virtual void show(void);
	virtual void show(int16_t, int16_t, int16_t, int16_t, uint8_t);
	virtual void slam(void);
	virtual void initPalette(void);
	static void add_items_to_list(void);
	static void renumber_shadow_squares(int16_t, int16_t);
	static void move_to(uint16_t, uint16_t, uint8_t, int16_t);
	static int16_t scrollTo(uint16_t, uint16_t, uint8_t, int16_t);
	static void drawBounds(Vport *);
	static void position(void);
	static void startQuake(int16_t);
	static void stopQuake(void);
	static void slamQuake(void);
	static void invertScreen(uint8_t);
	static void setScreenInverted(uint8_t);
	static void setCenterOn(uint16_t);
	static void move_rel(int16_t, int16_t, int16_t);
	static void set_roof(int16_t);
	static void find_new_roof(Item);
	static uint8_t roof_item_moved(void);
	static void init(void);
	static uint16_t getX(void);
	static uint16_t getY(void);
	static uint8_t getZ(void);
	static int16_t roof(void);
	static int16_t _h;
	static int16_t _v;
	static uint8_t _moved;
	static uint8_t _drawn;
	static uint8_t lastShowFootpads;
	static int16_t _roof;
	static uint16_t roof_item_x;
	static uint16_t roof_item_y;
	static uint8_t roof_item_z;
	static uint16_t centeredOn;
	static uint8_t needAnim;
	static uint16_t globBounds;
	static uint16_t globBoundsX;
	static uint16_t globBoundsY;
	static char quakeMagnitude;
	static char quakeX;
	static char quakeY;
	static char quakeDX;
	static char quakeDY;
	static uint8_t displayListCount;
	static uint8_t disableDrawBit;
	static uint8_t drawUgliKolur;
	static uint8_t screenInverted;
	static uint8_t skipFrames;
	static ScreenArea _validarea;	// part of the screen still showing the right picture
	static uint16_t _x;
	static uint16_t _y;
	static uint8_t _z;
	static uint8_t catchingAvatar;
};

class CameraProcess : public Process
{
public:
	CameraProcess(void);
	virtual void process(void);
	virtual void fail(int32_t);
	virtual void load(BaseFile *);
	virtual void save(BaseFile *);
	virtual void postLoad(void);
};

// Scrolls the camera to a world point, speeding up and slowing down.
class ScrollProcessPlusPlus : public Process
{
public:
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t destX;
	int16_t destY;
	int16_t destZ;
	int16_t step;
	int16_t steps;
	int16_t errX;
	int16_t errY;
	int16_t errZ;
	int16_t dx;
	int16_t dy;
	int16_t dz;
	int16_t sx;
	int16_t sy;
	int16_t sz;
	int16_t maxSpeed;
	int16_t speed;

	ScrollProcessPlusPlus(uint16_t, uint16_t, uint8_t, int16_t);
	virtual void process(void);
};

// Scrolls the screen in a line to a world point, Bresenham style.
class ScrollProcess : public Process
{
public:
	int16_t dx;
	int16_t dy;
	int16_t incStraight;
	int16_t incDiagonal;
	int16_t error;
	int16_t destX;
	int16_t destY;
	int16_t x;
	int16_t y;
	int16_t speed;
	uint8_t octant;

	ScrollProcess(uint16_t, uint16_t, uint8_t, int16_t);
	virtual void process(void);
};

void initU8Camera(void);
void ScreenToWorldCoords(int16_t, int16_t, uint16_t &, uint16_t &);
void ScreenToWorldCoords(int16_t, int16_t, uint16_t &, uint16_t &, uint8_t &, FastItem *, uint8_t *);
void WorldToScreenCoords(uint16_t, uint16_t, int16_t &, int16_t &);
void SwapInt(int16_t *, int16_t *);
void Line(Vport *, int16_t, int16_t, int16_t, int16_t, uint8_t);

extern uint8_t hideEditorItems;
extern Camera *theCamera;
extern CameraProcess *theCameraProcess;
extern Zbuffer theZbuffer;

#endif
