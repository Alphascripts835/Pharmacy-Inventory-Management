#ifndef MEDICINE_H
#define MEDICINE_H

#include <stdio.h>
#include <stddef.h>

#define CODE_LEN          16
#define NAME_LEN          50
#define MFR_LEN           40
#define BATCH_LEN         16
#define DATE_LEN          11      /* YYYY-MM-DD + '\0' */
#define HASH_SIZE         101     /* prime table size (Unit 2: division method) */
#define EXPIRY_WARN_DAYS  30
#define DATA_FILE         "medicines.dat"

/* Singly linked list of batches, kept sorted by expiry date (Unit 3) */
typedef struct Batch {
    char batchNo[BATCH_LEN];
    int qty;
    char expiry[DATE_LEN];
    struct Batch *next;
} Batch;

/* Medicine record; chained in a hash table by code (Unit 2) */
typedef struct Medicine {
    char code[CODE_LEN];
    char name[NAME_LEN];
    char manufacturer[MFR_LEN];
    double price;
    int reorderLevel;
    Batch *batches;
    struct Medicine *next;
} Medicine;

/* ---- input helpers (validated) ---- */
void   read_text(const char *prompt, char *buf, size_t size);
int    read_int(const char *prompt, int min, int max);
double read_double(const char *prompt, double min, double max);
void   read_code(const char *prompt, char *buf);
void   read_date(const char *prompt, char *buf, int allowPast);
int    confirm(const char *prompt);

/* ---- utilities ---- */
int  valid_date(const char *s);
int  days_to_expiry(const char *date);
void current_time_string(char *buf, size_t size, int withTime);
int  split_fields(char *line, char *fields[], int max);

/* ---- inventory core ---- */
void      inv_init(void);
void      inv_free(void);
int       inv_load(void);
int       inv_save(void);
Medicine *inv_find(const char *code);
int       med_total_qty(const Medicine *m);
int       med_sellable_qty(const Medicine *m);
int       med_sell(Medicine *m, int qty);

/* ---- menu operations ---- */
void med_add_ui(void);
void med_display_ui(void);
void med_search_ui(void);
void med_update_ui(void);
void med_delete_ui(void);
void med_stock_ui(void);
void med_lowstock_ui(void);
void med_expiry_ui(void);
void med_report_ui(void);
void med_startup_alerts(void);

#endif
