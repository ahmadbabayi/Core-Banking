# 1A F1/F2 Harness Report and Raw Output — Evidence (Permanent Copy)

> Sources:
> - `/tmp/core-banking-1a-f1-f2-harness-report.md` (temporary harness report)
> - `/tmp/core-banking-f1-f2-test/run.log` (raw run output)
> Type: independent harness evidence using the real `unitofwork.cpp` and a controlled fake driver, plus a real QSQLITE shared-close check.
> This permanent copy preserves the original content and raw output. It is not a new test run.
> The harness does not prove real QPSQL/PostgreSQL connection-loss behavior.
> No credentials or secrets are included.

---

# گزارش Harness هدفمند F1/F2 — اجرای واقعی UnitOfWork

> این گزارش اثبات کامل رفتار QPSQL در قطع ارتباط یا پذیرش نهایی ۱A نیست. Harness فقط منطق `UnitOfWork` واقعی و قرارداد فراخوانی Qt SQL را با یک درایور مصنوعی کنترل‌شده و یک آزمون واقعی QSQLITE برای shared-close می‌سنجد. ۱B شروع نشده است.

---

## ۱. محیط واقعی اجرا

- مسیر پروژه: `/home/ahmad/projects/CoreBanking`
- Qt: `5.15.8` (تشخیص با `qmake -v` و `pkg-config --modversion Qt5Core Qt5Sql`)
- کامپایلر: `g++ (Debian 12.2.0-14+deb12u1) 12.2.0`
- درایورهای SQL قابل‌مشاهده در زمان اجرا: `QSQLITE QPSQL QPSQL7`
- فایل‌های Harness فقط زیر `/tmp/core-banking-f1-f2-test/` ساخته شدند:
  - `/tmp/core-banking-f1-f2-test/main.cpp`
  - `/tmp/core-banking-f1-f2-test/test` (باینری)
  - `/tmp/core-banking-f1-f2-test/run.log` (خروجی اجرا)
- هیچ فایل مخزن، کد تولیدی، CMake، مستندات، Schema یا داده‌ای تغییر نکرد. به PostgreSQL/`corebanking` متصل نشد و credential خوانده نشد.
- Build اصلی پروژه تکرار نشد.

### دستور کامپایل واقعی

```bash
g++ -std=c++17 -fPIC /tmp/core-banking-f1-f2-test/main.cpp \
    /home/ahmad/projects/CoreBanking/src/infrastructure/unitofwork.cpp \
    $(pkg-config --cflags --libs Qt5Core Qt5Sql) \
    -I/home/ahmad/projects/CoreBanking/src/infrastructure \
    -o /tmp/core-banking-f1-f2-test/test
```

نتیجه: کامپایل بدون خطا. `unitofwork.cpp` و `unitofwork.h` واقعی پروژه مستقیماً کامپایل شدند؛ هیچ کپی یا شبیه‌سازی از UnitOfWork استفاده نشد.

### دستور اجرا

```bash
/tmp/core-banking-f1-f2-test/test
```

نتیجه کلی: `TOTAL FAILURES: 0` و `EXIT_CODE=0`.

---

## ۲. طراحی Harness

### ۲.۱ درایور مصنوعی

یک `QSqlDriver` سفارشی (`FakeDriver`) ساخته شد که:

- اتصال باز و معتبر دارد (`open`, `isOpen`, `close`).
- از Transactions پشتیبانی می‌کند (`hasFeature(Transactions) == true`).
- نتیجه `beginTransaction`، `commitTransaction` و `rollbackTransaction` با پرچم‌های مستقل قابل‌کنترل است.
- شمارنده فراخوانی Begin، Commit، Rollback و Close را در یک `struct Counters` ثبت می‌کند که **خارج از عمر درایور** در سناریو زنده است.
- شکست‌ها را با `QSqlError` مصنوعی و غیرحساس (`synthetic-begin`، `synthetic-commit`، `synthetic-rollback`) گزارش می‌کند؛ هیچ متن حساس یا SQL واقعی وجود ندارد.
- برای هر سناریو driver name و connection name یکتا استفاده می‌شود.
- cleanup خود Harness فقط وقتی `db.isOpen()` باشد `close()` را صدا می‌زند، و snapshot شمارنده‌ها **قبل از cleanup** گرفته می‌شود تا close مصنوعی Harness با close ناشی از UnitOfWork اشتباه نشود.

