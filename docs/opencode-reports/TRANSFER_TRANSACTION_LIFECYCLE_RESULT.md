# نتیجه مرحله T2 — اصلاح محدود چرخهٔ تراکنش Transfer با UnitOfWork

نوع مرحله: اصلاح محدود مالکیت تراکنش در `TransferService` با استفاده از `UnitOfWork` موجود، همراه با Build واقعی و بازبینی ساختاری کد.
در این مرحله اجرای برنامه/restart، آزمون HTTP/API، SQL، دادهٔ مالی، Fault Injection، Migration و هیچ عملیات نوشتنی Git انجام نشد.

## ۱. SHA مبنا و وضعیت اولیهٔ Git

```text
git rev-parse HEAD
a7156dd9ae74649352de0c81609a2e099e804508

git rev-parse --verify 'a7156dd^{commit}'
a7156dd9ae74649352de0c81609a2e099e804508
```

- HEAD دقیقاً برابر commit حل‌شدهٔ `a7156dd` است؛ هیچ checkout/reset/تطبیق اجباری انجام نشد.
- `git status --short`: بدون خروجی (working tree پاک).
- `git --no-pager diff --stat`: بدون خروجی.
- `git --no-pager diff --cached --stat`: بدون خروجی.
- `git diff --check`: بدون خطا.
- `git diff --cached --check`: بدون خطا.
- شاخه: `main` (بدون تغییر).

### تغییرات قبلی

هیچ تغییر محلی staged یا unstaged پیش از این مرحله وجود نداشت، بنابراین تفکیک دامنه T2 و حفظ کار قبلی لازم نشد.

### بررسی محیط Build (فقط کلیدهای مرتبط)

از `CMakeCache.txt` موجود فقط موارد زیر خوانده شد:

```text
CMAKE_HOME_DIRECTORY:INTERNAL=/home/it4850/Projects/CoreBanking
CMAKE_GENERATOR:STRING=Unix Makefiles
CMAKE_BUILD_TYPE:STRING=Debug
CMAKE_CXX_COMPILER:STRING=/usr/bin/g++
Qt5_DIR:PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt5
```

Cache با پروژه و محیط فعلی سازگار است؛ هیچ reconfigure انجام نشد. `CMakeLists.txt.user.650e485` بازسازی نشد و لمس نشد.

## ۲. فایل‌های تغییرکرده در این مرحله

| فایل | نوع تغییر |
|---|---|
| `src/application/transferservice.cpp` | تنها فایل کد تغییرکرده |
| `docs/opencode-reports/TRANSFER_TRANSACTION_LIFECYCLE_RESULT.md` | گزارش جدید همین مرحله |
| `docs/PROJECT_CONTEXT.md` | به‌روزرسانی بخش‌های مرتبط با Transfer/T2 |
| `docs/PROJECT_HISTORY.md` | افزودن رکورد جدید 18.30 |

فایل‌های خارج از دامنهٔ مجاز تغییر نکردند. به‌طور خاص: `transferservice.h`، `transfercontroller.*`، `src/main.cpp`، `unitofwork.*`، قراردادهای مشترک، Router، Schema، Migration، CMakeLists و فایل‌های Qt Creator شخصی دست‌نخورده‌اند.

## ۳. خلاصهٔ تغییر رفتار

### ۳.۱ مالکیت تراکنش

- `db.transaction()`، همهٔ `db.commit()`ها و همهٔ `db.rollback()`های مستقیم حذف شدند.
- یک نمونهٔ `UnitOfWork` ساخته می‌شود که همان `QSqlDatabase` مشترک درخواست را نگه می‌دارد؛ Queryهای محلی Transfer هم روی همان `db` اجرا می‌شوند. اتصال دوم و `BEGIN` تودرتو ساخته نشد.
- `UnitOfWork` در `.cpp` از طریق `#include "../infrastructure/unitofwork.h"` استفاده می‌شود؛ چون تغییر constructor عمومی، header و wiring ممنوع بود. این یعنی لایهٔ Application از این فایل به پیاده‌سازی Infrastructure وابسته است. این وابستگی یک بدهی ساختاری ثبت‌شده است، نه استخراج Repository یا بازطراحی لایه‌ها.

### ۳.۲ Begin

