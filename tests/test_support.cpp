#include "test_support.h"

#include <QFile>
#include <QImage>

QString TestSupport::writePng(const QString &path, const QSize &size, const QColor &color)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    for (int y = size.height() / 3; y < size.height(); ++y) {
        for (int x = size.width() / 3; x < size.width() * 2 / 3; ++x) image.setPixelColor(x, y, color);
    }
    return image.save(path, "PNG") ? path : QString();
}

bool TestSupport::writeUtf8(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QByteArray TestSupport::readBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
