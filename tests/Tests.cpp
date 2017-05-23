#include "Tests.h"

#include "QZStream.h"
#include "QCCZStream.h"

#undef compress

#include <QBuffer>
#include <QtTest>

#define QADD_COLUMN(type, name) \
	QTest::addColumn<type>(#name);

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
		auto compress = newCompressor(compressionType);
		QVERIFY(nullptr != compress);
		compress->setStream(&buffer);
		QVERIFY(compress->open(QIODevice::WriteOnly));
		QVERIFY(compress->isWritable() == buffer.isWritable());
		QVERIFY(compress->isReadable() == buffer.isReadable());
		QVERIFY(!compress->isReadable());
		QVERIFY(compress->isWritable());
		QVERIFY(compress->size() == 0);
		QVERIFY(compress->write(sourceBytes) == sourceBytes.size());
		QVERIFY(compress->size() == sourceBytes.size());
		QVERIFY(!compress->seek(sourceBytes.size()));
		QVERIFY(!compress->seek(0));
		compress->close();
		QVERIFY(!compress->hasError());
		QVERIFY(buffer.isOpen());
		delete compress;
	}

	{
		QBuffer buffer(&bytes);
		auto decompress = newDecompressor(
				compressionType, sourceBytes.size());
		QVERIFY(nullptr != decompress);
		decompress->setStream(&buffer);
		QVERIFY(decompress->open(QIODevice::ReadOnly));
		QVERIFY(decompress->isWritable() == buffer.isWritable());
		QVERIFY(decompress->isReadable() == buffer.isReadable());
		QVERIFY(decompress->isReadable());
		QVERIFY(!decompress->isWritable());
		QVERIFY(decompress->pos() == 0);
		QVERIFY(decompress->peek(sourceBytes.size()) == sourceBytes);
		QVERIFY(decompress->pos() == 0);
		QVERIFY(decompress->readAll() == sourceBytes);
		QVERIFY(decompress->pos() == sourceBytes.size());
		QVERIFY(decompress->seek(1));
		QVERIFY(decompress->pos() == 1);
		auto uncompressed = decompress->readAll();
		QVERIFY(
			0 == memcmp(
				uncompressed.data(),
				&sourceBytes.data()[1],
				uncompressed.size()));

		QVERIFY(decompress->pos() == sourceBytes.size());
		QVERIFY(decompress->seek(sourceBytes.size()));
		QVERIFY(!decompress->seek(sourceBytes.size() + 1));
		QVERIFY(decompress->pos() == sourceBytes.size());

		decompress->close();
		QVERIFY(!decompress->hasError());
		QVERIFY(buffer.isOpen());
		delete decompress;
	}
}

QZStreamBase *Tests::newCompressor(int type)
{
	switch (type)
	{
		case COMPRESS_Z:
			return new QZCompressionStream(nullptr, Z_BEST_COMPRESSION);

		case COMPRESS_CCZ:
			return new QCCZCompressionStream(nullptr, Z_BEST_COMPRESSION);
	}

	return nullptr;
}

QZStreamBase *Tests::newDecompressor(int type, int uncompressedSize)
{
	switch (type)
	{
		case COMPRESS_Z:
			return new QZDecompressionStream(nullptr, uncompressedSize);

		case COMPRESS_CCZ:
			return new QCCZDecompressionStream;
	}

	return nullptr;
}
