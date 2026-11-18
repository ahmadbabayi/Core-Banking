# نتیجه مرحله تجمیعی Transfer — C3، C5، C7، C8 — 2026-11-18

نوع مرحله: چهار اصلاح محدود در `TransferController`/`TransferService`، Build واقعی، آزمون‌های API محلیِ قابل‌اجرا و بررسی read-only دیتابیس. هیچ Migration/DDL، SQL نوشتنی، Fault Injection یا عملیات نوشتنی Git انجام نشد.

## ۱. مبنا و وضعیت اولیهٔ Git

- شاخه: `main`
- HEAD: `33add8f0260de52d000f26831b01970cfb48fbd6`
- `33add8f` همان commit حل‌شدهٔ مورد انتظار است (`fix(transfer): use unit of work for begin/commit/rollback lifecycle`).
- تغییرات T1 (سیاست minimum) و T2 (UnitOfWork) در سورس حاضر بودند.
- `git status --short` پیش از کار: بدون خروجی (working tree پاک).

## ۲. فایل‌های تغییرکرده

| فایل | تغییر |
|---|---|
| `src/web/transfercontroller.cpp` | C3 |
| `src/application/transferservice.cpp` | C5، C7، C8 |
| `docs/opencode-reports/TRANSFER_C3_C5_C7_C8_RESULT.md` | گزارش همین مرحله |
| `docs/PROJECT_CONTEXT.md` | وضعیت جاری و ماتریس |
| `docs/PROJECT_HISTORY.md` | رکورد 18.31 |

خارج از دامنه (بدون تغییر): `transferservice.h`، `transfercontroller.h`، `unitofwork.*`، Repositoryها، Router، `main.cpp`، Schema/Migration، سایر سرویس‌ها، CMake و فایل‌های Qt Creator.

## ۳. تغییرات

### C3 — مبلغ عددی JSON

- ورودی رشته‌ای و اعتبارسنجی سرویس بدون تغییر ماند.
- برای JSON number: `QString::number(value, 'f', 8)` ساخته می‌شود؛ اگر `canonical.toDouble() != numericAmount` بود، پیش از فراخوانی سرویس `422 INVALID_AMOUNT` برگردانده می‌شود (همان الگوی `DepositController`).
- عدد معتبر ۸ اعشاری همچنان پذیرفته می‌شود؛ مبلغ صرفاً رشته‌ای نشد.
- این گارد از گردکردن هنگام قالب‌بندی جلوگیری می‌کند، نه از افت دقت هنگام parse شدن JSON.

### C8 — طول description

- روی `description` نرمال‌شده با `trim` و با `QString::size()` بررسی می‌شود؛ بیش از ۵۰۰ پیش از `Begin` و هر نوشتن با `Rejected / INVALID_DESCRIPTION` رد می‌شود (HTTP 422).
- رفتار Controller برای نوع نامعتبر (400) و پذیرش description خالی/اختیاری تغییر نکرد.

### C5 — پیام‌های خطا

- همهٔ مسیرهای خطای `TransferService` بازبینی شدند؛ هیچ `lastError().text()` باقی نمانده است (grep در هر دو فایل بدون نتیجه).
- متن خام درایور/SQL از `TransferResult` حذف و با پیام‌های ثابت و مرتبط جایگزین شد. کدهای خطا و HTTP statusها حفظ شدند.
- سه helper داخلی (`loadLockedAccounts`، `persistRejection`، `recalculateAvailableBalance`) نیز پیام ثابت برمی‌گردانند؛ پیام امن «اختلاف ارز سرفصل و حساب» حفظ شد.
- پیام «نتیجه Commit نامعلوم است» و رفتار Commit/Rollback مصوب T2 دست‌نخورده ماند.
- هیچ متن خام خطا، SQL، اطلاعات مشتری یا تنظیمات اتصال در لاگ/گزارش جدید چاپ نشد.

### C7 — تفکیک حساب ناموجود از مانده مقدارنیافته

