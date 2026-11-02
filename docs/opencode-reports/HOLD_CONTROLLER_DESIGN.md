# Hold Controller & Read API — Supplementary Design

> تاریخ: 2026-11-01
>
> وضعیت: **طرح تکمیلی — منتظر تأیید؛ هیچ کدی تغییر نکرده و هیچ Build/Migration اجرا نشده است.**
>
> مبنا: `docs/opencode-reports/HOLD_API_DESIGN.md`، `docs/opencode-reports/HOLD_SERVICE_REVIEW.md`، `src/application/holdservice.*`، `src/web/depositcontroller.*`، `src/web/transfercontroller.*`، `src/web/accountcontroller.*`، `src/web/httprouter.*`، `src/main.cpp`، `database/CURRENT_SCHEMA.sql`.

## 1. خلاصه تصمیم‌ها

| # | موضوع | تصمیم پیشنهادی | نیازمند Schema جدید؟ |
|---|---|---|---|
| 1 | دامنه Idempotency | Transaction و Hold مستقل؛ یک کلید مشابه در هر دو مجاز؛ داخل هر دامنه یکتا + fingerprint | خیر |
| 2 | زمان پیش‌فرض | `CURRENT_TIMESTAMP` دیتابیس، نه ساعت برنامه | خیر |
| 3 | نگاشت خطا | SQLSTATE به‌عنوان طبقه‌بندی اولیه + متن/databaseText و نام Constraint برای تشخیص دقیق | خیر |
| 4 | متدهای خواندن | افزودن `getHoldById`، `getHoldsByAccount`، `getHoldEvents` با داده کامل وضعیت و تاریخچه | خیر |
| 5 | قرارداد Controller | 5 Endpoint، پاسخ 201/200 با `replayed`، نگاشت کامل HTTP، بدنه JSON مشخص | خیر |

---

## 2. تصمیم ۱ — دامنه Idempotency

### 2.1 وضعیت فعلی Schema (تأییدشده)

- `uq_transaction_api_idempotency` روی `transaction(channel, idempotency_key)` با شرط `channel = 'API'`.
- `uq_hold_api_idempotency` روی `account_hold(idempotency_key)` برای مقادیر غیر NULL.
- `uq_account_hold_event_idempotency` روی `account_hold_events(hold_id, idempotency_key)`.

### 2.2 تصمیم

- **دامنه Transaction (Deposit/Transfer):** کلید در `transaction` و فقط برای `channel='API'` یکتاست.
- **دامنه Hold Creation:** کلید در `account_hold` یکتاست (سراسری بین همه حساب‌ها).
- **دامنه Hold Event:** کلید در `account_hold_events` به‌صورت `(hold_id, idempotency_key)` یکتاست.
- یک کلید یکسان می‌تواند هم‌زمان در `transaction` و `account_hold` استفاده شود؛ این دو دامنه مستقل‌اند.
- کنترل Fingerprint در هر دامنه مستقل باقی می‌ماند:
  - Transaction: fingerprint موجود در `transaction.request_fingerprint`.
  - Hold: fingerprint موجود در `account_hold.request_fingerprint`.
  - Event: تطبیق پیلود با ردیف موجود در `account_hold_events` (بدون ستون Fingerprint).

### 2.3 اثر بر Advisory Lock

پیشوندهای موجود عمداً مستقل‌اند:

| عملیات | پیشوند کلید Advisory Lock |
|---|---|
| Deposit/Transfer | `API:` |
| Hold Creation | `HOLD:` |
| Hold Event | `HOLD-EVENT:` |

بنابراین استفاده از یک کلید در دو دامنه، باعث Serialize شدن غیرضروری نمی‌شود. داخل هر دامنه، کلید یکسان سریال می‌شود.

### 2.4 نتیجه

هیچ Migration یا تغییر Schema لازم نیست. رفتار فعلی HoldService با این تصمیم سازگار است و فقط باید در Controller/مستندات به‌صراحت مستند شود.

---

## 3. تصمیم ۲ — زمان پیش‌فرض از `CURRENT_TIMESTAMP` دیتابیس

### 3.1 تغییر پیشنهادی

در `HoldService` یک Helper اضافه شود:

```cpp
bool databaseNow(QSqlDatabase& db, QDateTime* now, QString* error);
// SELECT CURRENT_TIMESTAMP
```

قواعد:

1. مقدار `CURRENT_TIMESTAMP` **داخل همان تراکنش** و پس از شروع تراکنش خوانده شود.
2. برای `createHold` وقتی `effectiveAt` ارسال نشده باشد:
   - `effectiveAtInsert = dbNow`
   - در INSERT همان مقدار به‌صورت `CAST(:effective_at AS TIMESTAMPTZ)` بایند شود (به‌جای تکیه بر DEFAULT ستون).
   - مقایسه `expiresAt > effectiveAt` با `dbNow` انجام شود.
3. برای `createEvent` وقتی `effectiveAt` ارسال نشده باشد:
   - `effectiveAtInsert = dbNow`
   - همان مقدار در INSERT بایند شود.
