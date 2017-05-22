#pragma once

#include <QIODevice>

#include <zlib.h>

class QZStreamBase : public QIODevice
{
public:
	bool hasError() const;

protected:
	QZStreamBase(QIODevice *stream, QObject *parent = nullptr);

	virtual bool waitForReadyRead(int msecs) override;
	virtual bool waitForBytesWritten(int msecs) override;

	bool check(int code);

protected:
	bool openStream(OpenMode mode);

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

class QZDecompressionStream final : public QZStreamBase
{
	Q_OBJECT

public:
	explicit QZDecompressionStream(
		QIODevice *source,
		qint64 uncompressedSize = -1,
		QObject *parent = nullptr);

	virtual ~QZDecompressionStream() override;

	virtual bool isSequential() const override;

	virtual qint64 pos() const override;
	virtual bool open(OpenMode mode = ReadOnly) override;
	virtual void close() override;

	virtual qint64 size() const override;
	virtual bool seek(qint64 pos) override;

	virtual qint64 bytesAvailable() const override;
	virtual qint64 bytesToWrite() const override;

	virtual bool canReadLine() const override;

	virtual qint64 readData(char *data, qint64 maxlen) override;

private:
	virtual qint64 writeData(const char *, qint64) override;

private:
	qint64 mUncompressedSize;
};

class QZCompressionStream final : public QZStreamBase
{
	Q_OBJECT

public:
	explicit QZCompressionStream(
		QIODevice *target,
		int compressionLevel = -1,
		QObject *parent = nullptr);

	virtual ~QZCompressionStream() override;

	virtual bool isSequential() const override;

	virtual qint64 pos() const override;
	virtual bool open(OpenMode mode = WriteOnly) override;
	virtual void close() override;

	virtual qint64 size() const override;
	virtual bool seek(qint64 pos) override;

	virtual qint64 bytesToWrite() const override;
	virtual qint64 writeData(const char *data, qint64 maxlen) override;

private:
	virtual qint64 readData(char *, qint64) override;
	virtual qint64 bytesAvailable() const override;

	bool flushBuffer(int size = BUFFER_SIZE);

private:
	int mCompressionLevel;
};
