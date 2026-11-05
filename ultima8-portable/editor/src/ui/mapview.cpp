#include "ui/mapview.h"

#include <QApplication>
#include <QHelpEvent>
#include <QMenu>
#include <QToolTip>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "ui/names.h"

// Below this zoom the view draws the overview image instead of every item.
static const qreal OverviewZoom = 0.3;
static const qreal MinZoom = 1.0 / 32, MaxZoom = 8;
// The right-click list takes what is drawn within this many widget pixels of the pointer.
static const int PickRadius = 10;
static const int MaxPickList = 40;

MapView::MapView(QWidget *parent) : QWidget(parent)
{
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setMinimumSize(200, 150);
}

void MapView::openMap(const World &world, int map)
{
	render_.build(world, map);
	selected_ = -1;
	selection_.clear();
	related_.clear();
	hovered_ = highlight_ = -1;
	remakeOverview();
	fitMap();
	emit selectionChanged();
}

void MapView::refresh()
{
	if (!render_.world())
		return;
	render_.build(*render_.world(), render_.mapNumber());
	// What was deleted is no longer selected.
	const auto &items = render_.world()->map(render_.mapNumber()).items;
	auto gone = [&](int i) { return i < 0 || i >= int(items.size()) || !items[i].onMap(); };
	const int before = selection_.size();
	selection_.erase(std::remove_if(selection_.begin(), selection_.end(), gone), selection_.end());
	related_.erase(std::remove_if(related_.begin(), related_.end(), gone), related_.end());
	if (selected_ >= 0 && gone(selected_))
		selected_ = selection_.isEmpty() ? -1 : selection_.last();
	if (hovered_ >= 0 && gone(hovered_))
		hovered_ = -1;
	remakeOverview();
	update();
	if (selection_.size() != before)
		emit selectionChanged();
}

void MapView::clear()
{
	render_ = MapRender();
	overview_ = QImage();
	selected_ = -1;
	selection_.clear();
	related_.clear();
	hovered_ = highlight_ = -1;
	stopPlacing();
	update();
	emit overviewChanged();
	emit viewChanged();
	emit selectionChanged();
}

void MapView::remakeOverview()
{
	if (!render_.world())
		return;
	const QRect b = render_.bounds();
	// About 4096 pixels across at most.
	overviewScale_ = std::min<qreal>(0.25, 4096.0 / std::max(1, std::max(b.width(), b.height())));
	overview_ = render_.renderOverview(options_, overviewScale_);
	emit overviewChanged();
}

void MapView::setOptions(const LayerOptions &o)
{
	if (o == options_)
		return;
	LayerOptions old = options_;
	options_ = o;
	// Ranges, labels and heights are drawn over the view, not into the overview.
	old.eggRanges = o.eggRanges;
	old.labels = o.labels;
	old.heights = o.heights;
	if (old != o)
		remakeOverview();
	update();
}

void MapView::setXRay(const XRay &x)
{
	const QPointF c = xray_.center;
	xray_ = x;
	xray_.center = c;
	update();
}

void MapView::setZoom(qreal zoom, QPointF anchor)
{
	zoom = std::clamp(zoom, MinZoom, MaxZoom);
	if (anchor.x() < 0)
		anchor = QPointF(width() / 2.0, height() / 2.0);
	// Keep the map point under the anchor where it is.
	const QPointF before = toMap(anchor);
	zoom_ = zoom;
	const QPointF after = toMap(anchor);
	center_ += before - after;
	update();
	emit viewChanged();
}

void MapView::fitMap()
{
	const QRect b = render_.bounds();
	if (b.isEmpty())
		return;
	center_ = b.center();
	zoom_ = std::clamp(std::min(width() / qreal(b.width()), height() / qreal(b.height())) * 0.95, MinZoom, MaxZoom);
	update();
	emit viewChanged();
}

void MapView::centerOn(QPointF screen)
{
	center_ = screen;
	update();
	emit viewChanged();
}

void MapView::select(int item)
{
	selected_ = item;
	selection_.clear();
	if (item >= 0)
		selection_ << item;
	update();
	emit selectionChanged();
}

