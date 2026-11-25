# Core Banking — Project Context

> مرجع قواعد کسب‌وکار، تصمیم‌های معماری، قراردادهای فنی و وضعیت جاری قابلیت‌های پروژه.
>
> مخزن رسمی: https://github.com/ahmadbabayi/Core-Banking
>
> Schema مرجع: `database/CURRENT_SCHEMA.sql`
>
> تاریخچه و شواهد اجرا: `docs/PROJECT_HISTORY.md`
>
> این سند باید برای فهم قواعد جاری و وضعیت فعلی پروژه کافی باشد. شرح اجراهای گذشته و خروجی‌های طولانی در History و گزارش‌های `docs/opencode-reports/` نگهداری می‌شوند.

## راهنمای سریع عامل

### وضعیت جاری — 2026-11-25

- **تراز آزمایشی خواندنی (Trial Balance) — هستهٔ داخلی + مسیر HTTP (2026-11-25):** `TrialBalanceService::queryTrialBalance(currencyId, from, to)` روی قرارداد خواندنی مستقل `ITrialBalanceRepository` و پیاده‌سازی QPSQL `TrialBalanceRepository` که مستقیم از `journal_entry JOIN journal` با `journal.status = 'POSTED'` و مبنای تاریخ `journal.posting_date` می‌خواند (نه از `ledger_account_balance`/`account_balance` و نه از GL Inquiry صفحه‌بندی‌شده). یک SELECT پارامتری واحد که guard و گزارش را در همان snapshot اجرا می‌کند (بدون query دوم که بتواند حالتی متفاوت ببیند): CTE `flags` (وجود ارز + پرچم ناسازگاری ارزِ درون‌دامنه + پرچم مبلغ NaN/±Infinity) همراه CTE گزارش که کل محاسبه را در NUMERIC PostgreSQL انجام می‌دهد — `openingNet = SUM(debit−credit)` برای `posting_date < from` از ابتدای تاریخچه (نامحدود؛ سقف ۳۶۶ روز فقط روی گردش دوره است)، گردش بدهکار/بستانکار جداگانه در بازهٔ نیمه‌باز `[from,to)` (یعنی `from` شامل و `to` خارج)، `closingNet = openingNet + periodDebit − periodCredit` و شکستن علامت هر مبلغ به ستون بدهکار/بستانکار از علامت واقعی خالص (نه `account_category`). سرفصل فقط وقتی برمی‌گردد که ماندهٔ اول یا گردش دوره غیرصفر است؛ سرفصل غیرفعال/غیرقابل‌ثبت با ثبت تاریخی به خاطر وضعیت جاری حذف نمی‌شود؛ بدون roll-up والد/فرزند و بدون فیلتر وضعیت تراکنش؛ گردش بدهکار و بستانکار برابر دلیل حذف ردیف نیست و اصل و معکوس هرکدام با جهت ثبت‌شده در تاریخ خود وارد می‌شوند. `totals` جمع همان شش ستون پس از محاسبهٔ ستون‌ها در SQL است (نه خالص‌کردن کل دفتر) و `isBalanced` با برابری NUMERIC در هر سه جفت ستون (اول دوره/گردش/پایان) به‌دست می‌آید و فقط توازن گزارش را نشان می‌دهد، نه صحت تک‌تک اسناد. هر ردیف فقط `ledgerAccountId` (رشته)، `code`، `name` و شش مبلغ (`openingDebit`/`openingCredit`/`periodDebit`/`periodCredit`/`closingDebit`/`closingCredit`، رشتهٔ NUMERIC بدون float/گردکردن/محدودکردن scale)؛ فیلدهای مشتری/حساب/تراکنش/شرح افشا نمی‌شوند. ناسازگاری ارزِ درون‌دامنه (آرتیکل POSTED با واحد پول خواسته‌شده که سرفصل خودش واحد دیگری دارد و بی‌صدا حذف می‌شد) یا مبلغ غیرمتناهیِ درون‌دامنه → شکست کل گزارش؛ ناسازگاری ارزِ خارج از این گزارش (سمت آرتیکل در واحد پول دیگری) این گزارش را از کار نمی‌اندازد. **مسیر HTTP `GET /api/v1/trial-balance?currencyId=&from=&to=` فعال شد (2026-11-25):** هر سه پارامتر الزامی و دقیقاً یک‌بار؛ `currencyId` عدد صحیح مثبت qint64؛ تاریخ سخت‌گیرانهٔ `YYYY-MM-DD` بدون زمان/منطقهٔ زمانی؛ `from < to` و بازهٔ حداکثر ۳۶۶ روز. پاسخ `ApiResponse`: 200 (ارز موجود حتی با `rows=[]`)، 400 (`MISSING_CURRENCY_ID`/`INVALID_CURRENCY_ID`/`MISSING_FROM`/`MISSING_TO`/`INVALID_PERIOD`)، 404 سرفصل ارز ناموجود (`CURRENCY_NOT_FOUND`)، 500 (`TRIAL_BALANCE_FAILED` با پیام عمومی «Internal server error»، بدون متن SQL/درایور و بدون نتیجهٔ ناقص). گزارش نامتوازن اما قابل‌محاسبه خطای فنی نیست: 200 با `isBalanced=false`. بدون صفحه‌بندی/LIMIT؛ گزارش در لحظهٔ خواندن از اسنادِ `journal` موجود محاسبه می‌شود و «snapshot» فقط read snapshotِ واحد همان SELECT است (نه snapshot تاریخیِ قفل‌شده/آرشیوشده). شواهد: Build موفق target اصلی؛ تست مستقل سرویس با fake (۷ تابع، خروجی ۰)؛ اعتبارسنجی SELECTخواندنیِ مستقل از خود SQL گزارش روی دادهٔ واقعی با `corebanking_reader` (بازهٔ کامل با بستن 1580/1580 و گردش `1935.12345678` و `isBalanced=true`، مرز half-open `from` شامل/`to` خارج، ردیف فقط-flag برای بازهٔ خالی و `currency_found=0` برای ارز ناموجود). آزمون واقعی HTTP پایان‌به‌پایان با curl روی باینری نهایی (اجرای موقت تأییدشده با اتصال عادی/نقش `corebanking`، بدون تغییر bind/پورت/فایروال؛ PID و مسیر executable فرایند پاسخ‌دهندهٔ 8080 ثبت شد): ۲۰۰ غیرخالی با دقت `5.12345678` (سند ۴) و `isBalanced=true`؛ مرز `from` شامل/`to` خارج؛ ماندهٔ منتقل‌شده 1580/1580 با گردش صفر در بازهٔ پس از آخرین ثبت؛ بازهٔ قبل از اولین ثبت `rows=[]` با جمع صفر و متوازن؛ ماتریس 400 (ناقص/تکراری/سرریز/بازهٔ نامعتبر)؛ 404 ارز مثبت ناموجود؛ و عدم پسرفت GL Inquiry و Journal Inquiry. هر شش ستون و جمع‌ها با محاسبهٔ مستقل SELECTخواندنی تطبیق شدند و هر دو سند ۷ و ۸ (هرکدام 120) وارد گردش همان روز 2026-11-21 شدند. مرحله کامل تلقی می‌شود. بدون احراز هویت/مجوز؛ مسیر روی listener موجود برنامه ثبت شده و آمادهٔ انتشار عمومی نیست. مسیر `500` واقعی، گزارش چندارزی، فیلتر غیر POSTED و گزارش نامتوازن واقعی در HTTP با دادهٔ فعلی (تک‌ارز و همهٔ سندها `POSTED`) قابل تمرین نبودند.

- **استعلام خواندنی دفتر کل (GL Inquiry) — هستهٔ داخلی + مسیر HTTP صفحه‌بندی‌شده (2026-11-25):** `LedgerInquiryService::queryEntries(...)` در لایهٔ application روی قرارداد خواندنی مستقل `ILedgerInquiryRepository` (جدای از Repositoryهای مالی) و پیاده‌سازی QPSQL `LedgerInquiryRepository`. ابتدا وجود سرفصل با یک SELECT پارامتری خوانده می‌شود (تفکیک «سرفصل ناموجود» از «سرفصل موجود با صفحهٔ خالی»)، سپس آرتیکل‌های مستقیم آن با JOIN پارامتری `journal_entry → journal`، فیلتر بر `ledger_account_id` و `journal.status = 'POSTED'` و بازهٔ نیمه‌باز `[from,to)` روی `posting_date` و ترتیب صعودی `(posting_date, id)` خوانده می‌شوند؛ بدون قفل، بدون UoW مالی و بدون فیلتر وضعیت تراکنش. صفحه‌بندی با مکان‌نمای `(postingDate, entryId)` است و صفحهٔ بعد با fetch کردن `limit+1` ردیف تشخیص داده می‌شود (ردیف اضافهٔ probe هرگز برنمی‌گردد). هر ردیف فقط `entryId`، `journalId`، `transactionId`، `sequenceNo`، `postingDate`، `valueDate`، `currencyId`، `debitAmount`، `creditAmount`؛ مبلغ‌های NUMERIC با CAST به text (بدون float/گردکردن) و شناسه‌های qint64 رشته صادر می‌شوند. `accountId`/description/جزئیات مشتری/فیلدهای داخلی افشا نمی‌شوند؛ اصل و معکوس هرکدام مستقل با جهت خودشان برمی‌گردند (ادغام/خالص‌سازی نه)؛ جمع/مانده/Trial Balance جزو این سرویس نیست. ناسازگاری ارز هر ردیف خوانده‌شده (از جمله probe) با ارز سرفصل → شکست سراسری (نه حذف بی‌صدا). **مسیر HTTP `GET /api/v1/ledger-accounts/{ledgerAccountId}/entries?from=&to=&limit=&afterPostingDate=&afterEntryId=` فعال شد (2026-11-25):** `ledgerAccountId` عدد صحیح مثبت؛ `from`/`to` هرکدام دقیقاً یک‌بار، الزامی، فرمت سخت‌گیرانهٔ `YYYY-MM-DD` (بدون زمان/منطقهٔ زمانی)، `from < to` و بازهٔ حداکثر ۳۶۶ روز؛ `limit` اختیاری (پیش‌فرض ۵۰، بازهٔ ۱..۱۰۰)؛ مکان‌نما `afterPostingDate`+`afterEntryId` هر دو یا هیچ‌کدام، تاریخ داخل بازه و entryId مثبت. پاسخ با قالب `ApiResponse`: 200 (حتی با `rows=[]` برای سرفصل موجود)، 400 ورودی نامعتبر (`INVALID_LEDGER_ACCOUNT_ID`/`MISSING_FROM`/`MISSING_TO`/`INVALID_PERIOD`/`INVALID_LIMIT`/`INVALID_CURSOR`)، 404 سرفصل ناموجود (`LEDGER_ACCOUNT_NOT_FOUND`)، 500 خطای خواندن/ناسازگاری ارز (`LEDGER_INQUIRY_FAILED` با پیام عمومی «Internal server error»، بدون متن SQL/درایور و بدون نتیجهٔ ناقص). `data` فقط `ledgerAccountId`، `currencyId`، `from`، `to`، `rows`، `nextCursor` (null یا `{afterPostingDate, afterEntryId}` که از آخرین ردیف بازگشتی ساخته می‌شود، نه از probe). شواهد: Build موفق در target اصلی؛ تست مستقل سرویس با fake Repository (۱۰ تابع، خروجی ۰)؛ آزمون واقعی HTTP با curl (۲۰۰ با دقت `5.12345678`، اصل/معکوس با جهت مخالف در سرفصل مشترک، بازهٔ خالی، مرزهای half-open، دو صفحهٔ `limit=1` بدون تکرار/پرش، 400ها و 404) و regression مسیرهای موجود ledger-account و journal و transaction. بدون احراز هویت/مجوز، فقط مصرف محلی/داخلی و نه آمادهٔ انتشار. **حد پوشش تست:** مسیر `500` واقعی، ناسازگاری ارز و فیلتر non-POSTED در HTTP با دادهٔ واقعی قابل تمرین نبودند؛ رد ناسازگاری ارز فقط در لایهٔ Service با fake آزمون شد و فیلتر `journal.status = 'POSTED'`، ترتیب، `LIMIT limit+1` و بررسی ارز روی ردیف probe فقط با بازبینی کد پوشش دارند (همهٔ سندها `POSTED` و تک‌ارزند).

