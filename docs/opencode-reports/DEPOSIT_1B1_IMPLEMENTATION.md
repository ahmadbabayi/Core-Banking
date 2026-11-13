# گزارش پیاده‌سازی زیرمرحله 1B.1 — استخراج ساختاری Balance از Deposit

تاریخ: 2026-11-13
وضعیت: پیاده‌سازی و Build و آزمون هدفمند بدون PostgreSQL انجام شد. این گزارش تکمیل 1B یا رفع race را ادعا نمی‌کند.

## ۱. خلاصه اجرا

- SQLهای مربوط به Balance و خواندن ورودی‌های محاسبه موجودی از `DepositService` خارج شدند.
- قرارداد `IBalanceRepository` در Application و پیاده‌سازی `BalanceRepository` در Infrastructure اضافه شدند.
- `balances()` به قرارداد `ITransactionUnitOfWork` و پیاده‌سازی `UnitOfWork` اضافه شد.
- Repository جدید به همان `QSqlDatabase` و همان UnitOfWork/تراکنش متصل است؛ Connection یا transaction مستقل ساخته نشد.
- SQL، bindها، ترتیب Queryها و قفل‌ها، `CURRENT_TIMESTAMP`/`CURRENT_DATE`، precision رشته‌ای NUMERIC و فرمول فعلی `available_balance` حفظ شدند.
- اعتبارسنجی‌ها و شاخه‌های تصمیم Use Case در Deposit باقی ماندند.
- Journal، TransactionEntry، LedgerBalance، Transfer، HoldService و مسیرهای نوشتن Hold تغییر نکردند.
- تغییرات قبلی 1A.1 overwrite یا revert نشدند.
- Git فقط read-only بررسی شد؛ stage/commit/push انجام نشد.

## ۲. فایل‌های تغییر‌یافته

### ایجادشده

- `src/application/contracts/ibalancerepository.h`
- `src/infrastructure/repository/balancerepository.h`
- `src/infrastructure/repository/balancerepository.cpp`
- `tests/balance_repository_unitofwork_test.cpp`

### تغییر‌یافته

- `src/application/contracts/itransactionunitofwork.h`
- `src/infrastructure/unitofwork.h`
- `src/infrastructure/unitofwork.cpp`
- `src/application/depositservice.h`
- `src/application/depositservice.cpp`
- `CMakeLists.txt` فقط برای افزودن قرارداد و Repository جدید
- `docs/PROJECT_CONTEXT.md`
- `docs/PROJECT_HISTORY.md`

فایل‌های 1A.1 مانند `transactionservice.*`، `transactionrepository.*`، `main.cpp` و قراردادهای Transaction قبلی در این مرحله تغییر جدیدی دریافت نکردند؛ تغییرات موجود آن‌ها حفظ شد.

## ۳. تغییرات ساختاری

### قرارداد Application

`IBalanceRepository` فقط سه نیاز واقعی Deposit را منتشر می‌کند:

1. `lockAccountBalanceRow(...)`
2. `loadLockedAccountSnapshot(...)`
3. `applyDepositIncrement(...)`

مدل‌های `BalanceAccountSnapshot` و `BalanceSnapshotResult` در Application تعریف شدند. قرارداد هیچ `QSqlDatabase`، `QSqlQuery`، SQL یا متن خام خطای دیتابیس را منتشر نمی‌کند و از `OperationError` امن استفاده می‌کند.

### پیاده‌سازی Infrastructure

`BalanceRepository` همان SQLهای قبلی Deposit را در Infrastructure اجرا می‌کند:

- قفل ردیف `account_balance` با `SELECT ... FOR UPDATE`
- خواندن Snapshot قفل‌شده شامل وضعیت حساب/محصول/سرفصل، Holdهای فعال و سقف اضافه‌برداشت مؤثر با `FOR UPDATE OF a, b`
- `UPDATE account_balance` برای افزایش `ledger_balance` و بازمحاسبه فعلی `available_balance`

### اتصال به UnitOfWork

- `balances()` به `ITransactionUnitOfWork` اضافه شد.
- `UnitOfWork` یک `BalanceRepository` با همان `QSqlDatabase` خود می‌سازد.
- `DepositService` از `unitOfWork->balances()` استفاده می‌کند؛ نه Connection جدید و نه transaction مستقل.
- قفل Account، قفل Hold و SQLهای Journal/Ledger همچنان در Deposit و روی همان Connection مشترک باقی‌اند.
- ترتیب کلی قبلی حفظ شد: Account Lock → Balance Lock → Hold Lock → Snapshot → ... → Balance Update → ...

## ۴. حفظ SQL و رفتار

مقایسه خودکار رشته SQL نسخه قبل و بعد انجام شد. نتیجه:

| بخش | نتیجه | طول SQL |
|---|---|---|
| Snapshot ورودی‌های محاسبه | یکسان | ۲۴۴۷ کاراکتر |
| قفل `account_balance` | یکسان | ۸۰ کاراکتر |
| `UPDATE account_balance` | یکسان | ۲۹۶ کاراکتر |

همچنین موارد زیر تغییر نکردند:

- نام و ترتیب bindها
- ترتیب Queryها و قفل‌ها
- `CURRENT_TIMESTAMP` و `CURRENT_DATE`
- `CAST(... AS NUMERIC)` و نگهداری NUMERIC به‌صورت رشته
- فرمول فعلی `available_balance`
- استفاده از `available_balance` به‌عنوان Cache فعلی، نه جایگزین منبع محاسبه

