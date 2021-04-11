# Ceras

----


__ceras__ is yet another tiny deep learning engine.  This library mimiks tensorflow 1.x APIs, in pure C++20 and is header-only. CUDA acceleration is optional to _convolutional_ and _dense_ layers, as __ceras__ is written for ordinary devices such as a gaming laptop with a GeForce GTX 1060, in which the GPU memory is limited.

----


## Table of contents

* [Features](#features)
* [Build](#build)
* [Design](#design)
* [Examples](#examples)
* [Supported layers/operations](#supported-layers)
* [TODO](#todo)
* [License](#license)
* [Acknowledgements](#acknowledgements)


----

## Features
- Fast, with/without GPU:
    - 98% accuracy on MNIST in 10 epochs in 30s (loading dataset, training and validation on a laptop with Intel(R) Core(TM) i7-7700HQ and a mobile GTX 1060)
- Portable:
    - Runs anywhere as long as you have a compiler which supports C++20;
    - A Nvidia GPU is optional for acceleration, not a must;
    - header-only.
- Simply implemented:
    - mimicking Tensorflow grammar, but in C++.


## Build
__Using this library__:

copy the `include` directory to the working directory, then include the header file

```cpp
#include "ceras.hpp"
```

**Compile/link**:

```bash
g++ -c -std=c++20 -Wall -Wextra -ferror-limit=1 -ftemplate-backtrace-limit=0 -funsafe-math-optimizations  -Ofast -flto -pipe -march=native -DNDEBUG -o ./obj/test_mnist.o test/mnist.cc
g++ -o ./bin/test_mnist ./obj/test_mnist.o -funsafe-math-optimizations  -Ofast -flto -pipe -march=native
```

CUDA could be optionally enabled by defining macro `CUDA`: (tested with cuda 11.2.r11.2, gcc 10.2.0, note the compile/link options)

```bash
g++ -c -std=c++20 -Wall -Wextra -fmax-errors=1 -ftemplate-backtrace-limit=0 -funsafe-math-optimizations  -Ofast -flto -pipe -march=native -DCUDA -DNDEBUG -o ./obj/test_mnist.o test/mnist.cc
g++ -funsafe-math-optimizations  -Ofast -flto -pipe -march=native -o ./bin/test_mnist ./obj/test_mnist.o -L/opt/cuda/lib64 -pthread  -lcudart -lcublas
```

Note: As [Non-Type Template Parameters](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0732r2.pdf) is not yet implemented in clang, only gcc works with this library.

## Design



### [tensor](./include/tensor.hpp)
A `tensor` variable holds a multiple dimensional array.  A `tensor` instance can be generated by
```cpp
ceras::tensor<float> a{{2, 1, 2}, {0.0f, 0.1f, 0.2f, 0.3f}};
```
in which the template parameter `float` is for the data type, the first argument `{2, 1, 2}` is for the tensor shape, and the second argument `{0.0f, 0.1f, 0.2f, 0.3f}` is for the data stored in the tensor.

Quite a few operations, such as `+`, `-`, `*`,  `abs`,  `random`, `randn`, `reduce` and `max` are implemented for `tensor`. But these operations are there to serve the purpose of deep learning, not intend to be a generic tensor library.


### [constant](./include/constant.hpp)
A `constant` variable  holds a `tensor` instance, and this `tensor` is not supposed to be updated in its life-time.

```cpp
ceras::tensor<float> eye{{2, 2}, {1.0f, 0.0f, 0.0f, 1.0f}};
ceras::constant<ceras::tensor<float>> c_eye{eye};
```

### [place_holder](./include/place_holder.hpp)
A `place_holder` variable holds a position that a `tensor` will be fed later.

```cpp
ceras::place_holder<ceras::tensor<float>> input{};
// ......
session<ceras::float<float>> s;
ceras::tensor<float> a{{2, 1, 2}, {0.0f, 0.1f, 0.2f, 0.3f}};
s.bind(input, a ); // binding a tensor to a place_holder
```

### [variable](./include/variable.hpp)

A `variable` variable  holds a stateful `tensor`, and this `tensor` will be updated anytime. This is designed for the weights in a neural network, which will be updated in every epoch of the training.

```cpp
auto w = ceras::variable{ ceras::randn<float>( {28*28, 256}, 0.0, 10.0/(28.0*16.0) ) };
```

### [operation](./include/operation.hpp) and  computation graph
__ceras__ uses [expression template](https://en.wikipedia.org/wiki/Expression_templates) to represent a computation graph. A computation graph is a directed graph in which each node corresponds to a `variable`, a `place_holder`, a `constant` or an `operation`.  In __ceras__, these node types are grouped in a `Expression` concept.

For example, a computation graph computes output _Expression_ `z` of two input _Expression_ `x` and `y`. Here `x` and `y` are two input nodes of `z`, and `z` is the consumer of `x` and `y`.

![x+y=z](./assets/x_y_z.png)

If `x` and `y` are two tensors are to be binded in a later stage, the corresponding code is
```cpp
auto x = ceras::place_holder<ceras::tensor<float>>{};
auto y = ceras::place_holder<ceras::tensor<float>>{};
auto z = x + y;
```

This kind of expression is more useful when the computation is getting more complex, for example `z = σ(A*x+b)`

![axb](./assets/axy.png)

in which `x`, `A` and `b` are `variable`s / `place_holder`s / `constant`s, and `*`, `+` and `σ` are `operations`s.

If `A` and `b` are two variables, and `x` is a place_holder, then the corresponding code is
```cpp
auto x = ceras::place_holder<ceras::tensor<float>>{};
auto A = ceras::variable{ ceras::ones<float>({3, 3}) };//just for demostration, should not be initialized to ones
auto b = ceras::variable{ ceras::zeros<float>({3,}) };
auto z = sigmoid( A*x + b );
```

### [session](./include/session.hpp)

To evaluate the operations (computation graph), we need a session.

```cpp
auto s = ceras::session<ceras::tensor<float>>{};
```

Then we can bind a tensor to `x`,
```cpp
auto X = ceras::tensor<float>{{3,}, {1.0f, 2.0f, 3.0f}};
s.bind(x, X);
```

And evaluate the output at node `z`:
```cpp
auto result = s.run(z);
```

This will generate a result tensor with shape `(3,)` and values `(0.997527, 0.997527,0.997527)`. In addition, the `x`,  `A` and `b` can also be evaluated by calling
```cpp
auto _x = s.run(x);
auto _A = s.run(A);
auto _b = s.run(b);
```

By design, an instance of an expression has a builtin `forward()` method. When a session runs an expression, the `forward()` method will be invoked.

Please find the complete code from [this file](./test/session.cc).

### [loss](./include/loss.hpp)

A `loss` variable provides a metric between the expected output and the actual output of the computation graph. And a `loss` is implemented as an `Expression`. For example, the `mae` loss can be defined as

```cpp
template < Expression Lhs_Expression, Expression Rhs_Expression >
auto constexpr mae( Lhs_Expression const& ground_truth, Rhs_Expression const& output ) noexcept
{
    return mean_reduce(abs(ground_truth - output));
};
```
in which `mean_reduce`, `abs` and `-` are predefined operations. Usually the `ground_truth` is just a place_holder variable, and will be rebinded at every training epoch.

We can define our loss operation with a place_holder for the ground_truth

```cpp
auto ground_truth = ceras::place_holder<tensor<float>>{};
auto loss = mae(ground_truth, z);
```

### [optimizer](./include/optimizer.hpp)

An `optimizer` variable holds an instance of an `expression` of loss. When an session runs an optimizer, the builtin method `forward()` will be invoked. And we define an optimizer this way:

```cpp
unsigned long batch_size = ...;
float learning_rate = ...;
auto optimizer = ceras::sgd{loss, batch_size, learning_rate};
```

In a single epoch, we first execute a forward pass on the loss, with input `x` and `ground_truth` having been binded:
```cpp
s.bind( x, ...);
s.bind(ground_truth, ...);
s.run(loss);
```

then we execute a backward pass with the optimizer:
```cpp
s.run(optimizer);
```

By repeating forward pass and backward pass multiple times, the weights A and the bias b can be gradually updated.


### more details

__TODO__

## Examples


### implement VGG16

There are a few pre-defined layers in file `./include/layer.hpp`, such as `Input`, `Conv2D` and `Dense`. Starting from thses layers, we are already able to build a VGG16 model.


The input layer for VGG16 is defined as
```cpp
auto input = Input(); //  3D tensor input, (batch_size, 224, 224, 3)
```
followed by a convolutional layer and a relu activation
```cpp
auto l0 = relu( Conv2D( 64, {3, 3}, {224, 3, 3}, "same" )(input) ); // 224, 224, 64
```
and a max pooling layer
```cpp
auto l1 = max_pooling_2d( 2 ) ( l0 ); // 112, 112, 64
```
Then 2 convolutional layers and a max pooling layer
```cpp
auto l2 = relu( Conv2D( 128, {3, 3}, {112, 112, 64}, "same" )( l1 ) ); // 112, 112, 128
auto l3 = relu( Conv2D( 128, {3, 3}, {112, 112, 128}, "same" )( l2 ) ); // 112, 112, 128
auto l4 = max_pooling_2d( 2 ) ( l3 ); // 56, 56, 128
```
followed by 3 convolutional layers and a max pooling layer
```cpp
auto l5 = relu( Conv2D( 256, {3, 3}, {56, 56, 128}, "same" )( l4 ) ); // 56, 56, 256
auto l6 = relu( Conv2D( 256, {3, 3}, {56, 56, 256}, "same" )( l5 ) ); // 56, 56, 256
auto l7 = relu( Conv2D( 256, {3, 3}, {56, 56, 256}, "same" )( l6 ) ); // 56, 56, 256
auto l8 = max_pooling_2d( 2 ) ( l7 ); // 28, 28, 256
```
followed by another 3 convolutional layers and a max pooling layer
```cpp
auto l9 = relu( Conv2D( 512, {3, 3}, {28, 28, 256}, "same" )( l8 ) ); // 28, 28, 512
auto l10 = relu( Conv2D( 512, {3, 3}, {28, 28, 512}, "same" )( l9 ) ); // 28, 28, 512
auto l11 = relu( Conv2D( 512, {3, 3}, {28, 28, 512}, "same" )( l10 ) ); // 28, 28, 512
auto l12 = max_pooling_2d( 2 ) ( l11 ); // 14, 14, 512
```
and again
```cpp
auto l13 = relu( Conv2D( 512, {3, 3}, {14, 14, 512}, "same" )( l12 ) ); // 14, 14, 512
auto l14 = relu( Conv2D( 512, {3, 3}, {14, 14, 512}, "same" )( l13 ) ); // 14, 14, 512
auto l15 = relu( Conv2D( 512, {3, 3}, {14, 14, 512}, "same" )( l14 ) ); // 14, 14, 512
auto l16 = max_pooling_2d( 2 ) ( l15 ); // 7, 7, 512
```
then this 3d layer is flattened to 1d
```cpp
auto l17 = flatten( l16 ); // 7x7x512
```
followed by a dense layer
```cpp
auto l18 = relu( Dense( 4096, 7*7*512 )( l17 ) ); // 4096
```
and then 2 dense layers to the output layer
```cpp
auto l19 = relu( Dense( 4096, 4096 )( l18 ) ); // 4096
auto l20 = relu( Dense( 1000, 4096 )( l19 ) ); // 1000
auto output = l20;
```

With above codes, VGG16 model has been build. However, we not train this model here as we do not have the training set yet. But we can demonstrate the training process with mnist, which is a dataset much smaller than imagenet.



### [defining a 3-layered NN, 256+128 hidden units](./test/mnist_mini.cc) for mnist

**define a 3 layer model**

```cpp
// define computation graph, a 3-layered dense net with topology 784x256x128x10
using namespace ceras;
auto input = Input();

// 1st layer
auto l1 = relu( Dense( 256, 28*28 )( input ) );
// or enabling BN
//auto l1 = relu( BatchNormalization( {256,} )( Dense( 256, 28*28 )( input ) ) );

// 2nd layer
auto l2 = sigmoid( Dense( 128, 256 )( l1 ) );

// 3rd layer
auto output = Dense( 10, 128 )( l2 );

auto ground_truth = place_holder<tensor<float>>{}; // 1-D, 10
auto loss = cross_entropy_loss( ground_truth, output );
```

**preparing dataset**

```cpp
std::size_t const batch_size = 10;
tensor<float> input_images{ {batch_size, 28*28} };
tensor<float> output_labels{ {batch_size, 10} };

std::size_t const epoch = 1;
std::size_t const iteration_per_epoch = 60000/batch_size;
```

**prepare session**

```cpp
// creating session
session<tensor<float>> s;
s.bind( input, input_images );
s.bind( ground_truth, output_labels );
```

**define optimizer**

```cpp
float learning_rate = 1.0e-1f;
auto optimizer = gradient_descent{ loss, batch_size, learning_rate };
```


**start training**

```cpp
for ( auto e : range( epoch ) )
{
    for ( auto i : range( iteration_per_epoch ) )
    {
        s.run( loss ); //forward pass
        s.run( optimizer ); //backward pass
    }
}
```

**make prediction**

```cpp
std::size_t new_batch_size = 1;
tensor<float> new_input_images{ {new_batch_size, 28 * 28} };
s.bind( input, new_input_images );

for ( auto i : range( tests ) )
{
    //prepare new_input_images as inputs
    auto precition = s.run( output );
    //post precess prediction
}
```

### [alternative] [define a convolutional model](./test/mnist_conv2d_mini.cc)

```cpp
using namespace ceras;
auto input = Input(); // 28*28
auto l0 = reshape( {28, 28, 1} )( input ); // 28, 28, 1
auto l1 = relu( Conv2D( 32, {3, 3}, {28, 28, 1}, "valid" )( l0 ) );
auto l2 = max_pooling_2d( 2 ) ( l1 ); // 13, 13, 32
auto l3 = relu( Conv2D( 64, {3, 3}, {13, 13, 32}, "valid" )( l2 ) );
auto l4 = max_pooling_2d( 2 )( l3 ); //5, 5, 64
auto l5 = drop_out(0.5)( flatten( l4 ) );
auto output = Dense( 10, 5*5*64 )( l5 );

auto ground_truth = place_holder<tensor<float>>{}; // 1-D, 10
auto loss = cross_entropy_loss( ground_truth, output );
```

## Supported layers
+ [Operations](./include/operation.hpp):
    - [`plus`](#plus), or operator `+`;
    - [`multiply`](#multiply), or operator `*`, note this operation implies matrix-matrix multiplication, i.e., `dot` in numpy;
    - [`log`](#log);
    - `negative`;
    - `elementwise_product`, or `hadamard_product`;
    - `sum_reduct`;
    - `mean_reduce`;
    - `minus`;
    - `square`;
    - `abs`;
    - `exp`;
    - `clip`;
    - `reshape`;
    - `flatten`;
    - `identity`;
    - `transpose`;
    - `conv2d`;
    - `drop_out`;
    - `max_pooling_2d`;
    - `average_pooling_2d`;
    - `up_sampling_2d`;
    - `batch_normalization`;
    - `instance_normalization`;
    - `concatenate`, or `concat`.
+ [Activations](./include/activation.hpp):
    - `softmax`;
    - `selu`;
    - `softplus`;
    - `softsign`;
    - `sigmoid`;
    - `tanh`;
    - `relu`;
    - `leaky_relu`;
    - `elu`;
    - `exponential`;
    - `hard_sigmoid`;
    - `gelu`.
+ [Losses](./include/loss.hpp):
    - `mae`;
    - `mse`;
    - `cross_entropy`.
+ [Optimizers](./include/optimizer.hpp):
    - `sgd`;
    - `adagrad`;
    - `rmsprop`;
    - `adadelta`;
    - `adam`;
    - `gradient_descent`.

### plus

`plus` or `+` does element-wise addition. (note broadcasting is permitted.)

```cpp
    auto a = variable{ ones<float>( {2, 2} ) };
    auto b = variable{ zeros<float>( {2, 2} ) };
    auto ab = a+b; // or 'auto ab = plus( a, b );'
    ceras::session<ceras::tensor<double>> s;
    std::cout <<  s.run( ab );
```
this will produce a 2x2 matrix of `[ [1, 1], [1, 1] ]`. Full code is [here](./test/layer_plus.cc).

### multiply

`multiply` or `*` does matrix multiplication.

```cpp
    auto a = variable{ ones<float>( {2, 2} ) };
    auto b = variable{ ones<float>( {2, 2} ) };
    auto ab = a*b; // or 'auto ab = multiply( a, b );'
    ceras::session<ceras::tensor<double>> s;
    std::cout <<  s.run( ab );
```
this will produce a 2x2 matrix of [[2, 2], [2, 2]]. Full code is [here](./test/layer_multiply.cc).

### log

`log` does element-wise logarithm on each element.

```cpp
    auto a = variable{ ones<float>( {2, 2} ) };
    auto la = log(a);
    ceras::session<ceras::tensor<double>> s;
    std::cout <<  s.run( la );
```

this will produce a 2x2 matrix of [[0, 0], [0, ]]. Full code is [here](./test/layer_log.cc).



## TODO
+ save/load model
+ mimicking Tensorflow::Keras grammar
+ recurrent operations
+ provide a single-header file


## License

+ AGPLv3
+ BSD


## Acknowledgements

+ [Tensorflow 1](https://www.tensorflow.org/)
+ [TensorSlow](https://github.com/danielsabinasz/TensorSlow)
+ [Caffe](https://github.com/BVLC/caffe)
