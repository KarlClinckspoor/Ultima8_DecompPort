#include "ui/names.h"

#include <QFile>
#include <QObject>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

#include <algorithm>

#include "data/world.h"

namespace Names
{

QString mapName(int map)
{
	return QSettings().value(QStringLiteral("mapNames/%1").arg(map)).toString();
}

void setMapName(int map, const QString &name)
{
	QSettings s;
	if (name.trimmed().isEmpty())
		s.remove(QStringLiteral("mapNames/%1").arg(map));
	else
		s.setValue(QStringLiteral("mapNames/%1").arg(map), name.trimmed());
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
	return QSettings().value(QStringLiteral("shapeNames/%1").arg(shape)).toString();
}

void setShapeName(int shape, const QString &name)
{
	QSettings s;
	if (name.trimmed().isEmpty())
		s.remove(QStringLiteral("shapeNames/%1").arg(shape));
	else
		s.setValue(QStringLiteral("shapeNames/%1").arg(shape), name.trimmed());
}

bool fullNames()
{
	return QSettings().value(QStringLiteral("fullNames"), true).toBool();
}

void setFullNames(bool full)
{
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
