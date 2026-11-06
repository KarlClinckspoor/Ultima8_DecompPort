#include "ui/searchpanel.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
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

// Maps sort by number, the rest as text. The name's tooltip is made when it is first asked
// for: a search may list thousands of rows, and each tooltip runs the item's usecode.
class ResultRow : public QTreeWidgetItem
{
public:
	explicit ResultRow(const World *world) : world_(world) {}

	bool operator<(const QTreeWidgetItem &o) const override
	{
		const int c = treeWidget() ? treeWidget()->sortColumn() : NameColumn;
		if (c == MapColumn)
			return data(c, SortRole).toInt() < o.data(c, SortRole).toInt();
		return QTreeWidgetItem::operator<(o);
	}

	QVariant data(int column, int role) const override
	{
		if (role == Qt::ToolTipRole && column == NameColumn && world_)
		{
			const int map = QTreeWidgetItem::data(NameColumn, MapRole).toInt();
			const int item = QTreeWidgetItem::data(NameColumn, ItemRole).toInt();
			if (item >= 0 && size_t(item) < world_->map(map).items.size())
				return Names::itemTooltip(*world_, map, item);
		}
		return QTreeWidgetItem::data(column, role);
	}

private:
	const World *world_;
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
	case Term::Family: return familyName(type.family).contains(t.text, Qt::CaseInsensitive);
	case Term::Class:
		return world.usecode().className(world.classOf(it)).contains(t.text, Qt::CaseInsensitive) ||
			world.usecode().className(it.shape).contains(t.text, Qt::CaseInsensitive);
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
		// What costs nothing first; the names run the item's usecode the first time.
		if (Names::shapeName(it.shape).contains(t.text, Qt::CaseInsensitive))
			return true;
		if (world.usecode().className(world.classOf(it)).contains(t.text, Qt::CaseInsensitive))
			return true;
		if (familyName(type.family).contains(t.text, Qt::CaseInsensitive))
			return true;
		if (Names::itemName(world, map, it).contains(t.text, Qt::CaseInsensitive))
			return true;
		return world.itemName(map, it, false).contains(t.text, Qt::CaseInsensitive);
	}
	}
	return false;
}

}

namespace
{

int kindOf(const MapItem &it)
{
	switch (it.source)
	{
	case MapItem::Fixed: return SearchPanel::Fixed;
	case MapItem::Glob: return SearchPanel::Globs;
	case MapItem::NonFixed: return SearchPanel::Movable;
	case MapItem::Npc: return SearchPanel::Npcs;
	}
	return SearchPanel::Fixed;
}

}

// A search under way: where it has got to and what it has found.
struct SearchPanel::Job
{
	QVector<Term> terms;
	QVector<int> maps;
	int mapIndex = 0;
	size_t item = 0;
	bool inside = true;
	int kinds = AllKinds;
	int found = 0;
	QList<QTreeWidgetItem *> rows;
};

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
	go_ = new QPushButton(tr("Find"));
	go_->setToolTip(tr("Find (Enter); while a search runs, stop it"));
	kinds_ = QSettings().value(QStringLiteral("search/kinds"), int(AllKinds)).toInt() & AllKinds;
	const struct { Kind kind; const char *name, *tip; } kindRows[] = {
		{Fixed, QT_TR_NOOP("Fixed"), QT_TR_NOOP("Fixed items: land, walls, buildings and eggs (FIXED.DAT)")},
		{Globs, QT_TR_NOOP("Globs"), QT_TR_NOOP("The floors and scenery glob eggs put down: most of the items on many "
			"maps; leave them out for a faster search")},
		{Movable, QT_TR_NOOP("Movable"), QT_TR_NOOP("Movable items (NONFIXED.DAT, or the saved game's item cache)")},
		{Npcs, QT_TR_NOOP("NPCs"), QT_TR_NOOP("NPCs and what they carry")},
	};
	auto *kindRow = new QHBoxLayout;
	kindRow->setSpacing(2);
	kindRow->addWidget(new QLabel(tr("Look in:")));
	for (int k = 0; k < 4; k++)
	{
		auto *b = new QToolButton;
		b->setText(tr(kindRows[k].name));
		b->setToolTip(tr(kindRows[k].tip));
		b->setCheckable(true);
		b->setChecked(kinds_ & kindRows[k].kind);
		b->setAutoRaise(true);
		const Kind kind = kindRows[k].kind;
		connect(b, &QToolButton::toggled, this, [this, kind](bool on) {
			setKinds(on ? (kinds_ | kind) : (kinds_ & ~kind));
		});
		kindButtons_[k] = b;
		kindRow->addWidget(b);
	}
	kindRow->addStretch();
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
	connect(go_, &QPushButton::clicked, this, [this] {
		if (searching())
			stop();
		else
			search();
	});
	connect(scope_, &QComboBox::currentIndexChanged, this, [this] {
		if (!query_->text().trimmed().isEmpty())
			search();
	});

	auto *row = new QHBoxLayout;
	row->addWidget(query_, 1);
	row->addWidget(go_);
	auto *row2 = new QHBoxLayout;
	row2->addWidget(scope_);
	row2->addWidget(contents_);
	row2->addStretch();
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(4, 4, 4, 4);
	v->addLayout(row);
	v->addLayout(row2);
	v->addLayout(kindRow);
	v->addWidget(count_);
	v->addWidget(results_, 1);
}

