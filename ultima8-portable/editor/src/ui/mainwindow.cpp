#include "ui/mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFormLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <functional>

#include "ui/edits.h"
#include "ui/flagsview.h"
#include "ui/gamedata.h"
#include "ui/graphview.h"
#include "ui/itempanel.h"
#include "ui/manualview.h"
#include "ui/mapview.h"
#include "ui/minimap.h"
#include "ui/names.h"
#include "ui/searchpanel.h"
#include "ui/shapebrowser.h"
#include "ui/translatedialog.h"
#include "ui/usecodeview.h"

enum { MapColumn, NameColumn, ItemsColumn, ExitsColumn, PeopleColumn };

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
	flags_ = new FlagsView(editor_);
	tabs_->addTab(flags_, tr("Flags"));
	usecodeView_ = new UsecodeView;
	tabs_->addTab(usecodeView_, tr("Usecode"));
	tabs_->setDocumentMode(true);
	setCentralWidget(tabs_);

	panel_ = new ItemPanel;

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
	connect(view_, &MapView::selectionChanged, this, [this] {
		panel_->setSelectionCount(panel_->map() == view_->mapNumber() && view_->selection().contains(panel_->item())
			? int(view_->selection().size()) : 1);
		updateRelated();
	});
	connect(view_, &MapView::status, status_, &QLabel::setText);
	connect(view_, &MapView::viewChanged, this, [this] {
		zoomLabel_->setText(tr("Zoom %1%").arg(qRound(view_->zoom() * 100)));
	});
	connect(view_, &MapView::moveRequested, this, [this](int dx, int dy, int dz) {
		editor_->moveItems(view_->mapNumber(), view_->selection(), dx, dy, dz);
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
		if (editor_->deleteItems(map, selectionWith(map, item)))
			panel_->showItem(world_.get(), map, -1);
	});
	connect(panel_, &ItemPanel::duplicateItem, this, [this](int map, int item) {
		const int s = std::max(32, view_->snap());
		const QVector<int> added = editor_->duplicateItems(map, selectionWith(map, item), s, 0);
		if (added.size() == 1)
			goToItem(map, added.first(), true);
		else if (!added.isEmpty() && map == view_->mapNumber())
			view_->setSelection(added, added.last());
	});
	connect(panel_, &ItemPanel::nudge, this, [this](int dx, int dy, int dz) {
		if (panel_->item() >= 0)
			editor_->moveItems(panel_->map(), selectionWith(panel_->map(), panel_->item()), dx, dy, dz);
	});
	connect(panel_, &ItemPanel::namesChanged, this, &MainWindow::namesChanged);
	connect(panel_, &ItemPanel::find, this, [this](const QString &query) {
		searchDock_->show();
		searchDock_->raise();
		search_->run(query, true);
	});
	connect(search_, &SearchPanel::goToItem, this, [this](int map, int item) { goToItem(map, item, true); });
	connect(panel_, &ItemPanel::showUsecode, this, [this](int classId, int event) {
		tabs_->setCurrentWidget(usecodeView_);
		usecodeView_->showClass(classId, event);
	});
	connect(usecodeView_, &UsecodeView::goToItem, this, [this](int map, int item) { goToItem(map, item, true); });
	connect(usecodeView_, &UsecodeView::showFlag, this, [this](int bit) {
		tabs_->setCurrentWidget(flags_);
		flags_->showFlag(bit);
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
	// Names can follow flags (a person's name once the Avatar has met them).
	connect(editor_, &Editor::flagChanged, this, [this] {
		panel_->refresh();
		view_->update();
		updateTitle();
	});
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
	maps_->setColumnCount(5);
	maps_->setHeaderLabels({tr("Map"), tr("Name"), tr("Items"), tr("Exits"), tr("People")});
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
		Names::setMapName(map, it->text(NameColumn));
		namesChanged();
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

	search_ = new SearchPanel;
	searchDock_ = new QDockWidget(tr("Search"), this);
	searchDock_->setObjectName("search");
	searchDock_->setWidget(search_);
	addDockWidget(Qt::RightDockWidgetArea, searchDock_);
	tabifyDockWidget(shapeDock, searchDock_);
	shapeDock->raise();

	// The Item window: a dock beside Shapes and Search, or floating where it was left (the
	// window's state, docks included, is kept when the editor closes).
	details_ = new QDockWidget(tr("Item"), this);
	details_->setObjectName("item");
	details_->setWidget(panel_);
	details_->setMinimumWidth(320);
	addDockWidget(Qt::RightDockWidgetArea, details_, Qt::Horizontal);
	splitDockWidget(details_, shapeDock, Qt::Horizontal);
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
	entry(file, tr("E&xport Names..."), this, [this] { exportNames(); });
	entry(file, tr("I&mport Names..."), this, [this] { importNames(); });
	entry(file, tr("&Apply a Translation..."), this, [this] { applyTranslation(); });
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
	entry(edit, tr("&Find..."), this, [this] {
		searchDock_->show();
		searchDock_->raise();
		search_->setFocus();
		if (auto *line = search_->findChild<QLineEdit *>())
		{
			line->setFocus();
			line->selectAll();
		}
	}, QKeySequence::Find);
	edit->addSeparator();
	QMenu *keys = edit->addMenu(tr("On the Map"));
	keys->addAction(tr("Click: select; Ctrl+click: add or take away; Ctrl+drag: select what a box holds; Ctrl+A: all in view"))->setEnabled(false);
	keys->addAction(tr("Right-click: list everything at and around the pointer, to pick from"))->setEnabled(false);
	keys->addAction(tr("Drag the selection to move it; Shift+drag: up and down; Alt+drag: without the snap"))->setEnabled(false);
	keys->addAction(tr("Ctrl+arrows: move along x and y by the snap; Alt+arrows: by 1"))->setEnabled(false);
	keys->addAction(tr("Page Up/Down: up and down by 8 (Shift or Alt: by 1)"))->setEnabled(false);
	keys->addAction(tr("Delete: delete; Ctrl+D: copy; F: flip; [ and ]: frame"))->setEnabled(false);
	keys->addAction(tr("P: place the shape picked in Shapes; I: pick the selected item's shape"))->setEnabled(false);
	keys->addAction(tr("H: heights; X: see through around the pointer"))->setEnabled(false);

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
	heights_ = toggle(tr("Heights"), d.heights, tr("The bottom (blue) and top (orange) of the selected items and the one "
		"under the pointer, with their heights (H)"));
	heights_->setShortcut(QKeySequence(Qt::Key_H));
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

	// A menu of one slider for each layer (fixed, movable, globs, NPCs), as a toolbar button; the
	// values are kept in the settings under key.
	auto layerSliders = [this](const QString &title, const QString &button, const QString &tip, QSlider **out,
		const QString &key, int initial, const QString &resetText, int resetValue) {
		auto *menu = new QMenu(title, this);
		auto *box = new QWidget;
		auto *form = new QFormLayout(box);
		const QString layerNames[4] = {tr("Fixed"), tr("Movable"), tr("Globs"), tr("NPCs")};
		for (int i = 0; i < 4; i++)
		{
			out[i] = new QSlider(Qt::Horizontal);
			out[i]->setRange(0, 100);
			out[i]->setValue(key.isEmpty() ? initial : QSettings().value(QStringLiteral("%1%2").arg(key).arg(i), initial).toInt());
			out[i]->setMinimumWidth(160);
			connect(out[i], &QSlider::valueChanged, this, &MainWindow::applyOptions);
			form->addRow(layerNames[i], out[i]);
		}
		auto *reset = new QPushButton(resetText);
		connect(reset, &QPushButton::clicked, this, [out, resetValue] {
			for (int i = 0; i < 4; i++)
				out[i]->setValue(resetValue);
		});
		form->addRow(reset);
		auto *action = new QWidgetAction(this);
		action->setDefaultWidget(box);
		menu->addAction(action);
		auto *b = new QToolButton;
		b->setText(button);
		b->setToolTip(tip);
		b->setMenu(menu);
		b->setPopupMode(QToolButton::InstantPopup);
		return std::make_pair(menu, b);
	};

	// How opaque each layer is drawn.
	layers->addSeparator();
	auto [opacityMenu, opacityButton] = layerSliders(tr("Layer &Opacity"), tr("Opacity"),
		tr("How opaque each layer is drawn, to see through walls and roofs"), opacity_, QString(), 100,
		tr("All Opaque"), 100);
	layers->addMenu(opacityMenu);
	bar->addSeparator();
	bar->addWidget(opacityButton);

	// Seeing through around the pointer.
	QToolBar *xbar = addToolBar(tr("X-ray"));
	xbar->setObjectName("xray");
	xray_ = new QAction(tr("X-ray"), this);
	xray_->setCheckable(true);
	xray_->setShortcut(QKeySequence(Qt::Key_X));
	xray_->setToolTip(tr("See through around the pointer: what is at or above the height is faded (X)"));
	connect(xray_, &QAction::toggled, this, &MainWindow::applyOptions);
	layers->addAction(xray_);
	xbar->addAction(xray_);
	xbar->addWidget(new QLabel(tr(" above ")));
	xrayHeight_ = new QSpinBox;
	xrayHeight_->setRange(0, 255);
	xrayHeight_->setValue(QSettings().value("xray/height", 24).toInt());
	xrayHeight_->setToolTip(tr("Items at or above this height are faded inside the circle (a storey is about 40)"));
	connect(xrayHeight_, &QSpinBox::valueChanged, this, &MainWindow::applyOptions);
	xbar->addWidget(xrayHeight_);
	xbar->addWidget(new QLabel(tr(" radius ")));
	xrayRadius_ = new QSpinBox;
	xrayRadius_->setRange(16, 1024);
	xrayRadius_->setSingleStep(16);
	xrayRadius_->setValue(QSettings().value("xray/radius", 128).toInt());
	xrayRadius_->setToolTip(tr("The circle's radius, in screen pixels of the map"));
	connect(xrayRadius_, &QSpinBox::valueChanged, this, &MainWindow::applyOptions);
	xbar->addWidget(xrayRadius_);
	// How much of each layer still shows inside the circle.
	auto [fadeMenu, fadeButton] = layerSliders(tr("X-ray &Fade"), tr("Fade"),
		tr("How much of each layer still shows inside the circle, above the height; at the left it is left out"),
		xrayOpacity_, QStringLiteral("xray/opacity"), 15, tr("All Left Out"), 0);
	layers->addMenu(fadeMenu);
	xbar->addWidget(fadeButton);

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
	entry(viewMenu, tr("&Reset the Layout"), this, [this] {
		// The docks back where they start: Maps and Minimap left, Item, then Shapes and Search right.
		for (QDockWidget *d : findChildren<QDockWidget *>())
		{
			d->setFloating(false);
			d->show();
		}
		QDockWidget *maps = findChild<QDockWidget *>("maps"), *mini = findChild<QDockWidget *>("minimap");
		QDockWidget *shapes = findChild<QDockWidget *>("shapes");
		addDockWidget(Qt::LeftDockWidgetArea, maps);
		addDockWidget(Qt::LeftDockWidgetArea, mini);
		addDockWidget(Qt::RightDockWidgetArea, details_);
		addDockWidget(Qt::RightDockWidgetArea, shapes);
		splitDockWidget(details_, shapes, Qt::Horizontal);
		tabifyDockWidget(shapes, searchDock_);
		shapes->raise();
	});
	entry(viewMenu, tr("&Transitions"), this, [this] { tabs_->setCurrentWidget(graph_); }, QKeySequence(Qt::CTRL | Qt::Key_T));
	entry(viewMenu, tr("&Game Data"), this, [this] { tabs_->setCurrentWidget(gameData_); }, QKeySequence(Qt::CTRL | Qt::Key_G));
	entry(viewMenu, tr("&Flags"), this, [this] { tabs_->setCurrentWidget(flags_); }, QKeySequence(Qt::CTRL | Qt::Key_L));
	entry(viewMenu, tr("&Usecode"), this, [this] { tabs_->setCurrentWidget(usecodeView_); }, QKeySequence(Qt::CTRL | Qt::Key_U));
	viewMenu->addSeparator();
	fullNames_ = viewMenu->addAction(tr("F&ull Names"));
	fullNames_->setCheckable(true);
	fullNames_->setChecked(Names::fullNames());
	fullNames_->setToolTip(tr("Items go by the names they have once the story has got far enough, not by what the "
		"saved game's flags make them say (a person not met yet is still named)"));
	connect(fullNames_, &QAction::toggled, this, [this](bool on) {
		Names::setFullNames(on);
		namesChanged();
	});
	viewMenu->addSeparator();
	for (QDockWidget *d : findChildren<QDockWidget *>())
		viewMenu->addAction(d->toggleViewAction());

	QMenu *help = menuBar()->addMenu(tr("&Help"));
	entry(help, tr("&Manual"), this, [this] { showManual(QString()); }, QKeySequence::HelpContents);
	entry(help, tr("Manual: &Keys and Mouse"), this, [this] { showManual(QStringLiteral("keys")); });
	entry(help, tr("Manual: &Fields and What They Mean"), this, [this] { showManual(QStringLiteral("fields")); });
	help->addSeparator();
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

void MainWindow::showManual(const QString &anchor)
{
	if (!manual_)
		manual_ = new ManualView(this);
	manual_->show();
	manual_->raise();
	manual_->activateWindow();
	manual_->showSection(anchor);
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
	o.heights = heights_->isChecked();
	for (int i = 0; i < 4; i++)
		o.opacity[i] = opacity_[i]->value() / 100.0;
	ranges_->setEnabled(o.editorItems);
	labels_->setEnabled(o.editorItems);
	view_->setOptions(o);
	XRay x;
	x.on = xray_->isChecked();
	x.height = xrayHeight_->value();
	x.radius = xrayRadius_->value();
	for (int i = 0; i < 4; i++)
		x.opacity[i] = xrayOpacity_[i]->value() / 100.0;
	view_->setXRay(x);
	QSettings s;
	s.setValue("xray/height", x.height);
	s.setValue("xray/radius", int(x.radius));
	for (int i = 0; i < 4; i++)
		s.setValue(QStringLiteral("xray/opacity%1").arg(i), xrayOpacity_[i]->value());
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
	search_->setWorld(world_.get());
	usecodeView_->setWorld(world_.get());
	gameData_->reload();
	flags_->reload();
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
		it->setText(NameColumn, Names::mapName(m));
		it->setText(PeopleColumn, peopleOn(m));
		it->setToolTip(PeopleColumn, it->text(PeopleColumn));
		it->setForeground(PeopleColumn, QColor(150, 150, 150));
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
	maps_->header()->setSectionResizeMode(PeopleColumn, QHeaderView::Interactive);
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
		view_->setPrimary(world_->map(map).root(item));
		if (panel_->map() != map || panel_->item() != item)
			panel_->showItem(world_.get(), map, item);
		else
			panel_->refresh();
	}
	else
		panel_->refresh();
	usecodeView_->itemsChanged();
	updateRelated();
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
	const QVector<int> items = view_->selection();
	if (!world_ || item < 0 || items.isEmpty())
		return;
	const MapItem &f = world_->map(map).items[item];
	switch (action)
	{
	case MapView::Delete:
		if (editor_->deleteItems(map, items))
		{
			view_->select(-1);
			panel_->showItem(world_.get(), map, -1);
		}
		break;
	case MapView::Duplicate:
	{
		const int s = std::max(32, view_->snap());
		const QVector<int> added = editor_->duplicateItems(map, items, s, 0);
		if (!added.isEmpty())
		{
			view_->setSelection(added, added.last());
			panel_->showItem(world_.get(), map, added.last());
		}
		break;
	}
	case MapView::Flip:
		editor_->editItems(map, items, [](MapItem &i) { i.flags ^= FlagFlipped; },
			items.size() == 1 ? tr("Flip %1").arg(world_->describe(f)) : tr("Flip %n items", nullptr, int(items.size())));
		break;
	case MapView::NextFrame:
	case MapView::PrevFrame:
	{
		const Shapes &shapes = world_->shapes();
		const bool next = action == MapView::NextFrame;
		editor_->editItems(map, items, [&shapes, next](MapItem &i) {
			const int frames = std::max(1, shapes.frameCount(i.shape));
			i.frame = uint8_t((i.frame + (next ? 1 : frames - 1)) % frames);
		}, items.size() == 1 ? tr("Change the frame of %1").arg(world_->describe(f))
			: tr("Change the frame of %n items", nullptr, int(items.size())));
		break;
	}
	}
}

QVector<int> MainWindow::selectionWith(int map, int item) const
{
	if (map == view_->mapNumber() && view_->selection().contains(item))
		return view_->selection();
	return {item};
}

void MainWindow::updateRelated()
{
	QVector<int> related;
	const int map = view_->mapNumber();
	const int sel = view_->selected();
	if (world_ && map >= 0 && sel >= 0)
	{
		const MapData &m = world_->map(map);
		const MapItem &it = m.items[sel];
		QVector<int> keys;	// key numbers whose doors to show
		if (DoorInfo::isKey(it.shape) && it.quality)
			keys << it.quality;
		for (int k : it.contents)	// a keyring, or anything holding keys
			if (!m.items[k].deleted && DoorInfo::isKey(m.items[k].shape) && m.items[k].quality)
				keys << m.items[k].quality;
		for (size_t i = 0; i < m.items.size(); i++)
		{
			const MapItem &o = m.items[i];
			if (o.deleted || int(i) == sel)
				continue;
			if (DoorInfo::isDoor(o.shape) && keys.contains(DoorInfo::keyOf(o)) && o.onMap())
				related << int(i);
			// A door's keys, or what holds them.
			if (DoorInfo::isDoor(it.shape) && DoorInfo::isKey(o.shape) && o.quality == DoorInfo::keyOf(it))
			{
				const int root = m.root(int(i));
				if (root >= 0 && m.items[root].onMap() && !related.contains(root))
					related << root;
			}
		}
		for (const auto &l : view_->render().links(sel))
			related << (l.first == sel ? l.second : l.first);
	}
	view_->setRelated(related);
}

QString MainWindow::peopleOn(int map) const
{
	QStringList names;
	const MapData &m = world_->map(map);
	for (const MapItem &it : m.items)
		if (!it.deleted && it.source == MapItem::Npc && !it.contained() && it.z < 0xfb)
		{
			const QString n = world_->itemName(map, it, true);
			if (!n.isEmpty() && n[0].isUpper() && !names.contains(n))
				names << n;
		}
	if (names.size() > 6)
		return QStringLiteral("%1, ...").arg(names.mid(0, 6).join(QStringLiteral(", ")));
	return names.join(QStringLiteral(", "));
}

void MainWindow::namesChanged()
{
	graph_->refreshNames();
	panel_->refresh();
	view_->update();
	if (world_)
	{
		fillingList_ = true;
		for (int i = 0; i < maps_->topLevelItemCount(); i++)
		{
			QTreeWidgetItem *row = maps_->topLevelItem(i);
			row->setText(NameColumn, Names::mapName(row->data(MapColumn, Qt::UserRole).toInt()));
		}
		fillingList_ = false;
	}
	updateTitle();
}

void MainWindow::exportNames()
{
	const QString start = world_ ? QDir(world_->dataDir()).filePath("u8edit-names.txt") : QStringLiteral("u8edit-names.txt");
	const QString path = QFileDialog::getSaveFileName(this, tr("Export Names"), start, tr("Text files (*.txt);;All files (*)"));
	if (path.isEmpty())
		return;
	QString error;
	if (!Names::write(path, &error))
		QMessageBox::warning(this, tr("U8 Editor"), tr("Could not write %1: %2").arg(path, error));
	else
		statusBar()->showMessage(tr("Wrote the map and shape names to %1.").arg(path), 8000);
}

void MainWindow::importNames()
{
	const QString start = world_ ? world_->dataDir() : QString();
	const QString path = QFileDialog::getOpenFileName(this, tr("Import Names"), start, tr("Text files (*.txt);;All files (*)"));
	if (path.isEmpty())
		return;
	QString error;
	int count = 0;
	if (!Names::read(path, &count, &error))
		QMessageBox::warning(this, tr("U8 Editor"), tr("Could not read %1: %2").arg(path, error));
	else
		statusBar()->showMessage(tr("Read %n name(s) from %1.", nullptr, count).arg(path), 8000);
	namesChanged();
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
	search_->setCurrentMap(map);
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
	details_->show();
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
	flags_->reload();	// a FLAG.DAT may be new
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

// A translated copy of the English game folder (TranslateDialog); then, if wanted, open it.
void MainWindow::applyTranslation()
{
	TranslateDialog dialog(world_ ? world_->dataDir() : QString(), this);
	if (dialog.exec() != QDialog::Accepted || dialog.made().isEmpty())
		return;
	if (QMessageBox::question(this, tr("Apply a Translation"), tr("Open the new folder in the editor?"))
		!= QMessageBox::Yes || !askToSave(tr("opening the new folder")))
		return;
	QString error;
	if (!loadWorld(dialog.made(), QString(), &error))
		QMessageBox::warning(this, tr("U8 Editor"), error);
}
