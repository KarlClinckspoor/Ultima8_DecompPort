// The names the editor shows: the maps' and shapes' names the user gave (kept in the editor's
// settings, and written to and read from a names file to share them), and the name an item goes
// by everywhere: what it says when looked at, else its shape's name, else its usecode class.
#pragma once

#include <QString>

class World;
struct MapItem;

namespace Names
{

// A map's name, or empty.
QString mapName(int map);
void setMapName(int map, const QString &name);
// "5 · Tenebrae", or "Map 5".
QString mapLabel(int map);
// The same, short: "5 · Tenebrae", or "5".
QString mapShort(int map);

// The name given to a shape, or empty.
QString shapeName(int shape);
void setShapeName(int shape, const QString &name);

// Whether items go by their full names (the names they have once the story has got far enough)
// or by what the saved game's flags make them say.
bool fullNames();
void setFullNames(bool full);

// What the item goes by: its look name, else its shape's name, else its class name made
// readable ("DOOR_NS" is "door ns"), else its family.
QString itemName(const World &world, int map, const MapItem &item);
// The name and, when the name is not just the class, the class and shape: "key — KEY (shape 82:0)".
QString itemLabel(const World &world, int map, const MapItem &item);

// The names file: "map N = name" and "shape N = name" lines. Reading it adds to and replaces
// the names given; returns false and says why when the file cannot be used.
bool write(const QString &path, QString *error);
bool read(const QString &path, int *count, QString *error);

}
