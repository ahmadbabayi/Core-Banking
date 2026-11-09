# MIGRATION 018 FINAL READINESS — توابع، Service، استقرار و Restore

تاریخ: 2026-11-03

این گزارش فقط بررسی Read-only است. هیچ فایل، Migration، DDL/DML، Build، تست تغییردهنده دیتابیس یا عملیات Git اجرا نشد. دیتابیس فقط با نقش `corebanking_reader` بررسی شد.

---

## 1. مقایسه دقیق `validate_account_hold_event` قبلی و جدید

### 1.1 منبع مقایسه

- نسخه قبلی: `database/CURRENT_SCHEMA.sql:67-182` و `database/schema/015_account_holds.sql:109-...`
- نسخه جدید: `database/schema/018_hold_event_fingerprint.sql:335-628`
- مقایسه با Diff بین بدنه دو تابع انجام شد.

### 1.2 Error Codeها و SQLSTATEهای قبلی

| Error Code | SQLSTATE | وضعیت در نسخه جدید |
|---|---|---|
| `ACCOUNT_HOLD_NOT_FOUND` | `23503` | حفظ شده |
| `ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED` | `23505` | حفظ شده |
| `ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED` | `23514` | حفظ شده |
| `ACCOUNT_HOLD_NOT_YET_EFFECTIVE` | `23514` | حفظ شده |
| `ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME` | `23514` | حفظ شده |
| `ACCOUNT_HOLD_NOT_EXPIRED` | `23514` | حفظ شده |
| `EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE` | `23514` | حفظ شده |
| `ACCOUNT_HOLD_ALREADY_FULLY_RELEASED` | `23514` | حفظ شده |
| `ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT` | `23514` | حفظ شده |
| `PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT` | `23514` | حفظ شده |
| `FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT` | `23514` | حفظ شده |

هیچ Error Code قبلی حذف یا تغییر نکرده است.

### 1.3 Error Codeهای جدید

| Error Code | SQLSTATE | محل |
|---|---|---|
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED` | `23514` | Trigger |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID` | `23514` | Helper/Trigger |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_MISMATCH` | `23514` | Trigger |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED` | `0A000` | Dispatcher/Trigger |
| `ACCOUNT_HOLD_EVENT_INVALID_EVENT_TYPE` | `23514` | Trigger |
| `ACCOUNT_HOLD_EVENT_INVALID_REASON_CODE` | `23514` | Trigger |
| `ACCOUNT_HOLD_EVENT_INVALID_AMOUNT` | `23514` | Helper/Trigger |
| `ACCOUNT_HOLD_EVENT_INVALID_EFFECTIVE_AT` | `23514` | Helper/Trigger |

این کدها Fail-Closed هستند و جایگزین هیچ‌کدام از کدهای قبلی نشده‌اند.

### 1.4 ترتیب اجرای کنترل‌ها

#### نسخه قبلی

```text
1. v_now := clock_timestamp()
2. SELECT account_hold ... FOR UPDATE
3. ACCOUNT_HOLD_NOT_FOUND
4. Idempotency lookup
5. Exact field match → RETURN NULL / otherwise ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED
6. Future date check
7. Hold not yet effective check
8. Event before effective-time check
9. Expiry checks
10. SUM released / remaining
11. Already fully released check
12. Amount exceeds remaining check
13. Partial release boundary check
14. Full release amount equality check
15. RETURN NEW
```

#### نسخه جدید

```text
1. Metadata required / format / version checks
2. Trim event_type, reason_code, reference_type, reference_id
3. Event type / reason code validation
4. Normalize effective_at
5. Canonicalize and validate amount
6. Recompute fingerprint from normalized values
7. SELECT account_hold ... FOR UPDATE
8. ACCOUNT_HOLD_NOT_FOUND
9. v_now := clock_timestamp()
10. Idempotency lookup
11. Legacy field comparison OR versioned fingerprint comparison
12. Future date check
13. Hold not yet effective check
14. Event before effective-time check
15. Expiry checks
16. SUM released / remaining
17. Already fully released check
18. Amount exceeds remaining check
19. Partial release boundary check
20. Full release amount equality check
21. RETURN NEW
```

### 1.5 تغییرات رفتاری

#### تغییر عمدی

- `v_now` از ابتدای تابع به بعد از `SELECT ... FOR UPDATE` منتقل شده است.
- بنابراین کنترل‌های زمان/انقضا بر اساس زمان پس از دریافت قفل انجام می‌شوند.
- Idempotency همچنان قبل از Business Validation است.

