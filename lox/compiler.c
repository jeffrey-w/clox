#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"

#define PARAM_MAX 255
#define UNINITIALIZED -1

// TODO add support for switch statements and the ternary operator

static void initComplier(Compiler*, FunctionType);
static void advance();
static Token syntheticToken(const char* );
static void consume(TokenType, const char*);
static void declaration();
static void classDeclaration();
static void method();
static void funDeclaration();
static void function(FunctionType);
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
static void returnStatement();
static void ifStatement();
static void whileStatement();
static void forStatement();
static void patchJump(int offset);
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
static uint8_t initializers();
static void number(bool);
static void string(bool);
static void index(bool);
static void call(bool);
static uint8_t argumentList();
static void dot(bool); // TODO move this
static void unary(bool);
static void binary(bool);
static void and_(bool);
static void or_(bool);
static void grouping(bool);
static void super_(bool);
static void this_(bool);
static void variable(bool);
static void namedVariable(Token, bool);
static int resolveLocal(Compiler*, Token*);
static int resolveUpvalue(Compiler*, Token*);
static int addUpvalue(Compiler*, uint8_t, bool);
static bool isAssigned(uint8_t, uint8_t, uint8_t);
static bool isIncrement(bool*);
static void emitByte(uint8_t);
static void emitBytes(uint8_t, uint8_t);
static void emitConstant(Value);
static uint8_t makeConstant(Value);
static int emitJump(uint8_t);
static void emitLoop(int);
static void emitReturn();
static Chunk* currentChunk();
static ObjFunction* endCompiler();
static void error(const char*);
static void errorAtCurrent(const char*);
static void errorAt(Token*, const char*);

ParseRule rules[] = {
  { grouping, call,    PREC_CALL },       // TOKEN_LEFT_PAREN
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN
  { literal,  NULL,    PREC_NONE },       // TOKEN_LEFT_BRACE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE
  { NULL,     index,   PREC_CALL },       // TOKEN_LEFT_BRACK
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACK
  { NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA
  { NULL,     dot,     PREC_CALL },       // TOKEN_DOT
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SEMICOLON
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_SLASH
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_STAR
  { unary,    binary,  PREC_TERM },       // TOKEN_MINUS
  { variable, NULL,    PREC_TERM },       // TOKEN_MINUS_MINUS
  { NULL,     binary,  PREC_TERM },       // TOKEN_PLUS
  { variable, NULL,    PREC_TERM },       // TOKEN_PLUS_PLUS
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
  { super_,   NULL,    PREC_NONE },       // TOKEN_SUPER
  { this_,    NULL,    PREC_NONE },       // TOKEN_THIS
  { literal,  NULL,    PREC_NONE },       // TOKEN_TRUE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_VAR
  { NULL,     NULL,    PREC_NONE },       // TOKEN_WHILE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ERROR
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EOF
};

ObjFunction* compile(const char* source) {
	Compiler compiler;
	initScanner(source);
	initComplier(&compiler, TYPE_SCRIPT);
	parser.hadError = false;
	parser.panicMode = false;
	advance();
	while (!match(TOKEN_EOF)) {
		declaration();
	}
	ObjFunction* function = endCompiler();
	return parser.hadError ? NULL : function;
}

void initComplier(Compiler* compiler, FunctionType type) {
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	compiler->function = newFunction();
	current = compiler;
	if (type != TYPE_SCRIPT) {
		current->function->name = copyString(parser.previous.start, parser.previous.length);
	}
	Local* local = &current->locals[current->localCount++];
	local->depth = 0;
	local->isCaptured = false;
	if (type != TYPE_FUNCTION) {
		local->name.start = "this";
		local->name.length = 4;
	}
	else {
		local->name.start = "";
		local->name.length = 0;
	}
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

Token syntheticToken(const char* text) {
	Token token;
	token.start = text;
	token.length = (int)strlen(text);
	return token;
}

void consume(TokenType type, const char* message) {
	if (parser.current.type == type) {
		advance();
		return;
	}
	errorAtCurrent(message);
}

void declaration() {
	if (match(TOKEN_CLASS)) {
		classDeclaration();
	}
	else if (match(TOKEN_FUN)) {
		funDeclaration();
	}
	else if (match(TOKEN_VAR)) {
		varDeclaration();
	}
	else {
		statement();
	}
	if (parser.panicMode) {
		synchronize();
	}
}

void classDeclaration() {
	consume(TOKEN_IDENTIFIER, "Expect class name.");
	Token className = parser.previous;
	uint8_t nameConstant = identifierConstant(&className);
	declareVariable();
	emitBytes(OP_CLASS, nameConstant);
	defineVariable(nameConstant);
	ClassCompiler classCompiler;
	classCompiler.enclosing = currentClass;
	classCompiler.name = parser.previous;
	classCompiler.hasSuperclass = false;
	currentClass = &classCompiler;
	if (match(TOKEN_LESS)) {
		consume(TOKEN_IDENTIFIER, "Expect superclass name.");
		variable(false);
		if (identifiersEqual(&className, &parser.previous)) {
			error("A class cannot inherit from itself.");
		}
		beginScope();
		addLocal(syntheticToken("super"));
		defineVariable(0);
		namedVariable(className, false);
		emitByte(OP_INHERIT);
		classCompiler.hasSuperclass = true;
	}
	namedVariable(className, false);
	consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		method();
	}
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
	emitByte(OP_POP);
	if (classCompiler.hasSuperclass) {
		endScope();
	}
	currentClass = currentClass->enclosing;
}

void method() {
	consume(TOKEN_IDENTIFIER, "Expect method name.");
	uint8_t constant = identifierConstant(&parser.previous);
	FunctionType type = TYPE_METHOD;
	if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) { // TODO avoid magic constants
		type = TYPE_INITIALIZER;
	}
	function(type);
	emitBytes(OP_METHOD, constant);
}