void MapView::setPrimary(int item)
{
	if (item >= 0 && selection_.contains(item))
	{
		selected_ = item;
		update();
		emit selectionChanged();
	}
	else
		select(item);
}

void MapView::setSelection(const QVector<int> &items, int primary)
{
	selection_.clear();
	for (int i : items)
		if (i >= 0 && !selection_.contains(i))
			selection_ << i;
	if (primary >= 0 && !selection_.contains(primary))
		selection_ << primary;
	selected_ = primary >= 0 ? primary : (selection_.isEmpty() ? -1 : selection_.last());
	update();
	emit selectionChanged();
}

void MapView::toggleSelected(int item)
{
	if (item < 0)
		return;
	if (selection_.removeAll(item))
	{
		if (selected_ == item)
			selected_ = selection_.isEmpty() ? -1 : selection_.last();
	}
	else
	{
		selection_ << item;
		selected_ = item;
	}
	update();
	emit selectionChanged();
}

void MapView::setRelated(const QVector<int> &items)
{
	related_ = items;
	update();
}

void MapView::setHighlight(int item)
{
	if (highlight_ == item)
		return;
	highlight_ = item;
	update();
}

void MapView::showItem(int item, bool sel)
{
	const int e = render_.entryOf(item);
	if (e >= 0)
	{
		const MapRender::Entry &en = render_.entries()[e];
		if (zoom_ < 1)
			zoom_ = 1;
		centerOn(QPointF(en.sx, en.sy - en.frame->yoff / 2.0));
	}
	if (sel)
		setPrimary(item);
}

void MapView::startPlacing(int shape, int frame)
{
	placing_ = true;
	placeValid_ = false;
	place_ = MapItem();
	place_.shape = uint16_t(shape);
	place_.frame = uint8_t(frame);
	setCursor(Qt::CrossCursor);
	updatePlacement(mapFromGlobal(QCursor::pos()));
	setFocus();
	update();
}

void MapView::stopPlacing()
{
	if (!placing_)
		return;
	placing_ = false;
	placeValid_ = false;
	unsetCursor();
	update();
	emit placingStopped();
}

QPointF MapView::toMap(QPointF w) const
{
	return center_ + (w - QPointF(width() / 2.0, height() / 2.0)) / zoom_;
}

QRectF MapView::visibleArea() const
{
	return QRectF(toMap(QPointF(0, 0)), toMap(QPointF(width(), height())));
}

int MapView::snapped(int v) const
{
	if (snap_ <= 1)
		return v;
	return int(std::lround(double(v) / snap_)) * snap_;
}

int MapView::pickAt(QPointF widget) const
{
	if (!render_.world())
		return -1;
	return render_.pick(toMap(widget).toPoint(), options_, activeXRay());
}

// On screen, sx = x/4 - y/4 and sy = x/8 + y/8 - z: so a screen move at one height is
// x/4 = sy + sx/2 and y/4 = sy - sx/2. Fine (Alt) moves ignore the snap.
void MapView::dragMove(QPointF delta, bool vertical, bool fine, int &dx, int &dy, int &dz) const
{
	const QPointF d = delta / zoom_;
	dx = dy = dz = 0;
	if (vertical)
	{
		dz = int(std::lround(-d.y()));
		if (snap_ >= 8 && !fine)
			dz = int(std::lround(dz / 8.0)) * 8;
		return;
	}
	const int x = int(std::lround(4 * (d.y() + d.x() / 2)));
	const int y = int(std::lround(4 * (d.y() - d.x() / 2)));
	dx = fine ? x : snapped(x);
	dy = fine ? y : snapped(y);
}

