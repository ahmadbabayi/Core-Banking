# گزارش طراحی مرحله بعدی Balance در مسیر Deposit

تاریخ بررسی: 2026-11-16
نوع مرحله: بررسی کد، طراحی و مستندسازی — بدون Build، بدون HTTP، بدون SQL، بدون تغییر داده یا Schema.

## ۱. SHA و محدوده بررسی

- شاخه: `main`
- HEAD محلی: `fd23100c06f5862234f14019ac4b4eac26b0ff86`
- `origin/main`: `fd23100c06f5862234f14019ac4b4eac26b0ff86`
- working tree: پاک (بدون تغییر stage/commit)
- بنابراین مبنای بررسی قبلی GitHub (`fd23100`) با HEAD فعلی و upstream معادل است؛ اختلاف مؤثری یافت نشد.

منابع خوانده‌شده:

- `AGENTS.md`
- `docs/PROJECT_CONTEXT.md`
- `docs/PROJECT_HISTORY.md`
- `database/CURRENT_SCHEMA.sql`
- `database/schema/007_balances.sql`، `014_product_balance_rules.sql`، `015_account_holds.sql`، `016_allow_overdraft_balances.sql`
- `docs/opencode-reports/DEPOSIT_PERSISTENCE_EXTRACTION_FINAL.md`
- گزارش‌های مرتبط 1A، F1/F2 و استخراج 1B.1/1C.1/1C.2 از طریق Context و History
- کد جاری: `src/application/depositservice.*`، قراردادهای `src/application/contracts/*`، `src/infrastructure/repository/{balancerepository,accountstaterepository,holdrepository,journalrepository,transactionrepository}.{h,cpp}`، `src/application/{transactionservice,holdservice,transferservice,accountopeningservice}.cpp`، `src/web/httprouter.cpp`

محدوده: فقط Balance در مسیر Deposit. Transfer، HoldService و مسیرهای legacy فقط برای شناسایی نویسندگان و وابستگی‌ها خوانده شدند و تغییر نمی‌کنند. طراحی Balance Slice یا 1C در این مرحله انجام یا تأیید نمی‌شود.

## ۲. رفتار فعلی با ارجاع کد

### ۲.۱ خواندن مانده و ورودی‌های محاسبه

- `BalanceRepository::loadLockedAccountSnapshot` در `src/infrastructure/repository/balancerepository.cpp:56-147` تنها منبع خواندن Deposit است.
- این Query این مقادیر را برمی‌گرداند:
  - `a.id`, `a.status`, `a.currency_id`, `a.product_id`, `a.ledger_account_id` (`:62-64`, `:133-137`)
  - `active_holds` = `SUM(hs.currently_blocked_amount)` از VIEW `account_hold_status` (`:64-65`, `:138`)
  - `effective_overdraft_limit` از `product_balance_rule` و `account_balance_rule_override` (`:66-97`, `:139`)
  - وضعیت مؤثر محصول با `CURRENT_DATE` و `p.effective_from/effective_to` (`:98-101`, `:140`)
  - وضعیت/قابلیت ثبت/ارز/دسته سرفصل مشتری (`:102`, `:141-144`)
- این Snapshot **خودِ `b.ledger_balance` و `b.available_balance` را نمی‌خواند**. تنها از `account_balance` برای قفل/Join استفاده می‌کند (`:104`, `:108`).
- قفل Snapshot با `FOR UPDATE OF a, b` انجام می‌شود (`:108`)؛ ردیف‌های Hold قبلاً جداگانه قفل شده‌اند (`DepositService`، `src/application/depositservice.cpp:358-369`).

### ۲.۲ محاسبه و نوشتن

- نوشتن نهایی در `BalanceRepository::applyDepositIncrement`، `src/infrastructure/repository/balancerepository.cpp:149-191`:

```sql
UPDATE account_balance SET
 ledger_balance = ledger_balance + CAST(:ledger_amount AS NUMERIC),
 available_balance = ledger_balance + CAST(:available_amount AS NUMERIC)
   - CAST(:holds AS NUMERIC) + CAST(:overdraft_limit AS NUMERIC),
 updated_at = CURRENT_TIMESTAMP
WHERE account_id = :account_id
```

- در PostgreSQL، سمت راست UPDATE از مقدار **قبل از به‌روزرسانی** استفاده می‌کند؛ بنابراین `available_balance` برابر است با:
  `new_ledger_balance - active_holds + effective_overdraft_limit`.
