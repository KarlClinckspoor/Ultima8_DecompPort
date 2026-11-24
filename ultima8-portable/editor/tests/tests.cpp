// u8edit-tests: the editor against a copy of the game's data (U8_DATA).
//
//   U8_DATA=~/games/ultima8 QT_QPA_PLATFORM=offscreen build/editor/u8edit-tests
//
// The files are read and written back unchanged, then the main window is driven as a user would:
// select, drag, undo, place, delete, change a weapon, save; and what was saved is read again.

#include <QCryptographicHash>
#include <QDir>
#include <QLineEdit>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

#include "data/itemnames.h"
#include "data/language.h"
#include "data/world.h"
#include "ui/edits.h"
#include "ui/gamedata.h"
#include "ui/mainwindow.h"
#include "ui/translatedialog.h"
#include "ui/mapview.h"
#include "ui/searchpanel.h"
#include "ui/manualview.h"
#include "ui/usecodeview.h"
#include "ui/usecodegraph.h"
#include "ui/webexport.h"
#include "data/ucasm.h"
#include "data/ucdecomp.h"
#include "data/ucgen.h"
#include "data/ucparse.h"

#include <QGraphicsScene>
#include <QPainter>
#include "data/disasm.h"
#include "data/ucode.h"

#include <QTreeWidget>

#include <QTextBlock>
#include <QTextBrowser>

namespace
{

QString find(const QString &dir, const QString &name)
{
	QDir d(dir);
	for (const QString &e : d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot))
		if (e.compare(name, Qt::CaseInsensitive) == 0)
			return d.filePath(e);
	return QString();
}

bool copyTree(const QString &from, const QString &to)
{
	QDir().mkpath(to);
	for (const QFileInfo &f : QDir(from).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot))
	{
		const QString target = QDir(to).filePath(f.fileName());
		if (f.isDir() ? !copyTree(f.filePath(), target) : !QFile::copy(f.filePath(), target))
			return false;
		QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::WriteOwner);
	}
	return true;
}

// The item a map's NPC number stands for.
int npcItem(const World &w, int map, int npc)
{
	const auto &items = w.map(map).items;
	for (size_t i = 0; i < items.size(); i++)
		if (!items[i].deleted && items[i].source == MapItem::Npc && items[i].parent < 0 && items[i].record == npc)
			return int(i);
	return -1;
}

// A mouse move with buttons held, sent to the widget itself: QTest::mouseMove moves the cursor
// instead in some Qt versions (6.2), which the offscreen platform never turns into an event.
void mouseMove(QWidget *w, QPoint at, Qt::MouseButtons buttons = Qt::LeftButton)
{
	QMouseEvent e(QEvent::MouseMove, QPointF(at), QPointF(w->mapToGlobal(at)), Qt::NoButton, buttons, Qt::NoModifier);
	QApplication::sendEvent(w, &e);
}

// A point in the view where the item is drawn, or (-1, -1).
QPoint whereIs(MapView *view, int item)
{
	for (int r = 0; r < 60; r++)
		for (int dy = -r; dy <= r; dy++)
			for (int dx = -r; dx <= r; dx++)
			{
				const QPoint p = view->rect().center() + QPoint(dx, dy);
				const QPointF m = view->center() + (QPointF(p) - QPointF(view->width() / 2.0, view->height() / 2.0)) / view->zoom();
				if (view->render().pick(m.toPoint(), view->options()) == item)
					return p;
			}
	return QPoint(-1, -1);
}

}

class Tests : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void roundTrip();
	void translation();
	void editInWindow();
	void itemNames();
	void flags();
	void groupEdits();
	void doorsAndKeys();
	void picking();
	void manual();
	void usecodeListing();
	void usecodeGraph();
	void usecodeAssembly();
	void usecodeDecompile();
	void usecodeGenerate();
	void searchEveryMap();
	void webExport();

private:
	QTemporaryDir temp_;
	QString data_;
};

void Tests::initTestCase()
{
	QStandardPaths::setTestModeEnabled(true);
	const QString source = qEnvironmentVariable("U8_DATA");
	if (source.isEmpty())
		QSKIP("Set U8_DATA to an Ultima 8 folder.");
	QVERIFY(temp_.isValid());
	data_ = temp_.filePath("u8");
	for (const char *part : {"STATIC", "USECODE", "SAVEGAME"})
	{
		const QString from = find(source, part);
		QVERIFY2(!from.isEmpty(), part);
		QVERIFY(copyTree(from, QDir(data_).filePath(QFileInfo(from).fileName())));
	}
	const QString exe = find(source, "U8.EXE");
	if (!exe.isEmpty())
	{
		QVERIFY(QFile::copy(exe, QDir(data_).filePath("U8.EXE")));
		QFile::setPermissions(QDir(data_).filePath("U8.EXE"), QFile::permissions(QDir(data_).filePath("U8.EXE")) | QFileDevice::WriteOwner);
	}
}

// Every map written back as it was read gives the bytes it was read from.
void Tests::roundTrip()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	Flex fixed;
	QVERIFY(fixed.load(find(find(data_, "STATIC"), "FIXED.DAT"), &error));
	SaveFile save;
	QVERIFY(save.load(w.saveSource(), &error));
	Flex nonFixed;
	QVERIFY(nonFixed.setData(save.file("NONFIXED.DAT"), "NONFIXED.DAT", &error));
	for (int m = 1; m < 256; m++)
	{
		QCOMPARE(w.serializeForTest(m, MapItem::Fixed), fixed.record(m));
		if (!w.map(m).fromCache)
			QCOMPARE(w.serializeForTest(m, MapItem::NonFixed), nonFixed.record(m));
	}
	QVERIFY(!w.dirty());
}

