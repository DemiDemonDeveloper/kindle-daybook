#define _POSIX_C_SOURCE 200809L
#include "../vendor/font8x8_basic.h"
#include "model.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/input.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#endif
#define W 600
#define H 800
#define ROWS 5
static Model model;
static unsigned char pixels[W * H];
static char data_dir[700] = ".", state_path[1024], day[11], month_name[40],
            status_text[180] = "", draft[JOURNAL_MAX + 1] = "";
static char token[160] = "", chat_id[40] = "",
            tz[128] = "CST6CDT,M3.2.0,M11.1.0";
static int tab, page, screen, edit_id, kind, selected_id,
    dirty = 1, poll_seconds = 30, telegram_ready, shift, frame_count;
static int visible[MAX_TASKS], visible_count;
static int journal_mode, journal_page, journal_selected, journal_body_page;
static volatile sig_atomic_t running = 1;
static pid_t network_pid;
static int network_type, network_last_ok;
static time_t next_poll;
static char responses[10][REPLY_MAX];
static int response_count, response_index;
static void die_signal(int sig) {
  (void)sig;
  running = 0;
}
static void path(char *out, size_t n, const char *name) {
  snprintf(out, n, "%s/%s", data_dir, name);
}
static int child_status(pid_t p) {
  int s;
  while (waitpid(p, &s, 0) < 0) {
    if (errno != EINTR)
      return -1;
  }
  return WIFEXITED(s) ? WEXITSTATUS(s) : -1;
}
static char *read_file(const char *name, size_t limit) {
  FILE *f = fopen(name, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return NULL;
  }
  long n = ftell(f);
  if (n < 0 || (unsigned long)n > limit) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  char *s = calloc((size_t)n + 1, 1);
  if (s && fread(s, 1, (size_t)n, f) != (size_t)n) {
    free(s);
    s = NULL;
  }
  fclose(f);
  return s;
}
static void date_now(void) {
  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);
  strftime(day, sizeof(day), "%Y-%m-%d", &t);
  strftime(month_name, sizeof(month_name), "%B %Y", &t);
}
static void config_load(void) {
  char p[1024];
  path(p, sizeof(p), "config.json");
  char *s = read_file(p, 16000);
  if (!s)
    return;
  cJSON *j = cJSON_Parse(s);
  free(s);
  cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "telegram_token");
  if (cJSON_IsString(v) && strlen(v->valuestring) < sizeof(token))
    snprintf(token, sizeof(token), "%s", v->valuestring);
  v = cJSON_GetObjectItemCaseSensitive(j, "telegram_chat_id");
  if (cJSON_IsString(v) && strlen(v->valuestring) < sizeof(chat_id))
    snprintf(chat_id, sizeof(chat_id), "%s", v->valuestring);
  v = cJSON_GetObjectItemCaseSensitive(j, "poll_seconds");
  if (cJSON_IsNumber(v) && v->valueint >= 15 && v->valueint <= 3600)
    poll_seconds = v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "timezone");
  if (cJSON_IsString(v) && strlen(v->valuestring) < sizeof(tz))
    snprintf(tz, sizeof(tz), "%s", v->valuestring);
  cJSON_Delete(j);
  int tok_ok = *token != 0, id_ok = *chat_id != 0;
  for (char *c = token; *c; c++)
    if (!isalnum((unsigned char)*c) && *c != ':' && *c != '_' && *c != '-')
      tok_ok = 0;
  for (char *c = chat_id; *c; c++)
    if (!isdigit((unsigned char)*c))
      id_ok = 0;
  telegram_ready = tok_ok && id_ok;
}
static void rect(int x, int y, int w, int h, int shade) {
  for (int yy = y; yy < y + h && yy < H; yy++)
    if (yy >= 0)
      for (int xx = x; xx < x + w && xx < W; xx++)
        if (xx >= 0)
          pixels[yy * W + xx] = (unsigned char)shade;
}
static void text(int x, int y, const char *s, int scale, int shade) {
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '\n') {
      y += scale * 10;
      x = 24;
      continue;
    }
    if (c < 32 || c > 126)
      c = '?';
    for (int yy = 0; yy < 8; yy++)
      for (int xx = 0; xx < 8; xx++)
        if (((unsigned char)font8x8_basic[c][yy] >> xx) & 1)
          rect(x + xx * scale, y + yy * scale, scale, scale, shade);
    x += 8 * scale;
    if (x >= W)
      break;
  }
}
static void centered(int x, int y, int width, const char *s, int scale,
                     int color) {
  text(x + (width - (int)strlen(s) * 8 * scale) / 2, y, s, scale, color);
}
static void button(int x, int y, int w, int h, const char *s, int active) {
  rect(x, y, w, h, 0);
  rect(x + 2, y + 2, w - 4, h - 4, active ? 0 : 255);
  centered(x, y + (h - 16) / 2, w, s, 2, active ? 255 : 0);
}
static void wrapped(int x, int y, const char *s, int chars, int lines,
                    int scale) {
  size_t off = 0, len = strlen(s);
  for (int line = 0; line < lines && off < len; line++) {
    char b[80];
    size_t take = len - off;
    if (take > (size_t)chars)
      take = (size_t)chars;
    if (off + take < len) {
      size_t k = take;
      while (k > 0 && s[off + k] != ' ')
        k--;
      if (k > take / 2)
        take = k;
    }
    if (take >= sizeof(b))
      take = sizeof(b) - 1;
    memcpy(b, s + off, take);
    b[take] = 0;
    if (line == lines - 1 && off + take < len && take >= 3)
      memcpy(b + take - 3, "...", 3);
    text(x, y + line * (scale * 9), b, scale, 0);
    off += take;
    while (s[off] == ' ')
      off++;
  }
}
static void list_indices(void) {
  visible_count = 0;
  for (int i = 0; i < model.count; i++)
    if (task_visible(&model.tasks[i], tab, day))
      visible[visible_count++] = i;
  int last = visible_count ? (visible_count - 1) / ROWS : 0;
  if (page > last)
    page = last;
  if (page < 0)
    page = 0;
}
static void header(const char *title) {
  text(24, 20, title, 3, 0);
  button(328, 14, 130, 42, "Journal",
         screen == 5 || screen == 6 || screen == 7);
  button(470, 14, 106, 42, "Menu", 0);
  text(24, 62, day, 2, 0);
  rect(24, 92, 552, 2, 0);
}
static void date_ago(int ago, char out[11]) {
  time_t now = time(NULL);
  struct tm tm;
  localtime_r(&now, &tm);
  tm.tm_hour = 12;
  tm.tm_mday -= ago;
  tm.tm_isdst = -1;
  mktime(&tm);
  strftime(out, 11, "%Y-%m-%d", &tm);
}
static void draw_progress(void) {
  text(24, 182, "DAILY PROGRESS", 2, 0);
  text(24, 210, "Last 14 days - percent complete", 1, 0);
  text(7, 263, "100", 1, 0);
  text(22, 424, "0", 1, 0);
  rect(40, 432, 520, 2, 0);
  for (int i = 0; i < 14; i++) {
    char d[11], label[4];
    date_ago(13 - i, d);
    const DayProgress *p = model_progress(&model, d);
    int x = 47 + i * 37;
    int height = p && p->total ? 160 * p->done / p->total : 0;
    if (height)
      rect(x, 432 - height, 24, height, 0);
    else
      text(x + 7, 410, p ? "0" : "-", 1, 0);
    snprintf(label, sizeof(label), "%.2s", d + 8);
    text(x + 4, 447, label, 1, 0);
  }
  text(24, 478, "- = no recorded data", 1, 0);
  text(24, 516, "HABITS / LAST 7 DAYS", 2, 0);
  for (int j = 0; j < 7; j++) {
    char d[11];
    date_ago(6 - j, d);
    text(326 + j * 35, 551, d + 8, 1, 0);
  }
  int n = 0;
  for (int i = 0; i < model.count; i++)
    if (model.tasks[i].kind == 1) {
      if (n >= page * 4 && n < page * 4 + 4) {
        int y = 579 + (n - page * 4) * 27;
        wrapped(24, y, model.tasks[i].title, 17, 1, 2);
        for (int j = 0; j < 7; j++) {
          char d[11];
          date_ago(6 - j, d);
          int x = 323 + j * 35;
          rect(x, y - 2, 22, 22, 0);
          if (!task_done(&model.tasks[i], d))
            rect(x + 2, y, 18, 18, 255);
        }
      }
      n++;
    }
  if (!n)
    text(24, 590, "Add a daily routine to begin.", 2, 0);
  int pages = n ? (n - 1) / 4 + 1 : 1;
  if (page >= pages)
    page = pages - 1;
  if (page < 0)
    page = 0;
  button(24, 706, 240, 62, "Today", 0);
  button(286, 706, 64, 62, "<", 0);
  char label[20];
  snprintf(label, sizeof(label), "%d/%d", page + 1, pages);
  centered(350, 730, 150, label, 2, 0);
  button(512, 706, 64, 62, ">", 0);
}
static void draw_list(void) {
  header("DAYBOOK");
  button(24, 110, 132, 52, "Today", tab == 0);
  button(164, 110, 132, 52, "Daily", tab == 1);
  button(304, 110, 132, 52, "Goals", tab == 2);
  button(444, 110, 132, 52, "Progress", tab == 3);
  if (tab == 3) {
    draw_progress();
    return;
  }
  list_indices();
  int done = 0;
  for (int i = 0; i < visible_count; i++)
    done += task_done(&model.tasks[visible[i]], day);
  char b[100];
  snprintf(b, sizeof(b), "%d / %d complete", done, visible_count);
  text(24, 182, b, 2, 0);
  if (tab == 2)
    text(24, 208, month_name, 2, 0);
  if (!visible_count) {
    text(24, 290, tab == 2 ? "A little progress," : "A clear page.", 3, 0);
    text(24, 326, tab == 2 ? "every day." : "Make room for a task.", 2, 0);
    text(24, 392, "Tap + Add to begin.", 2, 0);
  }
  for (int row = 0; row < ROWS; row++) {
    int k = page * ROWS + row;
    if (k >= visible_count)
      break;
    Task *t = &model.tasks[visible[k]];
    int y = 242 + row * 84;
    int completed = task_done(t, day);
    button(24, y + 9, 40, 40, completed ? "X" : "", completed);
    wrapped(80, y + 5, t->title, 23, 2, 2);
    snprintf(b, sizeof(b), "#%d%s", t->id, t->kind == 1 ? "  every day" : "");
    text(80, y + 50, b, 1, 0);
    button(516, y + 9, 60, 44, "...", 0);
    rect(24, y + 78, 552, 1, 0);
  }
  if (*status_text)
    wrapped(24, 668, status_text, 34, 1, 2);
  else
    text(24, 668,
         telegram_ready ? (network_last_ok ? "Telegram connected"
                                           : "Telegram: waiting for sync")
                        : "Telegram: setup needed",
         2, 0);
  button(24, 706, 240, 62, "+ Add", 1);
  button(286, 706, 64, 62, "<", 0);
  snprintf(b, sizeof(b), "%d/%d", page + 1,
           visible_count ? (visible_count - 1) / ROWS + 1 : 1);
  centered(350, 730, 150, b, 2, 0);
  button(512, 706, 64, 62, ">", 0);
}
static size_t journal_line(const char *body, size_t off, char line[36]) {
  size_t len = strlen(body), end = off;
  int count = 0;
  while (end < len && body[end] != '\n' && count < 34) {
    end++;
    count++;
  }
  size_t next = end;
  if (end < len && body[end] != '\n') {
    size_t space = end;
    while (space > off && body[space] != ' ')
      space--;
    if (space > off + 15) {
      end = space;
      next = space;
    }
  }
  size_t take = end - off;
  if (take > 34)
    take = 34;
  memcpy(line, body + off, take);
  line[take] = 0;
  if (body[next] == '\n')
    next++;
  else
    while (body[next] == ' ')
      next++;
  if (next == off && off < len)
    next++;
  return next;
}
static int journal_pages(const char *body) {
  size_t off = 0;
  int lines = 0;
  while (body[off]) {
    char line[36];
    off = journal_line(body, off, line);
    lines++;
  }
  return lines ? (lines - 1) / 22 + 1 : 1;
}
static void draw_journal_list(void) {
  header("JOURNAL");
  text(24, 116, "A place for your thoughts.", 2, 0);
  int pages = model.journal_count ? (model.journal_count - 1) / 4 + 1 : 1;
  if (journal_page >= pages)
    journal_page = pages - 1;
  if (journal_page < 0)
    journal_page = 0;
  if (!model.journal_count) {
    text(24, 286, "Start with a few words.", 2, 0);
    text(24, 332, "Tap + Write below.", 2, 0);
  }
  for (int row = 0; row < 4; row++) {
    int index = model.journal_count - 1 - journal_page * 4 - row;
    if (index < 0)
      break;
    JournalEntry *e = &model.journal[index];
    int y = 166 + row * 116;
    char label[60], preview[100];
    snprintf(label, sizeof(label), "%s  J%d", e->day, e->id);
    text(24, y, label, 2, 0);
    snprintf(preview, sizeof(preview), "%.90s", e->body);
    for (char *c = preview; *c; c++)
      if (*c == '\n' || *c == '\r')
        *c = ' ';
    wrapped(24, y + 30, preview, 29, 2, 2);
    text(548, y + 30, ">", 2, 0);
    rect(24, y + 100, 552, 1, 0);
  }
  button(24, 706, 240, 62, "+ Write", 1);
  button(286, 706, 64, 62, "<", 0);
  char label[30];
  snprintf(label, sizeof(label), "%d/%d", journal_page + 1, pages);
  centered(350, 730, 150, label, 2, 0);
  button(512, 706, 64, 62, ">", 0);
}
static void draw_journal_entry(void) {
  JournalEntry *e = journal_find(&model, journal_selected);
  if (!e) {
    screen = 5;
    draw_journal_list();
    return;
  }
  header("JOURNAL");
  char label[70];
  snprintf(label, sizeof(label), "%s | Entry J%d", e->day, e->id);
  text(24, 110, label, 2, 0);
  int pages = journal_pages(e->body);
  if (journal_body_page >= pages)
    journal_body_page = pages - 1;
  if (journal_body_page < 0)
    journal_body_page = 0;
  size_t off = 0;
  int line_number = 0, drawn = 0;
  while (e->body[off]) {
    char line[36];
    off = journal_line(e->body, off, line);
    if (line_number++ >= journal_body_page * 22) {
      text(24, 156 + drawn * 18, line, 2, 0);
      if (++drawn >= 22)
        break;
    }
  }
  button(24, 610, 176, 56, "Edit", 0);
  button(212, 610, 176, 56, screen == 7 ? "Confirm" : "Delete?", screen == 7);
  button(400, 610, 176, 56, "Back", 0);
  if (screen == 7)
    text(24, 680, "Confirm deletes this entry.", 2, 0);
  button(24, 716, 100, 56, "<", 0);
  snprintf(label, sizeof(label), "%d/%d", journal_body_page + 1, pages);
  centered(124, 735, 352, label, 2, 0);
  button(476, 716, 100, 56, ">", 0);
}
static void journal_edit(int id) {
  journal_mode = 1;
  edit_id = id;
  JournalEntry *e = journal_find(&model, id);
  snprintf(draft, sizeof(draft), "%s", e ? e->body : "");
  screen = 1;
  status_text[0] = 0;
  dirty = 1;
}
static const char *keys[] = {"1234567890", "qwertyuiop", "asdfghjkl",
                             "zxcvbnm"};
