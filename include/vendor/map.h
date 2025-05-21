/*
 * map-macro by William R Swanson is marked with CC0 1.0 Universal.
 *
 * To view a copy of this license,
 * visit https://creativecommons.org/publicdomain/zero/1.0/
 */

// Modifications by John Viega primarily for namespacing.
//
// Original source:
// https://github.com/swansontec/map-macro/blob/master/map.h

#pragma once

#define N00B_EVAL0(...) __VA_ARGS__
#define N00B_EVAL1(...) N00B_EVAL0(N00B_EVAL0(N00B_EVAL0(__VA_ARGS__)))
#define N00B_EVAL2(...) N00B_EVAL1(N00B_EVAL1(N00B_EVAL1(__VA_ARGS__)))
#define N00B_EVAL3(...) N00B_EVAL2(N00B_EVAL2(N00B_EVAL2(__VA_ARGS__)))
#define N00B_EVAL4(...) N00B_EVAL3(N00B_EVAL3(N00B_EVAL3(__VA_ARGS__)))
#define N00B_EVAL5(...) N00B_EVAL4(N00B_EVAL4(N00B_EVAL4(__VA_ARGS__)))

#ifdef _MSC_VER
// MSVC needs more evaluations
#define N00B_EVAL6(...) N00B_EVAL5(N00B_EVAL5(N00B_EVAL5(__VA_ARGS__)))
#define N00B_EVAL(...)  N00B_EVAL6(N00B_EVAL6(__VA_ARGS__))
#else
#define N00B_EVAL(...) N00B_EVAL5(__VA_ARGS__)
#endif

#define N00B_MAP_END(...)
#define N00B_MAP_OUT

#define EMPTY()
#define DEFER(id) id EMPTY()

#define N00B_MAP_GET_END2()             0, N00B_MAP_END
#define N00B_MAP_GET_END1(...)          N00B_MAP_GET_END2
#define N00B_MAP_GET_END(...)           N00B_MAP_GET_END1
#define N00B_MAP_NEXT0(test, next, ...) next N00B_MAP_OUT
#define N00B_MAP_NEXT1(test, next)      DEFER(N00B_MAP_NEXT0)(test, next, 0)
#define N00B_MAP_NEXT(test, next)       N00B_MAP_NEXT1(N00B_MAP_GET_END test, \
                                                 next)
#define N00B_MAP_INC(X) N00B_MAP_INC_##X

#define N00B_MAP0(f, x, peek, ...) \
    f(x) DEFER(N00B_MAP_NEXT(peek, N00B_MAP1))(f, peek, __VA_ARGS__)
#define N00B_MAP1(f, x, peek, ...) \
    f(x) DEFER(N00B_MAP_NEXT(peek, N00B_MAP0))(f, peek, __VA_ARGS__)

#define N00B_MAP0_UD(f, userdata, x, peek, ...)                       \
    f(x, userdata) DEFER(N00B_MAP_NEXT(peek, N00B_MAP1_UD))(f,        \
                                                            userdata, \
                                                            peek,     \
                                                            __VA_ARGS__)
#define N00B_MAP1_UD(f, userdata, x, peek, ...)                       \
    f(x, userdata) DEFER(N00B_MAP_NEXT(peek, N00B_MAP0_UD))(f,        \
                                                            userdata, \
                                                            peek,     \
                                                            __VA_ARGS__)

#define N00B_MAP0_UD_I(f, userdata, index, x, peek, ...)                \
    f(x, userdata, index)                                               \
        DEFER(N00B_MAP_NEXT(peek, N00B_MAP1_UD_I))(f,                   \
                                                   userdata,            \
                                                   N00B_MAP_INC(index), \
                                                   peek,                \
                                                   __VA_ARGS__)
#define N00B_MAP1_UD_I(f, userdata, index, x, peek, ...)                \
    f(x, userdata, index)                                               \
        DEFER(N00B_MAP_NEXT(peek, N00B_MAP0_UD_I))(f,                   \
                                                   userdata,            \
                                                   N00B_MAP_INC(index), \
                                                   peek,                \
                                                   __VA_ARGS__)

#define N00B_MAP_LIST0(f, x, peek, ...) \
    , f(x) DEFER(N00B_MAP_NEXT(peek, N00B_MAP_LIST1))(f, peek, __VA_ARGS__)

