# HOLD EVENT VERSIONED IDEMPOTENCY — پیاده‌سازی Service و Controller

تاریخ: 2026-11-09

این گزارش پیاده‌سازی هماهنگ C++ با `database/schema/018_hold_event_fingerprint.sql` را ثبت می‌کند.

محدوده این مرحله:

- Canonicalizer نسخه ۱ در C++
- Parsing و Validation سخت‌گیرانه Timestamp
- رد U+0000 در Controller و Service
- INSERT سه ستون Metadata
- Lookup و Retry نسخه‌دار با Fallback Legacy
- Fail-Closed برای Metadata ناقص و Version ناشناخته
- نگاشت خطاهای Trigger جدید
- آزمون‌های Unit بدون اثر جانبی دیتابیس

انجام نشده است:

- اجرای Migration 018
- هر نوع DDL/DML یا نوشتن در دیتابیس
- تغییر `database/schema/018_hold_event_fingerprint.sql` یا `database/CURRENT_SCHEMA.sql`
- آزمون HTTP یا آزمون یکپارچه‌سازی نوشتن روی دیتابیس
- هر عملیات Git شامل stage، commit، push، reset، checkout یا branch

---

## 1. فایل‌های تغییر‌یافته و جدید

تغییر‌یافته:

- `src/application/holdservice.cpp`
- `src/web/holdcontroller.cpp`

جدید:

- `src/application/holdeventfingerprint.h`
- `tests/hold_event_fingerprint_and_validation_test.cpp`

مستندات:

- همین فایل
- `docs/PROJECT_CONTEXT.md`
- `docs/PROJECT_HISTORY.md`

`CMakeLists.txt` تغییر نکرد. Header جدید Header-Only است و با include در `holdservice.cpp` وارد Build می‌شود. فایل آزمون خارج از Build اصلی نگه داشته شده و دستی کامپایل شد.

---

## 2. Canonicalizer نسخه ۱

پیاده‌سازی در `src/application/holdeventfingerprint.h` قرار گرفت.

### 2.1 Trim نسخه ۱

مجموعه Whitespace دقیقاً همان مجموعه SQL است:

```text
U+0009 U+000A U+000B U+000C U+000D
U+0020 U+0085 U+00A0 U+1680
U+2000..U+200A
U+2028 U+2029 U+202F U+205F U+3000
```

ویژگی‌ها:

- فقط ابتدا و انتهای رشته Trim می‌شود.
- Whitespace داخلی حفظ می‌شود.
- U+001C تا U+001F جزو مجموعه نیستند.
- هیچ Unicode Normalization انجام نمی‌شود.
- `QString::trimmed()` جایگزین این Helper نشده است.

### 2.2 Canonical Amount

- حذف صفرهای انتهایی بخش اعشاری
- حذف نقطه اعشاری انتهایی
- رد Exponent و علامت منفی در Helper
- حفظ سازگاری با محدودیت موجود Service: مثبت بودن و حداکثر ۸ رقم اعشار

نمونه‌های آزمون:

```text
0.10        -> 0.1
1.23000000  -> 1.23
10.0        -> 10
10          -> 10
0.000000010 -> 0.00000001
```

### 2.3 Timestamp

- Timezone صریح الزامی است: `Z`، `+HH:MM`، `+HHMM` یا `+HH`.
- Timestamp بدون Timezone رد می‌شود.
- Timestamp نامعتبر رد می‌شود.
- مقدار به UTC تبدیل می‌شود.
- دقت به میلی‌ثانیه Truncate می‌شود، نه Round.
- خروجی دقیقاً با قالب SQL هماهنگ است:

```text
YYYY-MM-DDTHH:MM:SS.mmmZ
```

نکته: تست جداگانه نشان داد `QDateTime::fromString` برای بیش از سه رقم اعشار گرد می‌کند. به همین دلیل Parser ابتدا کسر ثانیه را دستی به سه رقم Truncate می‌کند و سپس Parsing نهایی را انجام می‌دهد.

### 2.4 Length-Prefix و Payload

Payload دقیقاً به این ترتیب ساخته می‌شود:

```text
HOLD_EVENT_FP
1
holdId=<decimal>
eventType=<utf8ByteLength>:<value>
amountProvided=<0|1>
amount=<utf8ByteLength>:<value>
effectiveAtProvided=<0|1>
effectiveAt=<utf8ByteLength>:<value>
reasonCode=<utf8ByteLength>:<value>
referenceType=<utf8ByteLength>:<value>
referenceId=<utf8ByteLength>:<value>
```