- **استعلام خواندنی یک تراکنش — هستهٔ داخلی + مسیر HTTP (2026-11-24):** `TransactionQueryService::queryById(id)` روی `ITransactionRepository` با استفادهٔ مجدد از `findById` موجود (SELECT پارامتری بدون `FOR UPDATE` و بدون مسیر idempotency مالی). خروجی `TransactionQueryResult` فقط شامل فیلدهای مجاز است: `id`، `transactionNumber`، `type`، `status`، `amount`، `currencyId`، `reversesTransactionId` (فقط برای REVERSAL) و زمان‌های `initiatedAt`/`completedAt`/`createdAt`/`updatedAt`؛ `idempotency_key`، `request_fingerprint`، `result_code`/`result_message`، description/reference/channel و جزئیات حساب طرف مقابل افشا نمی‌شوند و وضعیت `REVERSED` اصل همان‌طور ثبت‌شده (نه ادغام‌شده با رکورد REVERSAL) برمی‌گردد. **مسیر HTTP `GET /api/v1/transactions/{transactionId}` فعال شد (2026-11-24):** با قالب `ApiResponse`؛ شناسهٔ عدد صحیح مثبت در بازهٔ qint64 معتبر است و صفر/منفی/غیرعددی/سرریز → `400 INVALID_TRANSACTION_ID`؛ تراکنش ناموجود → `404 TRANSACTION_NOT_FOUND`؛ خطای خواندن → `500 TRANSACTION_LOOKUP_FAILED` با پیام عمومی (بدون متن خام SQL/درایور و بدون نتیجهٔ ناقص). پاسخ ۲۰۰ فقط فیلدهای مجاز: `id`، `transactionNumber`، `type`، `status`، `amount`، `currencyId`، `reversesTransactionId` (فقط برای REVERSAL)، `initiatedAt`/`completedAt`/`createdAt`/`updatedAt`. شناسه‌های qint64 و مبلغ `amount` رشته صادر می‌شوند (بدون تبدیل به double)؛ زمان‌ها بدون منطقهٔ زمانی به شکل `yyyy-MM-ddTHH:mm:ss[.mmm]`؛ `completedAt` نامعتبر JSON `null`. مسیر موجود `POST /api/v1/transactions/{id}/reversals` دست‌نخورده ماند. بدون احراز هویت/مجوز؛ فقط مصرف محلی/داخلی است و آمادهٔ انتشار عمومی نیست. سه شاهد جداگانه: (۱) Build برنامهٔ اصلی موفق؛ `transactionqueryservice.{h,cpp}` در target اصلی ثبت، کامپایل و لینک شد (تأیید با object فایل و نماد `queryById` در باینری) و بازتولید خودکار CMake فقط برای همین ثبت و طبق مجوز انجام شد؛ (۲) تست مستقل سرویس با fake Repository واقعاً لینک/اجرا شد (هر ۶ سناریو، خروجی ۰)؛ (۳) شواهد `SELECT` read-only با `corebanking_reader` روی تراکنش عادی/اصل REVERSED/رکورد REVERSAL تطبیق شد. دفتر کل و تراز آزمایشی همچنان خارج از دامنه و شروع‌نشده‌اند.
- **صورتحساب خواندنی یک حساب — هسته + مسیر HTTP (2026-11-23):** `AccountStatementService::query(accountId, from, to)` در لایهٔ application؛ بازهٔ نیمه‌باز `[from,to)` روی `completed_at`. خروجی = «گردش خواندنی»: `totalCredits`/`totalDebits` و ردیف‌های مرتب‌شده بر اساس زمان و شناسه، با `openingBalance`/`closingBalance` صریحاً `unavailable` (جعل نمی‌شود) و بدون `balanceAfter` (چون ماندهٔ تاریخی قابل بازسازی نیست؛ `account_balance` فقط ماندهٔ جاری است). هر ردیف فقط فیلدهای مجاز را دارد (transactionId، transactionNumber، transactionType، status، amount، currencyId، occurredAt، direction). **اثبات direction از کد:** `DESTINATION`=ورود وجه/CREDIT و `SOURCE`=خروج وجه/DEBIT (Deposit، Withdrawal، Transfer و Reversal). فقط وضعیت‌های دارای اثر مالی ثبت‌شده وارد می‌شوند: `COMPLETED` و اصل `REVERSED` (که پیش از برگشت کامل‌شده بود و `completed_at` خود را حفظ می‌کند)؛ `INITIATED/PROCESSING/FAILED/REJECTED/CANCELLED` (بدون اثر ثبت‌شده) و `FEE` کنار می‌روند؛ `REVERSAL` و اصل `REVERSED` به‌عنوان دو ردیف مستقل هرکدام دقیقاً یک‌بار با زمان تکمیل خودشان شمرده می‌شوند (نه حذف اصل، نه ادغام/خالص‌کردن). خواندن با JOIN پارامتری `transaction_entry`→`transaction` بدون `FOR UPDATE` و بدون مسیر idempotency/قفل؛ چندارزی ناسازگار با `CURRENCY_MISMATCH` صریح رد می‌شود. `idempotency_key`/fingerprint/`result_code`/description/جزئیات طرف مقابل افشا نمی‌شود. **HTTP فعال شد (2026-11-23):** `GET /api/v1/accounts/{accountId}/statement?from=...&to=...` — `accountId` عدد صحیح مثبت؛ `from`/`to` هرکدام دقیقاً یک‌بار و الزامی با فرمت `YYYY-MM-DDTHH:mm:ss` (میلی‌ثانیهٔ اختیاری، بدون offset/Z؛ چون `completed_at` بدون timezone است). بازهٔ نیمه‌باز `[from,to)` و `from < to`. پاسخ با قالب `ApiResponse`: 200 برای حساب موجود (حتی با `rows` خالی)، 400 ورودی نامعتبر (`INVALID_ACCOUNT_ID`/`MISSING_FROM`/`MISSING_TO`/`INVALID_PERIOD`)، 404 حساب ناموجود (`ACCOUNT_NOT_FOUND`)، 500 خطای Repository یا ناسازگاری داده با پیام عمومی (بدون افشای SQL). مبلغ‌ها و جمع‌ها رشتهٔ اعشاری صادر می‌شوند (هرگز JSON float)؛ `openingBalance`/`closingBalance` برابر `null` با پرچم availability `false`؛ `balanceAfter` و فیلدهای داخلی/حساس منتشر نمی‌شوند. قواعد هسته در HTTP تکرار یا تغییر داده نشده‌اند (اصل `REVERSED` و `REVERSAL` دو ردیف مستقل در زمان‌های خودشان). **بدون احراز هویت/مجوز؛ فقط محیط توسعهٔ محلی، نه آمادهٔ انتشار عمومی و بدون تغییر تنظیم شبکهٔ سراسری.** Schema و دادهٔ دیتابیس تغییر نکرده است. شواهد Build، تست fake هسته (۱۴ سناریو) و آزمون واقعی HTTP با curl در History 18.39.
- **استعلام خواندنی یک سند حسابداری با شناسه — هستهٔ داخلی + مسیر HTTP (2026-11-24):** `JournalQueryService::queryById(journalId)` در لایهٔ application روی قرارداد خواندنی مستقل `IJournalQueryRepository` (جدای از `IJournalRepository` مالی) و پیاده‌سازی QPSQL `JournalQueryRepository`. یک SELECT پارامتری با `LEFT JOIN journal → journal_entry`، فیلتر بر `j.id` و `ORDER BY je.sequence_no`؛ بدون قفل و بدون UoW مالی. `id <= 0` → `InvalidInput`/`INVALID_JOURNAL_ID` بدون فراخوانی Repository؛ شناسهٔ ناموجود → `NotFound`/`JOURNAL_NOT_FOUND`؛ خطای خواندن → `Failed`/`JOURNAL_LOOKUP_FAILED` با پیام عمومی (بدون متن SQL/درایور و بدون نتیجهٔ ناقص). خروجی موفق فقط سربرگ `id`، `transactionId`، `status` (وضعیت ذخیره‌شدهٔ خود سند بدون فیلتر: DRAFT/POSTED/REVERSED؛ از وضعیت تراکنش استنتاج نمی‌شود)، `postingDate`، `valueDate` و فهرست `lines`؛ هر آرتیکل فقط `sequenceNo`، `ledgerAccountId`، `accountId` اختیاری (NULL حساب صفر نمی‌شود)، `currencyId`، `debitAmount` و `creditAmount` (NUMERIC با CAST به text، بدون float/گردکردن). description و فیلدهای داخلی تراکنش برنگردانده می‌شوند؛ نبود سربرگ از سند موجود با lines خالی تفکیک می‌شود و ردیف NULL حاصل از LEFT JOIN آرتیکل نیست. اصل و سند معکوس فقط با شناسهٔ خودشان مستقل خوانده می‌شوند؛ جمع‌زنی/مانده‌سازی/اعتبار توازن جزو این سرویس نیست. **مسیر HTTP `GET /api/v1/journals/{journalId}` فعال شد (2026-11-24):** با قالب `ApiResponse`؛ شناسهٔ مثبت داخل بازهٔ qint64 معتبر و صفر/منفی/غیرعددی/سرریز → `400 INVALID_JOURNAL_ID`؛ ناموجود → `404 JOURNAL_NOT_FOUND`؛ خطای خواندن → `500 JOURNAL_LOOKUP_FAILED` با پیام عمومی. پاسخ ۲۰۰ فقط سربرگ `id`، `transactionId`، `status` (وضعیت ذخیره‌شدهٔ خود سند)، `postingDate`، `valueDate` و `lines` است؛ هر آرتیکل فقط `sequenceNo` (عدد)، `ledgerAccountId`، `accountId` (رشته یا JSON null)، `currencyId`، `debitAmount` و `creditAmount` (رشته)؛ شناسه‌های qint64 و مبلغ‌ها رشته (بدون افت دقت)، تاریخ‌ها `YYYY-MM-DD` و فهرست بدون آرتیکل `[]`. description و فیلدهای داخلی تراکنش منتشر نمی‌شوند. شواهد: Build (کامپایل/لینک در target اصلی)، آزمون واقعی HTTP با curl (سندهای ۴/۷/۸ با ترتیب آرتیکل/دقت مبلغ/account null/جهت معکوس اصل-معکوس و 400/404)، هستهٔ قبلی با تست مستقل fake (۷ تابع) و `SELECT` read-only با `corebanking_reader`؛ خطای واقعی ۵۰۰ و سندِ بدون آرتیکل از API واقعی آزمون نشدند. جزئیات: History 18.40 (هسته) و 18.41 (HTTP).
- **Reversal — برگشت کامل واریز بدون کارمزد (2026-11-21):** مسیر `POST /api/v1/transactions/{id}/reversals` پیاده‌سازی شد؛ دامنه فقط برگشت کاملِ واریز `COMPLETED` بدون کارمزد است. برگشت، تراکنش جدید `REVERSAL` با `reverses_transaction_id`، Journal معکوس (بدهکار/بستانکار جابه‌جا نسبت به Journal اصلی) و گذار وضعیت تراکنش اصلی به `REVERSED` را در یک `UnitOfWork` اتمیک انجام می‌دهد و با قفل سطر اصلی از برگشت دوگانه جلوگیری می‌کند؛ بررسی موجودی `ledger_balance − active_holds − minimum ≥ amount` است. Build موفق؛ آزمون واقعی API روی حساب اختصاصی 5 (Customer 4): برگشت موفق 120 (تراکنش 9 معکوس 8)، Replay همان کلید، تعارض کلید، رد برگشت دوگانه، هم‌زمانی دو کلید (دقیقاً یک موفق، تراکنش 12 معکوس 10)، رد موجودی ناکافی (تراکنش 13 REJECTED و تراکنش 11 دست‌نخورده) و موارد «ناموجود»/«غیرقابل‌برگشت» همگی با SELECT تأیید شدند. **برگشت برداشت/انتقال/جزئی و برگشت واریز دارای کارمزد خارج از محدوده و آزمون‌نشده‌اند؛** کف مانده فعال، Fault Injection، Rollback/Commit نامشخص و کامپایل fakeهای تست اجرا نشده‌اند. گزارش: [REVERSAL_DEPOSIT_RESULT.md](opencode-reports/REVERSAL_DEPOSIT_RESULT.md).
- **Reversal — هستهٔ داخلی برگشت کامل برداشت (2026-11-22):** هستهٔ داخلی `ReversalService::reverseWithdrawal(...)` (نه مسیر HTTP) روی `209329a` پیاده‌سازی شد و با یک تست سرویس با fake UnitOfWork واقعاً لینک و اجرا شد (۱۵ سناریو، خروجی ۰). دامنه فقط برگشت کامل `WITHDRAWAL` در وضعیت `COMPLETED`، بدون کارمزد. **پیش‌شرط صریح صلاحیت:** فراخوانندهٔ داخلی باید تأیید صلاحیت برگشت کامل را برای همان شناسهٔ برداشت بدهد (`WithdrawalReversalAuthorization`)؛ نبود تأیید (`REVERSAL_NOT_AUTHORIZED`) یا تأیید برای شناسهٔ دیگر (`REVERSAL_AUTHORIZATION_MISMATCH`) پیش از هر ثبت مالی رد می‌شود. صلاحیت از `COMPLETED`، مانده یا تنظیمات امروز استنتاج نمی‌شود. شکل Journal اصلی POSTED (بدهکار بدهی مشتری + بستانکار دارایی تسویه، هر دو برابر مبلغ اصل، سرفصل‌های متمایز) صرفاً برای اعتبارسنجی حسابداری بررسی می‌شود و **مدرک عدم مصرف اضافه‌برداشت نیست**. سرفصل تسویه از Journal تاریخی (نه تنظیمات امروز) با `findSettlementLedgerById` (دارایی + ACTIVE + قابل‌ثبت + ارز منطبق) تأیید می‌شود. اثر مالی: معکوس Journal، قلم تراکنش `DESTINATION`، افزایش دقیق مبلغ با `applyDepositIncrement` و `applyLedgerDeltas` (بدهکار تسویه / بستانکار مشتری)؛ بدون تغییر Hold و بدون کنترل کف مانده. Replay همان کلید/اصل بدون احراز دوبارهٔ صلاحیت، conflict، رد کلید دوم، برداشت کارمزددار، Journal جهت اشتباه/چندگانه، رد بدون تأیید و رد تأیید شناسهٔ دیگر آزمون شد. **مرز HTTP در replay:** fingerprint شاخهٔ برداشت `REVERSAL_WITHDRAWAL\n` از fingerprint واریز `REVERSAL\n` متمایز است؛ بنابراین `reverse(...)` (ورودی HTTP فقط‌واریز) با همان شناسه/کلیدی که یک برگشت برداشت با آن ثبت شده، نتیجهٔ برداشت را بازپخش نمی‌کند و با مقایسهٔ fingerprint در `TransactionService` واقعی به `IDEMPOTENCY_KEY_REUSED` می‌رسد، در حالی که replay داخلی بدون تأیید مجدد و replay واریز با fingerprint تاریخی همچنان موفق‌اند. **مسیر HTTP همچنان فقط برگشت واریز را می‌پذیرد و این هسته از HTTP یا پرچم بدنه قابل‌فراخوانی نیست.** مرجع صدور تأیید در محیط تولید خارج از دامنه است. این نتیجه تأیید PostgreSQL یا رقابت قفل یا آمادگی انتشار نیست.
- **Withdrawal — آماده‌سازی fixture (2026-11-18):** پس از بازبینی Routeها و SELECT، fixture اختصاصی فقط از API ساخته شد: Customer 3، حساب‌های A=3 (`1000000000034`) و B=4 (`1000000000055`) روی Product 1، هر دو ACTIVE با رکورد ماندهٔ اولیهٔ صفر و بدون Hold مؤثر. واریزهای idempotent با کلیدهای `wd-fixture-deposit-a-20261118`/`wd-fixture-deposit-b-20261118` مانده را به ۱۵۰ و ۲۵۰ رساندند (تراکنش‌های ۶/۷، Journalهای ۵/۶ متوازن و POSTED). حساب‌های ۱ و ۲ مرحلهٔ Transfer دست‌نخورده ماندند. **آماده برای برداشت عادی: بله. آماده برای آزمون کف مانده: خیر؛** جدول‌های Rule/Override صفر هستند و هیچ API مجاز ساخت آن‌ها وجود ندارد. هیچ Withdrawal/Transfer ارسال نشد. گزارش: [WITHDRAWAL_FIXTURE_READINESS_RESULT.md](opencode-reports/WITHDRAWAL_FIXTURE_READINESS_RESULT.md).
- **Withdrawal — آزمون Rule کف مانده (2026-11-18):** سیاست `GREATEST(product_minimum, override_minimum)` بازنویسی نشد. سه سناریوی وابسته به Rule (Override پایین‌تر، Override بالاتر، مرز دقیق) **اجرا نشدند**، چون هیچ API مجاز برای ساخت `product_balance_rule` یا `account_balance_rule_override` وجود ندارد و جدول‌های Rule خالی‌اند؛ ساخت محصول/حساب از API هم Rule نمی‌سازد. Rule مؤثر فعلی هر دو حساب ۱ و ۲ صفر است، بنابراین اجرای برداشت در این وضعیت چیزی دربارهٔ کف مانده اثبات نمی‌کند. پیش‌نیاز دقیق (سه ردیف Rule پیشنهادی برای اعمال دستی کاربر و برنامهٔ آزمون مرزی) در گزارش ثبت شده است. هیچ کد تولیدی، DDL، SQL نوشتنی یا دادهٔ جدیدی ایجاد نشد. گزارش: [WITHDRAWAL_BALANCE_RULE_TEST_RESULT.md](opencode-reports/WITHDRAWAL_BALANCE_RULE_TEST_RESULT.md).
- **Transfer — گذار وضعیت INITIATED → PROCESSING → COMPLETED (2026-11-18):** مسیر موفق `TransferService` اکنون پیش از نخستین نوشتن مالی و در همان `UnitOfWork` (بدون Commit میانی) با گارد `status = 'INITIATED'` به `PROCESSING` می‌رود و گذار نهایی `COMPLETED` فقط با گارد `status = 'PROCESSING'` انجام می‌شود؛ خطای SQL یا نرسیدن به وضعیت مبدأ `Failed / TRANSACTION_PROCESSING_FAILED` می‌دهد. SQL گذار در `transferstatetransitions.h` مشترک سرویس و آزمون است. Build موفق؛ آزمون متمرکز QSQLITE گذارها و گاردها را پاس کرد؛ روی fixture ساخته‌شده فقط از API، انتقال رشته‌ای با description ۵۰۰، انتقال عددی ۸ اعشاری، replay همان کلید/payload (بدون اثر تکراری)، تعارض کلید (422) و مقصد ناموجود با مبدأ موجود (مسیر C7) اجرا شد؛ مانده‌ها، Journal متوازن و نبود اثر تکراری با SELECT تأیید شد. `ACCOUNT_BALANCE_NOT_INITIALIZED` بدون fixture مجاز اجرا نشد و State Machine کامل (C4) بسته نشد. گزارش: [TRANSFER_STATE_TRANSITION_RESULT.md](opencode-reports/TRANSFER_STATE_TRANSITION_RESULT.md).
- **Transfer — اصلاح تجمیعی C3/C5/C7/C8 (2026-11-18):** چهار اصلاح محدود در `TransferController`/`TransferService` اعمال شد: C3 گارد round-trip مبلغ عددی JSON مثل Deposit (عدد ۹ اعشاری که قالب‌بندی تغییرش می‌دهد با `422 INVALID_AMOUNT` رد می‌شود، عدد معتبر ۸ اعشاری پذیرفته می‌شود)؛ C8 سقف ۵۰۰ کاراکتر روی description نرمال‌شده پیش از `Begin` (`Rejected / INVALID_DESCRIPTION`، بدون تغییر 400 برای نوع نامعتبر)؛ C5 حذف کامل متن خام `lastError`/SQL از `TransferResult` با حفظ کدها و HTTP statusها و پیام‌های ثابت؛ C7 تفکیک `ACCOUNT_NOT_FOUND` از `ACCOUNT_BALANCE_NOT_INITIALIZED` با شناسه‌های همان Queryهای قفل موجود و بدون Query/قفل تازه. Build موفق بود. ۹ سناریوی اعتبارسنجی از طریق API محلی اجرا شد (C3، C8 و حفظ رفتار Controller تأیید شدند) و بررسی read-only نشان داد هیچ تراکنش یا نوشتن مالی ایجاد نشده است. **مسیرهای موفق، replay، `ACCOUNT_BALANCE_NOT_INITIALIZED` و خطاهای واقعی DB اجرا نشدند**، چون دیتابیس توسعه (`accounts=0, customers=0, products=0`) هیچ fixture مجازی نداشت. T1/T2 و State Machine بسته نشدند. گزارش: [TRANSFER_C3_C5_C7_C8_RESULT.md](opencode-reports/TRANSFER_C3_C5_C7_C8_RESULT.md).
- **Transfer — چرخهٔ تراکنش با UnitOfWork، T2 (2026-11-18):** مالکیت Begin/Commit/Rollback در `TransferService` به `UnitOfWork` موجود منتقل شد؛ Queryهای محلی روی همان اتصال مشترک باقی ماندند و Repository استخراج نشد. شکست Commit دیگر Rollback نمی‌شود و `CommitOutcomeUnknown` به `Failed` + `DATABASE_COMMIT_FAILED` با پیام «نتیجه نامعلوم» می‌رسد؛ Rollback ناموفق در همهٔ مسیرها، از جمله idempotency conflict و نبود حساب مبدأ، به `DATABASE_ROLLBACK_FAILED` می‌رسد. Build موفق بود، اما هیچ آزمون runtime برای این مسیرها اجرا نشد. این مرحله تکمیل Transfer یا آمادگی انتشار نیست. گزارش: [TRANSFER_TRANSACTION_LIFECYCLE_RESULT.md](opencode-reports/TRANSFER_TRANSACTION_LIFECYCLE_RESULT.md).
- **انتقال محیط و آماده‌سازی پیش از T2 (2026-11-17):** پروژه همراه با تغییرات Commit‌نشده به مسیر `/home/it4850/Projects/CoreBanking` و Build به `/home/it4850/Projects/build-CoreBanking-Desktop-Debug` منتقل شده است؛ مسیرهای جاری در Context، History و `AGENTS.md` هماهنگ شدند و مسیرهای داخل رکوردهای تاریخی History دست‌نخورده ماندند. در این مرحله Build، اجرای برنامه، آزمون API و بررسی دیتابیس اجرا نشد. گزارش‌های کاربر (Build موفق در Qt Creator، اتصال دیتابیس و شروع HTTP server روی `8080`) در بخش ۳ همین سند ثبت شده‌اند و صحت Schema، مجوزهای کامل دیتابیس، APIهای مالی یا ایمنی Commit/Rollback را اثبات نمیکنند. T1 در همان وضعیت ثبت‌شده باقی است و در آن تاریخ T2 آغاز نشده بود.

