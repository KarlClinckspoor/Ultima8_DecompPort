#include "ui/maprender.h"

#include <QFontMetrics>
#include <QPainterPath>

#include <algorithm>

bool LayerOptions::operator==(const LayerOptions &o) const
{
	return fixed == o.fixed && nonFixed == o.nonFixed && globs == o.globs && npcs == o.npcs &&
		editorItems == o.editorItems && invisible == o.invisible && eggRanges == o.eggRanges &&
		labels == o.labels && roof == o.roof;
}

QPointF MapRender::toScreen(qreal x, qreal y, qreal z)
{
	const qreal x4 = x / 4, y4 = y / 4;
	return QPointF(x4 - y4, x4 / 2 + y4 / 2 - z);
}

QPointF MapRender::toWorld(QPointF s, qreal z)
{
	// sx = x4 - y4, sy + z = (x4 + y4) / 2
	const qreal sum = (s.y() + z) * 2;
	const qreal x4 = (sum + s.x()) / 2, y4 = (sum - s.x()) / 2;
	return QPointF(x4 * 4, y4 * 4);
}

void MapRender::build(const World &world, int map)
{
	world_ = &world;
	map_ = map;
	entries_.clear();
	const MapData &m = world.map(map);
	const Shapes &shapes = world.shapes();
	entryOfItem_.assign(m.items.size(), -1);
	for (size_t i = 0; i < m.items.size(); i++)
	{
		const MapItem &it = m.items[i];
		if (!it.onMap())
			continue;
		const Frame *f = shapes.frame(it.shape, it.frame);
		if (!f)
			f = shapes.frame(0, 0);
		if (!f)
			continue;
		const TypeInfo &t = shapes.type(it.shape);
		Entry e;
		e.item = int(i);
		e.frame = f;
		e.flip = it.flipped();
		e.family = t.family;
		e.editor = t.editor;
		e.invisible = it.flags & FlagInvisible;
		e.trans = t.trans;
		e.draw = t.draw;
		e.unknown1 = t.unknown1;
		e.x = it.x >> 2;
		e.y = it.y >> 2;
		e.z = it.z;
		// getFootpad: a quarter of world x and y, x and y swapped when flipped.
		int xd = t.xSize * 8, yd = t.ySize * 8;
		if (e.flip)
			std::swap(xd, yd);
		e.xd = xd;
		e.yd = yd;
		e.zd = t.zSize * 8;
		if (e.zd == 0 && !t.fixed)
			e.zd = 1;
		e.sx = e.x - e.y;
		e.sy = (e.x >> 1) + (e.y >> 1) - e.z;
		e.rect = QRect(e.sx + f->left(e.flip), e.sy + f->top(), f->width, f->height);
		entries_.push_back(e);
	}
	bounds_ = QRect();
	for (const Entry &e : entries_)
		bounds_ = bounds_.united(e.rect);
	sort();
	index();
}

int MapRender::entryOf(int item) const
{
	if (item < 0 || item >= int(entryOfItem_.size()))
		return -1;
	return entryOfItem_[item];
}

// DL_ItemNode::compare: 1 when a is drawn over b, 2 when under, 0 when their outlines miss.
static int compare(const MapRender::Entry &a, const MapRender::Entry &b)
{
	const int myXZ = a.x - a.z;
	if (myXZ - a.xd - a.zd >= b.x - b.z)
		return 0;
	if (myXZ <= (b.x - b.z) - b.xd - b.zd)
		return 0;
	const int myYZ = a.y - a.z, itsYZ = b.y - b.z;
	if (myYZ - a.yd - a.zd >= itsYZ)
		return 0;
	if (myYZ <= itsYZ - b.yd - b.zd)
		return 0;
	const int itsScreenX = b.x - b.y;
	if (a.sx - a.xd >= itsScreenX + b.yd)
		return 0;
	if (a.sx + a.yd <= itsScreenX - b.xd)
		return 0;

	if (a.x - a.xd >= b.x)
		return 1;
	if (a.x <= b.x - b.xd)
		return 2;
	if (a.y - a.yd >= b.y)
		return 1;
	if (a.y <= b.y - b.yd)
		return 2;

	const int itsTop = b.z + b.zd, myTop = a.z + a.zd;
	if (a.z >= itsTop && myTop > b.z)
		return 1;
	if (myTop <= b.z && a.z < itsTop)
		return 2;

	if (a.unknown1 && !b.unknown1)
		return 1;
	if (b.unknown1 && !a.unknown1)
		return 2;

	const int mySum = a.x - a.xd + a.y - a.yd + a.z + a.z;
	const int itsSum = b.x - b.xd + b.y - b.yd + b.z + b.z;
	if (mySum > itsSum)
		return 1;
	if (mySum < itsSum)
		return 2;
	return 0;
}

