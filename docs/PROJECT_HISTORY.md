# Core Banking — Project History

> این سند برای ثبت شواهد تاریخی آزمون‌ها، نتایج اجرای دستورات، Migrationها، Commitها و اشکالات و رفع آن‌ها است.
>
> قواعد کسب‌وکار و تصمیم‌های معماری پایدار در `docs/PROJECT_CONTEXT.md` نگهداری می‌شوند.
>
> نتایج تاریخی، وضعیت فعلی برنامه یا دیتابیس را تضمین نمی‌کنند. قبل از استفاده از شناسه‌های آزمایشی یا نتیجه‌های قدیمی، وضعیت واقعی بررسی شود.
>
> مخزن رسمی: https://github.com/ahmadbabayi/Core-Banking

## 1. شیوه ثبت شواهد

برای هر آزمون مهم، تا حدی که اطلاعات موجود باشد، موارد زیر ثبت شوند:

- تاریخ و زمان اجرای آزمون.
- Commit یا نسخه کد.
- محیط و نام دیتابیس.
- درخواست یا دستور دقیق.
- کد وضعیت HTTP یا خروجی واقعی.
- نتیجه مورد انتظار.
- وضعیت نهایی مانده‌ها و ثبت‌های حسابداری.
- محدودیت‌ها و آزمون‌هایی که هنوز انجام نشده‌اند.

اگر خروجی کامل درخواست یا پاسخ تاریخی حفظ نشده است، نباید متن آن بازسازی یا جعل شود. نتیجه خلاصه را با برچسب «نتیجه ثبت‌شده» و نه «خروجی کامل» ثبت کنید.

## 2. محیط و دستورات ثبت‌شده

محیط توسعه ثبت‌شده:
- Debian 12
- Qt 5.15.8
- GCC 12.2
- CMake 3.25
- PostgreSQL 15.x
- مسیر پروژه: `/home/ahmad/projects/CoreBanking`
- مسیر Build: `/home/ahmad/projects/build-CoreBanking-Desktop-Debug`
- دیتابیس اصلی: `corebanking`
- پورت API: `8080`

اتصال به دیتابیس اصلی:

```bash
psql -U corebanking -h localhost -d corebanking -W
```

دستور Build:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

آزمون Health API:

```bash
curl -i http://localhost:8080/api/v1/health
```

این دستورات نمونه‌های ثبت‌شده هستند؛ به‌خودی‌خود به معنای اجرای دوباره آن‌ها در زمان حاضر نیستند.

## 3. تاریخچه Migrationها

فهرست Migrationهای تاریخی شناخته‌شده:

| شماره | فایل | موضوع کلی |
|---|---|---|
| 001 | `001_geography_currency.sql` | جغرافیا و ارز |
| 002 | `002_remove_legacy_tables.sql` | حذف جداول قدیمی |
| 003 | `003_customer.sql` | ساختار مشتری |
| 004 | `004_customer_number_and_indexes.sql` | شماره مشتری و شاخص‌ها |
| 005 | `005_account_product_ledger.sql` | حساب، محصول و دفتر کل |
| 006 | `006_account_relationships.sql` | روابط حساب |
| 007 | `007_balances.sql` | مانده‌ها |
| 008 | `008_transaction.sql` | تراکنش |
| 009 | `009_journal.sql` | Journal |
| 010 | `010_financial_consistency.sql` | کنترل‌های یکپارچگی مالی |
| 011 | `011_city_code_global_unique.sql` | یکتایی سراسری کد شهر |
| 012 | `012_product_ledger_mapping.sql` | نگاشت محصول به دفتر کل |
| 013 | `013_account_type_serial.sql` | سریال مستقل نوع حساب |
| 014 | `014_product_balance_rules.sql` | قواعد مانده محصول و Override حساب |
| 015 | `015_account_holds.sql` | مسدودی وجه و رویدادهای آزادسازی |
| 016 | `016_allow_overdraft_balances.sql` | تغییرات مانده قابل برداشت، اضافه‌برداشت و Idempotency طبق سابقه پروژه |

نام و موضوع جدول بالا از سابقه پروژه جمع‌بندی شده است. برای جزئیات قطعی هر فایل، خود Migration مرجع است.

### 3.1 Migration 014 — قواعد مانده محصول

ساختارهای مربوط به قواعد مانده محصول و Override حساب ایجاد شدند.

قواعد مهم ثبت‌شده:
- حداقل مانده و مجازبودن اضافه‌برداشت مستقل‌اند.
- قواعد دارای بازه اعتبار هستند.
- برای بازه‌های زمانی، محدودیت‌های عدم هم‌پوشانی تعریف شده‌اند.
- Extension موردنیاز `btree_gist` در Schema ثبت شده است.

Migration 014 طبق سابقه قبلی با موفقیت اعمال شده بود. قبل از استفاده مجدد، وضعیت دیتابیس واقعی بررسی شود.

### 3.2 Migration 015 — مسدودی وجه

تاریخ ثبت و اجرا: 2026-10-24.

Commitهای مرتبط:
- `3258fdd` — `Add initial account holds migration`
- `ec4706e` — `Add hold event idempotency validation`

در تاریخ ثبت‌شده، هر دو Commit به `origin/main` ارسال شدند.

نسخه پشتیبان قبل از اجرا:

`database/backups/corebanking_before_015_20261024.dump`

طبق گزارش، Migration روی دیتابیس اصلی محلی `corebanking` اجرا شد و با `COMMIT` پایان یافت. وجود موارد زیر در دیتابیس اصلی بررسی و تأیید شده بود:
- `account_hold`
- `account_hold_events`
- `account_hold_status`
- Triggerهای اعتبارسنجی و محافظت از داده‌ها

### 3.3 آزمون‌های SQL برای Migration 015

این آزمون‌ها روی دیتابیس آزمایشی اجرا شدند و با `ROLLBACK` پایان یافتند؛ بنابراین نباید با آزمون یکپارچه HTTP یا اجرای داده‌های واقعی در دیتابیس اصلی اشتباه گرفته شوند.

| مورد آزمون | نتیجه ثبت‌شده |
|---|---|
| آزادسازی جزئی و محاسبه مبلغ باقی‌مانده | موفق |
| جلوگیری از درج دوباره رویداد در تکرار درخواست یکسان | موفق |
| آزادسازی کامل مبلغ باقی‌مانده | موفق |
| رد آزادسازی بیش از مبلغ باقی‌مانده | موفق |
| جلوگیری از `UPDATE` رویداد آزادسازی | موفق |
| جلوگیری از `DELETE` رویداد آزادسازی | موفق |
| صفرشدن مبلغ مسدود در زمان انقضا بدون نیاز به رویداد انقضا | موفق |
| الزام به `EXPIRY_RELEASE` برای آزادسازی مسدودی منقضی‌شده | موفق |
| رد استفاده مجدد از کلید Idempotency با محتوای متفاوت | موفق |
| حفظ رویداد اصلی در صورت استفاده نادرست از کلید | موفق |

این نتایج، آزمون‌های SQL ثبت‌شده هستند؛ آزمون بار هم‌زمان و آزمون کامل HTTP API برای مسدودی و آزادسازی با این شواهد اثبات نشده‌اند.

### 3.4 Migration 016 — تغییرات مالی و Idempotency

در الحاقیه پروژه با تاریخ 2026-10-26، موارد زیر گزارش شده بود:

- فایل `database/schema/016_allow_overdraft_balances.sql` در مخزن ثبت شده است.
- Migration در دیتابیس اصلی محلی `corebanking` با موفقیت اجرا شده است.
- فیلدهای زیر برای Idempotency تراکنش ایجاد و بررسی شده‌اند:
  - `idempotency_key`
  - `request_fingerprint`
  - `result_code`
  - `result_message`
- شاخص یکتای `uq_transaction_api_idempotency` ایجاد و بررسی شده است.
- قید `ck_account_balance_available` حذف شده است.
- قید نامنفی‌بودن `ledger_balance` حفظ شده است.
- وضعیت `REJECTED` برای تراکنش مجاز شده است.

این قسمت بر اساس گزارش تاریخی است؛ برای ادامه کار باید فایل Migration و Schema جاری دوباره بررسی شوند.

## 4. آزمون Health API

مسیر:

`GET /api/v1/health`

نتیجه تاریخی ثبت‌شده:

- وضعیت HTTP: `200 OK`
- بدنه:

```json
{"status":"UP"}
```

این نتیجه متعلق به اجرای قبلی است، نه اثبات در دسترس‌بودن سرور در زمان حاضر.

## 5. آزمون Product Management API

Commit مرتبط:

`7ca1fd9` — `Implement product management API`

مسیرهای ثبت‌شده:
- `POST /api/v1/products`
- `GET /api/v1/products`
- `GET /api/v1/products/{id}`
- `PATCH /api/v1/products/{id}/status`

در محیط آزمون، نتایج زیر ثبت شده‌اند:

| آزمون | نتیجه ثبت‌شده |
|---|---|
| دریافت فهرست اولیه | `count: 0` |
| فیلترهای `status=ACTIVE`، `accountTypeId=1` و `currencyId=1` در شروع آزمون | `count: 0` |
| ایجاد محصول بدون `currencyId` | `400`, کد `PRODUCT_REQUIRED_FIELDS` |
| ایجاد محصول معتبر | `201 Created` |
| شناسه محصول ایجادشده | `1` |
| استفاده از `currencyId=999` | `400`, کد `PRODUCT_INVALID_ACCOUNT_TYPE_OR_CURRENCY` |
| دریافت فهرست و جزئیات محصول | موفق |
| ایجاد دوباره کد محصول | `409`, کد `PRODUCT_DUPLICATE_CODE` |
| تغییر وضعیت محصول به `INACTIVE` | `200 OK` |

داده ایجادشده:
- کد: `TEST-CURRENT-IRR`
- نام: `Test Current Account IRR`
- `accountTypeId`: `1`
- `currencyId`: `1`
- `effectiveFrom`: `2026-10-12`
- شناسه: `1`

آخرین وضعیت ثبت‌شده در پایان آزمون، `INACTIVE` بود. وضعیت جاری محصول باید از API واقعی دوباره بررسی شود.

## 6. آزمون نگاشت Product–Ledger

در تاریخچه، نگاشت فعالی با شناسه `3` برای محصول `1` ثبت شده بود:

- `productLedgerMappingId`: `3`
- `productId`: `1`
- `ledgerAccountId`: `1`
- شروع اعتبار: `2026-10-14`
- پایان اعتبار: خالی
- وضعیت: فعال

این نگاشت در آزمون افتتاح حساب استفاده شد. این رکورد تاریخی است و پیش از استفاده مجدد باید از دیتابیس بررسی شود.

## 7. آزمون Ledger Account API

Commit مرتبط:

`7ca1fd9` — `Implement product management API`

مسیرهای API دفتر کل:
- `POST /api/v1/ledger-accounts`
- `GET /api/v1/ledger-accounts`
- `GET /api/v1/ledger-accounts/{id}`
- `PATCH /api/v1/ledger-accounts/{id}/status`

نتایج HTTP ثبت‌شده:

| آزمون | نتیجه |
|---|---|
| دریافت فهرست | `200 OK` |
| دریافت سرفصل با شناسه 1 | `200 OK` |
| فیلتر `currencyId=1` | `200 OK` |
| فیلتر `currencyId=abc` | `400 Bad Request` |
| ایجاد سرفصل با کد `20100102` | `201 Created` |
| ایجاد مجدد همان کد | `409 Conflict` |
| دریافت سرفصل با شناسه 2 | `200 OK` |
| تغییر وضعیت به `INACTIVE` | `200 OK` |
| فیلتر بر اساس وضعیت | موفق |
| بازگرداندن وضعیت به `ACTIVE` | `200 OK` |
| کد نامعتبر `123` | `400 Bad Request` |
| دسته‌بندی نامعتبر `SAVINGS` | `400 Bad Request` |
| والد ناموجود با شناسه `999999` | `404 Not Found` |

پس از آزمون‌های منفی، تعداد سرفصل‌ها همچنان دو مورد گزارش شد؛ بنابراین آزمون‌های نامعتبر رکورد جدید ایجاد نکرده بودند.

### داده‌های آزمون ثبت‌شده

1. شناسه `1`، کد `20100101`، نام `Test Current Account IRR`، ارز `1`، وضعیت `ACTIVE`، بدون والد.
2. شناسه `2`، کد `20100102`، نام `Test Savings Account IRR`، ارز `1`، والد `1`، وضعیت `ACTIVE`.

این نام‌ها نشان‌دهنده داده‌های آزمون‌اند و نباید معادل داده تولیدی فرض شوند.

## 8. آزمون Account Opening API

مسیر:

`POST /api/v1/accounts`

فیلدهای درخواست ثبت‌شده:
- `productId`
- `openingBranchId`
- `ownerCustomerIds`

مقادیر موجود در محیط آزمون:
- مشتری فعال: شناسه `49`
- محصول فعال: شناسه `1`
- شعبه فعال: شناسه `4`
- نگاشت محصول به سرفصل: شناسه `3`
- سرفصل دفتر کل: شناسه `1`

نتایج:

| آزمون | نتیجه |
|---|---|
| افتتاح حساب اول از طریق HTTP | `201 Created` |
| دریافت حساب اول | `200 OK` |
| افتتاح حساب دوم با همان مشتری، محصول و شعبه | `201 Created` |
| دریافت حساب دوم | `200 OK` |
| متفاوت‌بودن شماره حساب دو افتتاح | موفق |
| مانده اولیه حساب جدید | صفر |
| ایجاد Journal برای افتتاح حساب صفرمانده | انجام نشد |

شماره‌های مشاهده‌شده:
- `0100000000017`
- `0100000000022`

این دو شماره نمونه نتیجه اجرای الگوریتم هستند؛ نباید در کد به‌صورت ثابت استفاده شوند.

نتیجه تاریخی فوق نشان می‌دهد دو افتتاح حساب و دریافت جزئیات آن‌ها در محیط آزمون موفق بوده‌اند. نام دقیق دیتابیس مورد استفاده در این رکورد تاریخی مشخص نشده است؛ در نتیجه نباید بدون بررسی، آن را آزمون تأییدشده روی دیتابیس اصلی محسوب کرد.

## 9. آزمون Account API و چرخه عمر حساب

مسیرهای بررسی‌شده:
- `GET /api/v1/accounts/{id}`
- `PATCH /api/v1/accounts/{id}/status`

نتایج ثبت‌شده:

| آزمون | نتیجه |
|---|---|
| دریافت حساب موجود | `200 OK` |
| دریافت حساب `999999` | `404 Not Found` |
| وضعیت مقصد `UNKNOWN` | `400`, کد `INVALID_ACCOUNT_STATUS` |
| انتقال `ACTIVE` به `DORMANT` | `200 OK` |
| انتقال `DORMANT` به `ACTIVE` | `200 OK` |
| انتقال `ACTIVE` به `BLOCKED` | `200 OK` |
| انتقال `BLOCKED` به `ACTIVE` | `200 OK` |
| انتقال `ACTIVE` به `ACTIVE` | `422`, کد `ACCOUNT_STATUS_TRANSITION_REJECTED` |
| بستن حساب دوم | `200 OK` |
| فعال‌سازی حساب بسته‌شده | `422`, کد `ACCOUNT_STATUS_TRANSITION_REJECTED` |

در آزمون ثبت‌شده، پس از انتقال نامعتبر وضعیت قبلی حساب حفظ شد. مقدار `closedAt` حساب بسته‌شده نیز پس از رد فعال‌سازی مجدد حفظ شده بود.

وضعیت پایانی گزارش‌شده:
- حساب `1`: `ACTIVE`
- حساب `2`: `CLOSED`

این نتایج تمام ترکیب‌های وضعیت و تمام قواعد کسب‌وکار حساب را پوشش نمی‌دهند.

## 10. آزمون Deposit API در دیتابیس اصلی

تاریخ ثبت گزارش: 2026-10-26.

مسیر:

`POST /api/v1/deposits`

