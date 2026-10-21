#define _POSIX_C_SOURCE 200809L
#include "model.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void copy(char *d, const char *s, size_t n) {
  snprintf(d, n, "%s", s ? s : "");
}
void model_init(Model *m) {
  memset(m, 0, sizeof(*m));
  m->next_id = 1;
  m->next_journal_id = 1;
}
Task *task_find(Model *m, int id) {
  for (int i = 0; i < m->count; i++)
    if (m->tasks[i].id == id)
      return &m->tasks[i];
  return NULL;
}
JournalEntry *journal_find(Model *m, int id) {
  for (int i = 0; i < m->journal_count; i++)
    if (m->journal[i].id == id)
      return &m->journal[i];
  return NULL;
}
int task_done(const Task *t, const char *day) {
  if (t->kind != 1)
    return t->done;
  for (int i = 0; i < t->days_count; i++)
    if (!strcmp(t->days[i], day))
      return 1;
  return 0;
}
const DayProgress *model_progress(const Model *m, const char *day) {
  for (int i = 0; i < m->progress_count; i++)
    if (!strcmp(m->progress[i].day, day))
      return &m->progress[i];
  return NULL;
}
void model_record_progress(Model *m, const char *day) {
  DayProgress *p = NULL;
  for (int i = 0; i < m->progress_count; i++)
    if (!strcmp(m->progress[i].day, day))
      p = &m->progress[i];
  if (!p) {
    if (m->progress_count == HISTORY_DAYS) {
      memmove(m->progress, m->progress + 1,
              (HISTORY_DAYS - 1) * sizeof(*m->progress));
      m->progress_count--;
    }
    p = &m->progress[m->progress_count++];
    memset(p, 0, sizeof(*p));
    copy(p->day, day, sizeof(p->day));
  }
  p->total = p->done = p->habit_total = p->habit_done = 0;
  for (int i = 0; i < m->count; i++) {
    Task *t = &m->tasks[i];
    if (t->kind == 1) {
      p->habit_total++;
      p->habit_done += task_done(t, day);
    }
    if (task_visible(t, 0, day)) {
      p->total++;
      p->done += task_done(t, day);
    }
  }
}
int task_visible(const Task *t, int tab, const char *day) {
  if (tab == 0)
    return t->kind == 1 ||
           (t->kind == 0 && (!t->done || !strcmp(t->completed_on, day)));
  if (tab == 1)
    return t->kind == 1;
  return t->kind == 2 && strncmp(t->period, day, 7) == 0;
}
static int integer(cJSON *v, int min, int max) {
  return cJSON_IsNumber(v) && v->valuedouble >= min && v->valuedouble <= max &&
         v->valuedouble == (double)v->valueint;
}
int legacy_model_load(Model *m, const char *path, char *err, size_t n) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    if (errno == ENOENT) {
      model_init(m);
      return 1;
    }
    snprintf(err, n, "Cannot open task file: %s", strerror(errno));
    return 0;
  }
  if (fseek(f, 0, SEEK_END) || ftell(f) > 4 * 1024 * 1024) {
    fclose(f);
    copy(err, "Task file is too large", n);
    return 0;
  }
  long size = ftell(f);
  rewind(f);
  char *s = calloc((size_t)size + 1, 1);
  if (!s) {
    fclose(f);
    return 0;
  }
  size_t got = fread(s, 1, (size_t)size, f);
  fclose(f);
  cJSON *j = got == (size_t)size ? cJSON_Parse(s) : NULL;
  free(s);
  Model tmp;
  model_init(&tmp);
  cJSON *a = cJSON_GetObjectItemCaseSensitive(j, "tasks"),
        *next = cJSON_GetObjectItemCaseSensitive(j, "next_id"),
        *offset = cJSON_GetObjectItemCaseSensitive(j, "telegram_offset");
  int ok = cJSON_IsObject(j) &&
           integer(cJSON_GetObjectItemCaseSensitive(j, "schema"), 1, 3) &&
           cJSON_IsArray(a) && cJSON_GetArraySize(a) <= MAX_TASKS &&
           integer(next, 1, INT_MAX - 1);
  if (ok) {
    tmp.next_id = next->valueint;
    if (cJSON_IsNumber(offset) && offset->valuedouble >= 0 &&
        offset->valuedouble < 9007199254740991.0)
      tmp.offset = (long long)offset->valuedouble;
    else
      ok = 0;
  }
  cJSON *v;
  int max_id = 0;
  if (ok)
    cJSON_ArrayForEach(v, a) {
      Task *t = &tmp.tasks[tmp.count];
      cJSON *id = cJSON_GetObjectItemCaseSensitive(v, "id"),
            *kind = cJSON_GetObjectItemCaseSensitive(v, "kind"),
            *title = cJSON_GetObjectItemCaseSensitive(v, "title"),
            *p = cJSON_GetObjectItemCaseSensitive(v, "period"),
            *d = cJSON_GetObjectItemCaseSensitive(v, "completed_on"),
            *done = cJSON_GetObjectItemCaseSensitive(v, "done");
      if (!integer(id, 1, INT_MAX - 2) || !integer(kind, 0, 2) ||
          !cJSON_IsString(title) || !title->valuestring[0] ||
          strlen(title->valuestring) > TITLE_MAX || !cJSON_IsString(p) ||
          strlen(p->valuestring) > 10 || !cJSON_IsString(d) ||
          strlen(d->valuestring) > 10 || !cJSON_IsBool(done) ||
          task_find(&tmp, id->valueint)) {
        ok = 0;
        break;
      }
      t->id = id->valueint;
      t->kind = kind->valueint;
      t->done = cJSON_IsTrue(done);
      copy(t->title, title->valuestring, sizeof(t->title));
      copy(t->period, p->valuestring, sizeof(t->period));
      copy(t->completed_on, d->valuestring, sizeof(t->completed_on));
      cJSON *days = cJSON_GetObjectItemCaseSensitive(v, "days");
      if (cJSON_IsArray(days)) {
        if (cJSON_GetArraySize(days) > HISTORY_DAYS) {
          ok = 0;
          break;
        }
        cJSON *dd;
        cJSON_ArrayForEach(dd, days) {
          if (!cJSON_IsString(dd) || strlen(dd->valuestring) != 10) {
            ok = 0;
            break;
          }
          copy(t->days[t->days_count++], dd->valuestring, 11);
        }
      } else if (t->kind == 1 && t->completed_on[0])
        copy(t->days[t->days_count++], t->completed_on, 11);
      if (!ok)
        break;
      tmp.count++;
      if (t->id > max_id)
        max_id = t->id;
    }
  cJSON *ps = cJSON_GetObjectItemCaseSensitive(j, "progress");
  if (ps) {
    if (!cJSON_IsArray(ps) || cJSON_GetArraySize(ps) > HISTORY_DAYS)
      ok = 0;
    else {
      cJSON *pp;
      cJSON_ArrayForEach(pp, ps) {
        DayProgress *d = &tmp.progress[tmp.progress_count];
        cJSON *dd = cJSON_GetObjectItemCaseSensitive(pp, "day"),
              *total = cJSON_GetObjectItemCaseSensitive(pp, "total"),
              *done = cJSON_GetObjectItemCaseSensitive(pp, "done"),
              *ht = cJSON_GetObjectItemCaseSensitive(pp, "habit_total"),
              *hd = cJSON_GetObjectItemCaseSensitive(pp, "habit_done");
        if (!cJSON_IsString(dd) || strlen(dd->valuestring) != 10 ||
            !integer(total, 0, MAX_TASKS) ||
            !integer(done, 0, total->valueint) || !integer(ht, 0, MAX_TASKS) ||
            !integer(hd, 0, ht->valueint)) {
          ok = 0;
          break;
        }
        copy(d->day, dd->valuestring, 11);
        d->total = total->valueint;
        d->done = done->valueint;
        d->habit_total = ht->valueint;
        d->habit_done = hd->valueint;
        tmp.progress_count++;
      }
    }
  }
  cJSON *entries = cJSON_GetObjectItemCaseSensitive(j, "journal"),
        *next_j = cJSON_GetObjectItemCaseSensitive(j, "next_journal_id");
  int schema = cJSON_GetObjectItemCaseSensitive(j, "schema")
                   ? cJSON_GetObjectItemCaseSensitive(j, "schema")->valueint
                   : 0;
  if (entries || schema == 3) {
    if (!cJSON_IsArray(entries) || cJSON_GetArraySize(entries) > MAX_JOURNAL ||
        !integer(next_j, 1, INT_MAX - 1))
      ok = 0;
    else {
      tmp.next_journal_id = next_j->valueint;
      int max_j = 0;
      cJSON *entry;
      cJSON_ArrayForEach(entry, entries) {
        cJSON *id = cJSON_GetObjectItemCaseSensitive(entry, "id"),
              *date = cJSON_GetObjectItemCaseSensitive(entry, "day"),
              *body = cJSON_GetObjectItemCaseSensitive(entry, "body");
        if (!integer(id, 1, INT_MAX - 2) || !cJSON_IsString(date) ||
            strlen(date->valuestring) != 10 || !cJSON_IsString(body) ||
            !body->valuestring[0] || strlen(body->valuestring) > JOURNAL_MAX ||
            journal_find(&tmp, id->valueint)) {
          ok = 0;
          break;
        }
        JournalEntry *e = &tmp.journal[tmp.journal_count++];
        e->id = id->valueint;
        copy(e->day, date->valuestring, sizeof(e->day));
        copy(e->body, body->valuestring, sizeof(e->body));
        if (e->id > max_j)
          max_j = e->id;
      }
      if (tmp.next_journal_id <= max_j)
        ok = 0;
    }
  }
  if (tmp.next_id <= max_id)
    ok = 0;
  cJSON_Delete(j);
  if (!ok) {
    copy(err, "Task data is invalid; original file preserved", n);
    return 0;
  }
  *m = tmp;
  return 1;
}
static void append(char *out, size_t n, const char *s) {
  size_t k = strlen(out);
  if (k < n)
    snprintf(out + k, n - k, "%s", s);
}
static const char *help =
    "Commands:\n/add Buy groceries\n/daily Read for 20 minutes\n/goal Finish "
    "portfolio\n/list\n/habits\n/progress\n/done 3\n/undo 3\n/edit 3 New "
    "title\n/delete 3\nDaily routines reset each day. Goals belong to their "
    "creation month.\nJournal: /journal Your thoughts\n/journal (list)\n/jread "
    "1\n/jedit 1 New thoughts\n/jdelete 1";
