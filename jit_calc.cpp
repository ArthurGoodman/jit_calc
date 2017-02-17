#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <exception>
#include <memory>
#include <ctime>
#include <chrono>

#include "compiler.h"

typedef unsigned char byte;

struct Function {
    std::vector<byte> code;
    int stackSize;
};

class VM {
    double *stack = nullptr;
    size_t stackSize;
    byte *code;

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

    void allocate(size_t size) {
        delete stack;

        stack = new double[size];
        stackSize = size;
    }

    void setCode(byte *code) {
        this->code = code;
    }

    double run() {
        byte *ip = code;
        double *sp = stack + stackSize;

        while (true)
            switch (*(ip++)) {
            case Push:
                *(--sp) = *reinterpret_cast<const double *>(ip);
                ip += sizeof(double);
                break;

            case Add:
                *(sp + 1) += *sp;
                sp++;
                break;

            case Sub:
                *(sp + 1) -= *sp;
                sp++;
                break;

            case Mul:
                *(sp + 1) *= *sp;
                sp++;
                break;

            case Div:
                *(sp + 1) /= *sp;
                sp++;
                break;

            case Pow:
                *(sp + 1) = pow(*(sp + 1), *sp);
                sp++;
                break;

            case Ret:
                return *sp;

            default:
                throw std::runtime_error("invalid byte code");
            }

        return NAN;
    }

    x86::Function compile(const Function &f) {
        const byte *ip = f.code.data();
        int stackSize = f.stackSize;

        x86::Compiler c;

        c.push(x86::EBP);
        c.mov(x86::ESP, x86::EBP);
        c.sub(c.abs("stackSize"), x86::ESP);

        int sp = 0;

        std::vector<double> data;

        while (true)
            switch (*(ip++)) {
            case Push:
                if (ip > f.code.data() + 1)
                    c.fstpl(c.ref(-sp, x86::EBP));

                sp += 8;

                c.fldl(c.ref(c.abs("data") + data.size() * sizeof(double)));
                data.push_back(*reinterpret_cast<const double *>(ip));

                ip += sizeof(double);
                break;

            case Add:
                c.faddl(c.ref(-(sp -= 8), x86::EBP));
                break;

            case Sub:
                c.fsubrl(c.ref(-(sp -= 8), x86::EBP));
                break;

            case Mul:
                c.fmull(c.ref(-(sp -= 8), x86::EBP));
                break;

            case Div:
                c.fdivrl(c.ref(-(sp -= 8), x86::EBP));
                break;

            case Pow:
                c.fldl(c.ref(-sp + 8, x86::EBP));
                c.fstpl(c.ref(x86::ESP));
                c.fstpl(c.ref(8, x86::ESP));
                c.call(c.rel("pow"));

                stackSize = std::max(stackSize, static_cast<int>(sp + 16));
                sp -= 8;
                break;

            case Ret: {
                c.leave();
                c.ret();

                for (const double &constant : data)
                    c.constant(constant);

                const ByteArray &code = c.getCode();

                c.relocate("data", reinterpret_cast<int>(code.data() + code.size() - data.size() * sizeof(double)));
                c.relocate("stackSize", stackSize - 8);
                c.relocate("pow", reinterpret_cast<int>(pow));

                c.writeOBJ().write("a.o");
                system("objdump -d a.o");
                std::cout << "\n";

                return c.compileFunction();
            }

            default:
                throw std::runtime_error("invalid byte code");
            }

        return x86::Function();
    }
};

class Compiler;

class Node {
public:
    virtual ~Node() {
    }

    virtual double eval() = 0;
    virtual void compile(Compiler *c) = 0;
};

class Compiler {
    std::vector<byte> code;
    int sp, stackSize;

public:
    Function compile(std::shared_ptr<Node> tree) {
        code.clear();

        sp = 0;
        stackSize = 0;

        tree->compile(this);
        gen(VM::Ret);

        return { code, stackSize };
    }

    void gen(VM::ByteCode value) {
        code.push_back(value);
    }

    void gen(double value) {
        code.insert(code.end(), sizeof(value), 0);
        *reinterpret_cast<double *>(code.data() + code.size() - sizeof(value)) = value;
    }

    void push() {
        sp += 8;
        stackSize = std::max(stackSize, sp);
    }

    void pop() {
        sp -= 8;
    }
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

    void compile(Compiler *c) {
        c->gen(VM::Push);
        c->gen(value);
        c->push();
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
        c->pop();
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
        c->pop();
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
        c->pop();
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
        c->pop();
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
        c->pop();
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
    std::shared_ptr<Node> parse(const std::vector<Token> &tokens) {
        this->tokens = tokens;
        token = this->tokens.begin();

        Node *n = addSub();

        if (!check('e'))
            throw std::runtime_error("there's an excess part of expression");

        return std::shared_ptr<Node>(n);
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
    Lexer lexer;
    Parser parser;
    Compiler compiler;
    VM vm;

    std::cout << std::setprecision(16);

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
        } else if (str == "test") {
            const char *expr = "2 * (3 + 1 / 2) - 6 + 2 * (3 + 1 / 2) - 6 + 2 * (3 + 1 / 2) - 6 + 2 * (3 + 1 / 2) - 6 + 2 * (3 + 1 / 2) - 6";

            std::shared_ptr<Node> tree = parser.parse(lexer.lex(expr));
            Function func = compiler.compile(tree);
            x86::Function fObj = vm.compile(func);
            double (*f)() = reinterpret_cast<double (*)()>(fObj.getCode());

            vm.allocate(func.stackSize);
            vm.setCode(func.code.data());

            const int N = 1000000;
            double sum;

            std::chrono::high_resolution_clock::time_point begin, end;

            begin = std::chrono::high_resolution_clock::now();
            sum = 0;
            for (int i = 0; i < N; i++)
                sum += tree->eval();
            end = std::chrono::high_resolution_clock::now();

            std::cout << "tree:     sum=" << sum << " time=" << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " msec\n";

            begin = std::chrono::high_resolution_clock::now();
            sum = 0;
            for (int i = 0; i < N; i++)
                sum += vm.run();
            end = std::chrono::high_resolution_clock::now();

            std::cout << "bytecode: sum=" << sum << " time=" << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " msec\n";

            begin = std::chrono::high_resolution_clock::now();
            sum = 0;
            for (int i = 0; i < N; i++)
                sum += f();
            end = std::chrono::high_resolution_clock::now();

            std::cout << "x86 code: sum=" << sum << " time=" << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " msec\n";
        } else {
            try {
                x86::Function f = vm.compile(compiler.compile(parser.parse(lexer.lex(str))));
                std::cout << reinterpret_cast<double (*)()>(f.getCode())() << "\n";
            } catch (const std::exception &e) {
                std::cout << "error: " << e.what() << "\n";
            }
        }

        std::cout << "\n";
    }

    return 0;
}
