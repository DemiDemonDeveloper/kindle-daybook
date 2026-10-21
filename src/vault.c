#define _POSIX_C_SOURCE 200809L
#include "vault.h"
#include <ctype.h>
#include <errno.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* State combines public tasks, the PIN-wrapped private key, and sealed entries.
 * One atomic replacement commits journal edits and Telegram offset together. */
#define LIMIT (4u * 1024u * 1024u)
#define HEADER 146u
static void put32(unsigned char *p, unsigned v) {
  for (int i = 0; i < 4; i++)
    p[i] = (unsigned char)(v >> (i * 8));
}
static unsigned get32(const unsigned char *p) {
  return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 |
         (unsigned)p[3] << 24;
}
static int init(void) { return sodium_init() >= 0; }
int vault_pin_valid(const char *pin) {
  size_t len = strlen(pin);
  if (len < 6 || len > 12)
    return 0;
  for (size_t i = 0; i < len; i++)
    if (!isdigit((unsigned char)pin[i]))
      return 0;
  return 1;
}
static int derive(unsigned char key[32], const Vault *v, const char *pin) {
  return vault_pin_valid(pin) &&
         crypto_pwhash(key, 32, pin, strlen(pin), v->salt, 3,
                       32u * 1024u * 1024u, crypto_pwhash_ALG_ARGON2ID13) == 0;
}
static void associated(const Vault *v, unsigned char ad[56]) {
  memcpy(ad, "DBKJNL01", 8);
  memcpy(ad + 8, v->salt, 16);
  memcpy(ad + 24, v->public_key, 32);
}
static SealedEntry *find(Vault *v, int id) {
  for (int i = 0; i < v->count; i++)
    if (v->entries[i].id == id)
      return &v->entries[i];
  return NULL;
}
void vault_lock(Vault *v, Model *m) {
  if (!v->enabled)
    return;
  sodium_memzero(v->secret_key, 32);
  v->unlocked = 0;
  for (int i = 0; i < MAX_JOURNAL; i++) {
    int id = i < m->journal_count ? m->journal[i].id : 0;
    sodium_memzero(&m->journal[i], sizeof(m->journal[i]));
    m->journal[i].id = id;
  }
}
int vault_save(Vault *v, const Model *m, const char *path, char *err,
               size_t n) {
  if (!v->enabled)
    return model_save(m, path, err, n);
  if (!init() || m->journal_count < 0 || m->journal_count > MAX_JOURNAL)
    return 0;
  Vault *next = malloc(sizeof(*next));
  Model *tasks = malloc(sizeof(*tasks));
  if (!next || !tasks) {
    free(next);
    free(tasks);
    return 0;
  }
  *next = *v;
  next->count = 0;
  for (int i = 0; i < m->journal_count; i++) {
    const JournalEntry *e = &m->journal[i];
    SealedEntry *dest = &next->entries[next->count++];
    if (e->body[0]) {
      size_t len = strnlen(e->body, JOURNAL_MAX + 1);
      if (len > JOURNAL_MAX || strlen(e->day) != 10)
        goto fail;
      unsigned char plain[JOURNAL_MAX + 16];
      put32(plain, (unsigned)e->id);
      memcpy(plain + 4, e->day, 10);
      plain[14] = (unsigned char)len;
      plain[15] = (unsigned char)(len >> 8);
      memcpy(plain + 16, e->body, len);
      dest->id = e->id;
      dest->len = len + 16 + crypto_box_SEALBYTES;
      unsigned char digest[32];
      crypto_generichash(digest, 32, plain, len + 16, NULL, 0);
      SealedEntry *old = find(v, e->id);
      int rc = 0;
      if (old && !sodium_memcmp(old->digest, digest, 32))
        *dest = *old;
      else {
        rc = crypto_box_seal(dest->data, plain, len + 16, v->public_key);
        memcpy(dest->digest, digest, 32);
      }
      sodium_memzero(plain, sizeof(plain));
      sodium_memzero(digest, sizeof(digest));
      if (rc)
        goto fail;
    } else {
      SealedEntry *old = find(v, e->id);
      if (!old)
        goto fail;
      *dest = *old;
    }
  }
  *tasks = *m;
  tasks->journal_count = 0;
  sodium_memzero(tasks->journal, sizeof(tasks->journal));
  unsigned char *packed = NULL;
  size_t len = 0;
  int ok = model_pack(tasks, &packed, &len, err, n);
  sodium_memzero(tasks, sizeof(*tasks));
  free(tasks);
  tasks = NULL;
  if (!ok)
    goto fail;
  size_t total = HEADER + len;
  for (int i = 0; i < next->count; i++)
    total += 8 + next->entries[i].len;
  total += 32;
  unsigned char *out = malloc(total);
  if (!out) {
    free(packed);
    goto fail;
  }
  memcpy(out, "DBKSEC01", 8);
  put32(out + 8, (unsigned)len);
  memcpy(out + 12, v->salt, 16);
  memcpy(out + 28, v->nonce, 24);
  memcpy(out + 52, v->wrapped, 48);
  memcpy(out + 100, v->public_key, 32);
  put32(out + 132, (unsigned)next->count);
  put32(out + 136, (unsigned)m->next_journal_id);
  memset(out + 140, 0, 6);
  memcpy(out + HEADER, packed, len);
  free(packed);
  size_t pos = HEADER + len;
  for (int i = 0; i < next->count; i++) {
    SealedEntry *e = &next->entries[i];
    put32(out + pos, (unsigned)e->id);
    put32(out + pos + 4, (unsigned)e->len);
    memcpy(out + pos + 8, e->data, e->len);
    pos += 8 + e->len;
  }
  crypto_generichash(out + total - 32, 32, out, total - 32, NULL, 0);
  ok = store_write(path, out, total, err, n);
  free(out);
  if (ok)
    *v = *next;
  sodium_memzero(next, sizeof(*next));
  free(next);
  return ok;
fail:
  if (tasks) {
    sodium_memzero(tasks, sizeof(*tasks));
    free(tasks);
  }
  sodium_memzero(next, sizeof(*next));
  free(next);
  snprintf(err, n, "Could not encrypt/save journal; original preserved");
  return 0;
}
int vault_load(Vault *v, Model *m, const char *path, char *err, size_t n) {
  memset(v, 0, sizeof(*v));
  if (!init())
    return 0;
  FILE *f = fopen(path, "rb");
  if (!f)
    return model_load(m, path, err, n);
  unsigned char magic[8];
  size_t got = fread(magic, 1, 8, f);
  if (got != 8 || memcmp(magic, "DBKSEC01", 8)) {
    fclose(f);
    return model_load(m, path, err, n);
  }
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return 0;
  }
  long sz = ftell(f);
  rewind(f);
  if (sz < (long)HEADER + 32 || sz > (long)LIMIT) {
    fclose(f);
    return 0;
  }
  unsigned char *data = malloc((size_t)sz);
  int ok = data && fread(data, 1, (size_t)sz, f) == (size_t)sz;
  fclose(f);
  if (!ok) {
    free(data);
    return 0;
  }
  unsigned char digest[32];
  crypto_generichash(digest, 32, data, (size_t)sz - 32, NULL, 0);
  if (sodium_memcmp(digest, data + sz - 32, 32))
    goto fail;
  sz -= 32;
  unsigned len = get32(data + 8), count = get32(data + 132),
           next = get32(data + 136);
  if (len > (size_t)sz - HEADER || count > MAX_JOURNAL || next < 1 ||
      next > 2147483646u)
    goto fail;
  if (!model_unpack(m, data + HEADER, len, err, n) || m->journal_count)
    goto fail;
  memcpy(v->salt, data + 12, 16);
  memcpy(v->nonce, data + 28, 24);
  memcpy(v->wrapped, data + 52, 48);
  memcpy(v->public_key, data + 100, 32);
  size_t pos = HEADER + len;
  for (unsigned i = 0; i < count; i++) {
    if ((size_t)sz - pos < 8)
      goto fail;
    unsigned id = get32(data + pos), bytes = get32(data + pos + 4);
    pos += 8;
    if (id < 1 || id >= next || bytes < 65 || bytes > SEALED_MAX ||
        bytes > (size_t)sz - pos || find(v, (int)id))
      goto fail;
    SealedEntry *e = &v->entries[v->count++];
    e->id = (int)id;
    e->len = bytes;
    memcpy(e->data, data + pos, bytes);
    pos += bytes;
    m->journal[i].id = (int)id;
  }
  if (pos != (size_t)sz)
    goto fail;
  m->journal_count = (int)count;
  m->next_journal_id = (int)next;
  v->enabled = 1;
  free(data);
  return 1;
