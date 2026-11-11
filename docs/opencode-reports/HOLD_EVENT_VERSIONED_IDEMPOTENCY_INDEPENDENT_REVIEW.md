# HOLD EVENT VERSIONED IDEMPOTENCY — بازبینی مستقل و Read-Only

تاریخ: 2026-11-09

دامنه بازبینی:

- `src/application/holdeventfingerprint.h`
- `src/application/holdservice.cpp`
- `src/web/holdcontroller.cpp`
- `tests/hold_event_fingerprint_and_validation_test.cpp`
- `database/schema/018_hold_event_fingerprint.sql`
- `docs/opencode-reports/MIGRATION_018_FINAL_READINESS.md`

محدودیت‌های رعایت‌شده:

- هیچ فایلی به‌جز همین گزارش تغییر نکرد.
- هیچ DDL/DML اجرا نشد.
- به دیتابیس متصل نشد.
- Migration 018 اجرا نشد.
- هیچ Build یا Test اجرا نشد؛ وضعیت آزمون‌ها فقط از روی کد و فایل آزمون بررسی شد.
- هیچ عملیات Git انجام نشد.

---

## 1. خلاصه نتیجه

پیاده‌سازی C++ در مسیر اصلی با قرارداد Migration 018 هماهنگ است:

- Canonicalizer از نظر Trim، UTF-8 byte length، ترتیب Payload، Canonical Amount و Timestamp UTC/میلی‌ثانیه با SQL مطابقت دارد.
- INSERT هر سه ستون Metadata را با مقادیر واقعی می‌فرستد.
- Lookup چهار حالت Legacy، Metadata ناقص، Version ناشناخته و Version 1 را به‌درستی تفکیک می‌کند.
- مسیرهای Replay، Conflict، Savepoint/Unique و Trigger RETURN NULL از نظر ترتیبی سالم‌اند.
- رفتار Legacy حفظ شده است.
- U+0000 و Timestamp بدون Timezone در Controller و Service رد می‌شوند.
- نگاشت خطاهای جدید با SQLSTATE/Semantics Migration 018 سازگار است.

**هیچ ایراد قطعی که مانع آزمون یکپارچه‌سازی باشد پیدا نشد.**

با این حال، چند ریسک و شکاف آزمون وجود دارد که در بخش‌های 6 و 7 فهرست شده‌اند. مهم‌ترین آن‌ها Deployment Coupling و تغییر قرارداد Timestamp برای Hold است.

---

## 2. موارد تأییدشده

### 2.1 Canonicalizer

| موضوع | پیاده‌سازی C++ | قرارداد SQL | نتیجه |
|---|---|---|---|
| مجموعه Trim | `holdeventfingerprint.h:14-37` | `018_hold_event_fingerprint.sql:93-103` | مطابق |
| Trim فقط ابتدا/انتها | `holdeventfingerprint.h:39-51` | `btrim` در SQL | مطابق |
| Whitespace داخلی | `holdeventfingerprint.h:44-48` | `btrim` | مطابق |
| U+001C..U+001F خارج از مجموعه | `holdeventfingerprint.h:18-36` | مجموعه صریح SQL | مطابق |
| Canonical Amount | `holdeventfingerprint.h:58-82` | `018_hold_event_fingerprint.sql:120-143` | مطابق در محدوده ورودی Service |
| Timestamp UTC | `holdeventfingerprint.h:84-140` | `018_hold_event_fingerprint.sql:224-236` | مطابق |
| Truncate به میلی‌ثانیه | `holdeventfingerprint.h:103-131` | `date_trunc('milliseconds', ...)` | مطابق |
| فرمت خروجی Timestamp | `yyyy-MM-dd'T'HH:mm:ss.zzz'Z'` | `YYYY-MM-DD"T"HH24:MI:SS.MS"Z"` | مطابق |
| UTF-8 byte length | `holdeventfingerprint.h:215-217` | `octet_length(convert_to(..., 'UTF8'))` | مطابق |
| ترتیب Payload | `holdeventfingerprint.h:219-248` | `018_hold_event_fingerprint.sql:238-266` | مطابق |
| Trailing Newline | خط 248 | خط 266 | مطابق |
| SHA-256 lowercase | `holdeventfingerprint.h:266-268` | `encode(sha256(...), 'hex')` | مطابق |
| Amount فقط برای `PARTIAL_RELEASE` | `holdeventfingerprint.h:182-195` | `018_hold_event_fingerprint.sql:211-222` | مطابق |
| Presence زمان | `holdeventfingerprint.h:197-213` | `018_hold_event_fingerprint.sql:224-236` | مطابق |
| Empty بودن ReferenceType/Id | `holdeventfingerprint.h:169-172` | `NULLIF` و `COALESCE` | مطابق |

