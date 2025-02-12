#include "std.h"
#include "environ.h"
#include <algorithm>

Environ::Environ(const std::string& f, Type* r, int l, Environ* gs)
    :funcLabel(f), returnType(r), level(l), globals(gs) {
    decls = new DeclSeq();
    typeDecls = new DeclSeq();
    funcDecls = new DeclSeq();
    if (globals) globals->children.push_back(this);
}

Environ::~Environ() {
    if (globals) globals->children.remove(this);
    while (children.size()) delete children.back();
    for (; labels.size(); labels.pop_back()) delete labels.back();

    //delete all types
    delete decls;
    delete funcDecls;
    delete typeDecls;

    for (int k = 0; k < types.size(); ++k) delete types[k];
}

Decl* Environ::findDecl(const std::string& s) {
    for (Environ* e = this; e; e = e->globals) {
        if (Decl* d = e->decls->findDecl(s)) {
            if (d->kind & (DECL_LOCAL | DECL_PARAM)) {
                if (e == this) return d;
            }
            else return d;
        }
    }
    return 0;
}

Decl* Environ::findFunc(const std::string& s, const int params) {
    for (Environ* e = this; e; e = e->globals) {
        if (Decl* d = e->funcDecls->findDecl(s, params)) return d;
    }
    return 0;
}

Type* Environ::findType(const std::string& s) {
    if (s == "%") return Type::int_type;		//INTEGER
    if (s == "#") return Type::float_type;	//FLOATING POINT
    if (s == "$") return Type::string_type;	//STRING
    for (Environ* e = this; e; e = e->globals) {
        if (Decl* d = e->typeDecls->findDecl(s)) return d->type->structType();
    }
    return 0;
}

Label* Environ::findLabel(const std::string& s) {
    for (int k = 0; k < labels.size(); ++k) if (labels[k]->name == s) return labels[k];
    return 0;
}

Label* Environ::insertLabel(const std::string& s, int def, int src, int sz) {
    Label* l = new Label(s, def, src, sz);
    labels.push_back(l); return l;
}

std::string Environ::setBreak(const std::string& s) {
    std::string t = breakLabel;
    breakLabel = s; return t;
}

std::string Environ::setContinue(const std::string& s) {
    std::string t = continueLabel;
    continueLabel = s; return t;
}