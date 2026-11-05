// Maps as nodes and the ways between them as arrows: teleport eggs, and usecode teleports.
#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QWidget>

class QCheckBox;
class QGraphicsScene;
class World;
class GraphNode;
class GraphEdge;

class GraphCanvas : public QGraphicsView
{
	Q_OBJECT

public:
	explicit GraphCanvas(QWidget *parent = nullptr);

protected:
	void wheelEvent(QWheelEvent *) override;
};

class TransitionGraph : public QWidget
{
	Q_OBJECT

public:
	explicit TransitionGraph(QWidget *parent = nullptr);
	void setWorld(const World *world);
	// Map names changed: relabel.
	void refreshNames();
	void setCurrentMap(int map);
	QImage grabGraph(QSize size);
	void fit();

	// Node -1 stands for usecode that is not tied to any map (spells, books).
	static QString mapLabel(int map);

signals:
	void openMap(int map);
	// Show a transition: its source item, or its destination when it has no source.
	void showTransition(int index);

protected:
	void showEvent(QShowEvent *) override;

private:
	void rebuild();
	void arrange();
	void savePositions();
	friend class GraphNode;
	friend class GraphEdge;

	const World *world_ = nullptr;
	QGraphicsScene *scene_;
	GraphCanvas *view_;
	QCheckBox *usecodeEggs_;
	QCheckBox *npcTeleports_;
	QHash<int, GraphNode *> nodes_;
	QList<GraphEdge *> edges_;
	int current_ = -1;
	bool fitted_ = false;
};