#### افزوده‌های عمدی

- الزام Metadata کامل برای هر INSERT جدید
- بررسی نسخه و فرمت Fingerprint
- Canonicalization متن، مبلغ و Timestamp
- رد `NaN`، `Infinity`، `amount <= 0` و مبلغ Partial با بیش از ۸ رقم اعشار
- محاسبه مجدد Fingerprint از مقادیر نرمال‌شده
- شاخه Legacy صریح با heuristic قبلی
- Detect و Fail-Closed برای Metadata ناقص یا Version ناشناخته

#### تغییرات ناخواسته

هیچ Business Validation قبلی حذف نشده و ترتیب نسبی آن‌ها حفظ شده است. تنها تفاوت ترتیبی، انتقال `v_now` است.

نکته: در نسخه جدید، Canonical/Metadata validation قبل از Idempotency اجرا می‌شود. این از نظر طراحی لازم است، چون Fingerprint باید از مقادیر نرمال‌شده ساخته شود و داده غیرکانونیکال باید Fail-Closed شود. برای درخواست‌های معتبر Service، این ترتیب تغییری در نتیجه Idempotency ایجاد نمی‌کند.

### 1.6 تغییر رفتار برای INSERT مستقیم

INSERT مستقیم با داده غیرکانونیکال، Metadata ناقص، Version ناشناخته یا Fingerprint ناسازگار اکنون رد می‌شود. این تغییر عمدی و بخشی از Hardening است.

---

## 2. مسیرهای INSERT و Retry در Service فعلی

### 2.1 آیا Service فعلی پس از Migration 018 می‌تواند Event ثبت کند؟

**خیر.**

دلیل:

- INSERT فعلی در `src/application/holdservice.cpp:1427-1443` فقط ستون‌های قدیمی را می‌فرستد.
- ستون‌های `request_fingerprint`، `fingerprint_version` و `effective_at_provided` را نمی‌فرستد.
- Trigger جدید در ابتدا این شرط را دارد:

```sql
IF NEW.request_fingerprint IS NULL
   OR NEW.fingerprint_version IS NULL
   OR NEW.effective_at_provided IS NULL THEN
    RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED'
        USING ERRCODE = '23514';
END IF;
```

- بنابراین هر INSERT از Service فعلی با `ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED` شکست می‌خورد.
- `triggerErrorInfo` فعلی (`holdservice.cpp:458-492`) این کد جدید را نمی‌شناسد؛ نتیجه نهایی می‌تواند `Failed` و HTTP 500 باشد.

نتیجه: Migration 018 بدون Service سازگار، نوشتن Event را کاملاً متوقف می‌کند.

### 2.2 آیا مسیر Retry فعلی با Metadata جدید سازگار است؟

#### Retry ردیف‌های Legacy

تا حدی بله:

- Lookup فعلی (`holdservice.cpp:522-604`) Metadata جدید را SELECT نمی‌کند.
- برای ردیف Legacy که همه Metadata آن NULL است، همان مقایسه فیلدبه‌فیلد و heuristic قبلی اجرا می‌شود.
- Retry بدون `effectiveAt` روی ردیف Legacy بدون زمان می‌تواند Replay شود.
- Retry صریح روی ردیف Legacy صریح می‌تواند Replay شود.

#### Retry ردیف‌های جدید نسخه‌دار

کاملاً سازگار نیست:

- Lookup فعلی `request_fingerprint` و `fingerprint_version` را نمی‌خواند.
- بنابراین نمی‌تواند Replay نسخه‌دار را بر اساس Version/Fingerprint انجام دهد.
- برای ردیف‌های جدید بدون `effectiveAt`، heuristic فعلی معمولاً کار می‌کند؛ اما این تطابق بر اساس فیلدهاست، نه Versioned Fingerprint.
- برای ردیف‌های جدید صریح، مقایسه `effective_at` ممکن است کار کند؛ ولی مسیر به‌طور کامل Version-Aware نیست.
- حالت‌های نادر Legacy Ambiguity نیز حفظ می‌شود، چون Lookup فعلی Fingerprint ندارد.

نتیجه: Retry ردیف‌های Legacy در مسیر عادی قابل ادامه است، اما Retry ردیف‌های جدید و Mixed Deployment قابل اتکا نیست.

