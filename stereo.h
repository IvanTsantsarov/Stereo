#ifndef STEREO_H
#define STEREO_H

#include <QImage>
#include <QObject>

class Stereo
{
    QVector<float> mLeft, mRight, mLeftDisp, mRightDisp, mDepth;
    uint mSide = 0;
    uint mPixelsCount = 0;
    int mDisparitySize = 0;
    float mDepthK = 0.0f;
    QImage mLeftImage, mRightImage, mDepthImage;
    inline float leftValue(int x, int y){ return mLeft[x + y*mSide];}
    inline float rightValue(int x, int y){ return mRight[x + y*mSide];}

    static bool isPowerOfTwo(uint value);

    void fillStencil(int x, int y,
                     int &outStartX, int &outEndX,
                     QVector<int8_t>& outStencil, bool isLeft);

    uint compareWithStencil(int x, int y,
                            int sx, int ex,
                            const QVector<int8_t>& stencil,
                            QVector<int8_t> &stencilOther,
                            bool isLeft);

    void calculateDisparity(bool isLeft);

    void calculateDepth();

    bool mIsAborting = false;
public:

    enum Stage {
        Initial = 0,
        ImagesLoaded,
        ProcessingLeft,
        ProcessingRight,
        Ready
    };

    Stereo();
    bool loadImages(const QString& path);
    const QImage& leftImage(){ return mLeftImage; }
    const QImage& rightImage(){ return mRightImage; }
    const QImage& depthImage(){ return mDepthImage; }

    void process();
    void abort(){ mIsAborting = true; }
private:
    Stage mStage = Initial;
public:
    bool hasImages(){ return !mLeftImage.isNull() && !mRightImage.isNull(); }
    bool isProcessing(){ return Stage::ProcessingLeft == mStage || Stage::ProcessingRight == mStage; }
    bool hasLeftDisparing(){ return Stage::ProcessingLeft < mStage; }
    bool hasRightDisparing(){ return Stage::ProcessingRight < mStage; }
    bool isReady(){ return Stage::Ready == mStage; }
    bool isAborting(){ return mIsAborting; }
};

#endif // STEREO_H
