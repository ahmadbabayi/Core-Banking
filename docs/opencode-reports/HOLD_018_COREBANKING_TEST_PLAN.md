# HOLD 018 — برنامه آزمون روی دیتابیس توسعه `corebanking`

تاریخ: 2026-11-11

این سند فقط برنامه و SQLهای آماده‌شده است. هیچ دستوری در این مرحله اجرا نشد؛ نه اتصال به دیتابیس، نه API Test، نه DDL/DML و نه Migration.

توجه: بندهای این مقدمه وضعیت زمان تدوین برنامه را ثبت می‌کنند. اجرای Migration 018، به‌روزرسانی `CURRENT_SCHEMA.sql` توسط کاربر و نتایج واقعی آزمون در بخش‌های ۱۲ تا ۱۴ آمده است.

محدوده:

- دیتابیس توسعه `corebanking`؛ دیتابیس جدا ساخته نمی‌شود.
- Migration 018 و `CURRENT_SCHEMA.sql` تغییر نمی‌کنند.
- Fixtureهای Legacy باید پیش از اعمال 018 ساخته شوند. غیرفعال‌کردن Trigger یا Constraint راه عادی آزمون نیست.
- قرارداد فعلی `amount` برای `FULL_RELEASE` در Legacy و نگاشت HTTP `422` حفظ می‌شود.

---

## 1. وضعیت کد و تصمیم Legacy Hold

- برای Retry Holdهای Legacy با Timestamp دارای بیش از سه رقم اعشار، مسیر سازگاری صرفاً در کد اضافه شد:
  - Fingerprint فعلی با Parser جدید و Truncate ساخته می‌شود.
  - یک Fingerprint کاندید Legacy نیز با همان Parser قبلی Qt و Round قبلی ساخته می‌شود.
  - Replay فقط وقتی پذیرفته می‌شود که Fingerprint ذخیره‌شده با یکی از این دو برابر باشد.
  - این تغییر Schema یا قرارداد HTTP جدیدی اضافه نمی‌کند.
- Timestamp بدون Timezone در Hold همچنان با قرارداد جدید رد می‌شود. پشتیبانی از Retry قدیمی برای ورودی بدون Timezone نیازمند تصمیم درباره تقدم اعتبارسنجی و قرارداد API است و در این مرحله اعمال نشد.

---

## 2. پیش‌نیازهای قطعی

1. Backup کامل `corebanking` پیش از هر تغییر.
2. توقف سرویس قدیمی پیش از اعمال 018.
3. آماده‌بودن باینری جدید ساخته‌شده از working tree فعلی برای اجرا بلافاصله پس از 018.
4. دسترسی خواندن با نقش `corebanking_reader` برای بررسی‌ها.
5. اجرای Migration و هر DML مستقیم فقط توسط کاربر.
6. ثبت شناسه‌های Hold/Event ساخته‌شده و کلیدهای Idempotency در طول آزمون.

---

## 3. بررسی فقط‌خواندنی پیش از 018

این Queryها فقط `SELECT` هستند و وضعیت فعلی Schema و داده را مشخص می‌کنند.

### 3.1 وضعیت ستون‌ها

```sql
SELECT table_name, column_name, data_type
FROM information_schema.columns
WHERE table_name IN ('account_hold', 'account_hold_events')
  AND column_name IN (
      'idempotency_key',
      'request_fingerprint',
      'fingerprint_version',
      'effective_at_provided'
  )
ORDER BY table_name, column_name;
```

انتظار پیش از 018: ستون‌های `account_hold.idempotency_key` و `account_hold.request_fingerprint` وجود دارند؛ سه ستون Metadata در `account_hold_events` وجود ندارند.

### 3.2 وضعیت توابع و Constraintهای 018

```sql
SELECT
  to_regprocedure(
    'hold_event_fingerprint_v1(bigint,text,numeric,boolean,timestamptz,text,text,text)'
  ) AS fingerprint_v1,
  to_regprocedure(
    'hold_event_fingerprint(smallint,bigint,text,numeric,boolean,timestamptz,text,text,text)'
  ) AS fingerprint_dispatcher;

SELECT conname
FROM pg_constraint
WHERE conrelid = 'account_hold_events'::regclass
  AND conname LIKE 'ck_account_hold_event_%'
ORDER BY conname;
```

انتظار پیش از 018: توابع 018 وجود ندارند و Constraintهای جدید فهرست نمی‌شوند.

### 3.3 شمارش داده و بررسی زیرمیلی‌ثانیه

```sql
SELECT
  (SELECT count(*) FROM account_hold) AS hold_count,
  (SELECT count(*) FROM account_hold_events) AS event_count;

SELECT count(*) AS holds_with_submillisecond_values
FROM account_hold
WHERE effective_at <> date_trunc('milliseconds', effective_at)
   OR (expires_at IS NOT NULL
       AND expires_at <> date_trunc('milliseconds', expires_at));
```

