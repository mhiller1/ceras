#include "../include/ceras.hpp"
#include <iostream>

int main()
{
    using namespace ceras;

    auto a = variable{ ones<float>( {2, 2} ) };
    auto la = log(a);
    ceras::session<ceras::tensor<double>> s;
    std::cout <<  s.run( la );

    return 0;
}
