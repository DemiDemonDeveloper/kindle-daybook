#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#include "../src/vault.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int contains(const char *path, const char *needle) {
  FILE *f = fopen(path, "rb");
  assert(f);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  char *s = calloc((size_t)n + 1, 1);
  assert(s);
  assert(fread(s, 1, (size_t)n, f) == (size_t)n);
  fclose(f);
  int found = 0;
  size_t len = strlen(needle);
  for (size_t i = 0; i + len <= (size_t)n; i++)
    if (!memcmp(s + i, needle, len))
      found = 1;
  free(s);
  return found;
}
int main(void) {
  char folder[] = "/tmp/daybook-vault-XXXXXX", path[256], bad[256], err[180],
       reply[REPLY_MAX];
  assert(mkdtemp(folder));
  snprintf(path, sizeof(path), "%s/state.bin", folder);
  snprintf(bad, sizeof(bad), "%s/no-dir/state.bin", folder);
  Model *m = calloc(1, sizeof(*m)), *loaded = calloc(1, sizeof(*loaded));
  Vault *v = calloc(1, sizeof(*v)), *reopened = calloc(1, sizeof(*reopened));
  assert(m && loaded && v && reopened);
  model_init(m);
  assert(
      model_command(m, "/add Public task", "2026-10-21", reply, sizeof(reply)));
  assert(model_command(m, "/journal Secret-marker-original-entry", "2026-10-21",
                       reply, sizeof(reply)));
  assert(model_save(m, path, err, sizeof(err)));
  assert(!vault_enable(v, m, "123", path, err, sizeof(err)));
  assert(vault_enable(v, m, "73194682", path, err, sizeof(err)));
  assert(!contains(path, "Secret-marker-original-entry") &&
         !contains(path, "73194682"));
  vault_lock(v, m);
  assert(!v->unlocked && !m->journal[0].body[0]);
  assert(vault_load(reopened, loaded, path, err, sizeof(err)));
  assert(reopened->enabled && !reopened->unlocked && loaded->count == 1 &&
         loaded->journal_count == 1);
  assert(!vault_unlock(reopened, loaded, "00000000", err, sizeof(err)) &&
         !loaded->journal[0].body[0]);
  assert(model_command(loaded, "/journal Secret-marker-added-while-locked",
                       "2026-10-22", reply, sizeof(reply)));
  loaded->offset = 123;
  assert(vault_save(reopened, loaded, path, err, sizeof(err)));
  assert(!contains(path, "Secret-marker-added-while-locked"));
  vault_lock(reopened, loaded);
  assert(vault_load(v, m, path, err, sizeof(err)) && m->offset == 123);
  assert(vault_unlock(v, m, "73194682", err, sizeof(err)));
  assert(!strcmp(m->journal[0].body, "Secret-marker-original-entry"));
  assert(!strcmp(m->journal[1].body, "Secret-marker-added-while-locked"));
  assert(model_command(m, "/jedit 1 Secret-marker-revised-entry", "2026-10-22",
                       reply, sizeof(reply)));
  assert(model_command(m, "/jdelete 2", "2026-10-22", reply, sizeof(reply)));
  assert(vault_save(v, m, path, err, sizeof(err)));
  assert(!contains(path, "Secret-marker-revised-entry"));
  int count = v->count;
  assert(model_command(m, "/journal Failed write", "2026-10-22", reply,
                       sizeof(reply)));
  assert(!vault_save(v, m, bad, err, sizeof(err)) && v->count == count);
  assert(vault_load(reopened, loaded, path, err, sizeof(err)));
  assert(vault_unlock(reopened, loaded, "73194682", err, sizeof(err)));
  assert(loaded->journal_count == 1 &&
         !strcmp(loaded->journal[0].body, "Secret-marker-revised-entry"));
  FILE *f = fopen(path, "r+b");
  assert(f);
  fseek(f, 52, SEEK_SET);
  int c = fgetc(f);
  fseek(f, 52, SEEK_SET);
  fputc(c ^ 1, f);
  fclose(f);
  assert(!vault_load(v, m, path, err, sizeof(err)));
  free(m);
  free(loaded);
  free(v);
  free(reopened);
  puts("PASS: encrypted migration, wrong-PIN rejection, locked append across "
       "restart, edit/delete, atomic offset save, failure preservation, and "
       "tamper detection.");
  return 0;
}
