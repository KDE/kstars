#ifndef BASE64_H
#define BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup base64 Functions to convert from and to base64
 */
/*@{*/

/** \brief Convert bytes array to base64. 
    \param out output buffer in base64. The buffer size must be at least (4 * inlen / 3 + 4) bytes long.
    \param in input binary buffer
    \param inlen number of bytes to convert
    \return 0 on success, -1 on failure.
 */
extern int to64frombits(unsigned char *out, const unsigned char *in,
    int inlen);
    
/** \brief Convert base64 to bytes array.
    \param out output buffer in bytes. The buffer size must be at least (3 * size_of_in_buffer / 4) bytes long.
    \param in input base64 buffer
    \return 0 on success, -1 on failure.
 */

extern int from64tobits(char *out, const char *in);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif
