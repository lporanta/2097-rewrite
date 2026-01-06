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
	mat4_t m_angle = mat4_identity();
	mat4_set_yaw_pitch_roll(&m_angle, g.camera.angle);
	// render_set_model_mat(&m);
	// render_set_depth_write(false);
	render_set_cull_backface(false);
	render_set_blend_mode(RENDER_BLEND_LIGHTER);
	// render_set_depth_offset(-32.0);

	vec3_t head0;
	vec3_t head1;
	for (int i = 0; i < line_particles_active; i++) {
		line_particle_t *p = &line_particles[i];
		mat4_set_translation(&m, p->position);
		// mat4_set_yaw_pitch_roll(&m, g.camera.angle);
		render_set_model_mat(&m);
		if (p->type == LINE_PARTICLE_TYPE_SCRAPE) {
			head0 = vec3(0,5,0);
			head1 = vec3(0,-5,0);
			// head0 = vec3(5,0,0);
			// head1 = vec3(-5,0,0);
		}
		else if (p->type == LINE_PARTICLE_TYPE_RECHARGE) {
			head0 = vec3(5,0,0);
			head1 = vec3(-5,0,0);
		}
		head0 = vec3_transform(head0, &m_angle);
		head1 = vec3_transform(head1, &m_angle);
		vec3_t tail = vec3_mulf(p->velocity, -0.08);
		// printf("p->velocity: %f, %f, %f\n", p->velocity.x, p->velocity.y, p->velocity.z); 
		// printf("tail: %f, %f, %f\n", p->velocity.x, p->velocity.y, p->velocity.z); 
		// vec3_t tail = vec3(0,10,0.1*vec3_len(p->velocity);
		// vec3_t tail = vec3_transform(vec3_mulf(p->velocity, -1.00), &m_tail);
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = head0,
					.color = p->color
				},
				{
					.pos = head1,
					.color = p->color
				},
				{
					.pos = tail,
					.color = rgba(p->color.r,p->color.g,p->color.b,0)
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	// render_set_depth_offset(0.0);
	// render_set_depth_write(true);
	render_set_cull_backface(true);
	render_set_blend_mode(RENDER_BLEND_NORMAL);
}

void line_particles_spawn(vec3_t position, uint16_t type, vec3_t velocity, int size) {
	if (line_particles_active == LINE_PARTICLES_MAX) {
		return;
	}

	line_particle_t *p = &line_particles[line_particles_active++];
	// TODO:switch case
	if (type == LINE_PARTICLE_TYPE_SCRAPE) {
		p->color = (rand_int(0, 10) < 2) ? rgba(255,0,0,255) : rgba(255,255,255,255);
	}
	else if (type == LINE_PARTICLE_TYPE_RECHARGE) {
		p->color = rgba(255,0,0,255);
	}
	p->position = position;
	p->velocity = velocity;
	p->timer = rand_float(0.1, 0.4);
	// p->timer = 0.2;
	p->type = type;
	p->size.x = size;
	p->size.y = size;
}

