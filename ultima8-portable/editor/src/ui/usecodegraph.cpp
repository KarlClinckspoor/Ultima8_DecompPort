#include "ui/usecodegraph.h"

#include <QFontMetricsF>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

#include "data/ucode.h"
#include "data/world.h"
#include "ui/flagsview.h"

namespace
{

const QColor BoxFill(28, 33, 40), BoxLine(90, 101, 115), Text(216, 222, 230), Dim(122, 132, 146);
const QColor StringText(143, 209, 143), NoteText(143, 184, 255), HeadText(232, 200, 96);
const QColor TrueEdge(80, 200, 110), FalseEdge(235, 85, 75), JumpEdge(90, 150, 255), NextEdge(150, 160, 175);
const QColor ReadArrow(235, 70, 60), WriteArrow(70, 210, 100), Mark(255, 220, 80);
const qreal LayerGap = 56, BoxGap = 36, ArrowLength = 70;

QString lineText(const Ucode::Line &l)
{
	QString t = QStringLiteral("%1  %2").arg(l.at, 4, 16, QLatin1Char('0')).arg(l.op.leftJustified(18, QLatin1Char(' ')) + QLatin1Char(' '));
	if (!l.args.isEmpty())
		t += l.args + QLatin1Char(' ');
	if (l.jump >= 0)
		t += QStringLiteral("%1 ").arg(l.jump, 4, 16, QLatin1Char('0'));
	if (l.callClass >= 0)
		t += QStringLiteral("%1:%2 ").arg(l.callClass).arg(l.callOffset, 4, 16, QLatin1Char('0'));
	return t;
}

QString flagLabel(const Ucode::Line &l)
{
	const QString name = FlagsView::flagName(l.flagBit, l.flagWidth);
	return name.isEmpty() ? QObject::tr("flag %1").arg(l.flagBit) : QObject::tr("flag %1: %2").arg(l.flagBit).arg(name);
}

QColor edgeColor(Ucode::Edge::Kind k)
{
	switch (k)
	{
	case Ucode::Edge::Taken: return FalseEdge;	// "if false, go to": taken when the test fails
	case Ucode::Edge::NotTaken: return TrueEdge;
	case Ucode::Edge::Jump: return JumpEdge;
	case Ucode::Edge::Next: return NextEdge;
	}
	return NextEdge;
}

// An arrowhead at tip, pointing along the line from from.
void arrowHead(QPainterPath &path, QPointF from, QPointF tip, qreal size)
{
	const QLineF back(tip, from);
	if (back.length() <= 0)
		return;
	QLineF a = back, b = back;
	a.setLength(size);
	a.setAngle(back.angle() + 25);
	b.setLength(size);
	b.setAngle(back.angle() - 25);
	path.moveTo(tip);
	path.lineTo(a.p2());
	path.lineTo(b.p2());
	path.closeSubpath();
}

// The box that draws one block's lines.
class BlockItem : public QGraphicsItem
{
public:
	BlockItem(const QVector<Ucode::Line> &lines, const Ucode::Block &block, const QString &head, QSizeF size,
		const QFont &font, qreal lineHeight, qreal pad, int mark)
		: lines_(lines), block_(block), head_(head), size_(size), font_(font), lineHeight_(lineHeight), pad_(pad), mark_(mark)
	{
	}
	QRectF boundingRect() const override { return QRectF(QPointF(0, 0), size_).adjusted(-2, -2, 2, 2); }
	void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
	{
		p->setPen(QPen(BoxLine, 1));
		p->setBrush(BoxFill);
		p->drawRect(QRectF(QPointF(0, 0), size_));
		p->setFont(font_);
		const QFontMetricsF fm(font_);
		qreal y = pad_;
		if (!head_.isEmpty())
		{
			p->setPen(HeadText);
			p->drawText(QPointF(pad_, y + fm.ascent()), head_);
			y += lineHeight_;
		}
		for (int k = block_.first; k <= block_.last; k++)
		{
			const Ucode::Line &l = lines_[k];
			if (k == mark_)
			{
				p->fillRect(QRectF(1, y, size_.width() - 2, lineHeight_), QColor(Mark.red(), Mark.green(), Mark.blue(), 50));
			}
			const QString t = lineText(l);
			p->setPen(Dim);
			p->drawText(QPointF(pad_, y + fm.ascent()), t.left(4));
			p->setPen(l.flagBit >= 0 ? (l.flagWrite ? WriteArrow : ReadArrow) : Text);
			p->drawText(QPointF(pad_ + fm.horizontalAdvance(t.left(6)), y + fm.ascent()), t.mid(6));
			if (!l.note.isEmpty())
			{
				p->setPen(l.op.contains(QLatin1String("string")) ? StringText : NoteText);
				p->drawText(QPointF(pad_ + fm.horizontalAdvance(t), y + fm.ascent()), l.note);
			}
			y += lineHeight_;
		}
	}

private:
	const QVector<Ucode::Line> &lines_;
	Ucode::Block block_;
	QString head_;
	QSizeF size_;
	QFont font_;
	qreal lineHeight_, pad_;
	int mark_;
};

}

