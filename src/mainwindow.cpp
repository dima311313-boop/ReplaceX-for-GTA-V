#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "modernbutton.h"
#include <windows.h>
#include <winioctl.h>
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
#include <QGraphicsBlurEffect>
#include <QScreen>
#include <QSoundEffect>
#include <QCoreApplication>
#include <QInputDialog>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QParallelAnimationGroup>
#include <QProgressBar>
#include <QGraphicsDropShadowEffect>
#include <QStandardPaths>
#include <QSysInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QHttpPart>







QPixmap getPartiallyRoundedPixmap(const QPixmap& src, int radius, bool roundLeft) {
    if (src.isNull()) return src;

    QPixmap result(src.size());
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    if (roundLeft) {
        // Закругляем лево, право оставляем прямым
        path.moveTo(src.width(), 0);
        path.lineTo(radius, 0);
        path.arcTo(0, 0, radius * 2, radius * 2, 90, 90);
        path.lineTo(0, src.height() - radius);
        path.arcTo(0, src.height() - radius * 2, radius * 2, radius * 2, 180, 90);
        path.lineTo(src.width(), src.height());
    } else {
        // Закругляем право, лево оставляем прямым
        path.moveTo(0, 0);
        path.lineTo(src.width() - radius, 0);
        path.arcTo(src.width() - radius * 2, 0, radius * 2, radius * 2, 90, -90);
        path.lineTo(src.width(), src.height() - radius);
        path.arcTo(src.width() - radius * 2, src.height() - radius * 2, radius * 2, radius * 2, 0, -90);
        path.lineTo(0, src.height());
    }
    path.closeSubpath();

    painter.setClipPath(path);
    painter.drawPixmap(0, 0, src);
    return result;
}

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

    // Проверяем, есть ли мы в реестре
    QSettings settingsA("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (settingsA.contains("ReplaceX")) {
        ui->chkAutoZagruzkaWin->setChecked(true);
    } else {
        ui->chkAutoZagruzkaWin->setChecked(false);
    }

    QString razdel = "-----------------------------------------------------------------";

    QString banner = R"(
    ____             __                _  __
   / __ \___  ____  / /___ __________ | |/ /
  / /_/ / _ \/ __ \/ / __ `/ ___/ _ \ |   /
 / _, _/  __/ /_/ / / /_/ / /__/  __//   |
/_/ |_|\___/ .___/_/\__,_/\___/\___//_/|_|
          /_/                        v0.9.7
)";

    qDebug().noquote() << banner;

    // --- ИНИЦИАЛИЗАЦИЯ ПОТОКА ---
    // В конструкторе MainWindow, там где создаешь поток:
    m_workerThread = new QThread(this);
    m_worker = new FileWorker();
    m_worker->moveToThread(m_workerThread);


    // Соединяем сигналы MainWindow с методами FileWorker
    connect(this, &MainWindow::requestInstall, m_worker, &FileWorker::processInstallation);
    connect(this, &MainWindow::requestRestore, m_worker, &FileWorker::processRestoration);
    // В конструктор MainWindow:
    connect(this, &MainWindow::requestUnpack, m_worker, &FileWorker::processUnpack);
    connect(m_worker, &FileWorker::unpackFinished, this, &MainWindow::onUnpackResult);


    // Коннекты для РУЧНЫХ операций
    connect(this, &MainWindow::requestManualSmartReplace, m_worker, &FileWorker::manualSmartReplace);
    connect(this, &MainWindow::requestManualRestoreGunPacks, m_worker, &FileWorker::manualRestoreGunPacks);
    connect(this, &MainWindow::requestManualInstallGunPacks, m_worker, &FileWorker::manualInstallGunPacks);
    connect(this, &MainWindow::requestManualReplaceSounds, m_worker, &FileWorker::manualReplaceSounds);
    connect(this, &MainWindow::requestManualRestoreSounds, m_worker, &FileWorker::manualRestoreSounds);

    // Соединяем ответы FileWorker с интерфейсом
    connect(m_worker, &FileWorker::progressValue, ui->installProgress, &QProgressBar::setValue);
    connect(m_worker, &FileWorker::statusUpdate, this, &MainWindow::onWorkerStatus);
    connect(m_worker, &FileWorker::progressMessage, this, &MainWindow::onWorkerProgress);
    connect(m_worker, &FileWorker::operationFinished, this, &MainWindow::onWorkerFinished);

    // Очистка при завершении
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);



    m_workerThread->start(QThread::HighPriority);
    // --- КОНЕЦ ИНИЦИАЛИЗАЦИИ ПОТОКА ---


    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &MainWindow::onResult);

    QNetworkRequest request(QUrl("https://raw.githubusercontent.com/dima311313-boop/ReplaceX-for-GTA-V/ReplaceX/resources/donat,new.json"));
    manager->get(request);


    loadSettings();

    checkTimer = new QTimer(this);
    connect(checkTimer, &QTimer::timeout, this, &MainWindow::checkGtaProcess);
    checkTimer->start(3000); // Проверять раз в 3 секунды

    ui->infoSignals->setVisible(false);
    ui->chkHddWarning->setVisible(false);
    ui->lblHddAlert->setVisible(false);
    ui->installProgress->setVisible(false);
    ui->lineV->setVisible(false);
    ui->oknoPresets->setVisible(false);
    ui->oknoGta5V->setVisible(false);
    ui->btnExitNF->setVisible(false);
    ui->oknoSettings->setVisible(false);
    ui->oknoNF->setVisible(false);
    ui->btnNotification->setVisible(false);
    ui->oknoDiscleamer->setVisible(false);
    ui->oknoHDD->setVisible(false);
    ui->oknoKnopohki->setVisible(false);
    ui->btnExitGP->setVisible(false);
    ui->oknoZV->setVisible(false);
    ui->oknoDLC->setVisible(false);
    ui->oknoBR->setVisible(false);
    ui->oknoZM->setVisible(false);
    ui->Label_Text->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->Label_TextB->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->Label_bronik->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->Label_pistol->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->Label_zamen->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->Label_TextZ->setAttribute(Qt::WA_TransparentForMouseEvents);


    //получение пути
    connect(ui->leditPapka, &QLineEdit::textChanged, [this](const QString &text) {
        QSettings settings("MyCompany", "MyGameTool");
        settings.setValue("Paths/GameFolder", text);
    });    

    //бэкапы ган паков
    m_backupPath = QCoreApplication::applicationDirPath() + "/backups_gta";

    ui->oknoGP->setVisible(false);
    //кнопочки тг и д
    // === ИНИЦИАЛИЗАЦИЯ КОНТЕЙНЕРА ПОДСКАЗОК (С ТЕНЬЮ И ПЛАВНОСТЬЮ) ===
    // === ИНИЦИАЛИЗАЦИЯ ПОДСКАЗОК (С ТЕНЬЮ И СИСТЕМНОЙ ПЛАВНОСТЬЮ) ===
    infoPopup = new QLabel(this); // Обычный QLabel
    infoPopup->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    infoPopup->setStyleSheet(
        "background-color: #2c3e50;"
        "color: white;"
        "border-radius: 8px;"
        "padding: 8px;"
        "border: 1px solid #34495e;"
        );
    infoPopup->setWordWrap(true);

    // Навешиваем тень прямо на саму подсказку (больше никаких вложенных эффектов!)
    QGraphicsDropShadowEffect *popupShadow = new QGraphicsDropShadowEffect(infoPopup);
    popupShadow->setBlurRadius(15);
    popupShadow->setXOffset(0);
    popupShadow->setYOffset(3);
    popupShadow->setColor(QColor(0, 0, 0, 160));
    infoPopup->setGraphicsEffect(popupShadow);

    infoPopup->setWindowOpacity(0.0); // Системная прозрачность окна по умолчанию
    infoPopup->hide();

    // Регистрируем кнопку архива замененок в фильтре событий (остальные уже зарегистрированы)
    ui->btnArxivZM->installEventFilter(this);

    ui->btnTelegram->installEventFilter(this);
    ui->btnDonat->installEventFilter(this);
    ui->btnArxivRedux->installEventFilter(this);
    ui->btnArxivGuns->installEventFilter(this);
    ui->btnArxivSounds->installEventFilter(this);
    ui->btnArxivBR->installEventFilter(this);


    // === НАСТРОЙКА ШАБЛОНА ДЛЯ ПЛАВНОЙ АНИМАЦИИ (ПИСТОЛЕТ) ===

    // 1. Устанавливаем иконку в QLabel
    ui->Label_pistol->setPixmap(QPixmap(":/izobr/pistolet.png"));
    ui->Label_pistol->setScaledContents(true);

    // 2. Навешиваем графический эффект прозрачности на иконку
    QGraphicsOpacityEffect *iconOpacity = new QGraphicsOpacityEffect(ui->Label_pistol);
    ui->Label_pistol->setGraphicsEffect(iconOpacity);
    iconOpacity->setOpacity(0.7); // Изначально делаем тусклой (30% видимости)

    // 3. Запоминаем координаты текста из дизайнера
    m_textStartX = ui->Label_Text->x();
    m_textEndX = m_textStartX + 30;

    // 4. Регистрируем кнопку в фильтре событий
    ui->listPisol->installEventFilter(this);
    ui->btnGP_install->installEventFilter(this);


    // === НАСТРОЙКА ШАБЛОНА ДЛЯ ПЛАВНОЙ АНИМАЦИИ (БРОНИК) ===

    // 1. Устанавливаем иконку в QLabel
    ui->Label_bronik->setPixmap(QPixmap(":/izobr/bronik.png"));
    ui->Label_bronik->setScaledContents(true);

    // 2. Навешиваем графический эффект прозрачности на иконку
    QGraphicsOpacityEffect *iconOpacityB = new QGraphicsOpacityEffect(ui->Label_bronik);
    ui->Label_bronik->setGraphicsEffect(iconOpacityB);
    iconOpacityB->setOpacity(0.7); // Изначально делаем тусклой (30% видимости)

    // 3. Запоминаем координаты текста из дизайнера
    m_textStartX2 = ui->Label_TextB->x();
    m_textEndX2 = m_textStartX2 + 30;

    // 4. Регистрируем кнопку в фильтре событий
    ui->listBronik->installEventFilter(this);
    ui->btnBR_install->installEventFilter(this);


    // === НАСТРОЙКА ШАБЛОНА ДЛЯ ПЛАВНОЙ АНИМАЦИИ (ЗАМЕНЕНКА) ===

    // 1. Устанавливаем иконку в QLabel
    ui->Label_zamen->setPixmap(QPixmap(":/izobr/zamen.png"));
    ui->Label_zamen->setScaledContents(true);

    // 2. Навешиваем графический эффект прозрачности на иконку
    // ИСПРАВЛЕНО: Родителем эффекта теперь назначен ui->Label_zamen
    QGraphicsOpacityEffect *iconOpacityZ = new QGraphicsOpacityEffect(ui->Label_zamen);
    ui->Label_zamen->setGraphicsEffect(iconOpacityZ);
    iconOpacityZ->setOpacity(0.7); // Изначально делаем тусклой (30% видимости)

    // 3. Запоминаем координаты текста из дизайнера
    m_textStartX3 = ui->Label_TextZ->x();
    m_textEndX3 = m_textStartX3 + 30;

    // 4. Регистрируем кнопку в фильтре событий
    ui->listZamen->installEventFilter(this);
    ui->btnZM_install->installEventFilter(this);

    // === ВОЗВРАЩАЕМ ЦВЕТНОЕ НЕОНОВОЕ СВЕЧЕНИЕ (GLOW) ===

    // 1. Убираем стандартные рамки и фоны через простые стили (оставляем иконки чистыми)
    ui->btnSettings->setStyleSheet("border: none; background-color: transparent;");
    ui->btnDonat->setStyleSheet("border: none; background-color: transparent;");
    ui->btnTelegram->setStyleSheet("border: none; background-color: transparent;");

    // 2. Обязательно подписываем все три кнопки на фильтр событий для анимации свечения
    ui->btnSettings->installEventFilter(this);
    ui->btnDonat->installEventFilter(this);
    ui->btnTelegram->installEventFilter(this);

    // 3. Создаем сочные неоновые эффекты по умолчанию (blur = 0)
    QGraphicsOpacityEffect *opSettings = qobject_cast<QGraphicsOpacityEffect*>(ui->btnSettings->graphicsEffect());
    if (opSettings) delete opSettings; // Очищаем старые эффекты, если они остались в памяти
    QGraphicsDropShadowEffect *glowSettings = new QGraphicsDropShadowEffect(ui->btnSettings);
    glowSettings->setOffset(0, 0);
    glowSettings->setColor(QColor(255, 255, 255, 200)); // Белый неон
    glowSettings->setBlurRadius(0);
    ui->btnSettings->setGraphicsEffect(glowSettings);

    QGraphicsOpacityEffect *opDonat = qobject_cast<QGraphicsOpacityEffect*>(ui->btnDonat->graphicsEffect());
    if (opDonat) delete opDonat;
    QGraphicsDropShadowEffect *glowDonat = new QGraphicsDropShadowEffect(ui->btnDonat);
    glowDonat->setOffset(0, 0);
    glowDonat->setColor(QColor(255, 140, 0, 220)); // Сочный оранжевый неон под цвет буквы "D!"
    glowDonat->setBlurRadius(0);
    ui->btnDonat->setGraphicsEffect(glowDonat);

    QGraphicsOpacityEffect *opTelegram = qobject_cast<QGraphicsOpacityEffect*>(ui->btnTelegram->graphicsEffect());
    if (opTelegram) delete opTelegram;
    QGraphicsDropShadowEffect *glowTelegram = new QGraphicsDropShadowEffect(ui->btnTelegram);
    glowTelegram->setOffset(0, 0);
    glowTelegram->setColor(QColor(34, 158, 217, 220)); // Фирменный голубой неон Telegram
    glowTelegram->setBlurRadius(0);
    ui->btnTelegram->setGraphicsEffect(glowTelegram);

    // === СТАТИЧНОЕ НЕОНОВОЕ СВЕЧЕНИЕ ДЛЯ ЛОГОТИПА ===
    QGraphicsDropShadowEffect *logoGlow = new QGraphicsDropShadowEffect(ui->lblLogotip);
    logoGlow->setOffset(0, 0); // Ореол строго по центру логотипа
    logoGlow->setBlurRadius(30); // Широкое и мягкое рассеивание

    // Мягкий розово-персиковый цвет неона под тон твоей короны (#e9775b) с прозрачностью 120
    logoGlow->setColor(QColor(233, 119, 91, 120));

    ui->lblLogotip->setGraphicsEffect(logoGlow);
    //end knopocki


    this->setFixedSize(646, 374);

    qInfo() << "Программа запущенна.";
    ui->miniProgress->setText("Ожидание запуска игры");
    this->setWindowIcon(QIcon(":/izobr/IconG.ico"));
    this->setWindowTitle("ReplaceX");

    ui->lblHddAlert->setAttribute(Qt::WA_TransparentForMouseEvents);


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
    trayIcon->setIcon(QIcon(":/izobr/IconG.ico")); // Твоя иконка
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

    bool isSoundEnabled = settings.value("Settings/SoundEnabled", true).toBool();
    ui->checkSound->setChecked(isSoundEnabled);


    ui->leditLogsFile->setText(QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/ReplaceX.log"));

    int savedIndex = settings.value("SelectedPlatform", 0).toInt();

    // 2. Устанавливаем его в комбобокс
    ui->cmbServer->setCurrentIndex(savedIndex);

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


    //авто поиск корневой и вывод
    bool firstRun = !settings.contains("FirstRun");


    if (firstRun) {
        blurEf(true);
        ui->oknoGta5V->setVisible(true);
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

    qInfo() << "Файл настроек:" << settings.fileName();
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



    qDebug() << " --------------------------Пути к файлам--------------------------";
    qInfo() << "Пути к звукам загруженны:" << "Папка:" << m_x64AudioSfxPath << "Мод:" << m_modSoundPath;
    qInfo() << "Пути к update.rpf загруженны:" << "Редукс: " << settings.value("Paths/ReduxFile", "").toString() << "Оригинальный: " << settings.value("Paths/OriginalFile", "").toString() << "Папка: " << savedPath;
    qInfo() << "Пути к ган пакам загруженны: " << "Ган паки: " << m_gunPackSourcePath << "Папка: " << m_dlcPacksTargetPath;
    qDebug() << razdel;

    processTimer = new QTimer(this);
    connect(processTimer, &QTimer::timeout, this, &MainWindow::checkProcessLoop);
    processTimer->start(500);


    // В конструктор MainWindow::MainWindow
    m_soundSuccess = new QSoundEffect(this);
    // Используем qrc:/ для надежности
    m_soundSuccess->setSource(QUrl("qrc:/sounds/sounds/Yspeh.wav"));
    m_soundSuccess->setVolume(0.5);

    // Добавим отладку статуса
    connect(m_soundSuccess, &QSoundEffect::loadedChanged, this, [this]() {
        if (m_soundSuccess->isLoaded()) {
            qDebug() << "Звук успешно загружен и готовы!";
        }
    });

    m_soundError = new QSoundEffect(this);
    m_soundError->setSource(QUrl("qrc:/sounds/sounds/Error.wav"));
    m_soundError->setVolume(0.6);

    updatePresetsCombo();

    // АВТО-ЗАГРУЗКА ПОСЛЕДНЕГО ПРЕСЕТА
    QSettings s("MyCompany", "MyGameTool");
    QString lastPreset = s.value("LastPresetName").toString();
    if (!lastPreset.isEmpty()) {
        int idx = ui->cmbPresets->findText(lastPreset);
        if (idx > 0) { // Проверяем, что это не "Выберите пресет..." (индекс 0)
            ui->cmbPresets->setCurrentIndex(idx);
            on_cmbPresets_activated(idx); // Вызываем загрузку
            qInfo() << "Автоматически загружен последний пресет:" << lastPreset;
        }
    }
    // Проверяем диск
    QString gtaPath = ui->leditPapka->text();
    m_isHddDetected = isHDD(gtaPath);

    bool alreadySeen = settings.value("Settings/HddWarningSeen", false).toBool();

    if (m_isHddDetected && !alreadySeen) {
        ui->chkHddWarning->setVisible(true);
        ui->lblHddAlert->setVisible(true);
        ui->infoSignals->setVisible(true);
        ui->lblIgraInstallTo->setText("Ваша игра установленна на: HDD");
        ui->lblIgraInstallTo->setVisible(true);

        // Таймер для мигания
        m_blinkTimer = new QTimer(this);
        connect(m_blinkTimer, &QTimer::timeout, [this]() {
            ui->lblHddAlert->setVisible(!ui->lblHddAlert->isVisible());
        });
        m_blinkTimer->start(500); // Мигаем раз в полсекунды
    }
    else
    {
        ui->lblIgraInstallTo->setText("Ваша игра установленна на: SSD");
    }
    applyModernShadow(ui->oknoKnopohki);
    applyModernShadow(ui->oknoNF);

}

MainWindow::~MainWindow() {

    // Коннекты для замененок (ZM)
    connect(this, &MainWindow::requestManualRestoreZM, m_worker, &FileWorker::manualRestoreZM);
    connect(this, &MainWindow::requestManualInstallZM, m_worker, &FileWorker::manualInstallZM);
    ui->btnArxivZM->installEventFilter(this);
    // Коннекты для РУЧНЫХ операций броников
    connect(this, &MainWindow::requestManualRestoreArmorPacks, m_worker, &FileWorker::manualRestoreArmorPacks);
    connect(this, &MainWindow::requestManualInstallArmorPacks, m_worker, &FileWorker::manualInstallArmorPacks);

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

bool MainWindow::isHDD(QString path) {
    if (path.isEmpty()) return false;

    // Получаем букву диска (например, "C:")
    QString drive = path.left(2);
    std::wstring wDrive = L"\\\\.\\" + drive.toStdWString();

    HANDLE hDevice = CreateFileW(wDrive.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR result = {};
    DWORD bytesReturned = 0;

    // Если IncursSeekPenalty == true, значит это HDD
    bool success = DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                                   &result, sizeof(result), &bytesReturned, NULL);
    CloseHandle(hDevice);

    if (success) return result.IncursSeekPenalty;
    return false;
}

void MainWindow::applyModernShadow(QWidget* widget) {
    if (!widget) return;

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(widget);

    // Делаем тень абсолютно черной и плотной (alpha = 255),
    // чтобы она гарантированно выделялась на темно-сером фоне
    shadow->setColor(QColor(0, 0, 0, 255));

    if (widget == ui->oknoKnopohki) {
        // Левая панель с кнопками: увеличили размытие до 35 и сдвиг влево до -12
        shadow->setBlurRadius(35);
        shadow->setXOffset(-12);
        shadow->setYOffset(6);
    }
    else if (widget == ui->oknoPresets || widget == ui->oknoGP ||
             widget == ui->oknoDiscleamer || widget == ui->oknoZV ||
             widget == ui->oknoDLC || widget == ui->oknoBR || widget == ui->oknoZM) {
        // Правые всплывающие окна: увеличили размытие до 35 и сдвиг вправо до 12
        shadow->setBlurRadius(35);
        shadow->setXOffset(12);
        shadow->setYOffset(6);
    }
    else {
        // Центральные одиночные окна (Настройки, уведомления и др.):
        // Мощная, глубокая круговая тень с большим размытием
        shadow->setBlurRadius(45);
        shadow->setXOffset(0);
        shadow->setYOffset(10);
    }

    widget->setGraphicsEffect(shadow);
}

void MainWindow::animateWindowOpen(QWidget* target, QWidget* sourceBtn) {
    // 1. Список всех «всплывающих» окон
    QList<QWidget*> subWindows = {ui->oknoPresets, ui->oknoHDD, ui->oknoSettings,
                                   ui->oknoZV, ui->oknoGP, ui->oknoDiscleamer, ui->oknoDLC, ui->oknoBR, ui->oknoZM};

    // Скрываем все окна из списка, кроме того, которое открываем
    for(QWidget* w : subWindows) {
        if(w != target) w->hide();
    }

    // 2. ЛОГИКА ДЛЯ oknoKnopohki
    if (target == ui->oknoSettings) {
        ui->oknoKnopohki->hide();
    } else {
        ui->oknoKnopohki->show();
        ui->oknoKnopohki->raise(); // Держим панель кнопок сверху
    }

    // 3. Создаем «Снимок» (Snapshot)
    target->setGraphicsEffect(nullptr);

    target->setAttribute(Qt::WA_DontShowOnScreen);
    target->show();
    QPixmap snapshot(target->size());
    snapshot.fill(Qt::transparent);
    target->render(&snapshot);
    target->hide();
    target->setAttribute(Qt::WA_DontShowOnScreen, false);

    // 4. Слой анимации
    QLabel* animLayer = new QLabel(this);
    animLayer->setPixmap(snapshot);
    animLayer->setScaledContents(true);
    animLayer->setGeometry(sourceBtn->geometry());

    QGraphicsOpacityEffect* opacity = new QGraphicsOpacityEffect(animLayer);
    animLayer->setGraphicsEffect(opacity);
    animLayer->show();
    animLayer->raise();

    // 5. Групповая анимация
    QParallelAnimationGroup* group = new QParallelAnimationGroup(this);

    QPropertyAnimation* geoAnim = new QPropertyAnimation(animLayer, "geometry");
    geoAnim->setDuration(400);
    geoAnim->setStartValue(sourceBtn->geometry());
    geoAnim->setEndValue(target->geometry());
    geoAnim->setEasingCurve(QEasingCurve::OutBack);

    QPropertyAnimation* opaAnim = new QPropertyAnimation(opacity, "opacity");
    opaAnim->setDuration(300);
    opaAnim->setStartValue(0.0);
    opaAnim->setEndValue(1.0);

    group->addAnimation(geoAnim);
    group->addAnimation(opaAnim);

    // 6. Финал
    connect(group, &QParallelAnimationGroup::finished, this, [=]() {
        target->show();
        target->raise();

        // ПРИМЕНЯЕМ ТЕНЬ К РЕАЛЬНОМУ ОКНУ, КОГДА ОНО ПОЯВИЛОСЬ
        applyModernShadow(target);

        ui->btnExitGP->show();
        ui->btnExitGP->raise();

        animLayer->deleteLater();
        group->deleteLater();
    });

    group->start();
}
void MainWindow::animateFadeOut(QWidget* target) {
    if (!target || !target->isVisible()) return;

    // 1. Делаем снимок окна
    QPixmap snapshot(target->size());
    snapshot.fill(Qt::transparent);
    target->render(&snapshot);

    // Применяем закругление (чтобы края не дергались)
    bool isLeftRounded = (target == ui->oknoSettings);
    snapshot = getPartiallyRoundedPixmap(snapshot, 20, isLeftRounded);

    // 2. Создаем слой анимации
    QLabel* animLayer = new QLabel(this);
    animLayer->setPixmap(snapshot);
    animLayer->setScaledContents(true);
    animLayer->setGeometry(target->geometry());

    QGraphicsOpacityEffect* opacity = new QGraphicsOpacityEffect(animLayer);
    animLayer->setGraphicsEffect(opacity);
    animLayer->show();
    animLayer->raise();

    // Скрываем реальное окно мгновенно
    target->hide();

    // 3. Анимация затухания и легкого уменьшения
    QParallelAnimationGroup* group = new QParallelAnimationGroup(this);

    // Прозрачность в 0
    QPropertyAnimation* opaAnim = new QPropertyAnimation(opacity, "opacity");
    opaAnim->setDuration(300);
    opaAnim->setStartValue(1.0);
    opaAnim->setEndValue(0.0);
    opaAnim->setEasingCurve(QEasingCurve::OutCubic);

    // Легкое уменьшение размера (эффект ухода вдаль)
    QPropertyAnimation* geoAnim = new QPropertyAnimation(animLayer, "geometry");
    geoAnim->setDuration(300);
    QRect startGeo = target->geometry();
    // Окно уменьшится на 10 пикселей с каждой стороны
    QRect endGeo = startGeo.adjusted(10, 10, -10, -10);
    geoAnim->setStartValue(startGeo);
    geoAnim->setEndValue(endGeo);
    geoAnim->setEasingCurve(QEasingCurve::OutCubic);

    group->addAnimation(opaAnim);
    group->addAnimation(geoAnim);

    connect(group, &QParallelAnimationGroup::finished, [=]() {
        animLayer->deleteLater();
        group->deleteLater();
    });

    group->start();
}
void MainWindow::blurEf(bool enable)
{
    static QWidget *overlay = nullptr;

    if (enable) {
        if (!overlay) {
            overlay = new QWidget(this);
            overlay->setGeometry(this->rect());
            // Пропускаем клики СКВОЗЬ оверлей, чтобы они долетали до oknoGta5V
            overlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            overlay->show();
        }

        // 1. Блюрим ТОЛЬКО то, что под окном.
        // Если oknoGta5V лежит в centralwidget, блюр его достанет.
        // РЕШЕНИЕ: Применяем блюр к фоновым элементам отдельно или используем Snapshot
        QGraphicsBlurEffect *blur = new QGraphicsBlurEffect(this);
        blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
        ui->centralwidget->setGraphicsEffect(blur);

        QPropertyAnimation *anim = new QPropertyAnimation(blur, "blurRadius");
        anim->setDuration(600);
        anim->setStartValue(0);
        anim->setEndValue(25);
        anim->setEasingCurve(QEasingCurve::OutCirc);
        anim->start(QAbstractAnimation::DeleteWhenStopped);

        // 2. ВЫВОДИМ ОКНО ИЗ-ПОД БЛЮРА
        // Чтобы oknoGta5V не блюрилось, оно НЕ ДОЛЖНО быть ребенком centralwidget
        // Попробуй временно сменить ему родителя на само MainWindow
        ui->oknoGta5V->setParent(this);
        ui->oknoGta5V->show();
        ui->oknoGta5V->raise();

    } else {
        // Твой код снятия блюра (оставляем как есть, он норм)
        if (ui->centralwidget->graphicsEffect()) {
            QGraphicsBlurEffect *currentBlur = qobject_cast<QGraphicsBlurEffect*>(ui->centralwidget->graphicsEffect());
            QPropertyAnimation *anim = new QPropertyAnimation(currentBlur, "blurRadius");
            anim->setDuration(400);
            anim->setStartValue(currentBlur->blurRadius());
            anim->setEndValue(0);

            connect(anim, &QPropertyAnimation::finished, this, [=]() {
                ui->centralwidget->setGraphicsEffect(nullptr);
                if (overlay) { overlay->deleteLater(); overlay = nullptr; }
                ui->oknoGta5V->hide();
            });
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
}
const QString CURRENT_VERSION = "0.9.7";

void MainWindow::onResult(QNetworkReply *reply) {
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    reply->deleteLater(); // Удаляем сразу, чтобы не забыть
    if (pizda == 1) {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Ошибка сети:" << reply->errorString();
            pizda = 2;
            return;
        }
    }
    if(pizda == 2 || pizda == 1)  {
        if (!doc.isObject()) {
            qDebug() << "Ошибка: JSON не объект";
            pizda = 3;
            return;
        }
    }
    QJsonObject mainObj = doc.object();

    if(pizda == 2 || pizda == 1 || pizda == 3)  {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Сетевая ошибка:" << reply->errorString();
            reply->deleteLater();
            pizda = 4;
            return;
        }
    }

    // 1. Проверка на объект (теперь у нас корень - объект { })
    if (!doc.isObject()) {
        qDebug() << "Критическая ошибка: Ожидался JSON объект!";
        reply->deleteLater();
        return;
    }

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
    downloadUrl = updateObj["url"].toString(); // Так стало (запись в глобальное поле класса)
    QJsonArray changelogArray = updateObj["changelog"].toArray();
    QString changelogText;
    if (!changelogArray.isEmpty()) {
        changelogText = "\n\nЧто нового:\n";
        for (const QJsonValue &change : changelogArray) {
            changelogText += "• " + change.toString() + "\n";
        }
    }
    if(Y == false)
    {
        qDebug() << "Проверка версии. Сервер:" << remoteVersion << "Локальная:" << CURRENT_VERSION;
        Y = true;
    }
    if (!remoteVersion.isEmpty() && remoteVersion != CURRENT_VERSION) {
        if(N == false){
            if(CURRENT_VERSION > remoteVersion)
            {
                qDebug() << "Удачных тестов!";
            }
            else {
                qDebug() << "Доступна новая версия: " << remoteVersion;
                ui->lblUpVer->setText("Новая версия: " + remoteVersion);
                ui->lblSpisocIzm->setText(changelogText);
                ui->btnNotification->setVisible(true);
            }
        }
        N = true;
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
        qCritical() << ">>> ПРОПУСК: Источник и цель совпадают!";
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
        qCritical() << ">>> ОШИБКА WinAPI:" << err; // Если 32 — файл занят
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
    // Добавь этот блок внутрь метода getCurrentConfig():
    cfg.zmSource = ui->leditZM_Pack->text();
    cfg.zmTarget = ui->leditDLS_3->text();
    cfg.zmBackupPath = QCoreApplication::applicationDirPath() + "/backups_zm";
    cfg.useZM = ui->checkAutoLoadZM->isChecked();
    cfg.reduxPath = ui->leditRedux->text();
    cfg.originalPath = ui->leditOrig->text();
    cfg.gameUpdatePath = ui->leditPapka->text();
    cfg.gunPackSource = ui->leditGunPuck->text();
    cfg.dlcPacksTarget = ui->leditDLS->text();
    cfg.soundModPath = ui->leditModZV->text();
    cfg.sfxPath = ui->leditPapcaZV->text();

    // --- СБОР ДАННЫХ ДЛЯ БРОНИКОВ ---
    cfg.armorSource = ui->leditBR_Pack->text();
    cfg.armorTarget = ui->leditDLS_2->text();
    cfg.armorBackupPath = QCoreApplication::applicationDirPath() + "/backups_armor";
    cfg.useArmor = ui->checkAutoLoadBR->isChecked();
    // ---------------------------------

    cfg.backupPath = QCoreApplication::applicationDirPath() + "/backups_gta";
    cfg.soundBackupPath = QCoreApplication::applicationDirPath() + "/sound_backup";

    cfg.useRedux = ui->checkAutoLoad->isChecked();
    cfg.useGunPack = ui->checkAutoLoadGP->isChecked();
    cfg.useSounds = ui->checkAutoLoadZV->isChecked();

    return cfg;
}




void MainWindow::checkProcessLoop() {
    if (m_isOperationPending) return;

    int serverIndex = ui->cmbServer->currentIndex();

    // Проверка специфичных процессов
    bool isGtaRunning = isProcessRunning("GTA5.exe") ||
                        isProcessRunning("GTA5_Enhanced.exe") ||
                        isProcessRunning("GTA5_Enhanced_BE.exe");
    bool isEacRunning = isProcessRunning("EACLauncher.exe") || isProcessRunning("EasyAntiCheat_Launcher.exe");
    bool isRageRunning = isProcessRunning("RageMP.exe");
    bool isAltvRunning = isProcessRunning("altv-client.exe") || isProcessRunning("altv.exe");
    bool isRglRunning = isProcessRunning("Launcher.exe");

    // 1. СТРОГАЯ ЛОГИКА ЗАПУСКА
    bool shouldInstall = false;
    if (serverIndex == 2) { // RageMP
        // Установка только если есть признаки RageMP
        shouldInstall = isEacRunning && !isAltvRunning;
    } else if (serverIndex == 1) { // AltV
        // Установка только если есть признаки AltV
        shouldInstall = isGtaRunning && (!isEacRunning && !isRageRunning);
    } else {
        shouldInstall = isGtaRunning;
    }

    if (shouldInstall && !m_wasGameRunning) {
        m_wasGameRunning = true;
        m_isOperationPending = true;
        qDebug() << "Запуск установки для:" << (serverIndex == 2 ? "RageMP" : "AltV");
        emit requestInstall(getCurrentConfig());
        return;
    }

    // Фиксируем, что игра реально запустилась
    if (isGtaRunning) {
        ui->installProgress->setVisible(true);
        m_gameStarted = true;
    }

    // 2. ЛОГИКА ВОССТАНОВЛЕНИЯ
    // Условие: моды стоят, но игра НЕ запущена
    if (m_wasGameRunning && !isGtaRunning) {

        // Для RageMP: удаляем только если игра УЖЕ поработала (m_gameStarted)
        // ИЛИ если EAC закрылся, так и не запустив игру (отмена)
        bool userCancelled = (serverIndex == 2 && !isEacRunning && !m_gameStarted);

        if (m_gameStarted || userCancelled || serverIndex != 2) {
            m_wasGameRunning = false;
            m_gameStarted = false;
            m_isOperationPending = true;

            if (serverIndex == 2) { // RageMP
                QTimer::singleShot(3000, this, [this]() {
                    emit requestRestore(getCurrentConfig());
                });
            } else if (isRglRunning || serverIndex == 0) {
                emit requestRestore(getCurrentConfig());
            }
        }
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
    if (success) ui->installProgress->setValue(100);
    else ui->installProgress->setValue(0);

    // Проверяем, включен ли звук в настройках (через чекбокс)
    bool soundEnabled = ui->checkSound->isChecked();

    if (success) {
        if (soundEnabled && m_soundSuccess->isLoaded()) {
            m_soundSuccess->play();
            ui->installProgress->setVisible(false);
        }
    } else {
        if (soundEnabled) {
            m_soundError->play();
        }
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
        qInfo() << "AutoLoad успешно включен";
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

    // Автопоиск пути Legacy
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
            "C:/Program Files/Rockstar Games/GTAV Legacy",
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

            // Всплывающее окно убрано, оставляем только вывод статуса в углу
            ui->miniProgress->setText("Путь к корневой папке установлен");
        } else {
            // Если саму игру нашли, но директории update внутри нет
            qCritical() << "[AutoSearch] Путь найден, но директория 'update' отсутствует:" << foundPath;
            ui->leditPapka->setText("Ошибка");
            QMessageBox::warning(this, "Внимание", "Папка игры найдена, но внутри нет папки 'update'. Проверьте целостность файлов.");
        }
    } else {
        qCritical() << "[AutoSearch] Не удалось автоматически обнаружить путь к GTA 5 в реестре или стандартных директориях.";
        ui->leditPapka->setText("Ошибка");
        QMessageBox::critical(this, "Ошибка", "Не удалось найти GTA 5 Legacy автоматически. Пожалуйста, укажите папку вручную.");
    }
}
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->listPisol || obj == ui->btnGP_install) {
        // Достаем эффект прозрачности иконки
        QGraphicsOpacityEffect *opacityEff = qobject_cast<QGraphicsOpacityEffect*>(ui->Label_pistol->graphicsEffect());

        if (event->type() == QEvent::Enter) {
            // --- КУРСОР НАВЕДЕН (ПОЯВЛЕНИЕ И СМЕЩЕНИЕ) ---
            if (opacityEff) {
                ui->listPisol->setStyleSheet(
                    "QListWidget, QListView {"
                    "   background-color: #e9775b;"
                    "   border: 1px solid #333;"
                    "   border-top-right-radius: 0px;"
                    "   border-bottom-right-radius: 0px;"
                    "   border-top-left-radius: 20px;"
                    "   border-bottom-left-radius: 20px;"
                    "   border: 1px solid white;"
                    "   color: white;"
                    "   padding: 5px;"
                    "}"
                    );
                QPropertyAnimation *animOpacity = new QPropertyAnimation(opacityEff, "opacity");
                animOpacity->setDuration(250);
                animOpacity->setStartValue(opacityEff->opacity());
                animOpacity->setEndValue(1.0); // Загорается на 100%
                animOpacity->setEasingCurve(QEasingCurve::OutCubic);
                animOpacity->start(QAbstractAnimation::DeleteWhenStopped);
            }

            QPropertyAnimation *animMove = new QPropertyAnimation(ui->Label_Text, "pos");
            animMove->setDuration(250);
            animMove->setStartValue(ui->Label_Text->pos());
            animMove->setEndValue(QPoint(m_textEndX, ui->Label_Text->y()));
            animMove->setEasingCurve(QEasingCurve::OutCubic);
            animMove->start(QAbstractAnimation::DeleteWhenStopped);

            return true;
        }
        else if (event->type() == QEvent::Leave) {
            // --- КУРСОР УБРАН (ВОЗВРАТ К ТУСКЛОМУ СОСТОЯНИЮ) ---
            if (opacityEff) {
                QPropertyAnimation *animOpacity = new QPropertyAnimation(opacityEff, "opacity");
                animOpacity->setDuration(250);
                animOpacity->setStartValue(opacityEff->opacity());
                animOpacity->setEndValue(0.3); // ИЗМЕНЕНО: Возвращается к 30% прозрачности вместо 0.0
                animOpacity->setEasingCurve(QEasingCurve::OutCubic);
                animOpacity->start(QAbstractAnimation::DeleteWhenStopped);
                ui->listPisol->setStyleSheet(
                    "QListWidget, QListView {"
                    "   background-color: #36333b;"
                    "   border: 1px solid #333;"
                    "   border-top-right-radius: 0px;"
                    "   border-bottom-right-radius: 0px;"
                    "   border-top-left-radius: 20px;"
                    "   border-bottom-left-radius: 20px;"
                    "   background-color: #1e1e1e;"
                    "   border: 1px solid white;"
                    "   color: white;"
                    "   padding: 5px;"
                    "}"
                    );
            }

            QPropertyAnimation *animMove = new QPropertyAnimation(ui->Label_Text, "pos");
            animMove->setDuration(250);
            animMove->setStartValue(ui->Label_Text->pos());
            animMove->setEndValue(QPoint(m_textStartX, ui->Label_Text->y()));
            animMove->setEasingCurve(QEasingCurve::OutCubic);
            animMove->start(QAbstractAnimation::DeleteWhenStopped);

            return true;
        }
    }
    if (obj == ui->btnArxivZM) {
        if (event->type() == QEvent::Enter) {
            showTooltip(ui->btnArxivZM, "Авто распаковка архива с замененками. Программа найдет любые папки, содержащие dlc.rpf, и извлечет их.", true);
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            hideTooltip();
            return true;
        }
    }

    if (obj == ui->listZamen || obj == ui->btnZM_install) {
        QGraphicsOpacityEffect *opacityEff = qobject_cast<QGraphicsOpacityEffect*>(ui->Label_zamen->graphicsEffect());

        if (event->type() == QEvent::Enter) {
            // --- КУРСОР НАВЕДЕН (ПОЯВЛЕНИЕ И СМЕЩЕНИЕ) ---
            if (opacityEff) {
                ui->listZamen->setStyleSheet(
                    "QListWidget, QListView {"
                    "   background-color: #e9775b;"
                    "   border: 1px solid #333;"
                    "   border-top-right-radius: 0px;"
                    "   border-bottom-right-radius: 0px;"
                    "   border-top-left-radius: 20px;"
                    "   border-bottom-left-radius: 20px;"
                    "   border: 1px solid white;"
                    "   color: white;"
                    "   padding: 5px;"
                    "}"
                    );
                QPropertyAnimation *animOpacity = new QPropertyAnimation(opacityEff, "opacity");
                animOpacity->setDuration(250);
                animOpacity->setStartValue(opacityEff->opacity());
                animOpacity->setEndValue(1.0); // Загорается на 100%
                animOpacity->setEasingCurve(QEasingCurve::OutCubic);
                animOpacity->start(QAbstractAnimation::DeleteWhenStopped);
            }

            QPropertyAnimation *animMove = new QPropertyAnimation(ui->Label_TextZ, "pos");
            animMove->setDuration(250);
            animMove->setStartValue(ui->Label_TextZ->pos());
            animMove->setEndValue(QPoint(m_textEndX3, ui->Label_TextZ->y()));
            animMove->setEasingCurve(QEasingCurve::OutCubic);
            animMove->start(QAbstractAnimation::DeleteWhenStopped);

            return true;
        }
        else if (event->type() == QEvent::Leave) {
            // --- КУРСОР УБРАН (ВОЗВРАТ К ТУСКЛОМУ СОСТОЯНИЮ) ---
            if (opacityEff) {
                QPropertyAnimation *animOpacity = new QPropertyAnimation(opacityEff, "opacity");
                animOpacity->setDuration(250);
                animOpacity->setStartValue(opacityEff->opacity());
                animOpacity->setEndValue(0.3); // ИЗМЕНЕНО: Возвращается к 30% прозрачности вместо 0.0
                animOpacity->setEasingCurve(QEasingCurve::OutCubic);
                animOpacity->start(QAbstractAnimation::DeleteWhenStopped);
                ui->listZamen->setStyleSheet(
                    "QListWidget, QListView {"
                    "   background-color: #36333b;"
                    "   border: 1px solid #333;"
                    "   border-top-right-radius: 0px;"
                    "   border-bottom-right-radius: 0px;"
                    "   border-top-left-radius: 20px;"
                    "   border-bottom-left-radius: 20px;"
                    "   background-color: #1e1e1e;"
                    "   border: 1px solid white;"
                    "   color: white;"
                    "   padding: 5px;"
                    "}"
                    );
            }

            QPropertyAnimation *animMove = new QPropertyAnimation(ui->Label_TextZ, "pos");
            animMove->setDuration(250);
            animMove->setStartValue(ui->Label_TextZ->pos());
            animMove->setEndValue(QPoint(m_textStartX3, ui->Label_TextZ->y()));
            animMove->setEasingCurve(QEasingCurve::OutCubic);
            animMove->start(QAbstractAnimation::DeleteWhenStopped);

            return true;
        }
    }

    // 6. Обработка архива Броников
    if (obj == ui->btnArxivBR) {
        if (event->type() == QEvent::Enter) {
            showTooltip(ui->btnArxivBR, "Авто распаковка вашего архива с брониками - программа сама найдет папки вроде mpapartment или patchday...ng, извлечет и укажет пути к ним.", true);
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            hideTooltip();
            return true;
        }
    }

    if (obj == ui->listBronik || obj == ui->btnBR_install) {
        QGraphicsOpacityEffect *opacityEff = qobject_cast<QGraphicsOpacityEffect*>(ui->Label_bronik->graphicsEffect());

        if (event->type() == QEvent::Enter) {
            // --- КУРСОР НАВЕДЕН (ПОЯВЛЕНИЕ И СМЕЩЕНИЕ) ---
            if (opacityEff) {
                ui->listBronik->setStyleSheet(
                    "QListWidget, QListView {"
                    "   background-color: #e9775b;"
                    "   border: 1px solid #333;"
                    "   border-top-right-radius: 0px;"
                    "   border-bottom-right-radius: 0px;"
                    "   border-top-left-radius: 20px;"
                    "   border-bottom-left-radius: 20px;"
                    "   border: 1px solid white;"
                    "   color: white;"
                    "   padding: 5px;"
                    "}"
                    );
                QPropertyAnimation *animOpacity = new QPropertyAnimation(opacityEff, "opacity");
                animOpacity->setDuration(250);
                animOpacity->setStartValue(opacityEff->opacity());
                animOpacity->setEndValue(1.0); // Загорается на 100%
                animOpacity->setEasingCurve(QEasingCurve::OutCubic);
                animOpacity->start(QAbstractAnimation::DeleteWhenStopped);
            }

            QPropertyAnimation *animMove = new QPropertyAnimation(ui->Label_TextB, "pos");
            animMove->setDuration(250);
            animMove->setStartValue(ui->Label_TextB->pos());
            animMove->setEndValue(QPoint(m_textEndX2, ui->Label_TextB->y()));
            animMove->setEasingCurve(QEasingCurve::OutCubic);
            animMove->start(QAbstractAnimation::DeleteWhenStopped);

            return true;
        }
        else if (event->type() == QEvent::Leave) {
            // --- КУРСОР УБРАН (ВОЗВРАТ К ТУСКЛОМУ СОСТОЯНИЮ) ---
            if (opacityEff) {
                QPropertyAnimation *animOpacity = new QPropertyAnimation(opacityEff, "opacity");
                animOpacity->setDuration(250);
                animOpacity->setStartValue(opacityEff->opacity());
                animOpacity->setEndValue(0.3); // ИЗМЕНЕНО: Возвращается к 30% прозрачности вместо 0.0
                animOpacity->setEasingCurve(QEasingCurve::OutCubic);
                animOpacity->start(QAbstractAnimation::DeleteWhenStopped);
                ui->listBronik->setStyleSheet(
                    "QListWidget, QListView {"
                    "   background-color: #36333b;"
                    "   border: 1px solid #333;"
                    "   border-top-right-radius: 0px;"
                    "   border-bottom-right-radius: 0px;"
                    "   border-top-left-radius: 20px;"
                    "   border-bottom-left-radius: 20px;"
                    "   background-color: #1e1e1e;"
                    "   border: 1px solid white;"
                    "   color: white;"
                    "   padding: 5px;"
                    "}"
                    );
            }

            QPropertyAnimation *animMove = new QPropertyAnimation(ui->Label_TextB, "pos");
            animMove->setDuration(250);
            animMove->setStartValue(ui->Label_TextB->pos());
            animMove->setEndValue(QPoint(m_textStartX2, ui->Label_TextB->y()));
            animMove->setEasingCurve(QEasingCurve::OutCubic);
            animMove->start(QAbstractAnimation::DeleteWhenStopped);

            return true;
        }
    }
    // 1. Неоновая обработка btnTelegram
    if (obj == ui->btnTelegram) {
        QGraphicsDropShadowEffect *glow = qobject_cast<QGraphicsDropShadowEffect*>(ui->btnTelegram->graphicsEffect());

        if (event->type() == QEvent::Enter) {
            // Плавно зажигаем фирменный голубой неон вокруг кнопки
            if (glow) {
                QPropertyAnimation *a = new QPropertyAnimation(glow, "blurRadius");
                a->setDuration(180);
                a->setStartValue(glow->blurRadius());
                a->setEndValue(18); // Радиус свечения
                a->start(QAbstractAnimation::DeleteWhenStopped);
            }
            showTooltip(ui->btnTelegram, "Если у вас есть жалобы/предложения или вы хотите оставить<br>отзыв можете написать разработчику в Telegram", false);
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            // Плавно тушим неон до 0
            if (glow) {
                QPropertyAnimation *a = new QPropertyAnimation(glow, "blurRadius");
                a->setDuration(180);
                a->setStartValue(glow->blurRadius());
                a->setEndValue(0);
                a->start(QAbstractAnimation::DeleteWhenStopped);
            }
            hideTooltip();
            return true;
        }
    }

    // 2. Неоновая обработка btnDonat
    if (obj == ui->btnDonat) {
        QGraphicsDropShadowEffect *glow = qobject_cast<QGraphicsDropShadowEffect*>(ui->btnDonat->graphicsEffect());

        if (event->type() == QEvent::Enter) {
            // Плавно зажигаем сочный оранжевый неон вокруг кнопки
            if (glow) {
                QPropertyAnimation *a = new QPropertyAnimation(glow, "blurRadius");
                a->setDuration(180);
                a->setStartValue(glow->blurRadius());
                a->setEndValue(18);
                a->start(QAbstractAnimation::DeleteWhenStopped);
            }
            showTooltip(ui->btnDonat, "Благодарность проекту донатом", false);
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            // Плавно тушим неон до 0
            if (glow) {
                QPropertyAnimation *a = new QPropertyAnimation(glow, "blurRadius");
                a->setDuration(180);
                a->setStartValue(glow->blurRadius());
                a->setEndValue(0);
                a->start(QAbstractAnimation::DeleteWhenStopped);
            }
            hideTooltip();
            return true;
        }
    }

    // 3. Неоновая обработка btnSettings (Шестеренка)
    if (obj == ui->btnSettings) {
        QGraphicsDropShadowEffect *glow = qobject_cast<QGraphicsDropShadowEffect*>(ui->btnSettings->graphicsEffect());

        if (event->type() == QEvent::Enter) {
            // Плавно зажигаем белый ореол вокруг шестеренки
            if (glow) {
                QPropertyAnimation *a = new QPropertyAnimation(glow, "blurRadius");
                a->setDuration(180);
                a->setStartValue(glow->blurRadius());
                a->setEndValue(18);
                a->start(QAbstractAnimation::DeleteWhenStopped);
            }
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            // Плавно гасим белый ореол до 0
            if (glow) {
                QPropertyAnimation *a = new QPropertyAnimation(glow, "blurRadius");
                a->setDuration(180);
                a->setStartValue(glow->blurRadius());
                a->setEndValue(0);
                a->start(QAbstractAnimation::DeleteWhenStopped);
            }
            return true;
        }
    }
    // 3. Обработка архива Redux
    if (obj == ui->btnArxivRedux) {
        if (event->type() == QEvent::Enter) {
            showTooltip(ui->btnArxivRedux, "Авто распаковка вашего архива с редуксом - программа сама найдет нужные файлы, извлечет и укажет пути к ним. Процесс обнаружения и извлечения занимает от 5-10 секунд. (На данный момент из поддержуемых запароленных архивов - только архивы от Majestic-mods.ru)", true);
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            hideTooltip();
            return true;
        }
    }

    if (obj == ui->btnArxivGuns) {
        if (event->type() == QEvent::Enter) {
            showTooltip(ui->btnArxivGuns, "Авто распаковка вашего архива с ган паком - программа сама найдет нужные файлы, извлечет и укажет пути к ним. Процесс обнаружения и извлечения занимает от 5-10 секунд. (На данный момент из поддержуемых запароленных архивов - только архивы от Majestic-mods.ru)", true);
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            hideTooltip();
            return true;
        }
    }

    if (obj == ui->btnArxivSounds) {
        if (event->type() == QEvent::Enter) {
            showTooltip(ui->btnArxivSounds, "Авто распаковка вашего архива с модифицированными звуками - программа сама найдет нужные файлы, извлечет и укажет пути к ним. Процесс обнаружения и извлечения занимает от 5-10 секунд. (На данный момент из поддержуемых запароленных архивов - только архивы от Majestic-mods.ru)", true);
            return true;
        }
        else if (event->type() == QEvent::Leave) {
            hideTooltip();
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
void MainWindow::on_btnOknoDop_clicked() {
    ui->btnExitGP->move(564, 35);
    if(oknoDop) {
        ui->btnExitGP->setVisible(false);
        animateWindowOpen(ui->oknoDiscleamer, ui->btnOknoDop);
        ui->oknoKnopohki->show();
        ui->oknoKnopohki->raise();
        oknoDop = false;
    }
}
void MainWindow::on_btnExitGP_clicked()
{
    oknoDop = true;
    ui->lineV->setVisible(false);

    // Список всех твоих окон
    QList<QWidget*> windows = {ui->oknoPresets, ui->oknoSettings, ui->oknoGP,
                                ui->oknoDiscleamer, ui->oknoZV, ui->oknoHDD, ui->oknoDLC, ui->oknoBR, ui->oknoZM};

    // Запускаем анимацию для того окна, которое сейчас видно
    for(QWidget* w : windows) {
        if(w->isVisible()) {
            animateFadeOut(w);
        }
    }

    // Крестик и панель кнопок тоже плавно гасим (через обычную анимацию)
    QGraphicsOpacityEffect* exEff = qobject_cast<QGraphicsOpacityEffect*>(ui->btnExitGP->graphicsEffect());
    if(exEff) {
        QPropertyAnimation* a = new QPropertyAnimation(exEff, "opacity");
        a->setDuration(200);
        a->setStartValue(1.0);
        a->setEndValue(0.0);
        connect(a, &QPropertyAnimation::finished, ui->btnExitGP, &QWidget::hide);
        a->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        ui->btnExitGP->hide();
    }

    // Если нужно скрыть и панель кнопок (oknoKnopohki)
    if(ui->oknoKnopohki->isVisible()) {
        animateFadeOut(ui->oknoKnopohki);
    }
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
    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString basePath = rsReg.value("InstallFolder").toString();

    if (basePath.isEmpty()) {
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            basePath = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }

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

    qCritical() << "[AutoSearchDLS] Не удалось автоматически определить директорию dlcpacks для Ган-паков.";
    ui->leditDLS->setText("Ошибка"); // Выводим "Ошибка" в поле ввода

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

    bool isDlsOk = pathDLS.endsWith("/dlcpacks") && QDir(pathDLS).exists();
    bool pathHasForbidden = pathGunPack.contains("patchday18ng") || pathGunPack.contains("mpapartment");

    QDir gpDir(pathGunPack);
    bool hasRequiredContent = gpDir.exists("patchday18ng") || gpDir.exists("mpapartment");

    if (isDlsOk && !pathHasForbidden && hasRequiredContent) {
        QStringList conflicts;
        QStringList gpEntries = gpDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

        // Конфликт с брониками
        if (ui->checkAutoLoadBR->isChecked()) {
            QDir brDir(ui->leditBR_Pack->text());
            QStringList brEntries = brDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            foreach(const QString &folder, gpEntries) {
                if (brEntries.contains(folder, Qt::CaseInsensitive)) conflicts << folder + " (Броники)";
            }
        }

        // Конфликт с замененками
        if (ui->checkAutoLoadZM->isChecked()) {
            QDir zmDir(ui->leditZM_Pack->text());
            QStringList zmEntries = zmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            foreach(const QString &folder, gpEntries) {
                if (zmEntries.contains(folder, Qt::CaseInsensitive)) conflicts << folder + " (Замененки)";
            }
        }

        if (!conflicts.isEmpty()) {
            QMessageBox::warning(this, "Конфликт путей",
                                 "Обнаружены совпадающие папки установки в разных модах:\n" +
                                     conflicts.join("\n") +
                                     "\nОни перезапишут файлы друг друга при автозапуске!");
        }

        QSettings("MyCompany", "MyGameTool").setValue("checkAutoLoadGP", true);
    } else {
        ui->checkAutoLoadGP->blockSignals(true);
        ui->checkAutoLoadGP->setChecked(false);
        ui->checkAutoLoadGP->blockSignals(false);

        QStringList errors;
        if (!isDlsOk) errors << "- Путь DLS должен заканчиваться на 'dlcpacks'";
        if (pathHasForbidden) errors << "- В самом пути к ганпакам не должно быть 'patchday18ng' или 'mpapartment'";
        if (!hasRequiredContent) errors << "- Внутри папки должна быть папка 'patchday18ng' или 'mpapartment'";

        QMessageBox::critical(this, "Ошибка GunPack", "Проверьте условия:\n" + errors.join("\n"));
    }
}

void MainWindow::on_checkAutoLoadBR_toggled(bool checked) {
    if (!checked) {
        QSettings("MyCompany", "MyGameTool").setValue("checkAutoLoadBR", false);
        return;
    }

    QString pathDLS = QDir::fromNativeSeparators(ui->leditDLS_2->text()).toLower();
    QString pathArmor = QDir::fromNativeSeparators(ui->leditBR_Pack->text()).toLower();

    if (pathDLS.endsWith("/")) pathDLS.chop(1);
    if (pathArmor.endsWith("/")) pathArmor.chop(1);

    bool isDlsOk = pathDLS.endsWith("/dlcpacks") && QDir(pathDLS).exists();

    QDir brDir(pathArmor);
    bool hasRequiredContent = false;
    QStringList brEntries = brDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(const QString &entry, brEntries) {
        if (entry.compare("mpapartment", Qt::CaseInsensitive) == 0 ||
            (entry.startsWith("patchday", Qt::CaseInsensitive) && entry.endsWith("ng", Qt::CaseInsensitive))) {
            hasRequiredContent = true;
            break;
        }
    }

    if (isDlsOk && hasRequiredContent) {
        QStringList conflicts;

        // Конфликт с ганпаками
        if (ui->checkAutoLoadGP->isChecked()) {
            QDir gpDir(ui->leditGunPuck->text());
            QStringList gpEntries = gpDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            foreach(const QString &folder, brEntries) {
                if (gpEntries.contains(folder, Qt::CaseInsensitive)) conflicts << folder + " (Ган-паки)";
            }
        }

        // Конфликт с замененками
        if (ui->checkAutoLoadZM->isChecked()) {
            QDir zmDir(ui->leditZM_Pack->text());
            QStringList zmEntries = zmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            foreach(const QString &folder, brEntries) {
                if (zmEntries.contains(folder, Qt::CaseInsensitive)) conflicts << folder + " (Замененки)";
            }
        }

        if (!conflicts.isEmpty()) {
            QMessageBox::warning(this, "Конфликт путей",
                                 "Обнаружены совпадающие папки установки в разных модах:\n" +
                                     conflicts.join("\n") +
                                     "\nОни перезапишут файлы друг друга при автозапуске!");
        }

        QSettings("MyCompany", "MyGameTool").setValue("checkAutoLoadBR", true);
    } else {
        ui->checkAutoLoadBR->blockSignals(true);
        ui->checkAutoLoadBR->setChecked(false);
        ui->checkAutoLoadBR->blockSignals(false);

        QStringList errors;
        if (!isDlsOk) errors << "- Путь DLS должен заканчиваться на 'dlcpacks'";
        if (!hasRequiredContent) errors << "- В папке броников должна быть 'mpapartment' или любая папка 'patchday...ng'";

        QMessageBox::critical(this, "Ошибка Броников", "Проверьте условия:\n" + errors.join("\n"));
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
        qCritical() << ">>> Папка бэкапа не существует. Восстанавливать нечего.";
        return true;
    }

    // Получаем список всех файлов и папок в бэкапе
    QFileInfoList backupItems = backupDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);

    if (backupItems.isEmpty()) {
        qCritical() << ">>> Папка бэкапа пуста.";
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

            qWarning() << ">>> Файл занят, попытка удаления" << i+1;
            QThread::msleep(1000); // Ждем секунду, если игра еще закрывается
        }

        if (!removed) {
            qCritical() << ">>> ОШИБКА: Не удалось удалить модовый файл:" << targetPath;
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
            qCritical() << ">>> ОШИБКА копирования оригинала назад:" << itemName;
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
        qCritical() << "Исходный файл не найден:" << sourcePath;
        return false;
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QString dataFolder = appDir + "/data";
    QString destPath = dataFolder + "/update.rpf";

    qInfo() << "Целевая папка:" << dataFolder;
    qInfo() << "Полный путь к копии:" << destPath;

    QDir dir;
    if (!dir.mkpath(dataFolder)) {
        qCritical() << "Не удалось создать папку:" << dataFolder;
        return false;
    }

    if (QFile::exists(destPath) && !QFile::remove(destPath)) {
        qCritical() << "Не удалось удалить старый файл:" << destPath;
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
            qCritical() << "Ошибка: файл не найден после копирования:" << destPath;
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
void MainWindow::on_btnGanpacOpen_clicked() {
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoDLC, ui->btnGanpacOpen);
    ui->lineV->move(1 , 25);
    ui->lineV->setVisible(true);
}
void MainWindow::on_btnZVOpen_clicked() {
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoZV, ui->btnZVOpen);
    ui->lineV->move(1 , 85);
    ui->lineV->setVisible(true);
}

//установка звуков оружия

void MainWindow::on_btnAutoSearchZV_clicked() {
    qInfo() << "=== Автопоиск x64\\audio\\sfx ===";

    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString basePath = rsReg.value("InstallFolder").toString();

    if (basePath.isEmpty()) {
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            basePath = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }

    if (!basePath.isEmpty()) {
        QString sfxPath = basePath + "/x64/audio/sfx";
        if (QDir(sfxPath).exists()) {
            m_x64AudioSfxPath = QDir::toNativeSeparators(sfxPath);
            ui->leditPapcaZV->setText(m_x64AudioSfxPath);

            QSettings settings("MyCompany", "MyGameTool");
            settings.setValue("Paths/X64AudioSfx", m_x64AudioSfxPath);

            qInfo() << "Путь до x64\\audio\\sfx найден автоматически:" << m_x64AudioSfxPath;
            return;
        }
    }

    qCritical() << "[AutoSearchZV] Не удалось автоматически найти папку x64\\audio\\sfx в системном реестре.";
    ui->leditPapcaZV->setText("Ошибка"); // Выводим "Ошибка" в поле ввода
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

        qInfo() << "Путь до x64\\audio\\sfx установлен вручную:" << m_x64AudioSfxPath;
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

        qInfo() << "Путь к модам установлен:" << m_modSoundPath;
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
            qCritical() << ">>> ОШИБКА WinAPI при копировании звука:" << fileName << "Код ошибки:" << err;
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
                qCritical() << ">>> ОШИБКА бэкапа звука:" << fileName << "Error:" << GetLastError();
                return false;
            }
            qInfo() << ">>> Бэкап создан:" << fileName;
        }
    }
    return true;
}

void MainWindow::saveSettings() {
    QSettings s("MyCompany", "MyGameTool");
    s.setValue("ZmSourcePath", ui->leditZM_Pack->text());
    s.setValue("ZmTargetPath", ui->leditDLS_3->text());
    s.setValue("checkAutoLoadZM", ui->checkAutoLoadZM->isChecked());
    s.setValue("ArmorSourcePath", ui->leditBR_Pack->text());
    s.setValue("ArmorTargetPath", ui->leditDLS_2->text());
    s.setValue("checkAutoLoadBR", ui->checkAutoLoadBR->isChecked());
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
    qInfo() << "Настройки сохранены.";
}

void MainWindow::loadSettings() {
    QSettings s("MyCompany", "MyGameTool");
    ui->leditRedux->setText(s.value("ReduxPath").toString());
    ui->leditPapka->setText(s.value("GamePath").toString());
    ui->leditOrig->setText(s.value("OrigPath").toString());
    ui->leditGunPuck->setText(s.value("GunPackPath").toString());
    ui->leditDLS->setText(s.value("DlsPath").toString());
    ui->leditBR_Pack->setText(s.value("ArmorSourcePath").toString());
    ui->leditDLS_2->setText(s.value("ArmorTargetPath").toString());
    ui->checkAutoLoadBR->blockSignals(true);
    ui->checkAutoLoadBR->setChecked(s.value("checkAutoLoadBR", false).toBool());
    ui->checkAutoLoadBR->blockSignals(false);
    ui->leditZM_Pack->setText(s.value("ZmSourcePath").toString());
    ui->leditDLS_3->setText(s.value("ZmTargetPath").toString());
    ui->checkAutoLoadZM->blockSignals(true);
    ui->checkAutoLoadZM->setChecked(s.value("checkAutoLoadZM", false).toBool());
    ui->checkAutoLoadZM->blockSignals(false);

    m_zmSourcePath = s.value("ZmSourcePath").toString();
    m_zmTargetPath = s.value("ZmTargetPath").toString();

    m_armorSourcePath = s.value("ArmorSourcePath").toString();
    m_armorTargetPath = s.value("ArmorTargetPath").toString();
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

    ui->checkSound->setChecked(s.value("Settings/SoundEnabled", true).toBool());

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
    bool isSfxNeok = !pathModZV.endsWith("/sfx") && QDir(pathModZV).exists();

    // 2. Проверка папки с модами (откуда берем RESIDENT.rpf и т.д.)
    QDir modDir(pathModZV);
    bool hasResident = modDir.exists("RESIDENT.rpf");
    bool hasWeapons = modDir.exists("WEAPONS_PLAYER.rpf");
    bool hasZVOu = hasResident || hasWeapons;

    if (isSfxOk && hasZVOu && isSfxNeok) {
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
        if(!isSfxNeok) errors << "- Путь к звукам не должен содержать sfx, внимательно прочитайте инструкцию!!!";

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
        qCritical() << "ОШИБКА: Источник и цель — один и тот же файл!";
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
                qCritical() << ">>> ОШИБКА WinAPI: Не удалось удалить модовый звук:" << GetLastError();
                failCount++;
                continue;
            }
        }

        // Возвращаем чистый файл из бэкапа
        if (!CopyFileW((LPCWSTR)backupPath.utf16(), (LPCWSTR)targetPath.utf16(), FALSE)) {
            qCritical() << ">>> ОШИБКА WinAPI при возврате звука:" << GetLastError();
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
void MainWindow::on_btnHDD_OpenDis_clicked() {
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoHDD, ui->btnHDD_OpenDis);
    ui->lineV->move(1 , 145);
    ui->lineV->setVisible(true);
}
void MainWindow::on_btnNext_clicked()
{
    ui->lblIgraInstallTo->setVisible(false);
    ui->Instruction->setVisible(false);
    ui->btnNext->setVisible(false);
    ui->lblInstrucktion->setVisible(false);
    ui->infoSignals->setVisible(false);
    ui->chkHddWarning->setVisible(false);
    ui->lblHddAlert->setVisible(false);
}

void MainWindow::on_btnSaveTime_clicked()
{
    QSettings settings("MyCompany", "MyGameTool");
    settings.setValue("pauseTime", ui->leditTimeVvod->text());
    settings.sync(); // Сброс на диск

    // 2. Записываем число внутрь .bat файла
    saveTimeToFile();

    qInfo() << "Время сохранено!";

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
        qInfo() << "Авто-режим .bat включен";
    } else {
        // 3. Если выключили: останавливаем таймер
        checkTimer->stop();
        gtaWasRunning = false;
        qInfo() << "Авто-режим отключен.";
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
        qInfo() << "Обновлено время на:" << newTime;
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

    // ИСПРАВЛЕНО: Быстрая и легкая проверка процессов вместо медленного tasklist
    bool gtaFound = isProcessRunning("GTA5.exe") ||
                    isProcessRunning("GTA5_Enhanced.exe") ||
                    isProcessRunning("GTA5_Enhanced_BE.exe");

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
    qInfo() << "Время сохранено в настройки:" << ui->leditTimeVvod->text();
}
void MainWindow::runBatch() {
    saveTimeToFile(); // Сначала сохраняем актуальное время в файл

    QString folderPath = QCoreApplication::applicationDirPath() + "/time/";
    QString filePath = folderPath + "time.bat";

    std::wstring nativeFile = filePath.toStdWString();
    std::wstring nativeDir = folderPath.toStdWString();

    // Запускаем батник. 5-й параметр (nativeDir) лечит ошибку с PsSuspend64
    ShellExecute(NULL, L"open", nativeFile.c_str(), NULL, nativeDir.c_str(), SW_SHOWNORMAL);

    qInfo() << "Батник запущен";
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
    QDesktopServices::openUrl(QUrl(downloadUrl));
}
void MainWindow::on_btnExitNF_clicked()
{
    ui->oknoNF->setVisible(false);
    ui->btnExitNF->setVisible(false);
}
void MainWindow::on_btnAltV_clicked()
{
    blurEf(false);
    ui->oknoGta5V->setVisible(false);
    ui->cmbServer->setCurrentIndex(1);
    m_soundSuccess->play();

    // Если хочешь сразу сохранить в настройки:
    QSettings settings("MyCompany", "MyGameTool");
    settings.setValue("SelectedPlatform", 1);
}
void MainWindow::on_btnRage_clicked()
{
    blurEf(false);
    ui->oknoGta5V->setVisible(false);
    ui->cmbServer->setCurrentIndex(2);
    m_soundSuccess->play();

    // Если хочешь сразу сохранить в настройки:
    QSettings settings("MyCompany", "MyGameTool");
    settings.setValue("SelectedPlatform", 2);
}
void MainWindow::on_btnSettings_clicked() {
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoSettings, ui->btnSettings);
}
void MainWindow::on_cmbServer_currentIndexChanged(int index)
{
    if (index == 1) {
        qInfo() << "Выбран AltV";
        // Здесь твоя логика для AltV
    } else if (index == 2) {
        qInfo() << "Выбран RageMP";
        // Логика для Rage
    } else {
        qInfo() << "Ничего не выбрано";
    }
    QSettings settings("MyCompany", "MyGameTool");
    // Сохраняем индекс (например: 0 - Не выбрано, 1 - AltV, 2 - RageMP)
    settings.setValue("SelectedPlatform", index);
}
void MainWindow::on_btnOpenLogs_clicked()
{
    // Получаем полный путь к файлу лога рядом с экзешником
    QString logPath = QCoreApplication::applicationDirPath() + "/ReplaceX.log";

    QFileInfo checkFile(logPath);
    if (checkFile.exists()) {
        // Вариант А: Просто открыть сам текстовый файл (в блокноте)
        QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));

        // Вариант Б: Открыть ПАПКУ и выделить в ней этот файл (самый удобный вариант)
        // На Windows это делается так:
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(logPath);
        QProcess::startDetached("explorer", args);
    } else {
        qInfo() << "Файл лога еще не создан:" << logPath;
    }
}
void MainWindow::on_btnMajesticMods_clicked()
{
    QString MajesticMods = "https://majestic-mods.ru/";
    QDesktopServices::openUrl(QUrl(MajesticMods));
}
void MainWindow::on_checkSound_toggled(bool checked)
{
    QSettings settings("MyCompany", "MyGameTool");

    settings.setValue("Settings/SoundEnabled", checked);

    settings.sync();

    if (checked) {
        qInfo() << "Звуковые уведомления включены";
    } else {
        qInfo() << "Звуковые уведомления выключены";
    }
}

void MainWindow::on_checkSound_clicked()
{
    m_soundSuccess->play();
}
void MainWindow::on_btnGaid_clicked()
{
    QDesktopServices::openUrl(QUrl("https://youtu.be/y7JQ1iXufUE?si=mqFOa9UK32hMu8UT"));
}
void MainWindow::on_chkAutoZagruzkaWin_stateChanged(int state)
{
    // Путь в реестре, где Windows ищет программы для автозапуска
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

    // Получаем полный путь к твоему .exe файлу
    QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    if (state == Qt::Checked) {
        // Добавляем в автозагрузку.
        // Добавляем флаг "--autostart", чтобы программа знала, что она запустилась сама
        settings.setValue("ReplaceX", "\"" + appPath + "\" --autostart");
    } else {
        // Удаляем из автозагрузки
        settings.remove("ReplaceX");
    }
}
//прессеты
// 1. Функция обновления списка пресетов в комбобоксе
void MainWindow::updatePresetsCombo() {
    ui->cmbPresets->clear();
    ui->cmbPresets->addItem("Выберите пресет...");

    QSettings s("MyCompany", "MyGameTool");
    s.beginGroup("Presets");
    QStringList presets = s.childGroups();
    ui->cmbPresets->addItems(presets);
    s.endGroup();
}

// 2. Кнопка сохранения пресета
void MainWindow::on_btnSavePreset_clicked() {
    bool ok;
    QString name = QInputDialog::getText(this, "Новый пресет",
                                         "Введите название пресета:", QLineEdit::Normal,
                                         "", &ok);
    if (!ok || name.isEmpty()) return;

    // Создаем мини-диалог выбора компонентов
    QDialog dlg(this);
    dlg.setWindowTitle("Что сохранить?");
    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QCheckBox *cbRedux = new QCheckBox("Пути Редукса/Оригинала", &dlg);
    QCheckBox *cbGP = new QCheckBox("Пути Ган-паков", &dlg);
    QCheckBox *cbZV = new QCheckBox("Пути Звуков", &dlg);
    QCheckBox *cbBR = new QCheckBox("Пути Броников", &dlg);      // Новый чекбокс
    QCheckBox *cbZM = new QCheckBox("Пути Замененок", &dlg);    // Новый чекбокс

    cbRedux->setChecked(true);
    layout->addWidget(cbRedux);
    layout->addWidget(cbGP);
    layout->addWidget(cbZV);
    layout->addWidget(cbBR);                                    // Добавляем на макет
    layout->addWidget(cbZM);                                    // Добавляем на макет

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() == QDialog::Accepted) {
        QSettings s("MyCompany", "MyGameTool");
        s.beginGroup("Presets/" + name);

        if (cbRedux->isChecked()) {
            s.setValue("ReduxPath", ui->leditRedux->text());
            s.setValue("OrigPath", ui->leditOrig->text());
            s.setValue("GamePath", ui->leditPapka->text());
        }
        if (cbGP->isChecked()) {
            s.setValue("GunPackPath", ui->leditGunPuck->text());
            s.setValue("DlsPath", ui->leditDLS->text());
        }
        if (cbZV->isChecked()) {
            s.setValue("SoundMod", ui->leditModZV->text());
            s.setValue("SfxPath", ui->leditPapcaZV->text());
        }
        if (cbBR->isChecked()) {                                // Сохраняем броники
            s.setValue("ArmorSourcePath", ui->leditBR_Pack->text());
            s.setValue("ArmorTargetPath", ui->leditDLS_2->text());
        }
        if (cbZM->isChecked()) {                                // Сохраняем замененки
            s.setValue("ZmSourcePath", ui->leditZM_Pack->text());
            s.setValue("ZmTargetPath", ui->leditDLS_3->text());
        }

        s.endGroup();
        s.sync();

        updatePresetsCombo();
        ui->cmbPresets->setCurrentText(name);
        qInfo() << "Пресет сохранен:" << name;
    }
}

// 3. Загрузка пресета при выборе в комбобоксе
void MainWindow::on_cmbPresets_activated(int index) {
    QString name = ui->cmbPresets->itemText(index);
    if (name == "Выберите пресет...") return;

    QSettings s("MyCompany", "MyGameTool");
    s.beginGroup("Presets/" + name);

    // Загружаем только те поля, которые есть в этом пресете
    if (s.contains("ReduxPath")) ui->leditRedux->setText(s.value("ReduxPath").toString());
    if (s.contains("OrigPath")) ui->leditOrig->setText(s.value("OrigPath").toString());
    if (s.contains("GamePath")) ui->leditPapka->setText(s.value("GamePath").toString());
    if (s.contains("GunPackPath")) ui->leditGunPuck->setText(s.value("GunPackPath").toString());
    if (s.contains("DlsPath")) ui->leditDLS->setText(s.value("DlsPath").toString());

    // Загрузка путей броников
    if (s.contains("ArmorSourcePath")) {
        m_armorSourcePath = s.value("ArmorSourcePath").toString();
        ui->leditBR_Pack->setText(m_armorSourcePath);
    }
    if (s.contains("ArmorTargetPath")) {
        m_armorTargetPath = s.value("ArmorTargetPath").toString();
        ui->leditDLS_2->setText(m_armorTargetPath);
    }

    // Загрузка путей замененок
    if (s.contains("ZmSourcePath")) {
        m_zmSourcePath = s.value("ZmSourcePath").toString();
        ui->leditZM_Pack->setText(m_zmSourcePath);
    }
    if (s.contains("ZmTargetPath")) {
        m_zmTargetPath = s.value("ZmTargetPath").toString();
        ui->leditDLS_3->setText(m_zmTargetPath);
    }

    if (s.contains("SoundMod")) {
        m_modSoundPath = s.value("SoundMod").toString();
        ui->leditModZV->setText(m_modSoundPath);
    }
    if (s.contains("SfxPath")) {
        m_x64AudioSfxPath = s.value("SfxPath").toString();
        ui->leditPapcaZV->setText(m_x64AudioSfxPath);
    }

    s.endGroup();

    // Принудительно сохраняем как текущие настройки
    s.setValue("LastPresetName", name);
    s.sync();

    saveSettings();
    qInfo() << "Пресет загружен и запомнен:" << name;
}

// 4. Удаление пресета
void MainWindow::on_btnDeletePreset_clicked() {
    QString name = ui->cmbPresets->currentText();
    if (name == "Выберите пресет..." || name.isEmpty()) return;

    auto res = QMessageBox::question(this, "Удаление", "Удалить пресет " + name + "?");
    if (res == QMessageBox::Yes) {
        QSettings s("MyCompany", "MyGameTool");
        s.remove("Presets/" + name);

        // Если удаляем тот, что был последним — очищаем запись
        if (s.value("LastPresetName").toString() == name) {
            s.remove("LastPresetName");
        }

        s.sync();
        updatePresetsCombo();
    }
}
void MainWindow::on_btnOpenPress_K_clicked() {
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoPresets, ui->btnOpenPress_K);
    ui->lineV->move(1 , 205);
    ui->lineV->setVisible(true);
}

void MainWindow::on_btnAutorskiPrava_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/dima311313-boop/ReplaceX-for-GTA-V/blob/ReplaceX/LICENSE"));
}

// Реализация кнопок
void MainWindow::on_btnArxivRedux_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "Выберите архив с Redux", "", "Archives (*.rar *.zip *.7z)");
    if (!path.isEmpty()) {
        ui->leditRedux->setText("В процессе.....");
        emit requestUnpack(0, path, ui->leditOrig->text());
    }
}

void MainWindow::on_btnArxivGuns_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "Выберите архив с Ган-паком", "", "Archives (*.rar *.zip *.7z)");
    if (!path.isEmpty()) {
        ui->leditGunPuck->setText("В процессе.....");
        emit requestUnpack(1, path, "");
    }
}

void MainWindow::on_btnArxivSounds_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "Выберите архив со Звуками", "", "Archives (*.rar *.zip *.7z)");
    if (!path.isEmpty()) {
        ui->leditModZV->setText("В процессе.....");
        emit requestUnpack(2, path, "");
    }
}

// Слот получения результата
void MainWindow::onUnpackResult(int type, QString resultPath) {
    if (resultPath == "Ошибка") {
        if (type == 0) ui->leditRedux->setText("Ошибка");
        else if (type == 1) ui->leditGunPuck->setText("Ошибка");
        else if (type == 2) ui->leditModZV->setText("Ошибка");
        else if (type == 3) ui->leditBR_Pack->setText("Ошибка"); // Новый блок

        m_soundError->play();
        return;
    }

    if (type == 0) {
        ui->leditRedux->setText(resultPath);
    }
    else if (type == 1) {
        ui->leditGunPuck->setText(resultPath);
    }
    else if (type == 2) {
        ui->leditModZV->setText(resultPath);
        m_modSoundPath = resultPath;
    }
    else if (type == 3) { // Новый блок
        ui->leditBR_Pack->setText(resultPath);
        m_armorSourcePath = resultPath;
    }
    // Найди метод MainWindow::onUnpackResult и добавь проверку:
    else if (type == 4) {
        ui->leditZM_Pack->setText(resultPath);
        m_zmSourcePath = resultPath;

        // ПРОВЕРКА ОГРАНИЧЕНИЯ (более 5 папок в архиве)
        QDir dir(resultPath);
        QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        if (subdirs.size() > 5) {
            QMessageBox::warning(this, "Превышение лимита замененок",
                                 QString("Внимание! В распакованном архиве обнаружено %1 папок замененок.\n"
                                         "Для стабильности работы игры и избежания вылетов крайне рекомендуется "
                                         "устанавливать не более 4-5 папок замененок одновременно. "
                                         "Пожалуйста, сократите их количество вручную.")
                                     .arg(subdirs.size()));
        }
    }
    saveSettings();
    m_soundSuccess->play();
    ui->miniProgress->setText("Пути обновлены и сохранены");
}
void MainWindow::on_btnAutoCopyUpdate_clicked()
{
    QSettings settings("MyCompany", "MyGameTool");

    // Попробуем найти автоматически без вызова диалога
    QString rpfPath = "";
    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString basePath = rsReg.value("InstallFolder").toString();

    if (basePath.isEmpty()) {
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            basePath = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }

    if (!basePath.isEmpty()) {
        QString testPath = basePath + "/update/update.rpf";
        if (QFile::exists(testPath)) {
            rpfPath = QDir::toNativeSeparators(testPath);
        }
    }

    if (!rpfPath.isEmpty()) {
        if (copyUpdateRpfToAppDir(rpfPath)) {
            settings.setValue("FirstRun", true);
            settings.setValue("Paths/OriginalFile", ui->leditOrig->text());
            ui->miniProgress->setText("Оригинал успешно импортирован");
        } else {
            qCritical() << "[AutoCopyUpdate] Ошибка копирования автоматически найденного файла:" << rpfPath;
            ui->leditOrig->setText("Ошибка"); // Выводим "Ошибка" в поле ввода
        }
    } else {
        qCritical() << "[AutoCopyUpdate] Автопоиск файла update.rpf завершился ошибкой. Файл не найден в реестре или по путям Steam.";
        ui->leditOrig->setText("Ошибка"); // Выводим "Ошибка" в поле ввода

        // Опционально предлагаем ручной поиск
        QString manualPath = QFileDialog::getOpenFileName(
            this,
            "Найдите update.rpf (Grand Theft Auto V Legacy\\update\\update.rpf)",
            "",
            "RPF-файлы (*.rpf)"
            );
        if (!manualPath.isEmpty()) {
            if (copyUpdateRpfToAppDir(manualPath)) {
                settings.setValue("FirstRun", true);
                settings.setValue("Paths/OriginalFile", ui->leditOrig->text());
            }
        }
    }
}

void MainWindow::on_chkHddWarning_toggled(bool checked) {
    if (checked) {
        if (m_blinkTimer) m_blinkTimer->stop();
        ui->lblHddAlert->setVisible(false);
        ui->chkHddWarning->setVisible(false);
        ui->infoSignals->setVisible(false);

        // Запоминаем, что пользователь видел предупреждение
        QSettings settings("MyCompany", "MyGameTool");
        settings.setValue("Settings/HddWarningSeen", true);

        m_soundSuccess->play();
    }
}
void MainWindow::on_btnGP_install_clicked()
{
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoGP, ui->btnGP_install);
    ui->lineV->move(1 , 25);
    ui->lineV->setVisible(true);
}
void MainWindow::on_btnBR_install_clicked()
{
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoBR, ui->btnBR_install);
    ui->lineV->move(1 , 25);
    ui->lineV->setVisible(true);
}
// --- СЛОТЫ ДЛЯ БРОНИКОВ ---

// Автопоиск dlcpacks для броников (аналог ганпаков)
void MainWindow::on_btnAutoSearchDLS_2_clicked() {
    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString basePath = rsReg.value("InstallFolder").toString();

    if (basePath.isEmpty()) {
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            basePath = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }

    QString targetPath;
    if (!basePath.isEmpty()) {
        targetPath = basePath + "/update/x64/dlcpacks";
        if (QDir(targetPath).exists()) {
            m_armorTargetPath = QDir::toNativeSeparators(targetPath);
            ui->leditDLS_2->setText(m_armorTargetPath);
            QSettings("MyCompany", "MyGameTool").setValue("ArmorTargetPath", m_armorTargetPath);
            return;
        }
    }

    qCritical() << "[AutoSearchDLS2] Не удалось автоматически определить директорию dlcpacks для Броников.";
    ui->leditDLS_2->setText("Ошибка"); // Выводим "Ошибка" в поле ввода

    QString manualPath = QFileDialog::getExistingDirectory(this, "Укажите папку ...\\update\\x64\\dlcpacks", "C:/");
    if (!manualPath.isEmpty()) {
        m_armorTargetPath = QDir::toNativeSeparators(manualPath);
        ui->leditDLS_2->setText(m_armorTargetPath);
        QSettings("MyCompany", "MyGameTool").setValue("ArmorTargetPath", m_armorTargetPath);
    }
}
// Ручной выбор dlcpacks для броников
void MainWindow::on_btnDLS_2_clicked() {
    QString path = QFileDialog::getExistingDirectory(this, "Выберите папку ...\\update\\x64\\dlcpacks", m_armorTargetPath);
    if (!path.isEmpty()) {
        m_armorTargetPath = QDir::toNativeSeparators(path);
        ui->leditDLS_2->setText(m_armorTargetPath);
        QSettings("MyCompany", "MyGameTool").setValue("ArmorTargetPath", m_armorTargetPath);
    }
}

// Ручной выбор папки с брониками
void MainWindow::on_btnPapkaBR_clicked() {
    QString path = QFileDialog::getExistingDirectory(this, "Выберите папку с брониками", m_armorSourcePath);
    if (!path.isEmpty()) {
        m_armorSourcePath = QDir::toNativeSeparators(path);
        ui->leditBR_Pack->setText(m_armorSourcePath);
        QSettings("MyCompany", "MyGameTool").setValue("ArmorSourcePath", m_armorSourcePath);
    }
}

// Ручная установка броников
void MainWindow::on_btnReplaceBR_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualInstallArmorPacks(getCurrentConfig());
}

// Ручное восстановление оригиналов для броников
void MainWindow::on_btnReplaceOrigBR_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualRestoreArmorPacks(getCurrentConfig());
}

// Авто распаковка архива броников (Запуск распаковщика, тип 3)
void MainWindow::on_btnArxivBR_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "Выберите архив с брониками", "", "Archives (*.rar *.zip *.7z)");
    if (!path.isEmpty()) {
        ui->leditBR_Pack->setText("В процессе.....");
        emit requestUnpack(3, path, "");
    }
}

void MainWindow::on_btnBack_clicked()
{
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoDLC, ui->btnBack);
    ui->lineV->move(1 , 25);
    ui->lineV->setVisible(true);
}
void MainWindow::on_btnBack_2_clicked()
{
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoDLC, ui->btnBack_2);
    ui->lineV->move(1 , 25);
    ui->lineV->setVisible(true);
}
void MainWindow::on_btnBack_3_clicked()
{
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoDLC, ui->btnBack_2);
    ui->lineV->move(1 , 25);
    ui->lineV->setVisible(true);
}
void MainWindow::on_btnClearSetings_clicked()
{
    // 1. Показываем диалоговое окно подтверждения
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Подтверждение сброса",
        "Вы уверены, что хотите очистить настройки программы? Это очистит все ваши указанные пути.",
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        // 2. Очищаем настройки через встроенный механизм Qt (удаляет все ключи в ветке реестра)
        QSettings settings("MyCompany", "MyGameTool");
        settings.clear();
        settings.sync();

        // 3. Для дополнительной надежности принудительно выполняем команду реестра.
        // QProcess запускает утилиты без создания видимого окна консоли (полностью скрытно).
        QProcess proc;
        proc.start("reg", QStringList() << "delete" << "HKEY_CURRENT_USER\\Software\\MyCompany\\MyGameTool" << "/f");
        proc.waitForFinished();

        // 4. Очищаем все поля ввода в интерфейсе
        ui->leditRedux->clear();
        ui->leditOrig->clear();
        ui->leditPapka->clear();
        ui->leditGunPuck->clear();
        ui->leditDLS->clear();
        ui->leditModZV->clear();
        ui->leditPapcaZV->clear();
        ui->leditBR_Pack->clear();
        ui->leditDLS_2->clear();

        // 5. Сбрасываем все чекбоксы автоматической установки
        ui->checkAutoLoad->setChecked(false);
        ui->checkAutoLoadGP->setChecked(false);
        ui->checkAutoLoadZV->setChecked(false);
        ui->checkAutoLoadBR->setChecked(false);

        // 6. Обнуляем внутренние переменные в оперативной памяти
        m_modSoundPath.clear();
        m_x64AudioSfxPath.clear();
        m_gunPackSourcePath.clear();
        m_dlcPacksTargetPath.clear();
        m_armorSourcePath.clear();
        m_armorTargetPath.clear();

        qInfo() << "Настройки программы и реестр были успешно очищены.";

        // 7. Уведомляем пользователя об успешном завершении
        QMessageBox::information(this, "Успех", "Все настройки и сохраненные пути успешно сброшены.");
    }
}
// --- СЛОТЫ ДЛЯ ЗАМЕНЕНОК (ZM) ---

// Автопоиск dlcpacks для замененок
void MainWindow::on_btnAutoSearchDLS_3_clicked() {
    QSettings rsReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Rockstar Games\\Grand Theft Auto V", QSettings::NativeFormat);
    QString basePath = rsReg.value("InstallFolder").toString();

    if (basePath.isEmpty()) {
        QSettings steamReg("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        QString steamPath = steamReg.value("InstallPath").toString();
        if (!steamPath.isEmpty()) {
            basePath = steamPath + "/steamapps/common/Grand Theft Auto V";
        }
    }

    QString targetPath;
    if (!basePath.isEmpty()) {
        targetPath = basePath + "/update/x64/dlcpacks";
        if (QDir(targetPath).exists()) {
            m_zmTargetPath = QDir::toNativeSeparators(targetPath);
            ui->leditDLS_3->setText(m_zmTargetPath);
            QSettings("MyCompany", "MyGameTool").setValue("ZmTargetPath", m_zmTargetPath);
            return;
        }
    }

    qCritical() << "[AutoSearchDLS3] Не удалось автоматически определить директорию dlcpacks для Замененок.";
    ui->leditDLS_3->setText("Ошибка"); // Выводим "Ошибка" в поле ввода

    QString manualPath = QFileDialog::getExistingDirectory(this, "Укажите папку ...\\update\\x64\\dlcpacks", "C:/");
    if (!manualPath.isEmpty()) {
        m_zmTargetPath = QDir::toNativeSeparators(manualPath);
        ui->leditDLS_3->setText(m_zmTargetPath);
        QSettings("MyCompany", "MyGameTool").setValue("ZmTargetPath", m_zmTargetPath);
    }
}

// Ручной выбор dlcpacks для замененок
void MainWindow::on_btnDLS_3_clicked() {
    QString path = QFileDialog::getExistingDirectory(this, "Выберите папку ...\\update\\x64\\dlcpacks", m_zmTargetPath);
    if (!path.isEmpty()) {
        m_zmTargetPath = QDir::toNativeSeparators(path);
        ui->leditDLS_3->setText(m_zmTargetPath);
        QSettings("MyCompany", "MyGameTool").setValue("ZmTargetPath", m_zmTargetPath);
    }
}

// Ручной выбор папки с замененками
void MainWindow::on_btnPapkaZM_clicked() {
    QString path = QFileDialog::getExistingDirectory(this, "Выберите папку с замененками", m_zmSourcePath);
    if (!path.isEmpty()) {
        m_zmSourcePath = QDir::toNativeSeparators(path);
        ui->leditZM_Pack->setText(m_zmSourcePath);
        QSettings("MyCompany", "MyGameTool").setValue("ZmSourcePath", m_zmSourcePath);
    }
}

// Ручная установка замененок
void MainWindow::on_btnReplaceZM_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualInstallZM(getCurrentConfig());
}

// Ручное восстановление оригиналов замененок
void MainWindow::on_btnReplaceOrigZM_clicked() {
    if (m_isOperationPending) return;
    m_isOperationPending = true;
    emit requestManualRestoreZM(getCurrentConfig());
}

// Распаковка архива с замененками (тип 4)
void MainWindow::on_btnArxivZM_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "Выберите архив с замененками", "", "Archives (*.rar *.zip *.7z)");
    if (!path.isEmpty()) {
        ui->leditZM_Pack->setText("В процессе.....");
        emit requestUnpack(4, path, "");
    }
}

// Галочка автозагрузки замененок + ограничение на 5 папок + трехсторонний контроль конфликтов
void MainWindow::on_checkAutoLoadZM_toggled(bool checked) {
    if (!checked) {
        QSettings("MyCompany", "MyGameTool").setValue("checkAutoLoadZM", false);
        return;
    }

    QString pathDLS = QDir::fromNativeSeparators(ui->leditDLS_3->text()).toLower();
    QString pathZM = QDir::fromNativeSeparators(ui->leditZM_Pack->text()).toLower();

    if (pathDLS.endsWith("/")) pathDLS.chop(1);
    if (pathZM.endsWith("/")) pathZM.chop(1);

    bool isDlsOk = pathDLS.endsWith("/dlcpacks") && QDir(pathDLS).exists();

    // Проверяем, что внутри выбранной папки есть папки с dlc.rpf
    QDir zmDir(pathZM);
    bool hasDlcFolders = false;
    QStringList subdirs = zmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(const QString &dirName, subdirs) {
        QDir sub(pathZM + "/" + dirName);
        if (sub.exists("dlc.rpf")) {
            hasDlcFolders = true;
            break;
        }
    }

    if (isDlsOk && hasDlcFolders) {
        // Проверка лимита в 5 папок для стабильности
        if (subdirs.size() > 5) {
            QMessageBox::warning(this, "Превышение лимита замененок",
                                 QString("В выбранной папке обнаружено %1 папок.\n"
                                         "Для стабильности работы игры рекомендуется устанавливать "
                                         "не более 4-5 папок одновременно. Пожалуйста, сократите их количество.")
                                     .arg(subdirs.size()));
        }

        // Проверка на конфликты с другими dlcpacks модулями
        QStringList conflicts;

        // С ганпаками
        if (ui->checkAutoLoadGP->isChecked()) {
            QDir gpDir(ui->leditGunPuck->text());
            QStringList gpDirs = gpDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            foreach(const QString &d, subdirs) {
                if (gpDirs.contains(d, Qt::CaseInsensitive)) conflicts << d + " (Ган-пак)";
            }
        }

        // С брониками
        if (ui->checkAutoLoadBR->isChecked()) {
            QDir brDir(ui->leditBR_Pack->text());
            QStringList brDirs = brDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            foreach(const QString &d, subdirs) {
                if (brDirs.contains(d, Qt::CaseInsensitive)) conflicts << d + " (Броники)";
            }
        }

        if (!conflicts.isEmpty()) {
            QMessageBox::warning(this, "Конфликт путей",
                                 "Обнаружены совпадающие папки установки в разных модах:\n" +
                                     conflicts.join("\n") +
                                     "\nОни перезапишут файлы друг друга при автозапуске!");
        }

        QSettings("MyCompany", "MyGameTool").setValue("checkAutoLoadZM", true);
    } else {
        ui->checkAutoLoadZM->blockSignals(true);
        ui->checkAutoLoadZM->setChecked(false);
        ui->checkAutoLoadZM->blockSignals(false);

        QStringList errors;
        if (!isDlsOk) errors << "- Путь DLS должен заканчиваться на 'dlcpacks'";
        if (!hasDlcFolders) errors << "- В выбранной папке должна быть хотя бы одна папка, содержащая 'dlc.rpf'";

        QMessageBox::critical(this, "Ошибка Замененок", "Проверьте условия:\n" + errors.join("\n"));
    }
}
void MainWindow::on_btnZM_install_clicked()
{
    ui->btnExitGP->setVisible(false);
    ui->btnExitGP->move(564, 35);
    animateWindowOpen(ui->oknoZM, ui->btnZM_install);
    ui->lineV->move(1 , 25);
    ui->lineV->setVisible(true);
}
// Плавный показ подсказки с тенью над нужной кнопкой
void MainWindow::showTooltip(QWidget *targetWidget, const QString &text, bool wrap) {
    infoPopup->setText(text);
    if (wrap) {
        infoPopup->setFixedWidth(250);
    } else {
        infoPopup->setMinimumWidth(0);
        infoPopup->setMaximumWidth(QWIDGETSIZE_MAX);
        infoPopup->adjustSize();
        infoPopup->setFixedWidth(infoPopup->sizeHint().width());
    }
    infoPopup->adjustSize();

    // Расчет позиции
    QPoint globalPos = targetWidget->mapToGlobal(QPoint(0, 0));
    int x = globalPos.x() + (targetWidget->width() / 2) - (infoPopup->width() / 2);
    int y = globalPos.y() - infoPopup->height() - 5;

    infoPopup->move(x, y);
    infoPopup->show();

    // Плавно зажигаем системную прозрачность самого окна подсказки (без QGraphicsOpacityEffect!)
    QPropertyAnimation *anim = new QPropertyAnimation(infoPopup, "windowOpacity");
    anim->setDuration(220);
    anim->setStartValue(infoPopup->windowOpacity());
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::hideTooltip() {
    QPropertyAnimation *anim = new QPropertyAnimation(infoPopup, "windowOpacity");
    anim->setDuration(180);
    anim->setStartValue(infoPopup->windowOpacity());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, infoPopup, &QWidget::hide);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
