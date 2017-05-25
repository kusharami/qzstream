#include "QZStream.h"

QZStreamBase::QZStreamBase(QObject *parent)
	: QIODevice(parent)
	, mStream(nullptr)
	, mStreamPosition(0)
	, mStreamOriginalPosition(0)
	, mHasError(false)
{
	memset(&mZStream, 0, sizeof(mZStream));
}

QZStreamBase::QZStreamBase(QIODevice *stream, QObject *parent)
	: QIODevice(parent)
	, mStream(nullptr)
	, mStreamPosition(0)
	, mStreamOriginalPosition(0)
	, mHasError(false)
{
	memset(&mZStream, 0, sizeof(mZStream));
	setStream(stream);
}

void QZStreamBase::setStream(QIODevice *stream)
{
	if (stream != mStream)
	{
		close();

		mStream = stream;
		mStreamOriginalPosition = stream ? stream->pos() : 0;
	}
}

bool QZStreamBase::waitForReadyRead(int msecs)
{
	if (isReadable())
		return mStream->waitForReadyRead(msecs);

	return false;
}

bool QZStreamBase::waitForBytesWritten(int msecs)
{
	if (isWritable())
		return mStream->waitForBytesWritten(msecs);

	return false;
}

bool QZStreamBase::check(int code)
{
	if (code < 0)
	{
		mHasError = true;
		setErrorString(QLatin1String(mZStream.msg));
	}

	return !mHasError;
}

bool QZStreamBase::openStream(OpenMode mode)
{
	if (mStream->isOpen())
	{
		if (mStream->openMode() != mode)
		{
			mHasError = true;
			setErrorString("Different open modes.");
			return false;
		}
	}

	if (mStream->open(mode))
	{
		if (0 != (mode & (Append | Truncate)))
		{
			mStreamOriginalPosition = mStream->pos();
		}

		return true;
	}

	return false;
}

bool QZStreamBase::streamSeekInit()
{
	if (!mStream->isSequential() && !mStream->seek(mStreamPosition))
	{
		mHasError = true;
		setErrorString("Stream seek failed.");

		return false;
	}

	return true;
}

QZDecompressionStream::QZDecompressionStream(QObject *parent)
	: QZStreamBase(parent)
	, mUncompressedSize(-1)
{

}

QZDecompressionStream::QZDecompressionStream(
	QIODevice *source, qint64 uncompressedSize, QObject *parent)
//
	: QZStreamBase(source, parent)
	, mUncompressedSize(uncompressedSize)
{
}

QZDecompressionStream::~QZDecompressionStream()
{
	close();
}

bool QZDecompressionStream::isSequential() const
{
	return mStream->isSequential();
}

bool QZDecompressionStream::open(OpenMode mode)
{
	if (nullptr != mStream && initOpen(mode) && check(inflateInit(&mZStream)))
	{
		bool openOk = QZStreamBase::open(mode);
		Q_ASSERT(openOk);
		Q_UNUSED(openOk);

		QObject::connect(
			mStream, &QIODevice::readyRead,
			this, &QIODevice::readyRead);
		QObject::connect(
			mStream, &QIODevice::aboutToClose,
			this, &QZDecompressionStream::close);

		return true;
	}

	return false;
}

void QZDecompressionStream::close()
{
	if (!isOpen())
	{
		return;
	}

	QObject::disconnect(
		mStream, &QIODevice::readyRead,
		this, &QIODevice::readyRead);
	QObject::disconnect(
		mStream, &QIODevice::aboutToClose,
		this, &QZDecompressionStream::close);

	QZStreamBase::close();

	mStreamPosition -= mZStream.avail_in;
	streamSeekInit();
	check(inflateEnd(&mZStream));
}

qint64 QZDecompressionStream::size() const
{
	if (!isOpen())
		return 0;

	if (mUncompressedSize >= 0)
		return mUncompressedSize;

	return bytesAvailable();
}

qint64 QZDecompressionStream::bytesAvailable() const
{
	if (!isOpen())
		return 0;

	if (mUncompressedSize >= 0)
		return qMax(mUncompressedSize - pos(), qint64(0));

	return 0xFFFFFFFF;
}

