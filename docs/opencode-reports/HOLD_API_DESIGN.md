# Hold API — Design Report

> وضعیت: **پیش‌نویس طراحی — منتظر تأیید کاربر برای پیاده‌سازی**
>
> این گزارش فقط تحلیل و طراحی است. در این مرحله هیچ فایل کد، هیچ Migration و هیچ داده‌ای در دیتابیس تغییر نکرده است.
>
> تاریخ تهیه: 2026-11-01 (نسخه بازبینی‌شده ۲)
>
> منابع بررسی‌شده: `docs/PROJECT_CONTEXT.md`، `docs/PROJECT_HISTORY.md`، `database/CURRENT_SCHEMA.sql`، `src/web/httprouter.*`، `src/main.cpp`، `src/application/depositservice.*`، `src/application/transferservice.*`، `src/web/depositcontroller.*`، `src/web/transfercontroller.*`، `src/application/accountservice.*`، `src/web/accountcontroller.*`، `src/domain/account.*`، `src/domain/accountbalance.*`، `src/infrastructure/repository/accountrepository.*`، `src/infrastructure/repository/accountbalancerepository.*`، `database/schema/015_account_holds.sql`، `database/schema/016_allow_overdraft_balances.sql`، و بررسی read-only دیتابیس `corebanking`.

## اصلاحات اعمال‌شده در این نسخه

1. خواندن وضعیت Hold جدید از `account_hold_status` **پیش از Commit و داخل همان تراکنش** (createHold و createEvent).
2. تفکیک `ACCOUNT_NOT_FOUND` از `ACCOUNT_BALANCE_NOT_INITIALIZED` با بررسی وجود ردیف در هر مرحله Lock **از طریق `query.next()`** (نه `numRowsAffected()`)، و تفکیک خطای اجرای Query از نتیجه صفر ردیف.
3. اصلاح کامل منطق Idempotency رویداد: پارس زمان ورودی پیش از بررسی Idempotency، عدم مقایسه زمان پیش‌فرض در Retry، مقایسه مبلغ فقط برای `PARTIAL_RELEASE`، موفقیت Retry پس از `FULL_RELEASE`.
4. حذف شرط تکراری/غیرقابل‌دسترسی `PARTIAL_RELEASE` و نگاشت روشن خطاهای مبلغ.
5. یکسان‌سازی نام Error Codeها و نگاشت دقیق به پیام‌های واقعی Trigger.
6. تأیید نحو واقعی PostgreSQL و پارامترگذاری Qt برای Advisory Lock.
7. تطبیق شماره و نام Migration با فایل‌های واقعی `database/schema/` (۰۰۱ تا ۰۱۶ موجود؛ بعدی `017`).
8. بازمقایسه ترتیب Lock با Deposit و Transfer.

---

## 1. خلاصه اجرایی

هدف: طراحی و سپس پیاده‌سازی Hold API (ایجاد، نمایش، فهرست، آزادسازی و تاریخچه رویدادها) بر پایه جداول موجود `account_hold`، `account_hold_events` و VIEW `account_hold_status`.

خروجی طراحی:

- ۵ Endpoint روی `httprouter.cpp` موجود (بدون sub-router).
- دو فایل جدید `HoldService` (application) و `HoldController` (web).
- یک Migration افزایشی پیشنهادی (شماره `017`، نام `017_hold_idempotency.sql`) — **اجرا نمی‌شود**.
- Idempotency ایجاد Hold در جدول `account_hold` (نه `transaction`).
- Idempotency رویدادهای آزادسازی از طریق Trigger موجود `validate_account_hold_event` (بدون تغییر Trigger).
- ترتیب قفل یکسان با Deposit/Transfer: Account → Account Balance → Account Holds.
- `HoldService` هیچ تغییری روی `ledger_balance` یا `available_balance` نمی‌دهد.

---

## 2. وضعیت Git و Repository (زمان تهیه گزارش)

| مورد | مقدار |
|---|---|
| مسیر | `/home/ahmad/projects/CoreBanking` |
| Branch | `main` |
| HEAD | `41c48ad0e797b9911be909e76d612b79e5808585` |
| origin/main | هم‌سطح (up-to-date) |
| فایل تغییر‌یافته | `opencode.json` (unstaged؛ تنظیمات OpenCode، نه کد پروژه) |
| Migrationهای موجود | `001` تا `016` (تعداد ۱۶ فایل در `database/schema/`) |
| Migration بعدی | `017` — هیچ فایل `017*` وجود ندارد |

---

## 3. معماری واقعی موجود

```
HTTP (httpserver.cpp)
  → HttpRouter (httprouter.cpp)
    → Controller (web/*.cpp)
      → Service (application/*.cpp)
        → QSqlDatabase / SQL مستقیم (سرویس‌های جدید)
        یا Repository (infrastructure/repository) برای سرویس‌های قدیمی
```

- `DepositService` و `TransferService` مستقیماً SQL اجرا می‌کنند.
- `AccountService` و CRUD قدیمی از Repository استفاده می‌کنند.
- `TransferController` / `DepositController` با pointer اختیاری در Router سیم‌کشی می‌شوند (`httprouter.h:59-60`، `main.cpp:148-149`).
- پاسخ‌ها با `ApiResponse::success/error` (`apiresponse.cpp:9-38`): `{"success":true,"data":{...}}` و `{"success":false,"error":{"code","message"}}`.

---

## 4. تحلیل Deposit و Transfer

### 4.1 الگوهای مشترک

| جنبه | Deposit | Transfer |
|---|---|---|
| امضا | `deposit(accountId, amount, description, key)` | `transfer(sourceId, destId, amount, description, key)` |
| Result | `DepositResult { Status, transactionId, transactionNumber, code, message }` | `TransferResult { ... }` |
| normalizeAmount | `^(?:0\|[1-9][0-9]*)(?:\.[0-9]{1,8})?$` + حداقل یک رقم غیرصفر | یکسان |
| Advisory Lock | `SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))` | یکسان |
| Idempotency Lookup | `SELECT ... FROM transaction WHERE channel='API' AND idempotency_key=:key FOR UPDATE` | یکسان |
| Fingerprint | `"DEPOSIT\nid\namount\ndesc"` → SHA256 | `"TRANSFER\nsrc\ndst\namount\ndesc"` → SHA256 |
| Lock order | Account → Balance → Hold | یکسان |
| Journal | DRAFT → (کنترل توازن) → POSTED | یکسان |
| HTTP موفق | `201 Created` | یکسان |
| HTTP رد | `422 Unprocessable Entity` | یکسان |
| HTTP خطا | `500 Internal Server Error` | یکسان |

### 4.2 اعتبارسنجی حساب از `loadLockedAccount`

SQL (`depositservice.cpp:131-204`) با `JOIN account_balance` (نه LEFT JOIN) خوانده می‌شود. شرط‌ها (`depositservice.cpp:479-505`):

