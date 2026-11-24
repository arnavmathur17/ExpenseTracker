#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define BUDGET_FILE  "monthly_budgets.csv"
#define EXPENSE_FILE "expenses_v3.csv"
#define TOPUP_FILE   "topups.csv"

#define MAX_LINE     512
#define MAX_NAME     100
#define MAX_CAT      20
#define MAX_AMOUNT   1000000000.0
#define N_CATEGORIES 5

typedef long long Money;

typedef struct {
    int year, month;
    Money budget;
} Month;

typedef struct {
    int year, month, day;
    char name[MAX_NAME];
    Money actual;
    char category[MAX_CAT];
    Money saved;
} Expense;

typedef struct {
    Money spent, saved;
    Money by_category[N_CATEGORIES];
} Totals;

static const char *categories[] = {"Food", "Home", "Work", "Fun", "Misc"};

enum {
    MENU_ADD = 1, MENU_VIEW, MENU_SUMMARY, MENU_WALLET,
    MENU_TOPUP, MENU_PAST, MENU_EXIT
};
static const char *menu_items[] = {
    "Add Expense", "View Expenses", "Monthly Summary", "Savings Wallet",
    "Add Money to This Month", "View Past Month", "Exit"
};
#define N_MENU ((int)(sizeof(menu_items) / sizeof(menu_items[0])))

static int read_line(char *s, size_t n) {
    if (!fgets(s, (int)n, stdin)) return 0;
    size_t k = strcspn(s, "\n");
    if (s[k] == '\n') s[k] = 0;
    else { int c; while ((c = getchar()) != '\n' && c != EOF) {} }
    k = strlen(s);
    if (k && s[k - 1] == '\r') s[k - 1] = 0;
    return 1;
}

static Money ask_money(const char *prompt, int allow_zero) {
    char line[MAX_LINE], extra;
    double d;
    while (1) {
        printf("%s", prompt);
        if (!read_line(line, sizeof(line))) exit(0);
        if (sscanf(line, " %lf %c", &d, &extra) == 1 && isfinite(d) &&
            d <= MAX_AMOUNT && (allow_zero ? d >= 0 : d > 0))
            return llround(d * 100);
        printf("Enter a valid amount%s, e.g. 250 or 250.50.\n",
               allow_zero ? " (0 or more)" : " greater than 0");
    }
}

static int ask_int(const char *prompt, int min, int max) {
    char line[MAX_LINE], extra;
    int n;
    while (1) {
        printf("%s", prompt);
        if (!read_line(line, sizeof(line))) exit(0);
        if (sscanf(line, " %d %c", &n, &extra) == 1 && n >= min && n <= max)
            return n;
        printf("Please enter a number between %d and %d.\n", min, max);
    }
}

static int ask_day(int today) {
    char line[MAX_LINE], extra;
    int d;
    while (1) {
        printf("Day of month [1-%d, Enter = today]: ", today);
        if (!read_line(line, sizeof(line))) exit(0);
        if (!line[0]) return today;
        if (sscanf(line, " %d %c", &d, &extra) == 1 && d >= 1 && d <= today)
            return d;
        printf("Enter a day between 1 and %d.\n", today);
    }
}

static void print_money(Money p) {
    const char *sign = p < 0 ? "-" : "";
    if (p < 0) p = -p;
    printf("Rs. %s%lld.%02lld", sign, p / 100, p % 100);
}

static void show(const char *label, Money amount) {
    printf("%-30s", label);
    print_money(amount);
    puts("");
}

static void today_date(int *y, int *mo, int *d) {
    time_t t = time(NULL);
    struct tm *l = localtime(&t);
    if (!l) { puts("Cannot determine date."); exit(1); }
    *y = l->tm_year + 1900;
    *mo = l->tm_mon + 1;
    *d = l->tm_mday;
}

static int days_in_month(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m - 1];
}

static int parse_expense(const char *line, Expense *e) {
    return sscanf(line, "%d,%d,%d,%99[^,],%lld,%19[^,],%lld",
                  &e->year, &e->month, &e->day, e->name,
                  &e->actual, e->category, &e->saved) == 7;
}

static int get_month(int year, int month, Month *out) {
    FILE *f = fopen(BUDGET_FILE, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        Month m;
        if (sscanf(line, "%d,%d,%lld", &m.year, &m.month, &m.budget) == 3 &&
            m.year == year && m.month == month) { *out = m; found = 1; }
    }
    fclose(f);
    return found;
}

