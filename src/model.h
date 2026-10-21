#ifndef DAYBOOK_MODEL_H
#define DAYBOOK_MODEL_H
#include "../vendor/cJSON.h"
#include <stddef.h>
#define MAX_TASKS 256
#define TITLE_MAX 160
#define REPLY_MAX 3900
#define HISTORY_DAYS 90
#define MAX_JOURNAL 128
#define JOURNAL_MAX 2048
typedef struct {
  int id, kind, done;
  char title[TITLE_MAX + 1], period[11], completed_on[11];
  int days_count;
  char days[HISTORY_DAYS][11];
} Task;
typedef struct {
  char day[11];
  int total, done, habit_total, habit_done;
} DayProgress;
typedef struct {
  int id;
  char day[11], body[JOURNAL_MAX + 1];
} JournalEntry;
typedef struct {
  int next_id, count, progress_count, next_journal_id, journal_count;
  long long offset;
  Task tasks[MAX_TASKS];
  DayProgress progress[HISTORY_DAYS];
  JournalEntry journal[MAX_JOURNAL];
} Model;
void model_init(Model *m);
int model_load(Model *m, const char *path, char *err, size_t n);
int model_save(const Model *m, const char *path, char *err, size_t n);
int model_command(Model *m, const char *command, const char *day, char *reply,
                  size_t n);
int task_done(const Task *t, const char *day);
int task_visible(const Task *t, int tab, const char *day);
Task *task_find(Model *m, int id);
JournalEntry *journal_find(Model *m, int id);
void model_record_progress(Model *m, const char *day);
const DayProgress *model_progress(const Model *m, const char *day);
#endif
