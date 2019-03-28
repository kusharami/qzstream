#include "QCCZImageContainerHandler.h"

#include <QVariant>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QFileDevice>
#include <QFileInfo>
#include <QBuffer>

#include "QCCZStream.h"
#undef compress
#include <set>
#include <atomic>

QCCZImageContainerHandler::QCCZImageContainerHandler()
	: mTransformations(TransformationNone)
	, mReader(nullptr)
	, mDecompressor(nullptr)
	, mWriter(nullptr)
	, mCompressor(nullptr)
	, mQuality(-1)
	, mCompressionRatio(-1)
	, mGamma(0.f)
	, mOptimizedWrite(false)
	, mProgressiveScanWrite(false)
	, mAutoTransform(false)
{
	setFormat(formatStatic());
}

QCCZImageContainerHandler::~QCCZImageContainerHandler()
{
	delete mReader;
	delete mDecompressor;
	delete mWriter;
	delete mCompressor;
}

QByteArray QCCZImageContainerHandler::formatStatic()
{
	return QByteArrayLiteral("ccz");
}

bool QCCZImageContainerHandler::canRead() const
{
	if (mReader)
		return mReader->canRead();

	return CCZ::validateHeader(device());
}

bool QCCZImageContainerHandler::read(QImage *image)
{
	if (!ensureScanned())
		return false;

	bool handlerClip = mClipRect.isValid() && mReader->supportsOption(ClipRect);
	bool handlerScale = mScaledSize.isValid() &&
		(handlerClip || !mClipRect.isValid()) &&
		mReader->supportsOption(ScaledSize);
	bool handlerScaleClip = handlerScale && mScaledClipRect.isValid() &&
		mReader->supportsOption(ScaledClipRect);

	mReader->setClipRect(handlerClip ? mClipRect : QRect());
	mReader->setScaledSize(handlerScale ? mScaledSize : QSize());
	mReader->setScaledClipRect(handlerScaleClip ? mScaledClipRect : QRect());
	mReader->setQuality(mQuality);
	mReader->setGamma(mGamma);
	mReader->setAutoTransform(false);

	bool ok = mReader->read(image);
	if (!ok)
		return false;

	if (!handlerClip && mClipRect.isValid())
	{
		*image = image->copy(mClipRect);
	}
	if (!handlerScale && mScaledSize.isValid())
	{
		*image = image->scaled(
			mScaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	if (!handlerScaleClip && (handlerScale || mScaledSize.isValid()) &&
		mScaledClipRect.isValid())
	{
		*image = image->copy(mScaledClipRect);
	}

	return true;
}

bool QCCZImageContainerHandler::write(const QImage &image)
{
	if (!ensureWritable())
		return false;

	mCompressor->setCompressionLevel(
		compressionRatioToLevel(mCompressionRatio));

	mWriter->setQuality(mQuality);
	mWriter->setGamma(mGamma);
	mWriter->setDescription(mDescription);
	mWriter->setTransformation(mTransformations);
	mWriter->setOptimizedWrite(mOptimizedWrite);
	mWriter->setProgressiveScanWrite(mProgressiveScanWrite);

	return mWriter->write(image);
}

QVariant QCCZImageContainerHandler::option(ImageOption option) const
{
	switch (option)
	{
		case Name:
			break;

		case Description:
		{
			if (!ensureScanned())
				return mDescription;

			QStringList description;
			for (const auto &key : mReader->textKeys())
			{
				description.append(
					key + QStringLiteral(": ") + mReader->text(key));
			}
			return description.join(QStringLiteral("\n\n"));
		}

		case SubType:
			return subType();

		case SupportedSubTypes:
			return QVariant::fromValue(supportedSubTypes());

		case CompressionRatio:
			return mCompressionRatio;

		case OptimizedWrite:
			return mOptimizedWrite;

		case ProgressiveScanWrite:
			return mProgressiveScanWrite;

		case Quality:
		{
			if (!ensureScanned())
				return mQuality;

			return mReader->quality();
		}

		case Animation:
		{
			if (!ensureScanned())
				break;

			return mReader->supportsAnimation();
		}

		case Endianness:
		{
			if (!ensureScanned())
				break;

			return int(
				QImage::toPixelFormat(mReader->imageFormat()).byteOrder());
		}

		case BackgroundColor:
		{
			if (!ensureScanned())
				break;

			return mReader->backgroundColor();
		}

		case IncrementalReading:
		{
			if (!ensureScanned())
				break;

			return mReader->imageCount() > 1;
		}

		case Size:
		{
			if (!ensureScanned())
				break;

			return mReader->size();
		}

		case Gamma:
		{
			if (!ensureScanned())
				return mGamma;

			return mReader->gamma();
		}

		case ImageFormat:
		{
			if (!ensureScanned())
				break;

			return mReader->imageFormat();
		}

		case ImageTransformation:
		{
			if (!ensureScanned())
				return int(mTransformations);

			if (mReader->autoTransform())
				return 0;

			return int(mReader->transformation());
		}

		case TransformedByDefault:
			return mAutoTransform;

		case ClipRect:
			return mClipRect;

		case ScaledSize:
			return mScaledSize;

		case ScaledClipRect:
			return mScaledClipRect;
	}

	return QVariant();
}

void QCCZImageContainerHandler::setOption(
	ImageOption option, const QVariant &value)
{
	switch (option)
	{
		case TransformedByDefault:
		case Name:
		case ImageFormat:
		case Size:
		case IncrementalReading:
		case Animation:
		case SupportedSubTypes:
		case Endianness:
			break;

		case Description:
		{
			mDescription = value.toString();
			break;
		}

		case SubType:
			setSubType(value.toByteArray());
			break;

		case CompressionRatio:
		{
			bool ok;
			int r = value.toInt(&ok);

			mCompressionRatio = ok ? r : -1;
			break;
		}

		case OptimizedWrite:
		{
			mOptimizedWrite = value.toBool();
			break;
		}

		case ProgressiveScanWrite:
		{
			mProgressiveScanWrite = value.toBool();
			break;
		}

		case Gamma:
		{
			mGamma = value.toFloat();
			break;
		}

		case Quality:
		{
			bool ok;
			int q = value.toInt(&ok);

			mQuality = ok ? q : -1;
			break;
		}

		case ClipRect:
		{
			mClipRect = value.toRect();
			break;
		}

		case ScaledSize:
		{
			mScaledSize = value.toSize();
			break;
		}

		case ScaledClipRect:
		{
			mScaledClipRect = value.toRect();
			break;
		}

		case BackgroundColor:
		{
			if (!ensureScanned())
				break;
			mReader->setBackgroundColor(value.value<QColor>());
			break;
		}

		case ImageTransformation:
		{
			mTransformations = Transformations(value.toInt());
			break;
		}
	}
}

bool QCCZImageContainerHandler::supportsOption(ImageOption option) const
{
	if (option == TransformedByDefault)
	{
		if (ensureScanned())
			return mAutoTransform;

		return false;
	}

	return option != Name;
}

bool QCCZImageContainerHandler::jumpToImage(int imageNumber)
{
	if (!ensureScanned())
		return false;

	return mReader->jumpToImage(imageNumber);
}

int QCCZImageContainerHandler::loopCount() const
{
	if (!ensureScanned())
		return -1;

	return mReader->loopCount();
}

bool QCCZImageContainerHandler::jumpToNextImage()
{
	if (!ensureScanned())
		return false;

	return mReader->jumpToNextImage();
}

const QList<QByteArray> &QCCZImageContainerHandler::supportedSubTypes()
{
	static QList<QByteArray> result;

	if (result.isEmpty())
	{
		std::set<QByteArray> allImageFormats;
		{
			auto formats = QImageReader::supportedImageFormats();
			allImageFormats.insert(formats.begin(), formats.end());
			formats = QImageWriter::supportedImageFormats();
			allImageFormats.insert(formats.begin(), formats.end());
		}

		int capacity = int(allImageFormats.size());
		result.reserve(capacity);

		QBuffer dummy;
		dummy.open(QIODevice::ReadOnly);
		for (const auto &format : allImageFormats)
		{
			result.append(format);
			if (format == formatStatic())
			{
				continue;
			}

			QImageWriter writer(&dummy, format);
			auto subTypes = writer.supportedSubTypes();
			if (subTypes.isEmpty())
			{
				continue;
			}

			capacity += subTypes.count();
			result.reserve(capacity);
			for (const auto &subType : subTypes)
			{
				result.append(format + '.' + subType);
			}
		}
	}

	return result;
}

QByteArray QCCZImageContainerHandler::subType() const
{
	QByteArray result;
	if (ensureScanned())
	{
		result = mReader->format();
		auto subTypeStr = mReader->subType();
		if (!subTypeStr.isEmpty())
			result += '.' + subTypeStr;
	}
	return result;
}

void QCCZImageContainerHandler::setSubType(const QByteArray &t)
{
	int i = t.indexOf('.');
	auto format = (i >= 0) ? t.left(i) : t;
	QByteArray subType;
	if (i >= 0)
		subType = t.mid(i + 1);

	mWriteFormat = format;
	mWriteSubType = subType;

	if (mWriter)
	{
		mWriter->setFormat(format);
		mWriter->setDevice(nullptr);
		mWriter->setDevice(mCompressor);
		mWriter->setSubType(subType);
	}
}

int QCCZImageContainerHandler::compressionRatioToLevel(int ratio)
{
	if (ratio < 0)
		return -1;

	return (qMin(ratio, 100) * 9) / 91;
}

bool QCCZImageContainerHandler::ensureWritable()
{
	if (!mWriter && !mCompressor)
	{
		if (mWriteFormat.isEmpty())
		{
			auto fileDevice = qobject_cast<QFileDevice *>(device());
			if (fileDevice)
			{
				auto suffix =
					QFileInfo(fileDevice->fileName()).completeSuffix();
				auto split = suffix.split('.');
				suffix.clear();
				if (!split.isEmpty())
				{
					if (split.back().toLatin1().toLower() == formatStatic())
					{
						split.removeLast();
					}
				}
				if (!split.isEmpty())
				{
					mWriteFormat = split.back().toLatin1().toLower();
				}
			}
		}
		if (!mWriteFormat.isEmpty())
		{
			static const auto writableFormats =
				QImageWriter::supportedImageFormats();
			if (writableFormats.contains(mWriteFormat))
			{
				mCompressor = new QCCZCompressor(
					device(), compressionRatioToLevel(mCompressionRatio));
				if (!mCompressor->open(QIODevice::WriteOnly))
				{
					return false;
				}

				mWriter = new QImageWriter(mCompressor, mWriteFormat);
				mWriter->setSubType(mWriteSubType);
			}
		}
	}

	return mWriter && mWriter->canWrite();
}

bool QCCZImageContainerHandler::ensureScanned() const
{
	if (!mReader)
	{
		return scanDevice();
	}

	return mReader->canRead();
}

bool QCCZImageContainerHandler::scanDevice() const
{
	if (!canRead())
		return false;

	Q_ASSERT(!mDecompressor);
	mDecompressor = new QCCZDecompressor(device());
	if (!mDecompressor->open(QIODevice::ReadOnly))
	{
		return false;
	}

	Q_ASSERT(!mReader);
	mReader = new QImageReader(mDecompressor);

	bool ok = mReader->canRead();
	mAutoTransform = ok && mReader->autoTransform();
	return ok;
}

int QCCZImageContainerHandler::imageCount() const
{
	if (!ensureScanned())
		return -1;

	return mReader->imageCount();
}

int QCCZImageContainerHandler::nextImageDelay() const
{
	if (!ensureScanned())
		return -1;

	return mReader->nextImageDelay();
}

int QCCZImageContainerHandler::currentImageNumber() const
{
	if (!ensureScanned())
		return -1;

	return mReader->currentImageNumber();
}

QRect QCCZImageContainerHandler::currentImageRect() const
{
	if (!ensureScanned())
		return QRect();

	return mReader->currentImageRect();
}
