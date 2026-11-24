// u8edit.
//
//   u8edit [--data DIR] [--save FILE] [--map N]
//   u8edit [--data DIR] [--save FILE] --export-web OUT [--maps 1,3,...]
//
// An editor for Ultima VIII's world and game data.
//
// With no --data it uses U8_DATA, then the folder it last used, then the port's (data= in
// u8port.cfg), then asks. The switches after that are for testing and for pictures: they set up
// a view, save it, and quit. --export-web writes the maps for the Ultima Cartographer
// (ui/webexport.h) without a window, prints a size report, and quits.

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>

#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

#include "ui/graphview.h"
#include "ui/itempanel.h"
#include "ui/mainwindow.h"
#include "ui/mapview.h"
#include "ui/usecodeview.h"
#include "ui/webexport.h"

static void darkPalette(QApplication &app)
{
	app.setStyle(QStyleFactory::create("Fusion"));
	QPalette p;
	p.setColor(QPalette::Window, QColor(37, 37, 42));
	p.setColor(QPalette::WindowText, QColor(225, 225, 225));
	p.setColor(QPalette::Base, QColor(26, 26, 30));
	p.setColor(QPalette::AlternateBase, QColor(33, 33, 38));
	p.setColor(QPalette::ToolTipBase, QColor(45, 45, 52));
	p.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
	p.setColor(QPalette::Text, QColor(225, 225, 225));
	p.setColor(QPalette::Button, QColor(45, 45, 52));
	p.setColor(QPalette::ButtonText, QColor(225, 225, 225));
	p.setColor(QPalette::Highlight, QColor(60, 110, 180));
	p.setColor(QPalette::HighlightedText, Qt::white);
	p.setColor(QPalette::Link, QColor(110, 170, 255));
	p.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
	p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
	p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
	app.setPalette(p);
}

