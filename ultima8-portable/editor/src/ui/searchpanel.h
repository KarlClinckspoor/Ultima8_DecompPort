// Finding items on this map or in the whole game: words match what items go by (their names,
// classes, families and the names given to shapes), and terms like shape:82, q:9, key:9 or
// is:door narrow it down. Double-click a result to show it.
#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class World;
struct MapItem;

class SearchPanel : public QWidget
{
	Q_OBJECT

public:
	explicit SearchPanel(QWidget *parent = nullptr);
	void setWorld(const World *world);
	void setCurrentMap(int map) { map_ = map; }
	// Searches for query, on this map or in every map.
	void run(const QString &query, bool everyMap);

	// Whether item (on map) matches query; error says what is wrong with the query.
	static bool matches(const World &world, int map, int item, const QString &query, QString *error = nullptr);

signals:
	void goToItem(int map, int item);

private:
	void search();

	const World *world_ = nullptr;
	int map_ = -1;
	QLineEdit *query_;
	QComboBox *scope_;
	QCheckBox *contents_;
	QLabel *count_;
	QTreeWidget *results_;
};