### ۲.۲ محدودیت مشاهده private

- هیچ `#define private public`، تغییر هدر یا دسترسی به وضعیت private انجام نشد.
- رفتار فقط با API عمومی (`begin`, `commit`, `rollback`, `isActive`, `isCommitOutcomeUnknown`) و شمارنده‌های درایور سنجیده شد.
- بنابراین وضعیت `RollbackFailed` به‌طور مستقیم قابل خواندن نیست و فقط از رفتار قابل مشاهده استنباط می‌شود: `isActive() == false`، رد شدن rollback دوم با پیام «Rollback already failed» و عدم افزایش شمارنده driver.
- این یک محدودیت مشاهده است، نه نقص آزمون.

---

## ۳. نتایج سناریوها

### سناریو A — Commit ناموفق روی اتصال باز: PASS

Assertionهای بررسی‌شده:

- `begin` موفق و `isActive() == true`.
- `commit` ناموفق.
- `isCommitOutcomeUnknown() == true`.
- `rollback` بعدی از API رد شد و **به درایور نرسید**.
- پیش از خروج از scope، اتصال باز بود.
- هنگام خروج، Close رخ داد و Rollback همچنان صفر بود.

شمارنده‌های واقعی:

```text
COUNTERS A: begin=1 commit=1 rollback=0 close=1 openBefore=true openAfter=false
```

### سناریو B — Rollback صریح ناموفق روی اتصال باز: PASS

Assertionهای بررسی‌شده:

- اولین `rollback` به درایور رسید (`rollback=1`) و شکست خورد.
- فراخوانی دوم `rollback` توسط API رد شد و شمارنده افزایش نیافت (`rollback=1` ماند).
- پیش از خروج از scope، اتصال باز بود.
- هنگام خروج، اتصال بسته شد.
- Destructor دوباره rollback نکرد (`rollback` نهایی ۱).

شمارنده‌های واقعی:

```text
COUNTERS B: begin=1 commit=0 rollback=1 close=1 openBefore=true openAfter=false
```

### سناریو C — شکست Rollback در Destructor: PASS

Assertionهای بررسی‌شده:

- `begin` موفق؛ بدون پایان صریح از scope خارج شد.
- دقیقاً یک rollback ناموفق رخ داد (`rollback=1`).
- سپس Close رخ داد (`close=1`).
- `qWarning` مورد انتظار چاپ شد: `UnitOfWork: rollback failed during destruction; closing shared database connection`.

شمارنده‌های واقعی:

```text
COUNTERS C: begin=1 commit=0 rollback=1 close=1 openBefore=true openAfter=false
```

### سناریو D — BEGIN ناموفق روی اتصال باز: PASS

Assertionهای بررسی‌شده:

- `begin` ناموفق و `isActive() == false`.
- هیچ commit یا rollback رخ نداد (`commit=0`, `rollback=0`).
- پیش از خروج، اتصال باز بود.
- هنگام خروج، Close رخ داد (`close=1`).

شمارنده‌های واقعی:

```text
COUNTERS D: begin=1 commit=0 rollback=0 close=1 openBefore=true openAfter=false
```

### سناریو E1 — Commit موفق: PASS

- `begin` و `commit` موفق؛ `isActive() == false`.
- اتصال پس از scope باز ماند.
- هیچ rollback یا close اضافه‌ای رخ نداد.

```text
COUNTERS E1: begin=1 commit=1 rollback=0 close=0 openBefore=true openAfter=true
```

### سناریو E2 — Rollback صریح موفق: PASS

- `begin` و `rollback` موفق؛ `isActive() == false`.
- اتصال باز ماند.
- هیچ close یا commit اضافه‌ای رخ نداد.

