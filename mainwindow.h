#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Stereo;
class QPushButton;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

    void loadImages();
    Stereo* mStereo = nullptr;
    QString mLastError, mLastInfo;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void onInfo(const QString& txt);
    void onError(const QString& txt);
    static void setBtnImage(QPushButton* btn, const QImage& img);

private slots:
    void on_btnLeft_clicked();

    void on_btnRight_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
