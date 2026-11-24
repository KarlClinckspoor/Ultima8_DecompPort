// The web export (--export-web): the maps as the Ultima Cartographer draws them. Each map is a
// picture in screen pixels: its items in the order the map view draws them, their frames packed
// into atlas pages, what a click on an item shows, and the maps as the Transitions view lays
// them out. The contract is the Cartographer's docs/u8-export.md.
#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>

class World;
class QTextStream;

namespace WebExport
{

struct Options
{
	QVector<int> maps;	// empty: every map with items
	// Where each map sits in the Transitions view (node -1: usecode tied to no map).
	QHash<int, QPointF> graphPositions;
};

// Writes the export to dir (made if missing; the files it writes are replaced, anything else
// is left alone) and a size report and counts to report. False, with why, on failure.
bool write(const World &world, const QString &dir, const Options &options, QTextStream &report, QString *error);

}
