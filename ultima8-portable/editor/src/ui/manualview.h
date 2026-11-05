// The editor's manual (manual/manual.html, built into the program), in a window of its own.
// Passages inferred rather than read plainly from the game's code are brown, and their tooltip
// says so.
#pragma once

#include <QDialog>

class QTextBrowser;

class ManualView : public QDialog
{
	Q_OBJECT

public:
	explicit ManualView(QWidget *parent = nullptr);
	// Shows the section with this anchor (empty: the top).
	void showSection(const QString &anchor);
	QTextBrowser *browser() const { return browser_; }

	// The manual's HTML, as built into the program.
	static QString html();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	QTextBrowser *browser_;
};
