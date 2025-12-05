// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "configlib.h"


#include <algorithm>
#include <functional>
#include <queue>

using std::unique_ptr;
using std::make_unique;

namespace config_parser {

/**
 * Config Value Statement Semantics: (higher priority first)
 * 1. () : Parentheses for grouping
 * 2. @ : log2 (e.g. @8 = 3, @16 = 4)
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

inline unique_ptr<vector<Token>> tokenizeConfigValueExpression(const string &expr, uint32_t &errpos, string &err) {
    err.clear(); errpos = 0;
    auto out = make_unique<vector<Token>>();
    size_t i = 0;
    const size_t n = expr.size();

    auto skipSpaces = [&]() {
        while (i < n && std::isspace(static_cast<unsigned char>(expr[i]))) ++i;
    };

    skipSpaces();
    while (i < n) {
        char c = expr[i];
        // number
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = i;
            ConfigRealValue val = 0;
            Token t;
            t.pos = start;
            if (c == '0' && i + 1 < n && (expr[i+1] == 'x' || expr[i+1] == 'X')) {
                // hex
                i += 2; // skip 0x
                if (i >= n || !std::isxdigit(static_cast<unsigned char>(expr[i]))) {
                    err = string("Invalid hex literal"); errpos = i; return nullptr;
                }
                while (i < n && std::isxdigit(static_cast<unsigned char>(expr[i]))) {
                    char ch = expr[i++];
                    int d = 0;
                    if (ch >= '0' && ch <= '9') d = ch - '0';
                    else if (ch >= 'a' && ch <= 'f') d = 10 + (ch - 'a');
                    else if (ch >= 'A' && ch <= 'F') d = 10 + (ch - 'A');
                    val = (val << 4) + d;
                }
            } else {
                // decimal
                while (i < n && std::isdigit(static_cast<unsigned char>(expr[i]))) {
                    val = val * 10 + (expr[i++] - '0');
                }
            }
            t.type = TokenType::Number; t.text = expr.substr(start, i - start); t.value = val;
            out->push_back(std::move(t));
            skipSpaces();
            continue;
        }

        // identifier (starts with letter or underscore)
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            ++i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_')) ++i;
            Token t; t.type = TokenType::Identifier; t.text = expr.substr(start, i - start); t.pos = start;
            out->push_back(std::move(t));
            skipSpaces();
            continue;
        }

        // operators and punctuation
        // handle two-char operators first
        if (c == '<') {
            if (i + 1 < n && expr[i+1] == '<') { out->push_back(Token{TokenType::Shl, i, "<<", 0}); i += 2; }
            else if (i + 1 < n && expr[i+1] == '=') { out->push_back(Token{TokenType::Le, i, "<=", 0}); i += 2; }
            else { out->push_back(Token{TokenType::Lt, i, "<", 0}); ++i; }
            skipSpaces(); continue;
        }
        if (c == '>') {
            if (i + 1 < n && expr[i+1] == '>') { out->push_back(Token{TokenType::Shr, i, ">>", 0}); i += 2; }
            else if (i + 1 < n && expr[i+1] == '=') { out->push_back(Token{TokenType::Ge, i, ">=", 0}); i += 2; }
            else { out->push_back(Token{TokenType::Gt, i, ">", 0}); ++i; }
            skipSpaces(); continue;
        }
        if (c == '=') {
            if (i + 1 < n && expr[i+1] == '=') { out->push_back(Token{TokenType::Eq, i, "==", 0}); i += 2; skipSpaces(); continue; }
            // single '=' is not a valid operator in expressions here
            err = string("Unexpected token '='") ; errpos = static_cast<uint32_t>(i); return nullptr;
        }
        if (c == '!') {
            if (i + 1 < n && expr[i+1] == '=') { out->push_back(Token{TokenType::Neq, i, "!=", 0}); i += 2; }
            else { out->push_back(Token{TokenType::LNot, i, "!", 0}); ++i; }
            skipSpaces(); continue;
        }
        if (c == '&') {
            if (i + 1 < n && expr[i+1] == '&') { out->push_back(Token{TokenType::LAnd, i, "&&", 0}); i += 2; }
            else { out->push_back(Token{TokenType::BAnd, i, "&", 0}); ++i; }
            skipSpaces(); continue;
        }
        if (c == '|') {
            if (i + 1 < n && expr[i+1] == '|') { out->push_back(Token{TokenType::LOr, i, "||", 0}); i += 2; }
            else { out->push_back(Token{TokenType::BOr, i, "|", 0}); ++i; }
            skipSpaces(); continue;
        }
        if (c == '?') { out->push_back(Token{TokenType::Question, i, "?", 0}); ++i; skipSpaces(); continue; }
        if (c == ':') { out->push_back(Token{TokenType::Colon, i, ":", 0}); ++i; skipSpaces(); continue; }
        if (c == '(') { out->push_back(Token{TokenType::LParen, i, "(", 0}); ++i; skipSpaces(); continue; }
        if (c == ')') { out->push_back(Token{TokenType::RParen, i, ")", 0}); ++i; skipSpaces(); continue; }
        if (c == '@') { out->push_back(Token{TokenType::Log2, i, "@", 0}); ++i; skipSpaces(); continue; }
        if (c == '~') { out->push_back(Token{TokenType::BNot, i, "~", 0}); ++i; skipSpaces(); continue; }
        if (c == '*') { out->push_back(Token{TokenType::Mul, i, "*", 0}); ++i; skipSpaces(); continue; }
        if (c == '/') { out->push_back(Token{TokenType::Div, i, "/", 0}); ++i; skipSpaces(); continue; }
        if (c == '%') { out->push_back(Token{TokenType::Mod, i, "%", 0}); ++i; skipSpaces(); continue; }
        if (c == '+') { out->push_back(Token{TokenType::Add, i, "+", 0}); ++i; skipSpaces(); continue; }
        if (c == '-') {
            if (out->empty() ||
                out->back().type == TokenType::LParen ||
                out->back().type == TokenType::Log2 ||
                out->back().type == TokenType::BNot ||
                out->back().type == TokenType::LNot ||
                out->back().type == TokenType::UMinus ||
                out->back().type == TokenType::Mul ||
                out->back().type == TokenType::Div ||
                out->back().type == TokenType::Mod ||
                out->back().type == TokenType::Add ||
                out->back().type == TokenType::Sub ||
                out->back().type == TokenType::Shl ||
                out->back().type == TokenType::Shr ||
                out->back().type == TokenType::Lt ||
                out->back().type == TokenType::Le ||
                out->back().type == TokenType::Gt ||
                out->back().type == TokenType::Ge ||
                out->back().type == TokenType::Eq ||
                out->back().type == TokenType::Neq ||
                out->back().type == TokenType::BAnd ||
                out->back().type == TokenType::BXor ||
                out->back().type == TokenType::BOr ||
                out->back().type == TokenType::LAnd ||
                out->back().type == TokenType::LOr ||
                out->back().type == TokenType::Question) {
                // unary minus
                out->push_back(Token{TokenType::UMinus, i, "-", 0});
            } else {
                // binary minus
                out->push_back(Token{TokenType::Sub, i, "-", 0});
            }
            ++i;
            skipSpaces();
            continue;
        }
        if (c == '^') { out->push_back(Token{TokenType::BXor, i, "^", 0}); ++i; skipSpaces(); continue; }

        // unknown char
        err = string("Unexpected character: ") + c;
        errpos = static_cast<uint32_t>(i);
        return nullptr;
    }

    // push End token
    out->push_back(Token{TokenType::End, i, "", 0});
    return out;
}

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
inline unique_ptr<ASTNode> parseConfigValueExpression(const vector<Token> &tokens, uint32_t &errpos, string &err) {
    err.clear(); errpos = 0;
    size_t idx = 0;
    const size_t n = tokens.size();

    auto current = [&]() -> const Token& {
        if (idx < n) return tokens[idx];
        static Token endt{TokenType::End, 0, string(), 0};
        return endt;
    };

    std::function<unique_ptr<ASTNode>()> parseExpression;

    // forward declarations for precedence levels
    std::function<unique_ptr<ASTNode>()> parsePrimary;
    std::function<unique_ptr<ASTNode>()> parseUnary;
    std::function<unique_ptr<ASTNode>()> parseMulDiv;
    std::function<unique_ptr<ASTNode>()> parseAddSub;
    std::function<unique_ptr<ASTNode>()> parseShift;
    std::function<unique_ptr<ASTNode>()> parseRelational;
    std::function<unique_ptr<ASTNode>()> parseEquality;
    std::function<unique_ptr<ASTNode>()> parseBitAnd;
    std::function<unique_ptr<ASTNode>()> parseBitXor;
    std::function<unique_ptr<ASTNode>()> parseBitOr;
    std::function<unique_ptr<ASTNode>()> parseLogicalAnd;
    std::function<unique_ptr<ASTNode>()> parseLogicalOr;

    parsePrimary = [&]() -> unique_ptr<ASTNode> {
        const Token &t = current();
        if (t.type == TokenType::Number) {
            auto node = make_unique<ASTNode>();
            node->type = NodeType::Number;
            node->value = t.value;
            node->pos = t.pos;
            ++idx;
            return node;
        }
        if (t.type == TokenType::Identifier) {
            auto node = make_unique<ASTNode>();
            node->type = NodeType::Identifier;
            node->name = t.text;
            node->pos = t.pos;
            ++idx;
            return node;
        }
        if (t.type == TokenType::LParen) {
            ++idx;
            auto node = parseExpression();
            if (!node) return nullptr;
            if (current().type != TokenType::RParen) {
                err = string("Expected ')' "); errpos = static_cast<uint32_t>(idx); return nullptr;
            }
            ++idx;
            return node;
        }
        // unexpected token
        err = string("Unexpected token at primary: ") + t.text;
        errpos = static_cast<uint32_t>(idx);
        return nullptr;
    };

    parseUnary = [&]() -> unique_ptr<ASTNode> {
        const Token &t = current();
        if (t.type == TokenType::Log2 || t.type == TokenType::BNot || t.type == TokenType::LNot || t.type == TokenType::UMinus) {
            TokenType op = t.type;
            ++idx;
            auto operand = parseUnary();
            if (!operand) return nullptr;
            auto node = make_unique<ASTNode>();
            node->type = NodeType::UnaryOp;
            node->op = op;
            node->left = std::move(operand);
            node->pos = t.pos;
            return node;
        }
        return parsePrimary();
    };

    parseMulDiv = [&]() -> unique_ptr<ASTNode> {
        auto left = parseUnary(); if (!left) return nullptr;
        while (true) {
            TokenType op = TokenType::End;
            const Token &t = current();
            if (t.type == TokenType::Mul) op = TokenType::Mul;
            else if (t.type == TokenType::Div) op = TokenType::Div;
            else if (t.type == TokenType::Mod) op = TokenType::Mod;
            else break;
            ++idx;
            auto right = parseUnary(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = t.pos; left = std::move(node);
        }
        return left;
    };

    parseAddSub = [&]() -> unique_ptr<ASTNode> {
        auto left = parseMulDiv(); if (!left) return nullptr;
        while (true) {
            TokenType op = TokenType::End;
            const Token &t = current();
            if (t.type == TokenType::Add) op = TokenType::Add;
            else if (t.type == TokenType::Sub) op = TokenType::Sub;
            else break;
            ++idx;
            auto right = parseMulDiv(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = t.pos; left = std::move(node);
        }
        return left;
    };

    parseShift = [&]() -> unique_ptr<ASTNode> {
        auto left = parseAddSub(); if (!left) return nullptr;
        while (true) {
            TokenType op = TokenType::End; const Token &t = current();
            if (t.type == TokenType::Shl) op = TokenType::Shl;
            else if (t.type == TokenType::Shr) op = TokenType::Shr;
            else break;
            ++idx;
            auto right = parseAddSub(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = t.pos; left = std::move(node);
        }
        return left;
    };

    parseRelational = [&]() -> unique_ptr<ASTNode> {
        auto left = parseShift(); if (!left) return nullptr;
        while (true) {
            TokenType op = TokenType::End; const Token &t = current();
            if (t.type == TokenType::Lt) op = TokenType::Lt;
            else if (t.type == TokenType::Le) op = TokenType::Le;
            else if (t.type == TokenType::Gt) op = TokenType::Gt;
            else if (t.type == TokenType::Ge) op = TokenType::Ge;
            else break;
            ++idx;
            auto right = parseShift(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = t.pos; left = std::move(node);
        }
        return left;
    };

    parseEquality = [&]() -> unique_ptr<ASTNode> {
        auto left = parseRelational(); if (!left) return nullptr;
        while (true) {
            TokenType op = TokenType::End; const Token &t = current();
            if (t.type == TokenType::Eq) op = TokenType::Eq;
            else if (t.type == TokenType::Neq) op = TokenType::Neq;
            else break;
            ++idx;
            auto right = parseRelational(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = t.pos; left = std::move(node);
        }
        return left;
    };

    parseBitAnd = [&]() -> unique_ptr<ASTNode> {
        auto left = parseEquality(); if (!left) return nullptr;
        while (current().type == TokenType::BAnd) {
            size_t tpos = current().pos;
            TokenType op = TokenType::BAnd; ++idx; auto right = parseEquality(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = tpos; left = std::move(node);
        }
        return left;
    };

    parseBitXor = [&]() -> unique_ptr<ASTNode> {
        auto left = parseBitAnd(); if (!left) return nullptr;
        while (current().type == TokenType::BXor) {
            size_t tpos = current().pos;
            TokenType op = TokenType::BXor; ++idx; auto right = parseBitAnd(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = tpos; left = std::move(node);
        }
        return left;
    };

    parseBitOr = [&]() -> unique_ptr<ASTNode> {
        auto left = parseBitXor(); if (!left) return nullptr;
        while (current().type == TokenType::BOr) {
            size_t tpos = current().pos;
            TokenType op = TokenType::BOr; ++idx; auto right = parseBitXor(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = tpos; left = std::move(node);
        }
        return left;
    };

    parseLogicalAnd = [&]() -> unique_ptr<ASTNode> {
        auto left = parseBitOr(); if (!left) return nullptr;
        while (current().type == TokenType::LAnd) {
            size_t tpos = current().pos;
            TokenType op = TokenType::LAnd; ++idx; auto right = parseBitOr(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = tpos; left = std::move(node);
        }
        return left;
    };

    parseLogicalOr = [&]() -> unique_ptr<ASTNode> {
        auto left = parseLogicalAnd(); if (!left) return nullptr;
        while (current().type == TokenType::LOr) {
            size_t tpos = current().pos;
            TokenType op = TokenType::LOr; ++idx; auto right = parseLogicalAnd(); if (!right) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::BinaryOp; node->op = op; node->left = std::move(left); node->right = std::move(right); node->pos = tpos; left = std::move(node);
        }
        return left;
    };

    parseExpression = [&]() -> unique_ptr<ASTNode> {
        // conditional ?: is right-associative and has lowest precedence
        auto cond = parseLogicalOr(); if (!cond) return nullptr;
        if (current().type == TokenType::Question) {
            size_t qpos = current().pos;
            ++idx;
            auto then_branch = parseExpression(); if (!then_branch) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            if (current().type != TokenType::Colon) { err = string("Expected ':' in conditional"); errpos = static_cast<uint32_t>(idx); return nullptr; }
            ++idx;
            auto else_branch = parseExpression(); if (!else_branch) { errpos = static_cast<uint32_t>(idx); return nullptr; }
            auto node = make_unique<ASTNode>(); node->type = NodeType::Conditional; node->cond = std::move(cond); node->then_branch = std::move(then_branch); node->else_branch = std::move(else_branch); node->pos = qpos; return node;
        }
        return cond;
    };

    auto root = parseExpression();
    if (!root) return nullptr;
    if (current().type != TokenType::End) {
        err = string("Unexpected token after expression: ") + current().text;
        errpos = static_cast<uint32_t>(idx);
        return nullptr;
    }
    return root;
}

inline ConfigRealValue evaluateConfigValueExpression(const ASTNode &node, uint32_t &errpos, string &err) {
    err.clear(); errpos = 0;
    switch (node.type) {
        case NodeType::Number:
            return node.value;
        case NodeType::Identifier: {
            err = string("Undefined identifier: ") + node.name;
            errpos = node.pos;
            return 0;
        }
        case NodeType::UnaryOp: {
            ConfigRealValue operand = evaluateConfigValueExpression(*node.left, errpos, err);
            if (!err.empty()) return 0;
            switch (node.op) {
                case TokenType::Log2:
                    if (operand <= 0) {
                        err = string("Log2 of zero or negative number is undefined");
                        errpos = node.pos;
                        return 0;
                    }
                    if (operand & (operand - 1)) {
                        return (sizeof(ConfigRealValue) * 8) - std::__countl_zero(operand);
                    } else {
                        return (sizeof(ConfigRealValue) * 8) - std::__countl_zero(operand) - 1;
                    }
                case TokenType::BNot:
                    return ~operand;
                case TokenType::LNot:
                    return operand == 0 ? 1 : 0;
                case TokenType::UMinus:
                    return -operand;
                default:
                    err = string("Invalid unary operator");
                    errpos = node.pos;
                    return 0;
            }
        }
        case NodeType::BinaryOp: {
            ConfigRealValue left = evaluateConfigValueExpression(*node.left, errpos, err);
            if (!err.empty()) return 0;
            ConfigRealValue right = evaluateConfigValueExpression(*node.right, errpos, err);
            if (!err.empty()) return 0;
            switch (node.op) {
                case TokenType::Add:
                    return left + right;
                case TokenType::Sub:
                    return left - right;
                case TokenType::Mul:
                    return left * right;
                case TokenType::Div:
                    if (right == 0) {
                        err = string("Division by zero");
                        errpos = node.pos;
                        return 0;
                    }
                    return left / right;
                case TokenType::Mod:
                    if (right == 0) {
                        err = string("Modulo by zero");
                        errpos = node.pos;
                        return 0;
                    }
                    return left % right;
                default:
                    err = string("Invalid binary operator");
                    errpos = node.pos;
                    return 0;
            }
        }
        case NodeType::Conditional: {
            ConfigRealValue cond = evaluateConfigValueExpression(*node.cond, errpos, err);
            if (!err.empty()) return 0;
            if (cond != 0) {
                return evaluateConfigValueExpression(*node.then_branch, errpos, err);
            } else {
                return evaluateConfigValueExpression(*node.else_branch, errpos, err);
            }
        }
        default:
            err = string("Invalid AST node type");
            errpos = node.pos;
            return 0;
    }
}

inline unique_ptr<vector<ConfigName>> parseReferencedIdentifier(const string &valuestr, uint32_t &errpos, string &err) {
    auto tokens = tokenizeConfigValueExpression(valuestr, errpos, err);
    if (!tokens) return nullptr;
    auto ast = parseConfigValueExpression(*tokens, errpos, err);
    if (!ast) return nullptr;
    auto out = make_unique<vector<ConfigName>>();
    for (const auto &t : *tokens) {
        if (t.type == TokenType::Identifier) {
            out->push_back(t.text);
        }
    }
    return out;
}

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
