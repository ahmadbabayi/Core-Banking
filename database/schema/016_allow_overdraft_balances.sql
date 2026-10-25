-- ============================================================
-- Core Banking Database
-- Schema 016
-- Available balance and overdraft compatibility
-- PostgreSQL
--
-- Decision:
--   * ledger_balance remains non-negative. It represents the
--     customer's deposit liability balance only.
--   * available_balance may be negative because it is a derived
--     availability figure and may reflect active holds/overdraft.
--
-- This migration is safe to run against a database where the old
-- migration was partially applied: it drops the available-balance
-- check if present and restores the ledger-balance check if missing.
-- It also allows the REJECTED transaction state used by the API.
-- It deliberately does not change any ledger data.
-- ============================================================

BEGIN;

ALTER TABLE public.account_balance
    DROP CONSTRAINT IF EXISTS ck_account_balance_available;

DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1
          FROM pg_constraint
         WHERE conrelid = 'public.account_balance'::regclass
           AND conname = 'ck_account_balance_ledger'
    ) THEN
        ALTER TABLE public.account_balance
            ADD CONSTRAINT ck_account_balance_ledger
            CHECK (ledger_balance >= 0) NOT VALID;
    END IF;
END;
$$;

-- Validate the invariant. If historic negative deposit balances exist,
-- this fails rather than silently accepting an inconsistent ledger.
ALTER TABLE public.account_balance
    VALIDATE CONSTRAINT ck_account_balance_ledger;

-- Columns used by the API idempotency contract. ADD COLUMN IF NOT EXISTS
-- supports a database where part of the previous draft was already run.
ALTER TABLE public.transaction
    ADD COLUMN IF NOT EXISTS idempotency_key VARCHAR(100);
ALTER TABLE public.transaction
    ADD COLUMN IF NOT EXISTS request_fingerprint VARCHAR(64);
ALTER TABLE public.transaction
    ADD COLUMN IF NOT EXISTS result_code VARCHAR(100);
ALTER TABLE public.transaction
    ADD COLUMN IF NOT EXISTS result_message TEXT;

CREATE UNIQUE INDEX IF NOT EXISTS uq_transaction_api_idempotency
    ON public.transaction (channel, idempotency_key)
    WHERE channel = 'API' AND idempotency_key IS NOT NULL;

-- API business-rule rejections are persisted as REJECTED transactions.
-- The original constraint may use the explicit schema name or
-- PostgreSQL's generated name. Drop only the status-domain constraint;
-- keep unrelated checks that may also refer to status.
ALTER TABLE public.transaction
    DROP CONSTRAINT IF EXISTS ck_transaction_status;
ALTER TABLE public.transaction
    DROP CONSTRAINT IF EXISTS transaction_status_check;

ALTER TABLE public.transaction
    ADD CONSTRAINT ck_transaction_status
    CHECK (status IN (
        'INITIATED', 'PROCESSING', 'COMPLETED', 'REJECTED',
        'FAILED', 'CANCELLED', 'REVERSED'
    ));

COMMIT;
