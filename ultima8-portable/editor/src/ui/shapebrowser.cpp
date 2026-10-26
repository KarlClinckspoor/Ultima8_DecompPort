#include "ui/shapebrowser.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "data/world.h"

static const int IconSize = 56;

void ShapeModel::setWorld(const World *world)
{
	beginResetModel();
	world_ = world;
	icons_.clear();
	shapes_.clear();
	endResetModel();
	setFilter(QString(), -1);
}

// Text matches a shape's number exactly, or part of its usecode name or family.
void ShapeModel::setFilter(const QString &text, int family)
{
	beginResetModel();
	shapes_.clear();
	if (world_)
	{
		const Shapes &s = world_->shapes();
		bool isNumber = false;
		const int number = text.trimmed().toInt(&isNumber);
		for (int i = 1; i < s.count(); i++)
		{
			if (s.frameCount(i) == 0)
				continue;
			const TypeInfo &t = s.type(i);
			if (family >= 0 && t.family != family)
				continue;
			if (!text.trimmed().isEmpty())
			{
				const QString name = world_->usecode().className(i);
				const bool match = (isNumber && i == number) ||
					name.contains(text.trimmed(), Qt::CaseInsensitive) ||
					familyName(t.family).contains(text.trimmed(), Qt::CaseInsensitive);
				if (!match)
					continue;
			}
			shapes_ << i;
		}
	}
	endResetModel();
}

int ShapeModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : shapes_.size();
}

QVariant ShapeModel::data(const QModelIndex &index, int role) const
{
	const int shape = shapeAt(index.row());
	if (shape < 0 || !world_)
		return QVariant();
	switch (role)
	{
	case Qt::DisplayRole:
	{
		const QString name = world_->usecode().className(shape);
		return name.isEmpty() ? QString::number(shape) : QStringLiteral("%1\n%2").arg(shape).arg(name);
	}
	case Qt::ToolTipRole:
	{
		const TypeInfo &t = world_->shapes().type(shape);
		return QStringLiteral("Shape %1 %2\n%3, %4 frames, %5x%6x%7%8").arg(shape)
			.arg(world_->usecode().className(shape), familyName(t.family))
			.arg(world_->shapes().frameCount(shape)).arg(t.xSize * 32).arg(t.ySize * 32).arg(t.zSize * 8)
			.arg(t.fixed ? QStringLiteral(", fixed") : QString());
	}
	case Qt::DecorationRole:
	{
		auto it = icons_.find(shape);
		if (it != icons_.end())
			return *it;
		QPixmap pm(IconSize, IconSize);
		pm.fill(QColor(16, 16, 20));
		if (const Frame *f = world_->shapes().frame(shape, 0))
		{
			QPainter p(&pm);
			const qreal s = std::min<qreal>(1.0, std::min(IconSize / qreal(f->width), IconSize / qreal(f->height)));
			const QSizeF size = QSizeF(f->width, f->height) * s;
			p.setRenderHint(QPainter::SmoothPixmapTransform, s < 1);
			p.drawImage(QRectF(QPointF((IconSize - size.width()) / 2, (IconSize - size.height()) / 2), size), f->image);
		}
		icons_.insert(shape, pm);
		return pm;
	}
	}
	return QVariant();
}

