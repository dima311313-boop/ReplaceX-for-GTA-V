#include "fileworker.h"
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QDirIterator>

// Системное копирование (Файл или Папка) через WinAPI
bool FileWorker::winCopyPath(const QString &src, const QString &dst) {
    if (src.isEmpty() || !QFile::exists(src) && !QDir(src).exists()) return false;

    std::wstring wsrc = QDir::toNativeSeparators(src).toStdWString();
    wsrc.push_back(L'\0'); wsrc.push_back(L'\0');
    std::wstring wdst = QDir::toNativeSeparators(dst).toStdWString();
    wdst.push_back(L'\0'); wdst.push_back(L'\0');

    SHFILEOPSTRUCTW fileOp = {0};
    fileOp.wFunc = FO_COPY;
    fileOp.pFrom = wsrc.c_str();
    fileOp.pTo = wdst.c_str();
    fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_SILENT | FOF_NOERRORUI;

    return SHFileOperationW(&fileOp) == 0;
}

// Системное удаление через WinAPI
bool FileWorker::winRemovePath(const QString &path) {
    if (path.isEmpty() || !QFile::exists(path) && !QDir(path).exists()) return true;

    std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    wpath.push_back(L'\0'); wpath.push_back(L'\0');

    SHFILEOPSTRUCTW fileOp = {0};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = wpath.c_str();
    fileOp.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;

    return SHFileOperationW(&fileOp) == 0;
}

void FileWorker::processInstallation(FileWorker::Config config) {
    emit statusUpdate("Установка модов...");
    emit progressValue(0);

    // Добавляем config.useZM в подсчет шагов установки
    int total = (config.useRedux ? 1 : 0) + (config.useGunPack ? 1 : 0) + (config.useSounds ? 1 : 0) + (config.useArmor ? 1 : 0) + (config.useZM ? 1 : 0);
    if (total == 0) { emit operationFinished(true, "Задачи не выбраны"); return; }

    int current = 0;

    // 1. REDUX
    if (config.useRedux && !config.reduxPath.isEmpty()) {
        emit progressMessage("Установка Redux...");
        smartReplace(config.reduxPath, config.gameUpdatePath, "update.rpf");
        current++; emit progressValue((current * 100) / total);
    }

    // 2. GUNPACKS
    if (config.useGunPack && !config.gunPackSource.isEmpty()) {
        emit progressMessage("Установка GunPacks...");
        installGunPacks(config.gunPackSource, config.dlcPacksTarget, config.backupPath);
        current++; emit progressValue((current * 100) / total);
    }

    // 3. SOUNDS
    if (config.useSounds && !config.soundModPath.isEmpty()) {
        emit progressMessage("Установка звуков...");
        internalReplaceSounds(config.soundModPath, config.sfxPath, config.soundBackupPath);
        current++; emit progressValue((current * 100) / total);
    }

    // 4. ARMOR
    if (config.useArmor && !config.armorSource.isEmpty()) {
        emit progressMessage("Установка броников...");
        installGunPacks(config.armorSource, config.armorTarget, config.armorBackupPath);
        current++; emit progressValue((current * 100) / total);
    }

    // 5. ЗАМЕНЕНКИ / СУМКИ (ZM) (Новый блок)
    if (config.useZM && !config.zmSource.isEmpty()) {
        emit progressMessage("Установка замененок...");
        installGunPacks(config.zmSource, config.zmTarget, config.zmBackupPath);
        current++; emit progressValue((current * 100) / total);
    }

    emit operationFinished(true, "Установка завершена");
}

void FileWorker::processRestoration(FileWorker::Config config) {
    emit statusUpdate("Восстановление...");
    killGtaEcosystem();
    QThread::msleep(1000);

    // Восстанавливаем только то, что было включено в конфиге!
    if (config.useRedux && !config.originalPath.isEmpty()) {
        emit progressMessage("Возврат update.rpf...");
        smartReplace(config.originalPath, config.gameUpdatePath, "update.rpf");
    }

    if (config.useGunPack) {
        emit progressMessage("Возврат GunPacks...");
        restoreGunPacks(config.dlcPacksTarget, config.backupPath);
    }

    if (config.useSounds) {
        emit progressMessage("Возврат звуков...");
        internalRestoreSounds(config.sfxPath, config.soundBackupPath);
    }

    // Новый блок для броников
    if (config.useArmor) {
        emit progressMessage("Возврат броников...");
        restoreGunPacks(config.armorTarget, config.armorBackupPath);
    }
    // Добавь этот блок в самый конец метода FileWorker::processRestoration:
    if (config.useZM) {
        emit progressMessage("Возврат замененок...");
        restoreGunPacks(config.zmTarget, config.zmBackupPath);
    }

    emit operationFinished(true, "Оригиналы возвращены");
}

