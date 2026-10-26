// Everything about one item: its record, its shape's type flags, what it does as an egg, its
// contents, and the ways in and out of the map it is part of. The Edit tab changes it.
#pragma once

#include <QWidget>

#include <functional>
#include <vector>

#include "data/world.h"

class QCheckBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class ItemPanel : public QWidget
{
	Q_OBJECT

public:
	explicit ItemPanel(QWidget *parent = nullptr);
	void showItem(const World *world, int map, int item);
	// The item changed: show it again, keeping what is being typed in.
	void refresh();
	int map() const { return map_; }
	int item() const { return item_; }

signals:
	// Show this item on the map view (opening its map if need be).
	void goToItem(int map, int item);
	void editItem(int map, int item, const MapItem &fields, const QString &what);
	void editNpc(int npc, const NpcStats &stats);
	void deleteItem(int map, int item);
	void duplicateItem(int map, int item);
	void showShape(int shape, int frame);

private:
	struct Field
	{
		QString label;
		std::function<int(const MapItem &)> get;
		std::function<void(MapItem &, int)> set;
		int min, max;
		QString tip;
		QSpinBox *box = nullptr;
		QCheckBox *check = nullptr;	// a field that is 0 or 1
	};
	struct NpcField
	{
		QString label;
		std::function<int(const NpcStats &)> get;
		std::function<void(NpcStats &, int)> set;
		int min, max;
		QSpinBox *box = nullptr;
		QCheckBox *check = nullptr;
	};

	void fillDetails();
	void buildEditor();
	void updateEditor();
	void fieldChanged(Field &f, int value);
	QTreeWidgetItem *section(const QString &title);
	QTreeWidgetItem *row(QTreeWidgetItem *parent, const QString &field, const QString &value, int map = -1, int item = -1);

	const World *world_ = nullptr;
	int map_ = -1;
	int item_ = -1;
	QLabel *picture_;
	QLabel *title_;
	QTabWidget *tabs_;
	QTreeWidget *tree_;
	QWidget *editPage_;
	QVBoxLayout *editLayout_;
	QLabel *readOnly_;
	QPushButton *center_;
	QPushButton *follow_;
	QPushButton *duplicate_;
	QPushButton *delete_;
	int followMap_ = -1, followItem_ = -1;
	std::vector<Field> fields_;
	std::vector<NpcField> npcFields_;
	QString editorKey_;	// what the editor was built for
};
