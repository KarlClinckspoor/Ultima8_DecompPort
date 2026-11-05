#include "ui/searchpanel.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "data/world.h"
#include "ui/names.h"

enum { NameColumn, MapColumn, ShapeColumn, PositionColumn, InsideColumn };
enum { MapRole = Qt::UserRole, ItemRole, SortRole };
static const int MaxResults = 3000;

// Maps sort by number, the rest as text.
class ResultRow : public QTreeWidgetItem
{
public:
	bool operator<(const QTreeWidgetItem &o) const override
	{
		const int c = treeWidget() ? treeWidget()->sortColumn() : NameColumn;
		if (c == MapColumn)
			return data(c, SortRole).toInt() < o.data(c, SortRole).toInt();
		return QTreeWidgetItem::operator<(o);
	}
};

namespace
{

struct Term
{
	enum Kind { Text, Shape, Frame, Quality, Npc, MapNum, Id, Key, Family, Class, Is };
	Kind kind = Text;
	QString text;
	int value = 0;
};

// Splits the query into terms; "quoted words" stay together.
QVector<Term> parse(const QString &query, QString *error)
{
	QVector<Term> terms;
	static const QRegularExpression token(QStringLiteral("\"([^\"]*)\"|(\\S+)"));
	auto it = token.globalMatch(query);
	while (it.hasNext())
	{
		const QRegularExpressionMatch m = it.next();
		Term t;
		if (m.capturedStart(1) >= 0)
		{
			t.text = m.captured(1).toLower();
			if (!t.text.isEmpty())
				terms << t;
			continue;
		}
		const QString word = m.captured(2);
		const int colon = word.indexOf(QLatin1Char(':'));
		bool isNumber = false;
		const int number = word.toInt(&isNumber, 0);
		if (colon < 0)
		{
			if (isNumber)
			{
				t.kind = Term::Shape;
				t.value = number;
			}
			else
				t.text = word.toLower();
			terms << t;
			continue;
		}
		const QString key = word.left(colon).toLower(), value = word.mid(colon + 1);
		bool ok = false;
		t.value = value.toInt(&ok, 0);
		t.text = value.toLower();
		static const QHash<QString, Term::Kind> numeric{
			{"shape", Term::Shape}, {"s", Term::Shape}, {"frame", Term::Frame}, {"f", Term::Frame},
			{"quality", Term::Quality}, {"q", Term::Quality}, {"npc", Term::Npc}, {"npcnum", Term::Npc},
			{"mapnum", Term::MapNum}, {"id", Term::Id}, {"key", Term::Key}};
		static const QHash<QString, Term::Kind> textual{{"family", Term::Family}, {"class", Term::Class}, {"is", Term::Is}};
		if (numeric.contains(key))
		{
			if (!ok)
			{
				if (error)
					*error = QObject::tr("%1: wants a number").arg(word);
				return {};
			}
			t.kind = numeric.value(key);
		}
		else if (textual.contains(key))
			t.kind = textual.value(key);
		else
		{
			if (error)
				*error = QObject::tr("%1: unknown term (shape, frame, q, npc, mapnum, id, key, family, class, is)").arg(word);
			return {};
		}
		terms << t;
	}
	return terms;
}

bool matchTerm(const World &world, int map, const MapItem &it, const Term &t)
{
	const TypeInfo &type = world.shapes().type(it.shape);
	switch (t.kind)
	{
	case Term::Shape: return it.shape == t.value;
	case Term::Frame: return it.frame == t.value;
	case Term::Quality: return it.quality == t.value;
	case Term::Npc: return it.npcNum == t.value;
	case Term::MapNum: return it.mapNum == t.value;
	case Term::Id:
		if (type.family == FamilyTeleportEgg)
			return EggInfo::teleportId(it) == t.value;
		if (type.family == FamilyUsecodeEgg)
			return it.mapNum == t.value;
		if (type.family == FamilyMonsterEgg)
			return EggInfo::monsterId(it) == t.value;
		return false;
	case Term::Key:
		return (DoorInfo::isKey(it.shape) && it.quality == t.value) ||
			(DoorInfo::isDoor(it.shape) && DoorInfo::keyOf(it) == t.value);
	case Term::Family: return familyName(type.family).toLower().contains(t.text);
	case Term::Class:
		return world.usecode().className(world.classOf(it)).toLower().contains(t.text) ||
			world.usecode().className(it.shape).toLower().contains(t.text);
	case Term::Is:
		if (t.text == QLatin1String("door"))
			return DoorInfo::isDoor(it.shape);
		if (t.text == QLatin1String("key"))
			return DoorInfo::isKey(it.shape);
		if (t.text == QLatin1String("locked"))
			return DoorInfo::isDoor(it.shape) && DoorInfo::locked(it);
		if (t.text == QLatin1String("egg"))
			return type.family == FamilyTeleportEgg || type.family == FamilyMonsterEgg ||
				type.family == FamilyUsecodeEgg || type.family == FamilyGlobEgg;
		if (t.text == QLatin1String("npc"))
			return it.source == MapItem::Npc && !it.contained();
		if (t.text == QLatin1String("invisible"))
			return it.flags & FlagInvisible;
		if (t.text == QLatin1String("contained"))
			return it.contained();
		if (t.text == QLatin1String("fixed"))
			return it.source == MapItem::Fixed;
		if (t.text == QLatin1String("movable"))
			return it.source == MapItem::NonFixed;
		return false;
	case Term::Text:
	{
		if (Names::itemName(world, map, it).toLower().contains(t.text))
			return true;
		if (world.itemName(map, it, false).toLower().contains(t.text))
			return true;
		if (Names::shapeName(it.shape).toLower().contains(t.text))
			return true;
		if (world.usecode().className(world.classOf(it)).toLower().contains(t.text))
			return true;
		return familyName(type.family).toLower().contains(t.text);
	}
	}
	return false;
}

}

