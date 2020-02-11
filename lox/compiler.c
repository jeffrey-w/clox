#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"

#define UNINITIALIZED -1

static void initComplier(Compiler*);
static void advance();
static void consume(TokenType, const char*);
static void declaration();
static void varDeclaration();
static uint8_t parseVariable(const char*);
static void declareVariable();
static bool identifiersEqual(Token*, Token*);
static void addLocal(Token);
static uint8_t identifierConstant(Token*);
static void defineVariable(uint8_t);
static void markInitialized();
static void statement();
static void printStatement();
static void ifStatement();
static void patchJump(int offset);
static void whileStatement();
static void block();
static void beginScope();
static void endScope();
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
static void and_(bool);
static void or_(bool);
static void grouping(bool);
static void variable(bool);
static void namedVariable(Token, bool);
static int resolveLocal(Compiler*, Token*);
static void emitByte(uint8_t);
static void emitBytes(uint8_t, uint8_t);
static void emitConstant(Value);
static uint8_t makeConstant(Value);
static int emitJump(uint8_t);
static void emitLoop(int);
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
  { NULL,     binary,  PREC_EQUALITY },   // TOKEN_BANG_EQUAL
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL
  { NULL,     binary,  PREC_EQUALITY },   // TOKEN_EQUAL_EQUAL
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER_EQUAL
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS_EQUAL
  { variable, NULL,    PREC_NONE },       // TOKEN_IDENTIFIER
  { string,   NULL,    PREC_NONE },       // TOKEN_STRING
  { number,   NULL,    PREC_NONE },       // TOKEN_NUMBER
  { NULL,     and_,    PREC_AND },        // TOKEN_AND
  { NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE
  { literal,  NULL,    PREC_NONE },       // TOKEN_FALSE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FOR
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FUN
  { NULL,     NULL,    PREC_NONE },       // TOKEN_IF
  { literal,  NULL,    PREC_NONE },       // TOKEN_NIL
  { NULL,     or_,     PREC_OR },         // TOKEN_OR
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
	Compiler compiler;
	initScanner(source);
	initComplier(&compiler);
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

void initComplier(Compiler* compiler) {
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	current = compiler;
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
	declareVariable();
	if (current->scopeDepth) {
		return 0;
	}
	return identifierConstant(&parser.previous);
}

void declareVariable() {
	if (!current->scopeDepth) {
		return;
	}
	Token* name = &parser.previous; // TODO can't we just pass in parser.previous?
	for (int i = current->localCount - 1; i >= 0; i--) {
		Local* local = &current->locals[i];
		if (local->depth != UNINITIALIZED && local->depth < current->scopeDepth) {
			break;
		}
		if (identifiersEqual(name, &local->name)) {
			error("Variable with this name already declared in this scope.");
		}
	}
	addLocal(*name);
}

bool identifiersEqual(Token* a, Token* b) {
	if (a->length != b->length) {
		return false;
	}
	return !memcmp(a->start, b->start, a->length);
}

void addLocal(Token name) {
	if (current->localCount == UINT8_MAX) {
		error("Too many local variables in function.");
		return;
	}
	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = UNINITIALIZED;
}

uint8_t identifierConstant(Token* name) { // TODO optimize this so that previously added strings are not reinserted into the constant table
	return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

void defineVariable(uint8_t global) {
	if (current->scopeDepth) { // TODO get name of local to runtime
		markInitialized();
		return;
	}
	emitBytes(OP_DEFINE_GLOBAL, global);
}

void markInitialized() {
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

void statement() {
	if (match(TOKEN_PRINT)) {
		printStatement();
	}
	else if (match(TOKEN_IF)) {
		ifStatement();
	}
	else if (match(TOKEN_WHILE)) {
		whileStatement();
	}
	else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
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

void ifStatement() {
	int thenJump, elseJump;
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
	thenJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();
	elseJump = emitJump(OP_JUMP);
	patchJump(thenJump);
	emitByte(OP_POP);
	if (match(TOKEN_ELSE)) {
		statement();
		patchJump(elseJump);
	}
}

void patchJump(int offset) {
	int jump = currentChunk()->count - offset - 2;
	if (jump > UINT16_MAX) {
		error("Too much code to jump over.");
	}
	currentChunk()->code[offset] = (jump >> 8) & 0xff;
	currentChunk()->code[offset + 1] = jump & 0xff;
}

void whileStatement() {
	int loopStart = currentChunk()->count;
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condtion.");
	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();
	emitLoop(loopStart);
	patchJump(exitJump);
	emitByte(OP_POP);
}

void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

void beginScope() {
	current->scopeDepth++;
}

void endScope() {
	current->scopeDepth--;
	while (current->localCount && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		emitByte(OP_POP);
		current->localCount--;
	}
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

void and_(bool canAssign) {
	int endJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	parsePrecedence(PREC_AND);
	patchJump(endJump);
}

void or_(bool canAssign) {
	int elseJump = emitJump(OP_JUMP_IF_FALSE);
	int endJump = emitJump(OP_JUMP);
	patchJump(elseJump);
	emitByte(OP_POP);
	parsePrecedence(PREC_OR);
	patchJump(endJump);
}

void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

void variable(bool canAssign) {
	namedVariable(parser.previous, canAssign);
}

void namedVariable(Token name, bool canAssign) {
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);
	if (arg != UNINITIALIZED) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else {
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}
	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitBytes(setOp, (uint8_t)arg);
	}
	else {
		emitBytes(getOp, (uint8_t)arg);
	}
}

int resolveLocal(Compiler* compiler, Token* name) {
	for (int i = compiler->localCount - 1; i >= 0; i--) {
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == UNINITIALIZED) {
				error("Cannot read local variable in its own initializer.");
			}
			return i;
		}
	}
	return UNINITIALIZED;
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

int emitJump(uint8_t instruction) {
	emitByte(instruction);
	emitBytes(0xff, 0xff);
	return currentChunk()->count - 2;
}

void emitLoop(int loopStart) {
	emitByte(OP_LOOP);
	int offset = currentChunk()->count - loopStart + 2;
	if (offset > UINT16_MAX) {
		error("Loop body too large.");
	}
	emitByte((offset >> 8) & 0xff);
	emitByte(offset & 0xff);
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
