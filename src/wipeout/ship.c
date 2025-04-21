#include "../mem.h"
#include "../utils.h"
#include "../system.h"
#include "../input.h"

#include "object.h"
#include "scene.h"
#include "track.h"
#include "weapon.h"
#include "camera.h"
#include "image.h"
#include "object.h"
#include "ship.h"
#include "ship_ai.h"
#include "ship_player.h"
#include "game.h"
#include "race.h"
#include "sfx.h"
#include "particle.h"

void ships_load(void) {
	// 2097 models
	texture_list_t ship_textures = image_get_compressed_textures("wipeout2/common/terry.cmp");
	Object *ship_models = objects_load("wipeout2/common/terry.prm", ship_textures);

	texture_list_t collision_textures = image_get_compressed_textures("wipeout2/common/alcol.cmp");
	Object *collision_models = objects_load("wipeout2/common/alcol.prm", collision_textures);

	int object_index;
	Object *ship_model = ship_models;
	Object *collision_model = collision_models;

	for (object_index = 0; object_index < NUM_TEAMS && ship_model && collision_model; object_index++) {
		int ship_index = def.ship_model_to_pilot[object_index];
		g.ships[ship_index].model = ship_model;
		g.ships[ship_index].collision_model = collision_model;

		ship_model = ship_model->next;
		collision_model = collision_model->next;
	}

	error_if(object_index != NUM_TEAMS, "Expected %ld ship models, got %d", NUM_TEAMS, object_index);

	// TODO: how to get 2097 shadows?
	uint16_t shadow_textures_start = render_textures_len();
	image_get_texture_semi_trans("wipeout2/textures/shad1.tim");
	image_get_texture_semi_trans("wipeout2/textures/shad2.tim");
	image_get_texture_semi_trans("wipeout2/textures/shad3.tim");
	image_get_texture_semi_trans("wipeout2/textures/shad4.tim");

	for (int i = 0; i < len(g.ships); i++) {
		g.ships[i].shadow_texture = shadow_textures_start + (i >> 1);
	}
}


void ships_init(section_t *section) {
	section_t *start_sections[len(g.ships)];

	int ranks_to_pilots[NUM_PILOTS];

	// Initialize ranks with all pilots in order
	for (int i = 0; i < len(g.ships); i++) {
		ranks_to_pilots[i] = i;
	}

	// Randomize order for single race or new championship
	// if (g.race_type != RACE_TYPE_CHAMPIONSHIP || g.circut == CIRCUT_ALTIMA_VII) {
	// if (g.race_type != RACE_TYPE_CHAMPIONSHIP) {
	// 	shuffle(ranks_to_pilots, len(ranks_to_pilots));
	// }

	shuffle(ranks_to_pilots, len(ranks_to_pilots));

	// Randomize some tiers in an ongoing championship
	// else if (g.race_type == RACE_TYPE_CHAMPIONSHIP) {
	// 	// Initialize with current championship order
	// 	for (int i = 0; i < len(g.ships); i++) {
	// 		ranks_to_pilots[i] = g.championship_ranks[i].pilot;
	// 	}		
	// 	shuffle(ranks_to_pilots, 2); // shuffle 0..1
	// 	shuffle(ranks_to_pilots + 4, len(ranks_to_pilots)-5); // shuffle 4..len-1
	// }

	// player is always last
	for (int i = 0; i < len(ranks_to_pilots)-1; i++) {
		if (ranks_to_pilots[i] == g.pilot) {
			swap(ranks_to_pilots[i], ranks_to_pilots[i+1]);
		}
	}

	// TODO: something isn't right here
	int start_line_pos = def.circuts[g.circut].settings[g.race_class].start_line_pos;
	for (int i = 0; i < start_line_pos - 15; i++) {
		section = section->next;
	}
	for (int i = 0; i < len(g.ships); i++) {
		start_sections[i] = section;
		section = section->next;
		if ((i % 2) == 0) {
			section = section->next;
		}
	}

	for (int i = 0; i < len(ranks_to_pilots); i++) {
		int rank_inv = (len(g.ships)-1) - i;
		int pilot = ranks_to_pilots[i];
		ship_init(&g.ships[pilot], start_sections[rank_inv], pilot, rank_inv);
	}

	// init trail
	for (int i = 0; i < len(g.ships); i++) {
		for (int j = 0; j < NUM_SHIP_TRAIL_POINTS; j++) {
			g.ships[i].trail_buffer[j].left.color.a = 0;
			g.ships[i].trail_buffer[j].center.color.a = 0;
			g.ships[i].trail_buffer[j].right.color.a = 0;
		}
	}
}

static inline bool sort_rank_compare(pilot_points_t *pa, pilot_points_t *pb) {
	ship_t *a = &g.ships[pa->pilot];
	ship_t *b = &g.ships[pb->pilot];
	if (a->total_section_num == b->total_section_num) {
		vec3_t c0 = a->section->center;
		vec3_t c1 = a->section->next->center;
		vec3_t dir = vec3_sub(c1, c0);
		float pos_a = vec3_dot(vec3_sub(a->position, c0), dir);
		float pos_b = vec3_dot(vec3_sub(b->position, c0), dir);
		return (pos_a < pos_b);
	}
	else {
		return a->total_section_num < b->total_section_num;
	}
}

void ships_update(void) {
	if (g.race_type == RACE_TYPE_TIME_TRIAL) {
		ship_update(&g.ships[g.pilot]);
	}
	else {
		for (int i = 0; i < len(g.ships); i++) {
			ship_update(&g.ships[i]);
		}
		for (int j = 0; j < (len(g.ships) - 1); j++) {
			for (int i = j + 1; i < len(g.ships); i++) {
				ship_collide_with_ship(&g.ships[i], &g.ships[j]);
			}
		}

		if (flags_is(g.ships[g.pilot].flags, SHIP_RACING)) {
			sort(g.race_ranks, len(g.race_ranks), sort_rank_compare);
			for (int32_t i = 0; i < len(g.ships); i++) {
				g.ships[g.race_ranks[i].pilot].position_rank = i + 1;
			}
		}
	}
}

void ships_draw(void) {
	// Ship models
	for (int i = 0; i < len(g.ships); i++) {
		if (
			(flags_is(g.ships[i].flags, SHIP_VIEW_INTERNAL) && flags_not(g.ships[i].flags, SHIP_IN_RESCUE)) ||
			(g.race_type == RACE_TYPE_TIME_TRIAL && i != g.pilot)
		) {
			continue;
		}

		ship_draw(&g.ships[i]);
	}

	// Shadows
	render_set_model_mat(&mat4_identity());

	render_set_depth_write(false);
	render_set_depth_offset(-32.0);

	for (int i = 0; i < len(g.ships); i++) {
		if (
			(g.race_type == RACE_TYPE_TIME_TRIAL && i != g.pilot) ||
			flags_not(g.ships[i].flags, SHIP_VISIBLE) || 
			flags_is(g.ships[i].flags, SHIP_FLYING)
		) {
			continue;
		}

		ship_draw_shadow(&g.ships[i]);
	}

	render_set_depth_offset(0.0);
	render_set_depth_write(true);
	
	// Engine flare and trail
	for (int i = 0; i < len(g.ships); i++) {
		if (
			(g.race_type == RACE_TYPE_TIME_TRIAL && i != g.pilot) ||
			flags_not(g.ships[i].flags, SHIP_VISIBLE)
		) {
			continue;
		}
		// required or player flare ugly and race crashes
		ship_update_unit_vectors(&g.ships[i]);
		ship_draw_flare_psx(&g.ships[i]);
		if (
			i == g.pilot
			&& (g.ships[i].lap < NUM_LAPS && !g.is_attract_mode)
			&& flags_not(g.ships[i].flags, SHIP_IN_RESCUE)
		) {
			ship_draw_player_trail(&g.ships[i]);
		} else {
			ship_draw_trail(&g.ships[i]);
		}
	}
}