SearchPanel::~SearchPanel()
{
	if (!job_)
		return;
	qDeleteAll(job_->rows);
	delete job_;
	QApplication::restoreOverrideCursor();
}

void SearchPanel::setWorld(const World *world)
{
	end(true);
	world_ = world;
	results_->clear();
	count_->clear();
	found_ = 0;
}

void SearchPanel::setKinds(int kinds)
{
	kinds &= AllKinds;
	if (kinds == kinds_)
		return;
	kinds_ = kinds;
	QSettings().setValue(QStringLiteral("search/kinds"), kinds_);
	const Kind order[] = {Fixed, Globs, Movable, Npcs};
	for (int k = 0; k < 4; k++)
	{
		const QSignalBlocker hold(kindButtons_[k]);
		kindButtons_[k]->setChecked(kinds_ & order[k]);
	}
	if (!query_->text().trimmed().isEmpty() && world_)
		search();
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
	end(true);
	results_->clear();
	found_ = 0;
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
	if (!kinds_)
	{
		count_->setText(tr("Turn on at least one kind of item to look in."));
		return;
	}
	job_ = new Job;
	job_->terms = terms;
	if (scope_->currentIndex() == 1)
		job_->maps = world_->mapNumbers();
	else if (map_ >= 0)
		job_->maps << map_;
	job_->inside = contents_->isChecked();
	job_->kinds = kinds_;
	go_->setText(tr("Stop"));
	QApplication::setOverrideCursor(Qt::BusyCursor);
	step();
}

void SearchPanel::step()
{
	if (!job_)
		return;
	Job &j = *job_;
	QElapsedTimer slice;
	slice.start();
	int checked = 0;
	while (j.mapIndex < j.maps.size())
	{
		const int map = j.maps[j.mapIndex];
		const MapData &m = world_->map(map);
		for (; j.item < m.items.size(); j.item++)
		{
			// The window answers between slices of about 30 ms.
			if ((++checked & 255) == 0 && slice.elapsed() > 30)
			{
				count_->setText(tr("Searching map %1 (%2 of %3): %n found so far...", nullptr, j.found)
					.arg(Names::mapShort(map)).arg(j.mapIndex + 1).arg(j.maps.size()));
				QTimer::singleShot(0, this, &SearchPanel::step);
				return;
			}
			const MapItem &it = m.items[j.item];
			if (it.deleted || (!it.onMap() && !(j.inside && it.contained())) || !(kindOf(it) & j.kinds))
				continue;
			bool all = true;
			for (const Term &t : j.terms)
				if (!matchTerm(*world_, map, it, t))
				{
					all = false;
					break;
				}
			if (!all || ++j.found > MaxResults)
				continue;
			const int i = int(j.item);
			auto *row = new ResultRow(world_);
			row->setText(NameColumn, Names::itemName(*world_, map, it));
			row->setData(NameColumn, MapRole, map);
			row->setData(NameColumn, ItemRole, i);
			row->setText(MapColumn, Names::mapShort(map));
			row->setData(MapColumn, SortRole, map);
			row->setText(ShapeColumn, QStringLiteral("%1:%2").arg(it.shape).arg(it.frame));
			if (it.contained())
			{
				const MapItem &r = m.items[m.root(i)];
				row->setText(PositionColumn, QStringLiteral("%1, %2, %3").arg(r.x).arg(r.y).arg(r.z));
				row->setText(InsideColumn, Names::itemName(*world_, map, m.items[it.parent]));
			}
			else
				row->setText(PositionColumn, QStringLiteral("%1, %2, %3").arg(it.x).arg(it.y).arg(it.z));
			j.rows << row;
		}
		j.mapIndex++;
		j.item = 0;
	}
	end(false);
}

void SearchPanel::stop()
{
	end(true);
}

// Lists what the search found and says how much; stopped says it did not get to the end.
void SearchPanel::end(bool stopped)
{
	if (!job_)
		return;
	Job *j = job_;
	job_ = nullptr;
	found_ = j->found;
	results_->setSortingEnabled(false);
	results_->addTopLevelItems(j->rows);
	results_->setSortingEnabled(true);
	results_->sortByColumn(MapColumn, Qt::AscendingOrder);
	for (int c : {MapColumn, ShapeColumn, PositionColumn})
		results_->resizeColumnToContents(c);
	QApplication::restoreOverrideCursor();
	go_->setText(tr("Find"));
	QString text;
	if (j->found > MaxResults)
		text = tr("%1 found; the first %2 are listed.").arg(j->found).arg(MaxResults);
	else
		text = tr("%n found", nullptr, j->found) +
			(j->maps.size() > 1 ? tr(" in %n map(s)", nullptr, int(j->maps.size())) : QString());
	if (stopped)
		text = tr("Stopped at map %1 of %2. ").arg(qMin(j->mapIndex + 1, int(j->maps.size()))).arg(j->maps.size()) + text;
	if (j->kinds != AllKinds)
		text += tr(" (some kinds of item left out)");
	count_->setText(text);
	delete j;
	emit finished();
}
