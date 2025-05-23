#include "../mem.h"
#include "../utils.h"
#include "../system.h"

#include "object.h"
#include "track.h"
#include "ship.h"
#include "weapon.h"
#include "scene.h"
#include "droid.h"
#include "camera.h"
#include "object.h"
#include "game.h"

#define SCENE_START_BOOMS_MAX 4
#define SCENE_OIL_PUMPS_MAX 2
#define SCENE_RED_LIGHTS_MAX 4
#define SCENE_STANDS_MAX 20
#define SCENE_CAMERAS_MAX 40
#define SCENE_FANS_MAX 20

static Object *scene_objects;

static Object *sky_object;
static vec3_t sky_offset_initial;
static vec3_t sky_offset;

// TODO:
//
// Valparaiso
// wfall (-79330.000000, -8704.000000, 18960.000000)
//
// Phenitia Park
// radar
//
// Talon's Reach
// leaky pipes

static Object *start_booms[SCENE_START_BOOMS_MAX];
static int start_booms_len;

// static Object *oil_pumps[SCENE_OIL_PUMPS_MAX];
// static int oil_pumps_len;

static Object *red_lights[SCENE_RED_LIGHTS_MAX];
static int red_lights_len;

static Object *cameras[SCENE_CAMERAS_MAX];
static int cameras_len;

static Object *fans[SCENE_FANS_MAX];
static int fans_len;

static Object *train;
static vec3_t train_origin_initial;
static vec3_t train_origin_final;
static float train_lerp_factor;
static bool train_triggered;

// stand means spectators/crowd
typedef struct {
	sfx_t *sfx;
	vec3_t pos;
} scene_stand_t;
static scene_stand_t stands[SCENE_STANDS_MAX];
static int stands_len;

static struct {
	bool enabled;
	GT4	*primitives[80];
	int16_t *coords[80];
	int16_t grey_coords[80];	
} aurora_borealis;

static struct {
	bool triggered;
	Object *obj;
	vec3_t offset_initial;
	vec3_t offset;
} zeppelin;

void scene_load(const char *base_path, float sky_y_offset) {
	texture_list_t scene_textures = image_get_compressed_textures(get_path(base_path, "scene.cmp"));
	scene_objects = objects_load(get_path(base_path, "scene.prm"), scene_textures);
	
	texture_list_t sky_textures = image_get_compressed_textures(get_path(base_path, "sky.cmp"));
	sky_object = objects_load(get_path(base_path, "sky.prm") , sky_textures);
	sky_offset_initial = vec3(0, sky_y_offset, 0);
	sky_offset = sky_offset_initial;

	if (g.circut == CIRCUT_GARE_D_EUROPA) {
		train = objects_load("wipeout2/common/train.prm", image_get_compressed_textures("wipeout2/common/train.cmp"));
		train_origin_initial = train->origin;
		// train_origin_final = vec3(train->origin.x, train->origin.y, train->origin.z + 1000);
		// train_origin_final = vec3(48500.0, 3000.0, -100000.0);
		// train_origin_final = vec3(61072.0, 102.0, -72262.0);
		train_origin_final = vec3(61180.0, 40.0, -72262.0);
		train_lerp_factor = 0;
		train_triggered = false;
		printf("load train: %s (%f, %f, %f)\n", train->name, train->origin.x, train->origin.y, train->origin.z);
	}

	// Collect all objects that need to be updated each frame
	start_booms_len = 0;
	// oil_pumps_len = 0;
	cameras_len = 0;
	red_lights_len = 0;
	stands_len = 0;
	fans_len = 0;

	Object *obj = scene_objects;
	while (obj) {
		printf("load obj: %s (%f, %f, %f)\n", obj->name, obj->origin.x, obj->origin.y, obj->origin.z);
		mat4_set_translation(&obj->mat, obj->origin);

		if (str_starts_with(obj->name, "start")) {
			error_if(start_booms_len >= SCENE_START_BOOMS_MAX, "SCENE_START_BOOMS_MAX reached");
			start_booms[start_booms_len++] = obj;
		}
		else if (str_starts_with(obj->name, "redl")) {
			error_if(red_lights_len >= SCENE_RED_LIGHTS_MAX, "SCENE_RED_LIGHTS_MAX reached");
			red_lights[red_lights_len++] = obj;
		}
		// else if (str_starts_with(obj->name, "donkey")) {
		// 	error_if(oil_pumps_len >= SCENE_OIL_PUMPS_MAX, "SCENE_OIL_PUMPS_MAX reached");
		// 	oil_pumps[oil_pumps_len++] = obj;
		// }
		else if (
			str_starts_with(obj->name, "lostad") || 
			str_starts_with(obj->name, "stad_") ||
			str_starts_with(obj->name, "newstad_")
		) {
			error_if(stands_len >= SCENE_STANDS_MAX, "SCENE_STANDS_MAX reached");
			stands[stands_len++] = (scene_stand_t){.sfx = NULL, .pos = obj->origin};
		}
		else if (str_starts_with(obj->name, "camera")) {
			error_if(cameras_len >= SCENE_CAMERAS_MAX, "SCENE_CAMERAS_MAX reached");
			cameras[cameras_len++] = obj;
		}
		else if (str_starts_with(obj->name, "fan")) {
			error_if(cameras_len >= SCENE_CAMERAS_MAX, "SCENE_FANS_MAX reached");
			fans[fans_len++] = obj;
		}
		else if (str_starts_with(obj->name, "zeppelin")) {
			zeppelin.triggered = false;
			zeppelin.obj = obj;
			zeppelin.offset_initial = vec3_sub(obj->origin, vec3(0, 0, 4096));
			zeppelin.offset = zeppelin.offset_initial;
		}

		obj = obj->next;
	}


	aurora_borealis.enabled = false;
}

