#include "ui/graphview.h"

#include <QCheckBox>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QStyleOptionGraphicsItem>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "data/world.h"

static const qreal NodeRadius = 24;

QString TransitionGraph::mapLabel(int map)
{
	if (map < 0)
		return QObject::tr("Usecode (any map)");
	const QString name = QSettings().value(QStringLiteral("mapNames/%1").arg(map)).toString();
	return name.isEmpty() ? QObject::tr("Map %1").arg(map) : QStringLiteral("%1 · %2").arg(map).arg(name);
}

class GraphEdge : public QGraphicsPathItem
{
public:
	enum Category { Egg, UsecodeEgg, NpcTeleport };
	GraphEdge(TransitionGraph *graph, GraphNode *from, GraphNode *to, Category c);
	void adjust();
	void addTransition(int index) { transitions_ << index; }
	void finish();
	GraphNode *from() const { return from_; }
	GraphNode *to() const { return to_; }
	void setHighlighted(bool on);
	QPainterPath shape() const override;

protected:
	void paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w) override;
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override;

private:
	TransitionGraph *graph_;
	GraphNode *from_, *to_;
	Category category_;
	QList<int> transitions_;
	bool highlighted_ = false;
	QPolygonF head_;
	QPointF labelAt_;
};

class GraphNode : public QGraphicsItem
{
public:
	GraphNode(TransitionGraph *graph, int map) : graph_(graph), map_(map)
	{
		setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
		setCursor(Qt::OpenHandCursor);
		setZValue(2);
		setAcceptHoverEvents(true);
	}
	int map() const { return map_; }
	void addEdge(GraphEdge *e) { edges_ << e; }
	const QList<GraphEdge *> &edges() const { return edges_; }
	void setCurrent(bool on) { current_ = on; update(); }
	QRectF boundingRect() const override
	{
		return QRectF(-90, -NodeRadius - 2, 180, NodeRadius * 2 + 24);
	}
	QPainterPath shape() const override
	{
		QPainterPath p;
		p.addEllipse(QPointF(0, 0), NodeRadius, NodeRadius);
		return p;
	}

protected:
	void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
	{
		p->setRenderHint(QPainter::Antialiasing);
		QColor fill = map_ < 0 ? QColor(70, 50, 100) : QColor(40, 70, 110);
		if (current_)
			fill = QColor(150, 120, 20);
		if (isSelected())
			fill = fill.lighter(140);
		p->setBrush(fill);
		p->setPen(QPen(isSelected() || hover_ ? QColor(255, 255, 255) : QColor(150, 180, 220), 2));
		p->drawEllipse(QPointF(0, 0), NodeRadius, NodeRadius);
		p->setPen(Qt::white);
		QFont f = p->font();
		f.setBold(true);
		f.setPixelSize(15);
		p->setFont(f);
		p->drawText(QRectF(-NodeRadius, -NodeRadius, NodeRadius * 2, NodeRadius * 2), Qt::AlignCenter,
			map_ < 0 ? QStringLiteral("UC") : QString::number(map_));
		const QString name = QSettings().value(QStringLiteral("mapNames/%1").arg(map_)).toString();
		const QString label = map_ < 0 ? QObject::tr("usecode, any map") : name;
		if (!label.isEmpty())
		{
			f.setBold(false);
			f.setPixelSize(11);
			p->setFont(f);
			QRectF box(-90, NodeRadius + 2, 180, 18);
			p->setPen(QColor(220, 220, 220));
			p->drawText(box, Qt::AlignHCenter | Qt::AlignTop, label);
		}
	}
	QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
	{
		if (change == ItemPositionHasChanged)
			for (GraphEdge *e : edges_)
				e->adjust();
		if (change == ItemSelectedHasChanged)
			for (GraphEdge *e : edges_)
				e->setHighlighted(value.toBool());
		return QGraphicsItem::itemChange(change, value);
	}
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override
	{
		QGraphicsItem::mouseReleaseEvent(e);
		graph_->savePositions();
	}
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override
	{
		if (map_ >= 0)
			emit graph_->openMap(map_);
	}
	void hoverEnterEvent(QGraphicsSceneHoverEvent *) override { hover_ = true; update(); }
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override { hover_ = false; update(); }

private:
	TransitionGraph *graph_;
	int map_;
	bool current_ = false;
	bool hover_ = false;
	QList<GraphEdge *> edges_;
};

