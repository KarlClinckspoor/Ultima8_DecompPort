#include "ui/itempanel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

enum { MapRole = Qt::UserRole, ItemRole };

ItemPanel::ItemPanel(QWidget *parent) : QWidget(parent)
{
	picture_ = new QLabel;
	picture_->setFixedSize(112, 112);
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

	editPage_ = new QWidget;
	editLayout_ = new QVBoxLayout(editPage_);
	readOnly_ = new QLabel;
	readOnly_->setWordWrap(true);
	readOnly_->setStyleSheet("color: #e0b050;");
	auto *scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(editPage_);

	tabs_ = new QTabWidget;
	tabs_->addTab(scroll, tr("Edit"));
	tabs_->addTab(tree_, tr("Details"));

	center_ = new QPushButton(tr("Show on map"));
	connect(center_, &QPushButton::clicked, this, [this] { emit goToItem(map_, item_); });
	follow_ = new QPushButton;
	connect(follow_, &QPushButton::clicked, this, [this] { emit goToItem(followMap_, followItem_); });
	duplicate_ = new QPushButton(tr("Copy"));
	duplicate_->setToolTip(tr("A copy beside it (Ctrl+D on the map)"));
	connect(duplicate_, &QPushButton::clicked, this, [this] { emit duplicateItem(map_, item_); });
	delete_ = new QPushButton(tr("Delete"));
	delete_->setToolTip(tr("Delete it and what it holds (Delete on the map)"));
	connect(delete_, &QPushButton::clicked, this, [this] { emit deleteItem(map_, item_); });

	auto *head = new QHBoxLayout;
	head->addWidget(picture_);
	head->addWidget(title_, 1);
	auto *buttons = new QHBoxLayout;
	buttons->addWidget(center_);
	buttons->addWidget(follow_);
	buttons->addStretch();
	buttons->addWidget(duplicate_);
	buttons->addWidget(delete_);
	auto *v = new QVBoxLayout(this);
	v->addLayout(head);
	v->addWidget(tabs_, 1);
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
	case MapItem::Fixed: return it.record >= 0 ? QObject::tr("FIXED.DAT, record %1").arg(it.record) : QObject::tr("FIXED.DAT, new");
	case MapItem::NonFixed: return it.record >= 0 ? QObject::tr("NONFIXED.DAT, record %1").arg(it.record) : QObject::tr("NONFIXED.DAT, new");
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
	const bool valid = world && item >= 0 && item < int(world->map(map).items.size()) &&
		!world->map(map).items[item].deleted;
	center_->setEnabled(valid);
	if (!valid)
	{
		item_ = -1;
		tree_->clear();
		picture_->clear();
		title_->setText(tr("Click an item on the map."));
		follow_->hide();
		duplicate_->setEnabled(false);
		delete_->setEnabled(false);
		fields_.clear();
		npcFields_.clear();
		editorKey_.clear();
		buildEditor();
		return;
	}
	refresh();
}

void ItemPanel::refresh()
{
	if (!world_ || item_ < 0 || item_ >= int(world_->map(map_).items.size()) || world_->map(map_).items[item_].deleted)
	{
		showItem(world_, map_, -1);
		return;
	}
	const MapItem &it = world_->map(map_).items[item_];
	const Shapes &shapes = world_->shapes();
	title_->setText(world_->describe(it));
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
	fillDetails();
	const bool editable = world_->readOnlyReason(map_, item_).isEmpty();
	duplicate_->setEnabled(editable && it.source != MapItem::Npc);
	delete_->setEnabled(editable && it.source != MapItem::Npc);
	// The editor is made again only when what it shows changes: the item, its kind, or whether
	// it can be changed; otherwise its values are updated, so typing is not lost.
	const QString key = QStringLiteral("%1/%2/%3/%4/%5").arg(map_).arg(item_)
		.arg(shapes.type(it.shape).family).arg(editable).arg(it.frame == 0);
	if (key != editorKey_)
	{
		editorKey_ = key;
		buildEditor();
	}
	updateEditor();
}

