/* Execute the exact scalar kernel source as C for arithmetic/index checks.
 * Does not emulate OpenCL scheduling, synchronization, or device compilation.
 */
#include <math.h>
#include <stdint.h>
typedef uint32_t uint;
static uint current_id;
static uint get_global_id(uint dim) { (void)dim; return current_id; }
#define __global
#define __kernel
#define inline static inline
#include "../acf.cl"
#undef inline
void run_row(const float *input, float *output, uint head, uint available) {
    for (current_id=0; current_id<512; ++current_id)
        acf_direct(input, output, head, available, 0);
}
