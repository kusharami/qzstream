#include "QCCZStream.h"

#include "QZStream.h"

#include <QBuffer>
#include <QDataStream>

static const char CCZ_Signature[] = "CCZ!";
enum
{
	CCZ_SIGNATURE_SIZE = 4,
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

	mStreamOriginalPosition -= sizeof(CCZHeader);
}

bool QCCZDecompressionStream::initOpen(OpenMode mode)
{
	if (QZDecompressionStream::initOpen(mode))
	{
		do
		{
			if (!streamSeekInit())
				break;

			QDataStream stream(mStream);
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
			mStreamPosition += sizeof(CCZHeader);
			mStreamOriginalPosition = mStreamPosition;

			return true;
		} while (true);

		mHasError = true;
		setErrorString("No CCZ header.");
	}

	return false;
}

QCCZCompressionStream::QCCZCompressionStream(QObject *parent)
	: QZCompressionStream(parent)
	, mBytes(nullptr)
	, mBuffer(nullptr)
	, mTarget(nullptr)
	, mSavePosition(0)
{
}

QCCZCompressionStream::QCCZCompressionStream(
	QIODevice *target, int compressionLevel, QObject *parent)
	: QZCompressionStream(target, compressionLevel, parent)
	, mBytes(nullptr)
	, mBuffer(nullptr)
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
	mTarget = mStream;
	if (!QZCompressionStream::initOpen(mode))
		return false;

	mBytes = new QByteArray;
	mBuffer = new QBuffer(mBytes);
	mStream = mBuffer;
	mSavePosition = mStreamOriginalPosition;
	mStreamOriginalPosition = 0;

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

	Q_ASSERT(nullptr != mBuffer);
	Q_ASSERT(nullptr != mBytes);
	Q_ASSERT(nullptr != mTarget);

	mStream = mTarget;
	mStreamPosition = mSavePosition;
	mStreamOriginalPosition = mSavePosition;
	if (!mHasError)
	{
		bool result = false;
		do
		{
			if (!streamSeekInit())
				break;

			{
				QDataStream stream(mStream);
				stream.setByteOrder(QDataStream::BigEndian);

				CCZHeader header;
				stream.writeRawData(CCZ_Signature, CCZ_SIGNATURE_SIZE);
				header.compression_type = CCZ_COMPRESSION_ZLIB;
				stream << header.compression_type;
				header.version = CCZ_VERSION;
				stream << header.version;
				header.reserved = 0;
				stream << header.reserved;
				header.len = mZStream.total_in;
				stream << header.len;

				if (stream.status() != QDataStream::Ok)
					break;
			}

			if (mStream->write(*mBytes) != mBytes->size())
				break;

			result = true;
		} while (false);

		if (!result)
		{
			mHasError = true;
			setErrorString("Target stream write failed.");
		}
	}

	delete mBuffer;
	delete mBytes;
	mBuffer = nullptr;
	mBytes = nullptr;
}
