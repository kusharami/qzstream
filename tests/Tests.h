#pragma once

#include <QObject>

class QZStreamBase;

class Tests : public QObject
{
	Q_OBJECT

private slots:
	void test_data();
	void test();

private:
	enum
	{
		COMPRESS_Z,
		COMPRESS_CCZ
	};

	static QZStreamBase *newCompressor(int type);
	static QZStreamBase *newDecompressor(int type, int uncompressedSize);
};
