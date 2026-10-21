#define _DARWIN_C_SOURCE 1
#define main daybook_application_main
#include "../src/daybook.c"
#undef main
#include <assert.h>
int main(void) {
  char dir[] = "/tmp/daybook-ui-XXXXXX";
  assert(mkdtemp(dir));
  snprintf(data_dir, sizeof(data_dir), "%s", dir);
  path(state_path, sizeof(state_path), "state.json");
  snprintf(day, sizeof(day), "2026-10-21");
  model_init(&model);
  touch(30, 740);
  assert(screen == 1);
  touch(30, 485);
  touch(210, 425);
  assert(!strcmp(draft, "ar"));
  touch(400, 740);
  assert(screen == 0 && model.count == 1 &&
         !strcmp(model.tasks[0].title, "ar"));
  list_indices();
  touch(30, 740);
  snprintf(draft, sizeof(draft), "Read 20 minutes");
  touch(250, 270);
  touch(400, 740);
  assert(model.count == 2 && model.tasks[1].kind == 1);
  list_indices();
  touch(30, 260);
  assert(model.tasks[0].done);
  touch(550, 340);
  assert(screen == 2);
  touch(200, 530);
  assert(screen == 4 && model.count == 2);
  touch(200, 530);
  assert(screen == 0 && model.count == 1);
  snprintf(chat_id, sizeof(chat_id), "12345");
  cJSON *updates = cJSON_Parse(
      "[{\"update_id\":1,\"message\":{\"chat\":{\"id\":999,\"type\":"
      "\"private\"},\"text\":\"/delete "
      "1\"}},{\"update_id\":2,\"message\":{\"chat\":{\"id\":12345,\"type\":"
      "\"group\"},\"text\":\"/add Wrong "
      "group\"}},{\"update_id\":3,\"message\":{\"chat\":{\"id\":12345,\"type\":"
      "\"private\"},\"text\":\"/add Telegram task\"}}]");
  assert(updates);
  network_updates(updates);
  assert(model.count == 2 && model.offset == 4 &&
         !strcmp(model.tasks[1].title, "Telegram task"));
  network_updates(updates);
  assert(model.count == 2 && model.offset == 4);
  cJSON_Delete(updates);
  char reply[REPLY_MAX];
  model_command(&model, "/daily Walk", day, reply, sizeof(reply));
  model_command(&model, "/done 4", day, reply, sizeof(reply));
  assert(model_progress(&model, day)->habit_done == 1);
  assert(task_done(task_find(&model, 4), day));
  for (int i = 0; i < 95; i++) {
    time_t t = 1767225600 + (time_t)i * 86400;
    struct tm tm;
    gmtime_r(&t, &tm);
    char d[11];
    strftime(d, sizeof(d), "%Y-%m-%d", &tm);
    model_command(&model, "/done 4", d, reply, sizeof(reply));
  }
  assert(model.progress_count == 90 && task_find(&model, 4)->days_count == 90);
  assert(!model_progress(&model, "2026-01-01"));
  assert(model_save(&model, state_path, reply, sizeof(reply)));
  Model loaded;
  assert(model_load(&loaded, state_path, reply, sizeof(reply)));
  assert(loaded.progress_count == 90 &&
         task_find(&loaded, 4)->days_count == 90);
  puts("PASS: touch keyboard/add/toggle, delete confirmation, Telegram owner "
       "authorization and replay protection, and persisted habit/graph history "
       "retention.");
  return 0;
}