// File > Apply a Translation, with the Portuguese table: the usecode keeps its code and gets
// every string, the fonts get their letters, the voices go.
void Tests::translation()
{
	QString error;
	QFile f(QDir(data_).filePath(find(find(data_, "USECODE"), "EUSECODE.FLX")));
	QVERIFY(f.open(QIODevice::ReadOnly));
	const QByteArray usecode = f.readAll();
	QCOMPARE(Language::rebuildUsecode(usecode, {}, &error), usecode);
	QCOMPARE(Language::rebuildUsecode(usecode, Language::usecodeStrings(usecode, &error), &error), usecode);

	// A voice file and the music, to see which stay.
	QDir(data_).mkpath("SOUND");
	for (const char *name : {"SOUND/E80.FLX", "SOUND/MUSIC.FLX"})
	{
		QFile s(QDir(data_).filePath(name));
		QVERIFY(s.open(QIODevice::WriteOnly));
		s.write("x");
	}

	Language::Options o;
	o.englishDir = data_;
	o.tablePath = QStringLiteral(U8EDIT_LANG_DIR "/pt_BR.json");
	o.outputDir = temp_.filePath("PORTUGUESE");
	Language::Result r;
	QVERIFY2(Language::build(o, &r, &error), qPrintable(error));
	QVERIFY(r.translated > 10000);
	QCOMPARE(r.untranslated, 0);
	QVERIFY(r.largestClass < 0xffff);
	QVERIFY2(r.warnings.isEmpty(), qPrintable(r.warnings.join('\n')));

	QFile pt(find(find(o.outputDir, "USECODE"), "EUSECODE.FLX"));
	QVERIFY(pt.open(QIODevice::ReadOnly));
	const QByteArray translated = pt.readAll();
	const auto before = Language::usecodeStrings(usecode, &error);
	const auto after = Language::usecodeStrings(translated, &error);
	QCOMPARE(after.size(), before.size());
	for (auto it = before.begin(); it != before.end(); ++it)
		QCOMPARE(after.value(it.key()).size(), it->size());
	qInfo("EUSECODE.FLX %s, U8FONTS.FLX %s",
		QCryptographicHash::hash(translated, QCryptographicHash::Md5).toHex().constData(),
		[&] {
			QFile fonts(find(find(o.outputDir, "STATIC"), "U8FONTS.FLX"));
			fonts.open(QIODevice::ReadOnly);
			return QCryptographicHash::hash(fonts.readAll(), QCryptographicHash::Md5).toHex();
		}().constData());
	QVERIFY(!QFile::exists(QDir(o.outputDir).filePath("GAMEDAT/NONFIXED.DAT")));
	QCOMPARE(r.voicesRemoved, 1);
	QVERIFY(!QFile::exists(QDir(o.outputDir).filePath("SOUND/E80.FLX")));
	QVERIFY(QFile::exists(QDir(o.outputDir).filePath("SOUND/MUSIC.FLX")));

	// The dialog finds the table that comes with the editor and suggests a folder beside the
	// English one.
	TranslateDialog dialog(data_);
	QVERIFY(!TranslateDialog::bundledTables().isEmpty());
	const QList<QLineEdit *> edits = dialog.findChildren<QLineEdit *>();
	QCOMPARE(edits.size(), 3);
	QVERIFY(edits[0]->text().endsWith("pt_BR.json"));
	QCOMPARE(QDir::fromNativeSeparators(edits[2]->text()), QDir(data_ + "/..").absoluteFilePath("PORTUGUESE"));
}

void Tests::editInWindow()
{
	MainWindow window;
	window.resize(1200, 800);
	window.show();
	QString error;
	QVERIFY2(window.loadWorld(data_, QString(), &error), qPrintable(error));
	QVERIFY(window.openMap(3));
	const World &w = *window.world();
	MapView *view = window.mapView();

	// Select Devon by clicking him, then drag him.
	const int devon = npcItem(w, 3, 2);
	QVERIFY(devon >= 0);
	window.goToItem(3, devon, false);
	view->select(-1);
	QApplication::processEvents();
	const QPoint at = whereIs(view, devon);
	QVERIFY(at.x() >= 0);
	QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, at);
	QCOMPARE(view->selected(), devon);
	const MapItem before = w.map(3).items[devon];
	QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, at);
	mouseMove(view, at + QPoint(20, 10));
	mouseMove(view, at + QPoint(40, 20));
	QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, at + QPoint(40, 20));
	const MapItem moved = w.map(3).items[devon];
	QVERIFY(moved.x != before.x || moved.y != before.y);
	QCOMPARE(moved.z, before.z);
	QVERIFY(w.dirty());
	window.editor()->undo()->undo();
	QCOMPARE(w.map(3).items[devon].x, before.x);
	QCOMPARE(w.map(3).items[devon].y, before.y);

	// Place a barrel where Devon stands, then delete it with the key and bring it back.
	const size_t count = w.map(3).items.size();
	view->startPlacing(115, 0);
	QTest::mouseMove(view, at);
	QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, at);
	QVERIFY(w.map(3).items.size() > count);
	int barrel = -1;
	for (size_t i = count; i < w.map(3).items.size(); i++)
		if (w.map(3).items[i].shape == 115 && w.map(3).items[i].source == MapItem::NonFixed)
			barrel = int(i);
	QVERIFY(barrel >= 0);
	QTest::keyClick(view, Qt::Key_Escape);
	QVERIFY(!view->placing());
	view->select(barrel);
	QTest::keyClick(view, Qt::Key_Delete);
	QVERIFY(w.map(3).items[barrel].deleted);
	window.editor()->undo()->undo();
	QVERIFY(!w.map(3).items[barrel].deleted);
	const MapItem placed = w.map(3).items[barrel];

	// The mace's base damage, typed into the table.
	if (w.exeTables().loaded())
	{
		auto *weapons = window.gameData()->findChild<QTableWidget *>("weapons");
		QVERIFY(weapons);
		weapons->item(0, 2)->setText("50");
		QCOMPARE(w.exeTables().weapons[0].baseDamage, 50);
	}

	QVERIFY(window.save());
	QVERIFY(!w.dirty());
	World again;
	QVERIFY2(again.load(data_, QString(), &error), qPrintable(error));
	bool found = false;
	for (const MapItem &it : again.map(3).items)
		found |= !it.deleted && it.shape == 115 && it.x == placed.x && it.y == placed.y && it.z == placed.z;
	QVERIFY(found);
	const int devonAgain = npcItem(again, 3, 2);
	QCOMPARE(again.map(3).items[devonAgain].x, before.x);
	if (again.exeTables().loaded())
		QCOMPARE(again.exeTables().weapons[0].baseDamage, 50);
	QVERIFY(QFile::exists(QDir(data_).filePath("SAVEGAME/U8SAVE.000.orig")));
}