// The game keeps a graph of which overlapping item goes over which and draws it lower items
// first. Here the graph is built once for the whole map and walked to one drawing order.
void MapRender::sort()
{
	const int n = int(entries_.size());
	if (n == 0)
		return;
	// Items whose frames meet on screen, found through a grid of cells.
	const int cx = (bounds_.width() + Cell - 1) / Cell, cy = (bounds_.height() + Cell - 1) / Cell;
	std::vector<std::vector<int>> grid(size_t(cx) * cy);
	auto cellRange = [&](const QRect &r, int &x0, int &y0, int &x1, int &y1) {
		x0 = (r.left() - bounds_.left()) / Cell;
		y0 = (r.top() - bounds_.top()) / Cell;
		x1 = (r.right() - bounds_.left()) / Cell;
		y1 = (r.bottom() - bounds_.top()) / Cell;
	};
	for (int i = 0; i < n; i++)
	{
		int x0, y0, x1, y1;
		cellRange(entries_[i].rect, x0, y0, x1, y1);
		for (int y = y0; y <= y1; y++)
			for (int x = x0; x <= x1; x++)
				grid[size_t(y) * cx + x].push_back(i);
	}
	std::vector<std::vector<int>> below(static_cast<size_t>(n));	// below[i]: drawn before i
	for (int c = 0; c < cx * cy; c++)
	{
		const std::vector<int> &list = grid[c];
		const int gx = c % cx, gy = c / cx;
		for (size_t a = 0; a < list.size(); a++)
		{
			const Entry &ea = entries_[list[a]];
			for (size_t b = a + 1; b < list.size(); b++)
			{
				const Entry &eb = entries_[list[b]];
				const QRect common = ea.rect.intersected(eb.rect);
				if (common.isEmpty())
					continue;
				// Each pair once: in the cell holding the top left of where they meet.
				if ((common.left() - bounds_.left()) / Cell != gx || (common.top() - bounds_.top()) / Cell != gy)
					continue;
				int order = compare(ea, eb);
				if (order == 1)
					below[list[a]].push_back(list[b]);
				else if (order == 2)
					below[list[b]].push_back(list[a]);
			}
		}
	}

	// Depth first, lower items first; an item met again while its own walk is open is a loop,
	// which the game would also have had to break somewhere.
	std::vector<int> start(static_cast<size_t>(n));
	for (int i = 0; i < n; i++)
		start[i] = i;
	auto key = [&](int i) {
		const Entry &e = entries_[i];
		return e.x - e.xd + e.y - e.yd + 2 * e.z;
	};
	std::stable_sort(start.begin(), start.end(), [&](int a, int b) { return key(a) < key(b); });
	std::vector<uint8_t> state(size_t(n), 0);	// 0 new, 1 open, 2 done
	std::vector<int> order;
	order.reserve(size_t(n));
	std::vector<std::pair<int, size_t>> stack;
	for (int s : start)
	{
		if (state[s])
			continue;
		stack.push_back({s, 0});
		state[s] = 1;
		while (!stack.empty())
		{
			auto &[node, next] = stack.back();
			if (next < below[node].size())
			{
				int d = below[node][next++];
				if (state[d] == 0)
				{
					state[d] = 1;
					stack.push_back({d, 0});
				}
				continue;
			}
			state[node] = 2;
			order.push_back(node);
			stack.pop_back();
		}
	}
	std::vector<Entry> sorted;
	sorted.reserve(size_t(n));
	for (int i : order)
		sorted.push_back(entries_[i]);
	entries_.swap(sorted);
}

void MapRender::index()
{
	for (size_t i = 0; i < entries_.size(); i++)
		entryOfItem_[entries_[i].item] = int(i);
	cellsX_ = (bounds_.width() + Cell - 1) / Cell;
	cellsY_ = (bounds_.height() + Cell - 1) / Cell;
	cells_.assign(size_t(std::max(0, cellsX_ * cellsY_)), {});
	for (size_t i = 0; i < entries_.size(); i++)
	{
		const QRect &r = entries_[i].rect;
		const int x0 = (r.left() - bounds_.left()) / Cell, y0 = (r.top() - bounds_.top()) / Cell;
		const int x1 = (r.right() - bounds_.left()) / Cell, y1 = (r.bottom() - bounds_.top()) / Cell;
		for (int y = y0; y <= y1; y++)
			for (int x = x0; x <= x1; x++)
				cells_[size_t(y) * cellsX_ + x].push_back(int(i));
	}
}