- `ledger_balance` فقط افزایش می‌یابد. فرمول `minimum_balance` در Deposit استفاده نمی‌شود و Deposit هیچ بررسی کفایت/حداقل انجام نمی‌دهد (واریز افزایشی است).
- `DepositService` ترتیب تصمیم‌ها را حفظ می‌کند (`src/application/depositservice.cpp`):
  - `:262-286` currency lookup
  - `:331-343` Account row lock
  - `:345-356` Balance row lock
  - `:358-369` Hold row lock
  - `:371-382` Snapshot
  - `:384-410` checks: balance initialized، account ACTIVE، product ACTIVE/effective، ledger postable/LIABILITY/currency
  - `:412-430` settlement ledger lookup
  - `:432-470` ledger init/lock
  - `:472-553` transaction entry و Journal/توازن
  - `:555-571` Balance update
  - `:573-598` Ledger update
  - `:600-620` Journal post
  - `:622-639` Transaction complete و Commit

### ۲.۳ معنای سه مقدار

| مقدار | معنا و منبع | نوشتن | خواندن در تصمیم Deposit |
|---|---|---|---|
| `ledger_balance` | مانده ثبت‌شده سپرده مشتری؛ ستون `account_balance.ledger_balance` با قید `ck_account_balance_ledger` (`database/CURRENT_SCHEMA.sql:589-595`) | فقط افزایش در Deposit (`balancerepository.cpp:159-164`)، افزایش/کاهش در Transfer (`transferservice.cpp:243-276`)، مقدار صفر در افتتاح (`accountopeningservice.cpp:278-289`) | خوانده نمی‌شود؛ فقط در همان UPDATE برای محاسبه `available_balance` استفاده می‌شود |
| `active_holds` | مجموع `currently_blocked_amount` از VIEW `account_hold_status` (`database/CURRENT_SCHEMA.sql:724-779`) | HoldService ردیف‌های Hold/Event را می‌نویسد؛ هیچ‌کدام `ledger_balance` را تغییر نمی‌دهند | فقط برای Cache کردن `available_balance` |
| `available_balance` | Cache ذخیره‌شده در `account_balance` | Deposit (`balancerepository.cpp:159-164`)، Transfer (`transferservice.cpp:243-276`)؛ افتتاح مقدار صفر | در Deposit هیچ تصمیم مالی بر آن بنا نمی‌شود |

نکته: `available_balance` در Deposit فقط نوشته می‌شود و به‌عنوان ورودی تصمیم خوانده نمی‌شود. بنابراین «کهنه‌بودن» آن در این مسیر باعث عبور/رد مالی نمی‌شود، اما مقدار ذخیره‌شده پس از هر تغییر Hold تا واریز بعدی به‌روزرسانی نمی‌شود و برای مصرف‌کننده بیرونی قابل اتکا نیست.

### ۲.۴ قواعد محصول، حداقل مانده، وضعیت و Override

- `product_balance_rule` (`database/CURRENT_SCHEMA.sql:1382-1395`) و `account_balance_rule_override` (`:602-620`) در Schema موجودند.
- در Deposit فقط **overdraft** خوانده می‌شود؛ `minimum_balance` خوانده نمی‌شود (`balancerepository.cpp:66-97`).
- در Transfer هم `minimum_balance` و هم overdraft خوانده می‌شوند (`transferservice.cpp:122-166`).
- وضعیت حساب/محصول/سرفصل در Deposit بررسی می‌شود (`depositservice.cpp:384-410`)، اما قواعد حداقل مانده و Override حداقل در Deposit اثری ندارند.
- مقدار مؤثر overdraft در SQL فعلی:
  - فعال‌بودن: `product.overdraft_enabled AND COALESCE(override.overdraft_enabled, TRUE)`
  - سقف: `LEAST(product.overdraft_limit, COALESCE(override.overdraft_limit, product.overdraft_limit))`
  - بنابراین Override فقط می‌تواند محدودکننده/مساوی باشد، نه گشایش‌دهنده.
- مقدار مؤثر minimum در Transfer:
  - `COALESCE(override.minimum_balance, product.minimum_balance, 0)`
  - اینجا **LEAST وجود ندارد**؛ Override می‌تواند حداقل را کمتر از محصول کند. این یک ابهام/ناسازگاری قواعد است، نه یک قاعده مصوب روشن.

### ۲.۵ زمان مرجع

- همه Queryهای Deposit از `CURRENT_TIMESTAMP` و `CURRENT_DATE` بدون پارامتر زمان صریح استفاده می‌کنند:
  - قواعد مؤثر: `CURRENT_TIMESTAMP` (`balancerepository.cpp:69-96`)
  - وضعیت محصول: `CURRENT_DATE` (`:99-100`)
  - Hold فعال/منقضی: `CURRENT_TIMESTAMP` در VIEW (`CURRENT_SCHEMA.sql:769-777`)
  - `posting_date` و `value_date` ژورنال: `CURRENT_DATE` (`journalrepository.cpp:150-153`)
  - `updated_at`/`completed_at`: `CURRENT_TIMESTAMP` (`balancerepository.cpp:163`, `transactionrepository.cpp:579-581`)
