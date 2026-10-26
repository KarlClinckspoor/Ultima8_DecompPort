// Every change goes through here as an undoable command, and the views hear about it.
#pragma once

#include <QObject>
#include <QUndoStack>

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
	void editNpc(int npc, const NpcStats &stats);
	TableSnapshot tables() const;
	void editTables(const TableSnapshot &after, const QString &what);

	// Called by the commands.
	void notifyMap(int map, int item) { emit mapChanged(map, item); }
	void notifyNpc(int npc) { emit npcChanged(npc); }
	void notifyTables() { emit tablesChanged(); }
	void restoreTables(const TableSnapshot &t);

signals:
	void mapChanged(int map, int item);
	void npcChanged(int npc);
	void tablesChanged();
	void refused(const QString &why);

private:
	World *world_ = nullptr;
	QUndoStack undo_;
};