void ItemPanel::fillDetails()
{
	const World *world = world_;
	const int map = map_, item = item_;
	tree_->clear();
	followMap_ = followItem_ = -1;
	follow_->hide();
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
			if (!g.deleted && g.source == MapItem::Glob && g.parent == item)
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

	int held = 0;
	for (int k : it.contents)
		held += !m.items[k].deleted;
	if (held)
	{
		s = section(tr("Contents (%1)").arg(held));
		std::function<void(QTreeWidgetItem *, const MapItem &)> addKids = [&](QTreeWidgetItem *parent, const MapItem &c) {
			for (int k : c.contents)
			{
				const MapItem &kid = m.items[k];
				if (kid.deleted)
					continue;
				QString extra = kid.quality ? QStringLiteral(", quality %1").arg(kid.quality) : QString();
				QTreeWidgetItem *r = row(parent, world->describe(kid), tr("index %1%2").arg(k).arg(extra), map, k);
				addKids(r, kid);
			}
		};
		addKids(s, it);
	}
	tree_->resizeColumnToContents(0);
}

// ---- The editor ----

static void clearLayout(QLayout *layout)
{
	while (QLayoutItem *child = layout->takeAt(0))
	{
		if (QWidget *w = child->widget())
		{
			w->hide();
			w->deleteLater();
		}
		else if (QLayout *l = child->layout())
			clearLayout(l);
		delete child;
	}
}

