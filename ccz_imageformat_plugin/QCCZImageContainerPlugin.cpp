#include "QCCZImageContainerPlugin.h"

#include "QCCZImageContainerHandler.h"

#include <QIODevice>

QImageIOPlugin::Capabilities QCCZImageContainerPlugin::capabilities(
	QIODevice *device, const QByteArray &format) const
{
	if (format == QCCZImageContainerHandler::formatStatic())
	{
		return Capabilities(CanRead | CanWrite);
	}

	if (!format.isEmpty())
		return 0;

	if (nullptr == device || !device->isOpen())
		return 0;

	Capabilities result;

	if (device->isReadable() &&
		!QCCZImageContainerHandler::containedFormat(device).isEmpty())
	{
		result |= CanRead;
	}

	if (device->isWritable())
	{
		result |= CanWrite;
	}

	return result;
}

QImageIOHandler *QCCZImageContainerPlugin::create(
	QIODevice *device, const QByteArray &format) const
{
	auto handler = new QCCZImageContainerHandler;
	handler->setDevice(device);
	handler->setFormat(format);

	return handler;
}
