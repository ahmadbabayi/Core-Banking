--
-- PostgreSQL database dump
--

\restrict MDr2eXbbDnX8SUsPOY9EXV6pszHbseyZNML4erM6ALhogAznnuvNjLqHdab56vU

-- Dumped from database version 15.19 (Debian 15.19-0+deb12u1)
-- Dumped by pg_dump version 15.19 (Debian 15.19-0+deb12u1)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

--
-- Name: btree_gist; Type: EXTENSION; Schema: -; Owner: -
--

CREATE EXTENSION IF NOT EXISTS btree_gist WITH SCHEMA public;


--
-- Name: EXTENSION btree_gist; Type: COMMENT; Schema: -; Owner: -
--

COMMENT ON EXTENSION btree_gist IS 'support for indexing common datatypes in GiST';


--
-- Name: prevent_account_hold_event_mutation(); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION public.prevent_account_hold_event_mutation() RETURNS trigger
    LANGUAGE plpgsql
    AS $$
BEGIN
    RAISE EXCEPTION 'ACCOUNT_HOLD_EVENTS_ARE_IMMUTABLE'
        USING ERRCODE = '23514';
END;
$$;


--
-- Name: protect_account_hold(); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION public.protect_account_hold() RETURNS trigger
    LANGUAGE plpgsql
    AS $$
BEGIN
    RAISE EXCEPTION 'ACCOUNT_HOLD_IS_IMMUTABLE'
        USING ERRCODE = '23514';
END;
$$;


--
-- Name: validate_account_hold_event(); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION public.validate_account_hold_event() RETURNS trigger
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_hold account_hold%ROWTYPE;
    v_existing account_hold_events%ROWTYPE;
    v_released NUMERIC;
    v_remaining NUMERIC;
    v_now TIMESTAMPTZ;
BEGIN
    v_now := clock_timestamp();

    SELECT *
      INTO v_hold
      FROM account_hold
     WHERE id = NEW.hold_id
     FOR UPDATE;

    IF NOT FOUND THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_NOT_FOUND'
            USING ERRCODE = '23503';
    END IF;

    -- Check the idempotency key before applying release validation.
    -- This permits a retry even if the original event fully released
    -- the hold.
    SELECT *
      INTO v_existing
      FROM account_hold_events
     WHERE hold_id = NEW.hold_id
       AND idempotency_key = NEW.idempotency_key;

    IF FOUND THEN
        IF v_existing.event_type = NEW.event_type
           AND v_existing.amount = NEW.amount
           AND v_existing.effective_at = NEW.effective_at
           AND v_existing.reference_type
               IS NOT DISTINCT FROM NEW.reference_type
           AND v_existing.reference_id
               IS NOT DISTINCT FROM NEW.reference_id
           AND v_existing.reason_code = NEW.reason_code
        THEN
            -- Exact retry: do not insert a duplicate event.
            RETURN NULL;
        END IF;

        RAISE EXCEPTION 'ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED'
            USING ERRCODE = '23505';
    END IF;

    IF NEW.effective_at > v_now THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED'
            USING ERRCODE = '23514';
    END IF;

    IF v_now < v_hold.effective_at THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_NOT_YET_EFFECTIVE'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.effective_at < v_hold.effective_at THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.event_type = 'EXPIRY_RELEASE' THEN
        IF v_hold.expires_at IS NULL
           OR v_now < v_hold.expires_at THEN
            RAISE EXCEPTION 'ACCOUNT_HOLD_NOT_EXPIRED'
                USING ERRCODE = '23514';
        END IF;
    ELSE
        IF v_hold.expires_at IS NOT NULL
           AND v_now >= v_hold.expires_at THEN
            RAISE EXCEPTION 'EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE'
                USING ERRCODE = '23514';
        END IF;
    END IF;

    SELECT COALESCE(SUM(amount), 0)
      INTO v_released
      FROM account_hold_events
     WHERE hold_id = NEW.hold_id;

    v_remaining := v_hold.amount - v_released;

    IF v_remaining <= 0 THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_ALREADY_FULLY_RELEASED'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.amount > v_remaining THEN
        RAISE EXCEPTION 'ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.event_type = 'PARTIAL_RELEASE'
       AND NEW.amount >= v_remaining THEN
        RAISE EXCEPTION 'PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT'
            USING ERRCODE = '23514';
    END IF;

    IF NEW.event_type IN (
        'FULL_RELEASE',
        'EXPIRY_RELEASE',
        'CANCELLATION_RELEASE'
    ) AND NEW.amount <> v_remaining THEN
        RAISE EXCEPTION 'FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT'
            USING ERRCODE = '23514';
    END IF;

    RETURN NEW;
END;
$$;


SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: account; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account (
    id bigint NOT NULL,
    account_number character(13) NOT NULL,
    account_type_id bigint NOT NULL,
    product_id bigint NOT NULL,
    currency_id bigint NOT NULL,
    opening_branch_id bigint NOT NULL,
    ledger_account_id bigint NOT NULL,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    opened_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    closed_at timestamp without time zone,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    product_ledger_mapping_id bigint NOT NULL,
    CONSTRAINT ck_account_closed_at CHECK (((closed_at IS NULL) OR (closed_at >= opened_at))),
    CONSTRAINT ck_account_number CHECK ((account_number ~ '^[0-9]{13}$'::text)),
    CONSTRAINT ck_account_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'DORMANT'::character varying, 'BLOCKED'::character varying, 'CLOSED'::character varying])::text[])))
);


--
-- Name: account_authorization; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_authorization (
    id bigint NOT NULL,
    account_id bigint NOT NULL,
    customer_id bigint NOT NULL,
    authorization_type character varying(30) NOT NULL,
    valid_from date NOT NULL,
    valid_to date,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_account_authorization_dates CHECK (((valid_to IS NULL) OR (valid_to >= valid_from))),
    CONSTRAINT ck_account_authorization_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[]))),
    CONSTRAINT ck_account_authorization_type CHECK (((authorization_type)::text = ANY ((ARRAY['POWER_OF_ATTORNEY'::character varying, 'LEGAL_GUARDIAN'::character varying, 'LEGAL_REPRESENTATIVE'::character varying, 'AUTHORIZED_SIGNATORY'::character varying])::text[])))
);


--
-- Name: account_authorization_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account_authorization ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_authorization_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_balance; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_balance (
    account_id bigint NOT NULL,
    ledger_balance numeric DEFAULT 0 NOT NULL,
    available_balance numeric DEFAULT 0 NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_account_balance_ledger CHECK ((ledger_balance >= (0)::numeric))
);


--
-- Name: account_balance_rule_override; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_balance_rule_override (
    id bigint NOT NULL,
    account_id bigint NOT NULL,
    minimum_balance numeric,
    overdraft_enabled boolean,
    overdraft_limit numeric,
    effective_from timestamp with time zone NOT NULL,
    effective_to timestamp with time zone,
    reason_code character varying(50) NOT NULL,
    reference_number character varying(100),
    created_at timestamp with time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_account_balance_override_disabled CHECK (((overdraft_enabled IS DISTINCT FROM false) OR (overdraft_limit = (0)::numeric))),
    CONSTRAINT ck_account_balance_override_effective_dates CHECK (((effective_to IS NULL) OR (effective_to > effective_from))),
    CONSTRAINT ck_account_balance_override_has_setting CHECK (((minimum_balance IS NOT NULL) OR (overdraft_enabled IS NOT NULL))),
    CONSTRAINT ck_account_balance_override_limit CHECK (((overdraft_limit IS NULL) OR (overdraft_limit >= (0)::numeric))),
    CONSTRAINT ck_account_balance_override_minimum CHECK (((minimum_balance IS NULL) OR (minimum_balance >= (0)::numeric))),
    CONSTRAINT ck_account_balance_override_overdraft_pair CHECK (((overdraft_enabled IS NULL) = (overdraft_limit IS NULL))),
    CONSTRAINT ck_account_balance_override_reason CHECK ((btrim((reason_code)::text) <> ''::text))
);