// Names come from the class's look event, run in the editor's usecode machine: a key's by its
// quality, Devon's by what the flags say the Avatar knows of him.
void Tests::itemNames()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	ItemNames names(w, false);
	MapItem key;
	key.shape = 82;	// KEY
	key.quality = 59;
	QCOMPARE(names.look(0, key, &error), QStringLiteral("Key of the Conventicler"));
	key.quality = 0;
	QCOMPARE(names.look(0, key, &error), QStringLiteral("key blank"));

	int map = -1, devon = -1;
	for (int m : w.mapNumbers())
		if ((devon = npcItem(w, m, 2)) >= 0)
		{
			map = m;
			break;
		}
	QVERIFY(devon >= 0);
	const MapItem &it = w.map(map).items[devon];
	QCOMPARE(w.classOf(it), NpcClassBase + 2);
	// The full name does not wait for the story.
	QCOMPARE(w.itemName(map, it, true), QStringLiteral("Devon"));
	QCOMPARE(w.itemName(map, it, false), QStringLiteral("man"));
	w.setFlag(253, 2, 1);
	QCOMPARE(w.itemName(map, it, false), QStringLiteral("fisherman"));
	w.setFlag(252, 1, 1);
	QCOMPARE(w.itemName(map, it, false), QStringLiteral("Devon"));

	// An item with no look event has no name.
	MapItem plain;
	plain.shape = 190;
	QVERIFY(names.look(0, plain, &error).isEmpty());
	QVERIFY(!error.isEmpty());
}

// A flag set in the editor is written to the saved game's FLAG.DAT and read back.
void Tests::flags()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	QCOMPARE(w.flag(252, 1), 0);
	QVERIFY(!w.usecode().flagUses().isEmpty());
	w.setFlag(252, 1, 1);
	w.setFlag(1000, 4, 0x1f);	// kept to its 4 bits
	QCOMPARE(w.flag(252, 1), 1);
	QCOMPARE(w.flag(1000, 4), 0xf);
	QVERIFY(w.dirty());
	QVERIFY(w.pendingFiles().join(' ').contains("U8SAVE", Qt::CaseInsensitive));
	QVERIFY2(w.save(&error), qPrintable(error));
	QVERIFY(w.flagsInSave());

	World again;
	QVERIFY2(again.load(data_, QString(), &error), qPrintable(error));
	QVERIFY(again.flagsInSave());
	QCOMPARE(again.flag(252, 1), 1);
	QCOMPARE(again.flag(1000, 4), 0xf);
	QCOMPARE(again.flag(253, 2), 0);
	QCOMPARE(again.flagBytes().size(), int(World::FlagFileSize));
	SaveFile save;
	QVERIFY(save.load(again.saveSource(), &error));
	QCOMPARE(save.file("FLAG.DAT").size(), int(World::FlagFileSize));
}

// Moving, flipping, copying and deleting several items are each one step to undo.
void Tests::groupEdits()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	Editor editor;
	editor.setWorld(&w);
	int heard = 0;
	connect(&editor, &Editor::mapChanged, this, [&heard] { heard++; });
	const int map = 3;
	QVector<int> items;
	const auto &list = w.map(map).items;
	for (size_t i = 0; i < list.size() && items.size() < 5; i++)
		if (list[i].onMap() && list[i].source == MapItem::NonFixed)
			items << int(i);
	QCOMPARE(items.size(), 5);
	QVector<MapItem> before;
	for (int i : items)
		before << list[i];

	QVERIFY(editor.moveItems(map, items, 16, -8, 1));
	QCOMPARE(editor.undo()->count(), 1);
	QCOMPARE(heard, 1);	// the views hear once
	for (int k = 0; k < items.size(); k++)
	{
		QCOMPARE(int(list[items[k]].x), std::clamp(before[k].x + 16, 0, 0x7fff));
		QCOMPARE(int(list[items[k]].y), std::clamp(before[k].y - 8, 0, 0x7fff));
	}
	QVERIFY(editor.editItems(map, items, [](MapItem &i) { i.flags ^= FlagFlipped; }, "flip"));
	QCOMPARE(list[items[0]].flipped(), !before[0].flipped());
	const QVector<int> copies = editor.duplicateItems(map, items, 32, 0);
	QCOMPARE(copies.size(), items.size());
	QVERIFY(editor.deleteItems(map, items));
	for (int i : items)
		QVERIFY(list[i].deleted);
	QCOMPARE(editor.undo()->count(), 4);
	for (int k = 0; k < 4; k++)
		editor.undo()->undo();
	for (int k = 0; k < items.size(); k++)
	{
		QVERIFY(!list[items[k]].deleted);
		QVERIFY(list[items[k]].sameRecord(before[k]));
	}
	for (int c : copies)
		QVERIFY(list[c].deleted);
}

