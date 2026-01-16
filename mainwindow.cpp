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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    qDebug() << "Программа запущенна";
    this->setWindowIcon(QIcon(":/izobr/photo_2026-01-15_107-25-59-round-corners.ico"));
     this->setWindowTitle("ReplaceX");
    //закругление
    ui->btnTelegram->setStyleSheet(
        "QPushButton#btnTelegram {"
        "   border-radius: 25px;"
        "   background-color: #0088cc;"
        "   border: none;"
        "   background-image: url(:/izobr/telegram_icon.png);"
        "   background-position: center;"
        "   background-repeat: no-repeat;"
        "   padding: 0px;"
        "}"
        "QPushButton#btnTelegram:hover {"
        "   background-color: #00aaff;"
        "}"
        );
     ui->btnDonat->setStyleSheet(
         "QPushButton#btnDonat {"
         "   border-radius: 25px;"
         "   background-color: transparent;"
         "   border: none;"
         "   background-image: url(:/izobr/1647901232_1-abrakadabra-fun-p-donati-alers-1.jpg);"
         "   background-position: center;"
         "   background-repeat: no-repeat;"
         "   padding: 0px;"
         "}"
         "QPushButton#btnDonat:hover {"
         "   background-color: #555555;"
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
    processTimer->start(2000);
}

MainWindow::~MainWindow() {
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
    if (!ui->checkAutoLoad->isChecked()) return;

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

    //Игра появилась
    if (isGameActive && !wasGameRunning) {
        wasGameRunning = true;
        qDebug() << ">>> ИГРА ЗАПУЩЕНА. Подмен файлов...";

        // Пытаемся заменить файл. В 100мс окне это должно сработать.
        if (copyFileToGame(ui->leditRedux->text(), ui->leditPapka->text())) {
            qDebug() << ">>> УСПЕХ: Редукс установлен.";
            isReduxInstalled = true;
        } else {
            qDebug() << ">>> ОШИБКА: Не успели или файл заблокирован.";
        }
    }

    // СИТУАЦИЯ 2: Игра была запущена, а теперь её нет в процессах
    if (!isGameActive && wasGameRunning) {
        qDebug() << ">>> ИГРА ЗАКРЫТА. Закрытие лаунчера...";

        // Список процессов, которые нужно убить сразу
        QStringList rgsProcesses = {"SocialClubHelper.exe", "Launcher.exe", "RockstarService.exe"};
        for (const QString &proc : rgsProcesses) {
            killProcessByName(proc);
        }

        // Даем ОС 200 миллисекунд, чтобы освободить файл
        Sleep(200);

        if (copyFileToGame(ui->leditOrig->text(), ui->leditPapka->text())) {
            qDebug() << ">>> ОРИГИНАЛ ВОССТАНОВЛЕН.";
            wasGameRunning = false;
        }
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
        QMessageBox::critical(this, "Error", "Успех! Оригинал установлен.");
        isReduxInstalled = false;
    }
}

void MainWindow::on_btnReplaceRedux_clicked() {
    if (copyFileToGame(ui->leditRedux->text(), ui->leditPapka->text())) {
        QMessageBox::information(this, "Error", "Успех! Редукс установлен.");
        isReduxInstalled = true;
    }
}

void MainWindow::on_checkAutoLoad_toggled(bool checked) {
    QSettings("MyCompany", "MyGameTool").setValue("Settings/AutoLoad", checked);
}

void MainWindow::on_btnDonat_clicked() {

     QDesktopServices::openUrl(QUrl("https://www.donationalerts.com/r/kot21224"));
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
            qDebug() << "r3";
        } else {
            // Если саму игру нашли, но зашли не в ту папку
            QMessageBox::warning(this, "Внимание", "Папка игры найдена, но внутри нет папки 'update'. Проверьте целостность файлов.");
        }
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось найти GTA 5 Legacy автоматически. Пожалуйста, укажите папку вручную.");
    }
}
//tg
void MainWindow::on_btnTelegram_clicked()
{
    QString telegramUrl = "https://t.me/replacexDev";
    QDesktopServices::openUrl(QUrl(telegramUrl));
}
