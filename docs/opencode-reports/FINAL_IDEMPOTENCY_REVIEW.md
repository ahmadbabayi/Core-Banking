# FINAL IDEMPOTENCY REVIEW — effective_at/created_at و Canonical-form Validation

تاریخ بررسی: 2026-11-03

این گزارش فقط نتیجه بررسی کد، Schema و دیتابیس Read-only است. هیچ فایل کد، Migration، DDL/DML یا عملیات Git تغییر/اجرا نشد.

---

## 1. خلاصه تصمیم‌ها

1. شرط پیشنهادی زیر برای Trigger کنار گذاشته می‌شود:

   ```text
   NEW.effective_at = date_trunc('milliseconds', NEW.created_at)
   ```

   این شرط برای INSERTهای عادی Service برقرار است، اما برای INSERT مستقیم که به Default ستون‌ها تکیه می‌کند قابل اتکا نیست و می‌تواند INSERT معتبر را رد کند.

2. برای حالت `effective_at_provided = false`، Trigger مقدار `effective_at` را خودش به مقدار پیش‌فرض قطعی نرمال می‌کند:

   ```sql
   NEW.effective_at := date_trunc('milliseconds', CURRENT_TIMESTAMP);
   ```

   این مقدار همان مقداری است که Service فعلی در `databaseNow()` می‌سازد و در INSERT بایند می‌کند.

3. برای حالت `effective_at_provided = true`، مقدار `effective_at` به میلی‌ثانیه Truncate می‌شود؛ مقدار صریح کلاینت حفظ می‌شود.

4. Canonical-form validation در Trigger اجباری می‌شود. Trigger قبل از محاسبه مجدد Fingerprint، فیلدهای متنی و مبلغ Partial را با همان قواعد Service نرمال می‌کند. سپس Fingerprint را از مقادیر نرمال‌شده بازمحاسبه و با `request_fingerprint` ورودی مقایسه می‌کند.

5. INSERT مستقیم نمی‌تواند داده غیرکانونیکال ذخیره کند:
   - یا ورودی با قواعد V1 نرمال می‌شود و Fingerprint باید با مقدار نرمال‌شده برابر باشد؛
   - یا Fingerprint ناسازگار است و INSERT رد می‌شود.

---

## 2. بررسی دقیق `effective_at` و `created_at`

### 2.1 تعریف واقعی ستون‌ها

بر اساس `database/CURRENT_SCHEMA.sql:323-338` و `information_schema.columns`:

| ستون | نوع | Nullable | Default |
|---|---|---|---|
| `effective_at` | `timestamp with time zone` | NO | `CURRENT_TIMESTAMP` |
| `created_at` | `timestamp with time zone` | NO | `CURRENT_TIMESTAMP` |

هر دو Default برابر `CURRENT_TIMESTAMP` هستند و PostgreSQL این مقدار را با دقت میکروثانیه برمی‌گرداند. `CURRENT_TIMESTAMP` زمان شروع تراکنش است و در تمام تراکنش ثابت می‌ماند.

### 2.2 مسیرهای INSERT در HoldService

در کل کد برنامه فقط یک INSERT مستقیم به `account_hold_events` وجود دارد:

- `src/application/holdservice.cpp:1427` — INSERT اصلی در `createEvent`

مسیرهای مرتبط:

- `src/application/holdservice.cpp:1420` — `SAVEPOINT hold_event_insert`
- `src/application/holdservice.cpp:1453` — `ROLLBACK TO SAVEPOINT hold_event_insert` در تعارض Unique
- `src/application/holdservice.cpp:1513` — خواندن ردیف موجود وقتی Trigger با `RETURN NULL` درج را متوقف می‌کند

هیچ INSERT دیگری برای رویدادها در `src` وجود ندارد. Triggerهای دیتابیس هم INSERT جدیدی انجام نمی‌دهند؛ فقط اعتبارسنجی یا `RETURN NULL` می‌کنند.

### 2.3 مقدار دقیق برای Event بدون `effectiveAt`

در `createEvent`:

