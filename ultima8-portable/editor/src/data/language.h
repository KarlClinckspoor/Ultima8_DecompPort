// A new language for the game: a copy of the English game folder with the usecode's strings
// translated, the letters the translation needs added to the fonts, and (by choice) the
// English voices left out. The same as tools/lang/u8lang.py build, which documents the table
// and the font patch.
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

// A translation table (tools/lang/pt_BR.json): the English strings of the usecode, each with
// its translation, and the string positions to leave as they are.
struct TranslationTable
{
	struct Entry
	{
		QString translation;
		QString className;	// "197 GRAVE_NS": graves and plaques are carved without accents
	};

	QString language;		// "Português (Brasil)"
	QString folder;			// the folder it suggests, "PORTUGUESE"
	QString fontPatch;		// the font patch's path, beside the table; empty if it names none
	QHash<QString, Entry> strings;	// by the English text
	QHash<quint32, bool> keep;	// (class << 16 | string number) left as they are
	int count = 0;			// strings in the table
	int translated = 0;		// of those, with a translation

	bool load(const QString &path, QString *error);
	static quint32 key(int classId, int n) { return quint32(classId) << 16 | quint32(n); }
};

// A font patch (tools/lang/u8fonts-pt.json): frames to add to fonts, and the byte each
// character the translation uses becomes.
struct FontPatch
{
	struct Glyph
	{
		int font;
		int code;
		QByteArray frame;
	};

	QVector<Glyph> glyphs;
	QHash<QChar, uint8_t> characters;
	QByteArray baseSha256;
	qint64 baseSize = 0;

	bool load(const QString &path, QString *error);
	// U8FONTS.FLX with the glyphs: each frame appended to its font's shape and the frame table
	// pointing at it. Empty, with *error set, if a font is missing.
	QByteArray apply(const QByteArray &fonts, QString *error) const;
};

namespace Language
{

struct Options
{
	QString englishDir;	// the English game folder (the one with STATIC, USECODE, SOUND)
	QString tablePath;
	QString outputDir;	// made; must not exist yet, or be empty
	bool removeVoices = true;
};

struct Result
{
	int translated = 0;	// strings placed
	int untranslated = 0;	// strings the table had no translation for
	int largestClass = 0;
	int voicesRemoved = 0;
	QStringList warnings;
};

// Makes the language's folder. progress, if given, is told what is being done.
bool build(const Options &options, Result *result, QString *error,
	const std::function<void(const QString &)> &progress = {});

// The pieces, for tests and tools.

// Each usecode class's strings (push string, opcode 0x0d), in order, by class.
QHash<int, QVector<QByteArray>> usecodeStrings(const QByteArray &usecode, QString *error);
// The usecode with new strings ({class: every string of the class, in order}); classes left
// out keep theirs. Branches, calls, spawns and the event table are moved to match.
QByteArray rebuildUsecode(const QByteArray &usecode, const QHash<int, QVector<QByteArray>> &strings,
	QString *error);
// The usecode's text as the game's fonts draw it: CP437.
QString fromCp437(const QByteArray &bytes);
// A translated string as bytes for the fonts: ASCII, and the characters of the font patch.
// Carved text (graves, plaques) loses its accents. Empty, with *error set, if a character has
// no glyph.
QByteArray encode(const QString &text, const QHash<QChar, uint8_t> &characters, bool carved,
	QString *error);

}