// A key opens the doors whose quality's low byte is its quality (KEYRING's use routine), and
// the search finds both by key:N.
void Tests::doorsAndKeys()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	int keys = 0, matched = 0;
	for (int m : w.mapNumbers())
	{
		const auto &items = w.map(m).items;
		for (size_t i = 0; i < items.size(); i++)
		{
			const MapItem &k = items[i];
			if (k.deleted || !DoorInfo::isKey(k.shape) || !k.quality)
				continue;
			keys++;
			QVERIFY(SearchPanel::matches(w, m, int(i), QStringLiteral("key:%1").arg(k.quality)));
			bool found = false;
			for (int m2 : w.mapNumbers())
				for (size_t j = 0; j < w.map(m2).items.size() && !found; j++)
				{
					const MapItem &d = w.map(m2).items[j];
					if (!d.deleted && DoorInfo::isDoor(d.shape) && DoorInfo::keyOf(d) == k.quality)
					{
						found = true;
						QVERIFY(SearchPanel::matches(w, m2, int(j), QStringLiteral("key:%1 is:door").arg(k.quality)));
					}
				}
			matched += found;
		}
	}
	QVERIFY(keys > 0);
	QVERIFY2(matched * 2 > keys, qPrintable(QStringLiteral("%1 of %2 keys open a door").arg(matched).arg(keys)));

	// Words match names, a number alone is a shape, and a bad term says why.
	const auto &items = w.map(3).items;
	int devon = npcItem(w, 3, 2);
	if (devon >= 0)
		QVERIFY(SearchPanel::matches(w, 3, devon, QStringLiteral("devon")));
	for (size_t i = 0; i < items.size(); i++)
		if (items[i].onMap())
		{
			QVERIFY(SearchPanel::matches(w, 3, int(i), QString::number(items[i].shape)));
			break;
		}
	QVERIFY(!SearchPanel::matches(w, 3, 0, QStringLiteral("shape:x"), &error));
	QVERIFY(!error.isEmpty());
}

// The right-click list has what is drawn at the point first, then what is near it; X-ray
// passes over what it fades.
void Tests::picking()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	MapRender r;
	r.build(w, 3);
	LayerOptions o;
	int tested = 0;
	for (const MapRender::Entry &e : r.entries())
	{
		if (e.rect.width() < 8 || e.rect.height() < 8 || !r.shows(e, o))
			continue;
		const QPoint pt(e.sx, e.sy - 2);
		const int top = r.pick(pt, o);
		if (top < 0)
			continue;
		const std::vector<int> all = r.pickAll(pt, 6, o);
		QVERIFY(!all.empty());
		QCOMPARE(all.front(), top);
		QVERIFY(std::find(all.begin(), all.end(), top) != all.end());
		// Fading everything at the point makes the pick pass over it.
		XRay x;
		x.on = true;
		x.center = pt;
		x.radius = 4;
		x.height = 0;
		std::fill(std::begin(x.opacity), std::end(x.opacity), 0);
		QCOMPARE(r.pick(pt, o, &x), -1);
		if (++tested == 20)
			break;
	}
	QVERIFY(tested > 0);
}

// The manual is built in, its sections are where Help leads, and every passage marked as
// inferred (brown) says so in a tooltip.
void Tests::manual()
{
	ManualView view;
	QTextDocument *doc = view.browser()->document();
	QVERIFY(doc->toPlainText().size() > 2000);
	const QString html = ManualView::html();
	for (const char *anchor : {"contents", "keys", "fields"})
		QVERIFY2(html.contains(QStringLiteral("name=\"%1\"").arg(QLatin1String(anchor))), anchor);
	const QColor brown(QStringLiteral("#b0702c"));
	int inferred = 0;
	for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
		for (auto it = b.begin(); !it.atEnd(); ++it)
		{
			const QTextCharFormat f = it.fragment().charFormat();
			if (f.foreground().color() == brown)
			{
				inferred++;
				QVERIFY2(!f.toolTip().isEmpty(), qPrintable(it.fragment().text()));
			}
		}
	QVERIFY(inferred > 0);
	// Links inside the manual lead to anchors that are there.
	static const QRegularExpression link(QStringLiteral("href=\"#([^\"]+)\""));
	for (auto m = link.globalMatch(html); m.hasNext();)
	{
		const QString target = m.next().captured(1);
		QVERIFY2(html.contains(QStringLiteral("name=\"%1\"").arg(target)), qPrintable(target));
	}
}

