# MIGRATION 018 DRAFT REVIEW — Versioned Hold Event Fingerprint

تاریخ: 2026-11-03

این گزارش مربوط به پیش‌نویس Migration است و به‌هیچ‌وجه مجوز اجرای آن نیست. هیچ Migration، DDL/DML، Build، تست تغییردهنده دیتابیس یا عملیات Git اجرا نشد. بررسی دیتابیس فقط Read-only و با نقش `corebanking_reader` انجام شد.

## فایل‌ها

- Migration: `database/schema/018_hold_event_fingerprint.sql` — ۶۲۸ خط
- گزارش: `docs/opencode-reports/MIGRATION_018_DRAFT_REVIEW.md`

---

## 1. ساختار Migration

ترتیب داخل فایل:

1. `BEGIN`
2. افزودن سه ستون Metadata به `account_hold_events`
3. افزودن سه Constraint Metadata
4. ایجاد `hold_event_fp_trim`
5. ایجاد `hold_event_canonical_amount`
6. ایجاد `hold_event_fingerprint_v1`
7. ایجاد Dispatcher `hold_event_fingerprint`
8. جایگزینی تابع `validate_account_hold_event`
9. عدم تغییر Triggerهای موجود
10. `COMMIT`

این ترتیب صحیح است:

- تابع `validate_account_hold_event` قبل از ایجاد Helperها فراخوانی نمی‌شود؛ Helperها قبل از تابع اصلی ساخته شده‌اند.
- Triggerهای موجود در انتها بازسازی یا Drop نشده‌اند؛ `CREATE OR REPLACE FUNCTION` تابع مورد استفاده Trigger را به‌روزرسانی می‌کند و OID تابع حفظ می‌شود.
- چون همه DDL داخل یک تراکنش است، شکست در هر مرحله کل Migration را Rollback می‌کند.

---

## 2. بررسی Trigger Order

### وضعیت واقعی

- تنها Trigger کاربری `BEFORE INSERT`: `trg_validate_account_hold_event`
- `trg_account_hold_event_immutable`: فقط `BEFORE DELETE OR UPDATE`
- تنها `AFTER INSERT`: Trigger داخلی FK `RI_ConstraintTrigger_c_19493`
- هیچ Trigger کاربری `AFTER INSERT` وجود ندارد.

### نتیجه بررسی پیش‌نویس

- نرمال‌سازی در ابتدای `validate_account_hold_event` انجام می‌شود.
- Fingerprint بعد از نرمال‌سازی محاسبه/مقایسه می‌شود.
- `v_now` پس از اجرای موفق `SELECT ... FOR UPDATE` و قبل از Idempotency تعیین می‌شود؛ بنابراین Business Validation بر اساس زمان پس از دریافت قفل انجام می‌شود.
- Business Validation بعد از Idempotency و با مقادیر نرمال‌شده اجرا می‌شود.
- CHECK/NOT NULL جدول بعد از Trigger BEFORE INSERT اعمال می‌شوند؛ بنابراین `NEW.amount` و `NEW.effective_at` نرمال‌شده را می‌بینند.
- Trigger داخلی FK بعد از Constraintها اجرا می‌شود و فیلدها را تغییر نمی‌دهد.
- Trigger immutability دست‌نخورده باقی مانده است.

**نتیجه:** ترتیب Trigger درست است. ریسک آینده: اگر Trigger کاربری `BEFORE INSERT` جدیدی با نامی که پیش از `trg_validate_account_hold_event` مرتب می‌شود اضافه شود، ممکن است قبل از نرمال‌سازی اجرا شود. این ریسک در گزارش ثبت شده است.

---

## 3. بررسی Constraintها و Metadata

### 3.1 ستون‌ها

```sql
request_fingerprint VARCHAR(64)
fingerprint_version SMALLINT
effective_at_provided BOOLEAN
```

همه Nullable هستند تا ردیف‌های Legacy بدون Backfill باقی بمانند.

### 3.2 Constraintها

| Constraint | بررسی |
|---|---|
| `ck_account_hold_event_idempotency_metadata` | هر سه ستون همگی NULL یا همگی NOT NULL |
| `ck_account_hold_event_fingerprint_format` | `^[0-9a-f]{64}$` |
| `ck_account_hold_event_fingerprint_version` | `fingerprint_version = 1` |

