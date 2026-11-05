#include "ui/usecodeview.h"

#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "data/disasm.h"
#include "data/ucode.h"
#include "data/world.h"
#include "ui/names.h"

enum { ClassRole = Qt::UserRole, MapRole, ItemRole, OffsetRole };

QString UsecodeView::classRole(const World &world, int classId)
{
	if (classId >= UsecodeEggClassBase)
		return QObject::tr("usecode egg type %1").arg(classId - UsecodeEggClassBase);
	if (classId >= NpcClassBase)
		return QObject::tr("NPC %1's own class").arg(classId - NpcClassBase);
	const QString name = Names::shapeName(classId);
	return QObject::tr("the class of shape %1%2, %3").arg(classId)
		.arg(name.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(name), familyName(world.shapes().type(classId).family));
}

UsecodeView::UsecodeView(QWidget *parent) : QWidget(parent)
{
	find_ = new QLineEdit;
	find_->setPlaceholderText(tr("Class number or name"));
	find_->setClearButtonEnabled(true);
	connect(find_, &QLineEdit::textChanged, this, &UsecodeView::filter);
	classes_ = new QTreeWidget;
	classes_->setColumnCount(4);
	classes_->setHeaderLabels({tr("Class"), tr("Name"), tr("Events"), tr("Items")});
	classes_->setRootIsDecorated(false);
	classes_->setUniformRowHeights(true);
	classes_->setAlternatingRowColors(true);
	classes_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
	classes_->setToolTip(tr("Every class with code. Items: how many items on the maps run it"));
	connect(classes_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *it) {
		if (it && it->data(0, ClassRole).toInt() != class_)
			showClass(it->data(0, ClassRole).toInt());
	});
	auto *left = new QWidget;
	auto *lv = new QVBoxLayout(left);
	lv->setContentsMargins(0, 0, 0, 0);
	lv->addWidget(find_);
	lv->addWidget(classes_, 1);

	title_ = new QLabel;
	title_->setWordWrap(true);
	title_->setTextFormat(Qt::RichText);
	title_->setTextInteractionFlags(Qt::TextBrowserInteraction);
	connect(title_, &QLabel::linkActivated, this, [this](const QString &link) { openLink(QUrl(link)); });
	listing_ = new QTextBrowser;
	listing_->setOpenLinks(false);
	listing_->setLineWrapMode(QTextEdit::NoWrap);
	QFont mono(QStringLiteral("monospace"));
	mono.setStyleHint(QFont::TypeWriter);
	listing_->setFont(mono);
	connect(listing_, &QTextBrowser::anchorClicked, this, &UsecodeView::openLink);

	items_ = new QTreeWidget;
	items_->setColumnCount(3);
	items_->setHeaderLabels({tr("Item"), tr("Map"), tr("Position")});
	items_->setRootIsDecorated(false);
	items_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	items_->setToolTip(tr("The items on the maps that run this class; double-click to show one"));
	connect(items_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *it) {
		emit goToItem(it->data(0, MapRole).toInt(), it->data(0, ItemRole).toInt());
	});
	callers_ = new QTreeWidget;
	callers_->setColumnCount(2);
	callers_->setHeaderLabels({tr("Class"), tr("Routine")});
	callers_->setRootIsDecorated(false);
	callers_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	callers_->setToolTip(tr("Other classes that call or spawn this one's routines; double-click to go there"));
	connect(callers_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *it) {
		showClass(it->data(0, ClassRole).toInt(), -1, it->data(0, OffsetRole).toInt());
	});
	refs_ = new QTabWidget;
	refs_->addTab(items_, tr("Items"));
	refs_->addTab(callers_, tr("Called from"));

	auto *right = new QSplitter(Qt::Vertical);
	auto *top = new QWidget;
	auto *tv = new QVBoxLayout(top);
	tv->setContentsMargins(0, 0, 0, 0);
	tv->addWidget(title_);
	tv->addWidget(listing_, 1);
	right->addWidget(top);
	right->addWidget(refs_);
	right->setStretchFactor(0, 3);
	right->setStretchFactor(1, 1);

	auto *split = new QSplitter;
	split->addWidget(left);
	split->addWidget(right);
	split->setStretchFactor(1, 1);
	split->setSizes({300, 900});
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(4, 4, 4, 4);
	v->addWidget(split);
}

