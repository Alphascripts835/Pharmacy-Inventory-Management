# Pharmacy Inventory Management System (PIMS)

A menu-driven inventory and billing system for a pharmacy, written in pure C (C11, standard library only).
Built for the *Data Structures and Algorithms-1 (BCSE0301)* course project, applying Units 1-5.

## Features
- Add, display, search, update and delete medicines
- Stock management with multiple batches per medicine
- Low-stock alerts (per-medicine reorder level)
- Expiry tracking: expiry report, startup alerts, removal of expired batches
- Billing: cart with undo, tax and discount, FEFO stock deduction, bill history and sales summary
- Inventory report (screen and `inventory_report.txt`)
- Permanent storage in plain-text files (`medicines.dat`, `sales.dat`)
- Input validation on every field (numbers, dates, codes, text length)

## Data structures and algorithms used
| Concept (Unit) | Where it is used |
|---|---|
| Hash table, separate chaining, division method, prime size 101 (U2) | Medicine lookup by code (`inv_find`) |
| Linear search (U2) | Search by name keyword |
| Singly linked list (U3) | Batches of each medicine, kept sorted by expiry date |
| Stack, linked implementation (U4) | Billing cart; undo removes the last line |
| Merge sort, O(n log n), stable (U4/U5) | Sorted listings and reports (name, code, stock, price) |
| Structs, ADT-style API, file handling (U1) | `Medicine`, `Batch`; inventory functions hide the storage |
| Bill formula `P x Q + Tax - Discount` (U4) | Checkout |

## Build and run
```bash
gcc -std=c11 -Wall -Wextra -o pims main.c medicine.c sales.c
./pims            # Windows: pims.exe
```

## Files
- `main.c` - main menu
- `medicine.h / medicine.c` - data structures, validation, storage, inventory operations and reports
- `sales.h / sales.c` - billing cart, checkout, sales history

## Data format
`medicines.dat`: `M|code|name|manufacturer|price|reorderLevel` followed by `B|code|batchNo|qty|expiry` lines.
`sales.dat`: `BILL|id|datetime|subtotal|tax|discount|total` followed by `LINE|id|code|name|qty|price|amount` lines.
Both files are created automatically in the working directory.

## Notes
- Expiry dates use `YYYY-MM-DD`. Expired batches are never sold.
- Stock is sold from the batch expiring first (FEFO).
- Tax rate is `TAX_RATE` in `sales.h` (default 5%).