qint64 QZDecompressionStream::bytesToWrite() const
{
	return 0;
}

bool QZDecompressionStream::canReadLine() const
{
	return false;
}

qint64 QZDecompressionStream::readData(char *data, qint64 maxlen)
{
	if (seekInternal(pos()))
	{
		return readInternal(data, maxlen);
	}

	return 0;
}

bool QZDecompressionStream::initOpen(OpenMode mode)
{
	Q_ASSERT(!isOpen());
	if (0 == (mode & ReadOnly))
	{
		mHasError = true;
		setErrorString("Cannot open decompression stream whith no read mode.");
		return false;
	}

	if (!openStream(mode))
	{
		return false;
	}

	if (!mStream->isReadable())
	{
		mHasError = true;
		setErrorString("Source stream is not readable.");
		return false;
	}

	if (mStream->isTextModeEnabled())
	{
		mHasError = true;
		setErrorString("Source stream is not binary.");
		return false;
	}

	mode = ReadOnly | Unbuffered;

	mHasError = false;
	setErrorString(QString());
	mStreamPosition = mStreamOriginalPosition;
	mZStream.next_in = mBuffer;
	mZStream.avail_in = 0;

	return true;
}

bool QZDecompressionStream::seekInternal(qint64 pos)
{
	if (!isOpen())
		return false;

	if (isSequential())
		return true;

	qint64 currentPos = static_cast<qint64>(mZStream.total_out);
	currentPos -= pos;
	if (currentPos > 0)
	{
		if (!check(inflateReset(&mZStream)))
		{
			return false;
		}

		mStreamPosition = mStreamOriginalPosition;

		mZStream.next_in = mBuffer;
		mZStream.avail_in = 0;
	} else
	{
		pos = -currentPos;
	}

	while (pos > 0)
	{
		char buf[4098];

		auto blockSize = qMin(pos, qint64(sizeof(buf)));

		auto readBytes = readInternal(buf, blockSize);
		if (readBytes != blockSize)
		{
			return false;
		}
		pos -= readBytes;
	}

	return true;
}

qint64 QZDecompressionStream::readInternal(char *data, qint64 maxlen)
{
	if (!isOpen() || !streamSeekInit())
	{
		return 0;
	}

	mZStream.next_out = reinterpret_cast<Bytef *>(data);

	qint64 count = maxlen;
	quint32 blockSize = std::numeric_limits<quint32>::max();
	bool run = true;

	while (run && count > 0)
	{
		if (count < blockSize)
		{
			blockSize = static_cast<quint32>(count);
		}

		mZStream.avail_out = blockSize;

		while (mZStream.avail_out > 0)
		{
			if (mZStream.avail_in == 0)
			{
				mZStream.avail_in = static_cast<uInt>(
						mStream->read(
							reinterpret_cast<char *>(mBuffer),
							BUFFER_SIZE));

				if (mZStream.avail_in == 0)
				{
					run = false;
					break;
				}

				mZStream.next_in = mBuffer;
				mStreamPosition += mZStream.avail_in;
			}

			if (run && !check(inflate(&mZStream, Z_NO_FLUSH)))
			{
				run = false;
				break;
			}
		}

		count -= blockSize - mZStream.avail_out;
	}

	return maxlen - count;
}

qint64 QZDecompressionStream::writeData(const char *, qint64)
{
	return 0;
}

QZCompressionStream::QZCompressionStream(QObject *parent)
	: QZStreamBase(parent)
	, mCompressionLevel(Z_DEFAULT_COMPRESSION)
{

}

QZCompressionStream::QZCompressionStream(
	QIODevice *target, int compressionLevel, QObject *parent)
//
	: QZStreamBase(target, parent)
	, mCompressionLevel(compressionLevel)
{
}

QZCompressionStream::~QZCompressionStream()
{
	close();
}

bool QZCompressionStream::isSequential() const
{
	return true;
}

