#ifndef STEREO_H
#define STEREO_H

#include <QImage>
#include <QObject>
#include <opencv2/opencv.hpp>

typedef QVector<float> FloatVector;

class Stereo
{
    FloatVector mLeft, mRight; // normalized floating point intesity
    FloatVector mLeftDisp, mRightDisp, mDepth;
    QImage mLeftImage, mRightImage, mDisparityImage, mAnaglyphImage, mDepthImage;

    uint mSide = 0;
    uint mPixelsCount = 0;
    int mDisparitySize = 0;
    float mDepthK = 0.0f;

    inline float leftValue(int x, int y){ return mLeft[x + y*mSide];}
    inline float rightValue(int x, int y){ return mRight[x + y*mSide];}

    void cvDisparityDepth(float focalLengthMM,
                          float sensorSizeMM,
                          float distanceBetweenEyesM,
                          uint maxDisprity);

    void myDisparityDepth(float focalLengthMM,
                          float sensorSizeMM,
                          float distanceBetweenEyesM,
                          uint maxDisparity);


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
    bool loadImages(const QString& path, bool isSwap);
    const QImage& leftImage(){ return mLeftImage; }
    const QImage& rightImage(){ return mRightImage; }
    const QImage& disparityImage(){ return mDisparityImage; }
    const QImage& depthImage(){ return mDepthImage; }
    const QImage& anaglyphImage() { return mAnaglyphImage; }

    void process(bool isOpenCV,
                 float focalL,
                 float sensorW,
                 float distanceEyes,
                 int maxDisparity);
    void abort(){ mIsAborting = true; }

    static bool isPowerOfTwo(uint value) { return value != 0 && (value & (value - 1)) == 0; }
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