static int append_month(const Month *m) {
    FILE *f = fopen(BUDGET_FILE, "a");
    if (!f) return 0;
    int ok = fprintf(f, "%d,%d,%lld\n", m->year, m->month, m->budget) > 0;
    if (fclose(f)) ok = 0;
    return ok;
}

static Totals month_totals(int year, int month) {
    Totals t = {0};
    FILE *f = fopen(EXPENSE_FILE, "r");
    if (!f) return t;
    char line[MAX_LINE];
    Expense e;
    while (fgets(line, sizeof(line), f)) {
        if (!parse_expense(line, &e) || e.year != year || e.month != month)
            continue;
        t.spent += e.actual;
        t.saved += e.saved;
        for (int i = 0; i < N_CATEGORIES; ++i)
            if (strcmp(e.category, categories[i]) == 0) {
                t.by_category[i] += e.actual;
                break;
            }
    }
    fclose(f);
    return t;
}

static Money wallet_total(void) {
    Money total = 0;
    FILE *f = fopen(EXPENSE_FILE, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    Expense e;
    while (fgets(line, sizeof(line), f))
        if (parse_expense(line, &e)) total += e.saved;
    fclose(f);
    return total;
}

static Money month_topups(int year, int month) {
    Money total = 0;
    FILE *f = fopen(TOPUP_FILE, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int y, mo, d;
    Money amount;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "%d,%d,%d,%lld", &y, &mo, &d, &amount) == 4 &&
            y == year && mo == month) total += amount;
    fclose(f);
    return total;
}

static Money available(const Month *m) {
    return m->budget + month_topups(m->year, m->month);
}

static int initialize_month(Month *m, int year, int month) {
    if (get_month(year, month, m)) return 1;
    printf("\nNew month: %04d-%02d.\n", year, month);
    Money budget = ask_money("Enter your monthly budget (Rs.): ", 1);
    *m = (Month){year, month, budget};
    return append_month(m);
}

static void summary(const Month *m, int today) {
    Totals t = month_totals(m->year, m->month);
    Money topups = month_topups(m->year, m->month);
    Money avail = m->budget + topups;
    Money remaining = avail - t.spent - t.saved;

    printf("\n======= SUMMARY %04d-%02d =======\n", m->year, m->month);
    show("Monthly budget:", m->budget);
    show("Money added:", topups);
    show("Available this month:", avail);
    show("Actual expenses:", t.spent);
    show("Moved to savings wallet:", t.saved);
    show("Remaining to spend:", remaining);

    if (today > 0) {
        int days_left = days_in_month(m->year, m->month) - today + 1;
        if (remaining > 0) {
            printf("Daily budget (%d days incl. today): ", days_left);
            print_money(remaining / days_left);
            puts("");
        } else {
            puts("Daily budget: budget used up.");
        }
    }

    puts("\nCategory totals (actual spending):");
    Money largest = 0;
    for (int i = 0; i < N_CATEGORIES; ++i)
        if (t.by_category[i] > largest) largest = t.by_category[i];
    for (int i = 0; i < N_CATEGORIES; ++i) {
        printf("%-5s | ", categories[i]);
        int bars = largest ? (int)(40.0 * t.by_category[i] / largest) : 0;
        for (int j = 0; j < bars; ++j) putchar('#');
        printf(" ");
        print_money(t.by_category[i]);
        puts("");
    }
}

static void add_expense(const Month *m, int today) {
    char name[MAX_NAME];
    puts("\n--- Add Expense ---");
    do {
        printf("Expense name (optional, press Enter to skip): ");
        if (!read_line(name, sizeof(name))) exit(0);
        if (strchr(name, ',')) puts("Please avoid commas in expense names.");
    } while (strchr(name, ','));
    if (!name[0]) strcpy(name, "Unnamed");

    Money actual = ask_money("Expense amount (Rs.): ", 0);
    puts("Categories:");
    for (int i = 0; i < N_CATEGORIES; ++i) printf("%d. %s\n", i + 1, categories[i]);
    int c = ask_int("Choose category: ", 1, N_CATEGORIES) - 1;
    int day = ask_day(today);

    Money saved = (actual + 5) / 10;

    Totals t = month_totals(m->year, m->month);
    Money remaining = available(m) - t.spent - t.saved;
    if (actual + saved > remaining) {
        printf("WARNING: this needs ");
        print_money(actual + saved);
        printf(" but only ");
        print_money(remaining);
        puts(" is left.");
        if (ask_int("1. Add anyway  2. Cancel: ", 1, 2) == 2) {
            puts("Cancelled.");
            return;
        }
    }

    FILE *f = fopen(EXPENSE_FILE, "a");
    if (!f) { puts("ERROR: Could not open expense file."); return; }
    int ok = fprintf(f, "%d,%d,%d,%s,%lld,%s,%lld\n", m->year, m->month, day,
                     name, actual, categories[c], saved) > 0;
    if (fclose(f)) ok = 0;
    if (!ok) { puts("ERROR: Could not save expense."); return; }

    puts("\nExpense recorded.");
    show("Actual spent:", actual);
    show("10% moved to wallet:", saved);
    show("Total deducted from budget:", actual + saved);
}