### 2.3 بخش‌هایی از Service که قبل از فعال‌سازی مجدد نوشتن باید اصلاح شوند

1. **`fingerprintForEvent` — `holdservice.cpp:179`**
   - جایگزینی با Canonicalizer نسخه ۱ دقیقاً مطابق SQL.
   - Trim V1، Length-Prefix، UTF-8 byte length، Amount canonical، Timestamp UTC ms.

2. **Trim و Timestamp در `createEvent` — `holdservice.cpp:1101-1124`**
   - Helper صریح V1 به‌جای اتکا به `QString::trimmed()`.
   - Parser سخت‌گیرانه `effectiveAt` با Timezone اجباری.
   - کنترل U+0000.

3. **INSERT — `holdservice.cpp:1427-1443`**
   - افزودن سه ستون Metadata.
   - بایند `request_fingerprint`، `fingerprint_version=1` و `effective_at_provided`.

4. **Lookup — `holdservice.cpp:522-604`**
   - SELECT کردن سه ستون Metadata.
   - تشخیص همه NULL → Legacy Fallback.
   - Metadata ناقص → Fail-Closed.
   - Version پشتیبانی‌نشده → Fail-Closed.
   - Version پشتیبانی‌شده → محاسبه Fingerprint با همان نسخه و مقایسه.
   - Legacy branch باید Presence درخواست جدید و heuristic قدیمی را استفاده کند؛ نه مقایسه مستقیم با زمان تراکنش جدید.

5. **مسیر Retry — `holdservice.cpp:1167-1230`**
   - استفاده از نتیجه Lookup نسخه‌دار.
   - نگاشت Conflict/Version/Invalid به کدهای HTTP صحیح.
   - حفظ Snapshot/Event read-back و `replayed=true`.

6. **مسیر Unique Conflict / Savepoint — `holdservice.cpp:1453-1480`**
   - همین Lookup نسخه‌دار باید در Fallback استفاده شود.

7. **مسیر Trigger RETURN NULL — `holdservice.cpp:1499-1513`**
   - در صورت استفاده از Trigger جدید، ردیف موجود با Metadata خودش خوانده می‌شود.
   - این مسیر باید با Legacy و Versioned هر دو کار کند.

8. **`triggerErrorInfo` — `holdservice.cpp:458-492`**
   - افزودن کدهای جدید با نگاشت مناسب (Rejected/Failed).
   - `ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED` و کدهای Metadata نباید به‌اشتباه Replay یا 422 شوند.

9. **Controller — `src/web/holdcontroller.cpp:445-540`**
   - کنترل U+0000 در ورودی‌های String.
   - نگاشت کدهای خطای جدید در `httpStatusForCode` (`holdcontroller.cpp:29-80`).

10. **متغیر بلااستفاده Fingerprint**
    - `const QString fingerprint = fingerprintForEvent(...)` در `holdservice.cpp:1124` اکنون بلااستفاده است؛ باید در INSERT/Lookup واقعاً استفاده شود.

11. **آزمون‌ها**
    - شش سناریوی Idempotency
    - Legacy Retry (بدون زمان و صریح)
    - Version Mismatch
    - Direct INSERT ردشده
    - Metadata ناقص
    - U+0000
    - هم‌زمانی

---

## 3. برنامه استقرار و بازگشت

### 3.1 پیش‌نیازها

- تهیه Backup از دیتابیس.
- بررسی Git status و وجود Schema/Functionهای مورد انتظار.
- آماده بودن Build جدید Service و Rollback Script.
- اطمینان از اینکه هیچ Migration نیمه‌کاره‌ای اجرا نشده است.
- آماده بودن کوئری‌های Read-only برای بررسی بعد از Migration.

### 3.2 توقف و تخلیه درخواست‌ها

1. اعلام Maintenance Window.
2. توقف پذیرش درخواست‌های نوشتن Event، از جمله توقف کامل API یا غیرفعال‌کردن موقت Endpoint نوشتن.
3. صبر برای پایان تراکنش‌های در حال اجرا.
4. اطمینان از اینکه هیچ نمونه قدیمی Service در حال نوشتن نیست.
5. بررسی `pg_stat_activity` برای تراکنش‌های باز Hold/Event.

### 3.3 اعمال Migration

