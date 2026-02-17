#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QString>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QTimer>
#include <QMessageBox>
#include <QSettings>
#include <QFileInfo>
#include <windows.h>
#include <tlhelp32.h>
#include <QDesktopServices>
#include <QThread>
#include <QUrl>
#include <windows.h>
#include <shellapi.h>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QBitmap>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>


QPixmap getRoundedPixmap(const QPixmap& src, int radius) {
    if (src.isNull()) return src;


    QBitmap mask(src.size());
    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(src.rect(), Qt::white);

    QPainterPath path;
    path.addRoundedRect(src.rect(), radius, radius);


    painter.setBrush(Qt::black);
    painter.drawPath(path);
    painter.end();

    QPixmap result = src;
    result.setMask(mask);

    return result;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);



    // --- ИНИЦИАЛИЗАЦИЯ ПОТОКА ---
    m_workerThread = new QThread(this);
    m_worker = new FileWorker(); // Не передаем parent, так как он переедет в поток
    m_worker->moveToThread(m_workerThread);

    // Соединяем сигналы MainWindow с методами FileWorker
    connect(this, &MainWindow::requestInstall, m_worker, &FileWorker::processInstallation);
    connect(this, &MainWindow::requestRestore, m_worker, &FileWorker::processRestoration);

    // Коннекты для РУЧНЫХ операций
    connect(this, &MainWindow::requestManualSmartReplace, m_worker, &FileWorker::manualSmartReplace);
    connect(this, &MainWindow::requestManualRestoreGunPacks, m_worker, &FileWorker::manualRestoreGunPacks);
    connect(this, &MainWindow::requestManualInstallGunPacks, m_worker, &FileWorker::manualInstallGunPacks);
    connect(this, &MainWindow::requestManualReplaceSounds, m_worker, &FileWorker::manualReplaceSounds);
    connect(this, &MainWindow::requestManualRestoreSounds, m_worker, &FileWorker::manualRestoreSounds);

    // Соединяем ответы FileWorker с интерфейсом
    connect(m_worker, &FileWorker::statusUpdate, this, &MainWindow::onWorkerStatus);
    connect(m_worker, &FileWorker::progressMessage, this, &MainWindow::onWorkerProgress);
    connect(m_worker, &FileWorker::operationFinished, this, &MainWindow::onWorkerFinished);

    // Очистка при завершении
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();
    // --- КОНЕЦ ИНИЦИАЛИЗАЦИИ ПОТОКА ---


    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &MainWindow::onResult);

    QNetworkRequest request(QUrl("https://raw.githubusercontent.com/dima311313-boop/ReplaceX-for-GTA-V/ReplaceX/resources/donat,new.json"));
    manager->get(request);



    loadSettings();
    QPixmap iconPixmap(":/izobr/IconG.png");
    int cornerRadius = 25; // Чем больше число, тем сильнее закругление

    QPixmap roundedIcon = getRoundedPixmap(iconPixmap, cornerRadius);
    this->setWindowIcon(QIcon(roundedIcon));


    checkTimer = new QTimer(this);
    connect(checkTimer, &QTimer::timeout, this, &MainWindow::checkGtaProcess);
    checkTimer->start(3000); // Проверять раз в 3 секунды

    ui->btnExitNF->setVisible(false);
    ui->oknoNF->setVisible(false);
    ui->btnNotification->setVisible(false);
    ui->oknoDiscleamer->setVisible(false);
    ui->oknoHDD->setVisible(false);
    ui->oknoKnopohki->setVisible(false);
    ui->btnExitGP->setVisible(false);
    ui->oknoZV->setVisible(false);
    //получение пути
    connect(ui->leditPapka, &QLineEdit::textChanged, [this](const QString &text) {
        QSettings settings("MyCompany", "MyGameTool");
        settings.setValue("Paths/GameFolder", text);
    });


    //оконо установки ган паков (фон)
    ui->listView->setStyleSheet(
        "QListView {"
        "   background-color: #1e1e1e;"
        "   border: 2px solid #cccccc;" //цвет
        "   border-radius: 4px;" //углы
        "}"
        );
    //бэкапы ган паков
    m_backupPath = QCoreApplication::applicationDirPath() + "/backups_gta";

    ui->oknoGP->setVisible(false);
    //кнопочки тг и д
    infoPopup = new QLabel(this);
    infoPopup->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);

    infoPopup->setStyleSheet(
        "background-color: #2c3e50;"
        "color: white;"
        "border-radius: 8px;"
        "padding: 8px;"
        "border: 1px solid #34495e;"
        );
    infoPopup->setText("Если у вас есть жалобы/предложения или вы хотите оставить<br>отзыв можете написать разработчику в Telegram");
    infoPopup->adjustSize();

    popupOpacity = new QGraphicsOpacityEffect(infoPopup);
    infoPopup->setGraphicsEffect(popupOpacity);
    popupOpacity->setOpacity(0.0);
    infoPopup->hide();

    ui->btnTelegram->installEventFilter(this);
    ui->btnDonat->installEventFilter(this);
    //end knopocki


    this->setFixedSize(646, 374);

    qDebug() << "Программа запущенна.";
    ui->miniProgress->setText("Ожидание запуска игры");
    this->setWindowIcon(QIcon(":/izobr/photo_2026-01-15_107-25-59-round-corners.ico"));
    this->setWindowTitle("ReplaceX");
    //закругление tg
    ui->btnTelegram->setStyleSheet(
        "QPushButton#btnTelegram {"
        " border-radius: 25px;"
        " background-color: #0088cc;"
        " border: none;"
        " background-image: url(:/izobr/telegram_icon.png);"
        " background-position: center;"
        " background-repeat: no-repeat;"
        " padding: 0px;"
        "}"
        "QPushButton#btnTelegram:hover {"
        " background-color: #00aaff;"
        "}"
        );
    ui->btnDonat->setStyleSheet(
        "QPushButton#btnDonat {"
        " border-radius: 25px;"
        " background-color: #0088cc;"
        " border: none;"
        " background-image: url(:/izobr/1647901232_1-abrakadabra-fun-p-donati-alers-1.jpg);"
        " background-position: center;"
        " background-repeat: no-repeat;"
        " padding: 0px;"
        "}"
        "QPushButton#btnDonat:hover {"
        " background-color: #00aaff;"
        "}"
        );



    //трей
    trayMenu = new QMenu(this);
    QAction *restoreAction = new QAction("Развернуть", this);
    QAction *quitAction = new QAction("Выход", this);

    connect(restoreAction, &QAction::triggered, this, &MainWindow::showNormal);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    trayMenu->addAction(restoreAction);
    trayMenu->addSeparator();
    trayMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/izobr/photo_2026-01-15_107-25-59-round-corners.ico")); // Твоя иконка
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason){
        if(reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            this->showNormal();
            this->activateWindow();
        }
    });
    //конец трея

    connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    //Настройки для сохранения путей
    QSettings settings("MyCompany", "MyGameTool");
    //.bat
    // Читаем время. Если в реестре пусто 15 по умолчанию
    bool autoStart = settings.value("autoStart", false).toBool();
    QString savedTime = settings.value("pauseTime", "15").toString();
    ui->leditTimeVvod->setText(savedTime);

    // Устанавливаем значения в интерфейс
    ui->checkAutoOn_Off->setChecked(autoStart);
    ui->leditTimeVvod->setText(savedTime);

    // Если галочка была включена, запускаем таймер сразу
    if (autoStart) {
        checkTimer->start(2000);
    }

    // Загрузка состояния чекбокса для автоустановки звуков
    bool isCheckAutoLoadZV = settings.value("checkAutoLoadZV", false).toBool();
    ui->checkAutoLoadZV->setChecked(isCheckAutoLoadZV);

    // Подключение сигнала изменения состояния чекбокса
    connect(ui->checkAutoLoadZV, &QCheckBox::toggled, this, &MainWindow::on_checkAutoLoadZV_toggled);

    //звуки
    m_x64AudioSfxPath = settings.value("Paths/X64AudioSfx").toString();
    m_modSoundPath = settings.value("Paths/SoundMod").toString();


    // Подставляем в QLineEdit
    ui->leditPapcaZV->setText(m_x64AudioSfxPath);
    ui->leditModZV->setText(m_modSoundPath);

    qDebug() << "Настройки загружены:" << m_x64AudioSfxPath << m_modSoundPath;

    //авто поиск корневой и вывод
    bool firstRun = !settings.contains("FirstRun");


    if (firstRun) {
        // Автоматически ищем папку update
        QString updateFolder = autoFindUpdateFolder();

        if (!updateFolder.isEmpty()) {
            // Сохраняем в настройки
            settings.setValue("FirstRun", true);
            settings.setValue("Paths/GameFolder", updateFolder);  // Ключевое поле!

            // Записываем в UI
            ui->leditPapka->setText(updateFolder);
        } else {
            // Если не нашли автоматически — оставляем поле пустым
            ui->leditPapka->clear();
        }
    } else {
        // Загружаем сохранённый путь из настроек
        QString savedPath = settings.value("Paths/GameFolder").toString();
        if (!savedPath.isEmpty() && QDir(savedPath).exists()) {
            ui->leditPapka->setText(savedPath);
        } else {
            ui->leditPapka->clear();  // Сброс, если папка не найдена
        }
    }

    qDebug() << "Файл настроек:" << settings.fileName();
    //рпф

    if (firstRun) {
        QString rpfPath = findUpdateRpf();
        if (!rpfPath.isEmpty()) {
            if (copyUpdateRpfToAppDir(rpfPath)) {

                settings.setValue("FirstRun", true);
                settings.setValue("Paths/OriginalFile", ui->leditOrig->text());
            }
        }
    } else {
        ui->leditOrig->setText(settings.value("Paths/OriginalFile").toString());
    }


    //ган пак
    bool isChecked = settings.value("checkAutoLoadGP", false).toBool();
    ui->checkAutoLoadGP->setChecked(isChecked);

    m_gunPackSourcePath = settings.value("Paths/GunPackSource", "").toString();
    m_dlcPacksTargetPath = settings.value("Paths/DlcPacksTarget", "").toString();

    ui->leditGunPuck->setText(m_gunPackSourcePath);
    ui->leditDLS->setText(m_dlcPacksTargetPath);
    //end

    QString savedPath = settings.value("Paths/GameFolder", "").toString();
    if (savedPath.isEmpty()) {
        savedPath = findGTAPath();
    }

    ui->leditPapka->setText(savedPath);
    ui->leditOrig->setText(settings.value("Paths/OriginalFile", "").toString());
    ui->leditRedux->setText(settings.value("Paths/ReduxFile", "").toString());
    ui->checkAutoLoad->setChecked(settings.value("Settings/AutoLoad", false).toBool());

    processTimer = new QTimer(this);
    connect(processTimer, &QTimer::timeout, this, &MainWindow::checkProcessLoop);
    processTimer->start(500);
}