| شرط | کد خطا |
|---|---|
| `account.accountId == 0` | `ACCOUNT_BALANCE_NOT_INITIALIZED` |
| `accountStatus != "ACTIVE"` | `ACCOUNT_NOT_ACTIVE` |
| `productStatus != "ACTIVE"` (CASE بر اساس `p.status`، `effective_from <= CURRENT_DATE`، `effective_to IS NULL OR >= CURRENT_DATE`) | `PRODUCT_NOT_ACTIVE` |
| `customerLedgerStatus != "ACTIVE"` یا `!is_postable` یا `account_category != "LIABILITY"` | `CUSTOMER_LEDGER_NOT_POSTABLE` |
| `customerLedgerCurrencyId != account.currencyId` | `CUSTOMER_LEDGER_CURRENCY_MISMATCH` |

این شرط‌ها در `createHold` عیناً تکرار می‌شوند.

### 4.3 ترتیب قفل واقعی (تأیید مجدد)

Deposit (`depositservice.cpp`):

- `:424` → `SELECT id FROM account WHERE id = :account_id FOR UPDATE`
- `:441` → `SELECT account_id FROM account_balance WHERE account_id = :account_id FOR UPDATE`
- `:458` → `SELECT id FROM account_hold WHERE account_id = :account_id ORDER BY id FOR UPDATE`

Transfer (`transferservice.cpp`):

- `:499` → `SELECT id FROM account ... ORDER BY id FOR UPDATE`
- `:520` → `SELECT account_id FROM account_balance ... ORDER BY account_id FOR UPDATE`
- `:541` → `SELECT id FROM account_hold ... ORDER BY account_id, id FOR UPDATE`

ترتیب تأییدشده: **Account → Account Balance → Account Holds**. HoldService از همین ترتیب استفاده می‌کند.

---

## 5. وضعیت Router و `main.cpp`

- `httprouter.cpp` هیچ مسیر Hold ندارد (۶۲۵ خط بررسی شد).
- بخش Deposit: `httprouter.cpp:104-116`؛ Transfer: `httprouter.cpp:90-102`.
- سیم‌کشی pointer: `main.cpp:148-149`.
- هیچ `HoldService`/`HoldController` در `main.cpp` نیست.
- مسیر ناشناخته → `404 ROUTE_NOT_FOUND` (`httprouter.cpp:620-624`).

### 5.1 مسیرهای پیشنهادی (در همان `httprouter.cpp`، بعد از خط ۱۱۶)

- `POST /api/v1/holds`
- `GET /api/v1/holds` (با `?accountId=`)
- `GET /api/v1/holds/{id}`
- `POST /api/v1/holds/{id}/events`
- `GET /api/v1/holds/{id}/events`

الگوی pointer اختیاری + پاسخ `503 HOLD_SERVICE_UNAVAILABLE` مانند Deposit/Transfer. شناسه‌ها با `parsePositiveId` (`httprouter.cpp:66-77`).

---

## 6. ساختار دیتابیس Hold (تأیید از CURRENT_SCHEMA و دیتابیس واقعی)

### 6.1 `account_hold`

| ستون | نوع | Nullable | Default |
|---|---|---|---|
| `id` | `bigint` PK identity | NO | — |
| `account_id` | `bigint` | NO | — |
| `currency_id` | `bigint` | NO | — |
| `amount` | `numeric` (>0) | NO | — |
| `reason_code` | `varchar(50)` (btrim ≠ '') | NO | — |
| `reason_description` | `text` | YES | — |
| `reference_type` | `varchar(50)` | YES | — |
| `reference_id` | `varchar(100)` | YES | — |
| `created_at` | `timestamptz` | NO | CURRENT_TIMESTAMP |
| `effective_at` | `timestamptz` | NO | CURRENT_TIMESTAMP |
| `expires_at` | `timestamptz` | YES | — |

قیدها:

- `ck_account_hold_amount`: `amount > 0`
- `ck_account_hold_reason`: `btrim(reason_code) <> ''`
- `ck_account_hold_expiry`: `expires_at IS NULL OR expires_at > effective_at`
- FK: `(account_id, currency_id) → account(id, currency_id) ON DELETE RESTRICT`
- Trigger: `trg_protect_account_hold BEFORE UPDATE OR DELETE` → `ACCOUNT_HOLD_IS_IMMUTABLE`

### 6.2 `account_hold_events`

| ستون | نوع | Nullable |
|---|---|---|
| `id` | `bigint` PK identity | NO |
| `hold_id` | `bigint` | NO |
| `idempotency_key` | `varchar(100)` | NO |
| `event_type` | `varchar(30)` از ۴ نوع | NO |
| `amount` | `numeric` (>0) | NO |
| `effective_at` | `timestamptz` | NO |
| `created_at` | `timestamptz` | NO |
| `reference_type` | `varchar(50)` | YES |
| `reference_id` | `varchar(100)` | YES |
| `reason_code` | `varchar(50)` | NO |

- `uq_account_hold_event_idempotency: UNIQUE (hold_id, idempotency_key)`
- `ck_account_hold_event_type`: چهار نوع
- Triggerها: `trg_account_hold_event_immutable` (UPDATE/DELETE)، `trg_validate_account_hold_event` (BEFORE INSERT)

### 6.3 VIEW `account_hold_status`

ستون‌ها: `hold_id`, `account_id`, `currency_id`, `original_amount`, `remaining_amount`, `reason_code`, `reason_description`, `reference_type`, `reference_id`, `created_at`, `effective_at`, `expires_at`, `release_event_count`, `last_release_event_type`, `status`, `currently_blocked_amount`.

`status`: `SCHEDULED` / `ACTIVE` / `PARTIALLY_RELEASED` / `RELEASED` / `EXPIRED` / `CANCELLED`.

`currently_blocked_amount`: ۰ اگر `effective_at > now()` یا `expires_at <= now()`؛ در غیر این صورت `GREATEST(remaining_amount, 0)`.

### 6.4 رفتار واقعی Trigger `validate_account_hold_event` (تأیید با `pg_get_functiondef`)

1. Lock ردیف `account_hold` با `FOR UPDATE`.
2. بررسی Idempotency `(hold_id, idempotency_key)` **قبل از هر اعتبارسنجی مبلغ/زمان**:
   - payload یکسان → `RETURN NULL` (بدون INSERT، بدون خطا).
   - payload متفاوت → `RAISE ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED` (23505).
3. کنترل‌های زمانی و مبلغ.

**مهم:** Trigger زمان را با `v_existing.effective_at = NEW.effective_at` مقایسه می‌کند؛ یعنی Retry بدون ارسال زمان اگر Service مقدار پیش‌فرض جدید بفرستد، در سطح Trigger «payload متفاوت» تلقی می‌شود. بنابراین Service باید Retry را **قبل از INSERT** و با منطق تفکیک‌شده (بخش 11.4) تشخیص دهد تا به Trigger نرسد.

---

## 7. Available Balance — بررسی و راهکار

- `GET /api/v1/accounts/{id}` (`accountcontroller.cpp:71-100`) از `AccountRepository.findById()` استفاده می‌کند که فقط ستون‌های جدول `account` را می‌خواند (`accountrepository.cpp:149-165`).
- مدل `Account` (`domain/account.h`) هیچ فیلد balance ندارد.
- `AccountBalanceRepository` وجود دارد ولی در هیچ مسیر خواندن API استفاده نمی‌شود.
- `available_balance` پس از Hold «کهنه» می‌شود؛ فقط Deposit (`:696-717`) و Transfer (`:243-276`) آن را به‌روز می‌کنند.

