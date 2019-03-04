#include "QCCZStream.h"

#include "QZStream.h"

#include <QBuffer>
#include <QDataStream>

static const char CCZ_Signature[] = "CCZ!";

enum
{
	CCZ_SIGNATURE_SIZE = sizeof(CCZ_Signature) - 1,
	CCZ_VERSION = 2,
	CCZ_COMPRESSION_ZLIB = 0
};

// Format header
struct CCZHeader
{
	char sig[CCZ_SIGNATURE_SIZE]; // signature. Should be 'CCZ!' 4 bytes
	quint16 compression_type; // should 0 (See above for supported formats)
	quint16 version; // should be 2
	quint32 reserved; // Reserved for users
	quint32 len; // size of the uncompressed file

	bool readFrom(QIODevice *device);
};

namespace CCZ
{
bool validateHeader(QIODevice *device)
{
	if (!device || !device->isReadable())
		return false;

	device->startTransaction();
	CCZHeader header;
	bool ok = header.readFrom(device);
	device->rollbackTransaction();

	return ok;
}
}

QCCZDecompressor::QCCZDecompressor(QObject *parent)
	: QZDecompressor(parent)
{
}

QCCZDecompressor::QCCZDecompressor(QIODevice *source, QObject *parent)
	: QZDecompressor(source, -1, parent)
{
}

QCCZDecompressor::~QCCZDecompressor()
{
	close();
}

void QCCZDecompressor::close()
{
	if (!isOpen())
		return;

	QZDecompressor::close();

	mIODeviceOriginalPosition -= sizeof(CCZHeader);
}

bool QCCZDecompressor::initOpen(OpenMode mode)
{
	if (QZDecompressor::initOpen(mode))
	{
		do
		{
			if (!ioDeviceSeekInit())
				break;

			CCZHeader header;
			if (!header.readFrom(mIODevice))
				break;

			setUncompressedSize(header.len);

			mIODevicePosition += sizeof(CCZHeader);
			mIODeviceOriginalPosition = mIODevicePosition;

			return true;
		} while (true);

		mHasError = true;
		setErrorString("Bad CCZ header.");
	}

	return false;
}

QCCZCompressor::QCCZCompressor(QObject *parent)
	: QZCompressor(parent)
	, mBytes(nullptr)
	, mCCZBuffer(nullptr)
	, mTarget(nullptr)
	, mSavePosition(0)
{
}

QCCZCompressor::QCCZCompressor(
	QIODevice *target, int compressionLevel, QObject *parent)
	: QZCompressor(target, compressionLevel, parent)
	, mBytes(nullptr)
	, mCCZBuffer(nullptr)
	, mTarget(nullptr)
	, mSavePosition(0)
{
	mCompressionLevel = compressionLevel;
}

QCCZCompressor::~QCCZCompressor()
{
	close();
}

bool QCCZCompressor::open(OpenMode mode)
{
	bool ok = QZCompressor::open(mode);
	if (ok)
	{
		QObject::connect(
			mTarget, &QIODevice::aboutToClose, this, &QCCZCompressor::close);
	} else
	{
		delete mCCZBuffer;
		delete mBytes;
		mCCZBuffer = nullptr;
		mBytes = nullptr;
	}

	return ok;
}

bool QCCZCompressor::initOpen(OpenMode mode)
{
	mTarget = mIODevice;
	if (!QZCompressor::initOpen(mode))
		return false;

	mBytes = new QByteArray;
	mCCZBuffer = new QBuffer(mBytes);
	mIODevice = mCCZBuffer;
	mSavePosition = mIODeviceOriginalPosition;
	mIODeviceOriginalPosition = 0;

	bool bufferOpenOk = QZCompressor::initOpen(mode);
	Q_ASSERT(bufferOpenOk);
	Q_UNUSED(bufferOpenOk);

	return true;
}

void QCCZCompressor::close()
{
	if (!isOpen())
		return;

	QObject::disconnect(
		mTarget, &QIODevice::aboutToClose, this, &QCCZCompressor::close);

	QZCompressor::close();

	Q_ASSERT(nullptr != mCCZBuffer);
	Q_ASSERT(nullptr != mBytes);
	Q_ASSERT(nullptr != mTarget);

	mIODevice = mTarget;
	mIODevicePosition = mSavePosition;
	mIODeviceOriginalPosition = mSavePosition;
	if (!mHasError)
	{
		bool result = false;
		do
		{
			if (!ioDeviceSeekInit())
				break;

			if (mZStream.total_in > std::numeric_limits<quint32>::max())
				break;

			{
				QDataStream stream(mIODevice);
				stream.setByteOrder(QDataStream::BigEndian);

				CCZHeader header;
				stream.writeRawData(CCZ_Signature, CCZ_SIGNATURE_SIZE);
				header.compression_type = CCZ_COMPRESSION_ZLIB;
				stream << header.compression_type;
				header.version = CCZ_VERSION;
				stream << header.version;
				header.reserved = 0;
				stream << header.reserved;
				header.len = quint32(mZStream.total_in);
				stream << header.len;

				if (stream.status() != QDataStream::Ok)
					break;
			}

			if (mIODevice->write(*mBytes) != mBytes->size())
				break;

			result = true;
		} while (false);

		if (!result)
		{
			mHasError = true;
			setErrorString("Target stream write failed.");
		}
	}

	delete mCCZBuffer;
	delete mBytes;
	mCCZBuffer = nullptr;
	mBytes = nullptr;
	flushToFile();
}

bool CCZHeader::readFrom(QIODevice *device)
{
	QDataStream stream(device);
	stream.setByteOrder(QDataStream::BigEndian);
	stream.readRawData(sig, CCZ_SIGNATURE_SIZE);
	if (0 != memcmp(sig, CCZ_Signature, CCZ_SIGNATURE_SIZE))
		return false;

	stream >> compression_type;

	if (CCZ_COMPRESSION_ZLIB != compression_type)
		return false;

	stream >> version;
	if (CCZ_VERSION != version)
		return false;

	stream >> reserved;
	stream >> len;

	return stream.status() == QDataStream::Ok;
}
