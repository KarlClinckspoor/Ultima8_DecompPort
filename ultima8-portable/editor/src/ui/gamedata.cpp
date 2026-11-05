#include "ui/gamedata.h"

#include <QHeaderView>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "ui/edits.h"

namespace
{

QTableWidgetItem *fixedCell(const QString &text)
{
	auto *c = new QTableWidgetItem(text);
	c->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
	c->setForeground(QColor(170, 170, 180));
	return c;
}

QTableWidgetItem *numberCell(int v)
{
	auto *c = new QTableWidgetItem(QString::number(v));
	c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	return c;
}

QTableWidgetItem *bitsCell(int v)
{
	auto *c = new QTableWidgetItem(QStringLiteral("0x%1 (%2)").arg(v, 4, 16, QLatin1Char('0')).arg(damageTypeNames(v)));
	c->setToolTip(damageTypeNames(v) + QObject::tr("\nBits: 1 normal, 2 blade, 4 blunt, 8 fire, 0x10 undead, "
		"0x20 magic, 0x40 slayer, 0x80 pierce, 0x100 falling"));
	return c;
}

// The cell's number (what comes before any words), kept to a range; the old value when it is
// not a number.
int cellValue(QTableWidgetItem *c, int old, int min, int max)
{
	bool ok = false;
	const int v = c->text().trimmed().section(QLatin1Char(' '), 0, 0).toInt(&ok, 0);
	return ok ? std::clamp(v, min, max) : old;
}

QTableWidget *makeTable(const QStringList &headers)
{
	auto *t = new QTableWidget;
	t->setColumnCount(headers.size());
	t->setHorizontalHeaderLabels(headers);
	t->verticalHeader()->setVisible(false);
	t->setAlternatingRowColors(true);
	t->setSelectionMode(QAbstractItemView::SingleSelection);
	t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	t->horizontalHeader()->setStretchLastSection(true);
	return t;
}

}

GameDataView::GameDataView(Editor *editor, QWidget *parent) : QWidget(parent), editor_(editor)
{
	note_ = new QLabel;
	note_->setWordWrap(true);
	weapons_ = makeTable({tr("Weapon"), tr("Shape"), tr("Base damage"), tr("Random bonus"), tr("Dex attack"),
		tr("Dex defence"), tr("Armour class"), tr("Damage type")});
	weapons_->setToolTip(tr("A hit does strength / 5 + base damage + 0 to the random bonus (Npc::getDamageAmount)."));
	QStringList armourHeaders{tr("Armour"), tr("Shape")};
	for (int i = 0; i < 13; i++)
		armourHeaders << tr("Frames %1-%2").arg(i * 2).arg(i * 2 + 1);
	armour_ = makeTable(armourHeaders);
	armour_->setToolTip(tr("The armour class each piece adds, by its frame: frames 0 and 1 use the first number, 2 and 3 the next..."));
	monsters_ = makeTable({tr("Monster"), tr("Shape"), tr("HP min"), tr("HP max"), tr("Dex min"), tr("Dex max"),
		tr("Damage min"), tr("Damage max"), tr("Armour class"), tr("Alignment"), tr("Byte 8"), tr("Damage type"),
		tr("Defence type")});
	monsters_->setToolTip(tr("Monsters made by monster eggs and the like start with numbers between these (u8/mondata.cpp)."));
	npcs_ = makeTable({tr("NPC"), tr("Name"), tr("Map"), tr("Strength"), tr("Dexterity"), tr("Intelligence"),
		tr("Hit points"), tr("Mana"), tr("Alignment"), tr("Enemies")});
	npcs_->setToolTip(tr("NPCDATA.DAT in the saved game. Double-click a name to show the NPC on its map."));

	weapons_->setObjectName("weapons");
	armour_->setObjectName("armour");
	monsters_->setObjectName("monsters");
	npcs_->setObjectName("npcs");
	tabs_ = new QTabWidget;
	tabs_->addTab(weapons_, tr("Weapons"));
	tabs_->addTab(armour_, tr("Armour"));
	tabs_->addTab(monsters_, tr("Monsters"));
	tabs_->addTab(npcs_, tr("NPCs"));
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(4, 4, 4, 4);
	v->addWidget(note_);
	v->addWidget(tabs_, 1);

	connect(weapons_, &QTableWidget::cellChanged, this, &GameDataView::weaponChanged);
	connect(armour_, &QTableWidget::cellChanged, this, &GameDataView::armourChanged);
	connect(monsters_, &QTableWidget::cellChanged, this, &GameDataView::monsterChanged);
	connect(npcs_, &QTableWidget::cellChanged, this, &GameDataView::npcChanged);
	connect(npcs_, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
		if (column <= 1)
			emit showNpc(npcs_->item(row, 0)->data(Qt::UserRole).toInt());
	});
	// Queued: a change made in a table comes back while the table is still in its edit.
	connect(editor_, &Editor::tablesChanged, this, [this] {
		fillWeapons();
		fillArmour();
		fillMonsters();
	}, Qt::QueuedConnection);
	connect(editor_, &Editor::npcChanged, this, &GameDataView::reloadNpc, Qt::QueuedConnection);
}