1. اجرای `database/schema/018_hold_event_fingerprint.sql` توسط کاربر مجاز.
2. Migration داخل `BEGIN/COMMIT` است؛ شکست در هر مرحله باید Rollback شود.
3. بررسی Read-only بعد از اجرا:
   - وجود سه ستون
   - وجود سه Constraint
   - وجود توابع `hold_event_fp_trim`، `hold_event_canonical_amount`، `hold_event_fingerprint_v1` و Dispatcher
   - به‌روزرسانی بدنه `validate_account_hold_event`
   - حفظ Triggerهای `BEFORE INSERT` و `BEFORE DELETE OR UPDATE`
   - حفظ Unique Index

### 3.4 استقرار Service سازگار

1. استقرار Build جدید که Metadata را در INSERT بایند می‌کند و Lookup نسخه‌دار دارد.
2. راه‌اندازی Service.
3. بررسی Health Endpoint.
4. آزمون‌های کنترل‌شده با داده Synthetic:
   - ایجاد Hold و Event جدید
   - Retry دقیق همان Event
   - Retry Legacy بدون زمان
   - Conflict برای Fingerprint متفاوت
   - بررسی DB Read-only برای Metadata و Fingerprint

### 3.5 ازسرگیری نوشتن

1. فعال‌کردن مجدد Endpoint نوشتن.
2. پایش خطاهای Trigger/Service.
3. بررسی نمونه‌ای از Eventهای جدید و Legacy.

### 3.6 در صورت شکست استقرار Service پس از اعمال Migration

#### حالت A — هنوز هیچ Event نسخه‌دار جدیدی ثبت نشده است

- سریع‌ترین بازگشت: جایگزینی بدنه `validate_account_hold_event` با نسخه قبلی (بازگرداندن تابع قدیمی).
- ستون‌ها و Constraintها می‌توانند باقی بمانند؛ چون All-or-None اجازه NULL می‌دهد.
- Service قدیمی می‌تواند Eventهای جدید را به‌صورت Legacy (Metadata NULL) ثبت کند.
- در صورت نیاز به بازگشت کامل و تاییدشده:
  - بازگرداندن تابع قدیمی
  - Drop کردن Constraintهای Metadata
  - Drop کردن Functionهای جدید
  - Drop کردن ستون‌های جدید
  - فقط اگر هیچ ردیف نسخه‌داری وجود ندارد.

#### حالت B — برخی Eventهای نسخه‌دار جدید ثبت شده‌اند

- ستون‌ها را Drop نکنید.
- تابع Trigger را موقتاً به نسخه قبلی برگردانید تا Service قدیمی بتواند Event جدید Legacy ثبت کند.
- Service جدید را اصلاح و دوباره مستقر کنید.
- ردیف‌های نسخه‌دار باقی می‌مانند و Service جدید بعدی می‌تواند آن‌ها را با Versioned Lookup بخواند.
- برای بازگشت کامل، ابتدا باید درباره ردیف‌های نسخه‌دار تصمیم‌گیری و تأیید جداگانه گرفته شود.

#### نکته

- Migration شکست‌خورده خودش Transaction Rollback می‌شود؛ در این حالت Service قدیمی می‌تواند ادامه دهد.
- Rollback نباید Unique Index را تغییر دهد.
- Rollback نباید ردیف‌های Legacy را Update/Backfill کند.

---

## 4. Restore دامپ Legacy در Schema جدید

### 4.1 آیا رویه خاص لازم است؟

**بله، در حالت Data-only Restore به Schema جدید.**

دلیل:

- Constraintهای All-or-None/Format/Version برای ردیف‌های Legacy با Metadata NULL مشکلی ندارند.
- اما Trigger `trg_validate_account_hold_event` روی هر INSERT جدید Metadata کامل را الزام می‌کند.
- `pg_restore` برای داده از COPY/INSERT استفاده می‌کند و Triggerها روی COPY/INSERT اجرا می‌شوند.
- بنابراین Restore داده Legacy در Schema جدیدی که Trigger فعال دارد، با `ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED` شکست می‌خورد.

### 4.2 محدودیت‌ها و حالت‌های مختلف

1. **Full Restore از دامپ Post-018 در دیتابیس خالی**
   - `pg_restore` معمولاً Triggerها/Indexها را در مرحله Post-data بعد از Data ایجاد می‌کند.
   - در این حالت ممکن است داده Legacy قبل از ایجاد Trigger بارگذاری شود و خطا ندهد.
   - اما نباید به ترتیب داخلی `pg_restore` اتکا کرد؛ این رفتار در نسخه‌ها/گزینه‌ها و نوع دامپ می‌تواند متفاوت باشد.

