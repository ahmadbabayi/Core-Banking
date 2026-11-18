# نتیجه گذار وضعیت Transfer — INITIATED → PROCESSING → COMPLETED — 2026-11-18

نوع مرحله: یک تغییر محدود در مسیر موفق `TransferService`، Build واقعی، یک آزمون متمرکز گذار، آزمون واقعی API روی fixture ساخته‌شده فقط از API، و بررسی read-only دیتابیس. هیچ Migration/DDL، SQL نوشتنی، Fault Injection یا عملیات نوشتنی Git انجام نشد.

## ۱. مبنا

- HEAD: `279416c350930fc16edce5d602d39819b2ba4ffc` (`fix(transfer): validate inputs, sanitize errors, and distinguish missing balances`).
- `git status --short` در شروع: بدون خروجی.
- `git diff --check`: exit 0.

## ۲. فایل‌ها

| فایل | تغییر |
|---|---|
| `src/application/transferservice.cpp` | گذار PROCESSING و گارد وضعیت در مسیر موفق |
| `src/application/transferstatetransitions.h` | ثابت‌های SQL گذار، مشترک بین سرویس و آزمون متمرکز |
| `tests/transfer_status_transition_test.cpp` | آزمون متمرکز گذار با QSQLITE |
| `docs/opencode-reports/TRANSFER_STATE_TRANSITION_RESULT.md` | همین گزارش |
| `docs/PROJECT_CONTEXT.md` و `docs/PROJECT_HISTORY.md` | ثبت نتیجه |

بدون تغییر: Controller، قراردادها، `UnitOfWork`، Repositoryها، Schema/Migration، ترتیب قفل‌ها، کف مانده، fingerprint/replay، اصلاحات T4.

## ۳. تغییر رفتار

- پس از عبور از همهٔ اعتبارسنجی‌های کسب‌وکاری و پیش از نخستین نوشتن مالی، ردیف تراکنش با `WHERE id = :id AND status = 'INITIATED'` به `PROCESSING` می‌رود.
- اگر این گذار صفر ردیف یا خطای SQL بدهد، نتیجه `Failed / TRANSACTION_PROCESSING_FAILED` با پیام ثابت است و کل UnitOfWork رول‌بک می‌شود؛ هیچ Commit میانی اضافه نشد.
- گذار نهایی `COMPLETED` فقط با `WHERE id = :id AND status = 'PROCESSING'` انجام می‌شود؛ بنابراین COMPLETED بدون عبور از PROCESSING قابل ثبت نیست.
- مسیرهای رد کسب‌وکاری (که پیش از گذار PROCESSING هستند) همچنان `INITIATED → REJECTED` باقی ماندند.
- ترتیب قفل account → balance → hold، کف مانده، fingerprint/replay، پیام‌های امن T4 و رفتار Commit/Rollback دست‌نخورده ماندند.

## ۴. Build

```bash
cmake --build /home/it4850/Projects/build-CoreBanking-Desktop-Debug -j2
```

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/transferservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار.

## ۵. آزمون متمرکز گذار

```bash
g++ -std=c++17 -fPIC -I. tests/transfer_status_transition_test.cpp \
  $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
  -o /tmp/opencode/transfer_status_transition_test
/tmp/opencode/transfer_status_transition_test
```

خروجی واقعی:

```text
All transfer status transition tests passed
```

پوشش: گذار INITIATED → PROCESSING با گارد «دقیقاً یک ردیف»، رد تکرار همان گذار، گذار PROCESSING → COMPLETED همراه `result_code`/`completed_at`، رد COMPLETED مستقیم از INITIATED (اثبات نیاز به عبور از PROCESSING)، و رد بازگشت یک ردیف COMPLETED به PROCESSING. آزمون همان ثابت‌های SQL سرویس را اجرا می‌کند، اما خود سرویس PostgreSQL را اجرا نمی‌کند.

## ۶. دادهٔ آزمایشی (فقط از API محلی)