- در PostgreSQL، `CURRENT_TIMESTAMP` و `CURRENT_DATE` زمان **شروع تراکنش** را برمی‌گردانند و در طول همان تراکنش ثابت‌اند. پس همه خواندن‌ها/نوشتن‌های همین UoW یک زمان مرجع مشترک دارند.
- این «ثبات» تا زمانی است که یک تراکنش واحد باشد؛ پس از Commit یا در درخواست بعدی، زمان جدید اعمال می‌شود.
- هیچ تصمیم مستندی درباره اینکه «زمان مرجع باید آغاز درخواست، آغاز تراکنش، یا زمان قفل باشد» وجود ندارد؛ رفتار فعلی = زمان شروع تراکنش.

### ۲.۶ ترتیب قفل‌ها و نویسندگان دیگر

- Deposit: Account → Balance → Hold → Snapshot (`depositservice.cpp:331-382`).
- Transfer: Accountها به ترتیب صعودی id → Balanceها به ترتیب id → Holdها به ترتیب `(account_id, id)` (`transferservice.cpp:491-555`)؛ هم‌راستا با Deposit.
- HoldService: Account → Balance → Hold (`holdservice.cpp:914-965` و `:1400-...`).
- `loadLockedAccountSnapshot` هم `FOR UPDATE OF a, b` دارد (`balancerepository.cpp:108`).
- نویسندگان مؤثر بر مانده/Hold/قاعده:

| نویسنده | اثر | مسیر | وضعیت اتصال |
|---|---|---|---|
| `BalanceRepository` (Deposit) | افزایش `ledger_balance` و بازمحاسبه Cache `available_balance` | `depositservice.cpp:555-571` | فعال |
| `TransferService` | کاهش/افزایش `ledger_balance` هر دو حساب و بازمحاسبه Cache | `transferservice.cpp:243-276`, `:816-830` | فعال |
| `AccountOpeningService` | درج `account_balance` با صفر | `accountopeningservice.cpp:278-289` | فعال |
| `HoldService` | ایجاد/آزادسازی Hold و Event؛ تغییر غیرمستقیم `active_holds` و در نتیجه Cache بعدی | `holdservice.cpp` | فعال |
| `AccountBalanceRepository` (legacy) | `save`/`debit`/`credit` مستقیم روی `account_balance` | `src/infrastructure/repository/accountbalancerepository.cpp` | در CMake کامپایل می‌شود اما هیچ مصرف‌کننده‌ای در `src` ندارد؛ مسیر فعال نیست |
| نویسنده مستقیم SQL/دستی | هر تغییر خارج از API | خارج از برنامه | خارج از کنترل برنامه؛ فرض نمی‌شود |

نکته: `AccountBalanceRepository::credit` مقدار `available_balance` را جدا و بدون محاسبه Hold/Overdraft تغییر می‌دهد (`accountbalancerepository.cpp:118-130`). چون مصرف‌کننده ندارد، اثر فعلی ندارد؛ اما اگر در آینده وصل شود، نویسنده ناسازگار با فرمول خواهد بود.

## ۳. تفکیک «پیاده‌سازی موجود»، «قاعده مصوب»، «شواهد آزمون» و «ابهام»

| موضوع | پیاده‌سازی موجود | قاعده مصوب در Context | شواهد آزمون | ابهام |
|---|---|---|---|---|
| افزایش `ledger_balance` در واریز | بله (`applyDepositIncrement`) | بله | شواهد Deposit API و SQL read-only در History 18.17؛ پس از 1B.1 آزمون هدفمند QSQLITE؛ PostgreSQL پس از استخراج دوباره آزموده نشده | — |
| بازمحاسبه `available_balance` در واریز | بله، به‌صورت Cache | فرمول مفهومی مصوب؛ Cache بودن نیز مصوب | همان شواهد محدود؛ آزمون برابری با فرمول پس از هر تغییر Hold وجود ندارد | معنای دقیق در مرز حداقل مانده/Override روشن نیست |
| `minimum_balance` در Deposit | خیر | قاعده کلی موجود؛ برای واریز اثر مستقیم ندارد | — | انتظار از Deposit (اگر وجود دارد) تعریف نشده |
| Overdraft مؤثر | بله در SQL (فقط محدودکننده) | «Override نباید ممنوعیت پایه را دور بزند» | آزمون مستقیم Override وجود ندارد | هیچ تصمیم کسب‌وکاری درباره کف/سقف دقیق و اولویت Override ثبت نشده |
| Override حداقل | فقط در Transfer؛ بدون LEAST | حداقل و اضافه‌برداشت مستقل‌اند؛ Override نباید ممنوعیت پایه را دور بزند | آزمون مستقیم وجود ندارد | آیا Override حداقل می‌تواند گشایش‌دهنده باشد؟ پاسخ روشن نیست |
| زمان مرجع | `CURRENT_TIMESTAMP`/`CURRENT_DATE` = شروع تراکنش | قاعده‌ای صریح ثبت نشده | رفتار PostgreSQL در Context/History به‌عنوان تصمیم ثبت نشده | زمان مرجع مطلوب کسب‌وکار تعیین نشده |
| قفل قواعد مؤثر | هیچ قفل روی ردیف‌های `product_balance_rule`/`override` وجود ندارد | شکاف هم‌زمانی به‌عنوان ریسک باز ثبت شده | آزمون هم‌زمانی واقعی انجام نشده | سیاست حل (قفل/advisory/serializable/عدم نویسنده) تعیین نشده |
| Hold و `available_balance` | Hold مقدار Cache را تغییر نمی‌دهد | مصوب: Hold نباید `ledger_balance` را تغییر دهد | API و SQL read-only در History 18.14/18.15 | آیا Cache باید پس از Hold بازمحاسبه شود یا صرفاً Cache باشد؟ تصمیم نشده |
| API خواندن مانده | وجود ندارد | — | — | نیاز و قرارداد تعیین نشده |

