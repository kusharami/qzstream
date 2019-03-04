﻿#include "Tests.h"

#include "QZStream.h"
#include "QCCZStream.h"

#undef compress

#include <QBuffer>
#include <QImageReader>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QtTest>

#define QADD_COLUMN(type, name) QTest::addColumn<type>(#name);

void Tests::test_data()
{
	QADD_COLUMN(QByteArray, sourceBytes);
	QADD_COLUMN(int, compressionType);

	QByteArray empty(256, 0);
	QByteArray random(512, Qt::Uninitialized);

	QTest::newRow("empty_z") << empty << (int) COMPRESS_Z;
	QTest::newRow("random_z") << random << (int) COMPRESS_Z;
	QTest::newRow("empty_ccz") << empty << (int) COMPRESS_CCZ;
	QTest::newRow("random_ccz") << random << (int) COMPRESS_CCZ;
}

void Tests::test()
{
	QFETCH(QByteArray, sourceBytes);
	QFETCH(int, compressionType);

	QByteArray bytes;

	{
		QBuffer buffer(&bytes);
		QScopedPointer<QZStream> compress(newCompressor(compressionType));
		QVERIFY(nullptr != compress);
		compress->setIODevice(&buffer);
		QVERIFY(compress->open(QIODevice::WriteOnly));
		QVERIFY(compress->isWritable() == buffer.isWritable());
		QVERIFY(compress->isReadable() == buffer.isReadable());
		QVERIFY(!compress->isReadable());
		QVERIFY(compress->isWritable());
		QCOMPARE(compress->size(), 0);
		QCOMPARE(compress->write(sourceBytes), sourceBytes.size());
		QCOMPARE(compress->size(), sourceBytes.size());
		QRegularExpression seekRE("QIODevice::seek");
		QTest::ignoreMessage(QtWarningMsg, seekRE);
		QVERIFY(!compress->seek(sourceBytes.size()));
		QTest::ignoreMessage(QtWarningMsg, seekRE);
		QVERIFY(!compress->seek(0));
		compress->close();
		QVERIFY(!compress->hasError());
		QVERIFY(buffer.isOpen());
	}

	{
		QBuffer buffer(&bytes);
		QScopedPointer<QZStream> decompress(
			newDecompressor(compressionType, sourceBytes.size()));
		QVERIFY(nullptr != decompress);
		decompress->setIODevice(&buffer);
		QVERIFY(decompress->open(QIODevice::ReadOnly));
		QVERIFY(decompress->isWritable() == buffer.isWritable());
		QVERIFY(decompress->isReadable() == buffer.isReadable());
		QVERIFY(decompress->isReadable());
		QVERIFY(!decompress->isWritable());
		QCOMPARE(decompress->pos(), 0);
		QCOMPARE(decompress->peek(sourceBytes.size()), sourceBytes);
		QCOMPARE(decompress->pos(), 0);
		QCOMPARE(decompress->readAll(), sourceBytes);
		QCOMPARE(decompress->pos(), sourceBytes.size());
		QVERIFY(decompress->seek(1));
		QCOMPARE(decompress->pos(), 1);
		auto uncompressed = decompress->readAll();
		QCOMPARE(uncompressed,
			QByteArray::fromRawData(
				&sourceBytes.data()[1], uncompressed.size()));

		QCOMPARE(decompress->pos(), sourceBytes.size());
		QVERIFY(decompress->seek(0));
		QVERIFY(decompress->seek(sourceBytes.size() + 1));
		QCOMPARE(decompress->pos(), sourceBytes.size() + 1);
		QRegularExpression readRE("QIODevice::read");
		QTest::ignoreMessage(QtWarningMsg, readRE);
		QVERIFY(decompress->readAll().isEmpty());

		decompress->close();
		QVERIFY(!decompress->hasError());
		QVERIFY(buffer.isOpen());
	}
}

QZStream *Tests::newCompressor(int type)
{
	switch (type)
	{
		case COMPRESS_Z:
			return new QZCompressor(nullptr, Z_BEST_COMPRESSION);

		case COMPRESS_CCZ:
			return new QCCZCompressor(nullptr, Z_BEST_COMPRESSION);
	}

	return nullptr;
}

QZStream *Tests::newDecompressor(int type, int uncompressedSize)
{
	switch (type)
	{
		case COMPRESS_Z:
			return new QZDecompressor(nullptr, uncompressedSize);

		case COMPRESS_CCZ:
			return new QCCZDecompressor;
	}

	return nullptr;
}

void Tests::testImageFormatPlugin()
{
	QList<QList<QByteArray>> supported;
	supported.append(QImageReader::supportedImageFormats());
	supported.append(QImageWriter::supportedImageFormats());

	std::set<QByteArray> allFormats;
	for (auto &s : supported)
	{
		QVERIFY(s.indexOf("ccz") >= 0);
		allFormats.insert(s.begin(), s.end());
	}

	{
		QImageWriter writer;
		writer.setFormat("ccz");
		for (auto &format : allFormats)
		{
			if (format == "ccz")
				continue;
			QVERIFY(writer.supportedSubTypes().indexOf(format) >= 0);
		}
	}

	QImage test(16, 16, QImage::Format_RGBA8888);
	test.fill(Qt::green);

	QTemporaryDir dir;

	auto cczBmpFilePath = dir.filePath("test.bmp.ccz");
	{
		QVERIFY(test.save(cczBmpFilePath));
		QImage loaded(cczBmpFilePath);
		QCOMPARE(loaded.size(), QSize(16, 16));
	}

	auto cczPngFilePath = dir.filePath("test.png.ccz");
	{
		QImageWriter writer(cczPngFilePath);
		QVERIFY(writer.canWrite());
		QVERIFY(writer.write(test));
	}
	{
		QImageReader reader(cczPngFilePath);
		QVERIFY(reader.canRead());
		QCOMPARE(reader.subType(), QByteArrayLiteral("png"));
		auto loaded = reader.read();
		QCOMPARE(loaded.size(), QSize(16, 16));
	}

	QBuffer buffer;
	{
		buffer.open(QBuffer::WriteOnly);
		QImageWriter writer(&buffer, "ccz");
		writer.setCompression(70);
		writer.setSubType("jpg");
		QVERIFY(writer.canWrite());
		QVERIFY(writer.write(test));
		buffer.close();
	}
	{
		buffer.open(QBuffer::ReadOnly);
		QImageReader reader(&buffer);
		QCOMPARE(reader.format(), QByteArrayLiteral("ccz"));
		QVERIFY(reader.subType().startsWith("jpeg"));
		auto loaded = reader.read();
		QCOMPARE(loaded.size(), QSize(16, 16));
		buffer.close();
	}
}