- **Transfer — اصلاح محدود حداقل مانده، T1 (2026-11-16):** طبق گزارش اجرای T1، محاسبه حداقل در `loadLockedAccounts` با سیاست مصوب `MAX(product_minimum, account_override_minimum)` هم‌تراز شد؛ مقادیر ناموجود صفر در نظر گرفته می‌شوند و Override فقط افزایش‌دهنده است. Build و ۷ آزمون expression-level موفق گزارش شده‌اند؛ Query کامل تولیدی، Ruleهای واقعی PostgreSQL و انتخاب قواعد زمان‌مند مستقیماً آزمون نشده‌اند. این مرحله Commit/Rollback را اصلاح نکرده و به معنی آمادگی انتشار Transfer نیست. گزارش: [TRANSFER_MINIMUM_POLICY_RESULT.md](opencode-reports/TRANSFER_MINIMUM_POLICY_RESULT.md).
- **Withdrawal (2026-11-16):** `POST /api/v1/withdrawals` پیاده‌سازی و با API واقعی روی حساب‌های `1` و `3` آزمون شد. Journal برداشت (بدهکار سپرده/بستانکار سرفصل تسویه)، Idempotency/Replay/Conflict، رد موجودی ناکافی با Hold، حساب CLOSED/ناموجود و مبلغ نامعتبر بررسی شدند. سیاست حداقل مانده طبق تصمیم کاربر اعمال شد: `effective_minimum = MAX(product_minimum, account_override_minimum)` و Override فقط افزایش‌دهنده است. هیچ Migration/DDL انجام نشد. **تکمیل جامع/آمادگی انتشار نیست:** Overdraft واقعی، Rule واقعی PostgreSQL، هم‌زمانی و Fault Injection آزمون نشده‌اند؛ فرمول minimum در Transfer طبق گزارش T1 هم‌تراز شده است؛ آزمون یکپارچه Query تولیدی و قواعد واقعی PostgreSQL و بازبینی ایمنی Commit/Rollback همچنان باقی است. گزارش: [WITHDRAWAL_COMPLETION_RESULT.md](opencode-reports/WITHDRAWAL_COMPLETION_RESULT.md).
- **تکمیل Deposit (2026-11-16):** یک نقص مالی واقعی در `DepositController` اصلاح شد: مبلغ عددی JSON با بیش از ۸ رقم اعشار بی‌صدا گرد می‌شد و مبلغی متفاوت ثبت می‌کرد. اکنون چنین ورودی‌ای با همان کد موجود `INVALID_AMOUNT` و HTTP `422` رد می‌شود؛ ورودی عددی معتبر (مثلاً `0.12345678`) همچنان پذیرفته می‌شود. Build و آزمون‌های هدفمند و آزمون واقعی API روی حساب‌های موجود انجام شد و یک حساب رزرو برای مرحله‌های بعد ایجاد شد. هیچ Migration/DDL یا تغییر Schema انجام نشد. گزارش: [DEPOSIT_COMPLETION_RESULT.md](opencode-reports/DEPOSIT_COMPLETION_RESULT.md).
- **طراحی مرحله Balance (2026-11-16):** مرحله‌ای پیش از تکمیل Deposit، صرفاً بررسی/طراحی بود و هیچ کد/تست/داده‌ای تغییر نکرد. گزارش: [BALANCE_DEPOSIT_NEXT_STEP_DESIGN.md](opencode-reports/BALANCE_DEPOSIT_NEXT_STEP_DESIGN.md). زیرمرحله پیشنهادی Balance (1B.2) هنوز تأیید یا اجرا نشده و Balance Slice «شروع‌شده» محسوب نمی‌شود.
- **1A:** در دامنه محدود پذیرفته شده است. جزئیات و محدودیت‌ها: [1A_ACCEPTANCE_WITH_LIMITATIONS.md](opencode-reports/1A_ACCEPTANCE_WITH_LIMITATIONS.md).
- **1A.1:** انتقال قرارداد Transaction/UoW به Application و UoW per-request انجام شده است. در زمان آن مرحله، Balance/Journal SQL و `QSqlDatabase` به‌صورت انتقالی در Deposit باقی ماندند.
- **1B.1:** استخراج ساختاری Balance از Deposit انجام شده است: قرارداد `IBalanceRepository` در Application، پیاده‌سازی `BalanceRepository` در Infrastructure و `balances()` در قرارداد/پیاده‌سازی UoW با همان Connection و تراکنش. SQLها، bindها، ترتیب Queryها/قفل‌ها، زمان مرجع و فرمول محاسبه فعلی حفظ شده‌اند. این مرحله فقط استخراج ساختاری است؛ تکمیل 1B، رفع شکاف هم‌زمانی قواعد مؤثر یا اصلاح `available_balance` را ادعا نمی‌کند.
- **1C.1:** استخراج ساختاری Journal/Ledger از Deposit انجام شده است: قرارداد `IJournalRepository` در Application، پیاده‌سازی `JournalRepository` در Infrastructure و `journals()` در قرارداد/پیاده‌سازی UoW با همان Connection و تراکنش. SQLها، bindها، ترتیب عملیات/قفل‌ها و زمان‌ها حفظ شده‌اند.
- **1C.2:** بسته نهایی استخراج persistence از Deposit انجام شده است: Account currency lookup، Account row lock و Hold row lock به `IAccountStateRepository`/`IHoldRepository` در Application و پیاده‌سازی‌های Infrastructure منتقل شدند و از `accounts()`/`holds()` همان UoW استفاده می‌کنند. `DepositService` دیگر هیچ SQL، `QSqlDatabase`، `QSqlQuery` یا include مستقیم Infrastructure ندارد. این مرحله تکمیل Domain rules، رفع race یا آمادگی عملیاتی بانکی نیست.
- **1B:** در جریان است. فقط 1B.1 انجام شده و ادامه آن نیازمند تأیید جداگانه است.
- **1C:** در جریان است. استخراج ساختاری 1C.1 و 1C.2 انجام شده؛ ادامه آن نیازمند تأیید جداگانه است.
- **Balance Slice:** شروع نشده است. شکاف هم‌زمانی قواعد مؤثر، ابهام Override و زمان مرجع باید پیش از آن تعیین تکلیف شود. پیشنهاد کوچک‌ترین گام بعدی در [BALANCE_DEPOSIT_NEXT_STEP_DESIGN.md](opencode-reports/BALANCE_DEPOSIT_NEXT_STEP_DESIGN.md) ثبت شده است.
- **Hold API:** در دامنه مصوب خود بسته شده است؛ Migration 018 اجرا شده و `CURRENT_SCHEMA.sql` به‌روز است.
- **وضعیت fail-closed 1A:** پس از `CommitOutcomeUnknown`، `RollbackFailed` یا شکست BEGIN روی اتصال باز، اتصال مشترک بسته می‌ماند و بدون بازگشایی خودکار بسته است؛ دسترسی دیتابیس سرویس تا restart متوقف می‌شود. این مصالحه موقت ایمنی است، نه معماری نهایی مدیریت اتصال. پس از T2، مسیر Transfer نیز همین قرارداد را از طریق همان `UnitOfWork` به کار می‌برد.
- **ابهام Commit:** restart یا بستن اتصال، نتیجه Commit قبلی را تعیین نمی‌کند. Retry باید با همان `idempotencyKey` انجام شود و نتیجه پایدار را از دیتابیس بازیابی کند؛ عملیات مالی خودکار با کلید جدید مجاز نیست.
- **ریسک legacy:** Transfer از T2 فقط مالکیت Begin/Commit/Rollback را به `UnitOfWork` سپرده است و Queryهای مالی محلی خود را نگه داشته؛ Repository استخراج نشده است. Hold، AccountOpening، AccountService و چند Repository همچنان مستقیماً روی Connection مشترک تراکنش مدیریت می‌کنند و خارج از استخراج persistence مسیر Deposit هستند. HoldService همچنان نویسنده مستقل Hold است.
- **مانده قابل برداشت:** `available_balance` یک Cache کهنه‌شونده است و برای تصمیم مالی جدید قابل اتکا نیست؛ فرمول مفهومی باید در مرحله Balance تعیین تکلیف شود.
- **اضافه‌برداشت:** مسیر نیازمند تسویه همچنان با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود؛ اضافه‌برداشت فعال نشده است.

### نقاط توقف نیازمند تأیید

- ادامه 1B پس از 1B.1.
- ادامه 1C پس از 1C.2.
- طراحی یا اجرای Balance Slice.
- هر Migration، DDL، تغییر Schema، تغییر Credential یا تنظیم اتصال.
- هر تغییر رفتار مالی، قرارداد API یا تصمیم معماری جدید.
- فعال‌سازی HTTP برای هر endpoint استعلامی دیگر (مثلاً سایر گزارش‌های مالی) همچنان پس از طراحی/تأیید لایهٔ کنترل دسترسی. (استثناهای تأییدشدهٔ مراحل قبلی، صرفاً read-only و برای محیط توسعهٔ محلی در نبود احراز هویت/مجوز: مسیر HTTP صورتحساب `GET /api/v1/accounts/{accountId}/statement`، استعلام تراکنش `GET /api/v1/transactions/{transactionId}`، استعلام سند `GET /api/v1/journals/{journalId}`، استعلام دفتر کل `GET /api/v1/ledger-accounts/{ledgerAccountId}/entries` و تراز آزمایشی `GET /api/v1/trial-balance?currencyId=&from=&to=`؛ این استثناها به معنای فعال‌سازی عمومی/آمادهٔ انتشار نیستند.)
- Git stage/commit/branch/push.
- Build/HTTP/SQL/Fault Injection فقط در محدوده تأییدشده هر مرحله.

### ترتیب اعتبار و مطالعه

1. تصمیم‌های کسب‌وکار تأییدشده در همین سند.
2. `database/CURRENT_SCHEMA.sql` برای ساختار واقعی دیتابیس.
3. کد جاری و Router/Controllerها برای رفتار پیاده‌سازی‌شده.
4. Migrationها برای تاریخچه ساختار.
5. `PROJECT_HISTORY.md` و گزارش‌های `docs/opencode-reports/` برای شواهد تاریخی.

در صورت اختلاف، قاعده مصوب، ساختار دیتابیس و رفتار برنامه باید جداگانه بررسی شوند. وجود قید، Trigger، Migration یا کد قدیمی مجوز تغییر تصمیم کسب‌وکار نیست.

### گزارش‌های مرتبط

- [PROJECT_HISTORY.md](PROJECT_HISTORY.md) — سیر تاریخی تصمیم‌ها، آزمون‌ها، Buildها، Migrationها و Commitها.
- [1A_ACCEPTANCE_WITH_LIMITATIONS.md](opencode-reports/1A_ACCEPTANCE_WITH_LIMITATIONS.md) — پذیرش محدود 1A و محدودیت‌ها.
- [1A_CODE_REVIEW.md](opencode-reports/1A_CODE_REVIEW.md) — بازبینی کد و ریسک‌های 1A (شاهد تاریخی، نه اجرای جدید).
- [1A_F1_F2_IMPLEMENTATION.md](opencode-reports/1A_F1_F2_IMPLEMENTATION.md) — گزارش پیاده‌سازی اصلاح F1/F2.
- [1A_F1_F2_HARNESS_EVIDENCE.md](opencode-reports/1A_F1_F2_HARNESS_EVIDENCE.md) — گزارش Harness و خروجی خام آن.
- [DOCUMENTATION_REORGANIZATION.md](opencode-reports/DOCUMENTATION_REORGANIZATION.md) — گزارش همین بازآرایی مستندات.
- [BALANCE_DEPOSIT_NEXT_STEP_DESIGN.md](opencode-reports/BALANCE_DEPOSIT_NEXT_STEP_DESIGN.md) — گزارش طراحی مرحله بعدی Balance در مسیر Deposit (بررسی/طراحی، بدون اجرا).
- [DEPOSIT_COMPLETION_RESULT.md](opencode-reports/DEPOSIT_COMPLETION_RESULT.md) — گزارش تکمیل و آزمون واقعی Deposit و اصلاح مبلغ عددی.
- [WITHDRAWAL_COMPLETION_RESULT.md](opencode-reports/WITHDRAWAL_COMPLETION_RESULT.md) — گزارش پیاده‌سازی و آزمون واقعی Withdrawal و محدودیت‌های باز.
- [TRANSFER_TRANSACTION_LIFECYCLE_RESULT.md](opencode-reports/TRANSFER_TRANSACTION_LIFECYCLE_RESULT.md) — گزارش مرحله T2: انتقال مالکیت Begin/Commit/Rollback در Transfer به `UnitOfWork`، فقط با Build و بازبینی ساختاری.
- [TRANSFER_C3_C5_C7_C8_RESULT.md](opencode-reports/TRANSFER_C3_C5_C7_C8_RESULT.md) — گزارش مرحله تجمیعی C3/C5/C7/C8: Build موفق، ۹ آزمون اعتبارسنجی API و آزمون‌های موفق/replay اجرانشده به دلیل دیتابیس خالی.
- [TRANSFER_STATE_TRANSITION_RESULT.md](opencode-reports/TRANSFER_STATE_TRANSITION_RESULT.md) — گزارش گذار وضعیت Transfer: Build، آزمون متمرکز گذار و آزمون‌های واقعی API با fixture روی حساب‌های 1 و 2.
- [WITHDRAWAL_BALANCE_RULE_TEST_RESULT.md](opencode-reports/WITHDRAWAL_BALANCE_RULE_TEST_RESULT.md) — گزارش آزمون Rule کف مانده Withdrawal: سه سناریو اجرا نشد، با پیش‌نیاز دقیق Rule و برنامهٔ آزمون مرزی.
- [WITHDRAWAL_FIXTURE_READINESS_RESULT.md](opencode-reports/WITHDRAWAL_FIXTURE_READINESS_RESULT.md) — گزارش آماده‌سازی fixture Withdrawal: حساب‌های 3/4، واریزهای 150/250 و وضعیت مسدود Rule.
- [REVERSAL_DEPOSIT_RESULT.md](opencode-reports/REVERSAL_DEPOSIT_RESULT.md) — گزارش پیاده‌سازی و آزمون واقعی برگشت کامل واریز بدون کارمزد و محدودیت‌های باز.
- گزارش‌های Hold: [HOLD_SERVICE_IMPLEMENTATION.md](opencode-reports/HOLD_SERVICE_IMPLEMENTATION.md)، [HOLD_CONTROLLER_IMPLEMENTATION.md](opencode-reports/HOLD_CONTROLLER_IMPLEMENTATION.md)، [HOLD_018_COREBANKING_TEST_PLAN.md](opencode-reports/HOLD_018_COREBANKING_TEST_PLAN.md) و [HOLD_EVENT_FINAL_CLOSURE.md](opencode-reports/HOLD_EVENT_FINAL_CLOSURE.md).

