#pragma once

#include <QImageIOHandler>
#include <QSize>
#include <QSharedPointer>
#include <QRect>

class QImageReader;
class QImageWriter;
class QCCZDecompressor;
class QCCZCompressor;

class QCCZImageContainerHandler : public QImageIOHandler
{
	QByteArray mWriteFormat;
	QByteArray mWriteSubType;
	QRect mClipRect;
	QSize mScaledSize;
	QRect mScaledClipRect;
	QString mDescription;
	Transformations mTransformations;

	mutable QImageReader *mReader;
	mutable QCCZDecompressor *mDecompressor;
	QImageWriter *mWriter;
	QCCZCompressor *mCompressor;
	int mQuality;
	int mCompressionRatio;
	float mGamma;
	bool mOptimizedWrite;
	bool mProgressiveScanWrite;
	mutable bool mAutoTransform;

public:
	QCCZImageContainerHandler();
	virtual ~QCCZImageContainerHandler() override;

	static QByteArray formatStatic();

	void setSubType(const QByteArray &t);

	virtual bool canRead() const override;
	virtual bool read(QImage *image) override;
	virtual bool write(const QImage &image) override;

	virtual QVariant option(ImageOption option) const override;
	virtual void setOption(ImageOption option, const QVariant &value) override;
	virtual bool supportsOption(ImageOption option) const override;

	// incremental loading
	virtual bool jumpToNextImage() override;
	virtual bool jumpToImage(int imageNumber) override;
	virtual int loopCount() const override;
	virtual int imageCount() const override;
	virtual int nextImageDelay() const override;
	virtual int currentImageNumber() const override;
	virtual QRect currentImageRect() const override;

private:
	static const QList<QByteArray> &supportedSubTypes();
	QByteArray subType() const;
	static int compressionRatioToLevel(int ratio);

	bool ensureWritable();
	bool ensureScanned() const;
	bool scanDevice() const;
};