void ItemPanel::buildEditor()
{
	readOnly_->setParent(nullptr);
	clearLayout(editLayout_);
	fields_.clear();
	npcFields_.clear();
	if (!world_ || item_ < 0)
	{
		auto *hint = new QLabel(tr("Click an item on the map to change it, or pick a shape in the Shapes panel to place one."));
		hint->setWordWrap(true);
		editLayout_->addWidget(hint);
		editLayout_->addStretch();
		return;
	}
	const MapItem &it = world_->map(map_).items[item_];
	const TypeInfo &t = world_->shapes().type(it.shape);
	const QString why = world_->readOnlyReason(map_, item_);
	const bool editable = why.isEmpty();
	if (!editable)
	{
		readOnly_->setText(why);
		editLayout_->addWidget(readOnly_);
		readOnly_->show();
	}

	auto add = [&](std::vector<Field> &into, const QString &label, std::function<int(const MapItem &)> get,
		std::function<void(MapItem &, int)> set, int min, int max, const QString &tip = QString()) {
		Field f{label, get, set, min, max, tip};
		into.push_back(f);
	};
	std::vector<Field> item, special;
	const bool contained = it.contained();
	if (!contained)
	{
		add(item, tr("X"), [](const MapItem &i) { return int(i.x); }, [](MapItem &i, int v) { i.x = uint16_t(v); }, 0, 0x7fff,
			tr("World x: grows down and to the right"));
		add(item, tr("Y"), [](const MapItem &i) { return int(i.y); }, [](MapItem &i, int v) { i.y = uint16_t(v); }, 0, 0x7fff,
			tr("World y: grows down and to the left"));
		add(item, tr("Z"), [](const MapItem &i) { return int(i.z); }, [](MapItem &i, int v) { i.z = uint8_t(v); }, 0, 0xfa,
			tr("Height; 251 and up take an item out of play"));
	}
	add(item, tr("Shape"), [](const MapItem &i) { return int(i.shape); }, [](MapItem &i, int v) { i.shape = uint16_t(v); }, 1, 2047);
	add(item, tr("Frame"), [](const MapItem &i) { return int(i.frame); }, [](MapItem &i, int v) { i.frame = uint8_t(v); }, 0, -1);
	add(item, tr("Quality"), [](const MapItem &i) { return int(i.quality); }, [](MapItem &i, int v) { i.quality = uint16_t(v); }, 0, 0xffff,
		tr("Its meaning depends on the shape: a key's lock, a quantity, an egg's settings..."));
	add(item, tr("npcNum"), [](const MapItem &i) { return int(i.npcNum); }, [](MapItem &i, int v) { i.npcNum = uint8_t(v); }, 0, 254,
		tr("The NPC it belongs to; for eggs, the hatch ranges"));
	add(item, tr("mapNum"), [](const MapItem &i) { return int(i.mapNum); }, [](MapItem &i, int v) { i.mapNum = uint8_t(v); }, 0, 255,
		tr("For eggs, the egg id (a teleport egg's destination map)"));
	auto flag = [&](const QString &label, uint16_t bit, const QString &tip) {
		Field f{label, [bit](const MapItem &i) { return (i.flags & bit) ? 1 : 0; },
			[bit](MapItem &i, int v) { i.flags = uint16_t(v ? (i.flags | bit) : (i.flags & ~bit)); }, 0, 1, tip};
		item.push_back(f);
	};
	flag(tr("Flipped"), FlagFlipped, tr("Mirrored left to right, its footpad's x and y swapped (F on the map)"));
	flag(tr("Invisible"), FlagInvisible, tr("Not drawn in the game"));
	flag(tr("Owned"), FlagOwned, tr("Taking it is stealing"));

	QString specialTitle;
	auto ranges = [&]() {
		add(special, tr("X range"), [](const MapItem &i) { return EggInfo::xRange(i); },
			[](MapItem &i, int v) { i.npcNum = uint8_t((i.npcNum & 0x0f) | v << 4); }, 0, 15,
			tr("It hatches when the avatar is within this many 32-unit steps either side in x"));
		add(special, tr("Y range"), [](const MapItem &i) { return EggInfo::yRange(i); },
			[](MapItem &i, int v) { i.npcNum = uint8_t((i.npcNum & 0xf0) | v); }, 0, 15);
	};
	switch (t.family)
	{
	case FamilyTeleportEgg:
		specialTitle = tr("Teleport egg");
		add(special, tr("Sends"), [](const MapItem &i) { return i.frame == 0 ? 1 : 0; },
			[](MapItem &i, int v) { i.frame = v ? 0 : 1; }, 0, 1, tr("Frame 0 sends the avatar; frame 1 is where it arrives"));
		add(special, tr("Teleport id"), [](const MapItem &i) { return EggInfo::teleportId(i); },
			[](MapItem &i, int v) { i.quality = uint16_t((i.quality & 0xff00) | v); }, 0, 255,
			tr("A sender goes to the receiving egg with the same id on its destination map"));
		if (EggInfo::sends(it))
		{
			add(special, tr("Destination map"), [](const MapItem &i) { return EggInfo::destMap(i); },
				[](MapItem &i, int v) { i.mapNum = uint8_t(v); }, 0, 255);
			add(special, tr("Fade"), [](const MapItem &i) { return (i.quality >> 8) ? 1 : 0; },
				[](MapItem &i, int v) { i.quality = uint16_t((i.quality & 0xff) | (v ? 0x100 : 0)); }, 0, 1,
				tr("Fade the screen out and in"));
			ranges();
		}
		break;
	case FamilyMonsterEgg:
		specialTitle = tr("Monster egg");
		add(special, tr("Monster shape"), [](const MapItem &i) { return EggInfo::monsterShape(i); },
			[](MapItem &i, int v) { i.quality = uint16_t((i.quality & 0xf800) | v); }, 0, 2047);
		add(special, tr("Count"), [](const MapItem &i) { return EggInfo::monsterCount(i); },
			[](MapItem &i, int v) { i.npcNum = uint8_t((i.npcNum & 0xf8) | v); }, 0, 7,
			tr("Shares its bits with the y range"));
		add(special, tr("Probability"), [](const MapItem &i) { return EggInfo::monsterProb(i); },
			[](MapItem &i, int v) { i.quality = uint16_t((i.quality & 0x7ff) | v << 11); }, 0, 31);
		add(special, tr("Activity"), [](const MapItem &i) { return EggInfo::monsterActivity(i); },
			[](MapItem &i, int v) { i.mapNum = uint8_t((i.mapNum & 0xf8) | v); }, 0, 7);
		add(special, tr("Monster id"), [](const MapItem &i) { return EggInfo::monsterId(i); },
			[](MapItem &i, int v) { i.mapNum = uint8_t((i.mapNum & 7) | v << 3); }, 0, 31);
		ranges();
		break;
	case FamilyUsecodeEgg:
		specialTitle = tr("Usecode egg");
		add(special, tr("Egg type"), [](const MapItem &i) { return int(i.quality); },
			[](MapItem &i, int v) { i.quality = uint16_t(v); }, 0, 0xffff,
			tr("Hatching runs usecode class 1151 + this"));
		add(special, tr("Egg id"), [](const MapItem &i) { return int(i.mapNum); },
			[](MapItem &i, int v) { i.mapNum = uint8_t(v); }, 0, 255);
		ranges();
		break;
	case FamilyGlobEgg:
		specialTitle = tr("Glob egg");
		add(special, tr("Glob"), [](const MapItem &i) { return int(i.quality); },
			[](MapItem &i, int v) { i.quality = uint16_t(v); }, 0, 3071,
			tr("The group of items in GLOB.FLX it puts down, placed from the corner of its 512-unit region"));
		break;
	}

	auto makeGroup = [&](const QString &title, std::vector<Field> &list) {
		auto *box = new QGroupBox(title);
		auto *form = new QFormLayout(box);
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		for (Field &f : list)
		{
			QWidget *w;
			if (f.max == 1 && f.min == 0)
			{
				f.check = new QCheckBox;
				w = f.check;
			}
			else
			{
				f.box = new QSpinBox;
				f.box->setRange(f.min, f.max < 0 ? 255 : f.max);
				f.box->setKeyboardTracking(false);
				f.box->setAccelerated(true);
				w = f.box;
			}
			w->setEnabled(editable);
			w->setToolTip(f.tip);
			form->addRow(f.label, w);
		}
		editLayout_->addWidget(box);
		for (Field &f : list)
			fields_.push_back(f);
	};
	if (!special.empty())
		makeGroup(specialTitle, special);
	makeGroup(tr("Item"), item);

	if (it.source == MapItem::Npc && it.parent < 0 && it.record > 0 && it.record < 256 && world_->hasNpcData())
	{
		auto *box = new QGroupBox(tr("NPC %1").arg(it.record));
		auto *form = new QFormLayout(box);
		auto npc = [&](const QString &label, std::function<int(const NpcStats &)> get,
			std::function<void(NpcStats &, int)> set, int min, int max) {
			NpcField f{label, get, set, min, max};
			npcFields_.push_back(f);
		};
		npc(tr("Strength"), [](const NpcStats &s) { return s.str; }, [](NpcStats &s, int v) { s.str = v; }, 0, 127);
		npc(tr("Dexterity"), [](const NpcStats &s) { return s.dex; }, [](NpcStats &s, int v) { s.dex = v; }, 0, 127);
		npc(tr("Intelligence"), [](const NpcStats &s) { return s.intelligence; }, [](NpcStats &s, int v) { s.intelligence = v; }, 0, 127);
		npc(tr("Hit points"), [](const NpcStats &s) { return s.hp; }, [](NpcStats &s, int v) { s.hp = v; }, 0, 255);
		npc(tr("Mana"), [](const NpcStats &s) { return s.mana; }, [](NpcStats &s, int v) { s.mana = v; }, 0, 32767);
		npc(tr("Alignment"), [](const NpcStats &s) { return s.alignment & 0xf; },
			[](NpcStats &s, int v) { s.alignment = (s.alignment & 0xf0) | v; }, 0, 15);
		npc(tr("Enemies"), [](const NpcStats &s) { return s.alignment >> 4; },
			[](NpcStats &s, int v) { s.alignment = (s.alignment & 0x0f) | v << 4; }, 0, 15);
		auto bit = [&](const QString &label, bool second, int mask) {
			npc(label, [=](const NpcStats &s) { return ((second ? s.flags2 : s.flags1) & mask) ? 1 : 0; },
				[=](NpcStats &s, int v) {
					int &w = second ? s.flags2 : s.flags1;
					w = v ? (w | mask) : (w & ~mask);
				}, 0, 1);
		};
		bit(tr("Invincible"), false, NpcStats::Invincible);
		bit(tr("Immortal"), true, NpcStats::Immortal);
		bit(tr("Withstands death"), true, NpcStats::WithstandDeath);
		bit(tr("Poisoned"), true, NpcStats::Poisoned);
		bit(tr("Dead"), true, NpcStats::Dead);
		for (NpcField &f : npcFields_)
		{
			QWidget *w;
			if (f.max == 1)
			{
				f.check = new QCheckBox;
				w = f.check;
			}
			else
			{
				f.box = new QSpinBox;
				f.box->setRange(f.min, f.max);
				f.box->setKeyboardTracking(false);
				w = f.box;
			}
			form->addRow(f.label, w);
		}
		editLayout_->addWidget(box);
	}
	editLayout_->addStretch();

	// Connected once all are made, so the lists no longer move.
	for (size_t i = 0; i < fields_.size(); i++)
	{
		if (fields_[i].box)
			connect(fields_[i].box, &QSpinBox::valueChanged, this, [this, i](int v) { fieldChanged(fields_[i], v); });
		else
			connect(fields_[i].check, &QCheckBox::toggled, this, [this, i](bool v) { fieldChanged(fields_[i], v ? 1 : 0); });
	}
	for (size_t i = 0; i < npcFields_.size(); i++)
	{
		auto changed = [this, i](int v) {
			const int npc = world_->map(map_).items[item_].record;
			NpcStats s = world_->npcStats(npc);
			npcFields_[i].set(s, v);
			emit editNpc(npc, s);
		};
		if (npcFields_[i].box)
			connect(npcFields_[i].box, &QSpinBox::valueChanged, this, changed);
		else
			connect(npcFields_[i].check, &QCheckBox::toggled, this, [changed](bool v) { changed(v ? 1 : 0); });
	}
}