## ۴. وضعیت Override به‌طور مشخص

### ساختار Schema

- جدول `account_balance_rule_override` موجود است (`CURRENT_SCHEMA.sql:602-620`) با:
  - `minimum_balance`، `overdraft_enabled`، `overdraft_limit` (هر کدام nullable)
  - `effective_from`، `effective_to` (TIMESTAMPTZ)
  - `reason_code` اجباری و `reference_number`
- Constraintها: `ck_account_balance_override_has_setting`، `ck_account_balance_override_overdraft_pair`، `ck_account_balance_override_disabled`، `ck_account_balance_override_effective_dates`
- دو Exclusion Constraint جدا برای بازه‌های minimum و overdraft (`CURRENT_SCHEMA.sql:1752-1760`)

### مسیر خواندن و نوشتن

- خواندن: فقط به‌صورت Subquery داخل SQL در `BalanceRepository` و `TransferService`؛ هیچ قرارداد/Repository اختصاصی یا API خواندن ندارد.
- نوشتن: هیچ مسیر برنامه‌ای (Service/Controller/Repository) برای درج/ویرایش Override وجود ندارد. Router هم endpoint مدیریت قواعد/Override ندارد (`src/web/httprouter.cpp`).
- بنابراین Override در کد «خوانده می‌شود» ولی «مدیریت نمی‌شود».

### تقدم نسبت به قواعد محصول

- برای overdraft: محصول پایه است و Override فقط می‌تواند سخت‌گیرانه‌تر کند (`product AND override`, `LEAST`). این با قاعده مصوب «Override نباید ممنوعیت پایه را دور بزند» سازگار است.
- برای minimum: Override جایگزین می‌شود (`COALESCE`)، بدون سقف/کف؛ می‌تواند گشایش‌دهنده باشد. این با قاعده مصوب در تضاد نیست اما **تصمیم کسب‌وکاری صریح ندارد**.
- نتیجه: تقدم فعلی برای overdraft قابل دفاع است؛ برای minimum نیازمند تصمیم است.

### حل شکاف در محدوده فعلی

- بدون تصمیم کسب‌وکاری درباره:
  1. آیا Override حداقل می‌تواند گشایش‌دهنده باشد؟
  2. آیا Override باید فقط سخت‌گیرانه‌تر از محصول باشد (هم برای minimum و هم overdraft)؟
  3. سیاست هم‌زمانی درج قاعده جدید هنگام خواندن (قفل/advisory/تصمیم «هیچ نویسنده‌ای در دامنه فعلی وجود ندارد»)؟
  
  حل کامل ممکن نیست.
- هیچ Migration جدیدی برای Override در این مرحله لازم تشخیص داده نشد؛ ساختار پایه موجود است. اما اگر سیاست قفل سمت دیتابیس انتخاب شود، ممکن است Migration/Trigger یا حداقل تغییر SQL لازم شود.

## ۵. شکاف‌های Balance و وابستگی‌های خارج از دامنه

