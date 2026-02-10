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
#include <filecopier.h>
#include <QProcess>

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

    const int MAX_RESTORE_ATTEMPTS = 5;      //Максимальное число попыток
    const int BASE_DELAY_MS = 2000;       //Базовая задержка между попытками (2 сек)
    const int INCREASE_DELAY_MS = 1000;  //Увеличение задержки на каждую попытку


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




    // Вспомогательные функции
    bool smartReplace(const QString &source, const QString &targetDir, const QString &targetFileName);
    void killGtaEcosystem();
    bool isProcessRunning(const QString &exeName);
    void restoreAllBackups();
    bool m_wasGameRunning = false;



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
    void runBatch();
    bool oknoDop = true;
    bool safeCopy(const QString &src, const QString &destFolder, bool isRestoring);

protected:
    //События Qt
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;


private slots:
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
    //процессы
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void readProcessOutput();


    //Чекбоксы
    void on_checkAutoLoad_toggled(bool checked);      // Чекбокс: автозагрузка редукса
    void on_checkAutoLoadGP_toggled(bool checked);    // Чекбокс: автозагрузка GP
    void on_checkAutoLoadZV_toggled(bool checked);   // Чекбокс: автозагрузка звуков
    void on_checkAutoOn_Off_toggled(bool checked);  //Чекбокс: Авто вколючение задержки запуска игры
    void on_leditTimeVvod_editingFinished(); //лейбл задержки запуска


    //Святыня
    void checkProcessLoop();// Основной цикл мониторинга процессов
};

#endif // MAINWINDOW_H
