#include "QZStream.h"
#include <QFileDevice>

QZStream::QZStream(QObject *parent)
	: QIODevice(parent)
	, mIODevice(nullptr)
	, mIODeviceOriginalPosition(0)
	, mIODevicePosition(0)
	, mBuffer(new Bytef[BUFFER_SIZE])
	, mHasError(false)
{
	memset(&mZStream, 0, sizeof(mZStream));
}

QZStream::QZStream(QIODevice *stream, QObject *parent)
	: QZStream(parent)
{
	setIODevice(stream);
}

void QZStream::setIODevice(QIODevice *ioDevice)
{
	if (ioDevice != mIODevice)
	{
		close();

		mIODevice = ioDevice;
		mIODeviceOriginalPosition = ioDevice ? ioDevice->pos() : 0;
	}
}

bool QZStream::waitForReadyRead(int msecs)
{
	if (isReadable())
		return mIODevice->waitForReadyRead(msecs);

	return false;
}

bool QZStream::waitForBytesWritten(int msecs)
{
	if (isWritable())
		return mIODevice->waitForBytesWritten(msecs);

	return false;
}

bool QZStream::check(int code)
{
	if (code < 0)
	{
		mHasError = true;
		setErrorString(QLatin1String(mZStream.msg));
	}

	return !mHasError;
}

bool QZStream::openIODevice(OpenMode mode)
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

bool QZStream::ioDeviceSeekInit()
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

QZDecompressor::QZDecompressor(QObject *parent)
	: QZStream(parent)
	, mUncompressedSize(-1)
{
}

QZDecompressor::QZDecompressor(
	QIODevice *source, qint64 uncompressedSize, QObject *parent)
	: QZStream(source, parent)
	, mUncompressedSize(uncompressedSize)
{
}

QZDecompressor::~QZDecompressor()
{
	QZDecompressor::close();
}

bool QZDecompressor::isSequential() const
{
	return mIODevice->isSequential();
}

bool QZDecompressor::open(OpenMode mode)
{
	if (nullptr != mIODevice && initOpen(mode) && check(inflateInit(&mZStream)))
	{
		mode |= Unbuffered;
		mode &= ~(WriteOnly | Truncate | Append);
		bool openOk = QZStream::open(mode);
		Q_ASSERT(openOk);
		Q_UNUSED(openOk);

		QObject::connect(
			mIODevice, &QIODevice::readyRead, this, &QIODevice::readyRead);
		QObject::connect(
			mIODevice, &QIODevice::aboutToClose, this, &QZDecompressor::close);

		return true;
	}

	return false;
}

void QZDecompressor::close()
{
	if (!isOpen())
	{
		return;
	}

	QObject::disconnect(
		mIODevice, &QIODevice::readyRead, this, &QIODevice::readyRead);
	QObject::disconnect(
		mIODevice, &QIODevice::aboutToClose, this, &QZDecompressor::close);

	QZStream::close();

	mIODevicePosition -= mZStream.avail_in;
	ioDeviceSeekInit();
	check(inflateEnd(&mZStream));
}

qint64 QZDecompressor::size() const
{
	if (!isOpen())
		return 0;

	if (mUncompressedSize >= 0)
		return mUncompressedSize;

	return bytesAvailable();
}

qint64 QZDecompressor::bytesAvailable() const
{
	if (!isOpen())
		return 0;

	if (mHasError)
		return 0;

	if (mUncompressedSize >= 0)
		return qMax(mUncompressedSize - pos(), qint64(0));

	if (sizeof(qint64) == sizeof(uLong))
		return std::numeric_limits<qint64>::max();
	return qint64(std::numeric_limits<uLong>::max());
}

bool QZDecompressor::atEnd() const
{
	return bytesAvailable() == 0;
}

qint64 QZDecompressor::bytesToWrite() const
{
	return 0;
}

qint64 QZDecompressor::readData(char *data, qint64 maxlen)
{
	if (seekInternal(pos()))
	{
		return readInternal(data, maxlen);
	}

	return -1;
}

bool QZDecompressor::initOpen(OpenMode mode)
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
	mZStream.next_in = mBuffer.get();
	mZStream.avail_in = 0;

	return true;
}

bool QZDecompressor::seekInternal(qint64 pos)
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

		mZStream.next_in = mBuffer.get();
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

qint64 QZDecompressor::readInternal(char *data, qint64 maxlen)
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
					reinterpret_cast<char *>(mBuffer.get()), BUFFER_SIZE);
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

				mZStream.next_in = mBuffer.get();
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