bool FileWorker::installGunPacks(const QString &source, const QString &target, const QString &backup) {
    QDir srcDir(source);
    QFileInfoList items = srcDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    QDir().mkpath(backup);

    foreach (const QFileInfo &item, items) {
        QString targetPath = QDir::toNativeSeparators(target + "/" + item.fileName());
        QString backupPath = QDir::toNativeSeparators(backup + "/" + item.fileName());

        // БЭКАП С ПРОВЕРКОЙ
        if (!QFile::exists(backupPath) && !QDir(backupPath).exists()) {
            if (QFile::exists(targetPath) || QDir(targetPath).exists()) {
                if (!winCopyPath(targetPath, backupPath)) {
                    qCritical() << "Ошибка бэкапа:" << item.fileName();
                    return false;
                }
            }
        }

        // ЗАМЕНА
        winRemovePath(targetPath);
        winCopyPath(item.absoluteFilePath(), targetPath);
    }
    return true;
}

bool FileWorker::restoreGunPacks(const QString &target, const QString &backup) {
    QDir bkpDir(backup);
    QFileInfoList items = bkpDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    foreach (const QFileInfo &item, items) {
        QString targetPath = QDir::toNativeSeparators(target + "/" + item.fileName());
        winRemovePath(targetPath);
        if (winCopyPath(item.absoluteFilePath(), targetPath)) {
            winRemovePath(item.absoluteFilePath());
        }
    }
    return true;
}

bool FileWorker::smartReplace(const QString &source, const QString &targetDir, const QString &targetFileName) {
    QString dst = QDir::toNativeSeparators(targetDir + "/" + targetFileName);
    winRemovePath(dst);
    return winCopyPath(source, dst);
}

bool FileWorker::internalReplaceSounds(const QString &modPath, const QString &sfxPath, const QString &backupPath) {
    QDir modDir(modPath);
    QStringList files = modDir.entryList(QStringList() << "*.rpf", QDir::Files);
    QDir().mkpath(backupPath);
    foreach (const QString &f, files) {
        QString t = sfxPath + "/" + f;
        QString b = backupPath + "/" + f;
        if (!QFile::exists(b) && QFile::exists(t)) winCopyPath(t, b);
        winRemovePath(t);
        winCopyPath(modPath + "/" + f, t);
    }
    return true;
}

bool FileWorker::internalRestoreSounds(const QString &sfxPath, const QString &backupPath) {
    QDir bkpDir(backupPath);
    QFileInfoList files = bkpDir.entryInfoList(QStringList() << "*.rpf", QDir::Files);
    foreach (const QFileInfo &f, files) {
        QString t = sfxPath + "/" + f.fileName();
        winRemovePath(t);
        if (winCopyPath(f.absoluteFilePath(), t)) winRemovePath(f.absoluteFilePath());
    }
    return true;
}

