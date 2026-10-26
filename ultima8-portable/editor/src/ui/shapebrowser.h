// Every shape, to pick one to place: filter by number, usecode name or family, pick a frame,
// double-click (or press Place) and click on the map.
#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPixmap>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListView;
class QSpinBox;
class QComboBox;
class World;

class ShapeModel : public QAbstractListModel
{
	Q_OBJECT

public:
	explicit ShapeModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
	void setWorld(const World *world);
	void setFilter(const QString &text, int family);
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	int shapeAt(int row) const { return row >= 0 && row < shapes_.size() ? shapes_[row] : -1; }
	int rowOf(int shape) const { return shapes_.indexOf(shape); }

private:
	const World *world_ = nullptr;
	QVector<int> shapes_;
	mutable QHash<int, QPixmap> icons_;
};

class ShapeBrowser : public QWidget
{
	Q_OBJECT

public:
	explicit ShapeBrowser(QWidget *parent = nullptr);
	void setWorld(const World *world);
	int shape() const { return shape_; }
	int frame() const;
	// Shows this shape and frame (from the selected item, say).
	void showShape(int shape, int frame);

signals:
	void placeRequested(int shape, int frame);

private:
	void refilter();
	void choose(int shape);
	void updatePreview();

	const World *world_ = nullptr;
	ShapeModel *model_;
	QLineEdit *filter_;
	QComboBox *family_;
	QListView *list_;
	QSpinBox *frame_;
	QLabel *preview_;
	QLabel *info_;
	int shape_ = -1;
};
