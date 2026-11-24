#include "ui/webexport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include "data/world.h"
#include "ui/maprender.h"
#include "ui/names.h"

namespace WebExport
{

namespace
{

enum { Page = 2048 };
const uint32_t NoInfo = 0xffffffffu;

// items.bin flags.
enum : uint8_t
{
	AlwaysDrawn = 0x01,	// the draw flag: shown whatever the roof
	SourceShift = 1,	// two bits: MapItem::Source
	NotDrawn = 0x08,	// invisible, or a shape only editors show: listed for clicks only
	HasInfo = 0x10,
	Translucent = 0x20,	// u8edit draws it at 60% (the game recolours what is under it)
};

using FrameKey = std::tuple<int, int, bool>;	// shape, frame, flipped

struct Packed
{
	FrameKey key;
	const Frame *frame = nullptr;
	int page = 0, x = 0, y = 0;
};

struct MapOut
{
	int number = 0;
	MapRender render;
	std::vector<int> infoOf;	// item: its info.json index, or -1
	std::vector<int> infoItems;	// info.json index: item
	std::vector<std::pair<const MapRender::Entry *, FrameKey>> listed;	// in drawing order
	int drawn = 0;
	QRect bounds;
	qint64 itemsBytes = 0, infoBytes = 0;
};

void put16(QByteArray &b, int v)
{
	b.append(char(v & 0xff));
	b.append(char((v >> 8) & 0xff));
}

void put32(QByteArray &b, uint32_t v)
{
	for (int i = 0; i < 4; i++)
		b.append(char((v >> (8 * i)) & 0xff));
}

QString readable(const QString &className)
{
	return className.toLower().replace(QLatin1Char('_'), QLatin1Char(' '));
}

// The name u8edit gives a shape: the name given to it, else its class made readable, else its
// family.
QString shapeName(const World &world, int shape)
{
	QString name = Names::shapeName(shape);
	if (!name.isEmpty())
		return name;
	name = world.usecode().className(shape);
	if (!name.isEmpty())
		return readable(name);
	return familyName(world.shapes().type(shape).family);
}

// Names::itemName with full names on, whatever the setting.
QString itemName(const World &world, int map, const MapItem &item)
{
	QString name = world.itemName(map, item, true);
	if (!name.isEmpty())
		return name;
	name = Names::shapeName(item.shape);
	if (!name.isEmpty())
		return name;
	name = world.usecode().className(world.classOf(item));
	if (name.isEmpty())
		name = world.usecode().className(item.shape);
	if (!name.isEmpty())
		return readable(name);
	return familyName(world.shapes().type(item.shape).family);
}

// The name with how many there are, for a heap of something: "12 gold coins".
QString countedName(const World &world, int map, const MapItem &item)
{
	const QString name = itemName(world, map, item);
	const int family = world.shapes().type(item.shape).family;
	if ((family == FamilyQuantity || family == FamilyReagent) && item.quality > 1 &&
		!name.contains(QString::number(item.quality)))
		return QStringLiteral("%1 %2").arg(item.quality).arg(name);
	return name;
}

bool holdsAnything(const MapData &m, const MapItem &item)
{
	for (int k : item.contents)
		if (!m.items[k].deleted)
			return true;
	return false;
}

bool isEgg(int family)
{
	return family == FamilyTeleportEgg || family == FamilyMonsterEgg || family == FamilyUsecodeEgg ||
		family == FamilyGlobEgg;
}

// Whether a click on the item has more to show than its shape's name. Glob eggs have nothing
// to say a click needs: what they put down is on the map.
bool wantsInfo(const World &world, int map, const MapItem &it)
{
	const MapData &m = world.map(map);
	const int family = world.shapes().type(it.shape).family;
	if ((isEgg(family) && family != FamilyGlobEgg) || DoorInfo::isDoor(it.shape) || DoorInfo::isKey(it.shape) || holdsAnything(m, it))
		return true;
	if (it.source == MapItem::Glob)
		return false;
	if (it.source == MapItem::Npc)
		return true;
	if ((family == FamilyQuantity || family == FamilyReagent) && it.quality > 1)
		return true;
	const QString look = world.itemName(map, it, true);
	return !look.isEmpty() && look != shapeName(world, it.shape);
}

QJsonArray contentsOf(const World &world, int map, const MapItem &item)
{
	const MapData &m = world.map(map);
	QJsonArray out;
	for (int k : item.contents)
	{
		const MapItem &c = m.items[k];
		if (c.deleted)
			continue;
		const QString name = countedName(world, map, c);
		if (holdsAnything(m, c))
			out.append(QJsonObject{{"name", name}, {"contents", contentsOf(world, map, c)}});
		else
			out.append(name);
	}
	return out;
}

QString sourceName(MapItem::Source s)
{
	switch (s)
	{
	case MapItem::Fixed: return QStringLiteral("fixed");
	case MapItem::NonFixed: return QStringLiteral("movable");
	case MapItem::Glob: return QStringLiteral("glob");
	case MapItem::Npc: return QStringLiteral("npc");
	}
	return QString();
}

QString kindName(Transition::Kind k)
{
	switch (k)
	{
	case Transition::TeleportEgg: return QStringLiteral("teleportEgg");
	case Transition::UsecodeToEgg: return QStringLiteral("usecodeToEgg");
	case Transition::UsecodeNpcTeleport: return QStringLiteral("usecodeNpcTeleport");
	}
	return QString();
}

// Entry's screen position of a world point (MapRender::build).
QPoint screenOf(int x, int y, int z)
{
	const int x4 = x >> 2, y4 = y >> 2;
	return QPoint(x4 - y4, (x4 >> 1) + (y4 >> 1) - z);
}

bool writeFile(const QString &path, const QByteArray &bytes, QString *error)
{
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(bytes) != bytes.size())
	{
		if (error)
			*error = QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path), f.errorString());
		return false;
	}
	return true;
}