4. کنترل‌های زمانی Trigger (`future`, `not yet effective`, `before effective`) نیز با `dbNow` انجام شوند تا سرویس و دیتابیس از یک منبع زمان استفاده کنند.
5. خطای خواندن `CURRENT_TIMESTAMP` → Rollback و `HOLD_STATE_READ_FAILED` (500).

### 3.2 اثر بر Fingerprint

- زمان پیش‌فرض سرور **وارد Fingerprint نمی‌شود**؛ در حالت отсутствие زمان، رشته خالی استفاده می‌شود. بنابراین تغییر منبع زمان، Fingerprint را تغییر نمی‌دهد.
- زمان ارسالی کلاینت همچنان نرمال‌شده (UTC با میلی‌ثانیه) وارد Fingerprint می‌شود.
- نتیجه: هیچ Migration داده‌ای برای Fingerprint لازم نیست و ردیف‌های موجود سازگار می‌مانند.

### 3.3 اثر بر Retry

| سناریو | `createHold` | `createEvent` |
|---|---|---|
| هر دو درخواست بدون زمان | 200 (Fingerprint یکسان) | 200 (زمان در Pre-check مقایسه نمی‌شود) |
| هر دو با زمان یکسان | 200 | 200 |
| اول با زمان، Retry بدون زمان | 409 (زمان جزء Fingerprint است) | 200 (زمان فقط اگر ارسال شود مقایسه می‌شود) |
| اول بدون زمان، Retry با زمان | 409 | 200 اگر زمان ارسالی برابر مقدار ذخیره‌شده باشد؛ وگرنه 409 |
| زمان ارسالی متفاوت | 409 | 409 |

نکته: رفتار `createHold` با بخش ۸.۱ طراحی (زمان در Fingerprint) هم‌خوان است. اگر تصمیم بر آن باشد که Retry Hold نیز مانند Event نسبت به زمان حساس نباشد، باید Fingerprint Hold تغییر کند؛ در این طرح چنین تغییری پیشنهاد نمی‌شود.

### 3.4 نکته درباره `CURRENT_TIMESTAMP` و Trigger

- `CURRENT_TIMESTAMP` در PostgreSQL زمان شروع تراکنش است.
- Trigger از `clock_timestamp()` استفاده می‌کند که همیشه ≥ زمان شروع تراکنش است.
- بنابراین مقدار پیش‌فرض `effective_at = CURRENT_TIMESTAMP` هرگز «Future Dated» محسوب نمی‌شود.
- اگر تطابق دقیق‌تر با Trigger لازم باشد، می‌توان برای مقایسه‌ها از `clock_timestamp()` استفاده کرد؛ ولی طبق تصمیم شما `CURRENT_TIMESTAMP` مبنا قرار می‌گیرد.

### 3.5 نقاط تغییر در کد فعلی

- `src/application/holdservice.cpp:506-516` (تعیین زمان Hold)
- `src/application/holdservice.cpp:1044-1079` (تعیین و کنترل زمان Event)
- افزودن Helper جدید در Anonymous Namespace
- هیچ تغییری در `fingerprintForHold` یا `fingerprintForEvent` لازم نیست.

---

## 4. تصمیم ۳ — نگاشت مقاوم‌تر خطاهای Trigger

### 4.1 مشکل فعلی

نگاشت فعلی فقط `QSqlError::text()` را جست‌وجو می‌کند (`holdservice.cpp:284-307`). در QPSQL معمولاً متن خطای سرور در `text()` و `databaseText()` وجود دارد، ولی برای اطمینان کامل باید هر دو بررسی شوند و SQLSTATE نیز به‌عنوان طبقه‌بندی اولیه استفاده شود.

### 4.2 طرح پیشنهادی

ساختار نگاشت:

```cpp
struct TriggerErrorRule {
    QString sqlState;   // مثلاً "23514"
    QString code;       // مثلاً "ACCOUNT_HOLD_NOT_EXPIRED"
    bool rejected;      // true → Rejected، false → Failed
};
```

الگوریتم:

1. `const QString sqlState = error.nativeErrorCode();`
2. `const QString text = error.databaseText() + "\n" + error.text();`
3. ابتدا تطبیق دقیق کد Trigger در متن (کدها از هم زیررشته نیستند).
4. سپس تطبیق نام Constraint برای مسیرهای Unique.
5. اگر SQLSTATE `23514` بود ولی هیچ کد Trigger شناخته‌شده‌ای در متن نبود → خطای CHECK Constraint عادی → `HOLD_EVENT_CREATE_FAILED` یا `HOLD_CREATE_FAILED` (500).
6. اگر SQLSTATE `23503` بود و متن شامل `ACCOUNT_HOLD_NOT_FOUND` بود → Rejected با همان کد.
7. اگر SQLSTATE `23505` بود:
   - متن شامل `uq_hold_api_idempotency` یا `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED` → مسیر Idempotency.
   - متن شامل `uq_account_hold_event_idempotency` → تلاش برای خواندن Event موجود با `(hold_id, key)`؛ اگر یافت شد نتیجه قبلی، وگرنه `HOLD_EVENT_CREATE_FAILED`.