MainWindow::~MainWindow() {

    connect(ui->btnPapkaGP, &QPushButton::clicked, this, &MainWindow::on_btnPapkaGP_clicked);
    connect(ui->btnReplaceGunPuck, &QPushButton::clicked,
            this, &MainWindow::on_btnReplaceGunPuck_clicked);

    connect(ui->btnDLS, &QPushButton::clicked, this, &MainWindow::on_btnDLS_clicked);

    connect(ui->btnReplaceOrigGP, &QPushButton::clicked,
            this, &MainWindow::on_btnReplaceOrigGP_clicked);

    connect(ui->checkAutoLoadGP, &QCheckBox::toggled,
            this, &MainWindow::on_checkAutoLoadGP_toggled);

    m_workerThread->quit();
    m_workerThread->wait();

    delete ui;
}
const QString CURRENT_VERSION = "0.9.4"; //текущая версия

void MainWindow::onResult(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Сетевая ошибка:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    // 1. Проверка на объект (теперь у нас корень - объект { })
    if (!doc.isObject()) {
        qDebug() << "Критическая ошибка: Ожидался JSON объект!";
        reply->deleteLater();
        return;
    }

    QJsonObject mainObj = doc.object();

    // --- ЧАСТЬ 1: ДОНАТЕРЫ ---
    QJsonArray donatorsArray = mainObj["donators"].toArray();
    QString donatorsList = "Спасибо❤️: ";

    for (const QJsonValue &value : donatorsArray) {
        QJsonObject dObj = value.toObject();
        donatorsList += dObj["name"].toString() + " - " + QString::number(dObj["amount"].toDouble()) + " руб, ";
    }
    donatorsList.chop(2);
    ui->marqueeLabel->setText(donatorsList);

    // --- ЧАСТЬ 2: ОБНОВЛЕНИЕ ---
    QJsonObject updateObj = mainObj["update"].toObject();
    QString remoteVersion = updateObj["version"].toString();
    QString downloadUrl = updateObj["url"].toString();
    QJsonArray changelogArray = updateObj["changelog"].toArray();
    QString changelogText;
    if (!changelogArray.isEmpty()) {
        changelogText = "\n\nЧто нового:\n";
        for (const QJsonValue &change : changelogArray) {
            changelogText += "• " + change.toString() + "\n";
        }
    }

    qDebug() << "Проверка версии. Сервер:" << remoteVersion << "Локальная:" << CURRENT_VERSION;

    if (!remoteVersion.isEmpty() && remoteVersion != CURRENT_VERSION) {
        qDebug() << "Доступна новая версия: " << CURRENT_VERSION;
        ui->lblUpVer->setText("Новая версия: " + remoteVersion);
        ui->lblSpisocIzm->setText(changelogText);
        ui->btnNotification->setVisible(true);


    }

    reply->deleteLater();
}



//трей
void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            QTimer::singleShot(200, this, &MainWindow::hide);
            trayIcon->showMessage("ReplaceX", "Программа свернута в трей и продолжает работу", QSystemTrayIcon::Information, 2000);
        }
    }
    QMainWindow::changeEvent(event);
}


//ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
QString MainWindow::findGTAPath() {
    //Ргс Лаунчер
    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString path = rsReg.value("InstallFolder").toString();

    if (path.isEmpty()) {
        //Steam
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            path = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }
    return QDir::toNativeSeparators(path);
}

void MainWindow::killProcessByName(QString name) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnap, &pe)) {
        do {
            if (QString::fromWCharArray(pe.szExeFile) == name) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc) {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                }
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

bool MainWindow::copyFileToGame(QString sourcePath, QString destFolder) {
    if (sourcePath.isEmpty() || !QDir(destFolder).exists()) return false;

    QFileInfo srcInfo(sourcePath);
    QString fullDestPath = QDir::toNativeSeparators(destFolder + "/" + srcInfo.fileName());
    QString nativeSource = QDir::toNativeSeparators(sourcePath);

    // 1. ПРОВЕРКА: Чтобы не копировать файл в самого себя (от этого и затирается!)
    if (nativeSource.toLower() == fullDestPath.toLower()) {
        qDebug() << ">>> ПРОПУСК: Источник и цель совпадают!";
        return true;
    }

    if (srcInfo.size() <= 0) return false;

    // 2. ПЕРЕЗАПИСЬ:

    QFile::remove(fullDestPath); // Удал. старый файл

    if (CopyFileW((LPCWSTR)nativeSource.utf16(), (LPCWSTR)fullDestPath.utf16(), FALSE)) {
        qDebug() << ">>> УСПЕХ: Файл скопирован в" << fullDestPath;
        return true;
    } else {
        DWORD err = GetLastError();
        qDebug() << ">>> ОШИБКА WinAPI:" << err; // Если 32 — файл занят
        return false;
    }
}

//АВТОМАТИЗАЦИЯ
bool MainWindow::isFileBusy(QString filePath) {
    if (!QFile::exists(filePath)) return false;

    std::wstring wPath = QDir::toNativeSeparators(filePath).toStdWString();
    // Пробуем открыть файл "только для себя"
    HANDLE hFile = CreateFileW((LPCWSTR)wPath.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION) return true; // Файл занят
        return false;
    }
    CloseHandle(hFile);
    return false; // Файл свободен
}







// ГЛАВНЫЙ ФИКС: Функция умной замены с ретраями


