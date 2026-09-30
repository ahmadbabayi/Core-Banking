-- ============================================================
-- Core Banking Database
-- Schema 007
-- Account and Ledger Account Balances
-- PostgreSQL
-- ============================================================

CREATE TABLE account_balance (
    account_id BIGINT PRIMARY KEY,
    ledger_balance NUMERIC NOT NULL DEFAULT 0,
    available_balance NUMERIC NOT NULL DEFAULT 0,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_account_balance_account
        FOREIGN KEY (account_id)
        REFERENCES account(id)
        ON DELETE CASCADE,

    CONSTRAINT ck_account_balance_ledger
        CHECK (ledger_balance >= 0),

    CONSTRAINT ck_account_balance_available
        CHECK (available_balance >= 0)
);


CREATE TABLE ledger_account_balance (
    ledger_account_id BIGINT PRIMARY KEY,
    debit_total NUMERIC NOT NULL DEFAULT 0,
    credit_total NUMERIC NOT NULL DEFAULT 0,
    balance NUMERIC NOT NULL DEFAULT 0,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT fk_ledger_account_balance_ledger_account
        FOREIGN KEY (ledger_account_id)
        REFERENCES ledger_account(id)
        ON DELETE CASCADE,

    CONSTRAINT ck_ledger_account_balance_debit
        CHECK (debit_total >= 0),

    CONSTRAINT ck_ledger_account_balance_credit
        CHECK (credit_total >= 0)
);
