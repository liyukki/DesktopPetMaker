#include "petspeechbubblewindow.h"
#include "ui/theme/themeconstants.h"

#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kBubbleTextWidth = 232;
constexpr int kMaxBubbleLines = 4;
constexpr int kBubbleMargin = 10;
constexpr int kBubbleGap = 8;
constexpr int kAutoHideMs = 10 * 1000;
}

PetSpeechBubbleWindow::PetSpeechBubbleWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(0);

    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_label->setTextFormat(Qt::PlainText);
    m_label->setTextInteractionFlags(Qt::NoTextInteraction);
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QPalette labelPalette = m_label->palette();
    labelPalette.setColor(QPalette::WindowText, QColor(QString::fromLatin1(ThemeConstants::PrimaryText)));
    m_label->setPalette(labelPalette);
    QFont font = m_label->font();
    font.setPointSize(qMax(9, font.pointSize()));
    m_label->setFont(font);
    layout->addWidget(m_label);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &PetSpeechBubbleWindow::hideBubble);

    resize(260, 86);
    hide();
}

void PetSpeechBubbleWindow::showMessage(const QString &text)
{
    m_fullText = text;
    m_label->setText(shortenedText(m_fullText));
    m_label->setMaximumWidth(kBubbleTextWidth);
    m_label->setMaximumHeight(m_label->fontMetrics().lineSpacing() * kMaxBubbleLines + 4);
    adjustSize();
    resize(qMin(width(), 280), qMin(height(), 104));
    updateBubblePosition(m_lastPetGeometry);
    show();
    raise();
    m_hideTimer->start(kAutoHideMs);
}

QString PetSpeechBubbleWindow::fullText() const
{
    return m_fullText;
}

void PetSpeechBubbleWindow::hideBubble()
{
    if (m_hideTimer) {
        m_hideTimer->stop();
    }
    hide();
}

void PetSpeechBubbleWindow::updateBubblePosition(const QRect &petGeometry)
{
    if (!petGeometry.isValid()) {
        return;
    }
    m_lastPetGeometry = petGeometry;

    const QRect screen = currentScreenAvailableGeometry(petGeometry);
    const QSize bubbleSize = sizeHint().expandedTo(QSize(180, 56)).boundedTo(QSize(280, 104));
    resize(bubbleSize);

    QPoint pos(petGeometry.right() + kBubbleGap, petGeometry.top() - height() / 4);
    if (pos.x() + width() > screen.right()) {
        pos.setX(petGeometry.left() - width() - kBubbleGap);
    }
    if (pos.y() < screen.top()) {
        pos.setY(petGeometry.top() + kBubbleGap);
        if (pos.x() < screen.left() || pos.x() + width() > screen.right()) {
            pos.setX(petGeometry.right() + kBubbleGap);
        }
    }

    pos.setX(qBound(screen.left() + kBubbleMargin, pos.x(), screen.right() - width() - kBubbleMargin + 1));
    pos.setY(qBound(screen.top() + kBubbleMargin, pos.y(), screen.bottom() - height() - kBubbleMargin + 1));
    move(pos);
}

void PetSpeechBubbleWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit bubbleClicked();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

QString PetSpeechBubbleWindow::shortenedText(const QString &text) const
{
    QString normalized = text.simplified();
    if (normalized.isEmpty()) {
        return normalized;
    }

    const QFontMetrics metrics(m_label ? m_label->font() : font());
    const QString ellipsis = QString::fromUtf8("\xE2\x80\xA6");
    QStringList lines;
    QString current;
    bool truncated = false;

    for (int i = 0; i < normalized.size(); ++i) {
        const QChar ch = normalized.at(i);
        const QString candidate = current + ch;
        if (!current.isEmpty() && metrics.horizontalAdvance(candidate) > kBubbleTextWidth) {
            lines.append(current.trimmed());
            current.clear();
            if (lines.size() >= kMaxBubbleLines) {
                truncated = true;
                break;
            }
        }
        current.append(ch);
    }

    if (!truncated && !current.isEmpty()) {
        lines.append(current.trimmed());
    }

    if (lines.size() > kMaxBubbleLines) {
        lines = lines.mid(0, kMaxBubbleLines);
        truncated = true;
    }
    if (truncated || lines.join(QString()).size() < normalized.size()) {
        while (!lines.isEmpty()
               && metrics.horizontalAdvance(lines.last() + ellipsis) > kBubbleTextWidth
               && !lines.last().isEmpty()) {
            lines.last().chop(1);
        }
        if (lines.isEmpty()) {
            lines.append(ellipsis);
        } else {
            lines.last() = lines.last().trimmed() + ellipsis;
        }
    }

    return lines.join(QLatin1Char('\n'));
}

QRect PetSpeechBubbleWindow::currentScreenAvailableGeometry(const QRect &petGeometry) const
{
    QScreen *screen = QGuiApplication::screenAt(petGeometry.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    return screen ? screen->availableGeometry() : QRect(0, 0, 1536, 912);
}

void PetSpeechBubbleWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bubbleRect = rect().adjusted(2, 2, -2, -3);
    QPainterPath path;
    path.addRoundedRect(bubbleRect, 10, 10);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(24, 52, 59, 24));
    painter.drawRoundedRect(bubbleRect.translated(0, 2), 10, 10);
    painter.fillPath(path, QColor(255, 255, 255, 246));
    painter.setPen(QPen(QColor(QString::fromLatin1(ThemeConstants::Border)), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.setPen(QPen(QColor(QString::fromLatin1(ThemeConstants::Accent)), 3, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(14, 3), QPointF(width() - 14, 3));
}