نتیجه ثبت‌شده:
- واریز یک واحد به حساب `1` در دیتابیس اصلی محلی `corebanking` موفق بود.
- تراکنش ایجادشده: شناسه `1`.
- Journal ایجادشده: شناسه `1`.
- وضعیت Journal: `POSTED`.
- سند متوازن گزارش شد.
- پس از واریز، `ledger_balance` حساب `1` برابر `1` و `available_balance` نیز برابر `1` بود.
- Retry با همان کلید Idempotency همان تراکنش را برگرداند و رکورد تکراری ایجاد نکرد.

سرفصل‌های استفاده‌شده در سابقه:
- سرفصل تسویه `10100101`: شناسه `4`، ارز `1`، دسته `ASSET`، فعال و قابل ثبت.
- سرفصل سپرده مشتری `20100101`: دسته `LIABILITY`.

این گزارش به معنای آزمون کامل همه حالت‌های Idempotency یا تأیید نهایی یکپارچگی در بار هم‌زمان نیست. آزمون کلید یکسان با payload متفاوت، هم‌زمانی واقعی و تمام مسیرهای Rollback جداگانه لازم‌اند.

## 11. آزمون Internal Transfer API

مسیر:

`POST /api/v1/transfers`

**Snapshot تاریخی در 2026-10-26** (و نه وضعیت نهایی کنونی):
- در آن تاریخ، API انتقال داخلی در مخزن پیاده‌سازی شده بود.
- در آن snapshot، انتقال موفق روی دیتابیس اصلی `corebanking` هنوز ثبت نشده بود.
- حساب `2` در آن گزارش `CLOSED` بود؛ وضعیت جاری هر حساب باید از API/دیتابیس همان محیط بررسی شود.
- مسیر نیازمند تسویه اضافه‌برداشت با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شد.
- آزمون موفق انتقال در دیتابیس آزمایشی در سابقه قدیمی‌تر گزارش شده بود.

**اصلاح وضعیت در 2026-10-28:** کاربر تأیید کرده API انتقال داخلی تکمیل و تست شده است. این تأیید، عبارت «API تست نشده» را به‌عنوان وضعیت فعلی رفع می‌کند. با این حال، چون جزئیات تست جدید (محیط دیتابیس، payload، پاسخ و نتایج مانده/Journal) هنوز در این سند درج نشده، از آن نمی‌توان نتیجه گرفت که حتماً تست موفق در دیتابیس اصلی یا همه حالت‌های شکست اجرا شده است.

مواردی که همچنان در آزمون جامع نیازمند پوشش/ثبت شواهدند:
- انتقال با موجودی ناکافی و حفظ بی‌تغییری مانده در صورت رد.
- Rollback در تمام نقاط شکست.
- درخواست‌های هم‌زمان برای یک حساب مبدأ یا مقصد.
- Retry با کلید یکسان و محتوای متفاوت.
- تطبیق مانده حساب‌ها، مانده دفتر کل و Journal در همه سناریوها.
- مسیر نیازمند تسویه اضافه‌برداشت؛ تا تکمیل چرخه اضافه‌برداشت، رد امن این مسیر باید حفظ شود.

## 12. تاریخچه Commitهای مرتبط

Commitهای تاریخی ثبت‌شده:

| Commit | پیام |
|---|---|
| `ec035ff` | `Implement account transfer and financial posting` |
| `47815bd` | `Remove startup tests from main` |
| `b5c836a` | `Add product ledger mapping schema` |
| `b619965` | `Ignore database backup dumps` |
| `7ca1fd9` | `Implement product management API` |
| `0c5ac38` | `Add account opening and lifecycle API` |
| `eaf34ce` | `Add product and account balance rules schema` |
| `3258fdd` | `Add initial account holds migration` |
| `ec4706e` | `Add hold event idempotency validation` |
| `c37a1b7` | `Fix ledger balance posting for internal transfers` |
| `1fe1c1d` | `Implement deposit API and redesign internal transfers` |

این جدول بخشی از تاریخچه Commitهاست، نه تضمین ترتیب فعلی همه Commitها. در تاریخ 2026-10-26، دو Commit آخر بالا گزارش شده‌اند؛ SHA کامل HEAD و هماهنگی فعلی `origin/main` باید هنگام ادامه پروژه مجدداً بررسی شوند.

### جزئیات Commit افتتاح حساب

Commit `0c5ac38` شامل این تغییرات گزارش شده بود:
- سرویس افتتاح حساب.
- Controller افتتاح حساب.
- Controller دریافت حساب و تغییر وضعیت.
- اتصال مسیرها به HTTP Router.
- تغییرات لازم در `main.cpp` و `CMakeLists.txt`.
- Schema شماره 013 برای سریال مستقل نوع حساب.

### جزئیات تغییرات مسدودی

- `3258fdd`: افزودن Migration اولیه مسدودی.
- `ec4706e`: افزودن اعتبارسنجی Idempotency برای رویدادهای مسدودی.

### آخرین وضعیت Git ثبت‌شده

در تاریخ 2026-10-24، SHA محلی و ریموت زیر گزارش شده بود:

`ec4706e1a2d6dcb1b9ee0647d78b727ceed882c4`

این SHA پس از ثبت Commitهای 2026-10-26 دیگر نباید به‌عنوان آخرین HEAD فرض شود.

## 13. اشکالات و محدودیت‌های شناخته‌شده

### سازگاری سرویس‌های مالی با Schema جدید

سرویس‌های قدیمی مانده و ثبت حسابداری مرجع معماری جدید محسوب نمی‌شوند. مسیر انتقال واریز باید با قواعد و Schema جاری بررسی شود؛ این مورد به‌معنای منکرشدن تست APIهایی که کاربر تأیید کرده نیست، بلکه یکپارچگی مالی جامع و پوشش تمام سناریوها را هدف می‌گیرد.

### محاسبه مانده قابل برداشت

`available_balance` در Schema ذخیره می‌شود. وجود ستون به معنی تضمین محاسبه سازگار با مسدودی و اعتبار باقی‌مانده اضافه‌برداشت نیست.

### اضافه‌برداشت

تسویه اضافه‌برداشت و ثبت کامل بدهی مشتری و مطالبات بانک هنوز کامل و آزمون‌شده محسوب نمی‌شوند. مسیر انتقال نیازمند اضافه‌برداشت طبق سابقه با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود.

### مسدودی

Migration و Triggerهای مسدودی در دیتابیس ثبت شده‌اند، اما سرویس C++ و API کامل مسدودی و آزادسازی در وضعیت ثبت‌شده تأیید نشده‌اند.

### هم‌زمانی

آزمون SQL با `ROLLBACK` جایگزین آزمون بار هم‌زمان واقعی نیست. سناریوهای رقابتی در واریز، برداشت، انتقال، برگشت و آزادسازی مسدودی باید جداگانه آزمون شوند.

## 14. گام‌های بعدی ثبت‌شده در تاریخچه قبلی

این فهرست در تاریخچه قبلی نوشته شده بود و در بازبینی 2026-10-28 اصلاح شده است:
1. بررسی سازگاری `account_balance` و لایه‌های Repository/Service با Schema فعلی.
2. طراحی و تکمیل API ایجاد Hold، آزادسازی جزئی/کامل، انقضا و لغو؛ مسیرهای HTTP آن در Router بررسی‌شده وجود ندارند.
3. تکمیل Idempotency رویدادهای Hold: بازیابی نتیجه در Retry با همان `(hold_id, idempotency_key)` و یکسان‌بودن fingerprint/effective time.
4. تکمیل محاسبه اعتبار باقی‌مانده اضافه‌برداشت و ثبت بدهی مشتری/مطالبات بانک.
5. پوشش آزمون جامع HTTP و تطبیق مانده، Transaction، Journal، `ledger_account_balance` و رویدادهای Hold.
6. ثبت شواهد آزمون‌های مستقیم Province، Account Type و Product–Ledger Mapping در صورت موجودبودن/نیاز.
7. افزودن یا تعیین تکلیف endpointهای Withdrawal، Reversal، Balance و جست‌وجوی Transaction/Journal.
8. آزمون Idempotency با payload متفاوت، Rollback در نقاط شکست، موجودی ناکافی و بار هم‌زمان واقعی.
9. ثبت گزارش شفاف Build، تست HTTP و وضعیت Git. Commit/Push فقط پس از ارائه نتیجه و تأیید کاربر.

## 15. قاعده پایانی تاریخچه