- `unitOfWork.begin(&beginError)` جای `db.transaction()` را گرفت و outcome صریح بررسی می‌شود.
- هر outcome غیر از `BeginOutcome::Started` (شامل `Failed`، `AlreadyStarted` و `ConnectionUnavailable`) به `Status::Failed` با کد موجود `TRANSACTION_START_FAILED` و پیام ثابت و امن `Database transaction could not be started` می‌رسد؛ هیچ Query کسب‌وکار اجرا نمی‌شود.
- متن خطای دیتابیس در پاسخ یا log جدیدی از این مسیر نشت نمی‌کند.
- رفتار `DATABASE_UNAVAILABLE` پیش از شروع تراکنش (بررسی `isValid()/isOpen()`) بدون تغییر باقی ماند.
- فقط اگر `db.transaction()` روی اتصال باز شکست بخورد (`BeginOutcome::Failed`)، `UnitOfWork` خودش `m_closeConnectionOnDestruct` را ست می‌کند و بستن اتصال در destructor آن انجام می‌شود؛ `AlreadyStarted` و `ConnectionUnavailable` چنین علامتی نمی‌گذارند. در سرویس `db.close()` صدا زده نمی‌شود.

### ۳.۳ چهار مسیر Commit

همهٔ چهار مسیر فقط با `CommitOutcome::Committed` نتیجهٔ عادی خود را برمی‌گردانند:

1. **replay Completed** — `unitOfWork.commit` → در صورت `Committed` همان نتیجهٔ `Completed` قبلی؛ در غیر این صورت `Status::Failed`.
2. **replay Rejected** — `unitOfWork.commit` → در صورت `Committed` همان نتیجهٔ `Rejected` قبلی؛ در غیر این صورت `Status::Failed`.
3. **ثبت Rejection جدید (`rejectAndCommit`)** — پس از `persistRejection`، `unitOfWork.commit` → فقط `Committed` نتیجهٔ `Rejected` جدید را برمی‌گرداند.
4. **تکمیل Transfer جدید** — Commit نهایی فقط با `Committed` به `Completed` می‌رسد.

در `CommitOutcome::OutcomeUnknown`:

- `Status::Failed` با کد موجود `DATABASE_COMMIT_FAILED`.
- پیام امن و روشن: `The database commit outcome is unknown; the transfer may or may not have been recorded`.
- هیچ `rollback`، retry Commit یا تلاش برای تفسیر نتیجه انجام نمی‌شود.
- ادعا نمی‌شود عملیات ثبت نشده، برگشت خورده یا بی‌اثر بوده است.
- `transactionId` و `transactionNumber` موجود حفظ می‌شوند؛ حفظ آن‌ها اثبات persist شدن نیست.
- بستن اتصال نامطمئن به destructor خود `UnitOfWork` واگذار شده است (`State::CommitOutcomeUnknown`).

هر outcome غیرمنتظرهٔ دیگر Commit (در عمل `NotActive`) نیز `Status::Failed` با همان کد `DATABASE_COMMIT_FAILED` و پیام امن `Database commit failed` است؛ نه موفقیت و نه Rejected عادی.

### ۳.۴ Rollback

همهٔ مسیرهای Rollback Transfer به `unitOfWork.rollback` منتقل شدند و سیاست زیر روی همهٔ آن‌ها اعمال می‌شود، از جمله `rollbackFailure`، خروج زودهنگام idempotency conflict و خروج زودهنگام نبود حساب مبدأ:

- فقط `RollbackOutcome::RolledBack` اجازه می‌دهد نتیجهٔ اصلی (Failed یا Rejected) برگردد.
- اگر Rollback تأیید نشود (`Failed`، `NotActive`، `UnsafeState`): `Status::Failed` با کد `DATABASE_ROLLBACK_FAILED` و پیام ثابت و امن `Database transaction could not be safely rolled back`.
- `transactionId`/`transactionNumber` موجود در `TransferResult` حفظ می‌شوند.
- Rollback ناموفق دوباره اجرا نمی‌شود و به Rejected عادی تبدیل نمی‌شود.
- بسته‌شدن اتصال در destructor به وضعیت `UnitOfWork` بستگی دارد: در `Failed` و `UnsafeState` اتصال بسته می‌شود؛ `NotActive` به‌خودی‌خود اتصال را برای بستن علامت نمی‌زند.
- پیام ثابت شکست Rollback و کد خطای اصلی فقط در `qWarning` نگه داشته می‌شوند؛ این log ادعای قطعی دربارهٔ بسته‌شدن اتصال نمی‌کند و متن خام خطای دیتابیس، SQL، اطلاعات مشتری یا تنظیمات اتصال را اضافه نمی‌کند.

