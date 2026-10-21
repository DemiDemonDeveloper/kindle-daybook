#define _POSIX_C_SOURCE 200809L
#include "model.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define STORE_LIMIT (4u * 1024u * 1024u)
typedef struct {
  unsigned char *p;
  size_t n, cap, pos;
  int ok;
} Bytes;
int legacy_model_load(Model *, const char *, char *, size_t);
static void put(Bytes *b, const void *data, size_t n) {
  if (!b->ok || n > STORE_LIMIT - b->n) {
    b->ok = 0;
    return;
  }
  if (b->n + n > b->cap) {
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < b->n + n)
      cap *= 2;
    unsigned char *p = realloc(b->p, cap);
    if (!p) {
      b->ok = 0;
      return;
    }
    b->p = p;
    b->cap = cap;
  }
  memcpy(b->p + b->n, data, n);
  b->n += n;
}
static void number(Bytes *b, uint64_t v, int bytes) {
  for (int i = 0; i < bytes; i++) {
    unsigned char x = (unsigned char)(v >> (8 * i));
    put(b, &x, 1);
  }
}
static uint64_t read_number(Bytes *b, int bytes) {
  if (!b->ok || (size_t)bytes > b->n - b->pos) {
    b->ok = 0;
    return 0;
  }
  uint64_t v = 0;
  for (int i = 0; i < bytes; i++)
    v |= (uint64_t)b->p[b->pos++] << (8 * i);
  return v;
}
static void string(Bytes *b, const char *s, size_t max) {
  size_t n = strnlen(s, max + 1);
  if (n > max) {
    b->ok = 0;
    return;
  }
  number(b, n, 2);
  put(b, s, n);
}
static void read_string(Bytes *b, char *s, size_t max) {
  size_t n = (size_t)read_number(b, 2);
  if (!b->ok || n > max || n > b->n - b->pos || memchr(b->p + b->pos, 0, n)) {
    b->ok = 0;
    return;
  }
  memcpy(s, b->p + b->pos, n);
  s[n] = 0;
  b->pos += n;
}
static uint32_t checksum(const unsigned char *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}
static int decode(Model *m, Bytes *b) {
  Model *v = calloc(1, sizeof(*v));
  if (!v)
    return 0;
  model_init(v);
  v->next_id = (int)read_number(b, 4);
  v->offset = (long long)read_number(b, 8);
  v->count = (int)read_number(b, 2);
  if (v->count > MAX_TASKS || v->next_id < 1 || v->offset < 0) {
    free(v);
    return 0;
  }
  for (int i = 0; i < v->count && b->ok; i++) {
    Task *t = &v->tasks[i];
    t->id = (int)read_number(b, 4);
    t->kind = (int)read_number(b, 1);
    t->done = (int)read_number(b, 1);
    read_string(b, t->title, TITLE_MAX);
    read_string(b, t->period, 10);
    read_string(b, t->completed_on, 10);
    t->days_count = (int)read_number(b, 1);
    if (t->id < 1 || t->id >= v->next_id || t->kind > 2 || t->done > 1 ||
        !t->title[0] || t->days_count > HISTORY_DAYS)
      b->ok = 0;
    for (int j = 0; j < i; j++)
      if (v->tasks[j].id == t->id)
        b->ok = 0;
    for (int j = 0; j < t->days_count && b->ok; j++) {
      read_string(b, t->days[j], 10);
      if (strlen(t->days[j]) != 10)
        b->ok = 0;
    }
  }
  v->progress_count = (int)read_number(b, 1);
  if (v->progress_count > HISTORY_DAYS)
    b->ok = 0;
  for (int i = 0; i < v->progress_count && b->ok; i++) {
    DayProgress *d = &v->progress[i];
    read_string(b, d->day, 10);
    d->total = (int)read_number(b, 2);
    d->done = (int)read_number(b, 2);
    d->habit_total = (int)read_number(b, 2);
    d->habit_done = (int)read_number(b, 2);
    if (strlen(d->day) != 10 || d->total > MAX_TASKS || d->done > d->total ||
        d->habit_total > MAX_TASKS || d->habit_done > d->habit_total)
      b->ok = 0;
  }
  v->next_journal_id = (int)read_number(b, 4);
  v->journal_count = (int)read_number(b, 2);
  if (v->next_journal_id < 1 || v->journal_count > MAX_JOURNAL)
    b->ok = 0;
  for (int i = 0; i < v->journal_count && b->ok; i++) {
    JournalEntry *e = &v->journal[i];
    e->id = (int)read_number(b, 4);
    read_string(b, e->day, 10);
    read_string(b, e->body, JOURNAL_MAX);
    if (e->id < 1 || e->id >= v->next_journal_id || strlen(e->day) != 10 ||
        !e->body[0])
      b->ok = 0;
    for (int j = 0; j < i; j++)
      if (v->journal[j].id == e->id)
        b->ok = 0;
  }
  int ok = b->ok && b->pos == b->n;
  if (ok)
    *m = *v;
  free(v);
  return ok;
}
int model_load(Model *m, const char *path, char *err, size_t n) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    if (errno == ENOENT) {
      model_init(m);
      return 1;
    }
    snprintf(err, n, "Cannot open saved data: %s", strerror(errno));
    return 0;
  }
  unsigned char header[16];
  size_t got = fread(header, 1, 16, f);
  if (got && header[0] == '{') {
    fclose(f);
    return legacy_model_load(m, path, err, n);
  }
  if (got != 16 || memcmp(header, "DBKSTR01", 8)) {
    fclose(f);
    snprintf(err, n, "Saved data invalid; original preserved");
    return 0;
  }
  Bytes h = {.p = header, .n = 16, .pos = 8, .ok = 1};
  size_t len = (size_t)read_number(&h, 4);
  uint32_t sum = (uint32_t)read_number(&h, 4);
  if (!len || len > STORE_LIMIT) {
    fclose(f);
    snprintf(err, n, "Saved data size invalid");
    return 0;
  }
  Bytes b = {.p = malloc(len), .n = len, .ok = 1};
  int ok = b.p && fread(b.p, 1, len, f) == len && fgetc(f) == EOF;
  fclose(f);
  if (ok)
    ok = checksum(b.p, b.n) == sum && decode(m, &b);
  free(b.p);
  if (!ok)
    snprintf(err, n,
             "Saved data checksum or records invalid; original preserved");
  return ok;
}
int model_save(const Model *m, const char *path, char *err, size_t n) {
  Bytes b = {.ok = 1};
  number(&b, (uint32_t)m->next_id, 4);
  number(&b, (uint64_t)m->offset, 8);
  number(&b, (unsigned)m->count, 2);
  for (int i = 0; i < m->count; i++) {
    const Task *t = &m->tasks[i];
    number(&b, (uint32_t)t->id, 4);
    number(&b, (unsigned)t->kind, 1);
    number(&b, (unsigned)t->done, 1);
    string(&b, t->title, TITLE_MAX);
    string(&b, t->period, 10);
    string(&b, t->completed_on, 10);
    number(&b, (unsigned)t->days_count, 1);
    for (int j = 0; j < t->days_count; j++)
      string(&b, t->days[j], 10);
  }
  number(&b, (unsigned)m->progress_count, 1);
  for (int i = 0; i < m->progress_count; i++) {
    const DayProgress *d = &m->progress[i];
    string(&b, d->day, 10);
    number(&b, (unsigned)d->total, 2);
    number(&b, (unsigned)d->done, 2);
    number(&b, (unsigned)d->habit_total, 2);
    number(&b, (unsigned)d->habit_done, 2);
  }
  number(&b, (uint32_t)m->next_journal_id, 4);
  number(&b, (unsigned)m->journal_count, 2);
  for (int i = 0; i < m->journal_count; i++) {
    const JournalEntry *e = &m->journal[i];
    number(&b, (uint32_t)e->id, 4);
    string(&b, e->day, 10);
    string(&b, e->body, JOURNAL_MAX);
  }
  if (!b.ok) {
    free(b.p);
    snprintf(err, n, "Could not encode saved data");
    return 0;
  }
  Bytes header = {.ok = 1};
  put(&header, "DBKSTR01", 8);
  number(&header, b.n, 4);
  number(&header, checksum(b.p, b.n), 4);
  char tmp[1024];
  if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) {
    free(b.p);
    free(header.p);
    return 0;
  }
  int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600), ok = fd >= 0;
  for (int i = 0; i < 2 && ok; i++) {
    Bytes *part = i ? &b : &header;
    size_t off = 0;
    while (off < part->n) {
      ssize_t w = write(fd, part->p + off, part->n - off);
      if (w < 0 && errno == EINTR)
        continue;
      if (w <= 0) {
        ok = 0;
        break;
      }
      off += (size_t)w;
    }
  }
  free(b.p);
  free(header.p);
  if (fd >= 0) {
    if (ok && fsync(fd))
      ok = 0;
    if (close(fd))
      ok = 0;
  }
  if (ok && rename(tmp, path))
    ok = 0;
  if (!ok) {
    snprintf(err, n, "Could not save records: %s", strerror(errno));
    unlink(tmp);
    return 0;
  }
  char dir[1024];
  snprintf(dir, sizeof(dir), "%s", path);
  char *s = strrchr(dir, '/');
  if (s) {
    *s = 0;
    int d = open(*dir ? dir : "/", O_RDONLY);
    if (d >= 0) {
      fsync(d);
      close(d);
    }
  }
  return 1;
}
