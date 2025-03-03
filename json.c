#include <yasl/yasl.h>
#include "cJSON/cJSON.h"

static void parse_obj(struct YASL_State *S, const cJSON *obj);
static void parse_arr(struct YASL_State *S, const cJSON *arr);

static void switch_type(struct YASL_State *S, const cJSON *item) {
	if (cJSON_IsNumber(item)) {
		double d = item->valuedouble;
		int i = item->valueint;
		if (d != i) {
			YASL_pushfloat(S, d);
		} else {
			YASL_pushint(S, i);
		}
	} else if (cJSON_IsBool(item)) {
		YASL_pushbool(S, item->valueint);
	} else if (cJSON_IsString(item)) {
		YASL_pushzstr(S, item->valuestring);
	} else if (cJSON_IsObject(item)) {
		parse_obj(S, item);
	} else if (cJSON_IsArray(item)) {
		parse_arr(S, item);
	} else {
		YASL_pushundef(S);
	}
}

static void parse_arr(struct YASL_State *S, const cJSON *arr) {
	const cJSON *item;
	YASL_pushlist(S);
	cJSON_ArrayForEach(item, arr) {
		switch_type(S, item);
		YASL_listpush(S);
	}
}

static void parse_obj(struct YASL_State *S, const cJSON *obj) {
	const cJSON *item;
	YASL_pushtable(S);
	cJSON_ArrayForEach(item, obj) {
		YASL_pushzstr(S, item->string);
		switch_type(S, item);
		YASL_tableset(S);
	}
}

static int YASL_json_parse(struct YASL_State *S) {
	const char *s = YASL_peekcstr(S);
	const cJSON *json = cJSON_Parse(s);

	if (json == NULL) {
		YASL_pushundef(S);
		YASL_pushbool(S, false);
		return 2;
	}

	parse_obj(S, json);
	return 1;
}

int YASL_load_dyn_lib(struct YASL_State *S) {
	YASL_pushcfunction(S, YASL_json_parse, 1);

	return 1;
}
