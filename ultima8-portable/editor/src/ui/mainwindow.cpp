#include "ui/mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
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

#include "ui/edits.h"
#include "ui/gamedata.h"
#include "ui/graphview.h"
#include "ui/itempanel.h"
#include "ui/mapview.h"
#include "ui/minimap.h"
#include "ui/shapebrowser.h"

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
	editor_ = new Editor(this);
	view_ = new MapView;
	graph_ = new TransitionGraph;
	gameData_ = new GameDataView(editor_);
	tabs_ = new QTabWidget;
	tabs_->addTab(view_, tr("Map"));
	tabs_->addTab(graph_, tr("Transitions"));
	tabs_->addTab(gameData_, tr("Game Data"));
	tabs_->setDocumentMode(true);
	setCentralWidget(tabs_);

	panel_ = new ItemPanel;
	details_ = new QDialog(this, Qt::Tool);
	details_->setWindowTitle(tr("Item"));
	auto *dl = new QVBoxLayout(details_);
	dl->setContentsMargins(4, 4, 4, 4);
	dl->addWidget(panel_);
	details_->resize(440, 720);

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
	connect(view_, &MapView::moveRequested, this, [this](int item, int dx, int dy, int dz) {
		editor_->moveItem(view_->mapNumber(), item, dx, dy, dz);
	});
	connect(view_, &MapView::placeRequested, this, [this](const MapItem &it) {
		const int added = editor_->addItem(view_->mapNumber(), it, -1,
			tr("Place %1").arg(world_->describe(it)));
		if (added >= 0)
		{
			view_->select(added);
			panel_->showItem(world_.get(), view_->mapNumber(), added);
		}
	});
	connect(view_, &MapView::actionRequested, this, &MainWindow::editAction);
	connect(shapes_, &ShapeBrowser::placeRequested, this, [this](int shape, int frame) {
		if (view_->mapNumber() < 0)
			return;
		tabs_->setCurrentWidget(view_);
		view_->startPlacing(shape, frame);
	});
	connect(panel_, &ItemPanel::goToItem, this, [this](int map, int item) { goToItem(map, item, true); });
	connect(panel_, &ItemPanel::editItem, this, [this](int map, int item, const MapItem &f, const QString &what) {
		editor_->editItem(map, item, f, what);
	});
	connect(panel_, &ItemPanel::editNpc, editor_, &Editor::editNpc);
	connect(panel_, &ItemPanel::deleteItem, this, [this](int map, int item) {
		if (editor_->deleteItem(map, item))
			panel_->showItem(world_.get(), map, -1);
	});
	connect(panel_, &ItemPanel::duplicateItem, this, [this](int map, int item) {
		const int s = std::max(32, view_->snap());
		const int added = editor_->duplicateItem(map, item, s, 0);
		if (added >= 0)
			goToItem(map, added, true);
	});
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
	connect(gameData_, &GameDataView::showNpc, this, [this](int npc) {
		for (int m : world_->mapNumbers())
		{
			const auto &items = world_->map(m).items;
			for (size_t i = 0; i < items.size(); i++)
				if (!items[i].deleted && items[i].source == MapItem::Npc && items[i].parent < 0 && items[i].record == npc)
				{
					goToItem(m, int(i), true);
					return;
				}
		}
	});
	connect(tabs_, &QTabWidget::currentChanged, this, [this] {
		if (tabs_->currentWidget() == graph_ && graphStale_)
		{
			graphStale_ = false;
			graph_->setWorld(world_.get());
		}
	});

	connect(editor_, &Editor::mapChanged, this, &MainWindow::mapChanged);
	connect(editor_, &Editor::npcChanged, this, [this] {
		panel_->refresh();
		updateTitle();
	});
	connect(editor_, &Editor::tablesChanged, this, &MainWindow::updateTitle);
	connect(editor_, &Editor::refused, this, [this](const QString &why) {
		statusBar()->showMessage(why, 10000);
	});
	connect(editor_->undo(), &QUndoStack::cleanChanged, this, &MainWindow::updateTitle);

	QSettings s;
	restoreGeometry(s.value("window/geometry").toByteArray());
	restoreState(s.value("window/state").toByteArray());
}

MainWindow::~MainWindow() = default;

