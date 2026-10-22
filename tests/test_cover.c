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
  time_t saved_touch = last_touch;
  dirty = 0;
  for (int i = 0; i < 100; i++)
    touch(200, 530);
  assert(covered && screen == 4 && model.count == 1 && !dirty &&
         last_touch == saved_touch);
  assert(!power_button_event(0, 900));
  assert(!power_button_event(1, 1000));
  assert(power_button_event(0, 1100));
  assert(!covered && screen == 4 && model.count == 1);
  assert(touch_contact_should_discard(1050, monotonic_milliseconds()));
  assert(touch_contact_should_discard(1200, monotonic_milliseconds()));
  touch_quiet_until_ms = 0;
  assert(touch_contact_should_discard(1050, monotonic_milliseconds()));
  assert(!touch_contact_should_discard(1200, monotonic_milliseconds()));
  assert(!power_button_event(0, 1200));
  assert(!covered);
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
  assert(covered && screen == 3);
  assert(!power_button_event(1, 3000));
  assert(!power_button_event(2, 3500));
  assert(!power_button_event(0, 3600));
  assert(covered);
  assert(!power_button_event(1, 5000));
  assert(!power_button_event(0, 7000));
  assert(covered);
  assert(!power_button_event(1, 9000));
  assert(power_button_event(0, 9100));
  assert(!covered && screen == 3);
  assert(!power_button_event(1, 9200));
  assert(!power_button_event(0, 9300));
  assert(!covered);
  touch_quiet_until_ms = 0;
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
  puts("PASS: idle cover, editor protection, touch-only wake rejection and "
       "short Power-button toggling, mode "
       "persistence, wallpaper validation, and custom quotes.");
  return 0;
}
