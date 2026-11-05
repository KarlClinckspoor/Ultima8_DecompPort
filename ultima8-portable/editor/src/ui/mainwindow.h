#pragma once

#include <QMainWindow>

#include <memory>

#include "data/world.h"
#include "ui/maprender.h"

class QAction;
class QComboBox;
class QDialog;
class QDockWidget;
class QLabel;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTreeWidget;
class Editor;
class FlagsView;
class GameDataView;
class ItemPanel;
class MapView;
class ManualView;
class Minimap;
class SearchPanel;
class ShapeBrowser;
class TransitionGraph;
class UsecodeView;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow();
	~MainWindow() override;

	// Loads the game's data; on failure says why and returns false.
	bool loadWorld(const QString &dataDir, const QString &savePath, QString *error);
	bool openMap(int map);
	void goToItem(int map, int item, bool details);
	MapView *mapView() const { return view_; }
	TransitionGraph *graph() const { return graph_; }
	ItemPanel *itemPanel() const { return panel_; }
	GameDataView *gameData() const { return gameData_; }
	ShapeBrowser *shapeBrowser() const { return shapes_; }
	Editor *editor() const { return editor_; }
	QTabWidget *tabs() const { return tabs_; }
	UsecodeView *usecodeView() const { return usecodeView_; }
	void showDetails(int map, int item);
	// The manual, at the section with this anchor (empty: the top).
	void showManual(const QString &anchor);
	void setRoof(int z);
	const World *world() const { return world_.get(); }
	// Writes the changes; false (after saying why) when that failed.
	bool save();

	// The port's own home folder and the data folder it remembers (platform/matoya/home.c).
	static QString portHome();
	static QString portDataDir();

protected:
	void closeEvent(QCloseEvent *) override;

private:
	void applyTranslation();
	void buildMenus();
	void buildDocks();
	void fillMapList();
	void updateMapRow(int map);
	void applyOptions();
	void chooseDataDir();
	void chooseSave();
	void exportView();
	void updateTitle();
	bool askToSave(const QString &before);
	void mapChanged(int map, int item);
	void editAction(int action);
	void testInPort();
	// The map's selection when it holds item on map, else just item.
	QVector<int> selectionWith(int map, int item) const;
	// The items on this map that go with the selected one: the doors a key opens, a door's keys,
	// a teleport egg's partner.
	void updateRelated();
	// Names changed (given, read from a file, full or not): show them again.
	void namesChanged();
	void exportNames();
	void importNames();
	// Who stands on the map, by name, to tell maps apart.
	QString peopleOn(int map) const;

	std::unique_ptr<World> world_;
	Editor *editor_;
	MapView *view_;
	Minimap *minimap_;
	TransitionGraph *graph_;
	FlagsView *flags_ = nullptr;
	GameDataView *gameData_;
	ShapeBrowser *shapes_;
	QTabWidget *tabs_;
	QTreeWidget *maps_;
	ItemPanel *panel_;
	SearchPanel *search_;
	ManualView *manual_ = nullptr;
	UsecodeView *usecodeView_;
	QDockWidget *searchDock_;
	QDockWidget *details_;
	QLabel *status_;
	QLabel *zoomLabel_;
	QSpinBox *roof_;
	QComboBox *snap_;
	QAction *fixed_, *nonFixed_, *globs_, *npcs_, *eggs_, *invisible_, *ranges_, *labels_, *heights_;
	QAction *xray_, *fullNames_;
	QSpinBox *xrayHeight_, *xrayRadius_;
	QSlider *xrayOpacity_[4];
	QSlider *opacity_[4];
	bool fillingList_ = false;
	bool graphStale_ = false;
};