void ship_init(ship_t *self, section_t *section, int pilot, int inv_start_rank) {
	self->pilot = pilot;
	self->velocity = vec3(0, 0, 0);
	self->acceleration = vec3(0, 0, 0);
	self->angle = vec3(0, 0, 0);
	self->angular_velocity = vec3(0, 0, 0);
	self->turn_rate = 0;
	self->thrust_mag = 0;
	self->current_thrust_max = 0;
	self->turn_rate_from_hit = 0;
	self->brake_right = 0;
	self->brake_left = 0;
	self->flags = SHIP_RACING | SHIP_VISIBLE | SHIP_DIRECTION_FORWARD;
	self->weapon_type = WEAPON_TYPE_NONE;
	self->lap = -1;
	self->max_lap = -1;
	self->speed = 0;
	self->ebolt_timer = 0;
	self->revcon_timer = 0;
	self->special_timer = 0;
	self->weapon_target = NULL;
	self->mat = mat4_identity();

	self->update_timer = 0;
	self->last_impact_time = 0;

	self->last_boost_rumble = 666;
	self->trail_buffer_loc = 0;

	int team = def.pilots[pilot].team;
	self->mass =          def.teams[team].attributes[g.race_class].mass;
	self->thrust_max =    def.teams[team].attributes[g.race_class].thrust_max;
	self->skid =          def.teams[team].attributes[g.race_class].skid;
	self->turn_rate =     def.teams[team].attributes[g.race_class].turn_rate;
	self->turn_rate_max = def.teams[team].attributes[g.race_class].turn_rate_max;
	self->resistance =    def.teams[team].attributes[g.race_class].resistance;
	self->lap_time = 0;

	self->update_timer = UPDATE_TIME_INITIAL;
	self->position_rank = NUM_PILOTS - inv_start_rank;

	if (pilot == g.pilot) {
		self->update_func = ship_player_update_intro;
		self->remote_thrust_max = 2900;
		self->remote_thrust_mag = 46;
		self->fight_back = 0;
	}
	else {
		self->update_func = ship_ai_update_intro;
		self->remote_thrust_max = def.ai_settings[g.race_class][inv_start_rank-1].thrust_max;
		self->remote_thrust_mag = def.ai_settings[g.race_class][inv_start_rank-1].thrust_magnitude;
		self->fight_back = def.ai_settings[g.race_class][inv_start_rank-1].fight_back;
	}

	self->section = section;
	self->prev_section = section;
	float spread_base = def.circuts[g.circut].settings[g.race_class].spread_base;
	float spread_factor = def.circuts[g.circut].settings[g.race_class].spread_factor;
	int p = inv_start_rank - 1;
	self->start_accelerate_timer = p * (spread_base + (p * spread_factor)) * (1.0/30.0);

	track_face_t *face = g.track.faces + section->face_start;
	face++;
	if ((inv_start_rank % 2) != 0) {
		face++;
	}
	
	vec3_t face_point = vec3_mulf(vec3_add(face->tris[0].vertices[0].pos, face->tris[0].vertices[2].pos), 0.5);
	self->position = vec3_add(face_point, vec3_mulf(face->normal, 200));

	self->section_num = section->num;
	self->prev_section_num = section->num;
	self->total_section_num = section->num;

	section_t *next = section->next;
	vec3_t direction = vec3_sub(next->center, section->center);
	self->angle.y = -atan2(direction.x, direction.z);
}

void ship_draw(ship_t *self) {
	// TODO: something more efficient and DRY?
	// Figure out which side of the track the ship is on
	track_face_t *face = track_section_get_base_face(self->section);

	vec3_t to_face_vector = vec3_sub(
		face->tris[0].vertices[0].pos,
		face->tris[0].vertices[1].pos
	);

	vec3_t direction = vec3_sub(self->section->center, self->position);

	if (vec3_dot(direction, to_face_vector) > 0) {
		flags_add(self->flags, SHIP_LEFT_SIDE);
	}
	else {
		flags_rm(self->flags, SHIP_LEFT_SIDE);
		face++;
	}

	object_draw_colored(self->model, &self->mat, face->tris[0].vertices[0].color);
}

