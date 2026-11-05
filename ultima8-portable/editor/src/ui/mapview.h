// The map: drag to pan, wheel to zoom, click an item for its details. The selected items can be
// dragged to move them (with Shift, up and down), Ctrl+click and Ctrl+drag select several, a
// right-click lists everything at and around the pointer, and a shape from the browser can be
// placed.
#pragma once

#include <QImage>
#include <QWidget>

#include "ui/maprender.h"

class MapView : public QWidget
{
	Q_OBJECT

public:
	enum EditAction { Delete, Duplicate, Flip, NextFrame, PrevFrame };

	explicit MapView(QWidget *parent = nullptr);

	void openMap(const World &world, int map);
	// The map's items changed: draw it again, keeping the view where it is.
	void refresh();
	void clear();
	int mapNumber() const { return render_.mapNumber(); }
	const MapRender &render() const { return render_; }

	void setOptions(const LayerOptions &o);
	const LayerOptions &options() const { return options_; }
	// Seeing through what is above a height around the pointer; its center follows the mouse.
	void setXRay(const XRay &x);
	const XRay &xray() const { return xray_; }
	// Moves go in steps of this many world units (1 for none).
	void setSnap(int units) { snap_ = std::max(1, units); }
	int snap() const { return snap_; }

	// Placing: a shape follows the mouse; a click asks for it to be put there.
	void startPlacing(int shape, int frame);
	void stopPlacing();
	bool placing() const { return placing_; }

	qreal zoom() const { return zoom_; }
	void setZoom(qreal zoom, QPointF anchor = QPointF(-1, -1));
	void zoomBy(qreal factor) { setZoom(zoom_ * factor); }
	void fitMap();
	// The view's middle, in screen pixels of the map.
	QPointF center() const { return center_; }
	void centerOn(QPointF screen);
	void showItem(int item, bool select);

	// The selection: the primary item (shown in the Item window) and any others with it.
	int selected() const { return selected_; }
	const QVector<int> &selection() const { return selection_; }
	// Selects one item (or none, with -1).
	void select(int item);
	// Makes item the primary one, keeping the others when it is one of them.
	void setPrimary(int item);
	void setSelection(const QVector<int> &items, int primary);
	void toggleSelected(int item);
	// Items drawn as related to the selection (outlined in cyan).
	void setRelated(const QVector<int> &items);
	// An item named in a list being looked at, drawn highlighted.
	void setHighlight(int item);
	QRectF visibleArea() const;

	// The whole map, small, as the overview and minimap use it.
	const QImage &overview() const { return overview_; }
	qreal overviewScale() const { return overviewScale_; }
	// The view as it is drawn, for saving.
	QImage grabView();

	QSize sizeHint() const override { return QSize(800, 600); }

signals:
	// The user made item the primary one (or cleared the selection, with -1).
	void itemClicked(int item);
	void selectionChanged();
	// Move the selection by this much.
	void moveRequested(int dx, int dy, int dz);
	void placeRequested(const MapItem &item);
	void placingStopped();
	void actionRequested(int action);
	void viewChanged();
	void overviewChanged();
	void status(const QString &text);

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseMoveEvent(QMouseEvent *) override;
	void mouseReleaseEvent(QMouseEvent *) override;
	void wheelEvent(QWheelEvent *) override;
	void keyPressEvent(QKeyEvent *) override;
	void resizeEvent(QResizeEvent *) override;
	void leaveEvent(QEvent *) override;
	// The tooltip: what the item under the pointer goes by (Names::itemName).
	bool event(QEvent *) override;

private:
	void draw(QPainter &p, QSize size);
	QPointF toMap(QPointF widget) const;
	void remakeOverview();
	// The move a drag of the mouse by delta (widget pixels) stands for, in world units.
	void dragMove(QPointF delta, bool vertical, bool fine, int &dx, int &dy, int &dz) const;
	void updatePlacement(QPointF widget);
	int snapped(int v) const;
	int pickAt(QPointF widget) const;
	// The list of what is at and around the pointer, to choose from.
	void showPickMenu(QPoint widget);
	const XRay *activeXRay() const { return xray_.on && xrayInside_ ? &xray_ : nullptr; }

	MapRender render_;
	LayerOptions options_;
	XRay xray_;
	bool xrayInside_ = false;
	QImage overview_;
	qreal overviewScale_ = 0.25;
	QPointF center_;
	qreal zoom_ = 1;
	int selected_ = -1;
	QVector<int> selection_;
	QVector<int> related_;
	int hovered_ = -1;
	int highlight_ = -1;
	int snap_ = 32;

	enum Drag { None, Pan, MoveItem, Band };
	Drag drag_ = None;
	bool moved_ = false;
	QPoint pressAt_;
	QPointF pressCenter_;
	Qt::MouseButton pressButton_ = Qt::NoButton;
	int dragDx_ = 0, dragDy_ = 0, dragDz_ = 0;
	QRectF band_;	// screen pixels of the map

	bool placing_ = false;
	bool placeValid_ = false;
	MapItem place_;
};
