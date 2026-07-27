// Copyright (c) 2025 Meng Chengzhen, in Shandong University
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "configlib.h"

using std::unique_ptr;
using std::make_unique;

namespace config_parser {

/**
 * Config Value Statement Semantics: (higher priority first)
 * 1. () : Parentheses for grouping
 * 2. @ / clog2(...) : log2ceil (e.g. @8 = 3, clog2(15) = 4)
 * 3. ~ ! - : bitwise NOT, logical NOT, unary minus
 * 4. * / % : multiplication, division, modulo
 * 5. + - : addition, subtraction
 * 6. << >> : bitwise shift left, shift right
 * 7. < <= > >= : comparison
 * 8. == != : equality
 * 9. & : bitwise AND
 * 10. ^ : bitwise XOR
 * 11. | : bitwise OR
 * 12. && : logical AND
 * 13. || : logical OR
 * 14. ?: : conditional expression
 */
enum class TokenType {
    End,
    Number,
    Identifier,
    LParen,
    RParen,
    Log2,
    BNot,
    LNot,
    UMinus,
    Mul,
    Div,
    Mod,
    Add,
    Sub,
    Shl,
    Shr,
    Lt,
    Le,
    Gt,
    Ge,
    Eq,
    Neq,
    BAnd,
    BXor,
    BOr,
    LAnd,
    LOr,
    Question,
    Colon
};

typedef struct __Token {
    TokenType type;
    size_t    pos;
    string    text;
    ConfigRealValue value; // valid if type == Number
} Token;

unique_ptr<vector<Token>> tokenizeConfigValueExpression(const string &expr, uint32_t &errpos, string &err);

enum class NodeType {
    Number,
    Identifier,
    UnaryOp,
    BinaryOp,
    Conditional
};

typedef struct __ASTNode {
    NodeType type;
    size_t    pos;
    ConfigRealValue value; // valid if type == Number
    string    name;        // valid if type == Identifier
    TokenType op;          // valid if type == UnaryOp or BinaryOp
    unique_ptr<__ASTNode> left;  // valid if type == UnaryOp or BinaryOp
    unique_ptr<__ASTNode> right; // valid if type == BinaryOp
    unique_ptr<__ASTNode> cond;         // valid if type == Conditional
    unique_ptr<__ASTNode> then_branch;  // valid if type == Conditional
    unique_ptr<__ASTNode> else_branch;  // valid if type == Conditional
} ASTNode;

/**
 * Input tokens donot include Indentifier token type.
 * Identifiers should be resolved to their values before parsing.
 */
unique_ptr<ASTNode> parseConfigValueExpression(const vector<Token> &tokens, uint32_t &errpos, string &err);

ConfigRealValue evaluateConfigValueExpression(const ASTNode &node, uint32_t &errpos, string &err);

unique_ptr<vector<ConfigName>> parseReferencedIdentifier(const string &valuestr, uint32_t &errpos, string &err);

inline bool tokenEq(const string &expr1, const string &expr2) {
    uint32_t errpos;
    string err;
    auto tokens1 = tokenizeConfigValueExpression(expr1, errpos, err);
    if (!tokens1) return false;
    auto tokens2 = tokenizeConfigValueExpression(expr2, errpos, err);
    if (!tokens2) return false;
    if (tokens1->size() != tokens2->size()) return false;
    for (size_t i = 0; i < tokens1->size(); ++i) {
        if (tokens1->at(i).type != tokens2->at(i).type) return false;
        if (tokens1->at(i).text != tokens2->at(i).text) return false;
    }
    return true;
}

} // namespace config_parser