8. کدهای `ACCOUNT_HOLD_IS_IMMUTABLE` و `ACCOUNT_HOLD_EVENTS_ARE_IMMUTABLE` → Failed 500.
9. ناشناخته → Failed 500 با کد عمومی مرحله.

### 4.3 جدول نگاشت پیشنهادی

| Trigger Code | SQLSTATE | نوع | HTTP نهایی |
|---|---|---|---|
| `ACCOUNT_HOLD_NOT_FOUND` | 23503 | Rejected | 404 |
| `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED` | 23505 | Rejected | 409 |
| `ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED` | 23514 | Rejected | 422 |
| `ACCOUNT_HOLD_NOT_YET_EFFECTIVE` | 23514 | Rejected | 422 |
| `ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME` | 23514 | Rejected | 422 |
| `ACCOUNT_HOLD_NOT_EXPIRED` | 23514 | Rejected | 422 |
| `EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE` | 23514 | Rejected | 422 |
| `ACCOUNT_HOLD_ALREADY_FULLY_RELEASED` | 23514 | Rejected | 422 |
| `ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT` | 23514 | Rejected | 422 |
| `PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT` | 23514 | Rejected | 422 |
| `FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT` | 23514 | Rejected | 422 |
| `ACCOUNT_HOLD_IS_IMMUTABLE` | 23514 | Failed | 500 |
| `ACCOUNT_HOLD_EVENTS_ARE_IMMUTABLE` | 23514 | Failed | 500 |

### 4.4 استفاده در Service و Controller

- Service فقط `code` و `status` را برمی‌گرداند.
- Controller یک تابع `httpStatusForCode(code)` دارد که جدول بالا و کدهای سرویس را به HTTP نگاشت می‌کند.
- برای اطمینان، در مرحله Controller حداقل این سناریوها آزمون شوند:
  - Hold منقضی + `FULL_RELEASE` → `EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE`.
  - `PARTIAL_RELEASE` با مبلغ برابر باقی‌مانده → `PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT`.
  - `effectiveAt` در آینده → `ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED`.
  - کلید تکراری با پیلود متفاوت → `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED` یا `IDEMPOTENCY_KEY_REUSED`.

---

## 5. تصمیم ۴ — متدهای خواندن (Read API)

### 5.1 ساختار داده‌ها

در `holdservice.h` تعریف شوند:

```cpp
struct HoldStatusData {
    qint64 holdId = 0;
    qint64 accountId = 0;
    qint64 currencyId = 0;

    QString originalAmount;
    QString releasedAmount;
    QString remainingAmount;
    QString currentlyBlockedAmount;

    QString reasonCode;
    QString reasonDescription;   // خالی = NULL
    QString referenceType;       // خالی = NULL
    QString referenceId;         // خالی = NULL

    QString status;              // SCHEDULED / ACTIVE / ...
    QDateTime createdAt;
    QDateTime effectiveAt;
    QDateTime expiresAt;         // invalid = NULL

    int releaseEventCount = 0;
    QString lastReleaseEventType; // خالی = NULL
};

struct HoldEventData {
    qint64 eventId = 0;
    qint64 holdId = 0;
    QString eventType;
    QString amount;
    QDateTime effectiveAt;
    QDateTime createdAt;
    QString referenceType;       // خالی = NULL
    QString referenceId;         // خالی = NULL
    QString reasonCode;
};
```

قواعد نمایش:

- مبالغ همیشه `QString` می‌مانند تا دقت `NUMERIC` حفظ شود؛ تبدیل به `double` ممنوع است.
- `releasedAmount = GREATEST(original_amount - remaining_amount, 0)` و در SQL محاسبه می‌شود.
- فیلدهای اختیاری NULL در JSON به `null` تبدیل می‌شوند.
- زمان‌ها به‌صورت ISO 8601 با میلی‌ثانیه و UTC برگردانده می‌شوند: `toUTC().toString(Qt::ISODateWithMs)`.

### 5.2 Result Types

```cpp
struct HoldQueryResult {
    enum class Status { Found, NotFound, Failed };
    Status status = Status::Failed;
    HoldStatusData data;
    QString code;
    QString message;
};

struct HoldListResult {
    enum class Status { Completed, Failed };
    Status status = Status::Failed;
    QVector<HoldStatusData> holds;
    QString code;
    QString message;
};

struct HoldEventsResult {
    enum class Status { Completed, HoldNotFound, Failed };
    Status status = Status::Failed;
    QVector<HoldEventData> events;
    QString code;
    QString message;
};
```

### 5.3 امضاهای پیشنهادی

```cpp
HoldQueryResult getHoldById(qint64 holdId);
HoldListResult getHoldsByAccount(qint64 accountId);
HoldEventsResult getHoldEvents(qint64 holdId);
```

اعتبارسنجی ورودی در Service:

- `holdId <= 0` → `Failed` یا `HOLD_INVALID_HOLD_ID` (Controller معمولاً قبل از Service این را 400 می‌کند).
- `accountId <= 0` → `Failed` یا `HOLD_INVALID_ACCOUNT_ID`.

