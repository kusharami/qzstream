#include "QCCZStream.h"

#include "QZStream.h"

#include <QBuffer>
#include <QDataStream>

static const char CCZ_Signature[] = "CCZ!";
enum
{
	CCZ_SIGNATURE_SIZE = sizeof(CCZ_Signature) - 1,
	CCZ_VERSION = 2
};

// Format header
struct CCZHeader
{
	char sig[CCZ_SIGNATURE_SIZE]; // signature. Should be 'CCZ!' 4 bytes
	quint16 compression_type; // should 0 (See below for supported formats)
	quint16 version; // should be 2
	quint32 reserved; // Reserverd for users.
	quint32 len; // size of the uncompressed file
};

enum
{
	CCZ_COMPRESSION_ZLIB = 0
};

QCCZDecompressionStream::QCCZDecompressionStream(QObject *parent)
	: QZDecompressionStream(parent)
{
}

QCCZDecompressionStream::QCCZDecompressionStream(
	QIODevice *source, QObject *parent)
	: QZDecompressionStream(source, -1, parent)
{
}

QCCZDecompressionStream::~QCCZDecompressionStream()
{
	close();
}

void QCCZDecompressionStream::close()
{
	if (!isOpen())
		return;

	QZDecompressionStream::close();

	mIODeviceOriginalPosition -= sizeof(CCZHeader);
}

bool QCCZDecompressionStream::initOpen(OpenMode mode)
{
	if (QZDecompressionStream::initOpen(mode))
	{
		do
		{
			if (!ioDeviceSeekInit())
				break;

			QDataStream stream(mIODevice);
			stream.setByteOrder(QDataStream::BigEndian);
			CCZHeader header;
			stream.readRawData(header.sig, CCZ_SIGNATURE_SIZE);
			if (0 != memcmp(header.sig, CCZ_Signature, CCZ_SIGNATURE_SIZE))
				break;

			stream >> header.compression_type;

			if (CCZ_COMPRESSION_ZLIB != header.compression_type)
				break;

			stream >> header.version;
			if (CCZ_VERSION != header.version)
				break;

			stream >> header.reserved;
			stream >> header.len;

			if (stream.status() != QDataStream::Ok)
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

QCCZCompressionStream::QCCZCompressionStream(QObject *parent)
	: QZCompressionStream(parent)
	, mBytes(nullptr)
	, mCCZBuffer(nullptr)
	, mTarget(nullptr)
	, mSavePosition(0)
{
}

QCCZCompressionStream::QCCZCompressionStream(
	QIODevice *target, int compressionLevel, QObject *parent)
	: QZCompressionStream(target, compressionLevel, parent)
	, mBytes(nullptr)
	, mCCZBuffer(nullptr)
	, mTarget(nullptr)
	, mSavePosition(0)
{
	mCompressionLevel = compressionLevel;
}

QCCZCompressionStream::~QCCZCompressionStream()
{
	close();
}

bool QCCZCompressionStream::initOpen(OpenMode mode)
{
	mTarget = mIODevice;
	if (!QZCompressionStream::initOpen(mode))
		return false;

	mBytes = new QByteArray;
	mCCZBuffer = new QBuffer(mBytes);
	mIODevice = mCCZBuffer;
	mSavePosition = mIODeviceOriginalPosition;
	mIODeviceOriginalPosition = 0;

	bool bufferOpenOk = QZCompressionStream::initOpen(mode);
	Q_ASSERT(bufferOpenOk);
	Q_UNUSED(bufferOpenOk);

	return true;
}

void QCCZCompressionStream::close()
{
	if (!isOpen())
		return;

	QZCompressionStream::close();

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
}
