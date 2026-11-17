# نتیجه مرحله T1 — اصلاح سیاست حداقل مانده Transfer

نوع مرحله: اصلاح محدود و آزمون Expression سیاست حداقل مانده در Transfer. هیچ تغییر دیگری در کد، Schema، داده، Context/History یا Git انجام نشد.

## ۱. SHA و وضعیت Git

- شاخه: `main`
- HEAD: `d0f53a4931ccf74175f9904176713dac025e0c9b`
- `origin/main`: همان `d0f53a4`
- وضعیت اولیه: فقط `docs/opencode-reports/TRANSFER_REVIEW_RESULT.md` به‌صورت untracked (مورد انتظار مرحله قبل)؛ هیچ تغییر کد غیرمنتظره وجود نداشت.
- وضعیت پس از این مرحله:
  - `M src/application/transferservice.cpp`
  - `?? docs/opencode-reports/TRANSFER_REVIEW_RESULT.md` (مرحله قبل، دست‌نخورده)
  - `?? docs/opencode-reports/TRANSFER_MINIMUM_POLICY_RESULT.md` (همین گزارش)
- هیچ `stage`/`commit`/`push` اجرا نشد.

## ۲. تصمیم اعمال‌شده

```text
effective_minimum =
GREATEST(COALESCE(product_minimum, 0),
         COALESCE(account_override_minimum, 0))
```

- حداقل محصول کف الزام‌آور است.
- Override حساب فقط می‌تواند حداقل را افزایش دهد.
- `LEAST` استفاده نشد.
- شرط‌های زمانی (`CURRENT_TIMESTAMP` و بازه `[effective_from, effective_to)`) و انتخاب Rule بدون تغییر حفظ شدند.
- Hold، Overdraft، `available_balance`، fingerprint، API و سایر SQLها تغییر نکردند.

## ۳. Diff محدود

فقط بلوک محاسبه minimum در `loadLockedAccounts` واقع در `src/application/transferservice.cpp` تغییر کرد.

```diff
@@ -119,19 +119,21 @@ bool loadLockedAccounts(QSqlDatabase& db,
         " COALESCE((SELECT SUM(hs.currently_blocked_amount) "
         "             FROM account_hold_status hs "
         "            WHERE hs.account_id = a.id), 0), "
-        " COALESCE((SELECT o.minimum_balance "
-        "             FROM account_balance_rule_override o "
-        "            WHERE o.account_id = a.id "
-        "              AND o.minimum_balance IS NOT NULL "
-        "              AND o.effective_from <= CURRENT_TIMESTAMP "
-        "              AND (o.effective_to IS NULL OR o.effective_to > CURRENT_TIMESTAMP) "
-        "            ORDER BY o.effective_from DESC LIMIT 1), "
-        "          (SELECT pbr.minimum_balance "
+        " GREATEST( "
+        "   COALESCE((SELECT pbr.minimum_balance "
         "             FROM product_balance_rule pbr "
         "            WHERE pbr.product_id = a.product_id "
         "              AND pbr.effective_from <= CURRENT_TIMESTAMP "
         "              AND (pbr.effective_to IS NULL OR pbr.effective_to > CURRENT_TIMESTAMP) "
         "            ORDER BY pbr.effective_from DESC LIMIT 1), 0), "
+        "   COALESCE((SELECT o.minimum_balance "
+        "             FROM account_balance_rule_override o "
+        "            WHERE o.account_id = a.id "
+        "              AND o.minimum_balance IS NOT NULL "
+        "              AND o.effective_from <= CURRENT_TIMESTAMP "
+        "              AND (o.effective_to IS NULL OR o.effective_to > CURRENT_TIMESTAMP) "
+        "            ORDER BY o.effective_from DESC LIMIT 1), 0) "
+        " ), "
         " CASE WHEN COALESCE((SELECT pbr.overdraft_enabled "
```

`git diff --stat`: `1 file changed, 10 insertions(+), 8 deletions(-)`.
`git diff --check`: بدون خطا (`exit=0`).

## ۴. Build

```bash
cmake --build /home/ahmad/projects/build-CoreBanking-Desktop-Debug -j2
```

خروجی واقعی:

```text
[  1%] Building CXX object CMakeFiles/CoreBanking.dir/src/application/transferservice.cpp.o
[  2%] Linking CXX executable CoreBanking
[100%] Built target CoreBanking
```

نتیجه: موفق، بدون هشدار کامپایلر در خروجی.

## ۵. آزمون PostgreSQL read-only — هفت حالت

فایل موقت `/tmp/opencode/transfer_minimum_policy.sql` با `statement_timeout='5s'` و `default_transaction_read_only=on` اجرا شد. هیچ نوشتنی در جدول‌ها انجام نشد؛ داده فقط در `VALUES/CTE` بود.

```sql
SET statement_timeout = '5s';
SET default_transaction_read_only = on;

WITH cases(name, product_min, account_min, expected) AS (
    VALUES
        ('product100_no_override',       100::numeric, NULL::numeric, 100::numeric),
        ('product100_override20',        100::numeric, 20::numeric,   100::numeric),
        ('product100_override150',       100::numeric, 150::numeric,  150::numeric),
        ('product100_override0',         100::numeric, 0::numeric,    100::numeric),
        ('no_product_override20',        NULL::numeric, 20::numeric,   20::numeric),
        ('no_product_no_override',       NULL::numeric, NULL::numeric,  0::numeric),
        ('product100_ineffective_override_null', 100::numeric, NULL::numeric, 100::numeric)
)
SELECT name,
       GREATEST(COALESCE(product_min, 0), COALESCE(account_min, 0)) AS effective_minimum,
       expected,
       GREATEST(COALESCE(product_min, 0), COALESCE(account_min, 0)) = expected AS pass
FROM cases ORDER BY name;
```

