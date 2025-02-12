#include "std.h"
#include "nodes.h"
#include "../MultiLang/MultiLang.h"

static std::string fileLabel;
static std::map<std::string, std::string> fileMap;

void StmtNode::debug(int pos, Codegen* g) {
	if(g->debug) {
		TNode* t = fileLabel.size() ? global(fileLabel) : iconst(0);
		g->code(call("__bbDebugStmt", iconst(pos), t));
	}
}

void StmtSeqNode::reset(const std::string& file, const std::string& lab) {
	fileLabel = "";
	fileMap.clear();

	fileMap[file] = lab;
}

////////////////////////
// Statement Sequence //
////////////////////////
void StmtSeqNode::semant(Environ* e) {
	for(int k = 0; k < stmts.size(); ++k) {
		try { stmts[k]->semant(e); }
		catch(Ex& x) {
			if(x.pos < 0) x.pos = stmts[k]->pos;
			if(!x.file.size()) x.file = file;
			throw;
		}
	}
}

void StmtSeqNode::translate(Codegen* g) {

	std::string t = fileLabel;
	fileLabel = file.size() ? fileMap[file] : "";
	for(int k = 0; k < stmts.size(); ++k) {
		StmtNode* stmt = stmts[k];
		stmt->debug(stmts[k]->pos, g);
		try {
			stmt->translate(g);
		}
		catch(Ex& x) {
			if(x.pos < 0) x.pos = stmts[k]->pos;
			if(!x.file.size()) x.file = file;
			throw;
		}
	}
	fileLabel = t;
}

/////////////////
// An Include! //
/////////////////
void IncludeNode::semant(Environ* e) {

	label = genLabel();
	fileMap[file] = label;

	stmts->semant(e);
}

void IncludeNode::translate(Codegen* g) {

	if(g->debug) g->s_data(file, label);

	stmts->translate(g);
}

///////////////////
// a declaration //
///////////////////
void DeclStmtNode::semant(Environ* e) {
	decl->proto(e->decls, e);
	decl->semant(e);
}

void DeclStmtNode::translate(Codegen* g) {
	decl->translate(g);
}

//////////////////////////////
// Dim AND declare an Array //
//////////////////////////////
void DimNode::semant(Environ* e) {
	Type* t = tagType(tag, e);
	if(Decl* d = e->findDecl(ident)) {
		ArrayType* a = d->type->arrayType();
		if(!a || a->dims != exprs->size() || (t && a->elementType != t)) {
			ex(MultiLang::duplicate_identifier);
		}
		sem_type = a; sem_decl = 0;
	}
	else {
		if(e->level > 0) ex(MultiLang::array_not_found_in_main);
		if(!t) t = Type::int_type;
		sem_type = new ArrayType(t, exprs->size());
		sem_decl = e->decls->insertDecl(ident, sem_type, DECL_ARRAY);
		e->types.push_back(sem_type);
	}
	exprs->semant(e);
	exprs->castTo(Type::int_type, e);
}

void DimNode::translate(Codegen* g) {
	TNode* t;
	g->code(call("__bbUndimArray", global("_a" + ident)));
	for(int k = 0; k < exprs->size(); ++k) {
		t = add(global("_a" + ident), iconst(k * 4 + 12));
		t = move(exprs->exprs[k]->translate(g), mem(t));
		g->code(t);
	}
	g->code(call("__bbDimArray", global("_a" + ident)));

	if(!sem_decl) return;

	int et;
	Type* ty = sem_type->arrayType()->elementType;
	if(ty == Type::int_type) et = 1;
	else if(ty == Type::float_type) et = 2;
	else if(ty == Type::string_type) et = 3;
	else et = 5;

	g->align_data(4);
	g->i_data(0, "_a" + ident);
	g->i_data(et);
	g->i_data(exprs->size());
	for(int k = 0; k < exprs->size(); ++k) g->i_data(0);
}

////////////////
// Assignment //
////////////////
void AssNode::semant(Environ* e) {
	var->semant(e);
	if(var->sem_type->constType()) ex(MultiLang::constants_can_not_assigned_to);
	if(var->sem_type->vectorType()) ex(MultiLang::blitz_arrays_can_not_assigned_to);
	expr = expr->semant(e);
	expr = expr->castTo(var->sem_type, e);
}

void AssNode::translate(Codegen* g) {
	g->code(var->store(g, expr->translate(g)));
}

//////////////////////////
// Expression statement //
//////////////////////////
void ExprStmtNode::semant(Environ* e) {
	expr = expr->semant(e);
}

void ExprStmtNode::translate(Codegen* g) {
	TNode* t = expr->translate(g);
	if(expr->sem_type == Type::string_type) t = call("__bbStrRelease", t);
	g->code(t);
}