void MainWindow::killGtaEcosystem() {
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

bool MainWindow::isProcessRunning(const QString &exeName) {
    bool found = false;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe = {sizeof(pe)};
    if (Process32First(hSnap, &pe)) {
        do {
            if (QString::fromWCharArray(pe.szExeFile).toLower() == exeName.toLower()) {
                found = true;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}


FileWorker::Config MainWindow::getCurrentConfig() {
    FileWorker::Config cfg;
    cfg.reduxPath = ui->leditRedux->text();
    cfg.originalPath = ui->leditOrig->text();
    cfg.gameUpdatePath = ui->leditPapka->text();
    cfg.gunPackSource = ui->leditGunPuck->text();
    cfg.dlcPacksTarget = ui->leditDLS->text();
    cfg.backupPath = QCoreApplication::applicationDirPath() + "/backups_gta";
    cfg.soundModPath = ui->leditModZV->text();
    cfg.sfxPath = ui->leditPapcaZV->text();
    cfg.soundBackupPath = QCoreApplication::applicationDirPath() + "/sound_backup";
    cfg.useRedux = ui->checkAutoLoad->isChecked();
    cfg.useGunPack = ui->checkAutoLoadGP->isChecked();
    cfg.useSounds = ui->checkAutoLoadZV->isChecked();
    return cfg;
}





void MainWindow::checkProcessLoop() {
    if (m_isOperationPending) return; // Не проверяем, пока идет копия

    bool isGameActive = isProcessRunning("gta5.exe");

    if (isGameActive && !m_wasGameRunning) {
        m_wasGameRunning = true;
        m_isOperationPending = true;
        emit requestInstall(getCurrentConfig());
    }

    if (!isGameActive && m_wasGameRunning) {
        m_wasGameRunning = false;
        m_isOperationPending = true;
        emit requestRestore(getCurrentConfig());
    }
}

// Обработчики ответов от потока
void MainWindow::onWorkerStatus(QString status) {
    ui->miniProgress->setText(status);
}

void MainWindow::onWorkerProgress(QString msg) {
    qDebug() << "Worker:" << msg;

}

void MainWindow::onWorkerFinished(bool success, QString details) {
    m_isOperationPending = false;
    if (!success) {
        qDebug() << "Ошибка операции:" << details;
    }
    ui->miniProgress->setText(details);
}

// --- ОБРАБОТЧИКИ КНОПОК ---

void MainWindow::on_btnAddOrig_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "Выберите оригинал", "", "RPF (*.rpf)");
    if (!path.isEmpty()) {
        ui->leditOrig->setText(path);
        QSettings("MyCompany", "MyGameTool").setValue("Paths/OriginalFile", path);
    }
}

void MainWindow::on_btnAddRedux_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "Выберите редукс", "", "RPF (*.rpf)");
    if (!path.isEmpty()) {
        ui->leditRedux->setText(path);
        QSettings("MyCompany", "MyGameTool").setValue("Paths/ReduxFile", path);
    }
}

void MainWindow::on_btnPapka_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Папка GTA 5 update", ui->leditPapka->text());
    if (!dir.isEmpty()) {
        ui->leditPapka->setText(dir);
        QSettings("MyCompany", "MyGameTool").setValue("Paths/GameFolder", dir);
    }
}

void MainWindow::on_btnReplaceOrig_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualSmartReplace(ui->leditOrig->text(), ui->leditPapka->text(), "update.rpf");
}

void MainWindow::on_btnReplaceRedux_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualSmartReplace(ui->leditRedux->text(), ui->leditPapka->text(), "update.rpf");
}

void MainWindow::on_checkAutoLoad_toggled(bool checked) {
    if (!checked) {
        QSettings("MyCompany", "MyGameTool").setValue("Settings/AutoLoad", false);
        return;
    }

    QString pathOrig = QDir::fromNativeSeparators(ui->leditOrig->text()).toLower();
    QString pathPapka = QDir::fromNativeSeparators(ui->leditPapka->text()).toLower();
    QString pathRedux = QDir::fromNativeSeparators(ui->leditRedux->text()).toLower();

    // 1. Проверка leditOrig: должен быть файл update.rpf
    bool isOrigOk = pathOrig.endsWith("/update.rpf") && QFile::exists(pathOrig);

    // 2. Проверка leditPapka: должна быть папка update на конце
    // Удаляем лишние слэши в конце для точности
    if (pathPapka.endsWith("/")) pathPapka.chop(1);
    bool isPapkaOk = pathPapka.endsWith("/update") && QDir(pathPapka).exists();

    // 3. Проверка leditRedux: должен быть update.rpf и не совпадать с оригиналом
    bool isReduxOk = pathRedux.endsWith("/update.rpf") && QFile::exists(pathRedux);
    bool isNotSame = (pathOrig != pathRedux);

    if (isOrigOk && isPapkaOk && isReduxOk && isNotSame) {
        QSettings("MyCompany", "MyGameTool").setValue("Settings/AutoLoad", true);
        qDebug() << "AutoLoad успешно включен";
    } else {
        ui->checkAutoLoad->blockSignals(true);
        ui->checkAutoLoad->setChecked(false);
        ui->checkAutoLoad->blockSignals(false);

        QStringList errors;
        if (!isOrigOk) errors << "- В поле 'Оригинал' должен быть выбран файл update.rpf";
        if (!isPapkaOk) errors << "- В поле 'Папка игры' путь должен заканчиваться на папку 'update'";
        if (!isReduxOk) errors << "- В поле 'Редукс' должен быть выбран файл update.rpf";
        if (!isNotSame && !pathOrig.isEmpty()) errors << "- Пути Оригинала и Редукса не должны совпадать!";

        QMessageBox::critical(this, "Ошибка валидации", "Некорректные пути:\n" + errors.join("\n"));
    }
}

