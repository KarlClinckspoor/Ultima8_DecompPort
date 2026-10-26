// The map: drag to pan, wheel to zoom, click an item for its details. The selected item can be
// dragged to move it (with Shift, up and down), and a shape from the browser can be placed.
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
	int selected() const { return selected_; }
	void select(int item);
	QRectF visibleArea() const;

	// The whole map, small, as the overview and minimap use it.
	const QImage &overview() const { return overview_; }
	qreal overviewScale() const { return overviewScale_; }
	// The view as it is drawn, for saving.
	QImage grabView();

	QSize sizeHint() const override { return QSize(800, 600); }

signals:
	void itemClicked(int item);
	void moveRequested(int item, int dx, int dy, int dz);
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

private:
	void draw(QPainter &p, QSize size);
	QPointF toMap(QPointF widget) const;
	void remakeOverview();
	// The move a drag of the mouse by delta (widget pixels) stands for, in world units.
	void dragMove(QPointF delta, bool vertical, int &dx, int &dy, int &dz) const;
	void updatePlacement(QPointF widget);
	int snapped(int v) const;

	MapRender render_;
	LayerOptions options_;
	QImage overview_;
	qreal overviewScale_ = 0.25;
	QPointF center_;
	qreal zoom_ = 1;
	int selected_ = -1;
	int snap_ = 32;

	enum Drag { None, Pan, MoveItem };
	Drag drag_ = None;
	bool moved_ = false;
	QPoint pressAt_;
	QPointF pressCenter_;
	int dragDx_ = 0, dragDy_ = 0, dragDz_ = 0;

	bool placing_ = false;
	bool placeValid_ = false;
	MapItem place_;
};
