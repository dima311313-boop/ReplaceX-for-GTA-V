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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    ui->oknoKnopohki->setVisible(false);
    ui->oknoDiscleamer->setVisible(false);
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
        "   border: 2px solid #cccccc;"    // Серый цвет
        "   border-radius: 4px;"          // Скруглённые углы (опционально)        // Светло‑серый
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

    qDebug() << "Программа запущенна";
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

    delete ui;
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

    QFileInfo fileInfo(sourcePath);
    QString fullDestPath = destFolder + "/" + fileInfo.fileName();

    if (QFile::exists(fullDestPath)) {
        if (!QFile::remove(fullDestPath)) return false;
    }
    return QFile::copy(sourcePath, fullDestPath);
}

//АВТОМАТИЗАЦИЯ
bool MainWindow::isFileBusy(QString filePath) {

    std::wstring wPath = QDir::toNativeSeparators(filePath).toStdWString();

    HANDLE hFile = CreateFileW(
        (LPCWSTR)wPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
        );

    if (hFile == INVALID_HANDLE_VALUE) {
        return true;
    }

    CloseHandle(hFile);
    return false;
}



void MainWindow::checkProcessLoop() {
    if (!ui->checkAutoLoad->isChecked() && !ui->checkAutoLoadGP->isChecked() && !ui->checkAutoLoadZV->isChecked())
        return;

    bool isGameActive = false;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe)) {
        do {
            if (QString::fromWCharArray(pe.szExeFile).toLower() == "gta5.exe") {
                isGameActive = true;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);

    static bool wasGameRunning = false;

    // 1. ИГРА ЗАПУСТИЛАСЬ
    if (isGameActive && !wasGameRunning) {
        wasGameRunning = true;
        qDebug() << ">>> ИГРА ЗАПУЩЕНА. Подмена файлов...";
        ui->miniProgress->setText("Игра запущена. Подмена файлов...");

        // Установка редукса (если включено)
        if (ui->checkAutoLoad->isChecked()) {
            if (copyFileToGame(ui->leditRedux->text(), ui->leditPapka->text())) {
                qDebug() << ">>> УСПЕХ: Редукс установлен.";
                ui->miniProgress->setText("Редукс установлен!");
                isReduxInstalled = true;
            } else {
                qDebug() << ">>> ОШИБКА: Не удалось установить редукс.";
                ui->miniProgress->setText("Ошибка установки редукса");
            }
        }

        // Установка ган‑паков (если включено)
        if (ui->checkAutoLoadGP->isChecked()) {
            if (installGunPacks()) {
                qDebug() << ">>> УСПЕХ: Ган‑паки установлены.";
                ui->miniProgress->setText("Ган‑паки установлены");
                isGunPackInstalled = true;
            } else {
                qDebug() << ">>> ОШИБКА: Не удалось установить ган‑паки.";
                ui->miniProgress->setText("Ошибка установки ган‑паков");
            }
        }

        // Установка звуков (если включено)
        if (ui->checkAutoLoadZV->isChecked()) {
            if (installSounds()) {
                qDebug() << ">>> УСПЕХ: Звуки установлены.";
                ui->miniProgress->setText("Звуки установлены!");
                isSoundsInstalled = true;
            } else {
                qDebug() << ">>> ОШИБКА: Не удалось установить звуки.";
                ui->miniProgress->setText("Ошибка установки звуков");
            }
        }
    }

    // 2. ИГРА ЗАКРЫТА
    if (!isGameActive && wasGameRunning) {
        qDebug() << ">>> ИГРА ЗАКРЫТА. Начало восстановления...";
        ui->miniProgress->setText("Восстановление оригинальных файлов");

        // Завершение сопутствующих процессов
        QStringList rgsProcesses = {"SocialClubHelper.exe", "Launcher.exe", "RockstarService.exe"};
        for (const QString &proc : rgsProcesses) {
            killProcessByName(proc);
        }

        QThread::msleep(2000);  // Даём системе время на освобождение файлов

        // Восстановление редукса (если был установлен)
        if (ui->checkAutoLoad->isChecked() && isReduxInstalled) {
            if (copyFileToGame(ui->leditOrig->text(), ui->leditPapka->text())) {
                qDebug() << ">>> ОРИГИНАЛ ВОССТАНОВЛЕН (редукс).";
                ui->miniProgress->setText("Оригинальный рпф установлен");
            } else {
                qDebug() << ">>> ОШИБКА: Не удалось восстановить оригинал (редукс).";
                ui->miniProgress->setText("Ошибка установки оригинального рпф");
            }
            isReduxInstalled = false;
        }

        // Восстановление ган‑паков (если были установлены)
        if (ui->checkAutoLoadGP->isChecked() && isGunPackInstalled) {
            if (restoreGunPacks()) {
                qDebug() << ">>> ИСХОДНЫЕ ГАН‑ПАКИ ВОССТАНОВЛЕНЫ.";
                ui->miniProgress->setText("Исходные ган‑паки установлены");
            } else {
                qDebug() << ">>> ОШИБКА: Не удалось восстановить ган‑паки.";
                ui->miniProgress->setText("Ошибка установки оригинальных ган‑паков");
            }
            isGunPackInstalled = false;
        }

        // Восстановление звуков (если были установлены)
        if (ui->checkAutoLoadZV->isChecked() && isSoundsInstalled) {
            if (restoreSounds()) {
                qDebug() << ">>> ОРИГИНАЛЬНЫЕ ЗВУКИ ВОССТАНОВЛЕНЫ.";
                ui->miniProgress->setText("Оригинальные звуки восстановлены");
            } else {
                qDebug() << ">>> ОШИБКА: Не удалось восстановить звуки.";
                ui->miniProgress->setText("Ошибка восстановления звуков");
            }
            isSoundsInstalled = false;
        }

        wasGameRunning = false;
    }
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
    if (copyFileToGame(ui->leditOrig->text(), ui->leditPapka->text())) {
        QMessageBox::critical(this, "Ошибка.", "Успех! Оригинал установлен.");
        isReduxInstalled = false;
    }
}

void MainWindow::on_btnReplaceRedux_clicked() {
    if (copyFileToGame(ui->leditRedux->text(), ui->leditPapka->text())) {
        QMessageBox::information(this, "Ошибка.", "Успех! Редукс установлен.");
        isReduxInstalled = true;
    }
}

void MainWindow::on_checkAutoLoad_toggled(bool checked) {
    QSettings("MyCompany", "MyGameTool").setValue("Settings/AutoLoad", checked);
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
    ui->oknoKnopohki->setVisible(true);
    ui->oknoDiscleamer->setVisible(true);
    ui->btnExitGP->setVisible(true);
}
void MainWindow::on_btnExitGP_clicked()
{
    ui->oknoGP->setVisible(false);
    ui->oknoDiscleamer->setVisible(false);
    ui->oknoKnopohki->setVisible(false);
    ui->btnExitGP->setVisible(false);
    ui->oknoZV->setVisible(false);
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
void MainWindow::on_checkAutoLoadGP_toggled(bool checked)
{
    QSettings settings("MyCompany", "MyGameTool");
    settings.setValue("checkAutoLoadGP", checked);
    qDebug() << "Автоустановка ган‑паков:" << (checked ? "ВКЛЮЧЕНА" : "ВЫКЛЮЧЕНА");
}


void MainWindow::on_btnReplaceGunPuck_clicked()
{
    if (m_gunPackSourcePath.isEmpty() || m_dlcPacksTargetPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не указаны пути к источникам или цели!");
        return;
    }

    QDir sourceDir(m_gunPackSourcePath);
    QDir targetDir(m_dlcPacksTargetPath);

    int successCount = 0;
    int failCount = 0;

    // Получаем список элементов для копирования
    QFileInfoList items = sourceDir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::Name
        );

    foreach (const QFileInfo &item, items) {
        QString sourceItem = item.absoluteFilePath();
        QString targetItem = targetDir.absoluteFilePath(item.fileName());

        // 1. Если целевая сущность существует — делаем бэкап
        if (QFile::exists(targetItem) || QDir(targetItem).exists()) {
            QString backupItem = m_backupPath + "/" + item.fileName();

            // Удаляем старый бэкап, если есть
            if (QFile::exists(backupItem)) {
                QFile::remove(backupItem);
            } else if (QDir(backupItem).exists()) {
                QDir(backupItem).removeRecursively();
            }

            // Копируем в бэкап
            if (item.isDir()) {
                if (!copyDirectory(targetItem, backupItem)) {
                    failCount++;
                    continue;
                }
            } else {
                if (!QFile::copy(targetItem, backupItem)) {
                    failCount++;
                    continue;
                }
            }

            // Запоминаем путь к бэкапу
            m_backupMap[targetItem] = backupItem;
        }

        // 2. Удаляем целевую сущность (если есть)
        if (QFile::exists(targetItem)) {
            QFile::remove(targetItem);
        } else if (QDir(targetItem).exists()) {
            QDir(targetItem).removeRecursively();
        }

        // 3. Копируем из источника в цель
        if (item.isDir()) {
            if (copyDirectory(sourceItem, targetItem)) {
                successCount++;
            } else {
                failCount++;
            }
        } else {
            if (QFile::copy(sourceItem, targetItem)) {
                successCount++;
            } else {
                failCount++;
            }
        }
    }

    // Уведомление
    if (failCount == 0) {
        QMessageBox::information(this, "Бэкап и копирование",
                                 QString("Скопировано: %1\nБэкапы созданы").arg(successCount));
    } else {
        QMessageBox::warning(this, "Ошибки",
                             QString("Успешно: %1\nС ошибками: %2").arg(successCount).arg(failCount));
    }
}


bool MainWindow::copyDirectory(const QString &sourceDir, const QString &targetDir)
{
    QDir sourceDirectory(sourceDir);
    QDir targetDirectory(targetDir);

    // Создаём целевую папку, если её нет
    if (!targetDirectory.exists()) {
        if (!targetDirectory.mkpath(".")) {
            return false;
        }
    }

    // Получаем список элементов (файлы + папки)
    QFileInfoList fileList = sourceDirectory.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot
        );

    foreach (const QFileInfo &fileInfo, fileList) {
        QString sourcePath = fileInfo.absoluteFilePath();
        QString targetPath = targetDir + "/" + fileInfo.fileName();

        if (fileInfo.isDir()) {
            // Рекурсивно копируем подпапку
            if (!copyDirectory(sourcePath, targetPath)) {
                return false;
            }
        } else {
            // Копируем файл
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);  // удаляем существующий
            }
            if (!QFile::copy(sourcePath, targetPath)) {
                return false;
            }
        }
    }
    return true;
}
void MainWindow::on_btnReplaceOrigGP_clicked()
{
    if (m_backupMap.isEmpty()) {
        QMessageBox::information(this, "Нет бэкапов", "Нет сохранённых бэкапов для восстановления.");
        return;
    }

    int restoredCount = 0;
    int failCount = 0;

    for (auto it = m_backupMap.begin(); it != m_backupMap.end(); ++it) {
        const QString &targetPath = it.key();
        const QString &backupPath = it.value();

        // Удаляем текущую версию
        if (QFile::exists(targetPath)) {
            QFile::remove(targetPath);
        } else if (QDir(targetPath).exists()) {
            QDir(targetPath).removeRecursively();
        }

        // Восстанавливаем из бэкапа
        if (QDir(backupPath).exists()) {
            if (copyDirectory(backupPath, targetPath)) {
                restoredCount++;
            } else {
                failCount++;
            }
        } else if (QFile::exists(backupPath)) {
            if (QFile::copy(backupPath, targetPath)) {
                restoredCount++;
            } else {
                failCount++;
            }
        } else {
            failCount++;
        }
    }

    // Если ошибок не было — удаляем папку бэкапов
    if (failCount == 0) {
        QDir backupDir(m_backupPath);
        if (backupDir.exists()) {
            backupDir.removeRecursively();  // Удаляем всю папку
        }
        m_backupMap.clear();  // Очищаем карту

        QMessageBox::information(this, "Готово",
                                 QString("Восстановлено: %1 элементов. Бэкапы удалены.").arg(restoredCount));
    } else {
        QMessageBox::warning(this, "Частичное восстановление",
                             QString("Восстановлено: %1\nНе удалось: %2\nБэкапы сохранены для повторной попытки.")
                                 .arg(restoredCount).arg(failCount));
    }
}
bool MainWindow::installGunPacks() {
    if (m_gunPackSourcePath.isEmpty() || m_dlcPacksTargetPath.isEmpty())
        return false;

    QDir sourceDir(m_gunPackSourcePath);
    QDir targetDir(m_dlcPacksTargetPath);

    int successCount = 0;
    int failCount = 0;

    QFileInfoList items = sourceDir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::Name
        );

    foreach (const QFileInfo &item, items) {
        QString sourceItem = item.absoluteFilePath();
        QString targetItem = targetDir.absoluteFilePath(item.fileName());

        // 1. Бэкап существующего файла/папки в целевой директории
        if (QFile::exists(targetItem) || QDir(targetItem).exists()) {
            QString backupItem = m_backupPath + "/" + item.fileName();

            // Удаляем старый бэкап, если есть
            if (QFile::exists(backupItem)) {
                QFile::remove(backupItem);
            } else if (QDir(backupItem).exists()) {
                QDir(backupItem).removeRecursively();
            }

            // Копируем в бэкап
            if (item.isDir()) {
                if (!copyDirectory(targetItem, backupItem)) {
                    failCount++;
                    continue;
                }
            } else {
                if (!QFile::copy(targetItem, backupItem)) {
                    failCount++;
                    continue;
                }
            }

            m_backupMap[targetItem] = backupItem;
        }

        // 2. Удаляем целевую сущность (если есть)
        if (QFile::exists(targetItem)) {
            QFile::remove(targetItem);
        } else if (QDir(targetItem).exists()) {
            QDir(targetItem).removeRecursively();
        }

        // 3. Копируем из источника в цель
        if (item.isDir()) {
            if (copyDirectory(sourceItem, targetItem)) {
                successCount++;
            } else {
                failCount++;
            }
        } else {
            if (QFile::copy(sourceItem, targetItem)) {
                successCount++;
            } else {
                failCount++;
            }
        }
    }

    return (failCount == 0);
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
    if (m_backupMap.isEmpty()) {
        qDebug() << ">>> Нет бэкапов для восстановления.";
        return true;
    }

    int restoredCount = 0;
    int failCount = 0;

    for (auto it = m_backupMap.begin(); it != m_backupMap.end(); ++it) {
        const QString &targetPath = it.key();      // Куда восстанавливать (в игре)
        const QString &backupPath = it.value();   // Откуда брать бэкап

        // 1. Проверяем существование бэкапа
        if (!QFile::exists(backupPath) && !QDir(backupPath).exists()) {
            qDebug() << ">>> ОШИБКА: Бэкап не найден:" << backupPath;
            ui->miniProgress->setText("ОШИБКА: Бэкап не найден");
            failCount++;
            continue;
        }

        // 2. Если целевая папка существует — удаляем её содержимое (но не саму папку!)
        QDir targetDir(targetPath);
        if (targetDir.exists()) {
            // Удаляем все файлы/папки внутри targetPath, но не сам targetPath
            QStringList entries = targetDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
            foreach (const QString &entry, entries) {
                QFileInfo entryInfo(targetDir.absoluteFilePath(entry));
                if (entryInfo.isDir()) {
                    if (!removeWithRetry(targetDir.absoluteFilePath(entry), 3)) {
                        qDebug() << ">>> ОШИБКА: Не удалось удалить содержимое:" << entryInfo.absoluteFilePath();
                        failCount++;
                        continue;
                    }
                } else {
                    if (!QFile::remove(entryInfo.absoluteFilePath())) {
                        qDebug() << ">>> ОШИБКА: Не удалось удалить файл:" << entryInfo.absoluteFilePath();
                        failCount++;
                        continue;
                    }
                }
            }
        } else {
            // Если целевая папка не существует — создаём её
            if (!targetDir.mkpath(".")) {
                qDebug() << ">>> ОШИБКА: Не удалось создать папку:" << targetPath;
                failCount++;
                continue;
            }
        }

        // 3. Копируем содержимое бэкапа в целевую папку
        if (QDir(backupPath).exists()) {
            if (copyDirectory(backupPath, targetPath)) {
                restoredCount++;
            } else {
                qDebug() << ">>> ОШИБКА: Не удалось восстановить папку из бэкапа:" << backupPath;
                failCount++;
            }
        } else if (QFile::exists(backupPath)) {
            if (QFile::copy(backupPath, targetPath + "/" + QFileInfo(backupPath).fileName())) {
                restoredCount++;
            } else {
                qDebug() << ">>> ОШИБКА: Не удалось восстановить файл из бэкапа:" << backupPath;
                failCount++;
            }
        }
    }

    // 4. Если всё успешно — удаляем папку бэкапов
    if (failCount == 0) {
        QDir backupDir(m_backupPath);
        backupDir.removeRecursively();
        m_backupMap.clear();
        qDebug() << ">>> Все ган‑паки восстановлены. Бэкапы очищены.";
        return true;
    } else {
        qDebug() << ">>> Восстановление с ошибками: успешно" << restoredCount
                 << ", ошибок" << failCount
                 << ". Бэкапы сохранены для повторной попытки.";
        return false;
    }
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
    ui->oknoGP->setVisible(true);
    ui->oknoDiscleamer->setVisible(false);
    ui->oknoZV->setVisible(false);
}
void MainWindow::on_btnZVOpen_clicked()
{
    ui->oknoZV->setVisible(true);
    ui->oknoGP->setVisible(false);
    ui->oknoDiscleamer->setVisible(false);
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
    qDebug() << "=== Установка мод‑файлов .rpf ===";

    if (m_modSoundPath.isEmpty() || m_x64AudioSfxPath.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Укажите пути к модам и папке x64\\audio\\sfx.");
        return;
    }

    if (!QDir(m_modSoundPath).exists() || !QDir(m_x64AudioSfxPath).exists()) {
        QMessageBox::critical(this, "Ошибка", "Папки не найдены.");
        return;
    }

    QDir modDir(m_modSoundPath);
    QStringList modFiles = modDir.entryList(
        QStringList() << "*.rpf",
        QDir::Files
        );

    if (modFiles.isEmpty()) {
        QMessageBox::warning(this, "Нет файлов", "В папке модов нет файлов .rpf.");
        return;
    }

    // Бэкап оригиналов
    m_soundBackupDir = QCoreApplication::applicationDirPath() + "/sound_backup";
    QDir().mkpath(m_soundBackupDir);

    if (!backupOriginalRpfFiles(modFiles)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать бэкап.");
        return;
    }

    // Копирование модов
    if (copyRpfFiles(m_modSoundPath, m_x64AudioSfxPath)) {
        QMessageBox::information(this, "Готово", "Моды установлены!");
    } else {
        QMessageBox::critical(this, "Ошибка", "Установка не удалась.");
    }
}



