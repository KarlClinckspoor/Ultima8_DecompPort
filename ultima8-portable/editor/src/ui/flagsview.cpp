#include "ui/flagsview.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QSettings>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "ui/edits.h"

namespace
{

enum Column { ColFlag, ColBits, ColValue, ColName, ColReaders, ColWriters, ColNotes, ColCount };

QString key(int bit, int width, const char *what)
{
	return QStringLiteral("flags/%1.%2/%3").arg(bit).arg(width).arg(QLatin1String(what));
}

QTableWidgetItem *fixedCell(const QString &text)
{
	auto *c = new QTableWidgetItem(text);
	c->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
	return c;
}

QTableWidgetItem *numberCell(int v, bool editable)
{
	auto *c = new QTableWidgetItem;
	c->setData(Qt::DisplayRole, v);
	c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	if (!editable)
		c->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
	return c;
}

}

// What GAME-MECHANICS.md found the flags to be, offered until the user names them.
QString FlagsView::defaultName(int bit, int width)
{
	static const QMap<QPair<int, int>, const char *> names = {
		{{2, 1}, "the cheat menu's Jump Ahead was used"},
		{{15, 1}, "a conversation is going on"},
		{{54, 2}, "the Lurker: 1 freed by the earth spell, 2 Hydros free"},
		{{70, 1}, "recall: Central Tenebrae visited"},
		{{71, 1}, "recall: the Plateau visited"},
		{{72, 1}, "recall: the Upper Catacombs visited"},
		{{73, 1}, "recall: the Hall of the Mountain King visited"},
		{{74, 1}, "recall: Argentrock Isle visited"},
		{{75, 1}, "recall: Carthax Lake visited"},
		{{76, 1}, "recall: Daemon's Crag visited"},
		{{144, 2}, "Sorcery: the rival dealt with (1 Vardion, 2 Bane)"},
		{{158, 1}, "the Avatar has cheated"},
		{{252, 1}, "asked Devon who he is"},
		{{257, 1}, "said goodbye to Devon (he knows the Avatar's name)"},
		{{253, 2}, "Devon is known as the fisherman (when not 0)"},
		{{357, 3}, "Malchir's trial: tests passed (6: done)"},
	};
	return QString::fromLatin1(names.value({bit, width}, ""));
}

QString FlagsView::flagName(int bit, int width)
{
	const QString name = QSettings().value(key(bit, width, "name")).toString();
	return name.isEmpty() ? defaultName(bit, width) : name;
}

FlagsView::FlagsView(Editor *editor, QWidget *parent)
	: QWidget(parent), editor_(editor)
{
	auto *v = new QVBoxLayout(this);
	note_ = new QLabel;
	note_->setWordWrap(true);
	v->addWidget(note_);

	auto *bar = new QHBoxLayout;
	search_ = new QLineEdit;
	search_->setPlaceholderText(tr("Find: a flag's number, name or notes, or a class (DEVON)"));
	search_->setClearButtonEnabled(true);
	onlySet_ = new QCheckBox(tr("Only flags that are not 0"));
	bar->addWidget(search_, 1);
	bar->addWidget(onlySet_);
	v->addLayout(bar);

	table_ = new QTableWidget;
	table_->setColumnCount(ColCount);
	table_->setHorizontalHeaderLabels({tr("Flag"), tr("Bits"), tr("Value"), tr("Name"), tr("Read by"),
		tr("Written by"), tr("Notes")});
	table_->verticalHeader()->setVisible(false);
	table_->verticalHeader()->setDefaultSectionSize(table_->fontMetrics().height() + 6);
	table_->setAlternatingRowColors(true);
	table_->setSelectionBehavior(QAbstractItemView::SelectRows);
	table_->setSelectionMode(QAbstractItemView::SingleSelection);
	table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	table_->horizontalHeader()->setStretchLastSection(true);
	table_->setSortingEnabled(false);
	v->addWidget(table_, 1);

	auto *hint = new QLabel(tr("Double-click a flag's number or its classes to see where the usecode reads and "
		"writes it. Names and notes are kept for you; values are saved to the saved game's FLAG.DAT."));
	hint->setWordWrap(true);
	v->addWidget(hint);

	connect(search_, &QLineEdit::textChanged, this, &FlagsView::filter);
	connect(onlySet_, &QCheckBox::toggled, this, &FlagsView::filter);
	connect(table_, &QTableWidget::cellChanged, this, &FlagsView::cellChanged);
	connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
		if (column == ColFlag || column == ColBits || column == ColReaders || column == ColWriters)
			showUses(row);
	});
	connect(editor_, &Editor::flagChanged, this, &FlagsView::flagChanged);
}