--
-- Name: account_balance_rule_override_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account_balance_rule_override ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_balance_rule_override_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_hold; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_hold (
    id bigint NOT NULL,
    account_id bigint NOT NULL,
    currency_id bigint NOT NULL,
    amount numeric NOT NULL,
    reason_code character varying(50) NOT NULL,
    reason_description text,
    reference_type character varying(50),
    reference_id character varying(100),
    created_at timestamp with time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    effective_at timestamp with time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    expires_at timestamp with time zone,
    CONSTRAINT ck_account_hold_amount CHECK ((amount > (0)::numeric)),
    CONSTRAINT ck_account_hold_expiry CHECK (((expires_at IS NULL) OR (expires_at > effective_at))),
    CONSTRAINT ck_account_hold_reason CHECK ((btrim((reason_code)::text) <> ''::text))
);


--
-- Name: account_hold_events; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_hold_events (
    id bigint NOT NULL,
    hold_id bigint NOT NULL,
    idempotency_key character varying(100) NOT NULL,
    event_type character varying(30) NOT NULL,
    amount numeric NOT NULL,
    effective_at timestamp with time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    created_at timestamp with time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    reference_type character varying(50),
    reference_id character varying(100),
    reason_code character varying(50) NOT NULL,
    CONSTRAINT ck_account_hold_event_amount CHECK ((amount > (0)::numeric)),
    CONSTRAINT ck_account_hold_event_idempotency_key CHECK ((btrim((idempotency_key)::text) <> ''::text)),
    CONSTRAINT ck_account_hold_event_reason CHECK ((btrim((reason_code)::text) <> ''::text)),
    CONSTRAINT ck_account_hold_event_type CHECK (((event_type)::text = ANY ((ARRAY['PARTIAL_RELEASE'::character varying, 'FULL_RELEASE'::character varying, 'EXPIRY_RELEASE'::character varying, 'CANCELLATION_RELEASE'::character varying])::text[])))
);


--
-- Name: account_hold_events_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account_hold_events ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_hold_events_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_hold_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account_hold ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_hold_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_hold_status; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW public.account_hold_status AS
 WITH release_totals AS (
         SELECT h.id AS hold_id,
            COALESCE(sum(e.amount), (0)::numeric) AS released_amount,
            count(e.id) AS release_event_count,
            (array_agg(e.event_type ORDER BY e.id DESC) FILTER (WHERE (e.id IS NOT NULL)))[1] AS last_release_event_type
           FROM (public.account_hold h
             LEFT JOIN public.account_hold_events e ON ((e.hold_id = h.id)))
          GROUP BY h.id
        ), hold_amounts AS (
         SELECT h.id AS hold_id,
            h.account_id,
            h.currency_id,
            h.amount AS original_amount,
            (h.amount - r.released_amount) AS remaining_amount,
            h.reason_code,
            h.reason_description,
            h.reference_type,
            h.reference_id,
            h.created_at,
            h.effective_at,
            h.expires_at,
            r.release_event_count,
            r.last_release_event_type
           FROM (public.account_hold h
             JOIN release_totals r ON ((r.hold_id = h.id)))
        )
 SELECT hold_amounts.hold_id,
    hold_amounts.account_id,
    hold_amounts.currency_id,
    hold_amounts.original_amount,
    hold_amounts.remaining_amount,
    hold_amounts.reason_code,
    hold_amounts.reason_description,
    hold_amounts.reference_type,
    hold_amounts.reference_id,
    hold_amounts.created_at,
    hold_amounts.effective_at,
    hold_amounts.expires_at,
    hold_amounts.release_event_count,
    hold_amounts.last_release_event_type,
        CASE
            WHEN ((hold_amounts.remaining_amount = (0)::numeric) AND ((hold_amounts.last_release_event_type)::text = 'EXPIRY_RELEASE'::text)) THEN 'EXPIRED'::text
            WHEN ((hold_amounts.remaining_amount = (0)::numeric) AND ((hold_amounts.last_release_event_type)::text = 'CANCELLATION_RELEASE'::text)) THEN 'CANCELLED'::text
            WHEN (hold_amounts.remaining_amount = (0)::numeric) THEN 'RELEASED'::text
            WHEN (hold_amounts.effective_at > CURRENT_TIMESTAMP) THEN 'SCHEDULED'::text
            WHEN ((hold_amounts.expires_at IS NOT NULL) AND (hold_amounts.expires_at <= CURRENT_TIMESTAMP)) THEN 'EXPIRED'::text
            WHEN (hold_amounts.remaining_amount < hold_amounts.original_amount) THEN 'PARTIALLY_RELEASED'::text
            ELSE 'ACTIVE'::text
        END AS status,
        CASE
            WHEN (hold_amounts.effective_at > CURRENT_TIMESTAMP) THEN (0)::numeric
            WHEN ((hold_amounts.expires_at IS NOT NULL) AND (hold_amounts.expires_at <= CURRENT_TIMESTAMP)) THEN (0)::numeric
            ELSE GREATEST(hold_amounts.remaining_amount, (0)::numeric)
        END AS currently_blocked_amount
   FROM hold_amounts;


--
-- Name: account_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_mandate; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_mandate (
    id bigint NOT NULL,
    account_id bigint NOT NULL,
    name character varying(150) NOT NULL,
    valid_from date NOT NULL,
    valid_to date,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_account_mandate_dates CHECK (((valid_to IS NULL) OR (valid_to >= valid_from))),
    CONSTRAINT ck_account_mandate_name CHECK (((name)::text <> ''::text)),
    CONSTRAINT ck_account_mandate_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: account_mandate_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account_mandate ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_mandate_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_mandate_party; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_mandate_party (
    id bigint NOT NULL,
    mandate_id bigint NOT NULL,
    customer_id bigint NOT NULL,
    role character varying(30) DEFAULT 'SIGNATORY'::character varying NOT NULL,
    valid_from date NOT NULL,
    valid_to date,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_account_mandate_party_dates CHECK (((valid_to IS NULL) OR (valid_to >= valid_from))),
    CONSTRAINT ck_account_mandate_party_role CHECK (((role)::text = 'SIGNATORY'::text)),
    CONSTRAINT ck_account_mandate_party_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: account_mandate_party_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account_mandate_party ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_mandate_party_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_mandate_rule; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_mandate_rule (
    id bigint NOT NULL,
    mandate_id bigint NOT NULL,
    operation_type character varying(30) NOT NULL,
    required_signatures smallint NOT NULL,
    valid_from date NOT NULL,
    valid_to date,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_account_mandate_rule_dates CHECK (((valid_to IS NULL) OR (valid_to >= valid_from))),
    CONSTRAINT ck_account_mandate_rule_operation CHECK (((operation_type)::text = ANY ((ARRAY['WITHDRAWAL'::character varying, 'TRANSFER'::character varying, 'PAYMENT'::character varying, 'CLOSE_ACCOUNT'::character varying])::text[]))),
    CONSTRAINT ck_account_mandate_rule_required_signatures CHECK ((required_signatures >= 1)),
    CONSTRAINT ck_account_mandate_rule_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: account_mandate_rule_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account_mandate_rule ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_mandate_rule_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_owner; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_owner (
    id bigint NOT NULL,
    account_id bigint NOT NULL,
    customer_id bigint NOT NULL,
    valid_from date NOT NULL,
    valid_to date,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_account_owner_dates CHECK (((valid_to IS NULL) OR (valid_to >= valid_from))),
    CONSTRAINT ck_account_owner_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: account_owner_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account_owner ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_owner_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_type; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_type (
    id bigint NOT NULL,
    code character(2) NOT NULL,
    name character varying(100) NOT NULL,
    currency_id bigint NOT NULL,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_account_type_code CHECK ((code ~ '^[0-9]{2}$'::text)),
    CONSTRAINT ck_account_type_name CHECK (((name)::text <> ''::text)),
    CONSTRAINT ck_account_type_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: account_type_account_serial; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.account_type_account_serial (
    account_type_id bigint NOT NULL,
    last_serial bigint DEFAULT 0 NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_account_type_account_serial_range CHECK (((last_serial >= 0) AND (last_serial <= '9999999999'::bigint)))
);


--
-- Name: account_type_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.account_type ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.account_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: branch; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.branch (
    id bigint NOT NULL,
    branch_code character(7) NOT NULL,
    name character varying(150) NOT NULL,
    city_id bigint NOT NULL,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_branch_code CHECK ((branch_code ~ '^[0-9]{7}$'::text)),
    CONSTRAINT ck_branch_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying, 'CLOSED'::character varying])::text[])))
);