راهکار نمایش جاری (در زمان Query، نه ستون):

```sql
SELECT
  b.ledger_balance,
  COALESCE(hs.active_holds, 0) AS active_holds,
  COALESCE(od.effective_limit, 0) AS overdraft_limit,
  (b.ledger_balance - COALESCE(hs.active_holds, 0)
   + COALESCE(od.effective_limit, 0)) AS calculated_available
FROM account_balance b
LEFT JOIN (
  SELECT account_id, SUM(currently_blocked_amount) AS active_holds
  FROM account_hold_status
  GROUP BY account_id
) hs ON hs.account_id = b.account_id
-- subquery های overdraft مطابق depositservice.cpp:136-167
```

در این مرحله فقط مستند می‌شود؛ هیچ تغییر Schema/کد انجام نمی‌شود.

---

## 8. Fingerprint — قاعده طراحی

Fingerprint فقط از ورودی نرمال‌شده کلاینت ساخته می‌شود. اگر فیلدی ارسال نشده باشد، مقدار خالی استفاده می‌شود. پیش‌فرض‌های سرور بعد از Idempotency Check تعیین می‌شوند.

### 8.1 createHold

```
"HOLD\n" + accountId + "\n" + normalizedAmount + "\n"
+ reasonCode + "\n"
+ (reasonDescription or "") + "\n"
+ (referenceType or "") + "\n"
+ (referenceId or "") + "\n"
+ (effectiveAtInputNormalized or "") + "\n"
+ (expiresAtInputNormalized or "")
```

### 8.2 createEvent

```
"EVENT\n" + holdId + "\n" + eventType + "\n"
+ (PARTIAL_RELEASE ? normalizedAmount : "") + "\n"
+ (effectiveAtInputNormalized or "") + "\n"
+ reasonCode + "\n"
+ (referenceType or "") + "\n"
+ (referenceId or "")
```

هماهنگ با Deposit (`:74-86`) و Transfer (`:77-89`) که فقط ورودی را در Fingerprint می‌آورند.

---

## 9. تصمیم‌های کسب‌وکاری تأییدشده

| تصمیم | مقدار | تعارض |
|---|---|---|
| Idempotency ایجاد Hold | کلید جهانی، هماهنگ با Deposit/Transfer | بدون تعارض |
| Overdraft برای Hold | مجاز نیست؛ `AvailableForHold = Ledger Balance - Active Holds` | منطق سرویس، بدون تعارض |
| حساب غیرفعال مانع آزادسازی Hold | نیست | بدون تعارض |
| چند Hold فعال روی یک حساب | مجاز | بدون تعارض |
| Hold بدون `expires_at` | مجاز | بدون تعارض (`null` مجاز) |
| محدوده یکتایی `idempotency_key` | جهانی | مطابق `uq_transaction_api_idempotency` |

---

## 10. Migration پیشنهادی (اجرا نمی‌شود)

تطبیق با فایل‌های واقعی: `database/schema/` شامل `001_geography_currency.sql` تا `016_allow_overdraft_balances.sql` است. هیچ `017*` وجود ندارد. نام پیشنهادی: `017_hold_idempotency.sql`.

```sql
BEGIN;

-- ============================================================
-- 017: Add hold creation idempotency
-- ============================================================

ALTER TABLE account_hold
  ADD COLUMN idempotency_key VARCHAR(100),
  ADD COLUMN request_fingerprint VARCHAR(64);

ALTER TABLE account_hold
  ADD CONSTRAINT ck_hold_idempotency_pair
    CHECK (
      (idempotency_key IS NULL AND request_fingerprint IS NULL)
      OR
      (idempotency_key IS NOT NULL AND request_fingerprint IS NOT NULL)
    );

ALTER TABLE account_hold
  ADD CONSTRAINT ck_hold_idempotency_key_length
    CHECK (idempotency_key IS NULL OR LENGTH(BTRIM(idempotency_key)) BETWEEN 1 AND 100);

ALTER TABLE account_hold
  ADD CONSTRAINT ck_hold_fingerprint_length
    CHECK (request_fingerprint IS NULL OR LENGTH(request_fingerprint) = 64);

CREATE UNIQUE INDEX uq_hold_api_idempotency
  ON account_hold (idempotency_key)
  WHERE idempotency_key IS NOT NULL;

COMMIT;
```

نکات: ستون‌ها nullable برای سازگاری با ردیف‌های قدیمی؛ سرویس همیشه هر دو را غیر NULL می‌نویسد؛ `UNIQUE` فقط برای مقادیر غیر NULL؛ Trigger `trg_protect_account_hold` با این طرح تعارض ندارد (Idempotency فقط روی INSERT).

---

## 11. طراحی HoldService

### 11.1 فایل‌ها

| فایل | وضعیت |
|---|---|
| `src/application/holdservice.h` | ایجاد |
| `src/application/holdservice.cpp` | ایجاد |
| `src/web/holdcontroller.h` | ایجاد |
| `src/web/holdcontroller.cpp` | ایجاد |

### 11.2 ساختار

```cpp
struct HoldResult {
    enum class Status { Completed, Rejected, Failed };
    Status status = Status::Failed;
    qint64 holdId = 0;
    QString code;
    QString message;
};

struct HoldEventResult {
    enum class Status { Completed, Rejected, Failed };
    Status status = Status::Failed;
    qint64 eventId = 0;
    QString code;
    QString message;
};

class HoldService {
public:
    explicit HoldService(const QSqlDatabase& database);

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

private:
    QSqlDatabase m_database;
};
```

### 11.3 الگوریتم createHold (اصلاح‌شده)

