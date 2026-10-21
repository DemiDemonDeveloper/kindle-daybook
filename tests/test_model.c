#include "../src/model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void run(Model *m, const char *s, const char *day) {
  char r[REPLY_MAX];
  assert(model_command(m, s, day, r, sizeof(r)));
}
int main(void) {
  Model m;
  model_init(&m);
  char r[REPLY_MAX], err[256];
  run(&m, "/add Buy groceries", "2026-10-21");
  run(&m, "/daily Read 20 minutes", "2026-10-21");
  run(&m, "/goal Finish portfolio", "2026-10-21");
  assert(m.count == 3 && m.tasks[2].kind == 2);
  run(&m, "/done 2", "2026-10-21");
  assert(task_done(&m.tasks[1], "2026-10-21"));
  assert(!task_done(&m.tasks[1], "2026-10-22"));
  run(&m, "/undo 2", "2026-10-21");
  assert(!task_done(&m.tasks[1], "2026-10-21"));
  run(&m, "/done 1", "2026-10-21");
  assert(task_done(&m.tasks[0], "2026-10-22"));
  assert(!task_visible(&m.tasks[0], 0, "2026-10-22"));
  assert(task_visible(&m.tasks[2], 2, "2026-10-31"));
  assert(!task_visible(&m.tasks[2], 2, "2026-11-01"));
  assert(!model_command(&m, "/done 1oops", "2026-10-21", r, sizeof(r)));
  assert(m.count == 3);
  run(&m, "/edit 1 Eggs and bread", "2026-10-21");
  assert(!strcmp(m.tasks[0].title, "Eggs and bread"));
  run(&m, "/delete 1", "2026-10-21");
  assert(m.count == 2 && !task_find(&m, 1));
  run(&m, "/add Another task", "2026-10-21");
  assert(m.tasks[2].id == 4);
  char tmp[] = "/tmp/daybook-model-XXXXXX";
  int fd = mkstemp(tmp);
  assert(fd >= 0);
  close(fd);
  m.offset = 123456;
  assert(model_save(&m, tmp, err, sizeof(err)));
  Model loaded;
  assert(model_load(&loaded, tmp, err, sizeof(err)));
  assert(loaded.offset == 123456 && loaded.count == 3 &&
         !strcmp(loaded.tasks[2].title, "Another task"));
  FILE *f = fopen(tmp, "w");
  assert(f);
  fputs("{broken}", f);
  fclose(f);
  assert(!model_load(&loaded, tmp, err, sizeof(err)));
  f = fopen(tmp, "r");
  char b[16];
  assert(fgets(b, sizeof(b), f));
  fclose(f);
  assert(!strcmp(b, "{broken}"));
  unlink(tmp);
  model_init(&m);
  for (int i = 0; i < MAX_TASKS; i++)
    run(&m, "/add one", "2026-10-21");
  assert(!model_command(&m, "/add overflow", "2026-10-21", r, sizeof(r)) &&
         m.count == MAX_TASKS);
  puts("PASS: daily reset, monthly scoping, completion/undo, edits, deletion, "
       "ID stability, persistence, malformed-data protection, and capacity "
       "limits.");
  return 0;
}
