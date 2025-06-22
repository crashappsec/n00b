/**
 * @file komihash.h
 *
 * @version 5.27
 *
 * @brief The inclusion file for the "komihash" 64-bit hash function.
 *
 * The source code is written in ISO C99, with full C++ compliance enabled
 * conditionally and automatically, if compiled with a C++ compiler.
 *
 * This function is named the way it is named is to honor the Komi Republic
 * (located in Russia), native to the author.
 *
 * Description is available at https://github.com/avaneev/komihash
 *
 * E-mail: aleksey.vaneev@gmail.com or info@voxengo.com
 *
 * LICENSE:
 *
 * Copyright (c) 2021-2025 Aleksey Vaneev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include <stdint.h>
#define KOMIHASH_VER_STR "5.27" ///< KOMIHASH source code version string.

/**
 * @def KOMIHASH_NS_CUSTOM
 * @brief If this macro is defined externally, all symbols will be placed into
 * the C++ namespace specified by the macro, and won't be exported to the
 * global namespace. WARNING: if the defined value of the macro is empty, the
 * symbols will be placed into the global namespace anyway.
 */

/**
 * @def KOMIHASH_U64_C( x )
 * @brief Macro that defines a numeric value as unsigned 64-bit value.
 *
 * @param x Value.
 */
#define KOMIHASH_U64_C(x) ((uint64_t)x)

/**
 * @{
 * @brief Unsigned 64-bit constant that defines the initial state of the
 * hash function (first mantissa bits of PI).
 */

#define KOMIHASH_IVAL1 KOMIHASH_U64_C( 0x243F6A8885A308D3 )
#define KOMIHASH_IVAL2 KOMIHASH_U64_C( 0x13198A2E03707344 )
#define KOMIHASH_IVAL3 KOMIHASH_U64_C( 0xA4093822299F31D0 )
#define KOMIHASH_IVAL4 KOMIHASH_U64_C( 0x082EFA98EC4E6C89 )
#define KOMIHASH_IVAL5 KOMIHASH_U64_C( 0x452821E638D01377 )
#define KOMIHASH_IVAL6 KOMIHASH_U64_C( 0xBE5466CF34E90C6C )
#define KOMIHASH_IVAL7 KOMIHASH_U64_C( 0xC0AC29B7C97C50DD )
#define KOMIHASH_IVAL8 KOMIHASH_U64_C( 0x3F84D5B5B5470917 )

/** @} */

/**
 * @def KOMIHASH_VAL01
 * @brief Unsigned 64-bit constant with `01` bit-pair replication.
 */

#define KOMIHASH_VAL01 KOMIHASH_U64_C( 0x5555555555555555 )

/**
 * @def KOMIHASH_VAL10
 * @brief Unsigned 64-bit constant with `10` bit-pair replication.
 */

#define KOMIHASH_VAL10 KOMIHASH_U64_C( 0xAAAAAAAAAAAAAAAA )

/**
 * @def KOMIHASH_LITTLE_ENDIAN
 * @brief Endianness definition macro, can be used as a logical constant.
 * Equals 0, if C++20 `endian` is in use.
 *
 * Can be defined externally (e.g., =1, if endianness-correction and
 * hash-value portability are unnecessary in any case, to reduce overhead).
 */

/**
 * @def KOMIHASH_COND_EC( vl, vb )
 * @brief Macro that emits either `vl` or `vb`, depending on platform's
 * endianness.
 */