```
1.  اعتبارسنجی ورودی
    - DB valid/open → HOLD_DATABASE_UNAVAILABLE (500)
    - accountId > 0 → HOLD_INVALID_ACCOUNT_ID (400)
    - amount (normalizePositiveAmount) → HOLD_INVALID_AMOUNT (400)
    - idempotencyKey 1..100 → HOLD_INVALID_IDEMPOTENCY_KEY (400)
    - reasonCode غیرخالی → HOLD_INVALID_REASON_CODE (400)
    - reasonDescription <= 500 → HOLD_INVALID_DESCRIPTION (400)
    - referenceType <= 50, referenceId <= 100 (اختیاری)

2.  fingerprint = SHA256(ورودی نرمال‌شده کلاینت)   // بخش 8.1

3.  db.transaction() → شکست: HOLD_TRANSACTION_START_FAILED (500)

4.  Advisory Lock:
    SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))
    bind ":key" = "HOLD:" + idempotencyKey      // QString param، مطابق Deposit:264
    شکست → rollback → HOLD_IDEMPOTENCY_LOCK_FAILED (500)

5.  Idempotency lookup (در account_hold):
    SELECT id, request_fingerprint
      FROM account_hold
     WHERE idempotency_key = :key
       FOR UPDATE
    - exec() == false → rollback → HOLD_IDEMPOTENCY_LOOKUP_FAILED (500)
    - وجود ردیف با query.next() تشخیص داده می‌شود (نه numRowsAffected()).
    - اگر یافت شد:
        fingerprint یکسان؟
          → SELECT * FROM account_hold_status WHERE hold_id = :id   // داخل همان تراکنش
          → اگر خواندن status شکست: rollback → HOLD_STATE_READ_FAILED (500)
          → commit → Completed با holdId (HTTP 200)
        fingerprint متفاوت؟
          → rollback → IDEMPOTENCY_KEY_REUSED (409)

6.  تعیین زمان‌ها (پس از Idempotency):
    - effectiveAtParsed = client? parse : CURRENT_TIMESTAMP
    - expiresAtParsed   = client? parse : NULL
    - parse error → rollback → HOLD_INVALID_EFFECTIVE_AT / HOLD_INVALID_EXPIRES_AT (400)
    - اگر expiresAtParsed != NULL و <= effectiveAtParsed → HOLD_INVALID_EXPIRES_AT (400)

7.  Lock ترتیبی (Account → Balance → Hold) + بررسی نتیجه هر مرحله:
    قاعده تشخیص وجود ردیف برای هر SELECT ... FOR UPDATE:
      - ابتدا exec() را چک کن: اگر false → خطای اجرا (500 مربوط به همان مرحله).
      - سپس وجود ردیف را با query.next() تشخیص بده، نه با numRowsAffected().
      - در Qt QPSQL برای SELECT به numRowsAffected() اعتماد نکن.
      - «query.next() == false» یعنی «صفر ردیف» و باید از «exec() == false» جدا مدیریت شود.

    a) SELECT id FROM account WHERE id = :id FOR UPDATE
       exec() == false → rollback → HOLD_ACCOUNT_LOCK_FAILED (500)
       query.next() == false → rollback → ACCOUNT_NOT_FOUND (404)
       در غیر این صورت ردیف account قفل شد.
    b) SELECT account_id FROM account_balance WHERE account_id = :id FOR UPDATE
       exec() == false → rollback → HOLD_BALANCE_LOCK_FAILED (500)
       query.next() == false → rollback → ACCOUNT_BALANCE_NOT_INITIALIZED (422)
       در غیر این صورت ردیف account_balance قفل شد.
    c) SELECT id FROM account_hold WHERE account_id = :id ORDER BY id FOR UPDATE
       exec() == false → rollback → HOLD_LOCK_FAILED (500)
       query.next() == false مجاز است (هنوز Holdی نیست)؛ در غیر این صورت با while(query.next()) همه ردیف‌های Hold قفل می‌شوند.

8.  loadLockedAccount (همان SQL depositservice.cpp:131-204) برای وضعیت کامل
    - exec() == false → rollback → HOLD_STATE_READ_FAILED (500)
    - exec() == true ولی query.next() == false یا accountId == 0
      (ناسازگاری داخلی بعد از Lockهای موفق) → rollback → HOLD_STATE_READ_FAILED (500)
    - توجه: چون وجود account و account_balance قبلاً با Lock مرحله ۷ اثبات شده،
      نبود ردیف در این JOIN فقط ناسازگاری داخلی است، نه ACCOUNT_NOT_FOUND.
    - اعتبارسنجی مطابق بخش 4.2:
      ACCOUNT_NOT_ACTIVE (422)
      PRODUCT_NOT_ACTIVE (422)
      CUSTOMER_LEDGER_NOT_POSTABLE (422)
      CUSTOMER_LEDGER_CURRENCY_MISMATCH (422)

9.  AvailableForHold = ledger_balance - activeHolds
    اگر amount > AvailableForHold → rollback → INSUFFICIENT_AVAILABLE_FUNDS (422)

10. INSERT INTO account_hold
      (account_id, currency_id, amount, reason_code, reason_description,
       reference_type, reference_id, effective_at, expires_at,
       idempotency_key, request_fingerprint)
      VALUES (...)
      RETURNING id
    currency_id از state مرحله ۸ گرفته می‌شود.
    - UNIQUE VIOLATION روی uq_hold_api_idempotency (رقابت نادر):
        SELECT id, request_fingerprint FROM account_hold WHERE idempotency_key = :key
        fingerprint یکسان → خواندن status (داخل تراکنش) → commit → 200
        fingerprint متفاوت → rollback → IDEMPOTENCY_KEY_REUSED (409)
    - سایر خطاها → rollback → HOLD_CREATE_FAILED (500)

11. خواندن وضعیت Hold جدید — پیش از Commit و داخل همان تراکنش:
    SELECT * FROM account_hold_status WHERE hold_id = :new_id
    - exec() == false → rollback → HOLD_STATE_READ_FAILED (500)
    - exec() == true ولی query.next() == false → rollback → HOLD_STATE_READ_FAILED (500)
      (هر دو ۵۰۰ هستند ولی علت جدا تشخیص داده می‌شود؛ وجود ردیف با query.next() سنجیده می‌شود، نه numRowsAffected().)
    - این مرحله باید پیش از commit باشد تا Hold ثبت‌شده بدون وضعیتِ قابل‌گزارش نماند.

12. db.commit() → شکست: DATABASE_COMMIT_FAILED (500)

13. Completed(201) با holdId و وضعیت خوانده‌شده در مرحله ۱۱
```

**توجه اصلاح ۱:** خواندن `account_hold_status` هم در مسیر Retry (مرحله ۵) و هم در مسیر ایجاد جدید (مرحله ۱۱) پیش از `commit` انجام می‌شود؛ در صورت خطا rollback می‌شود و هرگز Holdِ commit‌شده با «شکست کاذب» گزارش نمی‌شود.

**توجه اصلاح ۲:** تشخیص عدم‌وجود حساب از عدم‌وجود `account_balance` با شمارش ردیف‌های Lock (۷a و ۷b) انجام می‌شود؛ `ACCOUNT_BALANCE_NOT_INITIALIZED` فقط وقتی برمی‌گردد که ردیف `account` وجود داشته باشد ولی ردیف `account_balance` نباشد.

### 11.4 الگوریتم createEvent (اصلاح‌شده Idempotency)

