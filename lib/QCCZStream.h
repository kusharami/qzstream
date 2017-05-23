#pragma once

#include "QZStream.h"

class QBuffer;

class QCCZDecompressionStream final : public QZDecompressionStream
{
	Q_OBJECT

public:
	explicit QCCZDecompressionStream(QObject *parent = nullptr);
	explicit QCCZDecompressionStream(
		QIODevice *source, QObject *parent = nullptr);

	virtual ~QCCZDecompressionStream() override;

	virtual void close() override;

protected:
	virtual bool initOpen(OpenMode mode) override;
};

class QCCZCompressionStream final : public QZCompressionStream
{
	Q_OBJECT

public:
	explicit QCCZCompressionStream(QObject *parent = nullptr);
	explicit QCCZCompressionStream(
		QIODevice *target,
		int compressionLevel = -1,
		QObject *parent = nullptr);

	virtual ~QCCZCompressionStream() override;

	virtual bool initOpen(OpenMode mode) override;
	virtual void close() override;

private:
	QByteArray *mBytes;
	QBuffer *mBuffer;
	QIODevice *mTarget;

	qint64 mSavePosition;
};