void scene_init(void) {
	scene_set_start_booms(0);
	for (int i = 0; i < stands_len; i++) {
		stands[i].sfx = sfx_reserve_loop(SFX_CROWD);
	}
}

void scene_update(void) {
	for (int i = 0; i < red_lights_len; i++) {
		scene_pulsate_red_light(red_lights[i]);
	}
	// for (int i = 0; i < oil_pumps_len; i++) {
	// 	scene_move_oil_pump(oil_pumps[i]);
	// }
	for (int i = 0; i < cameras_len; i++) {
		scene_move_cameras(cameras[i]);
	}
	for (int i = 0; i < stands_len; i++) {
		sfx_set_position(stands[i].sfx, stands[i].pos, vec3(0, 0, 0), 0.4);
	}

	if (aurora_borealis.enabled) {
		scene_update_aurora_borealis();
	}
	if (g.circut == CIRCUT_ODESSA_KEYS) {
		// highest point is about y:-22000
		float sky_swing_factor = clamp(-g.ships[g.pilot].position.y/22000, 0.0, 1.0);
		sky_offset = vec3(
			sky_offset_initial.x,
			sky_offset_initial.y - sin(system_cycle_time() * 0.5 * M_PI * 2) * sky_swing_factor * 1500,
			sky_offset_initial.z
		);
	}
	else if (g.circut == CIRCUT_TALONS_REACH) {
		for (int i = 0; i < fans_len; i++) {
			// fan origins:
			// starting line fan (-40062.000000, -5076.000000, -4510.000000)
			// pit stop fan (-60544.000000, -2576.000000, -23654.000000)
			if (fans[i]->origin.y == -2576.0) {
				// this fixes the pit stop fan orientation
				mat4_set_yaw_pitch_roll(&fans[i]->mat, vec3(0, 0.2, -system_time() * M_PI * 2.0 * 0.5));
			} else {
				mat4_set_yaw_pitch_roll(&fans[i]->mat, vec3(0, 0, -system_time() * M_PI * 2.0 * 0.5));
			}
		}
	}
	else if (g.circut == CIRCUT_GARE_D_EUROPA) {
		zeppelin.triggered = (g.ships[g.pilot].section->num > 100);

		if (zeppelin.triggered) {
			if (zeppelin.offset.z < 500000) {
				zeppelin.offset = vec3_add(zeppelin.offset, vec3(0, 0, 4096.0 * system_tick()));
			}
			mat4_set_translation(&zeppelin.obj->mat, zeppelin.offset);
		} else {
			zeppelin.offset = zeppelin.offset_initial;
			mat4_set_translation(&zeppelin.obj->mat, zeppelin.offset_initial);
		}

		train_triggered = (g.ships[g.pilot].section->num > 165);

		int train_direction = abs(g.ships[g.pilot].lap % 2);

		if (train_triggered) {
			train_lerp_factor -= 0.10 * system_tick() * ((train_direction*2)-1);
			train_lerp_factor = clamp(train_lerp_factor, 0.0, 1.0);
		} else {
			train_lerp_factor = train_direction;
		}

		// g.camera.position = vec3_lerp(train_origin_initial, train_origin_final, 0.8 ); //testing
		// printf("cam pos: %f, %f, %f\n", g.camera.position.x,g.camera.position.y,g.camera.position.z);
	}
}