QString kb(qint64 bytes)
{
	if (bytes >= 10 * 1024 * 1024)
		return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
	return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
}

// Takes away what an earlier export wrote, and nothing else.
void clearOld(QDir out)
{
	for (const QString &f : out.entryList({"atlas-*.png"}, QDir::Files))
		out.remove(f);
	for (const char *f : {"manifest.json", "frames.bin", "shapes.json", "graph.json"})
		out.remove(QLatin1String(f));
	QDir maps(out.filePath("maps"));
	static const QRegularExpression number(QStringLiteral("^-?\\d+$"));
	for (const QString &d : maps.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
	{
		if (!number.match(d).hasMatch())
			continue;
		QDir m(maps.filePath(d));
		m.remove("items.bin");
		m.remove("info.json");
		maps.rmdir(d);	// only when empty
	}
}

}

bool write(const World &world, const QString &dirPath, const Options &options, QTextStream &report, QString *error)
{
	const Shapes &shapes = world.shapes();
	QVector<int> mapList = options.maps.isEmpty() ? world.mapNumbers() : options.maps;
	std::sort(mapList.begin(), mapList.end());
	mapList.erase(std::unique(mapList.begin(), mapList.end()), mapList.end());
	const QVector<int> known = world.mapNumbers();
	for (int m : mapList)
		if (!known.contains(m))
		{
			if (error)
				*error = QStringLiteral("map %1 has no items").arg(m);
			return false;
		}

	QDir out(dirPath);
	if (!out.mkpath(".") || !out.mkpath("maps"))
	{
		if (error)
			*error = QStringLiteral("cannot make %1").arg(QDir::toNativeSeparators(dirPath));
		return false;
	}
	clearOld(out);

	// ---- Each map's items in drawing order, and which of them a click says more about ----
	const LayerOptions layers;	// u8edit's defaults
	const auto &transitions = world.transitions();
	std::map<int, MapOut> maps;
	for (int n : mapList)
	{
		MapOut &mo = maps[n];
		mo.number = n;
		mo.render.build(world, n);
	}
	// The items transitions start and end at always get an entry: the item itself when it is on
	// the map, else what holds it.
	auto onMapRoot = [&](int map, int item) {
		if (map < 0 || item < 0 || !maps.count(map))
			return -1;
		const MapData &d = world.map(map);
		const int r = d.root(item);
		return r >= 0 && d.items[r].onMap() ? r : -1;
	};
	std::map<int, std::set<int>> forced;
	for (const Transition &t : transitions)
	{
		if (int r = onMapRoot(t.fromMap, t.fromItem); r >= 0)
			forced[t.fromMap].insert(r);
		if (int r = onMapRoot(t.toMap, t.toItem); r >= 0)
			forced[t.toMap].insert(r);
	}

	std::map<FrameKey, int> frameIds;
	std::map<FrameKey, const Frame *> frameOf;
	for (auto &[n, mo] : maps)
	{
		const MapData &d = world.map(n);
		mo.infoOf.assign(d.items.size(), -1);
		for (const MapRender::Entry &e : mo.render.entries())
		{
			const MapItem &it = d.items[e.item];
			// The frame drawn: the item's, or shape 0's when it has none (MapRender::build).
			FrameKey key = shapes.frame(it.shape, it.frame) == e.frame ? FrameKey(it.shape, it.frame, e.flip)
				: FrameKey(0, 0, e.flip);
			frameOf[key] = e.frame;
			mo.listed.push_back({&e, key});
			if (wantsInfo(world, n, it) || (forced.count(n) && forced[n].count(e.item)))
			{
				mo.infoOf[e.item] = int(mo.infoItems.size());
				mo.infoItems.push_back(e.item);
			}
			// Without the editor's shapes and invisible items, the roof all the way up.
			if (mo.render.shows(e, layers))
			{
				mo.drawn++;
				mo.bounds = mo.bounds.united(e.rect);
			}
		}
	}
	{
		int id = 0;
		for (auto &[key, frame] : frameOf)
			frameIds[key] = id++;
	}

	// ---- The atlas: shelf-packed, the tallest frames first ----
	std::vector<Packed> packed;
	for (auto &[key, frame] : frameOf)
		packed.push_back({key, frame});
	std::vector<int> order(packed.size());
	for (size_t i = 0; i < order.size(); i++)
		order[i] = int(i);
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
		const Frame *fa = packed[a].frame, *fb = packed[b].frame;
		if (fa->height != fb->height)
			return fa->height > fb->height;
		return fa->width > fb->width;
	});
	int pages = 1, px = 0, py = 0, shelf = 0;
	for (int i : order)
	{
		Packed &p = packed[i];
		const int w = p.frame->width, h = p.frame->height;
		if (w <= 0 || h <= 0)
			continue;
		if (w > Page || h > Page)
		{
			if (error)
				*error = QStringLiteral("shape %1 frame %2 is bigger than an atlas page").arg(std::get<0>(p.key)).arg(std::get<1>(p.key));
			return false;
		}
		if (px + w > Page)
		{
			px = 0;
			py += shelf;
			shelf = 0;
		}
		if (py + h > Page)
		{
			pages++;
			px = py = shelf = 0;
		}
		p.page = pages - 1;
		p.x = px;
		p.y = py;
		px += w;
		shelf = std::max(shelf, h);
	}
	const int lastHeight = std::max(1, std::min<int>(Page, py + shelf));
	std::vector<QImage> atlas;
	for (int i = 0; i < pages; i++)
	{
		QImage img(Page, i == pages - 1 ? lastHeight : Page, QImage::Format_RGBA8888);
		img.fill(Qt::transparent);
		atlas.push_back(img);
	}
	const Palette &pal = shapes.palette();
	for (const Packed &p : packed)
	{
		const Frame *f = p.frame;
		const bool flip = std::get<2>(p.key);
		QImage &img = atlas[size_t(p.page)];
		for (int y = 0; y < f->height; y++)
		{
			uchar *line = img.scanLine(p.y + y) + size_t(p.x) * 4;
			for (int x = 0; x < f->width; x++)
			{
				const size_t at = size_t(y) * f->width + (flip ? f->width - 1 - x : x);
				if (!f->opaque[at])
					continue;
				const QRgb c = pal.color(f->pixels[at]);
				uchar *o = line + size_t(x) * 4;
				o[0] = uchar(qRed(c));
				o[1] = uchar(qGreen(c));
				o[2] = uchar(qBlue(c));
				o[3] = 255;
			}
		}
	}
	qint64 atlasBytes = 0;
	QJsonArray atlasJson;
	for (int i = 0; i < pages; i++)
	{
		const QString name = QStringLiteral("atlas-%1.png").arg(i);
		if (!atlas[size_t(i)].save(out.filePath(name), "PNG"))
		{
			if (error)
				*error = QStringLiteral("cannot write %1").arg(QDir::toNativeSeparators(out.filePath(name)));
			return false;
		}
		atlasBytes += QFileInfo(out.filePath(name)).size();
		atlasJson.append(QJsonObject{{"file", name}, {"width", Page}, {"height", atlas[size_t(i)].height()}});
	}

	// frames.bin: 16 bytes a frame id.
	QByteArray framesBin;
	for (const Packed &p : packed)	// in frame id order, as frameOf is
	{
		const Frame *f = p.frame;
		const bool flip = std::get<2>(p.key);
		framesBin.append(char(p.page));
		framesBin.append(char(0));
		put16(framesBin, p.x);
		put16(framesBin, p.y);
		put16(framesBin, f->width);
		put16(framesBin, f->height);
		put16(framesBin, f->left(flip));
		put16(framesBin, f->top());
		put16(framesBin, std::get<0>(p.key));
	}
	if (!writeFile(out.filePath("frames.bin"), framesBin, error))
		return false;

	// ---- Each map's items.bin and info.json ----
	auto infoIndex = [&](int map, int item) -> QJsonValue {
		const int r = onMapRoot(map, item);
		if (r < 0)
			return QJsonValue(QJsonValue::Null);
		const int i = maps.at(map).infoOf[r];
		return i >= 0 ? QJsonValue(i) : QJsonValue(QJsonValue::Null);
	};
	std::set<int> usedShapes;
	QJsonArray manifestMaps;
	qint64 itemsTotal = 0, infoTotal = 0;
	for (auto &[n, mo] : maps)
	{
		const MapData &d = world.map(n);
		QByteArray items;
		items.reserve(int(mo.listed.size() * 16));
		for (const auto &[e, key] : mo.listed)
		{
			const MapItem &it = d.items[e->item];
			usedShapes.insert(it.shape);
			uint8_t flags = uint8_t(it.source << SourceShift);
			if (e->draw)
				flags |= AlwaysDrawn;
			if (e->editor || e->invisible)
				flags |= NotDrawn;
			if (e->trans)
				flags |= Translucent;
			const int info = mo.infoOf[e->item];
			if (info >= 0)
				flags |= HasInfo;
			put32(items, uint32_t(e->sx));
			put32(items, uint32_t(e->sy));
			put16(items, frameIds.at(key));
			items.append(char(it.z));
			items.append(char(flags));
			put32(items, info >= 0 ? uint32_t(info) : NoInfo);
		}

		QJsonArray info;
		for (int item : mo.infoItems)
		{
			const MapItem &it = d.items[item];
			const TypeInfo &t = shapes.type(it.shape);
			QJsonObject o;
			o["item"] = item;
			o["name"] = countedName(world, n, it);
			o["shape"] = it.shape;
			o["frame"] = it.frame;
			o["x"] = it.x;
			o["y"] = it.y;
			o["z"] = it.z;
			o["quality"] = it.quality;
			o["source"] = sourceName(it.source);
			if (it.source == MapItem::Npc && !it.contained() && it.record > 0)
				o["npc"] = it.record;
			const QString cls = world.usecode().className(world.classOf(it));
			if (!cls.isEmpty())
				o["class"] = cls;
			if (it.flags & FlagInvisible)
				o["invisible"] = true;
			if (t.editor)
				o["editorOnly"] = true;

			QStringList desc;
			for (const Names::Fact &f : Names::itemFacts(world, n, item))
				if (!f.basic)
					desc << QStringLiteral("%1: %2").arg(f.field, f.value);
			if (!desc.isEmpty())
				o["desc"] = desc.join(QStringLiteral("; "));
			if (holdsAnything(d, it))
				o["contents"] = contentsOf(world, n, it);

			const int hatch[2] = {EggInfo::xRange(it) * 32, EggInfo::yRange(it) * 32};
			switch (t.family)
			{
			case FamilyTeleportEgg:
				if (EggInfo::sends(it))
				{
					const int dm = EggInfo::destMap(it);
					const int to = dm >= 0 ? world.map(dm).receivingEgg(shapes, EggInfo::teleportId(it)) : -1;
					o["egg"] = QJsonObject{{"kind", "teleport"}, {"sends", true}, {"toMap", dm},
						{"toItem", to >= 0 ? infoIndex(dm, to) : QJsonValue(QJsonValue::Null)},
						{"id", EggInfo::teleportId(it)}, {"fade", (it.quality >> 8) != 0},
						{"range", QJsonArray{hatch[0], hatch[1]}}};
				}
				else
					o["egg"] = QJsonObject{{"kind", "teleport"}, {"receives", true}, {"id", EggInfo::teleportId(it)}};
				break;
			case FamilyMonsterEgg:
			{
				QJsonObject egg{{"kind", "monster"}, {"shape", EggInfo::monsterShape(it)},
					{"count", EggInfo::monsterCount(it)}, {"chance", EggInfo::monsterProb(it)},
					{"activity", EggInfo::monsterActivity(it)}, {"id", EggInfo::monsterId(it)},
					{"range", QJsonArray{hatch[0], hatch[1]}}};
				const QString mon = world.usecode().className(EggInfo::monsterShape(it));
				if (!mon.isEmpty())
					egg["monster"] = mon;
				o["egg"] = egg;
				break;
			}
			case FamilyUsecodeEgg:
				o["egg"] = QJsonObject{{"kind", "usecode"}, {"classId", UsecodeEggClassBase + it.quality},
					{"id", it.mapNum}, {"range", QJsonArray{hatch[0], hatch[1]}}};
				break;
			case FamilyGlobEgg:
				o["egg"] = QJsonObject{{"kind", "glob"}, {"glob", it.quality}};
				break;
			}
			if (DoorInfo::isDoor(it.shape))
				o["door"] = QJsonObject{{"key", DoorInfo::keyOf(it)}, {"locked", DoorInfo::locked(it)},
					{"open", DoorInfo::open(it)}};
			else if (DoorInfo::isKey(it.shape))
				o["key"] = it.quality;

			if (it.source == MapItem::Npc && !it.contained() && it.record > 0 && world.hasNpcData())
			{
				const NpcStats s = world.npcStats(it.record);
				o["npcStats"] = QJsonObject{{"str", s.str}, {"dex", s.dex}, {"int", s.intelligence},
					{"hp", s.hp}, {"mana", s.mana}, {"alignment", s.alignment},
					{"dead", (s.flags2 & NpcStats::Dead) != 0}};
			}
			info.append(o);
		}

		const QString sub = QStringLiteral("maps/%1").arg(n);
		if (!out.mkpath(sub))
		{
			if (error)
				*error = QStringLiteral("cannot make %1").arg(QDir::toNativeSeparators(out.filePath(sub)));
			return false;
		}
		const QByteArray infoBytes = QJsonDocument(info).toJson(QJsonDocument::Compact);
		if (!writeFile(out.filePath(sub + "/items.bin"), items, error) ||
			!writeFile(out.filePath(sub + "/info.json"), infoBytes, error))
			return false;
		mo.itemsBytes = items.size();
		mo.infoBytes = infoBytes.size();
		itemsTotal += mo.itemsBytes;
		infoTotal += mo.infoBytes;

		QJsonObject entry{{"id", n}, {"items", int(mo.listed.size())}, {"drawn", mo.drawn},
			{"info", int(mo.infoItems.size())}};
		const QString name = Names::mapName(n);
		if (!name.isEmpty())
			entry["name"] = name;
		// Max is one past the last pixel.
		entry["bounds"] = QJsonObject{{"minX", mo.bounds.left()}, {"minY", mo.bounds.top()},
			{"maxX", mo.bounds.left() + mo.bounds.width()}, {"maxY", mo.bounds.top() + mo.bounds.height()}};
		manifestMaps.append(entry);
	}

	// ---- shapes.json: every shape an exported item has ----
	QJsonObject shapeNames;
	for (int s : usedShapes)
		shapeNames[QString::number(s)] = shapeName(world, s);
	const QByteArray shapesBytes = QJsonDocument(shapeNames).toJson(QJsonDocument::Compact);
	if (!writeFile(out.filePath("shapes.json"), shapesBytes, error))
		return false;

	// ---- graph.json: the Transitions view ----
	std::set<int> nodes(known.begin(), known.end());
	for (auto it = options.graphPositions.begin(); it != options.graphPositions.end(); ++it)
		nodes.insert(it.key());
	for (const Transition &t : transitions)
	{
		nodes.insert(t.fromMap);
		nodes.insert(t.toMap);
	}
	QJsonArray graphMaps;
	for (int n : nodes)
	{
		QJsonObject o{{"id", n}, {"exported", maps.count(n) > 0}};
		if (n < 0)
			o["name"] = QStringLiteral("Usecode (any map)");
		else
		{
			const QString name = Names::mapName(n);
			if (!name.isEmpty())
				o["name"] = name;
			const MapData &d = world.map(n);
			int count = 0;
			QJsonArray people;
			QStringList seen;
			for (const MapItem &it : d.items)
			{
				count += !it.deleted;
				// MainWindow::peopleOn, all of them.
				if (!it.deleted && it.source == MapItem::Npc && !it.contained() && it.z < 0xfb)
				{
					const QString p = world.itemName(n, it, true);
					if (!p.isEmpty() && p[0].isUpper() && !seen.contains(p))
					{
						seen << p;
						people.append(p);
					}
				}
			}
			int eggsOut = 0;
			for (const Transition &t : transitions)
				eggsOut += t.kind == Transition::TeleportEgg && t.fromMap == n && t.toMap != n;
			o["items"] = count;
			o["eggsOut"] = eggsOut;
			o["people"] = people;
		}
		if (options.graphPositions.contains(n))
		{
			const QPointF p = options.graphPositions.value(n);
			o["x"] = p.x();
			o["y"] = p.y();
		}
		graphMaps.append(o);
	}
	QJsonArray graphTransitions;
	for (const Transition &t : transitions)
	{
		QJsonObject o{{"kind", kindName(t.kind)}, {"fromMap", t.fromMap}, {"toMap", t.toMap},
			{"fromItem", infoIndex(t.fromMap, t.fromItem)}, {"toItem", infoIndex(t.toMap, t.toItem)},
			{"toEgg", t.toEgg >= 0 ? QJsonValue(t.toEgg) : QJsonValue(QJsonValue::Null)}};
		if (t.toItem >= 0 || t.kind == Transition::UsecodeNpcTeleport)
		{
			const QPoint s = screenOf(t.x, t.y, t.z);
			o["x"] = t.x;
			o["y"] = t.y;
			o["z"] = t.z;
			o["sx"] = s.x();
			o["sy"] = s.y();
		}
		const QString cls = t.classId >= 0 ? world.usecode().className(t.classId) : QString();
		o["class"] = cls.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(cls);
		if (t.classId >= 0)
			o["classId"] = t.classId;
		graphTransitions.append(o);
	}
	const QByteArray graphBytes = QJsonDocument(QJsonObject{{"maps", graphMaps}, {"transitions", graphTransitions}})
		.toJson(QJsonDocument::Indented);
	if (!writeFile(out.filePath("graph.json"), graphBytes, error))
		return false;

	// ---- manifest.json ----
	const QByteArray manifestBytes = QJsonDocument(QJsonObject{{"format", "u8web"}, {"version", 1},
		{"maps", manifestMaps}, {"atlas", atlasJson}, {"frames", int(packed.size())},
		{"shapes", int(usedShapes.size())}, {"transitions", int(transitions.size())},
		{"save", QFileInfo(world.saveSource()).fileName()}}).toJson(QJsonDocument::Indented);
	if (!writeFile(out.filePath("manifest.json"), manifestBytes, error))
		return false;

	// ---- The report ----
	report << QStringLiteral("u8web export to %1\n\n").arg(QDir::toNativeSeparators(out.absolutePath()));
	report << QStringLiteral("%1 %2 %3 %4 %5 %6 %7\n").arg("map", 5).arg("listed", 8).arg("drawn", 8)
		.arg("info", 7).arg("items.bin", 11).arg("info.json", 11).arg("total", 11);
	for (const auto &[n, mo] : maps)
		report << QStringLiteral("%1 %2 %3 %4 %5 %6 %7\n").arg(n, 5).arg(mo.listed.size(), 8).arg(mo.drawn, 8)
			.arg(mo.infoItems.size(), 7).arg(kb(mo.itemsBytes), 11).arg(kb(mo.infoBytes), 11)
			.arg(kb(mo.itemsBytes + mo.infoBytes), 11);
	const qint64 total = atlasBytes + framesBin.size() + shapesBytes.size() + graphBytes.size() +
		manifestBytes.size() + itemsTotal + infoTotal;
	report << "\nby kind:\n";
	auto line = [&](const QString &what, qint64 bytes) {
		report << QStringLiteral("  %1 %2 %3%\n").arg(what, -28).arg(kb(bytes), 11)
			.arg(total ? 100.0 * bytes / total : 0, 5, 'f', 1);
	};
	line(QStringLiteral("atlas (%1 page%2)").arg(pages).arg(pages == 1 ? "" : "s"), atlasBytes);
	line(QStringLiteral("frames.bin (%1 frames)").arg(packed.size()), framesBin.size());
	line(QStringLiteral("items.bin (%1 maps)").arg(maps.size()), itemsTotal);
	line(QStringLiteral("info.json"), infoTotal);
	line(QStringLiteral("shapes.json (%1 shapes)").arg(usedShapes.size()), shapesBytes.size());
	line(QStringLiteral("graph.json (%1 transitions)").arg(transitions.size()), graphBytes.size());
	line(QStringLiteral("manifest.json"), manifestBytes.size());
	report << QStringLiteral("  %1 %2\n").arg("total", -28).arg(kb(total), 11);
	report.flush();
	return true;
}

}