bool SearchPanel::matches(const World &world, int map, int item, const QString &query, QString *error)
{
	QString why;
	const QVector<Term> terms = parse(query, &why);
	if (error)
		*error = why;
	if (terms.isEmpty())
		return false;
	const MapItem &it = world.map(map).items[item];
	for (const Term &t : terms)
		if (!matchTerm(world, map, it, t))
			return false;
	return true;
}

SearchPanel::SearchPanel(QWidget *parent) : QWidget(parent)
{
	query_ = new QLineEdit;
	query_->setPlaceholderText(tr("Find items: words, shape:82, q:9, key:9, is:door..."));
	query_->setClearButtonEnabled(true);
	query_->setToolTip(tr("<p>Words match what items go by: their names, usecode classes, families and the "
		"names given to shapes. A number alone is a shape. Every term must match.</p>"
		"<p><b>shape:</b>N, <b>frame:</b>N, <b>q:</b>N (quality), <b>npc:</b>N, <b>mapnum:</b>N<br>"
		"<b>id:</b>N: a teleport egg's teleport id, a usecode egg's egg id, a monster egg's monster id<br>"
		"<b>key:</b>N: the keys of quality N and the doors they open<br>"
		"<b>family:</b>word, <b>class:</b>word<br>"
		"<b>is:</b>door, key, locked, egg, npc, invisible, contained, fixed, movable</p>"
		"<p>\"quoted words\" stay together. Numbers may be written 0x...</p>"));
	scope_ = new QComboBox;
	scope_->addItem(tr("This map"));
	scope_->addItem(tr("Every map"));
	contents_ = new QCheckBox(tr("Inside containers"));
	contents_->setChecked(true);
	contents_->setToolTip(tr("Also what containers and NPCs hold"));
	auto *go = new QPushButton(tr("Find"));
	count_ = new QLabel;
	count_->setWordWrap(true);

	results_ = new QTreeWidget;
	results_->setColumnCount(5);
	results_->setHeaderLabels({tr("Name"), tr("Map"), tr("Shape"), tr("Position"), tr("Inside")});
	results_->setRootIsDecorated(false);
	results_->setAlternatingRowColors(true);
	results_->setSortingEnabled(true);
	results_->setUniformRowHeights(true);
	results_->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
	results_->setToolTip(tr("Double-click a result to show it"));
	connect(results_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *it) {
		emit goToItem(it->data(NameColumn, MapRole).toInt(), it->data(NameColumn, ItemRole).toInt());
	});

	connect(query_, &QLineEdit::returnPressed, this, &SearchPanel::search);
	connect(go, &QPushButton::clicked, this, &SearchPanel::search);
	connect(scope_, &QComboBox::currentIndexChanged, this, [this] {
		if (!query_->text().trimmed().isEmpty())
			search();
	});

	auto *row = new QHBoxLayout;
	row->addWidget(query_, 1);
	row->addWidget(go);
	auto *row2 = new QHBoxLayout;
	row2->addWidget(scope_);
	row2->addWidget(contents_);
	row2->addStretch();
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(4, 4, 4, 4);
	v->addLayout(row);
	v->addLayout(row2);
	v->addWidget(count_);
	v->addWidget(results_, 1);
}