1. **نبود نویسنده/API برای قواعد مؤثر:** Override و `product_balance_rule` فقط خواندنی‌اند؛ بنابراین نمی‌توان سناریوهای واقعی Override را از مسیر API ساخت. آزمون آن نیازمند داده آزمایشی یا تصمیم درباره یک مسیر مدیریتی است.
2. **شکاف هم‌زمانی قواعد مؤثر:** خواندن قواعد بدون قفل انجام می‌شود. اگر نویسنده‌ای در آینده اضافه شود یا هم‌زمان با Deposit قاعده‌ای درج شود، مقدار مؤثر می‌تواند بین خواندن و Commit تغییر کند. Exclusion Constraint فقط از هم‌پوشانی بازه‌های موجود جلوگیری می‌کند، نه از درج قاعده جدید مؤثر در میانه تراکنش.
3. **ناسازگاری Override حداقل بین Deposit و Transfer:** Deposit حداقل را نمی‌خواند؛ Transfer آن را با `COALESCE` (قابل گشایش) اعمال می‌کند. هرگونه یکسان‌سازی نیازمند تصمیم و تغییر خارج از دامنه Deposit است.
4. **Cache کهنه `available_balance`:** پس از Hold جدید/آزادسازی، Cache تا واریز/انتقال بعدی به‌روزرسانی نمی‌شود. برای تصمیم مالی جدید قابل اتکا نیست.
5. **دو معنای متفاوت «قابل استفاده»:** HoldService از `ledger_balance - active_holds` (بدون Overdraft) استفاده می‌کند (`holdservice.cpp:1003-1022`)؛ Cache و Transfer Overdraft را اضافه می‌کنند. این تفاوت باید در Balance Slice صریح شود.
6. **حداقل مانده فقط در Transfer:** Deposit هیچ حداقلی اعمال نمی‌کند (به‌درستی برای واریز)؛ اما هیچ مسیر برداشت/Withdrawal برای اعمال آن وجود ندارد.
7. **نبود API خواندن مانده/تراکنش/Journal:** وجود جدول‌ها یا Repository‌ها به معنی وجود قابلیت خواندن نیست.
8. **نویسندگان legacy ناسازگار:** `AccountBalanceRepository` (بدون مصرف‌کننده) و `TransferService`/`AccountOpeningService` SQL مستقیم دارند و خارج از استخراج UoW Deposit هستند.
9. **شواهد آزمون ناکافی:** آزمون PostgreSQL واقعی، هم‌زمانی واقعی، Fault Injection و رفتار Commit نامعلوم برای این مسیر پس از استخراج‌های 1B.1/1C انجام نشده است.
10. **ابهام زمان مرجع:** زمان شروع تراکنش برای قواعد و Journal استفاده می‌شود، اما تصمیم کسب‌وکاری درباره زمان مرجع (مثلاً `effective_from` دقیقاً در لحظه مرز) ثبت نشده است.

## ۶. کوچک‌ترین زیرمرحله پیشنهادی برای ادامه 1B

**پیشنهاد: 1B.2 — «قرارداد حل قاعده مؤثر مانده» به‌صورت read-only و Deposit-scoped، بدون تغییر رفتار مالی.**

دامنه:

- فقط قرارداد خواندن ورودی‌های قاعده مؤثر در Deposit.
- خروجی قرارداد: `ledgerBalance` (در صورت نیاز برای آزمون)، `activeHolds`، `minimumBalance`، `effectiveOverdraftLimit`، `referenceTimestamp` (از `CURRENT_TIMESTAMP` همان تراکنش)، و منبع تصمیم (Product/Override).
- رفتار Deposit تغییر نمی‌کند: همچنان `ledger_balance` افزایش می‌یابد و `available_balance` دقیقاً با همان فرمول امروزی محاسبه می‌شود. `minimumBalance` در Deposit استفاده نمی‌شود (برای مصرف در Withdrawal/Transfer آینده و آزمون یکسانی قواعد).
- هیچ API/Route/Controller جدید، هیچ Migration و هیچ تغییر Schema.
- هیچ تغییر در Transfer، HoldService یا مسیر legacy.

چرا این کوچک‌ترین گام قابل‌تأیید است:

- استخراج 1B.1 محل SQL را جدا کرد، اما **معنای قاعده مؤثر** هنوز در SQL پراکنده و تکرارشده است و آزمون‌پذیر نیست.
- این گام یک Seam ایجاد می‌کند که بعداً می‌توان تصمیم‌های D1–D3 را بدون بازنویسی Deposit اعمال کرد.
- هیچ تصمیم کسب‌وکاری جدیدی را پیش‌فرض نمی‌گیرد و رفتار مالی را تغییر نمی‌دهد.

گزینه جایگزین (اگر حتی افزودن فیلد به قرارداد پذیرفته نشود): **1B.2-test-only** — فقط آزمون‌های PostgreSQL/HTTP برای تثبیت فرمول و تقدم فعلی، بدون هیچ تغییر کد. این گزینه امن‌ترین است اما قابلیت جدیدی اضافه نمی‌کند.

**موانع ادامه کامل 1B (بدون تصمیم):**

- D1: سیاست Override حداقل (گشایش‌دهنده یا فقط سخت‌گیرانه‌تر).
- D2: سیاست هم‌زمانی درج قاعده مؤثر (قفل/advisory/اعلام نبود نویسنده).
- D3: زمان مرجع رسمی برای اعمال قواعد (شروع تراکنش، زمان قفل، یا پارامتر صریح).

