#ifndef STEREO_H
#define STEREO_H

#include <QImage>
#include <QObject>

class Stereo
{
    QVector<float> mLeft, mRight, mLeftDisp, mRightDisp, mDepth;
    uint mSide = 0;
    uint mPixelsCount = 0;
    QImage mLeftImage, mRightImage;
    inline float leftValue(int x, int y){ return mLeft[x + y*mSide];}
    inline float rightValue(int x, int y){ return mRight[x + y*mSide];}

    static bool isPowerOfTwo(uint value);

    void fillStencil(int x, int y,
                     int &outStartX, int &outEndX,
                     QVector<int8_t>& outStencil, bool isLeft);

    uint compareWithStencil(int x, int y,
                            int sx, int ex,
                            const QVector<int8_t>& stencil, bool isLeft);

    void calculateDisparity(bool isLeft);
public:
    Stereo();
    bool loadImages(const QString& path);
    const QImage& leftImage(){ return mLeftImage; }
    const QImage& rightImage(){ return mRightImage; }



    void process();
};

#endif // STEREO_H
