#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define BUDGET_FILE "monthly_budgets.csv"
#define EXPENSE_FILE "expenses_v3.csv"
#define ACTIVE_FILE "active_month.txt"
#define MAX_LINE 512
#define MAX_NAME 100
#define N_CATEGORIES 5


typedef long long Money;
typedef struct {
    int year, month;
    Money budget, carried;
} Month;
static const char *categories[] = {"Food", "Home", "Work", "Fun", "Misc"};

static int read_line(char *s, size_t n) {
    if (!fgets(s, (int)n, stdin)) return 0;
    size_t k = strcspn(s, "\n");
    if (s[k] == '\n') s[k] = 0;
    else { int c; while ((c = getchar()) != '\n' && c != EOF) {} }
    k = strlen(s);
    if (k && s[k-1] == '\r') s[k-1] = 0;
    return 1;
}
static Money ask_money(const char *prompt, int positive) {
    char line[MAX_LINE], extra;
    double d;
    while (1) {
        printf("%s", prompt);
        if (!read_line(line, sizeof(line))) exit(0);
        if (sscanf(line, " %lf %c", &d, &extra) == 1 &&
            isfinite(d) && d >= 0 && (!positive || d > 0) && d <= 1000000000.0)
            return llround(d * 100);
        puts("Enter a valid non-negative amount, e.g. 250 or 250.50.");
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
static void print_money(Money p) {
    if (p < 0) { putchar('-'); p = -p; }
    printf("Rs. %lld.%02lld", p/100, p%100);
}
static int get_month(int year, int month, Month *out) {
    FILE *f = fopen(BUDGET_FILE, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        Month m;
        if (sscanf(line, "%d,%d,%lld,%lld", &m.year, &m.month,
                   &m.budget, &m.carried) == 4 &&
            m.year == year && m.month == month) { *out = m; found = 1; }
    }
    fclose(f);
    return found;
}
static int append_month(const Month *m) {
    FILE *f = fopen(BUDGET_FILE, "a");
    if (!f) return 0;
    int ok = fprintf(f, "%d,%d,%lld,%lld\n",
                     m->year, m->month, m->budget, m->carried) > 0;
    if (fclose(f)) ok = 0;
    return ok;
}
static Money month_savings(int year, int month) {
    FILE *f = fopen(EXPENSE_FILE, "r");
    if (!f) return 0;
    char line[MAX_LINE], name[MAX_NAME], category[20];
    int y, mo, day;
    Money actual, saving, total = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%d,%d,%d,%99[^,],%lld,%19[^,],%lld",
                   &y, &mo, &day, name, &actual, category, &saving) == 7 &&
            y == year && mo == month) total += saving;
    }
    fclose(f);
    return total;
}