#define N00B_MAP_LIST1(f, x, peek, ...) \
    , f(x) DEFER(N00B_MAP_NEXT(peek, N00B_MAP_LIST0))(f, peek, __VA_ARGS__)

#define N00B_MAP_LIST2(f, x, peek, ...) \
    f(x) DEFER(N00B_MAP_NEXT(peek, N00B_MAP_LIST1))(f, peek, __VA_ARGS__)

#define N00B_MAP_LIST0_UD(f, userdata, x, peek, ...)              \
    , f(x, userdata)                                              \
          DEFER(N00B_MAP_NEXT(peek, N00B_MAP_LIST1_UD))(f,        \
                                                        userdata, \
                                                        peek,     \
                                                        __VA_ARGS__)
#define N00B_MAP_LIST1_UD(f, userdata, x, peek, ...)              \
    , f(x, userdata)                                              \
          DEFER(N00B_MAP_NEXT(peek, N00B_MAP_LIST0_UD))(f,        \
                                                        userdata, \
                                                        peek,     \
                                                        __VA_ARGS__)
#define N00B_MAP_LIST2_UD(f, userdata, x, peek, ...)            \
    f(x, userdata)                                              \
        DEFER(N00B_MAP_NEXT(peek, N00B_MAP_LIST1_UD))(f,        \
                                                      userdata, \
                                                      peek,     \
                                                      __VA_ARGS__)

#define N00B_MAP_LIST0_UD_I(f, userdata, index, x, peek, ...)                  \
    , f(x, userdata, index)                                                    \
          DEFER(N00B_MAP_NEXT(peek, N00B_MAP_LIST1_UD_I))(f,                   \
                                                          userdata,            \
                                                          N00B_MAP_INC(index), \
                                                          peek,                \
                                                          __VA_ARGS__)
#define N00B_MAP_LIST1_UD_I(f, userdata, index, x, peek, ...)                  \
    , f(x, userdata, index)                                                    \
          DEFER(N00B_MAP_NEXT(peek, N00B_MAP_LIST0_UD_I))(f,                   \
                                                          userdata,            \
                                                          N00B_MAP_INC(index), \
                                                          peek,                \
                                                          __VA_ARGS__)
#define N00B_MAP_LIST2_UD_I(f, userdata, index, x, peek, ...)                \
    f(x, userdata, index)                                                    \
        DEFER(N00B_MAP_NEXT(peek, N00B_MAP_LIST0_UD_I))(f,                   \
                                                        userdata,            \
                                                        N00B_MAP_INC(index), \
                                                        peek,                \
                                                        __VA_ARGS__)

/**
 * Applies the function macro `f` to each of the remaining parameters.
 */