// --export-web: load, write, report, quit; 0 on success.
static int exportWeb(const QString &dir, const QString &save, const QString &out, const QString &mapList)
{
#ifdef _WIN32
	// A windowed program has no console; print to the one it was started from.
	if (!GetStdHandle(STD_OUTPUT_HANDLE) && AttachConsole(ATTACH_PARENT_PROCESS))
	{
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}
#endif
	QTextStream report(stdout);
	QTextStream err(stderr);
	if (dir.isEmpty())
	{
		err << "u8edit: no game folder (give --data)" << Qt::endl;
		return 1;
	}
	World world;
	QString error;
	if (!world.load(dir, save, &error))
	{
		err << "u8edit: " << error << Qt::endl;
		return 1;
	}
	WebExport::Options options;
	for (const QString &m : mapList.split(QLatin1Char(','), Qt::SkipEmptyParts))
	{
		bool ok = false;
		options.maps << m.trimmed().toInt(&ok);
		if (!ok)
		{
			err << "u8edit: --maps wants numbers, as 1,3,5" << Qt::endl;
			return 1;
		}
	}
	// The Transitions view's layout: the places saved in the settings, else its own arrangement.
	{
		TransitionGraph graph;
		graph.setWorld(&world);
		options.graphPositions = graph.positions();
	}
	if (!WebExport::write(world, out, options, report, &error))
	{
		err << "u8edit: " << error << Qt::endl;
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
#ifndef _WIN32
	// The export opens no window, so it can run where there is no display. (On Windows the
	// usual platform plugin is enough, and the offscreen one is often not shipped.)
	for (int i = 1; i < argc; i++)
		if (!strncmp(argv[i], "--export-web", 12) && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") &&
			qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
			qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
	QApplication app(argc, argv);
	QApplication::setOrganizationName("u8port");
	QApplication::setApplicationName("u8edit");
	darkPalette(app);

	QCommandLineParser cl;
	cl.setApplicationDescription("An editor for Ultima VIII's world and game data");
	cl.addHelpOption();
	QCommandLineOption data("data", "The game folder, with STATIC and USECODE.", "dir");
	QCommandLineOption save("save", "A saved game (U8SAVE.nnn) or a GAMEDAT folder to take movable items from.", "file");
	QCommandLineOption map("map", "The map to open.", "n");
	QCommandLineOption at("at", "Centre the view on world x,y,z.", "x,y,z");
	QCommandLineOption zoom("zoom", "The view's zoom (1 is actual size).", "z");
	QCommandLineOption pick("select", "Select the item with this index on the map and show its details.", "item");
	QCommandLineOption roof("roof", "Hide what is at or above this height.", "z");
	QCommandLineOption size("size", "The window's size.", "wxh");
	QCommandLineOption shot("shot", "Save the window to this PNG, then quit.", "file");
	QCommandLineOption graph("graph", "Show the transitions tab.");
	QCommandLineOption tab("tab", "Show a tab: map, transitions, data, flags or usecode.", "name");
	QCommandLineOption usecodeClass("class", "Show this usecode class in the Usecode tab.", "n");
	QCommandLineOption codeGraph("code-graph", "With --class: show it as a graph, at this event.", "event");
	QCommandLineOption noEggs("no-eggs", "Start with eggs hidden.");
	QCommandLineOption exportWebDir("export-web", "Write the maps for the Ultima Cartographer to this folder, then quit.", "dir");
	QCommandLineOption exportMaps("maps", "With --export-web: the maps to write (default: every map with items).", "1,3,...");
	cl.addOptions({data, save, map, at, zoom, pick, roof, size, shot, graph, tab, usecodeClass, codeGraph, noEggs,
		exportWebDir, exportMaps});
	cl.process(app);

	QString dir = cl.value(data);
	if (dir.isEmpty())
		dir = qEnvironmentVariable("U8_DATA");
	if (dir.isEmpty())
		dir = QSettings().value("dataDir").toString();
	if (dir.isEmpty())
		dir = MainWindow::portDataDir();

	if (cl.isSet(exportWebDir))
		return exportWeb(dir, cl.value(save), cl.value(exportWebDir), cl.value(exportMaps));

	MainWindow window;
	if (cl.isSet(size))
	{
		const QStringList wh = cl.value(size).split('x');
		if (wh.size() == 2)
			window.resize(wh[0].toInt(), wh[1].toInt());
	}
	else if (!QSettings().contains("window/geometry"))
		window.resize(1400, 900);
	window.show();

	QString error;
	while (!window.world())
	{
		if (!dir.isEmpty() && window.loadWorld(dir, cl.value(save), &error))
			break;
		if (!dir.isEmpty())
			QMessageBox::warning(&window, "U8 Editor", error);
		if (cl.isSet(shot))
			return 1;
		dir = QFileDialog::getExistingDirectory(&window, "The Ultima 8 folder (with STATIC and USECODE)");
		if (dir.isEmpty())
			break;
	}
	if (!window.world())
		return 0;

	if (cl.isSet(noEggs))
	{
		for (QAction *a : window.findChildren<QAction *>())
			if (a->text() == QObject::tr("Eggs"))
				a->setChecked(false);
	}
	if (cl.isSet(roof))
		window.setRoof(cl.value(roof).toInt());
	if (cl.isSet(map))
		window.openMap(cl.value(map).toInt());
	if (cl.isSet(graph))
		window.tabs()->setCurrentWidget(window.graph());
	if (cl.isSet(tab))
	{
		const QString want = cl.value(tab).toLower();
		for (int i = 0; i < window.tabs()->count(); i++)
			if (window.tabs()->tabText(i).toLower().startsWith(want == QLatin1String("data") ? QStringLiteral("game") : want))
				window.tabs()->setCurrentIndex(i);
	}
	if (cl.isSet(usecodeClass))
	{
		window.tabs()->setCurrentWidget(window.usecodeView());
		window.usecodeView()->showClass(cl.value(usecodeClass).toInt(0, 0));
		if (cl.isSet(codeGraph))
		{
			window.usecodeView()->setGraphMode(true);
			window.usecodeView()->showClass(cl.value(usecodeClass).toInt(0, 0), cl.value(codeGraph).toInt());
		}
	}

	// Positioning waits until the window has its size.
	QTimer::singleShot(0, &window, [&] {
		MapView *view = window.mapView();
		if (cl.isSet(zoom))
			view->setZoom(cl.value(zoom).toDouble());
		if (cl.isSet(at))
		{
			const QStringList xyz = cl.value(at).split(',');
			if (xyz.size() == 3)
				view->centerOn(MapRender::toScreen(xyz[0].toInt(), xyz[1].toInt(), xyz[2].toInt()));
		}
		if (cl.isSet(pick))
		{
			window.goToItem(view->mapNumber(), cl.value(pick).toInt(), true);
			if (cl.isSet(zoom))
				view->setZoom(cl.value(zoom).toDouble());
		}
		if (cl.isSet(shot))
		{
			QTimer::singleShot(300, &window, [&] {
				window.grab().save(cl.value(shot));
				QWidget *details = window.itemPanel()->window();
				if (details != &window && details->isVisible())
				{
					QString path = cl.value(shot);
					path.insert(path.lastIndexOf('.'), "-item");
					details->grab().save(path);
				}
				QApplication::quit();
			});
		}
	});
	return app.exec();
}
