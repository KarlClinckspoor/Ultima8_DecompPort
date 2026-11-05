// A usecode routine as a graph, as IDA draws a function: its instructions in boxes that run
// straight through, joined by its branches (green when a test holds, red when it fails, blue
// for go to), loops drawn back up the left. A read of a global flag has a red arrow coming into
// its line from the left; a write, a green arrow going out to the right. Double-click a line to
// follow its call, its branch or its flag.
#pragma once

#include <QGraphicsView>
#include <QVector>

#include "data/disasm.h"

class QGraphicsScene;
class World;

class UsecodeGraph : public QGraphicsView
{
	Q_OBJECT

public:
	explicit UsecodeGraph(QWidget *parent = nullptr);
	// Draws the routine of the class whose heading is lines[head]; offset (if in it) is shown
	// and marked.
	void showRoutine(const World *world, int classId, const QVector<Ucode::Line> &lines, int head, int offset = -1);
	int blockCount() const { return blocks_.size(); }
	// How many flag arrows are drawn: reads (red) and writes (green).
	int flagArrows(bool writes) const { return writes ? writes_ : reads_; }
	void fit();

signals:
	void openClass(int classId, int offset);
	void showFlag(int bit);

protected:
	void wheelEvent(QWheelEvent *) override;
	void keyPressEvent(QKeyEvent *) override;
	void mouseDoubleClickEvent(QMouseEvent *) override;

private:
	struct Box
	{
		QRectF rect;		// the box, in scene units
		int block;
	};
	// The line under a scene point, or -1.
	int lineAt(QPointF scene) const;
	void centerOnLine(int line);

	QGraphicsScene *scene_;
	QVector<Ucode::Line> lines_;
	QVector<Ucode::Block> blocks_;
	QVector<Box> boxes_;
	int classId_ = -1;
	int reads_ = 0, writes_ = 0;
	qreal lineHeight_ = 14, charWidth_ = 7, pad_ = 6;
};