////////////////
// user label //
////////////////
void LabelNode::semant(Environ* e) {
	if(Label* l = e->findLabel(ident)) {
		if(l->def >= 0) ex(MultiLang::duplicate_label);
		l->def = pos; l->data_sz = data_sz;
	}
	else e->insertLabel(ident, pos, -1, data_sz);
	ident = e->funcLabel + ident;
}

void LabelNode::translate(Codegen* g) {
	g->label("_l" + ident);
}

//////////////////
// Restore data //
//////////////////
void RestoreNode::semant(Environ* e) {
	if(e->level > 0) e = e->globals;

	if(ident.size() == 0) sem_label = 0;
	else {
		sem_label = e->findLabel(ident);
		if(!sem_label) sem_label = e->insertLabel(ident, -1, pos, -1);
	}
}

void RestoreNode::translate(Codegen* g) {
	TNode* t = global("__DATA");
	if(sem_label) t = add(t, iconst(sem_label->data_sz * 8));
	g->code(call("__bbRestore", t));
}

////////////////////
// Goto statement //
////////////////////
void GotoNode::semant(Environ* e) {
	if(!e->findLabel(ident)) {
		e->insertLabel(ident, -1, pos, -1);
	}
	ident = e->funcLabel + ident;
}

void GotoNode::translate(Codegen* g) {
	g->code(jump("_l" + ident));
}

/////////////////////
// Gosub statement //
/////////////////////
void GosubNode::semant(Environ* e) {
	if(e->level > 0) ex(MultiLang::gosub_may_not_used_inside_function);
	if(!e->findLabel(ident)) e->insertLabel(ident, -1, pos, -1);
	ident = e->funcLabel + ident;
}

void GosubNode::translate(Codegen* g) {
	g->code(jsr("_l" + ident));
}

//////////////////
// If statement //
//////////////////
void IfNode::semant(Environ* e) {
	expr = expr->semant(e);
	expr = expr->castTo(Type::int_type, e);
	stmts->semant(e);
	if(elseOpt) elseOpt->semant(e);
}

void IfNode::translate(Codegen* g) {
	if(ConstNode* c = expr->constNode()) {
		if(c->intValue()) stmts->translate(g);
		else if(elseOpt) elseOpt->translate(g);
	}
	else {
		std::string _else = genLabel();
		g->code(jumpf(expr->translate(g), _else));
		stmts->translate(g);
		if(elseOpt) {
			std::string _else2 = genLabel();
			g->code(jump(_else2));
			g->label(_else);
			elseOpt->translate(g);
			_else = _else2;
		}
		g->label(_else);
	}
}

///////////
// Break //
///////////
void ExitNode::semant(Environ* e) {
	sem_brk = e->breakLabel;
	if(!sem_brk.size()) ex(MultiLang::break_must_appear_inside_loop);
}

void ExitNode::translate(Codegen* g) {
	g->code(new TNode(IR_JUMP, 0, 0, sem_brk));
}

//////////////
// Continue //
//////////////
void ContinueNode::semant(Environ* e) {
	sem_cont = e->continueLabel;
	if (!sem_cont.size()) ex(MultiLang::continue_must_appear_inside_loop);
}
void ContinueNode::translate(Codegen* g) {
	g->code(new TNode(IR_JUMP, 0, 0, sem_cont));
}

/////////////////////
// While statement //
/////////////////////
void WhileNode::semant(Environ* e) {
	expr = expr->semant(e);
	expr = expr->castTo(Type::int_type, e);
	std::string brk = e->setBreak(sem_brk = genLabel());
	std::string cont = e->setContinue(sem_cont = genLabel());
	stmts->semant(e);
	e->setBreak(brk);
	e->setContinue(cont);
}

void WhileNode::translate(Codegen* g) {
	std::string loop = genLabel();
	if(ConstNode* c = expr->constNode()) {
		if(!c->intValue()) return;
		g->label(loop);
		stmts->translate(g);
		g->code(jump(loop));
	}
	else {
		std::string cond = genLabel();
		g->label(sem_cont);
		g->code(jump(cond));
		g->label(loop);
		stmts->translate(g);
		debug(wendPos, g);
		g->label(cond);
		g->code(jumpt(expr->translate(g), loop));
	}
	g->label(sem_brk);
}

///////////////////
// For/Next loop //
///////////////////
ForNode::ForNode(VarNode* var, ExprNode* from, ExprNode* to, ExprNode* step, StmtSeqNode* ss, int np)
	:var(var), fromExpr(from), toExpr(to), stepExpr(step), stmts(ss), nextPos(np) {
}

