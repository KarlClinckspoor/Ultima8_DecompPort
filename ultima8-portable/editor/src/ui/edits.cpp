#include "ui/edits.h"

#include <algorithm>
#include <cstring>

namespace
{

class EditItem : public QUndoCommand
{
public:
	EditItem(Editor *e, int map, int item, const MapItem &before, const MapItem &after, const QString &text,
		QUndoCommand *parent = nullptr)
		: QUndoCommand(text, parent), e_(e), map_(map), item_(item), before_(before), after_(after) {}
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
	AddItem(Editor *e, int map, const MapItem &item, int parent, const QString &text, QUndoCommand *group = nullptr)
		: QUndoCommand(text, group), e_(e), map_(map), item_(item), parent_(parent) {}
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
	DeleteItem(Editor *e, int map, int item, const QString &text, QUndoCommand *parent = nullptr)
		: QUndoCommand(text, parent), e_(e), map_(map), item_(item) {}
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

// Several commands as one step; the views hear once, after the last.
class Group : public QUndoCommand
{
public:
	Group(Editor *e, const QString &text) : QUndoCommand(text), e_(e) {}
	void redo() override
	{
		e_->hold();
		QUndoCommand::redo();
		e_->release();
	}
	void undo() override
	{
		e_->hold();
		QUndoCommand::undo();
		e_->release();
	}

private:
	Editor *e_;
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

class EditFlag : public QUndoCommand
{
public:
	EditFlag(Editor *e, int bit, int width, int before, int after, const QString &text)
		: QUndoCommand(text), e_(e), bit_(bit), width_(width), before_(before), after_(after) {}
	void redo() override { apply(after_); }
	void undo() override { apply(before_); }

private:
	void apply(int v)
	{
		e_->world()->setFlag(bit_, width_, v);
		e_->notifyFlag(bit_);
	}
	Editor *e_;
	int bit_, width_, before_, after_;
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

void Editor::notifyMap(int map, int item)
{
	if (!held_)
	{
		emit mapChanged(map, item);
		return;
	}
	// Remembers the last item that is still there, else just the map.
	heldAny_ = true;
	heldMap_ = map;
	if (item >= 0)
		heldItem_ = item;
}

void Editor::release()
{
	if (--held_ > 0 || !heldAny_)
		return;
	heldAny_ = false;
	const int map = heldMap_;
	int item = heldItem_;
	heldItem_ = -1;
	if (item >= 0 && (item >= int(world_->map(map).items.size()) || world_->map(map).items[item].deleted))
		item = -1;
	emit mapChanged(map, item);
}

QString Editor::whyNot(int map, int item, bool forDelete) const
{
	const QString why = world_->readOnlyReason(map, item);
	if (!why.isEmpty())
		return why;
	if (forDelete && world_->map(map).items[item].source == MapItem::Npc)
		return tr("NPCs cannot be deleted or copied; move them off the map (z 251) or to another map instead.");
	return QString();
}

bool Editor::moveItems(int map, const QVector<int> &items, int dx, int dy, int dz)
{
	if (items.size() == 1)
		return moveItem(map, items.first(), dx, dy, dz);
	return editItems(map, items, [&](MapItem &f) {
		f.x = uint16_t(std::clamp(int(f.x) + dx, 0, 0x7fff));
		f.y = uint16_t(std::clamp(int(f.y) + dy, 0, 0x7fff));
		f.z = uint8_t(std::clamp(int(f.z) + dz, 0, 0xfa));
	}, tr("Move %n items", nullptr, int(items.size())));
}

bool Editor::editItems(int map, const QVector<int> &items, const std::function<void(MapItem &)> &change, const QString &what)
{
	if (!world_ || items.isEmpty())
		return false;
	auto *group = new Group(this, what);
	QString refusal;
	for (int item : items)
	{
		const QString why = whyNot(map, item, false);
		if (!why.isEmpty())
		{
			refusal = why;
			continue;
		}
		const MapItem &now = world_->map(map).items[item];
		MapItem next = now;
		change(next);
		if (!now.sameRecord(next))
			new EditItem(this, map, item, now, next, what, group);
	}
	if (!refusal.isEmpty())
		emit refused(refusal);
	if (group->childCount() == 0)
	{
		delete group;
		return false;
	}
	undo_.push(group);
	return true;
}

bool Editor::deleteItems(int map, const QVector<int> &items)
{
	if (!world_ || items.isEmpty())
		return false;
	if (items.size() == 1)
		return deleteItem(map, items.first());
	auto *group = new Group(this, tr("Delete %n items", nullptr, int(items.size())));
	QString refusal;
	const MapData &m = world_->map(map);
	for (int item : items)
	{
		// What is inside a deleted container goes with it.
		bool inside = false;
		for (int p = m.items[item].parent; p >= 0 && !inside; p = m.items[p].parent)
			inside = items.contains(p) && m.items[item].contained();
		if (inside)
			continue;
		const QString why = whyNot(map, item, true);
		if (!why.isEmpty())
			refusal = why;
		else
			new DeleteItem(this, map, item, tr("Delete %1").arg(world_->describe(m.items[item])), group);
	}
	if (!refusal.isEmpty())
		emit refused(refusal);
	if (group->childCount() == 0)
	{
		delete group;
		return false;
	}
	undo_.push(group);
	return true;
}

QVector<int> Editor::duplicateItems(int map, const QVector<int> &items, int dx, int dy)
{
	QVector<int> out;
	if (!world_ || items.isEmpty())
		return out;
	if (items.size() == 1)
	{
		const int added = duplicateItem(map, items.first(), dx, dy);
		if (added >= 0)
			out << added;
		return out;
	}
	auto *group = new Group(this, tr("Copy %n items", nullptr, int(items.size())));
	QVector<AddItem *> adds;
	QString refusal;
	for (int item : items)
	{
		const QString why = whyNot(map, item, true);
		if (!why.isEmpty())
		{
			refusal = why;
			continue;
		}
		MapItem copy = world_->map(map).items[item];
		if (copy.contained())
			continue;	// copied with its container, or not at all
		if (world_->saveSource().isEmpty() && !world_->shapes().type(copy.shape).fixed)
		{
			refusal = tr("There is no saved game to put movable items in.");
			continue;
		}
		copy.x = uint16_t(std::clamp(int(copy.x) + dx, 0, 0x7fff));
		copy.y = uint16_t(std::clamp(int(copy.y) + dy, 0, 0x7fff));
		adds << new AddItem(this, map, copy, -1, tr("Copy %1").arg(world_->describe(copy)), group);
	}
	if (!refusal.isEmpty())
		emit refused(refusal);
	if (adds.isEmpty())
	{
		delete group;
		return out;
	}
	undo_.push(group);
	for (AddItem *a : adds)
		out << a->index();
	return out;
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

void Editor::editFlag(int bit, int width, int value)
{
	if (!world_)
		return;
	const int before = world_->flag(bit, width);
	const int after = int(int16_t(value & ((1 << width) - 1)));
	if (before == after)
		return;
	undo_.push(new EditFlag(this, bit, width, before, after, tr("Set flag %1 to %2").arg(bit).arg(after)));
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
