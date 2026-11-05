// The whole map, small, with the part the map view shows; click or drag to move there.
#pragma once

#include <QWidget>

class MapView;

class Minimap : public QWidget
{
	Q_OBJECT

public:
	explicit Minimap(MapView *view, QWidget *parent = nullptr);
	QSize sizeHint() const override { return QSize(280, 180); }

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseMoveEvent(QMouseEvent *) override;

private:
	QRectF imageRect() const;
	void moveTo(QPointF at);

	MapView *view_;
};
