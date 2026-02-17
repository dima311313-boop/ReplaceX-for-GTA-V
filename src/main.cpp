#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <cstdio> // Для fprintf

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
    // Используем AppDirPath, но лучше QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    QFile logFile(QCoreApplication::applicationDirPath() + "/ReplaceX.log.txt");
    if (logFile.open(QFile::Append | QFile::Text)) {
        QTextStream out(&logFile);
        out << fullMessage << Qt::endl;
        logFile.close();
    }

    // 3. ВЫВОД В КОНСОЛЬ (чтобы работало одновременно)
    // fprintf отправляет текст напрямую в консоль, минуя механизмы Qt
    fprintf(stderr, "%s\n", fullMessage.toLocal8Bit().constData());
    fflush(stderr); // Очистка буфера для мгновенного отображения
}

int main(int argc, char *argv[])
{
    // ОБЯЗАТЕЛЬНО: Регистрируем обработчик ДО создания QApplication
    qInstallMessageHandler(customMessageHandler);

    QApplication a(argc, argv);
    QFile logFile(QCoreApplication::applicationDirPath() + "/ReplaceX.log.txt");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logFile.close();
    }
    a.setStyle("Fusion");

    QCoreApplication::setOrganizationName("Replace X");
    QCoreApplication::setApplicationName("MyApp");

    MainWindow w;
    w.show();
    return a.exec();
}