خروجی واقعی:

```text
                 name                 | product_min | account_min | effective_minimum | expected | pass
--------------------------------------+-------------+-------------+-------------------+----------+------
 no_product_no_override               |             |             |                 0 |        0 | t
 no_product_override20                |             |          20 |                20 |       20 | t
 product100_ineffective_override_null |         100 |             |               100 |      100 | t
 product100_no_override               |         100 |             |               100 |      100 | t
 product100_override0                 |         100 |           0 |               100 |      100 | t
 product100_override150               |         100 |         150 |               150 |      150 | t
 product100_override20                |         100 |          20 |               100 |      100 | t
(7 rows)
```

همه ۷ حالت `pass = t`. مورد آخر فقط NULL semantics است (Override مؤثر وجود ندارد)، نه آزمون واقعی انقضای Rule.

### پوشش دقیق این آزمون

- **پوشش می‌دهد:** رفتار PostgreSQL برای `GREATEST` و `COALESCE` با NULL؛ اثر Override کوچک‌تر/بزرگ‌تر/صفر؛ پایه صفر وقتی Rule محصول نیست.
- **پوشش نمی‌دهد:** Subqueryهای واقعی جدول `product_balance_rule`/`account_balance_rule_override`، فیلترهای `CURRENT_TIMESTAMP`، انتخاب آخرین Rule مؤثر با `ORDER BY ... LIMIT 1`، و wiring کامل Query تولیدی `loadLockedAccounts`. این آزمون «فرمول Expression» است، نه اجرای Production Query.
- **ادعا نمی‌شود:** این اجرا اثبات انقضای واقعی Rule یا آزمون کامل Snapshot/Transfer است.

## ۶. آزمون مستقیم کد/Query تولیدی Transfer

تلاش برای اجرای مستقیم `loadLockedAccounts` تولیدی بدون بازآرایی، در محدوده این مرحله **عملی نبود**:

- `loadLockedAccounts` در anonymous namespace فایل `transferservice.cpp` است و قرارداد/هدر عمومی ندارد؛ فراخوانی مستقیم آن نیازمند include کردن فایل `.cpp` در تست یا استخراج تابع/Repository است که «بازآرایی/استخراج» محسوب می‌شود و در دامنه T1 مجاز نیست.
- اجرای Query تولیدی روی دیتابیس با نقش read-only ممکن نیست، چون Query شامل `FOR UPDATE OF a, b` است و نقش `corebanking_reader` اجازه آن را ندارد. استفاده از نقش نوشتنی/privileged ممنوع است.
- اجرای Query روی QSQLITE نیز ممکن نیست، چون SQL شامل `FOR UPDATE OF`، `GREATEST`، VIEW `account_hold_status` و جدول‌های Rule است.
- نوشتن ردیف Rule در دیتابیس مشترک برای پوشش seven-state در این مرحله ممنوع است.

بنابراین هیچ فایل تست جدیدی اضافه نشد و هیچ فرمول مستقلی در fake به‌عنوان تست رفتاری Transfer معرفی نمی‌شود. محدودیت صریح ثبت شد.

## ۷. محدودیت‌های باقی‌مانده

- رفع minimum به معنی رفع خطر Commit/Rollback یا آمادگی انتشار Transfer **نیست**. یافته‌های C1 (نبود تفکیک نتیجه نامعلوم Commit و Rollback در آن وضعیت)، C4 (State Machine)، C3 (مبلغ عددی)، C5 (نشت خطا)، C6، C7 و C8 در `TRANSFER_REVIEW_RESULT.md` همچنان باز هستند و در T1 تغییر نکردند.
- سیاست minimum با ردیف Rule واقعی PostgreSQL، مرزهای زمانی واقعی و انقضای واقعی Rule آزمون نشد.
- هیچ آزمون مالی API، هیچ هم‌زمانی واقعی، هیچ Fault Injection و هیچ restart انجام نشد.
- Build موفق فقط کامپایل را ثابت می‌کند؛ جایگزین آزمون رفتار PostgreSQL با Rule واقعی نیست.
- Deposit، Withdrawal، Hold، Schema، داده و مستندات Context/History تغییر نکردند.
- گزارش بازبینی قبلی (`TRANSFER_REVIEW_RESULT.md`) بازنویسی نشد.

## ۸. جمع‌بندی

- فقط سیاست حداقل مانده Transfer به `MAX(product, account override)` تغییر کرد و Build موفق بود.
- هفت حالت Expression روی PostgreSQL read-only با `VALUES/CTE` همگی pass شدند.
- تست مستقیم Query تولیدی بدون بازآرایی/دسترسی نوشتنی ممکن نبود؛ محدودیت ثبت شد.
- این مرحله Transfer را برای انتشار آماده نمی‌کند و فقط یکی از یافته‌های بازبینی (C2) را می‌بندد.
