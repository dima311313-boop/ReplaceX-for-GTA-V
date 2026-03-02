#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QSettings>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEvent>
#include <QProcess>
#include <QThread>
#include <QNetworkAccessManager>
#include <QPainter>
#include "fileworker.h"
#include <QSoundEffect>



QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    // Поток и рабочий
    QThread *m_workerThread;
    FileWorker *m_worker;

    QSoundEffect *m_soundSuccess;
    QSoundEffect *m_soundError;


    QTimer *onlineTimer;      // Таймер для авто-обновления
    void updateOnlineStatus(); // Функция запроса к серверу

    // Переменные состояния
    bool m_isOperationPending = false; // Чтобы не запускать копирование дважды

    // Утилиты
    FileWorker::Config getCurrentConfig();

    void setupSmoothMarquee(QString text);
    bool m_gameStarted = false;

    QNetworkAccessManager *manager;
    const int MAX_RESTORE_ATTEMPTS = 5;      //Максимальное число попыток
    const int BASE_DELAY_MS = 2000;       //Базовая задержка между попытками (2 сек)
    const int INCREASE_DELAY_MS = 1000;  //Увеличение задержки на каждую попытку

    QLabel *marqueeLabel;
    // === UI-элементы ===
    QLabel *infoPopup;                  //Всплывающее окошко с подсказкой
    QGraphicsOpacityEffect *popupOpacity; //Эффект прозрачности для анимации

    // === Элементы системного трея ===
    QSystemTrayIcon *trayIcon;         //Иконка в системном трее
    QMenu *trayMenu;                   //Контекстное меню трея

    QTimer *processTimer;              //Таймер для мониторинга процессов игры
    QProcess *batchProcess = nullptr;
    QTimer *checkTimer = nullptr;

    // === Пути к файлам и папкам ===
    QString configPathForUserFile;      //Путь к пользовательскому конфиг-файлу
    QString userFilePathConfig;          //Путь к конфигу пользователя
    QString fullFilePath;               //Полный путь к файлу (общее назначение)
    QString m_x64AudioSfxPath;        //Путь к папке x64/audio/sfx
    QString m_modSoundPath;             //Путь к мод-звукам
    QString m_soundBackupDir;          //Папка для бэкапов звуков
    QString m_gunPackSourcePath;       //Источник ган‑паков
    QString m_dlcPacksTargetPath;     //Целевая папка dlcpacks
    QString m_backupPath;              //Папка для общих бэкапов (рядом с .exe)
    QString m_originalDlcPacksPath;   //Оригинал dlcpacks (для восстановления)
    QString m_backupDlcPacksPath;     //Бэкап dlcpacks
    QString autoFindUpdateFolder();     //Метод автопоиска папки update
    QString findUpdateRpf();            //Метод поиска update.rpf
    QString findGTAPath();              //Метод поиска корневой папки GTA V
    QString batPath;


    int i = 0;
    bool OneOt = true;

    // Вспомогательные функции
    bool smartReplace(const QString &source, const QString &targetDir, const QString &targetFileName);
    void killGtaEcosystem();
    bool isProcessRunning(const QString &exeName);
    void restoreAllBackups();
    bool m_wasGameRunning = false;

    void checkUpdates();

    // === Флаги состояния ===
    bool isSoundsInstalled = false;    //Установлены ли звуки
    bool isGunPackInstalled = false;   //Установлены ли ган‑паки
    bool isReduxInstalled = false;      //Установлен ли редукс
    int saveAuto;                      //Флаг автосохранения
    //Вспомогательные функции
    QMap<QString, QString> m_backupMap; //Бэкапы
    //работа с файлами
    bool copyFileToGame(QString sourcePath, QString destFolder); // Копирование файла в игру
    bool copyDirectory(const QString &sourceDir, const QString &targetDir); // Копирование директории
    bool copyRpfFiles(const QString &sourceDir, const QString &targetDir); // Копирование RPF-файлов
    bool backupOriginalRpfFiles(const QStringList &modFiles);   // Бэкап оригинальных RPF
    bool installSounds();               // Установка звуков
    bool restoreSounds();              // Восстановление звуков
    bool installGunPacks();           // Установка ган‑паков
    bool restoreGunPacks();            // Восстановление ган‑паков
    bool removeWithRetry(const QString &path, int maxAttempts = 3); // Удаление с повторами
    bool copyUpdateRpfToAppDir(const QString &sourcePath); // Копирование update.rpf в папку приложения
    bool isValidUpdateRpfPath(const QString &path);
    bool isValidRpfPath(const QString &filePath);

    bool isFileBusy(QString filePath);   // Проверка занятости файла
    void killProcessByName(QString name); // Завершение процесса по имени



    //Управление настройками
    void saveSettings();                // Сохранение текущих путей в настройки
    void loadSettings();                 // Загрузка настроек
    void checkGtaProcess();          // Функция для таймера автозапуска
    void saveTimeToFile();
    bool isBatchRunning = false;
    bool gtaWasRunning = false;
    bool Y = false;
    bool N = false;
    void runBatch();
    bool oknoDop = true;
    bool safeCopy(const QString &src, const QString &destFolder, bool isRestoring);
    void blurEf(bool enable);