### تفکیک نقشه‌راه از وضعیت واقعی

- نقشه‌راه پروژه، **ترتیب طراحی و توسعه از صفر** را تعیین می‌کند و به‌تنهایی شاهدی بر وضعیت مخزن نیست.
- وضعیت واقعی مخزن باید مستقل و مبتنی بر شواهد گزارش شود: کد جاری، Schema، Router، و آزمون‌های واقعاً اجراشده.
- وجود Schema، Migration، Route، Repository یا enum به‌تنهایی به معنای تکمیل کل قابلیت نیست.
- تکمیل یک API زیرمجموعه به معنای تکمیل کل فاز نیست.
- Transaction Query، Account Statement، Journal Inquiry، General Ledger Inquiry و Trial Balance قابلیت‌های **متمایز** هستند و نباید با یکدیگر یا با Deposit/Transfer جمع شوند.
- در صورت اختلاف بین نقشه‌راه و وضعیت مخزن، گزارش شواهد مخزن مرجع است و اختلاف باید صریح ثبت شود.

### ماتریس تطبیق نقشه‌راه و وضعیت مخزن — 2026-11-16

این جدول از روی کد جاری، `database/CURRENT_SCHEMA.sql`، `src/web/httprouter.cpp` و شواهد ثبت‌شده در `PROJECT_HISTORY.md` تکمیل شده است. گستره فاز از تکمیل یک API زیرمجموعه نتیجه گرفته نشده است.

| # | فاز | قابلیت موجود و محدوده آن | وضعیت شواهد آزمون | شکاف باقی‌مانده | اقدام بعدی یا نیاز به تأیید |
|---|---|---|---|---|---|
| ۱ | زیرساخت و امنیت | Connection مشترک Qt SQL، UoW برای مسیر Deposit، مالکیت تراکنش در Use Case، fail-closed موقت | Harness F1/F2 در History 18.18/18.20؛ بدون آزمون QPSQL واقعی در قطع ارتباط | مدیریت مرکزی اتصال/recovery، آزمون واقعی QPSQL، ریسک مشترک مسیرهای legacy | تأیید جداگانه برای هر تغییر؛ آزمون اختلال نیازمند مجوز |
| ۲ | داده‌های پایه | Country، Province، City، Branch، Currency با CRUD طبق Router | تأیید کاربر 2026-10-28 برای Country/Currency/City/Branch؛ Province با شواهد آزمون ناکافی در سند | Province بدون شواهد آزمون ثبت‌شده | بررسی/تکمیل شواهد آزمون Province |
| ۳ | تعاریف حسابداری | Account Type، Ledger Account، Product–Ledger Mapping | تأیید کاربر برای Ledger Accounts؛ Account Type و Mapping با شواهد ناکافی | آزمون مستقیم endpointهای Account Type و Mapping | تکمیل شواهد آزمون؛ بدون بازنویسی |
| ۴ | مشتری | Customer ایجاد/خواندن با `nationalId`، Individual/Legal Entity، شماره‌گذاری ۱۰ رقمی | تأیید کاربر 2026-10-28 | شواهد تفصیلی آزمون محدود | ثبت شواهد آزمون در صورت نیاز |
| ۵ | محصول و قواعد | Product CRUD/وضعیت؛ `product_balance_rule` و `account_balance_rule_override` در Schema | تأیید کاربر برای Products؛ قواعد مانده فقط در Schema/SQL و بدون آزمون API | نبود API خواندن/نوشتن قواعد؛ ابهام Override حداقل؛ شکاف هم‌زمانی | تصمیم‌های D1–D3 گزارش طراحی Balance |
| ۶ | حساب و مالکیت | افتتاح حساب اتمیک با `account_balance` صفر، مالکیت اولیه، وضعیت/چرخه عمر | تأیید کاربر 2026-10-28 برای Opening و Status | مدیریت روابط/اختیارات چندامضایی و محدودیت عملیاتی API ندارد | فاز مستقل با طراحی جداگانه |
| ۷ | Transaction + Journal + Balance | Transaction/Journal/Ledger در Deposit؛ استخراج ساختاری 1A/1B.1/1C.1/1C.2 | API و SQL read-only در History 18.17؛ آزمون هدفمند QSQLITE در 18.21–18.23؛ بدون PostgreSQL واقعی | قاعده مؤثر مانده، زمان مرجع، Override، race و invariants ژورنال | زیرمرحله 1B.2 پیشنهادی و تصمیم‌های D1–D3 |
| ۸ | Deposit | `POST /api/v1/deposits` با Idempotency آدرس‌دهی‌شده، Journal متوازن، افزایش مانده؛ مبلغ عددی بدون گردکردن بی‌صدا | تأیید کاربر 2026-10-28؛ آزمون‌های 1A/1A.1 در History 18.17/18.20؛ تکمیل و آزمون واقعی API در 18.25 | Cache کهنه `available_balance`، حداقل مانده، هم‌ترازی legacy، نبود fixture برای محصول/سرفصل نامعتبر | ادامه در دامنه Balance با تأیید جداگانه |
| ۹ | Hold / وجه قابل استفاده / Balance Inquiry | Hold API بسته؛ محاسبه `ledger_balance - active_holds` در Hold | آزمون API Hold در History 18.14/18.15 | API خواندن مانده وجود ندارد؛ تفاوت معنای «قابل استفاده» بین Hold و Cache/Transfer | Balance Slice با تأیید جداگانه |
| ۱۰ | Limits | محدودیت عملیاتی حساب در Schema/قواعد مستند | شواهدی ثبت نشده | بدون API مدیریت، بدون اعمال اثبات‌شده در مسیرهای مالی | طراحی و تأیید مستقل |
| ۱۱ | Transfer / Withdrawal | `POST /api/v1/transfers` و `POST /api/v1/withdrawals` موجودند؛ سیاست حداقل برداشت `MAX(product, account override)` در Withdrawal اعمال شده و فرمول minimum در Transfer طبق گزارش T1 هم‌تراز شده است؛ از T2، Begin/Commit/Rollback در Transfer ساختاراً از `UnitOfWork` می‌آید؛ مسیر موفق Transfer اکنون در همان UoW از `INITIATED` به `PROCESSING` و سپس `COMPLETED` می‌رود؛ از مرحله تجمیعی C3/C5/C7/C8، مبلغ عددی JSON بدون گردکردن بی‌صدا کنترل می‌شود، description سقف ۵۰۰ نرمال‌شده دارد، پیام خطا متن خام SQL/درایور ندارد و نبود حساب از نبود ردیف مانده تفکیک می‌شود | Transfer: تأیید کاربر 2026-10-28 با شواهد ناقص؛ T1: Build و ۷ آزمون expression-level؛ T2: فقط Build موفق و بازبینی ساختاری؛ C3/C5/C7/C8: Build و ۹ آزمون اعتبارسنجی API؛ گذار وضعیت: Build، آزمون متمرکز QSQLITE و آزمون واقعی API شامل انتقال موفق/عدد/رشته/replay/تعارض/مقصد ناموجود با تأیید SELECT مانده و Journal؛ Withdrawal: آزمون واقعی API در History 18.26/18.27؛ fixture اختصاصی حساب‌های 3/4 با ماندهٔ 150/250 از API ساخته شد (History 18.34)؛ آزمون Rule کف مانده اجرا نشد (History 18.33/18.34: جدول‌های Rule خالی و API ساخت Rule وجود ندارد، پیش‌نیاز ثبت شد) | Overdraft واقعی و Rule واقعی PostgreSQL آزمون نشده؛ `ACCOUNT_BALANCE_NOT_INITIALIZED` و خطاهای واقعی DB آزمون runtime ندارند؛ State Machine کامل (C4)، هم‌زمانی و Recovery باقی است | آزمون ایزوله/مجاز برای `ACCOUNT_BALANCE_NOT_INITIALIZED` و خطاهای DB؛ اعمال Rule مجاز و اجرای سه سناریوی کف مانده Withdrawal؛ تکمیل State Machine و آزمون هم‌زمانی |
| ۱۲ | Reversal | `POST /api/v1/transactions/{id}/reversals` برای برگشت کامل واریز `COMPLETED` بدون کارمزد: تراکنش `REVERSAL` با `reverses_transaction_id`، Journal معکوس و گذار اصلی به `REVERSED`؛ کنترل برگشت دوگانه از طریق قفل سطر اصلی. هستهٔ داخلی `reverseWithdrawal` برای برگشت کامل برداشت (بدون کارمزد) پیاده شده ولی از مسیر HTTP قابل‌فراخوانی نیست؛ صلاحیت برگشت فقط با تأیید صریح فراخوانندهٔ داخلی برای همان شناسهٔ برداشت (`WithdrawalReversalAuthorization`) برقرار می‌شود و شکل Journal مدرک عدم مصرف اضافه‌برداشت نیست؛ fingerprint برداشت از واریز متمایز است تا مسیر HTTP نتیجهٔ برگشت برداشت را هم بازپخش نکند | Build موفق و آزمون واقعی API در History 18.35 (برگشت موفق، Replay، Conflict، برگشت دوگانه، هم‌زمانی، موجودی ناکافی، ناموجود، غیرقابل‌برگشت) با تأیید SELECT مانده و Journal؛ هستهٔ داخلی برگشت برداشت: Build موفق و تست سرویس لینک/اجراشده با fake UoW (۱۵ سناریو، خروجی ۰) در History 18.36 | برگشت برداشت/انتقال/جزئی و واریز دارای کارمزد از مسیر HTTP آزمون نشده؛ هستهٔ داخلی برداشت فقط با fake، بدون تأیید PostgreSQL/رقابت قفل؛ کف مانده فعال، Fault Injection و Rollback/Commit نامشخص آزمون نشده | تصمیم مستقل برای فعال‌سازی HTTP برگشت برداشت با تأیید جداگانه |
| ۱۳ | استعلام و گزارش‌گیری مالی | استعلام خواندنی یک تراکنش (`TransactionQueryService::queryById` + مسیر HTTP `GET /api/v1/transactions/{transactionId}`)، و صورتحساب خواندنی یک حساب (`AccountStatementService::query` + مسیر HTTP `GET /api/v1/accounts/{accountId}/statement?from=...&to=...`، گردش بدون ماندهٔ تاریخی) و استعلام خواندنی دفتر کل (`LedgerInquiryService::queryEntries` + مسیر HTTP `GET /api/v1/ledger-accounts/{ledgerAccountId}/entries?from=...&to=...`، صفحه‌بندی‌شده) و تراز آزمایشی خواندنی (`TrialBalanceService::queryTrialBalance` + مسیر HTTP `GET /api/v1/trial-balance?currencyId=&from=&to=`، تک‌ارزی روی POSTEDها) با خروجی فیلدهای مجاز | سه شاهد جدا در History 18.37 (تراکنش) و 18.38 (صورتحساب هسته): Build برنامه (سرویس‌ها در target اصلی کامپایل/لینک‌شده)، تست مستقل fake (۶ و ۱۴ تابع آزمون، خروجی ۰)، شواهد `SELECT` read-only با `corebanking_reader`؛ برای مسیر HTTP صورتحساب در History 18.39: Build برنامه، تست fake هسته و آزمون واقعی HTTP با curl (موفق ۵ ردیف، بازهٔ خالی، ورودی ناقص/تکراری/نامعتبر، `from >= to`، حساب ناموجود، نمایش مستقل اصل `REVERSED` و `REVERSAL`)؛ برای مسیر HTTP استعلام سند در History 18.41: Build برنامه و آزمون واقعی HTTP با curl (سندهای ۴/۷/۸ با ترتیب آرتیکل/دقت مبلغ/account null/جهت معکوس اصل-معکوس، و 400 صفر/منفی/غیرعددی/سرریز و 404 ناموجود)؛ برای مسیر HTTP استعلام تراکنش در History 18.42: Build برنامه و آزمون واقعی HTTP با curl (تراکنش‌های ۱/۴/۸/۹ با دقت مبلغ 5.12345678 و `reversesTransactionId` فقط برای REVERSAL و وضعیت `REVERSED` اصل، و 400 صفر/منفی/غیرعددی/سرریز و 404 ناموجود)؛ برای مسیر HTTP استعلام دفتر کل در History 18.43: Build برنامه، تست مستقل سرویس با fake (۱۰ تابع، خروجی ۰) و آزمون واقعی HTTP با curl (۲۰۰ با دقت 5.12345678، اصل/معکوس با جهت مخالف، بازهٔ خالی، مرزهای half-open، دو صفحهٔ `limit=1` بدون تکرار/پرش، 400ها و 404)؛ برای مسیر HTTP تراز آزمایشی در History 18.44: Build برنامه، تست مستقل سرویس با fake (۷ تابع، خروجی ۰) و آزمون واقعی HTTP با curl (۲۰۰ غیرخالی با دقت 5.12345678 و `isBalanced=true`، مرز half-open، ماندهٔ منتقل‌شده با گردش صفر، بازهٔ خالی، ماتریس 400، 404 ارز ناموجود، عدم پسرفت GL/Journal) و تطبیق شش ستون و جمع‌ها با SELECT مستقل | مسیر HTTP استعلام تراکنش فعال است (`GET /api/v1/transactions/{transactionId}` خواندنی، بدون احراز هویت/مجوز، نه آمادهٔ انتشار)؛ مسیر HTTP صورتحساب فقط برای محیط توسعهٔ محلی و بدون احراز هویت/مجوز فعال است (نه آمادهٔ انتشار)؛ Journal Inquiry فعال است (`GET /api/v1/journals/{journalId}` خواندنی، بدون احراز هویت/مجوز)؛ GL Inquiry فعال است (`GET /api/v1/ledger-accounts/{ledgerAccountId}/entries` خواندنی و صفحه‌بندی‌شده، بدون احراز هویت/مجوز)؛ Trial Balance پیاده‌سازی و آزمون HTTP پایان‌به‌پایان شد (هستهٔ داخلی + مسیر HTTP، بدون احراز هویت/مجوز)؛ ماندهٔ تاریخی (opening/closing/balanceAfter) در صورتحساب/گزارش‌های دیگر بازسازی نمی‌شود | طراحی/تأیید لایهٔ دسترسی (احراز هویت/مجوز) پیش از انتشار عمومی گزارش‌های خواندنی |
| ۱۴ | Overdraft / Fees | Overdraft در SQL محاسبه و در Transfer با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود؛ `transaction_fee` فقط در Schema | تأیید/آزمون چرخه اضافه‌برداشت وجود ندارد | چرخه کامل اعتبار/بدهی/تسویه و Fees پیاده نشده | تصمیم کسب‌وکار و فاز مستقل |
| ۱۵ | Reconciliation / پایان روز / بستن دوره | — | — | بدون Job، بدون API، بدون آزمون تطبیق جامع | طراحی و تأیید مستقل |

### وضعیت مرحله طراحی Balance (پیش از تکمیل Deposit) — 2026-11-16

- در آن مرحله صرفاً بررسی کد، طراحی و مستندسازی انجام شد؛ وضعیت فعلی Deposit در بخش «وضعیت جاری» و History بخش 18.25 ثبت شده است.
- در آن مرحله هیچ پیاده‌سازی جدید، Build، آزمون اجرایی، Migration یا تغییر داده مربوط به Deposit انجام نشد.
- زیرمرحله پیشنهادی Balance (1B.2) هنوز تأیید یا اجرا نشده است.
- Balance Slice با تولید گزارش طراحی «شروع‌شده» یا «انجام‌شده» محسوب نمی‌شود.
- ادامه 1C با این درخواست خودکار مجاز نمی‌شود.
- گزارش طراحی: [BALANCE_DEPOSIT_NEXT_STEP_DESIGN.md](opencode-reports/BALANCE_DEPOSIT_NEXT_STEP_DESIGN.md).


## 1. هدف، وضعیت و معماری جاری

Core Banking یک سامانه بانکداری هسته‌ای با backend مبتنی بر C++، Qt، PostgreSQL و HTTP/JSON است.

دامنه پروژه شامل مدیریت اطلاعات ضروری مشتری، جغرافیا و شعب، ارز، انواع حساب، محصولات بانکی، نگاشت دفتر کل، افتتاح حساب، روابط مالکیت و اختیارات، مانده‌ها، تراکنش‌ها، ثبت حسابداری، کارمزد، انتقال وجه، برگشت، اضافه‌برداشت، مسدودی وجه، محدودیت‌های عملیاتی و کنترل یکپارچگی مالی است.

