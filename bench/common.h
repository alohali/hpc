#include <iostream>
#include <arm_neon.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <cassert>


#define L1BUFFER_SIZE 8192
void build_pointer_chain(void *p, size_t stride, size_t length) {
  
  size_t num = length / stride;
  for (size_t i = 0; i < num; ++i) {
    size_t dst = (i == (num - 1))?  reinterpret_cast<size_t>(p) : reinterpret_cast<size_t>(p) + (i + 1) * stride;
    size_t* src = reinterpret_cast<size_t*>(reinterpret_cast<size_t>(p) + i * stride);
    *src = dst;
  }
}


void print_pointer_chain(void *p, size_t stride, size_t length) {

  size_t num = length / stride;
  for (size_t i = 0; i < num; ++i) {
    printf("%lx, %lx\n", reinterpret_cast<size_t>(p), *(reinterpret_cast<size_t*>(p)));
  }
}


void ldr_to_use_pattern(void *p, size_t loop) {
  assert((loop % 64) == 0); 
  __asm__ __volatile__ (
    ".align 2\n"
    "1:\n"
    ".rept 64\n"
    "ldr %0, [%0]\n"
    ".endr 64\n"
    "subs %1, %1, #1\n"
    "bne 1b\n"
    :
    :"r"(p),"r"(loop / 64)
    :"cc"
  );
}

void gemv_neon(float *a, float *b, float *c, int m, int k, float *l1buffer){
    float32x4_t vc[4];
    float32x4_t va[4];
    int maxbufferk = L1BUFFER_SIZE / 16 / 4;
    int ktile = k>maxbufferk? maxbufferk:k;
    for(int i=0; i<m; i+=16){
        vc[0] = vdupq_n_f32(0.);
        vc[1] = vdupq_n_f32(0.);
        vc[2] = vdupq_n_f32(0.);
        vc[3] = vdupq_n_f32(0.);
        for(int kt=0; kt<k; kt+=ktile){
            float *l1 = l1buffer;
            float32x4_t *tmp = (float32x4_t *)l1;
#if 0
            for(int prek=0;prek<ktile;prek++){
                *tmp++ = vld1q_f32(a+i+0 +(prek+kt)*m);
                *tmp++ = vld1q_f32(a+i+4 +(prek+kt)*m);
                *tmp++ = vld1q_f32(a+i+8 +(prek+kt)*m);
                *tmp++ = vld1q_f32(a+i+12+(prek+kt)*m);
            }
#endif
        __asm__ __volatile__ (
    
          ".align 2\n"
          "1:\n"
    #ifdef __aarch64__
          "ld1 {v0.4s}, [%1], #16   \n"
          "ld1 {v1.4s}, [%1], #16  \n"
          "ld1 {v2.4s}, [%1], #16   \n"
          "ld1 {v3.4s}, [%1], %3   \n"
          "st1 {v0.4s}, [%0] ,#16   \n"
          "st1 {v1.4s}, [%0] ,#16   \n"
          "st1 {v2.4s}, [%0] ,#16   \n"
          "st1 {v3.4s}, [%0] ,#16   \n"
    #else
    #endif  
          "subs %2, %2, #1\n"
          "bne 1b\n"
          :
          :"r"(tmp), "r"(a+i+kt*m),"r"(ktile), "r"(m-12)
          :"cc","r0","r1","r2","r3","q0","q1","q2","q3"
      
        );
        
            for(int j=0; j<ktile; j+=2){
                float32x2_t vb = vld1_f32(b+j+kt);
                for(int ji=0; ji<2; ji++){
                    va[0] = vld1q_f32(l1);l1+=4;
                    va[1] = vld1q_f32(l1);l1+=4;
                    va[2] = vld1q_f32(l1);l1+=4;
                    va[3] = vld1q_f32(l1);l1+=4;
                    vc[0] = vfmaq_lane_f32(vc[0], va[0], vb,0);
                    vc[1] = vfmaq_lane_f32(vc[1], va[1], vb,0);
                    vc[2] = vfmaq_lane_f32(vc[2], va[2], vb,0);
                    vc[3] = vfmaq_lane_f32(vc[3], va[3], vb,0);
                }
            }
        }
        vst1q_f32(c+i+0 , vc[0]);
        vst1q_f32(c+i+4 , vc[1]);
        vst1q_f32(c+i+8 , vc[2]);
        vst1q_f32(c+i+12, vc[3]);
    }
}
void ldr_bw(void *p, size_t length, size_t stride, size_t loop) {
  size_t iteration = length / stride;
  assert((iteration % 32) == 0);
  for (size_t l = 0; l < loop; ++l) { 
    void *temp_p = p;

    __asm__ __volatile__ (


      ".align 2\n"
      "1:\n"
      ".rept 4\n"
#ifdef __aarch64__

      "ld1 {v0.4s}, [%0], %2   \n"
      "ld1 {v1.4s}, [%0], %2   \n"
      "ld1 {v2.4s}, [%0], %2   \n"
      "ld1 {v3.4s}, [%0], %2   \n"
      "ld1 {v4.4s}, [%0], %2   \n"
      "ld1 {v5.4s}, [%0], %2   \n"
      "ld1 {v6.4s}, [%0], %2   \n"
      "ld1 {v7.4s}, [%0], %2   \n"
#else
      "vld1.f32 {d0-d1}, [%0], %2\n"
      "vld1.f32 {d2-d3}, [%0], %2\n"
      "vld1.f32 {d4-d5}, [%0], %2\n"
      "vld1.f32 {d6-d7}, [%0], %2\n"
      "vld1.f32 {d8-d9}, [%0], %2\n"
      "vld1.f32 {d10-d11}, [%0], %2\n"
      "vld1.f32 {d12-d13}, [%0], %2\n"
      "vld1.f32 {d14-d15}, [%0], %2\n"
#endif
      ".endr\n"
      "subs %1, %1, #1\n"
      "bne 1b\n"
      :
      :"r"(temp_p),"r"(iteration / 32), "r"(stride)
      :"cc","r0","r1","r2","r3","r4","q0","q1","q2","q3","q4","q5","q6","q7","q8","q9","q10","q11","q12","q13","q14","q15"
    
    );
  }
}

