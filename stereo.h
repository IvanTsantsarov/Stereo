#ifndef STEREO_H
#define STEREO_H

#include <QImage>
#include <QObject>

class Stereo
{
    QVector<float> mLeft, mRight;
    uint mSide = 0;
    QImage mLeftImage, mRightImage;

    static bool isPowerOfTwo(uint value);
public:
    Stereo();
    bool loadImages(const QString& path);
    const QImage& leftImage(){ return mLeftImage; }
    const QImage& rightImage(){ return mRightImage; }
};

#endif // STEREO_H