void GameDataView::reload()
{
	const World *w = editor_->world();
	QStringList notes;
	if (!w || !w->exeTables().loaded())
		notes << tr("Weapon, armour and monster statistics live in U8.EXE, and none that holds them was found in the game folder.");
	else
		notes << tr("Weapons, armour and monsters are written into %1 (the first time, the original is kept as U8.EXE.orig); "
			"DOS and the port both play with them.").arg(QStringLiteral("U8.EXE"));
	if (!w || !w->hasNpcData())
		notes << tr("The saved game has no NPCDATA.DAT, so NPCs cannot be changed.");
	note_->setText(notes.join(QLatin1Char(' ')));
	fillWeapons();
	fillArmour();
	fillMonsters();
	fillNpcs();
}

void GameDataView::fillWeapons()
{
	filling_ = true;
	const World *w = editor_->world();
	const auto &list = w ? w->exeTables().weapons : QVector<WeaponStats>();
	weapons_->setRowCount(list.size());
	for (int i = 0; i < list.size(); i++)
	{
		const WeaponStats &s = list[i];
		QString name = QString::fromLatin1(ExeTables::weaponName(i));
		if (name.isEmpty())
			name = w->usecode().className(s.shape);
		weapons_->setItem(i, 0, fixedCell(name));
		weapons_->setItem(i, 1, fixedCell(QString::number(s.shape)));
		weapons_->setItem(i, 2, numberCell(s.baseDamage));
		weapons_->setItem(i, 3, numberCell(s.damageModifier));
		weapons_->setItem(i, 4, numberCell(s.dexAttack));
		weapons_->setItem(i, 5, numberCell(s.dexDefend));
		weapons_->setItem(i, 6, numberCell(s.armourClass));
		weapons_->setItem(i, 7, bitsCell(s.damageType));
	}
	filling_ = false;
}

void GameDataView::fillArmour()
{
	filling_ = true;
	const World *w = editor_->world();
	const auto &list = w ? w->exeTables().armour : QVector<ArmourTable>();
	armour_->setRowCount(list.size());
	for (int i = 0; i < list.size(); i++)
	{
		const ArmourTable &a = list[i];
		armour_->setItem(i, 0, fixedCell(QString::fromLatin1(ExeTables::armourName(i))));
		armour_->setItem(i, 1, fixedCell(QString::number(a.shape)));
		for (int k = 0; k < 13; k++)
			armour_->setItem(i, 2 + k, k < a.bonus.size() ? numberCell(a.bonus[k]) : fixedCell(QString()));
	}
	filling_ = false;
}

void GameDataView::fillMonsters()
{
	filling_ = true;
	const World *w = editor_->world();
	const auto &list = w ? w->exeTables().monsters : QVector<MonsterStats>();
	monsters_->setRowCount(list.size());
	for (int i = 0; i < list.size(); i++)
	{
		const MonsterStats &m = list[i];
		QString name = m.shape ? w->usecode().className(m.shape) : tr("(unused)");
		if (name.isEmpty())
			name = tr("shape %1").arg(m.shape);
		monsters_->setItem(i, 0, fixedCell(name));
		monsters_->setItem(i, 1, fixedCell(QString::number(m.shape)));
		const int v[9] = {m.minHp, m.maxHp, m.minDex, m.maxDex, m.minDamage, m.maxDamage, m.armourClass, m.alignment, m.unknown8};
		for (int k = 0; k < 9; k++)
			monsters_->setItem(i, 2 + k, numberCell(v[k]));
		monsters_->setItem(i, 11, bitsCell(m.damageType));
		monsters_->setItem(i, 12, bitsCell(m.defenseType));
	}
	filling_ = false;
}