void FlagsView::reload()
{
	rows_.clear();
	const World *w = editor_->world();
	if (w)
	{
		QMap<QPair<int, int>, Row> found;
		for (const FlagUse &u : w->usecode().flagUses())
		{
			Row &r = found[{u.bit, u.width}];
			r.bit = u.bit;
			r.width = u.width;
			QStringList &list = u.write ? r.writers : r.readers;
			const QString cls = w->usecode().className(u.classId);
			if (!list.contains(cls))
				list << cls;
		}
		for (const Row &r : found)
			rows_ << r;
	}
	fill();
}

void FlagsView::fill()
{
	filling_ = true;
	const World *w = editor_->world();
	const bool canSave = w && !w->saveSource().isEmpty();
	if (!w)
		note_->setText(QString());
	else if (!canSave)
		note_->setText(tr("There is no saved game, so the flags are shown as a new game has them (all 0) and "
			"cannot be changed."));
	else
		note_->setText(tr("<b>%1</b> flags the usecode uses. Values from <b>%2</b>%3.")
			.arg(rows_.size())
			.arg(QDir::toNativeSeparators(QDir(w->dataDir()).relativeFilePath(w->saveSource())).toHtmlEscaped())
			.arg(w->flagsInSave() ? QString()
				: tr(", which has no FLAG.DAT: every flag starts at 0, as in a new game. A changed value "
					"is saved as a new FLAG.DAT, and a new game started from this saved game starts with it")));
	QSettings settings;
	table_->setRowCount(rows_.size());
	for (int i = 0; i < rows_.size(); i++)
	{
		const Row &r = rows_[i];
		table_->setItem(i, ColFlag, numberCell(r.bit, false));
		table_->setItem(i, ColBits, numberCell(r.width, false));
		table_->setItem(i, ColValue, numberCell(w ? w->flag(r.bit, r.width) : 0, canSave));
		const QString own = settings.value(key(r.bit, r.width, "name")).toString();
		auto *name = new QTableWidgetItem(own.isEmpty() ? defaultName(r.bit, r.width) : own);
		if (own.isEmpty() && !defaultName(r.bit, r.width).isEmpty())
		{
			name->setForeground(palette().color(QPalette::PlaceholderText));
			name->setToolTip(tr("What the editor knows of this flag; type to name it yourself."));
		}
		table_->setItem(i, ColName, name);
		table_->setItem(i, ColReaders, fixedCell(r.readers.join(", ")));
		table_->setItem(i, ColWriters, fixedCell(r.writers.join(", ")));
		table_->setItem(i, ColNotes, new QTableWidgetItem(settings.value(key(r.bit, r.width, "notes")).toString()));
	}
	table_->resizeColumnsToContents();
	table_->setColumnWidth(ColName, std::max(table_->columnWidth(ColName), 220));
	table_->setColumnWidth(ColReaders, std::min(table_->columnWidth(ColReaders), 260));
	table_->setColumnWidth(ColWriters, std::min(table_->columnWidth(ColWriters), 260));
	filling_ = false;
	filter();
}

