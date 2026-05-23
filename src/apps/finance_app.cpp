#include "finance_app.h"
#include "../ui.h"
#include <cmath>
#include <cstdio>
#include <string>

// ── Constants ───────────────────────────────────────────────────

static const int PAD = 16;
static const int GAP = 12;
static const int HEADER_H = 48;
static const int SUMMARY_H = 100;
static const int MID_H = 240;
static const int BOT_H = 210;

// ── Colors ──────────────────────────────────────────────────────

static const SDL_Color CARD_BG   = {22, 27, 55, 200};
static const SDL_Color WHITE     = {230, 230, 240, 255};
static const SDL_Color DIM       = {150, 160, 180, 255};
static const SDL_Color FAINT     = {255, 255, 255, 20};
static const SDL_Color GREEN     = {80, 200, 120, 255};
static const SDL_Color RED       = {220, 80, 80, 255};
static const SDL_Color PURPLE    = {160, 100, 240, 255};
static const SDL_Color ACCENT    = {100, 150, 255, 255};

// Category colors
static const SDL_Color C_HOUSING = {100, 100, 220, 255};  // indigo
static const SDL_Color C_FOOD    = {230, 150, 60, 255};   // orange
static const SDL_Color C_TRANS   = {80, 190, 120, 255};   // green
static const SDL_Color C_SHOP    = {220, 100, 160, 255};  // pink
static const SDL_Color C_ENTER   = {230, 200, 60, 255};   // yellow
static const SDL_Color C_OTHER   = {140, 140, 160, 255};  // gray

// ── Static helpers ──────────────────────────────────────────────

static void card_bg(SDL_Renderer* r, int x, int y, int w, int h) {
    draw::filled_rounded_rect(r, {x, y, w, h}, 8, CARD_BG);
}

static void pbar(SDL_Renderer* r, int x, int y, int w, int h, float pct, SDL_Color fg) {
    SDL_Color track = {40, 45, 70, 200};
    draw::filled_rounded_rect(r, {x, y, w, h}, h / 2, track);
    int fw = (int)(w * pct);
    if (fw > 0) {
        if (fw < h) fw = h;
        draw::filled_rounded_rect(r, {x, y, fw, h}, h / 2, fg);
    }
}

// fmoney: format a dollar amount with sign
// (available for future wiring; currently data is hardcoded)
__attribute__((unused))
static std::string fmoney(double v) {
    char buf[32];
    if (v < 0)
        snprintf(buf, sizeof(buf), "-$%.2f", -v);
    else
        snprintf(buf, sizeof(buf), "$%.2f", v);
    return buf;
}

static void draw_donut(SDL_Renderer* r, int cx, int cy, int outer_r, int inner_r,
                        const float* pcts, const SDL_Color* cols, int n) {
    float angle = -M_PI / 2.0f; // start at top
    float gap = 0.04f;          // gap between segments in radians
    for (int i = 0; i < n; i++) {
        float sweep = pcts[i] * 2.0f * M_PI - gap;
        if (sweep < 0.01f) { angle += pcts[i] * 2.0f * M_PI; continue; }
        SDL_SetRenderDrawColor(r, cols[i].r, cols[i].g, cols[i].b, cols[i].a);
        for (float a = angle; a < angle + sweep; a += 0.003f) {
            float cs = cosf(a), sn = sinf(a);
            int x1 = cx + (int)(inner_r * cs);
            int y1 = cy + (int)(inner_r * sn);
            int x2 = cx + (int)(outer_r * cs);
            int y2 = cy + (int)(outer_r * sn);
            SDL_RenderDrawLine(r, x1, y1, x2, y2);
        }
        angle += pcts[i] * 2.0f * M_PI;
    }
}

static void card_btn(SDL_Renderer* r, const Fonts* f, int x, int y, int w, const char* label) {
    draw::line(r, x + 8, y, x + w - 8, y, FAINT);
    draw::text_centered(r, f->small, label, x + w / 2, y + 8, ACCENT);
}

// ── Main render ─────────────────────────────────────────────────