#if !defined( KOMIHASH_LITTLE_ENDIAN )
	#if ( defined( __BYTE_ORDER__ ) && \
			__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ ) || \
		( defined( __BYTE_ORDER ) && __BYTE_ORDER == __LITTLE_ENDIAN ) || \
		defined( __LITTLE_ENDIAN__ ) || defined( _LITTLE_ENDIAN ) || \
		defined( _WIN32 ) || defined( i386 ) || defined( __i386 ) || \
		defined( __i386__ ) || defined( _M_IX86 ) || defined( _M_AMD64 ) || \
		defined( _X86_ ) || defined( __x86_64 ) || defined( __x86_64__ ) || \
		defined( __amd64 ) || defined( __amd64__ ) || defined( _M_ARM )

		#define KOMIHASH_LITTLE_ENDIAN 1

	#elif ( defined( __BYTE_ORDER__ ) && \
			__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ ) || \
		( defined( __BYTE_ORDER ) && __BYTE_ORDER == __BIG_ENDIAN ) || \
		defined( __BIG_ENDIAN__ ) || defined( _BIG_ENDIAN ) || \
		defined( __SYSC_ZARCH__ ) || defined( __zarch__ ) || \
		defined( __s390x__ ) || defined( __sparc ) || defined( __sparc__ )

		#define KOMIHASH_LITTLE_ENDIAN 0
		#define KOMIHASH_COND_EC( vl, vb ) ( vb )
       #endif

#endif // !defined( KOMIHASH_LITTLE_ENDIAN )

/**
 * @def KOMIHASH_ICC_GCC
 * @brief Macro that denotes the use of the ICC classic compiler with
 * GCC-style built-in functions.
 */

#if defined( __INTEL_COMPILER ) && __INTEL_COMPILER >= 1300 && \
	!defined( _MSC_VER )

	#define KOMIHASH_ICC_GCC

#endif // ICC check

/**
 * @def KOMIHASH_GCC_BUILTINS
 * @brief Macro that denotes availability of GCC-style built-in functions.
 */

#if defined( __GNUC__ ) || defined( __clang__ ) || \
	defined( __IBMC__ ) || defined( __IBMCPP__ ) || \
	defined( __COMPCERT__ ) || defined( KOMIHASH_ICC_GCC )

	#define KOMIHASH_GCC_BUILTINS

#endif // GCC built-ins check

/**
 * @def KOMIHASH_EC32( v )
 * @brief Macro that appies 32-bit byte-swapping, for endianness-correction.
 * Undefined for unknown compilers, if big-endian.
 *
 * @param v Value to byte-swap.
 */

/**
 * @def KOMIHASH_EC64( v )
 * @brief Macro that appies 64-bit byte-swapping, for endianness-correction.
 * Undefined for unknown compilers, if big-endian.
 *
 * @param v Value to byte-swap.
 */

#if KOMIHASH_LITTLE_ENDIAN

	#define KOMIHASH_EC32( v ) ( v )
	#define KOMIHASH_EC64( v ) ( v )

#else // KOMIHASH_LITTLE_ENDIAN

	#if defined( KOMIHASH_GCC_BUILTINS )

		#define KOMIHASH_EC32( v ) KOMIHASH_COND_EC( v, __builtin_bswap32( v ))
		#define KOMIHASH_EC64( v ) KOMIHASH_COND_EC( v, __builtin_bswap64( v ))

	#elif defined( _MSC_VER )


		#define KOMIHASH_EC32( v ) KOMIHASH_COND_EC( v, _byteswap_ulong( v ))
		#define KOMIHASH_EC64( v ) KOMIHASH_COND_EC( v, _byteswap_uint64( v ))
        #endif


#endif // KOMIHASH_LITTLE_ENDIAN

/**
 * @def KOMIHASH_LIKELY( x )
 * @brief Likelihood macro that is used for manually-guided
 * micro-optimization.
 *
 * @param x Expression that is likely to be evaluated to `true`.
 */

/**
 * @def KOMIHASH_UNLIKELY( x )
 * @brief Unlikelihood macro that is used for manually-guided
 * micro-optimization.
 *
 * @param x Expression that is unlikely to be evaluated to `true`.
 */

#define KOMIHASH_LIKELY( x ) ( __builtin_expect( x, 1 ))
#define KOMIHASH_UNLIKELY( x ) ( __builtin_expect( x, 0 ))

/**
 * @def KOMIHASH_PREFETCH( a )
 * @brief Memory address prefetch macro, to preload some data into CPU cache.
 *
 * Temporal locality=3, in case a collision resolution would be necessary,
 * or for a subsequent disk write.
 *
 * @param a Prefetch address.
 */

#if defined( KOMIHASH_GCC_BUILTINS ) && !defined( __COMPCERT__ )

	#define KOMIHASH_PREFETCH( a ) __builtin_prefetch( a, 0, 3 )