```
1.  اعتبارسنجی ورودی
    - DB valid/open → HOLD_DATABASE_UNAVAILABLE (500)
    - holdId > 0 → HOLD_INVALID_HOLD_ID (400)
    - eventType ∈ ۴ نوع → HOLD_INVALID_EVENT_TYPE (400)
    - idempotencyKey 1..100 → HOLD_INVALID_IDEMPOTENCY_KEY (400)
    - reasonCode غیرخالی → HOLD_INVALID_REASON_CODE (400)
    - برای PARTIAL_RELEASE فقط: amount معتبر → HOLD_INVALID_AMOUNT (400)

2.  پارس/نرمال‌سازی زمان ورودی کلاینت — پیش از بررسی Idempotency
    - effectiveAtProvided = !effectiveAtInput.trimmed().isEmpty()
    - اگر Provided: effectiveAtNormalized = parse(effectiveAtInput)
        parse error → HOLD_INVALID_EFFECTIVE_AT (400)
      (در Non-PARTIAL، amount ورودی نرمال‌سازی/استفاده نمی‌شود)

3.  fingerprint = SHA256(ورودی نرمال‌شده کلاینت)   // بخش 8.2
    (برای FULL/EXPIRY/CANCELLATION فیلد amount در fingerprint خالی است)

4.  db.transaction() → شکست: HOLD_TRANSACTION_START_FAILED (500)

5.  Advisory Lock:
    SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))
    bind ":key" = "HOLD-EVENT:" + idempotencyKey
    شکست → rollback → HOLD_IDEMPOTENCY_LOCK_FAILED (500)

6.  Idempotency Pre-check (پیش از هر اعتبارسنجی وضعیت/مبلغ):
    SELECT id, event_type, amount, effective_at,
           reference_type, reference_id, reason_code
      FROM account_hold_events
     WHERE hold_id = :hold_id AND idempotency_key = :key
       FOR UPDATE

    - exec() == false → rollback → HOLD_IDEMPOTENCY_LOOKUP_FAILED (500)
    - وجود ردیف با query.next() تشخیص داده می‌شود (نه numRowsAffected()).
    اگر ردیف یافت شد، تطبیق دقیق In-Txn:
      a) event_type == requested eventType
      b) amount: فقط اگر eventType == PARTIAL_RELEASE
            مقایسه normalized client amount با amount ذخیره‌شده
         برای FULL/EXPIRY/CANCELLATION: مقایسه مبلغ انجام نمی‌شود
      c) effective_at: فقط اگر effectiveAtProvided == true
            مقایسه normalized client time با effective_at ذخیره‌شده
         اگر کلاینت زمان نفرستاده: مقایسه زمان انجام نمی‌شود
      d) reference_type, reference_id با NULL-safe equality
      e) reason_code برابری

      اگر همه شرط‌های اعمال‌شده مطابق:
        → SELECT * FROM account_hold_status WHERE hold_id = :hold_id  // داخل تراکنش
        → اگر شکست: rollback → HOLD_STATE_READ_FAILED (500)
        → commit → Completed با eventId موجود (HTTP 200)
        → هیچ Event جدیدی درج نمی‌شود (حتی پس از FULL_RELEASE)
      وگرنه:
        → rollback → IDEMPOTENCY_KEY_REUSED (409)

7.  خواندن Hold:
    SELECT account_id, amount, effective_at, expires_at
      FROM account_hold WHERE id = :hold_id
    - exec() == false → rollback → HOLD_STATE_READ_FAILED (500)
    - exec() == true ولی query.next() == false → rollback → HOLD_NOT_FOUND (404)
    - (وجود ردیف با query.next() سنجیده می‌شود، نه numRowsAffected().)

8.  Lock ترتیبی (Account → Balance → Hold):
    قاعده تشخیص: ابتدا exec()؛ اگر false → HOLD_*_LOCK_FAILED (500).
    وجود ردیف فقط با query.next() سنجیده می‌شود؛ numRowsAffected() برای SELECT استفاده نمی‌شود.
    a) SELECT id FROM account WHERE id = :account_id FOR UPDATE
    b) SELECT account_id FROM account_balance WHERE account_id = :account_id FOR UPDATE
    c) SELECT id FROM account_hold WHERE id = :hold_id FOR UPDATE
       exec() == false → rollback → HOLD_LOCK_FAILED (500)
       query.next() == false → rollback → HOLD_NOT_FOUND (404)
    (وجود account/balance برای آزادسازی الزامی نیست طبق تصمیم؛ Lock آن‌ها انجام می‌شود ولی
     query.next() == false در a/b مجاز است و ادامه می‌دهیم. صفر ردیف با exec() == false اشتباه گرفته نمی‌شود.)

9.  خواندن مجدد وضعیت Hold از VIEW — داخل تراکنش و پس از Lock:
    SELECT hs.remaining_amount, hs.status, hs.currently_blocked_amount,
           h.amount, h.effective_at, h.expires_at, h.account_id
      FROM account_hold_status hs
      JOIN account_hold h ON h.id = hs.hold_id
     WHERE hs.hold_id = :hold_id
    - exec() == false → rollback → HOLD_STATE_READ_FAILED (500)
    - query.next() == false → rollback → HOLD_NOT_FOUND (404)
    - این قرائت مرجع محاسبه مبلغ و اعتبارسنجی است (نه قرائت مرحله ۷).
    - وجود ردیف با query.next() سنجیده می‌شود، نه numRowsAffected().

10. تعیین effectiveAt جدید برای INSERT:
    - effectiveAtInsert = effectiveAtProvided ? effectiveAtNormalized : CURRENT_TIMESTAMP

11. اعتبارسنجی زمان (پیش از INSERT؛ همان قواعد Trigger):
    - if effectiveAtInsert > now() → ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED (422)
    - if now() < hold.effective_at → ACCOUNT_HOLD_NOT_YET_EFFECTIVE (422)
    - if effectiveAtInsert < hold.effective_at → ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME (422)
    - if eventType == EXPIRY_RELEASE:
        if hold.expires_at IS NULL OR now() < hold.expires_at
          → ACCOUNT_HOLD_NOT_EXPIRED (422)
      else:
        if hold.expires_at IS NOT NULL AND now() >= hold.expires_at
          → EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE (422)

12. تعیین amountInsert:
    - FULL_RELEASE / CANCELLATION_RELEASE / EXPIRY_RELEASE:
        amountInsert = remaining
        اگر remaining <= 0 → ACCOUNT_HOLD_ALREADY_FULLY_RELEASED (422)
        (مبلغ ارسالی کلاینت کاملاً نادیده)
    - PARTIAL_RELEASE:
        amountInsert = client amount
        اگر amountInsert > remaining → ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT (422)
        اگر amountInsert == remaining → PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT (422)
        (دو شرط جدا و بدون هم‌پوشانی؛ مطابق دقیق ترتیب Trigger)

13. INSERT INTO account_hold_events
      (hold_id, idempotency_key, event_type, amount, effective_at,
       reference_type, reference_id, reason_code)
      VALUES (..., amountInsert, effectiveAtInsert, ...)
    - Trigger RAISE → rollback و نگاشت مطابق جدول بخش 14
    - Trigger RETURN NULL (فقط رقابت نادر که Advisory Lock آن را عملاً غیرممکن می‌کند):
        SELECT event با (hold_id, idempotency_key)
        → خواندن status (داخل تراکنش) → commit → 200
    - خطای دیگر → rollback → HOLD_EVENT_CREATE_FAILED (500)

14. خواندن وضعیت Hold جدید از account_hold_status — پیش از Commit (داخل تراکنش)
    SELECT * FROM account_hold_status WHERE hold_id = :hold_id
    - exec() == false → rollback → HOLD_STATE_READ_FAILED (500)
    - exec() == true ولی query.next() == false → rollback → HOLD_STATE_READ_FAILED (500)
    - وجود ردیف با query.next() سنجیده می‌شود، نه numRowsAffected().

15. db.commit() → شکست: DATABASE_COMMIT_FAILED (500)

16. Completed(201) با eventId و وضعیت خوانده‌شده
```

