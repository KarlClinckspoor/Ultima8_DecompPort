#include "ui/itempanel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

#include "data/world.h"

enum { MapRole = Qt::UserRole, ItemRole };

ItemPanel::ItemPanel(QWidget *parent) : QWidget(parent)
{
	picture_ = new QLabel;
	picture_->setFixedSize(132, 132);
	picture_->setAlignment(Qt::AlignCenter);
	picture_->setStyleSheet("background: #101014; border: 1px solid #333;");
	title_ = new QLabel;
	title_->setWordWrap(true);
	title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	QFont big = title_->font();
	big.setPointSizeF(big.pointSizeF() * 1.25);
	big.setBold(true);
	title_->setFont(big);

	tree_ = new QTreeWidget;
	tree_->setColumnCount(2);
	tree_->setHeaderLabels({tr("Field"), tr("Value")});
	tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	tree_->setAlternatingRowColors(true);
	tree_->setRootIsDecorated(true);
	tree_->setToolTip(tr("Double-click a linked item to show it"));
	connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it) {
		const QVariant m = it->data(0, MapRole);
		if (m.isValid())
			emit goToItem(m.toInt(), it->data(0, ItemRole).toInt());
	});

	center_ = new QPushButton(tr("Show on map"));
	connect(center_, &QPushButton::clicked, this, [this] { emit goToItem(map_, item_); });
	follow_ = new QPushButton;
	connect(follow_, &QPushButton::clicked, this, [this] { emit goToItem(followMap_, followItem_); });

	auto *head = new QHBoxLayout;
	head->addWidget(picture_);
	head->addWidget(title_, 1);
	auto *buttons = new QHBoxLayout;
	buttons->addWidget(center_);
	buttons->addWidget(follow_);
	buttons->addStretch();
	auto *v = new QVBoxLayout(this);
	v->addLayout(head);
	v->addWidget(tree_, 1);
	v->addLayout(buttons);
	showItem(nullptr, -1, -1);
}

QTreeWidgetItem *ItemPanel::section(const QString &title)
{
	auto *s = new QTreeWidgetItem(tree_, {title});
	QFont f = s->font(0);
	f.setBold(true);
	s->setFont(0, f);
	s->setFirstColumnSpanned(true);
	s->setExpanded(true);
	return s;
}

QTreeWidgetItem *ItemPanel::row(QTreeWidgetItem *parent, const QString &field, const QString &value, int map, int item)
{
	auto *r = new QTreeWidgetItem(parent, {field, value});
	if (map >= 0 && item >= 0)
	{
		r->setData(0, MapRole, map);
		r->setData(0, ItemRole, item);
		r->setForeground(1, QColor(110, 170, 255));
	}
	r->setToolTip(1, value);
	return r;
}

static QString sourceName(const MapItem &it)
{
	switch (it.source)
	{
	case MapItem::Fixed: return QObject::tr("FIXED.DAT, record %1").arg(it.record);
	case MapItem::NonFixed: return QObject::tr("NONFIXED.DAT, record %1").arg(it.record);
	case MapItem::Glob: return QObject::tr("glob, item %1").arg(it.record);
	case MapItem::Npc: return QObject::tr("NPC slot %1").arg(it.record);
	}
	return QString();
}

static QString hex(int v, int digits)
{
	return QStringLiteral("%1 (0x%2)").arg(v).arg(v, digits, 16, QLatin1Char('0'));
}