static int view_expenses(const Month *m) {
    printf("\n--- Expenses for %04d-%02d ---\n", m->year, m->month);
    FILE *f = fopen(EXPENSE_FILE, "r");
    int count = 0;
    if (f) {
        char line[MAX_LINE];
        Expense e;
        while (fgets(line, sizeof(line), f)) {
            if (!parse_expense(line, &e) || e.year != m->year || e.month != m->month)
                continue;
            printf("%d. %02d/%02d - %s (%s): ", ++count, e.day, e.month, e.name, e.category);
            print_money(e.actual);
            printf(" | saved ");
            print_money(e.saved);
            puts("");
        }
        fclose(f);
    }
    if (!count) puts("No expenses recorded yet.");
    return count;
}

static void add_money(const Month *m, int today) {
    puts("\n--- Add Money to This Month ---");
    Money amount = ask_money("Amount to add (Rs.): ", 0);
    FILE *f = fopen(TOPUP_FILE, "a");
    if (!f) { puts("ERROR: Could not open top-up file."); return; }
    int ok = fprintf(f, "%d,%d,%d,%lld\n", m->year, m->month, today, amount) > 0;
    if (fclose(f)) ok = 0;
    if (!ok) { puts("ERROR: Could not save."); return; }
    puts("Money added.");
    show("Available this month:", available(m));
}

static void show_wallet(const Month *m) {
    puts("\n--- Savings Wallet ---");
    show("Saved this month:", month_totals(m->year, m->month).saved);
    show("Total in wallet (all time):", wallet_total());
    puts("Wallet money is kept separate and is not part of your spending budget.");
}

static void view_past_month(void) {
    puts("\n--- View Past Month ---");
    int y = ask_int("Year: ", 2000, 2100);
    int mo = ask_int("Month [1-12]: ", 1, 12);
    Month past;
    if (!get_month(y, mo, &past)) {
        puts("No record for that month.");
        return;
    }
    summary(&past, 0);
    view_expenses(&past);
}

int main(void) {
    int year, month, day;
    today_date(&year, &month, &day);

    Month m;
    puts("==================================");
    puts("        EXPENSE TRACKER");
    puts("==================================");
    if (!initialize_month(&m, year, month)) {
        puts("ERROR: Could not load/save monthly budget.");
        return 1;
    }

    while (1) {

        today_date(&year, &month, &day);
        if (year != m.year || month != m.month) {
            puts("\nA new month has started!");
            if (!initialize_month(&m, year, month)) {
                puts("ERROR: Could not save new month.");
                return 1;
            }
        }

        Totals t = month_totals(m.year, m.month);
        printf("\n========== MAIN MENU ==========\n");
        printf("Month: %04d-%02d  (day %d)\n", m.year, m.month, day);
        show("Available:", available(&m));
        show("Remaining to spend:", available(&m) - t.spent - t.saved);
        for (int i = 0; i < N_MENU; ++i) printf("%d. %s\n", i + 1, menu_items[i]);

        char prompt[40];
        snprintf(prompt, sizeof(prompt), "Choose an option [1-%d]: ", N_MENU);
        switch (ask_int(prompt, 1, N_MENU)) {
            case MENU_ADD:     add_expense(&m, day);   break;
            case MENU_VIEW:    view_expenses(&m);      break;
            case MENU_SUMMARY: summary(&m, day);       break;
            case MENU_WALLET:  show_wallet(&m);        break;
            case MENU_TOPUP:   add_money(&m, day);     break;
            case MENU_PAST:    view_past_month();      break;
            case MENU_EXIT:    puts("Goodbye!");       return 0;
        }
    }
}