تا زمانی که D1–D3 تعیین تکلیف نشوند، ادعای «تکمیل 1B» یا «آمادگی Balance Slice» معتبر نیست.

## ۷. رابطه با 1C و Balance Slice

- **1B.2** فقط خواندن قاعده مؤثر و مستندسازی/آزمون آن است؛ به Journal/Ledger وابسته نیست و 1C را تغییر نمی‌دهد.
- **1C** (ادامه پس از 1C.2) به invariants ژورنال، posting rules و هم‌ترازی مسیرهای legacy مربوط است؛ 1B.2 آن را نه تکمیل می‌کند و نه مشروط می‌کند. هرگونه ادامه 1C نیازمند تأیید جداگانه است.
- **Balance Slice** قابلیت بزرگ‌تری است: تعریف مرجع «وجه قابل استفاده»، اعمال یکسان در Deposit/Transfer/Withdrawal/Hold، خواندن API مانده، و رفع شکاف هم‌زمانی. این مرحله با تولید این گزارش «شروع‌شده» محسوب نمی‌شود. Balance Slice پس از D1–D3 و 1B.2 و با تأیید صریح جداگانه قابل شروع است.
- Held API بسته است و در این مرحله باز نمی‌شود.

## ۸. ورودی/خروجی و قراردادهای پیشنهادی

قرارداد فقط در لایه Application (بدون `QSqlDatabase`/SQL/خطای خام):

```cpp
struct EffectiveBalanceRuleSnapshot
{
    QString ledgerBalance;          // NUMERIC به‌صورت رشته
    QString activeHolds;            // NUMERIC به‌صورت رشته
    QString minimumBalance;         // NUMERIC به‌صورت رشته
    QString effectiveOverdraftLimit;// NUMERIC به‌صورت رشته
    QString referenceTimestamp;     // CURRENT_TIMESTAMP همان تراکنش (رشته ISO)
    qint64 productRuleId = 0;       // برای ردیابی منبع؛ اختیاری
    qint64 overrideRuleId = 0;      // برای ردیابی منبع؛ اختیاری
};
```

- عملیات پیشنهادی: گسترش `IBalanceRepository::loadLockedAccountSnapshot` یا افزودن متد read-only جدید که همان Query را با ستون‌های بیشتر اجرا می‌کند.
- `DepositService` همچنان فقط `activeHolds` و `effectiveOverdraftLimit` را به `applyDepositIncrement` می‌دهد؛ فیلدهای جدید بدون مصرف در Deposit باقی می‌مانند تا مرحله‌های بعد.
- هیچ تغییر در `applyDepositIncrement` و SQL آن در 1B.2 پیشنهاد نمی‌شود.
- خطاها با `OperationError` امن و کدهای موجود منتقل می‌شوند؛ کد API جدیدی معرفی نمی‌شود.

## ۹. فهرست دقیق فایل‌های احتمالی پیاده‌سازی بعدی و علت تغییر

| فایل | علت تغییر احتمالی | ریسک |
|---|---|---|
| `src/application/contracts/ibalancerepository.h` | افزودن فیلدهای ورودی قاعده مؤثر به Snapshot | کم؛ قرارداد Application |
| `src/infrastructure/repository/balancerepository.cpp` | افزودن ستون‌های `minimum_balance`، `CURRENT_TIMESTAMP` و شناسه قواعد به همان SELECT؛ بدون تغییر تصمیم | متوسط؛ SQL تغییر می‌کند ولی رفتار مالی نه |
| `src/application/depositservice.cpp` | احتمالاً بدون تغییر؛ فقط اگر لازم باشد فیلدها را نادیده بگیرد | کم |
| `tests/balance_rule_resolution_test.cpp` (جدید) | آزمون قرارداد/نگاشت خطا با QSQLITE | کم |
| `tests/...` PostgreSQL/HTTP (جدید یا توسعه) | آزمون واقعی فرمول/تقدم/زمان | متوسط؛ نیازمند محیط توسعه |
| `CMakeLists.txt` | فقط اگر فایل تست/قرارداد جدید به Build اضافه شود | کم |
| `docs/PROJECT_CONTEXT.md`، `docs/PROJECT_HISTORY.md` | ثبت تصمیم/نتیجه | کم |

فایل‌های زیر در 1B.2 تغییر نمی‌کنند: `transferservice.*`، `holdservice.*`، `journalrepository.*`، `transactionrepository.*`، `database/*`، `src/web/*`.

## ۱۰. رفتارهای مالی/API که باید بدون تغییر بمانند

