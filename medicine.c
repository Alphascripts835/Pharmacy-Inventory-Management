#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "medicine.h"

#define MAX_QTY 100000
#define MAX_PRICE 100000.0

static Medicine *table[HASH_SIZE];   /* hash table with separate chaining */
static int medCount = 0;

typedef int (*MedCmp)(const Medicine *, const Medicine *);

/* ===================== input helpers ===================== */

static void trim(char *s)
{
    size_t n = strlen(s), i = 0;
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, strlen(s + i) + 1);
}

static void read_line(const char *prompt, char *line, size_t size)
{
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(line, (int)size, stdin)) {
            puts("\nInput closed. Goodbye.");
            exit(0);
        }
        size_t n = strlen(line);
        if (n > 0 && line[n - 1] == '\n') { line[n - 1] = '\0'; trim(line); return; }
        if (feof(stdin)) { trim(line); return; }
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
        puts("Input too long, try again.");
    }
}

void read_text(const char *prompt, char *buf, size_t size)
{
    char line[128];
    for (;;) {
        read_line(prompt, line, sizeof line);
        if (!line[0]) { puts("This field cannot be empty."); continue; }
        if (strchr(line, '|')) { puts("The '|' character is not allowed."); continue; }
        if (strlen(line) >= size) { printf("Maximum %zu characters.\n", size - 1); continue; }
        strcpy(buf, line);
        return;
    }
}

int read_int(const char *prompt, int min, int max)
{
    char line[64], *end;
    for (;;) {
        read_line(prompt, line, sizeof line);
        errno = 0;
        long v = strtol(line, &end, 10);
        if (line[0] && *end == '\0' && errno == 0 && v >= min && v <= max) return (int)v;
        printf("Enter a whole number from %d to %d.\n", min, max);
    }
}

double read_double(const char *prompt, double min, double max)
{
    char line[64], *end;
    for (;;) {
        read_line(prompt, line, sizeof line);
        errno = 0;
        double v = strtod(line, &end);
        if (line[0] && *end == '\0' && errno == 0 && v >= min && v <= max) return v;
        printf("Enter a number from %.2f to %.2f.\n", min, max);
    }
}

void read_code(const char *prompt, char *buf)
{
    for (;;) {
        read_text(prompt, buf, CODE_LEN);
        int ok = 1;
        for (char *p = buf; *p; p++) {
            if (!isalnum((unsigned char)*p) && *p != '-') { ok = 0; break; }
            *p = (char)toupper((unsigned char)*p);
        }
        if (ok) return;
        puts("Use letters, digits and '-' only.");
    }
}

int valid_date(const char *s)
{
    static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (strlen(s) != 10 || s[4] != '-' || s[7] != '-') return 0;
    for (int i = 0; i < 10; i++)
        if (i != 4 && i != 7 && !isdigit((unsigned char)s[i])) return 0;
    int y = atoi(s), m = atoi(s + 5), d = atoi(s + 8);
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1) return 0;
    int max = dim[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) max = 29;
    return d <= max;
}

void read_date(const char *prompt, char *buf, int allowPast)
{
    for (;;) {
        read_text(prompt, buf, DATE_LEN);
        if (!valid_date(buf)) { puts("Invalid date. Use YYYY-MM-DD."); continue; }
        if (!allowPast && days_to_expiry(buf) < 0) { puts("That date is already in the past."); continue; }
        return;
    }
}

int confirm(const char *prompt)
{
    char line[16];
    printf("%s ", prompt);
    for (;;) {
        read_line("(y/n): ", line, sizeof line);
        if (line[0] == 'y' || line[0] == 'Y') return 1;
        if (line[0] == 'n' || line[0] == 'N') return 0;
    }
}

/* ===================== utilities ===================== */

int days_to_expiry(const char *date)
{
    int y = 0, m = 0, d = 0;
    sscanf(date, "%d-%d-%d", &y, &m, &d);
    struct tm e;
    memset(&e, 0, sizeof e);
    e.tm_year = y - 1900; e.tm_mon = m - 1; e.tm_mday = d; e.tm_hour = 12; e.tm_isdst = -1;
    time_t te = mktime(&e);

    time_t now = time(NULL);
    struct tm n = *localtime(&now);
    n.tm_hour = 12; n.tm_min = 0; n.tm_sec = 0; n.tm_isdst = -1;
    time_t tn = mktime(&n);

    double diff = difftime(te, tn) / 86400.0;
    return (int)(diff >= 0 ? diff + 0.5 : diff - 0.5);
}