## ۵. تغییر پیام‌های خطا

کدهای API و شاخه‌های کنترل حفظ شدند. متن خام خطای Balance با پیام امن ثابت جایگزین شد:

| کد API | متن قبلی | متن فعلی |
|---|---|---|
| `ACCOUNT_BALANCE_LOCK_FAILED` | متن خام `QSqlError::text()` | `Account balance row lock failed` |
| `ACCOUNT_LOCK_OR_READ_FAILED` | متن خام `QSqlError::text()` | `Account balance state could not be loaded` |
| `ACCOUNT_BALANCE_UPDATE_FAILED` | متن خام `QSqlError::text()` | `Account balance update failed` |

هیچ جزئیات حساس جدیدی لاگ نشد.

## ۶. Build

دستور:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه واقعی: موفق.

```text
-- Configuring done
-- Generating done
-- Build files have been written to: /home/ahmad/projects/build-CoreBanking-Desktop-Debug
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/depositservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/repository/balancerepository.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/unitofwork.cpp.o
[  6%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  8%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[  9%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/depositcontroller.cpp.o
[ 10%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

CMake به‌دلیل تغییر فهرست فایل‌ها یک بار خودکار Reconfigure شد. دستور `cmake -S` جداگانه اجرا نشد.

## ۷. آزمون هدفمند

فایل آزمون در مسیر استاندارد `tests/balance_repository_unitofwork_test.cpp` نگهداری می‌شود.

دستور کامپایل و اجرا:

```bash
g++ -std=c++17 -fPIC -I. tests/balance_repository_unitofwork_test.cpp \
  src/infrastructure/unitofwork.cpp \
  src/infrastructure/repository/balancerepository.cpp \
  src/infrastructure/repository/transactionrepository.cpp \
  src/domain/transaction.cpp \
  $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
  -o /tmp/balance_repository_unitofwork_test

/tmp/balance_repository_unitofwork_test
```

خروجی واقعی:

```text
QSqlQuery::prepare: database not open
QSqlQuery::prepare: database not open
All balance repository and unit-of-work tests passed
```

دو هشدار `database not open` مربوط به آزمون عمدی اتصال بسته برای بررسی عدم نشت متن خام است؛ assertionها موفق بودند.

### پوشش آزمون

- `UnitOfWorkFactory` و `UnitOfWork` واقعی با دیتابیس حافظه‌ای `QSQLITE`
- استفاده از `balances()` در همان scope
- اعمال افزایش واریز داخل تراکنش و خواندن مقدار
- `rollback` و بازگشت مقدار اولیه
- اعمال دوم و `commit` و خواندن مقدار پایدارشده
- Account ناموجود و پیام امن `Account balance update failed` بدون نشت نام جدول/SQL
- اتصال بسته و پیام‌های امن ثابت برای قفل و Snapshot بدون نشت متن خام `not open`

### محدودیت صریح آزمون

آزمون PostgreSQL واقعی اجرا نشد. موارد زیر آزموده نشدند:

- رفتار واقعی `FOR UPDATE` و `FOR UPDATE OF a, b` در PostgreSQL
- دقت واقعی NUMERIC در PostgreSQL
- قواعد مؤثر Product/Override و شکاف هم‌زمانی آن‌ها
- Integration/Concurrency واقعی
- HTTP و Fault Injection واقعی

آزمون QSQLITE صرفاً wiring UnitOfWork/Repository، استفاده از همان Connection/تراکنش، مسیر rollback/commit و قرارداد خطای امن را می‌سنجد و جایگزین آزمون PostgreSQL نیست.

## ۸. داده‌ها و SQL واقعی

- هیچ داده‌ای در دیتابیس `corebanking` ایجاد یا تغییر نکرد.
- هیچ SQL واقعی روی PostgreSQL اجرا نشد.
- هیچ شناسه کسب‌وکاری جدیدی تولید نشد.
- آزمون فقط از دیتابیس حافظه‌ای `QSQLITE` استفاده کرد.

## ۹. بدهی‌ها و محدودیت‌های باقی‌مانده

- Journal، Ledger Balance، TransactionEntry و `QSqlDatabase` انتقالی هنوز در `DepositService` هستند.
- شکاف هم‌زمانی قواعد مؤثر و نبود Override حل نشده و برای Balance Slice باز است.
- زمان مرجع و روش محاسبه دقیق Domain، انتخاب `CURRENT_TIMESTAMP`/`CURRENT_DATE` و معنای `available_balance` تکمیل نشده‌اند.
- `available_balance` همچنان Cache کهنه‌شونده است و در این مرحله مبنای تصمیم مالی جدید قرار نگرفت.
- HoldService و مسیرهای نوشتن Hold تغییر نکردند.
- Behavior واقعی QPSQL/PostgreSQL در قطع ارتباط، هم‌زمانی و Fault Injection آزموده نشد.
- این مرحله تکمیل 1B، رفع race یا تکمیل معماری Deposit نیست.

## ۱۰. پیشنهاد Commit

پیام پیشنهادی (اجرا نشده است):

```text
refactor(balance): extract Deposit balance persistence into repository
```
