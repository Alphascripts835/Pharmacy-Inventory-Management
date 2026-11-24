#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "medicine.h"
#include "sales.h"

/* Billing cart is a linked stack (Unit 4): push = add line, pop = undo last line */
typedef struct CartLine {
    char code[CODE_LEN];
    char name[NAME_LEN];
    int qty;
    double price;
    struct CartLine *next;
} CartLine;

static CartLine *cartTop = NULL;

static int cart_push(const Medicine *m, int qty)
{
    CartLine *l = malloc(sizeof *l);
    if (!l) return 0;
    strcpy(l->code, m->code);
    strcpy(l->name, m->name);
    l->qty = qty;
    l->price = m->price;
    l->next = cartTop;
    cartTop = l;
    return 1;
}

static void cart_pop(void)
{
    if (!cartTop) { puts("Nothing to undo - cart is empty."); return; }
    CartLine *l = cartTop;
    cartTop = l->next;
    printf("Removed: %d x %s\n", l->qty, l->name);
    free(l);
}

static void cart_clear(void)
{
    while (cartTop) { CartLine *n = cartTop->next; free(cartTop); cartTop = n; }
}

static int cart_qty_for(const char *code)
{
    int t = 0;
    for (CartLine *l = cartTop; l; l = l->next)
        if (strcmp(l->code, code) == 0) t += l->qty;
    return t;
}

static double cart_subtotal(void)
{
    double s = 0;
    for (CartLine *l = cartTop; l; l = l->next) s += l->qty * l->price;
    return s;
}

static void cart_show(void)
{
    if (!cartTop) { puts("Cart is empty."); return; }
    printf("\n%-10s %-24s %5s %9s %10s   (latest line first)\n", "Code", "Name", "Qty", "Price", "Amount");
    for (int i = 0; i < 62; i++) putchar('-');
    putchar('\n');
    for (CartLine *l = cartTop; l; l = l->next)
        printf("%-10s %-24.24s %5d %9.2f %10.2f\n", l->code, l->name, l->qty, l->price, l->qty * l->price);
    printf("Subtotal: Rs.%.2f\n", cart_subtotal());
}

static void add_item(void)
{
    char code[CODE_LEN];
    read_code("Medicine code: ", code);
    Medicine *m = inv_find(code);
    if (!m) { puts("Medicine not found."); return; }
    int avail = med_sellable_qty(m) - cart_qty_for(code);
    printf("%s - Rs.%.2f - available (non-expired): %d\n", m->name, m->price, avail);
    if (avail <= 0) { puts("No sellable stock available."); return; }
    int qty = read_int("Quantity: ", 1, avail);
    if (!cart_push(m, qty)) puts("Out of memory.");
    else puts("Added to cart.");
}

static long next_bill_id(void)
{
    FILE *fp = fopen(SALES_FILE, "r");
    long max = 0;
    char line[256];
    if (!fp) return 1;
    while (fgets(line, sizeof line, fp))
        if (strncmp(line, "BILL|", 5) == 0) {
            long id = strtol(line + 5, NULL, 10);
            if (id > max) max = id;
        }
    fclose(fp);
    return max + 1;
}

static void print_receipt(long id, const char *when, double sub, double tax, double disc, double total)
{
    printf("\n========== BILL #%ld  %s ==========\n", id, when);
    for (CartLine *l = cartTop; l; l = l->next)
        printf("%-24.24s %4d x %8.2f = %10.2f\n", l->name, l->qty, l->price, l->qty * l->price);
    printf("Subtotal:                      %10.2f\n", sub);
    printf("Tax (%.0f%%):                     %10.2f\n", TAX_RATE * 100, tax);
    printf("Discount:                     -%10.2f\n", disc);
    printf("TOTAL (Rs.):                   %10.2f\n", total);
    puts("=========================================");
}

static int save_bill(long id, const char *when, double sub, double tax, double disc, double total)
{
    FILE *fp = fopen(SALES_FILE, "a");
    if (!fp) return 0;
    fprintf(fp, "BILL|%ld|%s|%.2f|%.2f|%.2f|%.2f\n", id, when, sub, tax, disc, total);
    for (CartLine *l = cartTop; l; l = l->next)
        fprintf(fp, "LINE|%ld|%s|%s|%d|%.2f|%.2f\n", id, l->code, l->name, l->qty, l->price, l->qty * l->price);
    int ok = !ferror(fp);
    if (fclose(fp) != 0) ok = 0;
    return ok;
}

