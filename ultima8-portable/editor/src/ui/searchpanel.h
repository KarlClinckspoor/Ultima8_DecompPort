// Finding items on this map or in the whole game: words match what items go by (their names,
// classes, families and the names given to shapes), and terms like shape:82, q:9, key:9 or
// is:door narrow it down. Double-click a result to show it.
#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class World;
struct MapItem;

class SearchPanel : public QWidget
{
	Q_OBJECT

public:
	// The kinds of item a search looks at; the buttons above the results turn each on and off.
	enum Kind { Fixed = 1, Globs = 2, Movable = 4, Npcs = 8, AllKinds = 15 };

	explicit SearchPanel(QWidget *parent = nullptr);
	~SearchPanel() override;
	void setWorld(const World *world);
	void setCurrentMap(int map) { map_ = map; }
	// Searches for query, on this map or in every map. A search runs a slice at a time, so the
	// window keeps answering; searching() is true until it ends and finished() is emitted.
	void run(const QString &query, bool everyMap);
	bool searching() const { return job_ != nullptr; }
	// What the last search found (all of it, not only the rows listed).
	int found() const { return found_; }
	int kinds() const { return kinds_; }
	void setKinds(int kinds);
	// Stops a search, keeping what it found so far.
	void stop();

	// Whether item (on map) matches query; error says what is wrong with the query.
	static bool matches(const World &world, int map, int item, const QString &query, QString *error = nullptr);

signals:
	void goToItem(int map, int item);
	void finished();

private:
	struct Job;
	void search();
	void step();
	void end(bool stopped);

	const World *world_ = nullptr;
	int map_ = -1;
	int found_ = 0;
	int kinds_ = AllKinds;
	Job *job_ = nullptr;
	QLineEdit *query_;
	QComboBox *scope_;
	QCheckBox *contents_;
	QToolButton *kindButtons_[4];
	QPushButton *go_;
	QLabel *count_;
	QTreeWidget *results_;
};