void MainWindow::on_btnReplaceOrigZV_clicked() {
    qDebug() << "=== Безопасное восстановление оригиналов ===";

    if (m_x64AudioSfxPath.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Укажите папку x64\\audio\\sfx.");
        return;
    }

    m_soundBackupDir = QCoreApplication::applicationDirPath() + "/sound_backup";
    if (!QDir(m_soundBackupDir).exists()) {
        QMessageBox::warning(this, "Нет бэкапа", "Папка sound_backup не найдена.");
        return;
    }

    QDir bkpDir(m_soundBackupDir);
    QFileInfoList files = bkpDir.entryInfoList(QStringList() << "*.rpf", QDir::Files);

    if (files.isEmpty()) {
        QMessageBox::warning(this, "Бэкап пуст", "В sound_backup нет файлов .rpf.");
        return;
    }

    int restored = 0;
    foreach (const QFileInfo &file, files) {
        QString srcPath = file.absoluteFilePath();
        QString destPath = m_x64AudioSfxPath + "/" + file.fileName();

        // 1. Удаляем существующий файл (если есть)
        if (QFile::exists(destPath)) {
            if (!QFile::remove(destPath)) {
                qDebug() << "Не удалось удалить для замены:" << destPath;
                continue;
            }
        }

        // 2. Копируем оригинал из бэкапа
        if (QFile::copy(srcPath, destPath)) {
            restored++;
            qDebug() << "Восстановлен:" << file.fileName();
        } else {
            qDebug() << "Ошибка восстановления:" << srcPath;
        }
    }

    // 3. Анализ результата и удаление бэкапа (если всё прошло успешно)
    if (restored == files.size()) {
        // Все файлы восстановлены — удаляем папку бэкапа
        bool backupRemoved = QDir(m_soundBackupDir).removeRecursively();
        if (backupRemoved) {
            qDebug() << "Папка бэкапа удалена:" << m_soundBackupDir;
            QMessageBox::information(
                this, "Готово",
                QString("Восстановлено %1 файлов. Папка бэкапа удалена.").arg(restored)
                );
        } else {
            qDebug() << "Не удалось удалить папку бэкапа:" << m_soundBackupDir;
            QMessageBox::warning(
                this, "Предупреждение",
                QString("Восстановлено %1 файлов, но папку бэкапа не удалось удалить вручную.").arg(restored)
                );
        }
    } else {
        // Часть файлов не восстановилась — оставляем бэкап для повторной попытки
        QMessageBox::warning(
            this, "Частичное восстановление",
            QString("Восстановлено %1 из %2 файлов. Папка бэкапа сохранена для повторной попытки.")
                .arg(restored).arg(files.size())
            );
    }

    qDebug() << "Восстановление завершено. Восстановлено:" << restored;
}