Test Vectorهای موجود در `tests/hold_event_fingerprint_and_validation_test.cpp:153-205` دقیقاً Payload و Hash مورد انتظار Migration را بازتولید می‌کنند.

### 2.2 INSERT

شواهد:

- INSERT سه ستون Metadata: `src/application/holdservice.cpp:1540-1551`
- بایند `request_fingerprint`: خط 1565
- بایند `fingerprint_version = 1`: خطوط 1566-1568
- بایند `effective_at_provided`: خط 1569
- محاسبه Fingerprint از مقادیر نرمال‌شده: خطوط 1227-1241
- Trim نسخه ۱ برای `eventType`، `reasonCode`، `referenceType` و `referenceId`: خطوط 1175-1180
- Timestamp نرمال‌شده: خطوط 1214-1225

نتیجه:

- مقدار Fingerprint ذخیره‌شده همان مقدار محاسبه‌شده در Service است.
- مقادیر متنی و Amount قبل از ساخت Fingerprint نرمال می‌شوند.
- Trigger 018 نیز همان مقادیر را دوباره نرمال می‌کند و باید به همان Fingerprint برسد.
- برای `effectiveAtProvided=false`، Fingerprint از رشته خالی استفاده می‌کند و Trigger مقدار پیش‌فرض تراکنش را در `effective_at` می‌گذارد؛ این با SQL سازگار است.
- برای `effectiveAtProvided=true`، Service مقدار UTC/میلی‌ثانیه را بایند می‌کند و Trigger همان instant را `date_trunc('milliseconds', ...)` می‌کند.

### 2.3 Lookup

شواهد:

- خواندن هر سه ستون: `src/application/holdservice.cpp:560-563`
- تشخیص NULL بودن: خطوط 622-633
- Legacy Fallback: `holdeventfingerprint.h:291-292` و `holdservice.cpp:656-664`
- Metadata ناقص: `holdeventfingerprint.h:294-295`
- Fingerprint نامعتبر: `holdeventfingerprint.h:297-298`
- Version ناشناخته: `holdeventfingerprint.h:300-301`
- Version 1 Match/Conflict: `holdeventfingerprint.h:303-306`
- Fail-Closed mapping: `holdservice.cpp:673-687`

نتیجه:

- هر سه NULL -> Legacy.
- هر ترکیب ناقص -> Fail-Closed.
- Version غیر از 1 -> Fail-Closed.
- Fingerprint ذخیره‌شده نامعتبر -> Fail-Closed.
- Version 1 فقط بر اساس Fingerprint مقایسه می‌شود.
- ترتیب بررسی فرمت قبل از Version با Trigger 018 مطابقت دارد (`018_hold_event_fingerprint.sql:359-367`).

### 2.4 تفکیک EffectiveAt Conflict

شواهد:

- `legacy_effective_matches`: `holdservice.cpp:567-572`
- `versioned_effective_matches`: `holdservice.cpp:573-578`
- انتخاب کد خطا: `holdservice.cpp:1301-1314` و `1605-1617`

نتیجه:

- برای Legacy، همان heuristic قبلی استفاده می‌شود.
- برای Versioned، Presence و مقدار `effective_at` جداگانه بررسی می‌شود.
- عدم تطابق EffectiveAt -> `IDEMPOTENCY_CONFLICT`.
- عدم تطابق سایر فیلدها -> `IDEMPOTENCY_KEY_REUSED`.