### ۳.۵ طول عمر Query و اتصال

- `UnitOfWork` پیش از همهٔ `QSqlQuery`های درخواست ساخته می‌شود و این قرارداد scope با کامنت کوتاه در کد روشن شده است.
- هیچ `db.close()` در سرویس فراخوانی نمی‌شود و هیچ helper یا scopeی با Query زنده اتصال را نمی‌بندد.
- Queryهای محلی در بلوک‌های تودرتو هستند و پیش از return تخریب می‌شوند؛ بنابراین در زمان اجرای destructor مربوط به `UnitOfWork` هیچ Query زنده‌ای از این درخواست باقی نیست.
- در مسیرهای replay، Commit داخل بلوکی انجام می‌شود که `QSqlQuery existing` هنوز در آن زنده است؛ اما این مسیر فقط Commit می‌کند و اتصال را نمی‌بندد؛ Query پیش از destructor `UnitOfWork` تخریب می‌شود.

## ۴. Build واقعی

دستور اجراشده (فقط Build موجود، بدون reconfigure). این Build پس از اصلاح نهاییِ log-only دوباره اجرا شد:

```bash
cmake --build /home/it4850/Projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی واقعی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/transferservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق. هشدار کامپایلری در خروجی مشاهده نشد. این Build فقط کامپایل را ثابت می‌کند و آزمون اجرایی Commit/Rollback نیست.

## ۵. جدول مسیرهای پایان تراکنش (بازبینی ساختاری کد، نه اجرا)

| مسیر | فایل/محل (`src/application/transferservice.cpp`) | Outcome بررسی‌شده | رفتار خروج |
|---|---|---|---|
| Begin ناموفق | `333-335` | هر outcome غیر از `BeginOutcome::Started` (شامل `Failed`، `AlreadyStarted`، `ConnectionUnavailable`) | `Failed` + `TRANSACTION_START_FAILED` + پیام ثابت؛ بدون Query کسب‌وکار. فقط `Failed` (شکست `db.transaction()` روی اتصال باز) اتصال را برای بستن در destructor علامت می‌زند |
| خطای Query با Rollback موفق | `rollbackFailure` در `349-358` و ۲۵ سایت فراخوانی | `RollbackOutcome::RolledBack` | `Failed` با کد/پیام اصلی همان مسیر؛ شناسه/شماره حفظ |
| Rollback ناموفق | `rollbackForError` در `337-347` | `Failed`/`NotActive`/`UnsafeState` | `Failed` + `DATABASE_ROLLBACK_FAILED` + پیام ثابت؛ بدون retry. در `Failed` و `UnsafeState` اتصال در destructor بسته می‌شود؛ `NotActive` اتصال را برای بستن علامت نمی‌زند. `qWarning` فقط پیام ثابت و کد اصلی را دارد و ادعای قطعی دربارهٔ بسته‌شدن اتصال نمی‌کند |
| idempotency conflict | `426-431` (`rollbackRejected`) | `RolledBack` یا ناموفق | Rollback موفق: `Rejected` + `IDEMPOTENCY_KEY_REUSED` با شناسه/شماره قبلی؛ ناموفق: `Failed` + `DATABASE_ROLLBACK_FAILED` |
| نبود حساب مبدأ | `487-491` (`rollbackRejected`) | `RolledBack` یا ناموفق | Rollback موفق: `Rejected` + `ACCOUNT_NOT_FOUND`؛ ناموفق: `Failed` + `DATABASE_ROLLBACK_FAILED` |
| Commit — replay Completed | `433-448` | `Committed` / `OutcomeUnknown` / `NotActive` | فقط `Committed` نتیجهٔ `Completed`؛ بقیه `Failed` |
| Commit — replay Rejected | `450-465` | همان | فقط `Committed` نتیجهٔ `Rejected`؛ بقیه `Failed` |
| Commit — ثبت Rejection جدید | `534-552` (`rejectAndCommit`) | همان | فقط `Committed` نتیجهٔ `Rejected` جدید؛ بقیه `Failed` |
| Commit — تکمیل Transfer جدید | `974-981` | همان | فقط `Committed` → `Completed`؛ بقیه `Failed` |
| `OutcomeUnknown` بدون Rollback بعدی | `commitFailure` در `371-383` | `CommitOutcome::OutcomeUnknown` | `Failed` + `DATABASE_COMMIT_FAILED` + پیام «نتیجه نامعلوم»؛ بدون rollback/retry Commit؛ بستن اتصال در destructor |
| outcome غیرمنتظرهٔ Commit | `commitFailure` در `371-383` | `NotActive` | `Failed` + `DATABASE_COMMIT_FAILED` + پیام ثابت |
| ترتیب تخریب Queryها پیش از بسته‌شدن اتصال | `UnitOfWork` در `329`؛ Queryها از `387` به بعد | قرارداد scope | UoW پیش از 모든 Query ساخته می‌شود؛ Queryها پیش از destructor UoW تخریب می‌شوند؛ هیچ `db.close()` در سرویس نیست |

این جدول حاصل خواندن کد است و آزمون اجرایی محسوب نمی‌شود.

## ۶. آزمون‌های واقعاً اجراشده

- فقط Build واقعی بخش ۴ اجرا شد.
- هیچ آزمون خودکاری برای چرخهٔ تراکنش Transfer اجرا نشد، چون در مخزن هیچ فایل تست Transfer وجود ندارد و `tests/` به هیچ target/CTest در `CMakeLists.txt` وصل نیست؛ اجرای آن‌ها نیازمند تغییر زیرساخت Build یا اتصال دیتابیس است که در دامنهٔ این مرحله مجاز نیست.
- آزمون expression-level T1 هیچ پوششی از چرخهٔ تراکنش ارائه نمی‌کند و در این مرحله دوباره اجرا یا به‌عنوان شاهد T2 معرفی نشد.
- بنابراین: رفتار خطاهای Begin/Commit/Rollback و مسیرهای `OutcomeUnknown`/`DATABASE_ROLLBACK_FAILED` در این مرحله **runtime-tested نیست** و فقط از روی کد بازبینی شده است.

## ۷. محدودیت‌ها و بررسی‌های اجرانشده

- T1 فقط اصلاح سیاست minimum بود؛ Query تولیدی Transfer هنوز آزمون یکپارچهٔ تأییدشده ندارد.
- اختلاف متن `SELECT` و ستون‌های خروجی در گزارش تاریخی T1 با حدس اصلاح نشد و شواهد آن به‌عنوان «مستقل تأییدشده» معرفی نمی‌شود.
- رفتار خطاهای Commit/Rollback در T2 بدون runtime test اثبات نشده است.
- چون Queryهای محلی همچنان مستقیم در سرویس هستند، این مرحله «تکمیل استخراج persistence» یا «تکمیل Transfer» نیست.
- بسته‌شدن اتصال مشترک نامطمئن ممكن است درخواست‌های بعدی را تا بازیابی اتصال با `DATABASE_UNAVAILABLE` مواجه کند؛ بازیابی اتصال خارج از دامنهٔ این مرحله است.
- یافته‌های باز C3/C4/C5/C7/C8 گزارش بازبینی Transfer (مبلغ عددی، State Machine، نشت پیام خطا در مسیرهای قدیمی، تفکیک حساب بدون مانده، طول description) در T2 تغییر نکردند.
- پیام‌های خطای قدیمی که از `lastError().text()` استفاده می‌کنند در این مرحله دست‌نخورده ماندند؛ فقط مسیرهای جدید T2 پیام امن ثابت دارند.
- هیچ ادعای ایمنی مالی اثبات‌شده، آمادگی انتشار یا رفع قطعی همهٔ ریسک‌ها مطرح نمی‌شود.
- اجرا نشد: اجرای برنامه/restart سرویس، HTTP/API test، SQL، ایجاد دادهٔ مالی، قطع اتصال/kill backend، Fault Injection، Migration یا اجرای `CURRENT_SCHEMA.sql`.

## ۸. وضعیت Git پس از مرحله

- هیچ `git add`/`commit`/`push`/تغییر branch اجرا نشد.
- تغییرات فقط در چهار فایل مجاز این مرحله هستند (بخش ۲).
