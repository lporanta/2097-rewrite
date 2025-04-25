#include "../mem.h"
#include "../utils.h"
#include "../system.h"
#include "../render.h"

#include "game.h"

#include "line_particle.h"

static line_particle_t *line_particles;
static int line_particles_active = 0;

void line_particles_load(void) {
	line_particles = mem_bump(sizeof(line_particle_t) * LINE_PARTICLES_MAX);
	line_particles_init();
}

void line_particles_init(void) {
	line_particles_active = 0;
}

void line_particles_update(void) {
	for (int i = 0; i < line_particles_active; i++) {
		line_particle_t *p = &line_particles[i];

		p->timer -= system_tick();
		p->position = vec3_add(p->position, vec3_mulf(p->velocity, system_tick()));
		if (p->timer < 0) {
			line_particles[i--] = line_particles[--line_particles_active];
			continue;
		}
	}
}

void line_particles_draw(void) {
	if (line_particles_active == 0) {
		return;
	}

	mat4_t m = mat4_identity();
	// render_set_model_mat(&m);
	// render_set_depth_write(false);
	render_set_blend_mode(RENDER_BLEND_LIGHTER);
	// render_set_depth_offset(-32.0);

	for (int i = 0; i < line_particles_active; i++) {
		line_particle_t *p = &line_particles[i];
		mat4_set_translation(&m, p->position);
		mat4_set_yaw_pitch_roll(&m, g.camera.angle);
		render_set_model_mat(&m);
		// northeast
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = p->color
				},
				{
					.pos = {-20, 20, 0},
					.color = p->color
				},
				{
					.pos = {20, 20, 0},
					.color = p->color
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	// render_set_depth_offset(0.0);
	// render_set_depth_write(true);
	render_set_blend_mode(RENDER_BLEND_NORMAL);
}

void line_particles_spawn(vec3_t position, uint16_t type, vec3_t velocity, int size) {
	if (line_particles_active == LINE_PARTICLES_MAX) {
		return;
	}

	line_particle_t *p = &line_particles[line_particles_active++];
	p->color = (rand_int(0, 10) < 4) ? rgba(255,0,0,255) : rgba(255,255,255,255);
	p->position = position;
	p->velocity = velocity;
	// p->timer = rand_float(0.75, 1.0);
	p->timer = 0.5;
	p->size.x = size;
	p->size.y = size;
}