void inst_bwsmla(void *p, size_t length,  size_t loop) {
  size_t iteration = length;
  assert((iteration % 32) == 0);
  for (size_t l = 0; l < loop; ++l) { 
    void *temp_p = p;
    void *temp_p1 = p+512;
    void *temp_p2 = p+2*512;
    void *temp_p3 = p+3*384;
    void *temp_p4 = p+4*384;

    __asm__ __volatile__ (
      "mov x10, %0\n"
      "mov x11, %2\n"
      "mov x12, %3\n"
      "mov x13, %4\n"
      "mov x14, %5\n"
      ".align 2\n"
      "1:\n"
      ".rept 2\n"
#ifdef __aarch64__

        "ld1 {v14.16b}, [x11]\n"
        "smull v0.8h, v12.8b, v8.8b\n"
        "ld1 {v15.16b}, [x12]\n"
        "smull v1.8h, v13.8b, v8.8b\n"

        "smull v2.8h, v14.8b, v8.8b\n"
        "ld1 {v10.16b, v11.16b}, [x10], #32\n"
        "smull v3.8h, v15.8b, v8.8b\n"
        "smlal2 v0.8h, v12.16b, v8.16b\n"
        "smlal2 v1.8h, v13.16b, v8.16b\n"
        "smlal2 v2.8h, v14.16b, v8.16b\n"
        "smlal2 v3.8h, v15.16b, v8.16b\n"
        "sadalp v16.4s, v0.8h\n"
        "smull v4.8h, v12.8b, v9.8b\n"
        "sadalp v17.4s, v1.8h\n"
        "smull v5.8h, v13.8b, v9.8b\n"
        "sadalp v18.4s, v2.8h\n"
        "smull v6.8h, v14.8b, v9.8b\n"
        "sadalp v19.4s, v3.8h\n"
        "smull v7.8h, v15.8b, v9.8b\n"
        "smlal2 v4.8h, v12.16b, v9.16b\n"
        "smlal2 v5.8h, v13.16b, v9.16b\n"
        "smlal2 v6.8h, v14.16b, v9.16b\n"
        "smlal2 v7.8h, v15.16b, v9.16b\n"
        "sadalp v20.4s, v4.8h\n"

        "ld1 {v8.16b, v9.16b}, [x10], #32\n"
        "smull v0.8h, v12.8b, v10.8b\n"
        "sadalp v21.4s, v5.8h\n"
        "smull v1.8h, v13.8b, v10.8b\n"
        "sadalp v22.4s, v6.8h\n"
        "smull v2.8h, v14.8b, v10.8b\n"
        "sadalp v23.4s, v7.8h\n"
        "smull v3.8h, v15.8b, v10.8b\n"
        "smlal2 v0.8h, v12.16b, v10.16b\n"
		"sub x10, x10, #64\n"
        "smlal2 v1.8h, v13.16b, v10.16b\n"
        "smlal2 v2.8h, v14.16b, v10.16b\n"
        "smlal2 v3.8h, v15.16b, v10.16b\n"
        "sadalp v24.4s, v0.8h\n"
        "smull v4.8h, v12.8b, v11.8b\n"
        "sadalp v25.4s, v1.8h\n"
        "smull v5.8h, v13.8b, v11.8b\n"
        "sadalp v26.4s, v2.8h\n"
        "smull v6.8h, v14.8b, v11.8b\n"
        "sadalp v27.4s, v3.8h\n"
        "smull v7.8h, v15.8b, v11.8b\n"
        "smlal2 v4.8h, v12.16b, v11.16b\n"
        "ld1 {v12.16b}, [x13]\n"
        "smlal2 v5.8h, v13.16b, v11.16b\n"
        "ld1 {v13.16b}, [x14]\n"
        "smlal2 v6.8h, v14.16b, v11.16b\n"
        "smlal2 v7.8h, v15.16b, v11.16b\n"
        "sadalp v28.4s, v4.8h\n"
        "sadalp v29.4s, v5.8h\n"
        "sadalp v30.4s, v6.8h\n"
        "sadalp v31.4s, v7.8h\n"
#else
      "vmla.f32 d0,d0,d0\n"
      "vmla.f32 d1,d1,d1\n"
      "vmla.f32 d2,d2,d2\n"
      "vmla.f32 d3,d3,d3\n"
      "vmla.f32 d4,d4,d4\n"
      "vmla.f32 d5,d5,d5\n"
      "vmla.f32 d6,d6,d6\n"
      "vmla.f32 d7,d7,d7\n"
      "vmla.f32 d8,d8,d8\n"
      "vmla.f32 d9,d9,d9\n"
      "vmla.f32 d10,d10,d10\n"
      "vmla.f32 d11,d11,d11\n"
      "vmla.f32 d12,d12,d12\n"
      "vmla.f32 d13,d13,d13\n"
      "vmla.f32 d14,d14,d14\n"
      "vmla.f32 d15,d15,d15\n"
#endif
      ".endr\n"
      "subs %1, %1, #1\n"
      "bne 1b\n"
      :
      :"r"(temp_p),"r"(iteration / 64), "r"(temp_p1), "r"(temp_p2), "r"(temp_p3), "r"(temp_p4)
      :"cc","r0","r1","r2","r3","r4","q0","q1","q2","q3","q4","q5","q6","q7","q8","q9","q10","q11","q12","q13","q14","q15", "q16", "q17", "q18", "q19", "q20", "q21", "q22", "q23","x10", "x11", "x12", "x13", "x14"

      );
  }
}

