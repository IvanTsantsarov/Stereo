#include "stereo.h"
#include "mainwindow.h"
#include <QDebug>
#include <algorithm>

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
#define RGB2VAL(__rgb__) ( qRed(__rgb__)*0.2989f +  qGreen(__rgb__)*0.5870f + qBlue(__rgb__)*0.1140f )

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

    for( uint y = 0; y < mSide; y ++) {
        for( uint x = 0; x < mSide; x ++) {
            QRgb pixelLeft = img.pixel(x, y);
            QRgb pixelRight = img.pixel(x + mSide, y);

            mLeftImage.setPixel(x, y, pixelLeft);
            mRightImage.setPixel(x, y, pixelRight);

            mLeft.append(RGB2VAL(pixelLeft));
            mRight.append(RGB2VAL(pixelRight));
        }
    }

    mStage = Stage::ImagesLoaded;

    // disparity left & right rows progress
    gMainWindow->setProgressSteps(mSide*2);

    INF << "Loading image done!";

    return true;
}

void Stereo::fillStencil(int x, int y,
                         int& outStartX, int& outEndX,
                         QVector<int8_t> &outStencil,
                         bool isLeft)
{
    // invalidate all values
    // posible values: [-1, 0, 1]
    outStencil.fill(100);

    // get current pixel value
    float value = isLeft ? leftValue(x, y) : rightValue(x, y);

    // set start and end borders
    int startx = std::max(x - STENCIL_SIDE, 0);
    int endx = std::min(x + STENCIL_SIDE, (int)mSide - 1);

    for( int dx = startx; dx <= endx; dx ++) {

        // calculate diff between current
        float diff = (isLeft ? leftValue(dx, y) : rightValue(dx, y)) - value;

        int outStencilIndex = dx - x + STENCIL_SIDE;
        if( std::abs(diff) < MIN_DIFF) {
            outStencil[outStencilIndex] = 0;
        }else
        if( diff < 0.0f ) {
            outStencil[outStencilIndex] = -1;
        }else
        {
            outStencil[outStencilIndex] = 1;
        }
    }

    outStartX = startx - x + STENCIL_SIDE;
    outEndX = endx - x + STENCIL_SIDE;
}

// fill stencil from other image at position (x,y)
// compare with input stencil
// return distance
uint Stereo::compareWithStencil(int x, int y, int sx, int ex,
                                const QVector<int8_t> &stencil,
                                QVector<int8_t>& stencilOther,
                                bool isLeft)
{
    int startOtherX, endOtherX;
    fillStencil(x, y, startOtherX, endOtherX, stencilOther, !isLeft);

    int startX = std::max(sx, startOtherX);
    int endX = std::min(ex, endOtherX);
    uint distance = 0;
    for( uint tx = startX; tx <= endX; tx ++) {
        if( stencil[tx] != stencilOther[tx] ) {
            distance ++;
        }
    }

    return distance;
}

void Stereo::calculateDisparity(bool isLeft)
{
    // stencil keeps first derivative that characterize a pixel
    // by surrounding pixels
    QVector<int8_t> stencil(STENCIL_SIZE);
    QVector<int8_t> stencilOther(STENCIL_SIZE);

    QVector<float> &disparity = isLeft ? mLeftDisp : mRightDisp;

    // vector with all distances for the row
    QVector<uint> distances(mSide);

    for( int y = 0; y < mSide; y ++) {
        for( int x = 0; x < mSide; x ++) {
            int sx, ex;
            fillStencil(x, y, sx, ex, stencil, isLeft);

            // find the minimum distances pixel
            uint minDist = STENCIL_SIZE;
            uint correspondingX = 0;

            uint oxstart = std::max(x - mDisparitySize, 0);
            uint oxend = std::min(x + mDisparitySize, (int)mSide-1);
            for( int ox = oxstart; ox < oxend; ox ++) {
                uint dist = compareWithStencil(ox, y, sx, ex, stencil, stencilOther, isLeft);
                if( dist < minDist ) {
                    minDist = dist;
                    correspondingX = ox;
                }
            }

            disparity[x + y*mSide] = x - correspondingX;
        }

        gMainWindow->stepProgress();
        if( mIsAborting ) {
            return;
        }
    }
}

void Stereo::calculateDepth()
{
    INF << "Calculating depth...";

    float minDepth = MAXFLOAT;
    float maxDepth = -MAXFLOAT;

    int index = 0;
    for( int y = 0; y < mSide; y ++) {
        for( int x = 0; x < mSide; x ++) {
            float valL = mLeftDisp[index];
            float valR = mRightDisp[index];
            if( valL > MAX_DIST_CMP) valL = valR;
            if( valR > MAX_DIST_CMP) valR = valL;
            float disparity = (valL + valR) * 0.5;

            if( disparity < MAX_DIST_CMP && disparity )  {
                float depth = mDepthK / disparity;

                if( depth > maxDepth ) maxDepth = depth;
                if( depth < minDepth ) minDepth = depth;

                mDepth[index] = depth ;
            }
            index ++;
        }
    }

    INF << "Min depth:" << minDepth << "Max depth:" << maxDepth;

    float scaleDepth = 1.0f / (maxDepth - minDepth);
    mDepthImage = QImage(mSide, mSide, mLeftImage.format());

    index = 0;
    for( int y = 0; y < mSide; y ++) {
        for( int x = 0; x < mSide; x ++) {
            float val = mDepth[index];
            if( val < MAX_DIST_CMP ) {
                // valid depth
                float intensityNorm = (val - minDepth) * scaleDepth;
                INF << val;
                uint8_t col = intensityNorm * 255;
                mDepthImage.setPixel(x, y, qRgb(col, col, col));
            }else {
                // invalid depth
                mDepthImage.setPixel(x, y, qRgb(255, 0, 255));
            }
            index ++;
        }
    }


}

void Stereo::process()
{
    mIsAborting = false;
    mDepth = mRightDisp = mLeftDisp = QVector<float>(mPixelsCount, INVALID_DIST);

    mDepth.fill(INVALID_DIST);
    mLeftDisp.fill(INVALID_DIST);
    mRightDisp.fill(INVALID_DIST);

    mStage = Stage::ProcessingLeft;
    calculateDisparity(true); // left

    if( !mIsAborting ) {
        mStage = Stage::ProcessingRight;
        calculateDisparity(false); // right
    }else{
        gMainWindow->aborted();
    }

    if( !mIsAborting) {
        calculateDepth();
        gMainWindow->finished();
    }else {
        gMainWindow->aborted();
    }


    mStage = Stage::Ready;
}