بررسی:

- ردیف‌های Legacy فعلی همه NULL خواهند بود و هر سه Constraint را پاس می‌کنند.
- هیچ `UPDATE` یا Backfill انجام نمی‌شود.
- ستون‌های Nullable بدون Default در PostgreSQL 15 بدون Table Rewrite اضافه می‌شوند.
- Validation خود Constraintها روی ۱۳ ردیف فعلی انجام می‌شود و چون همه NULL هستند، موفق است.
- Unique Index موجود `uq_account_hold_event_idempotency` تغییر نکرده است.
- Metadata ناقص توسط Constraint All-or-None و Trigger رد می‌شود.
- Metadata کامل ولی Version ناشناخته توسط Constraint و Trigger رد می‌شود.

**نتیجه:** Constraintها درست‌اند و با Legacy سازگارند.

---

## 4. بررسی شاخه Legacy در Trigger

### 4.1 تشخیص Legacy

```sql
v_existing_is_legacy := (
    v_existing.request_fingerprint IS NULL
    AND v_existing.fingerprint_version IS NULL
    AND v_existing.effective_at_provided IS NULL
);
```

- فقط وقتی هر سه NULL باشند Legacy است.
- Metadata ناقص در ردیف موجود → `ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID` و Fail-Closed.

### 4.2 مقایسه Legacy

برای Legacy، Fingerprint مقایسه نمی‌شود. فیلدها مقایسه می‌شوند:

- `event_type`
- `amount`
- `reference_type` با NULL-safe equality
- `reference_id` با NULL-safe equality
- `reason_code`

و برای زمان:

#### درخواست جدید بدون `effectiveAt`

```sql
v_existing.effective_at =
    date_trunc('milliseconds', v_existing.created_at)
```

زمان تراکنش جدید مقایسه نمی‌شود.

#### درخواست جدید با `effectiveAt` صریح

```sql
v_existing.effective_at = NEW.effective_at
AND v_existing.effective_at <>
    date_trunc('milliseconds', v_existing.created_at)
```

این همان heuristic قبلی Service است.

### 4.3 نتیجه Legacy

- Retry بدون زمان برای ردیف Legacy بدون زمان → Replay.
- Retry صریح با همان زمان برای ردیف Legacy صریح → Replay.
- ترکیب صریح/بدون‌زمان → Conflict.
- ایراد مرزی قبلی (زمان صریح دقیقاً برابر Default) عمداً حفظ شده است؛ چون قرارداد Legacy همان heuristic قبلی است.
- هیچ Update روی ردیف Legacy انجام نمی‌شود.

**نتیجه:** شاخه Legacy درست است و با گزارش قبلی `FINAL_IDEMPOTENCY_LEGACY_RETRY_CHECK.md` سازگار است.

---

## 5. بررسی Idempotency نسخه‌دار

برای ردیف جدید:

- اگر `fingerprint_version` و `request_fingerprint` برابر باشند → `RETURN NULL` و Replay.
- اگر متفاوت باشند → `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED` و Conflict.
- مقایسه Fingerprint بین دو نسخه مختلف انجام نمی‌شود؛ Version باید برابر باشد.
- Metadata کامل ولی Fingerprint ناسازگار قبل از رسیدن به این شاخه رد می‌شود.
- Metadata ناقص در NEW یا ردیف قبلی → Fail-Closed.
- Version ناشناخته در NEW → Fail-Closed.
- Version ناشناخته در ردیف قبلی → در مقایسه نسخه شکست می‌خورد و Conflict می‌دهد.

**نتیجه:** قرارداد Idempotency نسخه‌دار درست است.

---

## 6. بررسی Canonicalization

### 6.1 `hold_event_fp_trim`

- مجموعه Whitespace صریح V1: U+0009..U+000D، U+0020، U+0085، U+00A0، U+1680، U+2000..U+200A، U+2028، U+2029، U+202F، U+205F، U+3000.
- بدون Unicode Normalization.
- `STRICT` است؛ NULL → NULL.
- `IMMUTABLE` است؛ فقط `btrim` و ثابت Unicode.
- با Query Read-only معادل تست شد: حذف Whitespace از دو سر درست انجام می‌شود.

### 6.2 `hold_event_canonical_amount`

- `NaN`، `Infinity` و `-Infinity` را رد می‌کند.
- صفرهای انتهایی بعد از نقطه و نقطه انتهایی را حذف می‌کند.
- صفرهای عدد صحیح را حذف نمی‌کند.
- `STRICT` و `IMMUTABLE`.
- در Trigger برای `PARTIAL_RELEASE`:
  - مثبت بودن
  - حداکثر ۸ رقم اعشار
  - حداقل یک رقم غیرصفر
  - عدم وجود Exponent/جداکننده/علامت اضافی
  توسط Regex و `[1-9]` کنترل می‌شود.
- برای Eventهای غیر Partial، مبلغ در Fingerprint خالی است.
- برای Eventهای غیر Partial، `NaN`/`Infinity`/`-Infinity` و `amount <= 0` صریحاً رد می‌شوند.

نمونه‌های Read-only:

```text
0.10              -> 0.1
1.23000000        -> 1.23
10.0              -> 10
10                -> 10
0.000000010       -> 0.00000001
12345678901234567890.12345678 -> بدون تغییر
```

### 6.3 `hold_event_fingerprint_v1`

- Trim داخلی انجام می‌دهد؛ idempotent با Trigger.
- NULL/Empty برای Referenceها را به رشته خالی تبدیل می‌کند.
- طول فیلدها را با `octet_length(convert_to(..., 'UTF8'))` می‌سنجد.
- Timestamp صریح را به UTC و میلی‌ثانیه تبدیل می‌کند.
- برای `effective_at_provided=false`، زمان را در Payload خالی می‌گذارد.
- برای غیر Partial، `amountProvided=0` و `amount` خالی.
- Trailing Newline بعد از آخرین فیلد اضافه می‌شود.
- `STABLE` است، چون `convert_to` و `to_char` در PostgreSQL STABLE هستند.
- برای Index/Generated Column آینده طراحی نشده است.

**نتیجه:** Canonicalization با قرارداد V1 سازگار است.

---

## 7. تطبیق Test Vectorهای نسخه ۱

توابع در دیتابیس ایجاد نشدند. Payload معادل با Queryهای Read-only ساخته شد و Hashها با Test Vectorهای قبلی تطبیق داده شدند.

### Vector A — با `effectiveAt` صریح

```text
HOLD_EVENT_FP
1
holdId=7
eventType=15:PARTIAL_RELEASE
amountProvided=1
amount=4:0.01
effectiveAtProvided=1
effectiveAt=24:2026-11-03T16:23:06.000Z
reasonCode=2:R1
referenceType=0:
referenceId=0:
```

Hash مورد انتظار و تأییدشده:

```text
f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1
```

### Vector B — بدون `effectiveAt`

```text
HOLD_EVENT_FP
1
holdId=7
eventType=15:PARTIAL_RELEASE
amountProvided=1
amount=4:0.02
effectiveAtProvided=0
effectiveAt=0:
reasonCode=2:R2
referenceType=0:
referenceId=0:
```

Hash مورد انتظار و تأییدشده:

```text
5e7271fc717f758194af0a0a66247da6a65e34d669359d084876397a3fa5ea8f
```

**نتیجه:** Payload و Trailing Newline و Length-Prefix با Test Vectorها سازگارند.

---

## 8. تغییرات لازم در Service (انجام نشده)

Migration به‌تنهایی کافی نیست. برای هماهنگی کامل، این تغییرات C++ لازم است:

1. جایگزینی `fingerprintForEvent` با Canonicalizer نسخه ۱ دقیقاً مطابق SQL.
2. پیاده‌سازی Helper Trim صریح V1 در C++؛ عدم اتکا به `QString::trimmed()`.
3. استفاده از `toUtf8().size()` برای طول UTF-8، نه `QString::size()`.
4. Parser سخت‌گیرانه‌تر `effectiveAt` رویداد:
   - Timestamp بدون Timezone رد شود.
   - UTC با میلی‌ثانیه نرمال شود.
5. نرمال‌سازی مبلغ Partial دقیقاً مطابق SQL و رد `NaN/Infinity` (در C++ عملاً غیرممکن است، ولی قرارداد باید مستند باشد).
6. افزودن سه ستون Metadata به INSERT و بایند Fingerprint.
7. افزودن سه ستون Metadata به Lookup:
   - همه NULL → Legacy Fallback.
   - Metadata ناقص → Fail-Closed.
   - Version پشتیبانی‌نشده → Fail-Closed.
   - Version پشتیبانی‌شده → محاسبه Fingerprint با همان نسخه و مقایسه.
8. Legacy Fallback باید Presence درخواست جدید و heuristic قدیمی را استفاده کند، نه مقایسه مستقیم با زمان تراکنش جدید.
9. نگاشت کدهای خطای جدید در Controller:
   - `ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED`
   - `ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID`
   - `ACCOUNT_HOLD_EVENT_FINGERPRINT_MISMATCH`
   - `ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED`
   - `ACCOUNT_HOLD_EVENT_INVALID_EVENT_TYPE`
   - `ACCOUNT_HOLD_EVENT_INVALID_REASON_CODE`
   - `ACCOUNT_HOLD_EVENT_INVALID_AMOUNT`
   - `ACCOUNT_HOLD_EVENT_INVALID_EFFECTIVE_AT`
10. حذف وضعیت متغیر بلااستفاده Fingerprint در `createEvent` با استفاده واقعی از آن در INSERT/Lookup.
11. آزمون‌های شش‌گانه Idempotency، Legacy Retry، Version Mismatch، Direct INSERT ردشده، و هم‌زمانی.

### ترتیب استقرار

Migration و Service باید هماهنگ اعمال شوند:

- Migration قبل از Service جدید: Service قدیمی دیگر نمی‌تواند Event درج کند، چون Trigger Metadata الزام می‌کند.
- Service جدید قبل از Migration: INSERT به ستون‌های ناموجود شکست می‌خورد.
- پیشنهاد: توقف موقت نوشتن Event، اعمال Migration، استقرار Service جدید، سپس از سرگیری. یا یک استراتژی phased جداگانه با تأیید کاربر.

---

## 9. ریسک‌ها و نکات باقی‌مانده

1. **Deployment Coupling:** Migration 018 با Service فعلی سازگار نیست. این موضوع در بالای فایل Migration مستند شده است.
2. **Restore:** Restore دامپ‌های Legacy با Metadata NULL در Schema جدید توسط Trigger رد می‌شود. رویه‌های مجاز در فایل Migration مستند شده‌اند.
3. **Volatility:** `hold_event_fingerprint_v1` عمداً STABLE است. اگر در آینده برای Index/Generated Column استفاده شود، باید نسخه Immutable طراحی شود.
4. **NaN/Infinity:** CHECK فعلی `amount > 0` کافی نیست؛ Trigger این مقادیر را رد می‌کند. در صورت غیرفعال شدن Trigger، جدول هیچ محافظ دیگری برای NaN/Infinity ندارد.
5. **۸ رقم اعشار:** فقط در Trigger برای `PARTIAL_RELEASE` اعمال می‌شود؛ تابع Canonical به‌تنهایی آن را اعمال نمی‌کند.
6. **Legacy Ambiguity:** حالت مرزی Legacy (زمان صریح دقیقاً برابر Default) عمداً با همان heuristic قبلی حفظ شده است.
7. **Trigger Order آینده:** هر Trigger BEFORE INSERT جدید باید بعد از نرمال‌سازی اجرا شود یا نرمال‌سازی در Trigger موجود باقی بماند.
8. **Version 2 آینده:** نیازمند Migration جدید برای افزودن Canonicalizer، Dispatcher branch و به‌روزرسانی CHECK/Trigger است.
9. **Schema Qualification:** Helperها مانند بقیه Migrationها Unqualified هستند؛ در صورت نیاز به Hardening امنیتی می‌توان `SET search_path` یا Qualification اضافه کرد.
10. **عدم اجرا:** فایل Migration فقط از نظر منطقی و با Queryهای Read-only معادل بررسی شده است؛ در دیتابیس اجرا نشده و بنابراین صحت Syntax نهایی باید در زمان اعمال توسط کاربر تأیید شود.

---

## 10. بررسی تکمیلی و اصلاحات این دور

### 10.1 زمان `v_now` پس از قفل Hold

تغییر انجام‌شده:

- `v_now := clock_timestamp();` از ابتدای تابع حذف شد.
- اکنون بلافاصله بعد از موفقیت:

```sql
SELECT *
  INTO v_hold
  FROM account_hold
 WHERE id = NEW.hold_id
 FOR UPDATE;
```

و قبل از Idempotency مقداردهی می‌شود:

```sql
v_now := clock_timestamp();
```

ترتیب جدید:

```text
Metadata Check
→ Text/effective_at/amount Normalization
→ Fingerprint Recompute
→ Hold FOR UPDATE
→ v_now := clock_timestamp()
→ Idempotency
→ Business Validation
```

نتیجه:

- کنترل‌های `ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED`، `ACCOUNT_HOLD_NOT_YET_EFFECTIVE`، `ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME`، `ACCOUNT_HOLD_NOT_EXPIRED` و `EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE` اکنون بر اساس زمان پس از دریافت قفل انجام می‌شوند.
- Idempotency همچنان قبل از Business Validation است.
- `v_now` در مسیر Idempotency استفاده نمی‌شود؛ بنابراین مقداردهی آن قبل از Idempotency فقط برای آماده‌سازی Business Validation است و در مسیر Replay اثری ندارد.
- این تغییر عمدی است و در مقایسه با تابع قبلی تنها تغییر ترتیبی مرتبط با زمان است.

### 10.2 نوع دقیق `amount`

شواهد Schema:

- `database/CURRENT_SCHEMA.sql:328`:

```sql
amount numeric NOT NULL
```

- `information_schema.columns`:
  - `data_type = numeric`
  - `numeric_precision = NULL`
  - `numeric_scale = NULL`
- `pg_attribute`:
  - `format_type = numeric`
  - `atttypmod = -1`

نتیجه:

- ستون Precision/Scale ثابت ندارد و `NUMERIC` بدون Typmod است.
- PostgreSQL مقدار ورودی را برای این ستون قبل از Trigger گرد نمی‌کند.
- مقدارهای بیش از ۸ رقم اعشار در دیتابیس دقیق ذخیره می‌شوند و Trigger آن‌ها را برای `PARTIAL_RELEASE` رد می‌کند.
- تبدیل `numeric → text → canonical → numeric` برای اعداد متناهی دقیق است و گردشدن رخ نمی‌دهد.
- `NaN` و `Infinity` توسط CHECK فعلی `amount > 0` رد نمی‌شوند؛ Trigger آن‌ها را صریحاً رد می‌کند.

ریسک آینده:

- اگر در آینده ستون به `NUMERIC(p,s)` تبدیل شود، PostgreSQL ممکن است ورودی را قبل از BEFORE INSERT Trigger به Scale گرد کند. در آن حالت Trigger مقدار گردشده را می‌بیند و محدودیت ۸ رقم اعشار روی مقدار اصلی قابل تشخیص نیست.
- راهکار: ستون `amount` را بدون Typmod نگه دارید و محدودیت ۸ رقم اعشار را در Service و Trigger اعمال کنید. اگر روزی Typmod اضافه شد، Service باید مقدار خام را قبل از بایند اعتبارسنجی کند و Migration باید اثر گردشدن را مستند کند.
- در این Migration هیچ تغییر نوع ستون انجام نشده است.

### 10.3 مقایسه Business Validation قبلی و جدید

مقایسه تابع قبلی و جدید نشان داد تمام اعتبارسنجی‌های کسب‌وکاری و کدهای خطای قبلی حفظ شده‌اند:

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

همچنین این ترتیب‌ها حفظ شده‌اند:

- Idempotency قبل از Business Validation.
- Business Validation بعد از Idempotency.
- بررسی `v_remaining` و مبلغ بعد از بررسی زمان/انقضا، مطابق تابع قبلی.
- بررسی `PARTIAL_RELEASE` و `FULL/EXPIRY/CANCELLATION` مطابق تابع قبلی.
- `RETURN NEW` در انتهای مسیر موفق.

افزوده‌ها:

- Metadata/Version/Fingerprint checks
- Canonical text/amount normalization
- Reject `NaN/Infinity` و مبلغ غیرمثبت
- Reject `PARTIAL_RELEASE` با بیش از ۸ رقم اعشار
- Reject Metadata ناقص و Version ناشناخته
- Legacy branch صریح

