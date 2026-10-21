#define _POSIX_C_SOURCE 200809L
#include "../src/model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
  Model m;
  model_init(&m);
  char reply[REPLY_MAX], err[256];
  assert(model_command(&m, "/journal First thought\nSecond line: \"better\".",
                       "2026-10-21", reply, sizeof(reply)));
  assert(m.journal_count == 1 && m.journal[0].id == 1 &&
         !strcmp(m.journal[0].day, "2026-10-21"));
  assert(!model_command(&m, "/jread 1", "2026-10-21", reply, sizeof(reply)));
  assert(strstr(reply, "Second line"));
  assert(model_command(&m, "/jedit 1 An edited entry", "2026-10-22", reply,
                       sizeof(reply)));
  assert(!strcmp(m.journal[0].day, "2026-10-21"));
  char file[] = "/tmp/daybook-journal-XXXXXX";
  int fd = mkstemp(file);
  assert(fd >= 0);
  close(fd);
  assert(model_save(&m, file, err, sizeof(err)));
  Model loaded;
  assert(model_load(&loaded, file, err, sizeof(err)));
  assert(!strcmp(loaded.journal[0].body, "An edited entry"));
  FILE *f = fopen(file, "r+b");
  assert(f);
  assert(fseek(f, 20, SEEK_SET) == 0);
  int b = fgetc(f);
  fseek(f, 20, SEEK_SET);
  fputc(b ^ 1, f);
  fclose(f);
  assert(!model_load(&loaded, file, err, sizeof(err)));
  f = fopen(file, "wb");
  assert(f);
  fputs("{\"schema\":2,\"next_id\":2,\"telegram_offset\":55,\"tasks\":[{\"id\":"
        "1,\"kind\":1,\"done\":false,\"title\":\"Read\",\"period\":\"2026-10-"
        "21\",\"completed_on\":\"2026-10-21\",\"days\":[\"2026-10-21\"]}],"
        "\"progress\":[{\"day\":\"2026-10-21\",\"total\":1,\"done\":1,\"habit_"
        "total\":1,\"habit_done\":1}]}",
        f);
  fclose(f);
  assert(model_load(&loaded, file, err, sizeof(err)));
  assert(loaded.count == 1 && loaded.offset == 55 &&
         loaded.journal_count == 0 && loaded.next_journal_id == 1 &&
         task_done(&loaded.tasks[0], "2026-10-21"));
  assert(model_save(&loaded, file, err, sizeof(err)));
  assert(model_load(&loaded, file, err, sizeof(err)));
  assert(model_command(&m, "/jdelete 1", "2026-10-21", reply, sizeof(reply)));
  assert(m.journal_count == 0);
  assert(model_command(&m, "/journal Next entry", "2026-10-21", reply,
                       sizeof(reply)));
  assert(m.journal[0].id == 2);
  char huge[JOURNAL_MAX + 80];
  memset(huge, 'a', sizeof(huge));
  memcpy(huge, "/journal ", 9);
  huge[sizeof(huge) - 1] = 0;
  assert(!model_command(&m, huge, "2026-10-21", reply, sizeof(reply)));
  assert(m.journal_count == 1);
  unlink(file);
  puts("PASS: journal multiline/edit/read/delete, stable IDs, strings-file "
       "round trip and corruption detection, and migration preserving tasks, "
       "habits, history, and Telegram offset.");
  return 0;
}