#define N00B_MAP(f, ...) \
    N00B_EVAL(N00B_MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters and
 * inserts commas between the results.
 */
#define N00B_MAP_LIST(f, ...) \
    N00B_EVAL(N00B_MAP_LIST2(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters
 * and passes userdata as the second parameter to each invocation,
 * e.g. N00B_MAP_UD(f, x, a, b, c) evaluates to f(a, x) f(b, x) f(c,
 * x)
 */
#define N00B_MAP_UD(f, userdata, ...) \
    N00B_EVAL(N00B_MAP1_UD(f, userdata, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

/**
 * Applies the function macro `f` to each of the remaining parameters,
 * inserts commas between the results, and passes userdata as the
 * second parameter to each invocation, e.g. N00B_MAP_LIST_UD(f, x, a,
 * b, c) evaluates to f(a, x), f(b, x), f(c, x)
 */
#define N00B_MAP_LIST_UD(f, userdata, ...)   \
    N00B_EVAL(N00B_MAP_LIST2_UD(f,           \
                                userdata,    \
                                __VA_ARGS__, \
                                ()()(),      \
                                ()()(),      \
                                ()()(),      \
                                0))

/**
 * Applies the function macro `f` to each of the remaining parameters,
 * passes userdata as the second parameter to each invocation, and the
 * index of the invocation as the third parameter,
 * e.g. N00B_MAP_UD_I(f, x, a, b, c) evaluates to f(a, x, 0) f(b, x,
 * 1) f(c, x, 2)
 */
#define N00B_MAP_UD_I(f, userdata, ...)   \
    N00B_EVAL(N00B_MAP1_UD_I(f,           \
                             userdata,    \
                             0,           \
                             __VA_ARGS__, \
                             ()()(),      \
                             ()()(),      \
                             ()()(),      \
                             0))

/**
 * Applies the function macro `f` to each of the remaining parameters,
 * inserts commas between the results, passes userdata as the second
 * parameter to each invocation, and the index of the invocation as
 * the third parameter, e.g. N00B_MAP_LIST_UD_I(f, x, a, b, c)
 * evaluates to f(a, x, 0), f(b, x, 1), f(c, x, 2)
 */
#define N00B_MAP_LIST_UD_I(f, userdata, ...)   \
    N00B_EVAL(N00B_MAP_LIST2_UD_I(f,           \
                                  userdata,    \
                                  0,           \
                                  __VA_ARGS__, \
                                  ()()(),      \
                                  ()()(),      \
                                  ()()(),      \
                                  0))

/*
 * Because the preprocessor can't do arithmetic that produces integer
 * literals for the *_I macros, we have to do it manually.  Since the
 * number of parameters is limited anyways, this is sufficient for all
 * cases. If extra N00B_EVAL layers are added, these definitions have
 * to be extended. This is equivalent to the way Boost.preprocessor
 * does it.
 *
 * The *_I macros could alternatively pass C expressions such as (0),
 * (0+1), (0+1+1...) to the user macro, but passing 0, 1, 2 ...
 * allows the user to incorporate the index into C identifiers,
 * e.g. to define a function like test_##index () for each macro
 * invocation.
 */
#define N00B_MAP_INC_0   1
#define N00B_MAP_INC_1   2
#define N00B_MAP_INC_2   3
#define N00B_MAP_INC_3   4
#define N00B_MAP_INC_4   5
#define N00B_MAP_INC_5   6
#define N00B_MAP_INC_6   7
#define N00B_MAP_INC_7   8
#define N00B_MAP_INC_8   9
#define N00B_MAP_INC_9   10
#define N00B_MAP_INC_10  11
#define N00B_MAP_INC_11  12
#define N00B_MAP_INC_12  13
#define N00B_MAP_INC_13  14
#define N00B_MAP_INC_14  15
#define N00B_MAP_INC_15  16
#define N00B_MAP_INC_16  17
#define N00B_MAP_INC_17  18
#define N00B_MAP_INC_18  19
#define N00B_MAP_INC_19  20
#define N00B_MAP_INC_20  21
#define N00B_MAP_INC_21  22
#define N00B_MAP_INC_22  23
#define N00B_MAP_INC_23  24
#define N00B_MAP_INC_24  25
#define N00B_MAP_INC_25  26
#define N00B_MAP_INC_26  27
#define N00B_MAP_INC_27  28
#define N00B_MAP_INC_28  29
#define N00B_MAP_INC_29  30
#define N00B_MAP_INC_30  31
#define N00B_MAP_INC_31  32
#define N00B_MAP_INC_32  33
#define N00B_MAP_INC_33  34
#define N00B_MAP_INC_34  35
#define N00B_MAP_INC_35  36
#define N00B_MAP_INC_36  37
#define N00B_MAP_INC_37  38
#define N00B_MAP_INC_38  39
#define N00B_MAP_INC_39  40
#define N00B_MAP_INC_40  41
#define N00B_MAP_INC_41  42
#define N00B_MAP_INC_42  43
#define N00B_MAP_INC_43  44
#define N00B_MAP_INC_44  45
#define N00B_MAP_INC_45  46
#define N00B_MAP_INC_46  47
#define N00B_MAP_INC_47  48
#define N00B_MAP_INC_48  49
#define N00B_MAP_INC_49  50
#define N00B_MAP_INC_50  51
#define N00B_MAP_INC_51  52
#define N00B_MAP_INC_52  53
#define N00B_MAP_INC_53  54
#define N00B_MAP_INC_54  55
#define N00B_MAP_INC_55  56
#define N00B_MAP_INC_56  57
#define N00B_MAP_INC_57  58
#define N00B_MAP_INC_58  59
#define N00B_MAP_INC_59  60
#define N00B_MAP_INC_60  61
#define N00B_MAP_INC_61  62
#define N00B_MAP_INC_62  63
#define N00B_MAP_INC_63  64
#define N00B_MAP_INC_64  65
#define N00B_MAP_INC_65  66
#define N00B_MAP_INC_66  67
#define N00B_MAP_INC_67  68
#define N00B_MAP_INC_68  69
#define N00B_MAP_INC_69  70
#define N00B_MAP_INC_70  71
#define N00B_MAP_INC_71  72
#define N00B_MAP_INC_72  73
#define N00B_MAP_INC_73  74
#define N00B_MAP_INC_74  75
#define N00B_MAP_INC_75  76
#define N00B_MAP_INC_76  77
#define N00B_MAP_INC_77  78
#define N00B_MAP_INC_78  79
#define N00B_MAP_INC_79  80
#define N00B_MAP_INC_80  81
#define N00B_MAP_INC_81  82
#define N00B_MAP_INC_82  83
#define N00B_MAP_INC_83  84
#define N00B_MAP_INC_84  85
#define N00B_MAP_INC_85  86
#define N00B_MAP_INC_86  87
#define N00B_MAP_INC_87  88
#define N00B_MAP_INC_88  89
#define N00B_MAP_INC_89  90
#define N00B_MAP_INC_90  91
#define N00B_MAP_INC_91  92
#define N00B_MAP_INC_92  93
#define N00B_MAP_INC_93  94
#define N00B_MAP_INC_94  95
#define N00B_MAP_INC_95  96
#define N00B_MAP_INC_96  97
#define N00B_MAP_INC_97  98
#define N00B_MAP_INC_98  99
#define N00B_MAP_INC_99  100
#define N00B_MAP_INC_100 101
#define N00B_MAP_INC_101 102
#define N00B_MAP_INC_102 103
#define N00B_MAP_INC_103 104
#define N00B_MAP_INC_104 105
#define N00B_MAP_INC_105 106
#define N00B_MAP_INC_106 107
#define N00B_MAP_INC_107 108
#define N00B_MAP_INC_108 109
#define N00B_MAP_INC_109 110
#define N00B_MAP_INC_110 111
#define N00B_MAP_INC_111 112
#define N00B_MAP_INC_112 113
#define N00B_MAP_INC_113 114
#define N00B_MAP_INC_114 115
#define N00B_MAP_INC_115 116
#define N00B_MAP_INC_116 117
#define N00B_MAP_INC_117 118
#define N00B_MAP_INC_118 119
#define N00B_MAP_INC_119 120
#define N00B_MAP_INC_120 121
#define N00B_MAP_INC_121 122
#define N00B_MAP_INC_122 123
#define N00B_MAP_INC_123 124
#define N00B_MAP_INC_124 125
#define N00B_MAP_INC_125 126
#define N00B_MAP_INC_126 127
#define N00B_MAP_INC_127 128
#define N00B_MAP_INC_128 129
#define N00B_MAP_INC_129 130
#define N00B_MAP_INC_130 131
#define N00B_MAP_INC_131 132
#define N00B_MAP_INC_132 133
#define N00B_MAP_INC_133 134
#define N00B_MAP_INC_134 135
#define N00B_MAP_INC_135 136
#define N00B_MAP_INC_136 137
#define N00B_MAP_INC_137 138
#define N00B_MAP_INC_138 139
#define N00B_MAP_INC_139 140
#define N00B_MAP_INC_140 141
#define N00B_MAP_INC_141 142
#define N00B_MAP_INC_142 143
#define N00B_MAP_INC_143 144
#define N00B_MAP_INC_144 145
#define N00B_MAP_INC_145 146
#define N00B_MAP_INC_146 147
#define N00B_MAP_INC_147 148
#define N00B_MAP_INC_148 149
#define N00B_MAP_INC_149 150
#define N00B_MAP_INC_150 151
#define N00B_MAP_INC_151 152
#define N00B_MAP_INC_152 153
#define N00B_MAP_INC_153 154
#define N00B_MAP_INC_154 155
#define N00B_MAP_INC_155 156
#define N00B_MAP_INC_156 157
#define N00B_MAP_INC_157 158
#define N00B_MAP_INC_158 159
#define N00B_MAP_INC_159 160
#define N00B_MAP_INC_160 161
#define N00B_MAP_INC_161 162
#define N00B_MAP_INC_162 163
#define N00B_MAP_INC_163 164
#define N00B_MAP_INC_164 165
#define N00B_MAP_INC_165 166
#define N00B_MAP_INC_166 167
#define N00B_MAP_INC_167 168
#define N00B_MAP_INC_168 169
#define N00B_MAP_INC_169 170
#define N00B_MAP_INC_170 171
#define N00B_MAP_INC_171 172
#define N00B_MAP_INC_172 173
#define N00B_MAP_INC_173 174
#define N00B_MAP_INC_174 175
#define N00B_MAP_INC_175 176
#define N00B_MAP_INC_176 177
#define N00B_MAP_INC_177 178
#define N00B_MAP_INC_178 179
#define N00B_MAP_INC_179 180
#define N00B_MAP_INC_180 181
#define N00B_MAP_INC_181 182
#define N00B_MAP_INC_182 183
#define N00B_MAP_INC_183 184
#define N00B_MAP_INC_184 185
#define N00B_MAP_INC_185 186
#define N00B_MAP_INC_186 187
#define N00B_MAP_INC_187 188
#define N00B_MAP_INC_188 189
#define N00B_MAP_INC_189 190
#define N00B_MAP_INC_190 191
#define N00B_MAP_INC_191 192
#define N00B_MAP_INC_192 193
#define N00B_MAP_INC_193 194
#define N00B_MAP_INC_194 195
#define N00B_MAP_INC_195 196
#define N00B_MAP_INC_196 197
#define N00B_MAP_INC_197 198
#define N00B_MAP_INC_198 199
#define N00B_MAP_INC_199 200
#define N00B_MAP_INC_200 201
#define N00B_MAP_INC_201 202
#define N00B_MAP_INC_202 203
#define N00B_MAP_INC_203 204
#define N00B_MAP_INC_204 205
#define N00B_MAP_INC_205 206
#define N00B_MAP_INC_206 207
#define N00B_MAP_INC_207 208
#define N00B_MAP_INC_208 209
#define N00B_MAP_INC_209 210
#define N00B_MAP_INC_210 211
#define N00B_MAP_INC_211 212
#define N00B_MAP_INC_212 213
#define N00B_MAP_INC_213 214
#define N00B_MAP_INC_214 215
#define N00B_MAP_INC_215 216
#define N00B_MAP_INC_216 217
#define N00B_MAP_INC_217 218
#define N00B_MAP_INC_218 219
#define N00B_MAP_INC_219 220
#define N00B_MAP_INC_220 221
#define N00B_MAP_INC_221 222
#define N00B_MAP_INC_222 223
#define N00B_MAP_INC_223 224
#define N00B_MAP_INC_224 225
#define N00B_MAP_INC_225 226
#define N00B_MAP_INC_226 227
#define N00B_MAP_INC_227 228
#define N00B_MAP_INC_228 229
#define N00B_MAP_INC_229 230
#define N00B_MAP_INC_230 231
#define N00B_MAP_INC_231 232
#define N00B_MAP_INC_232 233
#define N00B_MAP_INC_233 234
#define N00B_MAP_INC_234 235
#define N00B_MAP_INC_235 236
#define N00B_MAP_INC_236 237
#define N00B_MAP_INC_237 238
#define N00B_MAP_INC_238 239
#define N00B_MAP_INC_239 240
#define N00B_MAP_INC_240 241
#define N00B_MAP_INC_241 242
#define N00B_MAP_INC_242 243
#define N00B_MAP_INC_243 244
#define N00B_MAP_INC_244 245
#define N00B_MAP_INC_245 246
#define N00B_MAP_INC_246 247
#define N00B_MAP_INC_247 248
#define N00B_MAP_INC_248 249
#define N00B_MAP_INC_249 250
#define N00B_MAP_INC_250 251
#define N00B_MAP_INC_251 252
#define N00B_MAP_INC_252 253
#define N00B_MAP_INC_253 254
#define N00B_MAP_INC_254 255
#define N00B_MAP_INC_255 256
#define N00B_MAP_INC_256 257
#define N00B_MAP_INC_257 258
#define N00B_MAP_INC_258 259
#define N00B_MAP_INC_259 260
#define N00B_MAP_INC_260 261
#define N00B_MAP_INC_261 262
#define N00B_MAP_INC_262 263
#define N00B_MAP_INC_263 264
#define N00B_MAP_INC_264 265
#define N00B_MAP_INC_265 266
#define N00B_MAP_INC_266 267
#define N00B_MAP_INC_267 268
#define N00B_MAP_INC_268 269
#define N00B_MAP_INC_269 270
#define N00B_MAP_INC_270 271
#define N00B_MAP_INC_271 272
#define N00B_MAP_INC_272 273
#define N00B_MAP_INC_273 274
#define N00B_MAP_INC_274 275
#define N00B_MAP_INC_275 276
#define N00B_MAP_INC_276 277
#define N00B_MAP_INC_277 278
#define N00B_MAP_INC_278 279
#define N00B_MAP_INC_279 280
#define N00B_MAP_INC_280 281
#define N00B_MAP_INC_281 282
#define N00B_MAP_INC_282 283
#define N00B_MAP_INC_283 284
#define N00B_MAP_INC_284 285
#define N00B_MAP_INC_285 286
#define N00B_MAP_INC_286 287
#define N00B_MAP_INC_287 288
#define N00B_MAP_INC_288 289
#define N00B_MAP_INC_289 290
#define N00B_MAP_INC_290 291
#define N00B_MAP_INC_291 292
#define N00B_MAP_INC_292 293
#define N00B_MAP_INC_293 294
#define N00B_MAP_INC_294 295
#define N00B_MAP_INC_295 296
#define N00B_MAP_INC_296 297
#define N00B_MAP_INC_297 298
#define N00B_MAP_INC_298 299
#define N00B_MAP_INC_299 300
#define N00B_MAP_INC_300 301
#define N00B_MAP_INC_301 302
#define N00B_MAP_INC_302 303
#define N00B_MAP_INC_303 304
#define N00B_MAP_INC_304 305
#define N00B_MAP_INC_305 306
#define N00B_MAP_INC_306 307
#define N00B_MAP_INC_307 308
#define N00B_MAP_INC_308 309
#define N00B_MAP_INC_309 310
#define N00B_MAP_INC_310 311
#define N00B_MAP_INC_311 312
#define N00B_MAP_INC_312 313
#define N00B_MAP_INC_313 314
#define N00B_MAP_INC_314 315
#define N00B_MAP_INC_315 316
#define N00B_MAP_INC_316 317
#define N00B_MAP_INC_317 318
#define N00B_MAP_INC_318 319
#define N00B_MAP_INC_319 320
#define N00B_MAP_INC_320 321
#define N00B_MAP_INC_321 322
#define N00B_MAP_INC_322 323
#define N00B_MAP_INC_323 324
#define N00B_MAP_INC_324 325
#define N00B_MAP_INC_325 326
#define N00B_MAP_INC_326 327
#define N00B_MAP_INC_327 328
#define N00B_MAP_INC_328 329
#define N00B_MAP_INC_329 330
#define N00B_MAP_INC_330 331
#define N00B_MAP_INC_331 332
#define N00B_MAP_INC_332 333
#define N00B_MAP_INC_333 334
#define N00B_MAP_INC_334 335
#define N00B_MAP_INC_335 336
#define N00B_MAP_INC_336 337
#define N00B_MAP_INC_337 338
#define N00B_MAP_INC_338 339
#define N00B_MAP_INC_339 340
#define N00B_MAP_INC_340 341
#define N00B_MAP_INC_341 342
#define N00B_MAP_INC_342 343
#define N00B_MAP_INC_343 344
#define N00B_MAP_INC_344 345
#define N00B_MAP_INC_345 346
#define N00B_MAP_INC_346 347
#define N00B_MAP_INC_347 348
#define N00B_MAP_INC_348 349
#define N00B_MAP_INC_349 350
#define N00B_MAP_INC_350 351
#define N00B_MAP_INC_351 352
#define N00B_MAP_INC_352 353
#define N00B_MAP_INC_353 354
#define N00B_MAP_INC_354 355
#define N00B_MAP_INC_355 356
#define N00B_MAP_INC_356 357
#define N00B_MAP_INC_357 358
#define N00B_MAP_INC_358 359
#define N00B_MAP_INC_359 360
#define N00B_MAP_INC_360 361
#define N00B_MAP_INC_361 362
#define N00B_MAP_INC_362 363
#define N00B_MAP_INC_363 364
#define N00B_MAP_INC_364 365
#define N00B_MAP_INC_365 366
