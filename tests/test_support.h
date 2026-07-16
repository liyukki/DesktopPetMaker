#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <QColor>
#include <QSize>
#include <QString>

namespace TestSupport {
QString writePng(const QString &path, const QSize &size, const QColor &color = Qt::red);
bool writeUtf8(const QString &path, const QByteArray &data);
QByteArray readBytes(const QString &path);
}

#endif // TEST_SUPPORT_H
