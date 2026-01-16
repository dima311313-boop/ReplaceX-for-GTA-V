#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QSettings>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    void changeEvent(QEvent *event) override;

private slots:
    void checkProcessLoop();
    void on_btnAddRedux_clicked();
    void on_btnAddOrig_clicked();
    void on_btnDonat_clicked();
    void on_btnReplaceOrig_clicked();
    void on_btnReplaceRedux_clicked();
    void on_checkAutoLoad_toggled(bool checked);
    void on_btnPapka_clicked();
    void on_btnAutoSearch_clicked();
    void on_btnTelegram_clicked();

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private:
    Ui::MainWindow *ui;
    QString configPathForUserFile;
    QString userFilePathConfig;
    QString fullFilePath;
    int saveAuto;
    QTimer *autoInstallTimer;
    bool isFileBusy(QString filePath);


    bool isReduxInstalled = false;
    QTimer *processTimer;
    void killProcessByName(QString name);
    QString findGTAPath();
    bool copyFileToGame(QString sourcePath, QString destFolder);

};
#endif // MAINWINDOW_H
