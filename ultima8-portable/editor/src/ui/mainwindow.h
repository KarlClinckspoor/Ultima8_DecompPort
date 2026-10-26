#pragma once

#include <QMainWindow>

#include <memory>

#include "data/world.h"
#include "ui/maprender.h"

class QAction;
class QComboBox;
class QDialog;
class QLabel;
class QSpinBox;
class QTabWidget;
class QTreeWidget;
class Editor;
class GameDataView;
class ItemPanel;
class MapView;
class Minimap;
class ShapeBrowser;
class TransitionGraph;

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
	void showDetails(int map, int item);
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

	std::unique_ptr<World> world_;
	Editor *editor_;
	MapView *view_;
	Minimap *minimap_;
	TransitionGraph *graph_;
	GameDataView *gameData_;
	ShapeBrowser *shapes_;
	QTabWidget *tabs_;
	QTreeWidget *maps_;
	ItemPanel *panel_;
	QDialog *details_;
	QLabel *status_;
	QLabel *zoomLabel_;
	QSpinBox *roof_;
	QComboBox *snap_;
	QAction *fixed_, *nonFixed_, *globs_, *npcs_, *eggs_, *invisible_, *ranges_, *labels_;
	bool fillingList_ = false;
	bool graphStale_ = false;
};