// Every class prints as assembly text and assembles back to exactly its own bytes, and the whole
// file made from those records is the original file, byte for byte.
void Tests::usecodeAssembly()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	const Usecode &u = w.usecode();
	QHash<int, QByteArray> records;
	int bad = 0;
	qint64 textBytes = 0;
	for (int k = 0; k < u.classCount(); k++)
	{
		if (!u.hasClass(k))
			continue;
		const QByteArray record = u.record(k);
		const QString text = Ucasm::print(record, k, &u, &error);
		if (text.isEmpty())
		{
			qWarning("class %d does not print: %s", k, qPrintable(error));
			bad++;
			continue;
		}
		textBytes += text.size();
		int id = -1;
		const QByteArray back = Ucasm::assemble(text, &id, &error);
		if (back != record || id != k)
		{
			qWarning("class %d does not assemble back to its bytes (%s)", k, qPrintable(error));
			if (!back.isEmpty())
				for (int i = 12; i < qMin(back.size(), record.size()); i++)
					if (back[i] != record[i])
					{
						qWarning("  first difference at record byte %d (code offset %x): %02x, was %02x; sizes %d, was %d",
							i, i - 12, uint8_t(back[i]), uint8_t(record[i]), int(back.size()), int(record.size()));
						break;
					}
			bad++;
		}
		records.insert(k, back);
	}
	QCOMPARE(bad, 0);
	QVERIFY(records.size() > 500);
	qInfo("%d classes, %lld characters of assembly", int(records.size()), textBytes);
	const QByteArray file = Language::replaceUsecodeClasses(u.file(), records, &error);
	QVERIFY2(!file.isEmpty(), qPrintable(error));
	QVERIFY(file == u.file());

	// A change moves what points into the class: a label's users follow it.
	const int key = DoorInfo::KeyShape;
	QString text = Ucasm::print(u.record(key), key, &u, &error);
	const QString pad = QStringLiteral("    push_imm 0\n    pop_result\n");
	text.replace(QStringLiteral("code\n"), QStringLiteral("code\n") + pad);
	int id;
	const QByteArray longer = Ucasm::assemble(text, &id, &error);
	QVERIFY2(!longer.isEmpty(), qPrintable(error));
	QCOMPARE(longer.size(), u.record(key).size() + 4);
	QCOMPARE(int(uint8_t(longer[4])) | int(uint8_t(longer[5])) << 8, int(longer.size()));
	// Its event table points further on, and printing it again assembles to the same bytes.
	int moved = 0;
	for (int e = 0; e < 32; e++)
		if (u.eventOffset(key, e))
		{
			const int off = int(uint8_t(longer[12 + e * 4])) | int(uint8_t(longer[12 + e * 4 + 1])) << 8;
			QVERIFY(off >= u.eventOffset(key, e));
			moved++;
		}
	QVERIFY(moved > 0);
	QCOMPARE(Ucasm::assemble(Ucasm::print(longer, key, &u, &error), &id, &error), longer);
}

// The decompiler lifts most routines of the game to C-like text and leaves the rest as assembly;
// the lifted text of a few known routines is what the code does.
void Tests::usecodeDecompile()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	const Usecode &u = w.usecode();
	int lifted = 0, total = 0, unparsed = 0, reprinted = 0;
	QHash<QString, int> why;
	for (int k = 0; k < u.classCount(); k++)
	{
		if (!u.hasClass(k))
			continue;
		QVector<Ucdecomp::Routine> routines;
		QVERIFY2(Ucdecomp::decompile(u.record(k), k, &routines, &error), qPrintable(error));
		for (const auto &r : routines)
		{
			total++;
			if (r.lifted)
			{
				lifted++;
				// The text parses, and prints again as the same text.
				Ucast::Block block;
				QString perr;
				if (!Ucparse::parseBlock(r.text, &block, &perr))
				{
					if (unparsed++ < 5)
						qWarning("class %d routine %04x does not parse: %s", k, r.start, qPrintable(perr));
				}
				else
				{
					QStringList lines;
					Ucast::render(block, 1, &lines);
					if (lines.join(QChar('\n')) != r.text && reprinted++ < 5)
						qWarning("class %d routine %04x prints differently after parsing", k, r.start);
				}
			}
			else
				why[r.why.section(QChar(' '), 0, 1)]++;
		}
	}
	QStringList reasons;
	for (auto it = why.cbegin(); it != why.cend(); ++it)
		reasons << QStringLiteral("%1 %2").arg(it.value()).arg(it.key());
	qInfo("%d of %d routines lifted; not lifted: %s", lifted, total, qPrintable(reasons.join(QStringLiteral(", "))));
	QVERIFY(total > 1500);
	QVERIFY(lifted * 100 / total >= 98);
	QCOMPARE(unparsed, 0);
	QCOMPARE(reprinted, 0);

	// U8_DECOMP_OUT=<folder> writes every class's text there, to read.
	const QString outDir = qEnvironmentVariable("U8_DECOMP_OUT");
	if (!outDir.isEmpty())
	{
		QDir().mkpath(outDir);
		for (int k = 0; k < u.classCount(); k++)
			if (u.hasClass(k))
			{
				QFile f(QDir(outDir).filePath(QStringLiteral("%1_%2.uc").arg(k, 4, 10, QLatin1Char('0')).arg(u.className(k))));
				if (f.open(QIODevice::WriteOnly))
					f.write(Ucdecomp::classText(u.record(k), k, &u, &error).toUtf8());
			}
	}
	const QString egg = Ucdecomp::classText(u.record(1181), 1181, &u, &error);
	QVERIFY(egg.contains(QStringLiteral("if (Egg::getEggId(arg_6) == 1) {\n        return;\n    }")));
	QVERIFY(egg.contains(QStringLiteral("await spawn 1409::1c19(0, 60);")));
	QVERIFY(egg.contains(QStringLiteral("await Item::hurl(&local_2, 0, 0, 0, 4);")));
	const QString stalag = Ucdecomp::classText(u.record(330), 330, &u, &error);
	QVERIFY(stalag.contains(QStringLiteral("await Item::bark(arg_6, (ptr)\"stalactite \");")));
	QVERIFY(stalag.contains(QStringLiteral("while (local_2 <= 10) {")));
	const QString keyring = Ucdecomp::classText(u.record(DoorInfo::KeyringShape), DoorInfo::KeyringShape, &u, &error);
	QVERIFY(keyring.contains(QStringLiteral("foreach (local_2 in search_container(")));
}