ForNode::~ForNode() {
	delete stmts;
	delete stepExpr;
	delete toExpr;
	delete fromExpr;
	delete var;
}

void ForNode::semant(Environ* e) {
	var->semant(e);
	Type* ty = var->sem_type;
	if(ty->constType()) ex(MultiLang::index_variable_can_not_constant);
	if(ty != Type::int_type && ty != Type::float_type) {
		ex(MultiLang::index_variable_must_integer_or_real);
	}
	fromExpr = fromExpr->semant(e);
	fromExpr = fromExpr->castTo(ty, e);
	toExpr = toExpr->semant(e);
	toExpr = toExpr->castTo(ty, e);
	stepExpr = stepExpr->semant(e);
	stepExpr = stepExpr->castTo(ty, e);

	if(!stepExpr->constNode()) ex(MultiLang::step_value_must_constant);

	std::string brk = e->setBreak(sem_brk = genLabel());
	std::string cont = e->setContinue(sem_cont = genLabel());
	stmts->semant(e);
	e->setBreak(brk);
	e->setContinue(cont);
}

void ForNode::translate(Codegen* g) {

	TNode* t; Type* ty = var->sem_type;

	//initial assignment
	g->code(var->store(g, fromExpr->translate(g)));

	std::string cond = genLabel();
	std::string loop = genLabel();
	g->code(jump(cond));
	g->label(loop);
	stmts->translate(g);

	//execute the step part
	debug(nextPos, g);
	g->label(sem_cont);
	int op = ty == Type::int_type ? IR_ADD : IR_FADD;
	t = new TNode(op, var->load(g), stepExpr->translate(g));
	g->code(var->store(g, t));

	//test for loop cond
	g->label(cond);
	op = stepExpr->constNode()->floatValue() > 0 ? '>' : '<';
	t = compare(op, var->load(g), toExpr->translate(g), ty);
	g->code(jumpf(t, loop));

	g->label(sem_brk);
}

///////////////////////////////
// For each object of a type //
///////////////////////////////
void ForEachNode::semant(Environ* e) {
	var->semant(e);
	Type* ty = var->sem_type;

	if(ty->structType() == 0) ex(MultiLang::index_variable_is_not_newtype);
	Type* t = e->findType(typeIdent);
	if(!t) ex(MultiLang::type_name_not_found);
	if (t != ty) ex(MultiLang::type_mismatch);

	std::string brk = e->setBreak(sem_brk = genLabel());
	std::string cont = e->setContinue(sem_cont = genLabel());
	stmts->semant(e);
	e->setBreak(brk);
	e->setContinue(cont);
}

void ForEachNode::translate(Codegen* g) {
	TNode* t, * l, * r;
	std::string _loop = genLabel();

	std::string objFirst, objNext;

	if(var->isObjParam()) {
		objFirst = "__bbObjEachFirst2";
		objNext = "__bbObjEachNext2";
	}
	else {
		objFirst = "__bbObjEachFirst";
		objNext = "__bbObjEachNext";
	}

	l = var->translate(g);
	r = global("_t" + typeIdent);
	t = jumpf(call(objFirst, l, r), sem_brk);
	g->code(t);

	g->label(_loop);
	stmts->translate(g);

	debug(nextPos, g);
	g->label(sem_cont);
	t = jumpt(call(objNext, var->translate(g)), _loop);
	g->code(t);

	g->label(sem_brk);
}

////////////////////////////
// Return from a function //
////////////////////////////
void ReturnNode::semant(Environ* e) {
	if(e->level <= 0 && expr) {
		ex(MultiLang::main_cannot_return_value);
	}
	if(e->level > 0) {
		if(!expr) {
			if(e->returnType == Type::float_type) {
				expr = new FloatConstNode(0);
			}
			else if(e->returnType == Type::string_type) {
				expr = new StringConstNode("");
			}
			else if(e->returnType->structType()) {
				expr = new NullConstNode();
			}
			else {
				expr = new IntConstNode(0);
			}
		}
		expr = expr->semant(e);
		expr = expr->castTo(e->returnType, e);
		returnLabel = e->funcLabel + "_leave";
	}
}

void ReturnNode::translate(Codegen* g) {
	if(!expr) {
		g->code(new TNode(IR_RET, 0, 0));
		return;
	}

	TNode* t = expr->translate(g);

	if(expr->sem_type == Type::float_type) {
		g->code(new TNode(IR_FRETURN, t, 0, returnLabel));
	}
	else {
		g->code(new TNode(IR_RETURN, t, 0, returnLabel));
	}
}

//////////////////////
// Delete statement //
//////////////////////
void DeleteNode::semant(Environ* e) {
	expr = expr->semant(e);
	if(expr->sem_type->structType() == 0) ex(MultiLang::cant_delete_non_newtype);
}