توجه: این Query نمی‌تواند ثابت کند ورودی اولیه بیش از سه رقم اعشار داشته یا نه؛ چون Parser قبلی مقدار را پیش از ذخیره به میلی‌ثانیه نرمال می‌کرد. این Query فقط ناهنجاری ذخیره‌شده را نشان می‌دهد.

### 3.4 فهرست Holdهای Legacy و کلیدهای آن‌ها

```sql
SELECT id, account_id, idempotency_key, amount,
       effective_at, expires_at, created_at,
       length(request_fingerprint) AS fingerprint_length
FROM account_hold
ORDER BY id;
```

### 3.5 گزینه‌های Fixture برای Legacy Event

```sql
SELECT id, hold_id, idempotency_key, event_type, amount,
       effective_at, created_at,
       reference_type, reference_id, reason_code,
       (effective_at = date_trunc('milliseconds', created_at))
         AS looks_like_absent_effective_at
FROM account_hold_events
ORDER BY id;
```

نمونه‌های مناسب:

```sql
SELECT id, hold_id, idempotency_key, event_type, amount,
       effective_at, created_at, reference_type, reference_id, reason_code
FROM account_hold_events
WHERE effective_at = date_trunc('milliseconds', created_at)
ORDER BY id DESC
LIMIT 5;

SELECT id, hold_id, idempotency_key, event_type, amount,
       effective_at, created_at, reference_type, reference_id, reason_code
FROM account_hold_events
WHERE effective_at <> date_trunc('milliseconds', created_at)
ORDER BY id DESC
LIMIT 5;
```

اگر هر دو نوع ردیف وجود داشته باشد، Fixture جدید لازم نیست و همین ردیف‌ها برای Legacy Retry بدون `effectiveAt` و با `effectiveAt` صریح استفاده می‌شوند. اگر نوع موردنیاز وجود نداشت، Fixture باید پیش از 018 و با باینری/سرویس قدیمی ساخته شود.

### 3.6 کنترل مجموع و تعداد رویدادها

```sql
SELECT h.id AS hold_id, h.amount AS original_amount,
       COALESCE(SUM(e.amount), 0) AS released_amount,
       h.amount - COALESCE(SUM(e.amount), 0) AS remaining_amount,
       count(e.id) AS event_count
FROM account_hold h
LEFT JOIN account_hold_events e ON e.hold_id = h.id
GROUP BY h.id, h.amount
ORDER BY h.id;
```

---

## 4. Fixtureهای پیش از 018

### 4.1 Legacy Event

- ترجیح: استفاده از ردیف‌های موجود بر اساس Queryهای 3.5.
- اگر ردیف مناسب نبود، پیش از 018 با باینری قدیمی و از طریق API واقعی یک Hold و Event با کلید مشخص ساخته شود.
- Payload و شناسه‌ها باید ثبت شوند تا Retry بعد از 018 دقیقاً همان درخواست باشد.
- ردیف Legacy نباید پس از 018 با غیرفعال‌کردن Trigger ساخته شود.

### 4.2 Legacy Hold با Timestamp بیش از سه رقم اعشار

- هدف: آزمون Regression سازگاری جدید.
- بررسی پیش از Migration نشان داد هیچ Hold موجودی مسیر Fallback را فعال نمی‌کند: تنها Hold با زمان صریح، `hold_id=7` با مقدار `.000` است و مرز ثانیه اجازه تفاوت Round/Truncate را نمی‌دهد؛ سایر Holdها زمان صریح در Fingerprint ندارند.
- باینری قدیمیِ پیش از 018 در Build Directory، `/home/ahmad` و `/tmp` موجود نیست.
- نیازی به Fixture پیش از 018 نیست: پس از 018 با باینری جدید یک Hold مصنوعی با `effectiveAt` سه‌رقمی مانند `2026-11-03T16:23:06.123Z` ساخته می‌شود؛ سپس Retry با `2026-11-03T16:23:06.1229Z` انجام می‌شود. Fingerprint فعلی `.122` و کاندید Legacy `.123` خواهد بود و انتظار `200 OK`، همان `holdId` و `replayed=true` است.
- اگر Fixture واقعیِ پیش از 018 الزامی باشد، باید باینری قدیمی جداگانه ساخته یا تهیه شود؛ باینری جدید فعلی نباید بازنویسی شود و ساخت Fixture با SQL و حدس Fingerprint مجاز نیست.

### 4.3 Timestamp بدون Timezone

- کاربر حفظ پاسخ `400` برای Timestamp بدون Timezone را پذیرفته است.
- برای Hold قدیمی با Timestamp بدون Timezone، Fixture جدید ساخته نمی‌شود و این حالت به‌عنوان محدودیت شناخته‌شده ثبت می‌شود.

---

## 5. ترتیب اجرا

