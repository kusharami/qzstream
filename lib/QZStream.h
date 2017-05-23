#pragma once

#include <QIODevice>

#include <zlib.h>

class QZStreamBase : public QIODevice
{
public:
	void setStream(QIODevice *stream);

	bool hasError() const;

protected:
	QZStreamBase(QObject *parent = nullptr);
	QZStreamBase(QIODevice *stream, QObject *parent = nullptr);

	virtual bool waitForReadyRead(int msecs) override;
	virtual bool waitForBytesWritten(int msecs) override;

	bool check(int code);

protected:
	bool openStream(OpenMode mode);
	bool streamSeekInit();

protected:
	QIODevice *mStream;
	qint64 mStreamOriginalPosition;
	qint64 mStreamPosition;

	enum
	{
		BUFFER_SIZE = 32768
	};

	Bytef mBuffer[BUFFER_SIZE];

	z_stream mZStream;

	bool mHasError;
};

inline bool QZStreamBase::hasError() const
{
	return mHasError;
}

class QZDecompressionStream : public QZStreamBase
{
	Q_OBJECT

public:
	explicit QZDecompressionStream(QObject *parent = nullptr);
	explicit QZDecompressionStream(
		QIODevice *source,
		qint64 uncompressedSize = -1,
		QObject *parent = nullptr);

	void setUncompressedSize(qint64 value);

	virtual ~QZDecompressionStream() override;

	virtual bool isSequential() const override;

	virtual qint64 pos() const override;
	virtual bool open(OpenMode mode = ReadOnly) override;
	virtual void close() override;

	virtual qint64 size() const override;
	virtual bool seek(qint64 pos) override;

	virtual qint64 bytesAvailable() const override;

	virtual bool canReadLine() const override;

	virtual qint64 readData(char *data, qint64 maxlen) override;

protected:
	virtual bool initOpen(OpenMode mode);

private:
	virtual qint64 bytesToWrite() const override;
	virtual qint64 writeData(const char *, qint64) override;

protected:
	qint64 mUncompressedSize;
};

inline void QZDecompressionStream::setUncompressedSize(qint64 value)
{
	mUncompressedSize = value;
}

class QZCompressionStream : public QZStreamBase
{
	Q_OBJECT

public:
	explicit QZCompressionStream(QObject *parent = nullptr);
	explicit QZCompressionStream(
		QIODevice *target,
		int compressionLevel = Z_DEFAULT_COMPRESSION,
		QObject *parent = nullptr);

	int compressionLevel() const;
	void setCompressionLevel(int level);

	virtual ~QZCompressionStream() override;

	virtual bool isSequential() const override;

	virtual qint64 pos() const override;
	virtual bool open(OpenMode mode = WriteOnly) override;
	virtual void close() override;

	virtual qint64 size() const override;
	virtual bool seek(qint64 pos) override;

	virtual qint64 bytesToWrite() const override;
	virtual qint64 writeData(const char *data, qint64 maxlen) override;

protected:
	virtual bool initOpen(OpenMode mode);

private:
	virtual qint64 readData(char *, qint64) override;
	virtual qint64 bytesAvailable() const override;

	bool flushBuffer(int size = BUFFER_SIZE);

protected:
	int mCompressionLevel;
};

inline int QZCompressionStream::compressionLevel() const
{
	return mCompressionLevel;
}

inline void QZCompressionStream::setCompressionLevel(int level)
{
	mCompressionLevel = level;
}