GraphEdge::GraphEdge(TransitionGraph *graph, GraphNode *from, GraphNode *to, Category c)
	: graph_(graph), from_(from), to_(to), category_(c)
{
	setZValue(1);
	setAcceptHoverEvents(true);
	from->addEdge(this);
	to->addEdge(this);
}

void GraphEdge::setHighlighted(bool on)
{
	highlighted_ = on;
	setZValue(on ? 1.5 : 1);
	update();
}

// The tooltip lists every way the arrow stands for.
void GraphEdge::finish()
{
	const World *w = graph_->world_;
	QStringList lines;
	lines << QStringLiteral("<b>%1 → %2</b>").arg(TransitionGraph::mapLabel(from_->map()).toHtmlEscaped(),
		TransitionGraph::mapLabel(to_->map()).toHtmlEscaped());
	for (int i : transitions_)
	{
		const Transition &t = w->transitions()[i];
		QString where = t.toEgg >= 0 ? QObject::tr("egg #%1").arg(t.toEgg)
			: QObject::tr("%1, %2, %3").arg(t.x).arg(t.y).arg(t.z);
		if (t.toEgg >= 0 && t.toItem < 0)
			where += QObject::tr(" (missing)");
		QString how;
		if (t.kind == Transition::TeleportEgg)
		{
			const MapItem &egg = w->map(t.fromMap).items[t.fromItem];
			how = QObject::tr("teleport egg at %1, %2, %3").arg(egg.x).arg(egg.y).arg(egg.z);
		}
		else
			how = QStringLiteral("%1: %2").arg(w->usecode().className(t.classId),
				t.kind == Transition::UsecodeToEgg ? QStringLiteral("teleportToEgg") : QStringLiteral("Npc::teleport"));
		lines << QStringLiteral("%1 → %2").arg(how.toHtmlEscaped(), where.toHtmlEscaped());
	}
	lines << QObject::tr("<i>Double-click to show the first.</i>");
	setToolTip(lines.join(QStringLiteral("<br>")));
	adjust();
}

void GraphEdge::adjust()
{
	const QPointF a = from_->pos(), b = to_->pos();
	QLineF line(a, b);
	if (line.length() < 1)
		return;
	// Bend each arrow to its right, so a pair going both ways shows as two; usecode arrows bend
	// further than egg ones.
	const QPointF dir = (b - a) / line.length();
	const QPointF normal(-dir.y(), dir.x());
	const qreal bend = 18 + 14 * category_;
	const QPointF mid = (a + b) / 2 + normal * bend;
	// Start and end on the circles.
	auto edgeOf = [&](QPointF centre, QPointF toward) {
		QLineF l(centre, toward);
		l.setLength(NodeRadius + 2);
		return l.p2();
	};
	const QPointF start = edgeOf(a, mid), end = edgeOf(b, mid);
	QPainterPath path(start);
	path.quadTo(mid, end);
	setPath(path);
	const QLineF tangent(mid, end);
	const qreal angle = std::atan2(-tangent.dy(), tangent.dx());
	const qreal size = 10;
	head_ = QPolygonF() << end
		<< end - QPointF(std::sin(angle + M_PI / 3) * size, std::cos(angle + M_PI / 3) * size)
		<< end - QPointF(std::sin(angle + M_PI - M_PI / 3) * size, std::cos(angle + M_PI - M_PI / 3) * size);
	labelAt_ = path.pointAtPercent(0.5);
}

QPainterPath GraphEdge::shape() const
{
	QPainterPathStroker s;
	s.setWidth(10);
	return s.createStroke(path());
}