History شاهدی از اتفاقات گذشته است. هر نتیجه باید همراه با محدودیت‌ها و محیط اجرای آن خوانده شود.

اگر در ادامه پروژه قاعده کسب‌وکار جدیدی تصویب شد، باید به `PROJECT_CONTEXT.md` منتقل شود. اگر آزمون جدیدی اجرا شد، نتیجه دقیق آن باید در همین فایل ثبت شود. سابقه قدیمی بدون آزمون جدید، مدرک کافی برای تأیید وضعیت جاری نیست.

## 16. بازبینی Router و اصلاح فهرست APIها — 2026-10-28

### منبع بررسی

- Commit گزارش‌شده از سوی کاربر: `038638e76a89012e1d94275b192514b4afb4696d`.
- فایل Router بررسی‌شده: `src/web/httprouter.cpp`.
- فایل سیم‌کشی Controllerها در برنامه: `src/main.cpp`.
- فهرست فایل‌های Build: `CMakeLists.txt`.
- اسناد بررسی‌شده: `docs/PROJECT_CONTEXT.md` و `docs/PROJECT_HISTORY.md`.

این بازبینی از کد Router برای استخراج method/path استفاده می‌کند، و از تأیید مستقیم کاربر برای وضعیت «تکمیل و تست‌شده» در دوازده گروه استفاده می‌کند. وجود یک route بدون گزارش تست، به‌تنهایی به‌عنوان تست‌شده ثبت نشده است. این بازبینی فقط مستنداتی/کدخوانی بود و در این نوبت Build یا آزمون HTTP جدیدی اجرا نشد؛ وضعیت آزمون APIهای دوازده‌گانه بر اساس تأیید کاربر است.

فایل‌های اصلاح‌شده‌ای که همراه این گزارش تحویل می‌شوند هنوز جایگزین فایل‌های درخت کاری کاربر نشده‌اند و Commit/Push جدیدی برای آن‌ها انجام نشده است.

### 16.1 فهرست کامل مسیرهای موجود در Router بررسی‌شده

| گروه | روش‌ها و مسیرها |
|---|---|
| Health | `GET /api/v1/health` |
| Transfers | `POST /api/v1/transfers` |
| Deposits | `POST /api/v1/deposits` |
| Accounts | `POST /api/v1/accounts`; `GET /api/v1/accounts/{id}`; `PATCH /api/v1/accounts/{id}/status` |
| Customers | `POST /api/v1/customers`; `GET /api/v1/customers?nationalId={value}`; `GET /api/v1/customers/{id}` |
| Currencies | `POST /api/v1/currencies`; `GET /api/v1/currencies`; `GET /api/v1/currencies/{id}` |
| Account Types | `POST /api/v1/account-types`; `GET /api/v1/account-types` (فیلتر `status`, `currencyId`); `GET /api/v1/account-types/{id}`; `PATCH /api/v1/account-types/{id}/status` |
| Products | `POST /api/v1/products`; `GET /api/v1/products` (فیلتر `status`, `accountTypeId`, `currencyId`); `GET /api/v1/products/{id}`; `PATCH /api/v1/products/{id}/status` |
| Product–Ledger Mappings | `POST /api/v1/product-ledger-mappings`; `GET /api/v1/product-ledger-mappings` (فیلتر `status`, `productId`, `ledgerAccountId`); `GET /api/v1/product-ledger-mappings/{id}`; `PATCH /api/v1/product-ledger-mappings/{id}/status`; `PATCH /api/v1/product-ledger-mappings/{id}/effective-to` |
| Ledger Accounts | `POST /api/v1/ledger-accounts`; `GET /api/v1/ledger-accounts` (فیلتر `status`, `category`, `currencyId`); `GET /api/v1/ledger-accounts/{id}`; `PATCH /api/v1/ledger-accounts/{id}/status` |
| Countries | `POST /api/v1/countries`; `GET /api/v1/countries`; `GET /api/v1/countries/{id}` |
| Provinces | `POST /api/v1/provinces`; `GET /api/v1/provinces`; `GET /api/v1/provinces/{id}` |
| Cities | `POST /api/v1/cities`; `GET /api/v1/cities`; `GET /api/v1/cities/{id}` |
| Branches | `POST /api/v1/branches`; `GET /api/v1/branches`; `GET /api/v1/branches/{id}`; `PUT /api/v1/branches/{id}`; `PATCH /api/v1/branches/{id}/status` |

Router در صورت ناشناخته‌بودن مسیر، پاسخ `404 Not Found` با کد `ROUTE_NOT_FOUND` برمی‌گرداند. برای Deposit و Transfer، اگر Controller مربوطه تنظیم نشده باشد، Router پاسخ `503 Service Unavailable` برمی‌گرداند.

### 16.2 APIهای تکمیل و تست‌شده — تأیید مستقیم کاربر در 2026-10-28

کاربر صریحاً تأیید کرد همه گروه‌های زیر پیاده‌سازی و تست شده‌اند. این جدول مستقل است تا هنگام ادامهٔ پروژه این قابلیت‌ها به‌اشتباه «باقی‌مانده» معرفی نشوند.

| گروه API | مسیرهای Router | وضعیت ثبت‌شده |
|---|---|---|
| Health | `GET /api/v1/health` | تکمیل و تست‌شده |
| Countries | `POST /api/v1/countries`؛ `GET /api/v1/countries`؛ `GET /api/v1/countries/{id}` | تکمیل و تست‌شده |
| Currencies | `POST /api/v1/currencies`؛ `GET /api/v1/currencies`؛ `GET /api/v1/currencies/{id}` | تکمیل و تست‌شده |
| Cities | `POST /api/v1/cities`؛ `GET /api/v1/cities`؛ `GET /api/v1/cities/{id}` | تکمیل و تست‌شده |
| Branches | `POST /api/v1/branches`؛ `GET /api/v1/branches`؛ `GET /api/v1/branches/{id}`؛ `PUT /api/v1/branches/{id}`؛ `PATCH /api/v1/branches/{id}/status` | تکمیل و تست‌شده |
| Customers | `POST /api/v1/customers`؛ `GET /api/v1/customers?nationalId={value}`؛ `GET /api/v1/customers/{id}` | تکمیل و تست‌شده |
| Products | `POST /api/v1/products`؛ `GET /api/v1/products`؛ `GET /api/v1/products/{id}`؛ `PATCH /api/v1/products/{id}/status` | تکمیل و تست‌شده |
| Ledger Accounts | `POST /api/v1/ledger-accounts`؛ `GET /api/v1/ledger-accounts`؛ `GET /api/v1/ledger-accounts/{id}`؛ `PATCH /api/v1/ledger-accounts/{id}/status` | تکمیل و تست‌شده |
| Account Opening | `POST /api/v1/accounts` | تکمیل و تست‌شده |
| Account Details / Status | `GET /api/v1/accounts/{id}`؛ `PATCH /api/v1/accounts/{id}/status` | تکمیل و تست‌شده |
| Deposits | `POST /api/v1/deposits` | تکمیل و تست‌شده |
| Internal Transfers | `POST /api/v1/transfers` | تکمیل و تست‌شده |

این وضعیت بر اساس تأیید مستقیم کاربر است. چون جزئیات کامل تست همه گروه‌ها (تاریخ، محیط، payload، پاسخ و تطبیق مانده/Journal) در History موجود نیست، هیچ خروجی HTTP جدیدی بازسازی یا حدس زده نشده و نباید همه این تست‌ها را خودبه‌خود «تست روی دیتابیس اصلی» یا «پوشش کامل همه سناریوها» نامید.

### 16.3 مسیرهای پیاده‌سازی‌شده که شواهد تست آن‌ها در History کافی نیست

برای Province، Account Type و تمام عملیات Product–Ledger Mapping، مسیر در Router وجود دارد ولی شواهد تست HTTP مستقیم هر عملیات در سابقه فعلی کامل نیست. نگاشت با شناسه `3` در آزمون افتتاح حساب استفاده شده؛ این تنها، به معنی تست همه endpointهای نگاشت نیست. این سه گروه را API مفقود تلقی نکنید؛ شواهد آزمون را بیابید یا فقط در صورت اجرا نشدن آزمون لازم، تست اضافه کنید.

### 16.4 مواردی که Router مسیرشان را ندارد

