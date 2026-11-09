# FINAL IDEMPOTENCY — Legacy Event Retry Compatibility Check

تاریخ: 2026-11-03

این گزارش فقط بررسی Read-only است. هیچ فایل کد، Migration، DDL/DML یا عملیات Git تغییر/اجرا نشد. تمام Queryهای دیتابیس با نقش `corebanking_reader` اجرا شدند.

---

## 1. خلاصه نتیجه

در مسیر عادی Retry، **Service قبل از INSERT رویداد Legacy را پیدا می‌کند و Replay موفق برمی‌گرداند**؛ بنابراین Trigger جدید حتی اجرا نمی‌شود و نتیجه همان `eventId` و همان `effectiveAt` ذخیره‌شده است.

اما اگر به هر دلیلی Pre-check سرویس رد شود و INSERT انجام شود، Trigger جدید در ابتدا `NEW.effective_at` را به زمان تراکنش جدید تغییر می‌دهد. اگر Trigger در شاخه Legacy همچنان `effective_at` را مستقیم با `NEW.effective_at` مقایسه کند، این مقایسه شکست می‌خورد و نتیجه `409 Conflict` می‌شود، نه Replay.

بنابراین:

- **برای مسیر عادی Service:** هیچ تغییری لازم نیست.
- **برای اینکه Trigger هم به‌عنوان Safety Net رفتار Legacy را حفظ کند:** شاخه Legacy در Trigger باید Presence درخواست جدید و heuristic زمان ذخیره‌شده قدیمی را مقایسه کند، نه `NEW.effective_at` را مستقیماً.
- Service فقط باید در پیاده‌سازی نسخه‌دار جدید، ابتدا حالت «همه Metadataها NULL» را تشخیص دهد و Fallback Legacy را اجرا کند.

---

## 2. مسیر واقعی Retry در HoldService

### 2.1 ترتیب فعلی

در `HoldService::createEvent`:

1. ورودی‌ها اعتبارسنجی می‌شوند.
2. `effectiveAtProvided` تعیین می‌شود.
3. تراکنش شروع می‌شود.
4. Advisory Lock روی `HOLD-EVENT:<idempotencyKey>` گرفته می‌شود.
5. **قبل از هر INSERT یا اعتبارسنجی کسب‌وکاری**، `lookupEventByKey` اجرا می‌شود
   (`src/application/holdservice.cpp:1167-1185`).
6. اگر ردیف پیدا شود و همه فیلدها مطابق باشند:
   - Snapshot وضعیت Hold خوانده می‌شود.
   - داده واقعی Event خوانده می‌شود.
   - تراکنش Commit می‌شود.
   - نتیجه `Completed` با `replayed=true` برگردانده می‌شود
     (`src/application/holdservice.cpp:1187-1230`).

بنابراین در حالت عادی، INSERT و Trigger اصلاً اجرا نمی‌شوند.

### 2.2 Query واقعی Lookup

`lookupEventByKey` در `src/application/holdservice.cpp:522` این شرط را برای زمان دارد:

```sql
CASE
  WHEN :effective_provided = 'false'
  THEN effective_at = date_trunc('milliseconds', created_at)
  ELSE effective_at = :client_effective
       AND effective_at <> date_trunc('milliseconds', created_at)
END AS effective_matches
```

برای Retry بدون `effectiveAt`:

- `effective_provided = 'false'`
- زمان درخواست جدید اصلاً با زمان ردیف قدیمی مقایسه نمی‌شود.
- فقط رابطه زمان ذخیره‌شده ردیف قدیمی با `created_at` همان ردیف بررسی می‌شود.

این همان heuristic سازگاری Legacy است.

### 2.3 مسیر Trigger فعلی

Trigger فعلی `trg_validate_account_hold_event` فقط `BEFORE INSERT` است
(`database/CURRENT_SCHEMA.sql:2207`).

اگر Service به Insert برسد:

1. Trigger ابتدا Hold را قفل و بررسی می‌کند.
2. ردیف موجود با `(hold_id, idempotency_key)` را می‌خواند.
3. در صورت وجود، فیلدها را مستقیم مقایسه می‌کند؛ از جمله:

```sql
v_existing.effective_at = NEW.effective_at
```

(`database/CURRENT_SCHEMA.sql:100-104`)

4. اگر همه برابر باشند `RETURN NULL`؛ در غیر این صورت
   `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED` می‌دهد.