1. Backup `corebanking`.
2. اجرای Queryهای بخش 3 و ثبت خروجی.
3. انتخاب یا ساخت Fixtureهای Legacy طبق بخش 4، پیش از 018.
4. توقف سرویس قدیمی.
5. اعمال `database/schema/018_hold_event_fingerprint.sql` توسط کاربر.
6. اجرای باینری جدید از `/home/ahmad/projects/build-CoreBanking-Desktop-Debug/CoreBanking`.
7. اجرای بررسی‌های پس از 018 (بخش 6).
8. اجرای آزمون‌های API (بخش 7) و آزمون‌های SQL کنترل‌شده (بخش 8).
9. ثبت شناسه‌ها، خروجی‌ها و نتیجه کنترل مجموع.

---

## 6. بررسی فقط‌خواندنی پس از 018

### 6.1 وجود ستون‌ها و Constraintها

```sql
SELECT table_name, column_name, data_type
FROM information_schema.columns
WHERE table_name = 'account_hold_events'
  AND column_name IN (
      'request_fingerprint',
      'fingerprint_version',
      'effective_at_provided'
  )
ORDER BY column_name;

SELECT conname
FROM pg_constraint
WHERE conrelid = 'account_hold_events'::regclass
  AND conname LIKE 'ck_account_hold_event_%'
ORDER BY conname;
```

### 6.2 کنترل Legacy بودن ردیف‌های قبلی

```sql
SELECT count(*) AS total_events,
       count(*) FILTER (
         WHERE request_fingerprint IS NULL
           AND fingerprint_version IS NULL
           AND effective_at_provided IS NULL
       ) AS legacy_events,
       count(*) FILTER (
         WHERE NOT (
           request_fingerprint IS NULL
           AND fingerprint_version IS NULL
           AND effective_at_provided IS NULL
         )
       ) AS non_legacy_events
FROM account_hold_events;
```

پیش از هر نوشتن جدید، انتظار: `legacy_events = total_events` و `non_legacy_events = 0`.

### 6.3 کنترل مجموع پس از آزمون

همان Query بخش 3.6 تکرار شود و تعداد/مجموع رویدادها با نتیجه مورد انتظار مقایسه شود.

---

## 7. آزمون‌های API پس از 018

پیش‌نیاز: باینری جدید، حساب Synthetic با موجودی کافی، کلیدهای یکتا و ثبت پاسخ‌ها.

### 7.1 تطابق Fingerprint SQL و C++

- یک Probe C++ کوچک که `HoldFingerprint::compute` یا `HoldEventFingerprintV1::fingerprint` را برای ورودی ثابت چاپ کند.
- همان ورودی با `SELECT hold_event_fingerprint(...)` در `psql`.
- حداقل یک ورودی با `reasonCode` فارسی مانند `سلام`.
- دو Hash باید دقیقاً برابر باشند.

### 7.2 Create / Replay / Conflict

```bash
curl -sS -X POST http://localhost:8080/api/v1/holds/{holdId}/events \
  -H 'Content-Type: application/json' \
  -d '{"eventType":"PARTIAL_RELEASE","amount":"0.10","reasonCode":"R","idempotencyKey":"KEY-1"}'
```

- اولین درخواست: `201 Created` و `eventId` جدید.
- همان درخواست: `200 OK`، همان `eventId`، `replayed=true`.
- همان کلید با `effectiveAt` متفاوت: `409 IDEMPOTENCY_CONFLICT`.
- همان کلید با فیلد دیگر متفاوت: `409 IDEMPOTENCY_KEY_REUSED`.

### 7.3 Legacy Event Retry

- بدون `effectiveAt` روی ردیف `looks_like_absent_effective_at = true` → Replay.
- با `effectiveAt` صریح برابر مقدار ذخیره‌شده → Replay.
- زمان متفاوت → `409`.
- کنترل: تعداد Eventها افزایش نیابد.

### 7.4 Legacy Hold Retry با Timestamp چهاررقمی

- همان درخواست Fixture بخش 4.2 پس از 018 تکرار شود.
- انتظار: `200 OK`، همان `holdId`، `replayed=true`.
- این آزمون مخصوص مسیر سازگاری Round/Truncate است.

### 7.5 رقابت

- دو درخواست هم‌زمان با کلید یکسان روی یک Hold → یک Event.
- دو درخواست هم‌زمان با کلیدهای متفاوت روی یک Hold → دو Event مستقل.
- سپس Query بخش 6.3 اجرا شود.

---

## 8. آزمون‌های SQL کنترل‌شده (فقط توسط کاربر)

این بخش DML مستقیم دارد و باید فقط توسط کاربر و فقط روی `corebanking` توسعه اجرا شود. هر آزمون داخل `BEGIN ... ROLLBACK` باشد و Trigger/Constraint غیرفعال نشود.

### 8.1 رد Metadata ناقص

