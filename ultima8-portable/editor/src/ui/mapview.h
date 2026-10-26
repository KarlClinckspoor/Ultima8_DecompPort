// The map: drag to pan, wheel to zoom, click an item for its details.
#pragma once

#include <QImage>
#include <QWidget>

#include "ui/maprender.h"

class MapView : public QWidget
{
	Q_OBJECT

public:
	explicit MapView(QWidget *parent = nullptr);

	void openMap(const World &world, int map);
	void clear();
	int mapNumber() const { return render_.mapNumber(); }
	const MapRender &render() const { return render_; }

	void setOptions(const LayerOptions &o);
	const LayerOptions &options() const { return options_; }

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

private:
	void draw(QPainter &p, QSize size);
	QPointF toMap(QPointF widget) const;
	void remakeOverview();

	MapRender render_;
	LayerOptions options_;
	QImage overview_;
	qreal overviewScale_ = 0.25;
	QPointF center_;
	qreal zoom_ = 1;
	int selected_ = -1;
	bool dragging_ = false;
	bool moved_ = false;
	QPoint pressAt_;
	QPointF pressCenter_;
};
