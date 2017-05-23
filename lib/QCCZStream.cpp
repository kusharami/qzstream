#include "QCCZStream.h"

#include "QZStream.h"

#include <QBuffer>

#ifdef Q_OS_WIN
#include <WinSock2.h>
#endif

static const char CCZ_Signature[] = "CCZ!";
enum
{
	CCZ_SIGNATURE_SIZE = 4,
	CCZ_VERSION = 2
};

// Format header
struct CCZHeader
{
	quint8 sig[CCZ_SIGNATURE_SIZE];	// signature. Should be 'CCZ!' 4 bytes
	quint16 compression_type;	// should 0 (See below for supported formats)
	quint16 version;	// should be 2
	quint32 reserved;	// Reserverd for users.
	quint32 len;	// size of the uncompressed file
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
		CCZHeader header;
		if (mStream->seek(mStreamPosition) &&
			mStream->read(
				reinterpret_cast<char *>(&header),
				sizeof(CCZHeader)) == sizeof(CCZHeader) &&
			0 == memcmp(header.sig, CCZ_Signature, CCZ_SIGNATURE_SIZE) &&
			ntohs(header.version) == CCZ_VERSION &&
			ntohs(header.compression_type) == CCZ_COMPRESSION_ZLIB)
		{
			setUncompressedSize(ntohl(header.len));
			mStreamPosition += sizeof(CCZHeader);
			mStreamOriginalPosition = mStreamPosition;

			return true;
		}

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
//
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
	mStreamOriginalPosition = mSavePosition;
	if (!mHasError)
	{
		CCZHeader header;
		memcpy(header.sig, CCZ_Signature, CCZ_SIGNATURE_SIZE);
		header.version = htons(CCZ_VERSION);
		header.compression_type = htons(CCZ_COMPRESSION_ZLIB);
		header.reserved = 0;
		header.len = htonl(mZStream.total_in);

		mStreamPosition = mStreamOriginalPosition;
		if (!streamSeekInit() ||
			mStream->write(
				reinterpret_cast<const char *>(&header),
				sizeof(header)) != sizeof(header) ||
			mStream->write(*mBytes) != mBytes->size())
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
