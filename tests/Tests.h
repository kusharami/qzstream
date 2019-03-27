#pragma once

#include <QObject>

class QZStream;

class Tests : public QObject
{
	Q_OBJECT

private slots:
	void test_data();
	void test();
	void testImageFormatPluginInit();
	void testImageFormatPluginFileSaveLoad();
	void testImageFormatPluginFileReadWrite();
	void testImageFormatPluginBufferReadWrite();

private:
	enum
	{
		COMPRESS_Z,
		COMPRESS_CCZ
	};

	static QZStream *newCompressor(int type);
	static QZStream *newDecompressor(int type, int uncompressedSize);
	static const QImage &testImage();
};