#else // Prefetch macro

	#define KOMIHASH_PREFETCH( a ) (void) 0

#endif // Prefetch macro



/**
 * @{
 * @brief Load unsigned value of corresponding bit-size, with
 * endianness-correction.
 *
 * An auxiliary function that returns an unsigned value created out of a
 * sequence of bytes in memory. This function is used to convert endianness
 * of in-memory unsigned values, and to avoid unaligned memory accesses.
 *
 * @param p Pointer to bytes in memory. Alignment is unimportant.
 * @return Endianness-corrected value from memory, typecasted to `uint64_t`.
 */

static inline uint64_t kh_lu32ec( const uint8_t* const p ) 
{
#if defined( KOMIHASH_EC32 )

	uint32_t v;
	memcpy( &v, p, 4 );

	return( KOMIHASH_EC32( v ));

#else // defined( KOMIHASH_EC32 )

	return( (uint32_t) ( p[ 0 ] | p[ 1 ] << 8 | p[ 2 ] << 16 | p[ 3 ] << 24 ));

#endif // defined( KOMIHASH_EC32 )
}

static inline uint64_t kh_lu64ec( const uint8_t* const p ) 
{
#if defined( KOMIHASH_EC64 )

	uint64_t v;
	memcpy( &v, p, 8 );

	return( KOMIHASH_EC64( v ));

#else // defined( KOMIHASH_EC64 )

	return( kh_lu32ec( p ) | kh_lu32ec( p + 4 ) << 32 );

#endif // defined( KOMIHASH_EC64 )
}

/** @} */

/**
 * @def KOMIHASH_M128_IMPL
 * @brief Aux macro for kh_m128() implementation.
 */

/**
 * @def KOMIHASH_EMULU( u, v )
 * @brief Aux macro for `__emulu()` intrinsic.
 *
 * @param u Multiplier 1.
 * @param v Multiplier 2.
 */

#if defined( _MSC_VER ) && ( defined( _M_ARM64 ) || defined( _M_ARM64EC ) || \
	( defined( __INTEL_COMPILER ) && defined( _M_X64 )))

	#define KOMIHASH_M128_IMPL \
		const uint64_t rh = __umulh( u, v ); \
		*rl = u * v; \
		*rha += rh;

#elif defined( _MSC_VER ) && ( defined( _M_AMD64 ) || defined( _M_IA64 ))

	#pragma intrinsic(_umul128)

	#define KOMIHASH_M128_IMPL \
		uint64_t rh; \
		*rl = _umul128( u, v, &rh ); \
		*rha += rh;

#elif defined( __SIZEOF_INT128__ ) || \
	( defined( KOMIHASH_ICC_GCC ) && defined( __x86_64__ ))

	#define KOMIHASH_M128_IMPL \
		__uint128_t r = u; \
		r *= v; \
		const uint64_t rh = (uint64_t) ( r >> 64 ); \
		*rl = (uint64_t) r; \
		*rha += rh;

#elif ( defined( __IBMC__ ) || defined( __IBMCPP__ )) && defined( __LP64__ )

	#define KOMIHASH_M128_IMPL \
		const uint64_t rh = __mulhdu( u, v ); \
		*rl = u * v; \
		*rha += rh;

#else // defined( __IBMC__ )

	#if defined( _MSC_VER ) && !defined( __INTEL_COMPILER ) && \
		!defined( _M_ARM )

		#pragma intrinsic(__emulu)

		#define KOMIHASH_EMULU( u, v ) __emulu( u, v )

	#else // __emulu

		#define KOMIHASH_EMULU( u, v ) ( (uint64_t) ( u ) * ( v ))

	#endif // __emulu

#endif // defined( __IBMC__ )

/**
 * @brief 64-bit by 64-bit unsigned multiplication with result accumulation.
 *
 * @param u Multiplier 1.
 * @param v Multiplier 2.
 * @param[out] rl The lower half of the 128-bit result.
 * @param[in,out] rha The accumulator to receive the higher half of the
 * 128-bit result.
 */