void FinanceApp::render(const RenderCtx& ctx, SDL_Rect cr) {
    last_rect_ = cr;
    SDL_Renderer* r = ctx.r;
    const Fonts* f = ctx.fonts;

    // Clip to content area
    SDL_RenderSetClipRect(r, &cr);

    int x = cr.x + PAD;
    int w = cr.w - PAD * 2;
    int y0 = cr.y + PAD - (int)scroll_y_;

    // Total content height
    content_h_ = PAD + HEADER_H + GAP + SUMMARY_H + GAP + MID_H + GAP + BOT_H + PAD;

    int cy = y0;

    // 1. Header
    render_header(r, f, x, cy, w);
    cy += HEADER_H + GAP;

    // 2. Summary row
    render_summary_row(r, f, x, cy, w);
    cy += SUMMARY_H + GAP;

    // 3. Middle row: Spending | Expenses | Accounts
    {
        int cw = (w - GAP * 2) / 3;
        render_spending_card(r, f, x, cy, cw, MID_H);
        render_expenses_card(r, f, x + cw + GAP, cy, cw, MID_H);
        render_accounts_card(r, f, x + (cw + GAP) * 2, cy, w - (cw + GAP) * 2, MID_H);
    }
    cy += MID_H + GAP;

    // 4. Bottom row: Budget | Transactions | Goals
    {
        int cw = (w - GAP * 2) / 3;
        render_budget_card(r, f, x, cy, cw, BOT_H);
        render_transactions_card(r, f, x + cw + GAP, cy, cw, BOT_H);
        render_goals_card(r, f, x + (cw + GAP) * 2, cy, w - (cw + GAP) * 2, BOT_H);
    }

    SDL_RenderSetClipRect(r, nullptr);
}

// ── Header ──────────────────────────────────────────────────────

void FinanceApp::render_header(SDL_Renderer* r, const Fonts* f, int x, int y, int w) {
    draw::text(r, f->widget, "Overview", x, y + 8, WHITE);

    draw::text(r, f->small, "Your financial summary", x, y + 34, DIM);

    // Date range (right-aligned)
    const char* date_range = "< May 1 - May 31, 2024 >";
    draw::text_right(r, f->body, date_range, x + w, y + 10, WHITE);

    // "This Month" dropdown
    draw::text_right(r, f->small, "This Month  v", x + w, y + 30, DIM);
}

// ── Summary Row ─────────────────────────────────────────────────

void FinanceApp::render_summary_row(SDL_Renderer* r, const Fonts* f, int x, int y, int w) {
    struct SummaryItem {
        const char* label;
        const char* amount;
        const char* subtitle;
        const char* change;
        SDL_Color color;
        Icon icon;
    };

    SummaryItem items[] = {
        {"Cash",      "$8,742.15",   "Available balance", "\xe2\x96\xb2 2.4%", GREEN,  Icon::Box},
        {"Debt",      "-$12,345.67", "Total outstanding", "\xe2\x96\xbc 1.2%", RED,    Icon::Target},
        {"Net Worth", "$24,146.48",  "Assets - Liabilities", "\xe2\x96\xb2 3.1%", PURPLE, Icon::Star},
        {"Income",    "$6,250.00",   "This month",        "\xe2\x96\xb2 5.0%", GREEN,  Icon::Sparkle},
    };

    int cw = (w - GAP * 3) / 4;

    for (int i = 0; i < 4; i++) {
        int cx = x + i * (cw + GAP);
        card_bg(r, cx, y, cw, SUMMARY_H);

        // Colored icon circle
        int icon_cx = cx + 24;
        int icon_cy = y + 30;
        draw::filled_circle(r, icon_cx, icon_cy, 14, {items[i].color.r, items[i].color.g, items[i].color.b, 50});
        draw::icon(r, items[i].icon, icon_cx, icon_cy, 14, items[i].color);

        // Label
        int tx = cx + 46;
        draw::text(r, f->small, items[i].label, tx, y + 14, DIM);

        // Amount
        draw::text(r, f->title, items[i].amount, tx, y + 30, WHITE);

        // Subtitle + change
        draw::text(r, f->small, items[i].subtitle, tx, y + 52, DIM);
        draw::text(r, f->small, items[i].change, tx, y + 68, items[i].color);
    }
}

// ── Spending Overview Card ──────────────────────────────────────