// A new item stands on what is under the mouse, or at height 0; its position is the corner of its
// footpad nearest the viewer, as items' positions are.
void MapView::updatePlacement(QPointF widget)
{
	placeValid_ = false;
	if (!placing_ || !render_.world() || !rect().contains(widget.toPoint()))
		return;
	const QPointF m = toMap(widget);
	int z = 0;
	const int under = render_.pick(m.toPoint(), options_, activeXRay());
	if (under >= 0)
	{
		const MapItem &it = render_.world()->map(render_.mapNumber()).items[under];
		z = std::min(250, it.z + render_.world()->shapes().type(it.shape).zSize * 8);
	}
	const QPointF w = MapRender::toWorld(m, z);
	auto align = [&](qreal v) {
		if (snap_ <= 1)
			return int(std::lround(v));
		return int(std::lround((v + 1) / snap_)) * snap_ - 1;
	};
	place_.x = uint16_t(std::clamp(align(w.x()), 0, 0x7fff));
	place_.y = uint16_t(std::clamp(align(w.y()), 0, 0x7fff));
	place_.z = uint8_t(z);
	placeValid_ = true;
	emit status(tr("Place shape %1:%2 at %3, %4, %5 (click to place, right-click or Esc to stop)")
		.arg(place_.shape).arg(place_.frame).arg(place_.x).arg(place_.y).arg(place_.z));
}

void MapView::draw(QPainter &p, QSize size)
{
	p.fillRect(QRect(QPoint(0, 0), size), QColor(12, 12, 16));
	if (!render_.world())
	{
		p.setPen(Qt::gray);
		p.drawText(QRect(QPoint(0, 0), size), Qt::AlignCenter, tr("Choose a map from the list."));
		return;
	}
	p.save();
	p.translate(size.width() / 2.0, size.height() / 2.0);
	p.scale(zoom_, zoom_);
	p.translate(-center_);
	const QRectF area(center_ - QPointF(size.width(), size.height()) / (2 * zoom_),
		QSizeF(size.width(), size.height()) / zoom_);
	const bool movingItems = drag_ == MoveItem && moved_;
	QSet<int> skip;
	if (movingItems)
		skip = QSet<int>(selection_.begin(), selection_.end());
	if (zoom_ < OverviewZoom && !overview_.isNull())
	{
		p.setRenderHint(QPainter::SmoothPixmapTransform, true);
		p.drawImage(QRectF(render_.bounds()), overview_);
	}
	else
	{
		p.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 1);
		render_.paint(p, area, options_, skip, activeXRay());
	}
	p.setRenderHint(QPainter::Antialiasing, true);
	Overlay ov;
	if (!movingItems)
	{
		ov.selected = selection_;
		ov.primary = selected_;
	}
	ov.hovered = drag_ == None ? hovered_ : -1;
	ov.highlight = highlight_;
	ov.related = related_;
	render_.paintOverlays(p, area, options_, zoom_, ov);
	if (movingItems)
	{
		const auto &items = render_.world()->map(render_.mapNumber()).items;
		for (int i : selection_)
		{
			MapItem moved = items[i];
			moved.x = uint16_t(std::clamp(moved.x + dragDx_, 0, 0x7fff));
			moved.y = uint16_t(std::clamp(moved.y + dragDy_, 0, 0x7fff));
			moved.z = uint8_t(std::clamp(moved.z + dragDz_, 0, 0xfa));
			render_.paintGhost(p, moved, 0.85, zoom_);
		}
	}
	if (placing_ && placeValid_)
		render_.paintGhost(p, place_, 0.7, zoom_);
	if (drag_ == Band && moved_)
	{
		p.setPen(QPen(QColor(255, 220, 80), 1 / zoom_, Qt::DashLine));
		p.setBrush(QColor(255, 220, 80, 30));
		p.drawRect(band_);
	}
	if (const XRay *x = activeXRay())
	{
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(QColor(255, 255, 255, 90), 1 / zoom_, Qt::DashLine));
		p.drawEllipse(x->center, x->radius, x->radius);
	}
	p.restore();
}

void MapView::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	draw(p, size());
}

QImage MapView::grabView()
{
	QImage img(size(), QImage::Format_ARGB32_Premultiplied);
	QPainter p(&img);
	draw(p, size());
	return img;
}