void inst_bw(void *p, size_t length,  size_t loop) {
  size_t iteration = length;
  assert((iteration % 32) == 0);
  for (size_t l = 0; l < loop; ++l) { 
    void *temp_p = p;
    __asm__ __volatile__ (
      ".align 2\n"
      "1:\n"
      ".rept 4\n"
#ifdef __aarch64__
      "fmla v0.4s, v0.4s,v0.4s\n"
      "fmla v1.4s, v1.4s,v1.4s\n"
      "fmla v2.4s, v2.4s,v2.4s\n"
      "fmla v3.4s, v3.4s,v3.4s\n"
      "fmla v4.4s, v4.4s,v4.4s\n"
      "fmla v5.4s, v5.4s,v5.4s\n"
      "fmla v6.4s, v6.4s,v6.4s\n"
      "fmla v7.4s, v7.4s,v7.4s\n"
      "fmla v8.4s, v8.4s,v8.4s\n"
      "fmla v9.4s, v9.4s,v9.4s\n"
      "fmla v10.4s, v10.4s, v10.4s\n"
      "fmla v11.4s, v11.4s, v11.4s\n"
      "fmla v12.4s, v12.4s, v12.4s\n"
      "fmla v13.4s, v13.4s, v13.4s\n"
      "fmla v14.4s, v14.4s, v14.4s\n"
      "fmla v15.4s, v15.4s, v15.4s\n"
#else
      "vmla.f32 d0,d0,d0\n"
      "vmla.f32 d1,d1,d1\n"
      "vmla.f32 d2,d2,d2\n"
      "vmla.f32 d3,d3,d3\n"
      "vmla.f32 d4,d4,d4\n"
      "vmla.f32 d5,d5,d5\n"
      "vmla.f32 d6,d6,d6\n"
      "vmla.f32 d7,d7,d7\n"
      "vmla.f32 d8,d8,d8\n"
      "vmla.f32 d9,d9,d9\n"
      "vmla.f32 d10,d10,d10\n"
      "vmla.f32 d11,d11,d11\n"
      "vmla.f32 d12,d12,d12\n"
      "vmla.f32 d13,d13,d13\n"
      "vmla.f32 d14,d14,d14\n"
      "vmla.f32 d15,d15,d15\n"
#endif
      ".endr\n"
      "subs %1, %1, #1\n"
      "bne 1b\n"
      :
      :"r"(temp_p),"r"(iteration / 64)
      :"cc","r0","r1","r2","r3","r4","q0","q1","q2","q3","q4","q5","q6","q7","q8","q9","q10","q11","q12","q13","q14","q15"

      );
  }
}
void str_bw(void *p, size_t length, size_t stride, size_t loop) {
  size_t iteration = length / stride;
  assert((iteration % 32) == 0);
  for (size_t l = 0; l < loop; ++l) { 
    void *temp_p = p;
    __asm__ __volatile__ (

      ".align 2\n"
      "1:\n"
      ".rept 4\n"
#ifdef __aarch64__
      "st1 {v0.4s}, [%0], %2   \n"
      "st1 {v1.4s}, [%0], %2   \n"
      "st1 {v2.4s}, [%0], %2   \n"
      "st1 {v3.4s}, [%0], %2   \n"
      "st1 {v4.4s}, [%0], %2   \n"
      "st1 {v5.4s}, [%0], %2   \n"
      "st1 {v6.4s}, [%0], %2   \n"
      "st1 {v7.4s}, [%0], %2   \n"

#else
      "vst1.f32 {d0-d1}, [%0], %2\n"
      "vst1.f32 {d2-d3}, [%0], %2\n"
      "vst1.f32 {d4-d5}, [%0], %2\n"
      "vst1.f32 {d6-d7}, [%0], %2\n"
      "vst1.f32 {d8-d9}, [%0], %2\n"
      "vst1.f32 {d10-d11}, [%0], %2\n"
      "vst1.f32 {d12-d13}, [%0], %2\n"
      "vst1.f32 {d14-d15}, [%0], %2\n"
#endif
      ".endr\n"
      "subs %1, %1, #1\n"
      "bne 1b\n"
      :
      :"r"(temp_p),"r"(iteration / 32), "r"(stride)
      :"cc","r0","r1","r2","r3","r4","q0","q1","q2","q3","q4","q5","q6","q7","q8","q9","q10","q11","q12","q13","q14","q15"

    );
  }
}

