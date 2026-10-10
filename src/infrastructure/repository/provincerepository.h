#ifndef PROVINCEREPOSITORY_H
#define PROVINCEREPOSITORY_H

#include "iprovincerepository.h"
#include <QSqlDatabase>

class ProvinceRepository : public IProvinceRepository
{
public:
    explicit ProvinceRepository(
        const QSqlDatabase& database
    );

    SaveResult save(Province& province) override;

    FindResult findById(
        long long id,
        Province& province
    ) override;

    ListResult findAll(
        QVector<Province>& provinces
    ) override;

private:
    QSqlDatabase db;
};

#endif // PROVINCEREPOSITORY_H