void GraphEdge::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
	p->setRenderHint(QPainter::Antialiasing);
	QColor c;
	Qt::PenStyle style = Qt::SolidLine;
	switch (category_)
	{
	case Egg: c = QColor(255, 160, 50); break;
	case UsecodeEgg: c = QColor(190, 120, 255); style = Qt::DashLine; break;
	case NpcTeleport: c = QColor(150, 150, 150); style = Qt::DotLine; break;
	}
	if (highlighted_)
		c = c.lighter(140);
	const qreal width = (highlighted_ ? 2.5 : 1.5) + std::min<qreal>(3, (transitions_.size() - 1) * 0.5);
	p->setPen(QPen(c, width, style));
	p->setBrush(Qt::NoBrush);
	p->drawPath(path());
	p->setPen(Qt::NoPen);
	p->setBrush(c);
	p->drawPolygon(head_);
	if (transitions_.size() > 1)
	{
		QFont f = p->font();
		f.setPixelSize(10);
		p->setFont(f);
		QRectF box(labelAt_ - QPointF(9, 7), QSizeF(18, 14));
		p->setBrush(QColor(20, 20, 26));
		p->setPen(QPen(c, 1));
		p->drawRoundedRect(box, 4, 4);
		p->setPen(c);
		p->drawText(box, Qt::AlignCenter, QString::number(transitions_.size()));
	}
}

void GraphEdge::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *)
{
	if (!transitions_.isEmpty())
		emit graph_->showTransition(transitions_.first());
}

GraphCanvas::GraphCanvas(QWidget *parent) : QGraphicsView(parent)
{
	setRenderHint(QPainter::Antialiasing);
	setDragMode(QGraphicsView::ScrollHandDrag);
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	setBackgroundBrush(QColor(18, 18, 24));
}

void GraphCanvas::wheelEvent(QWheelEvent *e)
{
	const qreal f = std::pow(1.2, e->angleDelta().y() / 120.0);
	scale(f, f);
}

TransitionGraph::TransitionGraph(QWidget *parent) : QWidget(parent)
{
	scene_ = new QGraphicsScene(this);
	view_ = new GraphCanvas;
	view_->setScene(scene_);
	usecodeEggs_ = new QCheckBox(tr("Usecode teleportToEgg"));
	usecodeEggs_->setChecked(true);
	usecodeEggs_->setToolTip(tr("Teleports usecode makes onto a teleport egg (dashed)"));
	npcTeleports_ = new QCheckBox(tr("Usecode Npc::teleport"));
	npcTeleports_->setToolTip(tr("Usecode moving an NPC, often the avatar, to a fixed place (dotted).\n"
		"Some of these move other NPCs off stage."));
	auto *arrangeButton = new QPushButton(tr("Arrange"));
	arrangeButton->setToolTip(tr("Lay the maps out again"));
	auto *fitButton = new QPushButton(tr("Fit"));
	connect(usecodeEggs_, &QCheckBox::toggled, this, &TransitionGraph::rebuild);
	connect(npcTeleports_, &QCheckBox::toggled, this, &TransitionGraph::rebuild);
	connect(arrangeButton, &QPushButton::clicked, this, [this] {
		arrange();
		savePositions();
		fit();
	});
	connect(fitButton, &QPushButton::clicked, this, &TransitionGraph::fit);

	auto *legend = new QLabel(tr("<span style='color:#ffa032'>━</span> teleport egg &nbsp; "
		"<span style='color:#be78ff'>┅</span> usecode &nbsp; Double-click a map to open it, an arrow to follow it."));
	auto *bar = new QHBoxLayout;
	bar->addWidget(usecodeEggs_);
	bar->addWidget(npcTeleports_);
	bar->addWidget(arrangeButton);
	bar->addWidget(fitButton);
	bar->addSpacing(12);
	bar->addWidget(legend, 1);
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(4, 4, 4, 4);
	v->addLayout(bar);
	v->addWidget(view_, 1);
}

