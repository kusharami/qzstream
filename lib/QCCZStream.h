#pragma once

#include "QZStream.h"

class QBuffer;

namespace CCZ
{
bool validateHeader(QIODevice *device);
}

class QCCZDecompressor final : public QZDecompressor
{
	Q_OBJECT

public:
	explicit QCCZDecompressor(QObject *parent = nullptr);
	explicit QCCZDecompressor(QIODevice *source, QObject *parent = nullptr);

	virtual ~QCCZDecompressor() override;

	virtual void close() override;

	inline quint32 userValue() const;

protected:
	virtual bool initOpen(OpenMode mode) override;

private:
	quint32 mUserValue;
};

quint32 QCCZDecompressor::userValue() const
{
	return mUserValue;
}

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

	inline quint32 userValue() const;
	inline void setUserValue(quint32 value);

protected:
	virtual bool initOpen(OpenMode mode) override;

private:
	QByteArray *mBytes;
	QBuffer *mCCZBuffer;
	QIODevice *mTarget;

	qint64 mSavePosition;
	quint32 mUserValue;
};

quint32 QCCZCompressor::userValue() const
{
	return mUserValue;
}

void QCCZCompressor::setUserValue(quint32 value)
{
	mUserValue = value;
}
