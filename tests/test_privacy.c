#define _DARWIN_C_SOURCE 1
#define main daybook_application_main
#include "../src/daybook.c"
#undef main
#include <assert.h>
static void enter_pin(const char *pin) {
  for (const char *p = pin; *p; p++) {
    int key = *p - '1';
    if (*p == '0')
      touch(250, 570);
    else
      touch(60 + (key % 3) * 184, 330 + (key / 3) * 82);
  }
  touch(300, 650);
}
int main(void) {
  char dir[] = "/tmp/daybook-private-XXXXXX", reply[REPLY_MAX];
  assert(mkdtemp(dir));
  snprintf(data_dir, sizeof(data_dir), "%s", dir);
  path(state_path, sizeof(state_path), "state.bin");
  snprintf(day, sizeof(day), "2026-10-21");
  model_init(&model);
  assert(model_command(&model, "/journal Original private memory", "2026-10-21",
                       reply, sizeof(reply)));
  touch(390, 30);
  assert(screen == 8 && pin_phase == 1);
  enter_pin("73194682");
  assert(pin_phase == 2 && !journal_vault.enabled);
  enter_pin("73194682");
  assert(screen == 5 && journal_vault.unlocked);
  touch(390, 30);
  assert(screen == 8 && !journal_vault.unlocked && !model.journal[0].body[0]);
  assert(!app_command("/jread 1", reply, sizeof(reply)) &&
         strstr(reply, "locked"));
  assert(!app_command("/journal", reply, sizeof(reply)) &&
         strstr(reply, "locked"));
  assert(!app_command("/jedit 1 Altered", reply, sizeof(reply)) &&
         strstr(reply, "locked"));
  assert(!app_command("/jdelete 1", reply, sizeof(reply)) &&
         model.journal_count == 1);
  assert(apply("/journal Added without reading"));
  assert(model.journal_count == 2 && !model.journal[1].body[0]);
  /* Locked write button opens only a new, blank entry. */
  touch(80, 740);
  assert(screen == 1 && journal_mode && !edit_id && !draft[0]);
  snprintf(draft, sizeof(draft), "Unfinished draft must survive locking");
  last_touch = 100;
  idle_tick(220);
  assert(screen == 8 && !draft[0] && sealed_draft_len);
  enter_pin("00000000");
  assert(screen == 8 && !journal_vault.unlocked);
  enter_pin("73194682");
  assert(screen == 1 && journal_vault.unlocked &&
         !strcmp(draft, "Unfinished draft must survive locking"));
  assert(!strcmp(model.journal[0].body, "Original private memory"));
  assert(!strcmp(model.journal[1].body, "Added without reading"));
  touch(80, 740);
  assert(screen == 5);
  assert(!app_command("/jread 1", reply, sizeof(reply)) &&
         strstr(reply, "Original private memory"));
  snprintf(chat_id, sizeof(chat_id), "12345");
  app_command("/jlock", reply, sizeof(reply));
  cJSON *updates =
      cJSON_Parse("[{\"update_id\":1,\"message\":{\"chat\":{\"id\":12345,"
                  "\"type\":\"private\"},\"text\":\"/jread 1\"}}]");
  assert(updates);
  network_updates(updates);
  cJSON_Delete(updates);
  assert(response_count == 1 && strstr(responses[0], "locked") &&
         !strstr(responses[0], "Original private memory"));
  assert(model.offset == 2 && !journal_vault.unlocked);
  puts("PASS: PIN setup/unlock, locked read/edit/delete denial, locked append, "
       "encrypted draft recovery, and Telegram read authorization.");
  return 0;
}
