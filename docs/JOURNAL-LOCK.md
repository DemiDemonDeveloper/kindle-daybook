# Journal privacy lock

The journal PIN protects past entries while leaving new-entry writing available. Task lists and the dashboard screen cover remain available without the journal PIN.

## Set the PIN

After installing this version, open Daybook. Choose a **6–12 digit PIN** on the Kindle and enter it again to confirm. This encrypts existing journal entries in the current `state.bin` file. Protection is enabled only after setup succeeds.

Keep the PIN somewhere safe: **there is no PIN recovery or reset that preserves encrypted entries**. The PIN is not stored as text and should never be sent through Telegram or posted in an issue.

## While locked

- On the Kindle, open Journal and choose **+ Write entry** to write a new entry. Saving returns to the locked journal screen.
- In your paired Telegram chat, `/journal Your thoughts` still adds an entry.
- Reading, listing previews, editing, and deleting past entries require unlocking. The task commands continue working normally.
- Unlock on the Kindle to read past entries locally or retrieve them through your paired Telegram bot. This version does not accept a PIN through Telegram.

Tap **Lock** in the journal header, leave for Menu, open the dashboard cover, or send `/jlock` to lock the journal again. It also locks after two minutes without Kindle touch input, including while a journal editor is open. The current unfinished editor draft is sealed in memory and restored on the next PIN unlock; save your draft before exiting Daybook because an unsaved draft does not survive restarting the app.

Five failed PIN attempts trigger a one-minute on-screen cooldown. No entries are erased for failed attempts.

## Saved-data protection

The implementation uses libsodium 1.0.22:

- Argon2id derives the wrapping key from the PIN, a random 16-byte salt, 32 MiB of memory, and three operations.
- XChaCha20-Poly1305 protects the journal's private decryption key.
- Sealed boxes encrypt individual entries with the public key. That allows new entries to be added without unlocking the private key.
- Task records, sealed entries, and the Telegram update offset are committed together by atomic replacement of `state.bin`.
- Locking clears decrypted entry text and the private key from the active journal model. PIN buffers are cleared after use. The active journal is loaded locked on restart.
- Screen images and Telegram transport files use temporary system storage and are removed after use, rather than persisting journal contents in the USB data folder. Old transport and screen caches are removed on a normal launch. An obsolete `state.json` is removed after successful PIN setup/unlock.

See the upstream descriptions of [sealed boxes](https://doc.libsodium.org/public-key_cryptography/sealed_boxes) and [password-based key derivation](https://doc.libsodium.org/password_hashing/default_phf). The vendored source archive is pinned by SHA-256 in `scripts/build-sodium.sh`, with its license in `vendor/libsodium.LICENSE`.

## Copies that the lock cannot hide

Messages already sent or received in Telegram remain in that chat's history. Older backups, exported images, and copies from previous versions may contain unencrypted journal text. Keep those copies private or remove them yourself when no longer needed. Replacing a file on flash storage is not secure erasure of its old contents.

Task data and Telegram credentials are not encrypted by this feature. A short numeric PIN is intended to deter casual access; it is not a promise of protection against someone with unrestricted control of the Kindle, its operating system, or your unlocked Telegram account. The application-level security design has not undergone an independent security audit.

Back up the protected `state.bin` file with the rest of `daybook/data`, and retain the PIN separately. Older Daybook releases cannot open the protected file; do not downgrade after enabling protection.

## Verification

Desktop tests cover encryption migration, wrong PINs, locked additions across restart, edits/deletion, save failures, corruption detection, touchscreen PIN setup, locked Telegram reads, and draft recovery. Address/undefined-behavior sanitizer checks pass. On the physical KT2, PIN setup succeeded, new-entry writing remained available while locked, and past entries required the PIN. The locked Telegram read policy has desktop test coverage; a separate physical Telegram lock test has not yet been recorded.
