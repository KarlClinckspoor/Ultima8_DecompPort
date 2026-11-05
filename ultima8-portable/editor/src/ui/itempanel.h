// Everything about one item: its record, its shape's type flags, what it does as an egg, its
// contents, and the ways in and out of the map it is part of. The Edit tab changes it.
#pragma once

#include <QWidget>

#include <functional>
#include <vector>

#include "data/world.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
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
	// How many items the map has selected with this one; moves and the buttons apply to all.
	void setSelectionCount(int n);

signals:
	// Show this item on the map view (opening its map if need be).
	void goToItem(int map, int item);
	void editItem(int map, int item, const MapItem &fields, const QString &what);
	void editNpc(int npc, const NpcStats &stats);
	void deleteItem(int map, int item);
	void duplicateItem(int map, int item);
	void showShape(int shape, int frame);
	// Move the item (and the others selected with it) by this much.
	void nudge(int dx, int dy, int dz);
	// A name was given to a shape.
	void namesChanged();
	// Search the whole game for this (SearchPanel).
	void find(const QString &query);

private:
	// The item's name when it has one, then its class, shape and frame.
	QString titleOf(const MapItem &it) const;
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
	// The doors a key opens, the keys that open a door, anywhere in the game.
	void fillLocks(const MapItem &it);
	// Where the follow button leads: one place, or a menu of several.
	void setFollow(const QString &text, const QVector<QPair<int, int>> &targets);
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
	QLabel *selectionNote_;
	QLineEdit *shapeName_ = nullptr;
	QComboBox *nudgeStep_ = nullptr;
	QPushButton *center_;
	QPushButton *follow_;
	QPushButton *findButton_;
	QString findQuery_;
	int selectionCount_ = 1;
	QPushButton *duplicate_;
	QPushButton *delete_;
	int followMap_ = -1, followItem_ = -1;
	std::vector<Field> fields_;
	std::vector<NpcField> npcFields_;
	QString editorKey_;	// what the editor was built for
};