void MainWindow::on_btnDonat_clicked() {

    QDesktopServices::openUrl(QUrl("https://www.donationalerts.com/r/kot6366363"));
}
//авто поиск
void MainWindow::on_btnAutoSearch_clicked() {
    QString foundPath = "";

    //авто поиск пути Legacy
    QStringList registryPaths = {
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\GTAV Legacy",
        "HKEY_CURRENT_USER\\Software\\Rockstar Games\\Grand Theft Auto V"
    };

    for (const QString &regPath : registryPaths) {
        QSettings reg(regPath, QSettings::NativeFormat);
        QString path = reg.value("InstallFolder").toString();
        if (!path.isEmpty() && QDir(path).exists()) {
            foundPath = path;
            break;
        }
    }

    if (foundPath.isEmpty()) {
        QStringList manualPaths = {
            "C:/Program Files/Rockstar Games/Grand Theft Auto V",
            "C:/Program Files/Rockstar Games/GTAV Legacy", // Частый путь для легаси
            "D:/Games/Rockstar Games/Grand Theft Auto V",
            "E:/Games/Grand Theft Auto V"
        };

        for (const QString &path : manualPaths) {
            if (QDir(path).exists()) {
                foundPath = path;
                break;
            }
        }
    }

    if (!foundPath.isEmpty()) {
        QDir gtaDir(foundPath);


        if (gtaDir.exists("update")) {
            QString updatePath = QDir::toNativeSeparators(gtaDir.absoluteFilePath("update"));
            ui->leditPapka->setText(updatePath);

            // Сохраняем результат
            QSettings("MyCompany", "MyGameTool").setValue("Paths/GameFolder", updatePath);
            QMessageBox::information(this, "Найдено!", "Путь найден (Legacy/Social Club):\n" + updatePath);
            qDebug() << "Путь найден";
            ui->miniProgress->setText("Путь к корневой папке установлен");
        } else {
            // Если саму игру нашли, но зашли не в ту папку
            QMessageBox::warning(this, "Внимание", "Папка игры найдена, но внутри нет папки 'update'. Проверьте целостность файлов.");
        }
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось найти GTA 5 Legacy автоматически. Пожалуйста, укажите папку вручную.");
    }
}
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // 1. Обработка btnTelegram
    if (obj == ui->btnTelegram) {
        if (event->type() == QEvent::Enter) {
            infoPopup->setText("Если у вас есть жалобы/предложения или вы хотите оставить<br>отзыв можете написать разработчику в Telegram");
            infoPopup->adjustSize();

            QPoint globalPos = ui->btnTelegram->mapToGlobal(QPoint(0, 0));
            int x = globalPos.x() + (ui->btnTelegram->width() / 2) - (infoPopup->width() / 2);
            int y = globalPos.y() - infoPopup->height() - 10;

            infoPopup->move(x, y);
            infoPopup->show();

            QPropertyAnimation *anim = new QPropertyAnimation(popupOpacity, "opacity");
            anim->setDuration(200);
            anim->setStartValue(popupOpacity->opacity());
            anim->setEndValue(1.0);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            QPropertyAnimation *anim = new QPropertyAnimation(popupOpacity, "opacity");
            anim->setDuration(200);
            anim->setStartValue(popupOpacity->opacity());
            anim->setEndValue(0.0);
            connect(anim, &QPropertyAnimation::finished, infoPopup, &QLabel::hide);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
            return true;
        }
    }

    // 2. Обработка btnDonat
    if (obj == ui->btnDonat) {
        if (event->type() == QEvent::Enter) {
            infoPopup->setText("Благодарность проекту донатом");
            infoPopup->adjustSize();

            QPoint globalPos = ui->btnDonat->mapToGlobal(QPoint(0, 0));
            int x = globalPos.x() + (ui->btnDonat->width() / 2) - (infoPopup->width() / 2);
            int y = globalPos.y() - infoPopup->height() - 10;

            infoPopup->move(x, y);
            infoPopup->show();

            QPropertyAnimation *anim = new QPropertyAnimation(popupOpacity, "opacity");
            anim->setDuration(200);
            anim->setStartValue(popupOpacity->opacity());
            anim->setEndValue(1.0);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            QPropertyAnimation *anim = new QPropertyAnimation(popupOpacity, "opacity");
            anim->setDuration(200);
            anim->setStartValue(popupOpacity->opacity());
            anim->setEndValue(0.0);
            connect(anim, &QPropertyAnimation::finished, infoPopup, &QLabel::hide);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}



//tg
void MainWindow::on_btnTelegram_clicked()
{
    QString telegramUrl = "https://t.me/replacexDev";
    QDesktopServices::openUrl(QUrl(telegramUrl));
}
//ган пак
void MainWindow::on_btnOknoDop_clicked()
{
    ui->oknoNF->setVisible(false);
    if(oknoDop == true){
        //дисклеймер
        QRect startRect = ui->btnOknoDop->geometry();
        QRect endRect = ui->oknoDiscleamer->geometry();

        ui->oknoDiscleamer->setGeometry(startRect);
        ui->oknoDiscleamer->setVisible(true);

        QPropertyAnimation *anim = new QPropertyAnimation(ui->oknoDiscleamer, "geometry");
        anim->setDuration(400);
        anim->setStartValue(startRect);
        anim->setEndValue(endRect);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QPropertyAnimation::DeleteWhenStopped);


        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
        ui->oknoDiscleamer->setGraphicsEffect(eff);
        ui->oknoDiscleamer->setVisible(true);

        QPropertyAnimation *a = new QPropertyAnimation(eff, "opacity");
        a->setDuration(50); // длительность в мс
        a->setStartValue(0);
        a->setEndValue(1);
        a->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
        a->start(QPropertyAnimation::DeleteWhenStopped);
        //кнопочки
        QRect startRectK = ui->btnOknoDop->geometry();
        QRect endRectK = ui->oknoKnopohki->geometry();

        ui->oknoKnopohki->setGeometry(startRect);
        ui->oknoKnopohki->setVisible(true);

        QPropertyAnimation *anim2 = new QPropertyAnimation(ui->oknoKnopohki, "geometry");
        anim2->setDuration(400);
        anim2->setStartValue(startRectK);
        anim2->setEndValue(endRectK);
        anim2->setEasingCurve(QEasingCurve::OutCubic);
        anim2->start(QPropertyAnimation::DeleteWhenStopped);


        QGraphicsOpacityEffect *eff2 = new QGraphicsOpacityEffect(this);
        ui->oknoDiscleamer->setGraphicsEffect(eff2);
        ui->oknoDiscleamer->setVisible(true);

        QPropertyAnimation *b = new QPropertyAnimation(eff2, "opacity");
        b->setDuration(50); // длительность в мс
        b->setStartValue(0);
        b->setEndValue(1);
        b->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
        b->start(QPropertyAnimation::DeleteWhenStopped);

        //кнопка выхода
        QRect startRectE = ui->btnOknoDop->geometry();
        QRect endRectE = ui->btnExitGP->geometry();

        ui->btnExitGP->setGeometry(startRect);
        ui->btnExitGP->setVisible(true);

        QPropertyAnimation *anim3 = new QPropertyAnimation(ui->btnExitGP, "geometry");
        anim3->setDuration(400);
        anim3->setStartValue(startRectE);
        anim3->setEndValue(endRectE);
        anim3->setEasingCurve(QEasingCurve::OutCubic);
        anim3->start(QPropertyAnimation::DeleteWhenStopped);


        QGraphicsOpacityEffect *eff3 = new QGraphicsOpacityEffect(this);
        ui->btnExitGP->setGraphicsEffect(eff3);
        ui->btnExitGP->setVisible(true);

        QPropertyAnimation *c = new QPropertyAnimation(eff3, "opacity");
        c->setDuration(50); // длительность в мс
        c->setStartValue(0);
        c->setEndValue(1);
        c->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
        c->start(QPropertyAnimation::DeleteWhenStopped);

        oknoDop = false;
    }
}
void MainWindow::on_btnExitGP_clicked()
{
    oknoDop = true;
    ui->oknoGP->setVisible(false);
    ui->oknoDiscleamer->setVisible(false);
    ui->oknoKnopohki->setVisible(false);
    ui->btnExitGP->setVisible(false);
    ui->oknoZV->setVisible(false);
    ui->oknoHDD->setVisible(false);
}
void MainWindow::on_btnPapkaGP_clicked()
{
    QString path = QFileDialog::getExistingDirectory(
        this,
        "Выберите папку с ган‑паками",
        m_gunPackSourcePath  // стартовая директория (если есть)
        );

    if (!path.isEmpty()) {
        m_gunPackSourcePath = QDir::toNativeSeparators(path);
        ui->leditGunPuck->setText(m_gunPackSourcePath);
        QSettings("MyCompany", "MyGameTool").setValue("Paths/GunPackSource", m_gunPackSourcePath);
    }
}
void MainWindow::on_btnAutoSearchDLS_clicked()
{
    // 1. Ищем в реестре (Rockstar/Steam)
    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString basePath = rsReg.value("InstallFolder").toString();

    if (basePath.isEmpty()) {
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            basePath = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }

    // 2. Формируем целевой путь
    QString targetPath;
    if (!basePath.isEmpty()) {
        targetPath = basePath + "/update/x64/dlcpacks";
        if (QDir(targetPath).exists()) {
            m_dlcPacksTargetPath = QDir::toNativeSeparators(targetPath);
            ui->leditDLS->setText(m_dlcPacksTargetPath);
            QSettings("MyCompany", "MyGameTool").setValue("Paths/DlcPacksTarget", m_dlcPacksTargetPath);
            return;
        }
    }

    // 3. Если не нашли — ручной выбор
    QString manualPath = QFileDialog::getExistingDirectory(
        this,
        "Укажите папку ...\\update\\x64\\dlcpacks",
        "C:/"
        );
    if (!manualPath.isEmpty()) {
        m_dlcPacksTargetPath = QDir::toNativeSeparators(manualPath);
        ui->leditDLS->setText(m_dlcPacksTargetPath);
        QSettings("MyCompany", "MyGameTool").setValue("Paths/DlcPacksTarget", m_dlcPacksTargetPath);
    }

}
void MainWindow::on_btnDLS_clicked()
{
    QString path = QFileDialog::getExistingDirectory(
        this,
        "Выберите папку ...\\update\\x64\\dlcpacks",
        m_dlcPacksTargetPath  // стартовая директория (если есть)
        );

    if (!path.isEmpty()) {
        m_dlcPacksTargetPath = QDir::toNativeSeparators(path);
        ui->leditDLS->setText(m_dlcPacksTargetPath);

        // Сохраняем в настройки
        QSettings("MyCompany", "MyGameTool").setValue("Paths/DlcPacksTarget", m_dlcPacksTargetPath);
    }
}
void MainWindow::on_checkAutoLoadGP_toggled(bool checked) {
    if (!checked) {
        QSettings("MyCompany", "MyGameTool").setValue("checkAutoLoadGP", false);
        return;
    }

    QString pathDLS = QDir::fromNativeSeparators(ui->leditDLS->text()).toLower();
    QString pathGunPack = QDir::fromNativeSeparators(ui->leditGunPuck->text()).toLower();

    if (pathDLS.endsWith("/")) pathDLS.chop(1);
    if (pathGunPack.endsWith("/")) pathGunPack.chop(1);

    // 1. Проверка leditDLS: папка dlcpacks на конце
    bool isDlsOk = pathDLS.endsWith("/dlcpacks") && QDir(pathDLS).exists();

    // 2. Проверка leditGunPuck: отсутствие запрещенных имен в самом ПУТИ
    bool pathHasForbidden = pathGunPack.contains("patchday18ng") || pathGunPack.contains("mpapartment");

    // 3. Проверка содержимого папки GunPuck
    QDir gpDir(pathGunPack);
    bool hasRequiredContent = gpDir.exists("patchday18ng") || gpDir.exists("mpapartment");

    if (isDlsOk && !pathHasForbidden && hasRequiredContent) {
        // Проверка количества файлов для предупреждения
        QStringList entries = gpDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        if (entries.size() > 2) {
            QMessageBox::warning(this, "Внимание", "В папке ганпаков больше 2-х файлов/папок. Убедитесь, что это не вызовет конфликтов.");
        }

        QSettings("MyCompany", "MyGameTool").setValue("checkAutoLoadGP", true);
    } else {
        ui->checkAutoLoadGP->blockSignals(true);
        ui->checkAutoLoadGP->setChecked(false);
        ui->checkAutoLoadGP->blockSignals(false);

        QStringList errors;
        if (!isDlsOk) errors << "- Путь DLS должен заканчиваться на 'dlcpacks'";
        if (pathHasForbidden) errors << "- В самом пути к ганпакам не должно быть имен 'patchday18ng' или 'mpapartment'";
        if (!hasRequiredContent) errors << "- Внутри выбранной папки должна быть папка 'patchday18ng' или 'mpapartment'";

        QMessageBox::critical(this, "Ошибка GunPack", "Проверьте условия:\n" + errors.join("\n"));
    }
}



void MainWindow::on_btnReplaceGunPuck_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualInstallGunPacks(getCurrentConfig());
}




