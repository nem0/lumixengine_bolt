#include "bt_tokenizer.h"
#include "bt_context.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static bt_bool can_start_identifier(char character) {
	return character > 0 && (isalpha(character) || character == '_' || character == '@');
}

static bt_bool can_contain_identifier(char character) {
	return isdigit(character) || can_start_identifier(character);
}

static bt_Token BT_TOKEN_EOF = {
	{ NULL, 0 },
	0, 0, 0,
	BT_TOKEN_EOS
};

static void tokenizer_error_unrecognized(bt_Tokenizer* tok, char got, int16_t line, int16_t col)
{
	char buffer[1024];
#ifdef _MSC_VER
	buffer[sprintf_s(buffer, sizeof(buffer), "Unrecognized character '%d'", got)] = 0;
#else
	buffer[sprintf(buffer, "Unrecognized character '%d'", got)] = 0;
#endif
	tok->context->on_error(BT_ERROR_PARSE, tok->source_name, buffer, line, col);
}

static bt_Token* make_token(bt_Context* ctx, bt_StrSlice source, uint16_t line, uint16_t col, uint16_t idx, bt_TokenType type)
{
	bt_Token* new_token = bt_gc_alloc(ctx,sizeof(bt_Token));
	new_token->source = source;
	new_token->line = line;
	new_token->col = col;
	new_token->idx = idx;
	new_token->type = type;

	return new_token;
}

bt_Tokenizer bt_open_tokenizer(bt_Context* context)
{
	bt_Tokenizer tok;
	tok.context = context;
	tok.source = tok.current = 0;
	tok.source_name = 0;
	tok.last_consumed = tok.line = tok.col = 0;

	bt_buffer_with_capacity(&tok.tokens, context, 32);
	bt_buffer_empty(&tok.temp_tokens);
	bt_buffer_with_capacity(&tok.literals, context, 4);
	
	bt_Literal lit = {
		BT_TOKEN_NUMBER_LITERAL
	};

	lit.as_num = 0;
	bt_buffer_push(context, &tok.literals, lit);

	lit.as_num = 1;
	bt_buffer_push(context, &tok.literals, lit);

	lit.type = BT_TOKEN_STRING_LITERAL;
	lit.as_str = (bt_StrSlice) { "", 0 };
	bt_buffer_push(context, &tok.literals, lit);
	
	tok.literal_zero  = make_token(context, (bt_StrSlice) { "0",     1 }, 0, 0, 0, BT_TOKEN_NUMBER_LITERAL);
	tok.literal_one   = make_token(context, (bt_StrSlice) { "1",     1 }, 0, 0, 1, BT_TOKEN_NUMBER_LITERAL);
	tok.literal_true  = make_token(context, (bt_StrSlice) { "true",  4 }, 0, 0, 0, BT_TOKEN_TRUE_LITERAL);
	tok.literal_false = make_token(context, (bt_StrSlice) { "false", 5 }, 0, 0, 0, BT_TOKEN_FALSE_LITERAL);
	tok.literal_null  = make_token(context, (bt_StrSlice) { "null",  4 }, 0, 0, 0, BT_TOKEN_NULL_LITERAL);
	tok.literal_empty_string = make_token(context, (bt_StrSlice) { "", 0 }, 0, 0, 2, BT_TOKEN_STRING_LITERAL);

	return tok;
}

void bt_close_tokenizer(bt_Tokenizer* tok)
{
	tok->line = tok->col = 0;

	for (uint32_t i = 0; i < tok->tokens.length; i++)
	{
		bt_gc_free(tok->context, tok->tokens.elements[i], sizeof(bt_Token));
	}

	for (uint32_t i = 0; i < tok->temp_tokens.length; i++)
	{
		bt_gc_free(tok->context, tok->temp_tokens.elements[i], sizeof(bt_Token));
	}

	bt_buffer_destroy(tok->context, &tok->tokens);
	bt_buffer_destroy(tok->context, &tok->temp_tokens);
	bt_buffer_destroy(tok->context, &tok->literals);

	bt_gc_free(tok->context, tok->literal_zero, sizeof(bt_Token));
	bt_gc_free(tok->context, tok->literal_one, sizeof(bt_Token));

	bt_gc_free(tok->context, (char*)tok->source, tok->source_len + 1);
	if(tok->source_name) bt_gc_free(tok->context, (char*)tok->source_name, tok->source_name_len + 1);

	tok->source = tok->current = 0;
}

