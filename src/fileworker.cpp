#include "fileworker.h"
#include <QDebug>

void FileWorker::processInstallation(FileWorker::Config config) {
    emit statusUpdate("Установка модов...");
    bool overallSuccess = true;

    if (config.useRedux && !config.reduxPath.isEmpty()) {
        emit progressMessage("Установка Redux...");
        if (!smartReplace(config.reduxPath, config.gameUpdatePath, "update.rpf")) overallSuccess = false;
    }

    if (config.useGunPack && !config.gunPackSource.isEmpty()) {
        emit progressMessage("Установка GunPacks...");
        if (!installGunPacks(config.gunPackSource, config.dlcPacksTarget, config.backupPath)) overallSuccess = false;
    }

    if (config.useSounds && !config.soundModPath.isEmpty()) {
        emit progressMessage("Установка звуков...");
        if (!internalReplaceSounds(config.soundModPath, config.sfxPath, config.soundBackupPath)) overallSuccess = false;
    }

    emit operationFinished(overallSuccess, overallSuccess ? "Все моды успешно установлены" : "Ошибки при установке");
}

void FileWorker::processRestoration(FileWorker::Config config) {
    emit statusUpdate("Возврат оригиналов...");
    killGtaEcosystem();
    QThread::msleep(2000);

    bool overallSuccess = true;

    emit progressMessage("Восстановление update.rpf...");
    if (!smartReplace(config.originalPath, config.gameUpdatePath, "update.rpf")) overallSuccess = false;

    emit progressMessage("Восстановление GunPacks...");
    if (!restoreGunPacks(config.dlcPacksTarget, config.backupPath)) overallSuccess = false;

    emit progressMessage("Восстановление звуков...");
    if (!internalRestoreSounds(config.sfxPath, config.soundBackupPath)) overallSuccess = false;

    emit operationFinished(overallSuccess, overallSuccess ? "Оригиналы успешно возвращены" : "Ошибки при восстановлении");
}

// --- РЕАЛИЗАЦИЯ РУЧНЫХ СЛОТОВ ---

void FileWorker::manualSmartReplace(const QString &source, const QString &targetDir, const QString &targetFileName) {
    emit statusUpdate("Ручная замена файла...");
    bool ok = smartReplace(source, targetDir, targetFileName);
    emit operationFinished(ok, ok ? "Файл успешно заменен" : "Ошибка при замене файла");
}

void FileWorker::manualRestoreGunPacks(FileWorker::Config config) {
    emit statusUpdate("Ручное восстановление GunPacks...");
    bool ok = restoreGunPacks(config.dlcPacksTarget, config.backupPath);
    emit operationFinished(ok, ok ? "GunPacks восстановлены" : "Ошибка восстановления GunPacks");
}

void FileWorker::manualInstallGunPacks(FileWorker::Config config) {
    emit statusUpdate("Ручная установка GunPacks...");
    bool ok = installGunPacks(config.gunPackSource, config.dlcPacksTarget, config.backupPath);
    emit operationFinished(ok, ok ? "GunPacks установлены" : "Ошибка установки GunPacks");
}

void FileWorker::manualReplaceSounds(FileWorker::Config config) {
    emit statusUpdate("Ручная установка звуков...");
    bool ok = internalReplaceSounds(config.soundModPath, config.sfxPath, config.soundBackupPath);
    emit operationFinished(ok, ok ? "Звуки установлены" : "Ошибка при установке звуков");
}

void FileWorker::manualRestoreSounds(FileWorker::Config config) {
    emit statusUpdate("Ручное восстановление звуков...");
    bool ok = internalRestoreSounds(config.sfxPath, config.soundBackupPath);
    emit operationFinished(ok, ok ? "Звуки восстановлены" : "Ошибка при восстановлении звуков");
}

// --- ВНУТРЕННЯЯ ЛОГИКА (WinAPI) ---

bool FileWorker::smartReplace(const QString &source, const QString &targetDir, const QString &targetFileName) {
    if (source.isEmpty() || !QFile::exists(source)) return false;
    QString fullDestPath = QDir::toNativeSeparators(targetDir + "/" + targetFileName);
    QString nativeSource = QDir::toNativeSeparators(source);
    if (nativeSource.toLower() == fullDestPath.toLower()) return true;

    for (int i = 0; i < 10; ++i) {
        if (QFile::exists(fullDestPath)) {
            SetFileAttributesW((LPCWSTR)fullDestPath.utf16(), FILE_ATTRIBUTE_NORMAL);
            if (!DeleteFileW((LPCWSTR)fullDestPath.utf16())) {
                QThread::msleep(1000);
                continue;
            }
        }
        if (CopyFileW((LPCWSTR)nativeSource.utf16(), (LPCWSTR)fullDestPath.utf16(), FALSE)) return true;
        QThread::msleep(1000);
    }
    return false;
}