- کدهای API و ترتیب کنترل‌های Deposit: `ACCOUNT_NOT_FOUND`، `ACCOUNT_NOT_ACTIVE`، `PRODUCT_NOT_ACTIVE`، `CUSTOMER_LEDGER_NOT_POSTABLE`، `CUSTOMER_LEDGER_CURRENCY_MISMATCH`، `SETTLEMENT_LEDGER_NOT_CONFIGURED`، `ACCOUNT_BALANCE_NOT_INITIALIZED`، `IDEMPOTENCY_KEY_REUSED`، `IDEMPOTENCY_IN_PROGRESS` و بقیه.
- fingerprint/canonicalization و دامنه Idempotency Deposit.
- `CommitOutcomeUnknown`، عدم Rollback پس از Commit نامعلوم، fail-closed فعلی و رفتار `RollbackFailed`.
- ترتیب Account lock → Balance lock → Hold lock → Snapshot.
- فرمول `available_balance` و عدم استفاده از آن به‌عنوان ورودی تصمیم.
- عدم تغییر `ledger_balance` توسط Hold.
- قرارداد HTTP و پاسخ‌های Deposit.
- عدم ایجاد Commit مستقل برای Transaction/Journal/Balance و باقی‌ماندن همه در یک UoW.

## ۱۱. تصمیم‌هایی که به تأیید کاربر نیاز دارند

1. **D1 — Override حداقل:** آیا Override می‌تواند `minimum_balance` را کمتر از محصول کند (رفتار فعلی Transfer) یا باید مثل Overdraft فقط سخت‌گیرانه‌تر باشد؟ این تصمیم قبل از هر یکسان‌سازی لازم است.
2. **D2 — هم‌زمانی قواعد:** آیا در دامنه فعلی «هیچ نویسنده قاعده» فرض می‌شود، یا باید قفل/advisory lock/سیاست دیتابیسی طراحی شود؟ اگر دوم، ممکن است Migration یا تغییر SQL لازم باشد.
3. **D3 — زمان مرجع:** زمان شروع تراکنش (`CURRENT_TIMESTAMP`) به‌عنوان مرجع رسمی پذیرفته می‌شود یا باید زمان/پارامتر دیگری تعیین شود؟
4. **D4 — محدوده 1B.2:** تأیید می‌شود که 1B.2 فقط read-only و بدون تغییر رفتار باشد، یا ترجیح می‌دهید فعلاً فقط آزمون (test-only) انجام شود؟
5. **D5 — مصرف `minimumBalance` در قرارداد:** آیا افزودن این فیلد بدون مصرف در Deposit پذیرفته است یا از قرارداد حذف شود تا When Needed اضافه گردد؟
6. **D6 — مجوز آزمون:** آزمون PostgreSQL/HTTP روی `corebanking` و ایجاد داده آزمایشی (در صورت نیاز) طبق قواعد AGENTS نیازمند تأیید صریح است.

## ۱۲. معیارهای پذیرش 1B.2

1. هیچ تغییر رفتاری در Deposit مشاهده نشود: همان کدهای خطا، همان ترتیب، همان fingerprint و همان مقادیر مالی.
2. Snapshot حاوی `activeHolds`، `minimumBalance`، `effectiveOverdraftLimit` و `referenceTimestamp` باشد و مقادیر آن با Query مرجع PostgreSQL برابر باشند.
3. `available_balance` پس از واریز برابر `new_ledger_balance - activeHolds + effectiveOverdraftLimit` باشد (در محدوده NUMERIC و بدون خطای گردکردن در C++).
4. تقدم فعلی Override برای Overdraft در آزمون مستند شود: محصول خاموش + Override روشن ⇒ خاموش؛ محصول روشن + Override با سقف کمتر ⇒ سقف کمتر.
5. حداقل مانده در Deposit اعمال/رد نشود (چون واریز است)، اما مقدار آن در قرارداد درست گزارش شود.
6. زمان مرجع در گزارش و آزمون به‌صورت «شروع تراکنش» ثبت شود؛ آزمون مرز `effective_from/effective_to` با همان زمان انجام شود.
7. هیچ SQL نوشتنی، Migration یا تغییر Schema در این مرحله رخ ندهد.
8. آزمون‌های QSQLITE/Unit و آزمون‌های PostgreSQL/HTTP به‌صورت جداگانه گزارش شوند؛ QSQLITE جایگزین PostgreSQL اعلام نشود.
9. همه فایل‌های تغییرکرده و پیام Commit پیشنهادی در پایان گزارش شوند؛ Commit اجرا نشود.

## ۱۳. برنامه تست تفکیک‌شده

### Unit

- نگاشت خطاهای امن Repository (بدون نشت SQL) برای Queryهای جدید.
- بررسی عدم تغییر fingerprint/canonicalization (آزمون موجود Deposit).
- بررسی اینکه `minimumBalance` در Deposit هیچ شاخه تصمیمی فعال نمی‌کند (با fake repository).

