#include "ui/edits.h"

#include <algorithm>
#include <cstring>

namespace
{

class EditItem : public QUndoCommand
{
public:
	EditItem(Editor *e, int map, int item, const MapItem &before, const MapItem &after, const QString &text)
		: QUndoCommand(text), e_(e), map_(map), item_(item), before_(before), after_(after) {}
	void redo() override { apply(after_); }
	void undo() override { apply(before_); }

private:
	void apply(const MapItem &v)
	{
		e_->world()->setItem(map_, item_, v);
		e_->notifyMap(map_, item_);
	}
	Editor *e_;
	int map_, item_;
	MapItem before_, after_;
};

class AddItem : public QUndoCommand
{
public:
	AddItem(Editor *e, int map, const MapItem &item, int parent, const QString &text)
		: QUndoCommand(text), e_(e), map_(map), item_(item), parent_(parent) {}
	void redo() override
	{
		if (index_ < 0)
			index_ = e_->world()->addItem(map_, item_, parent_);
		else
			e_->world()->setDeleted(map_, index_, false);
		e_->notifyMap(map_, index_);
	}
	void undo() override
	{
		e_->world()->setDeleted(map_, index_, true);
		e_->notifyMap(map_, -1);
	}
	int index() const { return index_; }

private:
	Editor *e_;
	int map_;
	MapItem item_;
	int parent_;
	int index_ = -1;
};

class DeleteItem : public QUndoCommand
{
public:
	DeleteItem(Editor *e, int map, int item, const QString &text)
		: QUndoCommand(text), e_(e), map_(map), item_(item) {}
	void redo() override
	{
		e_->world()->setDeleted(map_, item_, true);
		e_->notifyMap(map_, -1);
	}
	void undo() override
	{
		e_->world()->setDeleted(map_, item_, false);
		e_->notifyMap(map_, item_);
	}

private:
	Editor *e_;
	int map_, item_;
};

class EditNpc : public QUndoCommand
{
public:
	EditNpc(Editor *e, int npc, const NpcStats &before, const NpcStats &after, const QString &text)
		: QUndoCommand(text), e_(e), npc_(npc), before_(before), after_(after) {}
	void redo() override { apply(after_); }
	void undo() override { apply(before_); }

private:
	void apply(const NpcStats &s)
	{
		e_->world()->setNpcStats(npc_, s);
		e_->notifyNpc(npc_);
	}
	Editor *e_;
	int npc_;
	NpcStats before_, after_;
};

class EditTables : public QUndoCommand
{
public:
	EditTables(Editor *e, const TableSnapshot &before, const TableSnapshot &after, const QString &text)
		: QUndoCommand(text), e_(e), before_(before), after_(after) {}
	void redo() override { e_->restoreTables(after_); }
	void undo() override { e_->restoreTables(before_); }

private:
	Editor *e_;
	TableSnapshot before_, after_;
};

}

Editor::Editor(QObject *parent) : QObject(parent)
{
}

void Editor::setWorld(World *world)
{
	world_ = world;
	undo_.clear();
}

bool Editor::editItem(int map, int item, const MapItem &fields, const QString &what)
{
	if (!world_)
		return false;
	const QString why = world_->readOnlyReason(map, item);
	if (!why.isEmpty())
	{
		emit refused(why);
		return false;
	}
	const MapItem &now = world_->map(map).items[item];
	if (now.sameRecord(fields))
		return true;
	undo_.push(new EditItem(this, map, item, now, fields, what));
	return true;
}

bool Editor::moveItem(int map, int item, int dx, int dy, int dz)
{
	if (!world_ || (dx == 0 && dy == 0 && dz == 0))
		return false;
	MapItem f = world_->map(map).items[item];
	f.x = uint16_t(std::clamp(int(f.x) + dx, 0, 0x7fff));
	f.y = uint16_t(std::clamp(int(f.y) + dy, 0, 0x7fff));
	f.z = uint8_t(std::clamp(int(f.z) + dz, 0, 0xfa));
	return editItem(map, item, f, tr("Move %1").arg(world_->describe(f)));
}

int Editor::addItem(int map, const MapItem &item, int parent, const QString &what)
{
	if (!world_)
		return -1;
	if (world_->saveSource().isEmpty() && !world_->shapes().type(item.shape).fixed && parent < 0)
	{
		emit refused(tr("There is no saved game to put movable items in."));
		return -1;
	}
	if (parent >= 0)
	{
		const QString why = world_->readOnlyReason(map, parent);
		if (!why.isEmpty())
		{
			emit refused(why);
			return -1;
		}
	}
	auto *c = new AddItem(this, map, item, parent, what);
	undo_.push(c);
	return c->index();
}

bool Editor::deleteItem(int map, int item)
{
	if (!world_)
		return false;
	const QString why = world_->readOnlyReason(map, item);
	if (!why.isEmpty())
	{
		emit refused(why);
		return false;
	}
	if (world_->map(map).items[item].source == MapItem::Npc)
	{
		emit refused(tr("NPCs cannot be deleted; move them off the map (z 251) or to another map instead."));
		return false;
	}
	undo_.push(new DeleteItem(this, map, item, tr("Delete %1").arg(world_->describe(world_->map(map).items[item]))));
	return true;
}

int Editor::duplicateItem(int map, int item, int dx, int dy)
{
	if (!world_)
		return -1;
	const QString why = world_->readOnlyReason(map, item);
	if (!why.isEmpty())
	{
		emit refused(why);
		return -1;
	}
	MapItem copy = world_->map(map).items[item];
	if (copy.source == MapItem::Npc)
	{
		emit refused(tr("NPCs cannot be copied."));
		return -1;
	}
	copy.x = uint16_t(std::clamp(int(copy.x) + dx, 0, 0x7fff));
	copy.y = uint16_t(std::clamp(int(copy.y) + dy, 0, 0x7fff));
	return addItem(map, copy, copy.contained() ? copy.parent : -1, tr("Copy %1").arg(world_->describe(copy)));
}

void Editor::editNpc(int npc, const NpcStats &stats)
{
	if (!world_ || !world_->hasNpcData())
		return;
	const NpcStats before = world_->npcStats(npc);
	if (memcmp(&before, &stats, sizeof stats) == 0)
		return;
	undo_.push(new EditNpc(this, npc, before, stats, tr("Change NPC %1").arg(npc)));
}

TableSnapshot Editor::tables() const
{
	TableSnapshot t;
	if (world_)
	{
		t.weapons = world_->exeTables().weapons;
		t.armour = world_->exeTables().armour;
		t.monsters = world_->exeTables().monsters;
	}
	return t;
}

void Editor::restoreTables(const TableSnapshot &t)
{
	world_->exeTables().weapons = t.weapons;
	world_->exeTables().armour = t.armour;
	world_->exeTables().monsters = t.monsters;
	emit tablesChanged();
}

void Editor::editTables(const TableSnapshot &after, const QString &what)
{
	if (!world_)
		return;
	const TableSnapshot before = tables();
	if (before.weapons == after.weapons && before.armour == after.armour && before.monsters == after.monsters)
	{
		emit tablesChanged();	// shows the old value again in a cell that did not take
		return;
	}
	undo_.push(new EditTables(this, before, after, what));
}