این رفتار با تصمیم قبلی پروژه سازگار است.

### 2.5 Legacy Compatibility

شواهد:

- مقایسه فیلد‌به‌فیلد Legacy: `holdservice.cpp:563-572` و `635-664`
- Heuristic زمان بدون `effectiveAt`: خطوط 568-571
- Heuristic زمان با `effectiveAt` صریح: خطوط 570-571
- عدم مقایسه Fingerprint برای Legacy: `holdeventfingerprint.h:291-292`

نتیجه:

- Retry بدون `effectiveAt` روی ردیف Legacy بدون زمان مطابق رفتار قبلی Replay می‌شود.
- Retry با `effectiveAt` صریح روی ردیف Legacy صریح مطابق رفتار قبلی بررسی می‌شود.
- حالت مرزی «زمان صریح برابر Default» همچنان مثل قبل تفسیر می‌شود.
- هیچ Backfill یا تغییر داده Legacy لازم نیست.

### 2.6 مسیرهای تراکنش

شواهد:

- پیش‌بررسی Idempotency قبل از اعتبارسنجی کسب‌وکاری: `holdservice.cpp:1281-1355`
- Unique Conflict: `holdservice.cpp:1571-1620`
- `ROLLBACK TO SAVEPOINT`: خطوط 1575-1580
- Lookup مجدد در Savepoint: خطوط 1582-1585
- Trigger RETURN NULL: خطوط 1635-1655
- Snapshot و Event Read-back: خطوط 1661-1696
- Commit: خط 1690

نتیجه:

- در Conflict، تراکنش Rollback می‌شود.
- در Unique Conflict، ابتدا Savepoint Rollback می‌شود و سپس Lookup امن انجام می‌شود.
- در خطای Lookup پس از Savepoint، تراکنش کامل Rollback می‌شود.
- در Trigger RETURN NULL، ردیف موجود با کلید یکتا خوانده می‌شود و Replay ادامه می‌یابد.
- پس از Commit هیچ Query جدیدی برای پاسخ اجرا نمی‌شود.

### 2.7 Timestamp و U+0000

شواهد:

- Parser سخت‌گیرانه: `holdeventfingerprint.h:84-140`
- استفاده در Event: `holdservice.cpp:1214-1225`
- استفاده در Hold: `holdservice.cpp:756-779`
- بررسی U+0000 در Service Hold: `holdservice.cpp:711-719`
- بررسی U+0000 در Service Event: `holdservice.cpp:1161-1169`
- بررسی U+0000 در Controller Hold: `holdcontroller.cpp:334-339`
- بررسی U+0000 در Controller Event: `holdcontroller.cpp:561-566`

نتیجه:

- Timestamp بدون Timezone رد می‌شود.
- Timestamp نامعتبر رد می‌شود.
- U+0000 در همه فیلدهای متنی اصلی Hold و Event در Controller و Service رد می‌شود.

### 2.8 Error Mapping

شواهد:

- کدهای Trigger: `holdservice.cpp:461-483`
- پیام‌های Rejection: `holdservice.cpp:494-528`
- HTTP Mapping: `holdcontroller.cpp:29-110`

نتیجه:

- `ACCOUNT_HOLD_EVENT_FINGERPRINT_*` به‌صورت Failed/500 رفتار می‌کنند.
- Version پشتیبانی‌نشده Fail-Closed است.
- کدهای `ACCOUNT_HOLD_EVENT_INVALID_*` به 422 نگاشت می‌شوند.
- کدهای متنی به 400 نگاشت می‌شوند.
- کدهای Idempotency به 409 نگاشت می‌شوند.
- کدهای ناموجودی به 404 نگاشت می‌شوند.

---

## 3. ایرادهای قطعی

**هیچ ایراد قطعی که با Migration 018 ناسازگار باشد پیدا نشد.**

به‌طور خاص:

