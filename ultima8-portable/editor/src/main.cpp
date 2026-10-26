// u8edit.
//
//   u8edit [--data DIR] [--save FILE] [--map N]
//
// An editor for Ultima VIII's world and game data.
//
// With no --data it uses U8_DATA, then the folder it last used, then the port's (data= in
// u8port.cfg), then asks. The switches after that are for testing and for pictures: they set up
// a view, save it, and quit.

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTimer>

#include "ui/graphview.h"
#include "ui/itempanel.h"
#include "ui/mainwindow.h"
#include "ui/mapview.h"

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

int main(int argc, char **argv)
{
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
	QCommandLineOption tab("tab", "Show a tab: map, transitions or data.", "name");
	QCommandLineOption noEggs("no-eggs", "Start with eggs hidden.");
	cl.addOptions({data, save, map, at, zoom, pick, roof, size, shot, graph, tab, noEggs});
	cl.process(app);

	QString dir = cl.value(data);
	if (dir.isEmpty())
		dir = qEnvironmentVariable("U8_DATA");
	if (dir.isEmpty())
		dir = QSettings().value("dataDir").toString();
	if (dir.isEmpty())
		dir = MainWindow::portDataDir();

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
		window.tabs()->setCurrentIndex(QStringList{"map", "transitions", "data"}.indexOf(cl.value(tab).toLower()));

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