static void draw_editor(void) {
  header(journal_mode ? (edit_id ? "EDIT JOURNAL" : "NEW JOURNAL")
                      : (edit_id ? "EDIT TASK" : "NEW TASK"));
  if (journal_mode) {
    const char *tail = draft;
    size_t len = strlen(draft);
    if (len > 136) {
      tail = draft + len - 136;
      while ((*tail & 0xc0) == 0x80)
        tail++;
    }
    wrapped(24, 116, *draft ? tail : "Write whatever is on your mind.", 34, 4,
            2);
    char count[70];
    snprintf(count, sizeof(count), "%zu/%d bytes | Enter adds a line", len,
             JOURNAL_MAX);
    text(24, 248, count, 1, 0);
  } else {
    wrapped(24, 116, *draft ? draft : "Type a task below", 23, 4, 2);
    button(24, 246, 176, 52, "To-do", kind == 0);
    button(212, 246, 176, 52, "Daily", kind == 1);
    button(400, 246, 176, 52, "Goal", kind == 2);
  }
  for (int row = 0; row < 4; row++)
    for (int col = 0; keys[row][col]; col++) {
      char c[2] = {keys[row][col], 0};
      if (shift)
        c[0] = (char)toupper((unsigned char)c[0]);
      button(col * 60 + 3, 334 + row * 62, 54, 56, c, 0);
    }
  button(423, 520, 174, 56, "Delete", 0);
  button(3, 590, 114, 56, shift ? "abc" : "ABC", 0);
  button(123, 590, 354, 56, "Space", 0);
  button(483, 590, 114, 56, journal_mode ? "Enter" : "-", 0);
  button(24, 704, 264, 64, "Cancel", 0);
  button(312, 704, 264, 64, "Save", 1);
  if (*status_text)
    wrapped(24, 664, status_text, 34, 1, 2);
}
static void draw_detail(void) {
  Task *t = task_find(&model, selected_id);
  if (!t) {
    screen = 0;
    draw_list();
    return;
  }
  header("TASK");
  wrapped(24, 128, t->title, 23, 5, 2);
  char b[80];
  snprintf(b, sizeof(b), "Task #%d  %s", t->id,
           t->kind == 1   ? "Daily routine"
           : t->kind == 2 ? "Monthly goal"
                          : "To-do");
  text(24, 268, b, 2, 0);
  button(24, 330, 552, 64,
         task_done(t, day) ? "Mark incomplete" : "Mark complete", 1);
  button(24, 416, 552, 64, "Edit title", 0);
  button(24, 502, 552, 64, screen == 4 ? "CONFIRM DELETE" : "Delete task?",
         screen == 4);
  text(24, 596,
       screen == 4 ? "Tap confirm, or elsewhere to cancel."
                   : "Delete is permanent.",
       2, 0);
  button(24, 704, 552, 64, "Back", 0);
}
static void draw_menu(void) {
  header("SETTINGS");
  text(24, 128, "Daybook 0.2.1 alpha", 2, 0);
  text(24, 170, "Dedicated task dashboard", 2, 0);
  text(24, 224,
       telegram_ready ? "Telegram configured" : "Telegram not configured", 2,
       0);
  wrapped(24, 262,
          "Create a bot with BotFather, then run the setup helper on your Mac.",
          33, 4, 2);
  text(24, 380, "Daily tasks reset at midnight.", 2, 0);
  wrapped(24, 420,
          "Exit before connecting USB. Power management resumes when you exit.",
          33, 3, 2);
  button(24, 550, 552, 64, "Sync now", 0);
  button(24, 630, 552, 64, "Exit dashboard", 0);
  button(24, 710, 552, 64, "Back", 0);
}
static int write_frame(const char *file) {
  FILE *f = fopen(file, "wb");
  if (!f)
    return 0;
  int ok = fprintf(f, "P5\n%d %d\n255\n", W, H) > 0 &&
           fwrite(pixels, 1, sizeof(pixels), f) == sizeof(pixels);
  if (fclose(f))
    ok = 0;
  return ok;
}
static void render(const char *snapshot) {
  if (!snapshot) {
    char err[180] = "Save failed";
    if (!model_save(&model, state_path, err, sizeof(err)))
      snprintf(status_text, sizeof(status_text), "%s", err);
  }

  memset(pixels, 255, sizeof(pixels));
  if (screen == 1)
    draw_editor();
  else if (screen == 2 || screen == 4)
    draw_detail();
  else if (screen == 3)
    draw_menu();
  else if (screen == 5)
    draw_journal_list();
  else if (screen == 6 || screen == 7)
    draw_journal_entry();
  else
    draw_list();
  char frame[1024];
  if (snapshot)
    snprintf(frame, sizeof(frame), "%s", snapshot);
  else
    path(frame, sizeof(frame), "screen.pgm");
  if (!write_frame(frame)) {
    snprintf(status_text, sizeof(status_text), "Could not write screen");
    return;
  }
  if (!snapshot) {
    pid_t p = fork();
    if (p == 0) {
      if (frame_count % 10 == 0)
        execl("/var/local/kmc/bin/fbink", "fbink", "-q", "-f", "-i", frame,
              (char *)NULL);
      else
        execl("/var/local/kmc/bin/fbink", "fbink", "-q", "-i", frame,
              (char *)NULL);
      _exit(127);
    }
    if (p > 0 && child_status(p) != 0) {
      fprintf(stderr, "Display refresh failed\n");
      running = 0;
    }
    frame_count++;
  }
  dirty = 0;
}
static void history(const char *command) {
  char p[1024];
  path(p, sizeof(p), "history.log");
  FILE *f = fopen(p, "a");
  if (!f)
    return;
  fprintf(f, "%s\t", day);
  const char *record =
      !strncmp(command, "/journal", 8) || !strncmp(command, "/jedit", 6)
          ? "Journal entry saved or edited"
          : command;
  for (const unsigned char *c = (const unsigned char *)record; *c; c++)
    fputc(*c == '\n' || *c == '\r' || *c == '\t' ? ' ' : *c, f);
  fputc('\n', f);
  fclose(f);
}
static int apply(const char *command) {
  Model before = model;
  char reply[REPLY_MAX], err[180] = "Save failed";
  int changed = model_command(&model, command, day, reply, sizeof(reply));
  if (changed && !model_save(&model, state_path, err, sizeof(err))) {
    model = before;
    snprintf(status_text, sizeof(status_text), "%s", err);
    return 0;
  }
  if (changed)
    history(command);
  snprintf(status_text, sizeof(status_text), "%.170s", reply);
  dirty = 1;
  return changed;
}
static void edit_open(int id) {
  journal_mode = 0;
  edit_id = id;
  Task *t = task_find(&model, id);
  draft[0] = 0;
  kind = tab == 1 ? 1 : tab == 2 ? 2 : 0;
  if (t) {
    snprintf(draft, sizeof(draft), "%s", t->title);
    kind = t->kind;
  }
  screen = 1;
  status_text[0] = 0;
  dirty = 1;
}
static void touch(int x, int y) {
  char cmd[JOURNAL_MAX + 64];
  dirty = 1;
  status_text[0] = 0;
  if (x >= 328 && x < 458 && y < 58) {
    screen = 5;
    journal_page = 0;
    return;
  }
  if (x >= 470 && y < 90) {
    screen = 3;
    return;
  }
  if (screen == 5) {
    if (y >= 166 && y < 630) {
      int index = model.journal_count - 1 - journal_page * 4 - (y - 166) / 116;
      if (index >= 0 && index < model.journal_count) {
        journal_selected = model.journal[index].id;
        journal_body_page = 0;
        screen = 6;
      }
    } else if (y >= 706) {
      if (x < 270)
        journal_edit(0);
      else if (x < 350 && journal_page > 0)
        journal_page--;
      else if (x >= 512 && (journal_page + 1) * 4 < model.journal_count)
        journal_page++;
    }
    return;
  }
  if (screen == 6 || screen == 7) {
    JournalEntry *e = journal_find(&model, journal_selected);
    if (!e) {
      screen = 5;
      return;
    }
    if (y >= 610 && y < 666) {
      if (x < 206)
        journal_edit(e->id);
      else if (x < 394) {
        if (screen == 7) {
          snprintf(cmd, sizeof(cmd), "/jdelete %d", e->id);
          if (apply(cmd))
            screen = 5;
        } else
          screen = 7;
      } else
        screen = 5;
    } else if (screen == 7)
      screen = 6;
    else if (y >= 716) {
      if (x < 124 && journal_body_page > 0)
        journal_body_page--;
      else if (x >= 476 && journal_body_page + 1 < journal_pages(e->body))
        journal_body_page++;
    }
    return;
  }
  if (screen == 0) {
    if (y >= 110 && y < 163) {
      tab = x < 160 ? 0 : x < 300 ? 1 : x < 440 ? 2 : 3;
      page = 0;
      return;
    }
    if (tab == 3) {
      if (y >= 706) {
        if (x < 270) {
          tab = 0;
          page = 0;
        } else if (x < 350 && page > 0)
          page--;
        else if (x >= 512) {
          int habits = 0;
          for (int i = 0; i < model.count; i++)
            habits += model.tasks[i].kind == 1;
          if ((page + 1) * 4 < habits)
            page++;
        }
      }
      return;
    }
    if (y >= 242 && y < 662) {
      int k = page * ROWS + (y - 242) / 84;
      if (k < visible_count) {
        Task *t = &model.tasks[visible[k]];
        if (x >= 500) {
          selected_id = t->id;
          screen = 2;
        } else {
          snprintf(cmd, sizeof(cmd), "/%s %d",
                   task_done(t, day) ? "undo" : "done", t->id);
          apply(cmd);
        }
      }
      return;
    }
    if (y >= 706) {
      if (x < 270)
        edit_open(0);
      else if (x < 350)
        page--;
      else if (x >= 512)
        page++;
    }
    return;
  }
  if (screen == 1) {
    if (y >= 246 && y < 298 && !edit_id && !journal_mode) {
      kind = x < 206 ? 0 : x < 394 ? 1 : 2;
      return;
    }
    if (y >= 334 && y < 582) {
      int row = (y - 334) / 62, col = x / 60;
      size_t len = strlen(draft);
      if (row == 3 && x >= 420) {
        if (len) {
          size_t start = len - 1;
          while (start > 0 && ((unsigned char)draft[start] & 0xc0) == 0x80)
            start--;
          draft[start] = 0;
        }
      } else if (row < 4 && col < (int)strlen(keys[row]) &&
                 len < (size_t)(journal_mode ? JOURNAL_MAX : TITLE_MAX)) {
        draft[len] = shift ? (char)toupper((unsigned char)keys[row][col])
                           : keys[row][col];
        draft[len + 1] = 0;
      }
      return;
    }
    if (y >= 590 && y < 646) {
      size_t len = strlen(draft);
      if (x < 120)
        shift = !shift;
      else if (len < (size_t)(journal_mode ? JOURNAL_MAX : TITLE_MAX)) {
        draft[len] = x < 480 ? ' ' : journal_mode ? '\n' : '-';
        draft[len + 1] = 0;
      }
      return;
    }
    if (y >= 704) {
      if (x < 300)
        screen = journal_mode ? (edit_id ? 6 : 5) : 0;
      else {
        if (journal_mode) {
          if (edit_id)
            snprintf(cmd, sizeof(cmd), "/jedit %d %s", edit_id, draft);
          else
            snprintf(cmd, sizeof(cmd), "/journal %s", draft);
        } else if (edit_id)
          snprintf(cmd, sizeof(cmd), "/edit %d %s", edit_id, draft);
        else
          snprintf(cmd, sizeof(cmd), "/%s %s",
                   kind == 1   ? "daily"
                   : kind == 2 ? "goal"
                               : "add",
                   draft);
        if (apply(cmd))
          screen = journal_mode ? 5 : 0;
      }
    }
    return;
  }
  if (screen == 2) {
    Task *t = task_find(&model, selected_id);
    if (!t) {
      screen = 0;
      return;
    }
    if (y >= 330 && y < 394) {
      snprintf(cmd, sizeof(cmd), "/%s %d", task_done(t, day) ? "undo" : "done",
               t->id);
      apply(cmd);
    } else if (y >= 416 && y < 480)
      edit_open(t->id);
    else if (y >= 502 && y < 566) {
      screen = 4;
    } else if (y >= 704)
      screen = 0;
    return;
  }
  if (screen == 4) {
    if (y >= 502 && y < 566) {
      snprintf(cmd, sizeof(cmd), "/delete %d", selected_id);
      apply(cmd);
      screen = 0;
    } else
      screen = 2;
    return;
  }
  if (screen == 3) {
    if (y >= 550 && y < 614) {
      next_poll = 0;
      snprintf(status_text, sizeof(status_text), "%s",
               telegram_ready ? "Sync requested"
                              : "Run Telegram setup on your Mac");
      screen = 0;
    } else if (y >= 630 && y < 694)
      running = 0;
    else if (y >= 710)
      screen = 0;
  }
}
static int json_write(const char *file, cJSON *j) {
  char *s = cJSON_PrintUnformatted(j);
  if (!s)
    return 0;
  int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  size_t n = strlen(s), off = 0;
  while (fd >= 0 && off < n) {
    ssize_t w = write(fd, s + off, n - off);
    if (w < 0 && errno == EINTR)
      continue;
    if (w <= 0)
      break;
    off += (size_t)w;
  }
  int ok = fd >= 0 && off == n;
  if (fd >= 0)
    close(fd);
  free(s);
  return ok;
}
static void network_start(int type) {
  char req[1024], res[1024];
  path(req, sizeof(req), "telegram-request.json");
  path(res, sizeof(res), "telegram-response.json");
  cJSON *j = cJSON_CreateObject();
  if (type == 1) {
    cJSON_AddNumberToObject(j, "offset", (double)model.offset);
    cJSON_AddNumberToObject(j, "limit", 10);
    cJSON_AddNumberToObject(j, "timeout", 0);
    cJSON *a = cJSON_AddArrayToObject(j, "allowed_updates");
    cJSON_AddItemToArray(a, cJSON_CreateString("message"));
  } else {
    cJSON_AddStringToObject(j, "chat_id", chat_id);
    cJSON_AddStringToObject(j, "text", responses[response_index]);
  }
  int ok = json_write(req, j);
  cJSON_Delete(j);
  if (!ok)
    return;
  int fds[2];
  if (pipe(fds))
    return;
  network_pid = fork();
  if (network_pid == 0) {
    close(fds[1]);
    dup2(fds[0], STDIN_FILENO);
    close(fds[0]);
    char data[1100];
    snprintf(data, sizeof(data), "@%s", req);
    execlp("curl", "curl", "--silent", "--show-error", "--fail",
           "--connect-timeout", "8", "--max-time", "20", "--config", "-",
           "--header", "Content-Type: application/json", "--data-binary", data,
           "--output", res, (char *)NULL);
    _exit(127);
  }
  close(fds[0]);
  if (network_pid < 0) {
    close(fds[1]);
    network_pid = 0;
    return;
  }
  char conf[512];
  int n = snprintf(conf, sizeof(conf),
                   "url = \"https://api.telegram.org/bot%s/%s\"\n", token,
                   type == 1 ? "getUpdates" : "sendMessage");
  size_t sent = 0;
  while (sent < (size_t)n) {
    ssize_t w = write(fds[1], conf + sent, (size_t)n - sent);
    if (w < 0 && errno == EINTR)
      continue;
    if (w <= 0)
      break;
    sent += (size_t)w;
  }
  close(fds[1]);
  network_type = type;
}
static void network_updates(cJSON *result) {
  cJSON *u;
  cJSON_ArrayForEach(u, result) {
    cJSON *id = cJSON_GetObjectItemCaseSensitive(u, "update_id");
    if (!cJSON_IsNumber(id) || id->valuedouble < model.offset ||
        id->valuedouble > 9007199254740990.0)
      continue;
    Model before = model;
    model.offset = (long long)id->valuedouble + 1;
    cJSON *msg = cJSON_GetObjectItemCaseSensitive(u, "message"),
          *chat = cJSON_GetObjectItemCaseSensitive(msg, "chat"),
          *cid = cJSON_GetObjectItemCaseSensitive(chat, "id"),
          *typ = cJSON_GetObjectItemCaseSensitive(chat, "type"),
          *txt = cJSON_GetObjectItemCaseSensitive(msg, "text");
    char actual[40] = "", reply[REPLY_MAX] = "";
    if (cJSON_IsNumber(cid))
      snprintf(actual, sizeof(actual), "%.0f", cid->valuedouble);
    int authorized = cJSON_IsString(typ) &&
                     !strcmp(typ->valuestring, "private") &&
                     !strcmp(actual, chat_id) && cJSON_IsString(txt);
    int changed = authorized ? model_command(&model, txt->valuestring, day,
                                             reply, sizeof(reply))
                             : 0;
    char err[180] = "Save failed";
    if (!model_save(&model, state_path, err, sizeof(err))) {
      model = before;
      snprintf(status_text, sizeof(status_text), "%s", err);
      break;
    }
    if (changed) {
      history(txt->valuestring);
      dirty = 1;
    }
    if (authorized && response_count < 10)
      snprintf(responses[response_count++], REPLY_MAX, "%s", reply);
  }
}
static void network_tick(void) {
  if (!telegram_ready)
    return;
  time_t now = time(NULL);
  if (network_pid) {
    int s;
    pid_t r = waitpid(network_pid, &s, WNOHANG);
    if (r == 0)
      return;
    network_pid = 0;
    if (r < 0 || !WIFEXITED(s) || WEXITSTATUS(s) != 0) {
      fprintf(stderr, "Telegram %s transport failed (exit %d)\n",
              network_type == 1 ? "receive" : "reply",
              r < 0 || !WIFEXITED(s) ? -1 : WEXITSTATUS(s));
      if (network_type == 2)
        response_index++;
      if (network_last_ok) {
        network_last_ok = 0;
        dirty = 1;
      }
      next_poll = now + poll_seconds;
      return;
    }
    char p[1024];
    path(p, sizeof(p), "telegram-response.json");
    char *data = read_file(p, 256000);
    cJSON *j = data ? cJSON_Parse(data) : NULL;
    free(data);
    int ok = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j, "ok"));
    if (network_last_ok != ok) {
      network_last_ok = ok;
      dirty = 1;
    }
    if (network_type == 1) {
      response_count = response_index = 0;
      cJSON *a = cJSON_GetObjectItemCaseSensitive(j, "result");
      if (ok && cJSON_IsArray(a))
        network_updates(a);
      next_poll = now + poll_seconds;
    } else
      response_index++;
    cJSON_Delete(j);
  }
  if (!network_pid) {
    if (response_index < response_count)
      network_start(2);
    else if (now >= next_poll)
      network_start(1);
  }
}
static int sync_once(void) {
  if (!telegram_ready) {
    fprintf(stderr, "Telegram settings missing or invalid\n");
    return 1;
  }
  struct timespec pause = {0, 100000000};
  for (int i = 0; i < 550; i++) {
    network_tick();
    if (next_poll && !network_pid && response_index >= response_count) {
      printf("Telegram sync %s; local tasks: %d; journal entries: %d\n",
             network_last_ok ? "succeeded" : "failed", model.count,
             model.journal_count);
      return network_last_ok ? 0 : 1;
    }
    nanosleep(&pause, NULL);
  }
  if (network_pid) {
    kill(network_pid, SIGTERM);
    child_status(network_pid);
    network_pid = 0;
  }
  fprintf(stderr, "Telegram sync timed out\n");
  return 1;
}
#ifdef __linux__
static int input_open(int *xmin, int *xmax, int *ymin, int *ymax) {
  for (int i = 0; i < 16; i++) {
    char name[80];
    snprintf(name, sizeof(name), "/dev/input/event%d", i);
    int fd = open(name, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
      continue;
    struct input_absinfo x, y;
    int ok = ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &x) == 0 &&
             ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &y) == 0;
    if (!ok)
      ok = ioctl(fd, EVIOCGABS(ABS_X), &x) == 0 &&
           ioctl(fd, EVIOCGABS(ABS_Y), &y) == 0;
    if (ok && x.maximum > x.minimum && y.maximum > y.minimum) {
      *xmin = x.minimum;
      *xmax = x.maximum;
      *ymin = y.minimum;
      *ymax = y.maximum;
      fprintf(stderr, "Touch input: %s range %d..%d / %d..%d\n", name, *xmin,
              *xmax, *ymin, *ymax);
      return fd;
    }
    close(fd);
  }
  return -1;
}
static int hardware_run(void) {
  int xmin, xmax, ymin, ymax;
  int fd = input_open(&xmin, &xmax, &ymin, &ymax);
  if (fd < 0) {
    fprintf(stderr, "No touchscreen found\n");
    return 1;
  }
  if (ioctl(fd, EVIOCGRAB, 1)) {
    perror("Touchscreen grab");
    close(fd);
    return 1;
  }
  int rawx = xmin, rawy = ymin, slot = 0, pressed = 0, release = 0, seenx = 0,
      seeny = 0;
  char prior[11];
  snprintf(prior, sizeof(prior), "%s", day);
  render(NULL);
  while (running) {
    struct pollfd p = {fd, POLLIN, 0};
    int ready = poll(&p, 1, 200);
    if (ready < 0 && errno != EINTR)
      break;
    if (ready > 0 && (p.revents & (POLLERR | POLLHUP | POLLNVAL)))
      break;
    if (ready > 0 && (p.revents & POLLIN)) {
      struct input_event e;
      while (read(fd, &e, sizeof(e)) == (ssize_t)sizeof(e)) {
        if (e.type == EV_ABS && e.code == ABS_MT_SLOT)
          slot = e.value;
        if (e.type == EV_ABS && slot == 0) {
          if (e.code == ABS_X || e.code == ABS_MT_POSITION_X) {
            rawx = e.value;
            seenx = 1;
            pressed = 1;
          }
          if (e.code == ABS_Y || e.code == ABS_MT_POSITION_Y) {
            rawy = e.value;
            seeny = 1;
            pressed = 1;
          }
          if (e.code == ABS_MT_TRACKING_ID) {
            if (e.value < 0)
              release = 1;
            else
              pressed = 1;
          }
          if (e.code == ABS_PRESSURE) {
            if (e.value == 0)
              release = 1;
            else
              pressed = 1;
          }
        }
        if (e.type == EV_KEY && e.code == BTN_TOUCH) {
          if (e.value == 0)
            release = 1;
          else
            pressed = 1;
        }
        if (e.type == EV_SYN && e.code == SYN_DROPPED) {
          pressed = release = seenx = seeny = 0;
        }
        if (e.type == EV_SYN && e.code == SYN_REPORT && release) {
          if (pressed && seenx && seeny) {
            int x = (int)((long long)(rawx - xmin) * (W - 1) / (xmax - xmin)),
                y = (int)((long long)(rawy - ymin) * (H - 1) / (ymax - ymin));
            if (x >= 0 && x < W && y >= 0 && y < H)
              touch(x, y);
          }
          pressed = release = 0;
        }
      }
    }
    date_now();
    if (strcmp(prior, day)) {
      snprintf(prior, sizeof(prior), "%s", day);
      model_record_progress(&model, day);
      model_save(&model, state_path, status_text, sizeof(status_text));
      dirty = 1;
    }
    network_tick();
    if (dirty && running)
      render(NULL);
  }
  if (network_pid) {
    kill(network_pid, SIGTERM);
    child_status(network_pid);
  }
  ioctl(fd, EVIOCGRAB, 0);
  close(fd);
  return 0;
}
#endif
int main(int argc, char **argv) {
  const char *snapshot = NULL, *command = NULL;
  int demo = 0, probe = 0, sync = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--data") && i + 1 < argc)
      snprintf(data_dir, sizeof(data_dir), "%s", argv[++i]);
    else if (!strcmp(argv[i], "--snapshot") && i + 1 < argc)
      snapshot = argv[++i];
    else if (!strcmp(argv[i], "--command") && i + 1 < argc)
      command = argv[++i];
    else if (!strcmp(argv[i], "--view") && i + 1 < argc) {
      const char *v = argv[++i];
      tab = !strcmp(v, "daily")      ? 1
            : !strcmp(v, "goals")    ? 2
            : !strcmp(v, "progress") ? 3
                                     : 0;
    } else if (!strcmp(argv[i], "--journal-entry") && i + 1 < argc) {
      journal_selected = atoi(argv[++i]);
      screen = 6;
    } else if (!strcmp(argv[i], "--journal"))
      screen = 5;
    else if (!strcmp(argv[i], "--demo"))
      demo = 1;
    else if (!strcmp(argv[i], "--sync-once"))
      sync = 1;
    else if (!strcmp(argv[i], "--probe"))
      probe = 1;
    else {
      fprintf(stderr, "Usage: daybook [--data DIR] [--snapshot FILE] [--demo] "
                      "[--command TEXT] [--probe]\n");
      return 2;
    }
  }
  signal(SIGINT, die_signal);
  signal(SIGTERM, die_signal);
  signal(SIGPIPE, SIG_IGN);
  mkdir(data_dir, 0700);
  path(state_path, sizeof(state_path), "state.bin");
  config_load();
  setenv("TZ", tz, 1);
  tzset();
  date_now();
  char err[256] = "Cannot read task data";
  char load_path[1024];
  snprintf(load_path, sizeof(load_path), "%s", state_path);
  if (access(state_path, F_OK) != 0) {
    char legacy[1024];
    path(legacy, sizeof(legacy), "state.json");
    if (access(legacy, F_OK) == 0)
      snprintf(load_path, sizeof(load_path), "%s", legacy);
  }
  if (!model_load(&model, load_path, err, sizeof(err))) {
    fprintf(stderr, "%s\n", err);
    return 1;
  }