**توجه اصلاح ۳:** Idempotency Pre-check (مرحله ۶) پیش از اعتبارسنجی وضعیت/remaining اجرا می‌شود؛ بنابراین Retry عین پس از `FULL_RELEASE` نتیجه قبلی را برمی‌گرداند (۲۰۰) و Event تکراری نمی‌سازد. مبلغ فقط برای `PARTIAL_RELEASE` مقایسه می‌شود؛ زمان فقط اگر کلاینت آن را فرستاده باشد. مقایسه پیش‌فرض جدید با مقدار ذخیره‌شده انجام نمی‌شود.

**توجه اصلاح ۴:** برای `PARTIAL_RELEASE` دو خطای متمایز و بدون هم‌پوشانی:
- `amount > remaining` → `ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT`
- `amount == remaining` → `PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT`

این دقیقاً با ترتیب Trigger مطابقت دارد: Trigger ابتدا `NEW.amount > v_remaining` را چک می‌کند، سپس شرط `PARTIAL_RELEASE AND NEW.amount >= v_remaining` را (که فقط در حالت برابری می‌تواند برسد).

### 11.5 ترتیب قفل و جلوگیری از Deadlock

| عملیات | Account | Balance | Hold |
|---|---|---|---|
| Deposit | ۱ | ۲ | ۳ |
| Transfer | ۱ | ۲ | ۳ |
| createHold | ۱ | ۲ | ۳ |
| createEvent | ۱ | ۲ | ۳ |

ترتیب یکسان مانع Deadlock است. Holdها با `ORDER BY id` (و در Transfer با `ORDER BY account_id, id`) سریال می‌شوند.

### 11.6 هم‌زمانی createEvent

دو درخواست هم‌زمان با کلیدهای متفاوت روی یک Hold:

- Advisory Lock‌هایشان مستقل است.
- در قفل `account` یکی منتظر می‌ماند.
- پس از Commit اولی، دومی وضعیت تازه (مرحله ۹) را می‌خواند.
- اگر مجموع آزادسازی از اصل بیشتر شود، Trigger درخواست دوم را رد می‌کند.
- اگر دومی `FULL_RELEASE` و Hold قبلاً کامل آزاد شده باشد → `ACCOUNT_HOLD_ALREADY_FULLY_RELEASED`.
- اگر همان کلید (Retry) باشد → Pre-check مرحله ۶ نتیجه قبلی را می‌دهد (۲۰۰).

### 11.7 Idempotency — خلاصه

| سناریو | createHold | createEvent |
|---|---|---|
| اولین درخواست | 201 | 201 |
| Retry payload یکسان | 200 | 200 |
| Retry بدون time | 200 (زمان در مقایسه دخالت ندارد) | 200 (زمان در مقایسه دخالت ندارد) |
| کلید یکسان، payload متفاوت | 409 | 409 |
| Retry FULL_RELEASE بعد از آزادسازی کامل | — | 200 (Pre-check قبل از remaining) |
| رقابت نادر | UNIQUE VIOLATION → SELECT + fingerprint | Trigger RETURN NULL → SELECT event + 200 |

---

## 12. طراحی HoldController

```cpp
class HoldController {
public:
    explicit HoldController(HoldService& service);

    HttpResponse createHold(const QByteArray& body);
    HttpResponse getHoldById(qint64 id);
    HttpResponse getHoldsByAccount(const QByteArray& query);
    HttpResponse createEvent(qint64 id, const QByteArray& body);
    HttpResponse getEvents(qint64 id);

private:
    HoldService& service;
};
```

- پارس JSON مطابق `DepositController` (`:15-84`).
- موفق → `201` / `200` با `ApiResponse::success`.
- `Rejected` → `422` (به‌جز `IDEMPOTENCY_KEY_REUSED` که `409`).
- `Failed` → `500`.
- خواندن Hold/رویدادها از VIEW در Service نگه داشته می‌شود.
- `getHoldsByAccount` نبود/نامعتبر بودن `accountId` → `400`.

### 12.1 تغییرات Router

`httprouter.h`: `#include "holdcontroller.h"`، `setHoldController()`، member pointer.
`httprouter.cpp` (بعد از بخش Deposit): ۵ مسیر بخش 5.1 با `parsePositiveId`.

---

## 13. قرارداد JSON

### 13.1 POST /api/v1/holds

Request:

```json
{
  "accountId": 1,
  "amount": "500.00",
  "reasonCode": "DISPUTE",
  "reasonDescription": "Customer dispute hold",
  "referenceType": "TRANSACTION",
  "referenceId": "42",
  "effectiveAt": "2026-11-01T10:00:00+03:30",
  "expiresAt": "2026-12-01T10:00:00+03:30",
  "idempotencyKey": "hold-001"
}
```

`201`:

```json
{
  "success": true,
  "data": {
    "holdId": 1,
    "accountId": 1,
    "currencyId": 1,
    "originalAmount": "500.00",
    "remainingAmount": "500.00",
    "reasonCode": "DISPUTE",
    "reasonDescription": "Customer dispute hold",
    "referenceType": "TRANSACTION",
    "referenceId": "42",
    "createdAt": "2026-11-01T10:00:01+03:30",
    "effectiveAt": "2026-11-01T10:00:00+03:30",
    "expiresAt": "2026-12-01T10:00:00+03:30",
    "releaseEventCount": 0,
    "lastReleaseEventType": null,
    "status": "ACTIVE",
    "currentlyBlockedAmount": "500.00",
    "code": "HOLD_CREATED",
    "message": "Hold created"
  }
}
```

### 13.2 GET /api/v1/holds/{id}

`200`: همان `data` (بدون `code`/`message`).
`404`: `{"success":false,"error":{"code":"HOLD_NOT_FOUND","message":"Hold does not exist"}}`

### 13.3 GET /api/v1/holds?accountId={id}

```json
{
  "success": true,
  "data": {
    "holds": [
      { "holdId": 1, "accountId": 1, "status": "ACTIVE", "currentlyBlockedAmount": "500.00", "...": "..." }
    ]
  }
}
```

### 13.4 POST /api/v1/holds/{id}/events

Request (PARTIAL_RELEASE):

```json
{
  "eventType": "PARTIAL_RELEASE",
  "amount": "200.00",
  "effectiveAt": "2026-11-01T12:00:00+03:30",
  "reasonCode": "PARTIAL_SETTLEMENT",
  "referenceType": null,
  "referenceId": null,
  "idempotencyKey": "evt-001"
}
```

Request (FULL/CANCELLATION/EXPIRY) — `amount` اختیاری/نادیده:

```json
{
  "eventType": "FULL_RELEASE",
  "effectiveAt": "2026-11-01T12:00:00+03:30",
  "reasonCode": "RESOLVED",
  "idempotencyKey": "evt-002"
}
```

`201`:

```json
{
  "success": true,
  "data": {
    "eventId": 1,
    "holdId": 1,
    "eventType": "PARTIAL_RELEASE",
    "amount": "200.00",
    "effectiveAt": "2026-11-01T12:00:00+03:30",
    "reasonCode": "PARTIAL_SETTLEMENT",
    "remainingAmount": "300.00",
    "holdStatus": "PARTIALLY_RELEASED",
    "code": "HOLD_EVENT_CREATED",
    "message": "Hold event created"
  }
}
```

