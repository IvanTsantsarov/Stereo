#include "stereo.h"
#include "mainwindow.h"
#include <QDebug>
#include <algorithm>
#include <vector>


// /        STENCIL_SIZE       \
//(--------------X--------------)
// \STENCIL_SIDE/|\STENCIL_SIDE/
//               |
//         current value
#define STENCIL_SIDE 6
#define STENCIL_SIZE (STENCIL_SIDE*2 + 1)

#define DISPARITY_PART 0.1f

#define MIN_DIFF 0.0001f
#define INVALID_DIST 10e6
#define MAX_DIST_CMP 10e5

#define ERR qCritical() << __FILE__ << __FUNCTION__ << ":"
#define INF qInfo() << __FILE__ << __FUNCTION__ << ":"
#define RGB2VAL(__rgb__) ( __rgb__.red()*0.2989f +  __rgb__.green()*0.5870f + __rgb__.blue()*0.1140f )
#define RGBA2VAL(__rgba__) (( __rgba__.red()*0.2989f +  __rgba__.green()*0.5870f + __rgba__.blue()*0.1140f) * __rgba__.alpha() * 1.0f/255.0f)
#define RGB2VAL_NORM(__rgb__)(RGB2VAL(__rgb__) * 1.0f/255.0f)
#define RGBA2VAL_NORM(__rgba__)(RGBA2VAL(__rgba__) * 1.0f/255.0f)

#define MAX_FLOAT 1e8f

#define COLOR_BACK qRgb(255, 0, 255)

bool Stereo::isPowerOfTwo(uint value)
{
    uint bitCount = 0;
    uint valueTemp = value;

    while(valueTemp) {
        valueTemp >>= 1;
        bitCount++;
    }

    if( !bitCount ) {
        return false;
    }

    uint mask = 1 << (bitCount-1);
    return value & mask;
}


Stereo::Stereo() {

    mDepthK = 50.0f;//focalLengthPixels * baseline
}

// Loading double (left and right eye) images from a single image
bool Stereo::loadImages(const QString &path)
{
    INF << "Loading double (left & right) images. Single image must be squared, with side power of 2" << path << "...";

    QImage img(path);
    if( img.isNull() ) {
        ERR << "Ïmage not available";
        return false;
    }

    INF << "Width:" << img.width() << "Height:" << img.height() << "Format:" << img.format();

    if( img.width() % 2) {
        ERR << "Ïmage width is not even";
        return false;
    }

    mSide = img.width() / 2;

    if( mSide != img.height() ) {
        ERR << "Single image is not squared";
        return false;
    }

    if( !isPowerOfTwo(mSide) ) {
        ERR << "Single image side is not power of two (^2)";
        return false;
    }

    mPixelsCount = mSide * mSide;
    mDisparitySize = DISPARITY_PART * mSide;

    mLeft.reserve(mPixelsCount);
    mRight.reserve(mPixelsCount);

    mLeftImage = QImage(mSide, mSide, img.format());
    mRightImage = QImage(mSide, mSide, img.format());

    // mLeftImage.fill(QColor(255, 0, 0));
    // mRightImage.fill(QColor(0, 255, 0));

    bool hasAlpha = img.hasAlphaChannel();

    for( uint y = 0; y < mSide; y ++) {
        for( uint x = 0; x < mSide; x ++) {
            QColor pixelLeft = img.pixelColor(x, y);
            QColor pixelRight = img.pixelColor(x + mSide, y);

            mLeftImage.setPixelColor(x, y, pixelLeft);
            mRightImage.setPixelColor(x, y, pixelRight);

            if( hasAlpha ) {
                mLeft.append( RGBA2VAL_NORM(pixelLeft) );
                mRight.append( RGBA2VAL_NORM(pixelRight) );
            }else {
                mLeft.append( RGB2VAL_NORM(pixelLeft) );
                mRight.append( RGB2VAL_NORM(pixelRight) );
            }
        }
    }

    mStage = Stage::ImagesLoaded;

    // disparity left & right rows progress
    gMainWindow->setProgressSteps(mSide*2);

    INF << "Loading image done!";

    return true;
}