در طراحی جدید، قبل از این مقایسه، `NEW.effective_at` برای `effective_at_provided=false`
به `date_trunc('milliseconds', CURRENT_TIMESTAMP)` تراکنش جدید تغییر می‌کند.
این مقدار با زمان ذخیره‌شده Legacy برابر نیست.

---

## 3. پاسخ مستقیم به سؤال‌ها

### 3.1 آیا Service قبل از INSERT رویداد Legacy را پیدا می‌کند؟

**بله، در مسیر عادی.**

`lookupEventByKey` با `WHERE hold_id = :hold_id AND idempotency_key = :key`
و `FOR UPDATE` ردیف را پیدا می‌کند. برای ردیف Legacy که Metadata آن NULL است،
پیاده‌سازی نسخه‌دار جدید باید تشخیص دهد:

```text
request_fingerprint IS NULL
AND fingerprint_version IS NULL
AND effective_at_provided IS NULL
```

و به Fallback Legacy برود. در آن مسیر، Fingerprint ذخیره‌شده مقایسه نمی‌شود.

### 3.2 آیا heuristic فعلی می‌تواند زمان درخواست جدید را با زمان Legacy تطبیق دهد؟

**بله، چون اصلاً زمان درخواست جدید را مقایسه نمی‌کند.**

برای درخواست بدون `effectiveAt`، شرط این است:

```sql
effective_at = date_trunc('milliseconds', created_at)
```

این شرط فقط به ردیف ذخیره‌شده Legacy وابسته است. زمان تراکنش جدید هیچ نقشی ندارد.
برای ردیف Legacy که با Service و بدون `effectiveAt` ساخته شده است، این شرط
برقرار است؛ چون Service مقدار پیش‌فرض را به میلی‌ثانیه Truncate و ذخیره کرده است.

### 3.3 آیا Trigger جدید قبل از تشخیص Legacy زمان ورودی را تغییر می‌دهد؟

**بله، اگر Trigger اجرا شود.**

در طراحی پیشنهادی، در ابتدای Trigger:

```sql
IF NEW.effective_at_provided THEN
    NEW.effective_at := date_trunc('milliseconds', NEW.effective_at);
ELSE
    NEW.effective_at := date_trunc('milliseconds', CURRENT_TIMESTAMP);
END IF;
```

برای Retry بدون `effectiveAt`، `NEW.effective_at` برابر زمان تراکنش جدید می‌شود.
اگر شاخه Legacy بعد از این تغییر، `v_existing.effective_at = NEW.effective_at`
را مقایسه کند، مقایسه شکست می‌خورد.

### 3.4 آیا نتیجه نهایی Replay موفق است یا Conflict؟

دو مسیر دارد:

#### مسیر عادی (Pre-check موفق)

- Service ردیف Legacy را پیدا می‌کند.
- همه فیلدها مطابق‌اند.
- `replayed=true` و همان `eventId` و همان `effectiveAt` برمی‌گردد.
- Trigger اجرا نمی‌شود.
- نتیجه: **Replay موفق**.

#### مسیر Fallback (Pre-check رد شده و INSERT انجام شده)

- Trigger جدید زمان `NEW.effective_at` را به زمان تراکنش جدید تغییر می‌دهد.
- ردیف Legacy موجود پیدا می‌شود.
- اگر Trigger مقایسه مستقیم `effective_at` را انجام دهد، نتیجه نابرابر است.
- نتیجه: **Conflict، نه Replay**.

### 3.5 آیا تغییر منطق لازم است؟

- **Service:** برای مسیر عادی، تغییر لازم نیست. اما پیاده‌سازی نسخه‌دار جدید باید
  ابتدا Metadata کاملاً NULL را تشخیص دهد و به Fallback Legacy برود. اگر اشتباهاً
  بخواهد Fingerprint NULL را با Fingerprint جدید مقایسه کند، رفتار Legacy خراب می‌شود.
- **Trigger:** برای اینکه Trigger در مسیر Fallback هم رفتار Legacy را حفظ کند،
  شاخه Legacy باید Presence درخواست جدید را در نظر بگیرد:

```text
اگر ردیف موجود Legacy است:
  اگر NEW.effective_at_provided = false:
      effective_match = (v_existing.effective_at =
                         date_trunc('milliseconds', v_existing.created_at))
  اگر NEW.effective_at_provided = true:
      effective_match = (v_existing.effective_at = NEW.effective_at
                         AND v_existing.effective_at <>
                             date_trunc('milliseconds', v_existing.created_at))
```