// The tree gives the code back: every lifted routine is generated again and compared with the
// shipped bytes. (This first generator reads its choices from what the decompiler recorded.)
void Tests::usecodeGenerate()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	const Usecode &u = w.usecode();
	int total = 0, same = 0, shown = 0;
	QHash<QString, int> why;
	for (int k = 0; k < u.classCount(); k++)
	{
		if (!u.hasClass(k))
			continue;
		const QByteArray record = u.record(k);
		const QByteArray code = record.mid(12);
		QVector<Ucasm::Insn> insns;
		QVERIFY(Ucasm::decodeInsns(code, &insns, &error));
		QVector<Ucdecomp::Routine> routines;
		QVERIFY(Ucdecomp::decompile(record, k, &routines, &error));
		for (const auto &r : routines)
		{
			if (!r.lifted)
				continue;
			total++;
			QString gerr;
			const QByteArray gen = Ucgen::generate(r.tree, r.hasFrame, r.frame, &gerr);
			const QByteArray orig = code.mid(r.start, r.end - r.start);
			if (gen == orig)
			{
				same++;
				continue;
			}
			if (qEnvironmentVariable("U8_GEN_ROUTINE") == QStringLiteral("%1:%2").arg(k).arg(r.start, 4, 16, QLatin1Char('0')))
			{
				auto dump = [&](const char *title, const QByteArray &bytes) {
					QVector<Ucasm::Insn> v;
					QByteArray padded(0x80, char(0));
					padded.append(bytes);
					Ucasm::decodeInsns(padded, &v, nullptr);
					for (const auto &i : v)
					{
						QStringList a;
						for (int64_t x : i.args)
							a << QString::number(x);
						qInfo("%s %04x %s %s", title, i.at - 0x80, qPrintable(i.name), qPrintable(a.join(QChar(','))));
					}
				};
				dump("S", orig);
				dump("M", gen);
			}
			QString key;
			if (gen.isEmpty())
				key = QStringLiteral("cannot: ") + gerr.left(40);
			else
			{
				int d = 0;
				while (d < gen.size() && d < orig.size() && gen[d] == orig[d])
					d++;
				QString mn = QStringLiteral("(end)");
				for (const auto &i : insns)
					if (i.at <= r.start + d && r.start + d < i.at + i.size)
					{
						mn = i.name;
						break;
					}
				key = (gen.size() == orig.size() ? QStringLiteral("same size, ") : QStringLiteral("size differs, ")) + mn;
				if (shown++ < 12 || qEnvironmentVariable("U8_GEN_SHOW") == key)
				{
					qInfo("class %d routine %04x differs at +%x (%s): generated %d bytes, shipped %d", k, r.start, d, qPrintable(mn),
						int(gen.size()), int(orig.size()));
					auto list = [&](const QByteArray &bytes, int at) {
						QVector<Ucasm::Insn> v;
						QByteArray padded(0x80, char(0));
						padded.append(bytes);
						QStringList out;
						if (Ucasm::decodeInsns(padded, &v, nullptr))
							for (const auto &i : v)
								if (i.at >= 0x80 + at - 14 && i.at <= 0x80 + at + 14)
								{
									QStringList a;
									for (int64_t x : i.args)
										a << QString::number(x);
									out << QStringLiteral("%1 %2").arg(i.name, a.join(QChar(',')) + (i.text.isEmpty() ? QString() : QStringLiteral(" '%1'").arg(QString::fromLatin1(i.text.left(12)))));
								}
						return out.join(QStringLiteral(" | "));
					};
					qInfo("   shipped: %s", qPrintable(list(orig, d)));
					qInfo("   made:    %s", qPrintable(list(gen, d)));
				}
			}
			why[key]++;
		}
	}
	QList<QPair<int, QString>> sorted;
	for (auto it = why.cbegin(); it != why.cend(); ++it)
		sorted.append({it.value(), it.key()});
	std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
	QStringList lines;
	for (int i = 0; i < qMin(int(sorted.size()), 25); i++)
		lines << QStringLiteral("%1 %2").arg(sorted[i].first).arg(sorted[i].second);
	qInfo("%d of %d lifted routines generate to the shipped bytes; first differences: %s", same, total,
		qPrintable(lines.join(QStringLiteral("; "))));
	QVERIFY(total > 1500);
	QVERIFY(same * 100 / total >= 98);
}