- `effectiveAtProvided = false`
- `databaseNow()` از `CURRENT_TIMESTAMP` استفاده می‌کند و آن را به میلی‌ثانیه Truncate می‌کند (`holdservice.cpp:126-151`).
- `effectiveAtInsertNormalized = dbNowNormalized` (`holdservice.cpp:1339-1342`).
- INSERT صریحاً `:effective_at` را با همان مقدار بایند می‌کند (`holdservice.cpp:1439`).
- `created_at` در لیست ستون‌های INSERT نیست؛ بنابراین Default دیتابیس یعنی `CURRENT_TIMESTAMP` با دقت کامل میکروثانیه اعمال می‌شود.

نتیجه برای Event بدون `effectiveAt`:

```text
effective_at = date_trunc('milliseconds', transaction_start)
created_at    = transaction_start  (full microsecond precision)
```

بنابراین در مسیر عادی Service رابطه زیر برقرار است:

```sql
effective_at = date_trunc('milliseconds', created_at)
```

اما این رابطه نتیجه پیاده‌سازی Service است، نه یک قرارداد کلی دیتابیس.

### 2.4 شواهد دیتابیس

خروجی Read-only روی `account_hold_events`:

- ردیف‌های `1` تا `7`، `9`، `11` و `13`: رابطه `effective_at = date_trunc('milliseconds', created_at)` برقرار است.
- ردیف‌های `8`، `10` و `12`: `effectiveAt` صریح بوده و اختلاف حدود ۵ دقیقه با `created_at` دارند.
- در ردیف‌های پیش‌فرض، اختلاف `effective_at - created_at` منفی و در محدوده کمتر از یک میلی‌ثانیه است؛ یعنی `effective_at` همان `created_at` Truncate‌شده است.

این شواهد تأیید می‌کند که مسیر فعلی Service در حالت بدون `effectiveAt` همان مقدار Truncate‌شده را ذخیره می‌کند.

### 2.5 مقدار دقیق برای Event با `effectiveAt`

- `effectiveAtProvided = true`
- `effective_at` برابر Timestamp نرمال‌شده کلاینت (UTC با میلی‌ثانیه) است.
- `created_at` همچنان برابر زمان شروع تراکنش است.
- هیچ رابطه برابری بین این دو لازم نیست و نباید فرض شود.

### 2.6 ارزیابی شرط پیشنهادی Trigger

شرط زیر:

```sql
NEW.effective_at = date_trunc('milliseconds', NEW.created_at)
```

برای مسیر Service در حالت `false` برقرار است، اما به‌عنوان Invariant عمومی مناسب نیست:

1. **INSERT مستقیم با Defaultها:** اگر نویسنده `effective_at` و `created_at` را ارسال نکند، هر دو با `CURRENT_TIMESTAMP` کامل میکروثانیه پر می‌شوند. `effective_at` برابر `date_trunc('milliseconds', created_at)` نیست، مگر اینکه بخش میکروثانیه صفر باشد. بنابراین Trigger یک INSERT معتبر را رد می‌کند.
2. **وابستگی به پیاده‌سازی Service:** اگر Service در آینده از `clock_timestamp()` یا دقت دیگری استفاده کند، شرط می‌شکند.
3. **وابستگی به `created_at`:** اگر نویسنده `created_at` را صریح ارسال کند، شرط می‌تواند گمراه‌کننده یا قابل دور زدن شود.
4. **افزونگی:** Presence اکنون با ستون صریح `effective_at_provided` مشخص می‌شود و نیازی به استنتاج از زمان نیست.

نتیجه: شرط برابری باید حذف شود.

### 2.7 راهکار قطعی پیشنهادی

Trigger به‌جای بررسی برابری، مقدار پیش‌فرض را برای حالت `false` تضمین کند:

```sql
IF NEW.effective_at_provided THEN
    IF NEW.effective_at IS NULL THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_INVALID_EFFECTIVE_AT'
            USING ERRCODE = '23514';
    END IF;

    NEW.effective_at := date_trunc('milliseconds', NEW.effective_at);
ELSE
    NEW.effective_at := date_trunc('milliseconds', CURRENT_TIMESTAMP);
END IF;
```

دلایل انتخاب:

- `CURRENT_TIMESTAMP` همان مبنای `databaseNow()` در Service است؛ نه `clock_timestamp()`.
- این مقدار در تمام تراکنش ثابت است.
- برای مسیر Service، مقدار جدید با مقدار بایندشده برابر است؛ بنابراین تغییر مؤثری رخ نمی‌دهد.
- برای INSERT مستقیم، مقدار پیش‌فرض قطعی و Canonical می‌شود.
- دیگر هیچ وابستگی به `created_at` یا دقت میکروثانیه آن وجود ندارد.
- برای `true`، مقدار صریح کلاینت حفظ می‌شود و فقط دقت آن به میلی‌ثانیه یکسان می‌شود.

### 2.8 سازگاری با Legacy

- Trigger تغییر یافته فقط روی INSERTهای جدید اجرا می‌شود.
- ردیف‌های Legacy بدون Backfill و با Metadata NULL باقی می‌مانند.
- Lookup ردیف‌های Legacy همچنان با مسیر Fallback فیلدبه‌فیلد و heuristic فعلی انجام می‌شود.
- هیچ Update یا تغییر مقدار روی ردیف‌های Legacy انجام نمی‌شود.

---

## 3. اجباری بودن Canonical-form Validation

### 3.1 قرارداد مشترک Service و Trigger

قواعد V1:

- Trim فقط با مجموعه صریح Unicode White_Space V1.
- بدون Unicode Normalization.
- طول فیلدها در Fingerprint برابر تعداد بایت UTF-8 است.
- `NULL` و رشته خالی برای `referenceType` و `referenceId` در Fingerprint هر دو رشته خالی هستند.
- `reasonCode` باید پس از Trim غیرخالی باشد.
- `eventType` باید پس از Trim یکی از چهار نوع مجاز باشد.
- مبلغ `PARTIAL_RELEASE` باید Canonical باشد.
- برای `effective_at_provided = false`، `effective_at` در Fingerprint رشته خالی است.
- برای `effective_at_provided = true`، `effective_at` در Fingerprint برابر UTC با میلی‌ثانیه است.

### 3.2 مسئولیت Service

Service باید قبل از ساخت Fingerprint و قبل از INSERT:

- `eventType` را با V1 Trim کند و در مجموعه مجاز بررسی کند.
- `reasonCode` را با V1 Trim کند و غیرخالی بودن را بررسی کند.
- `referenceType` و `referenceId` را با V1 Trim کند و مقدار خالی را به NULL تبدیل کند.
- مبلغ `PARTIAL_RELEASE` را Canonical کند.
- `effectiveAt` را با قرارداد سخت‌گیرانه Timestamp نرمال کند.
- Fingerprint را از همان مقادیر نرمال‌شده‌ای بسازد که در INSERT بایند می‌شوند.

Service نباید به Default دیتابیس برای `effective_at` تکیه کند؛ بایند صریح باید حفظ شود. Trigger نقش Safety Net و تضمین‌کننده مسیرهای مستقیم را دارد.

### 3.3 ترتیب نرمال‌سازی و اعتبارسنجی در Trigger

ترتیب پیشنهادی در `validate_account_hold_event`:

1. بررسی وجود `request_fingerprint`، `fingerprint_version` و `effective_at_provided`.
2. بررسی نسخه مجاز؛ فعلاً فقط `1`.
3. بررسی شکل `request_fingerprint` با Regex `^[0-9a-f]{64}$`.
4. نرمال‌سازی فیلدهای متنی:

   ```sql
   NEW.event_type := public.hold_event_fp_trim(NEW.event_type);
   NEW.reason_code := public.hold_event_fp_trim(NEW.reason_code);
   NEW.reference_type := NULLIF(public.hold_event_fp_trim(NEW.reference_type), '');
   NEW.reference_id := NULLIF(public.hold_event_fp_trim(NEW.reference_id), '');
   ```

5. نرمال‌سازی `effective_at` طبق بخش 2.7.
6. نرمال‌سازی مبلغ Partial:

   ```sql
   IF NEW.event_type = 'PARTIAL_RELEASE' THEN
       NEW.amount := public.hold_event_canonical_amount(NEW.amount)::numeric;
   END IF;
   ```

7. اعتبارسنجی پس از نرمال‌سازی:
   - `eventType` در مجموعه مجاز باشد.
   - `reasonCode` غیرخالی باشد.
   - `effective_at_provided = true` و `effective_at` NULL نباشد.
