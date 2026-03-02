#ifndef FILEWORKER_H
#define FILEWORKER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <windows.h>
#include <tlhelp32.h>
#include <QSoundEffect>

class FileWorker : public QObject
{
    Q_OBJECT
public:
    explicit FileWorker(QObject *parent = nullptr) : QObject(parent) {}

    struct Config {
        QString reduxPath;
        QString originalPath;
        QString gameUpdatePath;

        QString gunPackSource;
        QString dlcPacksTarget;
        QString backupPath;

        QString soundModPath;
        QString sfxPath;
        QString soundBackupPath;

        bool useRedux;
        bool useGunPack;
        bool useSounds;
    };

signals:
    void progressMessage(QString message);
    void operationFinished(bool success, QString details);
    void statusUpdate(QString status);

public slots:
    // Автоматические задачи
    void processInstallation(FileWorker::Config config);
    void processRestoration(FileWorker::Config config);

    // РУЧНЫЕ ЗАДАЧИ (Вынесены из MainWindow)
    void manualSmartReplace(const QString &source, const QString &targetDir, const QString &targetFileName);
    void manualRestoreGunPacks(FileWorker::Config config);
    void manualReplaceSounds(FileWorker::Config config);
    void manualRestoreSounds(FileWorker::Config config);
    void manualInstallGunPacks(FileWorker::Config config);

private:
    // Внутренние методы (все используют WinAPI)
    bool smartReplace(const QString &source, const QString &targetDir, const QString &targetFileName);
    bool copyDirectory(const QString &sourceDir, const QString &targetDir);
    bool installGunPacks(const QString &source, const QString &target, const QString &backup);
    bool restoreGunPacks(const QString &target, const QString &backup);
    void killGtaEcosystem();

    // Вспомогательные методы для звуков
    bool internalReplaceSounds(const QString &modPath, const QString &sfxPath, const QString &backupPath);
    bool internalRestoreSounds(const QString &sfxPath, const QString &backupPath);
};

#endif // FILEWORKER_H