void FinanceApp::render_spending_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h) {
    card_bg(r, x, y, w, h);

    draw::text(r, f->title, "Spending Overview", x + PAD, y + PAD, WHITE);
    draw::text(r, f->small, "$3,842.50 total", x + PAD, y + 34, DIM);

    // Donut chart - left 40%
    int chart_w = w * 40 / 100;
    int chart_cx = x + chart_w / 2 + PAD / 2;
    int chart_cy = y + h / 2 + 20;
    int outer_r = 42;
    int inner_r = 28;

    float pcts[] = {0.35f, 0.22f, 0.15f, 0.12f, 0.10f, 0.06f};
    SDL_Color cols[] = {C_HOUSING, C_FOOD, C_TRANS, C_SHOP, C_ENTER, C_OTHER};

    draw_donut(r, chart_cx, chart_cy, outer_r, inner_r, pcts, cols, 6);

    // Center text
    draw::text_centered(r, f->small, "$3,842", chart_cx, chart_cy - 6, WHITE);

    // Legend - right 60%
    struct CatItem { const char* name; const char* pct; SDL_Color col; };
    CatItem cats[] = {
        {"Housing",       "35%", C_HOUSING},
        {"Food",          "22%", C_FOOD},
        {"Transport",     "15%", C_TRANS},
        {"Shopping",      "12%", C_SHOP},
        {"Entertainment", "10%", C_ENTER},
        {"Other",          "6%", C_OTHER},
    };

    int lx = x + chart_w + PAD;
    int ly = y + 56;
    for (int i = 0; i < 6; i++) {
        draw::filled_circle(r, lx + 4, ly + 6, 4, cats[i].col);
        draw::text(r, f->small, cats[i].name, lx + 14, ly, DIM);
        draw::text_right(r, f->small, cats[i].pct, x + w - PAD, ly, WHITE);
        ly += 22;
    }

    card_btn(r, f, x, y + h - 24, w, "View Details");
}

// ── Top Expenses Card ───────────────────────────────────────────

void FinanceApp::render_expenses_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h) {
    card_bg(r, x, y, w, h);

    draw::text(r, f->title, "Top Expenses", x + PAD, y + PAD, WHITE);

    struct ExpenseItem {
        const char* name;
        const char* category;
        const char* amount;
        float pct;
        SDL_Color col;
    };

    ExpenseItem items[] = {
        {"Rent",        "Housing",       "$1,200.00", 0.89f, C_HOUSING},
        {"Groceries",   "Food",          "$425.50",   0.65f, C_FOOD},
        {"Dining Out",  "Food",          "$312.75",   0.50f, C_FOOD},
        {"Gas",         "Transport",     "$185.00",   0.35f, C_TRANS},
        {"Electricity", "Housing",       "$150.00",   0.28f, C_HOUSING},
    };

    int iy = y + 42;
    for (int i = 0; i < 5; i++) {
        draw::filled_circle(r, x + PAD + 4, iy + 6, 4, items[i].col);
        draw::text(r, f->small, items[i].name, x + PAD + 14, iy, WHITE);
        draw::text(r, f->small, items[i].category, x + PAD + 14, iy + 14, DIM);
        draw::text_right(r, f->small, items[i].amount, x + w - PAD, iy, WHITE);

        // Thin progress bar
        int bar_y = iy + 28;
        pbar(r, x + PAD + 14, bar_y, w - PAD * 2 - 14, 3, items[i].pct, items[i].col);

        iy += 38;
    }
}

// ── Accounts Card ───────────────────────────────────────────────

void FinanceApp::render_accounts_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h) {
    card_bg(r, x, y, w, h);

    draw::text(r, f->title, "Accounts", x + PAD, y + PAD, WHITE);
    draw::text(r, f->small, "Total: $8,742.15", x + PAD, y + 34, DIM);

    struct AccountItem {
        const char* name;
        const char* amount;
        Icon icon;
    };

    AccountItem items[] = {
        {"Checking", "$3,245.50", Icon::Box},
        {"Savings",  "$4,231.75", Icon::Star},
        {"Cash",     "$265.90",   Icon::Briefcase},
    };

    int iy = y + 60;
    for (int i = 0; i < 3; i++) {
        // Icon circle
        draw::filled_circle(r, x + PAD + 14, iy + 10, 12, {ACCENT.r, ACCENT.g, ACCENT.b, 40});
        draw::icon(r, items[i].icon, x + PAD + 14, iy + 10, 10, ACCENT);

        draw::text(r, f->body, items[i].name, x + PAD + 34, iy + 2, WHITE);
        draw::text_right(r, f->body, items[i].amount, x + w - PAD, iy + 2, WHITE);

        if (i < 2) {
            draw::line(r, x + PAD, iy + 30, x + w - PAD, iy + 30, FAINT);
        }
        iy += 40;
    }

    card_btn(r, f, x, y + h - 24, w, "Manage Accounts");
}

// ── Budget Progress Card ────────────────────────────────────────