```sql
BEGIN;

INSERT INTO account_hold_events (
  hold_id, idempotency_key, event_type, amount, effective_at,
  reference_type, reference_id, reason_code,
  request_fingerprint, fingerprint_version, effective_at_provided
) VALUES (
  {holdId}, 'invalid-metadata-test', 'PARTIAL_RELEASE', 0.01,
  CURRENT_TIMESTAMP, NULL, NULL, 'R',
  NULL, 1, TRUE
);

ROLLBACK;
```

انتظار: خطای `ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED` و عدم درج ردیف.

### 8.2 رد Version نامعتبر و Fingerprint بدفرمت

```sql
BEGIN;

INSERT INTO account_hold_events (
  hold_id, idempotency_key, event_type, amount, effective_at,
  reference_type, reference_id, reason_code,
  request_fingerprint, fingerprint_version, effective_at_provided
) VALUES (
  {holdId}, 'invalid-version-test', 'PARTIAL_RELEASE', 0.01,
  CURRENT_TIMESTAMP, NULL, NULL, 'R',
  repeat('a', 64), 2, TRUE
);

ROLLBACK;
```

انتظار: خطای `ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED` و عدم درج ردیف. برای Fingerprint بدفرمت، همان INSERT با `request_fingerprint = 'bad'` و `fingerprint_version = 1` تکرار شود؛ انتظار `ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID`.

### 8.3 Trigger RETURN NULL روی ردیف Legacy

پیش‌نیاز: یک ردیف Legacy با `hold_id`، `idempotency_key`، `event_type`، `amount`، `effective_at`، `reference_*` و `reason_code` مشخص از Query 3.5. مقدار `effective_at_provided` بر اساس heuristic انتخاب شود. Fingerprint باید با تابع SQL محاسبه شود، نه حدس دستی.

```sql
BEGIN;

SELECT count(*) AS before_count
FROM account_hold_events
WHERE hold_id = {holdId} AND idempotency_key = '{legacyKey}';

SELECT hold_event_fingerprint(
  1,
  {holdId},
  '{eventType}',
  {amount}::numeric,
  {effectiveAtProvided}::boolean,
  {effectiveAt}::timestamptz,
  '{reasonCode}',
  {referenceTypeOrNull},
  {referenceIdOrNull}
) AS expected_fingerprint;

INSERT INTO account_hold_events (
  hold_id, idempotency_key, event_type, amount, effective_at,
  reference_type, reference_id, reason_code,
  request_fingerprint, fingerprint_version, effective_at_provided
) VALUES (
  {holdId}, '{legacyKey}', '{eventType}', {amount}::numeric,
  {effectiveAt}::timestamptz,
  {referenceTypeOrNull}, {referenceIdOrNull}, '{reasonCode}',
  hold_event_fingerprint(
    1, {holdId}, '{eventType}', {amount}::numeric,
    {effectiveAtProvided}::boolean, {effectiveAt}::timestamptz,
    '{reasonCode}', {referenceTypeOrNull}, {referenceIdOrNull}
  ),
  1, {effectiveAtProvided}::boolean
);

SELECT count(*) AS after_count
FROM account_hold_events
WHERE hold_id = {holdId} AND idempotency_key = '{legacyKey}';

ROLLBACK;
```

انتظار: `before_count = after_count` و عدم ایجاد ردیف جدید.

---

## 9. Rollback / Restore

- برگرداندن صرف باینری قدیمی پس از 018 کافی نیست؛ Trigger جدید Metadata کامل می‌خواهد و Service قدیمی آن را نمی‌فرستد.
- Reverse Migration رسمی وجود ندارد.
- Rollback نیازمند Restore دامپ/Schema پیش از 018 یا اسکریپت معکوس تأییدشده توسط کاربر است.
- رویه Restore دامپ پیش از 018 روی Schema پیش از 018 و سپس اعمال 018، یا غیرفعال‌سازی موقت Trigger اعتبارسنجی در زمان Restore، هنوز آزمون نشده است.
- هیچ رویه‌ای در این مرحله تأییدشده اعلام نمی‌شود.

---

## 10. نتیجه پیش از اجرا

- Migration 018 آماده اجراست، مشروط به Backup و توقف سرویس قدیمی.
- پس از 018 باید فقط باینری جدید working tree فعلی اجرا شود.
- Legacy Event Fixtureها موجود هستند و از ردیف‌های موجود قابل استفاده‌اند.
- برای Legacy Hold چهاررقمی، Fixture پیش از 018 لازم نیست؛ آزمون مصنوعی پس از 018 مسیر Fallback را پوشش می‌دهد. اگر Fixture واقعی پیش از 018 الزامی باشد، باینری قدیمی موجود نیست.
- Timestamp بدون Timezone برای Holdهای Legacy به‌عنوان محدودیت شناخته‌شده با `400` باقی می‌ماند.

---

## 11. نتیجه بررسی فقط‌خواندنی پیش از Migration — 2026-11-11

اتصال فقط با نقش `corebanking_reader` انجام شد. هیچ DDL/DML، API نوشتنی، توقف سرویس یا تغییر باینری انجام نشد.

### 11.1 Schema