void DeleteNode::translate(Codegen* g) {
	TNode* t = expr->translate(g);
	g->code(call("__bbObjDelete", t));
}

///////////////////////////
// Delete each of a type //
///////////////////////////
void DeleteEachNode::semant(Environ* e) {
	Type* t = e->findType(typeIdent);
	if(!t || t->structType() == 0) ex(MultiLang::specified_name_is_not_newtype_name);
}

void DeleteEachNode::translate(Codegen* g) {
	g->code(call("__bbObjDeleteEach", global("_t" + typeIdent)));
}

///////////////////////////
// Insert object in list //
///////////////////////////
void InsertNode::semant(Environ* e) {
	expr1 = expr1->semant(e);
	expr2 = expr2->semant(e);
	StructType* t1 = expr1->sem_type->structType();
	StructType* t2 = expr2->sem_type->structType();
	if(!t1 || !t2) ex(MultiLang::illegal_expression_type);
	if(t1 != t2) ex(MultiLang::objects_types_are_different);
}

void InsertNode::translate(Codegen* g) {
	TNode* t1 = expr1->translate(g);
	//if(g->debug) t1 = jumpf(t1, "__bbNullObjEx");
	TNode* t2 = expr2->translate(g);
	//if(g->debug) t2 = jumpf(t2, "__bbNullObjEx");
	std::string s = before ? "__bbObjInsBefore" : "__bbObjInsAfter";
	g->code(call(s, t1, t2));
}

////////////////////////
// A select statement //
////////////////////////
void SelectNode::semant(Environ* e) {
	expr = expr->semant(e);
	Type* ty = expr->sem_type;
	if(ty->structType()) ex(MultiLang::select_cannot_used_with_objects);

	//we need a temp var
	Decl* d = e->decls->insertDecl(genLabel(), expr->sem_type, DECL_LOCAL);
	sem_temp = new DeclVarNode(d);

	for(int k = 0; k < cases.size(); ++k) {
		CaseNode* c = cases[k];
		c->exprs->semant(e);
		c->exprs->castTo(ty, e);
		c->stmts->semant(e);
	}
	if(defStmts) defStmts->semant(e);
}

void SelectNode::translate(Codegen* g) {

	Type* ty = expr->sem_type;

	g->code(sem_temp->store(g, expr->translate(g)));

	std::vector<std::string> labs;
	std::string brk = genLabel();

	for(int k = 0; k < cases.size(); ++k) {
		CaseNode* c = cases[k];
		labs.push_back(genLabel());
		for(int j = 0; j < c->exprs->size(); ++j) {
			ExprNode* e = c->exprs->exprs[j];
			TNode* t = compare('=', sem_temp->load(g), e->translate(g), ty);
			g->code(jumpt(t, labs.back()));
		}
	}
	if(defStmts) defStmts->translate(g);
	g->code(jump(brk));
	for(int k = 0; k < cases.size(); ++k) {
		CaseNode* c = cases[k];
		g->label(labs[k]);
		c->stmts->translate(g);
		g->code(jump(brk));
	}

	g->label(brk);
}

////////////////////////////
// Repeat...Until/Forever //
////////////////////////////
void RepeatNode::semant(Environ* e) {
	sem_brk = genLabel();
	sem_cont = genLabel();
	std::string brk = e->setBreak(sem_brk);
	std::string cont = e->setContinue(sem_cont);
	stmts->semant(e);
	e->setBreak(brk);
	e->setContinue(cont);
	if(expr) {
		expr = expr->semant(e);
		expr = expr->castTo(Type::int_type, e);
	}
}

void RepeatNode::translate(Codegen* g) {

	std::string loop = genLabel();
	g->label(sem_cont);
	g->label(loop);
	stmts->translate(g);
	debug(untilPos, g);

	if(ConstNode* c = expr ? expr->constNode() : 0) {
		if(!c->intValue()) g->code(jump(loop));
	}
	else {
		if(expr) g->code(jumpf(expr->translate(g), loop));
		else g->code(jump(loop));
	}
	g->label(sem_brk);
}

///////////////
// Read data //
///////////////
void ReadNode::semant(Environ* e) {
	var->semant(e);
	if(var->sem_type->constType()) ex(MultiLang::constants_can_not_modified);
	if(var->sem_type->structType()) ex(MultiLang::data_can_not_read_into_object);
}

void ReadNode::translate(Codegen* g) {
	TNode* t;
	if(var->sem_type == Type::int_type) t = call("__bbReadInt");
	else if(var->sem_type == Type::float_type) t = fcall("__bbReadFloat");
	else t = call("__bbReadStr");
	g->code(var->store(g, t));
}