### 5.4 SQL پیشنهادی

#### `getHoldById`

```sql
SELECT
    hold_id,
    account_id,
    currency_id,
    original_amount,
    GREATEST(original_amount - remaining_amount, 0) AS released_amount,
    remaining_amount,
    currently_blocked_amount,
    status,
    reason_code,
    reason_description,
    reference_type,
    reference_id,
    created_at,
    effective_at,
    expires_at,
    release_event_count,
    last_release_event_type
FROM account_hold_status
WHERE hold_id = :hold_id
```

- `exec() == false` → `Failed` با `HOLD_STATE_READ_FAILED`.
- `exec() == true` و `next() == false` → `NotFound` با `HOLD_NOT_FOUND`.
- وجود ردیف فقط با `next()` سنجیده شود.

#### `getHoldsByAccount`

```sql
SELECT
    hold_id,
    account_id,
    currency_id,
    original_amount,
    GREATEST(original_amount - remaining_amount, 0) AS released_amount,
    remaining_amount,
    currently_blocked_amount,
    status,
    reason_code,
    reason_description,
    reference_type,
    reference_id,
    created_at,
    effective_at,
    expires_at,
    release_event_count,
    last_release_event_type
FROM account_hold_status
WHERE account_id = :account_id
ORDER BY hold_id
```

- `exec() == false` → `Failed` با `HOLD_STATE_READ_FAILED`.
- صفر ردیف → `Completed` با لیست خالی (`count = 0`).
- تصمیم پیشنهادی: برای حساب ناموجود نیز `200` با لیست خالی برگردد؛ بررسی وجود حساب در این Endpoint اجباری نیست. اگر سیاست بانکی متفاوتی لازم است، این تصمیم باید جداگانه تأیید شود.
- افزودن فیلتر `status` یا Pagination در این مرحله پیشنهاد نمی‌شود.

#### `getHoldEvents`

مرحله ۱ — بررسی وجود Hold:

```sql
SELECT 1 FROM account_hold WHERE id = :hold_id
```

- `exec() == false` → `Failed` با `HOLD_STATE_READ_FAILED`.
- `next() == false` → `HoldNotFound` با `HOLD_NOT_FOUND`.

مرحله ۲ — خواندن رویدادها:

```sql
SELECT
    id,
    hold_id,
    event_type,
    amount,
    effective_at,
    created_at,
    reference_type,
    reference_id,
    reason_code
FROM account_hold_events
WHERE hold_id = :hold_id
ORDER BY id
```

- `exec() == false` → `Failed` با `HOLD_STATE_READ_FAILED`.
- صفر ردیف → `Completed` با لیست خالی.
- `idempotency_key` در خروجی نمایش داده نمی‌شود.

### 5.5 Transaction برای خواندن

- هر یک از متدهای خواندن با Query مستقیم و بدون تراکنش صریح کافی است.
- اگر Snapshot کاملاً یکدست بین وجود Hold و لیست Eventها لازم باشد، می‌توان `getHoldEvents` را در یک تراکنش Read Only اجرا کرد:
  ```sql
  BEGIN;
  SET TRANSACTION READ ONLY;
  ...
  COMMIT;
  ```
  ولی چون Holdها Immutable هستند و رویدادها فقط اضافه می‌شوند، این کار برای این API ضروری نیست. پیشنهاد: بدون تراکنش، مگر اینکه در آزمون هم‌زمانی نیاز مشخص شود.

### 5.6 افزودن Snapshot به نتیجه Create

برای پاسخ‌های 201/200 نیازی به Query دوم پس از Commit نیست. پیشنهاد:

```cpp
struct HoldResult {
    // ... فیلدهای فعلی
    HoldStatusData snapshot;
    bool hasSnapshot = false;
};

struct HoldEventResult {
    // ... فیلدهای فعلی
    HoldStatusData snapshot;
    bool hasSnapshot = false;
};
```

- Service همان وضعیتی را که پیش از Commit می‌خواند، در `snapshot` نگه می‌دارد.
- در همه مسیرهای `Completed` (ایجاد جدید، Retry، Unique Conflict، Trigger RETURN NULL) `hasSnapshot = true`.
- Controller پاسخ را از `snapshot` می‌سازد؛ این کار از Query دوم و Race پس از Commit جلوگیری می‌کند.
- این تغییر در `holdservice.h` و `holdservice.cpp` در مرحله پیاده‌سازی Controller لازم است.

### 5.7 JSON خروجی خواندن

#### `GET /api/v1/holds/{id}`

```json
{
  "success": true,
  "data": {
    "holdId": 1,
    "accountId": 1,
    "currencyId": 1,
    "originalAmount": "500",
    "releasedAmount": "200",
    "remainingAmount": "300",
    "currentlyBlockedAmount": "300",
    "reasonCode": "DISPUTE",
    "reasonDescription": "Customer dispute hold",
    "referenceType": "TRANSACTION",
    "referenceId": "42",
    "status": "PARTIALLY_RELEASED",
    "createdAt": "2026-11-01T06:30:01.000Z",
    "effectiveAt": "2026-11-01T06:30:00.000Z",
    "expiresAt": "2026-12-01T06:30:00.000Z",
    "releaseEventCount": 1,
    "lastReleaseEventType": "PARTIAL_RELEASE"
  }
}
```

