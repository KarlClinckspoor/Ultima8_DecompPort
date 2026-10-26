#pragma once

#include <QMainWindow>

#include <memory>

#include "data/world.h"
#include "ui/maprender.h"

class QAction;
class QDialog;
class QLabel;
class QSpinBox;
class QTabWidget;
class QTreeWidget;
class ItemPanel;
class MapView;
class Minimap;
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
	QTabWidget *tabs() const { return tabs_; }
	void showDetails(int map, int item);
	const World *world() const { return world_.get(); }

	// The port's own home folder and the data folder it remembers (platform/matoya/home.c).
	static QString portHome();
	static QString portDataDir();

protected:
	void closeEvent(QCloseEvent *) override;

private:
	void buildMenus();
	void buildDocks();
	void fillMapList();
	void applyOptions();
	void chooseDataDir();
	void chooseSave();
	void exportView();
	void updateTitle();

	std::unique_ptr<World> world_;
	MapView *view_;
	Minimap *minimap_;
	TransitionGraph *graph_;
	QTabWidget *tabs_;
	QTreeWidget *maps_;
	ItemPanel *panel_;
	QDialog *details_;
	QLabel *status_;
	QLabel *zoomLabel_;
	QSpinBox *roof_;
	QAction *fixed_, *nonFixed_, *globs_, *npcs_, *editor_, *invisible_, *ranges_, *labels_;
	bool fillingList_ = false;
};
