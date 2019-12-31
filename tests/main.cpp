#include <QDebug>

#include <QtTest>
#include "Tests.h"

#ifdef CCZ_IMAGEFORMAT_STATIC
#include <QtPlugin>
Q_IMPORT_PLUGIN(QCCZImageContainerPlugin)
#endif

int main(int argc, char *argv[])
{
	qDebug() << "Init tests...";

	QTEST_SET_MAIN_SOURCE_PATH

	Tests tests;

	return QTest::qExec(&tests, argc, argv);
}
