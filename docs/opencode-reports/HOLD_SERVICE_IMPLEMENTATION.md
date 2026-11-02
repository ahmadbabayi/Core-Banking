# HoldService — Implementation Report (Stage 1 + Stage 2)

> تاریخ Stage 1: 2026-11-01
>
> تاریخ Stage 2: 2026-11-02
>
> وضعیت: **سرویس تکمیل شد؛ Build موفق؛ آزمون خواندن read-only موفق؛ آزمون HTTP/نوشتن انجام نشد (بدون Controller/Route)**
>
> مبنا: `docs/opencode-reports/HOLD_API_DESIGN.md`، `docs/opencode-reports/HOLD_CONTROLLER_DESIGN.md`

## 1. خلاصه

در این مرحله فقط لایه سرویس Hold پیاده‌سازی شد:

- `src/application/holdservice.h`
- `src/application/holdservice.cpp`

برای اینکه فایل جدید واقعاً در Build پروژه لحاظ شود، دو خط مربوطه به `CMakeLists.txt` اضافه شد (با تأیید قبلی کاربر). هیچ Controller، Route، Endpoint، `main.cpp`، Migration یا عملیات Git انجام نشد.

## 2. بررسی پیش‌نیاز Schema

Migration 017 توسط کاربر اجرا شده بود. بررسی read-only با نقش `corebanking_reader` روی دیتابیس `corebanking` موارد زیر را تأیید کرد:

| مورد | نتیجه |
|---|---|
| `account_hold.idempotency_key` | موجود، `varchar(100)`، nullable |
| `account_hold.request_fingerprint` | موجود، `varchar(64)`، nullable |
| `ck_hold_idempotency_pair` | موجود |
| `ck_hold_idempotency_key_length` | موجود |
| `ck_hold_fingerprint_length` | موجود |
| `uq_hold_api_idempotency` | موجود، UNIQUE جزئی روی `idempotency_key` برای مقادیر غیر NULL |
| `account_hold_status` | VIEW موجود |
| Trigger `trg_protect_account_hold` | موجود |

هیچ DDL/DML اجرا نشد. فایل `database/schema/017_hold_idempotency.sql` با دیتابیس هم‌خوان است. در بازبینی Stage 2 مشخص شد `database/CURRENT_SCHEMA.sql` ستون‌های Migration 017 را نشان نمی‌دهد و نسبت به دیتابیس واقعی قدیمی است؛ این snapshot در هیچ‌یک از دو مرحله تغییر نکرد.

## 3. فایل‌های تغییر‌یافته

| فایل | عملیات |
|---|---|
| `src/application/holdservice.h` | ایجاد |
| `src/application/holdservice.cpp` | ایجاد |
| `CMakeLists.txt` | افزودن دو فایل بالا به `add_executable` (با تأیید) |
| `docs/PROJECT_CONTEXT.md` | به‌روزرسانی وضعیت HoldService، Migration 017 و کارهای باقی‌مانده |
| `docs/PROJECT_HISTORY.md` | ثبت شواهد این مرحله |
| `docs/opencode-reports/HOLD_SERVICE_IMPLEMENTATION.md` | همین گزارش |

## 4. قرارداد سرویس

### 4.1 `HoldResult`

```cpp
struct HoldResult {
    enum class Status { Completed, Rejected, Failed };
    Status status = Status::Failed;
    qint64 holdId = 0;
    bool replayed = false;
    QString code;
    QString message;
};
```

### 4.2 `HoldEventResult`

```cpp
struct HoldEventResult {
    enum class Status { Completed, Rejected, Failed };
    Status status = Status::Failed;
    qint64 eventId = 0;
    bool replayed = false;
    QString code;
    QString message;
};
```

`replayed` یک افزودنی کوچک نسبت به طراحی است تا Controller مرحله بعد بتواند 201 (ایجاد جدید) را از 200 (Retry) تفکیک کند؛ طراحی در بخش 11.7 این تفکیک را لازم می‌داند ولی در ساختار Result فیلدی برای آن پیش‌بینی نکرده بود.

