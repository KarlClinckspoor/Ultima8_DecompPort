// Everything about one item: its record, its shape's type flags, what it does as an egg, its
// contents, and the ways in and out of the map it is part of.
#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class World;

class ItemPanel : public QWidget
{
	Q_OBJECT

public:
	explicit ItemPanel(QWidget *parent = nullptr);
	void showItem(const World *world, int map, int item);
	int map() const { return map_; }
	int item() const { return item_; }

signals:
	// Show this item on the map view (opening its map if need be).
	void goToItem(int map, int item);

private:
	QTreeWidgetItem *section(const QString &title);
	QTreeWidgetItem *row(QTreeWidgetItem *parent, const QString &field, const QString &value, int map = -1, int item = -1);

	const World *world_ = nullptr;
	int map_ = -1;
	int item_ = -1;
	QLabel *picture_;
	QLabel *title_;
	QTreeWidget *tree_;
	QPushButton *center_;
	QPushButton *follow_;
	int followMap_ = -1, followItem_ = -1;
};
