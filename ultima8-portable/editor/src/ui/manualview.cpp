#include "ui/manualview.h"

#include <QFile>
#include <QHelpEvent>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextBrowser>
#include <QToolTip>
#include <QVBoxLayout>

QString ManualView::html()
{
	QFile f(QStringLiteral(":/manual/manual.html"));
	if (!f.open(QIODevice::ReadOnly))
		return QObject::tr("<p>The manual is missing from this build.</p>");
	return QString::fromUtf8(f.readAll());
}

ManualView::ManualView(QWidget *parent) : QDialog(parent, Qt::Window)
{
	setWindowTitle(tr("U8 Editor Manual"));
	browser_ = new QTextBrowser;
	browser_->setOpenExternalLinks(true);
	browser_->setHtml(html());
	// QTextBrowser does not show a span's title by itself: the tooltip comes from here.
	browser_->viewport()->installEventFilter(this);

	auto *find = new QLineEdit;
	find->setPlaceholderText(tr("Find in the manual"));
	find->setClearButtonEnabled(true);
	auto *next = new QPushButton(tr("Next"));
	auto *top = new QPushButton(tr("Contents"));
	auto search = [this, find] {
		if (find->text().isEmpty())
			return;
		if (!browser_->find(find->text()))
		{
			// From the top again.
			browser_->moveCursor(QTextCursor::Start);
			browser_->find(find->text());
		}
	};
	connect(find, &QLineEdit::returnPressed, this, search);
	connect(next, &QPushButton::clicked, this, search);
	connect(top, &QPushButton::clicked, this, [this] { showSection(QStringLiteral("contents")); });

	auto *bar = new QHBoxLayout;
	bar->addWidget(top);
	bar->addStretch();
	bar->addWidget(find, 1);
	bar->addWidget(next);
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(6, 6, 6, 6);
	v->addLayout(bar);
	v->addWidget(browser_, 1);
	resize(820, 760);
}

void ManualView::showSection(const QString &anchor)
{
	if (anchor.isEmpty())
		browser_->moveCursor(QTextCursor::Start);
	else
		browser_->scrollToAnchor(anchor);
}

bool ManualView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == browser_->viewport() && event->type() == QEvent::ToolTip)
	{
		auto *help = static_cast<QHelpEvent *>(event);
		const QTextCursor at = browser_->cursorForPosition(help->pos());
		const QString tip = at.charFormat().toolTip();
		if (tip.isEmpty())
			QToolTip::hideText();
		else
			QToolTip::showText(help->globalPos(), tip, browser_->viewport());
		return true;
	}
	return QDialog::eventFilter(watched, event);
}