signals:
    // Сигналы для управления рабочим потоком
    void requestInstall(FileWorker::Config config);
    void requestRestore(FileWorker::Config config);

    // Сигналы для РУЧНОГО режима (выполняются в FileWorker)
    void requestManualSmartReplace(QString source, QString targetDir, QString targetFileName);
    void requestManualRestoreGunPacks(FileWorker::Config config);
    void requestManualInstallGunPacks(FileWorker::Config config);
    void requestManualReplaceSounds(FileWorker::Config config);
    void requestManualRestoreSounds(FileWorker::Config config);

protected:
    //События Qt
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;


private slots:
    // Слоты для получения ответов от рабочего
    void onWorkerFinished(bool success, QString details);
    void onWorkerStatus(QString status);
    void onWorkerProgress(QString msg);


    //Кнопочки
    void on_btnAddRedux_clicked();      // Кнопка: выбрать редукс
    void on_btnAddOrig_clicked();       // Кнопка: выбрать оригинал
    void on_btnDonat_clicked();         // Кнопка: переход на страницу донатов
    void on_btnReplaceOrig_clicked();   // Кнопка: заменить оригинал
    void on_btnReplaceRedux_clicked(); // Кнопка: заменить редукс
    void on_btnPapka_clicked();         // Кнопка: выбрать папку GTA
    void on_btnAutoSearch_clicked();    // Кнопка: автопоиск папки
    void on_btnTelegram_clicked();     // Кнопка: открыть Telegram
    void on_btnOknoDop_clicked();      // Кнопка: открыть доп. окно
    void on_btnExitGP_clicked();       // Кнопка: выход из режима GP
    void on_btnPapkaGP_clicked();      // Кнопка: выбрать папку GP
    void on_btnAutoSearchDLS_clicked();// Кнопка: автопоиск DLS
    void on_btnDLS_clicked();          // Кнопка: выбрать DLS
    void on_btnReplaceGunPuck_clicked();// Кнопка: заменить ганпак
    void on_btnReplaceOrigGP_clicked();// Кнопка: восстановить оригинал GP
    void on_btnGanpacOpen_clicked();  // Кнопка: открыть ганпак
    void on_btnZVOpen_clicked();      // Кнопка: открыть звуки
    void on_btnPapkaZV_clicked();     // Кнопка: выбрать папку звуков
    void on_btnAutoSearchZV_clicked();// Кнопка: автопоиск звуков
    void on_btnZVMod_clicked();       // Кнопка: выбрать мод звуков
    void on_btnReplaceModZV_clicked();// Кнопка: заменить мод звуков
    void on_btnReplaceOrigZV_clicked();// Кнопка: восстановить оригинал звуков
    void on_btnHDD_OpenDis_clicked(); //Кнопка: открыть окошко с установки HDD
    void on_btnNext_clicked(); //Копка: продолжить
    void on_btnSaveTime_clicked(); //Кнопка: Сохранить время задержки
    void on_btnOn_Off_HDD_clicked(); //Кнопка: вкллючения задержки запуска
    void on_btnNotification_clicked();
    void on_btnDownload_clicked();
    void on_btnExitNF_clicked();
    void on_btnAltV_clicked();
    void on_btnRage_clicked();
    void on_btnSettings_clicked();
    void on_btnOpenLogs_clicked();
    void on_btnMajesticMods_clicked();
    void on_cmbServer_currentIndexChanged(int index);
    void on_checkSound_clicked();
    void on_btnGaid_clicked();
    //процессы
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void readProcessOutput();
    void onResult(QNetworkReply *reply);


    //Чекбоксы
    void on_checkKnopki_toggled(bool checked);
    void on_checkSound_toggled(bool checked);
    void on_checkAutoLoad_toggled(bool checked);      // Чекбокс: автозагрузка редукса
    void on_checkAutoLoadGP_toggled(bool checked);    // Чекбокс: автозагрузка GP
    void on_checkAutoLoadZV_toggled(bool checked);   // Чекбокс: автозагрузка звуков
    void on_checkAutoOn_Off_toggled(bool checked);  //Чекбокс: Авто вколючение задержки запуска игры
    void on_leditTimeVvod_editingFinished(); //лейбл задержки запуска


    //Святыня
    void checkProcessLoop();// Основной цикл мониторинга процессов
};

#endif // MAINWINDOW_H
