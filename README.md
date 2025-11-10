# SimpleLedgerCpp

A minimal C++ demo implementation of the accounting/ledger system described in the UML diagrams and requirements.

Features:
- Domain models: User, Account, Category, Transaction
- In-memory repositories and services
- Simple auto-categorizer (keyword-based stub) with confidence scoring
- Reporting with ASCII bar charts for category breakdowns
- Search by category and keyword

Build (PowerShell, Windows):

mkdir build; cd build; cmake ..; cmake --build .

Run:
./Debug/simple_ledger.exe  (or check build folder depending on your generator)

This is a small demo to illustrate architecture and functionality from the UML. Extend as needed.
