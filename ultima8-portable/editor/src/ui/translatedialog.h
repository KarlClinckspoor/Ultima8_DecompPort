// File > Apply a Translation: makes a translated copy of the English game folder from a
// translation table (data/language.h).
#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

class TranslateDialog : public QDialog
{
	Q_OBJECT

public:
	// englishDir: the folder to start from, if known.
	explicit TranslateDialog(const QString &englishDir, QWidget *parent = nullptr);
	// The folder made, once Apply has succeeded.
	QString made() const { return made_; }
	// The translation tables that come with the editor (languages/ beside it).
	static QStringList bundledTables();

private:
	void tableChanged();
	void apply();
	QLineEdit *pathRow(const QString &label, bool folder, class QFormLayout *form);

	QLineEdit *table_;
	QLineEdit *english_;
	QLineEdit *output_;
	QCheckBox *removeVoices_;
	QLabel *about_;
	QLabel *status_;
	QPushButton *applyButton_;
	QString made_;
	QString suggestedFolder_;
};
