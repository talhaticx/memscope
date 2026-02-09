#define _POSIX_C_SOURCE 200809L
#include "ui/display.h"
#include "ui/gauge.h"
#include "ui/sparkline.h"
#include "ui/view.h"
#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// --- Color Definitions ---
#define COL_HEADER    1
#define COL_ROW       2
#define COL_SELECTED  3
#define COL_BAR       4
#define COL_BAR_LOW   5
#define COL_BAR_MED   6
#define COL_BAR_HIGH  7
#define COL_TAG_LEAK  8
#define COL_TAG_SPIKE 9
#define COL_BORDER    10
#define COL_FOOTER    11

// Additional colors for specific display elements
#define COL_MEM_VAL   12   // Memory value color (green)
#define COL_CPU_LOW   13   // Low CPU (green)
#define COL_CPU_MED   14   // Medium CPU (yellow) 
#define COL_CPU_HIGH  15   // High CPU (red)
#define COL_DELTA_POS 16   // Positive delta (yellow)
#define COL_DELTA_NEG 17   // Negative delta (green)

// --- State ---
static int group_mode = 0; // 0 = List, 1 = Group
static int scroll_offset = 0;

// --- Auto-baseline tracking (first-seen RSS per PID) ---
#define BASELINE_HASH_SIZE 4096
typedef struct {
    pid_t pid;
    uint64_t first_rss;  // First-seen RSS in bytes
    uint64_t start_time; // To detect PID reuse
} baseline_entry_t;
static baseline_entry_t baseline_hash[BASELINE_HASH_SIZE];

// Get or create baseline entry for a process
static uint64_t get_first_rss(pid_t pid, uint64_t current_rss, uint64_t start_time) {
    size_t idx = (size_t)pid % BASELINE_HASH_SIZE;
    baseline_entry_t *e = &baseline_hash[idx];
    
    // Check if this is same process (PID + start_time match) or new
    if (e->pid == pid && e->start_time == start_time) {
        return e->first_rss;  // Return cached first-seen value
    }
    
    // New process or PID reused - set new baseline
    e->pid = pid;
    e->first_rss = current_rss;
    e->start_time = start_time;
    return current_rss;
}

// Clear all baselines (when user presses Space for manual baseline)
static void clear_all_baselines(void) {
    memset(baseline_hash, 0, sizeof(baseline_hash));
}

typedef enum {
    SORT_CPU_DESC = 0,
    SORT_RSS_DESC,
    SORT_RSS_DELTA_DESC,
    SORT_PID_ASC
} sort_mode_t;

static sort_mode_t current_sort = SORT_RSS_DESC;

// --- View Item (List Mode) ---
typedef struct {
    const process_snapshot_t *proc;
    double cpu_pct;
    double rss_delta_mb;
    uint8_t tags;
} proc_view_t;

// --- Group Item (Group Mode) ---
typedef struct {
    char name[64];
    int count;
    uint64_t total_rss;
    double total_cpu;
    double total_delta;
    uint8_t tags; // Combined tags
    pid_t representative_pid; // PID of one process in group (for inspection)
} proc_group_t;

#define TAG_LEAK   0x01
#define TAG_SPIKE  0x02
#define TAG_ZOMBIE 0x04
#define TAG_HOARDER 0x08

// --- Helper: Get CPU color based on usage ---
static int get_cpu_color(double cpu_pct) {
    if (cpu_pct > 80.0) return COL_CPU_HIGH;
    if (cpu_pct > 40.0) return COL_CPU_MED;
    return COL_CPU_LOW;
}

// --- Comparison Functions ---
static int cmp_cpu(const void *a, const void *b) {
    const proc_view_t *pa = a, *pb = b;
    if (pa->cpu_pct != pb->cpu_pct) return (pa->cpu_pct < pb->cpu_pct) - (pa->cpu_pct > pb->cpu_pct);
    return pa->proc->pid - pb->proc->pid;
}
static int cmp_rss(const void *a, const void *b) {
    const proc_view_t *pa = a, *pb = b;
    if (pa->proc->rss_bytes != pb->proc->rss_bytes) return (pa->proc->rss_bytes < pb->proc->rss_bytes) - (pa->proc->rss_bytes > pb->proc->rss_bytes);
    return pa->proc->pid - pb->proc->pid;
}
static int cmp_delta(const void *a, const void *b) {
    const proc_view_t *pa = a, *pb = b;
    if (pa->rss_delta_mb != pb->rss_delta_mb) return (pa->rss_delta_mb < pb->rss_delta_mb) - (pa->rss_delta_mb > pb->rss_delta_mb);
    return pa->proc->pid - pb->proc->pid;
}
static int cmp_pid(const void *a, const void *b) {
    const proc_view_t *pa = a, *pb = b;
    return pa->proc->pid - pb->proc->pid;
}