// Every class disassembles; KEY's look event names its keys with Item::bark, its use event spawns
// KEYRING's routine, and the Usecode tab lists the keys on the maps as KEY's items.
void Tests::usecodeListing()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	const Usecode &u = w.usecode();
	int classes = 0;
	for (int k = 0; k < u.classCount(); k++)
		if (u.hasClass(k))
		{
			QVector<Ucode::Line> lines;
			QVERIFY2(Ucode::disassemble(u, k, &lines, &error), qPrintable(QStringLiteral("%1: %2").arg(k).arg(error)));
			QVERIFY(!lines.isEmpty());
			classes++;
		}
	QVERIFY(classes > 500);

	QVector<Ucode::Line> key;
	QVERIFY(Ucode::disassemble(u, DoorInfo::KeyShape, &key, &error));
	bool lookHead = false, bark = false, blank = false, spawnsKeyring = false;
	for (const Ucode::Line &l : key)
	{
		lookHead |= l.kind == Ucode::Line::Head && l.event == Ucode::EventLook;
		bark |= l.note.startsWith(QLatin1String("Item::bark"));
		blank |= l.note.contains(QLatin1String("key blank"));
		spawnsKeyring |= l.op == QLatin1String("spawn") && l.callClass == DoorInfo::KeyringShape;
	}
	QVERIFY(lookHead);
	QVERIFY(bark);
	QVERIFY(blank);
	QVERIFY(spawnsKeyring);
	bool calledByKey = false;
	for (const auto &c : Ucode::callers(u, DoorInfo::KeyringShape))
		calledByKey |= c.first == DoorInfo::KeyShape;
	QVERIFY(calledByKey);

	UsecodeView view;
	view.setWorld(&w);
	view.showClass(DoorInfo::KeyShape, Ucode::EventUse);
	QCOMPARE(view.currentClass(), int(DoorInfo::KeyShape));
	QVERIFY(view.listing()->toPlainText().contains(QLatin1String("event 1: use")));
	auto *items = view.findChildren<QTreeWidget *>().value(1);
	QVERIFY(items);
	QVERIFY(items->topLevelItemCount() > 0);
}

// Blocks end at branches and returns and start at their targets; every block but the first is
// reached from another; LOCKDOOR's area event reads flags (red arrows in) and the graph draws
// them. With U8EDIT_GRAPH_PNG set, the graph is saved there as an image.
void Tests::usecodeGraph()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	const Usecode &u = w.usecode();
	int routines = 0;
	for (int k = 0; k < u.classCount(); k++)
	{
		if (!u.hasClass(k))
			continue;
		QVector<Ucode::Line> lines;
		QVERIFY(Ucode::disassemble(u, k, &lines, &error));
		for (int h = 0; h < lines.size(); h++)
		{
			if (lines[h].kind != Ucode::Line::Head)
				continue;
			const QVector<Ucode::Block> blocks = Ucode::flowGraph(lines, h);
			routines++;
			QVector<int> reached(blocks.size(), 0);
			for (int b = 0; b < blocks.size(); b++)
			{
				QVERIFY(blocks[b].first <= blocks[b].last);
				for (int x = blocks[b].first; x < blocks[b].last; x++)
				{
					const int op = lines[x].opcode;
					QVERIFY2(op != 0x51 && op != 0x52 && op != 0x50, qPrintable(QStringLiteral("%1:%2").arg(k).arg(lines[x].at, 0, 16)));
				}
				for (const Ucode::Edge &e : blocks[b].out)
				{
					QVERIFY(e.to >= 0 && e.to < blocks.size());
					reached[e.to]++;
				}
			}
			for (int b = 1; b < blocks.size(); b++)
				if (!reached[b])
				{
					// Code after a return that nothing jumps to (dead code) has no way in.
					const int prev = lines[blocks[b - 1].last].opcode;
					QVERIFY2(prev == 0x50 || prev == 0x52, qPrintable(QStringLiteral("class %1 block %2").arg(k).arg(blocks[b].offset, 0, 16)));
				}
		}
	}
	QVERIFY(routines > 1000);

	const int lockdoor = UsecodeEggClassBase + 59;
	QCOMPARE(u.className(lockdoor), QStringLiteral("LOCKDOOR"));
	QVector<Ucode::Line> lines;
	QVERIFY(Ucode::disassemble(u, lockdoor, &lines, &error));
	int head = -1;
	for (int h = 0; h < lines.size(); h++)
		if (lines[h].kind == Ucode::Line::Head && lines[h].event == 15)
			head = h;
	QVERIFY(head >= 0);
	UsecodeGraph graph;
	graph.resize(1200, 900);
	graph.showRoutine(&w, lockdoor, lines, head);
	QVERIFY(graph.blockCount() > 3);
	QVERIFY(graph.flagArrows(false) > 0);
	// TELPFLAG (1215) sets the places the recall item knows: green arrows out.
	QCOMPARE(u.className(1215), QStringLiteral("TELPFLAG"));
	QVector<Ucode::Line> telp;
	QVERIFY(Ucode::disassemble(u, 1215, &telp, &error));
	int writes = 0;
	for (int h = 0; h < telp.size(); h++)
		if (telp[h].kind == Ucode::Line::Head)
		{
			UsecodeGraph g;
			g.showRoutine(&w, 1215, telp, h);
			writes += g.flagArrows(true);
		}
	QVERIFY(writes > 0);
	const QString png = qEnvironmentVariable("U8EDIT_GRAPH_PNG");
	if (!png.isEmpty())
	{
		const QRectF r = graph.scene()->sceneRect();
		QImage img((r.size() * 0.7).toSize(), QImage::Format_ARGB32_Premultiplied);
		img.fill(QColor(14, 16, 20));
		QPainter p(&img);
		p.setRenderHint(QPainter::Antialiasing);
		p.setRenderHint(QPainter::TextAntialiasing);
		graph.scene()->render(&p, QRectF(QPointF(0, 0), img.size()), r);
		p.end();
		img.save(png);
	}
}