void FinanceApp::render_budget_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h) {
    card_bg(r, x, y, w, h);

    draw::text(r, f->title, "Budget Progress", x + PAD, y + PAD, WHITE);

    struct BudgetItem {
        const char* name;
        const char* detail;
        float pct;
        SDL_Color col;
    };

    BudgetItem items[] = {
        {"Housing",       "89%", 0.89f, C_HOUSING},
        {"Food",          "78%", 0.78f, C_FOOD},
        {"Transport",     "69%", 0.69f, C_TRANS},
        {"Entertainment", "70%", 0.70f, C_ENTER},
    };

    int iy = y + 44;
    for (int i = 0; i < 4; i++) {
        draw::text(r, f->small, items[i].name, x + PAD, iy, DIM);
        draw::text_right(r, f->small, items[i].detail, x + w - PAD, iy, WHITE);
        iy += 16;
        pbar(r, x + PAD, iy, w - PAD * 2, 8, items[i].pct, items[i].col);
        iy += 22;
    }

    card_btn(r, f, x, y + h - 24, w, "Edit Budgets");
}

// ── Recent Transactions Card ────────────────────────────────────

void FinanceApp::render_transactions_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h) {
    card_bg(r, x, y, w, h);

    draw::text(r, f->title, "Recent Transactions", x + PAD, y + PAD, WHITE);

    struct TxItem {
        const char* name;
        const char* amount;
        const char* date;
        SDL_Color col;
    };

    TxItem items[] = {
        {"Grocery Store", "-$75.32",  "May 15", C_FOOD},
        {"Gas Station",   "-$45.00",  "May 14", C_TRANS},
        {"Electricity",   "-$150.00", "May 13", C_HOUSING},
        {"Netflix",       "-$15.99",  "May 12", C_ENTER},
    };

    int iy = y + 44;
    for (int i = 0; i < 4; i++) {
        draw::filled_circle(r, x + PAD + 4, iy + 6, 4, items[i].col);
        draw::text(r, f->small, items[i].name, x + PAD + 14, iy, WHITE);
        draw::text(r, f->small, items[i].date, x + PAD + 14, iy + 14, DIM);
        draw::text_right(r, f->small, items[i].amount, x + w - PAD, iy, RED);

        if (i < 3) {
            draw::line(r, x + PAD, iy + 30, x + w - PAD, iy + 30, FAINT);
        }
        iy += 36;
    }

    card_btn(r, f, x, y + h - 24, w, "View All");
}

// ── Goals Card ──────────────────────────────────────────────────

void FinanceApp::render_goals_card(SDL_Renderer* r, const Fonts* f, int x, int y, int w, int h) {
    card_bg(r, x, y, w, h);

    draw::text(r, f->title, "Goals", x + PAD, y + PAD, WHITE);

    // Emergency Fund
    int gy = y + 44;
    draw::text(r, f->small, "Emergency Fund", x + PAD, gy, WHITE);
    draw::text_right(r, f->small, "35%", x + w - PAD, gy, GREEN);
    gy += 16;
    draw::text(r, f->small, "$3,500 / $10,000", x + PAD, gy, DIM);
    gy += 16;
    pbar(r, x + PAD, gy, w - PAD * 2, 8, 0.35f, GREEN);
    gy += 20;

    // Vacation
    draw::text(r, f->small, "Vacation", x + PAD, gy, WHITE);
    draw::text_right(r, f->small, "40%", x + w - PAD, gy, PURPLE);
    gy += 16;
    draw::text(r, f->small, "$2,000 / $5,000", x + PAD, gy, DIM);
    gy += 16;
    pbar(r, x + PAD, gy, w - PAD * 2, 8, 0.40f, PURPLE);

    // Add New Goal button
    card_btn(r, f, x, y + h - 24, w, "+ Add New Goal");
}

// ── Input handlers ──────────────────────────────────────────────

void FinanceApp::on_mouse_move(int local_x, int local_y) {
    (void)local_x;
    (void)local_y;
}

void FinanceApp::on_scroll(int local_x, int local_y, int scroll_y) {
    (void)local_x;
    (void)local_y;

    scroll_y_ -= scroll_y * 20;

    int visible_h = last_rect_.h;
    int max_scroll = content_h_ - visible_h;
    if (max_scroll < 0) max_scroll = 0;

    if (scroll_y_ < 0) scroll_y_ = 0;
    if (scroll_y_ > max_scroll) scroll_y_ = max_scroll;
}
