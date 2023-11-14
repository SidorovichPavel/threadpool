#include <threadpool/threadpool.hpp>

#include <format>
#include <iostream>
#include <string_view>
#include <tuple>

int foo(int a, int b) {
    return a + b;
}

// *f /= *f + 1.f
void foo2(float* f) {
    *f /= *f + 1.f;
}

// f /= f + 1.f
void foo3(float& f) {
    f /= f + 1.f;
}

class Bar {
public:
    int foo(int a, int b) { return a + b; }
};

#define test(expr, cmp) std::cout << std::format("test {} on line {} : {}", #expr, __builtin_LINE(), ((expr) == (cmp) ? "PASS" : "FAILED")) << std::endl

int main(int argc, char* args[]) {

    threadpool::threadpool pool(10);

    test(pool.enqueue(&foo, 4, 6).get(), 10);
    test(pool.enqueue([](int a, int b) {return a + b + 1;}, 4, 5).get(), 10);

    Bar bar;
    test(pool.enqueue(&Bar::foo, bar, 4, 6).get(), 10);

    std::function<int(int, int)> fnc = [](int a, int b) {return a + b;};
    test(pool.enqueue(fnc, 4, 6).get(), 10);

    auto r1{ 0.5f };
    pool.enqueue(&foo2, &r1).get();
    test(r1, 0.5f / 1.5f);

    auto r2 = 0.5f;
    pool.enqueue(&foo3, r2).get();
    test(r2, 0.5f / 1.5f);

    return 0;
}