void MapView::mousePressEvent(QMouseEvent *e)
{
	setFocus();
	if (placing_)
	{
		if (e->button() == Qt::RightButton)
			stopPlacing();
		else if (e->button() == Qt::LeftButton)
		{
			updatePlacement(e->position());
			if (placeValid_)
				emit placeRequested(place_);
		}
		return;
	}
	moved_ = false;
	pressAt_ = e->position().toPoint();
	pressCenter_ = center_;
	pressButton_ = e->button();
	dragDx_ = dragDy_ = dragDz_ = 0;
	drag_ = Pan;
	if (e->button() != Qt::LeftButton || !render_.world())
		return;
	// Ctrl+drag draws a band to select what it holds; a press on a selected item drags the
	// selection.
	if (e->modifiers() & Qt::ControlModifier)
	{
		drag_ = Band;
		band_ = QRectF(toMap(e->position()), QSizeF());
	}
	else if (!selection_.isEmpty() && zoom_ >= OverviewZoom && selection_.contains(pickAt(e->position())))
		drag_ = MoveItem;
}

void MapView::mouseMoveEvent(QMouseEvent *e)
{
	const QPointF here = e->position();
	if (xray_.on)
	{
		xray_.center = toMap(here);
		xrayInside_ = true;
		update();
	}
	if (placing_)
	{
		updatePlacement(here);
		update();
		return;
	}
	if (drag_ != None)
	{
		if ((here.toPoint() - pressAt_).manhattanLength() > 3)
			moved_ = true;
		if (!moved_)
			return;
		if (drag_ == Pan)
		{
			setCursor(Qt::ClosedHandCursor);
			center_ = pressCenter_ - (here - pressAt_) / zoom_;
			emit viewChanged();
		}
		else if (drag_ == Band)
		{
			band_ = QRectF(toMap(QPointF(pressAt_)), toMap(here)).normalized();
			emit status(tr("Select what is in the box (Ctrl+drag)"));
		}
		else
		{
			setCursor(Qt::SizeAllCursor);
			dragMove(here - pressAt_, e->modifiers() & Qt::ShiftModifier, e->modifiers() & Qt::AltModifier,
				dragDx_, dragDy_, dragDz_);
			emit status(tr("Move %n item(s) by %1, %2, %3 (Shift: up and down; Alt: without the snap)", nullptr,
				int(selection_.size())).arg(dragDx_).arg(dragDy_).arg(dragDz_));
		}
		update();
		return;
	}
	if (!render_.world())
		return;
	const QPointF m = toMap(here);
	QString text;
	const int item = pickAt(here);
	if (item != hovered_)
	{
		hovered_ = item;
		update();
	}
	if (item >= 0)
	{
		const World &w = *render_.world();
		const MapItem &it = w.map(render_.mapNumber()).items[item];
		text = tr("%1 at %2, %3, %4 (top %5)").arg(Names::itemLabel(w, render_.mapNumber(), it))
			.arg(it.x).arg(it.y).arg(it.z).arg(it.z + w.shapes().type(it.shape).zSize * 8);
	}
	else
	{
		const QPointF w = MapRender::toWorld(m, 0);
		text = tr("x %1, y %2 (at height 0)").arg(int(w.x())).arg(int(w.y()));
	}
	emit status(text);
}

bool MapView::event(QEvent *e)
{
	if (e->type() != QEvent::ToolTip)
		return QWidget::event(e);
	auto *help = static_cast<QHelpEvent *>(e);
	const World *world = render_.world();
	const int item = world && drag_ == None ? pickAt(help->pos()) : -1;
	if (item < 0)
	{
		QToolTip::hideText();
		e->ignore();
		return true;
	}
	QToolTip::showText(help->globalPos(), Names::itemTooltip(*world, render_.mapNumber(), item), this);
	return true;
}