هیچ اعتبارسنجی کسب‌وکاری حذف نشده است. تنها تغییر ترتیبی، انتقال `v_now` به بعد از قفل Hold است. اضافه‌شدن اعتبارسنجی‌های Canonical/Metadata قبل از Idempotency لازم است، چون Fingerprint باید از مقادیر نرمال‌شده ساخته شود و Insert مستقیم غیرکانونیکال باید Fail-Closed شود.

### 10.4 کنترل U+0000 در Service

U+0000 (NUL) در `text`/`varchar` PostgreSQL قابل ذخیره نیست و ممکن است در لایه Qt/libpq باعث Truncate یا خطای نامشخص شود. بنابراین باید قبل از Fingerprint و قبل از بایند به دیتابیس رد شود.

فیلدهای نیازمند کنترل:

- `eventType`
- `reasonCode`
- `referenceType`
- `referenceId`
- `effectiveAt`
- `idempotencyKey` (اگرچه در Fingerprint نیست، در دیتابیس و Lookup ذخیره می‌شود)
- `amount` (رشته ورودی؛ هرچند Regex مبلغ آن را رد می‌کند، بررسی زودهنگام توصیه می‌شود)

محل پیشنهادی:

1. **Controller — `HoldController::createEvent`:**
   - بعد از استخراج `QJsonValue`ها و قبل از `trimmed()` و فراخوانی Service.
   - برای هر مقدار String بررسی شود:
     ```cpp
     value.contains(QChar(0x0000))
     ```
     یا:
     ```cpp
     value.indexOf(QChar::Null) >= 0
     ```
   - در صورت وجود NUL، پاسخ `400 Bad Request` با کد پیشنهادی `INVALID_TEXT_INPUT` یا استفاده از `INVALID_HOLD_EVENT_REQUEST`.
2. **Service — `HoldService::createEvent`:**
   - بعد از Trim و قبل از ساخت Fingerprint و شروع تراکنش.
   - همان بررسی به‌عنوان Defense-in-Depth.
   - در صورت وجود NUL، نتیجه `Rejected` با کد پیشنهادی `HOLD_INVALID_TEXT_INPUT` (نگاشت به HTTP 400) یا کد موجود `INVALID_HOLD_EVENT_REQUEST`.

نکته:

- `QString::trimmed()` کاراکتر U+0000 را حذف نمی‌کند؛ بنابراین بررسی صریح لازم است.
- بهتر است همین Helper برای `createHold` هم روی فیلدهای متنی ذخیره‌شده اعمال شود، اما این خارج از محدوده فعلی است.
- در این مرحله هیچ کد C++ تغییر نکرده است.

### 10.5 موارد حل‌نشده

- Migration هنوز اجرا نشده و صحت Syntax نهایی فقط با اجرای کنترل‌شده توسط کاربر تأیید می‌شود.
- تغییرات C++ (Canonicalizer، Timestamp سخت‌گیرانه، بایند Metadata، Lookup نسخه‌دار، کنترل U+0000 و نگاشت خطاها) انجام نشده است.
- هماهنگی Deployment و رویه Restore همچنان نیازمند رویه عملیاتی تأییدشده است.
- در صورت نیاز به Version 2، Migration جدید لازم است.

---

## 11. جمع‌بندی بازبینی

| بخش | وضعیت |
|---|---|
| ترتیب Trigger | درست؛ `v_now` بعد از قفل Hold |
| Constraintهای Metadata | درست |
| نوع `amount` | `numeric` بدون Typmod؛ بدون گردشدن قبل از Trigger |
| حفظ Business Validation | تمام checks و error codeهای قبلی حفظ شده‌اند |
| شاخه Legacy | درست و سازگار با گزارش قبلی |
| Idempotency نسخه‌دار | درست |
| Canonicalization | درست |
| Test Vectorها | تأییدشده با Query Read-only معادل |
| Backfill/UPDATE | ندارد |
| Immutability Trigger | دست‌نخورده |
| کنترل U+0000 | طراحی و محل پیشنهادی مستند شد؛ کد تغییر نکرد |
| نیازمند هماهنگی Service | بله؛ در گزارش مستند شد |
| اجرا | انجام نشد |

این گزارش صرفاً پیش‌نویس است و منتظر بازبینی و تأیید کاربر برای اجرا است.