### 4.3 امضاها

```cpp
HoldResult createHold(
    qint64 accountId,
    const QString& amount,
    const QString& reasonCode,
    const QString& reasonDescription,
    const QString& referenceType,
    const QString& referenceId,
    const QString& effectiveAt,
    const QString& expiresAt,
    const QString& idempotencyKey);

HoldEventResult createEvent(
    qint64 holdId,
    const QString& eventType,
    const QString& amount,
    const QString& effectiveAt,
    const QString& reasonCode,
    const QString& referenceType,
    const QString& referenceId,
    const QString& idempotencyKey);
```

## 5. `createHold` — مطابقت با طراحی

1. بررسی باز بودن دیتابیس → `HOLD_DATABASE_UNAVAILABLE`.
2. اعتبارسنجی `accountId`، مبلغ، `idempotencyKey`، `reasonCode`، `reasonDescription` و طول فیلدهای Reference.
3. پارس/نرمال‌سازی زمان‌های اختیاری برای ساخت fingerprint (ورودی نامعتبر → `HOLD_INVALID_EFFECTIVE_AT` یا `HOLD_INVALID_EXPIRES_AT`).
4. ساخت fingerprint مطابق بخش 8.1 طراحی.
5. شروع تراکنش.
6. Advisory Lock با `pg_advisory_xact_lock(hashtextextended(:key, 0))` و پیشوند `HOLD:`.
7. Idempotency Lookup روی `account_hold` با `FOR UPDATE`:
   - fingerprint یکسان → خواندن `account_hold_status` داخل تراکنش، Commit، نتیجه `Completed` با `replayed = true`.
   - fingerprint متفاوت → `IDEMPOTENCY_KEY_REUSED`.
8. تعیین زمان‌های نهایی و کنترل `expiresAt > effectiveAt`.
9. قفل ترتیبی Account → Account Balance → Account Hold (با `ORDER BY id` برای Holdها).
10. بارگذاری وضعیت قفل‌شده حساب (نسخه تطبیق‌یافته `loadLockedAccount` با `ledger_balance`).
11. کنترل `ACCOUNT_NOT_ACTIVE`، `PRODUCT_NOT_ACTIVE`، `CUSTOMER_LEDGER_NOT_POSTABLE`، `CUSTOMER_LEDGER_CURRENCY_MISMATCH`.
12. کنترل `AvailableForHold = ledger_balance - active holds` و رد با `INSUFFICIENT_AVAILABLE_FUNDS`.
13. INSERT در `account_hold` همراه `idempotency_key` و `request_fingerprint`.
14. خواندن `account_hold_status` برای Hold جدید پیش از Commit.
15. Commit و بازگشت `Completed` با `replayed = false`.

### 5.1 مسیر رقابت Idempotency در INSERT

پس از خطای `23505` روی `uq_hold_api_idempotency`، در PostgreSQL تراکنش Abort می‌شود؛ بنابراین پیش از ادامه Query از `SAVEPOINT hold_insert` و `ROLLBACK TO SAVEPOINT hold_insert` استفاده شد. سپس ردیف موجود با `idempotency_key` خوانده می‌شود:

- fingerprint یکسان → خواندن وضعیت، Commit، `Completed` با `replayed = true`.
- fingerprint متفاوت → `IDEMPOTENCY_KEY_REUSED`.
- عدم یافتن ردیف → `HOLD_CREATE_FAILED`.

این جزئیات در طراحی صریحاً ذکر نشده بود ولی برای رفتار درست مسیر رقابت لازم است.

## 6. `createEvent` — مطابقت با طراحی

