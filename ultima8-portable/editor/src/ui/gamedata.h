// The statistics behind combat: weapons, armour and monsters (in U8.EXE) and the NPCs (in the
// saved game), as tables to change.
#pragma once

#include <QWidget>

class QLabel;
class QTableWidget;
class QTabWidget;
class Editor;

class GameDataView : public QWidget
{
	Q_OBJECT

public:
	explicit GameDataView(Editor *editor, QWidget *parent = nullptr);
	// Fills the tables again from the world.
	void reload();
	void reloadNpc(int npc);

signals:
	void showNpc(int npc);

private:
	void fillWeapons();
	void fillArmour();
	void fillMonsters();
	void fillNpcs();
	void weaponChanged(int row, int column);
	void armourChanged(int row, int column);
	void monsterChanged(int row, int column);
	void npcChanged(int row, int column);

	Editor *editor_;
	QTabWidget *tabs_;
	QLabel *note_;
	QTableWidget *weapons_;
	QTableWidget *armour_;
	QTableWidget *monsters_;
	QTableWidget *npcs_;
	bool filling_ = false;
};
