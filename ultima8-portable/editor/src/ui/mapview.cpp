#include "ui/mapview.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

// Below this zoom the view draws the overview image instead of every item.
static const qreal OverviewZoom = 0.3;
static const qreal MinZoom = 1.0 / 32, MaxZoom = 8;

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
	remakeOverview();
	fitMap();
}

void MapView::refresh()
{
	if (!render_.world())
		return;
	render_.build(*render_.world(), render_.mapNumber());
	remakeOverview();
	update();
}

void MapView::clear()
{
	render_ = MapRender();
	overview_ = QImage();
	selected_ = -1;
	stopPlacing();
	update();
	emit overviewChanged();
	emit viewChanged();
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
	// Ranges and labels are drawn over the view, not into the overview.
	old.eggRanges = o.eggRanges;
	old.labels = o.labels;
	if (old != o)
		remakeOverview();
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
		select(item);
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

// On screen, sx = x/4 - y/4 and sy = x/8 + y/8 - z: so a screen move at one height is
// x/4 = sy + sx/2 and y/4 = sy - sx/2.
void MapView::dragMove(QPointF delta, bool vertical, int &dx, int &dy, int &dz) const
{
	const QPointF d = delta / zoom_;
	dx = dy = dz = 0;
	if (vertical)
	{
		dz = int(std::lround(-d.y()));
		if (snap_ >= 8)
			dz = int(std::lround(dz / 8.0)) * 8;
		return;
	}
	dx = snapped(int(std::lround(4 * (d.y() + d.x() / 2))));
	dy = snapped(int(std::lround(4 * (d.y() - d.x() / 2))));
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
	const int under = render_.pick(m.toPoint(), options_);
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
	const bool movingItem = drag_ == MoveItem && moved_;
	if (zoom_ < OverviewZoom && !overview_.isNull())
	{
		p.setRenderHint(QPainter::SmoothPixmapTransform, true);
		p.drawImage(QRectF(render_.bounds()), overview_);
	}
	else
	{
		p.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 1);
		render_.paint(p, area, options_, movingItem ? selected_ : -1);
	}
	p.setRenderHint(QPainter::Antialiasing, true);
	render_.paintOverlays(p, area, options_, zoom_, movingItem ? -1 : selected_);
	if (movingItem)
	{
		MapItem moved = render_.world()->map(render_.mapNumber()).items[selected_];
		moved.x = uint16_t(std::clamp(moved.x + dragDx_, 0, 0x7fff));
		moved.y = uint16_t(std::clamp(moved.y + dragDy_, 0, 0x7fff));
		moved.z = uint8_t(std::clamp(moved.z + dragDz_, 0, 0xfa));
		render_.paintGhost(p, moved, 0.85, zoom_);
	}
	if (placing_ && placeValid_)
		render_.paintGhost(p, place_, 0.7, zoom_);
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
	dragDx_ = dragDy_ = dragDz_ = 0;
	drag_ = Pan;
	// A press on the selected item drags it.
	if (e->button() == Qt::LeftButton && selected_ >= 0 && render_.world() && zoom_ >= OverviewZoom &&
		render_.pick(toMap(e->position()).toPoint(), options_) == selected_)
		drag_ = MoveItem;
}

void MapView::mouseMoveEvent(QMouseEvent *e)
{
	const QPointF here = e->position();
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
		else
		{
			setCursor(Qt::SizeAllCursor);
			dragMove(here - pressAt_, e->modifiers() & Qt::ShiftModifier, dragDx_, dragDy_, dragDz_);
			emit status(tr("Move by %1, %2, %3 (Shift: up and down)").arg(dragDx_).arg(dragDy_).arg(dragDz_));
		}
		update();
		return;
	}
	if (!render_.world())
		return;
	const QPointF m = toMap(here);
	QString text;
	const int item = render_.pick(m.toPoint(), options_);
	if (item >= 0)
	{
		const MapItem &it = render_.world()->map(render_.mapNumber()).items[item];
		text = tr("%1 at %2, %3, %4").arg(render_.world()->describe(it)).arg(it.x).arg(it.y).arg(it.z);
	}
	else
	{
		const QPointF w = MapRender::toWorld(m, 0);
		text = tr("x %1, y %2 (at height 0)").arg(int(w.x())).arg(int(w.y()));
	}
	emit status(text);
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
			emit moveRequested(selected_, dx, dy, dz);
		return;
	}
	if (!moved_ && e->button() == Qt::LeftButton && render_.world())
	{
		const int item = render_.pick(toMap(e->position()).toPoint(), options_);
		select(item);
		emit itemClicked(item);
	}
}

void MapView::leaveEvent(QEvent *)
{
	if (placing_)
	{
		placeValid_ = false;
		update();
	}
}

void MapView::wheelEvent(QWheelEvent *e)
{
	const qreal steps = e->angleDelta().y() / 120.0;
	if (steps != 0)
		setZoom(zoom_ * std::pow(1.25, steps), e->position());
	if (placing_)
		updatePlacement(e->position());
}

void MapView::keyPressEvent(QKeyEvent *e)
{
	const bool ctrl = e->modifiers() & Qt::ControlModifier;
	const bool shift = e->modifiers() & Qt::ShiftModifier;
	const bool editing = selected_ >= 0 && !placing_;
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
	// Ctrl and the arrows move the selected item along the world's x and y, a snap step at a time;
	// Page Up and Down raise and lower it.
	if (editing && ctrl && (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right ||
		e->key() == Qt::Key_Up || e->key() == Qt::Key_Down))
	{
		const int s = std::max(1, snap_);
		switch (e->key())
		{
		case Qt::Key_Left: emit moveRequested(selected_, -s, 0, 0); break;
		case Qt::Key_Right: emit moveRequested(selected_, s, 0, 0); break;
		case Qt::Key_Up: emit moveRequested(selected_, 0, -s, 0); break;
		case Qt::Key_Down: emit moveRequested(selected_, 0, s, 0); break;
		}
		return;
	}
	if (editing && (e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown))
	{
		const int s = shift ? 1 : 8;
		emit moveRequested(selected_, 0, 0, e->key() == Qt::Key_PageUp ? s : -s);
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