// Combines both left&right image into single Anaglyph image
QImage Stereo::anaglyphImage()
{
    QImage result(mSide, mSide, mLeftImage.format());

    for( uint y = 0; y < mSide; y ++) {
        int offset = y * mSide;
        for( uint x = 0; x < mSide; x ++) {
            int index = offset + x;
            float lval = mLeft[index] * 255;
            float rval = mRight[index]  * 255;
            float average = (rval+lval) * 0.5f;
            QColor pixel = QColor(lval, average, rval);
            result.setPixelColor(x, y, pixel);
        }
    }
    return result;
}


void Stereo::process(bool isOpenCV)
{
    mIsAborting = false;
    mDepth = mRightDisp = mLeftDisp = QVector<float>(mPixelsCount, INVALID_DIST);

    mDepth.fill(INVALID_DIST);
    mLeftDisp.fill(INVALID_DIST);
    mRightDisp.fill(INVALID_DIST);

    if( isOpenCV ) {
        // Use OpenCV disparity
        mDisparityImage = cvDisparity();
    }
    else {
        // To be implmented with own disparity
        mStage = Stage::ProcessingLeft;

        if( !mIsAborting ) {
            mStage = Stage::ProcessingRight;

        }else{
            gMainWindow->aborted();
        }
    }

    if( !mIsAborting) {
        gMainWindow->finished();
    }else {
        gMainWindow->aborted();
    }

    mStage = Stage::Ready;
}


// Computes the disparity map from rectified left and right cv::Mat images
QImage Stereo::cvDisparity() {

    cv::Mat left32(mSide, mSide, CV_32FC1, mLeft.data());
    cv::Mat right32(mSide, mSide, CV_32FC1, mRight.data());

    cv::Mat left, right;
    left32.convertTo(left, CV_8U, 255.0);
    right32.convertTo(right, CV_8U, 255.0);

    qDebug() << "Left:" << left.size().width <<"x" << left.size().height <<"x" << left.depth() <<"|"<< left.type();
    qDebug() << "Right:" << right.size().width <<"x" << right.size().height <<"x" << right.depth() <<"|"<< right.type();

    // Configure StereoSGBM parameters
    int minDisparity = 0;
    int numDisparities = 64; // Must be divisible by 16
    int blockSize = 1;       // Must be an odd number >= 1

    cv::Ptr<cv::StereoSGBM> sgbm = cv::StereoSGBM::create(
        minDisparity,
        numDisparities,
        blockSize
        );

    // Set additional SGBM parameters for smoother maps
    sgbm->setP1(8 * left.channels() * blockSize * blockSize);
    sgbm->setP2(32 * left.channels() * blockSize * blockSize);
    sgbm->setDisp12MaxDiff(1);
    sgbm->setPreFilterCap(63);
    sgbm->setUniquenessRatio(15);
    sgbm->setSpeckleWindowSize(100);
    sgbm->setSpeckleRange(32);
    sgbm->setMode(cv::StereoSGBM::MODE_SGBM);

    // Compute disparity (Output is CV_16S / 16-bit signed integer)
    cv::Mat disparity16S;
    sgbm->compute(left, right, disparity16S);

    // Normalize and convert to 8-bit unsigned integer (CV_8U) for visual representation
    cv::Mat disparity8U;
    cv::normalize(disparity16S, disparity8U, 0, 255, cv::NORM_MINMAX, CV_8UC1);

    QImage result(mSide, mSide, mLeftImage.format());

    int index = 0;
    for( int y = 0; y < mSide; y ++) {
        for( int x = 0; x < mSide; x ++) {
            uint8_t v = disparity8U.at<uint8_t>(index);
            result.setPixelColor(x, y, QColor(v, v, v));
            index ++;
        }
    }

    return result;
}