void ItemPanel::showItem(const World *world, int map, int item)
{
	world_ = world;
	map_ = map;
	item_ = item;
	tree_->clear();
	followMap_ = followItem_ = -1;
	follow_->hide();
	const bool valid = world && item >= 0 && item < int(world->map(map).items.size());
	center_->setEnabled(valid);
	if (!valid)
	{
		picture_->clear();
		title_->setText(tr("Click an item on the map."));
		return;
	}
	const MapData &m = world->map(map);
	const MapItem &it = m.items[item];
	const Shapes &shapes = world->shapes();
	const TypeInfo &t = shapes.type(it.shape);
	title_->setText(world->describe(it));

	// The frame, up to twice its size.
	QPixmap pic(picture_->size() - QSize(2, 2));
	pic.fill(QColor(16, 16, 20));
	if (const Frame *f = shapes.frame(it.shape, it.frame))
	{
		QPainter p(&pic);
		const QImage &img = it.flipped() ? f->flipped : f->image;
		qreal s = std::min<qreal>(2, std::min(pic.width() / qreal(f->width), pic.height() / qreal(f->height)));
		QSizeF size = QSizeF(f->width, f->height) * s;
		p.setRenderHint(QPainter::SmoothPixmapTransform, s < 1);
		p.drawImage(QRectF(QPointF((pic.width() - size.width()) / 2, (pic.height() - size.height()) / 2), size), img);
	}
	picture_->setPixmap(pic);

	QTreeWidgetItem *s = section(tr("Item"));
	row(s, tr("Map"), QString::number(map));
	row(s, tr("Index"), QString::number(item));
	row(s, tr("From"), sourceName(it));
	if (it.contained())
		row(s, tr("Depth"), QString::number(it.x));
	else
		row(s, tr("Position"), tr("x %1, y %2, z %3").arg(it.x).arg(it.y).arg(it.z));
	row(s, tr("Shape"), QString::number(it.shape));
	row(s, tr("Frame"), QString::number(it.frame));
	row(s, tr("Status"), QStringLiteral("0x%1: %2").arg(it.flags, 4, 16, QLatin1Char('0')).arg(flagNames(it.flags)));
	row(s, tr("Quality"), hex(it.quality, 4));
	row(s, tr("npcNum"), hex(it.npcNum, 2));
	row(s, tr("mapNum"), hex(it.mapNum, 2));
	if (it.parent >= 0)
	{
		const MapItem &par = m.items[it.parent];
		row(s, it.source == MapItem::Glob ? tr("Glob egg") : tr("Container"), world->describe(par), map, it.parent);
	}

	const int cls = world->classOf(it);
	s = section(tr("Usecode"));
	QString clsName = world->usecode().className(cls);
	row(s, tr("Class"), clsName.isEmpty() ? QString::number(cls) : QStringLiteral("%1 (%2)").arg(clsName).arg(cls));
	row(s, tr("Has code"), world->usecode().hasClass(cls) ? tr("yes") : tr("no"));

	s = section(tr("Type"));
	row(s, tr("Family"), familyName(t.family));
	row(s, tr("Footpad"), tr("%1 x %2 x %3 (world units)").arg(t.worldX()).arg(t.worldY()).arg(t.worldZ()));
	QStringList bits;
	auto add = [&](bool on, const char *name) { if (on) bits << QString::fromLatin1(name); };
	add(t.fixed, "fixed"); add(t.solid, "solid"); add(t.sea, "sea"); add(t.land, "land");
	add(t.occl, "occludes"); add(t.bag, "bag"); add(t.damaging, "damaging"); add(t.noisy, "noisy");
	add(t.draw, "always drawn"); add(t.ignore, "ignore"); add(t.roof, "roof"); add(t.trans, "translucent");
	add(t.editor, "editor only"); add(t.explode, "explodes"); add(t.unknown1, "unknown1"); add(t.unknown2, "unknown2");
	row(s, tr("Flags"), bits.isEmpty() ? tr("none") : bits.join(", "));
	row(s, tr("Weight, volume"), QStringLiteral("%1, %2").arg(t.weight).arg(t.volume));
	if (t.equipType)
		row(s, tr("Equips as"), QString::number(t.equipType));
	if (t.animType)
		row(s, tr("Animation"), tr("type %1, data %2, speed %3").arg(t.animType).arg(t.animData).arg(t.animSpeed));
	row(s, tr("Frames"), QString::number(shapes.frameCount(it.shape)));

	switch (t.family)
	{
	case FamilyTeleportEgg:
		s = section(tr("Teleport egg"));
		row(s, tr("Hatch range"), tr("%1 x %2 (x32 units either side)").arg(EggInfo::xRange(it)).arg(EggInfo::yRange(it)));
		row(s, tr("Teleport id"), QString::number(EggInfo::teleportId(it)));
		if (EggInfo::sends(it))
		{
			row(s, tr("Role"), tr("sends the avatar"));
			row(s, tr("Fade"), (it.quality >> 8) ? tr("yes") : tr("no"));
			const int dm = EggInfo::destMap(it);
			const int de = world->map(dm).receivingEgg(shapes, EggInfo::teleportId(it));
			row(s, tr("Goes to"), de >= 0 ? tr("map %1, egg #%2").arg(dm).arg(EggInfo::teleportId(it))
				: tr("map %1, egg #%2 (not found!)").arg(dm).arg(EggInfo::teleportId(it)), de >= 0 ? dm : -1, de);
			if (de >= 0)
			{
				followMap_ = dm;
				followItem_ = de;
				follow_->setText(tr("Go to destination"));
				follow_->show();
			}
		}
		else
			row(s, tr("Role"), tr("receives the avatar (frame 1)"));
		break;
	case FamilyMonsterEgg:
	{
		s = section(tr("Monster egg"));
		row(s, tr("Hatch range"), tr("%1 x %2 (x32 units either side)").arg(EggInfo::xRange(it)).arg(EggInfo::yRange(it)));
		const int ms = EggInfo::monsterShape(it);
		QString mn = world->usecode().className(ms);
		row(s, tr("Monster shape"), mn.isEmpty() ? QString::number(ms) : QStringLiteral("%1 (%2)").arg(mn).arg(ms));
		row(s, tr("Count"), QString::number(EggInfo::monsterCount(it)));
		row(s, tr("Probability"), QString::number(EggInfo::monsterProb(it)));
		row(s, tr("Activity"), QString::number(EggInfo::monsterActivity(it)));
		row(s, tr("Monster id"), QString::number(EggInfo::monsterId(it)));
		break;
	}
	case FamilyUsecodeEgg:
		s = section(tr("Usecode egg"));
		row(s, tr("Hatch range"), tr("%1 x %2 (x32 units either side)").arg(EggInfo::xRange(it)).arg(EggInfo::yRange(it)));
		row(s, tr("Egg id"), QString::number(it.mapNum));
		row(s, tr("Runs"), tr("class %1, event 7 (hatch)").arg(cls));
		break;
	case FamilyGlobEgg:
	{
		s = section(tr("Glob egg"));
		row(s, tr("Glob"), QString::number(it.quality));
		int n = 0;
		for (const MapItem &g : m.items)
			if (g.source == MapItem::Glob && g.parent == item)
				n++;
		row(s, tr("Items"), QString::number(n));
		break;
	}
	}

	// Ways in and out that involve this item.
	QTreeWidgetItem *ways = nullptr;
	for (const Transition &tr : world->transitions())
	{
		const bool out = tr.fromMap == map && tr.fromItem == item;
		const bool in = tr.toMap == map && tr.toItem == item;
		if (!out && !in)
			continue;
		if (!ways)
			ways = section(QObject::tr("Transitions"));
		QString how = tr.kind == Transition::TeleportEgg ? QObject::tr("teleport egg")
			: QStringLiteral("%1 (%2)").arg(world->usecode().className(tr.classId),
				tr.kind == Transition::UsecodeToEgg ? QStringLiteral("teleportToEgg") : QStringLiteral("Npc::teleport"));
		if (out)
		{
			QString where = tr.toEgg >= 0 ? QObject::tr("map %1, egg #%2").arg(tr.toMap).arg(tr.toEgg)
				: QObject::tr("map %1 at %2, %3, %4").arg(tr.toMap).arg(tr.x).arg(tr.y).arg(tr.z);
			row(ways, QObject::tr("To"), QStringLiteral("%1 by %2").arg(where, how), tr.toItem >= 0 ? tr.toMap : -1, tr.toItem);
		}
		if (in)
		{
			QString from = tr.fromMap >= 0 ? QObject::tr("map %1").arg(tr.fromMap) : QObject::tr("usecode anywhere");
			row(ways, QObject::tr("From"), QStringLiteral("%1 by %2").arg(from, how), tr.fromMap, tr.fromItem);
		}
	}

	if (!it.contents.empty())
	{
		s = section(tr("Contents (%1)").arg(it.contents.size()));
		std::function<void(QTreeWidgetItem *, const MapItem &)> addKids = [&](QTreeWidgetItem *parent, const MapItem &c) {
			for (int k : c.contents)
			{
				const MapItem &kid = m.items[k];
				QString extra = kid.quality ? QStringLiteral(", quality %1").arg(kid.quality) : QString();
				QTreeWidgetItem *r = row(parent, world->describe(kid), tr("index %1%2").arg(k).arg(extra), map, k);
				addKids(r, kid);
			}
		};
		addKids(s, it);
	}
	tree_->resizeColumnToContents(0);
}