ShapeBrowser::ShapeBrowser(QWidget *parent) : QWidget(parent)
{
	model_ = new ShapeModel(this);
	filter_ = new QLineEdit;
	filter_->setPlaceholderText(tr("Number, name or family"));
	filter_->setClearButtonEnabled(true);
	family_ = new QComboBox;
	family_->addItem(tr("All families"), -1);
	for (int f = 0; f <= 15; f++)
	{
		const QString name = familyName(f);
		if (!name.startsWith(QLatin1String("family")) || f == 15)
			family_->addItem(name, f);
	}
	list_ = new QListView;
	list_->setModel(model_);
	list_->setViewMode(QListView::IconMode);
	list_->setIconSize(QSize(IconSize, IconSize));
	list_->setGridSize(QSize(IconSize + 22, IconSize + 34));
	list_->setResizeMode(QListView::Adjust);
	list_->setMovement(QListView::Static);
	list_->setUniformItemSizes(true);
	list_->setWordWrap(true);
	list_->setSelectionMode(QAbstractItemView::SingleSelection);
	frame_ = new QSpinBox;
	frame_->setPrefix(tr("Frame "));
	preview_ = new QLabel;
	preview_->setFixedSize(96, 96);
	preview_->setAlignment(Qt::AlignCenter);
	preview_->setStyleSheet("background: #101014; border: 1px solid #333;");
	info_ = new QLabel;
	info_->setWordWrap(true);
	auto *place = new QPushButton(tr("Place"));
	place->setToolTip(tr("Then click on the map to put it there; right-click or Esc to stop. Double-click a shape does the same."));

	connect(filter_, &QLineEdit::textChanged, this, &ShapeBrowser::refilter);
	connect(family_, &QComboBox::currentIndexChanged, this, &ShapeBrowser::refilter);
	connect(list_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &i) {
		choose(model_->shapeAt(i.row()));
	});
	connect(list_, &QListView::doubleClicked, this, [this](const QModelIndex &i) {
		choose(model_->shapeAt(i.row()));
		if (shape_ >= 0)
			emit placeRequested(shape_, frame());
	});
	connect(frame_, &QSpinBox::valueChanged, this, &ShapeBrowser::updatePreview);
	connect(place, &QPushButton::clicked, this, [this] {
		if (shape_ >= 0)
			emit placeRequested(shape_, frame());
	});

	auto *top = new QHBoxLayout;
	top->addWidget(filter_, 1);
	top->addWidget(family_);
	auto *side = new QVBoxLayout;
	side->addWidget(frame_);
	side->addWidget(place);
	side->addStretch();
	auto *bottom = new QHBoxLayout;
	bottom->addWidget(preview_);
	bottom->addLayout(side);
	bottom->addWidget(info_, 1);
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(4, 4, 4, 4);
	v->addLayout(top);
	v->addWidget(list_, 1);
	v->addLayout(bottom);
	updatePreview();
}

void ShapeBrowser::setWorld(const World *world)
{
	world_ = world;
	model_->setWorld(world);
	refilter();
	choose(-1);
}

void ShapeBrowser::refilter()
{
	model_->setFilter(filter_->text(), family_->currentData().toInt());
	const int row = model_->rowOf(shape_);
	if (row >= 0)
		list_->setCurrentIndex(model_->index(row));
}

int ShapeBrowser::frame() const
{
	return frame_->value();
}

void ShapeBrowser::choose(int shape)
{
	if (shape == shape_)
		return;
	shape_ = shape;
	const int frames = world_ && shape >= 0 ? world_->shapes().frameCount(shape) : 1;
	frame_->setRange(0, std::max(0, frames - 1));
	updatePreview();
}

void ShapeBrowser::showShape(int shape, int frame)
{
	if (!world_)
		return;
	filter_->clear();
	family_->setCurrentIndex(0);
	choose(shape);
	frame_->setValue(frame);
	const int row = model_->rowOf(shape);
	if (row >= 0)
	{
		list_->setCurrentIndex(model_->index(row));
		list_->scrollTo(model_->index(row));
	}
}

void ShapeBrowser::updatePreview()
{
	QPixmap pic(preview_->size() - QSize(2, 2));
	pic.fill(QColor(16, 16, 20));
	if (!world_ || shape_ < 0)
	{
		preview_->setPixmap(pic);
		info_->setText(tr("Pick a shape."));
		return;
	}
	if (const Frame *f = world_->shapes().frame(shape_, frame()))
	{
		QPainter p(&pic);
		const qreal s = std::min<qreal>(2, std::min(pic.width() / qreal(f->width), pic.height() / qreal(f->height)));
		const QSizeF size = QSizeF(f->width, f->height) * s;
		p.setRenderHint(QPainter::SmoothPixmapTransform, s < 1);
		p.drawImage(QRectF(QPointF((pic.width() - size.width()) / 2, (pic.height() - size.height()) / 2), size), f->image);
	}
	preview_->setPixmap(pic);
	const TypeInfo &t = world_->shapes().type(shape_);
	const QString name = world_->usecode().className(shape_);
	info_->setText(tr("<b>%1</b> %2<br>%3%4<br>footpad %5 x %6 x %7")
		.arg(shape_).arg(name.toHtmlEscaped(), familyName(t.family), t.fixed ? tr(", fixed") : QString())
		.arg(t.worldX()).arg(t.worldY()).arg(t.worldZ()));
}