fail:
  free(data);
  sodium_memzero(v, sizeof(*v));
  snprintf(err, n, "Protected journal invalid; original preserved");
  return 0;
}
int vault_enable(Vault *v, Model *m, const char *pin, const char *path,
                 char *err, size_t n) {
  if (v->enabled || !init() || !vault_pin_valid(pin))
    return 0;
  unsigned char key[32], ad[56];
  Vault *fresh = calloc(1, sizeof(*fresh));
  if (!fresh)
    return 0;
  randombytes_buf(fresh->salt, 16);
  randombytes_buf(fresh->nonce, 24);
  crypto_box_keypair(fresh->public_key, fresh->secret_key);
  if (!derive(key, fresh, pin)) {
    sodium_memzero(key, sizeof(key));
    sodium_memzero(fresh, sizeof(*fresh));
    free(fresh);
    return 0;
  }
  associated(fresh, ad);
  crypto_aead_xchacha20poly1305_ietf_encrypt(fresh->wrapped, NULL,
                                             fresh->secret_key, 32, ad, 56,
                                             NULL, fresh->nonce, key);
  sodium_memzero(key, sizeof(key));
  fresh->enabled = 1;
  fresh->unlocked = 1;
  int ok = vault_save(fresh, m, path, err, n);
  if (ok)
    *v = *fresh;
  sodium_memzero(fresh, sizeof(*fresh));
  free(fresh);
  return ok;
}
int vault_unlock(Vault *v, Model *m, const char *pin, char *err, size_t n) {
  if (!v->enabled)
    return 0;
  unsigned char key[32], sk[32], ad[56];
  associated(v, ad);
  int ok = derive(key, v, pin);
  if (ok)
    ok = crypto_aead_xchacha20poly1305_ietf_decrypt(
             sk, NULL, NULL, v->wrapped, 48, ad, 56, v->nonce, key) == 0;
  sodium_memzero(key, sizeof(key));
  Model *copy = malloc(sizeof(*copy));
  if (!copy)
    ok = 0;
  if (ok) {
    *copy = *m;
    for (int i = 0; i < v->count && ok; i++) {
      SealedEntry *e = &v->entries[i];
      unsigned char plain[JOURNAL_MAX + 16];
      ok = e->len >= 65 &&
           crypto_box_seal_open(plain, e->data, e->len, v->public_key, sk) == 0;
      if (ok) {
        size_t bytes = (size_t)plain[14] | (size_t)plain[15] << 8;
        ok = get32(plain) == (unsigned)e->id && bytes > 0 &&
             bytes <= JOURNAL_MAX && bytes + 64 == e->len &&
             !memchr(plain + 16, 0, bytes);
        if (ok) {
          JournalEntry *entry = &copy->journal[i];
          entry->id = e->id;
          memcpy(entry->day, plain + 4, 10);
          entry->day[10] = 0;
          memcpy(entry->body, plain + 16, bytes);
          entry->body[bytes] = 0;
        }
      }
      sodium_memzero(plain, sizeof(plain));
    }
  }
  if (ok) {
    *m = *copy;
    memcpy(v->secret_key, sk, 32);
    v->unlocked = 1;
    /* Digests stay in memory and only avoid resealing unchanged entries. */
    for (int i = 0; i < v->count; i++)
      sodium_memzero(v->entries[i].digest, 32);
  }
  if (copy) {
    sodium_memzero(copy, sizeof(*copy));
    free(copy);
  }
  sodium_memzero(sk, sizeof(sk));
  if (!ok)
    snprintf(err, n, "PIN incorrect or journal damaged");
  return ok;
}