void UsecodeView::setWorld(const World *world)
{
	world_ = world;
	class_ = -1;
	usersStale_ = true;
	findUsers();
	fillClasses();
	listing_->clear();
	title_->clear();
	items_->clear();
	callers_->clear();
}

void UsecodeView::findUsers()
{
	if (!usersStale_)
		return;
	usersStale_ = false;
	users_.clear();
	if (!world_)
		return;
	for (int m : world_->mapNumbers())
	{
		const auto &items = world_->map(m).items;
		for (size_t i = 0; i < items.size(); i++)
			if (!items[i].deleted)
				users_[world_->classOf(items[i])].append({m, int(i)});
	}
}

void UsecodeView::fillClasses()
{
	classes_->clear();
	if (!world_)
		return;
	const Usecode &u = world_->usecode();
	QList<QTreeWidgetItem *> rows;
	for (int k = 0; k < u.classCount(); k++)
	{
		if (!u.hasClass(k))
			continue;
		QStringList events;
		for (int e = 0; e < Ucode::EventCount; e++)
			if (u.eventOffset(k, e))
				events << (Ucode::eventName(e).isEmpty() ? QString::number(e) : Ucode::eventName(e));
		auto *row = new QTreeWidgetItem({QString::number(k), u.className(k), events.join(QStringLiteral(", ")),
			users_.value(k).isEmpty() ? QString() : QString::number(users_.value(k).size())});
		row->setData(0, ClassRole, k);
		row->setToolTip(1, classRole(*world_, k));
		row->setTextAlignment(0, Qt::AlignRight | Qt::AlignVCenter);
		row->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
		rows << row;
	}
	classes_->addTopLevelItems(rows);
	for (int c : {0, 1, 3})
		classes_->resizeColumnToContents(c);
	filter(find_->text());
}

void UsecodeView::filter(const QString &text)
{
	const QString t = text.trimmed();
	bool number = false;
	const int n = t.toInt(&number, 0);
	for (int i = 0; i < classes_->topLevelItemCount(); i++)
	{
		QTreeWidgetItem *row = classes_->topLevelItem(i);
		const bool show = t.isEmpty() || (number ? row->data(0, ClassRole).toInt() == n
			: row->text(1).contains(t, Qt::CaseInsensitive) || row->text(2).contains(t, Qt::CaseInsensitive));
		row->setHidden(!show);
	}
}

static QString esc(const QString &s)
{
	return s.toHtmlEscaped();
}

