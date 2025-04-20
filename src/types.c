#include <math.h>
#include "types.h"
#include "utils.h"

rgba_t rgba_from_u32(uint32_t v) {
	return rgba(
		((v >> 24) & 0xff),
		((v >> 16) & 0xff),
		((v >> 8) & 0xff),
		255
	);
}

vec3_t vec3_wrap_angle(vec3_t a) {
	return vec3(wrap_angle(a.x), wrap_angle(a.y), wrap_angle(a.z));
}

float vec3_angle(vec3_t a, vec3_t b) {
	float magnitude = sqrt(
		(a.x * a.x + a.y * a.y + a.z * a.z) * 
		(b.x * b.x + b.y * b.y + b.z * b.z)
	);
	float cosine = (magnitude == 0)
		? 1
		: vec3_dot(a, b) / magnitude;
	return acos(clamp(cosine, -1, 1));
}

vec3_t vec3_transform(vec3_t a, mat4_t *mat) {
	float w = mat->m[3] * a.x + mat->m[7] * a.y + mat->m[11] * a.z + mat->m[15];
	if (w == 0) {
		w = 1;
	}
	return vec3(
		(mat->m[0] * a.x + mat->m[4] * a.y + mat->m[ 8] * a.z + mat->m[12]) / w,
		(mat->m[1] * a.x + mat->m[5] * a.y + mat->m[ 9] * a.z + mat->m[13]) / w,
		(mat->m[2] * a.x + mat->m[6] * a.y + mat->m[10] * a.z + mat->m[14]) / w
	);
}

vec3_t vec3_project_to_ray(vec3_t p, vec3_t r0, vec3_t r1) {
	vec3_t ray = vec3_normalize(vec3_sub(r1, r0));
	float dp = vec3_dot(vec3_sub(p, r0), ray);
	return vec3_add(r0, vec3_mulf(ray, dp));
}

float vec3_distance_to_plane(vec3_t p, vec3_t plane_pos, vec3_t plane_normal) {
	float dot_product = vec3_dot(vec3_sub(plane_pos, p), plane_normal);
	float norm_dot_product = vec3_dot(vec3_mulf(plane_normal, -1), plane_normal);
	return dot_product / norm_dot_product;
}

vec3_t vec3_reflect(vec3_t incidence, vec3_t normal, float f) {
	return vec3_add(incidence, vec3_mulf(normal, vec3_dot(normal, vec3_mulf(incidence, -1)) * f));
}

void mat4_set_translation(mat4_t *mat, vec3_t pos) {
	mat->cols[3][0] = pos.x;
	mat->cols[3][1] = pos.y;
	mat->cols[3][2] = pos.z;
}

void mat4_set_yaw_pitch_roll(mat4_t *mat, vec3_t rot) {
	float sx = sin( rot.x);
	float sy = sin(-rot.y);
	float sz = sin(-rot.z);
	float cx = cos( rot.x);
	float cy = cos(-rot.y);
	float cz = cos(-rot.z);

	mat->cols[0][0] = cy * cz + sx * sy * sz;
	mat->cols[1][0] = cz * sx * sy - cy * sz;
	mat->cols[2][0] = cx * sy;
	mat->cols[0][1] = cx * sz;
	mat->cols[1][1] = cx * cz;
	mat->cols[2][1] = -sx;
	mat->cols[0][2] = -cz * sy + cy * sx * sz;
	mat->cols[1][2] = cy * cz * sx + sy * sz;
	mat->cols[2][2] = cx * cy;
}

void mat4_set_roll_pitch_yaw(mat4_t *mat, vec3_t rot) {
	float sx = sin( rot.x);
	float sy = sin(-rot.y);
	float sz = sin(-rot.z);
	float cx = cos( rot.x);
	float cy = cos(-rot.y);
	float cz = cos(-rot.z);

	mat->cols[0][0] = cy * cz - sx * sy * sz;
	mat->cols[1][0] = -cx * sz;
	mat->cols[2][0] = cz * sy + cy * sx * sz;
	mat->cols[0][1] = cz * sx * sy + cy * sz;
	mat->cols[1][1] = cx *cz;
	mat->cols[2][1] = -cy * cz * sx + sy * sz;
	mat->cols[0][2] = -cx * sy;
	mat->cols[1][2] = sx;
	mat->cols[2][2] = cx * cy;
}

void mat4_translate(mat4_t *mat, vec3_t translation) {
	mat->m[12] = mat->m[0] * translation.x + mat->m[4] * translation.y + mat->m[8] * translation.z + mat->m[12];
	mat->m[13] = mat->m[1] * translation.x + mat->m[5] * translation.y + mat->m[9] * translation.z + mat->m[13];
	mat->m[14] = mat->m[2] * translation.x + mat->m[6] * translation.y + mat->m[10] * translation.z + mat->m[14];
	mat->m[15] = mat->m[3] * translation.x + mat->m[7] * translation.y + mat->m[11] * translation.z + mat->m[15];
}

