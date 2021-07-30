#include <iostream>
#include <vector>
#include <thread>
#include <immintrin.h>

volatile static int i = 0;

int f()
{
    int status;
    while ((status = _xbegin ()) != _XBEGIN_STARTED) ;

    i++;
    //FIXME: anyone can tell me why program will hang if cout here?
    //std::cout << "o" << std::endl;;
    _xend();

    return i;
}

int main()
{
    std::vector<std::thread> v(10);
    for(auto& t: v)
        t = std::thread([]{ for(int n = 0; n < 10; ++n) f(); });
    for(auto& t: v)
        t.join();
    std::cout << "transactional: " << i << std::endl;
}
