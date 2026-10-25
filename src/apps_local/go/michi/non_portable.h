// non_portable.h -- non portable parts of the code (for efficiency)

/* ===========================================================================
 * If an operaion can be implemented very efficiently on a given architecture
 * it must be placed in this file. A portable version of the function must be
 * provided.
 *
 * At the moment, only a few operations available on the Intel processors are
 * provided. In the future, more operations using SSE intrinsics or the 
 * auto vectorization capabilities of the compilers will be defined.
 * When/if users of other operating systems/compilers want to contribute they
 * can add another case in this file
 *
 * Will certainly need to be refined and improved in the next future
 * ===========================================================================
 */
#ifndef _MSC_VER    // for gcc
    #define __INLINE__ static inline
#else               // for Microsoft Visual Studio
    #define __INLINE__ static __inline
#endif

// ------------------------- Portable definitions -----------------------------
// of the efficient operations coded below for some (compiler/processor).
#ifdef PORTABLE
  __INLINE__ int popcnt_u32(unsigned int m32)
  // Compute the number of bits set (i.e. equal to 1) in the 32 bits word m32
  // This is a very naive implementation. Much better solutions exist.
  {
      int sum=0;
      for (int i=0 ; i<32 ; i++)
          if (m32 & (1<<i)) sum++;
      return sum;
  }

  __INLINE__ int bsf_u32(unsigned int m32)
  // Bit Scan Forward -- Search for the least significant bit set in m32
  // If m32 is 0, the result is undefined
  // This is a very naive implementation. Much better solutions exist.
  {
      for (int i=0 ; i<32 ; i++)
          if (m32 & (1<<i)) return i;
      return 32;
  }
#else
  // --------------------- Efficient/Non portable definitions -----------------
  #ifdef __APPLE__
    __INLINE__ int popcnt_u32(unsigned int m32) { return __builtin_popcount(m32);}
    __INLINE__ int bsf_u32(unsigned int m32) { return  __builtin_ctz(m32);}
  #elif __GNUC__
    // FORK CHANGE: no <x86intrin.h>. Upstream includes it here, and the include
    // is vestigial -- both functions below are GCC builtins and neither needs
    // it. On the Xtensa cross-compiler that header does not exist at all, so
    // every device build failed on it before this line went.
    __INLINE__ int popcnt_u32(unsigned int m32) { return __builtin_popcount(m32);}
    __INLINE__ int bsf_u32(unsigned int m32) { return  __builtin_ctz(m32);} 
  #elif  xxxxxxx
    // If useful : add here new implementation for another architecture
  #endif // End of case of architectures (compiler/processor)
#endif   // PORTABLE
