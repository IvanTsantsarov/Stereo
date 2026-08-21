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

    bool loadImages(const QString& imagePath, bool isSwap);
    void loadImagesDialog();
    Stereo* mStereo = nullptr;
    QString mLastError, mLastInfo;
    QString mLastFile;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void onInfo(const QString& txt);
    void onError(const QString& txt);
    static void setBtnImage(QPushButton* btn, const QImage& img);

    void process();
    void setProgressSteps(uint steps);
    void stepProgress();
    void aborted();
    void finished();

    void critical(const QString& msg, QString title = "Error");

private slots:
    void on_btnLeft_clicked();

    void on_btnRight_clicked();

    void on_btnProcess_clicked();


    void on_checkSwap_toggled(bool checked);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::MainWindow *ui;
};

extern MainWindow* gMainWindow;

#endif // MAINWINDOW_H