- `account_hold.idempotency_key` و `account_hold.request_fingerprint` وجود دارند.
- `account_hold_events` ستون‌های `request_fingerprint`، `fingerprint_version` و `effective_at_provided` را ندارد.
- توابع `hold_event_fingerprint_v1` و `hold_event_fingerprint` وجود ندارند.
- Constraintهای 018 وجود ندارند؛ Constraintهای قبلی `ck_account_hold_event_amount`، `ck_account_hold_event_idempotency_key`، `ck_account_hold_event_reason` و `ck_account_hold_event_type` موجودند.
- `uq_hold_api_idempotency` و `uq_account_hold_event_idempotency` موجودند و Triggerهای `trg_validate_account_hold_event` و `trg_account_hold_event_immutable` فعال‌اند.
- نتیجه: Schema روی 017 است و 018 اعمال نشده است.

### 11.2 داده

- تعداد Holdها: `7`؛ تعداد Eventها: `13`.
- هیچ مقدار زیرمیلی‌ثانیه در `account_hold` یا `account_hold_events` ذخیره نشده است.
- Holdهای `1` تا `6` در Fingerprint خود Timestamp صریح ندارند؛ Fingerprint آن‌ها با Timestamp خالی بازتولید شد.
- Hold `7` تنها Hold با Timestamp صریح است. Fingerprint ذخیره‌شده آن با `T1=2026-11-03T16:23:06.000Z` برابر است. به دلیل قرار گرفتن روی مرز `.000`، برای آزمون Round/Truncate چهاررقمی مناسب نیست.
- Event `8` با کلید `effat-review-1793723166-t1`، `PARTIAL_RELEASE`، مبلغ `0.01`، `reasonCode=R1` و `effectiveAt=2026-11-03T16:23:06.000Z` نمونه Legacy با زمان صریح است.
- Event `9` با کلید `effat-review-1793723166-t2`، `PARTIAL_RELEASE`، مبلغ `0.01` و `reasonCode=R2` نمونه Legacy بدون زمان است؛ `effective_at = date_trunc('milliseconds', created_at)`.
- Eventهای `10` تا `13` نیز ترکیب‌های دیگر زمان صریح/غایب را پوشش می‌دهند.
- Hold `7` وضعیت `PARTIALLY_RELEASED` با مبلغ اولیه `0.50`، آزادشده `0.06` و باقی‌مانده `0.44` دارد. حساب `1` فعال است و `available_for_hold=0.56` دارد.
- نتیجه: Fixtureهای Legacy Event موجودند؛ Fixture جدید لازم نیست. برای Hold چهاررقمی، آزمون مصنوعی پس از 018 کافی است.

### 11.3 باینری و سرویس

- تنها فایل اجرایی `CoreBanking` موجود، باینری جدید در `/home/ahmad/projects/build-CoreBanking-Desktop-Debug/CoreBanking` با زمان `2026-11-11 16:51` است.
- باینری پیش از 018 در Build Directory، `/home/ahmad` یا `/tmp` یافت نشد.
- هیچ فرآیند `CoreBanking` در حال اجرا نبود و پورت `8080` گوش نمی‌داد.
- نتیجه: برای Fixtureهای Event نیازی به باینری قدیمی نیست؛ باینری جدید نیز بازنویسی یا جایگزین نشد.

### 11.4 اقدام بعدی

1. Backup کامل `corebanking`.
2. اعمال `database/schema/018_hold_event_fingerprint.sql` توسط کاربر.
3. اجرای باینری جدید فعلی.
4. اجرای آزمون‌های بخش‌های 6 تا 8، با Fixtureهای Event موجود و Hold مصنوعی پس از 018.

---

## 12. نتیجه اجرای پس از 018 — 2026-11-11

این بخش پس از اجرای موفق Migration 018 توسط کاربر و بدون اجرای مجدد بررسی‌های پیش از Migration، Build یا Unit Test ثبت شده است. هیچ SQL نوشتنی مستقیم، Migration یا DDL توسط مدل اجرا نشد.

### 12.1 بررسی ساختار و داده پس از 018

اتصال فقط با نقش `corebanking_reader`:

- سه ستون `request_fingerprint`، `fingerprint_version` و `effective_at_provided` در `account_hold_events` موجودند.
- Constraintهای `ck_account_hold_event_fingerprint_format`، `ck_account_hold_event_fingerprint_version` و `ck_account_hold_event_idempotency_metadata` موجودند.
- توابع `hold_event_fp_trim`، `hold_event_canonical_amount`، `hold_event_fingerprint_v1` و `hold_event_fingerprint` موجودند.
- Triggerهای `trg_validate_account_hold_event` و `trg_account_hold_event_immutable` فعال‌اند.
- `database/CURRENT_SCHEMA.sql` همین اشیای مرتبط با 018 را منعکس می‌کند.
- پیش از نوشتن جدید: `total_events=13`، `legacy_events=13`، `non_legacy_events=0`، `hold_count=7`.
- پس از آزمون: `total_events=17`، `legacy_events=13`، `versioned_events=4`، `inconsistent_metadata=0`، `hold_count=9`.

