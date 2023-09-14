#include <libthreadpool/libthreadpool.h>

#include <iostream>

int main(int, char*[])
{
    auto sum = libthreadpool::add(1, 1);
    std::cout << sum << std::endl;
    return 0;
}