| موجودیت | شناسه | مقدار |
|---|---|---|
| Country | 1 | `364` |
| Province / City / Branch | 1 / 1 / 1 | `01` / `001` / `0000001` |
| Currency | 1 | `IRR`, minorUnit 2 |
| AccountType | 1 | `10` |
| Ledger ASSET (settlement) | 1 | `10100101` |
| Ledger LIABILITY | 2 | `20100101` |
| Customer | 1 / 2 | `TST-000001` / `TST-000002` |
| Product | 1 | `SAV-T5` |
| Mapping | 1 | product 1 → ledger 2 |
| Account A | 1 | `1000000000013` |
| Account B | 2 | `1000000000029` |
| Deposit A | tx 1 | `1000.00000000` |
| Deposit B | tx 2 | `100.00000000` |

## ۷. نتیجهٔ آزمون API

| # | سناریو | نتیجهٔ واقعی |
|---|---|---|
| A | مبلغ رشته‌ای `"10.00000000"` + description ۵۰۰ کاراکتری | `201 TRANSFER_COMPLETED`, tx `3`, `TRF-1794989910596-3C489564` |
| B | مبلغ عددی `5.12345678` | `201 TRANSFER_COMPLETED`, tx `4`, `TRF-1794989910606-5E4FDB56` |
| C | Replay همان کلید و payload A | `201` با همان tx `3` و همان شماره |
| D | همان کلید A با payload متفاوت | `422 IDEMPOTENCY_KEY_REUSED` |
| E | مبدأ موجود (1)، مقصد ناموجود (999999999) | `422 ACCOUNT_NOT_FOUND` با پیام `Both accounts must exist` |

سناریو E مسیر C7 مبتنی بر Queryهای قفل را اجرا کرد (پیام lookup زودهنگام مبدأ «Source account does not exist» نبود).

## ۸. بررسی دیتابیس (read-only با `corebanking_reader`)

مانده‌ها:

```text
account 1: ledger_balance = 984.87654322, available_balance = 984.87654322
account 2: ledger_balance = 115.12345678, available_balance = 115.12345678
```

تراکنش‌ها: tx 3 و 4 با وضعیت `COMPLETED` و `result_code = TRANSFER_COMPLETED`؛ tx 5 با وضعیت `REJECTED` و `ACCOUNT_NOT_FOUND`. فقط یک ردیف برای `t5-str-20261118` وجود دارد و replay ردیف یا اثر مالی تکراری نساخت.

Journalها: هر چهار Journal با وضعیت `POSTED`، هرکدام دقیقاً ۲ Entry، و debit = credit (۱۰ و ۵٫۱۲۳۴۵۶۷۸ برای انتقال‌ها). طول description تراکنش ۳ برابر ۵۰۰ ثبت شد.

`ledger_account_balance`: ASSET = `1100` و LIABILITY = `-1100.00000000`، سازگار با دو Deposit و دو Transfer.

## ۹. اجرانشده

- `ACCOUNT_BALANCE_NOT_INITIALIZED`: fixture مجاز وجود ندارد؛ حساب‌های ساخته‌شده با Account Opening همیشه ردیف مانده دارند و حذف ردیف ممنوع بود. اجرا نشد.
- مشاهدهٔ مستقیم و بیرونی وضعیت PROCESSING در PostgreSQL: چون گذارها در یک تراکنش واحد و بدون Commit میانی هستند، از اتصال دیگر دیده نمی‌شوند. گذر با آزمون متمرکز SQL و گارد `status='PROCESSING'` در UPDATE نهایی پوشش داده شد.
- Fault Injection، DDL/Migration و SQL نوشتنی اجرا نشدند.

## ۱۰. محدودیت‌ها

- این مرحله فقط گذار مسیر موفق را پوشش می‌دهد؛ State Machine کامل (C4)، همهٔ گذارهای مجاز/غیرمجاز، هم‌زمانی و Recovery بسته نشد.
- آزمون متمرکز، SQL گذار را روی QSQLITE اجرا می‌کند و جایگزین آزمون PostgreSQL سرویس کامل نیست؛ با این حال SQL سرویس و آزمون از یک ثابت مشترک می‌آیند.
- T1/T2 و سایر شکاف‌های ثبت‌شده تغییری نکردند.
