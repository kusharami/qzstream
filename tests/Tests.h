#pragma once

#include <QObject>

class Tests : public QObject
{
	Q_OBJECT

private slots:
	void test_data();
	void test();
};
