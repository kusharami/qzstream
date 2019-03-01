#pragma once

#include <QImageIOHandler>
#include <QSize>
#include <QSharedPointer>

class QImageReader;
class QImageWriter;
class QCCZDecompressor;
class QCCZCompressor;

class QCCZImageContainerHandler : public QImageIOHandler
{
	mutable QImageReader *mReader;
	mutable QCCZDecompressor *mDecompressor;
	QImageWriter *mWriter;
	QCCZCompressor *mCompressor;
	int mQuality;
	int mCompressionRatio;
	float mGamma;
	QByteArray mWriteFormat;
	QByteArray mWriteSubType;

public:
	QCCZImageContainerHandler();
	virtual ~QCCZImageContainerHandler() override;

	static QByteArray formatStatic();
	static QByteArray containedFormat(QIODevice *device);

	void setSubType(const QByteArray &t);

	virtual bool canRead() const override;
	virtual bool read(QImage *image) override;
	virtual bool write(const QImage &image) override;

	virtual QVariant option(ImageOption option) const override;
	virtual void setOption(ImageOption option, const QVariant &value) override;
	virtual bool supportsOption(ImageOption option) const override;

	// incremental loading
	virtual bool jumpToNextImage();
	virtual bool jumpToImage(int imageNumber);
	virtual int loopCount() const;
	virtual int imageCount() const;
	virtual int nextImageDelay() const;
	virtual int currentImageNumber() const;
	virtual QRect currentImageRect() const;

private:
	static const QList<QByteArray> &supportedSubTypes();
	QByteArray subType() const;
	static int compressionRatioToLevel(int ratio);

	bool ensureWritable();
	bool ensureScanned() const;
	bool scanDevice() const;
};
