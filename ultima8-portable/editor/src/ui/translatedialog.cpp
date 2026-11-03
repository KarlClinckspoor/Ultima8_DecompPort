#include "ui/translatedialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "data/language.h"

QStringList TranslateDialog::bundledTables()
{
	const QString app = QCoreApplication::applicationDirPath();
	QStringList dirs = {app + "/languages", app + "/../languages", app + "/../share/u8edit/languages"};
	QStringList out;
	for (const QString &d : dirs)
		for (const QFileInfo &f : QDir(d).entryInfoList({"*.json"}, QDir::Files, QDir::Name))
		{
			// font patches live beside the tables
			if (f.fileName().startsWith("u8fonts", Qt::CaseInsensitive))
				continue;
			if (!out.contains(f.canonicalFilePath()))
				out << f.canonicalFilePath();
		}
	return out;
}

QLineEdit *TranslateDialog::pathRow(const QString &label, bool folder, QFormLayout *form)
{
	auto *row = new QWidget;
	auto *h = new QHBoxLayout(row);
	h->setContentsMargins(0, 0, 0, 0);
	auto *edit = new QLineEdit;
	edit->setMinimumWidth(420);
	auto *browse = new QPushButton(tr("Choose..."));
	h->addWidget(edit, 1);
	h->addWidget(browse);
	form->addRow(label, row);
	connect(browse, &QPushButton::clicked, this, [this, edit, folder, label] {
		const QString start = edit->text().isEmpty() ? QDir::homePath() : edit->text();
		const QString path = folder
			? QFileDialog::getExistingDirectory(this, label, start)
			: QFileDialog::getOpenFileName(this, label, start, tr("Translation tables (*.json)"));
		if (!path.isEmpty())
			edit->setText(QDir::toNativeSeparators(path));
	});
	return edit;
}

TranslateDialog::TranslateDialog(const QString &englishDir, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Apply a Translation"));
	auto *v = new QVBoxLayout(this);
	auto *intro = new QLabel(tr(
		"<p>Makes a new game folder in another language: a copy of the English folder with the "
		"dialogue, books, signs and item names translated, and the letters the translation needs "
		"added to the fonts. The English folder is not changed. Buttons, menus and the intro stay "
		"in English.</p>"));
	intro->setWordWrap(true);
	v->addWidget(intro);

	auto *form = new QFormLayout;
	table_ = pathRow(tr("Translation table:"), false, form);
	about_ = new QLabel;
	form->addRow(QString(), about_);
	english_ = pathRow(tr("English game folder:"), true, form);
	output_ = pathRow(tr("New folder:"), true, form);
	v->addLayout(form);

	removeVoices_ = new QCheckBox(tr("Leave out the English voices (recommended)"));
	removeVoices_->setChecked(true);
	removeVoices_->setToolTip(tr(
		"The nine voiced characters (the Titans, the Guardian and a few others) speak only when "
		"the text matches the English recording, and show no text while they do. Without the "
		"voices, their lines show as translated text, as in Origin's French and German versions."));
	v->addWidget(removeVoices_);

	status_ = new QLabel;
	status_->setWordWrap(true);
	v->addWidget(status_);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
	applyButton_ = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
	v->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(applyButton_, &QPushButton::clicked, this, &TranslateDialog::apply);

	connect(table_, &QLineEdit::textChanged, this, &TranslateDialog::tableChanged);
	connect(english_, &QLineEdit::textChanged, this, [this] {
		// Suggest the new folder beside the English one, until one is typed.
		if (output_->isModified() || suggestedFolder_.isEmpty() || english_->text().isEmpty())
			return;
		QDir parent(QDir::fromNativeSeparators(english_->text()));
		parent.cdUp();
		output_->setText(QDir::toNativeSeparators(parent.filePath(suggestedFolder_)));
	});

	english_->setText(QDir::toNativeSeparators(englishDir));
	const QStringList tables = bundledTables();
	if (!tables.isEmpty())
		table_->setText(QDir::toNativeSeparators(tables.first()));
}

void TranslateDialog::tableChanged()
{
	TranslationTable t;
	QString error;
	if (table_->text().isEmpty() || !t.load(QDir::fromNativeSeparators(table_->text()), &error))
	{
		about_->setText(table_->text().isEmpty() ? QString() : error);
		applyButton_->setEnabled(false);
		return;
	}
	about_->setText(tr("%1: %2 of %3 strings translated.")
		.arg(t.language.isEmpty() ? tr("This table") : t.language)
		.arg(QLocale().toString(t.translated), QLocale().toString(t.count)));
	applyButton_->setEnabled(true);
	suggestedFolder_ = t.folder.isEmpty() ? QStringLiteral("TRANSLATED") : t.folder;
	if (!output_->isModified() && !english_->text().isEmpty())
	{
		QDir parent(QDir::fromNativeSeparators(english_->text()));
		parent.cdUp();
		output_->setText(QDir::toNativeSeparators(parent.filePath(suggestedFolder_)));
	}
}

void TranslateDialog::apply()
{
	Language::Options o;
	o.tablePath = QDir::fromNativeSeparators(table_->text());
	o.englishDir = QDir::fromNativeSeparators(english_->text());
	o.outputDir = QDir::fromNativeSeparators(output_->text());
	o.removeVoices = removeVoices_->isChecked();
	if (o.englishDir.isEmpty() || o.outputDir.isEmpty())
	{
		status_->setText(tr("Choose the English game folder and the new folder."));
		return;
	}
	setEnabled(false);
	QApplication::setOverrideCursor(Qt::WaitCursor);
	Language::Result r;
	QString error;
	const bool ok = Language::build(o, &r, &error, [this](const QString &step) {
		status_->setText(step + "...");
		QApplication::processEvents();
	});
	QApplication::restoreOverrideCursor();
	setEnabled(true);
	if (!ok)
	{
		status_->setText(QString());
		QMessageBox::warning(this, windowTitle(), error);
		return;
	}
	made_ = o.outputDir;
	QString text = tr("<p>Made <b>%1</b>.</p><p>%2 strings translated; %3 left as they were.")
		.arg(QDir::toNativeSeparators(o.outputDir).toHtmlEscaped())
		.arg(QLocale().toString(r.translated)).arg(QLocale().toString(r.untranslated));
	if (o.removeVoices)
		text += tr(" %1 voice files left out.").arg(r.voicesRemoved);
	text += "</p>";
	for (const QString &w : r.warnings)
		text += "<p>" + w.toHtmlEscaped() + "</p>";
	text += tr("<p>Play it by pointing the port at this folder (<tt>Ultima8 --data</tt>), or in DOSBox "
		"as the English one.</p>");
	QMessageBox::information(this, windowTitle(), text);
	accept();
}