پروژه در حال توسعه است و نباید آماده بهره‌برداری واقعی بانکی فرض شود. وجود Schema، Trigger، سرویس یا API به‌تنهایی به معنای تکمیل و صحت تمام قابلیت‌های مالی نیست.

### 1.1 معماری جاری

- لایه‌ها: Domain، Application/Service، Infrastructure/Repository و Web/HTTP.
- اتصال دیتابیس: یک Connection مشترک Qt SQL با نام `CoreBankingConnection` که مالک ساخت/بازکردن/بستن آن `Database` است. سرویس‌ها و Repositoryها کپی همان Connection را برای طول عمر پروسه نگه می‌دارند.
- مالکیت تراکنش: در مسیر 1A (Deposit) مالک `begin/commit/rollback` لایه Use Case از طریق `UnitOfWork` است. `TransactionService`، `TransactionRepository`، `BalanceRepository`، `JournalRepository`، `AccountStateRepository` و `HoldRepository` Commit/Rollback مستقل ندارند. از T2، `TransferService` نیز `begin/commit/rollback` را از `UnitOfWork` می‌گیرد، اما Queryهای مالی خود را مستقیماً اجرا می‌کند و از Repositoryها استفاده نمی‌کند. سایر سرویس‌های legacy هنوز مستقیماً روی Connection مشترک تراکنش مدیریت می‌کنند.
- Idempotency: Transaction با `idempotency_key` و `request_fingerprint`؛ Hold و Eventهای Hold قواعد جداگانه دارند.
- ثبت مالی: Transaction، Journal و تغییر مانده‌ها باید در یک تراکنش دیتابیس و با کنترل توازن Debit/Credit انجام شوند.
- HTTP: Router مسیرها را به Controllerها متصل می‌کند؛ خطاهای سرویس به کدهای پایدار HTTP نگاشت می‌شوند.

### 1.2 وضعیت توسعه و اعتبارسنجی

- آخرین وضعیت Git و Schema باید پیش از هر کار جدید read-only بررسی شود.
- آزمون یکپارچه‌سازی از طریق HTTP API واقعی انجام می‌شود؛ SQL مستقیم جایگزین آزمون مسیر کسب‌وکار نیست.
- نتیجه‌ای که اجرا یا مشاهده نشده، موفق اعلام نمی‌شود.
- وجود route به‌تنهایی اثبات‌کننده صحت Service/Repository یا پوشش همه سناریوها نیست.
- شرح تاریخی بازبینی 2026-10-28 و جزئیات Router/API در `PROJECT_HISTORY.md` بخش 16 نگهداری می‌شود.

## 2. منابع معتبر

- مخزن GitHub: `https://github.com/ahmadbabayi/Core-Banking`
- مسیر محلی: `/home/it4850/Projects/CoreBanking`
- شاخه اصلی: `main`
- Schema جاری: `database/CURRENT_SCHEMA.sql`
- Migrationها: `database/schema/*.sql`
- قواعد و تصمیم‌های پروژه: `docs/PROJECT_CONTEXT.md`
- تاریخچه آزمون و تغییرات: `docs/PROJECT_HISTORY.md`
- گزارش‌های تفصیلی: `docs/opencode-reports/`

### ترتیب اعتبار منابع

1. تصمیم‌های کسب‌وکار تأییدشده در این سند.
2. Schema جاری برای تشخیص ساختار واقعی دیتابیس.
3. کد جاری و قراردادهای API برای تشخیص رفتار پیاده‌سازی‌شده.
4. Migrationها برای شناخت تاریخچه ساختار.
5. History و گزارش‌ها برای شواهد تاریخی آزمون‌ها و تغییرات.

در صورت اختلاف، قاعده مصوب، ساختار دیتابیس و رفتار برنامه باید جداگانه بررسی شوند. وجود یک قید یا پیاده‌سازی قدیمی مجوز تغییر تصمیم کسب‌وکار نیست.

## 3. محیط توسعه و قواعد همکاری

محیط ثبت‌شده:

- Debian 12
- C++ و Qt 5.15.8
- GCC 12.2
- CMake 3.25
- PostgreSQL 15.x
- مسیر پروژه: `/home/it4850/Projects/CoreBanking`
- مسیر Build: `/home/it4850/Projects/build-CoreBanking-Desktop-Debug`
- دیتابیس اصلی: `corebanking`
- پورت HTTP: `8080`
- OpenCode: نسخه `2.3.0` — نسخه اعلام‌شده کاربر؛ مستقل بررسی نشده است.

این مشخصات باید در صورت بروز مسئله وابسته به محیط، با وضعیت واقعی همان زمان تطبیق داده شوند.

### گزارش انتقال محیط — 2026-11-17

- پروژه همراه با تغییرات Commit‌نشده از محیط قبلی منتقل شده است؛ موارد زیر گزارش کاربر هستند و بررسی مستقل نشده‌اند:
  - Build موفق در Qt Creator.
  - لاگ کاربر اتصال موفق دیتابیس و شروع HTTP server روی پورت `8080` را نشان می‌دهد.
  - ایجاد دیتابیس `corebanking` و نقش‌های `corebanking` و `corebanking_reader` و اجرای `CURRENT_SCHEMA.sql`.
  - اصلاح اتصال OpenCode توسط کاربر؛ تنظیمات اتصال در این مرحله تغییر داده نشد.
- در مرحله انتقال، Build، اجرای برنامه، آزمون HTTP/API، اتصال به دیتابیس و بررسی داده اجرا نشد.
- این گزارش‌ها تأیید صحت Schema، مجوزهای کامل دیتابیس، درستی APIهای مالی یا ایمنی Commit/Rollback نیستند.
- وضعیت T1 و محدودیت‌های ثبت‌شده آن بدون تغییر باقی است و T2 آغاز نشده است.

### اتصال به PostgreSQL

روش اتصال ثبت‌شده:

```bash
psql -U corebanking -h localhost -d corebanking -W
```

کاربر دیتابیس `corebanking` با کاربر سیستم‌عامل `ahmad` متفاوت است. استفاده از `psql -d corebanking` بدون تعیین کاربر ممکن است با خطای `role "ahmad" does not exist` روبه‌رو شود.

### انتخاب دیتابیس

در روند عادی، از دیتابیس اصلی `corebanking` استفاده شود. کاربر استفاده از دیتابیس `corebanking_test` را به‌عنوان روند معمول نمی‌خواهد و ترجیح می‌دهد پیش از تغییرات مهم از دیتابیس اصلی نسخه پشتیبان تهیه شود.

پیش از تغییر Schema:

1. وضعیت Git و Schema واقعی بررسی شود.
2. از دیتابیس اصلی نسخه پشتیبان تهیه و موفقیت آن بررسی شود.
3. Migration افزایشی و سازگار طراحی شود.
4. تغییر اجرا و آزمون شود.
5. پس از تأیید، `database/CURRENT_SCHEMA.sql` دوباره تولید و بازبینی شود.

برای تولید snapshot:

```bash
pg_dump -U corebanking -h localhost -d corebanking -W \
  --schema-only --no-owner --no-privileges \
  --file=database/CURRENT_SCHEMA.sql
```

`CURRENT_SCHEMA.sql` نمای ساختار جاری دیتابیس است، نه Migration؛ آن را مستقیماً برای ارتقای دیتابیس موجود اجرا نکنید.

برای انتخاب دیتابیس برنامه، روش ترجیحی فعلی تغییر نام دیتابیس در `src/infrastructure/database.cpp` است. بدون درخواست صریح، انتخاب دیتابیس مبتنی بر متغیرهای محیطی اضافه نشود.

### قواعد توسعه

- ابتدا کد واقعی، Schema و وضعیت Git بررسی شوند.
- فایل‌ها برای جایگزینی به‌صورت کامل ارائه شوند، نه قطعه‌کدهای پراکنده.
- آزمون یکپارچه‌سازی از طریق HTTP API واقعی انجام شود؛ SQL مستقیم جایگزین آزمون مسیر کسب‌وکار نیست.
- آزمون‌های موقت و حجیم به `main.cpp` برگردانده نشوند.
- Build، تست HTTP، کنترل نتایج مالی و وضعیت Git جداگانه گزارش شوند.
- نتیجه‌ای که اجرا یا مشاهده نشده، موفق اعلام نشود.
- پیش از Commit، Build و آزمون‌های لازم باید موفق باشند.
- Commit و Push فقط پس از تأیید کاربر انجام شوند.
- تغییرات کد، Schema و مستندات در Commitهای جداگانه نگهداری شوند، مگر اینکه کاربر صریحاً تصمیم دیگری بگیرد.
- مستندات بدون درخواست صریح کاربر به‌روزرسانی یا Commit نشوند.

## 4. مشتری

- سامانه Customer Management از Core Banking جدا است.
- Core Banking باید اطلاعات ضروری مشتری را برای عملیات بانکی عادی نگهداری کند؛ عملیات عادی نباید به استعلام لحظه‌ای از سامانه مدیریت مشتری وابسته باشد.
- شناسه داخلی مشتری `BIGINT` است، نه UUID.
- شماره مشتری ۱۰ رقمی و یکتا است.
- انواع مشتری `INDIVIDUAL` و `LEGAL_ENTITY` هستند.
- اطلاعات شخص حقیقی و حقوقی در جداول جداگانه نگهداری می‌شوند.
- `nationality_code` سه‌رقمی و مرتبط با `country(code)` است.
- وضعیت‌های مشتری: `ACTIVE`، `INACTIVE`، `BLOCKED` و `CLOSED`.
- مالکیت مشترک، رابطه‌ای در سطح حساب است؛ نوع جداگانه‌ای از مشتری نیست.

## 5. جغرافیا، شعب و ارز

- کد کشور سه‌رقمی است.
- کد شهر در کل جدول شهرها یکتا است.
- کد شعبه هفت‌رقمی است: سه رقم کد بانک و چهار رقم کد شعبه.
- ارز دارای کد الفبایی سه‌حرفی و کد عددی سه‌رقمی است.
- حساب، محصول و سرفصل دفتر کل باید از نظر ارز سازگار باشند.

قواعد تولید کدها باید در یک محل معتبر پیاده‌سازی شوند. طول و یکتایی داده‌ها در Schema و قواعد معناشناختی آن‌ها در منطق کسب‌وکار کنترل می‌شوند.

## 6. نوع حساب، محصول و شماره حساب

### کد انواع حساب

| کد | نوع |
|---|---|
| `01` | جاری |
| `02` | کوتاه‌مدت |
| `03` | پس‌انداز |
| `04` | بلندمدت |
| `21` | دولتی |
| `31` | شهرداری |
| `41` | مدارس و سازمان‌های دولتی |

این کدها بخشی از قرارداد شماره‌گذاری حساب هستند. تغییر آن‌ها مستلزم بررسی داده‌های موجود، تولید شماره، اعتبارسنجی و سازگاری نسخه‌هاست.

### تفاوت نوع حساب و محصول

- `Account Type` نوع حساب را مشخص می‌کند.
- `Product` شرایط و قواعد بانکی حساب را تعریف می‌کند.
- `Ledger Account` برای ثبت آثار مالی در دفتر کل استفاده می‌شود.

هر محصول به یک نوع حساب متصل است. ارز محصول باید با ارز نوع حساب سازگار باشد. محصولات مختلف می‌توانند به یک نوع حساب متصل شوند، اما شرایط مستقل داشته باشند.

محصول دارای کد یکتا، نام، نوع حساب، ارز، وضعیت و بازه اعتبار است. تاریخ پایان می‌تواند خالی باشد؛ در صورت وجود، نباید پیش از تاریخ شروع قرار گیرد.

### شماره حساب

شماره حساب ۱۳ رقمی است:

1. دو رقم نخست: کد نوع حساب.
2. ده رقم بعدی: سریال حساب که برای هر نوع حساب مستقل است.
3. رقم آخر: رقم کنترلی.

برای تولید و اعتبارسنجی، از `AccountNumberGenerator` موجود استفاده شود. الگوریتم نباید حدس زده یا به چند پیاده‌سازی ناسازگار تقسیم شود.

شماره حساب عمومی با شناسه داخلی حساب در دیتابیس یکسان نیست.

### افتتاح حساب

افتتاح حساب باید محصول، نوع حساب، ارز، شعبه، مشتریان مالک، نگاشت معتبر محصول به سرفصل و سرفصل دفتر کل را کنترل کند.

ایجاد حساب، روابط مالکیت و مانده اولیه صفر باید اتمیک باشد. در صورت شکست هر مرحله، تراکنش باید Rollback شود.

برای افتتاح حساب با مانده صفر، Journal ساختگی ایجاد نمی‌شود. واریز اولیه، در صورت نیاز، باید یک عملیات مالی مستقل و معتبر باشد.

## 7. مالکیت، نمایندگی و اختیارات حساب

مدل رابطه حساب باید نیازهای زیر را پوشش دهد:

- مالکیت مشترک.
- وکالت و نمایندگی.
- سرپرستی قانونی افراد صغیر.
- نمایندگی اشخاص حقوقی.
- اعضای هیئت‌مدیره و سهام‌داران.
- امضاداران مجاز و قواعد تعداد امضا.

انواع ثبت‌شده مجوز حساب شامل مواردی مانند `POWER_OF_ATTORNEY`، `LEGAL_GUARDIAN`، `LEGAL_REPRESENTATIVE` و `AUTHORIZED_SIGNATORY` هستند؛ برای نام‌ها و محدودیت‌های دقیق، Schema جاری مرجع است.

در API فعلی افتتاح حساب، فیلد `ownerCustomerIds` برای مشتریان مالک استفاده می‌شود. وجود این قابلیت به معنای کامل‌بودن ارزیابی وکالت، نمایندگی حقوقی یا چندامضایی نیست.

اعتبار زمانی اختیار، نوع عملیات، سطح دسترسی و تعداد امضاهای لازم باید در سرویس مربوط کنترل شود.

## 8. حساب‌های دفتر کل و نگاشت محصول

### حساب دفتر کل

کد سرفصل دفتر کل دقیقاً هشت‌رقمی است.

دسته‌بندی‌ها:

- `ASSET`
- `LIABILITY`
- `EQUITY`
- `REVENUE`
- `EXPENSE`

وضعیت‌های سرفصل `ACTIVE` و `INACTIVE` هستند. سرفصل می‌تواند سرفصل والد داشته باشد.

نمونه‌های تاریخی:

- `10100101`: سرفصل دارایی مرتبط با تسویه یا وجه نقد.
- `20100101`: سرفصل بدهی سپرده مشتری.

این نمونه‌ها نباید جایگزین بررسی تنظیمات جاری شوند.

### نگاشت محصول به دفتر کل

نگاشت محصول به سرفصل دفتر کل زمان‌مند است. هنگام افتتاح حساب، نگاشت معتبر در زمان مربوط باید پیدا شود. حساب، مطابق Schema، شناسه نگاشت انتخاب‌شده و شناسه سرفصل مرتبط را نگهداری می‌کند.

- شناسه سرفصل نباید در منطق عمومی عملیات مالی هاردکد شود.
- بازه اعتبار باید در انتخاب نگاشت رعایت شود.
- تغییر تنظیمات جدید نباید تاریخچه نگاشت‌های قبلی را بازنویسی کند.
- قیدهای هم‌پوشانی باید مطابق Schema واقعی رعایت شوند.
- سرفصل غیرفعال نباید بدون مجوز قواعد مربوط برای ثبت جدید انتخاب شود.

## 9. مانده حساب و قواعد مالی

### مانده سپرده مشتری

`ledger_balance` مانده ثبت‌شده سپرده مشتری است و طبق تصمیم کسب‌وکار نباید منفی شود.

Schema جاری ستون `ledger_balance` را در `account_balance` نگهداری می‌کند و قید نامنفی‌بودن دارد.

بدهی ناشی از اضافه‌برداشت نباید با منفی‌کردن مانده سپرده مشتری شبیه‌سازی شود.

### مانده قابل برداشت

`available_balance` تحت تأثیر مانده سپرده، مسدودی‌های فعال و اعتبار باقی‌مانده اضافه‌برداشت است.

فرمول مفهومی:

`available_balance = ledger_balance - active_holds + remaining_overdraft_limit`

در این رابطه:

- `ledger_balance`: مانده سپرده ثبت‌شده.
- `active_holds`: مجموع مبالغ مسدود فعال در زمان محاسبه.
- `remaining_overdraft_limit`: اعتبار مجاز اضافه‌برداشت که هنوز مصرف نشده است.

این فرمول باید با حداقل مانده، قواعد فعال محصول، Override معتبر حساب، وضعیت حساب و سیاست اضافه‌برداشت سازگار باشد.

