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
#include <shellapi.h> // ОБЯЗАТЕЛЬНО
#include <QObject>
#include <QString>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>



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

        QString armorSource;
        QString armorTarget;
        QString armorBackupPath;
        bool useArmor;

        // --- ДАННЫЕ ДЛЯ ЗАМЕНЕНОК (ZM) ---
        QString zmSource;
        QString zmTarget;
        QString zmBackupPath;
        bool useZM;
        // ---------------------------------

        bool useRedux;
        bool useGunPack;
        bool useSounds;
    };
signals:
    void unpackFinished(int type, QString resultPath);
    void debugLog(const QString &msg);
    void progressMessage(QString message);
    void progressValue(int value);
    void operationFinished(bool success, QString details);
    void statusUpdate(QString status);
    void extractionFinished(bool success, const QString &fullPath);
    void extractionStarted(); // To show "Extracting..." in UI

public slots:
    void manualRestoreZM(FileWorker::Config config);
    void manualInstallZM(FileWorker::Config config);
    void manualRestoreArmorPacks(FileWorker::Config config);
    void manualInstallArmorPacks(FileWorker::Config config);
    void processUnpack(int type, QString archivePath, QString originalUpdatePath);
    void processInstallation(FileWorker::Config config);
    void processRestoration(FileWorker::Config config);
    void manualSmartReplace(const QString &source, const QString &targetDir, const QString &targetFileName);
    void manualRestoreGunPacks(FileWorker::Config config);
    void manualReplaceSounds(FileWorker::Config config);
    void manualRestoreSounds(FileWorker::Config config);
    void manualInstallGunPacks(FileWorker::Config config);

private:
    bool winCopyPath(const QString &src, const QString &dst);
    bool winRemovePath(const QString &path);
    bool smartReplace(const QString &source, const QString &targetDir, const QString &targetFileName);
    bool installGunPacks(const QString &source, const QString &target, const QString &backup);
    bool restoreGunPacks(const QString &target, const QString &backup);
    void killGtaEcosystem();
    bool internalReplaceSounds(const QString &modPath, const QString &sfxPath, const QString &backupPath);
    bool internalRestoreSounds(const QString &sfxPath, const QString &backupPath);
};

#endif