void scene_draw(camera_t *camera) {
	vec3_t cam_pos = camera->position;
	vec3_t cam_dir = camera_forward(camera);

	// Sky
	render_set_depth_write(false);
	mat4_set_translation(&sky_object->mat, vec3_add(camera->position, sky_offset));
	object_draw(sky_object, &sky_object->mat);
	render_set_depth_write(true);

	// Train
	// TODO: train is hacky
	// there's the origin which is on the track
	// and there's an eye balled second reference point
	// which is sort of on the track
	//
	// smoothstep if it's a metro station?
	// but cooler if the metro just passes by?
	if (train && train_triggered && train_lerp_factor) {
		float train_length = 0.065;
		for (int i = 0; i < 8; i++) {
			// vec3_t train_car_pos = vec3_lerp(train_origin_initial, train_origin_final, -0.1 - i * train_length + smoothstep(0.0, 1.0, train_lerp_factor) * 1.6 );
			vec3_t train_car_pos = vec3_lerp(train_origin_initial, train_origin_final, -0.1 - i * train_length + train_lerp_factor * 1.8 );
			vec3_t diff = vec3_sub(cam_pos, train_car_pos);
			float cam_dot = vec3_dot(diff, cam_dir);
			float dist_sq = vec3_dot(diff, diff);
			if (
				cam_dot < train->radius && 
				dist_sq < (RENDER_FADEOUT_FAR * RENDER_FADEOUT_FAR)
			) {
				mat4_set_translation(&train->mat, train_car_pos);
				object_draw(train, &train->mat);
			}
		}
	}

	// Objects

	// Calculate the camera forward vector, so we can cull everything that's
	// behind. Ideally we'd want to do a full frustum culling here. FIXME.
	Object *object = scene_objects;
	
	while (object) {
		vec3_t diff = vec3_sub(cam_pos, object->origin);
		float cam_dot = vec3_dot(diff, cam_dir);
		float dist_sq = vec3_dot(diff, diff);
		if (
			cam_dot < object->radius && 
			dist_sq < (RENDER_FADEOUT_FAR * RENDER_FADEOUT_FAR)
		) {
			object_draw(object, &object->mat);
		}
		object = object->next;
	}
}

void scene_set_start_booms(int light_index) {
	
	int lights_len = 1;
	rgba_t color = rgba(0, 0, 0, 0);

	if (light_index == 0) { // reset all 3
		lights_len = 3;
		color = rgba(0x20, 0x20, 0x20, 0xff);
	}
	else if (light_index == 1) {
		color = rgba(0xff, 0x00, 0x00, 0xff);
	}
	else if (light_index == 2) {
		color = rgba(0xff, 0x80, 0x00, 0xff);
	}
	else if (light_index == 3) {
		color = rgba(0x00, 0xff, 0x00, 0xff);
	}

	for (int i = 0; i < start_booms_len; i++) {
		Prm libPoly = {.primitive = start_booms[i]->primitives};

		for (int j = 1; j < light_index; j++) {
			libPoly.gt4 += 1;
		}

		for (int j = 0; j < lights_len; j++) {
			for (int v = 0; v < 4; v++) {
				libPoly.gt4->color[v].r = color.r;
				libPoly.gt4->color[v].g = color.g;
				libPoly.gt4->color[v].b = color.b;
			}
			libPoly.gt4 += 1;
		}
	}
}

void scene_pulsate_red_light(Object *obj) {
	uint8_t r = clamp(sin(system_cycle_time() * M_PI * 2) * 128 + 128, 0, 255);
	Prm libPoly = {.primitive = obj->primitives};

	for (int v = 0; v < 4; v++) {
		libPoly.gt4->color[v].r = r;
		libPoly.gt4->color[v].g = 0x00;
		libPoly.gt4->color[v].b = 0x00;
	}
}

// void scene_move_oil_pump(Object *pump) {
// 	mat4_set_yaw_pitch_roll(&pump->mat, vec3(sin(system_cycle_time() * 0.125 * M_PI * 2), 0, 0));
// }