static inline
void kh_m128( const uint64_t u, const uint64_t v,
	uint64_t* const rl, uint64_t* const rha ) 
{
#if defined( KOMIHASH_M128_IMPL )

	KOMIHASH_M128_IMPL

	#undef KOMIHASH_M128_IMPL

#else // defined( KOMIHASH_M128_IMPL )

	// _umul128() code for 32-bit systems, adapted from Hacker's Delight,
	// Henry S. Warren, Jr.

	*rl = u * v;

	const uint32_t u0 = (uint32_t) u;
	const uint32_t v0 = (uint32_t) v;
	const uint64_t w0 = KOMIHASH_EMULU( u0, v0 );
	const uint32_t u1 = (uint32_t) ( u >> 32 );
	const uint32_t v1 = (uint32_t) ( v >> 32 );
	const uint64_t t = KOMIHASH_EMULU( u1, v0 ) + (uint32_t) ( w0 >> 32 );
	const uint64_t w1 = KOMIHASH_EMULU( u0, v1 ) + (uint32_t) t;

	*rha += KOMIHASH_EMULU( u1, v1 ) + (uint32_t) ( w1 >> 32 ) +
		(uint32_t) ( t >> 32 );

	#undef KOMIHASH_EMULU

#endif // defined( KOMIHASH_M128_IMPL )
}

/**
 * @def KOMIHASH_HASHROUND()
 * @brief Macro for a common hashing round without input.
 *
 * The three instructions in this macro (multiply, add, and XOR) represent the
 * simplest constantless PRNG, scalable to any even-sized state variables,
 * with the `Seed1` being the PRNG output (2^64 PRNG period). It passes
 * `PractRand` tests with rare non-systematic "unusual" evaluations.
 *
 * To make this PRNG reliable, self-starting, and eliminate a risk of
 * stopping, the following variant can be used, which adds a "register
 * checker-board", a source of raw entropy. The PRNG is available as the
 * komirand() function. Not required for hashing (but works for it) since the
 * input entropy is usually available in abundance during hashing.
 *
 * `Seed5 += 0xAAAAAAAAAAAAAAAA;`
 *
 * (the `0xAAAA...` constant should match register's size; essentially, it is
 * a replication of the `10` bit-pair; it is not an arbitrary constant).
 */

#define KOMIHASH_HASHROUND() \
	kh_m128( Seed1, Seed5, &Seed1, &Seed5 ); \
	Seed1 ^= Seed5

/**
 * @def KOMIHASH_HASH16( m )
 * @brief Macro for a common hashing round with 16-byte input.
 *
 * @param m Message pointer, alignment is unimportant.
 */

#define KOMIHASH_HASH16( m ) \
	kh_m128( kh_lu64ec( m ) ^ Seed1, \
		kh_lu64ec( m + 8 ) ^ Seed5, &Seed1, &Seed5 ); \
	Seed1 ^= Seed5

/**
 * @def KOMIHASH_HASHFIN()
 * @brief Macro for common hashing finalization round.
 *
 * The final hashing input is expected in the `r1h` and `r2h` temporary
 * variables. The macro inserts the function return instruction.
 */

#define KOMIHASH_HASHFIN() \
	kh_m128( r1h, r2h, &Seed1, &Seed5 ); \
	Seed1 ^= Seed5; \
	KOMIHASH_HASHROUND(); \
	return( Seed1 )

/**
 * @def KOMIHASH_HASHLOOP64()
 * @brief Macro for a common 64-byte full-performance hashing loop.
 *
 * Expects `Msg` and `MsgLen` values (greater than 63), requires initialized
 * `Seed1-8` values.
 *
 * The "shifting" arrangement of `Seed1-4` XORs (below) does not increase
 * individual `SeedN` PRNG period beyond 2^64, but reduces a chance of any
 * occassional synchronization between PRNG lanes happening. Practically,
 * `Seed1-4` together become a single "fused" 256-bit PRNG value, having 2^66
 * summary PRNG period.
 */