void SearchPanel::setWorld(const World *world)
{
	world_ = world;
	results_->clear();
	count_->clear();
}

void SearchPanel::run(const QString &query, bool everyMap)
{
	query_->setText(query);
	const QSignalBlocker hold(scope_);
	scope_->setCurrentIndex(everyMap ? 1 : 0);
	search();
}

void SearchPanel::search()
{
	results_->clear();
	if (!world_)
		return;
	QString error;
	const QVector<Term> terms = parse(query_->text(), &error);
	if (!error.isEmpty())
	{
		count_->setText(error);
		return;
	}
	if (terms.isEmpty())
	{
		count_->clear();
		return;
	}
	QApplication::setOverrideCursor(Qt::WaitCursor);
	QVector<int> maps;
	if (scope_->currentIndex() == 1)
		maps = world_->mapNumbers();
	else if (map_ >= 0)
		maps << map_;
	const bool inside = contents_->isChecked();
	int found = 0;
	QList<QTreeWidgetItem *> rows;
	for (int map : maps)
	{
		const MapData &m = world_->map(map);
		for (size_t i = 0; i < m.items.size(); i++)
		{
			const MapItem &it = m.items[i];
			if (it.deleted || (!it.onMap() && !(inside && it.contained())))
				continue;
			bool all = true;
			for (const Term &t : terms)
				if (!matchTerm(*world_, map, it, t))
				{
					all = false;
					break;
				}
			if (!all)
				continue;
			if (++found > MaxResults)
				continue;
			auto *row = new ResultRow;
			row->setText(NameColumn, Names::itemName(*world_, map, it));
			row->setToolTip(NameColumn, world_->describe(it));
			row->setData(NameColumn, MapRole, map);
			row->setData(NameColumn, ItemRole, int(i));
			row->setText(MapColumn, Names::mapShort(map));
			row->setData(MapColumn, SortRole, map);
			row->setText(ShapeColumn, QStringLiteral("%1:%2").arg(it.shape).arg(it.frame));
			if (it.contained())
			{
				const int root = m.root(int(i));
				const MapItem &r = m.items[root];
				row->setText(PositionColumn, QStringLiteral("%1, %2, %3").arg(r.x).arg(r.y).arg(r.z));
				row->setText(InsideColumn, Names::itemName(*world_, map, m.items[it.parent]));
			}
			else
				row->setText(PositionColumn, QStringLiteral("%1, %2, %3").arg(it.x).arg(it.y).arg(it.z));
			rows << row;
		}
	}
	results_->setSortingEnabled(false);
	results_->addTopLevelItems(rows);
	results_->setSortingEnabled(true);
	results_->sortByColumn(MapColumn, Qt::AscendingOrder);
	for (int c : {MapColumn, ShapeColumn, PositionColumn})
		results_->resizeColumnToContents(c);
	QApplication::restoreOverrideCursor();
	if (found > MaxResults)
		count_->setText(tr("%1 found; the first %2 are listed.").arg(found).arg(MaxResults));
	else
		count_->setText(tr("%n found", nullptr, found) + (maps.size() > 1 ? tr(" in %n map(s)", nullptr, int(maps.size())) : QString()));
}