#### `GET /api/v1/holds?accountId={id}`

```json
{
  "success": true,
  "data": {
    "accountId": 1,
    "holds": [
      {
        "holdId": 1,
        "accountId": 1,
        "currencyId": 1,
        "originalAmount": "500",
        "releasedAmount": "0",
        "remainingAmount": "500",
        "currentlyBlockedAmount": "500",
        "reasonCode": "DISPUTE",
        "reasonDescription": null,
        "referenceType": null,
        "referenceId": null,
        "status": "ACTIVE",
        "createdAt": "2026-11-01T06:30:01.000Z",
        "effectiveAt": "2026-11-01T06:30:00.000Z",
        "expiresAt": null,
        "releaseEventCount": 0,
        "lastReleaseEventType": null
      }
    ],
    "count": 1
  }
}
```

#### `GET /api/v1/holds/{id}/events`

```json
{
  "success": true,
  "data": {
    "holdId": 1,
    "events": [
      {
        "eventId": 1,
        "holdId": 1,
        "eventType": "PARTIAL_RELEASE",
        "amount": "200",
        "effectiveAt": "2026-11-01T08:30:00.000Z",
        "createdAt": "2026-11-01T08:30:01.000Z",
        "referenceType": null,
        "referenceId": null,
        "reasonCode": "PARTIAL_SETTLEMENT"
      }
    ],
    "count": 1
  }
}
```

---

## 6. تصمیم ۵ — قرارداد Controller

### 6.1 فهرست Endpointها

| Method | Path | Handler | موفق | خطاهای اصلی |
|---|---|---|---|---|
| `POST` | `/api/v1/holds` | `createHold` | 201 جدید / 200 Retry | 400، 404، 409، 422، 500، 503 |
| `GET` | `/api/v1/holds/{id}` | `getHoldById` | 200 | 400، 404، 500، 503 |
| `GET` | `/api/v1/holds?accountId={id}` | `getHoldsByAccount` | 200 | 400، 500، 503 |
| `POST` | `/api/v1/holds/{id}/events` | `createEvent` | 201 جدید / 200 Retry | 400، 404، 409، 422، 500، 503 |
| `GET` | `/api/v1/holds/{id}/events` | `getEvents` | 200 | 400، 404، 500، 503 |

### 6.2 Request — `POST /api/v1/holds`

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

قواعد:

- `accountId`: عدد صحیح مثبت و ≤ `2^53-1`.
- `amount`: فقط JSON string پذیرفته می‌شود. ورودی عددی JSON رد می‌شود و هیچ تبدیل یا گردکردنی انجام نمی‌شود. مقدار باید مثبت، بدون فاصله ابتدا/انتها، بدون قالب علمی و با حداکثر ۸ رقم اعشار باشد. `HoldService` مقدار معتبر را برای Fingerprint و ذخیره‌سازی نرمال می‌کند (حذف صفرهای انتهایی).
- `reasonCode`: رشته غیرخالی.
- `reasonDescription`، `referenceType`، `referenceId`، `effectiveAt`، `expiresAt`: اختیاری؛ `undefined`/`null`/رشته مجاز است.
- `idempotencyKey`: رشته غیرخالی.
- فیلدهای ناشناخته نادیده گرفته می‌شوند (مطابق Controllerهای موجود).

### 6.3 Response — `POST /api/v1/holds`

- اولین ایجاد موفق: `201 Created`.
- Retry با `replayed = true`: `200 OK`.
- بدنه در هر دو حالت یکسان است و از `snapshot` ساخته می‌شود:

```json
{
  "success": true,
  "data": {
    "holdId": 1,
    "accountId": 1,
    "currencyId": 1,
    "originalAmount": "500",
    "releasedAmount": "0",
    "remainingAmount": "500",
    "currentlyBlockedAmount": "500",
    "reasonCode": "DISPUTE",
    "reasonDescription": "Customer dispute hold",
    "referenceType": "TRANSACTION",
    "referenceId": "42",
    "status": "ACTIVE",
    "createdAt": "2026-11-01T06:30:01.000Z",
    "effectiveAt": "2026-11-01T06:30:00.000Z",
    "expiresAt": "2026-12-01T06:30:00.000Z",
    "releaseEventCount": 0,
    "lastReleaseEventType": null,
    "replayed": false,
    "code": "HOLD_CREATED",
    "message": "Hold created"
  }
}
```

- `replayed` افزودنی اختیاری برای شفافیت است؛ تفکیک اصلی 200/201 با HTTP Status انجام می‌شود. اگر مایل به حفظ دقیق قرارداد بخش ۱۳.۱ طراحی هستید، می‌توان `replayed` را از بدنه حذف کرد.
- `code` و `message` فقط در پاسخ Create می‌آیند؛ در GETها نمی‌آیند.