bool MainWindow::copyRpfFiles(const QString &sourceDir, const QString &targetDir) {
    QDir srcDir(sourceDir);
    QStringList files = srcDir.entryList(QStringList() << "*.rpf", QDir::Files);

    foreach (const QString &fileName, files) {
        QString sourcePath = srcDir.absoluteFilePath(fileName);
        QString targetPath = targetDir + "/" + fileName;

        // Удаляем существующий файл (если есть)
        if (QFile::exists(targetPath)) {
            QFile::remove(targetPath);
        }

        // Копируем файл
        if (!QFile::copy(sourcePath, targetPath)) {
            qDebug() << ">>> ОШИБКА: Не удалось скопировать файл:" << sourcePath << "в" << targetPath;
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
        QString originalPath = targetDir.absoluteFilePath(fileName);
        QString backupPath = m_soundBackupDir + "/" + fileName;

        if (QFile::exists(originalPath)) {
            // Удаляем старый бэкап, если есть
            if (QFile::exists(backupPath)) {
                QFile::remove(backupPath);
            }

            // Копируем оригинал в бэкап
            if (!QFile::copy(originalPath, backupPath)) {
                qDebug() << ">>> ОШИБКА: Не удалось создать бэкап файла:" << originalPath;
                return false;
            }
        }
    }
    return true;
}

void MainWindow::saveSettings() {
    QSettings settings("MyCompany", "MyGameTool");

    settings.setValue("Paths/X64AudioSfx", m_x64AudioSfxPath);
    settings.setValue("Paths/SoundMod", m_modSoundPath);

    qDebug() << "Настройки сохранены в реестр/файл.";
}

void MainWindow::on_checkAutoLoadZV_toggled(bool checked) {
    QSettings settings("MyCompany", "MyGameTool");
    settings.setValue("checkAutoLoadZV", checked);
    qDebug() << "Автоустановка звуков:" << (checked ? "ВКЛЮЧЕНА" : "ВЫКЛЮЧЕНА");
}

bool MainWindow::installSounds() {
    if (m_modSoundPath.isEmpty() || m_x64AudioSfxPath.isEmpty())
        return false;

    if (!QDir(m_modSoundPath).exists() || !QDir(m_x64AudioSfxPath).exists())
        return false;

    QDir modDir(m_modSoundPath);
    QStringList modFiles = modDir.entryList(QStringList() << "*.rpf", QDir::Files);

    if (modFiles.isEmpty())
        return false;

    // Бэкап оригиналов
    m_soundBackupDir = QCoreApplication::applicationDirPath() + "/sound_backup";
    QDir().mkpath(m_soundBackupDir);

    if (!backupOriginalRpfFiles(modFiles))
        return false;

    // Копирование модов
    return copyRpfFiles(m_modSoundPath, m_x64AudioSfxPath);
}

bool MainWindow::restoreSounds() {
    if (m_x64AudioSfxPath.isEmpty())
        return false;

    m_soundBackupDir = QCoreApplication::applicationDirPath() + "/sound_backup";
    if (!QDir(m_soundBackupDir).exists())
        return false;

    QDir bkpDir(m_soundBackupDir);
    QFileInfoList files = bkpDir.entryInfoList(QStringList() << "*.rpf", QDir::Files);

    if (files.isEmpty())
        return true; // Бэкапов нет — считаем, что всё ок

    int restoredCount = 0;
    int failCount = 0;

    foreach (const QFileInfo &fileInfo, files) {
        QString backupPath = fileInfo.absoluteFilePath();
        QString targetPath = m_x64AudioSfxPath + "/" + fileInfo.fileName();

        // Удаляем текущий файл (если есть)
        if (QFile::exists(targetPath)) {
            if (!QFile::remove(targetPath)) {
                qDebug() << ">>> ОШИБКА: Не удалось удалить файл при восстановлении:" << targetPath;
                failCount++;
                continue;
            }
        }

        // Копируем из бэкапа
        if (QFile::copy(backupPath, targetPath)) {
            restoredCount++;
        } else {
            qDebug() << ">>> ОШИБКА: Не удалось восстановить файл:" << backupPath << "в" << targetPath;
            failCount++;
        }
    }

    // Если всё успешно — удаляем папку бэкапов
    if (failCount == 0) {
        QDir backupDir(m_soundBackupDir);
        backupDir.removeRecursively();
        qDebug() << ">>> Все звуки восстановлены. Бэкапы очищены.";
        return true;
    } else {
        qDebug() << ">>> Восстановление звуков с ошибками: успешно" << restoredCount
                 << ", ошибок" << failCount
                 << ". Бэкапы сохранены для повторной попытки.";
        return false;
    }
}

