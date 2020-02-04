#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"

static void advance();
static void consume(TokenType type, const char* message);
static void expression();
static void parsePrecedence(Precedence precedence);
static ParseRule* getRule(TokenType type);
static void literal();
static void number();
static void unary();
static void binary();
static void grouping();
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t one, uint8_t two);
static void emitConstant(Value value);
static uint8_t makeConstant(Value value);
static void emitReturn();
static Chunk* currentChunk();
static void endCompiler();
static void error(const char* message);
static void errorAtCurrent(const char* message);
static void errorAt(Token* token, const char* message);

ParseRule rules[] = {
  { grouping, NULL,    PREC_NONE },       // TOKEN_LEFT_PAREN      
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN     
  { NULL,     NULL,    PREC_NONE },       // TOKEN_LEFT_BRACE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE     
  { NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_DOT             
  { unary,    binary,  PREC_TERM },       // TOKEN_MINUS           
  { NULL,     binary,  PREC_TERM },       // TOKEN_PLUS            
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SEMICOLON       
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_SLASH           
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_STAR            
  { unary,    NULL,    PREC_NONE },       // TOKEN_BANG            
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_BANG_EQUAL      
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL           
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_EQUAL_EQUAL     
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER         
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER_EQUAL   
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS            
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS_EQUAL      
  { NULL,     NULL,    PREC_NONE },       // TOKEN_IDENTIFIER      
  { NULL,     NULL,    PREC_NONE },       // TOKEN_STRING          
  { number,   NULL,    PREC_NONE },       // TOKEN_NUMBER          
  { NULL,     NULL,    PREC_NONE },       // TOKEN_AND             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE            
  { literal,  NULL,    PREC_NONE },       // TOKEN_FALSE           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FOR             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FUN             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_IF              
  { literal,  NULL,    PREC_NONE },       // TOKEN_NIL             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_OR              
  { NULL,     NULL,    PREC_NONE },       // TOKEN_PRINT           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN          
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SUPER           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_THIS            
  { literal,  NULL,    PREC_NONE },       // TOKEN_TRUE            
  { NULL,     NULL,    PREC_NONE },       // TOKEN_VAR             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_WHILE           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ERROR           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EOF             
};

bool compile(const char* source, Chunk* chunk) {
	initScanner(source);
	parser.hadError = false;
	parser.panicMode = false;
	compilingChunk = chunk;
	advance();
	expression();
	consume(TOKEN_EOF, "Expect end of expression.");
	endCompiler();
	return !parser.hadError;
}

void advance() {
	parser.previous = parser.current;
	while (true) {
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR) {
			break;
		}
		errorAtCurrent(parser.current.start);
	}
}

void consume(TokenType type, const char* message) {
	if (parser.current.type == type) {
		advance();
		return;
	}
	errorAtCurrent(message);
}

void expression() {
	parsePrecedence(PREC_ASSIGNMENT);
}

void parsePrecedence(Precedence precedence) {
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (!prefixRule) {
		error("Expect expression.");
		return;
	}
	prefixRule();
	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule();
	}
}

ParseRule* getRule(TokenType type) {
	return &rules[type];
}

void literal() {
	switch (parser.previous.type) {
	case TOKEN_FALSE:
		emitByte(OP_FALSE);
		break;
	case TOKEN_NIL:
		emitByte(OP_NIL);
		break;
	case TOKEN_TRUE:
		emitByte(OP_TRUE);
		break;
	default:
		return; // TODO need internal error logic
	}
}

void number() {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

void unary() {
	TokenType operatorType = parser.previous.type;
	parsePrecedence(PREC_UNARY);
	switch (operatorType) {
	case TOKEN_BANG:
		emitByte(OP_NOT);
		break;
	case TOKEN_MINUS:
		emitByte(OP_NEGATE);
		break;
	default:
		return; // TODO need internal error logic
	}
}

void binary() {
	TokenType operatorType = parser.previous.type;
	ParseRule* rule = getRule(operatorType);
	parsePrecedence((Precedence)(rule->precedence + 1));
	switch (operatorType) {
	case TOKEN_BANG_EQUAL:
		emitBytes(OP_EQUAL, OP_NOT);
		break;
	case TOKEN_EQUAL_EQUAL:
		emitByte(OP_EQUAL);
		break;
	case TOKEN_GREATER:
		emitByte(OP_GREATER);
		break;
	case TOKEN_GREATER_EQUAL:
		emitBytes(OP_LESS, OP_NOT);
		break;
	case TOKEN_LESS:
		emitByte(OP_LESS);
		break;
	case TOKEN_LESS_EQUAL:
		emitBytes(OP_GREATER, OP_NOT);
		break;
	case TOKEN_PLUS:
		emitByte(OP_ADD);
		break;
	case TOKEN_MINUS:
		emitByte(OP_SUBTRACT);
		break;
	case TOKEN_STAR:
		emitByte(OP_MULTIPLY);
		break;
	case TOKEN_SLASH:
		emitByte(OP_DIVIDE);
		break;
	default:
		return; // TODO need internal error logic
	}
}

void grouping() {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

void emitByte(uint8_t byte) {
	writeChunk(currentChunk(), byte, parser.previous.line);
}

void emitBytes(uint8_t one, uint8_t two) {
	emitByte(one);
	emitByte(two);
}

void emitConstant(Value value) {
	emitBytes(OP_CONSTANT, makeConstant(value));
}

uint8_t makeConstant(Value value) {
	int constant = addConstant(currentChunk(), value);
	if (constant > UINT8_MAX) {
		error("Too many constatns in one chunk.");
		return 0;
	}
	return (uint8_t)constant;
}

void emitReturn() {
	emitByte(OP_RETURN);
}

Chunk* currentChunk() {
	return compilingChunk;
}

void endCompiler() {
	emitReturn();
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), "code");
	}
#endif // DEBUG_PRINT_CODE

}

void error(const char* message) {
	errorAt(&parser.previous, message);
}

void errorAtCurrent(const char* message) {
	errorAt(&parser.current, message);
}

void errorAt(Token* token, const char* message) {
	if (parser.panicMode) {
		return;
	}
	parser.panicMode = true;
	fprintf(stderr, "[lined %d] Error", token->line);
	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	}
	else if (token->type == TOKEN_ERROR); // Nothing.
	else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}
	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}