void ship_draw_flare(ship_t *self) {	
	// TODO: shimmering?
	//clamp(vec3_len(self->thrust)/1550.0,0,255)

	// rgba_t color = rgba(0,0,255,clamp(((1+sin(system_time()*20.0))/2.0)*vec3_len(self->thrust)/155.0,90,255));
	float thrust_factor = clamp(vec3_len(self->thrust)/20000.0,0,1.0);
	if (self->pilot != g.pilot) {
		thrust_factor = 1.0;
	}
	mat4_t m = mat4_identity();
	mat4_set_yaw_pitch_roll(&m, vec3_add(g.camera.angle, vec3(0,0,M_PI / 2)));
	mat4_set_translation(&m, vec3_sub(vec3_add(self->position, vec3_mulf(self->dir_up, 10)), vec3_mulf(self->dir_forward, 430)));
	render_set_model_mat(&m);
	render_set_depth_write(false);
	render_set_blend_mode(RENDER_BLEND_NORMAL);
	// render_set_depth_offset(-32.0);
	render_set_depth_offset(0.0);

	// DIAMOND
	uint16_t radius = 64;
	for (int i = 0; i < 4; i++) {
		mat4_set_yaw_pitch_roll(
			&m,
			vec3_add(g.camera.angle, vec3(0,0,(M_PI / 2) * i))
		);
		render_set_model_mat(&m);
		// northeast
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,250)
				},
				{
					.pos = {thrust_factor*radius, 0, 0},
					.color = rgba(5,5,255,0)
				},
				{
					.pos = {0, thrust_factor*-radius, 0},
					.color = rgba(5,5,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	render_set_blend_mode(RENDER_BLEND_LIGHTER);

	// STAR
	// long axis
	uint16_t long_length = 180;
	uint16_t half_width = 7;
	uint16_t inner_alpha = 180;
	vec3_t offset = vec3(0, 0, M_PI / 4);
	
	for (int i = 0; i < 2; i++) {
		mat4_set_yaw_pitch_roll(
			&m,
			// vec3_add(g.camera.angle, vec3(0,0,(M_PI / 2) * i))
			vec3_add(
				g.camera.angle,
				vec3_add(offset, vec3(0, 0, M_PI * i))
				)
		);
		render_set_model_mat(&m);
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					.pos = {thrust_factor*long_length, 0, 0},
					.color = rgba(80,80,255,0)
				},
				{
					.pos = {thrust_factor*half_width, thrust_factor*-half_width, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					.pos = {thrust_factor*half_width, thrust_factor*half_width, 0},
					.color = rgba(80,80,255,0)
				},
				{
					.pos = {thrust_factor*long_length, 0, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	// short axis
	uint16_t short_length = 80;
	vec3_t offset_short = vec3(0, 0, (M_PI / 4) * 3);
	
	for (int i = 0; i < 2; i++) {
		mat4_set_yaw_pitch_roll(
			&m,
			// vec3_add(g.camera.angle, vec3(80,80,(M_PI / 2) * i))
			vec3_add(
				g.camera.angle,
				vec3_add(offset_short, vec3(0, 0, M_PI * i))
				)
		);
		render_set_model_mat(&m);
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					.pos = {thrust_factor*short_length, 0, 0},
					.color = rgba(80,80,255,0)
				},
				{
					.pos = {thrust_factor*half_width, thrust_factor*-half_width, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					.pos = {thrust_factor*half_width, thrust_factor*half_width, 0},
					.color = rgba(80,80,255,0)
				},
				{
					.pos = {thrust_factor*short_length, 0, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	// CENTER
	uint16_t c_radius = 25;
	for (int i = 0; i < 4; i++) {
		mat4_set_yaw_pitch_roll(
			&m,
			vec3_add(g.camera.angle, vec3(0,0,(M_PI / 2) * i))
		);
		render_set_model_mat(&m);
		// northeast
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(255,255,255,120)
				},
				{
					.pos = {thrust_factor*c_radius, 0, 0},
					.color = rgba(0,0,255,0)
				},
				{
					.pos = {0, thrust_factor*-c_radius, 0},
					.color = rgba(0,0,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	render_set_depth_offset(0.0);
	render_set_depth_write(true);
	render_set_blend_mode(RENDER_BLEND_NORMAL);
}

void ship_draw_flare_psx(ship_t *self) {	
	// TODO: shimmering?
	//clamp(vec3_len(self->thrust)/1550.0,0,255)

	// rgba_t color = rgba(0,0,255,clamp(((1+sin(system_time()*20.0))/2.0)*vec3_len(self->thrust)/155.0,90,255));
	// float thrust_factor = clamp(vec3_len(self->thrust))/20000.0,0,1.0);
	float thrust_factor = clamp(self->thrust_mag-10,0,500.0)/500.0;
	if (self->pilot != g.pilot || self->lap >= NUM_LAPS || g.is_attract_mode) {
		thrust_factor = 1.0;
	}
	thrust_factor += clamp(self->angle.x*3, -0.8, 0.0);
	mat4_t m = mat4_identity();
	mat4_set_yaw_pitch_roll(&m, vec3_add(g.camera.angle, vec3(0,0,M_PI / 2)));
	mat4_set_translation(&m, vec3_sub(vec3_add(self->position, vec3_mulf(self->dir_up, 10)), vec3_mulf(self->dir_forward, 430)));
	render_set_model_mat(&m);
	render_set_depth_write(false);
	render_set_blend_mode(RENDER_BLEND_NORMAL);
	// render_set_depth_offset(-32.0);
	render_set_depth_offset(0.0);

	// DIAMOND
	uint16_t radius = 65;
	for (int i = 0; i < 4; i++) {
		mat4_set_yaw_pitch_roll(
			&m,
			vec3_add(g.camera.angle, vec3(0,0,(M_PI / 2) * i))
		);
		render_set_model_mat(&m);
		// northeast
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(50,50,255,255)
				},
				{
					.pos = {thrust_factor*radius, 0, 0},
					.color = rgba(5,5,255,0)
				},
				{
					.pos = {0, thrust_factor*-radius, 0},
					.color = rgba(5,5,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	render_set_blend_mode(RENDER_BLEND_LIGHTER);

	// STAR
	// long axis
	uint16_t long_length = 200;
	uint16_t short_length = long_length/2;
	uint16_t half_width = 10;
	uint16_t half_width_tip = 6;
	uint16_t inner_alpha = 128;
	vec3_t offset = vec3(0, 0, M_PI / 4);
	
	for (int i = 0; i < 2; i++) {
		mat4_set_yaw_pitch_roll(
			&m,
			// vec3_add(g.camera.angle, vec3(0,0,(M_PI / 2) * i))
			vec3_add(
				g.camera.angle,
				vec3_add(offset, vec3(0, 0, M_PI * i))
				)
		);
		render_set_model_mat(&m);
		render_push_tris((tris_t) {
			.vertices = {
				{
					// .pos = {0, -thrust_factor*half_width, 0},
					.pos = {0, -half_width, 0},
					.color = rgba(80,80,255,0)
				},
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					// .pos = {thrust_factor*long_length, -thrust_factor*half_width_tip, 0},
					.pos = {thrust_factor*long_length, -half_width_tip, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					.pos = {thrust_factor*long_length, 0, 0},
					.color = rgba(80,80,255,0)
				},
				{
					// .pos = {thrust_factor*long_length, -thrust_factor*half_width_tip, 0},
					.pos = {thrust_factor*long_length, -half_width_tip, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
		// downside
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					// .pos = {0, thrust_factor*half_width, 0},
					.pos = {0, half_width, 0},
					.color = rgba(80,80,255,0)
				},
				{
					// .pos = {thrust_factor*long_length, thrust_factor*half_width_tip, 0},
					.pos = {thrust_factor*long_length, half_width_tip, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					// .pos = {thrust_factor*long_length, thrust_factor*half_width_tip, 0},
					.pos = {thrust_factor*long_length, half_width_tip, 0},
					.color = rgba(80,80,255,0)
				},
				{
					.pos = {thrust_factor*long_length, 0, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	// short axis
	vec3_t offset_short = vec3(0, 0, (M_PI / 4) * 3);
	
	for (int i = 0; i < 2; i++) {
		mat4_set_yaw_pitch_roll(
			&m,
			// vec3_add(g.camera.angle, vec3(80,80,(M_PI / 2) * i))
			vec3_add(
				g.camera.angle,
				vec3_add(offset_short, vec3(0, 0, M_PI * i))
				)
		);
		render_set_model_mat(&m);
		render_push_tris((tris_t) {
			.vertices = {
				{
					// .pos = {0, -thrust_factor*half_width, 0},
					.pos = {0, -half_width, 0},
					.color = rgba(80,80,255,0)
				},
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					// .pos = {thrust_factor*short_length, -thrust_factor*half_width_tip, 0},
					.pos = {thrust_factor*short_length, -half_width_tip, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					.pos = {thrust_factor*short_length, 0, 0},
					.color = rgba(80,80,255,0)
				},
				{
					// .pos = {thrust_factor*short_length, -thrust_factor*half_width_tip, 0},
					.pos = {thrust_factor*short_length, -half_width_tip, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
		// downside
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					// .pos = {0, thrust_factor*half_width, 0},
					.pos = {0, half_width, 0},
					.color = rgba(80,80,255,0)
				},
				{
					// .pos = {thrust_factor*short_length, thrust_factor*half_width_tip, 0},
					.pos = {thrust_factor*short_length, half_width_tip, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(80,80,255,inner_alpha)
				},
				{
					// .pos = {thrust_factor*short_length, thrust_factor*half_width_tip, 0},
					.pos = {thrust_factor*short_length, half_width_tip, 0},
					.color = rgba(80,80,255,0)
				},
				{
					.pos = {thrust_factor*short_length, 0, 0},
					.color = rgba(80,80,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	// CENTER
	uint16_t c_radius = 25;
	for (int i = 0; i < 4; i++) {
		mat4_set_yaw_pitch_roll(
			&m,
			vec3_add(g.camera.angle, vec3(0,0,(M_PI / 2) * i))
		);
		render_set_model_mat(&m);
		// northeast
		render_push_tris((tris_t) {
			.vertices = {
				{
					.pos = {0, 0, 0},
					.color = rgba(255,255,255,100)
				},
				{
					.pos = {thrust_factor*c_radius, 0, 0},
					.color = rgba(0,0,255,0)
				},
				{
					.pos = {0, thrust_factor*-c_radius, 0},
					.color = rgba(0,0,255,0)
				},
			}
		}, RENDER_NO_TEXTURE);
	}

	render_set_depth_offset(0.0);
	render_set_depth_write(true);
	render_set_blend_mode(RENDER_BLEND_NORMAL);
}

void ship_draw_player_trail(ship_t *self) {
	// TODO: shimmering? Check if under the ship and add M_PI offset?
	// float speed_factor = clamp(self->thrust_mag/200.0,0,1.0)*clamp((self->speed-1000)/20000.0,0,1.0);
	float speed_factor = clamp((self->speed-1000)/1000.0,0,1.0);

	mat4_t *m = &mat4_identity();
	mat4_set_yaw_pitch_roll(m, vec3(self->angle.x, self->angle.y - 0.10*self->angle.z, 1.0*self->angle.z));
	mat4_set_translation(m, vec3_sub(vec3_add(self->position, vec3_mulf(self->dir_up, 10)), vec3_mulf(self->dir_forward, 430))); //435
	render_set_model_mat(m);
	render_set_depth_write(false);
	// render_set_blend_mode(RENDER_BLEND_NORMAL);
	render_set_blend_mode(RENDER_BLEND_LIGHTER);
	// render_set_depth_offset(-32.0);
	render_set_depth_offset(0.0);

	uint16_t half_width = 40;
	uint16_t length = 60 + speed_factor * 160; //120
	// uint16_t center_alpha = speed_factor * 250; // + ((sin(system_time()*5)+1)/2.0) * 50;
	// uint16_t outer_rg;
	// bool hl_upper;
	// bool hl_lower;
	// rgba_t border_col = rgba(0,0,255,0);
	// rgba_t hl_col     = rgba(255,255,255,center_alpha*2);
	// rgba_t hl_col     = rgba(255,255,255,clamp(-1.0+speed_factor*5, 0.0, 1.0)*255);
	// rgba_t center_col = rgba(80,80,150,center_alpha);
	
	// if bright spot index == i,	we highlight center vertex upper
	// if bright spot index == i+1, we highlight center vertex lower
	// Is there a smarter way to do this? Definitely.
	// TODO: stop when out of sight
	int loc_0;
	int loc_1;
	int alpha;
	for (int j = 0; j < 2; j++) {
		if (j) {
			mat4_set_yaw_pitch_roll(m, vec3(self->angle.x, self->angle.y, M_PI+self->angle.z));
			render_set_model_mat(m);
		}
		for (int i = 0; i < 10; i++) {
			loc_0 = (NUM_SHIP_TRAIL_POINTS + self->trail_buffer_loc - i) % NUM_SHIP_TRAIL_POINTS;
			loc_1 = (NUM_SHIP_TRAIL_POINTS + self->trail_buffer_loc - i - 1) % NUM_SHIP_TRAIL_POINTS;
			alpha = (1.0 - ((float)i/(NUM_SHIP_TRAIL_POINTS-2))) * 255;

			// vertices
			vertex_t v_left_0 = self->trail_buffer[loc_0].left;
			vertex_t v_center_0 = self->trail_buffer[loc_0].center;
			vertex_t v_right_0 = self->trail_buffer[loc_0].right;
			vertex_t v_left_1 = self->trail_buffer[loc_1].left;
			vertex_t v_center_1 = self->trail_buffer[loc_1].center;
			vertex_t v_right_1 = self->trail_buffer[loc_1].right;
			//
			// hl_upper = (i == random_bright0 || i == random_bright1);
			// hl_lower = (i+1 == random_bright0 || i+1 == random_bright1);
			//
			// if (i == 0 && hl_lower) {
			// 	hl_upper = true;
			// }

			// left side
			render_push_tris((tris_t) {
				.vertices = {
					{
						.pos = {-half_width, 0, -i*length},
						.color = v_left_0.color
					},
					{
						.pos = {-half_width, 0, -length-i*length},
						.color = v_left_1.color
					},
					{
						// center vertex upper
						.pos = {0, 0, -i*length},
						.color = v_center_0.color
					},
				}
			}, RENDER_NO_TEXTURE);
			render_push_tris((tris_t) {
				.vertices = {
					{
						// center vertex upper
						.pos = {0, 0, -i*length},
						.color = v_center_0.color
					},
					{
						.pos = {-half_width, 0, -length-i*length},
						.color = v_left_1.color
					},
					{
						// center vertex lower
						.pos = {0, 0, -length-i*length},
						.color = v_center_1.color
					},
				}
			}, RENDER_NO_TEXTURE);

			// right side
			render_push_tris((tris_t) {
				.vertices = {
					{
						// center vertex upper
						.pos = {0, 0, -i*length},
						.color = v_center_0.color
					},
					{
						// center vertex lower
						.pos = {0, 0, -length-i*length},
						.color = v_center_1.color
					},
					{
						.pos = {half_width, 0, -i*length},
						.color = v_right_0.color
					},
				}
			}, RENDER_NO_TEXTURE);
			render_push_tris((tris_t) {
				.vertices = {
					{
						.pos = {half_width, 0, -i*length},
						.color = v_right_0.color
					},
					{
						// center vertex lower
						.pos = {0, 0, -length-i*length},
						.color = v_center_1.color
					},
					{
						.pos = {half_width, 0, -length-i*length},
						.color = v_right_1.color
					},
				}
			}, RENDER_NO_TEXTURE);
		}
	}

	render_set_depth_offset(0.0);
	render_set_depth_write(true);
	render_set_blend_mode(RENDER_BLEND_NORMAL);
}

void ship_draw_shadow(ship_t *self) {	
	track_face_t *face = track_section_get_base_face(self->section);

	vec3_t face_point = face->tris[0].vertices[0].pos;

	// vec3_t nose = vec3_add(self->position, vec3_mulf(self->dir_forward, 384));
	// vec3_t wngl = vec3_sub(vec3_sub(self->position, vec3_mulf(self->dir_right, 256+i*30)), vec3_mulf(self->dir_forward, 384));
	// vec3_t wngr = vec3_sub(vec3_add(self->position, vec3_mulf(self->dir_right, 256+i*30)), vec3_mulf(self->dir_forward, 384));
	//
	// nose = vec3_sub(nose, vec3_mulf(face->normal, vec3_distance_to_plane(nose, face_point, face->normal)));
	// wngl = vec3_sub(wngl, vec3_mulf(face->normal, vec3_distance_to_plane(wngl, face_point, face->normal)));
	// wngr = vec3_sub(wngr, vec3_mulf(face->normal, vec3_distance_to_plane(wngr, face_point, face->normal)));

	for (int i = 0; i < 7; i++) {
	vec3_t nose = vec3_add(self->position, vec3_mulf(self->dir_forward, 200+i*32));
	vec3_t wngl = vec3_sub(vec3_sub(self->position, vec3_mulf(self->dir_right, 128+i*32)), vec3_mulf(self->dir_forward, 384));
	vec3_t wngr = vec3_sub(vec3_add(self->position, vec3_mulf(self->dir_right, 128+i*32)), vec3_mulf(self->dir_forward, 384));

	nose = vec3_sub(nose, vec3_mulf(face->normal, vec3_distance_to_plane(nose, face_point, face->normal)));
	wngl = vec3_sub(wngl, vec3_mulf(face->normal, vec3_distance_to_plane(wngl, face_point, face->normal)));
	wngr = vec3_sub(wngr, vec3_mulf(face->normal, vec3_distance_to_plane(wngr, face_point, face->normal)));
	
	// rgba_t color = rgba(0 , 0 , 0, 128);
	rgba_t color = rgba(0 , 0 , 0, 8);
	render_push_tris((tris_t) {
		.vertices = {
			{
				.pos = {wngl.x, wngl.y, wngl.z},
				.uv = {0, 256},
				.color = color,
			},
			{
				.pos = {wngr.x, wngr.y, wngr.z},
				.uv = {128, 256},
				.color = color
			},
			{
				.pos = {nose.x, nose.y, nose.z},
				.uv = {64, 0},
				.color = color
			},
		}
	}, self->shadow_texture);
	}
}

void ship_update_unit_vectors(ship_t *self) {
	// Set Unit vectors of this ship
	float sx = sin(self->angle.x);
	float cx = cos(self->angle.x);
	float sy = sin(self->angle.y);
	float cy = cos(self->angle.y);
	float sz = sin(self->angle.z);
	float cz = cos(self->angle.z);

	self->dir_forward.x = -(sy * cx);
	self->dir_forward.y = - sx;
	self->dir_forward.z =  (cy * cx);

	self->dir_right.x =  (cy * cz) + (sy * sz * sx);
	self->dir_right.y = -(sz * cx);
	self->dir_right.z =  (sy * cz) - (cy * sx * sz);

	self->dir_up.x = (cy * sz) - (sy * sx * cz);
	self->dir_up.y = -(cx * cz);
	self->dir_up.z = (sy * sz) + (cy * sx * cz);
}

void ship_draw_trail(ship_t *self) {
	mat4_t *m = &mat4_identity();
	render_set_model_mat(m);
	// mat4_set_yaw_pitch_roll(m, vec3(self->angle.x, self->angle.y, self->angle.z));
	// mat4_set_translation(m, ); //435
	render_set_depth_write(false);
	// render_set_blend_mode(RENDER_BLEND_NORMAL);
	render_set_blend_mode(RENDER_BLEND_LIGHTER);
	// render_set_depth_offset(-32.0);
	render_set_depth_offset(0.0);
	int loc_0;
	int loc_1;
	int alpha;
	float speed_factor = clamp((self->speed-1000)/1000.0,0,1.0);
	// printf("--start trail draw--\n");
	for (int i = 0; i < NUM_SHIP_TRAIL_POINTS - 1; i++) {
		loc_0 = (NUM_SHIP_TRAIL_POINTS + self->trail_buffer_loc - i) % NUM_SHIP_TRAIL_POINTS;
		loc_1 = (NUM_SHIP_TRAIL_POINTS + self->trail_buffer_loc - i - 1) % NUM_SHIP_TRAIL_POINTS;
		alpha = speed_factor*(1.0 - ((float)i/(NUM_SHIP_TRAIL_POINTS-2))) * 255;
		// alpha = 255;
		// printf("alpha: %d\n", alpha);
		// printf("locations: %d, %d\n", loc_0, loc_1);

		// vertices
		vertex_t v_left_0 = self->trail_buffer[loc_0].left;
		vertex_t v_center_0 = self->trail_buffer[loc_0].center;
		vertex_t v_right_0 = self->trail_buffer[loc_0].right;
		vertex_t v_left_1 = self->trail_buffer[loc_1].left;
		vertex_t v_center_1 = self->trail_buffer[loc_1].center;
		vertex_t v_right_1 = self->trail_buffer[loc_1].right;

		// alpha
		v_center_0.color.a = alpha;
		v_center_1.color.a = alpha;
		
		// TODO: stop when out of sight
		// left side
		render_push_tris((tris_t) {
			.vertices = {
				v_left_0,
				v_left_1,
				v_center_0
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				v_center_0,
				v_left_1,
				v_center_1
			}
		}, RENDER_NO_TEXTURE);
		// left side under
		render_push_tris((tris_t) {
			.vertices = {
				v_left_0,
				v_center_0,
				v_left_1
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				v_center_0,
				v_center_1,
				v_left_1
			}
		}, RENDER_NO_TEXTURE);
		// right side
		render_push_tris((tris_t) {
			.vertices = {
				v_center_0,
				v_center_1,
				v_right_0
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				v_right_0,
				v_center_1,
				v_right_1
			}
		}, RENDER_NO_TEXTURE);
		// right side under
		render_push_tris((tris_t) {
			.vertices = {
				v_center_0,
				v_right_0,
				v_center_1
			}
		}, RENDER_NO_TEXTURE);
		render_push_tris((tris_t) {
			.vertices = {
				v_right_0,
				v_right_1,
				v_center_1
			}
		}, RENDER_NO_TEXTURE);
	}
	render_set_depth_offset(0.0);
	render_set_depth_write(true);
	render_set_blend_mode(RENDER_BLEND_NORMAL);
}

void ship_update_trail(ship_t *self) {
	// fine tuning
	uint16_t half_width = 40;
	float speed_factor = clamp((self->speed-1000)/20000.0,0,1.0);

	if (self->trail_last_incremented >= 1.0/30) {
		self->trail_buffer_loc = (self->trail_buffer_loc + 1) % NUM_SHIP_TRAIL_POINTS;
		self->trail_last_incremented = 0;

		// vertex color
		bool random_bright = (rand_int(0, 8) == 0);
		self->trail_buffer[self->trail_buffer_loc].left.color = (random_bright) ? rgba(255,255,255,speed_factor*0) : rgba(0,0,255,0);
		self->trail_buffer[self->trail_buffer_loc].center.color = (random_bright) ? rgba(255,255,255,speed_factor*250) : rgba(80,80,255,speed_factor*150);
		self->trail_buffer[self->trail_buffer_loc].right.color = (random_bright) ? rgba(255,255,255,speed_factor*0) : rgba(0,0,255,0);
	} else {
		self->trail_last_incremented += system_tick();
	}

	mat4_t *m = &mat4_identity();
	mat4_set_yaw_pitch_roll(m, vec3(self->angle.x, self->angle.y, self->angle.z));
	mat4_set_translation(m, vec3_sub(vec3_add(self->position, vec3_mulf(self->dir_up, 10)), vec3_mulf(self->dir_forward, 430))); //435
	
	vec3_t left = vec3_transform(vec3(-half_width, 0, 0), m);
	vec3_t center = vec3_transform(vec3(0, 0, 0), m);
	vec3_t right = vec3_transform(vec3(half_width, 0, 0), m);

	// printf("trail buffer loc left pos xyz: %f, %f. %f\n", left.x, left.y, left.z);
	// printf("trail buffer loc center pos xyz: %f, %f. %f\n", center.x, center.y, center.z);
	// printf("trail buffer loc right pos xyz: %f, %f. %f\n", right.x, right.y, right.z);

	// vertex position
	self->trail_buffer[self->trail_buffer_loc].left.pos = left;
	self->trail_buffer[self->trail_buffer_loc].center.pos = center;
	self->trail_buffer[self->trail_buffer_loc].right.pos = right;

	// printf("trail buffer loc: %d\n", self->trail_buffer_loc);
}

void ship_update(ship_t *self) {
	// Set Unit vectors of this ship
	float sx = sin(self->angle.x);
	float cx = cos(self->angle.x);
	float sy = sin(self->angle.y);
	float cy = cos(self->angle.y);
	float sz = sin(self->angle.z);
	float cz = cos(self->angle.z);

	self->dir_forward.x = -(sy * cx);
	self->dir_forward.y = - sx;
	self->dir_forward.z =  (cy * cx);

	self->dir_right.x =  (cy * cz) + (sy * sz * sx);
	self->dir_right.y = -(sz * cx);
	self->dir_right.z =  (sy * cz) - (cy * sx * sz);

	self->dir_up.x = (cy * sz) - (sy * sx * cz);
	self->dir_up.y = -(cx * cz);
	self->dir_up.z = (sy * sz) + (cy * sx * cz);

	self->prev_section = self->section;

	// To find the nearest section to the ship, the original source de-emphasizes
	// the .y component when calculating the distance to each section by a 
	// >> 2 shift. I.e. it tries to find the section that is more closely to the
	// horizontal x,z plane (directly underneath the ship), instead of finding 
	// the section with the "real" closest distance. Hence the bias of 
	// vec3(1, 0.25, 1) here.
	float distance;
	self->section = track_nearest_section(self->position, vec3(1, 0.25, 1), self->section, &distance);
	// What is this magic number?
	if (distance > 3700) {
		flags_add(self->flags, SHIP_FLYING);
	}
	else {
		flags_rm(self->flags, SHIP_FLYING);
	}

	self->prev_section_num = self->prev_section->num;
	self->section_num = self->section->num;

	// Figure out which side of the track the ship is on
	track_face_t *face = track_section_get_base_face(self->section);

	vec3_t to_face_vector = vec3_sub(
		face->tris[0].vertices[0].pos,
		face->tris[0].vertices[1].pos
	);

	vec3_t direction = vec3_sub(self->section->center, self->position);

	if (vec3_dot(direction, to_face_vector) > 0) {
		flags_add(self->flags, SHIP_LEFT_SIDE);
	}
	else {
		flags_rm(self->flags, SHIP_LEFT_SIDE);
		face++;
	}
	
	// if (self->pilot == g.pilot) {
	// 	printf("ship current face color: %d, %d, %d, %d\n",
	// 		face->tris[0].vertices[0].color.r,
	// 		face->tris[0].vertices[0].color.g,
	// 		face->tris[0].vertices[0].color.b,
	// 		face->tris[0].vertices[0].color.a
	// 		);
	// }
	// if (self->pilot == g.pilot) {
	// 	printf("ship current face flags: %d\n", face->flags);
	// 	printf("ship current section flags: %d\n", g.ships[g.pilot].section->flags);
	// 	printf("ship current section num: %d\n", g.ships[g.pilot].section->num);
	// }

	// Collect powerup
	if (
		g.race_type != RACE_TYPE_TIME_TRIAL &&
		flags_is(face->flags, FACE_PICKUP_ACTIVE) &&
		flags_not(self->flags, SHIP_SPECIALED) &&
		self->weapon_type == WEAPON_TYPE_NONE &&
		track_collect_pickups(face)
	) {
		if (self->pilot == g.pilot) {
			sfx_play(SFX_POWERUP);
			input_rumble(0.0, 1.0, 64);
			if (flags_is(self->flags, SHIP_SHIELDED)) {
				self->weapon_type = weapon_get_random_type(WEAPON_CLASS_PROJECTILE);
			}
			else {
				self->weapon_type = weapon_get_random_type(WEAPON_CLASS_ANY);
			}
		}
		else {
			self->weapon_type = 1;
		}
	}

	// Pit stop
	if (flags_is(face->flags, FACE_PIT_STOP) && face->flags>0) {
	// TODO: using FACE_PIT_STOP messes up checkpoints
	// if (flags_is(face->flags, 321) || flags_is(face->flags, 325)) {
		flags_add(self->flags, SHIP_ON_PIT_STOP);
	} else {
		flags_rm(self->flags, SHIP_ON_PIT_STOP);
	}

	self->last_impact_time += system_tick();
	
	// Call the active player/ai update function
	(self->update_func)(self);

	mat4_set_translation(&self->mat, self->position);
	mat4_set_yaw_pitch_roll(&self->mat, self->angle);

	// Race position and lap times
	self->lap_time += system_tick();

	int start_line_pos = def.circuts[g.circut].settings[g.race_class].start_line_pos;

	// Crossed line backwards
	if (self->prev_section_num == start_line_pos + 1 && self->section_num <= start_line_pos) {
		self->lap--;
	}

	// Crossed line forwards
	else if (self->prev_section_num == start_line_pos && self->section_num > start_line_pos) {
		self->lap++;

		// Is it the first time we're crossing the line for this lap?
		if (self->lap > self->max_lap) {
			self->max_lap = self->lap;

			if (self->lap > 0 && self->lap <= NUM_LAPS) {
				g.lap_times[self->pilot][self->lap-1] = self->lap_time;
			}
			self->lap_time = 0;

			if (g.race_type == RACE_TYPE_TIME_TRIAL) {
				self->weapon_type = WEAPON_TYPE_TURBO;
			}

			if (self->lap == NUM_LAPS && self->pilot == g.pilot) {
				race_end();
			}
		}
	}

	int section_num_from_line = self->section_num - (start_line_pos + 1);
	if (section_num_from_line < 0) {
		section_num_from_line += g.track.section_count;
	}
	self->total_section_num = self->lap * g.track.section_count + section_num_from_line;

	ship_update_trail(self);
}

vec3_t ship_cockpit(ship_t *self) {
	return vec3_add(self->position, vec3_mulf(self->dir_up, 128));
}

vec3_t ship_nose(ship_t *self) {
	return vec3_add(self->position, vec3_mulf(self->dir_forward, 512));
}

vec3_t ship_wing_left(ship_t *self) {
	return vec3_sub(vec3_sub(self->position, vec3_mulf(self->dir_right, 256)), vec3_mulf(self->dir_forward, 256));
}

vec3_t ship_wing_right(ship_t *self) {
	return vec3_sub(vec3_add(self->position, vec3_mulf(self->dir_right, 256)), vec3_mulf(self->dir_forward, 256));
}

static bool vec3_is_on_face(vec3_t pos, track_face_t *face, float alpha) {
	vec3_t plane_point = vec3_sub(pos, vec3_mulf(face->normal, alpha));
	vec3_t vec0 = vec3_sub(plane_point, face->tris[0].vertices[1].pos);
	vec3_t vec1 = vec3_sub(plane_point, face->tris[0].vertices[2].pos);
	vec3_t vec2 = vec3_sub(plane_point, face->tris[0].vertices[0].pos);
	vec3_t vec3 = vec3_sub(plane_point, face->tris[1].vertices[0].pos);

	float angle = 
		vec3_angle(vec0, vec2) +
		vec3_angle(vec2, vec3) +
		vec3_angle(vec3, vec1) +
		vec3_angle(vec1, vec0);

	// return (angle > M_PI * 2 - 0.01);
	return (angle > (0.91552734375 * M_PI * 2));
}

void ship_resolve_wing_collision(ship_t *self, track_face_t *face, float direction) {
	// original behaviour:
	// no yaw, no roll when minimal, speed scales roll (angle too?), push up and towards center always
	// TODO: how much is the orginal slowdown and what scales it
	vec3_t collision_vector = vec3_sub(self->section->center, face->tris[0].vertices[2].pos);
	// float collision_dot = vec3_dot(vec3_normalize(self->velocity), vec3_normalize(collision_vector));
	float collision_dot = vec3_dot(self->dir_forward, vec3_normalize(collision_vector));
	vec3_t to_center = vec3_normalize(vec3_sub(self->section->center, self->position));
	float angle = vec3_angle(collision_vector, self->dir_forward);

	self->velocity = vec3_mulf(self->velocity, 0.95);
	// self->thrust_mag *= 0.95;
	self->position = vec3_add(self->position, vec3_mulf(to_center, fabs(collision_dot) * 32));
	self->velocity = vec3_add(self->velocity, vec3_mulf(to_center, fabs(collision_dot) * 2048));
	// printf("c angle: %f\n", angle);
	// printf("c dot  : %f\n", collision_dot);

	// float magnitude = (fabsf(angle) * self->speed) * M_PI / (4096 * 16.0); // (6 velocity shift, 12 angle shift?)
	float magnitude = (fabsf(angle) * self->speed) * M_PI / 4096.0; // orig was too weak, but jnmartin84 added too much?

	// wing pos (and possible roll)
	vec3_t wing_pos;
	if (direction > 0) {
		// self->angular_velocity.z += magnitude;
		wing_pos = vec3_add(self->position, vec3_mulf(vec3_sub(self->dir_right, self->dir_forward), 256)); // >> 4??
	}
	else {
		// self->angular_velocity.z -= magnitude;	
		wing_pos = vec3_sub(self->position, vec3_mulf(vec3_sub(self->dir_right, self->dir_forward), 256)); // >> 4??
	}

	// wing yaw
	if (direction > 0) {
		self->angular_velocity.y += 0.1;
	}
	else {
		self->angular_velocity.y -= 0.1;	
	}

	if (self->last_impact_time > 0.2) {
		self->last_impact_time = 0;
		sfx_t *sfx = sfx_play_at(SFX_IMPACT, wing_pos, vec3(0, 0, 0), 0.1); // volume 1, TODO: scale with trauma?
		// sfx->pitch = 0.25;
		sfx->volume = 0.3;
		if (self->pilot == g.pilot) {
			input_rumble(0.0, 1.0, 128);
			camera_add_shake(&g.camera, 0.15);
		}
	}
}

void ship_resolve_nose_collision(ship_t *self, track_face_t *face, float direction) {
	// original behaviour:
	// push up and towards center (and backwards?), speed scales push up and yaw

	vec3_t to_center = vec3_normalize(vec3_sub(self->section->center, self->position));
	vec3_t collision_vector = vec3_sub(self->section->center, face->tris[0].vertices[2].pos);
	float angle = vec3_angle(collision_vector, self->dir_forward);
	self->velocity = vec3_sub(self->velocity, vec3_mulf(self->velocity, 0.5));
	self->thrust_mag *= 0.6;
	self->velocity = vec3_reflect(self->velocity, face->normal, 2);
	// self->position = vec3_sub(self->position, vec3_mulf(self->velocity, 0.015625)); // system_tick?
	self->position = vec3_sub(self->position, vec3_mulf(self->velocity, system_tick())); // system_tick?
	self->velocity = vec3_add(self->velocity, vec3_mulf(face->normal, 4096)); // div by 4096?
	self->velocity = vec3_add(self->velocity, vec3_mulf(to_center, 256));

	// float magnitude = (fabsf(angle) * self->speed) * 2 * M_PI / (4096.0); // was too weak
	// if (direction > 0) {
	// 	self->angular_velocity.y += magnitude;
	// }
	// else { 
	// 	self->angular_velocity.y -= magnitude;
	// }

	if (self->last_impact_time > 0.2) {
		self->last_impact_time = 0;
		sfx_t *sfx = sfx_play_at(SFX_IMPACT, ship_nose(self), vec3(0, 0, 0), 1.0); //1
		// sfx->pitch = 0.5;
		sfx->volume = 0.3;
		if (self->pilot == g.pilot) {
			input_rumble(1.0, 1.0, 128);
			camera_add_shake(&g.camera, 0.3);
		}
	}
}

void ship_collide_with_track(ship_t *self, track_face_t *face) {
	float alpha;
	section_t 	*trackPtr;
	bool collide;
	track_face_t *face2;

	trackPtr = self->section->next;
	vec3_t direction = vec3_sub(trackPtr->center, self->section->center);
	float down_track = vec3_dot(direction, self->dir_forward);

	if (down_track < 0) {
		flags_rm(self->flags, SHIP_DIRECTION_FORWARD);
	}
	else {
		flags_add(self->flags, SHIP_DIRECTION_FORWARD);
	}

	vec3_t to_face_vector = vec3_sub(face->tris[0].vertices[0].pos, face->tris[0].vertices[1].pos);
	direction = vec3_sub(self->section->center, self->position);
	float to_face = vec3_dot(direction, to_face_vector);

	face--;

	// Check against left hand side of track
	
	// FIXME: the collision checks in junctions are very flakey and often select
	// the wrong face to test for a collision.
	// Instead of this whole mess here, there should just be a function 
	// `track_get_nearest_face(section, pos)` that we call with the nose and 
	// wing positions and then just resolve against this face.

	if (to_face > 0) {
		flags_add(self->flags, SHIP_LEFT_SIDE);
		
		vec3_t face_point = face->tris[0].vertices[0].pos;

		alpha = vec3_distance_to_plane(ship_nose(self), face_point, face->normal);
		if (alpha <= 0) {
			if (flags_is(self->section->flags, SECTION_JUNCTION_START)) {
				collide = vec3_is_on_face(ship_nose(self), face, alpha);
				if (collide) {
					ship_resolve_nose_collision(self, face, -down_track);
				}
				else {
					face2 = g.track.faces + self->section->next->face_start;
					collide = vec3_is_on_face(ship_nose(self), face2, alpha);
					if (collide) {
						ship_resolve_nose_collision(self, face, -down_track);
					}
				}
			}
			else if (flags_is(self->section->flags, SECTION_JUNCTION_END)) {
				collide = vec3_is_on_face(ship_nose(self), face, alpha);
				if (collide) {
					ship_resolve_nose_collision(self, face, -down_track);
				}
				else {
					face2 = g.track.faces + self->section->prev->face_start;
					collide = vec3_is_on_face(ship_nose(self), face2, alpha);
					if (collide) {
						ship_resolve_nose_collision(self, face, -down_track);
					}
				}
			}
			else {
				ship_resolve_nose_collision(self, face, -down_track);
			}
			return;
		}

		alpha = vec3_distance_to_plane(ship_wing_left(self), face_point, face->normal);
		if (alpha <= 0) {
			if (
				flags_is(self->section->flags, SECTION_JUNCTION_START) || 
				flags_is(self->section->flags, SECTION_JUNCTION_END)
			) {
				collide = vec3_is_on_face(ship_wing_left(self), face, alpha);
				if (collide) {
					ship_resolve_nose_collision(self, face, -down_track);
				}
			}
			else {
				ship_resolve_wing_collision(self, face, -down_track);
			}
			return;
		}

		alpha = vec3_distance_to_plane(ship_wing_right(self), face_point, face->normal);
		if (alpha <= 0) {
			if (
				flags_is(self->section->flags, SECTION_JUNCTION_START) || 
				flags_is(self->section->flags, SECTION_JUNCTION_END)
			) {
				collide = vec3_is_on_face(ship_wing_right(self), face, alpha);
				if (collide) {
					ship_resolve_nose_collision(self, face, -down_track);
				}
			}
			else {
				ship_resolve_wing_collision(self, face, -down_track);
			}
			return;
		}
	}


	// Collision check against 2nd wall
	else {
		flags_rm(self->flags, SHIP_LEFT_SIDE);

		face++;
		while (face->flags & FACE_TRACK_BASE) {
			face++;
		}

		vec3_t face_point = face->tris[0].vertices[0].pos;

		alpha = vec3_distance_to_plane(ship_nose(self), face_point, face->normal);
		if (alpha <= 0) {
			if (flags_is(self->section->flags, SECTION_JUNCTION_START)) {
				collide = vec3_is_on_face(ship_nose(self), face, alpha);
				if (collide) {
					ship_resolve_nose_collision(self, face, down_track);
				}
				else {
					face2 = g.track.faces + self->section->next->face_start;
					face2 += 3;
					collide = vec3_is_on_face(ship_nose(self), face2, alpha);
					if (collide) {
						ship_resolve_nose_collision(self, face, -down_track);
					}
				}
			}
			else if (flags_is(self->section->flags, SECTION_JUNCTION_END)) {
				collide = vec3_is_on_face(ship_nose(self), face, alpha);
				if (collide) {
					ship_resolve_nose_collision(self, face, -down_track);
				}
				else {
					face2 = g.track.faces + self->section->prev->face_start;
					face2 += 3;
					collide = vec3_is_on_face(ship_nose(self), face2, alpha);
					if (collide) {
						ship_resolve_nose_collision(self, face2, -down_track);
					}
				}
			}
			else {
				ship_resolve_nose_collision(self, face, down_track);
			}
			return;
		}
		
		alpha = vec3_distance_to_plane(ship_wing_left(self), face_point, face->normal);
		if (alpha <= 0) {
			if (
				flags_is(self->section->flags, SECTION_JUNCTION_START) ||
				flags_is(self->section->flags, SECTION_JUNCTION_END)
			) {
				collide = vec3_is_on_face(ship_wing_left(self), face, alpha);
				if (collide) {
					ship_resolve_nose_collision(self, face, down_track);
				}
			}
			else {
				ship_resolve_wing_collision(self, face, down_track);
			}
			return;
		}

		alpha = vec3_distance_to_plane(ship_wing_right(self), face_point, face->normal);
		if (alpha <= 0) {
			if (
				flags_is(self->section->flags, SECTION_JUNCTION_START) ||
				flags_is(self->section->flags, SECTION_JUNCTION_END)
			) {
				collide = vec3_is_on_face(ship_wing_right(self), face, alpha);
				if (collide) {
					ship_resolve_nose_collision(self, face, down_track);
				}
			}
			else {
				ship_resolve_wing_collision(self, face, down_track);
			}
			return;
		}
	}
}

bool ship_intersects_ship(ship_t *self, ship_t *other) {
	// Get 4 points of collision model relative to the
	// camera
	vec3_t a = vec3_transform(other->collision_model->vertices[0], &other->mat);
	vec3_t b = vec3_transform(other->collision_model->vertices[1], &other->mat);
	vec3_t c = vec3_transform(other->collision_model->vertices[2], &other->mat);
	vec3_t d = vec3_transform(other->collision_model->vertices[3], &other->mat);

	vec3_t other_points[6] = {b, a, d, a, a, b};
	vec3_t other_lines[6] = {
		vec3_sub(c, b),
		vec3_sub(c, a),
		vec3_sub(c, d),
		vec3_sub(b, a),
		vec3_sub(d, a),
		vec3_sub(d, b)
	};

	Prm poly = {.primitive = other->collision_model->primitives};
	int primitives_len = other->collision_model->primitives_len;

	vec3_t p1, p2, p3;

	// for all 4 planes of the enemy ship
	for (int pi = 0; pi < primitives_len; pi++) {
		int16_t *indices;
		switch (poly.primitive->type) {
			case PRM_TYPE_F3:
				indices = poly.f3->coords;
				p1 =  vec3_transform(self->collision_model->vertices[indices[0]], &self->mat);
				p2 =  vec3_transform(self->collision_model->vertices[indices[1]], &self->mat);
				p3 =  vec3_transform(self->collision_model->vertices[indices[2]], &self->mat);
				poly.f3++;
				break;
			case PRM_TYPE_G3:
				indices = poly.g3->coords;
				p1 =  vec3_transform(self->collision_model->vertices[indices[0]], &self->mat);
				p2 =  vec3_transform(self->collision_model->vertices[indices[1]], &self->mat);
				p3 =  vec3_transform(self->collision_model->vertices[indices[2]], &self->mat);
				poly.g3++;
				break;
			case PRM_TYPE_FT3:
				indices = poly.ft3->coords;
				p1 =  vec3_transform(self->collision_model->vertices[indices[0]], &self->mat);
				p2 =  vec3_transform(self->collision_model->vertices[indices[1]], &self->mat);
				p3 =  vec3_transform(self->collision_model->vertices[indices[2]], &self->mat);
				poly.ft3++;
				break;
			case PRM_TYPE_GT3:
				indices = poly.gt3->coords;
				p1 =  vec3_transform(self->collision_model->vertices[indices[0]], &self->mat);
				p2 =  vec3_transform(self->collision_model->vertices[indices[1]], &self->mat);
				p3 =  vec3_transform(self->collision_model->vertices[indices[2]], &self->mat);
				poly.gt3++;
				break;
			default:
				break;
		}

		// Find polyGon line vectors
		vec3_t p1p2 = vec3_sub(p2, p1);
		vec3_t p1p3 = vec3_sub(p3, p1);

		// Find plane equations
		vec3_t plane1 = vec3_cross(p1p2, p1p3);

		for (int vi = 0; vi < 6; vi++) {
			float dp1 = vec3_dot(vec3_sub(p1, other_points[vi]), plane1);
			float dp2 = vec3_dot(other_lines[vi], plane1);
			
			if (dp2 != 0) {
				float norm = dp1 / dp2;

				if ((norm >= 0) && (norm <= 1)) {
					vec3_t term = vec3_mulf(other_lines[vi], norm);
					vec3_t res = vec3_add(term, other_points[vi]);

					vec3_t v0 = vec3_sub(p1, res);
					vec3_t v1 = vec3_sub(p2, res);
					vec3_t v2 = vec3_sub(p3, res);
					
					float angle =
						vec3_angle(v0, v1) +
						vec3_angle(v1, v2) +
						vec3_angle(v2, v0);

					if ((angle >= M_PI * 2 - M_PI * 0.1)) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

void ship_collide_with_ship(ship_t *self, ship_t *other) {
	float distance = vec3_len(vec3_sub(self->position, other->position));
	
	// Do a quick distance check; if ships are far apart, remove the collision flag
	// and early out.
	if (distance > 960) {
		flags_rm(self->flags, SHIP_COLL);
		flags_rm(other->flags, SHIP_COLL);
		return;
	}

	// Ships are close, do a real collision test
	if (!ship_intersects_ship(self, other)) {
		return;
	}

	// Ships did collide, resolve
	vec3_t vc = vec3_divf(
		vec3_add(
			vec3_mulf(self->velocity, self->mass),
			vec3_mulf(other->velocity, other->mass)
		),
		self->mass + other->mass
	);

	vec3_t ship_react = vec3_mulf(vec3_sub(vc, self->velocity), 0.5);
	vec3_t other_react = vec3_mulf(vec3_sub(vc, other->velocity), 0.5);
	self->position = vec3_sub(self->position, vec3_mulf(self->velocity, 0.015625)); // >> 6
	other->position = vec3_sub(other->position, vec3_mulf(other->velocity, 0.015625)); // >> 6
	// self->position = vec3_sub(self->position, vec3_mulf(self->velocity, system_tick()));
	// other->position = vec3_sub(other->position, vec3_mulf(other->velocity, system_tick()));

	self->velocity = vec3_add(vc, ship_react);
	other->velocity = vec3_add(vc, other_react);

	vec3_t res = vec3_sub(self->position, other->position);

	self->velocity = vec3_add(self->velocity, vec3_mulf(res, 2));  // << 2 // new wo1 is 4???
	// self->position = vec3_add(self->position, vec3_mulf(self->velocity, 0.015625)); // >> 6
	self->position = vec3_add(self->position, vec3_mulf(self->velocity, system_tick())); // >> 6

	other->velocity = vec3_sub(other->velocity, vec3_mulf(res, 2)); // << 2 // new wo1 is 4???
	// other->position = vec3_add(other->position, vec3_mulf(other->velocity, 0.015625)); // >> 6
	other->position = vec3_add(other->position, vec3_mulf(other->velocity, system_tick())); // >> 6

	if (
		flags_not(self->flags, SHIP_COLL) && 
		flags_not(other->flags, SHIP_COLL) &&
		self->last_impact_time > 0.2
	) {
		self->last_impact_time = 0;
		vec3_t sound_pos = vec3_mulf(vec3_add(self->position, other->position), 0.5);
		sfx_play_at(SFX_CRUNCH, sound_pos, vec3(0, 0, 0), 1);
	}
	flags_add(self->flags, SHIP_COLL);
	flags_add(other->flags, SHIP_COLL);
}
