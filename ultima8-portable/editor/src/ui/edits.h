// Every change goes through here as an undoable command, and the views hear about it.
#pragma once

#include <QObject>
#include <QUndoStack>

#include <functional>

#include "data/world.h"

struct TableSnapshot
{
	QVector<WeaponStats> weapons;
	QVector<ArmourTable> armour;
	QVector<MonsterStats> monsters;
};

class Editor : public QObject
{
	Q_OBJECT

public:
	explicit Editor(QObject *parent = nullptr);
	void setWorld(World *world);
	World *world() const { return world_; }
	QUndoStack *undo() { return &undo_; }

	// Each returns false (and says why through refused) when the item cannot be changed.
	bool editItem(int map, int item, const MapItem &fields, const QString &what);
	bool moveItem(int map, int item, int dx, int dy, int dz);
	int addItem(int map, const MapItem &item, int parent, const QString &what);
	bool deleteItem(int map, int item);
	int duplicateItem(int map, int item, int dx, int dy);
	// The same for several items, as one step to undo. Items that cannot be changed are left
	// as they are (and said why); the others are changed.
	bool moveItems(int map, const QVector<int> &items, int dx, int dy, int dz);
	bool editItems(int map, const QVector<int> &items, const std::function<void(MapItem &)> &change, const QString &what);
	bool deleteItems(int map, const QVector<int> &items);
	// The copies' indices, in the order of items.
	QVector<int> duplicateItems(int map, const QVector<int> &items, int dx, int dy);
	void editNpc(int npc, const NpcStats &stats);
	// A global flag's value (World::setFlag), width bits from bit.
	void editFlag(int bit, int width, int value);
	TableSnapshot tables() const;
	void editTables(const TableSnapshot &after, const QString &what);

	// Called by the commands. While a group of commands runs, the views hear once, at the end.
	void notifyMap(int map, int item);
	void hold() { held_++; }
	void release();
	void notifyNpc(int npc) { emit npcChanged(npc); }
	void notifyTables() { emit tablesChanged(); }
	void notifyFlag(int bit) { emit flagChanged(bit); }
	void restoreTables(const TableSnapshot &t);

signals:
	void mapChanged(int map, int item);
	void npcChanged(int npc);
	void tablesChanged();
	void flagChanged(int bit);
	void refused(const QString &why);

private:
	QString whyNot(int map, int item, bool forDelete) const;

	World *world_ = nullptr;
	QUndoStack undo_;
	int held_ = 0;
	int heldMap_ = -1, heldItem_ = -1;
	bool heldAny_ = false;
};
