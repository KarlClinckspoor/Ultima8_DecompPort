// The usecode, class by class: the list of classes with code (filtered by number or name), a
// class's routines disassembled, with its events, calls, branches and flags to follow, and what
// refers to it: the items on the maps that run it, and the classes that call it.
#pragma once

#include <QHash>
#include <QWidget>

#include "data/disasm.h"

class QComboBox;
class QStackedWidget;
class QToolButton;
class UsecodeGraph;
class QLabel;
class QLineEdit;
class QTabWidget;
class QTextBrowser;
class QTreeWidget;
class World;

class UsecodeView : public QWidget
{
	Q_OBJECT

public:
	explicit UsecodeView(QWidget *parent = nullptr);
	void setWorld(const World *world);
	// Items were changed: the lists of who runs a class are made again when next shown.
	void itemsChanged() { usersStale_ = true; }
	// Shows a class, at an event's routine (or at an offset in its code); -1 for the top.
	void showClass(int classId, int event = -1, int offset = -1);
	int currentClass() const { return class_; }
	QTextBrowser *listing() const { return listing_; }
	UsecodeGraph *graph() const { return graph_; }
	// Shows the code as a graph of the current routine (true) or as a listing.
	void setGraphMode(bool graph);

	// What a class is for: "the class of shape 82 (KEY)", "NPC 2's class", "usecode egg type 4".
	static QString classRole(const World &world, int classId);

signals:
	void goToItem(int map, int item);
	void showFlag(int bit);

private:
	void fillClasses();
	void filter(const QString &text);
	void openLink(const QUrl &url);
	void findUsers();
	// Goes to an event's routine or an offset in the class shown, in the listing and the graph.
	void goTo(int event, int offset);

	const World *world_ = nullptr;
	int class_ = -1;
	QHash<int, QVector<QPair<int, int>>> users_;	// class: the (map, item) pairs that run it
	bool usersStale_ = true;
	QLineEdit *find_;
	QTreeWidget *classes_;
	QLabel *title_;
	QTextBrowser *listing_;
	UsecodeGraph *graph_;
	QStackedWidget *stack_;
	QComboBox *routines_;
	QToolButton *graphButton_;
	QVector<Ucode::Line> lines_;	// the class shown
	QTabWidget *refs_;
	QTreeWidget *items_;
	QTreeWidget *callers_;
};