#ifdef __linux__
  char lockpath[1024];
  path(lockpath, sizeof(lockpath), "daybook.lock");
  int lock = open(lockpath, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
  if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) {
    fprintf(stderr, "Another Daybook instance is running\n");
    return 1;
  }
#endif
  if (demo) {
    model_init(&model);
    char reply[REPLY_MAX];
    model_command(&model, "/daily Read for 20 minutes", day, reply,
                  sizeof(reply));
    model_command(&model, "/add Plan the next small step", day, reply,
                  sizeof(reply));
    model_command(&model, "/add Take a walk", day, reply, sizeof(reply));
    model_command(&model, "/done 3", day, reply, sizeof(reply));
    snprintf(status_text, sizeof(status_text), "Preview - sample tasks");
  }
  model_record_progress(&model, day);
  if (command) {
    int ok = apply(command);
    puts(status_text);
    return ok ? 0 : 2;
  }
  if (snapshot) {
    render(snapshot);
    return 0;
  }
#ifdef __linux__
  if (probe) {
    int a, b, c, d;
    int fd = input_open(&a, &b, &c, &d);
    if (fd < 0)
      return 1;
    close(fd);
    if (access("/var/local/kmc/bin/fbink", X_OK))
      return 1;
    puts("Daybook: native executable, touchscreen, and FBInk are available.");
    return 0;
  }
  if (!model_save(&model, state_path, err, sizeof(err))) {
    fprintf(stderr, "%s\n", err);
    return 1;
  }
  if (sync)
    return sync_once();
  return hardware_run();
#else
  (void)probe;
  if (sync)
    return sync_once();
  (void)network_tick;
  (void)touch;
  fprintf(stderr, "Use --snapshot for a desktop preview; interactive mode runs "
                  "on Kindle Linux.\n");
  return 2;
#endif
}