در Router بررسی‌شده endpointی برای Hold، Hold release/cancel/expiry، Withdrawal، Reversal، دریافت Balance، فهرست/جزئیات Transaction یا Journal، مدیریت `product_balance_rule` و `account_balance_rule_override`، مدیریت روابط/اختیارات حساب و مدیریت مستقل محدودیت‌های عملیاتی دیده نشد. وجود جدول یا Migration مرتبط به معنی وجود API نیست. این موارد در بخش «کارهای باقی‌مانده» ثبت می‌شوند؛ هیچ مسیر پیشنهادی به‌عنوان قرارداد فعلی فرض نشده است.

## 17. فهرست کارهای باقی‌مانده پس از بازبینی 2026-10-28

1. **Hold API:** تکمیل Controller/Service/Repository و ثبت مسیرهای HTTP برای ایجاد، مشاهده و آزادسازی جزئی/کامل، لغو و انقضا. منطق دیتابیس Migration 015 وجود دارد، اما endpointهای آن در Router نیستند.
2. **Withdrawal:** تعیین قرارداد و پیاده‌سازی برداشت با بررسی وجه قابل برداشت، Holdها، حداقل مانده، اضافه‌برداشت مجاز و Journal متوازن.
3. **Reversal:** ایجاد API برگشت که تراکنش و Journal معکوس مستقل بسازد، برگشت دوگانه را با کنترل هم‌زمانی رد کند و اثر کارمزد/Hold/Overdraft را لحاظ کند.
4. **Overdraft settlement:** جایگزینی مسیر رد `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` فقط پس از پیاده‌سازی و آزمون ثبت بدهی مشتری و مطالبات بانک؛ مانده سپرده مشتری نباید برای نمایش بدهی منفی شود.
5. **Balance and history endpoints:** تصمیم درباره نیاز و قرارداد APIهای دریافت مانده، تراکنش‌ها و Journal؛ در صورت تأیید نیاز، پیاده‌سازی با مجوزهای دسترسی لازم.
6. **Available balance invariant:** بررسی یکپارچگی فرمول `ledger_balance - active_holds + remaining_overdraft_limit` در Deposit، Transfer و مسیرهای مالی آینده.
7. **بازنویسی/هم‌ترازی سرویس‌های مالی قدیمی:** سرویس‌ها و Repositoryهای قدیمی مانده حساب، تراکنش و ثبت دفتر کل باید با Schema بازطراحی‌شده تطبیق داده یا بازنویسی شوند. تا وقتی سازگاری آن‌ها با `CURRENT_SCHEMA.sql` و قواعد جدید (Hold، Idempotency، Journal و اضافه‌برداشت) اثبات نشده، آن‌ها را مرجع معماری فعلی فرض نکنید.
8. **Financial reconciliation:** کنترل اینکه هر Journal ثبت‌شده متوازن باشد و مانده حساب‌ها با Transactionها و مجموع Debit/Credit دفتر کل سازگار بماند.
9. **Concurrency / Idempotency / Rollback:** تست درخواست‌های رقابتی؛ کلید یکسان با payload متفاوت؛ Retry؛ موجودی ناکافی؛ شکست بین درج Transaction، Journal و به‌روزرسانی مانده؛ اطمینان از عدم ثبت تکراری.
10. **ثبت شواهد آزمون:** ثبت مستقیم تست‌های Province، Account Type و Product–Ledger Mapping؛ تکمیل جزئیات تست‌های APIهای تأییدشده توسط کاربر در صورت امکان، بدون جعل خروجی تاریخی.
11. **بازبینی نهایی Router و راه‌اندازی:** تأیید اینکه `main.cpp` Controllerها را به Router متصل می‌کند، Build جاری موفق است و پاسخ 503 ناشی از Controller تنظیم‌نشده رخ نمی‌دهد.
12. **به‌روزرسانی اسناد بعد از هر کار:** Context برای قراردادها، تصمیم‌ها و وضعیت فعلی؛ History برای SHA، محیط، دستور، پاسخ، کنترل مانده/Journal و محدودیت‌های آزمون.

این فهرست به معنای باقی‌ماندن دوازده گروه API تأییدشده از سوی کاربر نیست؛ موارد فوق عمدتاً endpointهایی هستند که Router ندارد، عمق کامل آزمون مالی را می‌خواهند، یا شواهد مستقیمشان در History ناقص است.

## 18. پیاده‌سازی HoldService — 2026-11-01

### 18.1 محدوده و فایل‌ها

- ایجاد `src/application/holdservice.h`
- ایجاد `src/application/holdservice.cpp`
- تغییر `CMakeLists.txt` فقط برای افزودن دو فایل بالا به target (با تأیید کاربر)
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- ایجاد گزارش `docs/opencode-reports/HOLD_SERVICE_IMPLEMENTATION.md`

در این مرحله هیچ Controller، Router، Endpoint، `main.cpp`، Migration یا تغییر Git انجام نشد.

### 18.2 پیش‌نیاز Schema

کاربر اجرای موفق Migration 017 را تأیید کرد. با اتصال read-only به `corebanking` موارد زیر تأیید شدند:

- ستون‌های `account_hold.idempotency_key` (`varchar(100)`, nullable) و `account_hold.request_fingerprint` (`varchar(64)`, nullable)
- `ck_hold_idempotency_pair`، `ck_hold_idempotency_key_length` و `ck_hold_fingerprint_length`
- ایندکس یکتای جزئی `uq_hold_api_idempotency` روی `idempotency_key` برای مقادیر غیر NULL
- وجود VIEW `account_hold_status` و Trigger اعتبارسنجی رویداد مطابق Migration 015

هیچ DDL یا DML اجرا نشد.

### 18.3 Build

دستور اجراشده:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

نتیجه واقعی خروجی:

```text
-- Configuring done
-- Generating done
-- Build files have been written to: /home/ahmad/projects/build-CoreBanking-Desktop-Debug
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/infrastructure/database.cpp.o
[  4%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

Build موفق بود. هیچ هشدار کامپایلر در خروجی مشاهده نشد. پس از یک اصلاح کوچک در بایندینگ پارامتر Idempotency، Build افزایشی دوباره اجرا شد و مجدداً با `[100%] Built target CoreBanking` پایان یافت.

### 18.4 آزمون‌ها

- آزمون HTTP انجام نشد؛ در این مرحله هیچ Controller/Route برای Hold وجود ندارد و فراخوانی سرویس از طریق API محلی ممکن نیست.
- داده آزمایشی ایجاد نشد و هیچ نوشتن مستقیم در دیتابیس انجام نشد.
- بررسی دیتابیس فقط read-only با نقش `corebanking_reader` انجام شد.
- آزمون عملکردی `createHold`/`createEvent`، Idempotency، قفل‌ها و سناریوهای خطا باید در مرحله Controller/Route و با API واقعی انجام و ثبت شود.

### 18.5 نکات پیاده‌سازی

- `createHold`: اعتبارسنجی ورودی، fingerprint بخش 8.1 طراحی، Advisory Lock با پیشوند `HOLD:`، Idempotency Lookup روی `account_hold`، قفل Account → Balance → Hold، بررسی وضعیت حساب/محصول/سرفصل، `AvailableForHold = ledger_balance - active holds`، INSERT با Idempotency، خواندن `account_hold_status` پیش از Commit.
- `createEvent`: پارس و نرمال‌سازی زمان پیش از Idempotency، fingerprint بخش 8.2، Advisory Lock با پیشوند `HOLD-EVENT:`، Pre-check رویداد پیش از اعتبارسنجی وضعیت/مبلغ، قفل Account → Balance → Hold، بازخوانی وضعیت از VIEW، قواعد مبلغ PARTIAL/FULL/EXPIRY/CANCELLATION و نگاشت خطاهای Trigger.
- در همه SELECTها خطای `exec()` از نتیجه صفر ردیف (`query.next() == false`) جدا مدیریت می‌شود و `numRowsAffected()` مبنای تشخیص وجود ردیف نیست.
- در مسیر رقابت نادر `uq_hold_api_idempotency`، برای امکان ادامه Query پس از خطای PostgreSQL از `SAVEPOINT hold_insert` و `ROLLBACK TO SAVEPOINT` استفاده شد؛ سپس ردیف موجود خوانده و نتیجه قبلی برگردانده می‌شود.
- فیلد `replayed` به `HoldResult` و `HoldEventResult` اضافه شد تا Controller مرحله بعد بتواند 201 (ایجاد جدید) را از 200 (Retry) تفکیک کند.
- کد خطای `HOLD_INVALID_REFERENCE` برای طول بیش از حد `referenceType`/`referenceId` اضافه شد؛ این مورد در جدول خطاهای طراحی به‌صراحت نیامده بود ولی برای جلوگیری از خطای 500 ناشی از طول ستون لازم است.
- برای محاسبه موجودی Hold، تابع بارگذاری وضعیت حساب علاوه بر فیلدهای Deposit، ستون `b.ledger_balance` را نیز می‌خواند؛ به همین دلیل SQL آن نسخه تطبیق‌یافته‌ی `loadLockedAccount` است و بخش Overdraft (که برای Hold استفاده نمی‌شود) حذف شده است.

### 18.6 محدودیت‌های باقی‌مانده

- Controller، Route و Endpointهای Hold هنوز وجود ندارند.
- قرارداد پاسخ JSON و نگاشت HTTP (201/200/422/409/404/500) در مرحله Controller پیاده‌سازی و آزمون می‌شود.
- آزمون هم‌زمانی واقعی، Retry با payload متفاوت، موجودی ناکافی و بررسی مالی پس از Commit هنوز اجرا نشده‌اند.

### 18.7 تکمیل HoldService — 2026-11-02

#### محدوده و فایل‌ها

- تغییر `src/application/holdservice.h`
- تغییر `src/application/holdservice.cpp`
- به‌روزرسانی `docs/PROJECT_CONTEXT.md`
- به‌روزرسانی همین سند
- به‌روزرسانی `docs/opencode-reports/HOLD_SERVICE_IMPLEMENTATION.md`

هیچ Controller، Route، Endpoint، Migration، تغییر Schema، `main.cpp`، `CMakeLists.txt`، `DepositService` یا `TransferService` تغییر نکرد. هیچ عملیات Git انجام نشد.

#### تغییرات پیاده‌سازی

- افزودن `databaseNow()` با خواندن `CURRENT_TIMESTAMP` داخل تراکنش و Truncate به میلی‌ثانیه. زمان پیش‌فرض Hold و Event اکنون از دیتابیس می‌آید.
- Fingerprint بدون زمان ارسالی تغییر نکرده است؛ زمان پیش‌فرض سرور وارد Fingerprint نمی‌شود.
- Idempotency Lookup در `createHold` و `createEvent` پیش از اعتبارسنجی‌های وابسته به وضعیت فعلی Hold باقی مانده است.
- افزودن `HoldStatusData`، `HoldEventData`، `HoldQueryResult`، `HoldListResult` و `HoldEventsResult`.
- افزودن `getHoldById`، `getHoldsByAccount` و `getHoldEvents`.
- افزودن `snapshot` و `hasSnapshot` به `HoldResult` و `HoldEventResult` و پرکردن آن در همه مسیرهای `Completed` پیش از Commit، شامل Retry و بازیابی Unique Conflict.
- مقاوم‌سازی نگاشت خطا با SQLSTATE، `databaseText()`، `text()` و نام Constraint/Index. کدهای اعتبارسنجی Trigger به Rejected و کدهای immutability/ناشناخته به Failed نگاشت می‌شوند.
- افزودن `SAVEPOINT hold_event_insert` و مسیر بازیابی `uq_account_hold_event_idempotency`. شکست `ROLLBACK TO SAVEPOINT` باعث پایان تراکنش با خطا می‌شود و ادامه Query انجام نمی‌شود.

#### Build

دستور اجراشده:

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق. هشدار کامپایلری در خروجی مشاهده نشد.

#### آزمون‌ها

- **آزمون HTTP: انجام نشد.** در این مرحله Controller/Route/Endpoint وجود ندارد و نوشتن از طریق API ممکن نیست.
- **آزمون نوشتن Hold/Event: انجام نشد.** هیچ داده آزمایشی ایجاد نشد و هیچ DML اجرا نشد.
- **بررسی Schema: فقط read-only** با نقش `corebanking_reader` روی `corebanking`:
  - وجود `account_hold.idempotency_key` و `request_fingerprint`
  - وجود ایندکس `uq_hold_api_idempotency`
  - وجود ستون‌های VIEW `account_hold_status`
  - وجود Triggerهای `trg_validate_account_hold_event` و `trg_account_hold_event_immutable`
  - خروجی نمونه `CURRENT_TIMESTAMP` با فرمت ISO-8601 UTC و میلی‌ثانیه
- **آزمون مسیرهای خواندن:** یک Harness موقت خارج از مخزن در `/tmp/opencode/hold_read_test.cpp` با اتصال read-only به نقش `corebanking_reader` اجرا شد. موارد بررسی‌شده:
  - `getHoldById(0)` → `Failed` با `HOLD_INVALID_HOLD_ID`
  - `getHoldById(999999999)` → `NotFound` با `HOLD_NOT_FOUND`
  - `getHoldsByAccount(0)` → `Failed` با `HOLD_INVALID_ACCOUNT_ID`
  - `getHoldsByAccount(1)` → `Completed` با لیست خالی
  - `getHoldEvents(0)` → `Failed` با `HOLD_INVALID_HOLD_ID`
  - `getHoldEvents(999999999)` → `HoldNotFound` با `HOLD_NOT_FOUND`
  - خروجی: `READ_METHOD_CHECKS_PASSED`
- در زمان اجرای آزمون، جدول‌های `account_hold` و `account_hold_events` صفر ردیف داشتند؛ بنابراین مسیرهای خواندن با داده واقعی Hold و Event و همچنین Snapshot مسیر Create هنوز آزمون نشده‌اند.

#### شناسه‌های ایجادشده

هیچ شناسه کسب‌وکاری جدیدی ایجاد نشد.

#### محدودیت‌های باقی‌مانده

- Controller، Route و Endpointهای Hold هنوز وجود ندارند.
- آزمون HTTP، Idempotency واقعی، Snapshot پس از Create، خطاهای Trigger و هم‌زمانی باید در مرحله Controller با API واقعی انجام شوند.
- `database/CURRENT_SCHEMA.sql` نسبت به Migration 017 همچنان قدیمی است؛ در این مرحله تغییر نکرد.

### 18.8 HoldController و اتصال HTTP — 2026-11-02

#### محدوده و فایل‌ها

- ایجاد `src/web/holdcontroller.h`
- ایجاد `src/web/holdcontroller.cpp`
- تغییر `src/web/httprouter.h` و `src/web/httprouter.cpp`
- تغییر `src/main.cpp`
- تغییر `CMakeLists.txt` برای افزودن فایل‌های Controller
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- ایجاد `docs/opencode-reports/HOLD_CONTROLLER_IMPLEMENTATION.md`

هیچ Migration، DDL/DML مستقیم، تغییر Schema، تغییر `DepositService`/`TransferService` یا عملیات Git انجام نشد.

#### Endpointهای پیاده‌شده

- `POST /api/v1/holds`
- `GET /api/v1/holds/{id}`
- `GET /api/v1/holds?accountId={id}`
- `POST /api/v1/holds/{id}/events`
- `GET /api/v1/holds/{id}/events`

Controller برای پاسخ Create از Snapshot سرویس استفاده می‌کند و پس از Commit هیچ Query جدیدی اجرا نمی‌کند. ایجاد جدید `201`، Retry دقیق `200`، تعارض Idempotency `409`، ورودی نامعتبر `400`، Hold/Account ناموجود `404`، رد کسب‌وکاری `422` و خطای داخلی `500` برمی‌گرداند. نبود Controller در Router نیز `503 HOLD_SERVICE_UNAVAILABLE` است.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
-- Configuring done
-- Generating done
-- Build files have been written to: /home/ahmad/projects/build-CoreBanking-Desktop-Debug
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

در تلاش اول، خطای نوع `QString` در برابر `HttpResponse::status` مشاهده و اصلاح شد؛ Build نهایی موفق بود.

#### آزمون HTTP محلی

سرور محلی از Build فعلی روی `http://localhost:8080` اجرا و پس از آزمون متوقف شد. همه نوشتن‌ها از API انجام شدند.