### 13.5 GET /api/v1/holds/{id}/events

```json
{
  "success": true,
  "data": {
    "events": [
      {
        "eventId": 1,
        "holdId": 1,
        "eventType": "PARTIAL_RELEASE",
        "amount": "200.00",
        "effectiveAt": "2026-11-01T12:00:00+03:30",
        "createdAt": "2026-11-01T12:00:01+03:30",
        "referenceType": null,
        "referenceId": null,
        "reasonCode": "PARTIAL_SETTLEMENT"
      }
    ]
  }
}
```

---

## 14. Error Codeهای یکسان‌شده و نگاشت به Trigger

### 14.1 Error Codeهای سرویس (منبع: کد Hold)

| Error Code | HTTP | منبع/شرط |
|---|---|---|
| `HOLD_DATABASE_UNAVAILABLE` | 500 | DB باز نیست |
| `HOLD_TRANSACTION_START_FAILED` | 500 | شروع تراکنش |
| `HOLD_IDEMPOTENCY_LOCK_FAILED` | 500 | Advisory Lock |
| `HOLD_IDEMPOTENCY_LOOKUP_FAILED` | 500 | exec() خطای SELECT idempotency (صفر ردیف خطا نیست) |
| `HOLD_ACCOUNT_LOCK_FAILED` | 500 | قفل account |
| `HOLD_BALANCE_LOCK_FAILED` | 500 | قفل account_balance |
| `HOLD_LOCK_FAILED` | 500 | قفل account_hold |
| `HOLD_STATE_READ_FAILED` | 500 | خواندن account_hold_status |
| `HOLD_CREATE_FAILED` | 500 | INSERT Hold |
| `HOLD_EVENT_CREATE_FAILED` | 500 | INSERT Event |
| `DATABASE_COMMIT_FAILED` | 500 | commit |
| `HOLD_INVALID_ACCOUNT_ID` | 400 | ورودی |
| `HOLD_INVALID_HOLD_ID` | 400 | ورودی/مسیر |
| `HOLD_INVALID_AMOUNT` | 400 | ورودی |
| `HOLD_INVALID_IDEMPOTENCY_KEY` | 400 | ورودی |
| `HOLD_INVALID_REASON_CODE` | 400 | ورودی |
| `HOLD_INVALID_DESCRIPTION` | 400 | ورودی |
| `HOLD_INVALID_EVENT_TYPE` | 400 | ورودی |
| `HOLD_INVALID_EFFECTIVE_AT` | 400 | پارس زمان |
| `HOLD_INVALID_EXPIRES_AT` | 400 | پارس/ترتیب زمان |
| `MISSING_ACCOUNT_ID` | 400 | GET فهرست |
| `ACCOUNT_NOT_FOUND` | 404 | account lock ۰ ردیف / lookup |
| `HOLD_NOT_FOUND` | 404 | Hold lookup |
| `ACCOUNT_BALANCE_NOT_INITIALIZED` | 422 | balance lock ۰ ردیف |
| `ACCOUNT_NOT_ACTIVE` | 422 | loadLockedAccount |
| `PRODUCT_NOT_ACTIVE` | 422 | loadLockedAccount |
| `CUSTOMER_LEDGER_NOT_POSTABLE` | 422 | loadLockedAccount |
| `CUSTOMER_LEDGER_CURRENCY_MISMATCH` | 422 | loadLockedAccount |
| `INSUFFICIENT_AVAILABLE_FUNDS` | 422 | منطق Hold |
| `IDEMPOTENCY_KEY_REUSED` | 409 | پیش‌بررسی idempotency |

### 14.2 Error Codeهای برگرفته از Trigger (نام‌ها دقیقاً همان پیام RAISE)

| Error Code (دقیقاً پیام Trigger) | HTTP | SQLSTATE | شرط |
|---|---|---|---|
| `ACCOUNT_HOLD_NOT_FOUND` | 404 | 23503 | hold_id ناموجود در Trigger |
| `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED` | 409 | 23505 | کلید تکراری با payload متفاوت (safety net) |
| `ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED` | 422 | 23514 | `NEW.effective_at > now()` |
| `ACCOUNT_HOLD_NOT_YET_EFFECTIVE` | 422 | 23514 | `now() < hold.effective_at` |
| `ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME` | 422 | 23514 | `NEW.effective_at < hold.effective_at` |
| `ACCOUNT_HOLD_NOT_EXPIRED` | 422 | 23514 | EXPIRY_RELEASE وقتی منقضی نشده |
| `EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE` | 422 | 23514 | Hold منقضی با رویداد غیر EXPIRY |
| `ACCOUNT_HOLD_ALREADY_FULLY_RELEASED` | 422 | 23514 | remaining <= 0 |
| `ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT` | 422 | 23514 | `NEW.amount > remaining` |
| `PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT` | 422 | 23514 | PARTIAL و `amount >= remaining` (در عمل برابری) |
| `FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT` | 422 | 23514 | FULL/EXPIRY/CANCELLATION و `amount <> remaining` |

نگاشت Service: با گرفتن `QSqlError` و تطبیق متن/`SQLSTATE`، کد Trigger را مستقیماً به HTTP مپ می‌کند (جدول بالا). پیش‌اعتبارسنجی‌های سرویس (مراحل ۱۱–۱۲) از همان نام‌ها استفاده می‌کنند تا کد خطا در همه مسیرها یکسان بماند.

Note: `ACCOUNT_HOLD_IS_IMMUTABLE` و `ACCOUNT_HOLD_EVENTS_ARE_IMMUTABLE` در مسیر عادی رخ نمی‌دهند (UPDATE/DELETE اجرا نمی‌کنیم) و در صورت وقوع، `500 HOLD_EVENT_CREATE_FAILED`/`HOLD_CREATE_FAILED` گزارش می‌شود.

---

## 15. نحو واقعی Advisory Lock (تأیید)

الگوی موجود در Deposit (`depositservice.cpp:262-270`) و Transfer (`transferservice.cpp:330-338`):

```cpp
QSqlQuery lock(db);
lock.prepare("SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))");
lock.bindValue(":key", QStringLiteral("API:") + idempotencyKey);

if (!lock.exec() || !lock.next())
    return rollbackFailure(...);
```

- `hashtextextended(text, bigint)` → bigint؛ `pg_advisory_xact_lock(bigint)` پذیرفته می‌شود.
- پارامتر `:key` از نوع QString به‌درستی به متن PostgreSQL بایند می‌شود (Qt placeholder).
- `pg_advisory_xact_lock` مقدار `void` برمی‌گرداند؛ `SELECT` یک ردیف برمی‌گرداند و `.next()` مقدار `true` می‌دهد (همان رفتار مورد اتکای Deposit/Transfer).
- HoldService از پیشوندهای جداگانه استفاده می‌کند تا با Deposit/Transfer تصادم نکند:
  - createHold: `"HOLD:" + idempotencyKey`
  - createEvent: `"HOLD-EVENT:" + idempotencyKey`

### 15.1 قاعده تشخیص ردیف برای SELECT (الزام پیاده‌سازی)

