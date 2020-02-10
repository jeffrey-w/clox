#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"

static void advance();
static void consume(TokenType, const char*);
static void declaration();
static void varDeclaration();
static uint8_t parseVariable(const char*);
static uint8_t identifierConstant(Token*);
static void defineVariable(uint8_t);
static void statement();
static void printStatement();
static void expressionStatement();
static void synchronize();
static bool match(TokenType);
static bool check(TokenType);
static void expression();
static void parsePrecedence(Precedence);
static ParseRule* getRule(TokenType);
static void literal(bool);
static void number(bool);
static void string(bool);
static void unary(bool);
static void binary(bool);
static void grouping(bool);
static void variable(bool);
static void namedVariable(Token, bool);
static void emitByte(uint8_t);
static void emitBytes(uint8_t, uint8_t);
static void emitConstant(Value);
static uint8_t makeConstant(Value);
static void emitReturn();
static Chunk* currentChunk();
static void endCompiler();
static void error(const char*);
static void errorAtCurrent(const char*);
static void errorAt(Token*, const char*);

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
  { variable, NULL,    PREC_NONE },       // TOKEN_IDENTIFIER      
  { string,   NULL,    PREC_NONE },       // TOKEN_STRING          
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
	while (!match(TOKEN_EOF)) {
		declaration();
	}
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

void declaration() {
	if (match(TOKEN_VAR)) {
		varDeclaration();
	}
	else {
		statement();
	}
	if (parser.panicMode) {
		synchronize();
	}
}

void varDeclaration() {
	uint8_t global = parseVariable("Expect variable name.");
	if (match(TOKEN_EQUAL)) {
		expression();
	}
	else {
		emitByte(OP_NIL);
	}
	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
	defineVariable(global);
}

uint8_t parseVariable(const char* errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);
	return identifierConstant(&parser.previous);
}

uint8_t identifierConstant(Token* name) { // TODO optimize this so that previously added strings are not reinserted into the constant table
	return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

void defineVariable(uint8_t global) {
	emitBytes(OP_DEFINE_GLOBAL, global);
}

void statement() {
	if (match(TOKEN_PRINT)) {
		printStatement();
	}
	else {
		expressionStatement();
	}
}

void printStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
}

void expressionStatement() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(OP_POP);
}

void synchronize() {
	parser.panicMode = false;
	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON) {
			return;
		}
		switch (parser.current.type) {
		case TOKEN_CLASS:
		case TOKEN_FUN:
		case TOKEN_VAR:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_PRINT:
		case TOKEN_RETURN:
			return;
		}
		advance();
	}
}

bool match(TokenType type) {
	if (!check(type)) {
		return false;
	}
	advance();
	return true;
}

bool check(TokenType type) {
	return parser.current.type == type;
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
	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);
	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}
	if (canAssign && match(TOKEN_EQUAL)) {
		error("Invalid assignment target.");
	}
}

ParseRule* getRule(TokenType type) {
	return &rules[type];
}

void literal(bool canAssign) {
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

void number(bool canAssign) {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

void string(bool canAssign) {
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

void unary(bool canAssign) {
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

void binary(bool canAssign) {
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

void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

void variable(bool canAssign) {
	namedVariable(parser.previous, canAssign);
}

void namedVariable(Token name, bool canAssign) {
	uint8_t arg = identifierConstant(&name);
	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitBytes(OP_SET_GLOBAL, arg);
	}
	else {
		emitBytes(OP_GET_GLOBAL, arg);
	}
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
	fprintf(stderr, "[line %d] Error", token->line); // TODO prints wrong line number if running 
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