- از همان Queryهای قفل موجود استفاده شد: `accountLocks` شناسه‌های `account` و `balanceLocks` شناسه‌های `account_balance` را برمی‌گردانند.
- اگر هر یک از دو حساب وجود نداشته باشد → `Rejected / ACCOUNT_NOT_FOUND` (مقدم).
- اگر هر دو حساب وجود داشته باشند ولی یکی ردیف مانده نداشته باشد → `Rejected / ACCOUNT_BALANCE_NOT_INITIALIZED`.
- خطای Query همچنان به `rollbackFailure` می‌رود و به «ردیف موجود نیست» تبدیل نمی‌شود.
- مسیر ثبت rejection و Commit، JOIN اسنپ‌شات، ترتیب قفل account → balance → hold و ترتیب صعودی شناسه‌ها تغییر نکردند؛ Query یا قفل تازه اضافه نشد.
- شاخهٔ دفاعی قبلی `accounts.size() != 2` (که پس از دو بررسی بالا فقط با اسنپ‌شات ناقص ممکن است) به `Failed / ACCOUNT_LOCK_OR_READ_FAILED` با پیام ثابت «Locked account snapshot is incomplete» تغییر کرد تا ناسازگاری داده به «حساب ناموجود» نسبت داده نشود.

## ۴. Build واقعی

```bash
cmake --build /home/it4850/Projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی واقعی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/transferservice.cpp.o
[  2%] Building CXX object CMakeFiles/CoreBanking.dir/src/web/transfercontroller.cpp.o
[  3%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلری. این Build آزمون اجرایی Commit/Rollback نیست.

## ۵. محیط آزمون API

- سرور از همان Build موجود روی `http://localhost:8080` اجرا شد؛ `GET /api/v1/health` پاسخ `200` و `{"data":{"status":"UP"},"success":true}` داد و پس از آزمون متوقف شد.
- **محدودیت محیط:** دیتابیس توسعهٔ `corebanking` هیچ دادهٔ پایهٔ قابل استفاده برای انتقال نداشت:

```text
accounts=0, balances=0, transactions=0, tx_entries=0, journals=0, journal_entries=0, holds=0
customers=0, products=0
```

- هیچ حساب تست مجاز یا fixture امنی وجود نداشت. مجموعهٔ کامل مشتری/محصول/سرفصل/حساب ساخته نشد، چون خارج از دامنهٔ این مرحله و یک اثر جانبی داده‌ای گسترده بود.

## ۶. نتیجهٔ آزمون‌های API (واقعی)

| # | سناریو | payload کلیدی | نتیجهٔ واقعی |
|---|---|---|---|
| A | عدد ۹ اعشاری که قالب‌بندی تغییرش می‌دهد | `amount: 0.123456789` | `422 INVALID_AMOUNT` (C3) |
| B | عدد معتبر ۸ اعشاری | `amount: 0.12345678` | `422 ACCOUNT_NOT_FOUND` — یعنی Controller آن را نپذیرفت/نگرد نکرد و به سرویس رسید (C3) |
| C | description با ۵۰۱ کاراکتر پس از trim | `"  " + "x"*501 + "  "` | `422 INVALID_DESCRIPTION` (C8) |
| D | description با ۵۰۰ کاراکتر پس از trim | `" " + "x"*500 + " "` | `422 ACCOUNT_NOT_FOUND` — یعنی طول مجاز بود و به سرویس رسید (C8) |
| E | مبلغ رشته‌ای معتبر | `amount: "1.00"` | `422 ACCOUNT_NOT_FOUND` |
| F | مبلغ رشته‌ای ۹ اعشاری | `amount: "0.123456789"` | `422 INVALID_AMOUNT` (اعتبارسنجی سرویس بدون تغییر) |
| G | description غایب | بدون description | `422 ACCOUNT_NOT_FOUND` (اختیاری‌بودن حفظ شد) |
| H | description غیررشته‌ای | `description: 123` | `400 INVALID_DESCRIPTION` (رفتار Controller حفظ شد) |
| I | حساب مقصد ناموجود | `destinationAccountId: 999999999` | `422 ACCOUNT_NOT_FOUND` |

