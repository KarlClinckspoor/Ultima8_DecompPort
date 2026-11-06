#include "ui/names.h"

#include <QFile>
#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

#include <algorithm>

#include "data/world.h"

namespace Names
{

namespace
{

// The names given, read from the settings once: a search asks for a name for every item in
// the game, and on Windows each settings read is a registry read.
struct Given
{
	QHash<int, QString> maps, shapes;
	bool fullNames = true;
};

Given &given()
{
	static Given g = [] {
		Given n;
		QSettings s;
		for (auto [group, names] : {std::pair{"mapNames", &n.maps}, std::pair{"shapeNames", &n.shapes}})
		{
			s.beginGroup(QLatin1String(group));
			for (const QString &key : s.childKeys())
			{
				bool ok = false;
				const int number = key.toInt(&ok);
				if (ok)
					names->insert(number, s.value(key).toString());
			}
			s.endGroup();
		}
		n.fullNames = s.value(QStringLiteral("fullNames"), true).toBool();
		return n;
	}();
	return g;
}

void setGiven(QHash<int, QString> &names, const char *group, int number, const QString &name)
{
	QSettings s;
	const QString key = QStringLiteral("%1/%2").arg(QLatin1String(group)).arg(number);
	if (name.trimmed().isEmpty())
	{
		names.remove(number);
		s.remove(key);
	}
	else
	{
		names.insert(number, name.trimmed());
		s.setValue(key, name.trimmed());
	}
}

}

QString mapName(int map)
{
	return given().maps.value(map);
}

void setMapName(int map, const QString &name)
{
	setGiven(given().maps, "mapNames", map, name);
}

QString mapLabel(int map)
{
	if (map < 0)
		return QObject::tr("Usecode (any map)");
	const QString name = mapName(map);
	return name.isEmpty() ? QObject::tr("Map %1").arg(map) : QStringLiteral("%1 · %2").arg(map).arg(name);
}

QString mapShort(int map)
{
	const QString name = mapName(map);
	return name.isEmpty() ? QString::number(map) : QStringLiteral("%1 · %2").arg(map).arg(name);
}

QString shapeName(int shape)
{
	return given().shapes.value(shape);
}

void setShapeName(int shape, const QString &name)
{
	setGiven(given().shapes, "shapeNames", shape, name);
}

bool fullNames()
{
	return given().fullNames;
}

void setFullNames(bool full)
{
	given().fullNames = full;
	QSettings().setValue(QStringLiteral("fullNames"), full);
}

QString itemName(const World &world, int map, const MapItem &item)
{
	QString name = world.itemName(map, item, fullNames());
	if (!name.isEmpty())
		return name;
	name = shapeName(item.shape);
	if (!name.isEmpty())
		return name;
	name = world.usecode().className(world.classOf(item));
	if (name.isEmpty())
		name = world.usecode().className(item.shape);
	if (!name.isEmpty())
		return name.toLower().replace(QLatin1Char('_'), QLatin1Char(' '));
	return familyName(world.shapes().type(item.shape).family);
}

QString itemLabel(const World &world, int map, const MapItem &item)
{
	const QString name = itemName(world, map, item);
	return QStringLiteral("%1 \u2014 %2").arg(name, world.describe(item));
}

QString itemTooltip(const World &world, int map, int item)
{
	const MapData &m = world.map(map);
	const MapItem &it = m.items[item];
	const TypeInfo &t = world.shapes().type(it.shape);
	QStringList rows;
	auto add = [&](const QString &field, const QString &value) {
		rows << QStringLiteral("<tr><td style='color:#9aa4b0; padding-right:8px'>%1</td><td>%2</td></tr>")
			.arg(field.toHtmlEscaped(), value.toHtmlEscaped());
	};
	const QString name = itemName(world, map, it);
	QString head = QStringLiteral("<b>%1</b>").arg(name.toHtmlEscaped());
	const QString now = world.itemName(map, it, false);
	if (fullNames() && !now.isEmpty() && now != name)
		head += QStringLiteral("<br><i>%1</i>").arg(QObject::tr("in this saved game: %1").arg(now).toHtmlEscaped());

	const QString cls = world.usecode().className(world.classOf(it));
	add(QObject::tr("Shape"), QObject::tr("%1, frame %2%3").arg(it.shape).arg(it.frame)
		.arg(cls.isEmpty() ? QString() : QObject::tr(" (class %1)").arg(cls)));
	if (it.contained())
		add(QObject::tr("Inside"), itemName(world, map, m.items[it.parent]));
	else
		add(QObject::tr("Position"), QObject::tr("%1, %2, %3; top %4").arg(it.x).arg(it.y).arg(it.z).arg(it.z + t.zSize * 8));
	add(QObject::tr("Size"), QObject::tr("%1 x %2 x %3 units").arg(t.worldX()).arg(t.worldY()).arg(t.worldZ()));
	QString from;
	switch (it.source)
	{
	case MapItem::Fixed: from = QObject::tr("fixed (FIXED.DAT)"); break;
	case MapItem::NonFixed: from = QObject::tr("movable (the saved game)"); break;
	case MapItem::Glob: from = QObject::tr("put down by glob egg %1 (edit the egg)").arg(it.parent); break;
	case MapItem::Npc: from = QObject::tr("an NPC or what one carries"); break;
	}
	add(QObject::tr("Kind"), QObject::tr("%1; family %2").arg(from, familyName(t.family)));

	switch (t.family)
	{
	case FamilyTeleportEgg:
		if (EggInfo::sends(it))
		{
			const int dm = EggInfo::destMap(it);
			const bool found = world.map(dm).receivingEgg(world.shapes(), EggInfo::teleportId(it)) >= 0;
			add(QObject::tr("Sends to"), QObject::tr("%1, egg #%2%3%4").arg(dm == map ? QObject::tr("this map") : mapLabel(dm))
				.arg(EggInfo::teleportId(it)).arg((it.quality >> 8) ? QObject::tr(", with a fade") : QString())
				.arg(found ? QString() : QObject::tr(" (no such egg!)")));
		}
		else
			add(QObject::tr("Receives"), QObject::tr("teleport id #%1").arg(EggInfo::teleportId(it)));
		break;
	case FamilyMonsterEgg:
	{
		const QString mon = world.usecode().className(EggInfo::monsterShape(it));
		add(QObject::tr("Hatches"), QObject::tr("%1 x %2 (shape %3), chance %4, activity %5")
			.arg(EggInfo::monsterCount(it)).arg(mon.isEmpty() ? QObject::tr("monster") : mon)
			.arg(EggInfo::monsterShape(it)).arg(EggInfo::monsterProb(it)).arg(EggInfo::monsterActivity(it)));
		break;
	}
	case FamilyUsecodeEgg:
		add(QObject::tr("Runs"), QObject::tr("class %1 (%2), egg id %3").arg(UsecodeEggClassBase + it.quality)
			.arg(cls.isEmpty() ? QStringLiteral("?") : cls).arg(it.mapNum));
		break;
	case FamilyGlobEgg:
		add(QObject::tr("Puts down"), QObject::tr("glob %1").arg(it.quality));
		break;
	case FamilyQuantity:
	case FamilyReagent:
		add(QObject::tr("Quantity"), QString::number(it.quality));
		break;
	}
	if (t.family == FamilyTeleportEgg || t.family == FamilyMonsterEgg || t.family == FamilyUsecodeEgg)
		add(QObject::tr("Hatch range"), QObject::tr("%1 x %2 units either side").arg(EggInfo::xRange(it) * 32).arg(EggInfo::yRange(it) * 32));

	if (DoorInfo::isDoor(it.shape))
	{
		QString state = DoorInfo::locked(it) ? QObject::tr("locked") : QObject::tr("not locked");
		if (DoorInfo::open(it))
			state += QObject::tr(", open");
		add(QObject::tr("Door"), QObject::tr("%1; key number %2").arg(state).arg(DoorInfo::keyOf(it)));
	}
	else if (DoorInfo::isKey(it.shape))
	{
		int doors = 0;
		for (const MapItem &d : m.items)
			doors += !d.deleted && DoorInfo::isDoor(d.shape) && it.quality && DoorInfo::keyOf(d) == it.quality;
		add(QObject::tr("Key"), it.quality ? QObject::tr("number %1; opens %n door(s) on this map", nullptr, doors).arg(it.quality)
			: QObject::tr("a blank (number 0)"));
	}

	int held = 0;
	for (int k : it.contents)
		held += !m.items[k].deleted;
	if (held)
		add(QObject::tr("Holds"), QObject::tr("%n item(s)", nullptr, held));

	if (it.source == MapItem::Npc && !it.contained() && it.record > 0 && world.hasNpcData())
	{
		const NpcStats s = world.npcStats(it.record);
		QString line = QObject::tr("NPC %1: strength %2, dexterity %3, intelligence %4, hit points %5")
			.arg(it.record).arg(s.str).arg(s.dex).arg(s.intelligence).arg(s.hp);
		if (s.flags2 & NpcStats::Dead)
			line += QObject::tr(", dead");
		add(QObject::tr("Character"), line);
	}

	QStringList flags;
	if (it.flags & FlagInvisible)
		flags << QObject::tr("invisible");
	if (it.flipped())
		flags << QObject::tr("flipped");
	if (it.flags & FlagOwned)
		flags << QObject::tr("owned (taking it is stealing)");
	if (t.editor)
		flags << QObject::tr("shown only in editors");
	if (!flags.isEmpty())
		add(QObject::tr("Flags"), flags.join(QStringLiteral(", ")));

	return QStringLiteral("%1<table cellspacing=0 cellpadding=0 style='margin-top:4px'>%2</table>")
		.arg(head, rows.join(QString()));
}

bool write(const QString &path, QString *error)
{
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
	{
		if (error)
			*error = f.errorString();
		return false;
	}
	QTextStream out(&f);
	out.setEncoding(QStringConverter::Utf8);
	out << "# u8edit names: map N = name, shape N = name\n";
	QSettings s;
	for (const char *group : {"mapNames", "shapeNames"})
	{
		s.beginGroup(QLatin1String(group));
		QStringList keys = s.childKeys();
		std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) { return a.toInt() < b.toInt(); });
		for (const QString &k : keys)
			out << (group[0] == 'm' ? "map " : "shape ") << k << " = " << s.value(k).toString() << "\n";
		s.endGroup();
	}
	return true;
}

bool read(const QString &path, int *count, QString *error)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		if (error)
			*error = f.errorString();
		return false;
	}
	QTextStream in(&f);
	in.setEncoding(QStringConverter::Utf8);
	static const QRegularExpression line(QStringLiteral("^\\s*(map|shape)\\s+(\\d+)\\s*=\\s*(.*)$"),
		QRegularExpression::CaseInsensitiveOption);
	int n = 0, lineNo = 0;
	while (!in.atEnd())
	{
		const QString text = in.readLine();
		lineNo++;
		if (text.trimmed().isEmpty() || text.trimmed().startsWith(QLatin1Char('#')))
			continue;
		const QRegularExpressionMatch m = line.match(text);
		if (!m.hasMatch())
		{
			if (error)
				*error = QObject::tr("line %1 is not \"map N = name\" or \"shape N = name\"").arg(lineNo);
			return false;
		}
		if (m.captured(1).toLower() == QLatin1String("map"))
			setMapName(m.captured(2).toInt(), m.captured(3));
		else
			setShapeName(m.captured(2).toInt(), m.captured(3));
		n++;
	}
	if (count)
		*count = n;
	return true;
}

}