// Group comparisons
static int cmp_group_cpu(const void *a, const void *b) {
    const proc_group_t *ga = a, *gb = b;
    if (ga->total_cpu != gb->total_cpu) return (ga->total_cpu < gb->total_cpu) - (ga->total_cpu > gb->total_cpu);
    return strcmp(ga->name, gb->name);
}
static int cmp_group_rss(const void *a, const void *b) {
    const proc_group_t *ga = a, *gb = b;
    if (ga->total_rss != gb->total_rss) return (ga->total_rss < gb->total_rss) - (ga->total_rss > gb->total_rss);
    return strcmp(ga->name, gb->name);
}
static int cmp_group_delta(const void *a, const void *b) {
    const proc_group_t *ga = a, *gb = b;
    if (ga->total_delta != gb->total_delta) return (ga->total_delta < gb->total_delta) - (ga->total_delta > gb->total_delta);
    return strcmp(ga->name, gb->name);
}

// --- Find process in prev sample ---
static const process_snapshot_t *find_prev(const sample_t *prev, pid_t pid, int *hint) {
    if (!prev) return NULL;
    for (int i = *hint; i < (int)prev->process_count; i++) {
        if (prev->processes[i].pid == pid) { *hint = i; return &prev->processes[i]; }
        if (prev->processes[i].pid > pid) return NULL;
    }
    return NULL;
}

// --- Public Functions ---
void ui_init(void) {
    // Enable UTF-8 support (MUST be before initscr)
    setlocale(LC_ALL, "");
    
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(0);
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        
        // btop-style color scheme: muted, professional
        init_pair(COL_HEADER,    COLOR_BLACK, COLOR_CYAN);
        init_pair(COL_ROW,       COLOR_WHITE, -1);        // Process names in white
        init_pair(COL_SELECTED,  COLOR_BLACK, COLOR_WHITE);
        init_pair(COL_BAR,       COLOR_GREEN, -1);
        init_pair(COL_BAR_LOW,   COLOR_GREEN, -1);
        init_pair(COL_BAR_MED,   COLOR_YELLOW, -1);
        init_pair(COL_BAR_HIGH,  COLOR_RED, -1);
        init_pair(COL_TAG_LEAK,  COLOR_RED, -1);
        init_pair(COL_TAG_SPIKE, COLOR_YELLOW, -1);       // Yellow for SPIKE (btop style)
        init_pair(COL_BORDER,    COLOR_CYAN, -1);         // Cyan borders (btop style)
        init_pair(COL_FOOTER,    COLOR_CYAN, -1);
        
        // Data value colors
        init_pair(COL_MEM_VAL,   COLOR_GREEN, -1);        // Memory values in green
        init_pair(COL_CPU_LOW,   COLOR_GREEN, -1);        // Low CPU green
        init_pair(COL_CPU_MED,   COLOR_YELLOW, -1);       // Medium CPU yellow
        init_pair(COL_CPU_HIGH,  COLOR_RED, -1);          // High CPU red
        init_pair(COL_DELTA_POS, COLOR_YELLOW, -1);       // Positive delta yellow
        init_pair(COL_DELTA_NEG, COLOR_GREEN, -1);        // Negative delta green
    }
}

void ui_cleanup(void) {
    endwin();
}

void ui_toggle_grouping(void) {
    group_mode = !group_mode;
    scroll_offset = 0; // Reset scroll on toggle
}

void ui_reset_baselines(void) {
    clear_all_baselines();
}

