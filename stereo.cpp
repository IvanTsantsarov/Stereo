#include "stereo.h"
#include "mainwindow.h"
#include <QDebug>

#define DISPARITY_PART 0.1f
#define MAX_FLOAT 1e16

#define ERR qCritical() << __FILE__ << __FUNCTION__ << ":"
#define INF qInfo() << __FILE__ << __FUNCTION__ << ":"
#define RGB2VAL(__rgb__) (__rgb__.red()*0.2989f +  __rgb__.green()*0.5870f + __rgb__.blue()*0.1140f )
#define COLOR_BACK qRgb(255, 0, 255)


Stereo::Stereo() {

    mDepthK = 50.0f;//focalLengthPixels * baseline
}

// Loading double (left and right eye) images from a single image
bool Stereo::loadImages(const QString &path, bool isSwap)
{
    INF << "Loading double (left & right) images from:" << path << "...";
    INF << (isSwap ? " Swap left and right" : "");

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

    int offsetLeft = isSwap ? mSide : 0;
    int offsetRight = isSwap ? 0 : mSide;

    for( uint y = 0; y < mSide; y ++) {
        for( uint x = 0; x < mSide; x ++) {
            QColor pixelLeft = img.pixelColor(x + offsetLeft, y);
            QColor pixelRight = img.pixelColor(x + offsetRight, y);

            mLeftImage.setPixelColor(x, y, pixelLeft);
            mRightImage.setPixelColor(x, y, pixelRight);

            // Calculate intensity from the color and normalize it [0..1]
            mLeft.append( RGB2VAL(pixelLeft) );
            mRight.append( RGB2VAL(pixelRight) );
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


 void Stereo::process(bool isOpenCV,
                     float focalL,
                     float sensorW,
                     float distanceEyes,
                     int maxDisparity)
{
    mIsAborting = false;

    mDisparityImage.fill(COLOR_BACK);
    mDepthImage.fill(COLOR_BACK);

    if( isOpenCV ) {

        // Use OpenCV disparity
        // on Blender 51mm cam standart params
        cvDisparityDepth( focalL, sensorW, distanceEyes, maxDisparity);
    }
    else {
        myDisparityDepth(focalL, sensorW, distanceEyes, maxDisparity);
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

//////////////////////////////////////////////////////////////////////////////////////////
// Obtain disparity map with OpenCV and calculate manualy the depth map
//////////////////////////////////////////////////////////////////////////////////////////
void Stereo::cvDisparityDepth(float focalLengthMM,
                              float sensorSizeMM,
                              float distanceBetweenEyesM,
                              uint maxDisprity ) {

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
    int numDisparities = maxDisprity; // Must be divisible by 16
    int blockSize = 3;       // Must be an odd number >= 1

    cv::Ptr<cv::StereoSGBM> sgbm = cv::StereoSGBM::create(
        minDisparity,
        numDisparities,
        blockSize
        );

    // Set additional SGBM (AI generated) parameters for smoother maps
    // Can be commented, they are not essential
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

    // Normalize and convert to 8-bit unsigned integer (CV_8U) for the GUI
    cv::Mat disparity8U;
    cv::normalize(disparity16S, disparity8U, 0, 255, cv::NORM_MINMAX, CV_8UC1);

    cv::Mat disparity32F;
    disparity16S.convertTo(disparity32F, CV_32F, 1.0 / 16.0);

    mDisparityImage.fill(COLOR_BACK);

    double minResultD, maxResultD;
    cv::minMaxLoc(disparity32F, &minResultD, &maxResultD);
    qDebug() << "Min&Max result disp:" << minResultD<< maxResultD;


    // Create disparity image
    for( int y = 0; y < mSide; y ++) {
        for( int x = 0; x < mSide; x ++) {
            uint8_t c = disparity8U.at<uint8_t>(y, x);
            mDisparityImage.setPixelColor(x, y, QColor(c, c, c));
        }
    }

    // Create an empty floating-point matrix for the depth map (CV_32F)
    cv::Mat depthMap = cv::Mat::zeros(disparity16S.size(), CV_32FC1);

    // Constant scaling factor: f * B * 16 (since raw disparity is multiplied by 16)
    float focalLengthPx = focalLengthMM / sensorSizeMM * mSide;
    float fB16 = focalLengthPx * distanceBetweenEyesM * 16.0;

    float depthMin = std::numeric_limits<float>::max();
    float depthMax = 0.0f;

    for (int y = 0; y < mSide; ++y) {
        for (int x = 0; x < mSide; ++x) {
            int16_t rawDisp = disparity16S.at<int16_t>(y, x);

            // Filter out invalid disparities (OpenCV sets bad pixels to <= 0 or tiny values)
            if (rawDisp <= 0) {
                depthMap.at<float>(y, x) = 0.0f; // 0.0 means unknown/infinite depth
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

            depthMap.at<float>(y, x) = d;
        }
    }

    qDebug() << "Min&Max depth:" << depthMin << depthMax;
    qDebug() << "fB:" << fB16;

    /////////////////////////////////////////////////////////////////////
    // This here is because there is some small amount of values that are
    // way bigger then others and the depth image is loking very dark
    // after "normalization"
    depthMax = 20; // trash
    /////////////////////////////////////////////////////////////////////

    float depthDeltaInv = 255.0f / (depthMax - depthMin);

    mDepthImage.fill(COLOR_BACK);

    for( int y = 0; y < mSide; y ++) {
        for( int x = 0; x < mSide; x ++) {
            float depth = depthMap.at<float>(y, x);
            float depthNorm = (depth - depthMin) * depthDeltaInv;

            if( depthNorm < 0.0f ) {
                continue;
            }

            uint8_t c = std::clamp( depthNorm, 0.0f, 255.0f);
            QColor col = QColor(c, c, c);
            mDepthImage.setPixelColor( x, y, col );
        }
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
// My disparity map and depth calculalations
//////////////////////////////////////////////////////////////////////////////////////////
void Stereo::myDisparityDepth(float focalLengthMM,
                              float sensorSizeMM,
                              float distanceBetweenEyesM,
                              uint maxDisparity)
{
    gMainWindow->setProgressSteps(4 * mSide);

    quint16 P1 = 10;
    quint16 P2 = 50;

    float focalLengthPx = focalLengthMM / sensorSizeMM * mSide;
    float fB = focalLengthPx * distanceBetweenEyesM;

    // Smaller (and faster) representations of the floating point image intensities
    QVector<quint8> left, right;
    left.resize(mPixelsCount);
    right.resize(mPixelsCount);
    for( auto i = 0; i < mPixelsCount; i++) {
        left[i] = static_cast<quint8>(mLeft[i]);
        right[i] = static_cast<quint8>(mRight[i]);
    }

    // Allocate cost volume
    QVector<QVector<quint8>> costVol(mPixelsCount, QVector<quint8>(maxDisparity, 0));

    auto getPixelIndex = [&](bool isRight, int index) {
        return isRight ? right[index] : left[index];
    };

    auto getPixel = [&](bool isRight, int x, int y) {
        // Clamping is a bit ugly and need to be optimized, but not now...
        x = std::clamp(x, 0, static_cast<int>(mSide-1));
        y = std::clamp(y, 0, static_cast<int>(mSide-1));
        return getPixelIndex(isRight, y*mSide + x);
    };

    auto calcDescriptor = [&](bool isRight, int x, int y) {
        quint8 d = 0;
        quint8 p = getPixel(isRight, x, y);
        d |= getPixel(isRight, x-1, y-1) > p ? 1 : 0; d <<= 1;
        d |= getPixel(isRight, x  , y-1) > p ? 1 : 0; d <<= 1;
        d |= getPixel(isRight, x+1, y-1) > p ? 1 : 0; d <<= 1;
        d |= getPixel(isRight, x-1, y  ) > p ? 1 : 0; d <<= 1;
        // d |= getPixel(isRight, x  , y  ) > p ? 1 : 0; d <<= 1; // current pixel
        d |= getPixel(isRight, x+1, y  ) > p ? 1 : 0; d <<= 1;
        d |= getPixel(isRight, x-1, y+1) > p ? 1 : 0; d <<= 1;
        d |= getPixel(isRight, x  , y+1) > p ? 1 : 0; d <<= 1;
        d |= getPixel(isRight, x+1, y+1) > p ? 1 : 0; d <<= 1;
        return d;
    };

    auto HammingDistance = [&](quint8 bitDescA, quint8 bitDescB) {
        // because __builtin_popcount() is available only on the GCC compiler
        // and we don't need to see inline asm here
        quint8 distance = 0;
        quint8 bit = 1;
        // Let's unroll it
        distance += (bitDescA & bit) ^ (bitDescB & bit) ? 1 : 0; bit <<= 1;
        distance += (bitDescA & bit) ^ (bitDescB & bit) ? 1 : 0; bit <<= 1;
        distance += (bitDescA & bit) ^ (bitDescB & bit) ? 1 : 0; bit <<= 1;
        distance += (bitDescA & bit) ^ (bitDescB & bit) ? 1 : 0; bit <<= 1;

        distance += (bitDescA & bit) ^ (bitDescB & bit) ? 1 : 0; bit <<= 1;
        distance += (bitDescA & bit) ^ (bitDescB & bit) ? 1 : 0; bit <<= 1;
        distance += (bitDescA & bit) ^ (bitDescB & bit) ? 1 : 0; bit <<= 1;
        distance += (bitDescA & bit) ^ (bitDescB & bit) ? 1 : 0;
        return distance;
    };

    // Calculate Cost volume
    for( auto y = 0; y < mSide; y ++) {
        for( auto x = maxDisparity; x < mSide; x ++) {

            quint8 leftDescriptor = calcDescriptor(false, x, y);
            for( auto d = 0; d < maxDisparity; d ++) {
                // -d if the right image appears on the left in the anaglyph image
                // otherwise it will be +d, but we have "swap images" option in the GUI
                quint8 rightDescriptor = calcDescriptor(true, x - d, y);
                costVol[y*mSide + x][d] = HammingDistance(leftDescriptor, rightDescriptor);
            }

        }
    }

    // Allocate path aggregation array
    QVector<QVector<quint32>> aggregatedCost(mPixelsCount, QVector<quint32>(maxDisparity, 0));

    auto setBounds = [&](int dir, int& start, int& end) {
        if( dir > 0 ) {
            start = 0; end = mSide;
        }else {
            start = mSide-1; end = -1;
        }
    };

    auto aggregate = [&](bool isHorizontal, int dir)
    {
        QVector<quint32> prevCost(maxDisparity);
        QVector<quint32> currCost(maxDisparity);

        int primStart, primEnd;
        int secStart, secEnd;

        setBounds(dir, primStart, primEnd);
        setBounds(1, secStart, secEnd);

        for( int sec = secStart; sec != secEnd; sec++ )
        {
            const int x = isHorizontal ? primStart : sec;
            const int y = isHorizontal ? sec : primStart;
            int pixelIndex = y * mSide + x;

            // First pixel
            for (int d = 0; d < maxDisparity; ++d)
            {
                prevCost[d] = costVol[pixelIndex][d];
                aggregatedCost[pixelIndex][d] += prevCost[d];
            }

            for (int prim = primStart + dir; prim != primEnd; prim += dir) {
                const int x = isHorizontal ? prim : sec;
                const int y = isHorizontal ? sec : prim;
                int pixelIndex = y * mSide + x;

                // Find the min of the previous cost
                quint32 minPrev = prevCost[0];
                for( quint16 val:prevCost) {
                    if( val < minPrev ) {
                        minPrev = val;
                    }
                }

                for( int d = 0; d < maxDisparity; d++ ) {
                    quint32 best = prevCost[d];

                    if (d > 0) {
                        best = std::min( best, prevCost[d - 1] + P1 );
                    }

                    if ((d + 1) < maxDisparity){
                        best = std::min( best, prevCost[d + 1] + P1 );
                    }

                    best = std::min(best, minPrev + P2);

                    quint32 value = static_cast<quint32>(costVol[pixelIndex][d]) + best - minPrev;
                    currCost[d] = value;
                    aggregatedCost[pixelIndex][d] += value;
                }

                std::swap(prevCost, currCost);
            }

            gMainWindow->stepProgress();
        }
    };


    // Left 2 Right
    aggregate(true,  1);

    // Right 2 Left
    aggregate(true, -1);

    // Top 2 Down
    aggregate(false, 1);

    // Down to Top
    aggregate(false, -1);


    QVector<float> disparity(mPixelsCount, 0.0f);
    int minDisp = std::numeric_limits<quint16>::max();
    int maxDisp = -minDisp;

    // Winner takes all
    for (int y = 0; y < mSide; ++y) {
        for (int x = 0; x < mSide; ++x) {
            int pixelIndex = y * mSide + x;

            quint32 bestCost = std::numeric_limits<quint32>::max();
            int bestDisp = 0;

            for (int d = 0; d < maxDisparity; ++d)
            {
                quint32 cost = aggregatedCost[pixelIndex][d];
                if (cost < bestCost)
                {
                    bestCost = cost;
                    bestDisp = d;
                }
            }

            if( bestDisp < minDisp) {
                minDisp = bestDisp;
            }

            if( bestDisp > maxDisp) {
                maxDisp = bestDisp;
            }

            disparity[pixelIndex] = static_cast<float>(bestDisp);
        }
    }


    float k = minDisp == maxDisp ? 0.0f : 255.0f / (maxDisp - minDisp);
    int pixelIndex = 0;
    for( auto y = 0; y < mSide; y ++) {
        for( auto x = 0; x < mSide; x ++) {
            quint8 c = (disparity[pixelIndex++] - minDisp) * k;
            QColor col(c, c, c);
            mDisparityImage.setPixelColor(x, y, col);
        }
    }

}