/* Total = P x Q (subtotal) + Tax - Discount, as in the report's bill formula */
static void checkout(void)
{
    if (!cartTop) { puts("Cart is empty."); return; }
    double sub = cart_subtotal();
    double pct = read_double("Discount % (0-100): ", 0, 100);
    double tax = sub * TAX_RATE, disc = sub * pct / 100.0, total = sub + tax - disc;
    char when[20];
    current_time_string(when, sizeof when, 1);
    long id = next_bill_id();

    for (CartLine *l = cartTop; l; l = l->next) {   /* re-check stock before selling */
        Medicine *m = inv_find(l->code);
        if (!m || med_sellable_qty(m) < cart_qty_for(l->code)) {
            printf("Insufficient stock for %s. Sale cancelled.\n", l->name);
            return;
        }
    }
    print_receipt(id, when, sub, tax, disc, total);
    if (!confirm("Confirm sale?")) { puts("Sale not confirmed; cart kept."); return; }

    for (CartLine *l = cartTop; l; l = l->next)
        med_sell(inv_find(l->code), l->qty);
    if (!inv_save()) puts("WARNING: could not save inventory to file!");
    if (!save_bill(id, when, sub, tax, disc, total)) puts("WARNING: could not record the bill in the sales file!");
    cart_clear();
    printf("Sale completed. Bill #%ld recorded.\n", id);
}

void billing_menu(void)
{
    for (;;) {
        puts("\n--- Billing ---");
        puts("1) Add item  2) Undo last item  3) View cart  4) Checkout  5) Clear cart  0) Back");
        int ch = read_int("Choice: ", 0, 5);
        switch (ch) {
            case 1: add_item(); break;
            case 2: cart_pop(); break;
            case 3: cart_show(); break;
            case 4: checkout(); break;
            case 5:
                if (cartTop && confirm("Discard all items in the cart?")) { cart_clear(); puts("Cart cleared."); }
                break;
            case 0:
                if (cartTop && !confirm("Cart has items. Discard and go back?")) break;
                cart_clear();
                return;
        }
    }
}

void sales_history_ui(void)
{
    FILE *fp = fopen(SALES_FILE, "r");
    if (!fp) { puts("No sales recorded yet."); return; }
    puts("1) Full history with summary  2) Summary only");
    int full = read_int("Choice: ", 1, 2) == 1;
    char today[20], line[256];
    current_time_string(today, sizeof today, 0);
    long bills = 0, units = 0;
    double revenue = 0, tax = 0, disc = 0, todayRev = 0;

    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *f[8];
        int n = split_fields(line, f, 8);
        if (n == 7 && strcmp(f[0], "BILL") == 0) {
            double s = strtod(f[3], NULL), t = strtod(f[4], NULL), d = strtod(f[5], NULL), tot = strtod(f[6], NULL);
            bills++; revenue += tot; tax += t; disc += d;
            if (strncmp(f[2], today, 10) == 0) todayRev += tot;
            if (full) printf("\nBill #%s  %s  Subtotal %.2f  Tax %.2f  Discount %.2f  TOTAL %.2f\n", f[1], f[2], s, t, d, tot);
        } else if (n == 7 && strcmp(f[0], "LINE") == 0) {
            int q = atoi(f[4]);
            units += q;
            if (full) printf("   %-10s %-24.24s %4d x %8.2f = %9.2f\n", f[2], f[3], q, strtod(f[5], NULL), strtod(f[6], NULL));
        }
    }
    fclose(fp);
    printf("\n===== SALES SUMMARY =====\nBills: %ld\nUnits sold: %ld\nRevenue (Rs.): %.2f\n"
           "Tax collected (Rs.): %.2f\nDiscounts given (Rs.): %.2f\nToday's revenue (Rs.): %.2f\n",
           bills, units, revenue, tax, disc, todayRev);
}