qint64 QZDecompressor::writeData(const char *, qint64)
{
	qWarning("QZDecompressionStream is read only!");
	return -1;
}

QZCompressor::QZCompressor(QObject *parent)
	: QZStream(parent)
	, mCompressionLevel(Z_DEFAULT_COMPRESSION)
{
}

QZCompressor::QZCompressor(
	QIODevice *target, int compressionLevel, QObject *parent)
	: QZStream(target, parent)
	, mCompressionLevel(compressionLevel)
{
}

QZCompressor::~QZCompressor()
{
	QZCompressor::close();
}

bool QZCompressor::isSequential() const
{
	return true;
}

bool QZCompressor::open(OpenMode mode)
{
	if (nullptr != mIODevice && initOpen(mode) &&
		check(deflateInit(&mZStream, mCompressionLevel)))
	{
		mode |= Truncate;
		mode &= ~(ReadOnly | Append);
		bool openOk = QZStream::open(mode);
		Q_ASSERT(openOk);
		Q_UNUSED(openOk);

		QObject::connect(
			mIODevice, &QIODevice::aboutToClose, this, &QZCompressor::close);
		return true;
	}

	return false;
}

void QZCompressor::flushToFile()
{
	if (!mHasError)
	{
		auto fileDevice = qobject_cast<QFileDevice *>(mIODevice);
		if (fileDevice)
		{
			if (!fileDevice->flush())
			{
				mHasError = true;
				setErrorString(fileDevice->errorString());
			}
		}
	}
}

void QZCompressor::close()
{
	if (!isOpen())
	{
		return;
	}

	QObject::disconnect(
		mIODevice, &QIODevice::aboutToClose, this, &QZCompressor::close);

	QZStream::close();

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

			mZStream.next_out = mBuffer.get();
			mZStream.avail_out = uInt(BUFFER_SIZE);
		}
	}

	if (!mHasError && mZStream.avail_out < BUFFER_SIZE)
	{
		flushBuffer(int(BUFFER_SIZE - mZStream.avail_out));
	}

	check(deflateEnd(&mZStream));
	flushToFile();
}

qint64 QZCompressor::size() const
{
	return isOpen() ? mZStream.total_in : 0;
}

bool QZCompressor::canReadLine() const
{
	qWarning("QZCompressionStream is write only!");
	return false;
}

qint64 QZCompressor::bytesToWrite() const
{
	return isOpen() ? mZStream.avail_out : 0;
}

qint64 QZCompressor::writeData(const char *data, qint64 maxlen)
{
	if (!mIODevice->isOpen() || !ioDeviceSeekInit())
	{
		mHasError = true;
		setErrorString("Target IO device seek failed.");
		return -1;
	}

	qint64 count = maxlen;
	auto blockSize = std::numeric_limits<decltype(mZStream.avail_in)>::max();

	mZStream.next_in =
		reinterpret_cast<decltype(mZStream.next_in)>(const_cast<char *>(data));

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

				mIODevicePosition += BUFFER_SIZE;
				mZStream.next_out = mBuffer.get();
				mZStream.avail_out = uInt(BUFFER_SIZE);
			}
		}

		count -= blockSize - mZStream.avail_in;
	}

	return maxlen - count;
}

bool QZCompressor::initOpen(OpenMode mode)
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
	mZStream.next_out = mBuffer.get();
	mZStream.avail_out = uInt(BUFFER_SIZE);

	return true;
}

qint64 QZCompressor::readData(char *, qint64)
{
	warnWriteOnly();
	return -1;
}

qint64 QZCompressor::bytesAvailable() const
{
	warnWriteOnly();
	return -1;
}

bool QZCompressor::flushBuffer(int size)
{
	Q_ASSERT(mIODevice->isOpen());
	if (mIODevice->write(reinterpret_cast<char *>(mBuffer.get()), size) != size)
	{
		mHasError = true;
		setErrorString(mIODevice->errorString());
		return false;
	}

	return true;
}

void QZCompressor::warnWriteOnly() const
{
	qWarning("QZCompressionStream is write only!");
}

void QZCompressor::setCompressionLevel(int level)
{
	if (mCompressionLevel == level)
		return;

	mCompressionLevel = level;
	if (!isOpen())
		return;

	check(deflateParams(&mZStream, mCompressionLevel, Z_DEFAULT_STRATEGY));
}
