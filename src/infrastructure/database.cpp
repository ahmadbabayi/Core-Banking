#include "database.h"

#include <QDebug>
#include <QSqlError>

namespace
{
const QString CONNECTION_NAME = "CoreBankingConnection";
}

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
    // Already connected
    if (m_database.isValid() && m_database.isOpen())
    {
        return true;
    }

    // Reuse existing Qt SQL connection if it already exists
    if (QSqlDatabase::contains(CONNECTION_NAME))
    {
        m_database = QSqlDatabase::database(CONNECTION_NAME);

        if (m_database.isOpen())
        {
            return true;
        }
    }
    else
    {
        m_database = QSqlDatabase::addDatabase(
            "QPSQL",
            CONNECTION_NAME
        );
    }

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

bool Database::isConnected() const
{
    return m_database.isValid() && m_database.isOpen();
}

QSqlDatabase Database::connection() const
{
    return m_database;
}