2. **Data-only Restore به Schema موجود Post-018**
   - Trigger از قبل وجود دارد.
   - ردیف‌های Legacy با Metadata NULL رد می‌شوند.
   - این حالت قطعاً به رویه خاص نیاز دارد.

3. **Restore دامپ Pre-018 به Schema Post-018**
   - دامپ ستون‌های جدید را ندارد.
   - ستون‌های جدید NULL می‌شوند.
   - Trigger فعال آن‌ها را رد می‌کند.

4. **Restore ردیف‌های نسخه‌دار**
   - اگر Metadata معتبر باشند و Trigger فعال باشد، Trigger Fingerprint را بازمحاسبه می‌کند.
   - اگر Fingerprint دامپ با Canonicalization فعلی سازگار نباشد، ردیف رد می‌شود.
   - ردیف‌های ناقص Metadata توسط Constraint رد می‌شوند.

### 4.3 رویه‌های مجاز

#### گزینه 1 — ترجیحی

1. دامپ Legacy را در Schema قبل از 018 Restore کنید.
2. سپس Migration 018 را اعمال کنید.
3. مزیت: هیچ Trigger جدیدی مانع Restore نیست؛ ردیف‌ها بعد از Migration Legacy می‌شوند.

#### گزینه 2 — Data-only Restore با Trigger غیرفعال

1. `trg_validate_account_hold_event` را به‌صورت کنترل‌شده غیرفعال کنید.
2. Data-only Restore را انجام دهید.
3. Trigger را فوراً دوباره فعال کنید.
4. بررسی کنید که ردیف‌های Legacy Metadata NULL دارند.
5. نیازمند مجوز Table Owner یا بالاتر است.

#### گزینه 3 — استفاده از `pg_restore --disable-triggers`

- نیازمند Superuser است.
- همه Triggerها، از جمله Immutability، موقتاً غیرفعال می‌شوند.
- باید فقط در Maintenance کنترل‌شده استفاده شود و بعد از Restore همه Triggerها دوباره فعال شوند.

#### گزینه 4 — آماده‌سازی Metadata قبل از Restore

- برای ردیف‌های Legacy توصیه نمی‌شود؛ چون Backfill و Update روی Eventهای قدیمی ممنوع است.
- فقط اگر ردیف‌ها واقعاً جدید باشند و Metadata معتبر داشته باشند قابل بررسی است.

### 4.4 نتیجه Restore

- برای عملکرد عادی نیازی به Backfill نیست.
- برای Restore دامپ Legacy به Schema جدید، یا باید Migration بعد از Restore اعمال شود، یا Trigger موقتاً غیرفعال شود.
- هیچ UPDATE/Backfill روی ردیف‌های Legacy در Migration 018 انجام نمی‌شود.
- Immutability Trigger باید فقط در رویه Restore کنترل‌شده و با تأیید جداگانه غیرفعال شود.

---

## 5. موارد حل‌نشده

1. Migration 018 هنوز اجرا نشده است.
2. Service فعلی با Trigger جدید سازگار نیست و نوشتن Event را متوقف می‌کند.
3. Retry ردیف‌های جدید Versioned با Service فعلی کامل و قابل اتکا نیست.
4. کنترل U+0000 در C++ پیاده‌سازی نشده است.
5. نگاشت کدهای خطای جدید در Service/Controller انجام نشده است.
6. رویه Deployment و Rollback باید با تأیید کاربر و در Maintenance Window اجرا شود.
7. رویه Restore باید قبل از استفاده واقعی تست شود.

---

## 6. جمع‌بندی

- تمام Business Validationها و Error Codeهای قبلی حفظ شده‌اند.
- تنها تغییر ترتیبی عمدی، انتقال `v_now` به بعد از قفل Hold است.
- افزوده‌ها Canonicalization، Fingerprint نسخه‌دار و Hardening هستند.
- Service فعلی پس از Migration نمی‌تواند Event جدید ثبت کند؛ Migration و Service باید هماهنگ مستقر شوند.
- Retry Legacy در مسیر عادی قابل ادامه است، ولی Retry نسخه‌دار و Mixed Deployment نیازمند Service جدید است.
- Restore دامپ Legacy در Schema جدید نیازمند رویه خاص است.
- Migration هنوز مجوز اجرا ندارد.