std::vector<int> MapRender::candidates(const QRect &area) const
{
	std::vector<int> out;
	const QRect a = area.intersected(bounds_);
	if (a.isEmpty())
		return out;
	const int x0 = (a.left() - bounds_.left()) / Cell, y0 = (a.top() - bounds_.top()) / Cell;
	const int x1 = (a.right() - bounds_.left()) / Cell, y1 = (a.bottom() - bounds_.top()) / Cell;
	for (int y = y0; y <= y1; y++)
		for (int x = x0; x <= x1; x++)
		{
			const auto &cell = cells_[size_t(y) * cellsX_ + x];
			out.insert(out.end(), cell.begin(), cell.end());
		}
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
	return out;
}

bool MapRender::shows(const Entry &e, const LayerOptions &o) const
{
	const MapItem &it = world_->map(map_).items[e.item];
	switch (it.source)
	{
	case MapItem::Fixed: if (!o.fixed) return false; break;
	case MapItem::NonFixed: if (!o.nonFixed) return false; break;
	case MapItem::Glob: if (!o.globs) return false; break;
	case MapItem::Npc: if (!o.npcs) return false; break;
	}
	if (e.editor && !o.editorItems)
		return false;
	if (e.invisible && !o.invisible)
		return false;
	// Camera::_roof: what is not always drawn shows only below the roof height.
	if (!e.draw && e.z >= o.roof)
		return false;
	return true;
}

static void drawEntry(QPainter &p, const MapRender::Entry &e)
{
	const QImage &img = e.flip ? e.frame->flipped : e.frame->image;
	if (e.invisible || e.trans)
	{
		// The game recolours what is under these through tables (XFORMPAL.DAT); a faint
		// image is the nearest thing here.
		p.setOpacity(e.invisible ? 0.35 : 0.6);
		p.drawImage(e.rect.topLeft(), img);
		p.setOpacity(1);
	}
	else
		p.drawImage(e.rect.topLeft(), img);
}

void MapRender::paint(QPainter &p, const QRectF &area, const LayerOptions &o) const
{
	for (int i : candidates(area.toAlignedRect()))
	{
		const Entry &e = entries_[i];
		if (shows(e, o) && e.rect.intersects(area.toAlignedRect()))
			drawEntry(p, e);
	}
}

QImage MapRender::renderOverview(const LayerOptions &o, qreal scale) const
{
	QSize size = (QSizeF(bounds_.size()) * scale).toSize().expandedTo(QSize(1, 1));
	QImage img(size, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::black);
	QPainter p(&img);
	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	p.scale(scale, scale);
	p.translate(-bounds_.topLeft());
	for (const Entry &e : entries_)
		if (shows(e, o))
			drawEntry(p, e);
	return img;
}

int MapRender::pick(QPoint pt, const LayerOptions &o) const
{
	std::vector<int> list = candidates(QRect(pt, QSize(1, 1)));
	for (auto it = list.rbegin(); it != list.rend(); ++it)
	{
		const Entry &e = entries_[*it];
		if (!shows(e, o) || !e.rect.contains(pt))
			continue;
		if (e.frame->hit(pt.x() - e.sx, pt.y() - e.sy, e.flip))
			return e.item;
	}
	return -1;
}

static QColor eggColor(const MapItem &it, int family)
{
	switch (family)
	{
	case FamilyTeleportEgg: return EggInfo::sends(it) ? QColor(255, 150, 30) : QColor(80, 220, 120);
	case FamilyMonsterEgg: return QColor(240, 70, 70);
	case FamilyUsecodeEgg: return QColor(190, 110, 255);
	case FamilyGlobEgg: return QColor(90, 170, 255);
	}
	return QColor(220, 220, 220);
}