| سناریو | نتیجه |
|---|---|
| `POST /api/v1/holds` با `stage3-hold-001-20261102` و مبلغ `0.10` | `201`، `holdId=1`، status=`ACTIVE`، snapshot کامل |
| Retry همان Hold با همان Payload | `200`، همان `holdId=1`، `replayed=true` |
| همان کلید با Payload متفاوت | `409 IDEMPOTENCY_KEY_REUSED` |
| `GET /api/v1/holds/1` | `200`، Snapshot منطبق |
| `GET /api/v1/holds/999999999` | `404 HOLD_NOT_FOUND` |
| `GET /api/v1/holds?accountId=1` | `200`، `count=1` |
| `GET /api/v1/holds` بدون `accountId` | `400 MISSING_ACCOUNT_ID` |
| `GET /api/v1/holds?accountId=abc` | `400 HOLD_INVALID_ACCOUNT_ID` |
| `GET /api/v1/holds/abc` | `400 INVALID_HOLD_ID` |
| `POST /holds/1/events` با PARTIAL `0.04` | `201`، `eventId=1`، remaining=`0.06`، status=`PARTIALLY_RELEASED` |
| Retry همان Event | `200`، همان `eventId=1`، `replayed=true` |
| `POST /holds/1/events` با FULL_RELEASE | `201`، `eventId=2`، remaining=`0`، status=`RELEASED` |
| Retry FULL_RELEASE | `200`، همان `eventId=2`، `replayed=true` |
| همان کلید Event با Payload متفاوت | `409 IDEMPOTENCY_KEY_REUSED` |
| `GET /holds/1/events` | `200`، `count=2`، هر دو Event |
| `POST /holds/999999999/events` | `404 HOLD_NOT_FOUND` |
| `GET /holds/999999999/events` | `404 HOLD_NOT_FOUND` |
| JSON نامعتبر | `400 INVALID_JSON` |
| `eventType` نامعتبر | `400 HOLD_INVALID_EVENT_TYPE` |
| PARTIAL برابر remaining | `422 PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT` |
| رویداد با `effectiveAt` آینده | `422 ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED` |
| Hold با مبلغ بیشتر از موجودی قابل Hold | `422 INSUFFICIENT_AVAILABLE_FUNDS` |
| Hold برای حساب ناموجود | `404 ACCOUNT_NOT_FOUND` |

داده‌های ساخته‌شده:

- Hold `1`: `stage3-hold-001-20261102`، مبلغ `0.10`، وضعیت نهایی `RELEASED`
- Event `1`: `PARTIAL_RELEASE` مبلغ `0.04` روی Hold `1`
- Event `2`: `FULL_RELEASE` مبلغ `0.06` روی Hold `1`
- Hold `2`: `stage3-hold-002-20261102`، مبلغ `0.05`، وضعیت نهایی `RELEASED`
- Event `3`: `FULL_RELEASE` مبلغ `0.05` روی Hold `2`

بررسی read-only دیتابیس:

- `account_balance` حساب `1` قبل و بعد از همه عملیات: `ledger_balance=1`، `available_balance=1`
- `account_hold_status`: Holdهای `1` و `2` با `remaining_amount=0`، `currently_blocked_amount=0` و وضعیت `RELEASED`
- `account_hold_events`: سه رویداد مطابق موارد بالا

#### محدودیت‌ها و انحراف‌های ثبت‌شده

- `HoldEventResult` در این مرحله فیلدهای واقعی Event را برنمی‌گرداند و Controller آن‌ها را از ورودی درخواست منعکس می‌کرد. این محدودیت در بخش 18.9 رفع شد.
- آزمون هم‌زمانی، بار، Expiry Release واقعی، CANCELLATION_RELEASE و پوشش کامل همه حالت‌های مرزی Trigger انجام نشده است.
- `database/CURRENT_SCHEMA.sql` همچنان نسبت به Migration 017 قدیمی است و تغییر نکرد.

### 18.9 اصلاح HoldEventResult برای بازگرداندن داده واقعی Event — 2026-11-02

#### محدوده و فایل‌ها

- تغییر `src/application/holdservice.h`
- تغییر `src/application/holdservice.cpp`
- تغییر `src/web/holdcontroller.h`
- تغییر `src/web/holdcontroller.cpp`
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- ایجاد `docs/opencode-reports/HOLD_EVENT_RESULT_FIX.md`

هیچ Migration، DDL/DML مستقیم، تغییر Schema، تغییر منطق کسب‌وکار یا تغییر در سرویس‌های دیگر انجام نشد.

#### تغییرات