void current_time_string(char *buf, size_t size, int withTime)
{
    time_t t = time(NULL);
    struct tm *tmv = localtime(&t);
    if (!tmv) { snprintf(buf, size, "unknown"); return; }
    strftime(buf, size, withTime ? "%Y-%m-%d %H:%M" : "%Y-%m-%d", tmv);
}

/* Splits a '|' separated line in place. Returns field count (max+1 if too many). */
int split_fields(char *line, char *fields[], int max)
{
    int n = 0;
    char *p = line;
    fields[n++] = p;
    while ((p = strchr(p, '|')) != NULL) {
        *p++ = '\0';
        if (n == max) return n + 1;
        fields[n++] = p;
    }
    return n;
}

static int icmp(const char *a, const char *b)
{
    while (*a && *b) {
        int x = tolower((unsigned char)*a++), y = tolower((unsigned char)*b++);
        if (x != y) return x - y;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static int icontains(const char *text, const char *key)
{
    size_t k = strlen(key);
    for (; *text; text++) {
        size_t i = 0;
        while (i < k && text[i] && tolower((unsigned char)text[i]) == tolower((unsigned char)key[i])) i++;
        if (i == k) return 1;
    }
    return 0;
}

/* ===================== hash table core ===================== */

static unsigned hash_code(const char *s)
{
    unsigned h = 0;
    while (*s) h = h * 31u + (unsigned char)*s++;
    return h % HASH_SIZE;
}

void inv_init(void)
{
    for (int i = 0; i < HASH_SIZE; i++) table[i] = NULL;
    medCount = 0;
}

static void free_batches(Batch *b)
{
    while (b) { Batch *n = b->next; free(b); b = n; }
}

void inv_free(void)
{
    for (int i = 0; i < HASH_SIZE; i++) {
        Medicine *m = table[i];
        while (m) { Medicine *n = m->next; free_batches(m->batches); free(m); m = n; }
        table[i] = NULL;
    }
    medCount = 0;
}

Medicine *inv_find(const char *code)
{
    for (Medicine *m = table[hash_code(code)]; m; m = m->next)
        if (strcmp(m->code, code) == 0) return m;
    return NULL;
}

static Medicine *create_medicine(const char *code, const char *name, const char *mfr,
                                 double price, int reorder)
{
    if (inv_find(code)) return NULL;
    Medicine *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    strcpy(m->code, code);
    strcpy(m->name, name);
    strcpy(m->manufacturer, mfr);
    m->price = price;
    m->reorderLevel = reorder;
    unsigned h = hash_code(code);
    m->next = table[h];
    table[h] = m;
    medCount++;
    return m;
}

static int remove_medicine(const char *code)
{
    Medicine **pp = &table[hash_code(code)];
    while (*pp) {
        if (strcmp((*pp)->code, code) == 0) {
            Medicine *m = *pp;
            *pp = m->next;
            free_batches(m->batches);
            free(m);
            medCount--;
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

/* Insert into the singly linked batch list, ordered by expiry (earliest first).
   Returns 0 ok, 1 same batch with different expiry, 2 quantity limit, -1 no memory. */
static int add_batch(Medicine *m, const char *batchNo, int qty, const char *expiry)
{
    for (Batch *b = m->batches; b; b = b->next) {
        if (strcmp(b->batchNo, batchNo) == 0) {
            if (strcmp(b->expiry, expiry) != 0) return 1;
            if (b->qty + qty > 1000000) return 2;
            b->qty += qty;
            return 0;
        }
    }
    Batch *nb = malloc(sizeof *nb);
    if (!nb) return -1;
    strcpy(nb->batchNo, batchNo);
    nb->qty = qty;
    strcpy(nb->expiry, expiry);
    Batch **pp = &m->batches;
    while (*pp && strcmp((*pp)->expiry, expiry) <= 0) pp = &(*pp)->next;
    nb->next = *pp;
    *pp = nb;
    return 0;
}

int med_total_qty(const Medicine *m)
{
    int t = 0;
    for (const Batch *b = m->batches; b; b = b->next) t += b->qty;
    return t;
}

int med_sellable_qty(const Medicine *m)
{
    int t = 0;
    for (const Batch *b = m->batches; b; b = b->next)
        if (days_to_expiry(b->expiry) >= 0) t += b->qty;
    return t;
}

/* Sells qty units, earliest-expiry batch first (FEFO). Expired batches are skipped. */
int med_sell(Medicine *m, int qty)
{
    if (qty <= 0 || med_sellable_qty(m) < qty) return 0;
    Batch **pp = &m->batches;
    while (qty > 0 && *pp) {
        Batch *b = *pp;
        if (days_to_expiry(b->expiry) < 0) { pp = &b->next; continue; }
        int take = b->qty < qty ? b->qty : qty;
        b->qty -= take;
        qty -= take;
        if (b->qty == 0) { *pp = b->next; free(b); }
        else pp = &b->next;
    }
    return 1;
}

/* ===================== file storage ===================== */

int inv_save(void)
{
    const char *tmp = DATA_FILE ".tmp";
    FILE *fp = fopen(tmp, "w");
    if (!fp) return 0;
    for (int i = 0; i < HASH_SIZE; i++)
        for (Medicine *m = table[i]; m; m = m->next) {
            fprintf(fp, "M|%s|%s|%s|%.2f|%d\n", m->code, m->name, m->manufacturer, m->price, m->reorderLevel);
            for (Batch *b = m->batches; b; b = b->next)
                fprintf(fp, "B|%s|%s|%d|%s\n", m->code, b->batchNo, b->qty, b->expiry);
        }
    int ok = !ferror(fp);
    if (fclose(fp) != 0) ok = 0;
    if (!ok) { remove(tmp); return 0; }
    if (rename(tmp, DATA_FILE) != 0) {
        remove(DATA_FILE);
        if (rename(tmp, DATA_FILE) != 0) return 0;
    }
    return 1;
}

/* Returns 1 if a file was loaded, 0 if none exists (first run). */
int inv_load(void)
{
    FILE *fp = fopen(DATA_FILE, "r");
    if (!fp) return 0;
    char line[256];
    int bad = 0;
    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        char *f[8];
        int n = split_fields(line, f, 8);
        char *end;
        if (n == 6 && strcmp(f[0], "M") == 0) {
            double price = strtod(f[4], &end);
            if (*end || !(price >= 0)) { bad++; continue; }
            long reorder = strtol(f[5], &end, 10);
            if (*end || reorder < 0 || reorder > MAX_QTY) { bad++; continue; }
            if (!f[1][0] || strlen(f[1]) >= CODE_LEN || strlen(f[2]) >= NAME_LEN || strlen(f[3]) >= MFR_LEN
                || !create_medicine(f[1], f[2], f[3], price, (int)reorder)) bad++;
        } else if (n == 5 && strcmp(f[0], "B") == 0) {
            Medicine *m = inv_find(f[1]);
            long qty = strtol(f[3], &end, 10);
            if (!m || *end || qty <= 0 || qty > 1000000 || !f[2][0] || strlen(f[2]) >= BATCH_LEN
                || !valid_date(f[4]) || add_batch(m, f[2], (int)qty, f[4]) != 0) bad++;
        } else {
            bad++;
        }
    }
    fclose(fp);
    if (bad) printf("Warning: %d invalid line(s) in %s were skipped.\n", bad, DATA_FILE);
    return 1;
}

static void save_or_warn(void)
{
    if (!inv_save()) puts("WARNING: could not save data to file!");
}

/* ===================== sorting (merge sort, Units 4-5) ===================== */

static int cmp_name(const Medicine *a, const Medicine *b)  { return icmp(a->name, b->name); }
static int cmp_code(const Medicine *a, const Medicine *b)  { return strcmp(a->code, b->code); }
static int cmp_qty(const Medicine *a, const Medicine *b)   { return med_total_qty(a) - med_total_qty(b); }
static int cmp_price(const Medicine *a, const Medicine *b) { return (a->price > b->price) - (a->price < b->price); }

static void merge_sort(Medicine **a, Medicine **tmp, int lo, int hi, MedCmp cmp)
{
    if (hi - lo < 2) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort(a, tmp, lo, mid, cmp);
    merge_sort(a, tmp, mid, hi, cmp);
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) tmp[k++] = (cmp(a[j], a[i]) < 0) ? a[j++] : a[i++];
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi) tmp[k++] = a[j++];
    memcpy(a + lo, tmp + lo, (size_t)(hi - lo) * sizeof *a);
}

/* Returns a malloc'd, sorted array of all medicines (caller frees). */
static Medicine **collect_sorted(int *n, MedCmp cmp)
{
    *n = medCount;
    if (medCount == 0) return NULL;
    Medicine **a = malloc(2 * (size_t)medCount * sizeof *a);
    if (!a) return NULL;
    int k = 0;
    for (int i = 0; i < HASH_SIZE; i++)
        for (Medicine *m = table[i]; m; m = m->next) a[k++] = m;
    merge_sort(a, a + medCount, 0, medCount, cmp);
    return a;
}

/* ===================== printing ===================== */

static void rule(int n)
{
    for (int i = 0; i < n; i++) putchar('-');
    putchar('\n');
}

static void print_header(void)
{
    printf("%-10s %-24s %-18s %9s %7s %7s %-6s\n", "Code", "Name", "Manufacturer", "Price", "Stock", "Reorder", "Status");
    rule(87);
}

static void print_row(const Medicine *m)
{
    int t = med_total_qty(m);
    printf("%-10s %-24.24s %-18.18s %9.2f %7d %7d %-6s\n", m->code, m->name, m->manufacturer,
           m->price, t, m->reorderLevel, t == 0 ? "EMPTY" : (t <= m->reorderLevel ? "LOW" : "OK"));
}

static void print_details(const Medicine *m)
{
    printf("\nCode:          %s\nName:          %s\nManufacturer:  %s\nUnit price:    Rs.%.2f\n"
           "Reorder level: %d\nTotal stock:   %d (sellable: %d)\n",
           m->code, m->name, m->manufacturer, m->price, m->reorderLevel,
           med_total_qty(m), med_sellable_qty(m));
    if (!m->batches) { puts("No batches in stock."); return; }
    printf("\n%-16s %8s %-11s %s\n", "Batch", "Qty", "Expiry", "Status");
    rule(50);
    for (const Batch *b = m->batches; b; b = b->next) {
        int d = days_to_expiry(b->expiry);
        if (d < 0) printf("%-16s %8d %-11s EXPIRED (%d day(s) ago)\n", b->batchNo, b->qty, b->expiry, -d);
        else       printf("%-16s %8d %-11s in %d day(s)\n", b->batchNo, b->qty, b->expiry, d);
    }
}

static Medicine *ask_medicine(void)
{
    char code[CODE_LEN];
    read_code("Medicine code: ", code);
    Medicine *m = inv_find(code);
    if (!m) puts("Medicine not found.");
    return m;
}

static double round2(double v) { return (double)(long)(v * 100.0 + 0.5) / 100.0; }

/* ===================== menu operations ===================== */

void med_add_ui(void)
{
    char code[CODE_LEN], name[NAME_LEN], mfr[MFR_LEN];
    read_code("Medicine code: ", code);
    if (inv_find(code)) { puts("A medicine with this code already exists."); return; }
    read_text("Name: ", name, sizeof name);
    read_text("Manufacturer: ", mfr, sizeof mfr);
    double price = round2(read_double("Unit price (Rs.): ", 0.01, MAX_PRICE));
    int reorder = read_int("Reorder (low-stock) level: ", 0, MAX_QTY);
    Medicine *m = create_medicine(code, name, mfr, price, reorder);
    if (!m) { puts("Could not add medicine (out of memory)."); return; }
    if (confirm("Add the first batch now?")) {
        char batch[BATCH_LEN], expiry[DATE_LEN];
        read_text("Batch number: ", batch, sizeof batch);
        int qty = read_int("Quantity: ", 1, MAX_QTY);
        read_date("Expiry date (YYYY-MM-DD): ", expiry, 0);
        if (add_batch(m, batch, qty, expiry) != 0) puts("Could not add batch.");
    }
    save_or_warn();
    puts("Medicine added.");
}

void med_display_ui(void)
{
    if (!medCount) { puts("No medicines in inventory."); return; }
    puts("Sort by: 1) Name  2) Code  3) Stock (lowest first)  4) Price");
    int ch = read_int("Choice: ", 1, 4);
    MedCmp cmps[] = {cmp_name, cmp_code, cmp_qty, cmp_price};
    int n;
    Medicine **a = collect_sorted(&n, cmps[ch - 1]);
    if (!a) { puts("Out of memory."); return; }
    print_header();
    for (int i = 0; i < n; i++) print_row(a[i]);
    printf("Total medicines: %d\n", n);
    free(a);
}

void med_search_ui(void)
{
    puts("Search by: 1) Code (hash lookup)  2) Name keyword (linear search)");
    int ch = read_int("Choice: ", 1, 2);
    if (ch == 1) {
        Medicine *m = ask_medicine();
        if (m) print_details(m);
        return;
    }
    char key[NAME_LEN];
    read_text("Keyword: ", key, sizeof key);
    int n, found = 0;
    Medicine **a = collect_sorted(&n, cmp_name);
    for (int i = 0; i < n; i++) {
        if (!icontains(a[i]->name, key)) continue;
        if (!found++) print_header();
        print_row(a[i]);
    }
    if (!found) puts("No matching medicines.");
    else printf("%d match(es). Use search by code for batch details.\n", found);
    free(a);
}

void med_update_ui(void)
{
    Medicine *m = ask_medicine();
    if (!m) return;
    print_details(m);
    puts("\nUpdate: 1) Name  2) Manufacturer  3) Unit price  4) Reorder level  5) Cancel");
    switch (read_int("Choice: ", 1, 5)) {
        case 1: read_text("New name: ", m->name, sizeof m->name); break;
        case 2: read_text("New manufacturer: ", m->manufacturer, sizeof m->manufacturer); break;
        case 3: m->price = round2(read_double("New unit price (Rs.): ", 0.01, MAX_PRICE)); break;
        case 4: m->reorderLevel = read_int("New reorder level: ", 0, MAX_QTY); break;
        default: return;
    }
    save_or_warn();
    puts("Record updated.");
}

void med_delete_ui(void)
{
    Medicine *m = ask_medicine();
    if (!m) return;
    print_details(m);
    if (!confirm("Delete this medicine and all its batches?")) { puts("Cancelled."); return; }
    char code[CODE_LEN];
    strcpy(code, m->code);
    remove_medicine(code);
    save_or_warn();
    puts("Medicine deleted.");
}

void med_stock_ui(void)
{
    puts("Stock: 1) Receive stock (add batch)  2) Adjust batch quantity  3) Back");
    int ch = read_int("Choice: ", 1, 3);
    if (ch == 3) return;
    Medicine *m = ask_medicine();
    if (!m) return;
    char batch[BATCH_LEN];
    if (ch == 1) {
        char expiry[DATE_LEN];
        read_text("Batch number: ", batch, sizeof batch);
        int qty = read_int("Quantity received: ", 1, MAX_QTY);
        read_date("Expiry date (YYYY-MM-DD): ", expiry, 0);
        switch (add_batch(m, batch, qty, expiry)) {
            case 0:  save_or_warn(); puts("Stock updated."); break;
            case 1:  puts("This batch number already exists with a different expiry date."); break;
            case 2:  puts("Batch quantity limit exceeded."); break;
            default: puts("Out of memory.");
        }
        return;
    }
    print_details(m);
    read_text("\nBatch number to adjust: ", batch, sizeof batch);
    Batch **pp = &m->batches;
    while (*pp && strcmp((*pp)->batchNo, batch) != 0) pp = &(*pp)->next;
    if (!*pp) { puts("Batch not found."); return; }
    int q = read_int("New quantity (0 removes the batch): ", 0, MAX_QTY);
    if (q == 0) { Batch *b = *pp; *pp = b->next; free(b); }
    else (*pp)->qty = q;
    save_or_warn();
    puts("Stock updated.");
}

void med_lowstock_ui(void)
{
    if (!medCount) { puts("No medicines in inventory."); return; }
    int n, count = 0;
    Medicine **a = collect_sorted(&n, cmp_qty);
    if (!a) { puts("Out of memory."); return; }
    for (int i = 0; i < n; i++) {
        if (med_total_qty(a[i]) > a[i]->reorderLevel) continue;
        if (!count++) print_header();
        print_row(a[i]);
    }
    if (!count) puts("No low-stock medicines.");
    else printf("%d medicine(s) at or below reorder level - restock needed.\n", count);
    free(a);
}

void med_expiry_ui(void)
{
    if (!medCount) { puts("No medicines in inventory."); return; }
    puts("Expiry: 1) Expiry report  2) Remove expired batches  3) Back");
    int ch = read_int("Choice: ", 1, 3);
    if (ch == 3) return;
    int n;
    Medicine **a = collect_sorted(&n, cmp_code);
    if (!a) { puts("Out of memory."); return; }
    if (ch == 1) {
        int window = read_int("Show batches expiring within how many days (1-365): ", 1, 365);
        int found = 0;
        for (int i = 0; i < n; i++)
            for (Batch *b = a[i]->batches; b; b = b->next) {
                int d = days_to_expiry(b->expiry);
                if (d > window) continue;
                if (!found++) {
                    printf("%-10s %-24s %-14s %6s %-11s %s\n", "Code", "Name", "Batch", "Qty", "Expiry", "Status");
                    rule(85);
                }
                printf("%-10s %-24.24s %-14s %6d %-11s ", a[i]->code, a[i]->name, b->batchNo, b->qty, b->expiry);
                if (d < 0) printf("EXPIRED (%d day(s) ago)\n", -d);
                else       printf("in %d day(s)\n", d);
            }
        if (!found) puts("No batches expired or expiring in that period.");
    } else {
        int batches = 0, units = 0;
        for (int i = 0; i < n; i++)
            for (Batch *b = a[i]->batches; b; b = b->next)
                if (days_to_expiry(b->expiry) < 0) { batches++; units += b->qty; }
        if (!batches) puts("No expired batches.");
        else if (printf("Found %d expired batch(es) with %d unit(s).\n", batches, units),
                 confirm("Remove them from stock?")) {
            for (int i = 0; i < n; i++) {
                Batch **pp = &a[i]->batches;
                while (*pp) {
                    if (days_to_expiry((*pp)->expiry) < 0) { Batch *b = *pp; *pp = b->next; free(b); }
                    else pp = &(*pp)->next;
                }
            }
            save_or_warn();
            puts("Expired stock removed.");
        } else puts("Cancelled.");
    }
    free(a);
}

/* ===================== alerts and reports ===================== */

static void count_alerts(int *low, int *expired, int *soon)
{
    *low = *expired = *soon = 0;
    for (int i = 0; i < HASH_SIZE; i++)
        for (Medicine *m = table[i]; m; m = m->next) {
            if (med_total_qty(m) <= m->reorderLevel) (*low)++;
            for (Batch *b = m->batches; b; b = b->next) {
                int d = days_to_expiry(b->expiry);
                if (d < 0) (*expired)++;
                else if (d <= EXPIRY_WARN_DAYS) (*soon)++;
            }
        }
}

void med_startup_alerts(void)
{
    int low, expired, soon;
    count_alerts(&low, &expired, &soon);
    if (low || expired || soon)
        printf("ALERTS: %d low-stock medicine(s), %d expired batch(es), %d batch(es) expiring within %d days.\n",
               low, expired, soon, EXPIRY_WARN_DAYS);
}

static void report_print(FILE *out)
{
    char when[20];
    current_time_string(when, sizeof when, 1);
    int units = 0, expUnits = 0;
    double value = 0, expValue = 0;
    for (int i = 0; i < HASH_SIZE; i++)
        for (Medicine *m = table[i]; m; m = m->next)
            for (Batch *b = m->batches; b; b = b->next) {
                double v = b->qty * m->price;
                units += b->qty;
                value += v;
                if (days_to_expiry(b->expiry) < 0) { expUnits += b->qty; expValue += v; }
            }
    int low, expired, soon;
    count_alerts(&low, &expired, &soon);
    fprintf(out, "===== INVENTORY REPORT (%s) =====\n", when);
    fprintf(out, "Medicines:               %d\n", medCount);
    fprintf(out, "Total units in stock:    %d\n", units);
    fprintf(out, "Total stock value:       Rs.%.2f\n", value);
    fprintf(out, "Low-stock medicines:     %d\n", low);
    fprintf(out, "Expired batches:         %d (%d units, Rs.%.2f)\n", expired, expUnits, expValue);
    fprintf(out, "Expiring in %d days:     %d batch(es)\n\n", EXPIRY_WARN_DAYS, soon);
    int n;
    Medicine **a = collect_sorted(&n, cmp_name);
    if (!a) return;
    fprintf(out, "%-10s %-24s %7s %7s %12s\n", "Code", "Name", "Stock", "Reorder", "Value (Rs.)");
    for (int i = 0; i < 65; i++) fputc('-', out);
    fputc('\n', out);
    for (int i = 0; i < n; i++) {
        double v = 0;
        for (Batch *b = a[i]->batches; b; b = b->next) v += b->qty * a[i]->price;
        fprintf(out, "%-10s %-24.24s %7d %7d %12.2f\n", a[i]->code, a[i]->name,
                med_total_qty(a[i]), a[i]->reorderLevel, v);
    }
    free(a);
}

void med_report_ui(void)
{
    if (!medCount) { puts("No medicines in inventory."); return; }
    report_print(stdout);
    if (confirm("\nSave this report to inventory_report.txt?")) {
        FILE *fp = fopen("inventory_report.txt", "w");
        if (fp) { report_print(fp); fclose(fp); puts("Report saved."); }
        else puts("Could not write the report file.");
    }
}