void copy_bw(void *dst, void *src, size_t length, size_t stride, size_t loop) {
  size_t iteration = length / stride;
  assert((iteration % 4) == 0);
  for (size_t l = 0; l < loop; ++l) { 
    __asm__ __volatile__ (

      ".align 2\n"
      "1:\n"

#ifdef __aarch64__
      "ld1 {v0.4s}, [%1], %3   \n"
      "ld1 {v1.4s}, [%1], %3   \n"
      "ld1 {v2.4s}, [%1], %3   \n"
      "ld1 {v3.4s}, [%1], %3   \n"
      "st1 {v0.4s}, [%0], %3   \n"
      "st1 {v1.4s}, [%0], %3   \n"
      "st1 {v2.4s}, [%0], %3   \n"
      "st1 {v3.4s}, [%0], %3   \n"
#else
      "vld1.f32 {d0-d1}, [%1], %3\n"
      "vld1.f32 {d2-d3}, [%1], %3\n"
      "vld1.f32 {d4-d5}, [%1], %3\n"
      "vld1.f32 {d6-d7}, [%1], %3\n"
      "vst1.f32 {d0-d1}, [%0], %3\n"
      "vst1.f32 {d2-d3}, [%0], %3\n"
      "vst1.f32 {d4-d5}, [%0], %3\n"
      "vst1.f32 {d6-d7}, [%0], %3\n"

#endif  
      "subs %2, %2, #1\n"
      "bne 1b\n"
      :
      :"r"(dst),"r"(src),"r"(iteration / 4), "r"(stride)
      :"cc","r0","r1","r2","r3","q0","q1","q2","q3"
  
    );
  }
}