// The footpad's box: the game's showFootpads outline (DL_ItemNode::draw).
static void outlineBox(QPainter &p, const MapRender::Entry &e)
{
	const QPointF s(e.sx, e.sy);
	const qreal xd = e.xd, yd = e.yd, zd = e.zd;
	auto at = [&](qreal dx, qreal dy, qreal dz) { return s + QPointF(dx, dy - dz); };
	const QPointF b0 = at(0, 0, 0), bx = at(-xd, -xd / 2, 0), by = at(yd, -yd / 2, 0);
	const QPointF t0 = at(0, 0, zd), tx = at(-xd, -xd / 2, zd), ty = at(yd, -yd / 2, zd);
	const QPointF txy = at(-xd + yd, -xd / 2 - yd / 2, zd);
	p.drawLine(b0, bx);
	p.drawLine(b0, by);
	p.drawLine(b0, t0);
	p.drawLine(by, ty);
	p.drawLine(bx, tx);
	p.drawLine(t0, ty);
	p.drawLine(t0, tx);
	p.drawLine(txy, ty);
	p.drawLine(txy, tx);
}

void MapRender::paintOverlays(QPainter &p, const QRectF &area, const LayerOptions &o, qreal scale, int selectedItem) const
{
	const MapData &m = world_->map(map_);
	const qreal pen = 1.0 / scale;
	if (o.editorItems && (o.eggRanges || o.labels))
	{
		QFont font = p.font();
		font.setPixelSize(std::max(1, int(11 / scale)));
		p.setFont(font);
		const QRect wide = area.toAlignedRect().adjusted(-600, -400, 600, 400);
		for (int i : candidates(wide))
		{
			const Entry &e = entries_[i];
			if (!(e.family == FamilyTeleportEgg || e.family == FamilyMonsterEgg ||
				e.family == FamilyUsecodeEgg || e.family == FamilyGlobEgg) || !shows(e, o))
				continue;
			const MapItem &it = m.items[e.item];
			const QColor c = eggColor(it, e.family);
			if (o.eggRanges && e.family != FamilyGlobEgg)
			{
				// EggHatcher::process: the avatar is inside when within range * 32 either side.
				const qreal rx = EggInfo::xRange(it) * 32, ry = EggInfo::yRange(it) * 32;
				QPolygonF poly;
				poly << toScreen(it.x - rx, it.y - ry, it.z) << toScreen(it.x + rx, it.y - ry, it.z)
					<< toScreen(it.x + rx, it.y + ry, it.z) << toScreen(it.x - rx, it.y + ry, it.z);
				QColor fill = c;
				fill.setAlpha(40);
				p.setBrush(fill);
				p.setPen(QPen(c, pen));
				p.drawPolygon(poly);
			}
			// Glob eggs are everywhere (each stands for a patch of floor), so they are named only
			// close up.
			if (o.labels && scale >= (e.family == FamilyGlobEgg ? 2.0 : 0.75))
			{
				QString text;
				const World &w = *world_;
				switch (e.family)
				{
				case FamilyTeleportEgg:
					text = EggInfo::sends(it)
						? QStringLiteral("→ map %1 #%2").arg(EggInfo::destMap(it)).arg(EggInfo::teleportId(it))
						: QStringLiteral("◎ #%1").arg(EggInfo::teleportId(it));
					break;
				case FamilyMonsterEgg:
				{
					QString name = w.usecode().className(EggInfo::monsterShape(it));
					text = QStringLiteral("monster %1").arg(name.isEmpty() ? QString::number(EggInfo::monsterShape(it)) : name);
					break;
				}
				case FamilyUsecodeEgg:
					text = w.usecode().className(w.classOf(it));
					if (text.isEmpty())
						text = QStringLiteral("egg %1").arg(it.quality);
					break;
				case FamilyGlobEgg:
					text = QStringLiteral("glob %1").arg(it.quality);
					break;
				}
				const QPointF at(e.sx, e.sy - e.frame->yoff - 3 / scale);
				QFontMetricsF fm(p.font());
				QRectF box(at.x() - fm.horizontalAdvance(text) / 2 - 2 / scale, at.y() - fm.height(),
					fm.horizontalAdvance(text) + 4 / scale, fm.height());
				p.setPen(Qt::NoPen);
				p.setBrush(QColor(0, 0, 0, 170));
				p.drawRect(box);
				p.setPen(c);
				p.drawText(box, Qt::AlignCenter, text);
			}
		}
	}
	const int sel = entryOf(selectedItem);
	if (sel >= 0)
	{
		const Entry &e = entries_[sel];
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(QColor(255, 255, 0), 2 * pen));
		outlineBox(p, e);
		p.setPen(QPen(QColor(255, 255, 0, 160), pen, Qt::DashLine));
		p.drawRect(QRectF(e.rect).adjusted(-1, -1, 1, 1));
	}
}