void ItemPanel::updateEditor()
{
	if (!world_ || item_ < 0)
		return;
	const MapItem &it = world_->map(map_).items[item_];
	for (Field &f : fields_)
	{
		const int v = f.get(it);
		if (f.box)
		{
			QSignalBlocker hold(f.box);
			if (f.max < 0)
				f.box->setMaximum(std::max(0, world_->shapes().frameCount(it.shape) - 1));
			f.box->setValue(v);
		}
		else
		{
			QSignalBlocker hold(f.check);
			f.check->setChecked(v != 0);
		}
	}
	if (!npcFields_.empty())
	{
		const NpcStats s = world_->npcStats(it.record);
		for (NpcField &f : npcFields_)
		{
			const int v = f.get(s);
			if (f.box)
			{
				QSignalBlocker hold(f.box);
				f.box->setValue(v);
			}
			else
			{
				QSignalBlocker hold(f.check);
				f.check->setChecked(v != 0);
			}
		}
	}
}

void ItemPanel::fieldChanged(Field &f, int value)
{
	if (!world_ || item_ < 0)
		return;
	MapItem next = world_->map(map_).items[item_];
	f.set(next, value);
	emit editItem(map_, item_, next, tr("Change %1 of %2").arg(f.label.toLower(), world_->describe(next)));
}
