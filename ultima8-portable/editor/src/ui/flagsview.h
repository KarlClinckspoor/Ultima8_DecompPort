// The Flags tab: usecode's global flags (u8/flag.cpp), the story's memory. Every flag the
// usecode reads or writes, its value in the open saved game, the classes that use it and
// where; values can be changed (saved to FLAG.DAT), and names and notes kept for each flag.
#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class Editor;

class FlagsView : public QWidget
{
	Q_OBJECT

public:
	explicit FlagsView(Editor *editor, QWidget *parent = nullptr);
	// Builds the table again from the world.
	void reload();
	// Shows only the flags this class uses.
	void showClass(const QString &className);
	// Shows only this flag.
	void showFlag(int bit) { showClass(QString::number(bit)); }

	// The name and notes kept for a flag, by this user (QSettings); the name falls back to
	// one the editor knows.
	static QString flagName(int bit, int width);
	static QString defaultName(int bit, int width);

private:
	struct Row
	{
		int bit;
		int width;
		QStringList readers;	// class names
		QStringList writers;
	};

	void fill();
	void filter();
	void cellChanged(int row, int column);
	void showUses(int row);
	void flagChanged(int bit);

	Editor *editor_;
	QLabel *note_;
	QLineEdit *search_;
	QCheckBox *onlySet_;
	QTableWidget *table_;
	QVector<Row> rows_;
	bool filling_ = false;
};