void MapView::showPickMenu(QPoint widget)
{
	const World *world = render_.world();
	if (!world)
		return;
	const int radius = std::max(1, int(std::lround(PickRadius / zoom_)));
	std::vector<int> items = render_.pickAll(toMap(QPointF(widget)).toPoint(), radius, options_);
	const auto &all = world->map(render_.mapNumber()).items;
	QMenu menu(this);
	menu.setToolTipsVisible(true);
	if (items.empty())
	{
		menu.addAction(tr("Nothing here"))->setEnabled(false);
		menu.exec(mapToGlobal(widget));
		return;
	}
	const int shown = std::min<int>(int(items.size()), MaxPickList);
	menu.addSection(items.size() > size_t(shown) ? tr("%1 of %2 items here (the topmost first)").arg(shown).arg(items.size())
		: tr("%n item(s) here (the topmost first)", nullptr, int(items.size())));
	for (int k = 0; k < shown; k++)
	{
		const int item = items[size_t(k)];
		const MapItem &it = all[item];
		QString text = tr("%1 \u2014 %2:%3  (%4, %5, %6)").arg(Names::itemName(*world, render_.mapNumber(), it))
			.arg(it.shape).arg(it.frame).arg(it.x).arg(it.y).arg(it.z);
		if (selection_.contains(item))
			text += tr("  [selected]");
		QAction *a = menu.addAction(text);
		if (const Frame *f = world->shapes().frame(it.shape, it.frame))
		{
			QImage icon(32, 32, QImage::Format_ARGB32_Premultiplied);
			icon.fill(Qt::transparent);
			QPainter ip(&icon);
			const QImage &img = it.flipped() ? f->flipped : f->image;
			const qreal s = std::min<qreal>(1, std::min(32.0 / f->width, 32.0 / f->height));
			const QSizeF size = QSizeF(f->width, f->height) * s;
			ip.setRenderHint(QPainter::SmoothPixmapTransform, true);
			ip.drawImage(QRectF(QPointF((32 - size.width()) / 2, (32 - size.height()) / 2), size), img);
			ip.end();
			a->setIcon(QIcon(QPixmap::fromImage(icon)));
		}
		a->setToolTip(Names::itemTooltip(*world, render_.mapNumber(), item));
		connect(a, &QAction::hovered, this, [this, item] { setHighlight(item); });
		connect(a, &QAction::triggered, this, [this, item] {
			if (QApplication::keyboardModifiers() & Qt::ControlModifier)
				toggleSelected(item);
			else
				select(item);
			emit itemClicked(selected_);
		});
	}
	if (items.size() > 1)
	{
		menu.addSeparator();
		QAction *all = menu.addAction(tr("Select All of These"));
		connect(all, &QAction::triggered, this, [this, items, shown] {
			setSelection(QVector<int>(items.begin(), items.begin() + shown), items.front());
			emit itemClicked(selected_);
		});
	}
	menu.addSection(tr("Ctrl+click an item to add it to the selection"));
	menu.exec(mapToGlobal(widget));
	setHighlight(-1);
}

void MapView::mouseReleaseEvent(QMouseEvent *e)
{
	if (drag_ == None)
		return;
	const Drag was = drag_;
	drag_ = None;
	unsetCursor();
	if (was == MoveItem && moved_)
	{
		const int dx = dragDx_, dy = dragDy_, dz = dragDz_;
		dragDx_ = dragDy_ = dragDz_ = 0;
		update();
		if (dx || dy || dz)
			emit moveRequested(dx, dy, dz);
		return;
	}
	if (was == Band)
	{
		if (moved_)
		{
			// Added to what was selected.
			QVector<int> sel = selection_;
			for (int i : render_.itemsIn(band_, options_))
				if (!sel.contains(i))
					sel << i;
			setSelection(sel, selected_ >= 0 ? selected_ : (sel.isEmpty() ? -1 : sel.first()));
			emit itemClicked(selected_);
			emit status(tr("%n item(s) selected", nullptr, int(selection_.size())));
		}
		else if (render_.world())
		{
			toggleSelected(pickAt(e->position()));
			emit itemClicked(selected_);
		}
		update();
		return;
	}
	if (moved_ || !render_.world())
		return;
	if (e->button() == Qt::LeftButton)
	{
		select(pickAt(e->position()));
		emit itemClicked(selected_);
	}
	else if (e->button() == Qt::RightButton)
		showPickMenu(e->position().toPoint());
}