bool FileWorker::copyDirectory(const QString &sourceDir, const QString &targetDir) {
    QDir sourceDirectory(sourceDir);
    if (!QDir().mkpath(targetDir)) return false;
    QFileInfoList fileList = sourceDirectory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    foreach (const QFileInfo &fileInfo, fileList) {
        QString srcPath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
        QString dstPath = QDir::toNativeSeparators(targetDir + "/" + fileInfo.fileName());
        if (fileInfo.isDir()) {
            if (!copyDirectory(srcPath, dstPath)) return false;
        } else {
            SetFileAttributesW((LPCWSTR)dstPath.utf16(), FILE_ATTRIBUTE_NORMAL);
            if (!CopyFileW((LPCWSTR)srcPath.utf16(), (LPCWSTR)dstPath.utf16(), FALSE)) return false;
        }
    }
    return true;
}

bool FileWorker::installGunPacks(const QString &source, const QString &target, const QString &backup) {
    QDir sourceDir(source);
    QDir targetDir(target);
    QDir().mkpath(backup);
    QFileInfoList items = sourceDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    foreach (const QFileInfo &item, items) {
        QString targetItem = QDir::toNativeSeparators(targetDir.absoluteFilePath(item.fileName()));
        QString backupItem = QDir::toNativeSeparators(backup + "/" + item.fileName());

        if (QFile::exists(targetItem) || QDir(targetItem).exists()) {
            if (item.isDir()) copyDirectory(targetItem, backupItem);
            else CopyFileW((LPCWSTR)targetItem.utf16(), (LPCWSTR)backupItem.utf16(), FALSE);
        }

        if (item.isDir()) {
            QDir(targetItem).removeRecursively();
            if (!copyDirectory(item.absoluteFilePath(), targetItem)) return false;
        } else {
            SetFileAttributesW((LPCWSTR)targetItem.utf16(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW((LPCWSTR)targetItem.utf16());
            if (!CopyFileW((LPCWSTR)item.absoluteFilePath().utf16(), (LPCWSTR)targetItem.utf16(), FALSE)) return false;
        }
    }
    return true;
}

bool FileWorker::restoreGunPacks(const QString &target, const QString &backup) {
    QDir backupDir(backup);
    if (!backupDir.exists()) return true;
    QFileInfoList items = backupDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    foreach (const QFileInfo &item, items) {
        QString targetPath = QDir::toNativeSeparators(target + "/" + item.fileName());
        if (item.isDir()) QDir(targetPath).removeRecursively();
        else {
            SetFileAttributesW((LPCWSTR)targetPath.utf16(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW((LPCWSTR)targetPath.utf16());
        }

        if (item.isDir()) copyDirectory(item.absoluteFilePath(), targetPath);
        else CopyFileW((LPCWSTR)item.absoluteFilePath().utf16(), (LPCWSTR)targetPath.utf16(), FALSE);
    }
    return true;
}

bool FileWorker::internalReplaceSounds(const QString &modPath, const QString &sfxPath, const QString &backupPath) {
    QDir modDir(modPath);
    QStringList files = modDir.entryList(QStringList() << "*.rpf", QDir::Files);
    QDir().mkpath(backupPath);

    for(const QString &f : files) {
        QString orig = QDir::toNativeSeparators(sfxPath + "/" + f);
        QString bkp = QDir::toNativeSeparators(backupPath + "/" + f);
        if (!QFile::exists(bkp) && QFile::exists(orig)) {
            CopyFileW((LPCWSTR)orig.utf16(), (LPCWSTR)bkp.utf16(), FALSE);
        }
        if (!smartReplace(modPath + "/" + f, sfxPath, f)) return false;
    }
    return true;
}

bool FileWorker::internalRestoreSounds(const QString &sfxPath, const QString &backupPath) {
    QDir bkpDir(backupPath);
    if (!bkpDir.exists()) return false;
    QFileInfoList files = bkpDir.entryInfoList(QStringList() << "*.rpf", QDir::Files);
    for(const QFileInfo &f : files) {
        if (!smartReplace(f.absoluteFilePath(), sfxPath, f.fileName())) return false;
    }
    return true;
}

void FileWorker::killGtaEcosystem() {
    QStringList procs = {"GTA5.exe", "SocialClubHelper.exe", "Launcher.exe", "RockstarService.exe"};
    for (const QString &name : procs) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32 pe = {sizeof(pe)};
        if (Process32First(hSnap, &pe)) {
            do {
                if (QString::fromWCharArray(pe.szExeFile).toLower() == name.toLower()) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProc) { TerminateProcess(hProc, 0); CloseHandle(hProc); }
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
}