- ترتیب فیلدهای Payload درست است.
- Length-Prefix بر اساس UTF-8 است.
- Timestamp به UTC و میلی‌ثانیه تبدیل می‌شود.
- INSERT Metadata کامل است.
- Lookup همه حالت‌های موردنیاز را پوشش می‌دهد.
- Legacy تغییر نکرده است.

---

## 4. ریسک‌های احتمالی

### 4.1 Deployment Coupling

`lookupEventByKey` و INSERT به ستون‌های جدید وابسته‌اند (`holdservice.cpp:560-563` و `1540-1551`). اگر Service جدید قبل از Migration 018 مستقر شود، Create/Retry رویداد شکست می‌خورد. اگر Migration 018 قبل از Service جدید اعمال شود، Service قدیمی نمی‌تواند رویداد جدید ثبت کند. این ریسک در Migration و گزارش آمادگی مستند شده است، اما باید در Maintenance Window کنترل شود.

### 4.2 تغییر قرارداد Timestamp برای Hold

Parser جدید فقط Timestamp دارای Timezone را می‌پذیرد (`holdeventfingerprint.h:93-101`) و برای `createHold` و `expiresAt` نیز استفاده می‌شود (`holdservice.cpp:756-779`). این تصمیم در `PROJECT_CONTEXT.md` ثبت شده است، اما برای کلاینت‌های قدیمی که Timestamp بدون Timezone می‌فرستادند یک تغییر رفتار محسوب می‌شود. پیش از استقرار باید تأیید شود که کلاینت‌ها Timestamp دارای Timezone می‌فرستند.

### 4.3 اختلاف Truncate/Round در Holdهای قدیمی

Parser جدید کسر ثانیه را Truncate می‌کند (`holdeventfingerprint.h:106-109`). نسخه قبلی Hold از `QDateTime::fromString(..., Qt::ISODateWithMs)` استفاده می‌کرد که برای بیش از سه رقم اعشار ممکن است Round کند. اگر Hold قدیمی با ورودی دارای بیش از سه رقم اعشار ایجاد شده باشد، Retry همان ورودی پس از استقرار ممکن است Fingerprint متفاوت تولید کند و 409 بگیرد. این حالت در داده‌های آزمایشی ثبت‌شده دیده نمی‌شود، اما یک ریسک Legacy است.

### 4.4 نگاشت 422 برای خطاهای Trigger

کدهای `ACCOUNT_HOLD_EVENT_INVALID_EVENT_TYPE`، `INVALID_REASON_CODE`، `INVALID_AMOUNT` و `INVALID_EFFECTIVE_AT` در Controller به 422 نگاشت شده‌اند (`holdcontroller.cpp:84-87`). در مسیر عادی Service این ورودی‌ها قبل از دیتابیس با 400 رد می‌شوند. بنابراین اگر این کدها از Trigger برگردند، احتمالاً نشانه ناسازگاری داخلی Service و Trigger است، نه ورودی کاربر. نگاشت آن‌ها به 422 می‌تواند یک خطای داخلی را به‌اشتباه خطای کاربر نشان دهد. این وضعیت در مسیر فعلی باید غیرقابل‌دسترس باشد، اما بهتر است قبل از Integration بررسی شود.

### 4.5 نمایش متن خطای دیتابیس در پاسخ 500

برای کدهای شناخته‌شده‌ای که `rejected=false` هستند، Service از `insertError.text()` به‌عنوان Message استفاده می‌کند (`holdservice.cpp:1628-1629`). Controller همان Message را در Response قرار می‌دهد (`holdcontroller.cpp:596-601`). این می‌تواند جزئیات داخلی دیتابیس را در پاسخ 500 نمایان کند. این رفتار جدید نیست، اما برای خطاهای جدید Fingerprint بهتر است پیام ثابت و غیرحساس استفاده شود.

### 4.6 ترتیب InvalidFingerprint و UnsupportedVersion

`classify` ابتدا فرمت Fingerprint را بررسی می‌کند و سپس Version را (`holdeventfingerprint.h:297-301`). این ترتیب با Trigger 018 مطابقت دارد. اما اگر در آینده Version جدیدی با فرمت Hash متفاوت اضافه شود، ردیف آن Version ممکن است به‌جای `VERSION_UNSUPPORTED` با `FINGERPRINT_INVALID` fail شود. هر دو Fail-Closed هستند، ولی کد خطا ممکن است گمراه‌کننده باشد.