// A search of every map by a word: how long it takes, and the kind buttons leave items out.
void Tests::searchEveryMap()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	SearchPanel panel;
	panel.setWorld(&w);
	for (const char *query : {"door", "mushroom", "is:egg"})
	{
		QElapsedTimer t;
		t.start();
		panel.run(QString::fromLatin1(query), true);
		QTRY_VERIFY_WITH_TIMEOUT(!panel.searching(), 300000);
		qInfo("search \"%s\", every map: %d found in %lld ms", query, panel.found(), t.elapsed());
		QVERIFY(panel.found() > 0);
	}

	// Leaving the glob items out finds fewer: the glob eggs' floors are on most maps.
	panel.setKinds(SearchPanel::AllKinds);
	panel.run(QStringLiteral("floor"), true);
	QTRY_VERIFY_WITH_TIMEOUT(!panel.searching(), 300000);
	const int all = panel.found();
	QElapsedTimer t;
	t.start();
	panel.setKinds(SearchPanel::AllKinds & ~SearchPanel::Globs);
	QTRY_VERIFY_WITH_TIMEOUT(!panel.searching(), 300000);
	qInfo("search \"floor\": %d found, %d without globs (%lld ms)", all, panel.found(), t.elapsed());
	QVERIFY(panel.found() < all);
	panel.setKinds(SearchPanel::AllKinds);
	QTRY_VERIFY_WITH_TIMEOUT(!panel.searching(), 300000);
	QCOMPARE(panel.found(), all);
}

// The web export of Tenebrae: drawing its items.bin in order, from the atlas, gives the map view's
// picture pixel for pixel; every reference resolves; a second export gives the same bytes.
void Tests::webExport()
{
	World w;
	QString error;
	QVERIFY2(w.load(data_, QString(), &error), qPrintable(error));
	const QString a = temp_.filePath("web-a"), b = temp_.filePath("web-b");
	WebExport::Options options;
	options.maps = {3};
	QString text;
	QTextStream report(&text);
	QVERIFY2(WebExport::write(w, a, options, report, &error), qPrintable(error));
	QVERIFY2(WebExport::write(w, b, options, report, &error), qPrintable(error));
	for (const QString &f : {"manifest.json", "frames.bin", "shapes.json", "graph.json", "atlas-0.png",
			"maps/3/items.bin", "maps/3/info.json"})
	{
		QFile fa(QDir(a).filePath(f)), fb(QDir(b).filePath(f));
		QVERIFY2(fa.open(QIODevice::ReadOnly) && fb.open(QIODevice::ReadOnly), qPrintable(f));
		QVERIFY2(fa.readAll() == fb.readAll(), qPrintable(f));
	}

	auto readAll = [&](const QString &f) {
		QFile file(QDir(a).filePath(f));
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	};
	const QJsonObject manifest = QJsonDocument::fromJson(readAll("manifest.json")).object();
	QCOMPARE(manifest["format"].toString(), QStringLiteral("u8web"));
	const QJsonObject entry = manifest["maps"].toArray().first().toObject();
	const QJsonObject bounds = entry["bounds"].toObject();
	std::vector<QImage> atlas;
	for (const QJsonValue &v : manifest["atlas"].toArray())
		atlas.push_back(QImage(QDir(a).filePath(v.toObject()["file"].toString())));
	const QByteArray frames = readAll("frames.bin"), items = readAll("maps/3/items.bin");
	const QJsonArray info = QJsonDocument::fromJson(readAll("maps/3/info.json")).array();
	auto u16 = [](const QByteArray &d, int at) { return int(uint8_t(d[at]) | uint8_t(d[at + 1]) << 8); };
	auto i16 = [&](const QByteArray &d, int at) { return int(int16_t(u16(d, at))); };
	auto i32 = [&](const QByteArray &d, int at) { return int(uint32_t(u16(d, at)) | uint32_t(u16(d, at + 2)) << 16); };

	MapRender r;
	r.build(w, 3);
	const LayerOptions o;
	int drawn = 0;
	for (const MapRender::Entry &e : r.entries())
		drawn += r.shows(e, o);
	QCOMPARE(entry["drawn"].toInt(), drawn);
	QCOMPARE(items.size(), 16 * entry["items"].toInt());
	const QImage want = r.renderOverview(o, 1);
	QCOMPARE(want.width(), bounds["maxX"].toInt() - bounds["minX"].toInt());
	QCOMPARE(want.height(), bounds["maxY"].toInt() - bounds["minY"].toInt());

	QImage got(want.size(), QImage::Format_ARGB32_Premultiplied);
	got.fill(Qt::black);
	QPainter p(&got);
	p.translate(-bounds["minX"].toInt(), -bounds["minY"].toInt());
	for (int i = 0; i < items.size() / 16; i++)
	{
		const int at = i * 16, id = u16(items, at + 8), flags = uint8_t(items[at + 11]);
		const uint32_t index = uint32_t(i32(items, at + 12));
		QVERIFY(id < frames.size() / 16);
		QCOMPARE(index != 0xffffffffu, bool(flags & 0x10));
		QVERIFY(index == 0xffffffffu || int(index) < info.size());
		if (flags & 0x08)
			continue;
		const int f = id * 16;
		const QImage tile = atlas[size_t(uint8_t(frames[f]))].copy(u16(frames, f + 2), u16(frames, f + 4),
			u16(frames, f + 6), u16(frames, f + 8));
		p.setOpacity(flags & 0x20 ? 0.6 : 1);
		p.drawImage(QPoint(i32(items, at) + i16(frames, f + 10), i32(items, at + 4) + i16(frames, f + 12)), tile);
	}
	p.end();
	QVERIFY(got == want.convertToFormat(QImage::Format_ARGB32_Premultiplied));

	const QJsonObject graph = QJsonDocument::fromJson(readAll("graph.json")).object();
	QCOMPARE(graph["transitions"].toArray().size(), w.transitions().size());
	for (const QJsonValue &v : graph["transitions"].toArray())
		for (const char *side : {"fromItem", "toItem"})
		{
			const QJsonValue idx = v.toObject()[QLatin1String(side)];
			QVERIFY(idx.isNull() || idx.toInt() < info.size());
		}
}

QTEST_MAIN(Tests)
#include "tests.moc"
