#include "stereo.h"
#include <QDebug>

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

    uint count = mSide * mSide;

    mLeft.reserve(count);
    mRight.reserve(count);

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

    INF << "Loading image done!";

    return true;
}