```text
COUNTERS E2: begin=1 commit=0 rollback=1 close=0 openBefore=true openAfter=true
```

### سناریو E3 — Rollback خودکار موفق در Destructor: PASS

- `begin` موفق؛ بدون پایان صریح از scope خارج شد.
- Destructor یک rollback موفق انجام داد.
- اتصال باز ماند و close اضافه رخ نداد.

```text
COUNTERS E3: begin=1 commit=0 rollback=1 close=0 openBefore=true openAfter=true
```

### سناریو F — آزمون واقعی shared-close با QSQLITE: PASS

- `QSQLITE` در زمان اجرا موجود بود؛ آزمون واقعی اجرا شد.
- یک اتصال `:memory:` با دو کپی `QSqlDatabase` ساخته شد.
- Query روی کپی دوم پیش از close موفق بود.
- `close()` روی کپی اول باعث شد `second.isOpen() == false` شود.
- Query روی کپی دوم پس از close شکست خورد.

نتیجه: PASS برای هر چهار assertion.

نکته: در خروجی، پیام stderr مربوط به `QSqlQuery::exec: database not open` با خط stdout تداخل نمایشی داشت (`secondQSqlQuery::exec...`). این فقط ترتیب بافر stdout/stderr است و assertion مربوطه PASS شد.

شمارنده‌های درایور مصنوعی در این سناریو استفاده نشدند؛ این آزمون رفتار واقعی plugin QSQLITE در Qt را می‌سنجد، نه FakeDriver.

---

## ۴. جمع‌بندی نتایج

| سناریو | نتیجه | begin | commit | rollback | close | openBefore | openAfter |
|---|---|---|---|---|---|---|---|
| A: commit failure | PASS | 1 | 1 | 0 | 1 | true | false |
| B: explicit rollback failure | PASS | 1 | 0 | 1 | 1 | true | false |
| C: destructor rollback failure | PASS | 1 | 0 | 1 | 1 | true | false |
| D: begin failure | PASS | 1 | 0 | 0 | 1 | true | false |
| E1: commit success | PASS | 1 | 1 | 0 | 0 | true | true |
| E2: explicit rollback success | PASS | 1 | 0 | 1 | 0 | true | true |
| E3: destructor rollback success | PASS | 1 | 0 | 1 | 0 | true | true |
| F: QSQLITE shared close | PASS | — | — | — | — | true | false (کپی دوم) |

---

## ۵. کدام معیارهای F1/F2 با اجرای واقعی تأیید شدند

### F1 — Commit نامعلوم و جلوگیری از استفاده مجدد

تأیید شد (در سطح UnitOfWork + FakeDriver):

- شکست commit به `CommitOutcomeUnknown` منجر می‌شود.
- rollback بعدی از API رد می‌شود و به درایور نمی‌رسد.
- اتصال پس از scope بسته می‌شود (`close=1`).
- هیچ rollback خودکاری پس از commit ناموفق رخ نمی‌دهد (`rollback=0`).

### F2 — ثبت صادقانه rollback و عدم retry خودکار

تأیید شد (در سطح UnitOfWork + FakeDriver):

- شکست rollback صریح باعث `RolledBack` نمی‌شود؛ rollback دوم رد می‌شود و شمارنده افزایش نمی‌یابد.
- شکست rollback در Destructor فقط یک بار تلاش می‌کند و سپس اتصال را می‌بندد.
- rollback موفق (صریح یا Destructor) به `RolledBack` می‌رسد و اتصال را نمی‌بندد.

### shared-close واقعی

تأیید شد (در سطح Qt/QSQLITE): close روی یک کپی، کپی دیگر همان connection name را هم غیرقابل‌استفاده می‌کند و Query روی آن شکست می‌خورد.

---

## ۶. چه چیزهایی هنوز اثبات نشده‌اند

