#include "data/itemnames.h"

#include <QObject>

#include <cstring>

#include "data/language.h"
#include "data/ucode.h"
#include "data/ucvm.h"
#include "data/world.h"

namespace
{

// The referent a looked-at item that is no NPC gets: any item number past the NPCs'.
enum { ItemReferent = 1000, Avatar = 1 };

// Answers the intrinsics a look event uses from the item's fields. The first bark is the name
// and ends the run; anything that needs more of the world stops it.
class NameHost : public UcVm::Host
{
public:
	NameHost(const World &world, int map, const MapItem &item, uint16_t referent)
		: world_(world), map_(map), item_(item), referent_(referent)
	{
	}

	QByteArray name;

	bool intrinsic(UcVm &vm, int number, const char *args, int argBytes, uint32_t *result, QString *why) override
	{
		const Ucode::Intrinsic *in = Ucode::intrinsic(number);
		if (!in)
		{
			*why = QObject::tr("it calls intrinsic %1, which the game does not have").arg(number);
			return false;
		}
		const QByteArray fn(in->name);
		const QByteArray call = fn.left(fn.indexOf('('));
		if (in->member)
		{
			if (argBytes < 4)
			{
				*why = QObject::tr("it calls %1 without an item").arg(QString::fromLatin1(call));
				return false;
			}
			const uint16_t who = vm.wordAt(uint32_t(UcVm::dword(args, 0)));
			if (call == "Item::bark")
			{
				name = vm.stringAt(uint32_t(UcVm::dword(args, 4)));
				*why = QString();
				return false;	// the name: no need to go on
			}
			if (who != referent_)
				return needs(call, why, QObject::tr("another item"));
			return member(call, args + 4, result, why);
		}
		return global(vm, call, args, result, why);
	}

private:
	bool needs(const QByteArray &call, QString *why, const QString &what = QString())
	{
		*why = what.isEmpty() ? QObject::tr("it needs %1, which the editor does not know").arg(QString::fromLatin1(call))
			: QObject::tr("it needs %1 of %2").arg(QString::fromLatin1(call), what);
		return false;
	}

	bool isNpc() const { return referent_ < 256; }

	int family() const { return world_.shapes().type(item_.shape).family; }

	bool member(const QByteArray &call, const char *args, uint32_t *r, QString *why)
	{
		const TypeInfo &type = world_.shapes().type(item_.shape);
		const int q = item_.quality;
		if (call == "Item::getType")
			*r = item_.shape;
		else if (call == "Item::getFrame")
			*r = item_.frame;
		else if (call == "Item::getQ")
			*r = uint16_t(q);
		else if (call == "Item::getQuality")
			*r = family() == FamilyQuality ? q : 0;
		else if (call == "Item::getQuantity")
			*r = family() == FamilyQuantity || family() == FamilyReagent ? q : 1;
		else if (call == "Item::getUnkEggType")
			*r = family() == FamilyUsecodeEgg ? q : 0;
		else if (call == "Item::getFamily")
			*r = uint32_t(family());
		else if (call == "Item::getStatus")
			*r = item_.flags;
		else if (call == "Item::getX")
			*r = item_.x;
		else if (call == "Item::getY")
			*r = item_.y;
		else if (call == "Item::getZ")
			*r = item_.z;
		else if (call == "Item::getMapArray")
			*r = item_.mapNum;
		else if (call == "Item::getNpcArray")
			*r = item_.npcNum;
		else if (call == "Item::isNpc")
			*r = isNpc();
		else if (call == "Item::getWeight")
			*r = type.weight;
		else if (call == "Item::getVolume")
			*r = type.volume;
		else if (call == "Item::getTypeFlag")
		{
			const int bit = UcVm::word(args, 0);
			const bool bits[16] = {type.fixed, type.solid, type.sea, type.land, type.occl, type.bag, type.damaging,
				type.noisy, type.draw, type.ignore, type.roof, type.trans, type.editor, type.explode, type.unknown1,
				type.unknown2};
			if (bit < 0 || bit >= 16)
				return needs(call, why);
			*r = bits[bit];
		}
		else if (call == "Egg::getEggXRange")
			*r = EggInfo::xRange(item_);
		else if (call == "Egg::getEggYRange")
			*r = EggInfo::yRange(item_);
		else if (call == "Egg::getEggId")
			*r = item_.mapNum;
		else if (call.startsWith("Npc::"))
			return npc(call, r, why);
		else if (call.contains("::get") || call.contains("::is") || call.contains("::can") || call == "Item::ask"
			|| call.contains("Input") || call.contains("::read") || call.contains("::find"))
			return needs(call, why);
		else
			*r = 0;	// something it does to the world: the editor leaves the world as it is
		return true;
	}

