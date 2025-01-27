// ./bin/test_gemm_optimization_nxn -m 2 -ops 7 -epochs 1000 -training_samples 1023 -iteration 1025 -learning_rate 1.0
#include "../include/ceras.hpp"
#include "./parser.hpp"
#include "../include/utils/fmt.hpp"
#include <iostream>
#include <cmath>
#include <cstdlib>

inline int send_message( std::string const& message )
{
    return 0;
}


//(A \* a) .*  (B \* b) * c -> C

int main( int argc, char** argv )
{
    using namespace ceras;
    random_generator.seed( 113 );
    constexpr float stop_threshold = 1.0e-7f;
    constexpr float success_threshold = 1.0e-3f;
    constexpr unsigned long final_iterations = 8;

    unsigned long scale = 16; // search in [-scale, scale]
    //unsigned long scale = 2; // search in [-scale, scale]
    unsigned long m = 2;
    unsigned long ops = 7;
    unsigned long epochs = 1024*4;
    unsigned long training_samples = 1024;
    unsigned long iterations = 1024;
    //float learning_rate = 1.0f;
    float learning_rate = 1.0e-1f;

    auto p_r = parser::make_option<unsigned long>( "-s", [&scale]( unsigned long _m ){ scale = _m; }  );
    auto p_m = parser::make_option<unsigned long>( "-m", [&m]( unsigned long _m ){ m = _m; }  );
    auto p_ops = parser::make_option<unsigned long>( "-ops", [&ops]( unsigned long _ops ){ ops = _ops; }  );
    auto p_epochs = parser::make_option<unsigned long>( "-epochs", [&epochs]( unsigned long _epochs ){ epochs = _epochs; }  );
    auto p_training_samples = parser::make_option<unsigned long>( "-training_samples", [&training_samples]( unsigned long _training_samples ){ training_samples = _training_samples; }  );
    auto p_iterations = parser::make_option<unsigned long>( "-iterations", [&iterations]( unsigned long _iterations ){ iterations = _iterations; }  );
    auto p_learning_rate = parser::make_option<float>( "-learning_rate", [&learning_rate]( float _learning_rate ){ learning_rate = _learning_rate; }  );

    parser::parse( argc, argv, p_m, p_ops, p_epochs, p_training_samples, p_iterations, p_learning_rate );
    {
        std::cout << "Configuration: \n\n";
        std::cout << "m = \t" << m << "\n";
        std::cout << "ops = \t" << ops << "\n\n";
        std::cout << "epochs = \t" << epochs << "\n";
        std::cout << "training_samples = \t" << training_samples << "\n";
        std::cout << "iterations = \t" << iterations << "\n";
        std::cout << "learning_rate = \t" << learning_rate << "\n";


        std::string const report = fmt::format( "Running gemm optimization with m={}, ops={}, epochs={}, training_samples={}, iterations={} and learning_rate={}", m, ops, epochs, training_samples, iterations, learning_rate );
        std::cout << "Generated report:\n" << report << "\n";
        send_message( report );

    }

    // prepare traing data
    tensor<float> train_A = rand<float>( {training_samples, m*m} );
    auto train_A_v = view<float, 3>{ train_A.data(), training_samples, m, m };
    tensor<float> train_B = rand<float>( {training_samples, m*m} );
    auto train_B_v = view<float, 3>{ train_B.data(), training_samples, m, m };
    tensor<float> train_C = zeros<float>( {training_samples, m*m} );
    auto train_C_v = view<float, 3>{ train_C.data(), training_samples, m, m };
    for ( auto idx : ceras::range( training_samples ) )
    {
        for ( auto row : ceras::range( m ) )
            for ( auto k : ceras::range( m ) )
                for ( auto col : ceras::range( m ) )
                    train_C_v[idx][row][col] += train_A_v[idx][row][k] * train_B_v[idx][k][col];
    }

    auto& s = get_default_session<tensor<float>>();

    auto ones_mmop = variable{ ones<float>({m*m, ops}) };
    auto ones_opmm = variable{ ones<float>({ops, m*m}) };

    auto A = place_holder<tensor<float>>();
    s.bind( A, train_A );

    auto tanh_a_ = variable{ randn<float>({m*m, scale}, 0.0f, 1.0f/std::sqrt(m*m*scale*1.0f)) };
    //auto sigmoid_a_ = variable{ randn<float>({m*m, scale}, 0.0f, 1.0f/std::sqrt(m*m*scale*1.0f)) };

    auto tanh__a = variable{ randn<float>({scale, ops}, 0.0f, 1.0f/std::sqrt(scale*ops*1.0f)) };
    //auto sigmoid__a = variable{ randn<float>({scale, ops}, 0.0f, 1.0f/std::sqrt(scale*ops*1.0f)) };

    auto sign_a = variable{ randn<float>( {m*m, ops}, 0.0f, 1.0f/std::sqrt(m*m*ops*1.0f) ) };

    auto B = place_holder<tensor<float>>();
    s.bind( B, train_B );

    auto tanh_b_ = variable{ randn<float>({m*m, scale}, 0.0f, 1.0f/std::sqrt(m*m*scale*1.0f)) };
    auto sigmoid_b_ = variable{ randn<float>({m*m, scale}, 0.0f, 1.0f/std::sqrt(m*m*scale*1.0f)) };

    auto tanh__b = variable{ randn<float>({scale, ops}, 0.0f, 1.0f/std::sqrt(scale*ops*1.0f)) };
    auto sigmoid__b = variable{ randn<float>({scale, ops}, 0.0f, 1.0f/std::sqrt(scale*ops*1.0f)) };

    auto sign_b = variable{ randn<float>( {m*m, ops}, 0.0f, 1.0f/std::sqrt(m*m*ops*1.0f) ) };

    auto C = place_holder<tensor<float>>();
    s.bind( C, train_C );

    auto tanh__c = variable{ randn<float>({ops, scale}, 0.0f, 1.0f/std::sqrt(ops*scale*1.0f)) };
    auto sigmoid__c = variable{ randn<float>({ops, scale}, 0.0f, 1.0f/std::sqrt(ops*scale*1.0f)) };

    auto tanh_c_ = variable{ randn<float>({scale, m*m}, 0.0f, 1.0f/std::sqrt(scale*m*m*1.0f)) };
    auto sigmoid_c_ = variable{ randn<float>({scale, m*m}, 0.0f, 1.0f/std::sqrt(scale*m*m*1.0f)) };

    auto sign_c = variable{ randn<float>( {ops, m*m}, 0.0f, 1.0f/std::sqrt(m*m*ops*1.0f) ) };

    bool found_flag = false;

    for ( auto it : ceras::range( iterations ) )
    {
        if (found_flag)
            break;

        //value<float> alpha{ 1.0f + static_cast<float>(1000.0 * it*it / (iterations*iterations)) };
        value<float> alpha{ 1.0f + 0.05f*it*(1.0f+0.001f*it)};

        // [N, ops] = [N, m^2] * [[m^2, scale] * [scale, ops]]
        //auto AA = A * ( elementwise_product( tanh( alpha * tanh_a_ ), sigmoid( alpha * sigmoid_a_ ) ) *
        //                elementwise_product( tanh( alpha * tanh__a ), sigmoid( alpha * sigmoid__a ) ) );
        auto AA = A * ( elementwise_product( tanh( alpha * tanh_a_ ) * tanh( alpha * tanh__a ), sigmoid( alpha * sign_a ) ) );

        // [N, ops] = [N, m^2] * [[m^2, scale] * [scale, ops]]
        //auto BB = B * ( elementwise_product( tanh( alpha * tanh_b_ ), sigmoid( alpha * sigmoid_b_ ) ) *
        //                elementwise_product( tanh( alpha * tanh__b ), sigmoid( alpha * sigmoid__b ) ) );
        auto BB = B * ( elementwise_product( tanh( alpha * tanh_b_ ) * tanh( alpha * tanh__b ), sigmoid( alpha * sign_b ) ) );

        // [N, ops]
        auto AB = elementwise_product( AA, BB );

        // [N, m^2] =  [N, ops] * [[op, scale] * [scale, m^2]]
        //auto CC = AB * ( elementwise_product( tanh( alpha * tanh__c ), sigmoid( alpha * sigmoid__c ) ) *
        //                 elementwise_product( tanh( alpha * tanh_c_ ), sigmoid( alpha * sigmoid_c_ ) ) );
        auto CC = AB * ( elementwise_product( tanh( alpha * tanh__c ) * tanh( alpha * tanh_c_ ), sigmoid( alpha * sign_c ) ) );
        auto loss = mse( CC, C );

        //auto optimizer = adam{ loss, training_samples, learning_rate*static_cast<float>(1.0-it/iterations) };
        auto optimizer = adam{ loss, 1, learning_rate*static_cast<float>(1.0-it/iterations) };
        for ( auto e : ceras::range( epochs ) )
        {
            auto current_error = s.run( loss );
            s.run( optimizer );
            if (current_error[0] <= stop_threshold && (it >= 512) )
            {
                found_flag = true;
                std::cout << "Maybe found with loss = " << current_error[0] << std::endl;
                std::cout << "BREAK!" << std::endl;
                break;
            }
            std::cout.precision( 8 );
            std::cout << "\33[2K\r";
            std::cout << "\rLoss at iteration\t" << it << ", epoch\t" << e << ":\t" << current_error[0] << std::flush;
            if (current_error[0]<stop_threshold)
            {
                std::cout << "\nError is already small enough, early stop at current iteration." << std::endl;
                break;
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;


    {
        value<float> alpha{ 1.0e2f };

        //auto AA = A * elementwise_product( tanh( alpha * a ), sigmoid( alpha * _a ) ); // shape ( 1, ops )
        //auto AA = A * ( elementwise_product( tanh( alpha * tanh_a_ ), sigmoid( alpha * sigmoid_a_ ) ) *
        //                elementwise_product( tanh( alpha * tanh__a ), sigmoid( alpha * sigmoid__a ) ) );
        auto AA = A * ( elementwise_product( tanh( alpha * tanh_a_ ) * tanh( alpha * tanh__a ), sigmoid( alpha * sign_a ) ) );
        //auto BB = B * elementwise_product( tanh( alpha * b ), sigmoid( alpha * _b ) ); // shape ( 1, ops )
        //auto BB = B * ( elementwise_product( tanh( alpha * tanh_b_ ), sigmoid( alpha * sigmoid_b_ ) ) *
        //                elementwise_product( tanh( alpha * tanh__b ), sigmoid( alpha * sigmoid__b ) ) );
        auto BB = B * ( elementwise_product( tanh( alpha * tanh_b_ ) * tanh( alpha * tanh__b ), sigmoid( alpha * sign_b ) ) );
        auto AB = elementwise_product( AA, BB ); // shape (1, ops ), real multiplication happens here
        //auto CC = AB * elementwise_product( tanh( alpha * c ), sigmoid( alpha * _c ) ); // shape ( 1, ops )
        //auto CC = AB * ( elementwise_product( tanh( alpha * tanh__c ), sigmoid( alpha * sigmoid__c ) ) *
        //                 elementwise_product( tanh( alpha * tanh_c_ ), sigmoid( alpha * sigmoid_c_ ) ) );
        auto CC = AB * ( elementwise_product( tanh( alpha * tanh__c ) * tanh( alpha * tanh_c_ ), sigmoid( alpha * sign_c ) ) );
        auto loss = mse( CC, C );
        auto optimizer = adam{ loss, training_samples, 1.0e-10f };

        for ( [[maybe_unused]] auto idx : ceras::range( final_iterations ) )
        {
            (void) idx;
            s.run( loss );
            s.run( optimizer );
        }

        auto current_error = s.run( loss );
        std::cout << "The final error is : " << current_error[0] << std::endl;
        send_message( fmt::format("Finished with error: {}.\n", current_error[0]) );

        //if ( (current_error[0] > success_threshold) && (!found_flag) ) return 0;

        if (current_error[0] >= success_threshold)
        {
            std::cout << "Failed to solve with a big error of " << current_error[0] << std::endl;
            //return 0;
        }

        {
            auto op = ( elementwise_product( tanh( alpha * tanh_a_ ) * tanh( alpha * tanh__a ), sigmoid( alpha * sign_a ) ) );
            auto _ = s.run( op );
            std::cout << "AA is \n" << _ << std::endl;

            send_message( fmt::format( "AA = {}\n", _ ) );
        }

        {
            //auto op = ( elementwise_product( tanh( alpha * tanh_b_ ), sigmoid( alpha * sigmoid_b_ ) ) *
            //            elementwise_product( tanh( alpha * tanh__b ), sigmoid( alpha * sigmoid__b ) ) );
            auto op = ( elementwise_product( tanh( alpha * tanh_b_ ) * tanh( alpha * tanh__b ), sigmoid( alpha * sign_b ) ) );
            auto _ = s.run( op );
            std::cout << "BB is \n" << _ << std::endl;

            send_message( fmt::format( "BB = {}\n", _ ) );
        }

        {
            //auto op = ( elementwise_product( tanh( alpha * tanh__c ), sigmoid( alpha * sigmoid__c ) ) *
            //             elementwise_product( tanh( alpha * tanh_c_ ), sigmoid( alpha * sigmoid_c_ ) ) );
            auto op = ( elementwise_product( tanh( alpha * tanh__c ) * tanh( alpha * tanh_c_ ), sigmoid( alpha * sign_c ) ) );
            auto _ = s.run( op );
            std::cout << "CC is \n" << _ << std::endl;

            send_message( fmt::format( "CC = {}\n", _ ) );
        }

    }

    return 0;
}

