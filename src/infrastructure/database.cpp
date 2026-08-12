#include "database.h"

#include <QDebug>
#include <QSqlError>

Database& Database::instance()
{
    static Database database;
    return database;
}

Database::Database()
{
}

Database::~Database()
{
    if (m_database.isOpen())
    {
        m_database.close();
    }
}

bool Database::connect()
{
    if (m_database.isOpen())
    {
        return true;
    }

    m_database = QSqlDatabase::addDatabase("QPSQL");

    m_database.setHostName("localhost");
    m_database.setPort(5432);
    m_database.setDatabaseName("corebanking");
    m_database.setUserName("corebanking");
    m_database.setPassword("CoreBanking123");

    if (!m_database.open())
    {
        qDebug() << "Database connection failed!";
        qDebug() << m_database.lastError().text();

        return false;
    }

    qDebug() << "Database connected successfully!";

    return true;
}

QSqlDatabase Database::connection()
{
    return m_database;
}