- `HoldEventResult` اکنون فیلدهای `eventId`، `holdId`، `eventType`، `amount`، `effectiveAt` و `reasonCode` را دارد.
- یک Loader جدید، رکورد Event را با `eventId` از `account_hold_events` داخل همان تراکنش می‌خواند.
- این Loader در مسیر ایجاد جدید، Trigger RETURN NULL، بازیابی Unique Conflict و Idempotency Retry پیش از Commit فراخوانی می‌شود.
- Controller دیگر از `amount`، `effectiveAt` و `reasonCode` ورودی برای ساخت پاسخ استفاده نمی‌کند و پاسخ را از `HoldEventResult` می‌سازد.
- هیچ Query پس از Commit اضافه نشد.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/main.cpp.o
[  4%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httpserver.cpp.o
[  5%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/httprouter.cpp.o
[  7%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  8%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

#### آزمون HTTP

- Hold جدید `3` از API با مبلغ `0.01` و کلید `stage4-hold-001-20261102` ایجاد شد.
- Event بدون `effectiveAt` با `FULL_RELEASE` و کلید `stage4-evt-full-001` ایجاد شد:
  - پاسخ ایجاد: `201`، `eventId=4`، `amount="0.01"`، `effectiveAt="2026-11-02T18:41:31.810Z"`، `holdStatus="RELEASED"`
  - Retry همان درخواست: `200`، همان `eventId=4`، همان `amount` و همان `effectiveAt`، `replayed=true`
- بررسی read-only دیتابیس:
  - رکورد Event `4`: `amount=0.01`، `effective_at=2026-11-02 22:11:31.81+03:30`، `reason_code=API_TEST_EVENT_DEFAULT_TIME`
  - زمان پاسخ `18:41:31.810Z` با مقدار ذخیره‌شده در دیتابیس منطبق است.
- `account_balance` حساب `1` پس از آزمون: `ledger_balance=1`، `available_balance=1`
- Hold `3` در `account_hold_status`: `remaining_amount=0`، `currently_blocked_amount=0`، `status=RELEASED`

#### محدودیت

آزمون هم‌زمانی و مسیرهای Expiry/Cancellation همچنان خارج از این اصلاح باقی مانده‌اند.

### 18.10 اصلاح دقت مبلغ در Hold API — 2026-11-02

#### محدوده و فایل‌ها

- تغییر `src/web/holdcontroller.cpp`
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- به‌روزرسانی `docs/opencode-reports/HOLD_CONTROLLER_DESIGN.md`
- به‌روزرسانی `docs/opencode-reports/HOLD_CONTROLLER_IMPLEMENTATION.md`
- ایجاد `docs/opencode-reports/HOLD_AMOUNT_VALIDATION_FIX.md`

هیچ Migration یا تغییر Schema، DDL/DML مستقیم، تغییر `HoldService`، تغییر `DepositService`/`TransferService` یا عملیات Git انجام نشد.

#### منطق اعتبارسنجی

- `amountToText` که ورودی عددی JSON را با `QString::number(value, 'f', 8)` تبدیل می‌کرد حذف شد.
- `amount` در Hold و Event فقط به‌صورت JSON string پذیرفته می‌شود.
- رشته خالی، فاصله ابتدا/انتها و ورودی عددی JSON در Controller با `400 INVALID_AMOUNT` رد می‌شوند.
- اعتبارسنجی دقیق‌تر مقدار (مثبت بودن، حداکثر ۸ رقم اعشار، رد قالب علمی/جداکننده/صفر) در `HoldService` باقی مانده و پاسخ آن `400 HOLD_INVALID_AMOUNT` است.
- `HoldService` مقدار معتبر را قبل از Fingerprint نرمال می‌کند؛ بنابراین `"0.10"` و `"0.1"` یک مقدار canonical دارند.
- ستون‌های مبلغ در Schema فعلی `numeric` بدون precision/scale ثابت هستند؛ محدودیت ۸ رقم اعشار قاعده موجود سرویس است و فرض جدیدی درباره دقت ارز ایجاد نشد.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

#### آزمون‌های HTTP

| ورودی | نتیجه |
|---|---|
| `"0.12"` | `201`، مبلغ ذخیره‌شده `0.12` |
| `"0.12345678"` | `201`، مبلغ ذخیره‌شده دقیقاً `0.12345678` |
| `"0.123456789"` | `400 HOLD_INVALID_AMOUNT` |
| عدد JSON `0.1` | `400 INVALID_AMOUNT` |
| `"0"` | `400 HOLD_INVALID_AMOUNT` |
| `"-0.1"` | `400 HOLD_INVALID_AMOUNT` |
| `""` | `400 INVALID_AMOUNT` |
| `" 0.1"` و `"0.1 "` | `400 INVALID_AMOUNT` |
| `"   "` | `400 INVALID_AMOUNT` |
| `"1e-3"` | `400 HOLD_INVALID_AMOUNT` |
| `"1,000.00"` | `400 HOLD_INVALID_AMOUNT` |
| `"+0.1"` | `400 HOLD_INVALID_AMOUNT` |
| `"01.0"` | `400 HOLD_INVALID_AMOUNT` |
| Event با `amount` عددی JSON | `400 INVALID_AMOUNT` |
| Event با `"0.123456789"` | `400 HOLD_INVALID_AMOUNT` |

آزمون Idempotency canonical:

- ایجاد Hold با `amount="0.10"` و کلید `amount-canonical-20261102` → `201`، `holdId=6`
- Retry همان کلید با `amount="0.1"` → `200`، همان `holdId=6`، `replayed=true`
- مقدار ذخیره‌شده در دیتابیس: `0.1`

#### داده‌های آزمایشی جدید

- Hold `4`، مبلغ `0.12`، کلید `amount-valid-2dp-20261102`، وضعیت نهایی `RELEASED`
- Hold `5`، مبلغ `0.12345678`، کلید `amount-valid-8dp-20261102`، وضعیت نهایی `RELEASED`
- Hold `6`، مبلغ `0.1`، کلید `amount-canonical-20261102`، وضعیت نهایی `RELEASED`
- Eventهای `5`، `6` و `7` برای آزادسازی کامل این Holdها

`account_balance` حساب `1` پس از آزمون: `ledger_balance=1`، `available_balance=1`.

#### محدودیت‌ها

- آزمون هم‌زمانی انجام نشد.
- هیچ مبلغی در مسیر عددی JSON گرد نشد؛ چنین ورودی‌ای اکنون رد می‌شود.

### 18.11 اصلاح Idempotency مربوط به `effectiveAt` رویداد — 2026-11-03

#### محدوده و فایل‌ها

- تغییر `src/application/holdservice.cpp`
- تغییر `src/web/holdcontroller.cpp`
- به‌روزرسانی `docs/PROJECT_CONTEXT.md` و همین سند
- ایجاد `docs/opencode-reports/HOLD_EVENT_EFFECTIVE_AT_IDEMPOTENCY_FIX.md`

هیچ Migration یا تغییر Schema، DDL/DML مستقیم، تغییر `DepositService`/`TransferService`، تغییر `main.cpp`/`CMakeLists.txt` یا عملیات Git انجام نشد.

#### بررسی Fingerprint و علت نقص

- `fingerprintForEvent` شامل `holdId`، `eventType`، `amount` (فقط `PARTIAL_RELEASE`)، `effectiveAt` (نرمال‌شده یا رشته خالی)، `reasonCode`، `referenceType` و `referenceId` است.
- نتیجه Fingerprint رویداد ذخیره نمی‌شود؛ `account_hold_events` ستون `request_fingerprint` ندارد و Lookup فیلدبه‌فیلد است.
- در `lookupEventByKey` قدیمی، اگر Retry بدون `effectiveAt` بود، شرط زمان Wildcard می‌شد و زمان ذخیره‌شده مقایسه نمی‌شد؛ در نتیجه Retry بدون زمان برای رویداد اولیه صریح به‌اشتباه Replay می‌شد.

#### منطق اصلاح‌شده

- Presence/absence زمان در Lookup اضافه شد. برای درخواست بدون `effectiveAt`، ردیف ذخیره‌شده باید `effective_at = date_trunc('milliseconds', created_at)` باشد. برای درخواست با زمان صریح، مقدار ذخیره‌شده باید با زمان نرمال‌شده کلاینت برابر باشد و شرط پیش‌فرض بالا را نداشته باشد.
- عدم تطابق `effectiveAt` با کد `IDEMPOTENCY_CONFLICT` و HTTP `409` پاسخ داده می‌شود. این کد در `HoldController` به `409 Conflict` نگاشت شد.
- سایر فیلدها همچنان با `IDEMPOTENCY_KEY_REUSED` و HTTP `409` رد می‌شوند.
- `fingerprintForEvent` تغییر نکرد، چون از قبل «غایب» را با رشته خالی و «صریح» را با مقدار نرمال‌شده تفکیک می‌کرد؛ نقص در منطق Lookup بود.

#### Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی نهایی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/holdcontroller.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/holdservice.cpp.o
[  4%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

#### آزمون‌های HTTP

سرور محلی `http://localhost:8080`، دیتابیس `corebanking`. Hold آزمایشی `7` روی حساب `1` با مبلغ `0.50` ساخته شد. زمان‌های صریح: `T1=2026-11-03T16:23:06.000Z` و `T2=2026-11-03T16:23:07.000Z`.

| سناریو | نتیجه |
|---|---|
| Retry یکسان با `effectiveAt` صریح | `201` سپس `200`، `replayed=true`، همان `eventId=8` |
| Retry یکسان بدون `effectiveAt` | `201` سپس `200`، `replayed=true`، همان `eventId=9` |
| اولیه صریح، Retry بدون زمان | `201` سپس `409 IDEMPOTENCY_CONFLICT` |
| اولیه بدون زمان، Retry صریح | `201` سپس `409 IDEMPOTENCY_CONFLICT` |
| دو `effectiveAt` صریح متفاوت | `201` سپس `409 IDEMPOTENCY_CONFLICT` |
| تفاوت `reasonCode` بدون زمان | `201` سپس `409 IDEMPOTENCY_KEY_REUSED` |

#### بررسی دیتابیس

با نقش read-only `corebanking_reader`:

- برای `hold_id=7` دقیقاً ۶ رویداد با ۶ کلید مجزا ثبت شد؛ Retryها رکورد تکراری ایجاد نکردند.
- مجموع آزادشده `0.06` از مبلغ اولیه `0.50` بود.
- ردیف‌های بدون زمان دارای `effective_at = date_trunc('milliseconds', created_at)` و ردیف‌های صریح دارای مقدار T1 بودند.

#### داده‌های آزمایشی جدید

- Hold `7`، مبلغ `0.50`، کلید `effat-review-1793723166-hold`
- Eventهای `8` تا `13` با کلیدهای `effat-review-1793723166-t1` تا `effat-review-1793723166-t6`

#### محدودیت‌ها

- تشخیص Presence زمان به رابطه `effective_at` با `created_at` وابسته است، چون ستون Presence/Fingerprint در جدول رویداد وجود ندارد.
- اگر زمان صریح درخواست اولیه دقیقاً برابر `date_trunc('milliseconds', created_at)` باشد، تشخیص ممکن است اشتباه شود. رفع قطعی نیازمند Migration و ذخیره Presence/Fingerprint است و در این مرحله انجام نشد.
- آزمون هم‌زمانی برای این تغییر انجام نشد.

## 19. قاعده پایانی تاریخچه

History شاهد اتفاقات گذشته است. برای هر نتیجه، محیط و محدوده آزمون مشخص شود. اگر کاربر وضعیت جدیدی از تست تأیید کرد، وضعیت خلاصه باید به‌روز شود، ولی نباید جزئیات نامشخص مانند نام دیتابیس، پاسخ HTTP یا خروجی SQL حدس زده شود.