--
-- Name: branch_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.branch ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.branch_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: city; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.city (
    id bigint NOT NULL,
    province_id bigint NOT NULL,
    code character varying(20) NOT NULL,
    name character varying(100) NOT NULL
);


--
-- Name: city_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.city ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.city_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: country; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.country (
    id bigint NOT NULL,
    code character(3) NOT NULL,
    name character varying(100) NOT NULL,
    CONSTRAINT ck_country_code CHECK ((code ~ '^[0-9]{3}$'::text))
);


--
-- Name: country_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.country ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.country_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: currency; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.currency (
    id bigint NOT NULL,
    code character(3) NOT NULL,
    numeric_code character(3) NOT NULL,
    name character varying(100) NOT NULL,
    minor_unit smallint NOT NULL,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_currency_code CHECK ((code ~ '^[A-Z]{3}$'::text)),
    CONSTRAINT ck_currency_minor_unit CHECK (((minor_unit >= 0) AND (minor_unit <= 6))),
    CONSTRAINT ck_currency_numeric_code CHECK ((numeric_code ~ '^[0-9]{3}$'::text)),
    CONSTRAINT ck_currency_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: currency_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.currency ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.currency_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: customer; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.customer (
    id bigint NOT NULL,
    customer_number character(10) NOT NULL,
    customer_type character varying(20) NOT NULL,
    nationality_code character(3) NOT NULL,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_customer_nationality_code CHECK ((nationality_code ~ '^[0-9]{3}$'::text)),
    CONSTRAINT ck_customer_number CHECK ((customer_number ~ '^[0-9]{10}$'::text)),
    CONSTRAINT ck_customer_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying, 'BLOCKED'::character varying, 'CLOSED'::character varying])::text[]))),
    CONSTRAINT ck_customer_type CHECK (((customer_type)::text = ANY ((ARRAY['INDIVIDUAL'::character varying, 'LEGAL_ENTITY'::character varying])::text[])))
);


--
-- Name: customer_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE public.customer_id_seq
    START WITH 1003
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: customer_id_seq1; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.customer ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.customer_id_seq1
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: individual; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.individual (
    customer_id bigint NOT NULL,
    national_id character varying(20) NOT NULL,
    first_name character varying(100) NOT NULL,
    last_name character varying(100) NOT NULL,
    CONSTRAINT ck_individual_first_name CHECK (((first_name)::text <> ''::text)),
    CONSTRAINT ck_individual_last_name CHECK (((last_name)::text <> ''::text)),
    CONSTRAINT ck_individual_national_id CHECK (((national_id)::text <> ''::text))
);


--
-- Name: journal; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.journal (
    id bigint NOT NULL,
    transaction_id bigint NOT NULL,
    posting_date date DEFAULT CURRENT_DATE NOT NULL,
    value_date date DEFAULT CURRENT_DATE NOT NULL,
    status character varying(20) DEFAULT 'DRAFT'::character varying NOT NULL,
    description character varying(500),
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_journal_status CHECK (((status)::text = ANY ((ARRAY['DRAFT'::character varying, 'POSTED'::character varying, 'REVERSED'::character varying])::text[])))
);


--
-- Name: journal_entry; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.journal_entry (
    id bigint NOT NULL,
    journal_id bigint NOT NULL,
    sequence_no integer NOT NULL,
    ledger_account_id bigint NOT NULL,
    account_id bigint,
    currency_id bigint NOT NULL,
    debit_amount numeric DEFAULT 0 NOT NULL,
    credit_amount numeric DEFAULT 0 NOT NULL,
    description character varying(500),
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_journal_entry_credit CHECK ((credit_amount >= (0)::numeric)),
    CONSTRAINT ck_journal_entry_debit CHECK ((debit_amount >= (0)::numeric)),
    CONSTRAINT ck_journal_entry_debit_or_credit CHECK ((((debit_amount > (0)::numeric) AND (credit_amount = (0)::numeric)) OR ((debit_amount = (0)::numeric) AND (credit_amount > (0)::numeric)))),
    CONSTRAINT ck_journal_entry_sequence CHECK ((sequence_no > 0))
);