سپس همراه با سایر فیلدها:

- اگر همه مطابق بودند → `RETURN NULL` (Replay).
- در غیر این صورت → Conflict.

این دقیقاً همان heuristic سرویس است و باعث می‌شود حتی در مسیر Fallback هم
Retry بدون زمان برای ردیف Legacy بدون زمان، Replay شود.

---

## 4. Test Vector مرحله‌به‌مرحله

### 4.1 داده فرضی ردیف Legacy

| فیلد | مقدار |
|---|---|
| `eventId` | `9` |
| `hold_id` | `7` |
| `idempotency_key` | `legacy-absent-001` |
| `event_type` | `PARTIAL_RELEASE` |
| `amount` | `0.01` |
| `effective_at` | `2026-11-03 19:58:25.389+03:30` |
| `created_at` | `2026-11-03 19:58:25.389346+03:30` |
| `reason_code` | `R2` |
| `reference_type` | `NULL` |
| `reference_id` | `NULL` |
| `request_fingerprint` | `NULL` |
| `fingerprint_version` | `NULL` |
| `effective_at_provided` | `NULL` |

این مقادیر بر اساس ردیف واقعی `id=9` در `account_hold_events` انتخاب شده‌اند.
در دیتابیس فعلی، ستون‌های Metadata هنوز وجود ندارند؛ پس از Migration، این ردیف
وضعیت NULL خواهد داشت و Legacy محسوب می‌شود.

### 4.2 درخواست Retry

```json
{
  "eventType": "PARTIAL_RELEASE",
  "amount": "0.01",
  "reasonCode": "R2",
  "idempotencyKey": "legacy-absent-001"
}
```

`effectiveAt` ارسال نمی‌شود.

فرض زمان تراکنش جدید:

```text
T_retry = 2026-11-04 10:00:00.000+03:30
NEW default effective_at = date_trunc('milliseconds', T_retry)
                           = 2026-11-04 10:00:00.000+03:30
```

### 4.3 مرحله 1 — Pre-check سرویس

`lookupEventByKey` ردیف `id=9` را با `hold_id=7` و همان کلید پیدا می‌کند.

مقایسه‌ها:

| مقایسه | مقدار قدیمی | مقدار درخواست | نتیجه |
|---|---|---|---|
| event_type | `PARTIAL_RELEASE` | `PARTIAL_RELEASE` | match |
| amount | `0.01` | `0.01` | match |
| effective heuristic | `effective_at = trunc(ms, created_at)` | provided=false | match |
| reference_type | NULL | NULL | match |
| reference_id | NULL | NULL | match |
| reason_code | `R2` | `R2` | match |

مقدار heuristic:

```text
effective_at = 2026-11-03 19:58:25.389+03:30
date_trunc('milliseconds', created_at)
    = date_trunc('milliseconds', 2026-11-03 19:58:25.389346+03:30)
    = 2026-11-03 19:58:25.389+03:30
→ true
```

### 4.4 مرحله 2 — نتیجه Pre-check

Service ردیف را پیدا می‌کند و `matches=true` است.

- Snapshot وضعیت Hold خوانده می‌شود.
- Event قبلی خوانده می‌شود.
- Commit انجام می‌شود.
- پاسخ:

```text
HTTP 200
eventId = 9
effectiveAt = 2026-11-03 19:58:25.389+03:30
replayed = true
```

هیچ INSERT و هیچ Trigger اجرا نمی‌شود.

### 4.5 مرحله 3 — اگر Pre-check رد شود (سناریوی Fallback)

اگر به هر دلیلی Service به INSERT برسد:

- `NEW.effective_at` توسط Service یا Trigger برابر
  `2026-11-04 10:00:00.000+03:30` می‌شود.
- Trigger ردیف Legacy `id=9` را پیدا می‌کند.
- مقایسه مستقیم فعلی:

```text
v_existing.effective_at = 2026-11-03 19:58:25.389+03:30
NEW.effective_at        = 2026-11-04 10:00:00.000+03:30
→ false
```

- نتیجه: `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED` → `409 Conflict`.

### 4.6 مرحله 4 — با Trigger اصلاح‌شده

اگر Trigger در شاخه Legacy از Presence و heuristic استفاده کند:

```text
NEW.effective_at_provided = false
v_existing.effective_at = 2026-11-03 19:58:25.389+03:30
date_trunc(ms, v_existing.created_at) = 2026-11-03 19:58:25.389+03:30
→ effective_match = true
```

