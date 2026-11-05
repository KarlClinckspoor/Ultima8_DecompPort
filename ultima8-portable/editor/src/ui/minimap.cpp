#include "ui/minimap.h"

#include <QMouseEvent>
#include <QPainter>

#include "ui/mapview.h"

Minimap::Minimap(MapView *view, QWidget *parent) : QWidget(parent), view_(view)
{
	setMinimumSize(120, 80);
	connect(view, &MapView::viewChanged, this, qOverload<>(&QWidget::update));
	connect(view, &MapView::overviewChanged, this, qOverload<>(&QWidget::update));
}

// Where the overview sits in the widget, kept to its shape.
QRectF Minimap::imageRect() const
{
	const QImage &img = view_->overview();
	if (img.isNull())
		return QRectF();
	QSizeF s = QSizeF(img.size()).scaled(QSizeF(size()) - QSizeF(8, 8), Qt::KeepAspectRatio);
	return QRectF(QPointF((width() - s.width()) / 2, (height() - s.height()) / 2), s);
}

void Minimap::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.fillRect(rect(), QColor(12, 12, 16));
	const QRectF r = imageRect();
	if (r.isEmpty())
		return;
	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	p.drawImage(r, view_->overview());

	// The map view's area, and the map's teleport eggs.
	const QRect b = view_->render().bounds();
	const qreal s = r.width() / b.width();
	auto toMini = [&](QPointF screen) { return r.topLeft() + (screen - QPointF(b.topLeft())) * s; };
	const MapRender &render = view_->render();
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setPen(Qt::NoPen);
	for (const MapRender::Entry &e : render.entries())
	{
		if (e.family != FamilyTeleportEgg)
			continue;
		const MapItem &it = render.world()->map(render.mapNumber()).items[e.item];
		p.setBrush(EggInfo::sends(it) ? QColor(255, 150, 30) : QColor(80, 220, 120));
		p.drawEllipse(toMini(QPointF(e.sx, e.sy)), 2.5, 2.5);
	}
	if (view_->selected() >= 0)
	{
		const int e = render.entryOf(view_->selected());
		if (e >= 0)
		{
			p.setBrush(QColor(255, 255, 0));
			p.drawEllipse(toMini(QPointF(render.entries()[e].sx, render.entries()[e].sy)), 3, 3);
		}
	}
	const QRectF v = view_->visibleArea();
	p.setBrush(QColor(255, 255, 255, 30));
	p.setPen(QPen(Qt::white, 1));
	p.drawRect(QRectF(toMini(v.topLeft()), toMini(v.bottomRight())));
}

void Minimap::moveTo(QPointF at)
{
	const QRectF r = imageRect();
	if (r.isEmpty())
		return;
	const QRect b = view_->render().bounds();
	const qreal s = r.width() / b.width();
	view_->centerOn(QPointF(b.topLeft()) + (at - r.topLeft()) / s);
}

void Minimap::mousePressEvent(QMouseEvent *e)
{
	moveTo(e->position());
}

void Minimap::mouseMoveEvent(QMouseEvent *e)
{
	if (e->buttons() & Qt::LeftButton)
		moveTo(e->position());
}
