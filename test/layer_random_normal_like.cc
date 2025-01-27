#include "../include/ceras.hpp"
#include <iostream>

int main()
{
    using namespace ceras;

    auto a = variable{ ones<float>( {3, 3} ) };
    auto r = random_normal_like( 1.0, 4.0 )( a );
    auto& s = ceras::get_default_session<ceras::tensor<float>>();
    std::cout <<  s.run( r );

    return 0;
}