	bool npc(const QByteArray &call, uint32_t *r, QString *why)
	{
		// An item that is no NPC reads as one with nothing set.
		const NpcStats s = isNpc() && world_.hasNpcData() ? world_.npcStats(referent_) : NpcStats();
		if (call == "Npc::isDead")
			*r = (s.flags2 & NpcStats::Dead) != 0;
		else if (call == "Npc::isImmortal")
			*r = (s.flags2 & NpcStats::Immortal) != 0;
		else if (call == "Npc::isWithstandDeath")
			*r = (s.flags2 & NpcStats::WithstandDeath) != 0;
		else if (call == "Npc::getMap")
			*r = uint32_t(map_);
		else if (call == "Npc::getStr")
			*r = uint32_t(s.str);
		else if (call == "Npc::getDex")
			*r = uint32_t(s.dex);
		else if (call == "Npc::getInt")
			*r = uint32_t(s.intelligence);
		else if (call == "Npc::getHp")
			*r = uint32_t(s.hp);
		else if (call == "Npc::getMana")
			*r = uint32_t(s.mana);
		else if (call == "Npc::getAlignment")
			*r = uint32_t(s.alignment & 0xf);
		else if (call == "Npc::getEnemyAlignment")
			*r = uint32_t(s.alignment >> 4 & 0xf);
		else if (call == "Npc::isBusy" || call == "Npc::isInCombat" || call == "Npc::isFeignDeath")
			*r = 0;
		else if (call.contains("::get") || call.contains("::is") || call.contains("::are"))
			return needs(call, why);
		else
			*r = 0;
		return true;
	}

	bool global(UcVm &vm, const QByteArray &call, const char *args, uint32_t *r, QString *why)
	{
		if (call == "getName")
			*r = vm.newString("Avatar");
		else if (call == "numToStr")
			*r = vm.newString(QByteArray::number(uint16_t(UcVm::word(args, 0))));
		else if (call == "strToNum")
			*r = uint32_t(vm.stringAt(uint32_t(UcVm::dword(args, 0))).toInt());
		else if (call == "urandom")
			*r = 0;
		else if (call == "rndRange")
			*r = uint32_t(UcVm::word(args, 0));
		else if (call == "getFamilyOfType")
			*r = world_.shapes().type(uint16_t(UcVm::word(args, 0))).family;
		else if (call == "AvatarCanCheat" || call == "getAvatarInStasis" || call == "IsAvatarInCombat"
			|| call == "isGameRunning")
			*r = call == "isGameRunning";
		else if (call.startsWith("TimeIn") || call == "getCurrentTimerTick")
			*r = 0;
		else if (call.contains("get") || call.contains("Get") || call.startsWith("is") || call.contains("::is")
			|| call.startsWith("can") || call == "Kernel::getNumProcesses" || call == "target")
			return needs(call, why);
		else
			*r = 0;
		return true;
	}

	const World &world_;
	int map_;
	const MapItem &item_;
	uint16_t referent_;
};

QPair<quint64, quint64> cacheKey(int map, const MapItem &item, int classId, uint16_t referent)
{
	return {quint64(uint16_t(classId)) << 48 | quint64(item.shape) << 32 | quint64(item.quality) << 16 | referent,
		quint64(item.frame) << 48 | quint64(item.npcNum) << 40 | quint64(item.mapNum) << 32 | uint32_t(map)};
}

}

QString ItemNames::look(int map, const MapItem &item, QString *why) const
{
	const int classId = world_.classOf(item);
	const Usecode &usecode = world_.usecode();
	if (!usecode.eventOffset(classId, Ucode::EventLook))
	{
		if (why)
			*why = QObject::tr("%1 has no look event").arg(usecode.className(classId));
		return QString();
	}
	const bool npc = classId >= NpcClassBase && classId < UsecodeEggClassBase;
	const uint16_t referent = npc ? uint16_t(classId - NpcClassBase) : uint16_t(ItemReferent);
	UcVm vm(usecode);
	vm.setFlags(full_ ? QByteArray(World::FlagFileSize, char(0xff)) : world_.flagBytes());
	NameHost host(world_, map, item, referent);
	QString reason;
	const bool finished = vm.runEvent(referent, classId, Ucode::EventLook, 0, host, &reason, 20000);
	if (!host.name.isEmpty())
		return Language::fromCp437(host.name).trimmed();
	if (why)
		*why = finished ? QObject::tr("it says nothing") : reason;
	return QString();
}

QString ItemNames::name(int map, const MapItem &item) const
{
	const int classId = world_.classOf(item);
	const bool npc = classId >= NpcClassBase && classId < UsecodeEggClassBase;
	const auto key = cacheKey(map, item, classId, npc ? uint16_t(classId - NpcClassBase) : uint16_t(ItemReferent));
	auto it = cache_.constFind(key);
	if (it != cache_.constEnd())
		return *it;
	QString n = look(map, item, nullptr);
	if (n.isEmpty() && full_)
		n = world_.itemName(map, item, false);
	cache_.insert(key, n);
	return n;
}