--
-- Name: journal_entry_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.journal_entry ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.journal_entry_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: journal_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.journal ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.journal_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: ledger_account; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.ledger_account (
    id bigint NOT NULL,
    code character(8) NOT NULL,
    name character varying(200) NOT NULL,
    account_category character varying(20) NOT NULL,
    parent_id bigint,
    currency_id bigint NOT NULL,
    is_postable boolean DEFAULT true NOT NULL,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_ledger_account_category CHECK (((account_category)::text = ANY ((ARRAY['ASSET'::character varying, 'LIABILITY'::character varying, 'EQUITY'::character varying, 'REVENUE'::character varying, 'EXPENSE'::character varying])::text[]))),
    CONSTRAINT ck_ledger_account_code CHECK ((code ~ '^[0-9]{8}$'::text)),
    CONSTRAINT ck_ledger_account_name CHECK (((name)::text <> ''::text)),
    CONSTRAINT ck_ledger_account_not_self_parent CHECK (((parent_id IS NULL) OR (parent_id <> id))),
    CONSTRAINT ck_ledger_account_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: ledger_account_balance; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.ledger_account_balance (
    ledger_account_id bigint NOT NULL,
    debit_total numeric DEFAULT 0 NOT NULL,
    credit_total numeric DEFAULT 0 NOT NULL,
    balance numeric DEFAULT 0 NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_ledger_account_balance_credit CHECK ((credit_total >= (0)::numeric)),
    CONSTRAINT ck_ledger_account_balance_debit CHECK ((debit_total >= (0)::numeric))
);


--
-- Name: ledger_account_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.ledger_account ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.ledger_account_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: legal_entity; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.legal_entity (
    customer_id bigint NOT NULL,
    legal_id character varying(30) NOT NULL,
    legal_name character varying(200) NOT NULL,
    registration_number character varying(50),
    CONSTRAINT ck_legal_entity_legal_id CHECK (((legal_id)::text <> ''::text)),
    CONSTRAINT ck_legal_entity_legal_name CHECK (((legal_name)::text <> ''::text))
);


--
-- Name: legal_entity_board_member; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.legal_entity_board_member (
    id bigint NOT NULL,
    legal_entity_customer_id bigint NOT NULL,
    customer_id bigint NOT NULL,
    "position" character varying(100) NOT NULL,
    start_date date NOT NULL,
    end_date date,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_board_member_dates CHECK (((end_date IS NULL) OR (end_date >= start_date))),
    CONSTRAINT ck_board_member_not_same_customer CHECK ((legal_entity_customer_id <> customer_id)),
    CONSTRAINT ck_board_member_position CHECK ((("position")::text <> ''::text)),
    CONSTRAINT ck_board_member_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: legal_entity_board_member_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.legal_entity_board_member ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.legal_entity_board_member_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: legal_entity_shareholder; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.legal_entity_shareholder (
    id bigint NOT NULL,
    legal_entity_customer_id bigint NOT NULL,
    shareholder_customer_id bigint NOT NULL,
    ownership_percentage numeric(9,6),
    share_count numeric(20,6),
    start_date date NOT NULL,
    end_date date,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_shareholder_dates CHECK (((end_date IS NULL) OR (end_date >= start_date))),
    CONSTRAINT ck_shareholder_not_same_customer CHECK ((legal_entity_customer_id <> shareholder_customer_id)),
    CONSTRAINT ck_shareholder_ownership_percentage CHECK (((ownership_percentage IS NULL) OR ((ownership_percentage > (0)::numeric) AND (ownership_percentage <= (100)::numeric)))),
    CONSTRAINT ck_shareholder_share_count CHECK (((share_count IS NULL) OR (share_count > (0)::numeric))),
    CONSTRAINT ck_shareholder_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: legal_entity_shareholder_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.legal_entity_shareholder ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.legal_entity_shareholder_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: product; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.product (
    id bigint NOT NULL,
    code character varying(50) NOT NULL,
    name character varying(150) NOT NULL,
    account_type_id bigint NOT NULL,
    currency_id bigint NOT NULL,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    effective_from date NOT NULL,
    effective_to date,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_product_code CHECK (((code)::text <> ''::text)),
    CONSTRAINT ck_product_effective_dates CHECK (((effective_to IS NULL) OR (effective_to >= effective_from))),
    CONSTRAINT ck_product_name CHECK (((name)::text <> ''::text)),
    CONSTRAINT ck_product_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: product_balance_rule; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.product_balance_rule (
    id bigint NOT NULL,
    product_id bigint NOT NULL,
    minimum_balance numeric DEFAULT 0 NOT NULL,
    overdraft_enabled boolean DEFAULT false NOT NULL,
    overdraft_limit numeric DEFAULT 0 NOT NULL,
    effective_from timestamp with time zone NOT NULL,
    effective_to timestamp with time zone,
    created_at timestamp with time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_product_balance_rule_effective_dates CHECK (((effective_to IS NULL) OR (effective_to > effective_from))),
    CONSTRAINT ck_product_balance_rule_minimum CHECK ((minimum_balance >= (0)::numeric)),
    CONSTRAINT ck_product_balance_rule_overdraft_disabled CHECK ((overdraft_enabled OR (overdraft_limit = (0)::numeric))),
    CONSTRAINT ck_product_balance_rule_overdraft_limit CHECK ((overdraft_limit >= (0)::numeric))
);


--
-- Name: product_balance_rule_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.product_balance_rule ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.product_balance_rule_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: product_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.product ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.product_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: product_ledger_mapping; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.product_ledger_mapping (
    id bigint NOT NULL,
    product_id bigint NOT NULL,
    account_type_id bigint NOT NULL,
    currency_id bigint NOT NULL,
    ledger_account_id bigint NOT NULL,
    status character varying(20) DEFAULT 'ACTIVE'::character varying NOT NULL,
    effective_from date NOT NULL,
    effective_to date,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_plm_effective_dates CHECK (((effective_to IS NULL) OR (effective_to > effective_from))),
    CONSTRAINT ck_plm_status CHECK (((status)::text = ANY ((ARRAY['ACTIVE'::character varying, 'INACTIVE'::character varying])::text[])))
);


--
-- Name: product_ledger_mapping_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.product_ledger_mapping ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.product_ledger_mapping_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: province; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.province (
    id bigint NOT NULL,
    country_id bigint NOT NULL,
    code character varying(20) NOT NULL,
    name character varying(100) NOT NULL
);


--
-- Name: province_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.province ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.province_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: transaction; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.transaction (
    id bigint NOT NULL,
    transaction_number character varying(50) NOT NULL,
    transaction_type character varying(20) NOT NULL,
    status character varying(20) DEFAULT 'INITIATED'::character varying NOT NULL,
    channel character varying(20) NOT NULL,
    amount numeric NOT NULL,
    currency_id bigint NOT NULL,
    reverses_transaction_id bigint,
    initiated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    completed_at timestamp without time zone,
    description character varying(500),
    reference_number character varying(100),
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    idempotency_key character varying(100),
    request_fingerprint character varying(64),
    result_code character varying(100),
    result_message text,
    CONSTRAINT ck_transaction_amount CHECK ((amount > (0)::numeric)),
    CONSTRAINT ck_transaction_channel CHECK (((channel)::text = ANY ((ARRAY['BRANCH'::character varying, 'ATM'::character varying, 'MOBILE'::character varying, 'INTERNET'::character varying, 'API'::character varying, 'SYSTEM'::character varying])::text[]))),
    CONSTRAINT ck_transaction_completed_at CHECK (((completed_at IS NULL) OR (completed_at >= initiated_at))),
    CONSTRAINT ck_transaction_not_self_reversal CHECK (((reverses_transaction_id IS NULL) OR (reverses_transaction_id <> id))),
    CONSTRAINT ck_transaction_number CHECK (((transaction_number)::text <> ''::text)),
    CONSTRAINT ck_transaction_reversal_reference CHECK ((((transaction_type)::text <> 'REVERSAL'::text) OR (reverses_transaction_id IS NOT NULL))),
    CONSTRAINT ck_transaction_status CHECK (((status)::text = ANY ((ARRAY['INITIATED'::character varying, 'PROCESSING'::character varying, 'COMPLETED'::character varying, 'REJECTED'::character varying, 'FAILED'::character varying, 'CANCELLED'::character varying, 'REVERSED'::character varying])::text[]))),
    CONSTRAINT ck_transaction_type CHECK (((transaction_type)::text = ANY ((ARRAY['DEPOSIT'::character varying, 'WITHDRAWAL'::character varying, 'TRANSFER'::character varying, 'PAYMENT'::character varying, 'FEE'::character varying, 'REVERSAL'::character varying])::text[])))
);


--
-- Name: transaction_entry; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.transaction_entry (
    id bigint NOT NULL,
    transaction_id bigint NOT NULL,
    account_id bigint NOT NULL,
    entry_type character varying(20) NOT NULL,
    amount numeric NOT NULL,
    currency_id bigint NOT NULL,
    sequence_no integer NOT NULL,
    description character varying(500),
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_transaction_entry_amount CHECK ((amount > (0)::numeric)),
    CONSTRAINT ck_transaction_entry_sequence CHECK ((sequence_no > 0)),
    CONSTRAINT ck_transaction_entry_type CHECK (((entry_type)::text = ANY ((ARRAY['SOURCE'::character varying, 'DESTINATION'::character varying])::text[])))
);


--
-- Name: transaction_entry_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.transaction_entry ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.transaction_entry_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: transaction_fee; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.transaction_fee (
    id bigint NOT NULL,
    transaction_id bigint NOT NULL,
    fee_type character varying(50) NOT NULL,
    amount numeric NOT NULL,
    currency_id bigint NOT NULL,
    ledger_account_id bigint NOT NULL,
    created_at timestamp without time zone DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT ck_transaction_fee_amount CHECK ((amount > (0)::numeric)),
    CONSTRAINT ck_transaction_fee_type CHECK (((fee_type)::text <> ''::text))
);


--
-- Name: transaction_fee_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.transaction_fee ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.transaction_fee_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: transaction_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE public.transaction_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: transaction_id_seq1; Type: SEQUENCE; Schema: public; Owner: -
--

ALTER TABLE public.transaction ALTER COLUMN id ADD GENERATED BY DEFAULT AS IDENTITY (
    SEQUENCE NAME public.transaction_id_seq1
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1
);


--
-- Name: account_authorization account_authorization_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_authorization
    ADD CONSTRAINT account_authorization_pkey PRIMARY KEY (id);


--
-- Name: account_balance account_balance_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_balance
    ADD CONSTRAINT account_balance_pkey PRIMARY KEY (account_id);


--
-- Name: account_balance_rule_override account_balance_rule_override_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_balance_rule_override
    ADD CONSTRAINT account_balance_rule_override_pkey PRIMARY KEY (id);


--
-- Name: account_hold_events account_hold_events_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_hold_events
    ADD CONSTRAINT account_hold_events_pkey PRIMARY KEY (id);


--
-- Name: account_hold account_hold_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_hold
    ADD CONSTRAINT account_hold_pkey PRIMARY KEY (id);


--
-- Name: account_mandate_party account_mandate_party_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_mandate_party
    ADD CONSTRAINT account_mandate_party_pkey PRIMARY KEY (id);


--
-- Name: account_mandate account_mandate_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_mandate
    ADD CONSTRAINT account_mandate_pkey PRIMARY KEY (id);


--
-- Name: account_mandate_rule account_mandate_rule_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_mandate_rule
    ADD CONSTRAINT account_mandate_rule_pkey PRIMARY KEY (id);


--
-- Name: account_owner account_owner_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_owner
    ADD CONSTRAINT account_owner_pkey PRIMARY KEY (id);


--
-- Name: account account_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT account_pkey PRIMARY KEY (id);


--
-- Name: account_type_account_serial account_type_account_serial_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_type_account_serial
    ADD CONSTRAINT account_type_account_serial_pkey PRIMARY KEY (account_type_id);


--
-- Name: account_type account_type_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_type
    ADD CONSTRAINT account_type_pkey PRIMARY KEY (id);


--
-- Name: branch branch_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.branch
    ADD CONSTRAINT branch_pkey PRIMARY KEY (id);


--
-- Name: city city_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.city
    ADD CONSTRAINT city_pkey PRIMARY KEY (id);


--
-- Name: country country_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.country
    ADD CONSTRAINT country_pkey PRIMARY KEY (id);


--
-- Name: currency currency_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.currency
    ADD CONSTRAINT currency_pkey PRIMARY KEY (id);


--
-- Name: customer customer_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.customer
    ADD CONSTRAINT customer_pkey PRIMARY KEY (id);


--
-- Name: account_balance_rule_override ex_account_balance_override_minimum_period; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_balance_rule_override
    ADD CONSTRAINT ex_account_balance_override_minimum_period EXCLUDE USING gist (account_id WITH =, tstzrange(effective_from, effective_to, '[)'::text) WITH &&) WHERE ((minimum_balance IS NOT NULL));


--
-- Name: account_balance_rule_override ex_account_balance_override_overdraft_period; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_balance_rule_override
    ADD CONSTRAINT ex_account_balance_override_overdraft_period EXCLUDE USING gist (account_id WITH =, tstzrange(effective_from, effective_to, '[)'::text) WITH &&) WHERE ((overdraft_enabled IS NOT NULL));


--
-- Name: product_ledger_mapping ex_plm_product_periods; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product_ledger_mapping
    ADD CONSTRAINT ex_plm_product_periods EXCLUDE USING gist (product_id WITH =, daterange(effective_from, effective_to, '[)'::text) WITH &&);


--
-- Name: product_balance_rule ex_product_balance_rule_period; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product_balance_rule
    ADD CONSTRAINT ex_product_balance_rule_period EXCLUDE USING gist (product_id WITH =, tstzrange(effective_from, effective_to, '[)'::text) WITH &&);


--
-- Name: individual individual_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.individual
    ADD CONSTRAINT individual_pkey PRIMARY KEY (customer_id);


--
-- Name: journal_entry journal_entry_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal_entry
    ADD CONSTRAINT journal_entry_pkey PRIMARY KEY (id);


--
-- Name: journal journal_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal
    ADD CONSTRAINT journal_pkey PRIMARY KEY (id);


--
-- Name: ledger_account_balance ledger_account_balance_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.ledger_account_balance
    ADD CONSTRAINT ledger_account_balance_pkey PRIMARY KEY (ledger_account_id);


--
-- Name: ledger_account ledger_account_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.ledger_account
    ADD CONSTRAINT ledger_account_pkey PRIMARY KEY (id);


--
-- Name: legal_entity_board_member legal_entity_board_member_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.legal_entity_board_member
    ADD CONSTRAINT legal_entity_board_member_pkey PRIMARY KEY (id);


--
-- Name: legal_entity legal_entity_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.legal_entity
    ADD CONSTRAINT legal_entity_pkey PRIMARY KEY (customer_id);


--
-- Name: legal_entity_shareholder legal_entity_shareholder_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.legal_entity_shareholder
    ADD CONSTRAINT legal_entity_shareholder_pkey PRIMARY KEY (id);


--
-- Name: product_balance_rule product_balance_rule_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product_balance_rule
    ADD CONSTRAINT product_balance_rule_pkey PRIMARY KEY (id);


--
-- Name: product_ledger_mapping product_ledger_mapping_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product_ledger_mapping
    ADD CONSTRAINT product_ledger_mapping_pkey PRIMARY KEY (id);


--
-- Name: product product_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product
    ADD CONSTRAINT product_pkey PRIMARY KEY (id);


--
-- Name: province province_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.province
    ADD CONSTRAINT province_pkey PRIMARY KEY (id);


--
-- Name: transaction_entry transaction_entry_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_entry
    ADD CONSTRAINT transaction_entry_pkey PRIMARY KEY (id);


--
-- Name: transaction_fee transaction_fee_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_fee
    ADD CONSTRAINT transaction_fee_pkey PRIMARY KEY (id);


--
-- Name: transaction transaction_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction
    ADD CONSTRAINT transaction_pkey PRIMARY KEY (id);


--
-- Name: account_hold_events uq_account_hold_event_idempotency; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_hold_events
    ADD CONSTRAINT uq_account_hold_event_idempotency UNIQUE (hold_id, idempotency_key);


--
-- Name: account uq_account_id_currency; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT uq_account_id_currency UNIQUE (id, currency_id);


--
-- Name: account uq_account_id_ledger_currency; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT uq_account_id_ledger_currency UNIQUE (id, ledger_account_id, currency_id);


--
-- Name: account uq_account_number; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT uq_account_number UNIQUE (account_number);


--
-- Name: account_type uq_account_type_code; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_type
    ADD CONSTRAINT uq_account_type_code UNIQUE (code);


--
-- Name: account_type uq_account_type_id_currency; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_type
    ADD CONSTRAINT uq_account_type_id_currency UNIQUE (id, currency_id);


--
-- Name: branch uq_branch_code; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.branch
    ADD CONSTRAINT uq_branch_code UNIQUE (branch_code);


--
-- Name: city uq_city_code; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.city
    ADD CONSTRAINT uq_city_code UNIQUE (code);


--
-- Name: country uq_country_code; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.country
    ADD CONSTRAINT uq_country_code UNIQUE (code);


--
-- Name: country uq_country_name; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.country
    ADD CONSTRAINT uq_country_name UNIQUE (name);


--
-- Name: currency uq_currency_code; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.currency
    ADD CONSTRAINT uq_currency_code UNIQUE (code);


--
-- Name: currency uq_currency_numeric_code; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.currency
    ADD CONSTRAINT uq_currency_numeric_code UNIQUE (numeric_code);


--
-- Name: customer uq_customer_number; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.customer
    ADD CONSTRAINT uq_customer_number UNIQUE (customer_number);


--
-- Name: individual uq_individual_national_id; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.individual
    ADD CONSTRAINT uq_individual_national_id UNIQUE (national_id);


--
-- Name: journal_entry uq_journal_entry_sequence; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal_entry
    ADD CONSTRAINT uq_journal_entry_sequence UNIQUE (journal_id, sequence_no);


--
-- Name: ledger_account uq_ledger_account_code; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.ledger_account
    ADD CONSTRAINT uq_ledger_account_code UNIQUE (code);


--
-- Name: ledger_account uq_ledger_account_id_currency; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.ledger_account
    ADD CONSTRAINT uq_ledger_account_id_currency UNIQUE (id, currency_id);


--
-- Name: legal_entity uq_legal_entity_legal_id; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.legal_entity
    ADD CONSTRAINT uq_legal_entity_legal_id UNIQUE (legal_id);


--
-- Name: product_ledger_mapping uq_plm_account_reference; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product_ledger_mapping
    ADD CONSTRAINT uq_plm_account_reference UNIQUE (id, product_id, account_type_id, currency_id, ledger_account_id);


--
-- Name: product uq_product_code; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product
    ADD CONSTRAINT uq_product_code UNIQUE (code);


--
-- Name: product uq_product_id_type_currency; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product
    ADD CONSTRAINT uq_product_id_type_currency UNIQUE (id, account_type_id, currency_id);


--
-- Name: province uq_province_country_code; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.province
    ADD CONSTRAINT uq_province_country_code UNIQUE (country_id, code);


--
-- Name: transaction_entry uq_transaction_entry_sequence; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_entry
    ADD CONSTRAINT uq_transaction_entry_sequence UNIQUE (transaction_id, sequence_no);


--
-- Name: transaction uq_transaction_id_currency; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction
    ADD CONSTRAINT uq_transaction_id_currency UNIQUE (id, currency_id);


--
-- Name: transaction uq_transaction_number; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction
    ADD CONSTRAINT uq_transaction_number UNIQUE (transaction_number);


--
-- Name: idx_account_account_type_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_account_type_id ON public.account USING btree (account_type_id);


--
-- Name: idx_account_authorization_customer_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_authorization_customer_id ON public.account_authorization USING btree (customer_id);


--
-- Name: idx_account_currency_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_currency_id ON public.account USING btree (currency_id);


--
-- Name: idx_account_ledger_account_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_ledger_account_id ON public.account USING btree (ledger_account_id);


--
-- Name: idx_account_mandate_account_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_mandate_account_id ON public.account_mandate USING btree (account_id);


--
-- Name: idx_account_mandate_party_customer_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_mandate_party_customer_id ON public.account_mandate_party USING btree (customer_id);


--
-- Name: idx_account_mandate_party_status; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_mandate_party_status ON public.account_mandate_party USING btree (status);


--
-- Name: idx_account_mandate_rule_status; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_mandate_rule_status ON public.account_mandate_rule USING btree (status);


--
-- Name: idx_account_mandate_status; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_mandate_status ON public.account_mandate USING btree (status);


--
-- Name: idx_account_opening_branch_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_opening_branch_id ON public.account USING btree (opening_branch_id);


--
-- Name: idx_account_owner_customer_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_owner_customer_id ON public.account_owner USING btree (customer_id);


--
-- Name: idx_account_product_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_product_id ON public.account USING btree (product_id);


--
-- Name: idx_account_product_ledger_mapping; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_product_ledger_mapping ON public.account USING btree (product_ledger_mapping_id);


--
-- Name: idx_account_status; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_status ON public.account USING btree (status);


--
-- Name: idx_account_type_currency_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_account_type_currency_id ON public.account_type USING btree (currency_id);


--
-- Name: idx_board_member_customer; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_board_member_customer ON public.legal_entity_board_member USING btree (customer_id);


--
-- Name: idx_board_member_legal_entity; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_board_member_legal_entity ON public.legal_entity_board_member USING btree (legal_entity_customer_id);


--
-- Name: idx_board_member_status; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_board_member_status ON public.legal_entity_board_member USING btree (status);


--
-- Name: idx_branch_city_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_branch_city_id ON public.branch USING btree (city_id);


--
-- Name: idx_city_province_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_city_province_id ON public.city USING btree (province_id);


--
-- Name: idx_customer_nationality_code; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_customer_nationality_code ON public.customer USING btree (nationality_code);


--
-- Name: idx_customer_status; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_customer_status ON public.customer USING btree (status);


--
-- Name: idx_journal_entry_account_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_journal_entry_account_id ON public.journal_entry USING btree (account_id);


--
-- Name: idx_journal_entry_currency_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_journal_entry_currency_id ON public.journal_entry USING btree (currency_id);


--
-- Name: idx_journal_entry_journal_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_journal_entry_journal_id ON public.journal_entry USING btree (journal_id);


--
-- Name: idx_journal_entry_ledger_account_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_journal_entry_ledger_account_id ON public.journal_entry USING btree (ledger_account_id);


--
-- Name: idx_journal_posting_date; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_journal_posting_date ON public.journal USING btree (posting_date);


--
-- Name: idx_journal_status; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_journal_status ON public.journal USING btree (status);


--
-- Name: idx_journal_transaction_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_journal_transaction_id ON public.journal USING btree (transaction_id);


--
-- Name: idx_journal_value_date; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_journal_value_date ON public.journal USING btree (value_date);


--
-- Name: idx_ledger_account_currency_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_ledger_account_currency_id ON public.ledger_account USING btree (currency_id);


--
-- Name: idx_ledger_account_parent_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_ledger_account_parent_id ON public.ledger_account USING btree (parent_id);


--
-- Name: idx_plm_ledger_account; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_plm_ledger_account ON public.product_ledger_mapping USING btree (ledger_account_id);


--
-- Name: idx_plm_product; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_plm_product ON public.product_ledger_mapping USING btree (product_id);


--
-- Name: idx_product_account_type_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_product_account_type_id ON public.product USING btree (account_type_id);


--
-- Name: idx_product_currency_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_product_currency_id ON public.product USING btree (currency_id);


--
-- Name: idx_province_country_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_province_country_id ON public.province USING btree (country_id);


--
-- Name: idx_shareholder_customer; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_shareholder_customer ON public.legal_entity_shareholder USING btree (shareholder_customer_id);


--
-- Name: idx_shareholder_legal_entity; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_shareholder_legal_entity ON public.legal_entity_shareholder USING btree (legal_entity_customer_id);


--
-- Name: idx_shareholder_status; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_shareholder_status ON public.legal_entity_shareholder USING btree (status);


--
-- Name: idx_transaction_channel; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_channel ON public.transaction USING btree (channel);


--
-- Name: idx_transaction_currency_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_currency_id ON public.transaction USING btree (currency_id);


--
-- Name: idx_transaction_entry_account_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_entry_account_id ON public.transaction_entry USING btree (account_id);


--
-- Name: idx_transaction_entry_currency_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_entry_currency_id ON public.transaction_entry USING btree (currency_id);


--
-- Name: idx_transaction_entry_transaction_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_entry_transaction_id ON public.transaction_entry USING btree (transaction_id);


--
-- Name: idx_transaction_fee_currency_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_fee_currency_id ON public.transaction_fee USING btree (currency_id);


--
-- Name: idx_transaction_fee_ledger_account_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_fee_ledger_account_id ON public.transaction_fee USING btree (ledger_account_id);


--
-- Name: idx_transaction_fee_transaction_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_fee_transaction_id ON public.transaction_fee USING btree (transaction_id);


--
-- Name: idx_transaction_initiated_at; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_initiated_at ON public.transaction USING btree (initiated_at);


--
-- Name: idx_transaction_reference_number; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_reference_number ON public.transaction USING btree (reference_number);


--
-- Name: idx_transaction_reverses_transaction_id; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_reverses_transaction_id ON public.transaction USING btree (reverses_transaction_id);


--
-- Name: idx_transaction_status; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_status ON public.transaction USING btree (status);


--
-- Name: idx_transaction_type; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX idx_transaction_type ON public.transaction USING btree (transaction_type);


--
-- Name: ix_account_balance_override_lookup; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX ix_account_balance_override_lookup ON public.account_balance_rule_override USING btree (account_id, effective_from DESC);


--
-- Name: ix_account_hold_account; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX ix_account_hold_account ON public.account_hold USING btree (account_id);


--
-- Name: ix_account_hold_events_hold; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX ix_account_hold_events_hold ON public.account_hold_events USING btree (hold_id, id);


--
-- Name: ix_account_hold_reference; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX ix_account_hold_reference ON public.account_hold USING btree (reference_type, reference_id) WHERE ((reference_type IS NOT NULL) AND (reference_id IS NOT NULL));


--
-- Name: ix_product_balance_rule_lookup; Type: INDEX; Schema: public; Owner: -
--

CREATE INDEX ix_product_balance_rule_lookup ON public.product_balance_rule USING btree (product_id, effective_from DESC);


--
-- Name: uq_account_authorization_active; Type: INDEX; Schema: public; Owner: -
--

CREATE UNIQUE INDEX uq_account_authorization_active ON public.account_authorization USING btree (account_id, customer_id, authorization_type) WHERE ((status)::text = 'ACTIVE'::text);


--
-- Name: uq_account_mandate_party_active; Type: INDEX; Schema: public; Owner: -
--

CREATE UNIQUE INDEX uq_account_mandate_party_active ON public.account_mandate_party USING btree (mandate_id, customer_id) WHERE ((status)::text = 'ACTIVE'::text);


--
-- Name: uq_account_mandate_rule_active; Type: INDEX; Schema: public; Owner: -
--

CREATE UNIQUE INDEX uq_account_mandate_rule_active ON public.account_mandate_rule USING btree (mandate_id, operation_type) WHERE ((status)::text = 'ACTIVE'::text);


--
-- Name: uq_account_owner_active; Type: INDEX; Schema: public; Owner: -
--

CREATE UNIQUE INDEX uq_account_owner_active ON public.account_owner USING btree (account_id, customer_id) WHERE ((status)::text = 'ACTIVE'::text);


--
-- Name: uq_transaction_api_idempotency; Type: INDEX; Schema: public; Owner: -
--

CREATE UNIQUE INDEX uq_transaction_api_idempotency ON public.transaction USING btree (channel, idempotency_key) WHERE (((channel)::text = 'API'::text) AND (idempotency_key IS NOT NULL));


--
-- Name: account_hold_events trg_account_hold_event_immutable; Type: TRIGGER; Schema: public; Owner: -
--

CREATE TRIGGER trg_account_hold_event_immutable BEFORE DELETE OR UPDATE ON public.account_hold_events FOR EACH ROW EXECUTE FUNCTION public.prevent_account_hold_event_mutation();


--
-- Name: account_hold trg_protect_account_hold; Type: TRIGGER; Schema: public; Owner: -
--

CREATE TRIGGER trg_protect_account_hold BEFORE DELETE OR UPDATE ON public.account_hold FOR EACH ROW EXECUTE FUNCTION public.protect_account_hold();


--
-- Name: account_hold_events trg_validate_account_hold_event; Type: TRIGGER; Schema: public; Owner: -
--

CREATE TRIGGER trg_validate_account_hold_event BEFORE INSERT ON public.account_hold_events FOR EACH ROW EXECUTE FUNCTION public.validate_account_hold_event();


--
-- Name: account fk_account_account_type; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT fk_account_account_type FOREIGN KEY (account_type_id) REFERENCES public.account_type(id);


--
-- Name: account_authorization fk_account_authorization_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_authorization
    ADD CONSTRAINT fk_account_authorization_account FOREIGN KEY (account_id) REFERENCES public.account(id);


--
-- Name: account_authorization fk_account_authorization_customer; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_authorization
    ADD CONSTRAINT fk_account_authorization_customer FOREIGN KEY (customer_id) REFERENCES public.customer(id);


--
-- Name: account_balance fk_account_balance_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_balance
    ADD CONSTRAINT fk_account_balance_account FOREIGN KEY (account_id) REFERENCES public.account(id) ON DELETE CASCADE;


--
-- Name: account_balance_rule_override fk_account_balance_rule_override_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_balance_rule_override
    ADD CONSTRAINT fk_account_balance_rule_override_account FOREIGN KEY (account_id) REFERENCES public.account(id) ON DELETE RESTRICT;


--
-- Name: account fk_account_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT fk_account_currency FOREIGN KEY (currency_id) REFERENCES public.currency(id);


--
-- Name: account_hold fk_account_hold_account_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_hold
    ADD CONSTRAINT fk_account_hold_account_currency FOREIGN KEY (account_id, currency_id) REFERENCES public.account(id, currency_id) ON DELETE RESTRICT;


--
-- Name: account_hold_events fk_account_hold_event_hold; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_hold_events
    ADD CONSTRAINT fk_account_hold_event_hold FOREIGN KEY (hold_id) REFERENCES public.account_hold(id) ON DELETE RESTRICT;


--
-- Name: account fk_account_ledger_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT fk_account_ledger_account FOREIGN KEY (ledger_account_id) REFERENCES public.ledger_account(id);


--
-- Name: account fk_account_ledger_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT fk_account_ledger_currency FOREIGN KEY (ledger_account_id, currency_id) REFERENCES public.ledger_account(id, currency_id);


--
-- Name: account_mandate fk_account_mandate_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_mandate
    ADD CONSTRAINT fk_account_mandate_account FOREIGN KEY (account_id) REFERENCES public.account(id);


--
-- Name: account_mandate_party fk_account_mandate_party_customer; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_mandate_party
    ADD CONSTRAINT fk_account_mandate_party_customer FOREIGN KEY (customer_id) REFERENCES public.customer(id);


--
-- Name: account_mandate_party fk_account_mandate_party_mandate; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_mandate_party
    ADD CONSTRAINT fk_account_mandate_party_mandate FOREIGN KEY (mandate_id) REFERENCES public.account_mandate(id) ON DELETE CASCADE;


--
-- Name: account_mandate_rule fk_account_mandate_rule_mandate; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_mandate_rule
    ADD CONSTRAINT fk_account_mandate_rule_mandate FOREIGN KEY (mandate_id) REFERENCES public.account_mandate(id) ON DELETE CASCADE;


--
-- Name: account fk_account_opening_branch; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT fk_account_opening_branch FOREIGN KEY (opening_branch_id) REFERENCES public.branch(id);


--
-- Name: account_owner fk_account_owner_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_owner
    ADD CONSTRAINT fk_account_owner_account FOREIGN KEY (account_id) REFERENCES public.account(id);


--
-- Name: account_owner fk_account_owner_customer; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_owner
    ADD CONSTRAINT fk_account_owner_customer FOREIGN KEY (customer_id) REFERENCES public.customer(id);


--
-- Name: account fk_account_product_ledger_mapping; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT fk_account_product_ledger_mapping FOREIGN KEY (product_ledger_mapping_id, product_id, account_type_id, currency_id, ledger_account_id) REFERENCES public.product_ledger_mapping(id, product_id, account_type_id, currency_id, ledger_account_id);


--
-- Name: account fk_account_product_type_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT fk_account_product_type_currency FOREIGN KEY (product_id, account_type_id, currency_id) REFERENCES public.product(id, account_type_id, currency_id);


--
-- Name: account_type_account_serial fk_account_type_account_serial_type; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_type_account_serial
    ADD CONSTRAINT fk_account_type_account_serial_type FOREIGN KEY (account_type_id) REFERENCES public.account_type(id);


--
-- Name: account_type fk_account_type_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.account_type
    ADD CONSTRAINT fk_account_type_currency FOREIGN KEY (currency_id) REFERENCES public.currency(id);


--
-- Name: legal_entity_board_member fk_board_member_customer; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.legal_entity_board_member
    ADD CONSTRAINT fk_board_member_customer FOREIGN KEY (customer_id) REFERENCES public.customer(id);


--
-- Name: legal_entity_board_member fk_board_member_legal_entity; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.legal_entity_board_member
    ADD CONSTRAINT fk_board_member_legal_entity FOREIGN KEY (legal_entity_customer_id) REFERENCES public.customer(id);


--
-- Name: branch fk_branch_city; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.branch
    ADD CONSTRAINT fk_branch_city FOREIGN KEY (city_id) REFERENCES public.city(id);


--
-- Name: city fk_city_province; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.city
    ADD CONSTRAINT fk_city_province FOREIGN KEY (province_id) REFERENCES public.province(id);


--
-- Name: customer fk_customer_nationality; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.customer
    ADD CONSTRAINT fk_customer_nationality FOREIGN KEY (nationality_code) REFERENCES public.country(code);


--
-- Name: individual fk_individual_customer; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.individual
    ADD CONSTRAINT fk_individual_customer FOREIGN KEY (customer_id) REFERENCES public.customer(id) ON DELETE CASCADE;


--
-- Name: journal_entry fk_journal_entry_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal_entry
    ADD CONSTRAINT fk_journal_entry_account FOREIGN KEY (account_id) REFERENCES public.account(id);


--
-- Name: journal_entry fk_journal_entry_account_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal_entry
    ADD CONSTRAINT fk_journal_entry_account_currency FOREIGN KEY (account_id, currency_id) REFERENCES public.account(id, currency_id);


--
-- Name: journal_entry fk_journal_entry_account_ledger_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal_entry
    ADD CONSTRAINT fk_journal_entry_account_ledger_currency FOREIGN KEY (account_id, ledger_account_id, currency_id) REFERENCES public.account(id, ledger_account_id, currency_id);


--
-- Name: journal_entry fk_journal_entry_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal_entry
    ADD CONSTRAINT fk_journal_entry_currency FOREIGN KEY (currency_id) REFERENCES public.currency(id);


--
-- Name: journal_entry fk_journal_entry_journal; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal_entry
    ADD CONSTRAINT fk_journal_entry_journal FOREIGN KEY (journal_id) REFERENCES public.journal(id) ON DELETE CASCADE;


--
-- Name: journal_entry fk_journal_entry_ledger_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal_entry
    ADD CONSTRAINT fk_journal_entry_ledger_account FOREIGN KEY (ledger_account_id) REFERENCES public.ledger_account(id);


--
-- Name: journal_entry fk_journal_entry_ledger_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal_entry
    ADD CONSTRAINT fk_journal_entry_ledger_currency FOREIGN KEY (ledger_account_id, currency_id) REFERENCES public.ledger_account(id, currency_id);


--
-- Name: journal fk_journal_transaction; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.journal
    ADD CONSTRAINT fk_journal_transaction FOREIGN KEY (transaction_id) REFERENCES public.transaction(id);


--
-- Name: ledger_account_balance fk_ledger_account_balance_ledger_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.ledger_account_balance
    ADD CONSTRAINT fk_ledger_account_balance_ledger_account FOREIGN KEY (ledger_account_id) REFERENCES public.ledger_account(id) ON DELETE CASCADE;


--
-- Name: ledger_account fk_ledger_account_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.ledger_account
    ADD CONSTRAINT fk_ledger_account_currency FOREIGN KEY (currency_id) REFERENCES public.currency(id);


--
-- Name: ledger_account fk_ledger_account_parent; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.ledger_account
    ADD CONSTRAINT fk_ledger_account_parent FOREIGN KEY (parent_id) REFERENCES public.ledger_account(id);


--
-- Name: ledger_account fk_ledger_account_parent_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.ledger_account
    ADD CONSTRAINT fk_ledger_account_parent_currency FOREIGN KEY (parent_id, currency_id) REFERENCES public.ledger_account(id, currency_id);


--
-- Name: legal_entity fk_legal_entity_customer; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.legal_entity
    ADD CONSTRAINT fk_legal_entity_customer FOREIGN KEY (customer_id) REFERENCES public.customer(id) ON DELETE CASCADE;


--
-- Name: product_ledger_mapping fk_plm_ledger_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product_ledger_mapping
    ADD CONSTRAINT fk_plm_ledger_currency FOREIGN KEY (ledger_account_id, currency_id) REFERENCES public.ledger_account(id, currency_id);


--
-- Name: product_ledger_mapping fk_plm_product_type_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product_ledger_mapping
    ADD CONSTRAINT fk_plm_product_type_currency FOREIGN KEY (product_id, account_type_id, currency_id) REFERENCES public.product(id, account_type_id, currency_id);


--
-- Name: product fk_product_account_type_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product
    ADD CONSTRAINT fk_product_account_type_currency FOREIGN KEY (account_type_id, currency_id) REFERENCES public.account_type(id, currency_id);


--
-- Name: product_balance_rule fk_product_balance_rule_product; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product_balance_rule
    ADD CONSTRAINT fk_product_balance_rule_product FOREIGN KEY (product_id) REFERENCES public.product(id) ON DELETE RESTRICT;


--
-- Name: product fk_product_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.product
    ADD CONSTRAINT fk_product_currency FOREIGN KEY (currency_id) REFERENCES public.currency(id);


--
-- Name: province fk_province_country; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.province
    ADD CONSTRAINT fk_province_country FOREIGN KEY (country_id) REFERENCES public.country(id);


--
-- Name: legal_entity_shareholder fk_shareholder_customer; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.legal_entity_shareholder
    ADD CONSTRAINT fk_shareholder_customer FOREIGN KEY (shareholder_customer_id) REFERENCES public.customer(id);


--
-- Name: legal_entity_shareholder fk_shareholder_legal_entity; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.legal_entity_shareholder
    ADD CONSTRAINT fk_shareholder_legal_entity FOREIGN KEY (legal_entity_customer_id) REFERENCES public.customer(id);


--
-- Name: transaction fk_transaction_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction
    ADD CONSTRAINT fk_transaction_currency FOREIGN KEY (currency_id) REFERENCES public.currency(id);


--
-- Name: transaction_entry fk_transaction_entry_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_entry
    ADD CONSTRAINT fk_transaction_entry_account FOREIGN KEY (account_id) REFERENCES public.account(id);


--
-- Name: transaction_entry fk_transaction_entry_account_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_entry
    ADD CONSTRAINT fk_transaction_entry_account_currency FOREIGN KEY (account_id, currency_id) REFERENCES public.account(id, currency_id);


--
-- Name: transaction_entry fk_transaction_entry_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_entry
    ADD CONSTRAINT fk_transaction_entry_currency FOREIGN KEY (currency_id) REFERENCES public.currency(id);


--
-- Name: transaction_entry fk_transaction_entry_transaction; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_entry
    ADD CONSTRAINT fk_transaction_entry_transaction FOREIGN KEY (transaction_id) REFERENCES public.transaction(id) ON DELETE CASCADE;


--
-- Name: transaction_entry fk_transaction_entry_transaction_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_entry
    ADD CONSTRAINT fk_transaction_entry_transaction_currency FOREIGN KEY (transaction_id, currency_id) REFERENCES public.transaction(id, currency_id);


--
-- Name: transaction_fee fk_transaction_fee_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_fee
    ADD CONSTRAINT fk_transaction_fee_currency FOREIGN KEY (currency_id) REFERENCES public.currency(id);


--
-- Name: transaction_fee fk_transaction_fee_ledger_account; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_fee
    ADD CONSTRAINT fk_transaction_fee_ledger_account FOREIGN KEY (ledger_account_id) REFERENCES public.ledger_account(id);


--
-- Name: transaction_fee fk_transaction_fee_ledger_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_fee
    ADD CONSTRAINT fk_transaction_fee_ledger_currency FOREIGN KEY (ledger_account_id, currency_id) REFERENCES public.ledger_account(id, currency_id);


--
-- Name: transaction_fee fk_transaction_fee_transaction; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_fee
    ADD CONSTRAINT fk_transaction_fee_transaction FOREIGN KEY (transaction_id) REFERENCES public.transaction(id) ON DELETE CASCADE;


--
-- Name: transaction_fee fk_transaction_fee_transaction_currency; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction_fee
    ADD CONSTRAINT fk_transaction_fee_transaction_currency FOREIGN KEY (transaction_id, currency_id) REFERENCES public.transaction(id, currency_id);


--
-- Name: transaction fk_transaction_reverses; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.transaction
    ADD CONSTRAINT fk_transaction_reverses FOREIGN KEY (reverses_transaction_id) REFERENCES public.transaction(id);


--
-- PostgreSQL database dump complete
--

\unrestrict MDr2eXbbDnX8SUsPOY9EXV6pszHbseyZNML4erM6ALhogAznnuvNjLqHdab56vU
