#ifndef MODERNBUTTON_H
#define MODERNBUTTON_H

#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QStyleOptionButton>

class ModernButton : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(double hoverAlpha READ hoverAlpha WRITE setHoverAlpha)

public:
    explicit ModernButton(QWidget *parent = nullptr) : QPushButton(parent), m_hoverAlpha(0.0) {
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(350);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, [this](const QVariant &v) {
            m_hoverAlpha = v.toDouble();
            update();
        });
    }

    // --- НОВАЯ ФУНКЦИЯ ДЛЯ УСТАНОВКИ КАРТИНКИ ---
    void setImagePath(const QString &path) {
        m_imagePath = path;
        update(); // Перерисовываем, чтобы подгрузить новую картинку
    }

    double hoverAlpha() const { return m_hoverAlpha; }
    void setHoverAlpha(double a) { m_hoverAlpha = a; update(); }

protected:
    void enterEvent(QEnterEvent *e) override {
        m_anim->setStartValue(m_hoverAlpha);
        m_anim->setEndValue(1.0);
        m_anim->start();
        QPushButton::enterEvent(e);
    }

    void leaveEvent(QEvent *e) override {
        m_anim->setStartValue(m_hoverAlpha);
        m_anim->setEndValue(0.0);
        m_anim->start();
        QPushButton::leaveEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 1. Рисуем градиент из CSS
        QStyleOptionButton opt;
        initStyleOption(&opt);
        style()->drawControl(QStyle::CE_PushButtonBevel, &opt, &p, this);

        // 2. Рисуем картинку (если путь задан и мышка наведена)
        if (m_hoverAlpha > 0 && !m_imagePath.isEmpty()) {
            p.setOpacity(m_hoverAlpha);
            QPixmap pix(m_imagePath); // Используем переменную вместо текста в кавычках

            if (!pix.isNull()) {
                QPainterPath path;
                path.addRoundedRect(rect(), 20, 20);
                p.setClipPath(path);
                p.drawPixmap(rect(), pix.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            }
        }

        // 3. Рисуем текст
        p.setOpacity(1.0);
        p.setPen(Qt::white);
        p.setFont(font());
        p.drawText(rect(), Qt::AlignCenter, text());
    }

private:
    double m_hoverAlpha;
    QVariantAnimation *m_anim;
    QString m_imagePath; // Переменная для хранения пути к картинке
};

#endif // MODERNBUTTON_H