Schema جاری `available_balance` را ذخیره می‌کند و فرمول فوق به‌صورت ستون محاسباتی یا قید پایگاه داده تضمین نشده است. بنابراین ذخیره این ستون به‌تنهایی تضمین‌کننده صحت آن نیست. `available_balance` یک Cache کهنه‌شونده است و برای تصمیم مالی جدید نباید بدون بازمحاسبه معتبر مبنا قرار گیرد.

موجودی قابل برداشت می‌تواند منفی باشد؛ این موضوع با منفی‌شدن مانده سپرده متفاوت است.

### قواعد مانده محصول

ساختارهای اصلی:

- `product_balance_rule`
- `account_balance_rule_override`

قواعد حداقل مانده و اضافه‌برداشت مستقل‌اند. اضافه‌برداشت تنها در صورت مجازبودن توسط قواعد معتبر محصول و حساب، و در محدوده سقف آن، قابل استفاده است.

- بازه اعتبار قواعد باید رعایت شود.
- قواعد زمان‌مند نباید با ویرایش گذشته بازنویسی شوند.
- Override حساب نباید ممنوعیت پایه محصول برای اضافه‌برداشت را دور بزند.
- **حداقل مانده (تصمیم قطعی 2026-11-16):** حداقل محصول کف الزام‌آور برداشت است و Override حساب فقط می‌تواند حداقل را افزایش دهد: `effective_minimum = MAX(product_minimum, account_override_minimum)`. `LEAST` برای این سیاست استفاده نمی‌شود. نبود Override حداقل محصول را حذف نمی‌کند؛ نبود Rule محصول پایه صفر دارد و Override مؤثر مثبت اعمال می‌شود. کاهش حداقل با معافیت مستقل خارج از دامنه است. فرمول minimum در Transfer طبق گزارش T1 هم‌تراز شده است؛ آزمون یکپارچه Query تولیدی و قواعد واقعی PostgreSQL و بازبینی ایمنی Commit/Rollback همچنان باقی است.
- نباید ادعا شود تمام بازه‌های Override با یک قید واحد از هم‌پوشانی منع شده‌اند؛ محدودیت واقعی به ستون‌ها و قیود تعریف‌شده در Schema بستگی دارد.

**شکاف باز پیش از Balance Slice:** `FOR SHARE` فقط ردیف موجود را قفل می‌کند. اگر Override یا قاعده مؤثر موجود نباشد، Exclusion Constraint به‌تنهایی مانع درج قاعده جدید مؤثر نمی‌شود. این شکاف هم‌زمانی پیش از Balance Slice باید تعیین تکلیف شود.

### سیاست اضافه‌برداشت

اضافه‌برداشت در این پروژه به معنای مجوز استفاده از اعتبار است، نه اجازه منفی‌شدن مانده سپرده.

بدهی مشتری و مطالبات بانک ناشی از اضافه‌برداشت باید جداگانه ثبت و حسابداری شوند. نمای کلی وضعیت مشتری می‌تواند شامل مانده سپرده، بدهی اضافه‌برداشت، مبلغ استفاده‌شده، اعتبار باقی‌مانده و مانده قابل برداشت باشد؛ اما این اعداد نباید با یکدیگر اشتباه شوند.

چرخه زیر باید مستقل پیاده‌سازی و آزمون شود:

- تعیین و کنترل سقف.
- ثبت مصرف اعتبار.
- ایجاد بدهی مشتری و مطالبات بانک.
- محاسبه اعتبار باقی‌مانده.
- بازپرداخت و تسویه.
- برگشت تراکنش مربوط به اضافه‌برداشت.