- هر خط با `\n` پایان می‌یابد.
- طول‌ها با `toUtf8().size()` محاسبه می‌شوند، نه `QString::size()`.
- برای `PARTIAL_RELEASE`، `amountProvided=1` و Amount وارد Payload می‌شود.
- برای سایر Eventها، `amountProvided=0` و Amount خالی است.
- در صورت نبود `effectiveAt`، `effectiveAtProvided=0` و مقدار خالی است.
- `referenceType` و `referenceId` پس از Trim، در حالت خالی به رشته خالی تبدیل می‌شوند.
- Hash خروجی SHA-256 lowercase با طول ۶۴ کاراکتر است.

---

## 3. تطبیق با Test Vectorها

### Vector A — با `effectiveAt` صریح

Payload:

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

Hash به‌دست‌آمده در آزمون C++:

```text
f541ed0f9447fc6e41b03dae6cef34e49d95e2f6978ba96a6b52c625c73bb1d1
```

### Vector B — بدون `effectiveAt`

Payload:

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

Hash به‌دست‌آمده در آزمون C++:

```text
5e7271fc717f758194af0a0a66247da6a65e34d669359d084876397a3fa5ea8f
```

هر دو Hash با Test Vectorهای تأییدشده Migration یکسان‌اند.

آزمون اضافی با متن UTF-8 نیز انجام شد:

```text
reasonCode=8:سلام
```

این نشان می‌دهد Length-Prefix بر اساس Byte length است، نه تعداد کاراکتر.

---

## 4. Parsing و Validation

### 4.1 Timestamp

- `HoldService::createEvent` برای `effectiveAt` از Parser سخت‌گیرانه جدید استفاده می‌کند.
- `HoldService::createHold` نیز برای `effectiveAt` و `expiresAt` از همان Parser استفاده می‌کند.
- Timestamp بدون Timezone با `400 HOLD_INVALID_EFFECTIVE_AT` یا `400 HOLD_INVALID_EXPIRES_AT` رد می‌شود.
- Timestamp نامعتبر نیز به همین کدها نگاشت می‌شود.

### 4.2 U+0000

در Controller برای هر فیلد متنی بررسی می‌شود:

```cpp
value.contains(QChar(0x0000))
```

- `createHold`: `amount`، `reasonCode`، `reasonDescription`، `referenceType`، `referenceId`، `effectiveAt`، `expiresAt`، `idempotencyKey`
- `createEvent`: `eventType`، `amount`، `effectiveAt`، `reasonCode`، `referenceType`، `referenceId`، `idempotencyKey`

در Service نیز همان بررسی به‌صورت Defense-in-Depth انجام می‌شود و در صورت وجود NUL نتیجه `Rejected` با کد `HOLD_INVALID_TEXT_INPUT` است.

Controller کد `INVALID_TEXT_INPUT` و HTTP `400 Bad Request` برمی‌گرداند. هر دو کد به `400` نگاشت شده‌اند.

### 4.3 Amount

- قرارداد قبلی حفظ شد: `amount` فقط JSON string، مثبت، بدون فاصله ابتدا/انتها، بدون Exponent و حداکثر ۸ رقم اعشار.
- محدودیت ۸ رقم اعشار برای `PARTIAL_RELEASE` همچنان در Service اعمال می‌شود.
- Amount قبل از Fingerprint با همان قاعده قبلی Canonical می‌شود.

---

## 5. INSERT رویداد

INSERT جدید `account_hold_events` این ستون‌ها را می‌فرستد:

- `request_fingerprint`
- `fingerprint_version = 1`
- `effective_at_provided`

Fingerprint واقعی محاسبه‌شده در INSERT بایند می‌شود. متغیر Fingerprint دیگر بلااستفاده نیست.

مقادیر متنی که در Fingerprint وارد می‌شوند، قبل از ساخت Fingerprint و قبل از INSERT با Trim نسخه ۱ نرمال می‌شوند. Timestamp نیز به UTC با میلی‌ثانیه نرمال می‌شود.

برای `effectiveAt` ارسال‌نشده:

- در INSERT مقدار پیش‌فرض تراکنش به میلی‌ثانیه در `effective_at` ذخیره می‌شود.
- در Fingerprint، `effectiveAtProvided=0` و مقدار خالی استفاده می‌شود.
- `effective_at_provided = FALSE` ذخیره می‌شود.

---

## 6. Lookup و Retry

