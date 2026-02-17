#ifndef MARQUEELABEL_H
#define MARQUEELABEL_H

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QStaticText> // Добавили этот инцилюд

class MarqueeLabel : public QWidget {
    Q_OBJECT
public:
    explicit MarqueeLabel(QWidget *parent = nullptr)
        : QWidget(parent), m_offset(0) {

        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &MarqueeLabel::scrollText);
        m_timer->start(20);
        setAttribute(Qt::WA_OpaquePaintEvent);
    }

    void setText(const QString &text) {
        // 1. Устанавливаем текст
        m_staticText.setText(text);

        // 2. ОТКЛЮЧАЕМ перенос по строкам (ставим бесконечную ширину)
        m_staticText.setTextWidth(-1);

        // 3. Подготавливаем текст и считаем ширину
        m_staticText.prepare(QTransform(), font());
        m_textWidth = m_staticText.size().width();

        m_offset = width();
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(this);
        painter.eraseRect(rect());

        if (m_staticText.text().isEmpty()) return;

        painter.setRenderHint(QPainter::TextAntialiasing);

        // Центрируем текст по вертикали
        int yPos = (height() - m_staticText.size().height()) / 2;

        // Рисуем статический текст (он отрисует HTML с цветами)
        painter.drawStaticText(m_offset, yPos, m_staticText);
    }

private slots:
    void scrollText() {
        if (m_staticText.text().isEmpty()) return;

        m_offset -= 1;
        if (m_offset < -m_textWidth) {
            m_offset = width();
        }
        repaint();
    }

private:
    QStaticText m_staticText; // Заменили QString на QStaticText
    int m_offset;
    int m_textWidth;
    QTimer *m_timer;
};

#endif

