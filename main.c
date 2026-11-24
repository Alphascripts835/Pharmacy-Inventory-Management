#include <stdio.h>
#include "medicine.h"
#include "sales.h"

static void print_menu(void)
{
    puts("\n=========== PHARMACY INVENTORY MANAGEMENT SYSTEM ===========");
    puts(" 1) Add medicine           7) Low-stock alerts");
    puts(" 2) Display medicines      8) Expiry tracking");
    puts(" 3) Search medicines       9) Sales and billing");
    puts(" 4) Update medicine       10) Sales history");
    puts(" 5) Delete medicine       11) Inventory report");
    puts(" 6) Manage stock           0) Exit");
}

int main(void)
{
    inv_init();
    if (inv_load()) puts("Inventory loaded from file.");
    else puts("No saved data found. Starting with an empty inventory.");
    med_startup_alerts();

    for (;;) {
        print_menu();
        switch (read_int("Enter choice: ", 0, 11)) {
            case 1:  med_add_ui(); break;
            case 2:  med_display_ui(); break;
            case 3:  med_search_ui(); break;
            case 4:  med_update_ui(); break;
            case 5:  med_delete_ui(); break;
            case 6:  med_stock_ui(); break;
            case 7:  med_lowstock_ui(); break;
            case 8:  med_expiry_ui(); break;
            case 9:  billing_menu(); break;
            case 10: sales_history_ui(); break;
            case 11: med_report_ui(); break;
            case 0:
                if (!inv_save()) puts("WARNING: could not save data to file!");
                inv_free();
                puts("Data saved. Goodbye!");
                return 0;
        }
    }
}
