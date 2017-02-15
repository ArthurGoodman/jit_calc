#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <exception>
#include <stack>
#include <memory>

typedef unsigned char byte;

class VM {
public:
    enum ByteCode {
        Push,
        Add,
        Sub,
        Mul,
        Div,
        Pow,
        Ret
    };

    double run(const std::vector<byte> &code) {
        std::stack<double> stack;
        size_t ip = 0;

        double left, right;

        while (true)
            switch (code[ip]) {
            case Push:
                stack.push(*reinterpret_cast<const double *>(code.data() + ++ip));
                ip += sizeof(double);
                break;

            case Add:
                right = stack.top();
                stack.pop();
                left = stack.top();
                stack.pop();
                stack.push(left + right);
                ip++;
                break;

            case Sub:
                right = stack.top();
                stack.pop();
                left = stack.top();
                stack.pop();
                stack.push(left - right);
                ip++;
                break;

            case Mul:
                right = stack.top();
                stack.pop();
                left = stack.top();
                stack.pop();
                stack.push(left * right);
                ip++;
                break;

            case Div:
                right = stack.top();
                stack.pop();
                left = stack.top();
                stack.pop();
                stack.push(left / right);
                ip++;
                break;

            case Pow:
                right = stack.top();
                stack.pop();
                left = stack.top();
                stack.pop();
                stack.push(pow(left, right));
                ip++;
                break;

            case Ret:
                return stack.top();

            default:
                return NAN;
            }
    }
};

class Node;

class Compiler {
    std::vector<byte> code;

public:
    std::vector<byte> compile(std::unique_ptr<Node> tree);

    void gen(VM::ByteCode value) {
        code.push_back(value);
    }

    void gen(double value) {
        code.insert(code.end(), sizeof(value), 0);
        *reinterpret_cast<double *>(code.data() + code.size() - sizeof(value)) = value;
    }
};

class Node {
public:
    virtual ~Node() {
    }

    virtual double eval() = 0;
    virtual void compile(Compiler *c) = 0;
};

std::vector<byte> Compiler::compile(std::unique_ptr<Node> tree) {
    code.clear();

    tree->compile(this);
    gen(VM::Ret);

    return code;
}

class ValueNode : public Node {
    double value;

public:
    ValueNode(double value)
        : value(value) {
    }

    double eval() {
        return value;
    }

    void compile(Compiler *c) {
        c->gen(VM::Push);
        c->gen(value);
    }
};

class BinaryNode : public Node {
protected:
    Node *left, *right;

    BinaryNode(Node *left, Node *right)
        : left(left)
        , right(right) {
    }

    ~BinaryNode() {
        delete left;
        delete right;
    }
};

class PlusNode : public BinaryNode {
public:
    PlusNode(Node *left, Node *right)
        : BinaryNode(left, right) {
    }

    double eval() {
        return left->eval() + right->eval();
    }

    void compile(Compiler *c) {
        left->compile(c);
        right->compile(c);

        c->gen(VM::Add);
    }
};

class MinusNode : public BinaryNode {
public:
    MinusNode(Node *left, Node *right)
        : BinaryNode(left, right) {
    }

    double eval() {
        return left->eval() - right->eval();
    }

    void compile(Compiler *c) {
        left->compile(c);
        right->compile(c);

        c->gen(VM::Sub);
    }
};

class MultiplyNode : public BinaryNode {
public:
    MultiplyNode(Node *left, Node *right)
        : BinaryNode(left, right) {
    }

    double eval() {
        return left->eval() * right->eval();
    }

    void compile(Compiler *c) {
        left->compile(c);
        right->compile(c);

        c->gen(VM::Mul);
    }
};

class DivideNode : public BinaryNode {
public:
    DivideNode(Node *left, Node *right)
        : BinaryNode(left, right) {
    }

    double eval() {
        return left->eval() / right->eval();
    }

    void compile(Compiler *c) {
        left->compile(c);
        right->compile(c);

        c->gen(VM::Div);
    }
};

class PowerNode : public BinaryNode {
public:
    PowerNode(Node *left, Node *right)
        : BinaryNode(left, right) {
    }

    double eval() {
        return pow(left->eval(), right->eval());
    }

    void compile(Compiler *c) {
        left->compile(c);
        right->compile(c);

        c->gen(VM::Pow);
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
    std::unique_ptr<Node> parse(const std::vector<Token> &tokens) {
        this->tokens = tokens;
        token = this->tokens.begin();

        Node *n = addSub();

        if (!check('e'))
            throw std::runtime_error("there's an excess part of expression");

        return std::unique_ptr<Node>(n);
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
                n = new PlusNode(n, mulDiv());
            else if (accept('-'))
                n = new MinusNode(n, mulDiv());
            else
                break;
        }

        return n;
    }

    Node *mulDiv() {
        Node *n = power();

        while (true) {
            if (accept('*'))
                n = new MultiplyNode(n, power());
            else if (accept('/'))
                n = new DivideNode(n, power());
            else
                break;
        }

        return n;
    }

    Node *power() {
        Node *n = unary();

        while (true) {
            if (accept('^'))
                n = new PowerNode(n, unary());
            else
                break;
        }

        return n;
    }

    Node *unary() {
        Node *n = nullptr;

        if (accept('+'))
            n = new PlusNode(new ValueNode(0), term());
        else if (accept('-'))
            n = new MinusNode(new ValueNode(0), term());
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
    while (true) {
        std::cout << "$ ";

        std::string str;
        std::getline(std::cin, str);

        if (str.empty())
            continue;
        else if (str == "exit")
            break;
        else if (str == "cls") {
            system("cls");
            continue;
        } else {
            try {
                Lexer lexer;
                Parser parser;
                Compiler compiler;
                VM vm;

                std::cout << vm.run(compiler.compile(parser.parse(lexer.lex(str)))) << "\n";
            } catch (const std::exception &e) {
                std::cout << "error: " << e.what() << "\n";
            }
        }

        std::cout << "\n";
    }

    return 0;
}