static int initialize_month(Month *m, int year, int month) {
    if (get_month(year, month, m)) return 1;
    int previous_year = month == 1 ? year-1 : year;
    int previous_month = month == 1 ? 12 : month-1;
    Month previous;
    Money carried = get_month(previous_year, previous_month, &previous)
                    ? month_savings(previous_year, previous_month) : 0;
    printf("\nFirst launch for %04d-%02d.\n", year, month);
    printf("Savings available from last month: "); print_money(carried); puts("");
    Money budget = ask_money("Enter your monthly budget (Rs.): ", 0);
    *m = (Month){year, month, budget, carried};
    return append_month(m);
}
static void summary(const Month *m, int day) {
    Money totals[N_CATEGORIES] = {0}, spent = 0, saved = 0;
    FILE *f = fopen(EXPENSE_FILE, "r");
    if (f) {
        char line[MAX_LINE], name[MAX_NAME], category[20];
        int y, mo, d;
        Money actual, saving;
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%d,%d,%d,%99[^,],%lld,%19[^,],%lld",
                       &y, &mo, &d, name, &actual, category, &saving) != 7 ||
                y != m->year || mo != m->month) continue;
            spent += actual;
            saved += saving;
            for (int i=0; i<N_CATEGORIES; ++i)
                if (strcmp(category, categories[i]) == 0) { totals[i] += actual; break; }
        }
        fclose(f);
    }
    Money available = m->budget + m->carried;
    Money remaining = available - spent - saved;
    printf("\n======= SUMMARY %04d-%02d =======\n", m->year, m->month);
    printf("Monthly budget:         "); print_money(m->budget); puts("");
    printf("Previous month's wallet:"); putchar(' '); print_money(m->carried); puts("");
    printf("Available this month:   "); print_money(available); puts("");
    printf("Actual expenses:        "); print_money(spent); puts("");
    printf("Saved for next month:   "); print_money(saved); puts("");
    printf("Total budget used:      "); print_money(spent + saved); puts("");
    printf("Remaining to spend:     "); print_money(remaining); puts("");
    int days_left;
    struct tm t = {0};
    t.tm_year = m->year - 1900;
    t.tm_mon = m->month;
    t.tm_mday = 0;
    mktime(&t);
    days_left = t.tm_mday - day;
    if (days_left > 0) {
        printf("Daily budget (%d days): ", days_left);
        print_money(remaining/days_left); puts("");
    } else puts("Daily budget: Last day of the month.");
    puts("\nCategory totals (actual spending):");
    Money largest = 0;
    for (int i=0; i<N_CATEGORIES; ++i) if (totals[i] > largest) largest = totals[i];
    for (int i=0; i<N_CATEGORIES; ++i) {
        printf("%-5s | ", categories[i]);
        int bars = largest ? (int)(40.0 * totals[i]/largest) : 0;
        for (int j=0; j<bars; ++j) putchar('#');
        printf(" "); print_money(totals[i]); puts("");
    }
}
static void add_expense(const Month *m, int day) {
    char name[MAX_NAME];
    puts("\n--- Add Expense ---");
    do {
        printf("Expense name (optional, press Enter to skip): ");
        if (!read_line(name, sizeof(name))) return;
        if (strchr(name, ',')) puts("Please avoid commas in expense names.");
    } while (strchr(name, ','));
    if (!name[0]) strcpy(name, "Unnamed");
    Money actual = ask_money("Expense amount (Rs.): ", 1);
    puts("Categories:");
    for (int i=0; i<N_CATEGORIES; ++i) printf("%d. %s\n", i+1, categories[i]);
    int c = ask_int("Choose category: ", 1, N_CATEGORIES) - 1;
    /* Save 10% of actual expenditure, rounded to the nearest paise. */
    Money saved = (actual + 5) / 10;
    FILE *f = fopen(EXPENSE_FILE, "a");
    if (!f) { puts("ERROR: Could not open expense file."); return; }
    int ok = fprintf(f, "%d,%d,%d,%s,%lld,%s,%lld\n",
                     m->year, m->month, day, name, actual, categories[c], saved) > 0;
    if (fclose(f)) ok = 0;
    if (!ok) { puts("ERROR: Could not save expense."); return; }
    puts("\nExpense recorded.");
    printf("Actual spent: "); print_money(actual); puts("");
    printf("10%% saved for next month: "); print_money(saved); puts("");
    printf("Total deducted from monthly budget: "); print_money(actual + saved); puts("");
}
static void view_expenses(const Month *m) {
    FILE *f = fopen(EXPENSE_FILE, "r");
    puts("\n--- This Month's Expenses ---");
    if (!f) { puts("No expenses recorded yet."); return; }
    char line[MAX_LINE], name[MAX_NAME], category[20];
    int y, mo, d, count=0;
    Money actual, saved;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%d,%d,%d,%99[^,],%lld,%19[^,],%lld",
                   &y, &mo, &d, name, &actual, category, &saved) != 7 ||
            y != m->year || mo != m->month) continue;
        printf("%d. %02d/%02d - %s (%s): ", ++count, d, mo, name, category);
        print_money(actual);
        printf(" | saved "); print_money(saved); puts("");
    }
    fclose(f);
    if (!count) puts("No expenses recorded yet.");
}
static void show_wallet(const Month *m) {
    puts("\n--- Savings Wallet ---");
    printf("From previous month, available now: ");
    print_money(m->carried); puts("");
    printf("Saved this month, available next month: ");
    print_money(month_savings(m->year, m->month)); puts("");

}
static int write_active_month(int year, int month) {
    FILE *f = fopen(ACTIVE_FILE, "w");
    if (!f) return 0;
    int ok = fprintf(f, "%d %d\n", year, month) > 0;
    if (fclose(f)) ok = 0;
    return ok;
}

int main(void) {
    time_t current = time(NULL);
    struct tm *local = localtime(&current);
    if (!local) { puts("Cannot determine date."); return 1; }
    int current_year = local->tm_year + 1900;
    int current_month = local->tm_mon + 1;
    int current_day = local->tm_mday;
    int year = current_year, month = current_month;
    FILE *active = fopen(ACTIVE_FILE, "r");
    if (active) {
        int y, mo;
        if (fscanf(active, "%d %d", &y, &mo) == 2 && mo >= 1 && mo <= 12 &&
            (y > year || (y == year && mo > month))) {
            year = y;
            month = mo;
        }
        fclose(active);
    }
    Month m;
    puts("==================================");
    puts("        EXPENSE TRACKER");
    puts("==================================");
    if (!initialize_month(&m, year, month)) {
        puts("ERROR: Could not load/save monthly budget."); return 1;
    }
    if (!write_active_month(year, month)) {
        puts("WARNING: Could not save active month.");
    }
    while (1) {
        int day = (year == current_year && month == current_month) ? current_day : 1;
        printf("\n========== MAIN MENU ==========\n");
        printf("Month: %04d-%02d\n", year, month);
        printf("Monthly budget: "); print_money(m.budget); puts("");
        puts("1. Add Expense");
        puts("2. View Expenses");
        puts("3. Monthly Summary");
        puts("4. Savings Wallet");
        puts("5. New Month");
        puts("6. Exit");
        int choice = ask_int("Choose an option [1-6]: ", 1, 6);
        switch (choice) {
            case 1: add_expense(&m, day); break;
            case 2: view_expenses(&m); break;
            case 3: summary(&m, day); break;
            case 4: show_wallet(&m); break;
            case 5: {
                int next_year = year, next_month = month + 1;
                if (next_month == 13) { next_month = 1; next_year++; }
                printf("Start new month %04d-%02d? Previous month will remain saved.\n",
                       next_year, next_month);
                int confirm = ask_int("1. Confirm  2. Cancel: ", 1, 2);
                if (confirm == 2) break;
                Month next;
                if (!initialize_month(&next, next_year, next_month)) {
                    puts("ERROR: Could not initialize new month."); break;
                }
                if (!write_active_month(next_year, next_month)) {
                    puts("ERROR: Could not save active month. Month not switched."); break;
                }
                year = next_year;
                month = next_month;
                m = next;
                puts("New month activated.");
                break;
            }
            case 6: puts("Goodbye!"); return 0;
        }
    }
}
