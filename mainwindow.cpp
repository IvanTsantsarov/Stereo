#include <QFileDialog>
#include <QMessageBox>
#include "mainwindow.h"
#include "stereo.h"
#include "ui_mainwindow.h"

MainWindow* gMainWindow = nullptr;
QtMessageHandler gOriginalHandler = nullptr;

#define BTN_SIDE 256
#define BTN_SIZE QSize(BTN_SIDE, BTN_SIDE)

void myMessageHandler(QtMsgType type, const QMessageLogContext & context, const QString &txt)
{
    if( !gMainWindow) {
        return;
    }

    switch(type) {
        case QtMsgType::QtFatalMsg: gMainWindow->onError(txt);; gMainWindow->close(); break;
        case QtMsgType::QtCriticalMsg: gMainWindow->onError(txt); break;
        case QtMsgType::QtInfoMsg: gMainWindow->onError(txt); break;
    }

        if( gOriginalHandler)
            gOriginalHandler(type, context, txt);
}

void MainWindow::setBtnImage(QPushButton *btn, const QImage &img)
{
    QPalette p = btn->palette();
    p.setBrush(btn->backgroundRole(), QBrush(img.scaled(BTN_SIDE, BTN_SIDE)));
    btn->setPalette(p);
    btn->setFlat(true);
    btn->setAutoFillBackground(true);
    btn->update();

    btn->setMinimumSize(BTN_SIZE);
    btn->setMaximumSize(BTN_SIZE);
    if( btn->toolTip().isEmpty() )
        btn->setToolTip(btn->text());
    btn->setText("");
}

void MainWindow::loadImages()
{
    if( mStereo ) {
        delete mStereo;
        mStereo = nullptr;
    }

    QString selected = QFileDialog::getOpenFileName( this, "Select a double (left and right) image", "../res", "*.png;*.jpeg");

    if( selected.isEmpty() ) {
        return;
    }

    mStereo = new Stereo;

    if( !mStereo->loadImages(selected)) {
        QMessageBox::critical(this, "Error opening image", mLastError );
        return;
    }

    MainWindow::setBtnImage(ui->btnLeft, mStereo->leftImage());
    MainWindow::setBtnImage(ui->btnRight, mStereo->rightImage());
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    gMainWindow = this;
    gOriginalHandler = qInstallMessageHandler(myMessageHandler);
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    if( mStereo ) {
        delete mStereo;
        mStereo = nullptr;
    }

    delete ui;
}

void MainWindow::onInfo(const QString &txt)
{
    mLastInfo = txt;
}

void MainWindow::onError(const QString &txt)
{
    mLastError = txt;
}


void MainWindow::on_btnLeft_clicked()
{
    loadImages();
}


void MainWindow::on_btnRight_clicked()
{
    loadImages();
}