void TransitionGraph::setWorld(const World *world)
{
	world_ = world;
	rebuild();
	fitted_ = false;
	if (isVisible())
	{
		fitted_ = true;
		fit();
	}
}

void TransitionGraph::refreshNames()
{
	scene_->update();
	for (GraphEdge *e : edges_)
		e->finish();
}

void TransitionGraph::setCurrentMap(int map)
{
	current_ = map;
	for (GraphNode *n : nodes_)
		n->setCurrent(n->map() == map);
}

void TransitionGraph::rebuild()
{
	scene_->clear();
	nodes_.clear();
	edges_.clear();
	if (!world_)
		return;
	auto node = [&](int map) {
		GraphNode *&n = nodes_[map];
		if (!n)
		{
			n = new GraphNode(this, map);
			scene_->addItem(n);
		}
		return n;
	};
	QHash<QString, GraphEdge *> byKey;
	QHash<int, int> loops;
	const auto &list = world_->transitions();
	for (int i = 0; i < list.size(); i++)
	{
		const Transition &t = list[i];
		GraphEdge::Category c = t.kind == Transition::TeleportEgg ? GraphEdge::Egg
			: t.kind == Transition::UsecodeToEgg ? GraphEdge::UsecodeEgg : GraphEdge::NpcTeleport;
		if ((c == GraphEdge::UsecodeEgg && !usecodeEggs_->isChecked()) ||
			(c == GraphEdge::NpcTeleport && !npcTeleports_->isChecked()))
			continue;
		if (t.fromMap == t.toMap)
		{
			loops[t.fromMap]++;
			continue;
		}
		const QString key = QStringLiteral("%1>%2/%3").arg(t.fromMap).arg(t.toMap).arg(int(c));
		GraphEdge *&e = byKey[key];
		if (!e)
		{
			e = new GraphEdge(this, node(t.fromMap), node(t.toMap), c);
			scene_->addItem(e);
			edges_ << e;
		}
		e->addTransition(i);
	}
	// Every map with items gets a node, joined or not.
	for (int m : world_->mapNumbers())
		node(m);
	for (GraphNode *n : nodes_)
	{
		const MapData &m = world_->map(n->map());
		QString tip = QStringLiteral("<b>%1</b>").arg(mapLabel(n->map()).toHtmlEscaped());
		if (n->map() >= 0)
			tip += tr("<br>%1 fixed, %2 movable, %3 from globs, %4 NPCs")
				.arg(m.fixedCount).arg(m.nonFixedCount).arg(m.globCount).arg(m.npcCount);
		if (loops.value(n->map()))
			tip += tr("<br>%1 teleports within the map").arg(loops.value(n->map()));
		tip += tr("<br><i>Double-click to open.</i>");
		n->setToolTip(tip);
	}

	// Saved places, or a fresh layout when any node has none.
	QSettings s;
	bool all = true;
	for (GraphNode *n : nodes_)
	{
		const QVariant v = s.value(QStringLiteral("graph/%1").arg(n->map()));
		if (v.isValid())
			n->setPos(v.toPointF());
		else
			all = false;
	}
	if (!all)
		arrange();
	for (GraphEdge *e : edges_)
		e->finish();
	setCurrentMap(current_);
}

