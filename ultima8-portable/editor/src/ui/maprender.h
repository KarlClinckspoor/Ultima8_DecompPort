// Draws one map the way the game's display list does: each item's frame at its screen position,
// in the order DL_ItemNode::compare gives for items that overlap (u8/dlasm.cpp).
#pragma once

#include <QImage>
#include <QPainter>
#include <QRect>
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

	bool operator==(const LayerOptions &o) const;
	bool operator!=(const LayerOptions &o) const { return !(*this == o); }
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
	// map screen pixels to the device.
	void paint(QPainter &p, const QRectF &area, const LayerOptions &o, int skipItem = -1) const;
	// One item drawn by itself where its fields put it, over the rest, with its outline.
	void paintGhost(QPainter &p, const MapItem &item, qreal opacity, qreal scale) const;
	// Egg ranges, labels and the selected item's outline; scale is device pixels per screen pixel.
	void paintOverlays(QPainter &p, const QRectF &area, const LayerOptions &o, qreal scale, int selectedItem) const;
	// The item whose drawn pixel is at screen point pt, or -1.
	int pick(QPoint pt, const LayerOptions &o) const;
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
	std::vector<int> candidates(const QRect &area) const;

	const World *world_ = nullptr;
	int map_ = -1;
	QRect bounds_;
	std::vector<Entry> entries_;		// in drawing order
	std::vector<int> entryOfItem_;
	int cellsX_ = 0, cellsY_ = 0;
	std::vector<std::vector<int>> cells_;	// entry numbers, ascending
};
