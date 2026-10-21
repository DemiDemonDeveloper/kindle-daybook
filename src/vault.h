#ifndef DAYBOOK_VAULT_H
#define DAYBOOK_VAULT_H
#include "model.h"
#define SEALED_MAX (JOURNAL_MAX + 64)
typedef struct {
  int id;
  size_t len;
  unsigned char data[SEALED_MAX], digest[32];
} SealedEntry;
typedef struct {
  int enabled, unlocked, count;
  unsigned char salt[16], nonce[24], wrapped[48], public_key[32],
      secret_key[32];
  SealedEntry entries[MAX_JOURNAL];
} Vault;
int vault_load(Vault *, Model *, const char *, char *, size_t);
int vault_save(Vault *, const Model *, const char *, char *, size_t);
int vault_enable(Vault *, Model *, const char *, const char *, char *, size_t);
int vault_unlock(Vault *, Model *, const char *, char *, size_t);
void vault_lock(Vault *, Model *);
int vault_pin_valid(const char *);
#endif