### 6.4 Request — `POST /api/v1/holds/{id}/events`

نمونه PARTIAL:

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

نمونه FULL/EXPIRY/CANCELLATION:

```json
{
  "eventType": "FULL_RELEASE",
  "effectiveAt": "2026-11-01T12:00:00+03:30",
  "reasonCode": "RESOLVED",
  "idempotencyKey": "evt-002"
}
```

قواعد:

- `eventType`: اجباری و یکی از `PARTIAL_RELEASE`، `FULL_RELEASE`، `EXPIRY_RELEASE`، `CANCELLATION_RELEASE`.
- `amount`: فقط برای `PARTIAL_RELEASE` اجباری و معتبر؛ در بقیه انواع نادیده گرفته می‌شود. اگر ارسال شود، باید JSON string معتبر با حداکثر ۸ رقم اعشار باشد؛ ورودی عددی JSON رد می‌شود.
- `effectiveAt`: اختیاری.
- `reasonCode`: اجباری.
- `referenceType`/`referenceId`: اختیاری.
- `idempotencyKey`: اجباری.

### 6.5 Response — `POST /api/v1/holds/{id}/events`

- اولین ایجاد: `201 Created`.
- Retry: `200 OK`.
- بدنه:

```json
{
  "success": true,
  "data": {
    "eventId": 1,
    "holdId": 1,
    "eventType": "PARTIAL_RELEASE",
    "amount": "200",
    "effectiveAt": "2026-11-01T08:30:00.000Z",
    "reasonCode": "PARTIAL_SETTLEMENT",
    "remainingAmount": "300",
    "holdStatus": "PARTIALLY_RELEASED",
    "currentlyBlockedAmount": "300",
    "releaseEventCount": 1,
    "lastReleaseEventType": "PARTIAL_RELEASE",
    "replayed": false,
    "code": "HOLD_EVENT_CREATED",
    "message": "Hold event created"
  }
}
```

- `remainingAmount`، `holdStatus`، `currentlyBlockedAmount`، `releaseEventCount` و `lastReleaseEventType` از `snapshot` پس از Event خوانده می‌شوند.

### 6.6 نگاشت HTTP پیشنهادی

#### 400 Bad Request

`INVALID_JSON`، `INVALID_JSON_OBJECT`، `INVALID_HOLD_REQUEST`، `INVALID_ACCOUNT_ID`، `INVALID_HOLD_ID`، `MISSING_ACCOUNT_ID`، `MISSING_IDEMPOTENCY_KEY`، `INVALID_AMOUNT`، `INVALID_DESCRIPTION`، `HOLD_INVALID_ACCOUNT_ID`، `HOLD_INVALID_HOLD_ID`، `HOLD_INVALID_AMOUNT`، `HOLD_INVALID_IDEMPOTENCY_KEY`، `HOLD_INVALID_REASON_CODE`، `HOLD_INVALID_DESCRIPTION`، `HOLD_INVALID_REFERENCE`، `HOLD_INVALID_EVENT_TYPE`، `HOLD_INVALID_EFFECTIVE_AT`، `HOLD_INVALID_EXPIRES_AT`.

#### 404 Not Found

`ACCOUNT_NOT_FOUND`، `HOLD_NOT_FOUND`، `ACCOUNT_HOLD_NOT_FOUND`.

#### 409 Conflict

`IDEMPOTENCY_KEY_REUSED`، `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED`.

#### 422 Unprocessable Entity

`ACCOUNT_BALANCE_NOT_INITIALIZED`، `ACCOUNT_NOT_ACTIVE`، `PRODUCT_NOT_ACTIVE`، `CUSTOMER_LEDGER_NOT_POSTABLE`، `CUSTOMER_LEDGER_CURRENCY_MISMATCH`، `INSUFFICIENT_AVAILABLE_FUNDS`، `ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED`، `ACCOUNT_HOLD_NOT_YET_EFFECTIVE`، `ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME`، `ACCOUNT_HOLD_NOT_EXPIRED`، `EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE`، `ACCOUNT_HOLD_ALREADY_FULLY_RELEASED`، `ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT`، `PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT`، `FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT`.

#### 500 Internal Server Error

`HOLD_DATABASE_UNAVAILABLE`، `HOLD_TRANSACTION_START_FAILED`، `HOLD_IDEMPOTENCY_LOCK_FAILED`، `HOLD_IDEMPOTENCY_LOOKUP_FAILED`، `HOLD_ACCOUNT_LOCK_FAILED`، `HOLD_BALANCE_LOCK_FAILED`، `HOLD_LOCK_FAILED`، `HOLD_STATE_READ_FAILED`، `HOLD_CREATE_FAILED`، `HOLD_EVENT_CREATE_FAILED`، `DATABASE_COMMIT_FAILED`.

#### 503 Service Unavailable

`HOLD_SERVICE_UNAVAILABLE` در صورت عدم تنظیم Controller در Router.

### 6.7 تفکیک 200 و 201

