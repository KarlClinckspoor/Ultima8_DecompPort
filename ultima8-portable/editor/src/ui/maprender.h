// Draws one map the way the game's display list does: each item's frame at its screen position,
// in the order DL_ItemNode::compare gives for items that overlap (u8/dlasm.cpp).
#pragma once

#include <QImage>
#include <QPainter>
#include <QRect>
#include <QSet>
#include <QVector>

#include <vector>

#include "data/world.h"

// What the views show.
struct LayerOptions
{
	bool fixed = true;
	bool nonFixed = true;
	bool globs = true;
	bool npcs = true;
	bool editorItems = false;	// eggs and the other shapes the game only shows its editor
	bool invisible = false;		// items with the invisible flag, drawn faintly
	bool eggRanges = true;		// with editorItems: where each egg hatches
	bool labels = true;			// with editorItems: what each egg does, when zoomed in
	int roof = 255;				// items without the draw flag show only below this height
	qreal opacity[4] = {1, 1, 1, 1};	// of fixed, movable, glob and NPC items (MapItem::Source)
	bool heights = false;		// the bottom and top of the selected and pointed at items

	bool operator==(const LayerOptions &o) const;
	bool operator!=(const LayerOptions &o) const { return !(*this == o); }
};

// Seeing through: in a circle around the pointer, what is at or above a height is faded (or,
// at opacity 0, left out), as the game's roof hides what is above the Avatar.
struct XRay
{
	bool on = false;
	QPointF center;		// screen pixels of the map
	qreal radius = 96;	// screen pixels of the map
	int height = 24;
	qreal opacity[4] = {0.15, 0.15, 0.15, 0.15};	// of fixed, movable, glob and NPC items
};

// What is drawn over the map besides eggs' ranges and labels.
struct Overlay
{
	QVector<int> selected;		// outlined
	int primary = -1;			// the one the Item window shows
	int hovered = -1;			// under the pointer
	int highlight = -1;			// named in a list being looked at
	QVector<int> related;		// the doors a key opens, the keys of a door, an egg's partner
};

class MapRender
{
public:
	struct Entry
	{
		int item = -1;
		int sx = 0, sy = 0;		// the hot spot, in screen pixels
		QRect rect;				// the frame's pixels on screen
		const Frame *frame = nullptr;
		bool flip = false;
		uint8_t family = 0;
		bool editor = false;
		bool invisible = false;
		bool trans = false;
		bool draw = false;
		// Footpad in display list units (world / 4), for ordering and outlines.
		int x = 0, y = 0, z = 0, xd = 0, yd = 0, zd = 0;
		bool unknown1 = false;
	};

	void build(const World &world, int map);
	int mapNumber() const { return map_; }
	const World *world() const { return world_; }
	QRect bounds() const { return bounds_; }
	const std::vector<Entry> &entries() const { return entries_; }
	// The entry drawing item, or -1.
	int entryOf(int item) const;

	bool shows(const Entry &e, const LayerOptions &o) const;
	// Draws the items whose pixels meet area (screen pixels) onto a painter already set up to
	// map screen pixels to the device, but those in skip.
	void paint(QPainter &p, const QRectF &area, const LayerOptions &o, const QSet<int> &skip = QSet<int>(),
		const XRay *xray = nullptr) const;
	// One item drawn by itself where its fields put it, over the rest, with its outline.
	void paintGhost(QPainter &p, const MapItem &item, qreal opacity, qreal scale) const;
	// Egg ranges and labels, the selected items' outlines, heights, and the lines from teleport
	// eggs to where they lead on this map; scale is device pixels per screen pixel.
	void paintOverlays(QPainter &p, const QRectF &area, const LayerOptions &o, qreal scale, const Overlay &ov) const;
	// The item whose drawn pixel is at screen point pt, or -1. What the X-ray all but hides is
	// passed over.
	int pick(QPoint pt, const LayerOptions &o, const XRay *xray = nullptr) const;
	// Every item drawn at pt, the topmost first, then those drawn within radius of it.
	std::vector<int> pickAll(QPoint pt, int radius, const LayerOptions &o) const;
	// Items whose hot spot is inside area (screen pixels), as a rubber band selects them.
	std::vector<int> itemsIn(const QRectF &area, const LayerOptions &o) const;
	// How opaque an entry is drawn: its layer's opacity, faint for the invisible and translucent,
	// and the X-ray's fade when it covers the entry at pt (pass a point outside the circle for
	// none).
	qreal opacityOf(const Entry &e, const LayerOptions &o, const XRay *xray = nullptr, QPointF pt = QPointF(-1e9, -1e9)) const;
	// The same-map transitions (teleport eggs, usecode) that start or end at item, as
	// (from, to) item pairs.
	QVector<QPair<int, int>> links(int item) const;
	// The whole map at the given scale. Safe to call from another thread.
	QImage renderOverview(const LayerOptions &o, qreal scale) const;

	// Screen position of a world point (DL_ItemNode::update).
	static QPointF toScreen(qreal x, qreal y, qreal z);
	// The world point on height z that is drawn at a screen point.
	static QPointF toWorld(QPointF screen, qreal z);

private:
	enum { Cell = 256 };
	void sort();
	void index();
	void paintHeights(QPainter &p, const Entry &e, qreal scale) const;
	std::vector<int> candidates(const QRect &area) const;

	const World *world_ = nullptr;
	int map_ = -1;
	QRect bounds_;
	std::vector<Entry> entries_;		// in drawing order
	std::vector<int> entryOfItem_;
	int cellsX_ = 0, cellsY_ = 0;
	std::vector<std::vector<int>> cells_;	// entry numbers, ascending
};