void GameDataView::fillNpcs()
{
	filling_ = true;
	npcs_->setRowCount(0);
	const World *w = editor_->world();
	if (!w || !w->hasNpcData())
	{
		filling_ = false;
		return;
	}
	// The NPCs that are somewhere: the ones with an item on a map.
	QVector<QPair<int, int>> found;	// npc, map
	for (int m : w->mapNumbers())
		for (const MapItem &it : w->map(m).items)
			if (!it.deleted && it.source == MapItem::Npc && it.parent < 0 && it.record > 0 && it.record < 256)
				found << qMakePair(it.record, m);
	std::sort(found.begin(), found.end());
	npcs_->setRowCount(found.size());
	for (int r = 0; r < found.size(); r++)
	{
		const int npc = found[r].first;
		const NpcStats s = w->npcStats(npc);
		auto *num = fixedCell(QString::number(npc));
		num->setData(Qt::UserRole, npc);
		npcs_->setItem(r, 0, num);
		npcs_->setItem(r, 1, fixedCell(w->usecode().className(NpcClassBase + npc)));
		npcs_->setItem(r, 2, fixedCell(QString::number(found[r].second)));
		const int v[7] = {s.str, s.dex, s.intelligence, s.hp, s.mana, s.alignment & 0xf, s.alignment >> 4};
		for (int k = 0; k < 7; k++)
			npcs_->setItem(r, 3 + k, numberCell(v[k]));
	}
	filling_ = false;
}

void GameDataView::reloadNpc(int npc)
{
	const World *w = editor_->world();
	for (int r = 0; r < npcs_->rowCount(); r++)
	{
		if (npcs_->item(r, 0)->data(Qt::UserRole).toInt() != npc)
			continue;
		filling_ = true;
		const NpcStats s = w->npcStats(npc);
		const int v[7] = {s.str, s.dex, s.intelligence, s.hp, s.mana, s.alignment & 0xf, s.alignment >> 4};
		for (int k = 0; k < 7; k++)
			npcs_->item(r, 3 + k)->setText(QString::number(v[k]));
		filling_ = false;
	}
}

void GameDataView::weaponChanged(int row, int column)
{
	if (filling_ || column < 2)
		return;
	TableSnapshot t = editor_->tables();
	WeaponStats &s = t.weapons[row];
	QTableWidgetItem *c = weapons_->item(row, column);
	switch (column)
	{
	case 2: s.baseDamage = cellValue(c, s.baseDamage, 0, 255); break;
	case 3: s.damageModifier = cellValue(c, s.damageModifier, 0, 255); break;
	case 4: s.dexAttack = cellValue(c, s.dexAttack, -128, 127); break;
	case 5: s.dexDefend = cellValue(c, s.dexDefend, -128, 127); break;
	case 6: s.armourClass = cellValue(c, s.armourClass, -128, 127); break;
	case 7: s.damageType = cellValue(c, s.damageType, 0, 0xffff); break;
	}
	editor_->editTables(t, tr("Change %1").arg(weapons_->item(row, 0)->text()));
}

void GameDataView::armourChanged(int row, int column)
{
	if (filling_ || column < 2)
		return;
	TableSnapshot t = editor_->tables();
	ArmourTable &a = t.armour[row];
	const int k = column - 2;
	if (k >= a.bonus.size())
		return;
	// -128 ends a list in the game, so it is not allowed as a value.
	a.bonus[k] = cellValue(armour_->item(row, column), a.bonus[k], -127, 127);
	editor_->editTables(t, tr("Change %1").arg(armour_->item(row, 0)->text()));
}

void GameDataView::monsterChanged(int row, int column)
{
	if (filling_ || column < 2)
		return;
	TableSnapshot t = editor_->tables();
	MonsterStats &m = t.monsters[row];
	QTableWidgetItem *c = monsters_->item(row, column);
	int *fields[9] = {&m.minHp, &m.maxHp, &m.minDex, &m.maxDex, &m.minDamage, &m.maxDamage, &m.armourClass,
		&m.alignment, &m.unknown8};
	if (column < 11)
		*fields[column - 2] = cellValue(c, *fields[column - 2], 0, 255);
	else if (column == 11)
		m.damageType = cellValue(c, m.damageType, 0, 0xffff);
	else
		m.defenseType = cellValue(c, m.defenseType, 0, 0xffff);
	editor_->editTables(t, tr("Change %1").arg(monsters_->item(row, 0)->text()));
}

void GameDataView::npcChanged(int row, int column)
{
	if (filling_ || column < 3)
		return;
	const int npc = npcs_->item(row, 0)->data(Qt::UserRole).toInt();
	NpcStats s = editor_->world()->npcStats(npc);
	QTableWidgetItem *c = npcs_->item(row, column);
	switch (column)
	{
	case 3: s.str = cellValue(c, s.str, 0, 127); break;
	case 4: s.dex = cellValue(c, s.dex, 0, 127); break;
	case 5: s.intelligence = cellValue(c, s.intelligence, 0, 127); break;
	case 6: s.hp = cellValue(c, s.hp, 0, 255); break;
	case 7: s.mana = cellValue(c, s.mana, 0, 32767); break;
	case 8: s.alignment = (s.alignment & 0xf0) | cellValue(c, s.alignment & 0xf, 0, 15); break;
	case 9: s.alignment = (s.alignment & 0x0f) | cellValue(c, s.alignment >> 4, 0, 15) << 4; break;
	}
	editor_->editNpc(npc, s);
}