void MapView::leaveEvent(QEvent *)
{
	if (placing_)
		placeValid_ = false;
	xrayInside_ = false;
	if (hovered_ >= 0)
		hovered_ = -1;
	update();
}

void MapView::wheelEvent(QWheelEvent *e)
{
	const qreal steps = e->angleDelta().y() / 120.0;
	if (steps != 0)
		setZoom(zoom_ * std::pow(1.25, steps), e->position());
	if (xray_.on)
		xray_.center = toMap(e->position());
	if (placing_)
		updatePlacement(e->position());
}

void MapView::keyPressEvent(QKeyEvent *e)
{
	const bool ctrl = e->modifiers() & Qt::ControlModifier;
	const bool shift = e->modifiers() & Qt::ShiftModifier;
	const bool alt = e->modifiers() & Qt::AltModifier;
	const bool editing = !selection_.isEmpty() && !placing_;
	const bool arrow = e->key() == Qt::Key_Left || e->key() == Qt::Key_Right || e->key() == Qt::Key_Up ||
		e->key() == Qt::Key_Down;
	if (e->key() == Qt::Key_Escape)
	{
		if (placing_)
			stopPlacing();
		else
		{
			select(-1);
			emit itemClicked(-1);
		}
		return;
	}
	// Ctrl and the arrows move the selection along the world's x and y, a snap step at a time;
	// Alt and the arrows by one unit. Page Up and Down raise and lower it (Shift or Alt: by one).
	if (editing && (ctrl || alt) && arrow)
	{
		const int s = alt ? 1 : std::max(1, snap_);
		switch (e->key())
		{
		case Qt::Key_Left: emit moveRequested(-s, 0, 0); break;
		case Qt::Key_Right: emit moveRequested(s, 0, 0); break;
		case Qt::Key_Up: emit moveRequested(0, -s, 0); break;
		case Qt::Key_Down: emit moveRequested(0, s, 0); break;
		}
		return;
	}
	if (editing && (e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown))
	{
		const int s = shift || alt ? 1 : 8;
		emit moveRequested(0, 0, e->key() == Qt::Key_PageUp ? s : -s);
		return;
	}
	if (ctrl && e->key() == Qt::Key_A && render_.world())
	{
		// Everything shown in the view.
		const std::vector<int> all = render_.itemsIn(visibleArea(), options_);
		setSelection(QVector<int>(all.begin(), all.end()), all.empty() ? -1 : all.front());
		emit itemClicked(selected_);
		emit status(tr("%n item(s) selected", nullptr, int(selection_.size())));
		return;
	}
	if (editing && !ctrl)
	{
		switch (e->key())
		{
		case Qt::Key_Delete:
		case Qt::Key_Backspace: emit actionRequested(Delete); return;
		case Qt::Key_F: emit actionRequested(Flip); return;
		case Qt::Key_BracketRight: emit actionRequested(NextFrame); return;
		case Qt::Key_BracketLeft: emit actionRequested(PrevFrame); return;
		}
	}
	if (editing && ctrl && e->key() == Qt::Key_D)
	{
		emit actionRequested(Duplicate);
		return;
	}
	const qreal step = 120 / zoom_;
	switch (e->key())
	{
	case Qt::Key_Left: centerOn(center_ + QPointF(-step, 0)); break;
	case Qt::Key_Right: centerOn(center_ + QPointF(step, 0)); break;
	case Qt::Key_Up: centerOn(center_ + QPointF(0, -step)); break;
	case Qt::Key_Down: centerOn(center_ + QPointF(0, step)); break;
	case Qt::Key_Plus:
	case Qt::Key_Equal: zoomBy(1.25); break;
	case Qt::Key_Minus: zoomBy(0.8); break;
	case Qt::Key_1: setZoom(1); break;
	case Qt::Key_0: fitMap(); break;
	default: QWidget::keyPressEvent(e); return;
	}
}

void MapView::resizeEvent(QResizeEvent *)
{
	emit viewChanged();
}
