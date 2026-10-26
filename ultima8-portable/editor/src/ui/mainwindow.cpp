#include "ui/mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextStream>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>

#include "ui/graphview.h"
#include "ui/itempanel.h"
#include "ui/mapview.h"
#include "ui/minimap.h"

enum { MapColumn, NameColumn, ItemsColumn, ExitsColumn };

QString MainWindow::portHome()
{
	const QString env = qEnvironmentVariable("U8_HOME");
	if (!env.isEmpty())
		return env;
#ifdef Q_OS_WIN
	return QDir(qEnvironmentVariable("APPDATA")).filePath("u8port");
#else
	return QDir::home().filePath(".u8port");
#endif
}

// data= in u8port.cfg, the folder the port last ran the game from.
QString MainWindow::portDataDir()
{
	QFile f(QDir(portHome()).filePath("u8port.cfg"));
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
		return QString();
	QTextStream in(&f);
	while (!in.atEnd())
	{
		const QString line = in.readLine();
		if (line.startsWith(QLatin1String("data=")))
			return line.mid(5).trimmed();
	}
	return QString();
}

MainWindow::MainWindow()
{
	setWindowTitle(tr("U8 Editor"));
	view_ = new MapView;
	graph_ = new TransitionGraph;
	tabs_ = new QTabWidget;
	tabs_->addTab(view_, tr("Map"));
	tabs_->addTab(graph_, tr("Transitions"));
	tabs_->setDocumentMode(true);
	setCentralWidget(tabs_);

	panel_ = new ItemPanel;
	details_ = new QDialog(this, Qt::Tool);
	details_->setWindowTitle(tr("Item"));
	auto *dl = new QVBoxLayout(details_);
	dl->setContentsMargins(4, 4, 4, 4);
	dl->addWidget(panel_);
	details_->resize(420, 560);

	status_ = new QLabel;
	zoomLabel_ = new QLabel;
	statusBar()->addWidget(status_, 1);
	statusBar()->addPermanentWidget(zoomLabel_);

	buildDocks();
	buildMenus();

	connect(view_, &MapView::itemClicked, this, [this](int item) {
		if (item >= 0)
			showDetails(view_->mapNumber(), item);
	});
	connect(view_, &MapView::status, status_, &QLabel::setText);
	connect(view_, &MapView::viewChanged, this, [this] {
		zoomLabel_->setText(tr("Zoom %1%").arg(qRound(view_->zoom() * 100)));
	});
	connect(panel_, &ItemPanel::goToItem, this, [this](int map, int item) { goToItem(map, item, true); });
	connect(graph_, &TransitionGraph::openMap, this, [this](int map) {
		openMap(map);
		tabs_->setCurrentWidget(view_);
	});
	connect(graph_, &TransitionGraph::showTransition, this, [this](int index) {
		const Transition &t = world_->transitions()[index];
		if (t.fromMap >= 0 && t.fromItem >= 0)
			goToItem(t.fromMap, t.fromItem, true);
		else if (t.toItem >= 0)
			goToItem(t.toMap, t.toItem, true);
		else
			openMap(t.toMap);
		tabs_->setCurrentWidget(view_);
	});

	QSettings s;
	restoreGeometry(s.value("window/geometry").toByteArray());
	restoreState(s.value("window/state").toByteArray());
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *e)
{
	QSettings s;
	s.setValue("window/geometry", saveGeometry());
	s.setValue("window/state", saveState());
	e->accept();
}

void MainWindow::buildDocks()
{
	maps_ = new QTreeWidget;
	maps_->setColumnCount(4);
	maps_->setHeaderLabels({tr("Map"), tr("Name"), tr("Items"), tr("Exits")});
	maps_->setRootIsDecorated(false);
	maps_->setAlternatingRowColors(true);
	maps_->setSortingEnabled(false);
	maps_->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
	maps_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
	maps_->setToolTip(tr("Double-click a name, or press F2, to name the map"));
	connect(maps_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *it) {
		if (it && !fillingList_)
			openMap(it->data(MapColumn, Qt::UserRole).toInt());
	});
	connect(maps_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *it, int column) {
		if (fillingList_ || column != NameColumn)
			return;
		const int map = it->data(MapColumn, Qt::UserRole).toInt();
		const QString name = it->text(NameColumn).trimmed();
		QSettings s;
		if (name.isEmpty())
			s.remove(QStringLiteral("mapNames/%1").arg(map));
		else
			s.setValue(QStringLiteral("mapNames/%1").arg(map), name);
		graph_->refreshNames();
		updateTitle();
	});
	auto *mapsDock = new QDockWidget(tr("Maps"), this);
	mapsDock->setObjectName("maps");
	mapsDock->setWidget(maps_);
	addDockWidget(Qt::LeftDockWidgetArea, mapsDock);

	minimap_ = new Minimap(view_);
	auto *miniDock = new QDockWidget(tr("Minimap"), this);
	miniDock->setObjectName("minimap");
	miniDock->setWidget(minimap_);
	addDockWidget(Qt::LeftDockWidgetArea, miniDock);
}