void bt_tokenizer_set_source(bt_Tokenizer* tok, const char* source)
{
	tok->source_len = strlen(source);
	char* new_source = bt_gc_alloc(tok->context, tok->source_len + 1);
	memcpy(new_source, source, tok->source_len);
	new_source[tok->source_len] = 0;

	tok->source = tok->current = new_source;
	tok->line = tok->col = 1;
}

void bt_tokenizer_set_source_name(bt_Tokenizer* tok, const char* source_name)
{
	if (!source_name) {
		if (tok->source_name) bt_gc_free(tok->context, (char*)tok->source_name, tok->source_name_len + 1);
		tok->source_name = NULL;
		return;
	}

	tok->source_name_len = strlen(source_name);
	char* new_source = bt_gc_alloc(tok->context, tok->source_name_len + 1);
	memcpy(new_source, source_name, tok->source_name_len);
	new_source[tok->source_name_len] = 0;

	tok->source_name = new_source;
}

bt_Token* bt_tokenizer_emit(bt_Tokenizer* tok)
{
	if (tok->last_consumed < (int32_t)tok->tokens.length)
	{
		return tok->tokens.elements[tok->last_consumed++];
	}

eat_whitespace:
	switch (*tok->current) {
	case 0:
		return &BT_TOKEN_EOF;
	case ' ':
		tok->current++; tok->col++;
		goto eat_whitespace;
	case '\t':
		tok->current++; tok->col += 3;
		goto eat_whitespace;
	case '\n':
		tok->current++; tok->line++; tok->col = 1;
		goto eat_whitespace;
	case '\r':
		tok->current++;
		goto eat_whitespace;
	case '/':
		if (*(tok->current + 1) == '/') {
			while (*tok->current && *tok->current != '\n') tok->current++;
			goto eat_whitespace;
		}
		else if (*(tok->current + 1) == '*') {
			uint8_t depth = 1;
			tok->current += 2; tok->col += 2;
			while (depth > 0) {
				if (*tok->current == 0) goto eat_whitespace;
				if (*tok->current == '*' && *(tok->current + 1) == '/') depth--;
				if (*tok->current == '/' && *(tok->current + 1) == '*') depth++;
				if (*tok->current == '\n') { tok->line++; tok->col = 1; }
				tok->current++; tok->col++;
			}
			tok->current++; tok->col++;
			goto eat_whitespace;
		}
	}

#define BT_SIMPLE_TOKEN(character, token_type)                \
	case (character): {                                       \
		bt_Token* token = make_token(                         \
			tok->context,                                     \
			(bt_StrSlice) { tok->current, 1 },				  \
			tok->line, tok->col, tok->tokens.length,		  \
			token_type										  \
		);													  \
		tok->current++; tok->col++;							  \
		bt_buffer_push(tok->context, &tok->tokens, token);	  \
		tok->last_consumed = tok->tokens.length;              \
		return bt_buffer_last(&tok->tokens);	              \
	}

#define BT_COMPOSITE_TOKEN(character, once, second, twice)        \
	case (character): {										      \
		uint8_t len = 1;										  \
		bt_TokenType type = once;					              \
		if (*(tok->current + 1) == (second)) {			          \
			len = 2;											  \
			type = twice;								          \
		}														  \
		bt_Token* token = make_token(                             \
			tok->context,                                         \
			(bt_StrSlice) { tok->current, 1 },				      \
			tok->line, tok->col, tok->tokens.length,		      \
			type										          \
		);													      \
		tok->current += len; tok->col += len;					  \
		bt_buffer_push(tok->context, &tok->tokens, token);		  \
		tok->last_consumed = tok->tokens.length;			      \
		return bt_buffer_last(&tok->tokens);		              \
	}

#define BT_COMPOSITE_TOKEN_3(character, once, second, twice, third, thrice) \
	case (character): {										      \
		uint8_t len = 1;										  \
		bt_TokenType type = once;					              \
		if (*(tok->current + 1) == (second)) {			          \
			len = 2;											  \
			type = twice;								          \
		}														  \
		if (*(tok->current + 1) == (third)) {			          \
			len = 2;											  \
			type = thrice;								          \
		}														  \
		bt_Token* token = make_token(                             \
			tok->context,                                         \
			(bt_StrSlice) { tok->current, 1 },				      \
			tok->line, tok->col, tok->tokens.length,              \
			type										          \
		);													      \
		tok->current += len; tok->col += len;					  \
		bt_buffer_push(tok->context, &tok->tokens, token);		  \
		tok->last_consumed = tok->tokens.length;			      \
		return bt_buffer_last(&tok->tokens);		              \
	}

#define BT_DOUBLEABLE_TOKEN(character, once, twice)               \
	BT_COMPOSITE_TOKEN(character, once, character, twice)

	switch (*tok->current) {
		BT_SIMPLE_TOKEN('(', BT_TOKEN_LEFTPAREN);   BT_SIMPLE_TOKEN(')', BT_TOKEN_RIGHTPAREN);
		BT_SIMPLE_TOKEN('{', BT_TOKEN_LEFTBRACE);   BT_SIMPLE_TOKEN('}', BT_TOKEN_RIGHTBRACE);
		BT_SIMPLE_TOKEN('[', BT_TOKEN_LEFTBRACKET); BT_SIMPLE_TOKEN(']', BT_TOKEN_RIGHTBRACKET);
		
		BT_SIMPLE_TOKEN(':', BT_TOKEN_COLON);

		BT_SIMPLE_TOKEN(',', BT_TOKEN_COMMA);
		BT_SIMPLE_TOKEN(';', BT_TOKEN_SEMICOLON);
		BT_SIMPLE_TOKEN('|', BT_TOKEN_UNION);
		BT_SIMPLE_TOKEN('#', BT_TOKEN_POUND);
		
		BT_COMPOSITE_TOKEN_3('?', BT_TOKEN_QUESTION, '?', BT_TOKEN_NULLCOALESCE, '.', BT_TOKEN_QUESTIONPERIOD);
		BT_DOUBLEABLE_TOKEN('.', BT_TOKEN_PERIOD, BT_TOKEN_VARARG);
		BT_COMPOSITE_TOKEN_3('=', BT_TOKEN_ASSIGN, '=', BT_TOKEN_EQUALS, '>', BT_TOKEN_FATARROW);

		BT_COMPOSITE_TOKEN('!', BT_TOKEN_BANG, '=', BT_TOKEN_NOTEQ);
		BT_COMPOSITE_TOKEN('+', BT_TOKEN_PLUS, '=', BT_TOKEN_PLUSEQ);
		BT_COMPOSITE_TOKEN('-', BT_TOKEN_MINUS, '=', BT_TOKEN_MINUSEQ);
		BT_COMPOSITE_TOKEN('*', BT_TOKEN_MUL, '=', BT_TOKEN_MULEQ);
		BT_COMPOSITE_TOKEN('/', BT_TOKEN_DIV, '=', BT_TOKEN_DIVEQ);
		BT_COMPOSITE_TOKEN('<', BT_TOKEN_LT, '=', BT_TOKEN_LTE);
		BT_COMPOSITE_TOKEN('>', BT_TOKEN_GT, '=', BT_TOKEN_GTE);
	}

#define BT_TEST_KEYWORD(keyword, token, ttype) \
	if((sizeof(keyword) - 1) == token->source.length && strncmp(token->source.source, keyword, token->source.length) == 0) \
		token->type = ttype;

	if (can_start_identifier(*tok->current)) {
		char* current = tok->current;

		while (can_contain_identifier(*current)) current++;

		uint8_t length = (uint8_t)(current - tok->current);
		
		bt_Token* token = make_token(
			tok->context, 
			(bt_StrSlice) { tok->current, length },
			tok->line, tok->col, tok->tokens.length,
			BT_TOKEN_IDENTIFIER
		);													      

		     BT_TEST_KEYWORD("let", token, BT_TOKEN_LET)
		else BT_TEST_KEYWORD("const", token, BT_TOKEN_CONST)
		else BT_TEST_KEYWORD("fn", token, BT_TOKEN_FN)
		else BT_TEST_KEYWORD("return", token, BT_TOKEN_RETURN)
		else BT_TEST_KEYWORD("type", token, BT_TOKEN_TYPE)
		else BT_TEST_KEYWORD("if", token, BT_TOKEN_IF)
		else BT_TEST_KEYWORD("else", token, BT_TOKEN_ELSE)
		else BT_TEST_KEYWORD("for", token, BT_TOKEN_FOR)
		else BT_TEST_KEYWORD("in", token, BT_TOKEN_IN)
		else BT_TEST_KEYWORD("to", token, BT_TOKEN_TO)
		else BT_TEST_KEYWORD("by", token, BT_TOKEN_BY)
		else BT_TEST_KEYWORD("true", token, BT_TOKEN_TRUE_LITERAL)
		else BT_TEST_KEYWORD("false", token, BT_TOKEN_FALSE_LITERAL)
		else BT_TEST_KEYWORD("null", token, BT_TOKEN_NULL_LITERAL)
		else BT_TEST_KEYWORD("and", token, BT_TOKEN_AND)
		else BT_TEST_KEYWORD("or", token, BT_TOKEN_OR)
		else BT_TEST_KEYWORD("not", token, BT_TOKEN_NOT)
		else BT_TEST_KEYWORD("import", token, BT_TOKEN_IMPORT)
		else BT_TEST_KEYWORD("export", token, BT_TOKEN_EXPORT)
		else BT_TEST_KEYWORD("as", token, BT_TOKEN_AS)
		else BT_TEST_KEYWORD("from", token, BT_TOKEN_FROM)
		else BT_TEST_KEYWORD("is", token, BT_TOKEN_IS)
		else BT_TEST_KEYWORD("final", token, BT_TOKEN_FINAL)
		else BT_TEST_KEYWORD("unsealed", token, BT_TOKEN_UNSEALED)
		else BT_TEST_KEYWORD("typeof", token, BT_TOKEN_TYPEOF)
		else BT_TEST_KEYWORD("enum", token, BT_TOKEN_ENUM)
		else BT_TEST_KEYWORD("break", token, BT_TOKEN_BREAK)
		else BT_TEST_KEYWORD("continue", token, BT_TOKEN_CONTINUE)
		else BT_TEST_KEYWORD("do", token, BT_TOKEN_DO)
		else BT_TEST_KEYWORD("then", token, BT_TOKEN_THEN)
		else BT_TEST_KEYWORD("match", token, BT_TOKEN_MATCH)

		tok->current += length; tok->col += length;
		bt_buffer_push(tok->context, &tok->tokens, token);
		tok->last_consumed = tok->tokens.length;
		return bt_buffer_last(&tok->tokens);					  
	}

	if (*tok->current < 0) {
		tokenizer_error_unrecognized(tok, *tok->current, tok->line, tok->col);
		tok->current++;
		return &BT_TOKEN_EOF;
	}
	
	if (isdigit(*tok->current)) {
		char* end = NULL;
		bt_number num = strtod(tok->current, &end);
		if (end != tok->current) {
			uint8_t length = (uint8_t)(end - tok->current);

			bt_Literal lit = {
				BT_TOKEN_NUMBER_LITERAL
			};
			lit.as_num = num;

			bt_buffer_push(tok->context, &tok->literals, lit);

			bt_Token* token = make_token(
				tok->context,
				(bt_StrSlice) { tok->current, length },
				tok->line, tok->col, tok->literals.length - 1,
				BT_TOKEN_NUMBER_LITERAL
			);

			tok->current += length; tok->col += length;
		
			bt_buffer_push(tok->context, &tok->tokens, token);
			tok->last_consumed = tok->tokens.length;
			return bt_buffer_last(&tok->tokens);
		}
	}

	if (*tok->current == '"') {
		uint16_t start_line = tok->line, start_col = tok->col;

		tok->current++; tok->col++;

		char* start = tok->current;
		while (*tok->current != '"' && *tok->current) {
			if (*tok->current == '\\') {
				if (*(tok->current + 1) == '"') {
					tok->current++; tok->col++;
				}

				if (*(tok->current + 1) == 'n') {
					tok->current++; tok->col++;
				}

				if (*(tok->current + 1) == 't') {
					tok->current++; tok->col++;
				}

				if (*(tok->current + 1) == 'r') {
					tok->current++; tok->col++;
				}
				if (*(tok->current + 1) == '\\') {
					tok->current++; tok->col++;
				}

			}
			else if (*tok->current == '\n') {
				tok->col = 1; tok->line++;
			}

			tok->current++; tok->col++;
		}

		if (*tok->current) {
			tok->current++; tok->col++;
		}

		uint16_t length = (uint16_t)(tok->current - start - 1);

		bt_Literal lit = { BT_TOKEN_STRING_LITERAL };
		lit.as_str = (bt_StrSlice) { start, length };

		bt_buffer_push(tok->context, &tok->literals, lit);
		bt_Token* token = make_token(
			tok->context,
			(bt_StrSlice) { start - 1, length + 2 },
			start_line, start_col, tok->literals.length - 1,
			BT_TOKEN_STRING_LITERAL
		);

		bt_buffer_push(tok->context, &tok->tokens, token);
		tok->last_consumed = tok->tokens.length;
		return bt_buffer_last(&tok->tokens);
	}

	return &BT_TOKEN_EOF;
}

