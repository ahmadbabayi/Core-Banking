#include <QCoreApplication>
#include <QDebug>

#include "infrastructure/database.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (!Database::instance().connect())
    {
        qDebug() << "Application cannot start.";

        return 1;
    }

    qDebug() << "Core Banking started.";

    return app.exec();
}
