#include "symbols.h"

Token::Token(Tag tag_, int line_) : tag(tag_), line(line_) {}

Token::Token(int64_t val, int line_) : tag(Tag::INT), value(val), line(line_) {}

Token::Token(double val, int line_)
    : tag(Tag::DOUBLE), value(val), line(line_) {}

Token::Token(TypeID type, int line_)
    : tag(Tag::TYPE), value(type), line(line_) {}

Token::Token(Tag tag_, const std::string &val, int line_)
    : tag(tag_), value(val), line(line_) {}

int64_t Token::getInt() { return std::get<int64_t>(value); }

double Token::getDouble() { return std::get<double>(value); }

std::string Token::getString() { return std::get<std::string>(value); }

TypeID Token::getType() { return std::get<TypeID>(value); }