bt_Token* bt_tokenizer_peek(bt_Tokenizer* tok)
{
	if (tok->last_consumed == tok->tokens.length)
	{
		if (bt_tokenizer_emit(tok)->type == BT_TOKEN_EOS) return &BT_TOKEN_EOF;
		tok->last_consumed--;
	}

	return tok->tokens.elements[tok->last_consumed];
}

static const char* get_tok_name(bt_TokenType type)
{
	switch (type) {
	case BT_TOKEN_UNKNOWN: return "<unknown>";
	case BT_TOKEN_EOS: return "<eos>";
	case BT_TOKEN_IDENTIFIER: return "<identifier>";
	case BT_TOKEN_FALSE_LITERAL: return "false";
	case BT_TOKEN_TRUE_LITERAL: return "true";
	case BT_TOKEN_STRING_LITERAL: return "<string literal>";
	case BT_TOKEN_IDENTIFIER_LITERAL: return "<identifier>";
	case BT_TOKEN_NUMBER_LITERAL: return "<number literal>";
	case BT_TOKEN_NULL_LITERAL: return "<null>";
	case BT_TOKEN_LEFTPAREN: return "(";
	case BT_TOKEN_RIGHTPAREN: return ")";
	case BT_TOKEN_LEFTBRACE: return "{";
	case BT_TOKEN_RIGHTBRACE: return "}";
	case BT_TOKEN_LEFTBRACKET: return "[";
	case BT_TOKEN_RIGHTBRACKET: return "]";
	case BT_TOKEN_COLON: return ":";
	case BT_TOKEN_SEMICOLON: return ";";
	case BT_TOKEN_PERIOD: return ".";
	case BT_TOKEN_COMMA: return ",";
	case BT_TOKEN_QUESTION: return "?";
	case BT_TOKEN_VARARG: return "..";
	case BT_TOKEN_NULLCOALESCE: return "??";
	case BT_TOKEN_GT: return ">";
	case BT_TOKEN_GTE: return ">=";
	case BT_TOKEN_LT: return "<";
	case BT_TOKEN_LTE: return "<=";
	case BT_TOKEN_ASSIGN: return "=";
	case BT_TOKEN_EQUALS: return "==";
	case BT_TOKEN_BANG: return "!";
	case BT_TOKEN_NOTEQ: return "!=";
	case BT_TOKEN_PLUS: return "+";
	case BT_TOKEN_PLUSEQ: return "+=";
	case BT_TOKEN_MINUS: return "-";
	case BT_TOKEN_MINUSEQ: return "-=";
	case BT_TOKEN_MUL: return "*";
	case BT_TOKEN_MULEQ: return "*=";
	case BT_TOKEN_DIV: return "/";
	case BT_TOKEN_DIVEQ: return "/=";
	case BT_TOKEN_LET: return "let";
	case BT_TOKEN_CONST: return "const";
	case BT_TOKEN_FN: return "fn";
	case BT_TOKEN_RETURN: return "return";
	case BT_TOKEN_TYPE: return "type";
	case BT_TOKEN_IF: return "if";
	case BT_TOKEN_ELSE: return "else";
	case BT_TOKEN_FOR: return "for";
	case BT_TOKEN_IN: return "in";
	case BT_TOKEN_TO: return "to";
	case BT_TOKEN_BY: return "by";
	case BT_TOKEN_IS: return "is";
	case BT_TOKEN_AS: return "as";
	case BT_TOKEN_DO: return "do";
	case BT_TOKEN_THEN: return "then";
	case BT_TOKEN_FINAL: return "final";
	case BT_TOKEN_UNSEALED: return "unsealed";
	case BT_TOKEN_FATARROW: return "=>";
	case BT_TOKEN_ENUM: return "enum";
	case BT_TOKEN_BREAK: return "break";
	case BT_TOKEN_CONTINUE: return "continue";
	case BT_TOKEN_OR: return "or";
	case BT_TOKEN_AND: return "and";
	case BT_TOKEN_NOT: return "not";
	case BT_TOKEN_UNION: return "|";
	case BT_TOKEN_TYPEOF: return "typeof";
	case BT_TOKEN_IMPORT: return "import";
	case BT_TOKEN_EXPORT: return "export";
	case BT_TOKEN_FROM: return "from";
	default: return "UNHANDLED TOKEN";
	}
}

