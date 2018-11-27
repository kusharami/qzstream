#include "QZStream.h"

QZStreamBase::QZStreamBase(QObject *parent)
	: QIODevice(parent)
	, mIODevice(nullptr)
	, mIODeviceOriginalPosition(0)
	, mIODevicePosition(0)
	, mHasError(false)
{
	memset(&mZStream, 0, sizeof(mZStream));
}

QZStreamBase::QZStreamBase(QIODevice *stream, QObject *parent)
	: QZStreamBase(parent)
{
	setIODevice(stream);
}

void QZStreamBase::setIODevice(QIODevice *ioDevice)
{
	if (ioDevice != mIODevice)
	{
		close();

		mIODevice = ioDevice;
		mIODeviceOriginalPosition = ioDevice ? ioDevice->pos() : 0;
	}
}

bool QZStreamBase::waitForReadyRead(int msecs)
{
	if (isReadable())
		return mIODevice->waitForReadyRead(msecs);

	return false;
}

bool QZStreamBase::waitForBytesWritten(int msecs)
{
	if (isWritable())
		return mIODevice->waitForBytesWritten(msecs);

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

bool QZStreamBase::openIODevice(OpenMode mode)
{
	mode &= ~(QIODevice::Text | QIODevice::Unbuffered);

	if (mode & (Append | Truncate))
	{
		mode |= WriteOnly;
	}

	if (mIODevice->isOpen())
	{
		if ((mIODevice->openMode() & mode) != mode)
		{
			mHasError = true;
			setErrorString("Different open modes.");
			return false;
		}

		return true;
	}

	if (mIODevice->open(mode))
	{
		if (0 != (mode & (Append | Truncate)))
		{
			mIODeviceOriginalPosition = mIODevice->pos();
		}

		return true;
	}

	return false;
}

bool QZStreamBase::ioDeviceSeekInit()
{
	if (mIODevice->isTextModeEnabled() ||
		(!mIODevice->isSequential() && !mIODevice->seek(mIODevicePosition)))
	{
		mHasError = true;
		setErrorString("IO device seek failed.");
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
	return mIODevice->isSequential();
}

bool QZDecompressionStream::open(OpenMode mode)
{
	if (nullptr != mIODevice && initOpen(mode) && check(inflateInit(&mZStream)))
	{
		mode |= Unbuffered;
		mode &= ~(WriteOnly | Truncate | Append);
		bool openOk = QZStreamBase::open(mode);
		Q_ASSERT(openOk);
		Q_UNUSED(openOk);

		QObject::connect(
			mIODevice, &QIODevice::readyRead, this, &QIODevice::readyRead);
		QObject::connect(mIODevice, &QIODevice::aboutToClose, this,
			&QZDecompressionStream::close);

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
		mIODevice, &QIODevice::readyRead, this, &QIODevice::readyRead);
	QObject::disconnect(mIODevice, &QIODevice::aboutToClose, this,
		&QZDecompressionStream::close);

	QZStreamBase::close();

	mIODevicePosition -= mZStream.avail_in;
	ioDeviceSeekInit();
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

	if (mHasError)
		return 0;

	if (mUncompressedSize >= 0)
		return qMax(mUncompressedSize - pos(), qint64(0));

	return 0xFFFFFFFF;
}

bool QZDecompressionStream::atEnd() const
{
	return bytesAvailable() == 0;
}

qint64 QZDecompressionStream::bytesToWrite() const
{
	return 0;
}

qint64 QZDecompressionStream::readData(char *data, qint64 maxlen)
{
	if (seekInternal(pos()))
	{
		return readInternal(data, maxlen);
	}

	return -1;
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

	if (!openIODevice(mode))
	{
		return false;
	}

	if (!mIODevice->isReadable())
	{
		mHasError = true;
		setErrorString("Source IO device is not readable.");
		return false;
	}

	if (mIODevice->isTextModeEnabled())
	{
		mHasError = true;
		setErrorString("Source IO device is not binary.");
		return false;
	}

	mHasError = false;
	setErrorString(QString());
	mIODevicePosition = mIODeviceOriginalPosition;
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

		mIODevicePosition = mIODeviceOriginalPosition;

		mZStream.next_in = mBuffer;
		mZStream.avail_in = 0;
	} else
	{
		pos = -currentPos;
	}

	while (pos > 0)
	{
		char buf[4096];

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
	if (!isOpen() || !ioDeviceSeekInit())
	{
		return -1;
	}

	mZStream.next_out = reinterpret_cast<decltype(mZStream.next_out)>(data);

	qint64 count = maxlen;
	auto blockSize = std::numeric_limits<decltype(mZStream.avail_out)>::max();
	bool run = true;

	while (run && count > 0)
	{
		if (count < blockSize)
		{
			blockSize = static_cast<decltype(blockSize)>(count);
		}

		mZStream.avail_out = blockSize;

		while (mZStream.avail_out > 0)
		{
			if (mZStream.avail_in == 0)
			{
				auto readResult = mIODevice->read(
					reinterpret_cast<char *>(mBuffer), sizeof(mBuffer));
				mZStream.avail_in = readResult >= 0
					? static_cast<decltype(mZStream.avail_in)>(readResult)
					: 0;

				if (readResult <= 0)
				{
					run = false;
					mUncompressedSize = qint64(mZStream.total_out);
					if (readResult < 0)
					{
						mHasError = true;
						setErrorString(mIODevice->errorString());
					}
					break;
				}

				mZStream.next_in = mBuffer;
				mIODevicePosition += mZStream.avail_in;
			}

			int code = inflate(&mZStream, Z_NO_FLUSH);
			if (code == Z_STREAM_END || !check(code))
			{
				run = false;
				mUncompressedSize = qint64(mZStream.total_out);
				break;
			}
		}

		count -= blockSize - mZStream.avail_out;
	}

	return maxlen - count;
}

qint64 QZDecompressionStream::writeData(const char *, qint64)
{
	qWarning("QZDecompressionStream is read only!");
	return -1;
}

QZCompressionStream::QZCompressionStream(QObject *parent)
	: QZStreamBase(parent)
	, mCompressionLevel(Z_DEFAULT_COMPRESSION)
{
}

QZCompressionStream::QZCompressionStream(
	QIODevice *target, int compressionLevel, QObject *parent)
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
	if (nullptr != mIODevice && initOpen(mode) &&
		check(deflateInit(&mZStream, mCompressionLevel)))
	{
		mode |= Truncate;
		mode &= ~(ReadOnly | Append);
		bool openOk = QZStreamBase::open(mode);
		Q_ASSERT(openOk);
		Q_UNUSED(openOk);

		QObject::connect(mIODevice, &QIODevice::aboutToClose, this,
			&QZCompressionStream::close);
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
		mIODevice, &QIODevice::aboutToClose, this, &QZCompressionStream::close);

	QZStreamBase::close();

	mZStream.next_in = Z_NULL;
	mZStream.avail_in = 0;
	if (ioDeviceSeekInit())
	{
		while (true)
		{
			int result = deflate(&mZStream, Z_FINISH);
			if (!check(result))
				break;

			if (result == Z_STREAM_END)
				break;

			Q_ASSERT(mZStream.avail_out == 0);

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

bool QZCompressionStream::canReadLine() const
{
	qWarning("QZCompressionStream is write only!");
	return false;
}

qint64 QZCompressionStream::bytesToWrite() const
{
	return isOpen() ? mZStream.avail_out : 0;
}

qint64 QZCompressionStream::writeData(const char *data, qint64 maxlen)
{
	if (!mIODevice->isOpen() || !ioDeviceSeekInit())
	{
		mHasError = true;
		setErrorString("Target IO device seek failed.");
		return -1;
	}

	qint64 count = maxlen;
	auto blockSize = std::numeric_limits<decltype(mZStream.avail_in)>::max();

	mZStream.next_in = reinterpret_cast<decltype(mZStream.next_in)>(data);

	bool run = true;
	while (run && count > 0)
	{
		if (count < blockSize)
		{
			blockSize = static_cast<decltype(blockSize)>(count);
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

				mIODevicePosition += sizeof(mBuffer);
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
	Q_ASSERT(nullptr != mIODevice);

	if (0 == (mode & WriteOnly))
	{
		mHasError = true;
		setErrorString("Cannot open compression stream whith no write mode.");
		return false;
	}

	if (!openIODevice(mode))
	{
		return false;
	}

	if (!mIODevice->isWritable())
	{
		mHasError = true;
		setErrorString("Target IO device is not writable.");
		return false;
	}

	if (mIODevice->isTextModeEnabled())
	{
		mHasError = true;
		setErrorString("Target IO device is not binary.");
		return false;
	}

	mHasError = false;
	setErrorString(QString());

	mIODevicePosition = mIODeviceOriginalPosition;
	mZStream.next_out = mBuffer;
	mZStream.avail_out = sizeof(mBuffer);

	return true;
}

qint64 QZCompressionStream::readData(char *, qint64)
{
	warnWriteOnly();
	return -1;
}

qint64 QZCompressionStream::bytesAvailable() const
{
	warnWriteOnly();
	return -1;
}

bool QZCompressionStream::flushBuffer(int size)
{
	Q_ASSERT(mIODevice->isOpen());
	if (mIODevice->write(reinterpret_cast<char *>(mBuffer), size) != size)
	{
		mHasError = true;
		setErrorString(mIODevice->errorString());
		return false;
	}

	return true;
}

void QZCompressionStream::warnWriteOnly() const
{
	qWarning("QZCompressionStream is write only!");
}