void add_in_place_bw(void *p, size_t length, size_t stride, size_t loop) {
  size_t iteration = length / stride;
  assert((iteration % 8) == 0);
  for (size_t l = 0; l < loop; ++l) { 
#ifdef __aarch64__
#else
    __asm__ __volatile__ (
      "mov r0, %0\n"
      "mov r1, %0\n"
      ".align 2\n"
      "1:\n"
      "vld1.f32 {d0-d1}, [r0], %2\n"
      "vld1.f32 {d2-d3}, [r0], %2\n"
      "vadd.f32 q0, q0, q15\n"
      "vld1.f32 {d4-d5}, [r0], %2\n"
      "vadd.f32 q1, q1, q15\n"
      "vld1.f32 {d6-d7}, [r0], %2\n"
      "vadd.f32 q2, q2, q15\n"
      "vst1.f32 {d0-d1}, [r1], %2\n"
      "vld1.f32 {d8-d9}, [r0], %2\n"
      "vadd.f32 q3, q3, q15\n"
      "vst1.f32 {d2-d3}, [r1], %2\n"
      "vld1.f32 {d10-d11}, [r0], %2\n"
      "vadd.f32 q4, q4, q15\n"
      "vst1.f32 {d4-d5}, [r1], %2\n"
      "vld1.f32 {d12-d13}, [r0], %2\n"
      "vadd.f32 q5, q5, q15\n"
      "vst1.f32 {d6-d7}, [r1], %2\n"
      "vld1.f32 {d14-d15}, [r0], %2\n"
      "vadd.f32 q6, q6, q15\n"
      "vst1.f32 {d8-d9}, [r1], %2\n"
      "vadd.f32 q7, q7, q15\n"
      "vst1.f32 {d10-d11}, [r1], %2\n"
      "vst1.f32 {d12-d13}, [r1], %2\n"
      "vst1.f32 {d14-d15}, [r1], %2\n"
      "subs %1, %1, #1\n"
      "bne 1b\n"
      :
      :"r"(p),"r"(iteration / 8), "r"(stride)
      :"cc","r0","r1","r2","r3","r4","q0","q1","q2","q3","q4","q5","q6","q7","q8","q9","q10","q11","q12","q13","q14","q15" 
    );
#endif
  }
}