1. بررسی باز بودن دیتابیس → `HOLD_DATABASE_UNAVAILABLE`.
2. اعتبارسنجی `holdId`، نوع رویداد، `idempotencyKey`، `reasonCode`، طول Reference و مبلغ فقط برای `PARTIAL_RELEASE`.
3. پارس و نرمال‌سازی `effectiveAt` **پیش از** بررسی Idempotency.
4. ساخت fingerprint مطابق بخش 8.2 (مبلغ فقط برای `PARTIAL_RELEASE`).
5. شروع تراکنش.
6. Advisory Lock با پیشوند `HOLD-EVENT:`.
7. Idempotency Pre-check روی `account_hold_events` پیش از هر اعتبارسنجی وضعیت/مبلغ:
   - تطبیق `event_type`، مبلغ (فقط PARTIAL)، `effective_at` (فقط اگر کلاینت فرستاده باشد)، `reference_type`/`reference_id` با NULL-safe equality و `reason_code`.
   - تطبیق کامل → خواندن وضعیت، Commit، `Completed` با `replayed = true`.
   - عدم تطبیق → `IDEMPOTENCY_KEY_REUSED`.
8. خواندن `account_hold` برای گرفتن `account_id` (۰ ردیف → `HOLD_NOT_FOUND`).
9. قفل ترتیبی Account → Account Balance → Hold. نبود Account/Balance مانع آزادسازی نیست؛ نبود Hold → `HOLD_NOT_FOUND`.
10. بازخوانی وضعیت Hold از `account_hold_status` داخل تراکنش.
11. تعیین `effectiveAt` نهایی و کنترل‌های زمانی مطابق Trigger.
12. کنترل‌های مبلغ:
    - `remaining <= 0` → `ACCOUNT_HOLD_ALREADY_FULLY_RELEASED`.
    - PARTIAL: `amount > remaining` → `ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT`؛ `amount == remaining` → `PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT`.
    - FULL/EXPIRY/CANCELLATION: `amountInsert = remaining`.
13. INSERT در `account_hold_events`.
14. در صورت `RETURN NULL` توسط Trigger (رقابت نادر)، خواندن Event موجود و بازگشت نتیجه قبلی با `replayed = true`.
15. خواندن `account_hold_status` پیش از Commit.
16. Commit و بازگشت `Completed`.

### 6.1 نگاشت خطاهای Trigger

پیام‌های `RAISE EXCEPTION` با جست‌وجوی متن خطای `QSqlError` به همان Error Code طراحی نگاشت می‌شوند:

- `ACCOUNT_HOLD_NOT_FOUND`
- `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED`
- `ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED`
- `ACCOUNT_HOLD_NOT_YET_EFFECTIVE`
- `ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME`
- `ACCOUNT_HOLD_NOT_EXPIRED`
- `EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE`
- `ACCOUNT_HOLD_ALREADY_FULLY_RELEASED`
- `ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT`
- `PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT`
- `FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT`

خطاهای `ACCOUNT_HOLD_IS_IMMUTABLE` و `ACCOUNT_HOLD_EVENTS_ARE_IMMUTABLE` در مسیر عادی رخ نمی‌دهند و در صورت وقوع به‌صورت `HOLD_EVENT_CREATE_FAILED` گزارش می‌شوند.

## 7. قواعد Qt/QPSQL رعایت‌شده

- در همه SELECTها ابتدا `exec()` بررسی می‌شود.
- وجود ردیف فقط با `query.next()` سنجیده می‌شود؛ `numRowsAffected()` مبنای وجود ردیف نیست.
- برای SELECTهای چندردیفی (قفل Holdهای یک حساب) از `while (query.next())` استفاده شده و خطای `exec()` جدا مدیریت می‌شود.
- زمان‌ها به‌صورت رشته ISO-8601 نرمال‌شده (UTC با میلی‌ثانیه) و با `CAST(... AS TIMESTAMPTZ)` بایند می‌شوند.
- مقایسه‌های مبلغ و تطبیق Idempotency در SQL با `CAST(... AS NUMERIC)` انجام می‌شود تا خطای گردکردن اعشاری رخ ندهد.

