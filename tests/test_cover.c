#define _DARWIN_C_SOURCE 1
#define main daybook_application_main
#include "../src/daybook.c"
#undef main
#include <assert.h>
int main(void) {
  char dir[] = "/tmp/daybook-cover-XXXXXX";
  assert(mkdtemp(dir));
  snprintf(data_dir, sizeof(data_dir), "%s", dir);
  last_touch = 100;
  idle_seconds = 300;
  screen = 0;
  idle_tick(399);
  assert(!covered);
  idle_tick(400);
  assert(covered);
  int count = cover_number;
  idle_tick(900);
  assert(cover_number == (unsigned)count);
  screen = 4;
  selected_id = 1;
  model_init(&model);
  char reply[REPLY_MAX];
  assert(model_command(&model, "/add Keep this task", "2026-10-21", reply,
                       sizeof(reply)));
  touch(200, 530);
  assert(!covered && screen == 4 && model.count == 1);
  screen = 1;
  last_touch = 100;
  idle_tick(1000);
  assert(!covered);
  screen = 0;
  idle_seconds = 0;
  idle_tick(1000);
  assert(!covered);
  screen = 3;
  touch(200, 380);
  assert(covered);
  touch(200, 380);
  assert(!covered && screen == 3);
  touch(200, 460);
  assert(wallpaper_mode);
  wallpaper_mode = 0;
  cover_settings_load();
  assert(wallpaper_mode);
  char file[1024];
  path(file, sizeof(file), "wallpaper.pgm");
  FILE *f = fopen(file, "wb");
  assert(f);
  fputs("P5\n600 800\n255\n", f);
  for (int i = 0; i < W * H; i++)
    fputc(77, f);
  fclose(f);
  assert(draw_wallpaper());
  assert(pixels[0] == 77 && pixels[W * H - 1] == 77);
  f = fopen(file, "wb");
  assert(f);
  fputs("P5\n600 800\n255\nshort", f);
  fclose(f);
  assert(!draw_wallpaper());
  assert(pixels[0] == 255);
  path(file, sizeof(file), "quotes.txt");
  f = fopen(file, "w");
  assert(f);
  fputs("Make one small thing happen.\nKeep going.\n", f);
  fclose(f);
  wallpaper_mode = 0;
  draw_cover();
  assert(cover_quote_count == 2);
  puts("PASS: idle cover, editor protection, wake touch isolation, mode "
       "persistence, wallpaper validation, and custom quotes.");
  return 0;
}