- **رفتار QPSQL/PostgreSQL در قطع ارتباط واقعی:** Harness با FakeDriver کار می‌کند. اینکه COMMIT یا ROLLBACK در PostgreSQL در شرایط قطع شبکه/خاتمه backend دقیقاً چه نتیجه‌ای می‌دهد و Qt/QPSQL چگونه آن را گزارش می‌کند، اثبات نشده است.
- **Retry پس از Commit نامعلوم روی دیتابیس واقعی:** اینکه پس از restart/recovery، Retry با همان idempotency key نتیجه پایدار را از دیتابیس بازیابی می‌کند، در این Harness آزموده نشد (نیازمند اتصال واقعی و سناریوی commit در-ابهام است).
- **اتصال مشترک همه سرویس‌ها در سناریوی واقعی:** Harness فقط UnitOfWork و یک اتصال را می‌سنجد. رفتار هم‌زمانی/ترتیبی همه سرویس‌های legacy روی اتصال مشترک اثبات نشده است.
- **HTTP/JSON و قرارداد API:** هیچ درخواست HTTP اجرا نشد. نگاشت `Failed` به HTTP 500 فقط از کدخوانی `DepositController` می‌آید، نه آزمون HTTP.
- **مسیر Deposit کامل:** Harness مستقیماً UnitOfWork را صدا می‌زند، نه `DepositService::deposit`. رفتار انتها‌به‌انتها Deposit با FakeDriver آزموده نشد.
- **ریسک مسیرهای legacy:** Transfer/Hold/AccountOpening/AccountService و Repositoryها خارج از این Harness و خارج از دامنه اصلاح‌اند.
- **private state:** `RollbackFailed` به‌طور مستقیم خوانده نشد؛ استنباط از رفتار عمومی و شمارنده‌هاست.

---

## ۷. آیا نقص تازه‌ای دیده شد؟

- در UnitOfWork هیچ نقص تازه‌ای در سناریوهای اجراشده دیده نشد؛ همه ۵۰+ assertion PASS شدند و `TOTAL FAILURES: 0` بود.
- تنها خروجی اضافه، `qWarning` مورد انتظار در سناریو C است.
- هیچ نقص مرتبط با commit/rollback/close در دامنه Harness مشاهده نشد.
- همچنان محدودیت مشاهده private و محدودیت FakeDriver در برابر QPSQL واقعی وجود دارد.

---

## ۸. تأیید دست‌نخوردن فایل‌های مخزن

- Harness فقط در `/tmp/core-banking-f1-f2-test/` ساخته، کامپایل و اجرا شد.
- هیچ فایل مخزن تغییر نکرد؛ `git status` دقیقاً همان مجموعه قبلی modified/untracked را نشان می‌دهد و فایل جدیدی در مخزن اضافه نشد.
- Build اصلی پروژه تکرار نشد؛ فقط `unitofwork.cpp` واقعی در Harness کامپایل شد.
- Git index/Commit/Branch/Push انجام نشد.

---

## ۹. محدودیت و وضعیت نهایی

- Harness موفق است، اما معادل اثبات کامل رفتار شکست PostgreSQL یا پذیرش نهایی ۱A نیست.
- F1/F2 در سطح منطق UnitOfWork و قرارداد Qt SQL با شواهد اجرایی تأیید شدند.
- برای اثبات رفتار QPSQL در قطع ارتباط و Retry پس از Commit نامعلوم، یک آزمون کنترل‌شده واقعی با تأیید جداگانه لازم است.
- ۱A نهایی اعلام نمی‌شود و ۱B شروع نشد. پس از این گزارش متوقف می‌شوم.


---

## خروجی خام اجرای Harness (run.log)