void ui_draw(const sample_t *curr, const sample_t *compare, int selected_idx, int has_baseline) {
    erase();
    int screen_w = COLS;
    int screen_h = LINES;
    int visible_rows = screen_h - 6;

    // Initialize ViewState for this frame
    view_begin_frame(selected_idx, scroll_offset, visible_rows, group_mode);
    if (scroll_offset < 0) scroll_offset = 0;

    // Header
    attron(COLOR_PAIR(COL_HEADER) | A_BOLD);
    mvhline(0, 0, ' ', screen_w);
    mvprintw(0, 1, " MEMSCOPE %s", group_mode ? "[GROUP] " : "");
    attroff(COLOR_PAIR(COL_HEADER) | A_BOLD);
    
    // Baseline indicator (after MEMSCOPE title)
    if (has_baseline) {
        attron(COLOR_PAIR(COL_TAG_SPIKE) | A_BOLD);
        printw("[BASELINE]");
        attroff(COLOR_PAIR(COL_TAG_SPIKE) | A_BOLD);
    }

    // System Stats
    double ram_used = (curr->system_total_ram - curr->system_free_ram) / (1024.0 * 1024.0 * 1024.0);
    double ram_total = curr->system_total_ram / (1024.0 * 1024.0 * 1024.0);
    double ram_pct = (ram_total > 0) ? (ram_used / ram_total) * 100 : 0;
    
    mvprintw(0, 20, "RAM ");
    gauge_draw(0, 24, 16, ram_pct, 100.0);
    mvprintw(0, 41, "%.1f/%.0fG", ram_used, ram_total);
    
    // Sort Indicator
    const char *sort_str = "CPU";
    if (current_sort == SORT_RSS_DESC) sort_str = "MEM";
    if (current_sort == SORT_RSS_DELTA_DESC) sort_str = "DELTA";
    if (current_sort == SORT_PID_ASC) sort_str = "PID";
    mvprintw(0, screen_w - 15, "[SORT: %s]", sort_str);

    // Columns
    attron(COLOR_PAIR(COL_BORDER));
    mvhline(1, 0, '-', screen_w);
    attroff(COLOR_PAIR(COL_BORDER));
    
    attron(A_BOLD);
    if (group_mode) {
        mvprintw(2, 0, " %-16s %5s %9s %6s %10s %s", "GROUP", "CNT", "RSS", "CPU%", "DELTA", "TAGS");
    } else {
        mvprintw(2, 0, " %-6s %-16s %9s %6s %10s %s", "PID", "NAME", "RSS", "CPU%", "DELTA", "TAGS");
    }
    attroff(A_BOLD);
    
    attron(COLOR_PAIR(COL_BORDER));
    mvhline(3, 0, '-', screen_w);
    attroff(COLOR_PAIR(COL_BORDER));

    // Data Processing
    void *items = NULL;
    size_t count = 0;

    if (!group_mode) {
        // --- LIST MODE ---
        proc_view_t *views = malloc(sizeof(proc_view_t) * curr->process_count);
        uint64_t dt_ms = compare ? (curr->timestamp_ms - compare->timestamp_ms) : 1;
        if (dt_ms == 0) dt_ms = 1;
        int hint = 0;
        
        for (size_t i = 0; i < curr->process_count; i++) {
            const process_snapshot_t *p = &curr->processes[i];
            views[i].proc = p;
            views[i].cpu_pct = 0;
            views[i].rss_delta_mb = 0;
            views[i].tags = 0;
            uint64_t first_rss = get_first_rss(p->pid, p->rss_bytes, p->start_time);
            views[i].rss_delta_mb = ((int64_t)p->rss_bytes - (int64_t)first_rss) / (1024.0 * 1024.0);
            const process_snapshot_t *old = find_prev(compare, p->pid, &hint);
            if (old) {
                uint64_t cpu_d = (p->utime_ms + p->stime_ms) - (old->utime_ms + old->stime_ms);
                views[i].cpu_pct = (double)cpu_d * 100.0 / (double)dt_ms;
            }
            if (views[i].rss_delta_mb > 10.0) views[i].tags |= TAG_LEAK;
            if (views[i].cpu_pct > 80.0) views[i].tags |= TAG_SPIKE;
            if (p->state == 'Z') views[i].tags |= TAG_ZOMBIE;
            if (p->rss_bytes > 2ULL * 1024 * 1024 * 1024) views[i].tags |= TAG_HOARDER;
        }
        
        // Sort
        switch (current_sort) {
            case SORT_CPU_DESC: qsort(views, curr->process_count, sizeof(proc_view_t), cmp_cpu); break;
            case SORT_RSS_DESC: qsort(views, curr->process_count, sizeof(proc_view_t), cmp_rss); break;
            case SORT_RSS_DELTA_DESC: qsort(views, curr->process_count, sizeof(proc_view_t), cmp_delta); break;
            case SORT_PID_ASC: qsort(views, curr->process_count, sizeof(proc_view_t), cmp_pid); break;
        }
        items = views;
        count = curr->process_count;
        
        // Populate ViewState with sorted data
        for (size_t i = 0; i < count && i < VIEW_MAX_ROWS; i++) {
            ViewRow vr = {
                .pid = views[i].proc->pid,
                .cpu_pct = views[i].cpu_pct,
                .rss_bytes = views[i].proc->rss_bytes,
                .rss_delta_mb = views[i].rss_delta_mb,
                .tags = views[i].tags,
                .is_group = false,
                .group_count = 1
            };
            strncpy(vr.comm, views[i].proc->comm, sizeof(vr.comm) - 1);
            view_add_row(&vr);
        }
    } else {
        // --- GROUP MODE ---
        // Max groups possible is process_count, usually much less
        proc_group_t *groups = calloc(curr->process_count, sizeof(proc_group_t));
        count = 0;
        
        uint64_t dt_ms = compare ? (curr->timestamp_ms - compare->timestamp_ms) : 1;
        if (dt_ms == 0) dt_ms = 1;
        int hint = 0;

        for (size_t i = 0; i < curr->process_count; i++) {
            const process_snapshot_t *p = &curr->processes[i];
            
            // Find existing group
            int found = -1;
            for (size_t g = 0; g < count; g++) {
                if (strncmp(groups[g].name, p->comm, 63) == 0) {
                    found = g;
                    break;
                }
            }
            
            if (found == -1) {
                found = count++;
                strncpy(groups[found].name, p->comm, 63);
                groups[found].representative_pid = p->pid; // Use first found as rep
            }
            
            proc_group_t *grp = &groups[found];
            grp->count++;
            grp->total_rss += p->rss_bytes;
            if (p->rss_bytes > 2ULL * 1024 * 1024 * 1024) grp->tags |= TAG_HOARDER;
            if (p->state == 'Z') grp->tags |= TAG_ZOMBIE;

            const process_snapshot_t *old = find_prev(compare, p->pid, &hint);
            if (old) {
                uint64_t cpu_d = (p->utime_ms + p->stime_ms) - (old->utime_ms + old->stime_ms);
                double cpu = (double)cpu_d * 100.0 / (double)dt_ms;
                double delta = ((int64_t)p->rss_bytes - (int64_t)old->rss_bytes) / (1024.0 * 1024.0);
                
                grp->total_cpu += cpu;
                grp->total_delta += delta;
            }
        }
        
        // Tags for groups
        for (size_t i = 0; i < count; i++) {
            if (groups[i].total_delta > 15.0) groups[i].tags |= TAG_LEAK; // Higher threshold for groups
            if (groups[i].total_cpu > 100.0) groups[i].tags |= TAG_SPIKE; // > 1 core
        }
        
        // Sort groups
        switch (current_sort) {
            case SORT_CPU_DESC: qsort(groups, count, sizeof(proc_group_t), cmp_group_cpu); break;
            case SORT_RSS_DESC: qsort(groups, count, sizeof(proc_group_t), cmp_group_rss); break;
            case SORT_RSS_DELTA_DESC: qsort(groups, count, sizeof(proc_group_t), cmp_group_delta); break;
            default: qsort(groups, count, sizeof(proc_group_t), cmp_group_cpu); break;
        }
        items = groups;
        
        // Populate ViewState with sorted group data
        for (size_t i = 0; i < count && i < VIEW_MAX_ROWS; i++) {
            ViewRow vr = {
                .pid = groups[i].representative_pid,
                .cpu_pct = groups[i].total_cpu,
                .rss_bytes = groups[i].total_rss,
                .rss_delta_mb = groups[i].total_delta,
                .tags = groups[i].tags,
                .is_group = true,
                .group_count = groups[i].count
            };
            strncpy(vr.comm, groups[i].name, sizeof(vr.comm) - 1);
            view_add_row(&vr);
        }
    }

    // Finalize ViewState and get accurate count
    view_end_frame();
    const ViewState *vs = view_get_state();
    count = vs->count;  // Use ViewState count
    
    // Scroll Logic (based on finalized ViewState)
    if (selected_idx < scroll_offset) scroll_offset = selected_idx;
    if (selected_idx >= scroll_offset + visible_rows) scroll_offset = selected_idx - visible_rows + 1;
    if (scroll_offset < 0) scroll_offset = 0;

    // Draw Items
    int row_start = 4;
    int rows_to_draw = visible_rows;
    
    for (int i = 0; i < rows_to_draw; i++) {
        int idx = scroll_offset + i;
        if (idx >= (int)count) break;
        int row = row_start + i;
        
        if (idx == selected_idx) attron(COLOR_PAIR(COL_SELECTED));
        mvhline(row, 0, ' ', screen_w);
        
        if (!group_mode) {
            // List Row - btop style
            proc_view_t *v = &((proc_view_t*)items)[idx];
            
            // PID in default color
            mvprintw(row, 0, " %-6d", v->proc->pid);
            
            // Process name in WHITE (btop style - no rainbow)
            if (idx != selected_idx) attron(COLOR_PAIR(COL_ROW) | A_BOLD);
            mvprintw(row, 8, "%-16.16s", v->proc->comm);
            if (idx != selected_idx) attroff(COLOR_PAIR(COL_ROW) | A_BOLD);
            
            // Memory value in GREEN (right-aligned)
            if (idx != selected_idx) attron(COLOR_PAIR(COL_MEM_VAL));
            mvprintw(row, 25, "%7.1f MB", v->proc->rss_bytes / (1024.0*1024.0));
            if (idx != selected_idx) attroff(COLOR_PAIR(COL_MEM_VAL));
            
            // CPU value - color coded
            int cpu_col = get_cpu_color(v->cpu_pct);
            if (idx != selected_idx) attron(COLOR_PAIR(cpu_col));
            mvprintw(row, 36, "%6.1f%%", v->cpu_pct);
            if (idx != selected_idx) attroff(COLOR_PAIR(cpu_col));
            
            // Delta - green if negative, yellow if positive
            int delta_col = (v->rss_delta_mb >= 0) ? COL_DELTA_POS : COL_DELTA_NEG;
            if (idx != selected_idx) attron(COLOR_PAIR(delta_col));
            mvprintw(row, 44, "%+8.2f MB", v->rss_delta_mb);
            if (idx != selected_idx) attroff(COLOR_PAIR(delta_col));
            
            // Tags
            int tag_x = 58;
            if (v->tags & TAG_LEAK) { 
                if (idx != selected_idx) attron(COLOR_PAIR(COL_TAG_LEAK)); 
                mvaddstr(row, tag_x, "LEAK "); 
                if (idx != selected_idx) attroff(COLOR_PAIR(COL_TAG_LEAK)); 
                tag_x+=5; 
            }
            if (v->tags & TAG_SPIKE) { 
                if (idx != selected_idx) attron(COLOR_PAIR(COL_TAG_SPIKE)); 
                mvaddstr(row, tag_x, "SPIKE"); 
                if (idx != selected_idx) attroff(COLOR_PAIR(COL_TAG_SPIKE)); 
            }
        } else {
            // Group Row - btop style
            proc_group_t *g = &((proc_group_t*)items)[idx];
            
            // Group name in WHITE
            if (idx != selected_idx) attron(COLOR_PAIR(COL_ROW) | A_BOLD);
            mvprintw(row, 1, "%-16.16s", g->name);
            if (idx != selected_idx) attroff(COLOR_PAIR(COL_ROW) | A_BOLD);
            
            mvprintw(row, 18, "(x%d)", g->count);
            
            // Memory in GREEN
            if (idx != selected_idx) attron(COLOR_PAIR(COL_MEM_VAL));
            mvprintw(row, 25, "%7.1f MB", g->total_rss / (1024.0*1024.0));
            if (idx != selected_idx) attroff(COLOR_PAIR(COL_MEM_VAL));
            
            // CPU color coded  
            int cpu_col = get_cpu_color(g->total_cpu);
            if (idx != selected_idx) attron(COLOR_PAIR(cpu_col));
            mvprintw(row, 36, "%6.1f%%", g->total_cpu);
            if (idx != selected_idx) attroff(COLOR_PAIR(cpu_col));
            
            // Delta
            int delta_col = (g->total_delta >= 0) ? COL_DELTA_POS : COL_DELTA_NEG;
            if (idx != selected_idx) attron(COLOR_PAIR(delta_col));
            mvprintw(row, 44, "%+8.2f MB", g->total_delta);
            if (idx != selected_idx) attroff(COLOR_PAIR(delta_col));
                    
            int tag_x = 58;
            if (g->tags & TAG_LEAK) { 
                if (idx != selected_idx) attron(COLOR_PAIR(COL_TAG_LEAK)); 
                mvaddstr(row, tag_x, "LEAK "); 
                if (idx != selected_idx) attroff(COLOR_PAIR(COL_TAG_LEAK)); 
                tag_x+=5; 
            }
            if (g->tags & TAG_SPIKE) { 
                if (idx != selected_idx) attron(COLOR_PAIR(COL_TAG_SPIKE)); 
                mvaddstr(row, tag_x, "SPIKE"); 
                if (idx != selected_idx) attroff(COLOR_PAIR(COL_TAG_SPIKE)); 
            }
        }
        
        if (idx == selected_idx) attroff(COLOR_PAIR(COL_SELECTED));
    }

    // Footer
    attron(COLOR_PAIR(COL_BORDER));
    mvhline(screen_h - 2, 0, '-', screen_w);
    attroff(COLOR_PAIR(COL_BORDER));
    
    attron(COLOR_PAIR(COL_FOOTER));
    mvprintw(screen_h - 1, 1, 
             " 'g':Group/list | Space:Baseline | Enter:Inspect | q:Quit | %zu procs ", curr->process_count);
    attroff(COLOR_PAIR(COL_FOOTER));

    refresh();
    free(items);
}