void FlagsView::filter()
{
	const QString text = search_->text().trimmed();
	bool isNumber = false;
	const int number = text.toInt(&isNumber);
	for (int i = 0; i < table_->rowCount(); i++)
	{
		bool show = true;
		if (onlySet_->isChecked())
			show = table_->item(i, ColValue)->data(Qt::DisplayRole).toInt() != 0;
		if (show && !text.isEmpty())
		{
			if (isNumber)
				show = rows_[i].bit == number;
			else
				show = table_->item(i, ColName)->text().contains(text, Qt::CaseInsensitive)
					|| table_->item(i, ColNotes)->text().contains(text, Qt::CaseInsensitive)
					|| rows_[i].readers.contains(text, Qt::CaseInsensitive)
					|| rows_[i].writers.contains(text, Qt::CaseInsensitive);
		}
		table_->setRowHidden(i, !show);
	}
}

void FlagsView::showClass(const QString &className)
{
	search_->setText(className);
}

void FlagsView::cellChanged(int row, int column)
{
	if (filling_ || row < 0 || row >= rows_.size())
		return;
	const Row &r = rows_[row];
	QSettings settings;
	if (column == ColName)
	{
		QTableWidgetItem *c = table_->item(row, column);
		if (c->text() == defaultName(r.bit, r.width))
			settings.remove(key(r.bit, r.width, "name"));
		else
			settings.setValue(key(r.bit, r.width, "name"), c->text());
	}
	else if (column == ColNotes)
		settings.setValue(key(r.bit, r.width, "notes"), table_->item(row, column)->text());
	else if (column == ColValue)
	{
		bool ok = false;
		const int v = table_->item(row, column)->data(Qt::DisplayRole).toInt(&ok);
		if (ok)
			editor_->editFlag(r.bit, r.width, v);
		flagChanged(r.bit);	// shows what was kept (the value within the flag's bits)
	}
}

void FlagsView::flagChanged(int bit)
{
	const World *w = editor_->world();
	if (!w)
		return;
	filling_ = true;
	Q_UNUSED(bit);	// flags of different widths share bits: refresh them all
	for (int i = 0; i < rows_.size(); i++)
		table_->item(i, ColValue)->setData(Qt::DisplayRole, w->flag(rows_[i].bit, rows_[i].width));
	filling_ = false;
}

void FlagsView::showUses(int row)
{
	const World *w = editor_->world();
	if (!w || row < 0 || row >= rows_.size())
		return;
	const Row &r = rows_[row];
	QDialog dialog(this);
	const QString name = flagName(r.bit, r.width);
	dialog.setWindowTitle(name.isEmpty() ? tr("Flag %1").arg(r.bit) : tr("Flag %1: %2").arg(r.bit).arg(name));
	auto *v = new QVBoxLayout(&dialog);
	auto *tree = new QTreeWidget;
	tree->setHeaderLabels({tr("Class"), tr("Routine"), tr("At"), tr("Does"), tr("Strings around it")});
	tree->setRootIsDecorated(false);
	tree->setAlternatingRowColors(true);
	tree->setWordWrap(true);
	for (const FlagUse &u : w->usecode().flagUses())
	{
		if (u.bit != r.bit || u.width != r.width)
			continue;
		QString does = u.write ? (u.value != INT_MIN ? tr("sets it to %1").arg(u.value) : tr("sets it")) : tr("reads it");
		QStringList around = w->usecode().stringsAround(u.classId, u.offset, 2);
		for (QString &s : around)
			if (s != QStringLiteral("◆"))
				s = QStringLiteral("“%1”").arg(s.trimmed().left(70));
		auto *item = new QTreeWidgetItem({w->usecode().className(u.classId), w->usecode().routineAt(u.classId, u.offset),
			QString::number(u.offset, 16), does, around.join(QStringLiteral("  "))});
		item->setToolTip(4, around.join(QStringLiteral("\n")));
		tree->addTopLevelItem(item);
	}
	for (int c = 0; c < 4; c++)
		tree->resizeColumnToContents(c);
	v->addWidget(new QLabel(tr("Where the usecode reads and writes flag %1 (%2 bits). ◆ marks the flag's place "
		"among the routine's strings.").arg(r.bit).arg(r.width)));
	v->addWidget(tree, 1);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	v->addWidget(buttons);
	dialog.resize(1000, 480);
	dialog.exec();
}
