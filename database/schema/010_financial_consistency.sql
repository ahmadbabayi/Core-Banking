-- ============================================================
-- Core Banking Database
-- Schema 010
-- Financial Consistency Constraints
-- PostgreSQL
-- ============================================================

BEGIN;

-- ============================================================
-- 1. Ledger account
-- ============================================================

-- Required for composite foreign keys involving currency.
ALTER TABLE ledger_account
    ADD CONSTRAINT uq_ledger_account_id_currency
    UNIQUE (id, currency_id);

-- A ledger account's parent must belong to the same currency.
ALTER TABLE ledger_account
    ADD CONSTRAINT fk_ledger_account_parent_currency
    FOREIGN KEY (parent_id, currency_id)
    REFERENCES ledger_account (id, currency_id);


-- ============================================================
-- 2. Account
-- ============================================================

-- Required for composite foreign keys involving currency.
ALTER TABLE account
    ADD CONSTRAINT uq_account_id_currency
    UNIQUE (id, currency_id);

-- Required for validating that an account's ledger account
-- belongs to the same currency and is the ledger account
-- assigned to that account.
ALTER TABLE account
    ADD CONSTRAINT uq_account_id_ledger_currency
    UNIQUE (id, ledger_account_id, currency_id);

-- An account and its ledger account must use the same currency.
ALTER TABLE account
    ADD CONSTRAINT fk_account_ledger_currency
    FOREIGN KEY (ledger_account_id, currency_id)
    REFERENCES ledger_account (id, currency_id);


-- ============================================================
-- 3. Transaction
-- ============================================================

-- Required for transaction_entry and transaction_fee
-- currency consistency.
ALTER TABLE transaction
    ADD CONSTRAINT uq_transaction_id_currency
    UNIQUE (id, currency_id);


-- ============================================================
-- 4. Transaction entry
-- ============================================================

-- Transaction entry must use the same currency as its transaction.
ALTER TABLE transaction_entry
    ADD CONSTRAINT fk_transaction_entry_transaction_currency
    FOREIGN KEY (transaction_id, currency_id)
    REFERENCES transaction (id, currency_id);

-- Transaction entry must use the same currency as its account.
ALTER TABLE transaction_entry
    ADD CONSTRAINT fk_transaction_entry_account_currency
    FOREIGN KEY (account_id, currency_id)
    REFERENCES account (id, currency_id);


-- ============================================================
-- 5. Journal
-- ============================================================

-- Value date is intentionally NOT constrained to be greater
-- than or equal to posting date at database level.
--
-- Back-valued / backdated posting rules belong to the
-- Posting Service / Business Rules layer.
ALTER TABLE journal
    DROP CONSTRAINT ck_journal_value_date;


-- ============================================================
-- 6. Journal entry
-- ============================================================

-- Journal entry must use the same currency as its ledger account.
ALTER TABLE journal_entry
    ADD CONSTRAINT fk_journal_entry_ledger_currency
    FOREIGN KEY (ledger_account_id, currency_id)
    REFERENCES ledger_account (id, currency_id);

-- When account_id is present, journal entry must use the
-- same currency as the account.
ALTER TABLE journal_entry
    ADD CONSTRAINT fk_journal_entry_account_currency
    FOREIGN KEY (account_id, currency_id)
    REFERENCES account (id, currency_id);

-- When account_id is present, the journal entry's ledger
-- account must be exactly the ledger account assigned to
-- that account.
ALTER TABLE journal_entry
    ADD CONSTRAINT fk_journal_entry_account_ledger_currency
    FOREIGN KEY (account_id, ledger_account_id, currency_id)
    REFERENCES account (id, ledger_account_id, currency_id);


-- ============================================================
-- 7. Transaction fee
-- ============================================================

-- Fee must use the same currency as the transaction.
ALTER TABLE transaction_fee
    ADD CONSTRAINT fk_transaction_fee_transaction_currency
    FOREIGN KEY (transaction_id, currency_id)
    REFERENCES transaction (id, currency_id);

-- Fee must use the same currency as its fee ledger account.
ALTER TABLE transaction_fee
    ADD CONSTRAINT fk_transaction_fee_ledger_currency
    FOREIGN KEY (ledger_account_id, currency_id)
    REFERENCES ledger_account (id, currency_id);


COMMIT;