// Get selected PID using ViewState (THE FIX for the Enter key bug)
// This now reads from the frozen snapshot, NOT recalculating
pid_t ui_get_selected_pid(const sample_t *curr, const sample_t *prev, int selected_idx) {
    (void)curr;  // Unused - we use ViewState now
    (void)prev;
    (void)selected_idx;
    
    // Simply delegate to the ViewState which was frozen during render
    return view_get_selected_pid();
}

void ui_draw_detail(pid_t pid, const smaps_breakdown_t *smaps, const smaps_breakdown_t *baseline,
                   const sample_t *curr, const double *history, int history_count) {
    erase();
    int w = COLS, h = LINES;
    
    const process_snapshot_t *proc = NULL;
    for (size_t i = 0; i < curr->process_count; i++) {
        if (curr->processes[i].pid == pid) { proc = &curr->processes[i]; break; }
    }

    // === HEADER ===
    attron(COLOR_PAIR(COL_HEADER) | A_BOLD);
    mvhline(0, 0, ' ', w);
    mvprintw(0, 1, " INSPECT: %s (PID %d) ", proc ? proc->comm : "???", pid);
    attroff(COLOR_PAIR(COL_HEADER) | A_BOLD);
    
    // Divider
    attron(COLOR_PAIR(COL_BORDER));
    mvhline(1, 0, '-', w);
    attroff(COLOR_PAIR(COL_BORDER));

    // === LAYOUT: Two columns ===
    // Left column (2-45): Memory breakdown
    // Right column (48-end): Memory trend sparkline
    
    int left_col = 2;
    int right_col = 48;
    
    // === LEFT COLUMN: MEMORY BREAKDOWN ===
    int row = 3;
    attron(A_BOLD);
    mvprintw(row++, left_col, "MEMORY BREAKDOWN");
    attroff(A_BOLD);
    
    attron(COLOR_PAIR(COL_BORDER));
    mvhline(row++, left_col, '-', 44);
    attroff(COLOR_PAIR(COL_BORDER));
    
    double total_rss = smaps->total_rss_kb / 1024.0;
    double baseline_rss = baseline ? baseline->total_rss_kb / 1024.0 : total_rss;
    // Use 2x baseline as max for gauge to show growth visually
    // At baseline, gauge is 50% full. At 2x baseline, 100% full.
    double gauge_max = baseline_rss * 2.0;
    if (gauge_max < 1.0) gauge_max = total_rss * 2.0;
    if (gauge_max < 1.0) gauge_max = 100.0; // Fallback
    int has_data = (total_rss > 0.01); // Check if we have real data
    if (total_rss < 0.01) total_rss = 1.0; // Avoid div by zero for gauge
    
    // Show message for kernel threads (no userspace memory)
    if (!has_data) {
        attron(COLOR_PAIR(COL_BORDER));
        mvprintw(row++, left_col, "(Kernel thread - no userspace memory)");
        attroff(COLOR_PAIR(COL_BORDER));
        row++;
    }
    
    // Memory entries with clean formatting
    // Format: Label    [========    ]  Value  (Pct%)
    
    // Private/Heap
    double heap_mb = smaps->heap_kb / 1024.0;
    double heap_baseline = baseline ? baseline->heap_kb / 1024.0 : heap_mb;
    double heap_delta = heap_mb - heap_baseline;
    double heap_max = heap_baseline * 2.0;
    if (heap_max < 1.0) heap_max = gauge_max;
    mvprintw(row, left_col, "Private");
    gauge_draw_colored(row, left_col + 9, 16, heap_mb, heap_max, COL_BAR_MED);
    attron(COLOR_PAIR(COL_MEM_VAL));
    mvprintw(row, left_col + 26, "%7.2f MB", heap_mb);
    attroff(COLOR_PAIR(COL_MEM_VAL));
    // Delta
    int delta_col = (heap_delta >= 0) ? COL_DELTA_POS : COL_DELTA_NEG;
    attron(COLOR_PAIR(delta_col));
    mvprintw(row, left_col + 37, "%+.1f", heap_delta);
    attroff(COLOR_PAIR(delta_col));
    row++;
    
    // Anon
    double anon_mb = smaps->anon_kb / 1024.0;
    double anon_baseline = baseline ? baseline->anon_kb / 1024.0 : anon_mb;
    double anon_delta = anon_mb - anon_baseline;
    double anon_max = anon_baseline * 2.0;
    if (anon_max < 1.0) anon_max = gauge_max;
    mvprintw(row, left_col, "Anon");
    gauge_draw_colored(row, left_col + 9, 16, anon_mb, anon_max, COL_BAR_LOW);
    attron(COLOR_PAIR(COL_MEM_VAL));
    mvprintw(row, left_col + 26, "%7.2f MB", anon_mb);
    attroff(COLOR_PAIR(COL_MEM_VAL));
    delta_col = (anon_delta >= 0) ? COL_DELTA_POS : COL_DELTA_NEG;
    attron(COLOR_PAIR(delta_col));
    mvprintw(row, left_col + 37, "%+.1f", anon_delta);
    attroff(COLOR_PAIR(delta_col));
    row++;
    
    // Shared/File-backed
    double shared_mb = smaps->shared_kb / 1024.0;
    double shared_baseline = baseline ? baseline->shared_kb / 1024.0 : shared_mb;
    double shared_delta = shared_mb - shared_baseline;
    double shared_max = shared_baseline * 2.0;
    if (shared_max < 1.0) shared_max = gauge_max > 10.0 ? gauge_max * 0.1 : 10.0; // File is usually small
    mvprintw(row, left_col, "File");
    gauge_draw_colored(row, left_col + 9, 16, shared_mb, shared_max, COL_BORDER);
    attron(COLOR_PAIR(COL_MEM_VAL));
    mvprintw(row, left_col + 26, "%7.2f MB", shared_mb);
    attroff(COLOR_PAIR(COL_MEM_VAL));
    delta_col = (shared_delta >= 0) ? COL_DELTA_POS : COL_DELTA_NEG;
    attron(COLOR_PAIR(delta_col));
    mvprintw(row, left_col + 37, "%+.1f", shared_delta);
    attroff(COLOR_PAIR(delta_col));
    row++;
    
    // Swap
    double swap_mb = smaps->swap_kb / 1024.0;
    mvprintw(row, left_col, "Swap");
    int swap_col = (swap_mb > 0) ? COL_TAG_LEAK : COL_BORDER;
    gauge_draw_colored(row, left_col + 9, 18, swap_mb, (swap_mb > 0 ? swap_mb : 1.0), swap_col);
    if (swap_mb > 0) attron(COLOR_PAIR(COL_TAG_LEAK));
    else attron(COLOR_PAIR(COL_MEM_VAL));
    mvprintw(row, left_col + 28, "%7.2f MB", swap_mb);
    if (swap_mb > 0) attroff(COLOR_PAIR(COL_TAG_LEAK));
    else attroff(COLOR_PAIR(COL_MEM_VAL));
    row++;

    // Total line
    row++;
    attron(COLOR_PAIR(COL_BORDER));
    mvhline(row++, left_col, '-', 40);
    attroff(COLOR_PAIR(COL_BORDER));
    
    attron(A_BOLD);
    double pss_mb = smaps->total_pss_kb / 1024.0;
    double rss_delta = total_rss - baseline_rss;
    mvprintw(row, left_col, "Total RSS:");
    attron(COLOR_PAIR(COL_MEM_VAL));
    mvprintw(row, left_col + 26, "%7.2f MB", total_rss);
    attroff(COLOR_PAIR(COL_MEM_VAL));
    delta_col = (rss_delta >= 0) ? COL_DELTA_POS : COL_DELTA_NEG;
    attron(COLOR_PAIR(delta_col));
    mvprintw(row, left_col + 37, "%+.1f", rss_delta);
    attroff(COLOR_PAIR(delta_col));
    row++;
    mvprintw(row, left_col, "Total PSS:");
    mvprintw(row, left_col + 26, "%7.2f MB", pss_mb);
    attroff(A_BOLD);
    row++;
    
    // Swap warning
    if (smaps->swap_kb > 0) {
        row++;
        attron(COLOR_PAIR(COL_TAG_LEAK) | A_BOLD);
        mvprintw(row, left_col, "!! SWAPPING DETECTED !!");
        attroff(COLOR_PAIR(COL_TAG_LEAK) | A_BOLD);
    }

    // === RIGHT COLUMN: MEMORY HISTORY ===
    row = 3;
    attron(A_BOLD);
    mvprintw(row++, right_col, "MEMORY TREND (Last %d sec)", history_count > 0 ? history_count : 0);
    attroff(A_BOLD);
    
    attron(COLOR_PAIR(COL_BORDER));
    mvhline(row++, right_col, '-', w - right_col - 2);
    attroff(COLOR_PAIR(COL_BORDER));
    
    // Sparkline display - multi-row vertical bar graph
    if (history_count > 0) {
        // Find min/max for display
        double hmin = history[0], hmax = history[0];
        for (int i = 1; i < history_count; i++) {
            if (history[i] < hmin) hmin = history[i];
            if (history[i] > hmax) hmax = history[i];
        }
        
        // Graph dimensions - fit within right column
        int graph_height = 6;  // Number of rows for the graph
        int spark_width = w - right_col - 12; // Leave room for Y-axis labels
        if (spark_width > 40) spark_width = 40;
        if (spark_width < 10) spark_width = 10;
        if (spark_width > history_count) spark_width = history_count;
        
        // Calculate range with padding
        double range = hmax - hmin;
        if (range < 1.0) range = 1.0; // Minimum range of 1MB
        double padded_min = hmin - range * 0.05;
        double padded_max = hmax + range * 0.05;
        if (padded_min < 0) padded_min = 0;
        double padded_range = padded_max - padded_min;
        if (padded_range < 1.0) padded_range = 1.0;
        
        // Draw Y-axis labels (top, middle, bottom)
        attron(COLOR_PAIR(COL_BORDER));
        mvprintw(row, right_col, "%6.0f", padded_max);
        mvprintw(row + graph_height/2, right_col, "%6.0f", (padded_max + padded_min) / 2.0);
        mvprintw(row + graph_height - 1, right_col, "%6.0f", padded_min);
        attroff(COLOR_PAIR(COL_BORDER));
        
        // Draw multi-row graph
        int graph_x = right_col + 7;
        size_t start_idx = (size_t)history_count > (size_t)spark_width ? (size_t)history_count - (size_t)spark_width : 0;
        
        for (int graph_row = 0; graph_row < graph_height; graph_row++) {
            double row_threshold = padded_max - (padded_range * (graph_row + 0.5) / graph_height);
            
            for (int col = 0; col < spark_width && (start_idx + col) < (size_t)history_count; col++) {
                double val = history[start_idx + col];
                
                // Determine fill level for this cell
                double cell_top = padded_max - (padded_range * graph_row / graph_height);
                double cell_bottom = padded_max - (padded_range * (graph_row + 1) / graph_height);
                
                move(row + graph_row, graph_x + col);
                
                if (val >= cell_top) {
                    // Full block
                    attron(COLOR_PAIR(COL_BAR_LOW) | A_BOLD);
                    addstr("█");
                    attroff(COLOR_PAIR(COL_BAR_LOW) | A_BOLD);
                } else if (val > cell_bottom) {
                    // Partial block - calculate which Unicode block char to use
                    double fill = (val - cell_bottom) / (cell_top - cell_bottom);
                    const char *blocks[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
                    int idx = (int)(fill * 8);
                    if (idx < 0) idx = 0;
                    if (idx > 8) idx = 8;
                    attron(COLOR_PAIR(COL_BAR_LOW));
                    addstr(blocks[idx]);
                    attroff(COLOR_PAIR(COL_BAR_LOW));
                } else {
                    // Empty
                    addstr(" ");
                }
            }
        }
        
        row += graph_height + 1;
        
        // Min/Max/Current labels
        mvprintw(row++, right_col, "Range: %.1f - %.1f MB", hmin, hmax);
        attron(COLOR_PAIR(COL_MEM_VAL) | A_BOLD);
        mvprintw(row++, right_col, "Current: %.2f MB", history[history_count - 1]);
        attroff(COLOR_PAIR(COL_MEM_VAL) | A_BOLD);
    } else {
        attron(COLOR_PAIR(COL_BORDER));
        mvprintw(row++, right_col, "(collecting data...)");
        attroff(COLOR_PAIR(COL_BORDER));
    }

    // === FOOTER ===
    attron(COLOR_PAIR(COL_BORDER));
    mvhline(h - 2, 0, '-', w);
    attroff(COLOR_PAIR(COL_BORDER));
    
    attron(COLOR_PAIR(COL_FOOTER));
    mvprintw(h - 1, 1, " ESC:Back | Space:Reset Baseline | RSS=Resident  PSS=Proportional ");
    attroff(COLOR_PAIR(COL_FOOTER));
    
    refresh();
}

int ui_poll_input(void) {
    int ch = getch();
    if (ch == ERR) return 0;
    flushinp();
    switch (ch) {
        case 'q': case 27: return 1;
        case KEY_UP: case 'k': return 2;
        case KEY_DOWN: case 'j': return 3;
        case 10: case KEY_ENTER: return 4;
        case ' ': return 5;
        case 'w': return 6;
        case 'g': return 7; // Group Toggle
        
        case 'c': current_sort = SORT_CPU_DESC; break;
        case 'm': current_sort = SORT_RSS_DESC; break;
        case 'd': current_sort = SORT_RSS_DELTA_DESC; break;
        case 'p': current_sort = SORT_PID_ASC; break;
    }
    return 0;
}