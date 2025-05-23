#include "system.h"
#include "input.h"
#include "render.h"
#include "platform.h"
#include "mem.h"
#include "utils.h"

#include "wipeout/game.h"

static double time_real;
static double time_scaled;
static double time_scale = 1.0;
static double tick_last;
static double cycle_time = 0;

void system_init(void) {
	time_real = platform_now();
	input_init();
	render_init(platform_screen_size());
	game_init();
}

void system_cleanup(void) {
	render_cleanup();
	input_cleanup();
}

void system_exit(void) {
	platform_exit();
}

void system_update(void) {
	double time_real_now = platform_now();
	double real_delta = time_real_now - time_real;
	// if (real_delta < 1.0 / 30.0) {
	// 	return;
	// }
	time_real = time_real_now;
	tick_last = min(real_delta, 0.1) * time_scale;
	time_scaled += tick_last;

	// printf("time_real: %f\n", time_real);
	// printf("time_scaled: %f\n", time_scaled);
	// printf("time_scale: %f\n", time_scale );
	// printf("tick_last: %f\n", tick_last);
	// printf("cycle_time: %f\n", cycle_time);

	// FIXME: come up with a better way to wrap the cycle_time, so that it
	// doesn't lose precision, but also doesn't jump upon reset.
	cycle_time = time_scaled;
	if (cycle_time > 3600 * M_PI) {
		cycle_time -= 3600 * M_PI;
	}

	// render_set_resolution(RENDER_RES_NATIVE); //testing
	// render_set_projection_fov(90.0 + (uint)system_time() % 30); //testing
	
	render_frame_prepare();
	
	game_update();

	render_frame_end();
	input_clear();
	mem_temp_check();
}

void system_reset_cycle_time(void) {
	cycle_time = 0;
}

void system_resize(vec2i_t size) {
	render_set_screen_size(size);
}

double system_time_scale_get(void) {
	return time_scale;
}

void system_time_scale_set(double scale) {
	time_scale = scale;
}

double system_tick(void) {
	return tick_last;
}

double system_time(void) {
	return time_scaled;
}

double system_cycle_time(void) {
	return cycle_time;
}