#define KOMIHASH_HASHLOOP64() \
	do \
	{ \
		kh_m128( kh_lu64ec( Msg ) ^ Seed1, \
			kh_lu64ec( Msg + 32 ) ^ Seed5, &Seed1, &Seed5 ); \
	\
		kh_m128( kh_lu64ec( Msg + 8 ) ^ Seed2, \
			kh_lu64ec( Msg + 40 ) ^ Seed6, &Seed2, &Seed6 ); \
	\
		kh_m128( kh_lu64ec( Msg + 16 ) ^ Seed3, \
			kh_lu64ec( Msg + 48 ) ^ Seed7, &Seed3, &Seed7 ); \
	\
		kh_m128( kh_lu64ec( Msg + 24 ) ^ Seed4, \
			kh_lu64ec( Msg + 56 ) ^ Seed8, &Seed4, &Seed8 ); \
	\
		Msg += 64; \
		MsgLen -= 64; \
	\
		KOMIHASH_PREFETCH( Msg ); \
	\
		Seed4 ^= Seed7; \
		Seed1 ^= Seed8; \
		Seed3 ^= Seed6; \
		Seed2 ^= Seed5; \
	\
	} while KOMIHASH_LIKELY( MsgLen > 63 )

/**
 * @brief The hashing epilogue function (for internal use).
 *
 * @param Msg Pointer to the remaining part of the message.
 * @param MsgLen Remaining part's length, can be 0.
 * @param Seed1 Latest `Seed1` value.
 * @param Seed5 Latest `Seed5` value.
 * @return 64-bit hash value.
 */

static inline uint64_t komihash_epi( const uint8_t* Msg, size_t MsgLen,
	uint64_t Seed1, uint64_t Seed5 ) 
{
	uint64_t r1h, r2h;

	if( MsgLen > 31 )
	{
		KOMIHASH_HASH16( Msg );
		KOMIHASH_HASH16( Msg + 16 );

		MsgLen -= 32;
		Msg += 32;
	}

	if( MsgLen > 15 )
	{
		KOMIHASH_HASH16( Msg );

		MsgLen -= 16;
		Msg += 16;
	}

	int ml8 = (int) ( MsgLen * 8 );

	if( MsgLen < 8 )
	{
		ml8 ^= 56;
		r1h = kh_lu64ec( Msg + MsgLen - 8 ) >> 8 | (uint64_t) 1 << 56;
		r2h = Seed5;
		r1h = ( r1h >> ml8 ) ^ Seed1;
	}
	else
	{
		r2h = kh_lu64ec( Msg + MsgLen - 8 ) >> 8 | (uint64_t) 1 << 56;
		ml8 ^= 120;
		r1h = kh_lu64ec( Msg ) ^ Seed1;
		r2h = ( r2h >> ml8 ) ^ Seed5;
	}

	KOMIHASH_HASHFIN();
}

/**
 * @brief KOMIHASH 64-bit hash function.
 *
 * Produces and returns a 64-bit hash value of the specified message, string,
 * or binary data block. Designed for 64-bit hash-table and hash-map uses, and
 * can be also used for checksums. Produces identical hashes on both big- and
 * little-endian systems.
 *
 * @param Msg0 The message to produce a hash from. The alignment of this
 * pointer is unimportant. It is valid to pass 0 when `MsgLen` equals 0
 * (assuming that compiler's implementation of the address prefetch is
 * non-failing, as per GCC specification).
 * @param MsgLen Message's length, in bytes, can be zero.
 * @param UseSeed Optional value, to use instead of the default seed. To use
 * the default seed, set to 0. This value can have any number of significant
 * bits, and any statistical quality. May need endianness-correction via
 * KOMIHASH_EC64(), if this value is shared between big- and little-endian
 * systems.
 * @return 64-bit hash of the input data. Should be endianness-corrected when
 * this value is shared between big- and little-endian systems.
 */

