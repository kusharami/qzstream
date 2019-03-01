#pragma once

#include "QZStream.h"

class QBuffer;

class QCCZDecompressor final : public QZDecompressor
{
	Q_OBJECT

public:
	explicit QCCZDecompressor(QObject *parent = nullptr);
	explicit QCCZDecompressor(QIODevice *source, QObject *parent = nullptr);

	virtual ~QCCZDecompressor() override;

	virtual void close() override;

protected:
	virtual bool initOpen(OpenMode mode) override;
};

class QCCZCompressor final : public QZCompressor
{
	Q_OBJECT

public:
	explicit QCCZCompressor(QObject *parent = nullptr);
	explicit QCCZCompressor(QIODevice *target, int compressionLevel = -1,
		QObject *parent = nullptr);

	virtual ~QCCZCompressor() override;

	virtual bool open(OpenMode mode = WriteOnly) override;
	virtual void close() override;

protected:
	virtual bool initOpen(OpenMode mode) override;

private:
	QByteArray *mBytes;
	QBuffer *mCCZBuffer;
	QIODevice *mTarget;

	qint64 mSavePosition;
};