`lookupEventByKey` اکنون سه ستون Metadata را می‌خواند و بر اساس آن تصمیم می‌گیرد:

```text
request_fingerprint
fingerprint_version
effective_at_provided
```

### 6.1 هر سه NULL — Legacy Fallback

- Fingerprint مقایسه نمی‌شود.
- مقایسه فیلد‌به‌فیلد قبلی حفظ می‌شود.
- Heuristic قبلی زمان حفظ می‌شود:
  - درخواست بدون `effectiveAt`: `effective_at = date_trunc('milliseconds', created_at)`
  - درخواست با `effectiveAt` صریح: برابری با مقدار کلاینت و عدم برابری با مقدار پیش‌فرض
- عدم تطابق زمان همچنان `IDEMPOTENCY_CONFLICT` و `409` می‌دهد.
- عدم تطابق سایر فیلدها همچنان `IDEMPOTENCY_KEY_REUSED` و `409` می‌دهد.

### 6.2 Metadata ناقص

Fail-Closed با کد:

```text
ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID
```

### 6.3 Version ناشناخته

Fail-Closed با کد:

```text
ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED
```

هیچ مقایسه Fingerprint بین دو Version انجام نمی‌شود.

### 6.4 Version 1

- فرمت Fingerprint ذخیره‌شده با Regex `^[0-9a-f]{64}$` بررسی می‌شود.
- Fingerprint درخواست با همان Canonicalizer نسخه ۱ محاسبه می‌شود.
- فقط `version = 1` و برابری Fingerprint باعث Replay می‌شود.
- Fingerprint متفاوت باعث Conflict می‌شود.
- برای حفظ تفکیک خطای قبلی، تطابق مؤلفه `effectiveAt` نیز جداگانه محاسبه می‌شود:
  - عدم تطابق EffectiveAt -> `IDEMPOTENCY_CONFLICT`
  - عدم تطابق سایر فیلدها -> `IDEMPOTENCY_KEY_REUSED`

### 6.5 مسیرهای Unique Conflict و Savepoint

- در تعارض Unique، پس از `ROLLBACK TO SAVEPOINT hold_event_insert` همان Lookup نسخه‌دار اجرا می‌شود.
- Fail-Closed و Conflict در این مسیر نیز حفظ می‌شوند.
- Replay دقیق، Snapshot، Event Read-back و `replayed=true` حفظ شده‌اند.

### 6.6 مسیر Trigger RETURN NULL

- INSERT بدون بازگشت ردیف، ردیف موجود را با `(hold_id, idempotency_key)` می‌خواند.
- این مسیر برای ردیف Legacy و Versioned یکسان کار می‌کند.
- نتیجه به‌صورت Replay با همان `eventId` برگردانده می‌شود.

---

## 7. نگاشت خطاها

### 7.1 Service

کدهای جدید Trigger به `triggerErrorInfo` اضافه شدند:

| کد | نتیجه Service |
|---|---|
| `ACCOUNT_HOLD_EVENT_INVALID_EVENT_TYPE` | Rejected |
| `ACCOUNT_HOLD_EVENT_INVALID_REASON_CODE` | Rejected |
| `ACCOUNT_HOLD_EVENT_INVALID_AMOUNT` | Rejected |
| `ACCOUNT_HOLD_EVENT_INVALID_EFFECTIVE_AT` | Rejected |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED` | Failed / Fail-Closed |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID` | Failed / Fail-Closed |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_MISMATCH` | Failed / Fail-Closed |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED` | Failed / Fail-Closed |

### 7.2 Controller

