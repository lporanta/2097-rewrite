#ifndef LINE_PARTICLE_H
#define LINE_PARTICLE_H

#include "../types.h"

#define LINE_PARTICLES_MAX 1024

#define LINE_PARTICLE_TYPE_NONE -1
#define LINE_PARTICLE_TYPE_SCRAPE 0

typedef struct line_particle_t {
	vec3_t position;
	vec3_t velocity;
	vec2i_t size;
	rgba_t color;
	float timer;
	uint16_t type;
} line_particle_t;

void line_particles_load(void);
void line_particles_init(void);
void line_particles_spawn(vec3_t position, uint16_t type, vec3_t velocity, int size);
void line_particles_draw(void);
void line_particles_update(void);

#endif