static inline uint64_t komihash( const void* const Msg0, size_t MsgLen,
	const uint64_t UseSeed ) 
{
	const uint8_t* Msg = (const uint8_t*) Msg0;

	uint64_t Seed1 = KOMIHASH_IVAL1 ^ ( UseSeed & KOMIHASH_VAL01 );
	uint64_t Seed5 = KOMIHASH_IVAL5 ^ ( UseSeed & KOMIHASH_VAL10 );
	uint64_t r1h, r2h;

	KOMIHASH_PREFETCH( Msg );

	KOMIHASH_HASHROUND(); // Required for Perlin Noise.

	if KOMIHASH_LIKELY( MsgLen < 16 )
	{
		r1h = Seed1;
		r2h = Seed5;

		if( MsgLen > 7 )
		{
			// The following XOR instructions are equivalent to mixing a
			// message with a cryptographic one-time-pad (bitwise modulo 2
			// addition). Message's statistics and distribution are thus
			// unimportant.

			r1h ^= kh_lu64ec( Msg );

			if( MsgLen < 12 )
			{
				int ml8 = (int) ( MsgLen * 8 );
				const uint64_t m = (uint64_t) ( Msg[ MsgLen - 3 ] |
					Msg[ MsgLen - 1 ] << 16 | 1 << 24 |
					Msg[ MsgLen - 2 ] << 8 );

				ml8 ^= 88;
				r2h ^= m >> ml8;
			}
			else
			{
				const int mhs = (int) ( 128 - MsgLen * 8 );
				const uint64_t mh = ( kh_lu32ec( Msg + MsgLen - 4 ) |
					(uint64_t) 1 << 32 ) >> mhs;

				const uint64_t ml = kh_lu32ec( Msg + 8 );

				r2h ^= mh << 32 | ml;
			}
		}
		else
		if KOMIHASH_LIKELY( MsgLen != 0 )
		{
			const int ml8 = (int) ( MsgLen * 8 );

			if( MsgLen < 4 )
			{
				r1h ^= (uint64_t) 1 << ml8;
				r1h ^= (uint64_t) Msg[ 0 ];

				if( MsgLen != 1 )
				{
					r1h ^= (uint64_t) Msg[ 1 ] << 8;

					if( MsgLen != 2 )
					{
						r1h ^= (uint64_t) Msg[ 2 ] << 16;
					}
				}
			}
			else
			{
				const int mhs = 64 - ml8;
				const uint64_t mh = ( kh_lu32ec( Msg + MsgLen - 4 ) |
					(uint64_t) 1 << 32 ) >> mhs;

				const uint64_t ml = kh_lu32ec( Msg );

				r1h ^= mh << 32 | ml;
			}
		}
	}
	else
	{
		if KOMIHASH_UNLIKELY( MsgLen > 31 )
		{
			goto _long;
		}

		KOMIHASH_HASH16( Msg );

		int ml8 = (int) ( MsgLen * 8 );

		if( MsgLen < 24 )
		{
			ml8 ^= 184;
			r1h = kh_lu64ec( Msg + MsgLen - 8 ) >> 8 | (uint64_t) 1 << 56;
			r2h = Seed5;
			r1h = ( r1h >> ml8 ) ^ Seed1;

			KOMIHASH_HASHFIN();
		}
		else
		{
			r2h = kh_lu64ec( Msg + MsgLen - 8 ) >> 8 | (uint64_t) 1 << 56;
			ml8 ^= 248;
			r1h = kh_lu64ec( Msg + 16 ) ^ Seed1;
			r2h = ( r2h >> ml8 ) ^ Seed5;
		}
	}

	KOMIHASH_HASHFIN();

_long:
	if KOMIHASH_LIKELY( MsgLen > 63 )
	{
		uint64_t Seed2 = KOMIHASH_IVAL2 ^ Seed1;
		uint64_t Seed3 = KOMIHASH_IVAL3 ^ Seed1;
		uint64_t Seed4 = KOMIHASH_IVAL4 ^ Seed1;
		uint64_t Seed6 = KOMIHASH_IVAL6 ^ Seed5;
		uint64_t Seed7 = KOMIHASH_IVAL7 ^ Seed5;
		uint64_t Seed8 = KOMIHASH_IVAL8 ^ Seed5;

		KOMIHASH_HASHLOOP64();

		Seed5 ^= Seed6 ^ Seed7 ^ Seed8;
		Seed1 ^= Seed2 ^ Seed3 ^ Seed4;
	}

	return( komihash_epi( Msg, MsgLen, Seed1, Seed5 ));
}
