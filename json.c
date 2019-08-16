
#include "yasl.h"
#include "interpreter/VM.h"
#include "compiler/parser.h"

static int handle_escapes(struct Lexer *lex, size_t *i) {
	char buffer[9];
	char tmp;
	char *end;
	switch ((lex)->c) {
	case '"':
		(lex)->value[(*i)++] = '"';
		break;
	case '\\':
		(lex)->value[(*i)++] = '\\';
		break;
	case '/':
		(lex)->value[(*i)++] = '/';
		break;
	case 'b':
		(lex)->value[(*i)++] = '\b';
		break;
	case 'f':
		(lex)->value[(*i)++] = '\f';
		break;
	case 'n':
		(lex)->value[(*i)++] = '\n';
		break;
	case 'r':
		(lex)->value[(*i)++] = '\r';
		break;
	case 't':
		(lex)->value[(*i)++] = '\t';
		break;
	case 'u':
		buffer[0] = lex_getchar(lex);
		buffer[1] = lex_getchar(lex);
		buffer[2] = lex_getchar(lex);
		buffer[3] = lex_getchar(lex);
		buffer[4] = '\0';
		tmp = (char) strtol(buffer, &end, 16);
		if (end != buffer + 4) {
			//YASL_PRINT_ERROR_SYNTAX("Invalid hex string escape in line %zd.\n", lex->line);
			while (lex->c != '\n' && lex->c != '"') lex_getchar(lex);
			lex_error(lex);
			return 1;
		}
		lex->value[(*i)++] = tmp;
		return 0;
	default:
		//YASL_PRINT_ERROR_SYNTAX("Invalid string escape sequence in line %zd.\n", (lex)->line);
		while (lex->c != '\n' && lex->c != '"') lex_getchar(lex);
		lex_error(lex);
		return 1;
	}
	return 0;
}

static int lex_eatstring(struct Lexer *lex) {
	if (lex->c == '"') {
		lex->val_len = 6;
		lex->value = (char *)realloc(lex->value, lex->val_len);
		size_t i = 0;
		lex->type = T_STR;

		lex_getchar(lex);
		while (lex->c != '"' && !lxeof(lex->file)) {
			if (lex->c == '\n') {
				//YASL_PRINT_ERROR_SYNTAX("Unclosed string literal in line %zd.\n", lex->line);
				lex_error(lex);
				return 1;
			}

			if (lex->c == '\\') {
				lex_getchar(lex);
				if (handle_escapes(lex, &i)) {
					return 1;
				}
			} else {
				lex->value[i++] = lex->c;
			}
			lex_getchar(lex);
			if (i == lex->val_len) {
				lex->val_len *= 2;
				lex->value = (char *)realloc(lex->value, lex->val_len);
			}
		}

		lex->val_len = i;

		if (lxeof(lex->file)) {
			//YASL_PRINT_ERROR_SYNTAX("Unclosed string literal in line %zd.\n", lex->line);
			lex_error(lex);
			return 1;
		}

		return 1;

	}
	return 0;
}

struct YASL_Object parse_value(struct Parser *parser);

struct YASL_Object parse_object(struct Parser *parser) {
	struct YASL_Object tmp = YASL_TABLE(rcht_new());

	eattok(parser, T_LBRC);
	while (parser->lex.type != T_LBRC && parser->lex.type != T_EOF) {
		struct YASL_Object key = YASL_STR(YASL_String_new_sized_heap(0, parser->lex.val_len, parser->lex.value));
		parser->lex.value = NULL;
		eattok(parser, T_STR);
		eattok(parser, T_COLON);
		struct YASL_Object value = parse_value(parser);
		YASL_Table_set(&tmp, &key, &value);
		if (parser->lex.type != T_COMMA) break;
		eattok(parser, T_COMMA);
	}

	eattok(parser, T_RBRC);
	return tmp;
}

struct YASL_Object parse_array(struct Parser *parser) {
	const struct YASL_Object tmp = YASL_LIST(rcls_new());

	eattok(parser, T_LSQB);
	while (parser->lex.type != T_RSQB && parser->lex.type != T_EOF) {
		struct YASL_Object value = parse_value(parser);
		YASL_List_append(YASL_GETLIST(tmp), value);
		if (parser->lex.type != T_COMMA) break;
		eattok(parser, T_COMMA);
	}

	eattok(parser, T_RSQB);
	return tmp;
}

struct YASL_Object parse_value(struct Parser *parser) {
	struct YASL_Object tmp;
	switch (parser->lex.type) {
	case T_STR:
		tmp = YASL_STR(YASL_String_new_sized_heap(0, parser->lex.val_len, parser->lex.value));
		parser->lex.value = NULL;
		eattok(parser, T_STR);
		break;
	case T_INT:
		tmp = YASL_INT(strtoll(parser->lex.value, (char **) NULL, 10));
		eattok(parser, T_INT);
		break;
	case T_FLOAT:
		tmp = YASL_FLOAT(strtod(parser->lex.value, (char **) NULL));
		eattok(parser, T_FLOAT);
		break;
	case T_BOOL:
		tmp = YASL_BOOL(strcmp(parser->lex.value, "true") == 0);
		eattok(parser, T_BOOL);
		break;
	case T_LSQB:
		tmp = parse_array(parser);
		break;
	case T_LBRC:
		tmp = parse_object(parser);
		break;
	case T_ID:
		if (strcmp(parser->lex.value, "null") == 0) {
			tmp = YASL_UNDEF();
			eattok(parser, T_ID);
			break;
		}
	default:
		lex_error(&parser->lex);
		tmp = YASL_UNDEF();
		break;
	}
	return tmp;
}

int YASL_json(struct YASL_State *S) {
	if (!YASL_ISSTR(vm_peek((struct VM *)S))) {
		YASL_PRINT_ERROR_BAD_ARG_TYPE("json", 0, Y_STR, vm_peek((struct VM *)S).type);
		return YASL_TYPE_ERROR;
	}

	struct YASL_String *str = vm_popstr((struct VM *)S);
	size_t bytes_len = YASL_String_len(str);
	char *bytes = str->str + str->start;

	struct Parser parser = NEW_PARSER(lexinput_new_bb(bytes, bytes_len));
	gettok(&parser.lex);
	struct YASL_Object tmp = parse_object(&parser);

	if (parser.status == YASL_SUCCESS)
		vm_push((struct VM *)S, tmp);
	else
		vm_pushundef((struct VM *)S);
	return YASL_SUCCESS;
}

int YASL_load_json(struct YASL_State *S) {
	struct YASL_Object *json = YASL_CFunction(YASL_json, 1);

	YASL_declglobal(S, "json");
	YASL_pushobject(S, json);
	YASL_setglobal(S, "json");

	return YASL_SUCCESS;

}