bool MainWindow::copyDirectory(const QString &sourceDir, const QString &targetDir)
{
    QDir sourceDirectory(sourceDir);
    if (!QDir().mkpath(targetDir)) return false;

    QFileInfoList fileList = sourceDirectory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);

    foreach (const QFileInfo &fileInfo, fileList) {
        QString srcPath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
        QString dstPath = QDir::toNativeSeparators(targetDir + "/" + fileInfo.fileName());

        if (fileInfo.isDir()) {
            if (!copyDirectory(srcPath, dstPath)) return false;
        } else {
            // КРИТИЧЕСКОЕ ИЗМЕНЕНИЕ: используем WinAPI вместо QFile::copy
            if (!CopyFileW((LPCWSTR)srcPath.utf16(), (LPCWSTR)dstPath.utf16(), FALSE)) {
                qDebug() << ">>> Ошибка копирования файла в директории:" << GetLastError();
                return false;
            }
        }
    }
    return true;
}

void MainWindow::on_btnReplaceOrigGP_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualRestoreGunPacks(getCurrentConfig());
}

bool MainWindow::removeWithRetry(const QString &path, int maxAttempts) {
    QDir dir(path);
    for (int i = 0; i < maxAttempts; ++i) {
        if (dir.removeRecursively()) {
            return true;
        }
        QThread::msleep(500);  // пауза 500 мс между попытками
    }
    return false;
}


bool MainWindow::restoreGunPacks() {
    qDebug() << ">>> Запуск процесса восстановления GunPacks...";

    QDir backupDir(m_backupPath);
    if (!backupDir.exists()) {
        qDebug() << ">>> Папка бэкапа не существует. Восстанавливать нечего.";
        return true;
    }

    // Получаем список всех файлов и папок в бэкапе
    QFileInfoList backupItems = backupDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);

    if (backupItems.isEmpty()) {
        qDebug() << ">>> Папка бэкапа пуста.";
        return true;
    }

    int restoredCount = 0;
    int totalCount = backupItems.size();
    bool overallSuccess = true;

    foreach (const QFileInfo &backupItem, backupItems) {
        QString itemName = backupItem.fileName();
        QString targetPath = QDir::toNativeSeparators(m_dlcPacksTargetPath + "/" + itemName);
        QString sourcePath = backupItem.absoluteFilePath();

        qDebug() << ">>> Восстановление:" << itemName;

        // 1. Удаляем модовую версию в папке игры (с попытками)
        bool removed = false;
        for(int i = 0; i < 5; ++i) {
            if (!QFile::exists(targetPath) && !QDir(targetPath).exists()) {
                removed = true;
                break;
            }

            if (backupItem.isDir()) {
                QDir dirToRemove(targetPath);
                if (dirToRemove.removeRecursively()) { removed = true; break; }
            } else {
                if (QFile::remove(targetPath)) { removed = true; break; }
            }

            qDebug() << ">>> Файл занят, попытка удаления" << i+1;
            QThread::msleep(1000); // Ждем секунду, если игра еще закрывается
        }

        if (!removed) {
            qDebug() << ">>> ОШИБКА: Не удалось удалить модовый файл:" << targetPath;
            overallSuccess = false;
            continue;
        }

        // 2. Копируем оригинал из бэкапа на место
        bool copied = false;
        if (backupItem.isDir()) {
            if (copyDirectory(sourcePath, targetPath)) copied = true;
        } else {
            if (QFile::copy(sourcePath, targetPath)) copied = true;
        }

        if (copied) {
            restoredCount++;
            // После успешного восстановления удаляем файл из бэкапа
            if (backupItem.isDir()) QDir(sourcePath).removeRecursively();
            else QFile::remove(sourcePath);
            qDebug() << ">>> УСПЕШНО восстановлен:" << itemName;
        } else {
            qDebug() << ">>> ОШИБКА копирования оригинала назад:" << itemName;
            overallSuccess = false;
        }
    }

    // Если папка бэкапа теперь пуста — удаляем её совсем
    if (backupDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        backupDir.rmdir(".");
    }

    m_backupMap.clear(); // Очищаем временную карту в памяти
    return overallSuccess;
}
//авто поиск рпф
QString MainWindow::findUpdateRpf()
{
    // 1. Проверяем реестр (Rockstar/Steam)
    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString basePath = rsReg.value("InstallFolder").toString();

    if (basePath.isEmpty()) {
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            basePath = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }

    // 2. Формируем путь к update.rpf
    if (!basePath.isEmpty()) {
        QString rpfPath = basePath + "/update/update.rpf";
        if (QFile::exists(rpfPath)) {
            return QDir::toNativeSeparators(rpfPath);
        }
    }

    // 3. Ручной поиск, если не нашли
    QString manualPath = QFileDialog::getOpenFileName(
        this,
        "Найдите update.rpf (Grand Theft Auto V Legacy\\update\\update.rpf)",
        "",
        "RPF-файлы (*.rpf)"
        );
    return QDir::toNativeSeparators(manualPath);
}

bool MainWindow::copyUpdateRpfToAppDir(const QString &sourcePath)
{
    if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) {
        qDebug() << "Исходный файл не найден:" << sourcePath;
        return false;
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QString dataFolder = appDir + "/data";
    QString destPath = dataFolder + "/update.rpf";

    qDebug() << "Целевая папка:" << dataFolder;
    qDebug() << "Полный путь к копии:" << destPath;

    QDir dir;
    if (!dir.mkpath(dataFolder)) {
        qDebug() << "Не удалось создать папку:" << dataFolder;
        return false;
    }

    if (QFile::exists(destPath) && !QFile::remove(destPath)) {
        qDebug() << "Не удалось удалить старый файл:" << destPath;
        return false;
    }

    if (QFile::copy(sourcePath, destPath)) {
        if (QFile::exists(destPath)) {
            qDebug() << "Файл скопирован успешно. Обновляем QLabel...";
            ui->leditOrig->setText(QDir::toNativeSeparators(destPath));

            QSettings settings("MyCompany", "MyGameTool");
            settings.setValue("Paths/OriginalFile", destPath);
            settings.sync();
            return true;
        } else {
            qDebug() << "Ошибка: файл не найден после копирования:" << destPath;
            return false;
        }
    } else {
        qDebug() << "Ошибка копирования из" << sourcePath << "в" << destPath;
        return false;
    }
}

