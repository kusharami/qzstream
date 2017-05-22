#include "Tests.h"

#include "QZStream.h"

#undef compress

#include <QBuffer>
#include <QtTest>

#define QADD_COLUMN(type, name) \
	QTest::addColumn<type>(#name);

void Tests::test_data()
{
	QADD_COLUMN(QByteArray, sourceBytes);

	QTest::newRow("empty") << QByteArray(256, 0);
	QTest::newRow("random") << QByteArray(512, Qt::Uninitialized);
}

void Tests::test()
{
	QFETCH(QByteArray, sourceBytes);

	QByteArray bytes;

	{
		QBuffer buffer(&bytes);
		QZCompressionStream compress(&buffer, Z_BEST_COMPRESSION);
		QVERIFY(compress.open());
		auto openMode1 = compress.openMode() & ~QIODevice::Unbuffered;
		auto openMode2 = buffer.openMode() & ~QIODevice::Unbuffered;
		QVERIFY(openMode1 == openMode2);
		QVERIFY(!compress.isReadable());
		QVERIFY(compress.isWritable());
		QVERIFY(compress.pos() == 0);
		QVERIFY(compress.write(sourceBytes) == sourceBytes.size());
		QVERIFY(compress.pos() == sourceBytes.size());
		QVERIFY(compress.seek(sourceBytes.size()));
		QVERIFY(!compress.seek(0));
		compress.close();
		QVERIFY(!compress.hasError());
		QVERIFY(buffer.isOpen());
	}

	{
		QBuffer buffer(&bytes);
		QZDecompressionStream decompress(&buffer, sourceBytes.size());
		QVERIFY(decompress.open());
		auto openMode1 = decompress.openMode() & ~QIODevice::Unbuffered;
		auto openMode2 = buffer.openMode() & ~QIODevice::Unbuffered;
		QVERIFY(openMode1 == openMode2);
		QVERIFY(decompress.isReadable());
		QVERIFY(!decompress.isWritable());

		QVERIFY(decompress.readAll() == sourceBytes);
		QVERIFY(decompress.seek(1));
		auto uncompressed = decompress.readAll();
		QVERIFY(
			0 == memcmp(
				uncompressed.data(),
				&sourceBytes.data()[1],
				uncompressed.size()));

		QVERIFY(decompress.seek(sourceBytes.size()));
		QVERIFY(!decompress.seek(sourceBytes.size() + 1));

		decompress.close();
		QVERIFY(!decompress.hasError());
		QVERIFY(buffer.isOpen());
	}
}