static void tokenizer_error(bt_Tokenizer* tok, bt_Token* got, bt_TokenType expected)
{
	char buffer[1024];
#ifdef _MSC_VER
	buffer[sprintf_s(buffer, sizeof(buffer), "Expected token '%s', got '%.*s'", get_tok_name(expected), got->source.length, got->source.source)] = 0;
#else
	buffer[sprintf(buffer, "Expected token '%s', got '%.*s'", get_tok_name(expected), got->source.length, got->source.source)] = 0;
#endif
	tok->context->on_error(BT_ERROR_PARSE, tok->source_name, buffer, got->line, got->col);
}

bt_bool bt_tokenizer_expect(bt_Tokenizer* tok, bt_TokenType type)
{
	bt_Token* token = bt_tokenizer_emit(tok);
	bt_bool result = token->type == type;
	
	if (!result) {
		tokenizer_error(tok, token, type);
	}
	
	return result;
}

bt_Token* bt_tokenizer_make_identifier(bt_Tokenizer* tok, bt_StrSlice name)
{
	bt_Token* token = make_token(tok->context, name, 0, 0, 0, BT_TOKEN_IDENTIFIER);
	bt_buffer_push(tok->context, &tok->temp_tokens, token);
	return token;
}

bt_Token* bt_tokenizer_make_operator(bt_Tokenizer* tok, bt_TokenType op)
{
	bt_Token* token = make_token(tok->context, (bt_StrSlice) { .source = 0, .length = 0 }, 0, 0, 0, op);
	bt_buffer_push(tok->context, &tok->temp_tokens, token);
	return token;
}