### 12.2 باینری و سرویس

- باینری اجراشده: `/home/ahmad/projects/build-CoreBanking-Desktop-Debug/CoreBanking` (زمان `2026-11-11 16:51`). باینری قدیمی اجرا نشد.
- فرآیند با PID `5240` روی پورت `8080` اجرا شد.
- `GET /api/v1/health` → `200`، `{"data":{"status":"UP"},"success":true}`.

### 12.3 تطابق Fingerprint C++ و SQL (بخش 7.1)

Probe موقت `/tmp/fp_probe.cpp` با `HoldEventFingerprintV1::fingerprint` کامپایل و اجرا شد.

| ورودی | C++ | SQL |
|---|---|---|
| Vector A | `f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1` | `f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1` |
| `reasonCode=سلام` | `3ece7c699eb93232f9d0589a92e720d51dc1871eb14eb274a8decf13e7756ee2` | `3ece7c699eb93232f9d0589a92e720d51dc1871eb14eb274a8decf13e7756ee2` |

نتیجه: برابر و موفق.

### 12.4 Create / Replay / Conflict (بخش 7.2)

- ایجاد Hold: حساب `1`، مبلغ `0.10`، کلید `hold018-api-20261111-h1` → `201`، `holdId=8`.
- ایجاد Event: `PARTIAL_RELEASE`، `0.01`، `R`، کلید `hold018-api-20261111-e1` → `201`، `eventId=14`.
- همان درخواست → `200`، همان `eventId=14`، `replayed=true`.
- همان کلید با `effectiveAt=2026-11-03T16:23:06.000Z` → `409 IDEMPOTENCY_CONFLICT`.
- همان کلید با `amount=0.02` → `409 IDEMPOTENCY_KEY_REUSED`.

### 12.5 Legacy Event Retry (بخش 7.3)

- Event `8` (`effat-review-1793723166-t1`) با `effectiveAt=2026-11-03T16:23:06.000Z` → `200 replayed=true`، همان `eventId=8`.
- Event `9` (`effat-review-1793723166-t2`) بدون `effectiveAt` → `200 replayed=true`، همان `eventId=9`.
- همان کلید Event `9` با `effectiveAt=2026-11-03T16:28:25.390Z` → `409 IDEMPOTENCY_CONFLICT`.
- تعداد Eventها افزایش نیافت.

### 12.6 Legacy Hold Retry با Timestamp چهاررقمی (بخش 7.4)

- ایجاد Hold پس از 018 با `effectiveAt=2026-11-03T16:23:06.123Z` و کلید `hold018-api-20261111-h2` → `201`، `holdId=9`.
- Retry با `effectiveAt=2026-11-03T16:23:06.1229Z` → `200 replayed=true`، همان `holdId=9`.
- این آزمون مسیر Fallback Round/Truncate را روی Hold مصنوعی ساخته‌شده پس از 018 می‌آزماید و آزمون رکورد ساخته‌شده با باینری قدیمی نیست.

### 12.7 رقابت (بخش 7.5)

- دو درخواست هم‌زمان با کلید یکسان `hold018-api-20261111-c1` روی Hold `8`: یکی `201 eventId=15` و دیگری `200 replayed=true` با همان `eventId=15`؛ فقط یک ردیف.
- دو درخواست هم‌زمان با کلیدهای متفاوت `hold018-api-20261111-c2a` و `hold018-api-20261111-c2b` روی Hold `8` → دو Event مستقل `eventId=16` و `eventId=17` با `201`.
- کنترل مجموع Hold `8`: مبلغ اولیه `0.10`، آزادشده `0.04`، باقی‌مانده `0.06`، `event_count=4`.

### 12.8 Timestamp بدون Timezone

- درخواست با `effectiveAt=2026-11-03T16:23:06` → `400 HOLD_INVALID_EFFECTIVE_AT`.
- هیچ ردیفی با کلید `hold018-api-20261111-tz` ایجاد نشد. این محدودیت پذیرفته‌شده باقی می‌ماند.

### 12.9 داده‌های ایجادشده

- Hold `8` روی حساب `1`، کلید `hold018-api-20261111-h1`، مبلغ `0.10`، `reasonCode=H018`، وضعیت `PARTIALLY_RELEASED`، آزادشده `0.04`، باقی‌مانده `0.06`.
- Hold `9` روی حساب `1`، کلید `hold018-api-20261111-h2`، مبلغ `0.02`، `reasonCode=H018-ROUND`، بدون Event، باقی‌مانده `0.02`.
- Eventهای `14` تا `17` روی Hold `8` با کلیدهای `hold018-api-20261111-e1`، `...-c1`، `...-c2a` و `...-c2b`؛ همه `PARTIAL_RELEASE`، مبلغ `0.01`، Metadata کامل، `fingerprint_version=1`، `effective_at_provided=false`.
- هیچ رکورد قبلی تغییر یا حذف نشد.