// A force directed layout (Fruchterman and Reingold) of one group of joined maps: nodes push
// apart, arrows pull together, cooling over the steps. It starts from a circle, so it gives the
// same picture each time.
static std::vector<QPointF> layoutGroup(int n, const std::vector<std::pair<int, int>> &springs)
{
	std::vector<QPointF> pos(static_cast<size_t>(n)), disp(static_cast<size_t>(n));
	for (int i = 0; i < n; i++)
	{
		const qreal angle = 2 * M_PI * i / n;
		pos[i] = QPointF(std::cos(angle), std::sin(angle)) * (40.0 * n);
	}
	const qreal k = 120;	// the length an arrow likes to be
	qreal temperature = 100;
	for (int step = 0; step < 800 && n > 1; step++)
	{
		std::fill(disp.begin(), disp.end(), QPointF());
		for (int i = 0; i < n; i++)
			for (int j = i + 1; j < n; j++)
			{
				QPointF d = pos[i] - pos[j];
				qreal len = std::max<qreal>(1, std::hypot(d.x(), d.y()));
				QPointF push = d / len * (k * k / len);
				disp[i] += push;
				disp[j] -= push;
			}
		for (auto [a, b] : springs)
		{
			QPointF d = pos[a] - pos[b];
			qreal len = std::max<qreal>(1, std::hypot(d.x(), d.y()));
			QPointF pull = d / len * (len * len / k);
			disp[a] -= pull;
			disp[b] += pull;
		}
		for (int i = 0; i < n; i++)
		{
			qreal len = std::hypot(disp[i].x(), disp[i].y());
			if (len > 0)
				pos[i] += disp[i] / len * std::min(len, temperature);
		}
		temperature = std::max<qreal>(1, temperature * 0.99);
	}
	return pos;
}

// Each group of joined maps is laid out by itself; the groups are then packed in rows, the
// biggest first, and the maps with no arrows go in rows below.
void TransitionGraph::arrange()
{
	QList<GraphNode *> all = nodes_.values();
	std::sort(all.begin(), all.end(), [](GraphNode *a, GraphNode *b) { return a->map() < b->map(); });
	QHash<GraphNode *, int> group;
	QList<QList<GraphNode *>> groups;
	QList<GraphNode *> alone;
	for (GraphNode *start : all)
	{
		if (group.contains(start))
			continue;
		if (start->edges().isEmpty())
		{
			alone << start;
			continue;
		}
		QList<GraphNode *> members{start};
		group[start] = groups.size();
		for (int i = 0; i < members.size(); i++)
			for (GraphEdge *e : members[i]->edges())
				for (GraphNode *other : {e->from(), e->to()})
					if (!group.contains(other))
					{
						group[other] = groups.size();
						members << other;
					}
		groups << members;
	}
	std::stable_sort(groups.begin(), groups.end(), [](const auto &a, const auto &b) { return a.size() > b.size(); });

	const qreal gap = 110, rowWidth = 1600;
	QPointF cursor(0, 0);
	qreal rowHeight = 0;
	for (const QList<GraphNode *> &members : groups)
	{
		QHash<GraphNode *, int> at;
		for (int i = 0; i < members.size(); i++)
			at[members[i]] = i;
		std::vector<std::pair<int, int>> springs;
		for (GraphEdge *e : edges_)
			if (at.contains(e->from()) && at.contains(e->to()))
				springs.push_back({at[e->from()], at[e->to()]});
		std::vector<QPointF> pos = layoutGroup(members.size(), springs);
		QRectF box;
		for (const QPointF &p : pos)
			box |= QRectF(p, QSizeF(1, 1));
		if (cursor.x() > 0 && cursor.x() + box.width() > rowWidth)
		{
			cursor = QPointF(0, cursor.y() + rowHeight + gap);
			rowHeight = 0;
		}
		for (int i = 0; i < members.size(); i++)
			members[i]->setPos(cursor + pos[i] - box.topLeft());
		cursor.rx() += box.width() + gap;
		rowHeight = std::max(rowHeight, box.height());
	}
	cursor = QPointF(0, cursor.y() + rowHeight + gap);
	const int perRow = int(rowWidth / 90);
	for (int i = 0; i < alone.size(); i++)
		alone[i]->setPos(cursor + QPointF((i % perRow) * 90, (i / perRow) * 90));
}

void TransitionGraph::fit()
{
	view_->fitInView(scene_->itemsBoundingRect().adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
}

void TransitionGraph::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	if (!fitted_)
	{
		fitted_ = true;
		fit();
	}
}

void TransitionGraph::savePositions()
{
	QSettings s;
	for (GraphNode *n : nodes_)
		s.setValue(QStringLiteral("graph/%1").arg(n->map()), n->pos());
}

QImage TransitionGraph::grabGraph(QSize size)
{
	resize(size);
	fit();
	return grab().toImage();
}
