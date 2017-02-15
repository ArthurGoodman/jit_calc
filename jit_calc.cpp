#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <exception>

class Node {
public:
    virtual ~Node() {
    }

    virtual double eval() = 0;
};

class ValueNode : public Node {
    double value;

public:
    ValueNode(double value)
        : value(value) {
    }

    double eval() {
        return value;
    }
};

class BinaryNode : public Node {
    char op;
    Node *left, *right;

public:
    BinaryNode(Node *left, char op, Node *right)
        : op(op)
        , left(left)
        , right(right) {
    }

    double eval() {
        switch (op) {
        case '+':
            return left->eval() + right->eval();

        case '-':
            return left->eval() - right->eval();

        case '*':
            return left->eval() * right->eval();

        case '/':
            return left->eval() / right->eval();

        case '^':
            return pow(left->eval(), right->eval());

        default:
            return NAN;
        }
    }
};

struct Token {
    char id;
    std::string text;
};

class Lexer {
public:
    std::vector<Token> lex(const std::string &expr) {
        size_t pos = 0;
        std::vector<Token> tokens;

        while (true) {
            while (isspace(expr[pos]))
                pos++;

            if (expr[pos] == '\0') {
                tokens.push_back({ 'e', "" });
                break;
            } else if (isdigit(expr[pos])) {
                std::string text;

                while (isdigit(expr[pos]))
                    text += expr[pos++];

                if (expr[pos] == '.')
                    do
                        text += expr[pos++];
                    while (isdigit(expr[pos]));

                tokens.push_back({ 'n', text });
            } else if (isalpha(expr[pos])) {
                std::string text;

                while (isalnum(expr[pos]))
                    text += expr[pos++];

                tokens.push_back({ 'u', text });
            } else
                tokens.push_back({ (std::string("+-*/^()").find(expr[pos]) != std::string::npos ? expr[pos] : 'u'), std::string() + expr[pos++] });
        }

        return tokens;
    }
};

class Parser {
    std::vector<Token> tokens;
    std::vector<Token>::const_iterator token;

public:
    Node *parse(const std::vector<Token> &tokens) {
        this->tokens = tokens;
        token = this->tokens.begin();

        Node *n = addSub();

        if (!check('e'))
            throw std::runtime_error("there's an excess part of expression");

        return n;
    }

private:
    void getToken() {
        ++token;
    }

    bool check(char id) {
        return token->id == id;
    }

    bool accept(char id) {
        if (check(id)) {
            getToken();
            return true;
        }

        return false;
    }

    Node *addSub() {
        Node *n = mulDiv();

        while (true) {
            if (accept('+'))
                n = new BinaryNode(n, '+', mulDiv());
            else if (accept('-'))
                n = new BinaryNode(n, '-', mulDiv());
            else
                break;
        }

        return n;
    }

    Node *mulDiv() {
        Node *n = power();

        while (true) {
            if (accept('*'))
                n = new BinaryNode(n, '*', power());
            else if (accept('/'))
                n = new BinaryNode(n, '/', power());
            else
                break;
        }

        return n;
    }

    Node *power() {
        Node *n = unary();

        while (true) {
            if (accept('^'))
                n = new BinaryNode(n, '^', unary());
            else
                break;
        }

        return n;
    }

    Node *unary() {
        Node *n = nullptr;

        if (accept('+'))
            n = new BinaryNode(new ValueNode(0), '+', term());
        else if (accept('-'))
            n = new BinaryNode(new ValueNode(0), '-', term());
        else
            n = term();

        return n;
    }

    Node *term() {
        Node *n = nullptr;

        if (check('n')) {
            n = new ValueNode(std::stod(token->text));
            getToken();
        } else if (accept('(')) {
            n = addSub();

            if (!accept(')'))
                throw std::runtime_error("unmatched parentheses");
        } else if (check('u'))
            throw std::runtime_error("unknown token '" + token->text + "'");
        else if (check('e'))
            throw std::runtime_error("unexpected end of expression");
        else
            throw std::runtime_error("unexpected token '" + token->text + "'");

        return n;
    }
};

int main() {
    Lexer lexer;
    Parser parser;

    while (true) {
        std::cout << "$ ";

        std::string line;
        std::getline(std::cin, line);

        if (line.empty())
            continue;
        else if (line == "exit")
            break;
        else if (line == "cls") {
            system("cls");
            continue;
        } else {
            try {
                Node *n = parser.parse(lexer.lex(line));
                std::cout << n->eval() << "\n";
            } catch (const std::exception &e) {
                std::cout << "error: " << e.what() << "\n";
            }
        }

        std::cout << "\n";
    }

    return 0;
}