QString MainWindow::autoFindUpdateFolder() {
    // 1. Проверяем реестр (Rockstar Games)
    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString basePath = rsReg.value("InstallFolder").toString();

    if (basePath.isEmpty()) {
        // 2. Проверяем Steam
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            basePath = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }

    // 3. Формируем путь к папке update
    if (!basePath.isEmpty()) {
        QString updatePath = basePath + "/update";
        if (QDir(updatePath).exists()) {
            return QDir::toNativeSeparators(updatePath);  // Возвращаем полный путь
        }
    }

    // 4. Если не нашли — возвращаем пустую строку
    return "";
}
//окошко доп функций
void MainWindow::on_btnGanpacOpen_clicked()
{
    //ган пак
    QRect startRectGP = ui->btnGanpacOpen->geometry();
    QRect endRectGP = ui->oknoGP->geometry();

    ui->oknoGP->setGeometry(startRectGP);
    ui->oknoGP->setVisible(true);

    QPropertyAnimation *anim4 = new QPropertyAnimation(ui->oknoGP, "geometry");
    anim4->setDuration(400);
    anim4->setStartValue(startRectGP);
    anim4->setEndValue(endRectGP);
    anim4->setEasingCurve(QEasingCurve::OutCubic);
    anim4->start(QPropertyAnimation::DeleteWhenStopped);


    QGraphicsOpacityEffect *eff4 = new QGraphicsOpacityEffect(this);
    ui->oknoGP->setGraphicsEffect(eff4);
    ui->oknoGP->setVisible(true);

    QPropertyAnimation *r = new QPropertyAnimation(eff4, "opacity");
    r->setDuration(50); // длительность в мс
    r->setStartValue(0);
    r->setEndValue(1);
    r->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
    r->start(QPropertyAnimation::DeleteWhenStopped);
    //закрыть окно
    QRect startRectE = ui->btnGanpacOpen->geometry();
    QRect endRectE = ui->btnExitGP->geometry();

    ui->btnExitGP->setGeometry(startRectE);
    ui->btnExitGP->setVisible(true);

    QPropertyAnimation *anim3 = new QPropertyAnimation(ui->btnExitGP, "geometry");
    anim3->setDuration(400);
    anim3->setStartValue(startRectE);
    anim3->setEndValue(endRectE);
    anim3->setEasingCurve(QEasingCurve::OutCubic);
    anim3->start(QPropertyAnimation::DeleteWhenStopped);


    QGraphicsOpacityEffect *eff3 = new QGraphicsOpacityEffect(this);
    ui->btnExitGP->setGraphicsEffect(eff3);
    ui->btnExitGP->setVisible(true);

    QPropertyAnimation *c = new QPropertyAnimation(eff3, "opacity");
    c->setDuration(50); // длительность в мс
    c->setStartValue(0);
    c->setEndValue(1);
    c->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
    c->start(QPropertyAnimation::DeleteWhenStopped);

    ui->oknoZV->setVisible(false);
    ui->oknoDiscleamer->setVisible(false);
    ui->oknoHDD->setVisible(false);
}
void MainWindow::on_btnZVOpen_clicked()
{
    //звуки
    QRect startRectZV = ui->btnZVOpen->geometry();
    QRect endRectZV = ui->oknoZV->geometry();

    ui->oknoZV->setGeometry(startRectZV);
    ui->oknoZV->setVisible(true);

    QPropertyAnimation *anim5 = new QPropertyAnimation(ui->oknoZV, "geometry");
    anim5->setDuration(400);
    anim5->setStartValue(startRectZV);
    anim5->setEndValue(endRectZV);
    anim5->setEasingCurve(QEasingCurve::OutCubic);
    anim5->start(QPropertyAnimation::DeleteWhenStopped);


    QGraphicsOpacityEffect *eff5 = new QGraphicsOpacityEffect(this);
    ui->oknoZV->setGraphicsEffect(eff5);
    ui->oknoZV->setVisible(true);

    QPropertyAnimation *zv = new QPropertyAnimation(eff5, "opacity");
    zv->setDuration(50); // длительность в мс
    zv->setStartValue(0);
    zv->setEndValue(1);
    zv->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
    zv->start(QPropertyAnimation::DeleteWhenStopped);

    //закрыть окно
    QRect startRectE = ui->btnZVOpen->geometry();
    QRect endRectE = ui->btnExitGP->geometry();

    ui->btnExitGP->setGeometry(startRectE);
    ui->btnExitGP->setVisible(true);

    QPropertyAnimation *anim3 = new QPropertyAnimation(ui->btnExitGP, "geometry");
    anim3->setDuration(400);
    anim3->setStartValue(startRectE);
    anim3->setEndValue(endRectE);
    anim3->setEasingCurve(QEasingCurve::OutCubic);
    anim3->start(QPropertyAnimation::DeleteWhenStopped);


    QGraphicsOpacityEffect *eff3 = new QGraphicsOpacityEffect(this);
    ui->btnExitGP->setGraphicsEffect(eff3);
    ui->btnExitGP->setVisible(true);

    QPropertyAnimation *c = new QPropertyAnimation(eff3, "opacity");
    c->setDuration(50); // длительность в мс
    c->setStartValue(0);
    c->setEndValue(1);
    c->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
    c->start(QPropertyAnimation::DeleteWhenStopped);

    ui->oknoGP->setVisible(false);
    ui->oknoDiscleamer->setVisible(false);
    ui->oknoHDD->setVisible(false);
}

//установка звуков оружия

void MainWindow::on_btnAutoSearchZV_clicked() {
    qDebug() << "=== Автопоиск x64\\audio\\sfx ===";


    // 1. Проверка реестра Rockstar
    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString basePath = rsReg.value("InstallFolder").toString();

    if (basePath.isEmpty()) {
        // 2. Проверка Steam
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            basePath = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }

    // 3. Формируем путь до x64\audio\sfx
    if (!basePath.isEmpty()) {
        QString sfxPath = basePath + "/x64/audio/sfx";
        if (QDir(sfxPath).exists()) {
            m_x64AudioSfxPath = QDir::toNativeSeparators(sfxPath);
            ui->leditPapcaZV->setText(m_x64AudioSfxPath);

            QSettings settings("MyCompany", "MyGameTool");
            settings.setValue("Paths/X64AudioSfx", m_x64AudioSfxPath);


            qDebug() << "Путь до x64\\audio\\sfx найден:" << m_x64AudioSfxPath;
            QMessageBox::information(this, "Найдено", "Путь до x64\\audio\\sfx определён автоматически.");
            return;
        }
    }

    qDebug() << "Автопоиск не удался.";
    QMessageBox::warning(this, "Не найдено", "Автопоиск папки x64\\audio\\sfx не удался. Укажите вручную.");
}

void MainWindow::on_btnZVMod_clicked() {
    QString path = QFileDialog::getExistingDirectory(
        this, "Выберите папку x64\\audio\\sfx игры", m_x64AudioSfxPath
        );

    if (!path.isEmpty()) {
        m_x64AudioSfxPath = QDir::toNativeSeparators(path);
        ui->leditPapcaZV->setText(m_x64AudioSfxPath);

        QSettings settings("MyCompany", "MyGameTool");
        settings.setValue("Paths/X64AudioSfx", m_x64AudioSfxPath);

        qDebug() << "Путь до x64\\audio\\sfx установлен вручную:" << m_x64AudioSfxPath;
    }
}


void MainWindow::on_btnPapkaZV_clicked() {
    QString path = QFileDialog::getExistingDirectory(
        this, "Выберите папку с мод‑файлами .rpf", m_modSoundPath
        );

    if (!path.isEmpty()) {
        m_modSoundPath = QDir::toNativeSeparators(path);
        ui->leditModZV->setText(m_modSoundPath);

        QSettings settings("MyCompany", "MyGameTool");
        settings.setValue("Paths/SoundMod", m_modSoundPath);

        qDebug() << "Путь к модам установлен:" << m_modSoundPath;
    }
}


void MainWindow::on_btnReplaceModZV_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualReplaceSounds(getCurrentConfig());
}

void MainWindow::on_btnReplaceOrigZV_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualRestoreSounds(getCurrentConfig());
}