bool QZCompressionStream::open(OpenMode mode)
{
	if (nullptr != mStream && initOpen(mode) &&
		check(deflateInit(&mZStream, mCompressionLevel)))
	{
		bool openOk = QZStreamBase::open(mode);
		Q_ASSERT(openOk);
		Q_UNUSED(openOk);

		QObject::connect(
			mStream, &QIODevice::aboutToClose,
			this, &QZCompressionStream::close);
		return true;
	}

	return false;
}

void QZCompressionStream::close()
{
	if (!isOpen())
	{
		return;
	}

	QObject::disconnect(
		mStream, &QIODevice::aboutToClose,
		this, &QZCompressionStream::close);

	QZStreamBase::close();

	mZStream.next_in = Z_NULL;
	mZStream.avail_in = 0;
	if (streamSeekInit())
	{
		while (true)
		{
			int result = deflate(&mZStream, Z_FINISH);
			if (result == Z_STREAM_END || mZStream.avail_out != 0)
				break;

			if (!check(result))
				break;

			if (!flushBuffer())
				break;

			mZStream.next_out = mBuffer;
			mZStream.avail_out = sizeof(mBuffer);
		}
	}

	if (!mHasError && mZStream.avail_out < sizeof(mBuffer))
	{
		flushBuffer(sizeof(mBuffer) - mZStream.avail_out);
	}

	check(deflateEnd(&mZStream));
}

qint64 QZCompressionStream::size() const
{
	return isOpen() ? mZStream.total_in : 0;
}

qint64 QZCompressionStream::bytesToWrite() const
{
	return isOpen() ? mZStream.avail_out : 0;
}

qint64 QZCompressionStream::writeData(const char *data, qint64 maxlen)
{
	if (!mStream->isOpen() || !streamSeekInit())
	{
		mHasError = true;
		setErrorString("Target stream seek failed.");
		return 0;
	}

	qint64 count = maxlen;
	quint32 blockSize = std::numeric_limits<quint32>::max();

	mZStream.next_in = (Bytef *) data;

	bool run = true;
	while (run && count > 0)
	{
		if (count < blockSize)
		{
			blockSize = static_cast<quint32>(count);
		}

		mZStream.avail_in = blockSize;

		while (mZStream.avail_in > 0)
		{
			if (!check(deflate(&mZStream, Z_NO_FLUSH)))
			{
				run = false;
				break;
			}
			if (mZStream.avail_out == 0)
			{
				if (!flushBuffer())
				{
					run = false;
					break;
				}

				mStreamPosition += sizeof(mBuffer);
				mZStream.next_out = mBuffer;
				mZStream.avail_out = sizeof(mBuffer);
			}
		}

		count -= blockSize - mZStream.avail_in;
	}

	return maxlen - count;
}

bool QZCompressionStream::initOpen(OpenMode mode)
{
	Q_ASSERT(!isOpen());
	Q_ASSERT(nullptr != mStream);

	if (0 == (mode & WriteOnly))
	{
		mHasError = true;
		setErrorString("Cannot open compression stream whith no write mode.");
		return false;
	}

	if (!openStream(mode))
	{
		return false;
	}

	if (!mStream->isWritable())
	{
		mHasError = true;
		setErrorString("Target stream is not writable.");
		return false;
	}

	if (mStream->isTextModeEnabled())
	{
		mHasError = true;
		setErrorString("Target stream is not binary.");
		return false;
	}

	mode = WriteOnly | Unbuffered;

	mHasError = false;
	setErrorString(QString());

	mStreamPosition = mStreamOriginalPosition;
	mZStream.next_out = mBuffer;
	mZStream.avail_out = sizeof(mBuffer);

	return true;
}

qint64 QZCompressionStream::readData(char *, qint64)
{
	return 0;
}

qint64 QZCompressionStream::bytesAvailable() const
{
	return 0;
}

bool QZCompressionStream::flushBuffer(int size)
{
	Q_ASSERT(mStream->isOpen());
	if (mStream->write(reinterpret_cast<char *>(mBuffer), size) != size)
	{
		mHasError = true;
		setErrorString("Target stream write failed.");
		return false;
	}

	return true;
}
