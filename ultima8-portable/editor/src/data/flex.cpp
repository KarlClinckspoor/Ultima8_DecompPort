#include "data/flex.h"

#include <QFile>
#include <QFileInfo>

static bool readWhole(const QString &path, QByteArray &out, QString *error)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
	{
		if (error)
			*error = QStringLiteral("Cannot open %1: %2").arg(path, f.errorString());
		return false;
	}
	out = f.readAll();
	return true;
}

bool Flex::load(const QString &path, QString *error)
{
	QByteArray bytes;
	if (!readWhole(path, bytes, error))
		return false;
	return setData(bytes, QFileInfo(path).fileName(), error);
}

bool Flex::setData(const QByteArray &data, const QString &name, QString *error)
{
	data_ = data;
	index_.clear();
	if (data_.size() < 0x80)
	{
		if (error)
			*error = QStringLiteral("%1 is too short to be a flex file").arg(name);
		return false;
	}
	uint32_t count = read32(data_.constData() + 0x54);
	if (count > 65536 || 0x80 + qint64(count) * 8 > data_.size())
	{
		if (error)
			*error = QStringLiteral("%1 is not a flex file").arg(name);
		return false;
	}
	index_.resize(count);
	for (uint32_t i = 0; i < count; i++)
	{
		const char *p = data_.constData() + 0x80 + i * 8;
		Entry e{read32(p), read32(p + 4)};
		if (qint64(e.offset) + e.size > data_.size())
			e = Entry{0, 0};
		index_[i] = e;
	}
	return true;
}

int Flex::size(int record) const
{
	if (record < 0 || record >= count())
		return 0;
	return int(index_[record].size);
}

const char *Flex::data(int record) const
{
	if (size(record) == 0)
		return nullptr;
	return data_.constData() + index_[record].offset;
}

QByteArray Flex::record(int record) const
{
	if (size(record) == 0)
		return QByteArray();
	return data_.mid(index_[record].offset, index_[record].size);
}

bool SaveFile::load(const QString &path, QString *error)
{
	QByteArray d;
	files_.clear();
	if (!readWhole(path, d, error))
		return false;
	static const char id[] = "Ultima 8 SaveGame File.";
	const int start = int(sizeof id);	// with its NUL
	auto bad = [&]() {
		if (error)
			*error = QStringLiteral("%1 is not an Ultima 8 saved game").arg(QFileInfo(path).fileName());
		return false;
	};
	if (d.size() < start + 2 || !d.startsWith(id))
		return bad();
	int at = start;
	int count = read16(d.constData() + at);
	at += 2;
	for (int i = 0; i < count; i++)
	{
		if (at + 4 > d.size())
			return bad();
		uint32_t nameLen = read32(d.constData() + at);
		at += 4;
		if (nameLen == 0 || at + qint64(nameLen) + 4 > d.size())
			return bad();
		QString name = QString::fromLatin1(d.constData() + at, int(nameLen) - 1);
		at += int(nameLen);
		uint32_t size = read32(d.constData() + at);
		at += 4;
		if (at + qint64(size) > d.size())
			return bad();
		files_.append({name, d.mid(at, int(size))});
		at += int(size);
	}
	return true;
}

bool SaveFile::has(const QString &name) const
{
	for (const auto &f : files_)
		if (f.first.compare(name, Qt::CaseInsensitive) == 0)
			return true;
	return false;
}

QByteArray SaveFile::file(const QString &name) const
{
	for (const auto &f : files_)
		if (f.first.compare(name, Qt::CaseInsensitive) == 0)
			return f.second;
	return QByteArray();
}

QByteArray SaveFile::build(const QVector<QPair<QString, QByteArray>> &files)
{
	QByteArray out("Ultima 8 SaveGame File.", 24);	// with its NUL
	auto put32 = [&](uint32_t v) {
		for (int i = 0; i < 4; i++)
			out.append(char((v >> (8 * i)) & 0xff));
	};
	out.append(char(files.size() & 0xff));
	out.append(char(files.size() >> 8));
	for (const auto &f : files)
	{
		QByteArray name = f.first.toLatin1();
		put32(uint32_t(name.size() + 1));
		out.append(name);
		out.append('\0');
		put32(uint32_t(f.second.size()));
		out.append(f.second);
	}
	return out;
}