نکتهٔ مهم: در همهٔ موارد بالا حساب مبدأ (`1`) نیز وجود نداشت؛ بنابراین `ACCOUNT_NOT_FOUND` از مسیر زودهنگام lookup حساب مبدأ آمد و مسیر جدید C7 (تفکیک بر اساس Queryهای قفل) اجرا نشد. کلیدهای Idempotency استفاده‌شده: `t4-c3-a-20261118`، `t4-c3-b-20261118`، `t4-c8-c-20261118`، `t4-c8-d-20261118`، `t4-c3-e-20261118`، `t4-c3-f-20261118`، `t4-c8-g-20261118`، `t4-c8-h-20261118`، `t4-c7-i-20261118`.

## ۷. آزمون‌های اجرانشده (صریح)

| مورد درخواستی | وضعیت | دلیل |
|---|---|---|
| انتقال موفق با مبلغ رشته‌ای معتبر | اجرا نشد | هیچ حساب/مشتری/محصول/سرفصل موجود نبود |
| انتقال موفق با مبلغ عددی معتبر ۸ اعشاری | فقط پذیرش Controller تأیید شد؛ انتقال اجرا نشد | همان محدودیت محیط |
| ثبت و بازیابی description ۵۰۰ کاراکتری در DB | اجرا نشد | نیازمند انتقال موفق |
| حساب مقصد ناموجود با مبدأ موجود | اجرا نشد | مبدأ موجود نبود؛ مسیر C7 اجرا نشد |
| حساب موجود بدون ردیف مانده → `ACCOUNT_BALANCE_NOT_INITIALIZED` | اجرا نشد | fixture امن موجود نبود و حذف ردیف مانده ممنوع بود |
| replay همان payload/کلید و بررسی نبود اثر مالی تکراری | اجرا نشد | نیازمند انتقال موفق |
| مسیرهای خطای Commit/Rollback (T2) و C5 با خطای واقعی DB | اجرا نشد | نیازمند Fault Injection/خطای واقعی دیتابیس |

## ۸. بررسی دیتابیس (read-only با `corebanking_reader`)

پس از آزمون‌های API:

```text
accounts=0, balances=0, transactions=0, tx_entries=0,
journals=0, journal_entries=0, holds=0
```

- `SELECT id, transaction_number, status, result_code, amount, idempotency_key FROM transaction ...` → بدون ردیف.
- نتیجه: ردهای اعتبارسنجی C3/C8 هیچ تراکنش یا نوشتن مالی ایجاد نکردند.
- بررسی «انتقال موفق مانده و Journal مورد انتظار دارد» و «replay اثر مالی تکراری ندارد» اجرا نشد، چون هیچ انتقال موفقی امکان‌پذیر نبود.

## ۹. شناسه‌های دادهٔ تست

هیچ شناسهٔ کسب‌وکاری جدیدی ایجاد نشد (نه حساب، نه تراکنش، نه Journal). فقط درخواست‌های ردشدهٔ API اجرا شدند.

## ۱۰. محدودیت‌ها

- T1 (Query تولیدی با Rule واقعی PostgreSQL) و T2 (آزمون runtime چرخهٔ Commit/Rollback) همچنان بسته‌نشده‌اند.
- C5 فقط با بازبینی کد تأیید شده و با خطای واقعی دیتابیس آزمون نشده است.
- C7 فقط ساختاراً پیاده شده؛ مسیرهای `ACCOUNT_BALANCE_NOT_INITIALIZED` و تفکیک مقصد ناموجود آزمون runtime ندارند.
- C4 (State Machine)، محدودیت‌های هم‌زمانی، Overdraft واقعی و آمادگی انتشار همچنان باز هستند.
- آزمون‌های این مرحله روی دیتابیس خالی انجام شد و به‌تنهایی صحت مالی Transfer را اثبات نمی‌کنند.