- `HoldResult::replayed == false` و `HoldEventResult::replayed == false` → `201 Created`.
- `replayed == true` → `200 OK`.
- برای `Rejected`/`Failed` مقدار `replayed` اهمیتی ندارد؛ وضعیت HTTP از جدول خطا تعیین می‌شود.
- در پاسخ Create، فیلد `replayed` نیز برای شفافیت پیشنهاد می‌شود.

---

## 7. ساختار Controller و Router

### 7.1 `HoldController`

```cpp
class HoldController
{
public:
    explicit HoldController(HoldService& service);

    HttpResponse createHold(const QByteArray& body);
    HttpResponse getHoldById(qint64 id);
    HttpResponse getHoldsByAccount(const QByteArray& query);
    HttpResponse createEvent(qint64 id, const QByteArray& body);
    HttpResponse getEvents(qint64 id);

private:
    QJsonObject holdToJson(const HoldStatusData& hold) const;
    QJsonObject eventToJson(const HoldEventData& event) const;
    QJsonObject eventCreateToJson(
        const HoldEventResult& result) const;

    HoldService& service;
};
```

- `holdToJson` برای GET detail، GET list و پاسخ‌های Create استفاده می‌شود.
- `eventToJson` برای GET events.
- `eventCreateToJson` از `result.snapshot` و فیلدهای Event استفاده می‌کند.
- پارس JSON و اعتبارسنجی ورودی مطابق `DepositController`/`TransferController` باشد.
- برای فیلدهای اختیاری، تابع کمکی `optionalString` پیشنهاد می‌شود که `undefined`/`null` را به رشته خالی تبدیل کند.

### 7.2 Router

- `src/web/httprouter.h`:
  - `#include "holdcontroller.h"`
  - `void setHoldController(HoldController* controller);`
  - `HoldController* holdController = nullptr;`
- `src/web/httprouter.cpp` بعد از بخش Deposit:
  - `POST /api/v1/holds`
  - `GET /api/v1/holds`
  - `GET /api/v1/holds/{id}`
  - `POST /api/v1/holds/{id}/events`
  - `GET /api/v1/holds/{id}/events`
- در همه مسیرها اگر `holdController == nullptr` → `503` با `HOLD_SERVICE_UNAVAILABLE`.
- برای شناسه‌ها از `parsePositiveId` موجود استفاده شود.
- برای `GET /api/v1/holds` از `QUrlQuery` استفاده شود؛ `accountId` خالی → `400 MISSING_ACCOUNT_ID`، نامعتبر → `400 HOLD_INVALID_ACCOUNT_ID`.
- الگوی مسیرها باید طوری باشد که `/holds/{id}/events` قبل از `/holds/{id}` بررسی شود یا شرط `suffix` دقیق باشد تا مسیرها اشتباه تطبیق نکنند.

### 7.3 `main.cpp`

```cpp
HoldService holdService(db);
HoldController holdController(holdService);
...
router.setHoldController(&holdController);
```

### 7.4 `CMakeLists.txt`

افزودن:

```text
src/web/holdcontroller.h
src/web/holdcontroller.cpp
```

### 7.5 فایل‌های نهایی مرحله Controller

| فایل | عملیات |
|---|---|
| `src/web/holdcontroller.h` | ایجاد |
| `src/web/holdcontroller.cpp` | ایجاد |
| `src/application/holdservice.h` | تغییر: افزودن Read structs، snapshot و read methods |
| `src/application/holdservice.cpp` | تغییر: DB now، error mapping، read methods، snapshot |
| `src/web/httprouter.h` | تغییر: HoldController pointer/setter |
| `src/web/httprouter.cpp` | تغییر: ۵ مسیر Hold |
| `src/main.cpp` | تغییر: ساخت و اتصال HoldController |
| `CMakeLists.txt` | تغییر: افزودن فایل‌های Controller |

---

## 8. خلاصه تغییرات لازم در HoldService

1. **زمان پیش‌فرض:** افزودن `databaseNow()` و جایگزینی `QDateTime::currentDateTimeUtc()` در تعیین زمان پیش‌فرض و مقایسه‌های زمانی.
2. **Snapshot:** افزودن `HoldStatusData` به `HoldResult`/`HoldEventResult` و مقداردهی در همه مسیرهای Completed.
3. **Read methods:** افزودن `getHoldById`، `getHoldsByAccount`، `getHoldEvents`.
4. **Error mapping:** مقاوم‌سازی `triggerErrorCode` با SQLSTATE، `databaseText()` و نام Constraint.
5. **بدون تغییر:** دامنه Idempotency، Schema، ترتیب قفل، Fingerprint (به‌جز منبع زمان پیش‌فرض که وارد Fingerprint نمی‌شود).

---

## 9. برنامه آزمون مرحله Controller

### 9.1 Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

### 9.2 آزمون HTTP