bool MainWindow::askToSave(const QString &before)
{
	if (!world_ || !world_->dirty())
		return true;
	const auto answer = QMessageBox::question(this, tr("U8 Editor"),
		tr("Save the changes before %1?\n\nThese files will be written: %2").arg(before, world_->pendingFiles().join(", ")),
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
	if (answer == QMessageBox::Cancel)
		return false;
	if (answer == QMessageBox::Save)
		return save();
	return true;
}

void MainWindow::closeEvent(QCloseEvent *e)
{
	if (!askToSave(tr("closing")))
	{
		e->ignore();
		return;
	}
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

	shapes_ = new ShapeBrowser;
	auto *shapeDock = new QDockWidget(tr("Shapes"), this);
	shapeDock->setObjectName("shapes");
	shapeDock->setWidget(shapes_);
	addDockWidget(Qt::RightDockWidgetArea, shapeDock);
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
	entry(file, tr("Open Saved &Game..."), this, [this] { chooseSave(); });
	entry(file, tr("Open the Game in &Progress (GAMEDAT)"), this, [this] {
		if (!world_ || !askToSave(tr("loading another game")))
			return;
		const QString gamedat = QDir(world_->dataDir()).filePath("GAMEDAT");
		QString error;
		if (!QFile::exists(QDir(gamedat).filePath("NONFIXED.DAT")))
			QMessageBox::information(this, tr("U8 Editor"), tr("The game folder has no game in progress."));
		else if (!loadWorld(world_->dataDir(), gamedat, &error))
			QMessageBox::warning(this, tr("U8 Editor"), error);
	});
	entry(file, tr("Use the Game's Starting State"), this, [this] {
		if (!askToSave(tr("loading another game")))
			return;
		QString error;
		if (!loadWorld(world_ ? world_->dataDir() : QString(), QString(), &error))
			QMessageBox::warning(this, tr("U8 Editor"), error);
	});
	file->addSeparator();
	QAction *saveAction = entry(file, tr("&Save"), this, [this] { save(); }, QKeySequence::Save);
	entry(file, tr("&Test in the Port"), this, [this] { testInPort(); }, QKeySequence(Qt::Key_F5));
	entry(file, tr("Choose the Port &Program..."), this, [this] {
		const QString path = QFileDialog::getOpenFileName(this, tr("The port (Ultima8)"),
			QSettings().value("portProgram").toString());
		if (!path.isEmpty())
			QSettings().setValue("portProgram", path);
	});
	file->addSeparator();
	entry(file, tr("&Export View as Image..."), this, [this] { exportView(); });
	file->addSeparator();
	entry(file, tr("&Quit"), this, [this] { close(); }, QKeySequence::Quit);

	QMenu *edit = menuBar()->addMenu(tr("&Edit"));
	QAction *undo = editor_->undo()->createUndoAction(this, tr("&Undo"));
	undo->setShortcut(QKeySequence::Undo);
	QAction *redo = editor_->undo()->createRedoAction(this, tr("&Redo"));
	redo->setShortcut(QKeySequence::Redo);
	edit->addAction(undo);
	edit->addAction(redo);
	edit->addSeparator();
	entry(edit, tr("Place the Shape"), this, [this] {
		if (shapes_->shape() >= 0 && view_->mapNumber() >= 0)
		{
			tabs_->setCurrentWidget(view_);
			view_->startPlacing(shapes_->shape(), shapes_->frame());
		}
	}, QKeySequence(Qt::Key_P));
	entry(edit, tr("Pick the Selected Item's Shape"), this, [this] {
		if (view_->selected() >= 0)
		{
			const MapItem &it = world_->map(view_->mapNumber()).items[view_->selected()];
			shapes_->showShape(it.shape, it.frame);
		}
	}, QKeySequence(Qt::Key_I));
	edit->addSeparator();
	QMenu *keys = edit->addMenu(tr("On the Map"));
	keys->addAction(tr("Drag the selected item to move it; Shift+drag moves it up and down"))->setEnabled(false);
	keys->addAction(tr("Ctrl+arrows: move it along x and y; Page Up/Down: up and down (Shift: by 1)"))->setEnabled(false);
	keys->addAction(tr("Delete: delete; Ctrl+D: copy; F: flip; [ and ]: frame"))->setEnabled(false);
	keys->addAction(tr("P: place the shape picked in Shapes; I: pick the selected item's shape"))->setEnabled(false);

	QToolBar *bar = addToolBar(tr("Layers"));
	bar->setObjectName("layers");
	bar->addAction(saveAction);
	bar->addAction(undo);
	bar->addAction(redo);
	bar->addSeparator();
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
	eggs_ = toggle(tr("Eggs"), true, tr("Eggs and the other shapes only the game's editor shows"));
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
	connect(roof_, &QSpinBox::valueChanged, this, &MainWindow::applyOptions);
	bar->addWidget(roof_);
	bar->addWidget(new QLabel(tr("  Snap ")));
	snap_ = new QComboBox;
	for (int s : {1, 2, 8, 16, 32, 64, 128, 256})
		snap_->addItem(s == 1 ? tr("off") : QString::number(s), s);
	snap_->setToolTip(tr("Moves and placing go in steps of this many world units (a floor tile is 128 or 256)"));
	connect(snap_, &QComboBox::currentIndexChanged, this, [this] {
		view_->setSnap(snap_->currentData().toInt());
		QSettings().setValue("snap", snap_->currentData().toInt());
	});
	snap_->setCurrentIndex(std::max(0, snap_->findData(QSettings().value("snap", 32).toInt())));
	view_->setSnap(snap_->currentData().toInt());
	bar->addWidget(snap_);

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
	entry(viewMenu, tr("&Transitions"), this, [this] { tabs_->setCurrentWidget(graph_); }, QKeySequence(Qt::CTRL | Qt::Key_T));
	entry(viewMenu, tr("&Game Data"), this, [this] { tabs_->setCurrentWidget(gameData_); }, QKeySequence(Qt::CTRL | Qt::Key_G));
	for (QDockWidget *d : findChildren<QDockWidget *>())
		viewMenu->addAction(d->toggleViewAction());

	QMenu *help = menuBar()->addMenu(tr("&Help"));
	entry(help, tr("&About"), this, [this] {
		QMessageBox::about(this, tr("U8 Editor"),
			tr("<p><b>U8 Editor</b>: an editor for Ultima VIII's world.</p>"
				"<p>Drag to move the view, wheel to zoom, click an item for its details and to change it. "
				"Drag the selected item to move it. Pick a shape in Shapes and click on the map to place it.</p>"
				"<p>Saving writes STATIC/FIXED.DAT, the saved game and U8.EXE, keeping each original as .orig "
				"the first time. Test in the Port starts a new game from what was saved.</p>"
				"<p>The formats are read and written as the game does; see the decompiled sources in game/item "
				"and game/u8.</p>"));
	});
	applyOptions();
}

void MainWindow::setRoof(int z)
{
	roof_->setValue(z);
}

void MainWindow::applyOptions()
{
	LayerOptions o;
	o.fixed = fixed_->isChecked();
	o.nonFixed = nonFixed_->isChecked();
	o.globs = globs_->isChecked();
	o.npcs = npcs_->isChecked();
	o.editorItems = eggs_->isChecked();
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
	editor_->setWorld(nullptr);
	world_ = std::move(w);
	editor_->setWorld(world_.get());
	QSettings().setValue("dataDir", dataDir);
	fillMapList();
	graph_->setWorld(world_.get());
	graphStale_ = false;
	shapes_->setWorld(world_.get());
	gameData_->reload();
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

static QString itemCount(const MapData &d)
{
	int n = 0;
	for (const MapItem &it : d.items)
		n += !it.deleted;
	return QString::number(n);
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
		it->setText(ItemsColumn, itemCount(d));
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

void MainWindow::updateMapRow(int map)
{
	int exits = 0;
	for (const Transition &t : world_->transitions())
		exits += t.kind == Transition::TeleportEgg && t.fromMap == map && t.toMap != map;
	fillingList_ = true;
	for (int i = 0; i < maps_->topLevelItemCount(); i++)
	{
		QTreeWidgetItem *row = maps_->topLevelItem(i);
		if (row->data(MapColumn, Qt::UserRole).toInt() != map)
			continue;
		const MapData &d = world_->map(map);
		row->setText(ItemsColumn, itemCount(d));
		row->setText(ExitsColumn, exits ? QString::number(exits) : QString());
		row->setToolTip(MapColumn, tr("%1 fixed, %2 movable, %3 from globs, %4 NPCs")
			.arg(d.fixedCount).arg(d.nonFixedCount).arg(d.globCount).arg(d.npcCount));
	}
	fillingList_ = false;
}

// After a change (or its undoing): draw the map again, and show the item that changed.
void MainWindow::mapChanged(int map, int item)
{
	if (map != view_->mapNumber())
		openMap(map);
	view_->refresh();
	if (item >= 0)
	{
		view_->select(world_->map(map).root(item));
		if (panel_->map() != map || panel_->item() != item)
			panel_->showItem(world_.get(), map, item);
		else
			panel_->refresh();
	}
	else
	{
		const int sel = view_->selected();
		if (sel >= 0 && (sel >= int(world_->map(map).items.size()) || world_->map(map).items[sel].deleted))
			view_->select(-1);
		panel_->refresh();
	}
	updateMapRow(map);
	if (tabs_->currentWidget() == graph_)
		graph_->setWorld(world_.get());
	else
		graphStale_ = true;
	updateTitle();
}

void MainWindow::editAction(int action)
{
	const int map = view_->mapNumber();
	const int item = view_->selected();
	if (!world_ || item < 0)
		return;
	MapItem f = world_->map(map).items[item];
	switch (action)
	{
	case MapView::Delete:
		if (editor_->deleteItem(map, item))
		{
			view_->select(-1);
			panel_->showItem(world_.get(), map, -1);
		}
		break;
	case MapView::Duplicate:
	{
		const int s = std::max(32, view_->snap());
		const int added = editor_->duplicateItem(map, item, s, 0);
		if (added >= 0)
		{
			view_->select(added);
			panel_->showItem(world_.get(), map, added);
		}
		break;
	}
	case MapView::Flip:
		f.flags ^= FlagFlipped;
		editor_->editItem(map, item, f, tr("Flip %1").arg(world_->describe(f)));
		break;
	case MapView::NextFrame:
	case MapView::PrevFrame:
	{
		const int frames = std::max(1, world_->shapes().frameCount(f.shape));
		f.frame = uint8_t((f.frame + (action == MapView::NextFrame ? 1 : frames - 1)) % frames);
		editor_->editItem(map, item, f, tr("Change the frame of %1").arg(world_->describe(f)));
		break;
	}
	}
}

bool MainWindow::openMap(int map)
{
	if (!world_ || world_->map(map).empty())
		return false;
	if (view_->mapNumber() != map || !view_->render().world())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		view_->stopPlacing();
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
	const int onMap = m.root(item);
	tabs_->setCurrentWidget(view_);
	if (onMap >= 0 && m.items[onMap].onMap())
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

bool MainWindow::save()
{
	if (!world_ || !world_->dirty())
	{
		statusBar()->showMessage(tr("Nothing to save."), 4000);
		return true;
	}
	const QStringList files = world_->pendingFiles();
	QString error;
	QApplication::setOverrideCursor(Qt::WaitCursor);
	const bool ok = world_->save(&error);
	QApplication::restoreOverrideCursor();
	if (!ok)
	{
		QMessageBox::warning(this, tr("U8 Editor"), tr("Saving failed: %1").arg(error));
		return false;
	}
	editor_->undo()->setClean();
	statusBar()->showMessage(tr("Saved %1.").arg(files.join(", ")), 8000);
	updateTitle();
	return true;
}

// The port, run on a home folder of its own that holds only the saved game being edited, as
// SAVEGAME/U8SAVE.000: with no GAMEDAT the game starts a new game from it.
void MainWindow::testInPort()
{
	if (!world_)
		return;
	if (world_->dirty() && !askToSave(tr("testing")))
		return;
	if (world_->dirty())
		return;
	QString program = QSettings().value("portProgram").toString();
	if (program.isEmpty() || !QFile::exists(program))
	{
		const QDir here(QCoreApplication::applicationDirPath());
#ifdef Q_OS_WIN
		const QString name = QStringLiteral("Ultima8.exe");
#else
		const QString name = QStringLiteral("Ultima8");
#endif
		for (const QString &candidate : {here.filePath(name), here.filePath("../" + name)})
			if (QFile::exists(candidate))
				program = QDir::cleanPath(candidate);
	}
	if (program.isEmpty() || !QFile::exists(program))
	{
		program = QFileDialog::getOpenFileName(this, tr("Where is the port (Ultima8)?"));
		if (program.isEmpty())
			return;
	}
	QSettings().setValue("portProgram", program);

	QDir home(QDir::temp().filePath("u8edit-test-home"));
	home.removeRecursively();
	home.mkpath("SAVEGAME");
	const QString save = world_->saveSource();
	const QString start = home.filePath("SAVEGAME/U8SAVE.000");
	if (!save.isEmpty() && QFileInfo(save).isDir())
	{
		// A GAMEDAT folder: the game carries on from it.
		home.mkpath("GAMEDAT");
		for (const QFileInfo &f : QDir(save).entryInfoList(QDir::Files))
			QFile::copy(f.filePath(), home.filePath("GAMEDAT/" + f.fileName()));
		const QDir saves(QDir(world_->dataDir()).filePath("SAVEGAME"));
		QFile::copy(saves.filePath("U8SAVE.000"), start);
	}
	else if (!save.isEmpty())
		QFile::copy(save, start);
	QStringList args{"--data", world_->dataDir(), "--home", home.path(), "--no-recording"};
	if (!QProcess::startDetached(program, args))
		QMessageBox::warning(this, tr("U8 Editor"), tr("Could not start %1").arg(program));
	else
		statusBar()->showMessage(tr("Started the port on a new game from the saved files."), 8000);
}

void MainWindow::chooseDataDir()
{
	if (!askToSave(tr("opening another game folder")))
		return;
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
	if (!askToSave(tr("opening another saved game")))
		return;
	QString start = QDir(world_->dataDir()).filePath("SAVEGAME");
	if (!QDir(start).exists())
		start = QDir(portHome()).filePath("SAVEGAME");
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
	if (world_ && world_->dirty())
		title.prepend(QStringLiteral("* "));
	setWindowTitle(title);
}
