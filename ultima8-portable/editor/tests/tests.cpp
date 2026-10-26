// u8edit-tests: the editor against a copy of the game's data (U8_DATA).
//
//   U8_DATA=~/games/ultima8 QT_QPA_PLATFORM=offscreen build/editor/u8edit-tests
//
// The files are read and written back unchanged, then the main window is driven as a user would:
// select, drag, undo, place, delete, change a weapon, save; and what was saved is read again.

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

#include "data/world.h"
#include "ui/edits.h"
#include "ui/gamedata.h"
#include "ui/mainwindow.h"
#include "ui/mapview.h"

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
	void editInWindow();

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
	QTest::mouseMove(view, at + QPoint(20, 10));
	QTest::mouseMove(view, at + QPoint(40, 20));
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

QTEST_MAIN(Tests)
#include "tests.moc"
