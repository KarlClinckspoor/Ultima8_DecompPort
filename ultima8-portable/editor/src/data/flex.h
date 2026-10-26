// The game's two container formats: flex files (numbered records) and saved games (named files).
#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

#include <vector>

#include <cstdint>

inline uint16_t read16(const char *p)
{
	return uint16_t(uint8_t(p[0]) | uint8_t(p[1]) << 8);
}

inline uint32_t read32(const char *p)
{
	return uint32_t(read16(p)) | uint32_t(read16(p + 2)) << 16;
}

// A flex file: an 0x80 byte header (a label, then the record count at 0x54), a table of
// (offset, size) pairs, then the records (polib/flex.h). An empty record has size 0.
class Flex
{
public:
	bool load(const QString &path, QString *error);
	bool setData(const QByteArray &data, const QString &name, QString *error);
	int count() const { return int(index_.size()); }
	int size(int record) const;
	QByteArray record(int record) const;
	// The record's bytes, valid while this Flex lives; null when the record is empty.
	const char *data(int record) const;

private:
	struct Entry
	{
		uint32_t offset;
		uint32_t size;
	};
	QByteArray data_;
	std::vector<Entry> index_;
};

// A saved game, U8SAVE.nnn: "Ultima 8 SaveGame File." and a NUL, a 16-bit file count, then for
// each file its name's length (with its NUL), the name, its size and its bytes
// (SavedGames::pack in u8/savegame.cpp). Names are kept upper case.
class SaveFile
{
public:
	bool load(const QString &path, QString *error);
	bool has(const QString &name) const { return files_.contains(name.toUpper()); }
	QByteArray file(const QString &name) const { return files_.value(name.toUpper()); }
	QStringList names() const { return files_.keys(); }

private:
	QMap<QString, QByteArray> files_;
};