UsecodeGraph::UsecodeGraph(QWidget *parent) : QGraphicsView(parent)
{
	scene_ = new QGraphicsScene(this);
	setScene(scene_);
	setDragMode(QGraphicsView::ScrollHandDrag);
	setRenderHint(QPainter::Antialiasing, true);
	setRenderHint(QPainter::TextAntialiasing, true);
	setBackgroundBrush(QColor(14, 16, 20));
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	QFont mono(QStringLiteral("monospace"));
	mono.setStyleHint(QFont::TypeWriter);
	setFont(mono);
	const QFontMetricsF fm(mono);
	lineHeight_ = std::ceil(fm.height() + 1);
	charWidth_ = fm.horizontalAdvance(QLatin1Char('m'));
	setToolTip(tr("Wheel to zoom, drag to move, 0 to fit. Double-click a line to follow its call, branch or flag.\n"
		"Green: the test holds; red: it fails; blue: go to. A red arrow in: a flag read; a green arrow out: a flag written."));
}

void UsecodeGraph::showRoutine(const World *world, int classId, const QVector<Ucode::Line> &lines, int head, int offset)
{
	Q_UNUSED(world);
	scene_->clear();
	boxes_.clear();
	reads_ = writes_ = 0;
	classId_ = classId;
	lines_ = lines;
	blocks_ = Ucode::flowGraph(lines_, head);
	const int n = blocks_.size();
	if (n == 0)
		return;
	const QFont mono = font();
	const QFontMetricsF fm(mono);

	// Back edges (loops): an edge to a block whose walk is still open.
	QVector<QVector<bool>> back(n);
	for (int b = 0; b < n; b++)
		back[b].fill(false, blocks_[b].out.size());
	QVector<int> state(n, 0), order;
	std::function<void(int)> walk = [&](int b) {
		state[b] = 1;
		for (int e = 0; e < blocks_[b].out.size(); e++)
		{
			const int t = blocks_[b].out[e].to;
			if (state[t] == 1)
				back[b][e] = true;
			else if (state[t] == 0)
				walk(t);
		}
		state[b] = 2;
		order.prepend(b);
	};
	for (int b = 0; b < n; b++)
		if (!state[b])
			walk(b);

	// Layers: the longest path from the start, over forward edges.
	QVector<int> layer(n, 0);
	for (int b : order)
		for (int e = 0; e < blocks_[b].out.size(); e++)
			if (!back[b][e])
				layer[blocks_[b].out[e].to] = std::max(layer[blocks_[b].out[e].to], layer[b] + 1);
	const int layers = *std::max_element(layer.begin(), layer.end()) + 1;
	QVector<QVector<int>> rows(layers);
	for (int b = 0; b < n; b++)
		rows[layer[b]].append(b);

	// Sizes, with room beside each box for its flags' arrows and labels.
	QVector<QSizeF> size(n);
	QVector<qreal> leftRoom(n, 0), rightRoom(n, 0);
	for (int b = 0; b < n; b++)
	{
		qreal w = 0;
		int count = blocks_[b].last - blocks_[b].first + 1;
		for (int k = blocks_[b].first; k <= blocks_[b].last; k++)
		{
			const Ucode::Line &l = lines_[k];
			w = std::max(w, fm.horizontalAdvance(lineText(l) + l.note));
			if (l.flagBit >= 0)
				(l.flagWrite ? rightRoom[b] : leftRoom[b]) = std::max(l.flagWrite ? rightRoom[b] : leftRoom[b],
					ArrowLength + fm.horizontalAdvance(flagLabel(l)) + 8);
		}
		if (b == 0)
			count++;	// the heading
		size[b] = QSizeF(w + 2 * pad_, count * lineHeight_ + 2 * pad_);
	}

	// Order in each layer by where the blocks above lead from, a few times down.
	QVector<qreal> pos(n, 0);
	for (int r = 0; r < layers; r++)
		for (int k = 0; k < rows[r].size(); k++)
			pos[rows[r][k]] = k;
	for (int sweep = 0; sweep < 4; sweep++)
		for (int r = 1; r < layers; r++)
		{
			QHash<int, qreal> bary;
			for (int b : rows[r])
			{
				qreal sum = 0;
				int c = 0;
				for (int a = 0; a < n; a++)
					for (int e = 0; e < blocks_[a].out.size(); e++)
						if (blocks_[a].out[e].to == b && !back[a][e])
						{
							sum += pos[a];
							c++;
						}
				bary[b] = c ? sum / c : pos[b];
			}
			std::stable_sort(rows[r].begin(), rows[r].end(), [&](int x, int y) { return bary[x] < bary[y]; });
			for (int k = 0; k < rows[r].size(); k++)
				pos[rows[r][k]] = k;
		}

	// Places: each layer a row, each block under the middle of those it comes from, kept apart.
	QVector<QPointF> at(n);
	qreal y = 0;
	QVector<qreal> centre(n, 0);
	for (int r = 0; r < layers; r++)
	{
		qreal tallest = 0;
		QVector<qreal> want;
		for (int b : rows[r])
		{
			qreal sum = 0;
			int c = 0;
			for (int a = 0; a < n; a++)
				for (int e = 0; e < blocks_[a].out.size(); e++)
					if (blocks_[a].out[e].to == b && !back[a][e] && layer[a] < r)
					{
						sum += centre[a];
						c++;
					}
			want << (c ? sum / c : 0);
			tallest = std::max(tallest, size[b].height());
		}
		// Left to right, no closer than the gap; then the whole row moved to sit on its wishes.
		qreal x = -1e9, shiftSum = 0;
		QVector<qreal> left(rows[r].size());
		for (int k = 0; k < rows[r].size(); k++)
		{
			const int b = rows[r][k];
			const qreal l = std::max(want[k] - size[b].width() / 2, x + leftRoom[b]);
			left[k] = l;
			x = l + size[b].width() + rightRoom[b] + BoxGap;
			shiftSum += want[k] - (l + size[b].width() / 2);
		}
		const qreal shift = rows[r].isEmpty() ? 0 : shiftSum / rows[r].size();
		for (int k = 0; k < rows[r].size(); k++)
		{
			const int b = rows[r][k];
			at[b] = QPointF(left[k] + shift, y);
			centre[b] = at[b].x() + size[b].width() / 2;
		}
		y += tallest + LayerGap;
	}

	// The boxes.
	int mark = -1;
	for (int k = 0; k < lines_.size(); k++)
		if (lines_[k].kind == Ucode::Line::Code && lines_[k].at == offset)
			mark = k;
	const Ucode::Line &h = lines_[head];
	const QString headText = h.event >= 0
		? tr("event %1: %2").arg(h.event).arg(Ucode::eventName(h.event).isEmpty() ? tr("(never sent)") : Ucode::eventName(h.event))
		: tr("routine %1").arg(h.at, 4, 16, QLatin1Char('0'));
	for (int b = 0; b < n; b++)
	{
		auto *item = new BlockItem(lines_, blocks_[b], b == 0 ? headText : QString(), size[b], mono, lineHeight_, pad_, mark);
		item->setPos(at[b]);
		item->setZValue(1);
		scene_->addItem(item);
		boxes_.append({QRectF(at[b], size[b]), b});
	}

	// The flags: reads come in from the left in red, writes go out to the right in green.
	for (int b = 0; b < n; b++)
	{
		qreal ly = at[b].y() + pad_ + (b == 0 ? lineHeight_ : 0);
		for (int k = blocks_[b].first; k <= blocks_[b].last; k++, ly += lineHeight_)
		{
			const Ucode::Line &l = lines_[k];
			if (l.flagBit < 0)
				continue;
			const qreal my = ly + lineHeight_ / 2;
			const QColor c = l.flagWrite ? WriteArrow : ReadArrow;
			QPointF from, to;
			if (l.flagWrite)
			{
				from = QPointF(at[b].x() + size[b].width(), my);
				to = from + QPointF(ArrowLength, 0);
				writes_++;
			}
			else
			{
				to = QPointF(at[b].x(), my);
				from = to - QPointF(ArrowLength, 0);
				reads_++;
			}
			QPainterPath path(from);
			path.lineTo(to);
			QPainterPath head;
			arrowHead(head, from, to, 9);
			auto *line = scene_->addPath(path, QPen(c, 2));
			auto *tip = scene_->addPath(head, QPen(c, 1), c);
			line->setZValue(2);
			tip->setZValue(2);
			auto *label = scene_->addSimpleText(flagLabel(l), mono);
			label->setBrush(c);
			const QRectF lr = label->boundingRect();
			label->setPos(l.flagWrite ? QPointF(to.x() + 6, my - lr.height() / 2) : QPointF(from.x() - lr.width() - 6, my - lr.height() / 2));
			label->setZValue(2);
			const QString tip2 = l.flagWrite ? tr("Writes %1 (%2 bits)").arg(flagLabel(l)).arg(l.flagWidth)
				: tr("Reads %1 (%2 bits)").arg(flagLabel(l)).arg(l.flagWidth);
			line->setToolTip(tip2);
			label->setToolTip(tip2);
		}
	}

	// The branches: down from the bottom of a box to the top of the next, loops around the left.
	QVector<int> inCount(n, 0), inUsed(n, 0);
	for (int b = 0; b < n; b++)
		for (const Ucode::Edge &e : blocks_[b].out)
			inCount[e.to]++;
	qreal leftmost = 1e9;
	for (int b = 0; b < n; b++)
		leftmost = std::min(leftmost, at[b].x() - leftRoom[b]);
	int loops = 0, detours = 0;
	for (int b = 0; b < n; b++)
	{
		const auto &out = blocks_[b].out;
		for (int e = 0; e < out.size(); e++)
		{
			const int t = out[e].to;
			const QPointF start(at[b].x() + size[b].width() * (e + 1) / (out.size() + 1), at[b].y() + size[b].height());
			const QPointF end(at[t].x() + size[t].width() * (++inUsed[t]) / (inCount[t] + 1), at[t].y());
			QPainterPath path(start);
			if (back[b][e] || end.y() <= start.y())
			{
				// Round the left of everything, a little further out for each loop.
				const qreal x = leftmost - 24 - 12 * loops++;
				path.lineTo(start + QPointF(0, 14));
				path.lineTo(QPointF(x, start.y() + 14));
				path.lineTo(QPointF(x, end.y() - 14));
				path.lineTo(QPointF(end.x(), end.y() - 14));
				path.lineTo(end);
			}
			else
			{
				// A box in a layer between would be crossed: go round it on the right, in a
				// channel of the edge's own.
				qreal channel = -1e9;
				for (int o = 0; o < n; o++)
				{
					if (layer[o] <= layer[b] || layer[o] >= layer[t])
						continue;
					const qreal x0 = at[o].x() - 4, x1 = at[o].x() + size[o].width() + 4;
					const qreal lo = std::min(start.x(), end.x()), hi = std::max(start.x(), end.x());
					if (x1 >= lo && x0 <= hi)
						channel = std::max(channel, x1 + rightRoom[o]);
				}
				if (channel > -1e9)
				{
					const qreal x = channel + 14 + 8 * (detours++ % 6);
					path.lineTo(start + QPointF(0, 12));
					path.lineTo(QPointF(x, start.y() + 12));
					path.lineTo(QPointF(x, end.y() - 12));
					path.lineTo(QPointF(end.x(), end.y() - 12));
					path.lineTo(end);
				}
				else
				{
					const qreal mid = (end.y() - start.y()) / 2;
					path.cubicTo(start + QPointF(0, mid), end - QPointF(0, mid), end);
				}
			}
			const QColor c = edgeColor(out[e].kind);
			auto *item = scene_->addPath(path, QPen(c, 1.6));
			QPainterPath head;
			arrowHead(head, end - QPointF(0, 10), end, 8);
			scene_->addPath(head, QPen(c, 1), c);
			item->setZValue(0);
			item->setToolTip(QStringLiteral("%1 → %2").arg(blocks_[b].offset, 4, 16, QLatin1Char('0'))
				.arg(blocks_[t].offset, 4, 16, QLatin1Char('0')));
		}
	}

	scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-40, -40, 40, 40));
	resetTransform();
	if (mark >= 0)
		centerOnLine(mark);
	else
		fit();
}