void funDeclaration() {
	uint8_t global = parseVariable("Expect function name.");
	markInitialized();
	function(TYPE_FUNCTION);
	defineVariable(global);
}

void function(FunctionType type) {
	Compiler compiler;
	initComplier(&compiler, type);
	beginScope();
	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > PARAM_MAX) {
				// TODO concatenate PARAM_MAX to rest of msg before passing to errorAtCurrent
				errorAtCurrent("Cannot have more than 255 parameters.");
			}
			uint8_t paramConstant = parseVariable("Expect parameter name.");
			defineVariable(paramConstant);
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
	consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block();
	ObjFunction* function = endCompiler();
	emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
	for (int i = 0; i < function->upvalueCount; i++) {
		uint8_t isLocal = compiler.upvalues[i].isLocal ? 1 : 0;
		uint8_t index = compiler.upvalues[i].index;
		emitBytes(isLocal, index);
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
	local->isCaptured = false;
}

uint8_t identifierConstant(Token* name) {
	ValueArray constants = currentChunk()->constants;
	for (int i = 0; i < constants.count; i++) {
		if (IS_STRING(constants.values[i])) {
			ObjString* string = AS_STRING(constants.values[i]);
			if (STRCMP(name->length, string->length, name->start, string->data)) {
				return i;
			}
		}
	}
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
	if (!current->scopeDepth) {
		return;
	}
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

void statement() {
	if (match(TOKEN_PRINT)) {
		printStatement();
	}
	else if (match(TOKEN_RETURN)) {
		returnStatement();
	}
	else if (match(TOKEN_IF)) {
		ifStatement();
	}
	else if (match(TOKEN_WHILE)) {
		whileStatement();
	}
	else if (match(TOKEN_FOR)) {
		forStatement();
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

void returnStatement() {
	if (current->type == TYPE_SCRIPT) {
		error("Cannot return from top-level code.");
	}
	if (match(TOKEN_SEMICOLON)) {
		emitReturn();
	}
	else {
		if (current->type == TYPE_INITIALIZER) {
			error("Cannot return a type from an initializer.");
		}
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
		emitByte(OP_RETURN);
	}
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

void forStatement() {
	int loopStart, exitJump;
	beginScope();
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
	if (match(TOKEN_SEMICOLON)); // No initializer.
	else if (match(TOKEN_VAR)) {
		varDeclaration();
	}
	else {
		expressionStatement();
	}
	loopStart = currentChunk()->count;
	exitJump = UNINITIALIZED;
	if (!match(TOKEN_SEMICOLON)) {
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
		exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP);
	}
	if (!match(TOKEN_RIGHT_PAREN)) {
		int bodyJump = emitJump(OP_JUMP);
		int incrementStart = currentChunk()->count;
		expression();
		emitByte(OP_POP);
		consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
		emitLoop(loopStart);
		loopStart = incrementStart;
		patchJump(bodyJump);
	}
	statement();
	emitLoop(loopStart);
	if (exitJump != UNINITIALIZED) {
		patchJump(exitJump);
		emitByte(OP_POP);
	}
	endScope();
}

void patchJump(int offset) {
	int jump = currentChunk()->count - offset - 2;
	if (jump > UINT16_MAX) {
		error("Too much code to jump over.");
	}
	currentChunk()->code[offset] = (jump >> 8) & 0xff;
	currentChunk()->code[offset + 1] = jump & 0xff;
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
		if (current->locals[current->localCount - 1].isCaptured) {
			emitByte(OP_CLOSE_UPVALUE);
		}
		else {
			emitByte(OP_POP);
		}
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
	case TOKEN_LEFT_BRACE: {
		uint8_t elements = initializers();
		emitBytes(OP_ARRAY, elements);
		break;
	}
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

uint8_t initializers() {
	uint8_t count = 0;
	if (!check(TOKEN_RIGHT_BRACE)) {
		do {
			expression();
			if (count == PARAM_MAX) {
				// TODO concatenate PARAM_MAX to rest of msg before passing to error
				error("Cannot initialize an array with more than 255 elements.");
			}
			count++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after array initializers.");
	return count;
}

void number(bool canAssign) {
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

void string(bool canAssign) {
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

void index(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_BRACK, "Expect ']' after index.");
	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		emitByte(OP_SET_INDEX);
	}
	else {
		emitByte(OP_GET_INDEX);
	}
}

void call(bool canAssign) {
	uint8_t argCount = argumentList();
	emitBytes(OP_CALL, argCount);
}

uint8_t argumentList() {
	uint8_t argCount = 0;
	if (!check(TOKEN_RIGHT_PAREN)) {
		do {
			expression();
			if (argCount == PARAM_MAX) {
				// TODO concatenate PARAM_MAX to rest of msg before passing to error
				error("Cannot have more than 255 arguments");
			}
			argCount++;
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
	return argCount;
}

void dot(bool canAssign) {
	consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
	uint8_t name = identifierConstant(&parser.previous);
	if (canAssign && isAssigned(OP_GET_PROPERTY, OP_SET_PROPERTY, name)) {
		return;
	}
	if (match(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList();
		emitBytes(OP_INVOKE, name);
		emitByte(argCount);
	}
	else {
		emitBytes(OP_GET_PROPERTY, name);
	}
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

void super_(bool canAssign) {
	if (!currentClass) {
		error("Cannot use 'super' outside of a class.");
	}
	else if (!currentClass->hasSuperclass) {
		error("Cannot use 'super' in a class with no superclass.");
	}
	consume(TOKEN_DOT, "Expect '.' after 'super'");
	consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
	uint8_t name = identifierConstant(&parser.previous);
	namedVariable(syntheticToken("this"), false);
	if (match(TOKEN_LEFT_PAREN)) {
		uint8_t argCount = argumentList();
		namedVariable(syntheticToken("super"), false);
		emitBytes(OP_SUPER_INVOKE, name);
		emitByte(argCount);
	}
	else {
		namedVariable(syntheticToken("super"), false);
		emitBytes(OP_GET_SUPER, name);
	}
}

void this_(bool canAssign) {
	if (!currentClass) {
		error("Cannot use 'this' outside of a class.");
		return;
	}
	variable(false);
}

void variable(bool canAssign) {
	namedVariable(parser.previous, canAssign);
}

void namedVariable(Token name, bool canAssign) {
	int arg;
	uint8_t getOp, setOp;
	if (name.type == TOKEN_MINUS_MINUS || name.type == TOKEN_PLUS_PLUS) {
		if (check(TOKEN_IDENTIFIER)) {
			name = parser.current;
		}
	}
	if ((arg = resolveLocal(current, &name)) != UNINITIALIZED) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else if ((arg = resolveUpvalue(current, &name)) != UNINITIALIZED) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	}
	else {
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}
	if (canAssign && isAssigned(getOp, setOp, arg)) {
		return;
	}
	emitBytes(getOp, (uint8_t)arg);
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

int resolveUpvalue(Compiler* compiler, Token* name) {
	int local, upvalue;
	if (!compiler->enclosing) {
		return UNINITIALIZED;
	}
	local = resolveLocal(compiler->enclosing, name);
	if (local != UNINITIALIZED) {
		compiler->enclosing->locals[local].isCaptured = true;
		return addUpvalue(compiler, (uint8_t)local, true);
	}
	upvalue = resolveUpvalue(compiler->enclosing, name);
	if (upvalue != UNINITIALIZED) {
		return addUpvalue(compiler, (uint8_t)local, false);
	}
	return UNINITIALIZED;
}

int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
	int upvalueCount = compiler->function->upvalueCount;
	for (int i = 0; i < upvalueCount; i++) {
		Upvalue* upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->isLocal == isLocal) {
			return i;
		}
	}
	if (upvalueCount == UINT8_COUNT) {
		error("Too many closure variables in function.");
		return 0;
	}
	compiler->upvalues[upvalueCount].isLocal = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	return compiler->function->upvalueCount++;
}

bool isAssigned(uint8_t getOp, uint8_t setOp, uint8_t arg) {
	bool postfix;
	if (match(TOKEN_EQUAL)) {
		expression();
		emitBytes(setOp, arg);
		return true;
	}
	else if (isIncrement(&postfix)) {
		emitBytes(getOp, arg);
		emitBytes(parser.previous.type == TOKEN_MINUS_MINUS
			? OP_DECREMENT : OP_INCREMENT, (uint8_t)postfix);
		emitBytes(setOp, arg);
		if (postfix) {
			emitByte(OP_POP);
		}
		else {
			consume(TOKEN_IDENTIFIER, "Expect identifier after prefix operator.");
		}
		return true;
	}
	return false;
}

bool isIncrement(bool* postfix) {
	TokenType type = parser.previous.type;
	if (type == TOKEN_MINUS_MINUS || type == TOKEN_PLUS_PLUS) {
		*postfix = false;
		return true;
	}
	return (*postfix = match(TOKEN_MINUS_MINUS) || match(TOKEN_PLUS_PLUS));
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
	if (current->type == TYPE_INITIALIZER) {
		emitBytes(OP_GET_LOCAL, 0);
	}
	else {
		emitByte(OP_NIL);
	}
	emitByte(OP_RETURN);
}

Chunk* currentChunk() {
	return &current->function->chunk;
}

ObjFunction* endCompiler() {
	emitReturn();
	ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		printf("Compilation summary for: ");
		disassembleChunk(currentChunk(), function->name ? function->name->data : "script");
		printf("\n");
	}
#endif // DEBUG_PRINT_CODE
	current = current->enclosing;
	return function;

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

void markCompilerRoots() {
	Compiler* compiler = current;
	while (compiler) {
		markObject((Obj*)compiler->function);
		compiler = compiler->enclosing;
	}
}