```text
UnitOfWork: rollback failed during destruction; closing shared database connection
Qt version: 5.15.8
Available SQL drivers: QSQLITE QPSQL QPSQL7

SCENARIO A: commit failure on open connection
PASS: A: connection opens
PASS: A: fake driver is available
PASS: A: begin succeeds
PASS: A: unit of work is active after begin
PASS: A: commit fails
PASS: A: public state reports CommitOutcomeUnknown
PASS: A: rollback after unknown commit is rejected
PASS: A: rejected rollback did not reach the driver
PASS: A: shared connection is still open before scope exit
PASS: A: shared connection is closed after scope exit
PASS: A: driver begin count is 1
PASS: A: driver commit count is 1
PASS: A: driver rollback count remains 0 after scope exit
PASS: A: driver close count is 1
COUNTERS A: begin=1 commit=1 rollback=0 close=1 openBefore=true openAfter=false

SCENARIO B: explicit rollback failure on open connection
PASS: B: connection opens
PASS: B: fake driver is available
PASS: B: begin succeeds
PASS: B: first rollback fails
PASS: B: first rollback reached the driver
PASS: B: second rollback is rejected by the API
PASS: B: second rollback did not reach the driver
PASS: B: shared connection is still open before scope exit
PASS: B: shared connection is closed after scope exit
PASS: B: driver begin count is 1
PASS: B: driver commit count is 0
PASS: B: destructor did not retry the failed rollback
PASS: B: driver close count is 1
COUNTERS B: begin=1 commit=0 rollback=1 close=1 openBefore=true openAfter=false

SCENARIO C: rollback failure in destructor
PASS: C: connection opens
PASS: C: fake driver is available
PASS: C: begin succeeds
PASS: C: unit of work is active before scope exit
PASS: C: shared connection is closed after scope exit
PASS: C: driver begin count is 1
PASS: C: driver commit count is 0
PASS: C: exactly one rollback was attempted in the destructor
PASS: C: driver close count is 1
COUNTERS C: begin=1 commit=0 rollback=1 close=1 openBefore=true openAfter=false

SCENARIO D: begin failure on open connection
PASS: D: connection opens
PASS: D: fake driver is available
PASS: D: begin fails
PASS: D: unit of work is not active after failed begin
PASS: D: begin reached the driver once
PASS: D: no commit reached the driver
PASS: D: no rollback reached the driver
PASS: D: shared connection is still open before scope exit
PASS: D: shared connection is closed after scope exit
PASS: D: driver begin count is 1
PASS: D: driver commit count is 0
PASS: D: driver rollback count is 0
PASS: D: driver close count is 1
COUNTERS D: begin=1 commit=0 rollback=0 close=1 openBefore=true openAfter=false

SCENARIO E1: successful commit
PASS: E1: connection opens
PASS: E1: begin succeeds
PASS: E1: commit succeeds
PASS: E1: unit of work is not active after commit
PASS: E1: shared connection remains open after successful commit
PASS: E1: driver begin count is 1
PASS: E1: driver commit count is 1
PASS: E1: driver rollback count is 0
PASS: E1: destructor did not close after successful commit
COUNTERS E1: begin=1 commit=1 rollback=0 close=0 openBefore=true openAfter=true

SCENARIO E2: successful explicit rollback
PASS: E2: connection opens
PASS: E2: begin succeeds
PASS: E2: rollback succeeds
PASS: E2: unit of work is not active after rollback
PASS: E2: shared connection remains open after successful rollback
PASS: E2: driver begin count is 1
PASS: E2: driver commit count is 0
PASS: E2: driver rollback count is 1
PASS: E2: destructor did not close after successful rollback
COUNTERS E2: begin=1 commit=0 rollback=1 close=0 openBefore=true openAfter=true

SCENARIO E3: successful automatic rollback in destructor
PASS: E3: connection opens
PASS: E3: begin succeeds
PASS: E3: unit of work is active before scope exit
PASS: E3: shared connection remains open after destructor rollback
PASS: E3: driver begin count is 1
PASS: E3: driver commit count is 0
PASS: E3: driver rollback count is 1
PASS: E3: destructor did not close after successful rollback
COUNTERS E3: begin=1 commit=0 rollback=1 close=0 openBefore=true openAfter=true

SCENARIO F: QSQLITE shared close behavior
PASS: F: QSQLITE connection opens
PASS: F: second copy reports open
PASS: F: query on secondQSqlQuery::exec: database not open
 copy succeeds before close
PASS: F: first copy reports closed
PASS: F: second copy reports closed after close on first copy
PASS: F: query on second copy fails after close on first copy

TOTAL FAILURES: 0
```
