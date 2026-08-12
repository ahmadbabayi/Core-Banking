#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>

class Database
{
public:
    static Database& instance();

    bool connect();
    bool isConnected() const;

    QSqlDatabase connection() const;

private:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    QSqlDatabase m_database;
};

#endif // DATABASE_H