void UsecodeGraph::fit()
{
	if (boxes_.isEmpty())
		return;
	fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
	// Not larger than actual size; when that makes it too small to read, the start at actual size.
	if (transform().m11() > 1)
		resetTransform();
	if (transform().m11() < 0.35)
	{
		resetTransform();
		centerOn(boxes_.first().rect.center().x(), boxes_.first().rect.top() + viewport()->height() / 2.0 - 20);
	}
}

void UsecodeGraph::centerOnLine(int line)
{
	for (const Box &b : boxes_)
	{
		const Ucode::Block &bl = blocks_[b.block];
		if (line >= bl.first && line <= bl.last)
		{
			const qreal y = b.rect.top() + pad_ + (b.block == 0 ? lineHeight_ : 0) + (line - bl.first) * lineHeight_;
			centerOn(b.rect.center().x(), y);
			return;
		}
	}
}

int UsecodeGraph::lineAt(QPointF scene) const
{
	for (const Box &b : boxes_)
	{
		if (!b.rect.contains(scene))
			continue;
		const Ucode::Block &bl = blocks_[b.block];
		const int k = bl.first + int((scene.y() - b.rect.top() - pad_ - (b.block == 0 ? lineHeight_ : 0)) / lineHeight_);
		return k >= bl.first && k <= bl.last ? k : -1;
	}
	return -1;
}