void UsecodeView::showClass(int classId, int event, int offset)
{
	if (!world_)
		return;
	const Usecode &u = world_->usecode();
	if (usersStale_)
	{
		findUsers();
		class_ = -1;	// the item list is made again
	}
	if (classId != class_)
	{
		class_ = classId;
		QVector<Ucode::Line> lines;
		QString error;
		QString html = QStringLiteral("<html><body style='white-space:pre'>");
		if (!Ucode::disassemble(u, classId, &lines, &error))
			html += esc(error);
		for (const Ucode::Line &l : lines)
		{
			if (l.kind == Ucode::Line::Head)
			{
				const QString what = l.event >= 0
					? tr("event %1: %2").arg(l.event).arg(Ucode::eventName(l.event).isEmpty() ? tr("(never sent)") : Ucode::eventName(l.event))
					: tr("routine %1").arg(l.at, 4, 16, QLatin1Char('0'));
				html += QStringLiteral("<a name='o%1'></a>%2<b style='color:#e8c860'>%3</b>\n")
					.arg(l.at).arg(l.event >= 0 ? QStringLiteral("<a name='e%1'></a>").arg(l.event) : QString(), esc(what));
				continue;
			}
			QString text = QStringLiteral("<a name='o%1'></a><span style='color:#7a8492'>%2</span>  %3")
				.arg(l.at).arg(l.at, 4, 16, QLatin1Char('0')).arg(esc(l.op).leftJustified(21, QLatin1Char(' ')) + QLatin1Char(' '));
			if (!l.args.isEmpty())
				text += esc(l.args) + QLatin1Char(' ');
			if (l.jump >= 0)
				text += QStringLiteral("<a href='go:%1'>%2</a> ").arg(l.jump).arg(l.jump, 4, 16, QLatin1Char('0'));
			if (l.callClass >= 0)
				text += QStringLiteral("<a href='class:%1:%2'>%3:%4</a> ").arg(l.callClass).arg(l.callOffset)
					.arg(l.callClass).arg(l.callOffset, 4, 16, QLatin1Char('0'));
			if (l.flagBit >= 0)
				text += QStringLiteral("<a href='flag:%1'>%2</a> ").arg(l.flagBit).arg(tr("(flag)"));
			if (!l.note.isEmpty())
				text += QStringLiteral("<span style='color:%1'>%2</span>")
					.arg(l.op.contains(QLatin1String("string")) ? QStringLiteral("#8fd18f") : QStringLiteral("#8fb8ff"), esc(l.note));
			html += text + QLatin1Char('\n');
		}
		html += QStringLiteral("</body></html>");
		listing_->setHtml(html);

		QStringList events;
		for (int e = 0; e < Ucode::EventCount; e++)
			if (u.eventOffset(classId, e))
				events << QStringLiteral("<a href='event:%1'>%2</a>").arg(e)
					.arg(Ucode::eventName(e).isEmpty() ? QString::number(e) : Ucode::eventName(e));
		const QString name = u.className(classId);
		title_->setText(QStringLiteral("<b>%1</b> &mdash; %2<br>%3")
			.arg(esc(name.isEmpty() ? tr("class %1").arg(classId) : QStringLiteral("%1 (%2)").arg(name).arg(classId)),
				esc(classRole(*world_, classId)),
				events.isEmpty() ? tr("No events.") : tr("Events: %1").arg(events.join(QStringLiteral(", ")))));

		items_->clear();
		QList<QTreeWidgetItem *> rows;
		const auto users = users_.value(classId);
		for (int k = 0; k < users.size() && k < 2000; k++)
		{
			const int m = users[k].first, i = users[k].second;
			const MapItem &it = world_->map(m).items[i];
			if (it.deleted)
				continue;
			const MapData &d = world_->map(m);
			const MapItem &r = d.items[d.root(i)];
			auto *row = new QTreeWidgetItem({Names::itemName(*world_, m, it), Names::mapShort(m),
				QStringLiteral("%1, %2, %3").arg(r.x).arg(r.y).arg(r.z)});
			row->setData(0, MapRole, m);
			row->setData(0, ItemRole, i);
			row->setToolTip(0, Names::itemTooltip(*world_, m, i));
			rows << row;
		}
		items_->addTopLevelItems(rows);
		refs_->setTabText(0, tr("Items (%1)").arg(users.size()));

		callers_->clear();
		const auto calls = Ucode::callers(u, classId);
		for (const auto &c : calls)
		{
			const QString n = u.className(c.first);
			auto *row = new QTreeWidgetItem({n.isEmpty() ? QString::number(c.first) : QStringLiteral("%1 (%2)").arg(n).arg(c.first),
				u.routineAt(c.first, c.second)});
			row->setData(0, ClassRole, c.first);
			row->setData(0, OffsetRole, c.second);
			callers_->addTopLevelItem(row);
		}
		refs_->setTabText(1, tr("Called from (%1)").arg(calls.size()));

		for (int i = 0; i < classes_->topLevelItemCount(); i++)
			if (classes_->topLevelItem(i)->data(0, ClassRole).toInt() == classId)
			{
				const QSignalBlocker hold(classes_);
				classes_->setCurrentItem(classes_->topLevelItem(i));
				classes_->scrollToItem(classes_->topLevelItem(i));
			}
	}
	if (offset >= 0)
		listing_->scrollToAnchor(QStringLiteral("o%1").arg(offset));
	else if (event >= 0)
		listing_->scrollToAnchor(QStringLiteral("e%1").arg(event));
	else
		listing_->moveCursor(QTextCursor::Start);
}

void UsecodeView::openLink(const QUrl &url)
{
	const QString s = url.toString();
	const QStringList parts = s.split(QLatin1Char(':'));
	if (parts.size() >= 2 && parts[0] == QLatin1String("go"))
		listing_->scrollToAnchor(QStringLiteral("o%1").arg(parts[1]));
	else if (parts.size() >= 3 && parts[0] == QLatin1String("class"))
		showClass(parts[1].toInt(), -1, parts[2].toInt());
	else if (parts.size() >= 2 && parts[0] == QLatin1String("event"))
		listing_->scrollToAnchor(QStringLiteral("e%1").arg(parts[1]));
	else if (parts.size() >= 2 && parts[0] == QLatin1String("flag"))
		emit showFlag(parts[1].toInt());
}