### 4.7 دقت Timestamp در INSERT مستقیم

Service قبل از INSERT، Timestamp را به سه رقم اعشار نرمال می‌کند. بنابراین مسیر API با Trigger سازگار است. اگر در آینده INSERT مستقیم با بیش از شش رقم اعشار انجام شود، PostgreSQL ممکن است قبل از `date_trunc` به Microsecond گرد کند و نتیجه در حالت‌های مرزی با Truncate مستقیم C++ متفاوت باشد. این ریسک در مسیر فعلی Service وجود ندارد.

---

## 5. تفاوت با قرارداد Migration 018

در مسیر API، تفاوت رفتاری مؤثری پیدا نشد.

تفاوت‌های فنی قابل ذکر:

1. SQL Fingerprint روی مقادیر Typed (`NUMERIC` و `TIMESTAMPTZ`) اجرا می‌شود، در حالی که C++ روی رشته‌های نرمال‌شده اجرا می‌شود. برای ورودی‌های مجاز Service، نتیجه یکسان است.
2. Service پیش‌بررسی Idempotency را قبل از قفل Hold انجام می‌دهد، اما Advisory Lock و Trigger به‌عنوان Safety Net باقی می‌مانند. این با تصمیم طراحی قبلی سازگار است و ترتیب قفل Account/Balance/Hold را تغییر نمی‌دهد.
3. Service برای ردیف‌های Versioned فقط Fingerprint ذخیره‌شده را مقایسه می‌کند و Fingerprint را از فیلدهای ذخیره‌شده بازمحاسبه نمی‌کند. Trigger 018 نیز برای ردیف موجود همین کار را می‌کند؛ بنابراین ناسازگاری نیست.
4. برای `effectiveAtProvided=false`، Service مقدار `effective_at` را از `CURRENT_TIMESTAMP` تراکنش می‌فرستد، اما Trigger آن را با مقدار پیش‌فرض خود جایگزین می‌کند. چون این مقدار در Fingerprint وارد نمی‌شود، اختلاف میلی‌ثانیه‌ای اثری ندارد.

---

## 6. تست‌های انجام‌نشده

این موارد در فایل آزمون فعلی پوشش داده نشده‌اند:

### Helper-Level

- Trim برای تک‌تک Codepointهای مجموعه V1 به‌صورت جداگانه.
- Trim برای ترکیب چند Codepoint در دو انتها.
- Canonical Amount برای `0.000`، `000.1`، `1.`، و مقادیر بزرگ.
- Timestamp با Offset `+03` و `-03:30`.
- Timestamp با کسر ۷ تا ۹ رقم و بررسی رفتار Truncate.
- Timestamp با ثانیه کبیسه یا مقادیر مرزی.
- Fingerprint برای `FULL_RELEASE` با Reason/Reference چندبایتی و مقایسه Hash کامل.
- Fingerprint برای `PARTIAL_RELEASE` با مقدار `0.10` و `0.1` و اثبات یکسان بودن نتیجه.
- حالت `referenceType`/`referenceId` با Whitespace فقط.
- `classify` برای تمام ترکیب‌های Metadata ناقص، نه فقط یک ترکیب.
- `classify` برای Version 0، Version منفی و Version 2.

### Service-Level

- INSERT واقعی و بازخوانی Metadata از دیتابیس.
- Replay واقعی Event نسخه‌دار.
- Conflict واقعی Fingerprint.
- Legacy Retry بدون `effectiveAt`.
- Legacy Retry با `effectiveAt` صریح.
- Metadata ناقص در ردیف واقعی.
- Version ناشناخته در ردیف واقعی.
- Fingerprint نامعتبر در ردیف واقعی.
- تفکیک `IDEMPOTENCY_CONFLICT` از `IDEMPOTENCY_KEY_REUSED` در Lookup واقعی.
- مسیر Unique Conflict و Savepoint.
- مسیر Trigger RETURN NULL.
- هم‌زمانی دو درخواست با کلید یکسان.
- رفتار `createHold` با Timestamp بدون Timezone.
- رفتار `createEvent` با Timestamp بدون Timezone.
- رفتار `expiresAt` با Timestamp بدون Timezone و Timestamp نامعتبر.
- U+0000 در تمام فیلدهای Hold و Event، نه فقط چند فیلد نمونه.