### 12.10 آزمون‌های SQL کنترل‌شده — خارج از دامنه این مرحله و به تصمیم کاربر

این بخش‌ها DML مستقیم دارند و طبق مرز دسترسی پروژه باید فقط توسط نویسنده دیتابیس و فقط روی `corebanking` اجرا شوند. اجرای آن‌ها شرط پایان آزمون API این مرحله نیست و به تصمیم کاربر خارج از این دامنه واگذار شده است. هیچ‌کدام در این اجرا انجام نشده‌اند و موفق گزارش نمی‌شوند. دستورهای دقیق:

#### 12.10.1 رد Metadata ناقص (بخش 8.1)

```sql
BEGIN;

INSERT INTO account_hold_events (
  hold_id, idempotency_key, event_type, amount, effective_at,
  reference_type, reference_id, reason_code,
  request_fingerprint, fingerprint_version, effective_at_provided
) VALUES (
  8, 'invalid-metadata-test', 'PARTIAL_RELEASE', 0.01,
  CURRENT_TIMESTAMP, NULL, NULL, 'R',
  NULL, 1, TRUE
);

ROLLBACK;
```

هدف: انتظار خطای `ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED` و عدم درج ردیف.

#### 12.10.2 رد Version نامعتبر و Fingerprint بدفرمت (بخش 8.2)

```sql
BEGIN;

INSERT INTO account_hold_events (
  hold_id, idempotency_key, event_type, amount, effective_at,
  reference_type, reference_id, reason_code,
  request_fingerprint, fingerprint_version, effective_at_provided
) VALUES (
  8, 'invalid-version-test', 'PARTIAL_RELEASE', 0.01,
  CURRENT_TIMESTAMP, NULL, NULL, 'R',
  repeat('a', 64), 2, TRUE
);

ROLLBACK;
```

هدف: انتظار `ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED` و عدم درج ردیف.

```sql
BEGIN;

INSERT INTO account_hold_events (
  hold_id, idempotency_key, event_type, amount, effective_at,
  reference_type, reference_id, reason_code,
  request_fingerprint, fingerprint_version, effective_at_provided
) VALUES (
  8, 'invalid-format-test', 'PARTIAL_RELEASE', 0.01,
  CURRENT_TIMESTAMP, NULL, NULL, 'R',
  'bad', 1, TRUE
);

ROLLBACK;
```

هدف: انتظار `ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID` و عدم درج ردیف.

#### 12.10.3 Trigger RETURN NULL روی ردیف Legacy (بخش 8.3)

```sql
BEGIN;

SELECT count(*) AS before_count
FROM account_hold_events
WHERE hold_id = 7 AND idempotency_key = 'effat-review-1793723166-t1';

SELECT hold_event_fingerprint(
  1::smallint,
  7::bigint,
  'PARTIAL_RELEASE'::text,
  0.01::numeric,
  true,
  '2026-11-03T16:23:06.000Z'::timestamptz,
  'R1'::text,
  NULL::text,
  NULL::text
) AS expected_fingerprint;

INSERT INTO account_hold_events (
  hold_id, idempotency_key, event_type, amount, effective_at,
  reference_type, reference_id, reason_code,
  request_fingerprint, fingerprint_version, effective_at_provided
) VALUES (
  7, 'effat-review-1793723166-t1', 'PARTIAL_RELEASE', 0.01,
  '2026-11-03T16:23:06.000Z'::timestamptz, NULL, NULL, 'R1',
  hold_event_fingerprint(
    1::smallint, 7::bigint, 'PARTIAL_RELEASE'::text, 0.01::numeric,
    true, '2026-11-03T16:23:06.000Z'::timestamptz,
    'R1'::text, NULL::text, NULL::text
  ),
  1, true
);

SELECT count(*) AS after_count
FROM account_hold_events
WHERE hold_id = 7 AND idempotency_key = 'effat-review-1793723166-t1';

ROLLBACK;
```

هدف: انتظار `before_count = after_count = 1` و عدم ایجاد ردیف جدید؛ Trigger باید `RETURN NULL` کند. `effective_at_provided=true` بر اساس زمان صریح ذخیره‌شده Event `8` انتخاب شده است.

### 12.11 وضعیت نهایی مرحله

- بررسی‌های read-only پس از 018، اجرای باینری جدید و همه آزمون‌های API بخش ۷ موفق بودند.
- آزمون‌های SQL بخش ۸ شرط پایان این مرحله نیستند و به تصمیم کاربر خارج از دامنه آزمون API واگذار شده‌اند؛ اجرا نشده‌اند و موفق اعلام نمی‌شوند.
- وضعیت واقعی: استقرار 018 و آزمون یکپارچه‌سازی API انجام شد. مسیرهای Expiry/Cancellation در بخش ۱۳ همین سند بعداً آزموده و تأیید شدند. آزمون بار، مرزهای اضافه و رویه Restore/Rollback صرفاً آزموده‌نشده‌اند و شرط جدید پایان مرحله نیستند.