// A menu entry; QMenu::addAction's own overloads for this changed between Qt 6 versions.
static QAction *entry(QMenu *menu, const QString &text, QObject *context, std::function<void()> fn,
	const QKeySequence &key = QKeySequence())
{
	QAction *a = menu->addAction(text);
	a->setShortcut(key);
	QObject::connect(a, &QAction::triggered, context, [fn] { fn(); });
	return a;
}

void MainWindow::buildMenus()
{
	QMenu *file = menuBar()->addMenu(tr("&File"));
	entry(file, tr("Open &Game Folder..."), this, [this] { chooseDataDir(); }, QKeySequence::Open);
	entry(file, tr("Open &Saved Game..."), this, [this] { chooseSave(); });
	entry(file, tr("Use the Game's &Starting State"), this, [this] {
		QString error;
		if (!loadWorld(world_ ? world_->dataDir() : QString(), QString(), &error))
			QMessageBox::warning(this, tr("U8 Editor"), error);
	});
	file->addSeparator();
	entry(file, tr("&Export View as Image..."), this, [this] { exportView(); });
	file->addSeparator();
	entry(file, tr("&Quit"), this, [this] { close(); }, QKeySequence::Quit);

	QToolBar *bar = addToolBar(tr("Layers"));
	bar->setObjectName("layers");
	QMenu *layers = menuBar()->addMenu(tr("&Layers"));
	auto toggle = [&](const QString &text, bool on, const QString &tip) {
		QAction *a = new QAction(text, this);
		a->setCheckable(true);
		a->setChecked(on);
		a->setToolTip(tip);
		connect(a, &QAction::toggled, this, &MainWindow::applyOptions);
		layers->addAction(a);
		bar->addAction(a);
		return a;
	};
	const LayerOptions d;
	fixed_ = toggle(tr("Fixed"), d.fixed, tr("Items from FIXED.DAT: the land, walls and buildings"));
	globs_ = toggle(tr("Globs"), d.globs, tr("Items that glob eggs stand for (GLOB.FLX): floors and scenery"));
	nonFixed_ = toggle(tr("Movable"), d.nonFixed, tr("Items from the saved game's NONFIXED.DAT"));
	npcs_ = toggle(tr("NPCs"), d.npcs, tr("Characters and what they carry"));
	bar->addSeparator();
	layers->addSeparator();
	editor_ = toggle(tr("Eggs"), true, tr("Eggs and the other shapes only the game's editor shows"));
	ranges_ = toggle(tr("Egg Ranges"), d.eggRanges, tr("Where each egg hatches"));
	labels_ = toggle(tr("Labels"), d.labels, tr("What each egg does, when zoomed in"));
	invisible_ = toggle(tr("Invisible"), d.invisible, tr("Items with the invisible flag, drawn faintly"));
	bar->addSeparator();
	bar->addWidget(new QLabel(tr(" Roof below ")));
	roof_ = new QSpinBox;
	roof_->setRange(0, 255);
	roof_->setValue(255);
	roof_->setSpecialValueText(tr("none"));
	roof_->setToolTip(tr("Hide what is at or above this height, as the game does inside a building.\n"
		"255 shows everything; try 40 or so inside houses."));
	roof_->setWrapping(false);
	connect(roof_, &QSpinBox::valueChanged, this, &MainWindow::applyOptions);
	bar->addWidget(roof_);

	QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
	entry(viewMenu, tr("Zoom &In"), this, [this] { view_->zoomBy(1.25); }, QKeySequence::ZoomIn);
	entry(viewMenu, tr("Zoom &Out"), this, [this] { view_->zoomBy(0.8); }, QKeySequence::ZoomOut);
	entry(viewMenu, tr("&Actual Size"), this, [this] { view_->setZoom(1); }, QKeySequence(Qt::CTRL | Qt::Key_1));
	entry(viewMenu, tr("&Whole Map"), this, [this] { view_->fitMap(); }, QKeySequence(Qt::CTRL | Qt::Key_0));
	viewMenu->addSeparator();
	entry(viewMenu, tr("Item &Details"), this, [this] {
		details_->show();
		details_->raise();
	}, QKeySequence(Qt::CTRL | Qt::Key_I));
	entry(viewMenu, tr("&Transitions"), this, [this] { tabs_->setCurrentWidget(graph_); },
		QKeySequence(Qt::CTRL | Qt::Key_T));
	for (QDockWidget *d : findChildren<QDockWidget *>())
		viewMenu->addAction(d->toggleViewAction());

	QMenu *help = menuBar()->addMenu(tr("&Help"));
	entry(help, tr("&About"), this, [this] {
		QMessageBox::about(this, tr("U8 Editor"),
			tr("<p><b>U8 Editor</b>: a viewer for Ultima VIII's world, the start of an editor.</p>"
				"<p>Drag to move, wheel to zoom, click an item for its details. Arrow keys move, "
				"+ and - zoom, 1 shows actual size and 0 the whole map.</p>"
				"<p>The formats are read as the game reads them; see the decompiled sources in game/item "
				"and game/u8.</p>"));
	});
	applyOptions();
}