bool MainWindow::copyRpfFiles(const QString &sourceDir, const QString &targetDir) {
    QDir srcDir(sourceDir);
    QStringList files = srcDir.entryList(QStringList() << "*.rpf", QDir::Files);

    foreach (const QString &fileName, files) {
        QString sourcePath = QDir::toNativeSeparators(srcDir.absoluteFilePath(fileName));
        QString targetPath = QDir::toNativeSeparators(targetDir + "/" + fileName);

        // Снимаем защиту с целевого файла, если он существует
        if (QFile::exists(targetPath)) {
            SetFileAttributesW((LPCWSTR)targetPath.utf16(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW((LPCWSTR)targetPath.utf16()); // Удаляем через WinAPI для надежности
        }

        // Копируем через WinAPI
        if (!CopyFileW((LPCWSTR)sourcePath.utf16(), (LPCWSTR)targetPath.utf16(), FALSE)) {
            DWORD err = GetLastError();
            qDebug() << ">>> ОШИБКА WinAPI при копировании звука:" << fileName << "Код ошибки:" << err;
            return false;
        }
    }
    return true;
}


bool MainWindow::backupOriginalRpfFiles(const QStringList &modFiles) {
    m_soundBackupDir = QCoreApplication::applicationDirPath() + "/sound_backup";
    QDir().mkpath(m_soundBackupDir);

    QDir targetDir(m_x64AudioSfxPath);

    foreach (const QString &fileName, modFiles) {
        QString originalPath = QDir::toNativeSeparators(targetDir.absoluteFilePath(fileName));
        QString backupPath = QDir::toNativeSeparators(m_soundBackupDir + "/" + fileName);

        if (QFile::exists(originalPath)) {
            // Если бэкап уже есть, подготавливаем его к перезаписи
            if (QFile::exists(backupPath)) {
                SetFileAttributesW((LPCWSTR)backupPath.utf16(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW((LPCWSTR)backupPath.utf16());
            }

            // Копируем оригинал в бэкап через WinAPI
            if (!CopyFileW((LPCWSTR)originalPath.utf16(), (LPCWSTR)backupPath.utf16(), FALSE)) {
                qDebug() << ">>> ОШИБКА бэкапа звука:" << fileName << "Error:" << GetLastError();
                return false;
            }
            qDebug() << ">>> Бэкап создан:" << fileName;
        }
    }
    return true;
}

void MainWindow::saveSettings() {
    QSettings s("MyCompany", "MyGameTool");
    s.setValue("ReduxPath", ui->leditRedux->text());
    s.setValue("GamePath", ui->leditPapka->text());
    s.setValue("OrigPath", ui->leditOrig->text());
    s.setValue("GunPackPath", ui->leditGunPuck->text());
    s.setValue("DlsPath", ui->leditDLS->text());
    s.setValue("SoundMod", m_modSoundPath);
    s.setValue("SfxPath", m_x64AudioSfxPath);

    // ДОБАВЛЯЕМ ГАЛОЧКИ
    s.setValue("checkAutoLoad", ui->checkAutoLoad->isChecked());
    s.setValue("checkAutoLoadGP", ui->checkAutoLoadGP->isChecked());
    s.setValue("checkAutoLoadZV", ui->checkAutoLoadZV->isChecked());

    s.sync();
    qDebug() << "Настройки сохранены.";
}

void MainWindow::loadSettings() {
    QSettings s("MyCompany", "MyGameTool");
    ui->leditRedux->setText(s.value("ReduxPath").toString());
    ui->leditPapka->setText(s.value("GamePath").toString());
    ui->leditOrig->setText(s.value("OrigPath").toString());
    ui->leditGunPuck->setText(s.value("GunPackPath").toString());
    ui->leditDLS->setText(s.value("DlsPath").toString());
    m_modSoundPath = s.value("SoundMod").toString();
    m_x64AudioSfxPath = s.value("SfxPath").toString();

    ui->checkAutoLoad->blockSignals(true);
    ui->checkAutoLoad->setChecked(s.value("Settings/AutoLoad", false).toBool());
    ui->checkAutoLoad->blockSignals(false);

    ui->checkAutoLoadGP->blockSignals(true);
    ui->checkAutoLoadGP->setChecked(s.value("checkAutoLoadGP", false).toBool());
    ui->checkAutoLoadGP->blockSignals(false);

    ui->checkAutoLoadZV->blockSignals(true);
    ui->checkAutoLoadZV->setChecked(s.value("checkAutoLoadZV", false).toBool());
    ui->checkAutoLoadZV->blockSignals(false);

    qDebug() << "Настройки загружены: " << ui->leditPapka->text() << ui->leditOrig->text();
}


void MainWindow::on_checkAutoLoadZV_toggled(bool checked) {
    if (!checked) {
        QSettings("MyCompany", "MyGameTool").setValue("checkAutoLoadZV", false);
        return;
    }

    QString pathSfx = QDir::fromNativeSeparators(ui->leditPapcaZV->text()).toLower();
    QString pathModZV = QDir::fromNativeSeparators(ui->leditModZV->text()).toLower(); // Папка с модами звуков

    if (pathSfx.endsWith("/")) pathSfx.chop(1);
    if (pathModZV.endsWith("/")) pathModZV.chop(1);

    // 1. Проверка leditPapcaZV: папка sfx на конце
    bool isSfxOk = pathSfx.endsWith("/sfx") && QDir(pathSfx).exists();

    // 2. Проверка папки с модами (откуда берем RESIDENT.rpf и т.д.)
    QDir modDir(pathModZV);
    bool hasResident = modDir.exists("RESIDENT.rpf");
    bool hasWeapons = modDir.exists("WEAPONS_PLAYER.rpf");
    bool hasZVOu = hasResident || hasWeapons;

    if (isSfxOk && hasZVOu) {
        // Предупреждение о количестве файлов
        QStringList entries = modDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        if (entries.size() > 2) {
            QMessageBox::warning(this, "Внимание", "В папке звуков больше 2-х файлов. Программа заменит только нужные .rpf файлы.");
        }

        QSettings("MyCompany", "MyGameTool").setValue("checkAutoLoadZV", true);
    } else {
        ui->checkAutoLoadZV->blockSignals(true);
        ui->checkAutoLoadZV->setChecked(false);
        ui->checkAutoLoadZV->blockSignals(false);

        QStringList errors;
        if (!isSfxOk) errors << "- Путь к папке игры должен заканчиваться на 'sfx'";
        if (!hasZVOu) errors << "- В папке модов не найдены RESIDENT.rpf или WEAPONS_PLAYER.rpf";

        QMessageBox::critical(this, "Ошибка звуков", "Проверьте пути звуков:\n" + errors.join("\n"));
    }
}

bool MainWindow::installSounds() {
    if (m_modSoundPath.isEmpty() || m_x64AudioSfxPath.isEmpty()) return false;

    // Путь к бэкапу делаем постоянным
    m_soundBackupDir = QCoreApplication::applicationDirPath() + "/sound_backup";
    QDir().mkpath(m_soundBackupDir);

    QDir modDir(m_modSoundPath);
    QStringList modFiles = modDir.entryList(QStringList() << "*.rpf", QDir::Files);

    // ШАГ 1: Умный бэкап
    foreach (const QString &fileName, modFiles) {
        QString originalInGame = m_x64AudioSfxPath + "/" + fileName;
        QString backupPath = m_soundBackupDir + "/" + fileName;

        // Копируем в бэкап ТОЛЬКО если там еще нет такого файла
        if (!QFile::exists(backupPath)) {
            if (QFile::exists(originalInGame)) {
                QFile::copy(originalInGame, backupPath);
                qDebug() << "Забекаплен чистый оригинал:" << fileName;
            }
        }
    }

    // ШАГ 2: Установка модов
    // Используем твою функцию, но убеждаемся, что она ПЕРЕЗАПИСЫВАЕТ файлы
    return copyRpfFiles(m_modSoundPath, m_x64AudioSfxPath);
}
bool MainWindow::safeCopy(const QString &src, const QString &destFolder, bool isRestoring) {
    if (src.isEmpty() || destFolder.isEmpty()) return false;

    QFileInfo srcInfo(src);
    QString fullDestPath = QDir::toNativeSeparators(destFolder + "/" + srcInfo.fileName());
    QString nativeSource = QDir::toNativeSeparators(src);

    // Если пути одинаковые — это ошибка настроек
    if (nativeSource.toLower() == fullDestPath.toLower()) {
        qDebug() << "!!! ОШИБКА: Источник и цель — один и тот же файл!";
        return false;
    }

    // Снимаем защиту и удаляем старый файл, если он мешает
    if (QFile::exists(fullDestPath)) {
        SetFileAttributesW((LPCWSTR)fullDestPath.utf16(), FILE_ATTRIBUTE_NORMAL);
        QFile::remove(fullDestPath);
    }

    if (CopyFileW((LPCWSTR)nativeSource.utf16(), (LPCWSTR)fullDestPath.utf16(), FALSE)) {
        if (isRestoring) {
            SetFileAttributesW((LPCWSTR)fullDestPath.utf16(), FILE_ATTRIBUTE_READONLY);
        }
        return true;
    }
    return false;
}


bool MainWindow::restoreSounds() {
    if (m_x64AudioSfxPath.isEmpty()) return false;

    m_soundBackupDir = QCoreApplication::applicationDirPath() + "/sound_backup";
    QDir bkpDir(m_soundBackupDir);
    if (!bkpDir.exists()) return true;

    QFileInfoList files = bkpDir.entryInfoList(QStringList() << "*.rpf", QDir::Files);
    if (files.isEmpty()) return true;

    int failCount = 0;

    foreach (const QFileInfo &fileInfo, files) {
        QString backupPath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
        QString targetPath = QDir::toNativeSeparators(m_x64AudioSfxPath + "/" + fileInfo.fileName());

        // Снимаем защиту с модового файла в папке игры
        if (QFile::exists(targetPath)) {
            SetFileAttributesW((LPCWSTR)targetPath.utf16(), FILE_ATTRIBUTE_NORMAL);
            if (!DeleteFileW((LPCWSTR)targetPath.utf16())) {
                qDebug() << ">>> ОШИБКА WinAPI: Не удалось удалить модовый звук:" << GetLastError();
                failCount++;
                continue;
            }
        }

        // Возвращаем чистый файл из бэкапа
        if (!CopyFileW((LPCWSTR)backupPath.utf16(), (LPCWSTR)targetPath.utf16(), FALSE)) {
            qDebug() << ">>> ОШИБКА WinAPI при возврате звука:" << GetLastError();
            failCount++;
        }
    }

    if (failCount == 0) {
        QDir(m_soundBackupDir).removeRecursively();
        return true;
    }
    return false;
}
//HDD
void MainWindow::on_btnHDD_OpenDis_clicked()
{
    //hdd
    QRect startRectHHD = ui->btnHDD_OpenDis->geometry();
    QRect endRectHHD = ui->oknoHDD->geometry();

    ui->oknoHDD->setGeometry(startRectHHD);
    ui->oknoHDD->setVisible(true);

    QPropertyAnimation *anim6 = new QPropertyAnimation(ui->oknoHDD, "geometry");
    anim6->setDuration(400);
    anim6->setStartValue(startRectHHD);
    anim6->setEndValue(endRectHHD);
    anim6->setEasingCurve(QEasingCurve::OutCubic);
    anim6->start(QPropertyAnimation::DeleteWhenStopped);


    QGraphicsOpacityEffect *eff6 = new QGraphicsOpacityEffect(this);
    ui->oknoHDD->setGraphicsEffect(eff6);
    ui->oknoHDD->setVisible(true);

    QPropertyAnimation *hdd = new QPropertyAnimation(eff6, "opacity");
    hdd->setDuration(50); // длительность в мс
    hdd->setStartValue(0);
    hdd->setEndValue(1);
    hdd->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
    hdd->start(QPropertyAnimation::DeleteWhenStopped);

    //кнопка выхода
    QRect startRectE = ui->btnHDD_OpenDis->geometry();
    QRect endRectE = ui->btnExitGP->geometry();

    ui->btnExitGP->setGeometry(startRectE);
    ui->btnExitGP->setVisible(true);

    QPropertyAnimation *anim3 = new QPropertyAnimation(ui->btnExitGP, "geometry");
    anim3->setDuration(400);
    anim3->setStartValue(startRectE);
    anim3->setEndValue(endRectE);
    anim3->setEasingCurve(QEasingCurve::OutCubic);
    anim3->start(QPropertyAnimation::DeleteWhenStopped);


    QGraphicsOpacityEffect *eff3 = new QGraphicsOpacityEffect(this);
    ui->btnExitGP->setGraphicsEffect(eff3);
    ui->btnExitGP->setVisible(true);

    QPropertyAnimation *c = new QPropertyAnimation(eff3, "opacity");
    c->setDuration(50); // длительность в мс
    c->setStartValue(0);
    c->setEndValue(1);
    c->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
    c->start(QPropertyAnimation::DeleteWhenStopped);

    ui->oknoHDD->setVisible(true);
    ui->oknoDiscleamer->setVisible(false);
    ui->oknoGP->setVisible(false);
    ui->oknoZV->setVisible(false);
}
void MainWindow::on_btnNext_clicked()
{
    ui->Instruction->setVisible(false);
    ui->btnNext->setVisible(false);
    ui->lblInstrucktion->setVisible(false);
}

void MainWindow::on_btnSaveTime_clicked()
{
    QSettings settings("MyCompany", "MyGameTool");
    settings.setValue("pauseTime", ui->leditTimeVvod->text());
    settings.sync(); // Сброс на диск

    // 2. Записываем число внутрь .bat файла
    saveTimeToFile();

    qDebug() << "Время сохранено!";

}

// В заголовочном файле: QProcess *batchProcess = nullptr;

void MainWindow::on_btnOn_Off_HDD_clicked() {

    runBatch();
}



void MainWindow::on_checkAutoOn_Off_toggled(bool checked)
{
    // 1. Сохраняем состояние галочки в реестр/файл настроек
    QSettings settings("MyCompany", "MyGameTool");
    settings.setValue("autoStart", checked);
    settings.sync(); // Принудительно записываем на диск

    if (checked) {
        // 2. Если включили: сбрасываем флаг и запускаем таймер
        gtaWasRunning = false;
        checkTimer->start(2000); // Проверка каждые 2 сек
        qDebug() << "Авто-режим .bat включен";
    } else {
        // 3. Если выключили: останавливаем таймер
        checkTimer->stop();
        gtaWasRunning = false;
        qDebug() << "Авто-режим отключен.";
    }
}

void MainWindow::saveTimeToFile() {
    QString newTime = ui->leditTimeVvod->text();
    QString filePath = QCoreApplication::applicationDirPath() + "/time/time.bat";
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString content = file.readAll();
    file.close();

    // РЕШЕНИЕ: Используем QRegularExpression для поиска первого вхождения
    QRegularExpression re("(timeout /t )(\\d+)");
    QRegularExpressionMatch match = re.match(content);

    if (match.hasMatch()) {
        // Мы НЕ используем content.replace(re, ...), так как это заменит ВСЕ вхождения.
        // Вместо этого мы берем позицию и длину ТОЛЬКО первого числа (группа 2).
        int start = match.capturedStart(2);
        int length = match.capturedLength(2);

        // Заменяем только этот сегмент строки
        content.replace(start, length, newTime);
        qDebug() << "Обновлено время на:" << newTime;
    }

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&file);
        out << content;
        file.close();

        // Unblock file for execution
        QString powerShellCmd = "powershell -Command \"Unblock-File -Path '" + filePath + "'\"";
        QProcess::execute(powerShellCmd);
    }
}



void MainWindow::checkGtaProcess() {
    if (!ui->checkAutoOn_Off->isChecked()) return;

    QProcess tasklist;
    tasklist.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq GTA5.exe");
    tasklist.waitForFinished();
    QString output = tasklist.readAllStandardOutput();

    bool gtaFound = output.contains("GTA5.exe");

    if (gtaFound && !gtaWasRunning) {
        gtaWasRunning = true;
        runBatch();
    }
    else if (!gtaFound) {
        gtaWasRunning = false;
    }
}

void MainWindow::handleProcessError(QProcess::ProcessError error) {
    qCritical() << "ОШИБКА QProcess:" << error;
    qCritical() << "Ошибка бат:" << batchProcess->errorString();
}

void MainWindow::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qDebug() << "Процесс завершен. Код выхода:" << exitCode << "Статус:" << exitStatus;
    // Прочитай весь оставшийся вывод, если он есть
    readProcessOutput();
}

void MainWindow::readProcessOutput() {
    // Выводим весь стандартный вывод (stdout и stderr) в консоль
    qDebug() << "Вывод батника:" << batchProcess->readAllStandardOutput();
    qDebug() << "Ошибки батника:" << batchProcess->readAllStandardError();
}

void MainWindow::on_leditTimeVvod_editingFinished()
{
    QSettings settings("MyCompany", "MyGameTool");
    settings.setValue("pauseTime", ui->leditTimeVvod->text());
    settings.sync(); // Принудительно сохраняем на диск
    qDebug() << "Время сохранено в настройки:" << ui->leditTimeVvod->text();
}
void MainWindow::runBatch() {
    saveTimeToFile(); // Сначала сохраняем актуальное время в файл

    QString folderPath = QCoreApplication::applicationDirPath() + "/time/";
    QString filePath = folderPath + "time.bat";

    std::wstring nativeFile = filePath.toStdWString();
    std::wstring nativeDir = folderPath.toStdWString();

    // Запускаем батник. 5-й параметр (nativeDir) лечит ошибку с PsSuspend64
    ShellExecute(NULL, L"open", nativeFile.c_str(), NULL, nativeDir.c_str(), SW_SHOWNORMAL);

    qDebug() << "Батник запущен";
}

bool MainWindow::isValidRpfPath(const QString &filePath) {
    if (filePath.isEmpty()) return false;
    QFileInfo fi(filePath);
    return fi.exists() && fi.fileName().toLower() == "update.rpf";
}
//Notif
void MainWindow::on_btnNotification_clicked()
{
    ui->btnExitNF->setVisible(true);

    QRect startRectNF = ui->btnNotification->geometry();
    QRect endRectNF = ui->oknoNF->geometry();

    ui->oknoNF->setGeometry(startRectNF);
    ui->oknoNF->setVisible(true);

    QPropertyAnimation *anim00 = new QPropertyAnimation(ui->oknoNF, "geometry");
    anim00->setDuration(400);
    anim00->setStartValue(startRectNF);
    anim00->setEndValue(endRectNF);
    anim00->setEasingCurve(QEasingCurve::OutCubic);
    anim00->start(QPropertyAnimation::DeleteWhenStopped);


    QGraphicsOpacityEffect *eff88 = new QGraphicsOpacityEffect(this);
    ui->oknoNF->setGraphicsEffect(eff88);
    ui->oknoNF->setVisible(true);

    QPropertyAnimation *zov = new QPropertyAnimation(eff88, "opacity");
    zov->setDuration(50); // длительность в мс
    zov->setStartValue(0);
    zov->setEndValue(1);
    zov->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
    zov->start(QPropertyAnimation::DeleteWhenStopped);

    //закрыть окно
    QRect startRectKLS = ui->btnNotification->geometry();
    QRect endRectKLS = ui->btnExitNF->geometry();

    ui->btnExitNF->setGeometry(startRectKLS);
    ui->btnExitNF->setVisible(true);

    QPropertyAnimation *anim333 = new QPropertyAnimation(ui->btnExitNF, "geometry");
    anim333->setDuration(400);
    anim333->setStartValue(startRectKLS);
    anim333->setEndValue(endRectKLS);
    anim333->setEasingCurve(QEasingCurve::OutCubic);
    anim333->start(QPropertyAnimation::DeleteWhenStopped);


    QGraphicsOpacityEffect *eff99 = new QGraphicsOpacityEffect(this);
    ui->btnExitNF->setGraphicsEffect(eff99);
    ui->btnExitNF->setVisible(true);

    QPropertyAnimation *p = new QPropertyAnimation(eff99, "opacity");
    p->setDuration(50); // длительность в мс
    p->setStartValue(0);
    p->setEndValue(1);
    p->setEasingCurve(QEasingCurve::InBack); // тип сглаживания
    p->start(QPropertyAnimation::DeleteWhenStopped);
}
void MainWindow::on_btnDownload_clicked()
{
    QDesktopServices::openUrl(QUrl("https://majestic-mods.ru/load/soft/replacex_0_9_0_beta/14-1-0-192"));
}
void MainWindow::on_btnExitNF_clicked()
{
    ui->oknoNF->setVisible(false);
    ui->btnExitNF->setVisible(false);
}