void mat4_mul(mat4_t *res, mat4_t *a, mat4_t *b) {
	res->m[ 0] = b->m[ 0] * a->m[0] + b->m[ 1] * a->m[4] + b->m[ 2] * a->m[ 8] + b->m[ 3] * a->m[12];
	res->m[ 1] = b->m[ 0] * a->m[1] + b->m[ 1] * a->m[5] + b->m[ 2] * a->m[ 9] + b->m[ 3] * a->m[13];
	res->m[ 2] = b->m[ 0] * a->m[2] + b->m[ 1] * a->m[6] + b->m[ 2] * a->m[10] + b->m[ 3] * a->m[14];
	res->m[ 3] = b->m[ 0] * a->m[3] + b->m[ 1] * a->m[7] + b->m[ 2] * a->m[11] + b->m[ 3] * a->m[15];
	res->m[ 4] = b->m[ 4] * a->m[0] + b->m[ 5] * a->m[4] + b->m[ 6] * a->m[ 8] + b->m[ 7] * a->m[12];
	res->m[ 5] = b->m[ 4] * a->m[1] + b->m[ 5] * a->m[5] + b->m[ 6] * a->m[ 9] + b->m[ 7] * a->m[13];
	res->m[ 6] = b->m[ 4] * a->m[2] + b->m[ 5] * a->m[6] + b->m[ 6] * a->m[10] + b->m[ 7] * a->m[14];
	res->m[ 7] = b->m[ 4] * a->m[3] + b->m[ 5] * a->m[7] + b->m[ 6] * a->m[11] + b->m[ 7] * a->m[15];
	res->m[ 8] = b->m[ 8] * a->m[0] + b->m[ 9] * a->m[4] + b->m[10] * a->m[ 8] + b->m[11] * a->m[12];
	res->m[ 9] = b->m[ 8] * a->m[1] + b->m[ 9] * a->m[5] + b->m[10] * a->m[ 9] + b->m[11] * a->m[13];
	res->m[10] = b->m[ 8] * a->m[2] + b->m[ 9] * a->m[6] + b->m[10] * a->m[10] + b->m[11] * a->m[14];
	res->m[11] = b->m[ 8] * a->m[3] + b->m[ 9] * a->m[7] + b->m[10] * a->m[11] + b->m[11] * a->m[15];
	res->m[12] = b->m[12] * a->m[0] + b->m[13] * a->m[4] + b->m[14] * a->m[ 8] + b->m[15] * a->m[12];
	res->m[13] = b->m[12] * a->m[1] + b->m[13] * a->m[5] + b->m[14] * a->m[ 9] + b->m[15] * a->m[13];
	res->m[14] = b->m[12] * a->m[2] + b->m[13] * a->m[6] + b->m[14] * a->m[10] + b->m[15] * a->m[14];
	res->m[15] = b->m[12] * a->m[3] + b->m[13] * a->m[7] + b->m[14] * a->m[11] + b->m[15] * a->m[15];
}

void mat4_scale(mat4_t *mat, float k) {
	mat->cols[0][0] *= k;
	mat->cols[1][0] *= k;
	mat->cols[2][0] *= k;
	mat->cols[0][1] *= k;
	mat->cols[1][1] *= k;
	mat->cols[2][1] *= k;
	mat->cols[0][2] *= k;
	mat->cols[1][2] *= k;
	mat->cols[2][2] *= k;
}

vec3_t vec3_angle_from_mat4(mat4_t *mat) {
	// R = [ 
	//        R11 , R12 , R13, 
	//        R21 , R22 , R23,
	//        R31 , R32 , R33  
	//     ]
	//     00 10 20
	//     01 11 21
	//     02 12 22
	float yaw;
	float pitch;
	float roll;

	if (mat->cols[0][2] != 1 && mat->cols[0][2] != -1) {
	     float pitch_1 = -1*asin(mat->cols[0][2]);
	     float pitch_2 = M_PI - pitch_1;
	     float roll_1 = atan2( mat->cols[1][2] / cos(pitch_1), mat->cols[2][2] /cos(pitch_1) );
	     float roll_2 = atan2( mat->cols[1][2] / cos(pitch_2), mat->cols[2][2] /cos(pitch_2) );
	     float yaw_1 = atan2( mat->cols[0][1] / cos(pitch_1), mat->cols[0][0] / cos(pitch_1) );
	     float yaw_2 = atan2( mat->cols[0][1] / cos(pitch_2), mat->cols[0][0] / cos(pitch_2) );

	     pitch = pitch_1;
	     roll = roll_1;
	     yaw = yaw_1;
	} else { 
	     yaw = 0; // anything (we default this to zero)
	     if (mat->cols[0][2] == -1) { 
		pitch = M_PI/2.0;
		roll = yaw + atan2(mat->cols[1][0],mat->cols[2][0]);
		} else { 
			pitch = -M_PI/2.0; 
			roll = -1*yaw + atan2(-1*mat->cols[1][0],-1*mat->cols[2][0]);
		}
	}

	return vec3(yaw, pitch, roll);
}