### Controller-Level

- نگاشت HTTP برای همه کدهای Fingerprint جدید.
- نگاشت HTTP برای کدهای `ACCOUNT_HOLD_EVENT_INVALID_*`.
- Response Body برای خطاهای Fail-Closed.
- رفتار با `effectiveAt` بدون Timezone و پاسخ 400.
- رفتار با U+0000 در هر فیلد ورودی.
- رفتار با `amount` عددی JSON و `amount` بیش از ۸ رقم اعشار.

### Database-Level

- اعمال Migration 018.
- بررسی وجود سه ستون و Constraintها.
- بررسی توابع SQL.
- آزمون INSERT مستقیم با Metadata ناقص، Version ناشناخته، Fingerprint نامعتبر و Fingerprint سازگار.
- آزمون Restore/Rollback طبق گزارش آمادگی.

---

## 7. پیشنهاد اصلاح، بدون اعمال

1. **تست‌های یکپارچه‌سازی پس از Migration 018 اضافه شوند.**
   - Replay/Conflict نسخه‌دار
   - Legacy Retry بدون و با `effectiveAt`
   - Metadata ناقص و Version ناشناخته
   - Unique Conflict و Trigger RETURN NULL
   - هم‌زمانی

2. **تست‌های Unit تکمیل شوند.**
   - `classify` برای همه ترکیب‌های Metadata.
   - تفکیک Conflict ناشی از EffectiveAt.
   - `createHold` و `expiresAt` با Timestamp بدون Timezone.
   - U+0000 در تمام فیلدها.
   - Amount canonical برای مقادیر مرزی.

3. **نگاشت 422 برای `ACCOUNT_HOLD_EVENT_INVALID_*` بازبینی شود.**
   - اگر این کدها فقط در ناسازگاری داخلی Service/Trigger قابل بروز هستند، بهتر است 500 یا کد Fail-Closed جداگانه داشته باشند.
   - اگر به‌عنوان Validation نهایی Trigger پذیرفته می‌شوند، باید دلیل آن در مستندات ثبت شود.

4. **پیام خطاهای Fingerprint در پاسخ 500 ساده و غیرحساس شود.**
   - به‌جای `insertError.text()` برای این کدها از Message ثابت استفاده شود.

5. **سازگاری Timestamp کلاینت‌های قدیمی مستند یا تأیید شود.**
   - الزام Timezone برای `createHold` و `expiresAt` باید در قرارداد API و Release Note ثبت شود.
   - در صورت وجود Holdهای قدیمی با Timestamp بیش از سه رقم اعشار، سناریوی Retry آن‌ها بررسی شود.

6. **در صورت نیاز به Version 2، بررسی Version قبل از فرمت Fingerprint یا کد خطای مستقل در نظر گرفته شود.**
   - فعلاً هر دو Fail-Closed هستند، اما پیام دقیق‌تر می‌تواند عملیات را ساده‌تر کند.

---

## 8. نتیجه نهایی

**A. آماده ورود به مرحله آزمون یکپارچه‌سازی**

دلیل:

- هیچ ناسازگاری قطعی با Migration 018 در Canonicalizer، INSERT، Lookup، Legacy، Transaction یا Error Mapping پیدا نشد.
- ریسک‌های موجود عمدتاً Deployment/Compatibility و شکاف پوشش آزمون هستند، نه نقص منطقی شناخته‌شده در مسیر اصلی.
- آزمون یکپارچه‌سازی دیتابیس باید پس از اعمال Migration 018 و با تمرکز بر موارد بخش 6 انجام شود.
