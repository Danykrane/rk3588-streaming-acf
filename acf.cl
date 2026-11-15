/* Design sketch, OpenCL C 1.2. Compile checked; not run on Mali.
 * Host writes normalized input at head before launch, keeps buffers alive,
 * and serializes launches with input writes and matrix readers.
 * Initialize the complete input ring to zero before first launch.
 * input: 4096 floats; output: 2000*500 floats, physical row-major ring.
 * available: min(number of received samples,4096); row: next physical row.
 * Launch 512 work-items, suggested local size 64 (query device limits).
 */
inline float sample(__global const float *input, uint head, uint age) {
    return input[(head + 4096u - age) & 4095u];
}
inline float product(__global const float *input, uint head,
                     uint k, uint q, float alpha) {
    float delayed = sample(input, head, k+q);
    if (alpha != 0.0f)
        delayed = (1.0f-alpha)*delayed + alpha*sample(input, head, k+q+1u);
    return sample(input, head, k) * delayed;
}
__kernel void acf_direct(__global const float *input,
                         __global float *output,
                         uint head, uint available, uint row) {
    uint c = get_global_id(0);
    if (c >= 500u) return;
    uint f = c+1u, n = 2000u/f, q = 500u/f;
    uint r = 2000u%f, a = 500u%f;
    float beta = (float)r/(float)f, alpha = (float)a/(float)f;
    uint oldest = n - (r == 0u) + q + (a != 0u);
    if (available <= oldest) {
        output[row*500u+c] = NAN;
        return;
    }
    float sum = 0.0f;
    for (uint k=0; k<n; ++k) sum += product(input, head, k, q, alpha);
    if (beta != 0.0f) sum += beta*product(input, head, n, q, alpha);
    output[row*500u+c] = sum*0.0005f;
}
