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

void MapView::clear()
{
	render_ = MapRender();
	overview_ = QImage();
	selected_ = -1;
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

QPointF MapView::toMap(QPointF w) const
{
	return center_ + (w - QPointF(width() / 2.0, height() / 2.0)) / zoom_;
}

QRectF MapView::visibleArea() const
{
	return QRectF(toMap(QPointF(0, 0)), toMap(QPointF(width(), height())));
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
	if (zoom_ < OverviewZoom && !overview_.isNull())
	{
		p.setRenderHint(QPainter::SmoothPixmapTransform, true);
		p.drawImage(QRectF(render_.bounds()), overview_);
	}
	else
	{
		p.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 1);
		render_.paint(p, area, options_);
	}
	p.setRenderHint(QPainter::Antialiasing, true);
	render_.paintOverlays(p, area, options_, zoom_, selected_);
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
	if (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton || e->button() == Qt::RightButton)
	{
		dragging_ = true;
		moved_ = false;
		pressAt_ = e->position().toPoint();
		pressCenter_ = center_;
	}
}

void MapView::mouseMoveEvent(QMouseEvent *e)
{
	const QPointF here = e->position();
	if (dragging_)
	{
		if ((here.toPoint() - pressAt_).manhattanLength() > 3)
			moved_ = true;
		if (moved_)
		{
			setCursor(Qt::ClosedHandCursor);
			center_ = pressCenter_ - (here - pressAt_) / zoom_;
			update();
			emit viewChanged();
		}
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
	if (!dragging_)
		return;
	dragging_ = false;
	unsetCursor();
	if (!moved_ && e->button() == Qt::LeftButton && render_.world())
	{
		const int item = render_.pick(toMap(e->position()).toPoint(), options_);
		select(item);
		emit itemClicked(item);
	}
}

void MapView::wheelEvent(QWheelEvent *e)
{
	const qreal steps = e->angleDelta().y() / 120.0;
	if (steps != 0)
		setZoom(zoom_ * std::pow(1.25, steps), e->position());
}

void MapView::keyPressEvent(QKeyEvent *e)
{
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