سایر فیلدها هم match هستند.

- Trigger `RETURN NULL` می‌دهد.
- INSERT ردیفی برنمی‌گرداند.
- Service در مسیر `!insert.next()` ردیف موجود را با همان Key می‌خواند
  (`holdservice.cpp:1513`)، `eventId=9` را برمی‌گرداند و `replayed=true` می‌گذارد.
- نتیجه: **Replay موفق با همان eventId و همان effectiveAt**.

---

## 5. شواهد Read-only

### 5.1 ردیف واقعی مورد استفاده

```sql
SELECT id, hold_id, idempotency_key, event_type, amount,
       effective_at, created_at
FROM account_hold_events
WHERE id = 9;
```

نتیجه:

```text
id = 9
hold_id = 7
idempotency_key = effat-review-1793723166-t2
event_type = PARTIAL_RELEASE
amount = 0.01
effective_at = 2026-11-03 19:58:25.389+03:30
created_at = 2026-11-03 19:58:25.389346+03:30
```

### 5.2 شبیه‌سازی شرط Lookup

```sql
SELECT id,
       (event_type = 'PARTIAL_RELEASE') AS type_matches,
       (0.01::numeric = amount) AS amount_matches,
       (effective_at = date_trunc('milliseconds', created_at)) AS effective_matches,
       (reference_type IS NOT DISTINCT FROM NULL) AS ref_type_matches,
       (reference_id IS NOT DISTINCT FROM NULL) AS ref_id_matches,
       (reason_code = 'R2') AS reason_matches
FROM account_hold_events
WHERE id = 9;
```

نتیجه واقعی:

```text
type_matches       = t
amount_matches     = t
effective_matches  = t
ref_type_matches   = t
ref_id_matches     = t
reason_matches     = t
```

### 5.3 شبیه‌سازی مقایسه مستقیم Trigger

```sql
SELECT effective_at,
       TIMESTAMPTZ '2026-11-04 10:00:00.000+03:30' AS retry_effective_at,
       (effective_at = TIMESTAMPTZ '2026-11-04 10:00:00.000+03:30') AS direct_equality,
       (effective_at = date_trunc('milliseconds', created_at)) AS heuristic_match
FROM account_hold_events
WHERE id = 9;
```

نتیجه واقعی:

```text
direct_equality  = f
heuristic_match  = t
```

این نشان می‌دهد مقایسه مستقیم `NEW.effective_at` در Trigger با ردیف Legacy شکست می‌خورد،
ولی heuristic فعلی موفق است.

---

## 6. نتیجه نهایی

| سؤال | پاسخ |
|---|---|
| Service قبل از INSERT ردیف Legacy را پیدا می‌کند؟ | بله، در مسیر عادی |
| heuristic زمان جدید را با زمان Legacy تطبیق می‌دهد؟ | بله؛ چون زمان جدید را مقایسه نمی‌کند و رابطه قدیمی را بررسی می‌کند |
| Trigger جدید قبل از تشخیص Legacy زمان را تغییر می‌دهد؟ | بله، اگر اجرا شود |
| نتیجه مسیر عادی چیست؟ | Replay موفق با همان eventId و effectiveAt |
| نتیجه مسیر Fallback با مقایسه مستقیم چیست؟ | Conflict با 409 |
| آیا تغییر لازم است؟ | Service: فقط تشخیص صحیح Legacy در Lookup نسخه‌دار. Trigger: شاخه Legacy باید Presence و heuristic را جایگزین مقایسه مستقیم کند تا Safety Net هم Replay بدهد |
| Backfill/UPDATE لازم است؟ | خیر |

---

## 7. توصیه پیاده‌سازی

1. در Lookup نسخه‌دار:
   - اگر هر سه ستون Metadata NULL هستند → Legacy Fallback.
   - هرگز Fingerprint جدید را با `request_fingerprint=NULL` مقایسه نکن.
2. در Trigger:
   - نرمال‌سازی `effective_at` را برای ردیف‌های جدید نگه دار.
   - در شاخه Legacy، از `NEW.effective_at_provided` و heuristic روی ردیف موجود استفاده کن؛
     نه مقایسه مستقیم `NEW.effective_at` با `v_existing.effective_at`.
3. هیچ Backfill یا Update انجام نده.

هیچ فایل کد یا Migration تغییر نکرد و هیچ DDL/DML اجرا نشد.