static int command_inner(Model *m, const char *input, const char *day,
                         char *reply, size_t n) {
  char b[JOURNAL_MAX + 64];
  reply[0] = 0;
  if (!input || strlen(input) >= sizeof(b)) {
    copy(reply, "Message is too long", n);
    return 0;
  }
  copy(b, input, sizeof(b));
  char *s = b;
  while (isspace((unsigned char)*s))
    s++;
  size_t len = strlen(s);
  while (len && isspace((unsigned char)s[len - 1]))
    s[--len] = 0;
  if (!*s) {
    copy(reply, "Please enter a task", n);
    return 0;
  }
  char cmd[32] = "/add", *arg = s;
  if (*s == '/') {
    arg = strpbrk(s, " \t\r\n");
    if (arg)
      *arg++ = 0;
    else
      arg = s + strlen(s);
    char *at = strchr(s, '@');
    if (at)
      *at = 0;
    copy(cmd, s, sizeof(cmd));
    while (isspace((unsigned char)*arg))
      arg++;
  }
  if (!strcmp(cmd, "/start") || !strcmp(cmd, "/help")) {
    copy(reply, help, n);
    return 0;
  }
  if (!strcmp(cmd, "/journal")) {
    if (!*arg) {
      copy(reply, "Journal entries:\n", n);
      if (!m->journal_count)
        append(reply, n, "No entries yet. Use /journal Your thoughts");
      for (int i = m->journal_count - 1; i >= 0 && i >= m->journal_count - 8;
           i--) {
        JournalEntry *e = &m->journal[i];
        char preview[70], line[120];
        copy(preview, e->body, sizeof(preview));
        for (char *p = preview; *p; p++)
          if (*p == '\n' || *p == '\r')
            *p = ' ';
        snprintf(line, sizeof(line), "J%d %s: %s\n", e->id, e->day, preview);
        append(reply, n, line);
      }
      append(reply, n, "\nRead an entry with /jread NUMBER");
      return 0;
    }
    if (strlen(arg) > JOURNAL_MAX) {
      copy(reply, "Entry is too long; use up to 2048 bytes", n);
      return 0;
    }
    if (m->journal_count >= MAX_JOURNAL || m->next_journal_id >= INT_MAX - 1) {
      copy(reply, "Journal is full; delete an old entry first", n);
      return 0;
    }
    JournalEntry *e = &m->journal[m->journal_count++];
    e->id = m->next_journal_id++;
    copy(e->day, day, sizeof(e->day));
    copy(e->body, arg, sizeof(e->body));
    snprintf(reply, n, "Saved journal J%d for %s", e->id, e->day);
    return 1;
  }
  if (!strcmp(cmd, "/jread") || !strcmp(cmd, "/jedit") ||
      !strcmp(cmd, "/jdelete")) {
    char *end;
    errno = 0;
    long id = strtol(arg, &end, 10);
    while (isspace((unsigned char)*end))
      end++;
    if (errno || id <= 0 || id > INT_MAX - 2 || end == arg ||
        (strcmp(cmd, "/jedit") && *end)) {
      copy(reply, "Use a journal number from /journal", n);
      return 0;
    }
    JournalEntry *e = journal_find(m, (int)id);
    if (!e) {
      copy(reply, "Journal entry not found", n);
      return 0;
    }
    if (!strcmp(cmd, "/jread")) {
      snprintf(reply, n, "J%d | %s\n\n%s", e->id, e->day, e->body);
      return 0;
    }
    if (!strcmp(cmd, "/jedit")) {
      if (!*end || strlen(end) > JOURNAL_MAX) {
        copy(reply, "Use /jedit NUMBER New entry (up to 2048 bytes)", n);
        return 0;
      }
      copy(e->body, end, sizeof(e->body));
      snprintf(reply, n, "Updated journal J%d", e->id);
      return 1;
    }
    int index = (int)(e - m->journal);
    memmove(e, e + 1, (size_t)(m->journal_count - index - 1) * sizeof(*e));
    m->journal_count--;
    snprintf(reply, n, "Deleted journal J%ld", id);
    return 1;
  }
  if (!strcmp(cmd, "/progress")) {
    const DayProgress *p = model_progress(m, day);
    snprintf(
        reply, n,
        "Today: %d/%d tasks complete. Habits: %d/%d. View the Progress tab on "
        "your Kindle for the 14-day graph and seven-day habit grid.",
        p ? p->done : 0, p ? p->total : 0, p ? p->habit_done : 0,
        p ? p->habit_total : 0);
    return 0;
  }
  if (!strcmp(cmd, "/list") || !strcmp(cmd, "/habits")) {
    copy(reply, "Your tasks:\n", n);
    for (int i = 0; i < m->count; i++) {
      Task *t = &m->tasks[i];
      if (!strcmp(cmd, "/habits") && t->kind != 1)
        continue;
      if (t->kind == 2 && strncmp(t->period, day, 7))
        continue;
      char line[220];
      snprintf(line, sizeof(line), "%d [%c] %s%s\n", t->id,
               task_done(t, day) ? 'x' : ' ',
               t->kind == 1   ? "Daily: "
               : t->kind == 2 ? "Goal: "
                              : "",
               t->title);
      if (strlen(reply) + strlen(line) + 45 >= n) {
        append(reply, n, "More tasks available on the Kindle.\n");
        break;
      }
      append(reply, n, line);
    }
    return 0;
  }
  if (!strcmp(cmd, "/add") || !strcmp(cmd, "/daily") || !strcmp(cmd, "/goal")) {
    if (!*arg || strlen(arg) > TITLE_MAX) {
      copy(reply, "Use a title between 1 and 160 bytes", n);
      return 0;
    }
    if (m->count >= MAX_TASKS || m->next_id >= INT_MAX - 1) {
      copy(reply, "Task limit reached; delete an old task first", n);
      return 0;
    }
    Task *t = &m->tasks[m->count++];
    memset(t, 0, sizeof(*t));
    t->id = m->next_id++;
    t->kind = !strcmp(cmd, "/daily") ? 1 : !strcmp(cmd, "/goal") ? 2 : 0;
    copy(t->title, arg, sizeof(t->title));
    copy(t->period, day, sizeof(t->period));
    snprintf(reply, n, "Added #%d: %s", t->id, t->title);
    return 1;
  }
  if (!strcmp(cmd, "/done") || !strcmp(cmd, "/undo") ||
      !strcmp(cmd, "/delete") || !strcmp(cmd, "/edit")) {
    char *end;
    errno = 0;
    long id = strtol(arg, &end, 10);
    while (isspace((unsigned char)*end))
      end++;
    if (errno || id <= 0 || id > INT_MAX - 2 || end == arg ||
        ((strcmp(cmd, "/edit") != 0) && *end)) {
      copy(reply, "Use a valid task number from /list", n);
      return 0;
    }
    Task *t = task_find(m, (int)id);
    if (!t) {
      copy(reply, "Task not found; use /list", n);
      return 0;
    }
    if (!strcmp(cmd, "/edit")) {
      if (!*end || strlen(end) > TITLE_MAX) {
        copy(reply, "Use /edit NUMBER New title (up to 160 bytes)", n);
        return 0;
      }
      copy(t->title, end, sizeof(t->title));
      snprintf(reply, n, "Updated #%d", t->id);
      return 1;
    }
    if (!strcmp(cmd, "/delete")) {
      int index = (int)(t - m->tasks);
      memmove(t, t + 1, (size_t)(m->count - index - 1) * sizeof(*t));
      m->count--;
      snprintf(reply, n, "Deleted #%ld", id);
      return 1;
    }
    int done = !strcmp(cmd, "/done");
    if (t->kind == 1) {
      int found = -1;
      for (int k = 0; k < t->days_count; k++)
        if (!strcmp(t->days[k], day))
          found = k;
      if (done && found < 0) {
        if (t->days_count == HISTORY_DAYS) {
          memmove(t->days, t->days + 1, (HISTORY_DAYS - 1) * sizeof(*t->days));
          t->days_count--;
        }
        copy(t->days[t->days_count++], day, 11);
      } else if (!done && found >= 0) {
        memmove(t->days + found, t->days + found + 1,
                (size_t)(t->days_count - found - 1) * sizeof(*t->days));
        t->days_count--;
      }
      copy(t->completed_on, done ? day : "", sizeof(t->completed_on));
    } else {
      t->done = done;
      copy(t->completed_on, done ? day : "", sizeof(t->completed_on));
    }
    snprintf(reply, n, "%s #%ld", done ? "Completed" : "Reopened", id);
    return 1;
  }
  copy(reply, help, n);
  return 0;
}

int model_command(Model *m, const char *command, const char *day, char *reply,
                  size_t n) {
  int changed = command_inner(m, command, day, reply, n);
  if (changed)
    model_record_progress(m, day);
  return changed;
}
