#include "iconprovider.h"

#include "themeconstants.h"

#include <QAbstractButton>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {
void drawIcon(QPainter &painter, AppIcon name, const QRectF &r)
{
    const qreal x = r.x();
    const qreal y = r.y();
    const qreal w = r.width();
    const qreal h = r.height();
    QPainterPath path;

    switch (name) {
    case AppIcon::Home:
        path.moveTo(x + w * .12, y + h * .48);
        path.lineTo(x + w * .5, y + h * .16);
        path.lineTo(x + w * .88, y + h * .48);
        path.moveTo(x + w * .23, y + h * .42);
        path.lineTo(x + w * .23, y + h * .84);
        path.lineTo(x + w * .77, y + h * .84);
        path.lineTo(x + w * .77, y + h * .42);
        break;
    case AppIcon::Pet:
        painter.drawEllipse(QRectF(x + w * .2, y + h * .28, w * .6, h * .56));
        painter.drawLine(QPointF(x + w * .28, y + h * .35), QPointF(x + w * .2, y + h * .12));
        painter.drawLine(QPointF(x + w * .72, y + h * .35), QPointF(x + w * .8, y + h * .12));
        painter.drawPoint(QPointF(x + w * .4, y + h * .53));
        painter.drawPoint(QPointF(x + w * .6, y + h * .53));
        return;
    case AppIcon::Motion:
        path.moveTo(x + w * .08, y + h * .62);
        path.cubicTo(x + w * .25, y + h * .18, x + w * .42, y + h * .85, x + w * .58, y + h * .42);
        path.cubicTo(x + w * .68, y + h * .18, x + w * .78, y + h * .42, x + w * .92, y + h * .2);
        break;
    case AppIcon::Chat:
        painter.drawRoundedRect(QRectF(x + w * .12, y + h * .16, w * .76, h * .56), 3, 3);
        painter.drawLine(QPointF(x + w * .35, y + h * .72), QPointF(x + w * .25, y + h * .88));
        painter.drawLine(QPointF(x + w * .35, y + h * .72), QPointF(x + w * .48, y + h * .72));
        return;
    case AppIcon::Role:
        painter.drawEllipse(QRectF(x + w * .34, y + h * .1, w * .32, h * .32));
        painter.drawArc(QRectF(x + w * .16, y + h * .38, w * .68, h * .52), 0, 180 * 16);
        return;
    case AppIcon::Cloud:
        path.moveTo(x + w * .22, y + h * .72);
        path.cubicTo(x + w * .03, y + h * .67, x + w * .12, y + h * .42, x + w * .3, y + h * .44);
        path.cubicTo(x + w * .35, y + h * .15, x + w * .73, y + h * .19, x + w * .75, y + h * .48);
        path.cubicTo(x + w * .98, y + h * .45, x + w * .98, y + h * .72, x + w * .78, y + h * .72);
        path.closeSubpath();
        break;
    case AppIcon::Runtime:
        painter.drawEllipse(QRectF(x + w * .12, y + h * .12, w * .76, h * .76));
        painter.drawLine(QPointF(x + w * .5, y + h * .5), QPointF(x + w * .5, y + h * .26));
        painter.drawLine(QPointF(x + w * .5, y + h * .5), QPointF(x + w * .69, y + h * .62));
        return;
    case AppIcon::Tools:
        painter.drawLine(QPointF(x + w * .2, y + h * .8), QPointF(x + w * .78, y + h * .22));
        painter.drawEllipse(QRectF(x + w * .12, y + h * .66, w * .24, h * .24));
        painter.drawEllipse(QRectF(x + w * .64, y + h * .1, w * .24, h * .24));
        return;
    case AppIcon::Settings:
        painter.drawEllipse(QRectF(x + w * .18, y + h * .18, w * .64, h * .64));
        painter.drawEllipse(QRectF(x + w * .4, y + h * .4, w * .2, h * .2));
        for (int i = 0; i < 4; ++i) {
            const qreal px = i % 2 ? x + w * .82 : x + w * .18;
            const qreal py = i / 2 ? y + h * .82 : y + h * .18;
            painter.drawLine(QPointF(x + w * .5, y + h * .5), QPointF(px, py));
        }
        return;
    case AppIcon::Play:
        path.moveTo(x + w * .28, y + h * .16);
        path.lineTo(x + w * .82, y + h * .5);
        path.lineTo(x + w * .28, y + h * .84);
        path.closeSubpath();
        break;
    case AppIcon::Stop:
        painter.drawRoundedRect(QRectF(x + w * .2, y + h * .2, w * .6, h * .6), 2, 2);
        return;
    case AppIcon::Add:
        painter.drawLine(QPointF(x + w * .5, y + h * .16), QPointF(x + w * .5, y + h * .84));
        painter.drawLine(QPointF(x + w * .16, y + h * .5), QPointF(x + w * .84, y + h * .5));
        return;
    case AppIcon::Delete:
        painter.drawRoundedRect(QRectF(x + w * .25, y + h * .28, w * .5, h * .58), 2, 2);
        painter.drawLine(QPointF(x + w * .18, y + h * .24), QPointF(x + w * .82, y + h * .24));
        painter.drawLine(QPointF(x + w * .38, y + h * .14), QPointF(x + w * .62, y + h * .14));
        return;
    case AppIcon::Edit:
        painter.drawLine(QPointF(x + w * .2, y + h * .78), QPointF(x + w * .72, y + h * .26));
        painter.drawLine(QPointF(x + w * .2, y + h * .78), QPointF(x + w * .16, y + h * .86));
        painter.drawLine(QPointF(x + w * .72, y + h * .26), QPointF(x + w * .84, y + h * .14));
        return;
    case AppIcon::Refresh:
        painter.drawArc(QRectF(x + w * .14, y + h * .14, w * .72, h * .72), 35 * 16, 280 * 16);
        painter.drawLine(QPointF(x + w * .78, y + h * .18), QPointF(x + w * .88, y + h * .36));
        painter.drawLine(QPointF(x + w * .78, y + h * .18), QPointF(x + w * .6, y + h * .2));
        return;
    case AppIcon::Import:
        painter.drawLine(QPointF(x + w * .5, y + h * .12), QPointF(x + w * .5, y + h * .62));
        painter.drawLine(QPointF(x + w * .3, y + h * .42), QPointF(x + w * .5, y + h * .62));
        painter.drawLine(QPointF(x + w * .7, y + h * .42), QPointF(x + w * .5, y + h * .62));
        painter.drawLine(QPointF(x + w * .18, y + h * .82), QPointF(x + w * .82, y + h * .82));
        return;
    case AppIcon::Copy:
        painter.drawRoundedRect(QRectF(x + w * .28, y + h * .26, w * .54, h * .58), 2, 2);
        painter.drawRoundedRect(QRectF(x + w * .14, y + h * .12, w * .54, h * .58), 2, 2);
        return;
    case AppIcon::Locate:
        painter.drawEllipse(QRectF(x + w * .14, y + h * .14, w * .72, h * .72));
        painter.drawEllipse(QRectF(x + w * .4, y + h * .4, w * .2, h * .2));
        painter.drawLine(QPointF(x + w * .5, y + h * .04), QPointF(x + w * .5, y + h * .22));
        painter.drawLine(QPointF(x + w * .5, y + h * .78), QPointF(x + w * .5, y + h * .96));
        return;
    case AppIcon::Save:
        painter.drawRoundedRect(QRectF(x + w * .16, y + h * .12, w * .68, h * .76), 2, 2);
        painter.drawRect(QRectF(x + w * .28, y + h * .14, w * .36, h * .24));
        painter.drawEllipse(QRectF(x + w * .32, y + h * .5, w * .36, h * .28));
        return;
    case AppIcon::Test:
        painter.drawLine(QPointF(x + w * .42, y + h * .12), QPointF(x + w * .42, y + h * .38));
        painter.drawLine(QPointF(x + w * .58, y + h * .12), QPointF(x + w * .58, y + h * .38));
        path.moveTo(x + w * .3, y + h * .12);
        path.lineTo(x + w * .7, y + h * .12);
        path.moveTo(x + w * .42, y + h * .38);
        path.lineTo(x + w * .2, y + h * .82);
        path.lineTo(x + w * .8, y + h * .82);
        path.lineTo(x + w * .58, y + h * .38);
        break;
    }
    painter.drawPath(path);
}
}

QIcon IconProvider::icon(AppIcon name, int size)
{
    static QHash<quint64, QIcon> cache;
    const quint64 key = (static_cast<quint64>(static_cast<int>(name)) << 32) | static_cast<quint32>(size);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(QString::fromLatin1(ThemeConstants::PrimaryText)), qMax(1.4, size / 11.0),
             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    drawIcon(painter, name, QRectF(1.5, 1.5, size - 3.0, size - 3.0));
    painter.end();

    const QIcon value(pixmap);
    cache.insert(key, value);
    return value;
}

void IconProvider::apply(QAbstractButton *button, AppIcon name, int size)
{
    if (!button) return;
    button->setIcon(icon(name, size));
    button->setIconSize(QSize(size, size));
}
