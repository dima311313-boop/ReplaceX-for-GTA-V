#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "modernbutton.h"
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
          /_/                        v0.9.5
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

    // пульсация
    QGraphicsOpacityEffect *onlineEff = new QGraphicsOpacityEffect(ui->lblOnline);
    ui->lblOnline->setGraphicsEffect(onlineEff);

    QPropertyAnimation *pulse = new QPropertyAnimation(onlineEff, "opacity");
    pulse->setDuration(2000);     // Сделаем чуть медленнее (2 секунды)
    pulse->setStartValue(1.0);
    pulse->setEndValue(0.75);     // Затухание всего на 25% (будет очень мягко)
    pulse->setEasingCurve(QEasingCurve::InOutQuad); // Более плавная кривая
    pulse->setLoopCount(-1);
    pulse->start();

    //получение пути
    connect(ui->leditPapka, &QLineEdit::textChanged, [this](const QString &text) {
        QSettings settings("MyCompany", "MyGameTool");
        settings.setValue("Paths/GameFolder", text);
    });    

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
    ui->btnArxivRedux->installEventFilter(this);
    ui->btnArxivGuns->installEventFilter(this);
    ui->btnArxivSounds->installEventFilter(this);
    //end knopocki


    this->setFixedSize(646, 374);

    qInfo() << "Программа запущенна.";
    ui->miniProgress->setText("Ожидание запуска игры");
    this->setWindowIcon(QIcon(":/izobr/IconG.ico"));
    this->setWindowTitle("ReplaceX");



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

    bool isKnopkiEnabled = settings.value("Settings/KnopkiEnable", false).toBool();
    ui->checkKnopki->setChecked(isKnopkiEnabled);


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
    qInfo() << "Пути к звукам загруженны:" << m_x64AudioSfxPath << m_modSoundPath;
    qInfo() << "Пути к update.rpf загруженны:" << "Редукс: " << settings.value("Paths/ReduxFile", "").toString() << "Оригинальный: " << settings.value("Paths/OriginalFile", "").toString() << "Папка: " << savedPath;
    qInfo() << "Пути к ган пакам загруженны: " << "Ган паки: " << m_gunPackSourcePath << "Папка: " << m_dlcPacksTargetPath;
    qDebug() << razdel;

    processTimer = new QTimer(this);
    connect(processTimer, &QTimer::timeout, this, &MainWindow::checkProcessLoop);
    processTimer->start(500);


    onlineTimer = new QTimer(this);
    connect(onlineTimer, &QTimer::timeout, this, &MainWindow::updateOnlineStatus);
    onlineTimer->start(45000); //(45 секунд)

    // Сразу делаем первый запрос
    updateOnlineStatus();

    // В конструктор MainWindow::MainWindow
    m_soundSuccess = new QSoundEffect(this);
    // Используем qrc:/ для надежности
    m_soundSuccess->setSource(QUrl("qrc:/sounds/sounds/Yspeh.wav"));
    m_soundSuccess->setVolume(0.5);

    // Добавим отладку статуса
    connect(m_soundSuccess, &QSoundEffect::loadedChanged, this, [this]() {
        if (m_soundSuccess->isLoaded()) {
            qDebug() << "Звук успеха загружен и готов!";
        }
    });

    m_soundError = new QSoundEffect(this);
    m_soundError->setSource(QUrl("qrc:/sounds/sounds/Error.wav"));
    m_soundError->setVolume(0.6);

    bool state = ui->checkKnopki->isChecked();

    if (state) {
        ui->btnReplaceRedux->setImagePath(":/izobr/Redux01-removebg-preview.png");

        ui->btnReplaceOrig->setImagePath(":/izobr/Orig01.png");
    }

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
void MainWindow::applyModernShadow(QWidget* widget) {
    if (!widget) return;

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(30);      // Насколько мягкая тень
    shadow->setXOffset(0);         // Смещение по горизонтали
    shadow->setYOffset(0);         // Смещение по вертикали (0 для эффекта свечения)
    shadow->setColor(QColor(0, 0, 0, 200)); // Черная тень с прозрачностью
    widget->setGraphicsEffect(shadow);
}