## 8. تغییرات نسبت به طراحی

| مورد | توضیح |
|---|---|
| فیلد `replayed` | افزودنی برای تفکیک 201/200 در Controller مرحله بعد |
| `HOLD_INVALID_REFERENCE` | برای طول بیش از حد `referenceType`/`referenceId`؛ در جدول خطاهای طراحی نبود |
| `SAVEPOINT` در INSERT Hold | لازم برای ادامه تراکنش پس از خطای `23505` در PostgreSQL |
| `loadLockedHoldAccount` با `ledger_balance` | طراحی به `loadLockedAccount` اشاره کرده بود که `ledger_balance` را انتخاب نمی‌کند؛ برای محاسبه `AvailableForHold` افزوده شد و بخش Overdraft حذف شد |
| پارس زمان پیش از تراکنش در `createHold` | برای ساخت fingerprint نرمال‌شده لازم است؛ زمان نامعتبر در همان ابتدا 400 می‌دهد |

هیچ‌کدام از این موارد منطق تأییدشده قفل‌گذاری، Idempotency یا قواعد مالی را تغییر نمی‌دهد.

## 9. Build

دستور طبق AGENTS.md:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی واقعی:

```text
-- Configuring done
-- Generating done
-- Build files have been written to: /home/ahmad/projects/build-CoreBanking-Desktop-Debug
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/database.cpp.o
[  4%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

پس از یک اصلاح کوچک در بایندینگ `:effective_provided`، Build افزایشی دوباره اجرا شد:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه نهایی: **موفق**. هشدار کامپایلری در خروجی مشاهده نشد.

## 10. آزمون‌ها و داده‌ها

- **آزمون HTTP: انجام نشد.** دلیل: در این مرحله Controller/Route/Endpoint وجود ندارد و `HoldService` به Router یا `main.cpp` متصل نیست.
- **نوشتن داده: انجام نشد.** هیچ Hold، Event یا داده آزمایشی ایجاد نشد.
- **بررسی دیتابیس: فقط read-only** با `corebanking_reader` برای تأیید Migration 017 و ساختار VIEW/Trigger.
- **آزمون‌های باقی‌مانده برای مرحله بعد:** ایجاد Hold، Retry، تعارض Idempotency، PARTIAL/FULL/EXPIRY/CANCELLATION، موجودی ناکافی، حساب/مانده ناموجود، هم‌زمانی و بررسی `account_hold_status` و `account_hold_events` از طریق API واقعی.

## 11. وضعیت Migration

- Migration 017 توسط کاربر اجرا شده و با بررسی read-only تأیید شد.
- در این مرحله هیچ Migration جدیدی ایجاد یا اجرا نشد.

## 12. Stage 2 — تکمیل HoldService (2026-11-02)

### 12.1 تغییرات قرارداد

- `HoldStatusData` و `HoldEventData` اضافه شدند.
- `HoldQueryResult`، `HoldListResult` و `HoldEventsResult` اضافه شدند.
- `HoldResult` و `HoldEventResult` اکنون دارای `snapshot` و `hasSnapshot` هستند.
- متدهای خواندن اضافه شدند:

```cpp
HoldQueryResult getHoldById(qint64 holdId);
HoldListResult getHoldsByAccount(qint64 accountId);
HoldEventsResult getHoldEvents(qint64 holdId);
```

### 12.2 زمان دیتابیس و Idempotency

- `databaseNow()` مقدار `CURRENT_TIMESTAMP` را داخل تراکنش جاری می‌خواند، به میلی‌ثانیه Truncate می‌کند و به فرمت ISO-8601 UTC تبدیل می‌کند.
- زمان پیش‌فرض Hold و Event از همین مقدار استفاده می‌کند و همان مقدار در INSERT بایند می‌شود.
- Idempotency Lookup همچنان پیش از اعتبارسنجی‌های وابسته به وضعیت فعلی Hold انجام می‌شود.
- `fingerprintForHold` و `fingerprintForEvent` تغییر نکردند. زمان پیش‌فرض سرور وارد Fingerprint نمی‌شود؛ زمان ارسالی کلاینت مانند قبل نرمال‌شده وارد Fingerprint می‌شود.

### 12.3 Snapshot

Snapshot در همه مسیرهای `Completed` و پیش از Commit خوانده می‌شود:

- ایجاد جدید Hold و Event
- Retry Hold و Event
- بازیابی Unique Conflict Hold
- بازیابی Unique Conflict Event
- Trigger RETURN NULL برای Event تکراری

هیچ Query جدیدی پس از Commit انجام نمی‌شود. شکست خواندن Snapshot باعث Rollback و `HOLD_STATE_READ_FAILED` می‌شود.

### 12.4 نگاشت خطا و SAVEPOINT

- تشخیص خطا با SQLSTATE، ترکیب `databaseText()` و `text()` و نام Constraint/Index انجام می‌شود.
- کدهای اعتبارسنجی Trigger به `Rejected` و کدهای immutability/ناشناخته به `Failed` نگاشت می‌شوند.
- `uq_hold_api_idempotency` فقط در مسیر Hold Idempotency استفاده می‌شود.
- `uq_account_hold_event_idempotency` در مسیر Event با `SAVEPOINT hold_event_insert` مدیریت می‌شود.
- اگر `ROLLBACK TO SAVEPOINT` موفق نشود، سرویس تراکنش را ادامه نمی‌دهد و `HOLD_CREATE_FAILED` یا `HOLD_EVENT_CREATE_FAILED` برمی‌گرداند.

### 12.5 متدهای خواندن

- `getHoldById`: خواندن از VIEW `account_hold_status`؛ نبود ردیف → `NotFound` با `HOLD_NOT_FOUND`.
- `getHoldsByAccount`: لیست مرتب بر اساس `hold_id`؛ حساب ناموجود یا بدون Hold → `Completed` با لیست خالی.
- `getHoldEvents`: ابتدا بررسی وجود Hold و سپس خواندن رویدادها با `ORDER BY id`؛ نبود Hold → `HoldNotFound`.
- مبالغ `QString` می‌مانند و `releasedAmount` با `GREATEST(original_amount - remaining_amount, 0)` محاسبه می‌شود.
- `idempotency_key` در خروجی Event نمایش داده نمی‌شود.

### 12.6 Build Stage 2

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

### 12.7 آزمون‌ها

- آزمون HTTP انجام نشد؛ Controller/Route وجود ندارد.
- هیچ داده Hold/Event ایجاد نشد و هیچ DML اجرا نشد.
- بررسی Schema فقط read-only با `corebanking_reader` انجام شد.
- مسیرهای خواندن با Harness موقت خارج از مخزن (`/tmp/opencode/hold_read_test.cpp`) و اتصال read-only اجرا شدند:
  - invalid Hold ID → `HOLD_INVALID_HOLD_ID`
  - Hold ناموجود → `HOLD_NOT_FOUND`
  - invalid Account ID → `HOLD_INVALID_ACCOUNT_ID`
  - حساب بدون Hold → `Completed` با لیست خالی
  - invalid Hold ID برای Event → `HOLD_INVALID_HOLD_ID`
  - Hold ناموجود برای Event → `HOLD_NOT_FOUND`
  - خروجی: `READ_METHOD_CHECKS_PASSED`
- جدول‌های Hold و Event در زمان آزمون صفر ردیف بودند؛ Snapshot مسیر Create و خواندن داده واقعی هنوز آزمون نشده‌اند.

## 13. گام بعدی پیشنهادی

ایجاد `HoldController` و مسیرهای HTTP مطابق بخش‌های 5.1، 12 و 13 طراحی، سپس اتصال به Router و `main.cpp` و اجرای آزمون‌های HTTP/DB. این کار نیازمند تأیید جداگانه کاربر است.
