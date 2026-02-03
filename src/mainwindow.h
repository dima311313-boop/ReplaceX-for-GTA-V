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

    // === UI-элементы ===
    QLabel *infoPopup;                  // Всплывающее окошко с подсказкой
    QGraphicsOpacityEffect *popupOpacity; // Эффект прозрачности для анимации

    // === Элементы системного трея ===
    QSystemTrayIcon *trayIcon;         // Иконка в системном трее
    QMenu *trayMenu;                   // Контекстное меню трея

    QTimer *processTimer;              // Таймер для мониторинга процессов игры
    QTimer *autoInstallTimer;          // Таймер для автоустановки (не используется?)

    // === Пути к файлам и папкам ===
    QString configPathForUserFile;      // Путь к пользовательскому конфиг-файлу
    QString userFilePathConfig;          // Путь к конфигу пользователя
    QString fullFilePath;               // Полный путь к файлу (общее назначение)
    QString m_x64AudioSfxPath;        // Путь к папке x64/audio/sfx
    QString m_modSoundPath;             // Путь к мод-звукам
    QString m_soundBackupDir;          // Папка для бэкапов звуков
    QString m_gunPackSourcePath;       // Источник ган‑паков
    QString m_dlcPacksTargetPath;     // Целевая папка dlcpacks
    QString m_backupPath;              // Папка для общих бэкапов (рядом с .exe)
    QString m_originalDlcPacksPath;   // Оригинал dlcpacks (для восстановления)
    QString m_backupDlcPacksPath;     // Бэкап dlcpacks
    QString autoFindUpdateFolder();     // Метод автопоиска папки update
    QString findUpdateRpf();            // Метод поиска update.rpf
    QString findGTAPath();              // Метод поиска корневой папки GTA V

    // === Флаги состояния ===
    bool isSoundsInstalled = false;    // Установлены ли звуки
    bool isGunPackInstalled = false;   // Установлены ли ган‑паки
    bool isReduxInstalled = false;      // Установлен ли редукс
    int saveAuto;                      // Флаг автосохранения (неясно назначение)

    // === Вспомогательные структуры ===
    QMap<QString, QString> m_backupMap; // Карта бэкапов: целевой путь → путь к бэкапу

    // === Основные методы работы с файлами ===
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

    bool isFileBusy(QString filePath);   // Проверка занятости файла
    void killProcessByName(QString name); // Завершение процесса по имени


    // === Методы управления настройками ===
    void saveSettings();                // Сохранение текущих путей в настройки
    void loadSettings();                 // Загрузка настроек

protected:
    // Переопределённые события Qt
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;


private slots:
    // === Обработчики кнопок ===
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

    // === Обработчики чекбоксов ===
    void on_checkAutoLoad_toggled(bool checked);      // Чекбокс: автозагрузка редукса
    void on_checkAutoLoadGP_toggled(bool checked);    // Чекбокс: автозагрузка GP
    void on_checkAutoLoadZV_toggled(bool checked);   // Чекбокс: автозагрузка звуков


    // === Системные слоты ===
    void checkProcessLoop();            // Основной цикл мониторинга процессов
};

#endif // MAINWINDOW_H
