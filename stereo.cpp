#include "stereo.h"
#include "mainwindow.h"
#include <QDebug>

#define DISPARITY_PART 0.1f
#define MAX_FLOAT 1e16

#define ERR qCritical() << __FILE__ << __FUNCTION__ << ":"
#define INF qInfo() << __FILE__ << __FUNCTION__ << ":"
#define RGB2VAL(__rgb__) (__rgb__.red()*0.2989f +  __rgb__.green()*0.5870f + __rgb__.blue()*0.1140f )
#define RGBA2VAL(__rgba__) (( __rgba__.red()*0.2989f +  __rgba__.green()*0.5870f + __rgba__.blue()*0.1140f) * __rgba__.alpha() * 1.0f/255.0f)
#define RGB2VAL_NORM(__rgb__) std::clamp( RGB2VAL(__rgb__) * 1.0f/255.0f, 0.0f, 1.0f)
#define RGBA2VAL_NORM(__rgba__) std::clamp( RGBA2VAL(__rgba__) * 1.0f/255.0f, 0.0f, 1.0f)

#define COLOR_BACK qRgb(255, 0, 255)




Stereo::Stereo() {

    mDepthK = 50.0f;//focalLengthPixels * baseline
}

// Loading double (left and right eye) images from a single image
bool Stereo::loadImages(const QString &path, bool isSwap)
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

    /*
    if( !isPowerOfTwo(mSide) ) {
        ERR << "Single image side is not power of two (^2)";
        return false;
    }
    */

    mPixelsCount = mSide * mSide;
    mDisparitySize = DISPARITY_PART * mSide;

    mLeft.clear();
    mRight.clear();
    mLeft.reserve(mPixelsCount);
    mRight.reserve(mPixelsCount);

    mLeftImage = QImage(mSide, mSide, img.format());
    mRightImage = QImage(mSide, mSide, img.format());
    mDepthImage = QImage(mSide, mSide, img.format());
    mAnaglyphImage = QImage(mSide, mSide, img.format());
    mDisparityImage = QImage(mSide, mSide, img.format());

    // mLeftImage.fill(QColor(255, 0, 0));
    // mRightImage.fill(QColor(0, 255, 0));

    bool hasAlpha = img.hasAlphaChannel();

    int offsetLeft = isSwap ? mSide : 0;
    int offsetRight = isSwap ? 0 : mSide;

    for( uint y = 0; y < mSide; y ++) {
        for( uint x = 0; x < mSide; x ++) {
            QColor pixelLeft = img.pixelColor(x + offsetLeft, y);
            QColor pixelRight = img.pixelColor(x + offsetRight, y);

            mLeftImage.setPixelColor(x, y, pixelLeft);
            mRightImage.setPixelColor(x, y, pixelRight);

            // Calculate intensity from the color and normalize it [0..1]
            if( hasAlpha ) {
                mLeft.append( RGBA2VAL(pixelLeft) );
                mRight.append( RGBA2VAL(pixelRight) );
            }else {
                mLeft.append( RGB2VAL(pixelLeft) );
                mRight.append( RGB2VAL(pixelRight) );
            }
        }
    }

    // Combines both left&right intensity images into single Anaglyph image
    // (to verify that images are properly loaded)
    for( uint y = 0; y < mSide; y ++) {
        int offset = y * mSide;
        for( uint x = 0; x < mSide; x ++) {
            int index = offset + x;
            float lval = mLeft[index];
            float rval = mRight[index];
            float average = (rval+lval) * 0.5f;

            // RED image part is from the LEFT image
            // BLUE image part is from the RIGHT image
            QColor pixel = QColor(lval, average, rval);
            mAnaglyphImage.setPixelColor(x, y, pixel);
        }
    }

    mStage = Stage::ImagesLoaded;

    // disparity left & right rows progress
    gMainWindow->setProgressSteps(mSide*2);

    INF << "Loading image done!";

    return true;
}


 void Stereo::process(bool isOpenCV)
{
    mIsAborting = false;
    mDepth = mRightDisp = mLeftDisp = QVector<float>(mPixelsCount, 0.0f);

    if( isOpenCV ) {

        // Use OpenCV disparity
        // on Blender 51mm cam standart params
        cvDisparityDepth( 51, 36, 0.065f);
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


// Obtain disparity map and depth map with OpenCV
void Stereo::cvDisparityDepth(float focalLengthMM, float sensorSizeMM, float distanceBetweenEyesM ) {

    cv::Mat left32(mSide, mSide, CV_32FC1, mLeft.data());
    cv::Mat right32(mSide, mSide, CV_32FC1, mRight.data());

    double minL, maxL;
    double minR, maxR;

    cv::minMaxLoc(left32, &minL, &maxL);
    cv::minMaxLoc(right32, &minR, &maxR);

    qDebug() << "Left intensity:" << minL << maxL;
    qDebug() << "Right intensity:" << minR << maxR;

    cv::Mat left, right;
    left32.convertTo(left, CV_8U);
    right32.convertTo(right, CV_8U);

    qDebug() << "OpenCV disparity & depth";
    qDebug() << "Left:" << left.size().width <<"x" << left.size().height <<"x" << left.depth() <<"|"<< left.type();
    qDebug() << "Right:" << right.size().width <<"x" << right.size().height <<"x" << right.depth() <<"|"<< right.type();

    // Configure StereoSGBM parameters
    int minDisparity = 0;
    int numDisparities = 64; // Must be divisible by 16
    int blockSize = 3;       // Must be an odd number >= 1

    cv::Ptr<cv::StereoSGBM> sgbm = cv::StereoSGBM::create(
        minDisparity,
        numDisparities,
        blockSize
        );

    // Set additional SGBM parameters for smoother maps
    // (AI generated)

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

    cv::Mat disparity32F;
    disparity16S.convertTo(disparity32F, CV_32F, 1.0 / 16.0);

    mDisparityImage.fill(COLOR_BACK);

    float maxDisparity = 0.0f;

    for( int y = 0; y < mSide; y ++) {
        for( int x = 0; x < mSide; x ++) {
            float d = disparity32F.at<float>(y, x);
            if( d > maxDisparity) {
                maxDisparity = d;
            }
        }
    }

    float minD = 0;
    for( int y = 0; y < mSide; y ++) {
        for( int x = 0; x < mSide; x ++) {
            uint8_t c = disparity8U.at<uint8_t>(y, x);
            mDisparityImage.setPixelColor(x, y, QColor(c, c, c));
        }
    }

    qDebug() << "Min disparity:" << minD;

    // Create an empty floating-point matrix for the depth map (CV_32F)
    cv::Mat depthMap = cv::Mat::zeros(disparity16S.size(), CV_32FC1);

    // Constant scaling factor: f * B * 16 (since raw disparity is multiplied by 16)
    float focalLengthPx = focalLengthMM / sensorSizeMM * mSide;
    float fB16 = focalLengthPx * distanceBetweenEyesM * 16.0;

    float depthMin = std::numeric_limits<float>::max();
    float depthMax = 0.0f;

    for (int r = 0; r < disparity16S.rows; ++r) {
        // Direct pointer access for performance
        const int16_t* dispRow = disparity16S.ptr<int16_t>(r);
        float* depthRow = depthMap.ptr<float>(r);

        for (int c = 0; c < disparity16S.cols; ++c) {
            int16_t rawDisp = dispRow[c];

            // Filter out invalid disparities (OpenCV sets bad pixels to <= 0 or tiny values)
            if (rawDisp <= 0) {
                depthRow[c] = 0.0f; // 0.0 means unknown/infinite depth
                continue;
            }

            // Depth calculation: (f * B) / (rawDisp / 16.0) -> (f * B * 16) / rawDisp
            float d = fB16 / static_cast<float>(rawDisp);
            if( depthMax < d ) {
                depthMax = d;
            }
            if( depthMin > d) {
                depthMin = d;
            }
            depthRow[c] = d;
        }
    }

    float depthDeltaInv = 255.0f / (depthMax - depthMin);

    mDepthImage.fill(COLOR_BACK);

    for( int y = 0; y < mSide; y ++) {
        for( int x = 0; x < mSide; x ++) {
            float d = (depthMap.at<float>(y, x) - depthMin) * depthDeltaInv;

            if( d < 0.0f ) {
                d = -d;//continue;
            }
            uint8_t c = std::clamp( d, 0.0f, 255.0f);
            QColor col = QColor(c, c, c);
            mDepthImage.setPixelColor( x, y, col );
        }
    }
}