در تمام SELECTها (به‌ویژه `SELECT ... FOR UPDATE` برای Account، Account Balance و Hold، و خواندن `account_hold_status` و `account_hold_events`):

1. ابتدا نتیجه `query.exec()` را بررسی کن:
   - `exec() == false` → **خطای اجرای Query** (کد خطای ۵۰۰ مربوط به همان مرحله؛ هرگز به‌عنوان «ردیف ناموجود» تفسیر نشود).
2. سپس وجود ردیف را فقط با `query.next()` تشخیص بده:
   - `exec() == true` و `query.next() == false` → **صفر ردیف** (مثلاً `ACCOUNT_NOT_FOUND`، `ACCOUNT_BALANCE_NOT_INITIALIZED`، `HOLD_NOT_FOUND`).
3. در Qt QPSQL برای SELECT به `numRowsAffected()` اعتماد نکن و آن را مبنای تشخیص وجود ردیف قرار نده.
4. برای SELECTهایی که چند ردیف برمی‌گردانند (مثل قفل همه Holdهای یک حساب)، با `while (query.next())` پیمایش کن؛ و `exec() == false` را جدا از «صفر ردیف» مدیریت کن.

این قاعده در مراحل ۵، ۶، ۷، ۸، ۹، ۱۱ (createHold) و مراحل ۶، ۷، ۸، ۹، ۱۴ (createEvent) اعمال شده است.

---

## 16. فایل‌های درگیر

| عملیات | فایل |
|---|---|
| ایجاد | `src/application/holdservice.h` |
| ایجاد | `src/application/holdservice.cpp` |
| ایجاد | `src/web/holdcontroller.h` |
| ایجاد | `src/web/holdcontroller.cpp` |
| تغییر | `src/web/httprouter.h` |
| تغییر | `src/web/httprouter.cpp` |
| تغییر | `src/main.cpp` |
| تغییر | `CMakeLists.txt` |
| ایجاد (بدون اجرا) | `database/schema/017_hold_idempotency.sql` |
| بازتولید بعد از اجرای Migration (فقط با تأیید) | `database/CURRENT_SCHEMA.sql` |

هیچ Repository جدید ساخته نمی‌شود؛ `HoldService` مستقیماً SQL اجرا می‌کند (هم‌راستا با Deposit/Transfer).

---

## 17. برنامه تست HTTP و بررسی read-only دیتابیس

پیش‌نیاز: سرور روی `http://localhost:8080` و حساب ACTIVE دارای `account_balance`. بررسی SQL فقط با `corebanking_reader`.

### A. ایجاد Hold
```bash
curl -i -X POST http://localhost:8080/api/v1/holds \
  -H "Content-Type: application/json" \
  -d '{"accountId":1,"amount":"500.00","reasonCode":"TEST","reasonDescription":"Test hold","idempotencyKey":"hold-001"}'
```
`201`. سپس:
```sql
SELECT * FROM account_hold_status WHERE hold_id = :id;
```

### B. Retry عین → `200` با `holdId` یکسان.

### C. Idempotency conflict
```bash
curl -i -X POST http://localhost:8080/api/v1/holds \
  -H "Content-Type: application/json" \
  -d '{"accountId":1,"amount":"999.00","reasonCode":"OTHER","idempotencyKey":"hold-001"}'
```
`409 IDEMPOTENCY_KEY_REUSED`.

### D. Fingerprint بدون time
دو درخواست یکسان بدون `effectiveAt`/`expiresAt` → دومی `200`.

### E. GET تکی / ناموجود
`GET /holds/1` → `200`؛ `GET /holds/99999` → `404`.

### F. فهرست
`GET /holds?accountId=1` → `200`؛ `GET /holds` → `400 MISSING_ACCOUNT_ID`.

### G. PARTIAL_RELEASE
```bash
curl -i -X POST http://localhost:8080/api/v1/holds/1/events \
  -H "Content-Type: application/json" \
  -d '{"eventType":"PARTIAL_RELEASE","amount":"200.00","reasonCode":"PARTIAL","idempotencyKey":"evt-001"}'
```
`201`. SQL: `remaining_amount=300`, `status='PARTIALLY_RELEASED'`.

### H. Retry event عین → `200` با `eventId` یکسان.

### I. FULL_RELEASE
```bash
curl -i -X POST http://localhost:8080/api/v1/holds/1/events \
  -H "Content-Type: application/json" \
  -d '{"eventType":"FULL_RELEASE","reasonCode":"FULL","idempotencyKey":"evt-002"}'
```
`201`. SQL: `remaining_amount=0`, `status='RELEASED'`.

### J. Retry FULL_RELEASE بعد از آزادسازی کامل → `200` (بدون Event جدید).

### K. خطاهای مبلغ PARTIAL
- `amount > remaining` → `422 ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT`
- `amount == remaining` → `422 PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT`

### L. تاریخچه
`GET /holds/1/events` → `200` با آرایه رویدادها.

### M. انقضا
Hold با `effectiveAt=2025-01-01T00:00:00+00:00` و `expiresAt=2025-06-01T00:00:00+00:00` → `201`.
SQL: `status='EXPIRED'`, `currently_blocked_amount=0`.
سپس `EXPIRY_RELEASE` → `201` و `remaining_amount=0`, `status='EXPIRED'`.

### N. Hold بدون expires_at → `201`.

### O. CANCELLATION_RELEASE روی Hold فعال → `201`, `status='CANCELLED'`.

### P. موجودی ناکافی → `422 INSUFFICIENT_AVAILABLE_FUNDS`.

### Q. حساب ناموجود → `404 ACCOUNT_NOT_FOUND`.
حساب موجود بدون `account_balance` (سناریوی کنترل‌شده) → `422 ACCOUNT_BALANCE_NOT_INITIALIZED` (تفکیک اصلاح ۲).

همه بررسی‌های SQL فقط SELECT روی `account_hold`, `account_hold_events`, `account_hold_status`, `account_balance`.

---

## 18. موارد حل‌نشده / نیازمند تصمیم

1. API خواندن موجودی محاسبه‌شده (`GET .../balance`) خارج از دامنه فعلی است؛ در صورت نیاز، محاسبه در زمان Query.
2. اگر تصمیم Overdraft برای Hold تغییر کند، فقط منطق `AvailableForHold` عوض می‌شود؛ Schema تغییری لازم ندارد.
3. سقف تعداد Hold فعال تعریف نشده است.
4. لایه احراز هویت/مجوز در پروژه وجود ندارد؛ Hold API نیز بدون آن خواهد بود مگر جداگانه تعریف شود.

---

## 19. تأیید عدم تغییر

در تهیه این گزارش:

- هیچ فایل کد در `src/` تغییر نکرد.
- هیچ Migration ایجاد یا اجرا نشد.
- هیچ DDL/DML روی دیتابیس اجرا نشد؛ فقط SELECT و catalog queries با `corebanking_reader`.
- تنها فایل تغییر‌یافته در این مرحله: همین گزارش (`docs/opencode-reports/HOLD_API_DESIGN.md`). (`opencode.json` از قبل unstaged بود.)

منتظر تأیید نهایی برای شروع پیاده‌سازی.