1. `POST /api/v1/holds` معتبر → `201`؛ سپس `GET /holds/{id}` و بررسی VIEW.
2. همان درخواست با همان کلید → `200` و همان `holdId`.
3. همان کلید با پیلود متفاوت → `409 IDEMPOTENCY_KEY_REUSED`.
4. `POST /holds` بدون `effectiveAt` → `201`؛ بررسی `effective_at` برابر زمان دیتابیس؛ Retry → `200`.
5. `GET /holds?accountId={id}` → `200` با لیست؛ بدون `accountId` → `400 MISSING_ACCOUNT_ID`.
6. `GET /holds/{id}` ناموجود → `404 HOLD_NOT_FOUND`.
7. `PARTIAL_RELEASE` معتبر → `201`؛ بررسی `remainingAmount` و وضعیت.
8. Retry همان Event → `200` با همان `eventId`.
9. `FULL_RELEASE` → `201`؛ `remainingAmount = 0` و وضعیت `RELEASED`.
10. Retry بعد از `FULL_RELEASE` → `200` بدون Event جدید.
11. `PARTIAL_RELEASE` با مبلغ برابر باقی‌مانده → `422 PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT`.
12. `PARTIAL_RELEASE` با مبلغ بیشتر از باقی‌مانده → `422 ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT`.
13. Hold منقضی + `FULL_RELEASE` → `422 EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE`.
14. `EXPIRY_RELEASE` معتبر → `201` و وضعیت `EXPIRED`.
15. `CANCELLATION_RELEASE` روی Hold فعال → `201` و وضعیت `CANCELLED`.
16. موجودی ناکافی → `422 INSUFFICIENT_AVAILABLE_FUNDS`.
17. حساب ناموجود → `404 ACCOUNT_NOT_FOUND`.
18. حساب بدون `account_balance` → `422 ACCOUNT_BALANCE_NOT_INITIALIZED`.
19. `GET /holds/{id}/events` → `200` با تاریخچه؛ Hold ناموجود → `404`.
20. `eventType` نامعتبر، `amount` نامعتبر، `idempotencyKey` خالی و زمان نامعتبر → `400`.
21. در صورت استفاده از pointer/setter: عدم اتصال Controller → `503 HOLD_SERVICE_UNAVAILABLE`.

### 9.3 بررسی دیتابیس (فقط read-only با `corebanking_reader`)

- `account_hold`: ستون‌های Idempotency، `amount`، `effective_at`، `expires_at`.
- `account_hold_events`: تعداد و مبلغ رویدادها.
- `account_hold_status`: `remaining_amount`، `status`، `currently_blocked_amount`، `release_event_count`.
- تأیید عدم تغییر `ledger_balance` و `available_balance` توسط Hold/Release.
- ثبت شناسه‌های داده آزمایشی ایجادشده.

### 9.4 آزمون هم‌زمانی

- دو `createHold` با دو کلید متفاوت روی یک حساب → هر دو باید موجودی را به‌درستی رعایت کنند.
- دو `createEvent` با دو کلید متفاوت روی یک Hold → جمع آزادسازی نباید از اصل بیشتر شود.
- Retry هم‌زمان با کلید یکسان → فقط یک ردیف و پاسخ‌های 200/201 سازگار.
- ترجیحاً یک آزمون هم‌زمانی برای Hold و Deposit/Transfer روی یک حساب.

---

## 10. سؤالات باز و تصمیم‌های باقی‌مانده

1. **لیست برای حساب ناموجود:** پیشنهاد این طرح `200` با لیست خالی است. اگر `404 ACCOUNT_NOT_FOUND` ترجیح داده می‌شود، یک Query وجود حساب اضافه شود.
2. **نمایش `replayed` در بدنه:** پیشنهاد افزودن برای شفافیت است؛ اگر قرارداد دقیق بخش ۱۳ طراحی باید بدون تغییر بماند، حذف شود.
3. **فرمت زمان خروجی:** پیشنهاد UTC با میلی‌ثانیه (`...Z`). اگر کلاینت Offset محلی می‌خواهد، قرارداد باید جداگانه تعیین شود.
4. **Pagination لیست Holdها و Eventها:** در این مرحله پیشنهاد نمی‌شود.
5. **فیلتر وضعیت در لیست:** در این مرحله پیشنهاد نمی‌شود.
6. **استفاده از `CURRENT_TIMESTAMP` در برابر `clock_timestamp()`:** طبق تصمیم شما `CURRENT_TIMESTAMP` مبنا است؛ اگر اختلاف مرزی با Trigger مشکلی ایجاد کرد، می‌توان به `clock_timestamp()` تغییر داد.
7. **یکتایی سراسری کلید بین Transaction و Hold:** طبق تصمیم ۱ مستقل است؛ Migration جدید لازم نیست.

---

## 11. تأیید عدم تغییر

در تهیه این طرح:

- هیچ فایل کد در `src/` تغییر نکرد.
- هیچ Build اجرا نشد.
- هیچ Migration، DDL یا DML اجرا نشد.
- تنها فایل ایجادشده همین گزارش است: `docs/opencode-reports/HOLD_CONTROLLER_DESIGN.md`.

منتظر تأیید برای شروع مرحله Controller/Route یا اصلاح این طرح هستم.


