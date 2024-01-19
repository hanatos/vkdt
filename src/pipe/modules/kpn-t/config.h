#define EL_PER_UVEC4 8 // how many float16_t elements per uvec4/128 bit load
#define WIDTH 32
#define SKEW 8  // or 0 if WIDTH % 16 != 0
#define N_BLOCKS (WIDTH / 16)     // how many blocks in the weights for one layer, when we can work on 16x16 at a time.
#define N_ITERS 8 // or 2 if WIDTH >= 256 // going over pixels: how many iterations working on batches of px
#define N_HIDDEN_LAYERS 1
#define GRAD_SCALE 128.0 // avoid underflow in f16 loss/gradients
// modes for convolution by kernel:
#define APPLY_SOFTMAX 1
// #define APPLY_SIGMOID 2
#define APPLY_PLAIN 3
#define APPLY_DEBUG 4
// #define APPLY_ACTIVATION APPLY_SOFTMAX
#define APPLY_ACTIVATION APPLY_DEBUG // XXX DEBUG
// modes for alpha blending
#define ALPHA_CONST 1
#define ALPHA_PLAIN 2
#define ALPHA_SIGMOID 3
// #define ALPHA_ACTIVATION ALPHA_PLAIN
#define ALPHA_ACTIVATION ALPHA_CONST // XXX DEBUG
// apply softmax + alpha plain seems to be a winning combination
// both plain also works (and through negative filter weights may be more expressive)

#define MLP_ACTIVATION_RELU 1
#define MLP_ACTIVATION_LEAKY_RELU 2
#define MLP_ACTIVATION_NONE 3
// #define MLP_ACTIVATION MLP_ACTIVATION_LEAKY_RELU // best candidate for results
#define MLP_ACTIVATION MLP_ACTIVATION_NONE // XXX debug deriv outside mlp with large offset DERIV_EPS

// XXX FIXME: cannot so far reproduce exactly the derivatives:
// deriv is always like 4ev too bright as compared to diff.
// XXX for hardcoded A=1 in mlp, diff is brighter!
// this is not about sum (disabled the other layers)
// this is not about mlp (hardcoded A=1 on output of inner layers)
// don't think this is the summation of the loss (double and triple checked code)
// don't think it's the summing in mulw (triple checked)
// could be the computation of dEdK (passed on from dEdI, single w[7], not much opportunity for error)
// experiment: find w to multiply global brightness change: works really well also with one fully switched on hidden layer
// FIXED: something memory garbage: if attaching different output to view0, graph stops to work! (for instance debug instead of vis)
// FIXED: i.e. only shows last 1/4 of the graph and the rest is randomly overwritten..
#define DEBUG_DERIV // debug derivatives instead of training
#define DERIV_EPS 1e-1 // lower will only show numeric jitter

#if 0 // check memory bounds before access
#define CHK_WGT(base, stride) if(base + 15 * stride + 16/EL_PER_UVEC4 <= WIDTH * WIDTH * (N_HIDDEN_LAYERS+1)/EL_PER_UVEC4)
#define CHK_SHM(base, stride) if(base + 15 * stride + 16/EL_PER_UVEC4 <= ((16 + 16*N_ITERS) * (WIDTH + SKEW)) / EL_PER_UVEC4)
#define CHK_OUT(base, stride) if(base + 15 * stride + 16/EL_PER_UVEC4 <= push.batch_size * 16 / EL_PER_UVEC4)
#else // nothing
#define CHK_WGT(base, stride)
#define CHK_SHM(base, stride)
#define CHK_OUT(base, stride)
#endif
