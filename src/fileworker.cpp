#include "fileworker.h"
#include <QDebug>


void FileWorker::processInstallation(FileWorker::Config config) {
    emit statusUpdate("Установка модов...");
    QStringList failedComponents;

    if (config.useRedux && !config.reduxPath.isEmpty()) {
        emit progressMessage("Установка Redux...");
        if (!smartReplace(config.reduxPath, config.gameUpdatePath, "update.rpf")) {
            failedComponents << "Redux (update.rpf)";
        }
    }

    if (config.useGunPack && !config.gunPackSource.isEmpty()) {
        emit progressMessage("Установка GunPacks...");
        if (!installGunPacks(config.gunPackSource, config.dlcPacksTarget, config.backupPath)) {
            failedComponents << "GunPacks";
        }
    }

    if (config.useSounds && !config.soundModPath.isEmpty()) {
        emit progressMessage("Установка звуков...");
        if (!internalReplaceSounds(config.soundModPath, config.sfxPath, config.soundBackupPath)) {
            failedComponents << "Звуковые моды";
        }
    }

    if (failedComponents.isEmpty()) {
        emit operationFinished(true, "Все моды успешно установлены");
    } else {
        QString errorMsg = "Ошибка при установке: " + failedComponents.join(", ");
        emit operationFinished(false, errorMsg);
    }
}

void FileWorker::processRestoration(FileWorker::Config config) {
    emit statusUpdate("Возврат оригиналов...");
    killGtaEcosystem();
    QThread::msleep(2000); // Даем время процессам завершиться

    QStringList failedSteps;

    emit progressMessage("Восстановление update.rpf...");
    if (!smartReplace(config.originalPath, config.gameUpdatePath, "update.rpf")) {
        failedSteps << "update.rpf";
    }

    emit progressMessage("Восстановление GunPacks...");
    if (!restoreGunPacks(config.dlcPacksTarget, config.backupPath)) {
        failedSteps << "GunPacks";
    }

    emit progressMessage("Восстановление звуков...");
    if (!internalRestoreSounds(config.sfxPath, config.soundBackupPath)) {
        failedSteps << "Звуковые архивы";
    }

    if (failedSteps.isEmpty()) {
        emit operationFinished(true, "Оригиналы успешно возвращены");
    } else {
        QString errorDetail = "Не удалось восстановить: " + failedSteps.join(", ");
        emit operationFinished(false, errorDetail);
    }
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

    QFileInfo srcInfo(source);
    qint64 srcSize = srcInfo.size();

    // ПРОВЕРКА 1: Если источник 0 КБ — это ошибка, не копируем
    if (srcSize <= 0) {
        qCritical() << "ОШИБКА: Исходный файл пуст (0 КБ):" << source;
        return false;
    }

    QString fullDestPath = QDir::toNativeSeparators(targetDir + "/" + targetFileName);
    QString nativeSource = QDir::toNativeSeparators(source);

    if (nativeSource.toLower() == fullDestPath.toLower()) return true;

    // Пытаемся 20 раз с короткой паузой (100мс)
    for (int i = 0; i < 20; ++i) {
        if (QFile::exists(fullDestPath)) {
            SetFileAttributesW((LPCWSTR)fullDestPath.utf16(), FILE_ATTRIBUTE_NORMAL);
        }

        // Копируем СРАЗУ поверх (FALSE позволяет перезаписывать)
        if (CopyFileW((LPCWSTR)nativeSource.utf16(), (LPCWSTR)fullDestPath.utf16(), FALSE)) {
            // ПРОВЕРКА 2: После копирования проверяем размер целевого файла
            QFileInfo destInfo(fullDestPath);
            if (destInfo.exists() && destInfo.size() == srcSize) {
                return true;
            } else {
                qWarning() << "Попытка" << i+1 << ": Файл скопирован неверно (размер не совпадает). Пробуем снова...";
            }
        }

        QThread::msleep(100);
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
            if (QFile::exists(dstPath)) {
                SetFileAttributesW((LPCWSTR)dstPath.utf16(), FILE_ATTRIBUTE_NORMAL);
            }
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

        // УМНЫЙ БЭКАП: Только если его еще нет
        if (!QFile::exists(backupItem) && !QDir(backupItem).exists()) {
            if (QFile::exists(targetItem) || QDir(targetItem).exists()) {
                if (item.isDir()) copyDirectory(targetItem, backupItem);
                else CopyFileW((LPCWSTR)targetItem.utf16(), (LPCWSTR)backupItem.utf16(), FALSE);
            }
        }

        if (item.isDir()) {
            QDir(targetItem).removeRecursively();
            if (!copyDirectory(item.absoluteFilePath(), targetItem)) return false;
        } else {
            if (!smartReplace(item.absoluteFilePath(), target, item.fileName())) return false;
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

        // Удаляем модовое
        if (item.isDir()) QDir(targetPath).removeRecursively();
        else {
            SetFileAttributesW((LPCWSTR)targetPath.utf16(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW((LPCWSTR)targetPath.utf16());
        }

        // Возвращаем оригинал
        if (item.isDir()) {
            if (!copyDirectory(item.absoluteFilePath(), targetPath)) return false;
        } else {
            if (!CopyFileW((LPCWSTR)item.absoluteFilePath().utf16(), (LPCWSTR)targetPath.utf16(), FALSE)) return false;
        }

        // Очищаем бэкап
        if (item.isDir()) QDir(item.absoluteFilePath()).removeRecursively();
        else QFile::remove(item.absoluteFilePath());
    }
    return true;
}

bool FileWorker::internalReplaceSounds(const QString &modPath, const QString &sfxPath, const QString &backupPath) {
    QDir modDir(modPath);
    QStringList files = modDir.entryList(QStringList() << "*.rpf", QDir::Files);
    QDir().mkpath(backupPath);

    foreach (const QString &f, files) {
        QString targetItem = QDir::toNativeSeparators(sfxPath + "/" + f);
        QString backupItem = QDir::toNativeSeparators(backupPath + "/" + f);
        QString modItem = QDir::toNativeSeparators(modPath + "/" + f);

        // УМНЫЙ БЭКАП
        if (!QFile::exists(backupItem)) {
            if (QFile::exists(targetItem)) {
                CopyFileW((LPCWSTR)targetItem.utf16(), (LPCWSTR)backupItem.utf16(), FALSE);
            }
        }

        // БЫСТРАЯ ЗАМЕНА
        if (!smartReplace(modItem, sfxPath, f)) return false;
    }
    return true;
}

bool FileWorker::internalRestoreSounds(const QString &sfxPath, const QString &backupPath) {
    QDir bkpDir(backupPath);
    if (!bkpDir.exists()) return true;
    QFileInfoList files = bkpDir.entryInfoList(QStringList() << "*.rpf", QDir::Files);
    foreach (const QFileInfo &f, files) {
        if (!smartReplace(f.absoluteFilePath(), sfxPath, f.fileName())) return false;
        QFile::remove(f.absoluteFilePath());
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