| کد | HTTP |
|---|---|
| `INVALID_TEXT_INPUT` | `400 Bad Request` |
| `HOLD_INVALID_TEXT_INPUT` | `400 Bad Request` |
| `ACCOUNT_HOLD_EVENT_INVALID_EVENT_TYPE` | `422 Unprocessable Entity` |
| `ACCOUNT_HOLD_EVENT_INVALID_REASON_CODE` | `422 Unprocessable Entity` |
| `ACCOUNT_HOLD_EVENT_INVALID_AMOUNT` | `422 Unprocessable Entity` |
| `ACCOUNT_HOLD_EVENT_INVALID_EFFECTIVE_AT` | `422 Unprocessable Entity` |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_REQUIRED` | `500 Internal Server Error` |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_INVALID` | `500 Internal Server Error` |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_MISMATCH` | `500 Internal Server Error` |
| `ACCOUNT_HOLD_EVENT_FINGERPRINT_VERSION_UNSUPPORTED` | `500 Internal Server Error` |

---

## 8. آزمون‌ها

### 8.1 فایل آزمون

`tests/hold_event_fingerprint_and_validation_test.cpp`

این آزمون کاملاً Unit و بدون اتصال یا نوشتن در دیتابیس است.

پوشش:

- Trim مجموعه نسخه ۱
- حفظ Whitespace داخلی
- عدم Trim برای U+001C تا U+001F
- Canonical Amount
- Timestamp با `Z` و Offset
- تبدیل به UTC
- Truncate به میلی‌ثانیه بدون Round
- رد Timestamp بدون Timezone
- رد Timestamp نامعتبر
- هر دو Test Vector
- Length-Prefix بر اساس UTF-8 byte length
- Trim در Fingerprint
- Legacy Match / Legacy Conflict
- Versioned Match / Versioned Conflict
- Metadata ناقص
- Version ناشناخته
- Fingerprint ذخیره‌شده نامعتبر
- U+0000 در Service برای Hold و Event
- U+0000 در Controller برای Hold و Event

### 8.2 فرمان اجرا

```bash
g++ -std=c++17 -fPIC -I. \
  tests/hold_event_fingerprint_and_validation_test.cpp \
  src/application/holdservice.cpp \
  src/web/holdcontroller.cpp \
  src/web/apiresponse.cpp \
  $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
  -o /tmp/hold_event_fingerprint_and_validation_test

/tmp/hold_event_fingerprint_and_validation_test
```

خروجی:

```text
All hold event fingerprint and validation tests passed
```

### 8.3 Build پروژه

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق.

---

## 9. آزمون‌هایی که هنوز اجرا نشده‌اند

این موارد نیازمند اعمال Migration 018 و اجرای سرویس روی دیتابیس محلی هستند و در این مرحله عمداً اجرا نشدند:

- Replay واقعی Event نسخه‌دار از طریق API
- Conflict واقعی با Fingerprint متفاوت
- Legacy Retry واقعی بدون `effectiveAt`
- Legacy Retry واقعی با `effectiveAt` صریح
- Replay از مسیر Trigger RETURN NULL
- هم‌زمانی واقعی و Unique Conflict
- بررسی Read-only ستون‌های Metadata ردیف‌های جدید
- بررسی رفتار Trigger جدید روی INSERT مستقیم غیرکانونیکال

آزمون منطق تصمیم‌گیری این مسیرها به‌صورت Unit انجام شد، اما جای آزمون یکپارچه‌سازی دیتابیس را نمی‌گیرد.

---

## 10. موارد نیازمند بررسی پیش از استقرار

1. Migration 018 هنوز اجرا نشده است؛ Schema فعلی ستون‌های جدید را ندارد.
2. Service جدید برای Lookup و INSERT به ستون‌های `request_fingerprint`، `fingerprint_version` و `effective_at_provided` وابسته است. بنابراین استقرار Service جدید بدون Migration 018 کار نمی‌کند.
3. Migration 018 بدون Service جدید نیز نوشتن Event را متوقف می‌کند. Migration و Service باید در یک Maintenance Window هماهنگ اعمال شوند.
4. Backup دیتابیس و آماده‌سازی Rollback طبق `MIGRATION_018_FINAL_READINESS.md` لازم است.
5. آزمون‌های یکپارچه‌سازی فهرست‌شده در بخش ۹ باید پس از Migration و با داده Synthetic انجام شوند.
6. رویه Restore دامپ Legacy در Schema جدید باید قبل از استفاده واقعی بررسی شود.
7. هر Version آینده Fingerprint نیازمند Migration و Canonicalizer جدید است؛ Version 2 در این کد پشتیبانی نمی‌شود.
8. هیچ تغییری در `DepositService` یا `TransferService` انجام نشده است.

---

## 11. جمع‌بندی

- Canonicalizer C++ با قرارداد نسخه ۱ Migration 018 هماهنگ است.
- هر دو Test Vector تأییدشده در آزمون Unit بازتولید شدند.
- Metadata در INSERT ارسال می‌شود و Lookup نسخه‌دار شده است.
- Legacy Fallback، Fail-Closed و Version Ignorance طبق قرارداد پیاده‌سازی شدند.
- U+0000 و Timestamp بدون Timezone رد می‌شوند.
- Build موفق است و آزمون‌های Unit بدون اثر جانبی دیتابیس موفق‌اند.
- Migration و آزمون‌های یکپارچه‌سازی دیتابیس برای مرحله استقرار باقی مانده‌اند.