8. بازمحاسبه Fingerprint از `NEW` نرمال‌شده و مقایسه با `NEW.request_fingerprint`.
9. بررسی Idempotency:
   - ردیف جدید: مقایسه Version و Fingerprint.
   - ردیف Legacy: Fallback فیلدبه‌فیلد فعلی.
10. اعتبارسنجی‌های کسب‌وکاری موجود (زمان آینده، انقضا، مبلغ و ...).

نکته مهم: بازمحاسبه Fingerprint باید **بعد از** همه نرمال‌سازی‌ها انجام شود تا Fingerprint دقیقاً نماینده مقادیر ذخیره‌شده باشد.

### 3.4 فیلدهای متنی

- `eventType`:
  - Service و Trigger هر دو Trim می‌کنند.
  - مقدار نهایی باید دقیقاً یکی از انواع مجاز باشد.
  - INSERT مستقیم با Whitespace اضافی و Fingerprint Canonical پذیرفته و نرمال ذخیره می‌شود.
  - INSERT مستقیم با Fingerprint محاسبه‌شده از مقدار خام رد می‌شود.
- `reasonCode`:
  - Trim اجباری است.
  - پس از Trim باید غیرخالی باشد.
  - مقدار ذخیره‌شده همیشه Trim‌شده است.
- `referenceType` و `referenceId`:
  - Trim اجباری است.
  - پس از Trim، مقدار خالی به NULL تبدیل می‌شود.
  - در Fingerprint، NULL و رشته خالی هر دو رشته خالی هستند.
  - مقدار ذخیره‌شده یا NULL است یا نسخه Trim‌شده غیرخالی.

### 3.5 مبلغ `PARTIAL_RELEASE`

- Canonical amount:
  - بدون صفر انتهایی غیرلازم
  - بدون نقطه اعشار انتهایی
  - بدون Exponent، جداکننده هزارگان یا علامت اضافی
  - بدون صفر ابتدایی غیرلازم
- Service مقدار Canonical را بایند می‌کند.
- Trigger مقدار شماره‌ای `NEW.amount` را به نمایش Canonical تبدیل و سپس به `numeric` برمی‌گرداند تا Scale ذخیره‌شده هم یکسان شود.
- INSERT مستقیم با `0.10` و Fingerprint `0.1` نرمال و پذیرفته می‌شود.
- INSERT مستقیم با `0.10` و Fingerprint `0.10` پس از نرمال‌سازی رد می‌شود.
- برای انواع غیر از `PARTIAL_RELEASE`، مبلغ در Fingerprint خالی است و رفتار فعلی حفظ می‌شود.

### 3.6 سازگاری Fingerprint با مقادیر ذخیره‌شده

با ترتیب بخش 3.3:

- Fingerprint از `NEW` نرمال‌شده محاسبه می‌شود.
- مقدار ذخیره‌شده همان `NEW` نرمال‌شده است.
- بنابراین هر ردیف جدید رابطه زیر را دارد:

```text
stored.request_fingerprint = canonical_fingerprint(stored fields)
```

- اگر نویسنده‌ای Fingerprint را از مقدار خام محاسبه کند، پس از نرمال‌سازی Trigger مقدار بازمحاسبه‌شده متفاوت می‌شود و INSERT رد می‌شود.
- اگر نویسنده Fingerprint را از مقدار Canonical محاسبه کند، Trigger مقدار خام را نرمال می‌کند و INSERT پذیرفته می‌شود.

### 3.7 تضمین INSERT مستقیم

INSERT مستقیم در یکی از این دو حالت است:

1. Metadata کامل، نسخه `1`، Fingerprint Canonical و سازگار با مقادیر نرمال‌شده → پذیرفته و داده Canonical ذخیره می‌شود.
2. هر ناسازگاری در Metadata، Version، Canonical form یا Fingerprint → رد می‌شود.

بنابراین INSERT مستقیم نمی‌تواند ردیف جدیدی با فیلدهای غیرکانونیکال و Fingerprint سازگار ایجاد کند.

### 3.8 Legacy