void scene_move_cameras(Object *cam) {
	vec3_t target = vec3_sub(g.ships[g.pilot].position, cam->origin);
	float height = sqrt(target.x * target.x + target.z * target.z);
	float cam_angle_x = -atan2(target.y, height);
	float cam_angle_y = -atan2(target.x, target.z);
	float y_offset = (cam->origin.x + cam->origin.z) / 50;
	mat4_set_translation(
		&cam->mat,
		vec3(
			cam->origin.x,
			cam->origin.y - 150 - sin(y_offset + system_cycle_time() * 0.5 * M_PI * 2) * 100,
			cam->origin.z
		)
	);
	// mat4_set_yaw_pitch_roll(&cam->mat, vec3(0, sin(system_cycle_time() * 0.125 * M_PI * 2), 0));
	mat4_set_yaw_pitch_roll(
		&cam->mat,
		vec3_wrap_angle(vec3(
			-cam_angle_x + sin(system_cycle_time() * 0.125 * M_PI * 2) * 0.1,
			M_PI + cam_angle_y + sin(system_cycle_time() * 0.145 * M_PI * 2) * 0.1,
			0
		)
	));
}

void scene_init_aurora_borealis(void) {
	aurora_borealis.enabled = true;
	clear(aurora_borealis.grey_coords);

	int count = 0;
	int16_t *coords;
	float y;

	Prm poly = {.primitive = sky_object->primitives};
	for (int i = 0; i < sky_object->primitives_len; i++) {
		switch (poly.primitive->type) {
		case PRM_TYPE_GT3:
			poly.gt3 += 1;
			break;
		case PRM_TYPE_GT4:
			coords = poly.gt4->coords;
			y = sky_object->vertices[coords[0]].y;
			if (y < -6000) { // -8000
				aurora_borealis.primitives[count] = poly.gt4;
				if (y > -6800) {
					aurora_borealis.coords[count] = poly.gt4->coords;
					aurora_borealis.grey_coords[count] = -1;
				}
				else if (y < -11000) {
					aurora_borealis.coords[count] = poly.gt4->coords;
					aurora_borealis.grey_coords[count] = -2;
				}
				else {
					aurora_borealis.coords[count] = poly.gt4->coords;
				}
				count++;
			}
			poly.gt4 += 1;
			break;
		}
	}
}

void scene_update_aurora_borealis(void) {
	float phase = system_time() / 30.0;
	for (int i = 0; i < 80; i++) {
		int16_t *coords = aurora_borealis.coords[i];

		if (aurora_borealis.grey_coords[i] != -2) {
			aurora_borealis.primitives[i]->color[0].r = (sin(coords[0] * phase) * 64.0) + 190;
			aurora_borealis.primitives[i]->color[0].g = (sin(coords[0] * (phase + 0.054)) * 64.0) + 190;
			aurora_borealis.primitives[i]->color[0].b = (sin(coords[0] * (phase + 0.039)) * 64.0) + 190;
		}
		if (aurora_borealis.grey_coords[i] != -2) {
			aurora_borealis.primitives[i]->color[1].r = (sin(coords[1] * phase) * 64.0) + 190;
			aurora_borealis.primitives[i]->color[1].g = (sin(coords[1] * (phase + 0.054)) * 64.0) + 190;
			aurora_borealis.primitives[i]->color[1].b = (sin(coords[1] * (phase + 0.039)) * 64.0) + 190;
		}
		if (aurora_borealis.grey_coords[i] != -1) {
			aurora_borealis.primitives[i]->color[2].r = (sin(coords[2] * phase) * 64.0) + 190;
			aurora_borealis.primitives[i]->color[2].g = (sin(coords[2] * (phase + 0.054)) * 64.0) + 190;
			aurora_borealis.primitives[i]->color[2].b = (sin(coords[2] * (phase + 0.039)) * 64.0) + 190;
		}

		if (aurora_borealis.grey_coords[i] != -1) {
			aurora_borealis.primitives[i]->color[3].r = (sin(coords[3] * phase) * 64.0) + 190;
			aurora_borealis.primitives[i]->color[3].g = (sin(coords[3] * (phase + 0.054)) * 64.0) + 190;
			aurora_borealis.primitives[i]->color[3].b = (sin(coords[3] * (phase + 0.039)) * 64.0) + 190;
		}
	}
}