### Orchestration

- Fake UoW: فراخوانی‌های Account → Balance → Hold → Snapshot → Journal → Balance update → Ledger → Post → Complete با همان ترتیب.
- Replay/Conflict/InProgress و عدم ورود به Balance/Journal.
- شکست Snapshot/Update و Rollback.
- `DATABASE_UNAVAILABLE` و عدم ساخت UoW.

### PostgreSQL

- روی `corebanking` (با تأیید): واریز به حساب دارای Hold فعال و قاعده Override؛ سپس بررسی:
  - `ledger_balance` جدید
  - `available_balance` ذخیره‌شده
  - `SUM(currently_blocked_amount)`
  - مقدار مؤثر Overdraft
- آزمون مرزهای `effective_from/effective_to` با تراکنش کنترل‌شده و زمان صریح.
- آزمون `minimum_balance` در قرارداد (فقط خواندن) و عدم اثر آن روی واریز.
- آزمون عدم منفی‌شدن `ledger_balance` (قید `ck_account_balance_ledger`).
- آزمون تطبیق `ledger_account_balance` و Journal پس از واریز.

### HTTP

- Deposit موفق به حساب دارای Hold و Override؛ پاسخ و شناسه‌ها بدون تغییر.
- Retry همان کلید؛ Conflict با payload متفاوت.
- حساب غیر ACTIVE، محصول غیرفعال، سرفصل نامناسب.
- بررسی read-only پس از HTTP: Transaction، Journal، `account_balance`، `ledger_account_balance` و Hold sum.

### Concurrency

- Deposit و Hold create/release هم‌زمان روی یک حساب: انتظار Serialize شدن با Account lock؛ بررسی نهایی `ledger_balance` و `active_holds` و Cache.
- Deposit و Transfer هم‌زمان روی یک حساب: بررسی ترتیب قفل و عدم Deadlock در سطح سناریو.
- درج قاعده جدید در میانه Deposit (اگر نویسنده‌ای فعال شود): این آزمون باید **شکاف D2 را مستند کند**، نه ادعای رفع آن.
- دو Deposit هم‌زمان با کلیدهای متفاوت: افزایش درست `ledger_balance` و دو Transaction/Journal مستقل.

### Fault / Restart

- قطع اتصال یا restart پس از Commit نامعلوم: Retry با همان `idempotencyKey` باید نتیجه پایدار را بازیابی کند و واریز دوباره اعمال نشود.
- شکست در میانه (پس از Update Balance و پیش از Commit): نباید مقدار نیمه‌کاره باقی بماند؛ Rollback باید `ledger_balance` و Cache را برگرداند.
- Fail-closed فعلی پس از `RollbackFailed`/`CommitOutcomeUnknown` باید بدون تغییر باقی بماند.
- هیچ Retry خودکار با کلید جدید و هیچ Rollback پس از Commit نامعلوم مجاز نیست.

## ۱۴. محدودیت‌هایی که پس از 1B.2 همچنان باقی می‌مانند

- شکاف هم‌زمانی قواعد مؤثر (تا D2 حل نشود).
- نبود نویسنده/API مدیریت `product_balance_rule` و `account_balance_rule_override`.
- ابهام Override حداقل (تا D1 حل نشود).
- `available_balance` همچنان Cache کهنه‌شونده و غیرقابل اتکا برای تصمیم جدید.
- تفاوت معنای «قابل استفاده» بین Hold (بدون Overdraft) و Cache/Transfer (با Overdraft) تا Balance Slice.
- انتقال/یکسان‌سازی Deposit با Transfer/Hold/AccountOpening انجام نمی‌شود.
- Withdrawal، Reversal و API خواندن مانده/تراکنش/Journal همچنان وجود ندارند.
- آزمون PostgreSQL واقعی، هم‌زمانی واقعی، Fault Injection و رفتار QPSQL در Commit نامعلوم تا اجرای آن‌ها اثبات‌نشده باقی می‌ماند.
- 1B و 1C با این گزارش «تکمیل‌شده» اعلام نمی‌شوند و Balance Slice آغاز نمی‌شود.

## ۱۵. نتیجه

- رفتار فعلی Deposit از نظر ساختاری استخراج شده است، اما معنای قاعده مؤثر مانده، زمان مرجع و سیاست Override/هم‌زمانی هنوز تصمیم‌گیری و آزمون نشده‌اند.
- کوچک‌ترین گام ایمن، تثبیت read-only قرارداد «حل قاعده مؤثر» بدون تغییر رفتار است؛ گام مالی بعدی (Balance Slice) به تصمیم‌های D1–D3 و تأیید جداگانه نیاز دارد.
- این گزارش هیچ آزمون، Build، SQL یا تغییر داده‌ای را ادعا نمی‌کند.
