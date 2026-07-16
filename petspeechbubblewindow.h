#ifndef PETSPEECHBUBBLEWINDOW_H
#define PETSPEECHBUBBLEWINDOW_H

#include <QWidget>

class QLabel;
class QMouseEvent;
class QPaintEvent;
class QTimer;

class PetSpeechBubbleWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PetSpeechBubbleWindow(QWidget *parent = nullptr);

    void showMessage(const QString &text);
    void hideBubble();
    void updateBubblePosition(const QRect &petGeometry);
    QString fullText() const;

signals:
    void bubbleClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString shortenedText(const QString &text) const;
    QRect currentScreenAvailableGeometry(const QRect &petGeometry) const;

    QLabel *m_label {nullptr};
    QTimer *m_hideTimer {nullptr};
    QRect m_lastPetGeometry;
    QString m_fullText;
};

#endif // PETSPEECHBUBBLEWINDOW_H
