#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <cstdio> // Для fprintf
#include <QStringList>
#include <QPalette> // Добавлено для принудительной настройки темы
#include <QColor>   // Добавлено

// Функция логирования
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);

    // 1. Формируем строку лога
    QString timestamp = QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss");
    QString typeStr;
    switch (type) {
    case QtDebugMsg:    typeStr = "[DEBUG]"; break;
    case QtInfoMsg:     typeStr = "[INFO ]"; break;
    case QtWarningMsg:  typeStr = "[WARN ]"; break;
    case QtCriticalMsg: typeStr = "[CRIT ]"; break;
    case QtFatalMsg:    typeStr = "[FATAL]"; break;
    }

    QString fullMessage = QString("%1 %2: %3").arg(timestamp, typeStr, msg);

    // 2. ЗАПИСЬ В ФАЙЛ
    QFile logFile(QCoreApplication::applicationDirPath() + "/ReplaceX.log");
    if (logFile.open(QFile::Append | QFile::Text)) {
        QTextStream out(&logFile);
        out << fullMessage << Qt::endl;
        logFile.close();
    }

    // 3. ВЫВОД В КОНСОЛЬ
    fprintf(stderr, "%s\n", fullMessage.toLocal8Bit().constData());
    fflush(stderr); // Очистка буфера для мгновенного отображения
}

int main(int argc, char *argv[])
{
    // ОБЯЗАТЕЛЬНО: Регистрируем обработчик ДО создания QApplication
    qInstallMessageHandler(customMessageHandler);

    QApplication a(argc, argv);
    QFile logFile(QCoreApplication::applicationDirPath() + "/ReplaceX.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logFile.close();
    }

    // Устанавливаем стиль Fusion
    a.setStyle("Fusion");

    // НАСТРОЙКА ПРИНУДИТЕЛЬНОЙ ТЕМНОЙ ПАЛИТРЫ (Игнорирует светлую тему Windows)
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(30, 30, 30));          // Глубокий темный цвет для основного фона окон
    darkPalette.setColor(QPalette::WindowText, Qt::white);               // Цвет текста на окнах
    darkPalette.setColor(QPalette::Base, QColor(48, 48, 48));            // ИЗМЕНЕНО: Поля ввода и чекбоксы стали заметно светлее фона
    darkPalette.setColor(QPalette::AlternateBase, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);                     // Основной цвет текста
    darkPalette.setColor(QPalette::Button, QColor(45, 45, 45));          // ИЗМЕНЕНО: Стандартные кнопки тоже стали чуть светлее
    darkPalette.setColor(QPalette::ButtonText, Qt::white);               // Текст на кнопках
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(233, 119, 91));          // Цвет ссылок (#e9775b)
    darkPalette.setColor(QPalette::Highlight, QColor(233, 119, 91));     // Цвет выделения элементов (#e9775b)
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);          // Текст выделенного элемента
    darkPalette.setColor(QPalette::Light, QColor(48, 48, 48));           // Рамки списков в тон полей ввода

    // Настройка принудительной прозрачности/цветов для неактивных (выключенных) элементов
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));

    // Применяем палитру ко всему приложению глобально
    a.setPalette(darkPalette);

    a.setWindowIcon(QIcon(":/izobr/IconG.ico"));

    QCoreApplication::setOrganizationName("Replace X");
    QCoreApplication::setApplicationName("MyApp");

    MainWindow w;
    bool startMinimized = false;
    QStringList args = QCoreApplication::arguments();

    // Ищем наш флаг --autostart, который мы прописали в реестре
    if (args.contains("--autostart")) {
        startMinimized = true;
    }

    if (startMinimized) {
        w.hide();
    } else {
        w.show();
    }
    return a.exec();
}