void MainWindow::applyOptions()
{
	LayerOptions o;
	o.fixed = fixed_->isChecked();
	o.nonFixed = nonFixed_->isChecked();
	o.globs = globs_->isChecked();
	o.npcs = npcs_->isChecked();
	o.editorItems = editor_->isChecked();
	o.eggRanges = ranges_->isChecked();
	o.labels = labels_->isChecked();
	o.invisible = invisible_->isChecked();
	o.roof = roof_->value() == 0 ? 255 : roof_->value();
	ranges_->setEnabled(o.editorItems);
	labels_->setEnabled(o.editorItems);
	view_->setOptions(o);
}

bool MainWindow::loadWorld(const QString &dataDir, const QString &savePath, QString *error)
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
	auto w = std::make_unique<World>();
	bool ok = w->load(dataDir, savePath, error);
	QApplication::restoreOverrideCursor();
	if (!ok)
		return false;
	const int previous = view_->mapNumber();
	view_->clear();
	panel_->showItem(nullptr, -1, -1);
	world_ = std::move(w);
	QSettings().setValue("dataDir", dataDir);
	fillMapList();
	graph_->setWorld(world_.get());
	if (!world_->notes().isEmpty())
		statusBar()->showMessage(world_->notes().trimmed(), 15000);
	const QVector<int> maps = world_->mapNumbers();
	int start = maps.contains(previous) ? previous : QSettings().value("lastMap", -1).toInt();
	if (!maps.contains(start) && !maps.isEmpty())
		start = maps.contains(3) ? 3 : maps.first();
	if (start >= 0)
		openMap(start);
	updateTitle();
	return true;
}

void MainWindow::fillMapList()
{
	fillingList_ = true;
	maps_->clear();
	QSettings s;
	QHash<int, int> exits;
	for (const Transition &t : world_->transitions())
		if (t.kind == Transition::TeleportEgg && t.fromMap != t.toMap)
			exits[t.fromMap]++;
	for (int m : world_->mapNumbers())
	{
		const MapData &d = world_->map(m);
		auto *it = new QTreeWidgetItem(maps_);
		it->setData(MapColumn, Qt::UserRole, m);
		it->setText(MapColumn, QString::number(m));
		it->setText(NameColumn, s.value(QStringLiteral("mapNames/%1").arg(m)).toString());
		it->setText(ItemsColumn, QString::number(d.items.size()));
		it->setText(ExitsColumn, exits.value(m) ? QString::number(exits.value(m)) : QString());
		it->setTextAlignment(MapColumn, Qt::AlignRight | Qt::AlignVCenter);
		it->setTextAlignment(ItemsColumn, Qt::AlignRight | Qt::AlignVCenter);
		it->setTextAlignment(ExitsColumn, Qt::AlignRight | Qt::AlignVCenter);
		it->setToolTip(MapColumn, tr("%1 fixed, %2 movable, %3 from globs, %4 NPCs")
			.arg(d.fixedCount).arg(d.nonFixedCount).arg(d.globCount).arg(d.npcCount));
		it->setFlags(it->flags() | Qt::ItemIsEditable);
	}
	for (int c : {MapColumn, ItemsColumn, ExitsColumn})
		maps_->resizeColumnToContents(c);
	fillingList_ = false;
}