- Trigger نرمال‌سازی فقط روی INSERTهای جدید اثر دارد.
- ردیف‌های Legacy تغییر نمی‌کنند.
- Lookup Legacy همچنان Fallback فیلدبه‌فیلد دارد.
- اگر ردیف Legacy توسط Service ساخته شده باشد، فیلدهای متنی آن از قبل Trim‌شده‌اند؛ اما Metadata نسخه‌دار ندارد.

---

## 4. Test Caseهای پیشنهادی

### 4.1 `effective_at` و Trigger

| Test | نتیجه مورد انتظار |
|---|---|
| Event بدون `effectiveAt` از طریق Service | `effective_at = date_trunc('milliseconds', CURRENT_TIMESTAMP)` و `created_at` با دقت میکروثانیه |
| Event بدون `effectiveAt` با INSERT مستقیم و Metadata کامل | Trigger مقدار پیش‌فرض Canonical را جایگزین می‌کند و INSERT پذیرفته می‌شود |
| Event با `effectiveAt` صریح و دقت میکروثانیه | مقدار به میلی‌ثانیه Truncate می‌شود |
| Event با `effectiveAtProvided=false` ولی زمان دلخواه | زمان با Default تراکنش جایگزین می‌شود |
| Event با `effectiveAtProvided=true` و `effective_at = NULL` | رد می‌شود |
| Event با `effectiveAtProvided=true` و Timestamp نامعتبر | رد می‌شود |
| Event بدون Metadata | رد می‌شود |
| ردیف‌های Legacy | بدون تغییر و با Fallback فعلی |

### 4.2 Canonical-form Validation

| Test | نتیجه مورد انتظار |
|---|---|
| `eventType = " PARTIAL_RELEASE "` و Fingerprint Canonical | Trim و پذیرفته می‌شود |
| `eventType` خام با Fingerprint خام | رد به‌دلیل Mismatch |
| `reasonCode = " R1 "` و Fingerprint `"R1"` | Trim و پذیرفته می‌شود |
| `reasonCode` فقط Whitespace | رد می‌شود |
| `referenceType = ""` یا Whitespace و Fingerprint خالی | به NULL تبدیل و پذیرفته می‌شود |
| `referenceId` با Whitespace اطراف و Fingerprint Trim‌شده | Trim و پذیرفته می‌شود |
| `amount = 0.10` و Fingerprint `0.1` | نرمال و پذیرفته می‌شود |
| `amount = 0.10` و Fingerprint `0.10` | رد به‌دلیل Mismatch |
| مبلغ غیر Partial متفاوت | Fingerprint تغییر نمی‌کند |
| Fingerprint صحیح ولی Metadata ناقص | رد می‌شود |
| نسخه ناشناخته | رد می‌شود |

### 4.3 Fingerprint و سازگاری ذخیره‌شده

| Test | نتیجه مورد انتظار |
|---|---|
| Fingerprint بازمحاسبه‌شده با ورودی برابر | پذیرفته می‌شود |
| یک Hex digit تغییر کند | رد می‌شود |
| Fingerprint از فیلد خام محاسبه شود | پس از نرمال‌سازی رد می‌شود |
| Fingerprint از فیلد Canonical محاسبه شود ولی ورودی خام باشد | نرمال و پذیرفته می‌شود |
| `effectiveAtProvided` با Payload ناسازگار | رد می‌شود |
| RDI Legacy با Metadata NULL | بدون بازمحاسبه Fingerprint و با Fallback |

---

## 5. نتیجه نهایی

- شرط `NEW.effective_at = date_trunc('milliseconds', NEW.created_at)` به‌عنوان Invariant Trigger حذف می‌شود.
- مقدار پیش‌فرض `effective_at` برای حالت بدون زمان در Trigger با `date_trunc('milliseconds', CURRENT_TIMESTAMP)` تضمین می‌شود.
- Canonical-form validation در Trigger اجباری و قبل از بازمحاسبه Fingerprint انجام می‌شود.
- Service و Trigger از یک قرارداد Trim/Amount/Timestamp مشترک استفاده می‌کنند.
- INSERT مستقیم نمی‌تواند داده غیرکانونیکال با Fingerprint سازگار ایجاد کند.
- Legacy بدون Backfill و با رفتار فعلی حفظ می‌شود.

هیچ تغییری در کد، Schema، Migration یا داده‌ها در این مرحله انجام نشد.