void MainWindow::animateWindowOpen(QWidget* target, QWidget* sourceBtn) {
    // 1. Список всех «всплывающих» окон
    QList<QWidget*> subWindows = {ui->oknoPresets, ui->oknoHDD, ui->oknoSettings,
                                   ui->oknoZV, ui->oknoGP, ui->oknoDiscleamer};

    // Скрываем все окна из списка, кроме того, которое открываем
    for(QWidget* w : subWindows) {
        if(w != target) w->hide();
    }

    // 2. ЛОГИКА ДЛЯ oknoKnopohki: скрываем только если идем в Настройки
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

// Твоя текущая версия программы
const QString CURRENT_VERSION = "0.9.6";

void MainWindow::onResult(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Ошибка сети:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    // Читаем данные от сервера
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject mainObj = doc.object();

    // --- 1. ОНЛАЙН СТАТУС ---
    // Сервер присылает его в поле "online_count"
    if (mainObj.contains("online_count")) {
        int online = mainObj["online_count"].toInt();
        ui->lblOnline->setText(QString("🟢 Онлайн: %1").arg(online));
    }

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Сетевая ошибка:" << reply->errorString();
        reply->deleteLater();
        return;
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
    QString downloadUrl = updateObj["url"].toString();
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
void MainWindow::updateOnlineStatus() {
    // 1. Формируем строку
    QString urlString = QString("https://replacex-server.onrender.com/api/stats?v=%1").arg(CURRENT_VERSION);

    // 2. Используем фигурные скобки {}, чтобы компилятор не путался
    QNetworkRequest request{QUrl(urlString)};

    // Теперь manager->get увидит объект request, а не функцию
    manager->get(request);
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
    // Очень важно: берем текст ПРЯМО из QLineEdit
    cfg.reduxPath = ui->leditRedux->text();
    cfg.originalPath = ui->leditOrig->text();
    cfg.gameUpdatePath = ui->leditPapka->text();
    cfg.gunPackSource = ui->leditGunPuck->text();
    cfg.dlcPacksTarget = ui->leditDLS->text();
    cfg.soundModPath = ui->leditModZV->text();
    cfg.sfxPath = ui->leditPapcaZV->text();

    // Папки бэкапов (лучше делать абсолютными)
    cfg.backupPath = QCoreApplication::applicationDirPath() + "/backups_gta";
    cfg.soundBackupPath = QCoreApplication::applicationDirPath() + "/sound_backup";

    // Галочки
    cfg.useRedux = ui->checkAutoLoad->isChecked();
    cfg.useGunPack = ui->checkAutoLoadGP->isChecked();
    cfg.useSounds = ui->checkAutoLoadZV->isChecked();

    return cfg;
}




void MainWindow::checkProcessLoop() {
    if (m_isOperationPending) return;

    int serverIndex = ui->cmbServer->currentIndex();

    // Проверка специфичных процессов
    bool isGtaRunning = isProcessRunning("GTA5.exe");
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
    if (obj == ui->btnArxivRedux) {
        if (event->type() == QEvent::Enter) {
            infoPopup->setFixedWidth(250);
            infoPopup->setWordWrap(true);
            infoPopup->adjustSize();
            infoPopup->setText("Авто распаковка вашего архива с редуксом - программа сама найдет нужные файлы, извлечет и укажет пути к ним. Процесс обнаружения и извлечения занимает от 5-10 секунд. (На данный момент из поддержуемых запароленных архивов - только архивы от Majestic-mods.ru)");
            infoPopup->adjustSize();

            QPoint globalPos = ui->btnArxivRedux->mapToGlobal(QPoint(0, 0));
            int x = globalPos.x() + (ui->btnArxivRedux->width() / 2) - (infoPopup->width() / 2);
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
    if (obj == ui->btnArxivGuns) {
        if (event->type() == QEvent::Enter) {
            infoPopup->setFixedWidth(250);
            infoPopup->setWordWrap(true);
            infoPopup->adjustSize();
            infoPopup->setText("Авто распаковка вашего архива с ган паком - программа сама найдет нужные файлы, извлечет и укажет пути к ним. Процесс обнаружения и извлечения занимает от 5-10 секунд. (На данный момент из поддержуемых запароленных архивов - только архивы от Majestic-mods.ru)");
            infoPopup->adjustSize();

            QPoint globalPos = ui->btnArxivGuns->mapToGlobal(QPoint(0, 0));
            int x = globalPos.x() + (ui->btnArxivGuns->width() / 2) - (infoPopup->width() / 2);
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
    if (obj == ui->btnArxivSounds) {
        if (event->type() == QEvent::Enter) {
            infoPopup->setFixedWidth(250);
            infoPopup->setWordWrap(true);
            infoPopup->adjustSize();
            infoPopup->setText("Авто распаковка вашего архива с модифицированными звуками - программа сама найдет нужные файлы, извлечет и укажет пути к ним. Процесс обнаружения и извлечения занимает от 5-10секунд. (На данный момент из поддержуемых запароленных архивов - только архивы от Majestic-mods.ru)");
            infoPopup->adjustSize();

            QPoint globalPos = ui->btnArxivSounds->mapToGlobal(QPoint(0, 0));
            int x = globalPos.x() + (ui->btnArxivSounds->width() / 2) - (infoPopup->width() / 2);
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
void MainWindow::on_btnOknoDop_clicked() {
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
                                ui->oknoDiscleamer, ui->oknoZV, ui->oknoHDD};

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
    animateWindowOpen(ui->oknoGP, ui->btnGanpacOpen);
    ui->lineV->move(1 , 25);
    ui->lineV->setVisible(true);
}
void MainWindow::on_btnZVOpen_clicked() {
    ui->btnExitGP->setVisible(false);
    animateWindowOpen(ui->oknoZV, ui->btnZVOpen);
    ui->lineV->move(1 , 85);
    ui->lineV->setVisible(true);
}

//установка звуков оружия

void MainWindow::on_btnAutoSearchZV_clicked() {
    qInfo() << "=== Автопоиск x64\\audio\\sfx ===";


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


            qInfo() << "Путь до x64\\audio\\sfx найден:" << m_x64AudioSfxPath;
            /*
            QMessageBox::information(this, "Найдено", "Путь до x64\\audio\\sfx определён автоматически.");
            */
            return;
        }
    }

    qInfo() << "Автопоиск не удался.";
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
    animateWindowOpen(ui->oknoHDD, ui->btnHDD_OpenDis);
    ui->lineV->move(1 , 145);
    ui->lineV->setVisible(true);
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
    QDesktopServices::openUrl(QUrl("https://majestic-mods.ru/load/soft/replacex_0_9_0_beta/14-1-0-192"));
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
void MainWindow::on_checkKnopki_toggled(bool checked)
{
    QSettings settings("MyCompany", "MyGameTool");

    settings.setValue("Settings/KnopkiEnable", checked);

    settings.sync();
    if(checked)
    {
        ui->btnReplaceRedux->setImagePath(":/izobr/Redux01-removebg-preview.png");
        ui->btnReplaceOrig->setImagePath(":/izobr/Orig01.png");
    }
    else
    {
        ui->btnReplaceRedux->setImagePath("0");
        ui->btnReplaceOrig->setImagePath("0");
    }
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

    cbRedux->setChecked(true);
    layout->addWidget(cbRedux);
    layout->addWidget(cbGP);
    layout->addWidget(cbZV);

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
        // Если ошибка — пишем её в нужное поле и играем звук ошибки
        if (type == 0) ui->leditRedux->setText("Ошибка");
        else if (type == 1) ui->leditGunPuck->setText("Ошибка");
        else if (type == 2) ui->leditModZV->setText("Ошибка");

        m_soundError->play();
        return;
    }

    // Если всё ок — устанавливаем путь
    if (type == 0) {
        ui->leditRedux->setText(resultPath);
    }
    else if (type == 1) {
        ui->leditGunPuck->setText(resultPath);
    }
    else if (type == 2) {
        ui->leditModZV->setText(resultPath);
        // Синхронизируем внутреннюю переменную для звуков, если она используется
        m_modSoundPath = resultPath;
    }

    // ГЛАВНОЕ: Сохраняем в реестр/файл настроек
    saveSettings();

    m_soundSuccess->play();
    ui->miniProgress->setText("Пути обновлены и сохранены");
}

void MainWindow::on_btnAutoCopyUpdate_clicked()
{
    QSettings settings("MyCompany", "MyGameTool");
    QString rpfPath = findUpdateRpf();
    if (!rpfPath.isEmpty()) {
        if (copyUpdateRpfToAppDir(rpfPath)) {

            settings.setValue("FirstRun", true);
            settings.setValue("Paths/OriginalFile", ui->leditOrig->text());
        }
    }
}