void FileWorker::killGtaEcosystem() {
    // ИСПРАВЛЕНО: Добавлены процессы версии Enhanced
    QStringList procs = {
        "GTA5.exe",
        "GTA5_Enhanced.exe",
        "GTA5_Enhanced_BE.exe",
        "SocialClubHelper.exe",
        "Launcher.exe",
        "RockstarService.exe"
    };
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

void FileWorker::processUnpack(int type, QString archivePath, QString originalUpdatePath) {
    emit statusUpdate("В процессе.....");
    emit progressValue(10);

    QString appDir = QCoreApplication::applicationDirPath();
    bool isRar = archivePath.endsWith(".rar", Qt::CaseInsensitive);
    QString tool = isRar ? "UnRAR.exe" : "7za.exe";
    QString toolPtr = QDir::toNativeSeparators(appDir + "/" + tool);
    QString password = "majestic-mods.ru";

    QString folderName = (type == 0) ? "unrarRedux" : (type == 1) ? "unrarGuns" : "unrarSounds";
    QString destDir = QDir::toNativeSeparators(appDir + "/" + folderName);

    QDir(destDir).removeRecursively();
    QDir().mkpath(destDir);

    QProcess proc;
    QStringList args;

    // Используем "x" для сохранения структуры папок
    if (isRar) {
        args << "x" << "-p" + password << "-y" << "-ai" << archivePath << "*.*" << destDir + "\\";
    } else {
        args << "x" << archivePath << "-o" + destDir << "-p" + password << "-y" << "-r";
    }

    proc.start(toolPtr, args);
    if (!proc.waitForFinished(180000)) {
        proc.kill();
        emit unpackFinished(type, "Ошибка");
        return;
    }

    emit progressValue(70);
    emit statusUpdate("Очистка мусора...");

    // Найди этот блок внутри void FileWorker::processUnpack(...)
    // --- ЛОГИКА ОРГАНИЗАЦИИ И ОЧИСТКИ ---
    QStringList targets;
    if (type == 0) targets << "update.rpf";
    else if (type == 1) targets << "mpapartment" << "patchday18ng";
    else if (type == 2) targets << "RESIDENT.rpf" << "WEAPONS_PLAYER.rpf";
    else if (type == 3) targets << "mpapartment"; // patchday11ng и другие обработаем кодом ниже

    // 1. Ищем наши цели во всех подпапках (решаем проблему матрешек)
    QDirIterator it(destDir, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    struct FoundItem { QString name; QString oldPath; bool isDir; };
    QList<FoundItem> found;

    // Найди цикл прохода по файлам while (it.hasNext()) внутри processUnpack
    while (it.hasNext()) {
        it.next();
        if (type == 3) {
            // Ослабленная валидация для броников: принимаем mpapartment или любые папки patchday...ng
            QString name = it.fileName().toLower();
            if (name == "mpapartment" || (name.startsWith("patchday") && name.endsWith("ng"))) {
                found << FoundItem{it.fileName(), it.filePath(), it.fileInfo().isDir()};
            }
        }
        else if (type == 4) {
            // Ослабленная защита для замененок: принимаем любую папку мода, если в ней лежит dlc.rpf
            if (it.fileInfo().isDir()) {
                QDir subDir(it.filePath());
                if (subDir.exists("dlc.rpf")) {
                    found << FoundItem{it.fileName(), it.filePath(), true};
                }
            }
        }
        else {
            foreach(QString t, targets) {
                if (it.fileName().compare(t, Qt::CaseInsensitive) == 0) {
                    found << FoundItem{it.fileName(), it.filePath(), it.fileInfo().isDir()};
                }
            }
        }
    }

    if (found.isEmpty()) {
        emit unpackFinished(type, "Ошибка");
        return;
    }

    // 2. Переносим всё нужное во временную папку
    QString tempDir = appDir + "/temp_extract";
    QDir().mkpath(tempDir);
    foreach(auto item, found) {
        QString newPath = tempDir + "/" + item.name;
        if (item.isDir) {
            // Переносим папку
            QDir().rename(item.oldPath, newPath);

            // --- ВОТ ОНА, ЗАЧИСТКА ВНУТРИ ПАПКИ ---
            // Проходим по всем файлам внутри перенесенной папки (рекурсивно)
            QDirIterator subIt(newPath, QDir::Files, QDirIterator::Subdirectories);
            while (subIt.hasNext()) {
                subIt.next();
                // Если файл НЕ заканчивается на .rpf — удаляем его без жалости
                if (!subIt.fileName().endsWith(".rpf", Qt::CaseInsensitive)) {
                    QFile::remove(subIt.filePath());
                }
            }
        } else {
            // Если это одиночный файл (как update.rpf), просто переносим
            QFile::rename(item.oldPath, newPath);
        }
    }

    // 3. Сносим всё в unrar папке (там остался мусор и пустые папки)
    QDir(destDir).removeRecursively();
    QDir().mkpath(destDir);

    // 4. Возвращаем нужное из темпа в чистую папку
    QDir tDir(tempDir);
    foreach(QString f, tDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
        QDir().rename(tempDir + "/" + f, destDir + "/" + f);
    }
    tDir.removeRecursively();

    // Финальная проверка для Redux
    if (type == 0) {
        QFileInfo fi(destDir + "/update.rpf");
        if (!fi.exists() || fi.size() == 0) {
            emit unpackFinished(type, "Ошибка");
            return;
        }
    }

    emit progressValue(100);
    QString finalPath = QDir::toNativeSeparators(destDir);
    if (type == 0) finalPath += "\\update.rpf";

    emit unpackFinished(type, finalPath);
}



// Реализация ручных слотов (просто вызывают внутренние методы)
void FileWorker::manualRestoreZM(FileWorker::Config c) { bool ok = restoreGunPacks(c.zmTarget, c.zmBackupPath); emit operationFinished(ok, ok?"Успех":"Ошибка"); }
void FileWorker::manualInstallZM(FileWorker::Config c) { bool ok = installGunPacks(c.zmSource, c.zmTarget, c.zmBackupPath); emit operationFinished(ok, ok?"Успех":"Ошибка"); }
void FileWorker::manualRestoreArmorPacks(FileWorker::Config c) { bool ok = restoreGunPacks(c.armorTarget, c.armorBackupPath); emit operationFinished(ok, ok?"Успех":"Ошибка"); }
void FileWorker::manualInstallArmorPacks(FileWorker::Config c) { bool ok = installGunPacks(c.armorSource, c.armorTarget, c.armorBackupPath); emit operationFinished(ok, ok?"Успех":"Ошибка"); }
void FileWorker::manualSmartReplace(const QString &s, const QString &td, const QString &tf) { emit progressValue(50); bool ok = smartReplace(s, td, tf); emit progressValue(100); emit operationFinished(ok, ok?"Успех":"Ошибка"); }
void FileWorker::manualRestoreGunPacks(FileWorker::Config c) { bool ok = restoreGunPacks(c.dlcPacksTarget, c.backupPath); emit operationFinished(ok, ok?"Успех":"Ошибка"); }
void FileWorker::manualInstallGunPacks(FileWorker::Config c) { bool ok = installGunPacks(c.gunPackSource, c.dlcPacksTarget, c.backupPath); emit operationFinished(ok, ok?"Успех":"Ошибка"); }
void FileWorker::manualReplaceSounds(FileWorker::Config c) { bool ok = internalReplaceSounds(c.soundModPath, c.sfxPath, c.soundBackupPath); emit operationFinished(ok, ok?"Успех":"Ошибка"); }
void FileWorker::manualRestoreSounds(FileWorker::Config c) { bool ok = internalRestoreSounds(c.sfxPath, c.soundBackupPath); emit operationFinished(ok, ok?"Успех":"Ошибка"); }
