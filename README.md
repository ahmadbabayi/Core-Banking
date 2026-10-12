# Core Banking

**An API-driven Core Banking system built with C++, Qt, and PostgreSQL.**

Core Banking is an ongoing software development project focused on building a modular banking backend with customer management, account services, financial transactions, and accounting-oriented transaction records.

The project is being developed incrementally, with an emphasis on clear separation of responsibilities, database consistency, and API-based integration testing.

> **Project status:** Work in progress. The system is not production-ready and must not be used to process real financial transactions.

## Overview

The project aims to provide a maintainable foundation for core banking operations, including:

- Customer registration and management
- Bank account management and account number generation
- Financial transaction processing
- Account transfers
- Transaction entries and journal records
- PostgreSQL-based persistence
- HTTP API endpoints for integration with clients and other services

Feature availability may vary as development progresses.

## Technology Stack

- **Language:** C++17
- **Framework:** Qt 5
- **Database:** PostgreSQL
- **Networking:** Qt Network
- **Database access:** Qt SQL
- **Build system:** CMake
- **API testing:** HTTP requests and JSON

## Architecture

The codebase is organized into distinct areas:

- `src/domain/` — Core domain objects and financial concepts
- `src/application/` — Application services and business operations
- `src/infrastructure/` — Database connectivity and repository implementations
- `src/web/` — HTTP server, request routing, and API controllers
- `database/` — Database schema and SQL migration files

The intended design separates business logic from HTTP handling and database persistence.

## Development Environment

The project has been developed and tested in a Linux environment using Debian, Qt, CMake, GCC, and PostgreSQL.

### Prerequisites

Install the following development tools and libraries for your distribution:

- C++17-compatible compiler
- CMake 3.16 or newer
- Qt 5 development packages for Core, SQL, and Network
- PostgreSQL server and client tools

### Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

Before running the application, configure PostgreSQL and the database connection according to the project's current implementation.

## API

The application includes an HTTP server and API routing infrastructure.

A health endpoint has been used during development:

```http
GET /api/v1/health
```

Example response:

```json
{
  "status": "UP"
}
```

The availability and behavior of individual endpoints depend on the current implementation.

## Development Principles

The project follows these principles:

- Keep business logic separate from transport and persistence layers.
- Maintain consistency between application code and database schemas.
- Use database transactions for operations that require atomic financial posting.
- Validate API inputs and return appropriate HTTP responses.
- Test integration flows through the HTTP API.
- Make changes incrementally and verify builds and tests before committing.

## Roadmap

Planned development areas include:

- Completing customer and account API workflows
- Expanding account relationship management
- Strengthening transaction and transfer processing
- Enforcing financial posting and balance consistency
- Expanding API validation and integration tests
- Improving developer documentation and deployment instructions

The roadmap will evolve as implementation and testing progress.

## Disclaimer

This repository is a software development project intended for learning, experimentation, and ongoing engineering work.

It has not been certified or audited for production banking use. Security, access control, operational resilience, regulatory compliance, reconciliation, and financial correctness require further implementation and independent verification before any real-world deployment.