void UsecodeGraph::mouseDoubleClickEvent(QMouseEvent *e)
{
	const int k = lineAt(mapToScene(e->position().toPoint()));
	if (k < 0)
	{
		QGraphicsView::mouseDoubleClickEvent(e);
		return;
	}
	const Ucode::Line &l = lines_[k];
	if (l.callClass >= 0)
		emit openClass(l.callClass, l.callOffset);
	else if (l.flagBit >= 0)
		emit showFlag(l.flagBit);
	else if (l.jump >= 0)
	{
		for (int t = 0; t < lines_.size(); t++)
			if (lines_[t].kind == Ucode::Line::Code && lines_[t].at == l.jump)
			{
				centerOnLine(t);
				break;
			}
	}
}

void UsecodeGraph::wheelEvent(QWheelEvent *e)
{
	const qreal steps = e->angleDelta().y() / 120.0;
	const qreal factor = std::pow(1.2, steps);
	const qreal now = transform().m11();
	if (now * factor < 0.05 || now * factor > 4)
		return;
	scale(factor, factor);
}

void UsecodeGraph::keyPressEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_0)
		fit();
	else if (e->key() == Qt::Key_1)
		resetTransform();
	else if (e->key() == Qt::Key_Plus || e->key() == Qt::Key_Equal)
		scale(1.2, 1.2);
	else if (e->key() == Qt::Key_Minus)
		scale(1 / 1.2, 1 / 1.2);
	else
		QGraphicsView::keyPressEvent(e);
}