طبق آخرین وضعیت ثبت‌شده، انتقالی که به تسویه اضافه‌برداشت نیاز داشته باشد با خطای `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود. این مسیر تا پیاده‌سازی و آزمون کامل نباید قابلیت تکمیل‌شده محسوب شود. فعال‌سازی اضافه‌برداشت بدون تصمیم و آزمون مستقل مجاز نیست.

### مانده دفتر کل

قرارداد حسابداری پروژه:

`balance = debit_total - credit_total`

در این قرارداد، مانده حساب‌های بدهی که ماهیت بستانکار دارند ممکن است منفی نشان داده شود. این امر با ممنوعیت منفی‌شدن مانده سپرده مشتری تناقض ندارد؛ این‌ها دو سطح مالی متفاوت‌اند.

`ledger_account_balance` مجموع بدهکار، مجموع بستانکار و مانده را نگهداری می‌کند. Schema به‌تنهایی تضمین نمی‌کند که مقدار `balance` همواره برابر اختلاف دو مجموع باشد؛ منطق ثبت، کنترل سازگاری و آزمون نیز باید این ناوردا را حفظ کنند.

## 10. تراکنش، Journal و ثبت حسابداری

### وضعیت‌های تراکنش

وضعیت‌های تعریف‌شده در مدل کسب‌وکار:

- `INITIATED`
- `PROCESSING`
- `COMPLETED`
- `REJECTED`
- `FAILED`
- `CANCELLED`
- `REVERSED`

چرخه عمر:

- `INITIATED` می‌تواند به `PROCESSING` یا `CANCELLED` برود.
- `PROCESSING` می‌تواند به وضعیت نهایی مناسب، از جمله `COMPLETED`، `REJECTED`، `FAILED` یا `CANCELLED` برود.
- `COMPLETED` فقط پس از ثبت موفق عملیات معکوس می‌تواند `REVERSED` شود.
- وضعیت نهایی نباید دوباره وارد چرخه اجرای عادی شود.
- `REJECTED` به معنای رد کسب‌وکاری، `FAILED` به معنای خطای فنی و `CANCELLED` به معنای لغو است.
- علت رد یا شکست باید ساخت‌یافته ثبت شود.

نام وضعیت‌ها در مدل هدف باید با Schema و کد جاری تطبیق داده شود؛ تغییر مستندات به‌تنهایی وضعیت جدیدی به دیتابیس اضافه نمی‌کند.

### چرخه عمر Journal

- `DRAFT` به `POSTED` پس از اعتبارسنجی و ثبت اتمیک می‌رود.
- `POSTED` فقط از مسیر برگشت معتبر می‌تواند به وضعیت برگشتی منتقل شود.
- Journal ثبت‌شده نباید به `DRAFT` بازگردانده شود.
- مجموع بدهکار و بستانکار هر سند باید برابر باشد.
- ثبت Journal، تغییر مانده‌ها و تکمیل تراکنش کسب‌وکاری باید در یک تراکنش دیتابیس انجام شوند.

### اصول حسابداری عملیات

- واریز نقدی: بدهکار صندوق، بستانکار بدهی سپرده مشتری.
- برداشت نقدی: بدهکار بدهی سپرده مشتری، بستانکار صندوق.
- انتقال داخلی: بدهکار بدهی سپرده حساب مبدأ، بستانکار بدهی سپرده حساب مقصد.
- کارمزد: بدهکار بدهی سپرده مشتری، بستانکار درآمد کارمزد.
- استفاده از اضافه‌برداشت: بدهکار مطالبات بانک، بستانکار بدهی سپرده مشتری.

ثبت واقعی اضافه‌برداشت تنها پس از تکمیل و آزمون چرخه مربوط مجاز است.

کارمزدهای وابسته باید در همان عملیات مالی اتمیک ثبت شوند. سرفصل کارمزد باید از تنظیمات معتبر انتخاب شود، نه شناسه ثابت در کد.

## 11. انتقال وجه، برگشت و ارز

انتقال داخلی در مرحله فعلی باید بین حساب‌های هم‌ارز انجام شود. تبدیل ارز نیازمند مسیر مستقل FX، قواعد نرخ تبدیل، گردکردن و ثبت‌های حسابداری مربوط است.

در انتقال:

- حساب مبدأ و مقصد، وضعیت، ارز و محدودیت‌های آن‌ها بررسی شوند.
- قواعد موجودی، مسدودی، حداقل مانده و اضافه‌برداشت رعایت شوند.
- وجه قابل برداشت باید برای اصل مبلغ و کارمزد کافی باشد.
- در صورت رد یا شکست، ثبت مالی ناقص مجاز نیست.

برگشت تراکنش، یک تراکنش جدید از نوع `REVERSAL` همراه با Journal معکوس است؛ تراکنش اصلی نباید حذف یا بازنویسی شود.

برای هر تراکنش اصلی حداکثر یک برگشت موفق مجاز است. کنترل هم‌زمانی باید از برگشت دوگانه جلوگیری کند. وضعیت `REVERSED` فقط پس از موفقیت ثبت معکوس اعمال می‌شود.

برگشت باید آثار جانبی مربوط به مسدودی، کارمزد، محدودیت‌ها و بدهی اضافه‌برداشت را نیز در نظر بگیرد.

## 12. Idempotency و هم‌زمانی

تکرار درخواست مالی، برای مثال در اثر Timeout شبکه، نباید عملیات مالی را دوباره اجرا کند.

فیلدهای ثبت‌شده در تراکنش:

- `idempotency_key`
- `request_fingerprint`
- `result_code`
- `result_message`

نام شاخص ثبت‌شده:

- `uq_transaction_api_idempotency`

قرارداد درخواست تکراری:

- همان کلید و همان محتوا باید نتیجه عملیات اولیه را بازگرداند.
- همان کلید با محتوای متفاوت باید رد شود.
- نتیجه نهایی عملیات باید قابل بازیابی باشد.
- کنترل تکرار باید در برابر درخواست‌های هم‌زمان نیز درست عمل کند.

وجود فیلد و شاخص به‌تنهایی رفتار کامل Idempotency را تضمین نمی‌کند؛ کد سرویس و آزمون‌های مناسب نیز لازم‌اند.

برای رویدادهای آزادسازی مسدودی، اگر `effective_at` در تشخیص تکرار یکسان نقش دارد، Retry باید همان زمان مؤثر درخواست اصلی را حفظ کند.

آزمون هم‌زمانی باید علاوه بر پاسخ HTTP، مانده نهایی حساب‌ها، تعداد تراکنش‌ها و Journalهای ایجادشده را بررسی کند.

**قاعده 1A برای نتیجه نامعلوم:** بستن اتصال یا restart نتیجه Commit قبلی را تعیین نمی‌کند. Retry باید با همان `idempotencyKey` انجام شود و نتیجه پایدار را از دیتابیس بازیابی کند؛ عملیات مالی خودکار با کلید جدید مجاز نیست.

## 13. مسدودی وجه (Hold)

ساختارهای ثبت‌شده:

- `account_hold`
- `account_hold_events`
- `account_hold_status`

مسدودی وجه مانع استفاده از بخشی از موجودی می‌شود و به‌خودی‌خود نباید `ledger_balance` را تغییر دهد.

وضعیت جاری مسدودی از اطلاعات اصلی آن، رویدادهای آزادسازی و زمان مؤثر به‌دست می‌آید. مبلغ فعال باید حتی پس از رسیدن زمان انقضا، بدون وابستگی به اجرای به‌موقع Job، از محاسبه مسدودی حذف شود.

اطلاعات اصلی مسدودی و رویدادهای ثبت‌شده باید تاریخچه قابل اتکا داشته باشند.

انواع ثبت‌شده آزادسازی:

- `PARTIAL_RELEASE`
- `FULL_RELEASE`
- `EXPIRY_RELEASE`
- `CANCELLATION_RELEASE`

قواعد:

- مجموع آزادسازی از مبلغ اولیه بیشتر نشود.
- آزادسازی جزئی، فقط مبلغ باقی‌مانده را کاهش دهد.
- آزادسازی کامل، انقضا و لغو از یکدیگر قابل تشخیص باشند.
- درخواست آزادسازی کلید Idempotency داشته باشد.
- کلید یکسان با محتوای متفاوت رد شود.
- آزادسازی هم‌زمان روی یک مسدودی سریالی شود.
- رویدادهای تاریخی آزادسازی تغییرناپذیر باشند.
- بستن حساب دارای مسدودی حل‌نشده طبق قواعد کسب‌وکار کنترل شود.

### قواعد جاری پیاده‌سازی Hold

- `HoldService` در `src/application/holdservice.*` شامل `createHold`، `createEvent`، `getHoldById`، `getHoldsByAccount` و `getHoldEvents` است.
- ترتیب قفل: Account → Account Balance → Account Hold؛ هم‌راستا با Deposit و Transfer.
- `AvailableForHold = ledger_balance - active holds` است و از اضافه‌برداشت برای Hold استفاده نمی‌شود.
- Hold و Event هیچ تغییری در `ledger_balance` یا `available_balance` نمی‌دهند.
- Idempotency Hold روی `account_hold` با `idempotency_key` و `request_fingerprint` و ایندکس `uq_hold_api_idempotency` انجام می‌شود.
- Idempotency Event روی `(hold_id, idempotency_key)` است و پیش از اعتبارسنجی وضعیت/مبلغ بررسی می‌شود؛ بنابراین Retry پس از `FULL_RELEASE` نیز نتیجه قبلی را برمی‌گرداند.
- زمان پیش‌فرض از `CURRENT_TIMESTAMP` دیتابیس داخل همان تراکنش خوانده و به میلی‌ثانیه Truncate می‌شود؛ زمان پیش‌فرض سرور وارد Fingerprint نمی‌شود.
- `HoldResult` و `HoldEventResult` فیلد `replayed` و Snapshot دارند؛ Snapshot پیش از Commit و داخل همان تراکنش خوانده می‌شود.
- مسیرهای Replay، Conflict، Unique Conflict با Savepoint و Trigger RETURN NULL حفظ شده‌اند. شکست `ROLLBACK TO SAVEPOINT` باعث پایان امن تراکنش می‌شود.
- اگر Trigger درج رویداد کاملاً تکراری را با `RETURN NULL` متوقف کند، سرویس باید رویداد قبلی را با `(hold_id, idempotency_key)` بازیابی و نتیجه قبلی را برگرداند.
- نگاشت خطا با SQLSTATE، `databaseText()`/`text()` و نام Constraint/Index انجام می‌شود؛ خطاهای Trigger اعتبارسنجی به `Rejected` و Immutability/ناشناخته به `Failed` نگاشت می‌شوند.
- Canonicalizer نسخه ۱ در `src/application/holdeventfingerprint.h` شامل Trim با Unicode White_Space، UTF-8 byte length، Canonical Amount، Timestamp به UTC با دقت میلی‌ثانیه و Payload با Length-Prefix است.
- `request_fingerprint`، `fingerprint_version = 1` و `effective_at_provided` در Event ذخیره می‌شوند. Metadata ناقص و Version ناشناخته Fail-Closed هستند؛ Legacy Fallback با مقایسه فیلد‌به‌فیلد حفظ شده است.
- Timestamp صریح باید Timezone داشته باشد؛ Timestamp بدون Timezone یا نامعتبر رد می‌شود. U+0000 در ورودی‌های متنی Hold و Event رد می‌شود.
- تفکیک `IDEMPOTENCY_CONFLICT` و `IDEMPOTENCY_KEY_REUSED` مطابق رفتار ثبت‌شده حفظ می‌شود.
- Hold API در دامنه مصوب خود بسته است. آزمون SQL مستقیم بخش ۸ برنامه آزمون Hold، آزمون بار، مرزهای اضافه و رویه Restore/Rollback اجرا نشده‌اند و موفق اعلام نمی‌شوند.

شرح تفصیلی اجراها، Buildها، آزمون‌های API، داده‌های آزمایشی و محدودیت‌های تاریخی در `PROJECT_HISTORY.md` بخش‌های 18.7 تا 18.16 و گزارش‌های Hold در `docs/opencode-reports/` نگهداری می‌شود.

## 14. محدودیت‌های عملیاتی حساب

محدودیت عملیاتی با مسدودی وجه یکی نیست.

محدودیت عملیاتی نوع عملیات مجاز را کنترل می‌کند؛ مسدودی وجه مبلغ مشخصی را از دسترس خارج می‌کند.

- دستورهای عدم واریز، عدم برداشت و عدم انجام هر دو باید از هم قابل تفکیک باشند.
- محدودیت‌های مستقل می‌توانند هم‌زمان وجود داشته باشند.
- لغو یک محدودیت نباید سایر محدودیت‌ها را لغو کند.
- محدودیت‌ها باید با توجه به ماهیت عملیات اعمال شوند، نه صرفاً کانال یا درخواست‌کننده.
- انتقال باید محدودیت مبدأ و مقصد را مطابق نقش آن‌ها بررسی کند.
- کارمزد و برداشت خودکار نیز باید تابع قواعد مرتبط باشند.
- برگشت و اصلاح نباید راه دورزدن محدودیت فراهم کنند.
- بستن حساب دارای دستور حل‌نشده باید طبق قواعد مربوط کنترل شود.

کامل‌بودن این بخش باید با بررسی Schema، سرویس‌ها و آزمون واقعی تأیید شود.

## 15. وضعیت حساب

وضعیت‌های حساب:

- `ACTIVE`
- `DORMANT`
- `BLOCKED`
- `CLOSED`

انتقال وضعیت باید در سرویس حساب اعتبارسنجی شود. مقدار وضعیت معتبر به‌تنهایی به معنای مجازبودن انتقال از وضعیت فعلی نیست.

برای بستن حساب باید وضعیت مانده، مسدودی‌ها و وابستگی‌های مالی بررسی شود. بستن حساب نباید اضافه‌برداشت جدید ایجاد یا بدهی موجود را پاک کند. حداقل مانده نیز نباید مانع برداشت وجوه مثبت در چارچوب مجازِ بستن حساب شود.

پس از بسته‌شدن حساب، فعال‌سازی مجدد باید تابع قواعد چرخه عمر باشد.

## 16. قراردادهای API و وضعیت تکمیل

### 16.1 APIهای تکمیل و تست‌شده — تأیید کاربر در 2026-10-28

موارد زیر به‌طور صریح از سوی کاربر به‌عنوان پیاده‌سازی‌شده و تست‌شده تأیید شده‌اند. جزئیات مسیرها از `src/web/httprouter.cpp` استخراج شده‌اند. این فهرست به معنی تأیید همه سناریوهای هم‌زمانی، خطا، Rollback یا آمادگی بهره‌برداری بانکی نیست.

| گروه API | مسیرهای ثبت‌شده در Router | وضعیت |
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
| Withdrawals | `POST /api/v1/withdrawals` | پیاده‌سازی‌شده و آزمون واقعی API در دامنه محدود؛ وابسته به تصمیم قواعد حداقل/Override و فاقد آزمون Overdraft واقعی |
| Internal Transfers | `POST /api/v1/transfers` | تکمیل و تست‌شده |
| Reversals (برگشت واریز) | `POST /api/v1/transactions/{id}/reversals` | پیاده‌سازی‌شده و آزمون واقعی API در دامنه محدود (فقط برگشت کامل واریز بدون کارمزد)؛ برگشت برداشت/انتقال/جزئی و واریز دارای کارمزد خارج از دامنه. هستهٔ داخلی برگشت کامل برداشت در `ReversalService::reverseWithdrawal` با پیش‌شرط صریح تأیید صلاحیت از فراخوانندهٔ داخلی پیاده شده، اما از این مسیر HTTP قابل‌فراخوانی نیست و مسیر HTTP همچنان فقط واریز را می‌پذیرد |

**محدودیت شواهد:** History حاوی گزارش جزئی برخی تست‌ها است، اما خروجی کامل همه تست‌های مورد تأیید کاربر، مخصوصاً گروه‌های جغرافیا و شعب و آخرین تست Transfer، در این فایل ثبت نشده است. هنگام اجرای دوباره تست‌ها، تاریخ، SHA، دیتابیس، درخواست، پاسخ و کنترل مانده/Journal به History اضافه شود. از این کمبود مستندات نباید نتیجه گرفت APIها تست نشده‌اند؛ وضعیت تکمیل و تست آن‌ها طبق تأیید کاربر پذیرفته شده است.

### 16.1.1 Hold API

| گروه API | مسیرهای ثبت‌شده در Router | وضعیت |
|---|---|---|
| Holds | `POST /api/v1/holds`؛ `GET /api/v1/holds/{id}`؛ `GET /api/v1/holds?accountId={id}`؛ `POST /api/v1/holds/{id}/events`؛ `GET /api/v1/holds/{id}/events` | پیاده‌سازی و آزمون API در دامنه مصوب؛ بسته‌شده |

جزئیات آزمون‌های Hold، هم‌زمانی، Legacy Retry، Expiry/Cancellation و داده‌های آزمایشی در `PROJECT_HISTORY.md` بخش‌های 18.14 و 18.15 و `opencode-reports/HOLD_018_COREBANKING_TEST_PLAN.md` نگهداری می‌شود.

### 16.2 مسیرهای موجود در Router که تست مستقیمشان در History ثبت نشده است

این مسیرها در Router متصل به Controller هستند، اما سند تاریخچه فعلی شواهد کافی از اجرای تست HTTP مستقیم و کامل آن‌ها ندارد. بنابراین آن‌ها را «API مفقود» نمی‌دانیم؛ فقط لازم است شواهد آزمونشان ثبت یا تأیید شود.

**Provinces**

- `POST /api/v1/provinces`
- `GET /api/v1/provinces`
- `GET /api/v1/provinces/{id}`

**Account Types**

- `POST /api/v1/account-types`
- `GET /api/v1/account-types` با فیلترهای `status` و `currencyId`
- `GET /api/v1/account-types/{id}`
- `PATCH /api/v1/account-types/{id}/status`

**Product–Ledger Mappings**

- `POST /api/v1/product-ledger-mappings`
- `GET /api/v1/product-ledger-mappings` با فیلترهای `status`، `productId` و `ledgerAccountId`
- `GET /api/v1/product-ledger-mappings/{id}`
- `PATCH /api/v1/product-ledger-mappings/{id}/status`
- `PATCH /api/v1/product-ledger-mappings/{id}/effective-to`

History استفاده از یک نگاشت فعال در آزمون افتتاح حساب را ثبت می‌کند؛ این موضوع به‌تنهایی همه endpointهای مدیریت نگاشت را پوشش نمی‌دهد.

### 16.3 فیلترهای APIهای موجود

- Products: `status`، `accountTypeId`، `currencyId`.
- Ledger Accounts: `status`، `category`، `currencyId`.
- Account Types: `status`، `currencyId`.
- Product–Ledger Mappings: `status`، `productId`، `ledgerAccountId`.
- Customers: پارامتر `nationalId` برای مسیر `GET /api/v1/customers` اجباری است.
- Holds: پارامتر `accountId` برای مسیر `GET /api/v1/holds` اجباری است.

فیلدهای شناخته‌شده ایجاد محصول: `code`، `name`، `accountTypeId`، `currencyId`، `effectiveFrom` و `effectiveTo`؛ مقدار `effectiveTo` می‌تواند `null` باشد. برای افتتاح حساب، فیلدهای `productId`، `openingBranchId` و `ownerCustomerIds` ثبت شده‌اند. قرارداد کامل body و پاسخ باید در صورت تغییر از Controller مربوطه استخراج شود؛ این فهرست جایگزین قرارداد کامل JSON نیست.

### 16.4 مسیرهایی که در Router فعلی مشاهده نشده‌اند

در `src/web/httprouter.cpp` بررسی‌شده، مسیر HTTP مستقیمی برای موارد زیر وجود ندارد؛ وجود جدول، Migration یا منطق داخلی را نباید با وجود API اشتباه گرفت:

- دریافت مانده حساب از مسیر اختصاصی.
- دریافت فهرست/جزئیات تراکنش و Journal.
- مدیریت قواعد مانده محصول (`product_balance_rule`) و Override سطح حساب (`account_balance_rule_override`) از طریق endpoint مستقل.
- مدیریت روابط حساب، مالکیت و اختیارات/مجوزهای صاحبان حساب از طریق endpoint مستقل.
- مدیریت محدودیت‌های عملیاتی حساب از طریق endpoint مستقل.

اگر بخشی از این موارد در مرحله بعدی موردنیاز باشد، ابتدا باید قرارداد endpoint، دسترسی‌ها، تراکنش اتمیک و قواعد حسابداری طراحی شود؛ مسیرهای پیشنهادی را بدون تأیید به‌عنوان قرارداد فعلی معرفی نکنید.

### 16.5 الزامات مشترک عملیات مالی

- اعتبارسنجی داده و وضعیت حساب قبل از ثبت مالی.
- پاسخ سازگار با نتیجه واقعی تراکنش.
- عدم تغییر مستقیم مانده از مسیر API.
- Idempotency، شامل رفتار کلید یکسان با payload متفاوت.
- ثبت اتمیک Transaction، Journal و تغییر مانده‌ها.
- رعایت ارز، حداقل مانده، Holdها، اضافه‌برداشت و محدودیت‌های معتبر.
- کنترل حسابداری با توازن Debit/Credit و تطبیق `ledger_account_balance`.
- پاسخ خطای پایدار در حالت پیکربندی‌نشدن سرویس وابسته؛ خطا نباید به ثبت جزئی منجر شود.

وجود route به‌تنهایی اثبات‌کننده صحت منطق Service/Repository یا پوشش کامل همه سناریوها نیست. در عین حال، طبق تأیید کاربر، گروه‌های بخش 16.1 از نظر پیاده‌سازی و تست API تکمیل شده‌اند؛ کارهای باقی‌مانده در بخش 18 مربوط به APIهای مالی هنوز route نشده، هم‌ترازی سرویس‌های مالی قدیمی با Schema جدید و پوشش جامع سناریوهای ایمنی/یکپارچگی است.

## 17. Migrationها و تغییرات Schema

Migrationهای تاریخی شناخته‌شده:

- `001_geography_currency.sql`
- `002_remove_legacy_tables.sql`
- `003_customer.sql`
- `004_customer_number_and_indexes.sql`
- `005_account_product_ledger.sql`
- `006_account_relationships.sql`
- `007_balances.sql`
- `008_transaction.sql`
- `009_journal.sql`
- `010_financial_consistency.sql`
- `011_city_code_global_unique.sql`
- `012_product_ledger_mapping.sql`
- `013_account_type_serial.sql`
- `014_product_balance_rules.sql`
- `015_account_holds.sql`
- `016_allow_overdraft_balances.sql`
- `017_hold_idempotency.sql`
- `018_hold_event_fingerprint.sql`

این فهرست سابقه تغییرات است؛ وضعیت اعمال Migration در هر محیط باید جداگانه تأیید شود. جزئیات اجرا، تاریخ و اشیای هر Migration در `PROJECT_HISTORY.md` بخش 3 و گزارش‌های Hold نگهداری می‌شود.

- Migration 015 ساختار مسدودی و Triggerهای مربوط را تعریف می‌کند. طبق سابقه در 2026-10-24 روی دیتابیس اصلی محلی اجرا شده است.
- Migration 016 تغییرات مانده قابل برداشت و Idempotency تراکنش را تعریف می‌کند؛ طبق سابقه اجرا شده است. جزئیات قطعی باید با فایل Migration و Schema جاری تطبیق داده شود.
- Migration 017 ستون‌های Idempotency Hold و ایندکس `uq_hold_api_idempotency` را اضافه می‌کند؛ اجرای موفق در 2026-11-01 تأیید شده است.
- Migration 018 ستون‌های Metadata، Constraintها، توابع Canonicalization و Trigger جدید Event را اضافه می‌کند؛ اجرای موفق با COMMIT در 2026-11-11 تأیید شده، `CURRENT_SCHEMA.sql` از دیتابیس واقعی به‌روز شده و دامپ پیش از 018 در `database/backups` موجود است.

## 18. وضعیت قابلیت‌ها و کارهای باقی‌مانده

### 18.1 APIهای تکمیل و تست‌شده

طبق تأیید صریح کاربر در 2026-10-28: Health، Countries، Currencies، Cities، Branches، Customers، Products، Ledger Accounts، Account Opening، Account Status، Deposits و Internal Transfers پیاده‌سازی و تست شده‌اند. مسیرهای HTTP آن‌ها در بخش 16.1 فهرست شده‌اند. این تأیید، انجام تست API را مشخص می‌کند، نه تکمیل همه تست‌های هم‌زمانی/بار و نه آماده‌بودن برای بانکداری واقعی.

Hold API نیز پس از اجرای Migration 018 و آزمون یکپارچه‌سازی API در 2026-11-11 در دامنه مصوب خود بسته شد؛ جزئیات در History بخش‌های 18.14 و 18.15 و گزارش‌های Hold آمده است.

### 18.2 مسیرهای پیاده‌سازی‌شده با شواهد آزمون ناکافی در سند

- Province API.
- Account Type API.
- Product–Ledger Mapping API: کاربرد نگاشت در تست افتتاح حساب ثبت شده، اما آزمون مستقیم همه عملیات مدیریت نگاشت در History مستند نیست.

برای این موارد، کد route وجود دارد؛ ابتدا شواهد آزمون موجود بررسی شود و فقط اگر تست واقعاً اجرا نشده بود، تست HTTP اضافه شود. پیاده‌سازی مجدد بدون نیاز انجام نشود.

### 18.3 قابلیت‌های/APIهای باقی‌مانده

1. **Withdrawal API:** `POST /api/v1/withdrawals` پیاده‌سازی و آزمون واقعی شده است (موفق، Replay، Conflict، موجودی ناکافی، Hold، حساب CLOSED/ناموجود، مبلغ نامعتبر). ثبت: بدهکار سپرده مشتری / بستانکار سرفصل تسویه، در همان UoW. سیاست حداقل مانده قطعی و اعمال‌شده است: `MAX(product_minimum, account_override_minimum)` با Override فقط افزایش‌دهنده. محدودیت‌های باز: Overdraft واقعی، Rule واقعی PostgreSQL، هم‌زمانی و Fault Injection آزمون نشده‌اند؛ فرمول minimum در Transfer طبق گزارش T1 هم‌تراز شده است؛ آزمون یکپارچه Query تولیدی و قواعد واقعی PostgreSQL و بازبینی ایمنی Commit/Rollback همچنان باقی است.
2. **Reversal API:** `POST /api/v1/transactions/{id}/reversals` پیاده‌سازی و آزمون واقعی شده است، اما فقط در دامنهٔ **برگشت کاملِ واریز `COMPLETED` بدون کارمزد**. برگشت، تراکنش `REVERSAL` جدید با `reverses_transaction_id` و Journal معکوس می‌سازد، تراکنش اصلی را حذف/بازنویسی نمی‌کند و پس از ثبت کامل، اصلی را `REVERSED` می‌کند. **بیرون از دامنه:** برگشت برداشت، برگشت انتقال، برگشت جزئی و برگشت واریز دارای کارمزد. هستهٔ داخلی برگشت کامل برداشت (`ReversalService::reverseWithdrawal`) با قرارداد جدید `findSettlementLedgerById` و **پیش‌شرط صریح تأیید صلاحیت از فراخوانندهٔ داخلی برای همان شناسهٔ برداشت** پیاده و با fake UoW لینک/اجرا شده است (شکل Journal صرفاً اعتبارسنجی حسابداری است و مدرک عدم مصرف اضافه‌برداشت نیست)، اما از این مسیر HTTP فعال نیست و مسیر HTTP همچنان فقط واریز را می‌پذیرد. همچنین کف مانده فعال، هم‌زمانی واقعی دیتابیس، Fault Injection و Rollback/Commit نامشخص آزمون نشده‌اند.
3. **Balance/Transaction/Journal read APIs:** پرس‌وجوی خواندنی یک تراکنش با `GET /api/v1/transactions/{transactionId}` (History 18.42)، یک سند با `GET /api/v1/journals/{journalId}` (History 18.41) و دفتر کلِ صفحه‌بندی‌شده با `GET /api/v1/ledger-accounts/{ledgerAccountId}/entries` (History 18.43) فعال شده‌اند. endpoint اختصاصی برای دریافت مانده در Router فعلی دیده نشده است؛ مشخص شود برای کلاینت/عملیات لازم است یا خیر.
4. **چرخه اضافه‌برداشت:** تسویه اضافه‌برداشت و ثبت هم‌زمان بدهی مشتری و مطالبات بانک تکمیل/تأیید نشده است. مسیر نیازمند تسویه همچنان با `OVERDRAFT_SETTLEMENT_NOT_CONFIGURED` رد می‌شود.
5. **مانده قابل برداشت:** حفظ فرمول مفهومی `ledger_balance - active_holds + remaining_overdraft_limit` در تمام مسیرهای مالی لازم است؛ صرف ذخیره شدن `available_balance` کافی نیست و این Cache برای تصمیم مالی جدید قابل اتکا نیست.
6. **Reconciliation:** تطبیق `account_balance`، تراکنش، Journal و `ledger_account_balance` و بررسی ناورداهای مالی باید پوشش جامع داشته باشد.
7. **بازنویسی/هم‌ترازی سرویس‌های مالی قدیمی:** سرویس‌ها و Repositoryهای قدیمی مربوط به مانده حساب، تراکنش و ثبت دفتر کل را با Schema و قواعد بازطراحی‌شده هماهنگ یا در صورت لزوم بازنویسی کنید. کد قدیمی نباید مرجع معماری جدید فرض شود؛ سازگاری آن باید با `database/CURRENT_SCHEMA.sql`، قواعد Hold، Idempotency، Journal و اضافه‌برداشت اثبات شود. زیرمرحله‌های 1A، 1B.1، 1C.1 و 1C.2 مسیر Transaction/Idempotency، Balance، Journal/Ledger، Account currency lookup/row lock و Hold row lock در Deposit را به زیرساخت جدید منتقل کرده‌اند؛ Transfer، HoldService و سایر سرویس‌ها هنوز در دامنه این بازنویسی باقی مانده‌اند.
8. **تست‌های تکمیلی مالی:** آزمون رقابت هم‌زمان، کلید Idempotency تکراری با محتوای متفاوت، rollback در نقاط شکست، موجودی ناکافی، و اطمینان از عدم ایجاد Transaction/Journal تکراری.
9. **قواعد دسترسی و محدودیت‌ها:** بررسی شود وضعیت حساب، رابطه مالکیت/اختیار و محدودیت عملیاتی در تمام عملیات مالی اثر می‌گذارند؛ API مستقل برخی تنظیمات در Router دیده نمی‌شود.
10. **شواهد تست‌ها:** تاریخ، SHA، دیتابیس، درخواست/پاسخ و نتیجه بررسی مانده/Journal برای APIهای تأییدشده و به‌خصوص آخرین تست Transfer در History کامل شود.

### 18.4 اصول وضعیت‌دهی

- **تصمیم طراحی:** قاعده کسب‌وکار تصویب شده است.
- **پیاده‌سازی‌شده:** کد قابلیت وجود دارد.
- **تست API تأییدشده:** تست HTTP/عملکرد API اجرا شده و وضعیت آن از سوی کاربر یا شواهد آزمون تأیید شده است.
- **تست‌شده روی دیتابیس اصلی:** فقط وقتی نام محیط و نتیجه واقعی روشن باشد این برچسب استفاده شود.
- **آزمون جامع مالی:** سناریوهای شکست، هم‌زمانی، Idempotency و تطبیق دفتر کل نیز پوشش داده شده‌اند.
- **آماده بهره‌برداری:** فقط پس از بررسی جامع مالی، امنیتی، عملیاتی، هم‌زمانی و آزمون‌های مرتبط؛ API تست‌شده لزوماً آماده بهره‌برداری نیست.

### 18.5 وضعیت و پذیرش محدود 1A

- `UnitOfWork` در `src/infrastructure/unitofwork.h` و `.cpp` اضافه شده است. مالک `begin/commit/rollback` لایه Use Case است؛ `TransactionService` و Repositoryها Commit مستقل ندارند.
- شکست `commit` به‌صورت `CommitOutcomeUnknown` نگهداری می‌شود. پس از آن هیچ Rollback خودکاری انجام نمی‌شود. شکست Rollback به وضعیت مستقل `RollbackFailed` می‌رود و Retry خودکار ندارد.
- `RolledBack` فقط پس از موفقیت واقعی rollback ثبت می‌شود.
- پس از `CommitOutcomeUnknown`، `RollbackFailed` یا شکست BEGIN روی اتصال باز، اتصال مشترک در مرز امن بسته می‌شود و بدون بازگشایی خودکار بسته می‌ماند؛ دسترسی دیتابیس سرویس تا restart متوقف می‌شود. این fail-closed موقت است، نه معماری نهایی.
- مدل `Transaction` در `src/domain/transaction.*` بازنویسی شد: `REJECTED`، فیلدهای Idempotency/نتیجه و جدول انتقال مجاز `INITIATED -> PROCESSING -> COMPLETED/REJECTED`.
- در مسیر Deposit فقط زنجیره `INITIATED -> PROCESSING -> COMPLETED/REJECTED` استفاده می‌شود؛ مدل Domain همچنان جدول انتقال بخش 10 (شامل `INITIATED -> CANCELLED`) را حفظ می‌کند. این تفکیک برای رفع ابهام مستنداتی است و رفتار جدیدی ایجاد نمی‌کند.
- `TransactionRepository` و `ITransactionRepository` بازنویسی شدند: Lookup و Find با نتیجه Typed، تبدیل وضعیت/نوع/کانال به‌صورت Fail-Closed و Update وضعیت با Expected Status.
- `TransactionService` در `src/application/transactionservice.*` اضافه شد: Advisory Lock روی کلید API، Lookup پیش از اعتبارسنجی شرایط متغیر حساب، ایجاد `INITIATED` و انتقال به `PROCESSING`، تکمیل و رد.
- `DepositService` فقط برای Transaction و Idempotency از زیرساخت جدید استفاده می‌کند. SQL مربوط به Balance، Journal، TransactionEntry، LedgerBalance و بررسی توازن در این مرحله بازنویسی نشد.
- الگوریتم Fingerprint، Canonicalization، دامنه Idempotency و قرارداد HTTP فعلی Deposit تغییر نکرد. Retry کلیدهای ثبت‌شده پیش از این مرحله نیز آزموده شد.
- 1A در دامنه محدود پذیرفته شده است. گزارش: [1A_ACCEPTANCE_WITH_LIMITATIONS.md](opencode-reports/1A_ACCEPTANCE_WITH_LIMITATIONS.md).
- محدودیت‌های باز: رفتار واقعی QPSQL/PostgreSQL در قطع ارتباط و Retry پس از Commit نامعلوم آزموده نشده؛ آزمون قطعی Rollback میانی بدون Fault Injection انجام نشد؛ آزمون «هم‌زمانی» به دلیل سرور تک‌ریسمانی و Connection مشترک، هم‌پوشانی واقعی دو تراکنش دیتابیس را اثبات نمی‌کند.
- مسیرهای legacy (Transfer/Hold/AccountOpening/AccountService و چند Repository) خارج از اصلاح 1A هستند و ریسک مشترک اتصال دارند.
- `available_balance` همچنان Cache کهنه‌شونده است و در 1A تصمیم مالی جدیدی بر آن بنا نشد.
- شکاف قفل قواعد مؤثر ناموجود برای مرحله Balance باز باقی است: `FOR SHARE` فقط ردیف موجود را قفل می‌کند و اگر Override یا قاعده مؤثر موجود نباشد، Exclusion Constraint به‌تنهایی مانع درج قاعده جدید مؤثر نمی‌شود. این موضوع پیش از Balance Slice باید تعیین تکلیف شود.
- پذیرش محدود 1A معادل آمادگی کامل برای بهره‌برداری بانکی نیست. 1B و Balance Slice شروع نشده‌اند.

#### 1A.1 — انتقال قرارداد Transaction/UoW — 2026-11-12

- `ITransactionRepository` به `src/application/contracts/` منتقل شد و `TransactionService` به همان قرارداد Application وابسته است.
- `ITransactionUnitOfWork` و `ITransactionUnitOfWorkFactory` در Application تعریف شدند؛ `UnitOfWork` و `UnitOfWorkFactory` پیاده‌سازی Infrastructure هستند.
- هر فراخوانی Deposit یک UnitOfWork مستقل از factory می‌گیرد؛ `TransactionRepository` همان scope و همان Connection/تراکنش را استفاده می‌کند.
- `DepositService` دیگر `UnitOfWork` یا `TransactionRepository` concrete نمی‌سازد و `database()` از قرارداد UoW حذف شده است.
- SQL مربوط به `transaction_entry` با همان SQL، پارامترها و ترتیب قبلی به `TransactionRepository` منتقل شد.
- قرارداد جدید Transaction/UoW از `OperationError` امن استفاده می‌کند؛ کدهای API حفظ شده‌اند و متن خام SQL در این قراردادها نشت نمی‌کند.
- رفتار F1/F2 حفظ شده است: `CommitOutcomeUnknown` و `RollbackFailed` جدا، بدون rollback خودکار پس از commit ناموفق، و fail-closed فعلی.
- انتقالی تا 1C: `QSqlDatabase` تزریق‌شده و SQLهای Balance/Journal در آن مرحله در `DepositService` باقی ماندند. در 1B.1 بخش Balance و در 1C.1 بخش Journal/Ledger خارج شدند؛ `QSqlDatabase` انتقالی فقط برای Account/Hold/currency SQL باقی است.

#### 1B.1 — استخراج ساختاری Balance از Deposit — 2026-11-13

- `IBalanceRepository` و مدل‌های `BalanceAccountSnapshot`/`BalanceSnapshotResult` در `src/application/contracts/ibalancerepository.h` تعریف شدند. قرارداد هیچ `QSqlDatabase`، `QSqlQuery`، SQL یا متن خام خطای دیتابیس منتشر نمی‌کند.
- `BalanceRepository` در `src/infrastructure/repository/balancerepository.*` سه عملیات موردنیاز Deposit را پیاده‌سازی می‌کند: قفل ردیف `account_balance`، خواندن Snapshot قفل‌شده ورودی‌های محاسبه موجودی، و اعمال افزایش واریز روی `account_balance`.
- `balances()` به `ITransactionUnitOfWork` اضافه شد. `UnitOfWork` همان `QSqlDatabase` و همان scope را به `BalanceRepository` می‌دهد؛ Connection یا تراکنش مستقل ایجاد نشد.
- `DepositService` فقط از `IBalanceRepository` استفاده می‌کند و SQLهای Balance از آن خارج شدند. اعتبارسنجی‌ها و شاخه‌های تصمیم Use Case در Deposit باقی ماندند.
- SQL، bindها، ترتیب Queryها و قفل‌ها، `CURRENT_TIMESTAMP`/`CURRENT_DATE`، precision رشته‌ای NUMERIC و فرمول فعلی `available_balance` تغییر نکردند. `available_balance` کش‌شده جایگزین منبع محاسبه نشد.
- متن خام خطاهای Balance با پیام‌های امن ثابت جایگزین شد؛ کدهای API مانند `ACCOUNT_BALANCE_LOCK_FAILED`، `ACCOUNT_LOCK_OR_READ_FAILED` و `ACCOUNT_BALANCE_UPDATE_FAILED` حفظ شدند.
- خارج از دامنه و بدون تغییر: Journal، Transfer، HoldService و مسیرهای نوشتن Hold، Schema/Migration/داده، معیارهای Domain و شکاف هم‌زمانی قواعد مؤثر.
- این مرحله تکمیل 1B، رفع race یا تکمیل معماری Deposit نیست. جزئیات آزمون‌ها و محدودیت‌ها در `PROJECT_HISTORY.md` بخش 18.21 آمده است.

#### 1C.1 — استخراج ساختاری Journal/Ledger از Deposit — 2026-11-13

- `IJournalRepository` و مدل‌های نتیجه آن در `src/application/contracts/ijournalrepository.h` تعریف شدند. قرارداد هیچ `QSqlDatabase`، `QSqlQuery`، SQL یا متن خام خطای دیتابیس منتشر نمی‌کند.
- `JournalRepository` در `src/infrastructure/repository/journalrepository.*` عملیات واقعی Deposit را پیاده‌سازی می‌کند: lookup سرفصل تسویه با `FOR SHARE`، مقداردهی اولیه `ledger_account_balance`، قفل ردیف‌های Ledger، ایجاد Journal پیش‌نویس، درج دو سطر Journal، خواندن توازن، به‌روزرسانی تجمعی Ledger و posting مشروط.
- `journals()` به `ITransactionUnitOfWork` اضافه شد. `UnitOfWork` همان `QSqlDatabase` و همان scope را به `JournalRepository` می‌دهد؛ Connection یا تراکنش مستقل ایجاد نشد.
- SQL، bindها، ترتیب عملیات و قفل‌ها، `CURRENT_DATE`/`CURRENT_TIMESTAMP`، ترتیب debit/credit و precision NUMERIC حفظ شدند.
- شاخه‌های تصمیم Use Case در Deposit باقی ماندند: رد حساب/سرفصل، بررسی تعداد ردیف‌های قفل‌شده، توازن دو سطری و نگاشت کدهای خطا.
- متن خام خطاهای Journal/Ledger با پیام‌های امن ثابت جایگزین شد؛ کدهای API حفظ شدند. جزئیات نگاشت در `PROJECT_HISTORY.md` بخش 18.22 آمده است.
- SQLهای باقی‌مانده در آن مرحله: Account currency lookup، Account row lock و Hold row lock. این سه مورد در 1C.2 منتقل شدند و `QSqlDatabase`/Qt SQL از Deposit حذف شد.
- خارج از دامنه و بدون تغییر: Domain invariants جدید، Transfer، HoldService و مسیرهای نوشتن Hold، Schema/Migration/داده، زمان مرجع، isolation و شکاف هم‌زمانی قواعد مؤثر.
- این مرحله تکمیل کل 1B/1C، رفع race یا آمادگی عملیاتی بانکی نیست.

#### 1C.2 — بسته نهایی استخراج persistence از Deposit — 2026-11-13

- `IAccountStateRepository` در Application برای Account currency lookup و Account row lock و `IHoldRepository` برای Hold row lock تعریف شدند. قراردادها SQL، `QSqlDatabase`، `QSqlQuery` یا خطای خام دیتابیس منتشر نمی‌کنند.
- `AccountStateRepository` و `HoldRepository` در Infrastructure همان SQL، bindها، ترتیب و طول عمر قفل‌های قبلی را حفظ می‌کنند.
- `accounts()` و `holds()` به `ITransactionUnitOfWork` اضافه شدند؛ هر دو Repository متعلق به همان UoW و همان Connection هستند. `begin/commit/rollback` فقط تحت UoW باقی ماند.
- `DepositService` دیگر هیچ SQL اجرایی، `QSqlDatabase`، `QSqlQuery`، `QSqlError` یا include مستقیم Infrastructure ندارد. تشخیص `DATABASE_UNAVAILABLE` از طریق `ITransactionUnitOfWorkFactory::isConnectionAvailable()` انجام می‌شود تا کد API و ترتیب کنترل قبلی حفظ شود.
- ترتیب کامل عملیات و قفل‌ها، fingerprint/canonicalization/replay/conflict/retry، F1/F2، `CommitOutcomeUnknown`، عدم rollback پس از commit نامعلوم و fail-closed حفظ شدند.
- متن خام خطاهای Account/Hold با پیام‌های امن ثابت جایگزین شد؛ کدهای API حفظ شدند. جزئیات نگاشت در `PROJECT_HISTORY.md` بخش 18.23 آمده است.
- این مرحله استخراج ساختاری است و Domain rules، رفع race، زمان مرجع، محاسبات دقیق یا آمادگی عملیاتی بانکی را تکمیل نمی‌کند.

شرح تفصیلی اجرای 1A، 1A.1، 1B.1، 1C.1 و 1C.2 و شواهد آن در `PROJECT_HISTORY.md` بخش‌های 18.17، 18.18، 18.20، 18.21، 18.22 و 18.23 نگهداری می‌شود.

## 19. قواعد نگهداری مستندات

`PROJECT_CONTEXT.md` باید قواعد کسب‌وکار، تصمیم‌های معماری تثبیت‌شده، وضعیت جاری و ریسک‌های باز را در خود داشته باشد. شرح اجراهای گذشته و خروجی‌های طولانی باید در `PROJECT_HISTORY.md` یا گزارش‌های `docs/opencode-reports/` نگهداری شود و Context فقط خلاصه و لینک داشته باشد.

`PROJECT_HISTORY.md` باید نتایج دقیق آزمون‌ها، درخواست‌ها و پاسخ‌های HTTP، خروجی‌های SQL و Build، اشکالات گذشته، Migrationها و Commitها را نگهداری کند.

تاریخچه آزمون‌ها مدرک اجرای آزمون در یک زمان مشخص است، نه تضمین رفتار فعلی.

هر تصمیم کسب‌وکار که تأیید می‌شود باید در Context ثبت شود؛ ارجاع به History جایگزین این کار نیست. مستندات پیش از Commit باید برای بررسی به کاربر ارائه شوند.

## 20. دستور ادامه کار

در ادامه پروژه:

1. Git، Schema و فایل‌های کد مرتبط با مرحله فعلی بررسی شوند.
2. قواعد قطعی دوباره از ابتدا مطرح نشوند، مگر اینکه تناقض واقعی پیدا شود.
3. قبل از تغییر مالی، منطق حسابداری و آثار آن مشخص شوند.
4. تغییرات در فایل‌های کامل و قابل جایگزینی ارائه شوند.
5. Build و آزمون‌های HTTP اجرا شوند.
6. برای عملیات مالی، مانده، Journal و تاریخچه رویدادها نیز بررسی شوند.
7. نتیجه واقعی و محدودیت آزمون‌ها ثبت شود.
8. پس از تأیید کاربر، تغییرات در Git ثبت و Push شوند.
