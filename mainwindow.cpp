#include <QFileDialog>
#include <QMessageBox>
#include "mainwindow.h"
#include "stereo.h"
#include "ui_mainwindow.h"
#include <QMessageBox>

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
        default:
            break;
    }

        if( gOriginalHandler)
            gOriginalHandler(type, context, txt);
}

void MainWindow::setBtnImage(QPushButton *btn, const QImage &img)
{
    int btnMaxSize = std::max(btn->width(), btn->height());
    QSize btnSize(btnMaxSize, btnMaxSize);
    QPalette p = btn->palette();
    p.setBrush(btn->backgroundRole(), QBrush(img.scaled(btnSize)));
    btn->setPalette(p);
    btn->setFlat(true);
    btn->setAutoFillBackground(true);
    btn->update();

    //btn->setMinimumSize(btnSize);
    //btn->setMaximumSize(btnSize);
    if( btn->toolTip().isEmpty() )
        btn->setToolTip(btn->text());
    btn->setText("");
}

void MainWindow::process()
{
    ui->btnProcess->setText("Processing...");
    QCoreApplication::processEvents();

    if( ui->checkOpenCV->isChecked() ) {
        uint maxDisp = ui->spinMaxDisparity->value();
        if( !Stereo::isPowerOfTwo(maxDisp) || !(maxDisp >= 16) ) {
            critical("In OpenCV Max Disparity or Disparity Count must be dividable by 16" );
            ui->spinMaxDisparity->setFocus();
            return;
        }
    }


    mStereo->process(ui->checkOpenCV->isChecked(),
                     ui->doubleFocalLenght->value(),
                     ui->doubleSensorSize->value(),
                     ui->doubleEyeDistance->value(),
                     ui->spinMaxDisparity->value());
}

void MainWindow::setProgressSteps(uint steps)
{
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(steps);
    ui->progressBar->setValue(0);
}

void MainWindow::stepProgress()
{
    ui->progressBar->setValue(ui->progressBar->value() + 1);
    QCoreApplication::processEvents();
}

void MainWindow::aborted()
{
    ui->btnProcess->setText("Aborted");
}

void MainWindow::finished()
{
    ui->progressBar->setValue(ui->progressBar->maximum());

    setBtnImage(ui->btnDisparity, mStereo->disparityImage());
    setBtnImage(ui->btnDepth, mStereo->depthImage());

    ui->btnProcess->setText("Ready!");
}

void MainWindow::critical(const QString &msg, QString title)
{
    QMessageBox::critical(this, title, msg );
}

bool MainWindow::loadImages(const QString &imagePath, bool isSwap)
{
    if( !mStereo->loadImages(imagePath, isSwap)) {
        critical(mLastError, "Error opening image" );
        return false;
    }

    MainWindow::setBtnImage(ui->btnLeft, mStereo->leftImage());
    MainWindow::setBtnImage(ui->btnRight, mStereo->rightImage());
    MainWindow::setBtnImage(ui->btnAnaglypth, mStereo->anaglyphImage());
    return true;
}

void MainWindow::loadImagesDialog()
{
    QString filter = "Images (*.png *.jpg);;All Files (*)";
    QString selected = QFileDialog::getOpenFileName( this,
                                                    "Select a double (left and right) image",
                                                    "../../res", filter);
    if( selected.isEmpty() ) {
        return;
    }

    if( mStereo ) {
        delete mStereo;
        mStereo = nullptr;
    }

    mStereo = new Stereo;

    if( loadImages(selected, ui->checkSwap->isChecked()) ) {
        mLastFile = selected;
    }
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
    loadImagesDialog();
}


void MainWindow::on_btnRight_clicked()
{
    loadImagesDialog();
}


void MainWindow::on_btnProcess_clicked()
{
    if( !mStereo || !mStereo->hasImages()) {
        critical("First press upper left or right image slots to load left & right images...", "No ready!");
        return;
    }

    if( mStereo->isAborting()) {
        QMessageBox::warning(this, "Aborting...", "Wait to finish aboring");
        return;
    }

    if( !mStereo->isProcessing()) {
        process();
    }else {
        mStereo->abort();
        ui->btnProcess->setText("Aborting...");
        QCoreApplication::processEvents();
    }
}



void MainWindow::resizeEvent(QResizeEvent *event)
{
    if( !mStereo) {
        return;
    }

    auto applyImage = [&](QPushButton* btn, const QImage& img) {
        if( img.isNull() ) {
            return;
        }

        setBtnImage(btn, img);
    };

    applyImage(ui->btnLeft, mStereo->leftImage());
    applyImage(ui->btnRight, mStereo->rightImage());
    applyImage(ui->btnAnaglypth, mStereo->anaglyphImage());
    applyImage(ui->btnDisparity, mStereo->disparityImage());
    applyImage(ui->btnDepth, mStereo->depthImage());
}


void MainWindow::on_checkSwap_toggled(bool checked)
{
    if( !mLastFile.isEmpty() ) {
        loadImages(mLastFile, ui->checkSwap->isChecked());
    }
}

