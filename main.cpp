#include "mainwindow.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QStandardPaths>

const QString LOG_FOLDER = QCoreApplication::applicationDirPath();
const QString LOG_FILE_PATH = LOG_FOLDER + "/ReplaceX.log.txt";

void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);

    QFile logFile(LOG_FILE_PATH);
    if (!logFile.open(QFile::Append | QFile::Text))
        return;

    QTextStream out(&logFile);
    out << QDateTime::currentDateTime().toString("[dd.MM.yyyy hh:mm:ss] ");

    switch (type) {
    case QtDebugMsg: out << "[DEBUG] " << msg; break;
    case QtInfoMsg: out << "[INFO] " << msg; break;
    case QtWarningMsg: out << "[WARN] " << msg; break;
    case QtCriticalMsg: out << "[CRITICAL] " << msg; break;
    case QtFatalMsg: out << "[FATAL] " << msg; break;
    }

    out << "\n";
    out.flush();
    logFile.close();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qInstallMessageHandler(customMessageHandler);

    QCoreApplication::setOrganizationName("Replace X");
    QCoreApplication::setApplicationName("MyApp");

    MainWindow w;
    w.show();
    return a.exec();
}