bool MainWindow::openMap(int map)
{
	if (!world_ || world_->map(map).empty())
		return false;
	if (view_->mapNumber() != map || !view_->render().world())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		view_->openMap(*world_, map);
		QApplication::restoreOverrideCursor();
		QSettings().setValue("lastMap", map);
	}
	fillingList_ = true;
	for (int i = 0; i < maps_->topLevelItemCount(); i++)
		if (maps_->topLevelItem(i)->data(MapColumn, Qt::UserRole).toInt() == map)
			maps_->setCurrentItem(maps_->topLevelItem(i));
	fillingList_ = false;
	graph_->setCurrentMap(map);
	updateTitle();
	return true;
}

// Shows the item on its map; a contained item shows the item it is (at last) inside of.
void MainWindow::goToItem(int map, int item, bool details)
{
	if (!world_ || !openMap(map))
		return;
	const MapData &m = world_->map(map);
	if (item < 0 || item >= int(m.items.size()))
		return;
	int onMap = item;
	while (onMap >= 0 && !m.items[onMap].onMap())
		onMap = m.items[onMap].parent;
	tabs_->setCurrentWidget(view_);
	if (onMap >= 0)
		view_->showItem(onMap, true);
	if (details)
		showDetails(map, item);
}

void MainWindow::showDetails(int map, int item)
{
	panel_->showItem(world_.get(), map, item);
	if (!details_->isVisible())
	{
		// First time: beside the window's right edge, over the map.
		QPoint at = mapToGlobal(QPoint(width() - details_->width() - 24, 80));
		details_->move(at);
		details_->show();
	}
	details_->raise();
}

void MainWindow::chooseDataDir()
{
	QString start = world_ ? world_->dataDir() : QSettings().value("dataDir").toString();
	QString dir = QFileDialog::getExistingDirectory(this, tr("The Ultima 8 folder (with STATIC and USECODE)"), start);
	if (dir.isEmpty())
		return;
	QString error;
	if (!loadWorld(dir, QString(), &error))
		QMessageBox::warning(this, tr("U8 Editor"), error);
}

void MainWindow::chooseSave()
{
	if (!world_)
		return chooseDataDir();
	QString start = QDir(portHome()).filePath("SAVEGAME");
	if (!QDir(start).exists())
		start = QDir(world_->dataDir()).filePath("SAVEGAME");
	QString path = QFileDialog::getOpenFileName(this, tr("A saved game"), start, tr("Saved games (U8SAVE.* u8save.*);;All files (*)"));
	if (path.isEmpty())
		return;
	QString error;
	if (!loadWorld(world_->dataDir(), path, &error))
		QMessageBox::warning(this, tr("U8 Editor"), error);
}

void MainWindow::exportView()
{
	QString path = QFileDialog::getSaveFileName(this, tr("Export View"),
		QStringLiteral("map%1.png").arg(view_->mapNumber()), tr("PNG images (*.png)"));
	if (path.isEmpty())
		return;
	QImage img = tabs_->currentWidget() == graph_ ? graph_->grab().toImage() : view_->grabView();
	if (!img.save(path))
		QMessageBox::warning(this, tr("U8 Editor"), tr("Could not write %1").arg(path));
}

void MainWindow::updateTitle()
{
	QString title = tr("U8 Editor");
	if (world_ && view_->mapNumber() >= 0)
		title = QStringLiteral("%1 — %2").arg(TransitionGraph::mapLabel(view_->mapNumber()), title);
	if (world_ && !world_->saveSource().isEmpty())
		title += QStringLiteral(" [%1]").arg(QFileInfo(world_->saveSource()).fileName());
	setWindowTitle(title);
}
