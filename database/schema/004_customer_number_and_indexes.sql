-- ============================================================
-- Core Banking Database
-- Migration 004
-- Refine Customer Number and Remove Redundant Indexes
-- PostgreSQL
-- ============================================================

BEGIN;


-- ------------------------------------------------------------
-- 1. Remove redundant indexes
--
-- customer_id is already the PRIMARY KEY of both tables.
-- PostgreSQL automatically creates an index for each
-- PRIMARY KEY, so these additional indexes are redundant.
-- ------------------------------------------------------------

DROP INDEX IF EXISTS idx_individual_customer_id;

DROP INDEX IF EXISTS idx_legal_entity_customer_id;


-- ------------------------------------------------------------
-- 2. Change customer_number to fixed 10-digit format
-- ------------------------------------------------------------

ALTER TABLE customer
    DROP CONSTRAINT ck_customer_number;

ALTER TABLE customer
    ALTER COLUMN customer_number TYPE CHAR(10)
    USING customer_number::CHAR(10);

ALTER TABLE customer
    ADD CONSTRAINT ck_customer_number
    CHECK (customer_number ~ '^[0-9]{10}$');


COMMIT;