---

## 13. نتیجه آزمون API مسیرهای Expiry/Cancellation — 2026-11-11

بررسی کد نشان داد `CANCELLATION_RELEASE` و `EXPIRY_RELEASE` از قبل در `HoldService::createEvent` و Endpoint عمومی `POST /api/v1/holds/{id}/events` پیاده‌سازی شده‌اند. بنابراین تغییر کد، Schema یا Endpoint جدید لازم نبود و انجام نشد. Build مجدد نیز چون کد تغییر نکرد اجرا نشد.

### 13.1 Cancellation

- ایجاد Hold `10` (حساب `1`، مبلغ `0.03`، کلید `hold018-cancel-20261111-h1`) → `201`، وضعیت `ACTIVE`.
- `CANCELLATION_RELEASE` با کلید `hold018-cancel-20261111-e1` → `201`، `eventId=18`، `holdStatus=CANCELLED`، `remainingAmount=0`، `currentlyBlockedAmount=0`.
- Retry همان Event → `200`، همان `eventId=18`، `replayed=true`.
- `FULL_RELEASE` جدید با کلید `hold018-cancel-20261111-e2` → `422 ACCOUNT_HOLD_ALREADY_FULLY_RELEASED`.

### 13.2 Expiry

- ایجاد Hold `11` (حساب `1`، مبلغ `0.03`، `effectiveAt=2026-11-01T00:00:00Z`، `expiresAt=2026-11-02T00:00:00Z`، کلید `hold018-exp-20261111-h1`) → `201`، وضعیت `EXPIRED`، `currentlyBlockedAmount=0`.
- `CANCELLATION_RELEASE` روی Hold منقضی با کلید `hold018-exp-20261111-e0` → `422 EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE`؛ بدون Event.
- `EXPIRY_RELEASE` روی Hold غیرمنقضی `8` با کلید `hold018-exp-20261111-e2` → `422 ACCOUNT_HOLD_NOT_EXPIRED`؛ بدون Event.
- `EXPIRY_RELEASE` روی Hold `11` با کلید `hold018-exp-20261111-e1` → `201`، `eventId=19`، `holdStatus=EXPIRED`، `remainingAmount=0`.
- Retry همان Event → `200`، همان `eventId=19`، `replayed=true`.

### 13.3 کنترل SQL فقط‌خواندنی

- `hold_count=11`، `event_count=19`، `legacy_events=13`، `versioned_events=6`.
- Hold `10`: `status=CANCELLED`، `remaining_amount=0.00`، `release_event_count=1`، `last_release_event_type=CANCELLATION_RELEASE`.
- Hold `11`: `status=EXPIRED`، `remaining_amount=0.00`، `release_event_count=1`، `last_release_event_type=EXPIRY_RELEASE`.
- Hold `8` بدون تغییر: `PARTIALLY_RELEASED`، باقی‌مانده `0.06`، `event_count=4`.
- کلیدهای `hold018-exp-20261111-e0` و `hold018-exp-20261111-e2` هیچ ردیفی ندارند.
- `ledger_balance` حساب‌های `1` و `2` بدون تغییر (`1` و `0`).

### 13.4 نتیجه

- Expiry/Cancellation از طریق API فعلی موفق و با SQL فقط‌خواندنی تأیید شدند.
- آزمون SQL مستقیم بخش ۸ شرط پایان این مرحله نیست و به تصمیم کاربر خارج از دامنه آزمون API واگذار شده است.
- آزمون بار، مرزهای اضافه و رویه Restore/Rollback صرفاً آزموده‌نشده‌اند و شرط جدید پایان مرحله نیستند.

---

## 14. بستن دامنه آزمون API — 2026-11-11

- Migration 018 اجرا شده و `CURRENT_SCHEMA.sql` پس از آن از دیتابیس واقعی به‌روز و تطابق read-only آن بررسی شده است؛ چون Schema تغییر جدیدی نکرد، فایل دوباره تولید نشد.
- نتایج واقعی آزمون‌های API، شامل Create/Replay/Conflict، Legacy Retry، هم‌زمانی، Cancellation و Expiry، به‌همراه داده‌های آزمایشی باقی‌مانده در بخش‌های ۱۲ و ۱۳ همین سند ثبت شده‌اند.
- آزمون‌های SQL مستقیم بخش ۸ در انتظار اجرای کاربر نیستند؛ به تصمیم کاربر خارج از دامنه آزمون API این مرحله واگذار شده‌اند.
- آزمون بار، مرزهای اضافه و Restore/Rollback صرفاً آزموده‌نشده‌اند و شرط جدید پایان مرحله نیستند.
- کار ضروری دیگری برای پایان API مسدودی باقی نمانده است و این مرحله در دامنه مصوب خود بسته است.
