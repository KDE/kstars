/*----------------------------------------------------------------------------*/
/**
  @file     xmemory.c
  @author   Nicolas Devillard
  @date     Oct 2000
  @version  $Revision$
  @brief    POSIX-compatible extended memory handling.

  xmemory is a small and efficient module offering memory extension 
  capabitilies to ANSI C programs running on POSIX-compliant systems. It
  offers several useful features such as memory leak detection, protection for 
  free on NULL or unallocated pointers, and virtually unlimited memory space.
  xmemory requires the @c mmap() system call to be implemented in the local C 
  library to function. This module has been tested on a number of current Unix 
  flavours and is reported to work fine.
  The current limitation is the limited number of pointers it can handle at
  the same time.
  See the documentation attached to this module for more information.
*/
/*----------------------------------------------------------------------------*/

/*
    $Id$
    $Author$
    $Date$
    $Revision$
*/

/*-----------------------------------------------------------------------------
                                Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>

/*-----------------------------------------------------------------------------
                                Defines
 -----------------------------------------------------------------------------*/

/**
  This symbol sets the debug level for the xmemory module. Debug levels are 
  defined as follows:

  0   no debugging.
  1   add tracing for memory leaks and diagnostics in xmemory_status 
  2   add lots of debug messages
*/
#ifndef XMEMORY_DEBUG
#define XMEMORY_DEBUG       0
#endif

/** Initial number of entries in memory table */
/* In order so ensure good performance, this number should be
   somewhat higher that the actual number of pointers used */
#ifndef XMEMORY_MAXPTRS
#define XMEMORY_MAXPTRS     8192
#endif

/** Identify true RAM memory */
#define MEMTYPE_RAM         'R'
/** Identify swap memory */
#define MEMTYPE_SWAP        'S'
/** Identify memory-mapped file */
#define MEMTYPE_MMAP        'M'

/** Minimal page size in bytes */
#define MEMPAGESZ           2048

/** Size of temporary dir name */
#define TMPDIRNAMESZ        1024

/** Size of temporary file names */
#define TMPFILENAMESZ       1024

/** Size of source file names */
#define SRCFILENAMESZ       64

/** Size of mapped file names */
#define MAPFILENAMESZ       512

/*-----------------------------------------------------------------------------
                                Macros
 -----------------------------------------------------------------------------*/

/**
  @def      xmem_debug
  @brief    Macro to hide away debug code at compile time
 */
#if (XMEMORY_DEBUG>=2)
#define xmem_debug( code ) { code }
#else
#define xmem_debug( code )
#endif

/* A very simple hash */
#define PTR_HASH(ptr) (((unsigned long int) ptr) % XMEMORY_MAXPTRS)

/*-----------------------------------------------------------------------------
                        Private variables
 -----------------------------------------------------------------------------*/

/** Active flag */
static int  xmemory_active=1 ;

/** Initialization flag */
static int  xmemory_initialized=0 ;

/** Path to temporary directory */
static char xmemory_tmpdirname[TMPDIRNAMESZ] = "." ;

/*----------------------------------------------------------------------------*/
/**
  @var      xmemory_table
  @brief    Main memory table (INTERNAL)

  This table holds a list pointer cells (all the ones allocated so far).
  It is strictly internal to this source file.
 */
/*----------------------------------------------------------------------------*/
static struct {
    /** Number of active cells */
    int         ncells ;
    /** List of pointers (outside of cells for efficiency reason) */
    void    *   pointer[XMEMORY_MAXPTRS] ;
    /** Memory cells */
    struct {
        /* Common to all pointers */
        /** Pointed size in bytes */
        size_t      size ;

#if (XMEMORY_DEBUG>=1)
        /** Name of the source file where the alloc was requested */
        char        filename[SRCFILENAMESZ] ;
        /** Line number where the alloc was requested */
        int         lineno ;
#endif
        /** Memory type: RAM, swap, or mapped file */
        char        memtype ;

        /* Swap memory only */
        /** Swap file ID */
        int         swapfileid ;
        /** Swap file descriptor */
        int         swapfd ;

        /* Mapped files only */
        /** Name of mapped file */
        char        mm_filename[MAPFILENAMESZ];
        /** Hash of mapped file name for quick search */
        unsigned    mm_hash ;
        /** Reference counter for this pointer */
        int         mm_refcount ;
    } cell[XMEMORY_MAXPTRS] ;

    /** Total allocated memory in bytes */
    size_t              alloc_total ;
    /** Total allocated RAM in bytes */
    size_t              alloc_ram ;
    /** Total allocated VM in bytes */
    size_t              alloc_swap ;
    /** Peak allocation ever seen for diagnostics */
    size_t              alloc_max ;
    /** Peak number of pointers ever seen for diagnostics */
    int                 max_cells ;

    /** Current number of swap files */
    int                 nswapfiles ;
    /** Registration counter for swap files */
    int                 file_reg ;

    /** Current number of memory-mapped files */
    int                 n_mm_files ;
    /** Current number of mappings derived from files */
    int                 n_mm_mappings ;

#ifdef __linux__
    /** Page size in bytes (Linux only) */
    int                 pagesize ;
    /** Value found for RLIMIT_DATA (Linux only) */
    int                 rlimit_data ;
#endif

} xmemory_table ;

/*-----------------------------------------------------------------------------
                    Private function prototypes 
 -----------------------------------------------------------------------------*/

static unsigned xmemory_hash(char *) ;
static void xmemory_init(void) ;
static void xmemory_cleanup(void);
static int xmemory_addcell(void*, size_t, char*, int, char, int, int, char*) ;
static int xmemory_remcell(int) ;
static void xmemory_dumpcell(int, FILE*) ;
static char * xmemory_tmpfilename(int) ;
void xmemory_status_(char *, int) ;

/*-----------------------------------------------------------------------------
                            Function codes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Activate xmemory
  @return   void

  This function activates xmemory activity. Make sure it is always used 
  consistently with xmemory_off().
 */
/*----------------------------------------------------------------------------*/
void xmemory_on(void)
{
    xmemory_active = 1 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Deactivate xmemory
  @return   void

  This function deactivates xmemory activity. Make sure it is always used 
  consistently with xmemory_on().
 */
/*----------------------------------------------------------------------------*/
void xmemory_off(void)
{
    xmemory_active = 0 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Hash a string to an unsigned value.
  @param    key     String to hash
  @return   1 unsigned value as a hash for the given string.
  
  This hash function has been taken from an Article in Dr Dobbs Journal. This 
  is normally a collision-free function, distributing keys evenly. The key is 
  stored anyway in the struct so that collision can be avoided by comparing the
  key itself in last resort.
 */
/*----------------------------------------------------------------------------*/
static unsigned xmemory_hash(char * key)
{
    int         len ;
    unsigned    hash ;
    int         i ;

    len = strlen(key);
    for (hash=0, i=0 ; i<len ; i++) {
        hash += (unsigned)key[i] ;
        hash += (hash<<10);
        hash ^= (hash>>6) ;
    }
    hash += (hash <<3);
    hash ^= (hash >>11);
    hash += (hash <<15);
    return hash ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Initialize extended memory features.
  @return   void

  This function is implicitly called by the first malloc() or calloc() or 
  strdup() execution. It allocates a minimal number of memory cells into
  the global extended memory table. It also install atexit routines the first 
  time it is called, and increases the number of possible descriptors to the 
  maximum.
 */
/*----------------------------------------------------------------------------*/
static void xmemory_init(void)
{
    struct rlimit rlim ;

    xmem_debug(
        fprintf(stderr,
                "xmem: initializing main table size=%d ptrs (%ld bytes)\n",
                XMEMORY_MAXPTRS,
                (long)sizeof(xmemory_table));
    );
    /* Initialize memory table */
    memset(&xmemory_table, 0, sizeof(xmemory_table));

    /* Install cleanup routine at exit */
    atexit(xmemory_cleanup);
        
    /* Increase number of descriptors to maximum */
    getrlimit(RLIMIT_NOFILE, &rlim) ;
    xmem_debug(
        fprintf(stderr, "xmem: increasing from %ld to %ld file handles\n",
                (long)rlim.rlim_cur,
                (long)rlim.rlim_max);
    );
    rlim.rlim_cur = rlim.rlim_max ;
    setrlimit(RLIMIT_NOFILE, &rlim) ;

#ifdef __linux__
    /* Get RLIMIT_DATA on Linux */
    getrlimit(RLIMIT_DATA, &rlim);
    xmemory_table.rlimit_data = rlim.rlim_cur ;
    xmem_debug(
        fprintf(stderr, "xmem: got RLIMIT_DATA=%d\n",
                xmemory_table.rlimit_data);
    );
    /* Get page size on Linux */
    xmemory_table.pagesize = getpagesize();

#endif
    return ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Removes all swap files.
  @return   void

  This routine will delete all swap files from the temporary area.
 */
/*----------------------------------------------------------------------------*/
static void xmemory_cleanup(void)
{
    int     reg ;

    if (xmemory_table.file_reg>0) {
        xmem_debug(
            fprintf(stderr, "xmem: cleaning up swap files... ");
        );
        /*
         * Call remove() on all possible VM files. If the file exists, it
         * is effectively removed. It it does not, ignore the error.
         * This is not the cleanest way of doing it, but this function is
         * meant to be called also in cases of emergency (e.g. segfault),
         * so it should not rely on a correct memory table.
         */
        for (reg=0 ; reg<xmemory_table.file_reg ; reg++) {
            remove(xmemory_tmpfilename(reg+1));
        }
        xmem_debug(
            fprintf(stderr, "xmem: done cleaning swap files\n");
        );
    }
    return ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Add allocation cell to xmemory_table.
  @param    pointer         Pointer value.
  @param    size            Pointer size.
  @param    filename        Name of the C source file where alloc was done.
  @param    lineno          Line # where the allocation took place.
  @param    memtype         Memory type: RAM or SWAP.
  @param    swapfileid      Associated swap file ID (if any).
  @param    swapfd          Associated swap file descriptor (if any).
  @param    mm_filename     Mapped file name (if any)
  @return   the index in the xmemory_table of the added cell

  Add a memory cell in the xtended memory table to register that a new 
  allocation took place. 
  This call is not protected against illegal parameter values, so make sure 
  the passed values are correct!
 */
/*----------------------------------------------------------------------------*/
static int xmemory_addcell(
        void    *   pointer,
        size_t      size,
        char    *   filename,
        int         lineno,
        char        memtype,
        int         swapfileid,
        int         swapfd,
        char    *   mm_filename)
{
    int pos, ii ;
    
    /* Check there is still some space left */
    if (xmemory_table.ncells >= XMEMORY_MAXPTRS) {
        fprintf(stderr, "fatal xmemory error: reached max pointers (%d)\n",
                XMEMORY_MAXPTRS);
        exit(-1);
    }
    /* Find an available slot */
        pos = PTR_HASH(pointer);
        for (ii = 0 ; ii<XMEMORY_MAXPTRS ; ii++) {
            if (++pos == XMEMORY_MAXPTRS) pos = 0;
            if (xmemory_table.pointer[pos] == NULL) break ;
        }
        xmem_debug(
            fprintf(stderr, "xmem: freecell found at pos %d\n", pos);
        );
    /* Store information */
    xmemory_table.pointer[pos] = pointer ;
    xmemory_table.cell[pos].size = size ;

    /* Filename and line number */
#if (XMEMORY_DEBUG>=1)
    if (filename) {
        strncpy(xmemory_table.cell[pos].filename, filename, SRCFILENAMESZ-1) ;
    } else {
        xmemory_table.cell[pos].filename[0] = (char)0 ;
    }
    xmemory_table.cell[pos].lineno = lineno ;
#endif

    xmemory_table.cell[pos].memtype = memtype ;
    xmemory_table.cell[pos].swapfileid = swapfileid ;
    xmemory_table.cell[pos].swapfd = swapfd ;

    if (mm_filename!=NULL) {
        strncpy(xmemory_table.cell[pos].mm_filename, mm_filename,MAPFILENAMESZ);
        xmemory_table.cell[pos].mm_hash = xmemory_hash(mm_filename);
        xmemory_table.cell[pos].mm_refcount = 1 ;
    } else {
        xmemory_table.cell[pos].mm_filename[0] = 0 ;
        xmemory_table.cell[pos].mm_hash = 0 ;
        xmemory_table.cell[pos].mm_refcount = 0 ;
    }
    xmemory_table.ncells ++ ;
    if (xmemory_table.ncells > xmemory_table.max_cells)
        xmemory_table.max_cells = xmemory_table.ncells ;
    return pos ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Remove a memory cell from the xtended memory table.
  @param    pos     Position of the pointer in the table.
  @return   int 0 if Ok, -1 if error occurred.

  Remove the specified cell in xmemory_table. 
  This call is not protected against illegal parameter values, so make sure 
  the passed values are correct!
 */
/*----------------------------------------------------------------------------*/
static int xmemory_remcell(int pos)
{
    xmem_debug(
        fprintf(stderr, "xmem: removing cell from pos %d (cached pos)\n", pos);
    );
    /* Set pointer to NULL */
    xmemory_table.pointer[pos] = NULL ;
    /* Decrement number of allocated pointers */
    xmemory_table.ncells -- ;
    return 0 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Dump a memory cell to an open file pointer.
  @param    cell    Cell to dump.
  @param    out     Open file pointer to dump to.
  @return   void

  This function is meant for debugging purposes only. It takes in input a
  pointer to a memory cell and dumps it to the requested file pointer (it
  is Ok to provide stdout or stderr as file pointers). If the passed
  position is invalid or the table pointer is NULL, this function returns
  immediately.
 */
/*----------------------------------------------------------------------------*/
static void xmemory_dumpcell(int pos, FILE * out)
{
    if (pos<0 || pos>=XMEMORY_MAXPTRS) return ;
    if (xmemory_table.pointer[pos]==NULL) return ;

    if (xmemory_table.cell[pos].memtype == MEMTYPE_MMAP) {
#if (XMEMORY_DEBUG>=1)
        fprintf(out,
            "M(%p) - %s (%d) maps [%s] for %ld bytes",
            xmemory_table.pointer[pos],
            xmemory_table.cell[pos].filename,
            xmemory_table.cell[pos].lineno,
            xmemory_table.cell[pos].mm_filename,
            (long)xmemory_table.cell[pos].size);
#else
        fprintf(out,
            "M(%p) maps [%s] for %ld bytes",
            xmemory_table.pointer[pos],
            xmemory_table.cell[pos].mm_filename,
            (long)xmemory_table.cell[pos].size);
#endif
    } else {
#if (XMEMORY_DEBUG>=1)
        fprintf(out, "%c(%p) - %s (%d) for %ld bytes",
            xmemory_table.cell[pos].memtype,
            xmemory_table.pointer[pos],
            xmemory_table.cell[pos].filename,
            xmemory_table.cell[pos].lineno,
            (long)xmemory_table.cell[pos].size);
#else
        fprintf(out, "%c(%p) for %ld bytes",
            xmemory_table.cell[pos].memtype,
            xmemory_table.pointer[pos],
            (long)xmemory_table.cell[pos].size);
#endif
    }
    if (xmemory_table.cell[pos].memtype==MEMTYPE_SWAP) {
        fprintf(out, " swf[%s][%d]",
                xmemory_tmpfilename(xmemory_table.cell[pos].swapfileid),
                xmemory_table.cell[pos].swapfd);
    }
    fprintf(out, "\n");
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Compute filename associated to a temporary file ID.
  @param    reg     Registration number of temporary file name.
  @return   pointer to statically allocated char string.

  This function computes the valid file name associated to a temporary file
  ID. It computes the result, stores it in an internal static string and
  returns a pointer to it.
 */
/*----------------------------------------------------------------------------*/
static char * xmemory_tmpfilename(int reg)
{
    static char xmem_tmpfilename[TMPFILENAMESZ] ;
    /* Create file name using tmp directory as a base */
    sprintf(xmem_tmpfilename, "%s/vmswap_%05ld_%05x", xmemory_tmpdirname,
            (long)getpid(), reg) ;
    return xmem_tmpfilename ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Allocate memory.
  @param    size        Size (in bytes) to allocate.
  @param    filename    Name of the file where the alloc took place.
  @param    lineno      Line number in the file.
  @return   1 newly allocated pointer.

  This function is a replacement call for malloc. It should never be called
  directly but through a macro instead, as:

  @code
  xmemory_malloc(size, __FILE__, __LINE__)
  @endcode
 */
/*----------------------------------------------------------------------------*/
void * xmemory_malloc(size_t size, char * filename, int lineno)
{
    void    *   ptr ;
    char    *   fname ;
    int         swapfileid ;
    int         swapfd ;
    char        wbuf[MEMPAGESZ] ;
    int         nbufs ;
    int         memtype ;
    int         i ;
    int         pos ;
#ifdef __linux__
    int         p ;
#endif

    if (xmemory_active==0) return malloc(size);

    /* Initialize table if needed */
    if (xmemory_initialized==0) {
        xmemory_init() ;
        xmemory_initialized++ ;
    }

    /* Protect the call */
    if (size==0) {
        xmem_debug(
            fprintf(stderr, "xmem: malloc called with 0 size - %s (%d)\n",
                    filename, lineno);
        );
        return NULL ;
    }

    /* Try to allocate in memory */
#ifdef __linux__
    /* Linux does not honor the RLIMIT_DATA limit.
     * The only way to limit the amount of memory taken by
     * a process is to set RLIMIT_AS, which unfortunately also
     * limits down the maximal amount of memory addressable with
     * mmap() calls, making on-the-fly swap space creation useless
     * in this module. To avoid this, the RLIMIT_DATA value
     * is honored here with this test.
     */
    ptr = NULL ;
    if (xmemory_table.rlimit_data<1) {
        /* No limit set on RLIMIT_DATA: proceed with malloc */
        ptr = malloc(size);
    } else if (xmemory_table.alloc_total+size <= xmemory_table.rlimit_data) {
        /* Next allocation will still be within limits: proceed */
        ptr = malloc(size);
    }
#else
    ptr = malloc(size);
#endif
    if (ptr==NULL) {
        /* No more RAM available: try to allocate private swap */
        xmem_debug(
            fprintf(stderr, "xmem: hit a NULL pointer -- swapping\n");
        );

        /* Create swap file with rights: rw-rw-rw- */
        swapfileid = ++ xmemory_table.file_reg ;
        fname = xmemory_tmpfilename(swapfileid);
        swapfd = open(fname, O_RDWR | O_CREAT);
        if (swapfd==-1) {
            fprintf(stderr, "xmem: fatal error: cannot create swap file\n");
            exit(-1);
        }
        fchmod(swapfd, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

        /* Compute number of passes to insert buffer */
        nbufs = size / MEMPAGESZ ;
        if (size % MEMPAGESZ != 0) nbufs ++ ;

        /* Dump empty buffers into file */
        memset(wbuf, 0, MEMPAGESZ);
        for (i=0 ; i<nbufs ; i++) {
            if (write(swapfd, wbuf, MEMPAGESZ)==-1) {
                perror("write");
                fprintf(stderr,
                        "xmem: fatal error: cannot create swapfile\n");
                close(swapfd);
                remove(fname);
                exit(-1);
            }
        }

        /* mmap() the swap file */
        ptr = (void*)mmap(0,
                          nbufs * MEMPAGESZ,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE,
                          swapfd,
                          0);
        if ((char*)ptr == (char*)-1) {
            perror("mmap");
            fprintf(stderr,
                    "xmem: fatal error: mmap failed for swap file\n");
            close(swapfd);
            remove(fname);
            exit(-1);
        }

        xmem_debug(
            fprintf(stderr, "xmem: swap [%s] created for %ld bytes\n", fname, 
                (long)size);
        );

        memtype = MEMTYPE_SWAP ;
        xmemory_table.alloc_swap += size ;
        xmemory_table.nswapfiles ++ ;
    } else {
        /* Memory allocation succeeded */
#ifdef __linux__
        /*
         * On Linux, the returned pointer might not be honored later.
         * To make sure the returned memory is actually usable, it has to
         * be touched. The following will touch one byte every 'pagesize'
         * bytes to make sure all blocks are visited and properly allocated
         * by the OS.
         */
        xmem_debug(
            fprintf(stderr, "xmem: touching memory (Linux)\n");
        );
        for (p=0 ; p<size ; p+=xmemory_table.pagesize) ((char*)ptr)[p] = 0 ;
#endif
        swapfd = -1 ;
        swapfileid = -1 ;
        memtype = MEMTYPE_RAM ;
        xmemory_table.alloc_ram   += size ;
    }
    
    /* Print out message in debug mode */
    xmem_debug(
        fprintf(stderr, "xmem: %p alloc(%ld) in %s (%d)\n", ptr, (long)size, 
            filename, lineno) ;
    );

    /* Add cell into general table */
    pos = xmemory_addcell(  ptr,
                            size,
                            filename,
                            lineno,
                            memtype,
                            swapfileid,
                            swapfd,
                            NULL);
    /* Adjust size */
    xmemory_table.alloc_total += size ;
    /* Remember biggest allocated block */
    if (xmemory_table.alloc_total > xmemory_table.alloc_max)
        xmemory_table.alloc_max = xmemory_table.alloc_total ;

    /* Insert memory stamp */
    return (void*)ptr ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Allocate memory.
  @param    nmemb       Number of elements to allocate.
  @param    size        Size (in bytes) of each element.
  @param    filename    Name of the file where the alloc took place.
  @param    lineno      Line number in the file.
  @return   1 newly allocated pointer.

  This function is a replacement call for calloc. It should never be called
  directly but through a macro instead, as:

  @code
  xmemory_calloc(nmemb, size, __FILE__, __LINE__)
  @endcode
 */
/*----------------------------------------------------------------------------*/
void * xmemory_calloc(size_t nmemb, size_t size, char * filename, int lineno)
{
    void    *   ptr ;

    if (xmemory_active==0) return calloc(nmemb, size) ;
    ptr = xmemory_malloc(nmemb * size, filename, lineno) ;
    return memset(ptr, 0, nmemb * size) ;
}
/*----------------------------------------------------------------------------*/
/**
  @brief    Map a file's contents to memory as a char pointer.
  @param    name        Name of the file to map
  @param    offs        Offset to the first mapped byte in file.
  @param    size        Returned size of the mapped file in bytes.
  @param    srcname     Name of the source file making the call.
  @param    srclin      Line # where the call was made.
  @return   A pointer to char, to be freed using xmemory_free().

  This function takes in input the name of a file. It tries to map the file 
  into memory and if it succeeds, returns the file's contents as a char pointer.
  It also modifies the input size variable to be the size of the mapped file in
  bytes. This function is normally never directly called but through the 
  falloc() macro.

  The offset indicates the starting point for the mapping, i.e. if you are not 
  interested in mapping the whole file but only from a given place.
 */
/*----------------------------------------------------------------------------*/
char * xmemory_falloc(
        char    *   name,
        size_t      offs,
        size_t  *   size,
        char    *   srcname,
        int         srclin)
{
    unsigned        mm_hash ;
    char        *   ptr ;
    struct stat     sta ;
    int             fd ;
    int             nptrs ;
    int             i ;

    if (xmemory_active==0) {
        fprintf(stderr, "xmem: cannot use falloc() when xmemory inactive\n");
        return NULL ;
    }
    /* Protect the call */
    if (offs<0) {
        xmem_debug(
            fprintf(stderr, "xmem: falloc offs<0 - %s (%d)\n",
                    srcname, srclin);
        );
        return NULL ;
    }
    if (size!=NULL) *size = 0 ;

    /* Initialize table if needed */
    if (xmemory_initialized==0) {
        xmemory_init() ;
        xmemory_initialized++ ;
    }

    if (xmemory_table.ncells>0) {
        /* Check if file has already been mapped */
        /* Compute hash for this name */
        mm_hash = xmemory_hash(name);
        /* Loop over all memory cells */
        nptrs=0 ;
        for (i=0 ; i<XMEMORY_MAXPTRS ; i++) {
            if (xmemory_table.pointer[i]!=NULL)
                nptrs++ ;
            if ((xmemory_table.pointer[i]!=NULL) &&
                (xmemory_table.cell[i].mm_filename != NULL) &&
                (xmemory_table.cell[i].mm_hash == mm_hash)) {
                if (!strncmp(xmemory_table.cell[i].mm_filename, name,
                             MAPFILENAMESZ)) {
                    /* File already mapped */
                    /* Check offset consistency wrt file size */
                    if (offs >= xmemory_table.cell[i].size) {
                        xmem_debug(
                            fprintf(stderr,
                                "xmem: falloc offset larger than file size");
                        );
                        return NULL ;
                    }
                    /* Increase reference counter */
                    xmemory_table.cell[i].mm_refcount ++ ;
                    xmem_debug(
                        fprintf(stderr,
                                "xmem: incref on %s (%d mappings)\n",
                                name,
                                xmemory_table.cell[i].mm_refcount);
                    );
                    /* Increase number of mappings */
                    xmemory_table.n_mm_mappings ++ ;
                    /* Build up return pointer */
                    ptr = (char*)xmemory_table.pointer[i] + offs ;
                    /* Available size is filesize minus offset */
                    if (size!=NULL) {
                        *size = xmemory_table.cell[i].size - offs ;
                    }
                    /* Return constructed pointer as void * */
                    return (void*)ptr ;
                }
            }
            if (nptrs>=xmemory_table.ncells) break ;
        }
    }

    /* First mapping attempt for this file */
    /* Check file's existence and compute its size */
    if (stat(name, &sta)==-1) {
        xmem_debug(
            fprintf(stderr, "xmem: cannot stat file %s - %s (%d)\n",
                    name, srcname, srclin);
        );
        return NULL ;
    }
    /* Check offset request does not go past end of file */
    if (offs>=sta.st_size) {
        xmem_debug(
            fprintf(stderr,
                "xmem: falloc offsets larger than file size");
        );
        return NULL ;
    }

    /* Open file */
    if ((fd=open(name, O_RDONLY))==-1) {
        xmem_debug(
            fprintf(stderr, "xmem: cannot open file %s - %s (%d)\n",
                    name, srcname, srclin);
        );
        return NULL ;
    }

    /* Memory-map input file */
    ptr = (char*)mmap(0, sta.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,fd,0);
    
    /* Close file */
    close(fd);
    if (ptr == (char*)-1 || ptr==NULL) {
        xmem_debug(
            perror("mmap");
            fprintf(stderr, "xmem: falloc cannot mmap file %s", name);
        );
        return NULL ;
    }

    xmemory_table.n_mm_files ++ ;
    xmemory_table.n_mm_mappings ++ ;
    xmem_debug(
        fprintf(stderr,
                "xmem: falloc mmap succeeded for [%s] - %s (%d)\n",
                name, srcname, srclin);
    );

    /* Add cell into general table */
    (void) xmemory_addcell((void*)ptr, sta.st_size, srcname, srclin, 
                           MEMTYPE_MMAP, -1, -1, name) ;

    if (size!=NULL) (*size) = sta.st_size ;
    
    return ptr + offs ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Free memory.
  @param    ptr         Pointer to free.
  @param    filename    Name of the file where the dealloc took place.
  @param    lineno      Line number in the file.
  @return   void

  Free the memory associated to a given pointer. Prints out a warning on stderr
  if the requested pointer is NULL or cannot be found in the extended memory 
  table.
 */
/*----------------------------------------------------------------------------*/
void xmemory_free(void * ptr, char * filename, int lineno)
{
    int     i ;
    int     pos ;
    char *  swapname ;
    int     nptrs ;
    int     ii;

    /* Do nothing for a NULL pointer */
    if (ptr==NULL) {
        /* Output a warning */
        fprintf(stderr, "xmem: free requested on NULL pointer -- %s (%d)\n",
                filename, lineno);
        return ;
    }
    if (xmemory_active==0) {
        free(ptr);
        return ;
    }

    /* Locate pointer in main table */
    nptrs = 0 ;
    pos = -1 ;
    i = PTR_HASH(ptr);
    for (ii=0 ; ii<XMEMORY_MAXPTRS ; ii++) {
        if (++i == XMEMORY_MAXPTRS) i = 0;
        if (xmemory_table.pointer[i] == NULL) continue ;
        nptrs++ ;
        if (xmemory_table.pointer[i] == ptr) {
            pos=i ;
            break ;
        }
        if (xmemory_table.cell[i].memtype==MEMTYPE_MMAP) {
            if (((char*)xmemory_table.pointer[i]<=(char*)ptr) &&
                (((char*)xmemory_table.pointer[i] + 
                  xmemory_table.cell[i].size) >= (char*)ptr)) {
                pos = i ;
                break ;
            }
        }
        if (nptrs>=xmemory_table.ncells) break ;
    }
    if (pos==-1) {
        fprintf(stderr,
                "xmem: %s (%d) free requested on unallocated pointer (%p)\n",
                filename, lineno, ptr);
        /* Pointer sent to system's free() function, maybe it should not? */
        free(ptr);
        return ;
    }

    /* Deallocate pointer */
    switch (xmemory_table.cell[pos].memtype) {
        case MEMTYPE_RAM:
            /* --- RAM pointer */
            /* Free normal memory pointer */
            free(ptr);
            xmemory_table.alloc_ram -= xmemory_table.cell[pos].size ;
            break ;
        case MEMTYPE_SWAP:
            /* --- SWAP pointer */
            swapname = xmemory_tmpfilename(xmemory_table.cell[pos].swapfileid);
            xmem_debug(
                    fprintf(stderr, "xmem: deallocating swap file [%s]\n", 
                        swapname);
            );
            /* Munmap file */
            if (munmap(ptr, xmemory_table.cell[pos].size)!=0) {
                xmem_debug( perror("munmap"); );
            }
            /* Close swap file */
            if (close(xmemory_table.cell[pos].swapfd)==-1) {
                xmem_debug( perror("close"); );
            }
            /* Remove swap file */
            if (remove(swapname)!=0) {
                xmem_debug( perror("remove"); );
            }
            xmemory_table.alloc_swap -= xmemory_table.cell[pos].size ;
            xmemory_table.nswapfiles -- ;
            break ;
        case MEMTYPE_MMAP:
            /* --- MEMORY-MAPPED pointer */
            /* Decrease reference count */
            xmemory_table.cell[pos].mm_refcount -- ;
            /* Decrease total number of mappings */
            xmemory_table.n_mm_mappings -- ;
            /* Non-null ref count means the file stays mapped */
            if (xmemory_table.cell[pos].mm_refcount>0) {
                xmem_debug(
                        fprintf(stderr, "xmem: decref on %s (%d mappings)\n",
                            xmemory_table.cell[pos].mm_filename,
                            xmemory_table.cell[pos].mm_refcount);
                );
                return ;
            }
            /* Ref count reached zero: unmap the file */
            xmem_debug(
                    fprintf(stderr,
                        "xmem: unmapping file %s\n",
                        xmemory_table.cell[pos].mm_filename);
            );
            munmap((char*)xmemory_table.pointer[pos],
                    xmemory_table.cell[pos].size);
            /* Decrease total number of mapped files */
            xmemory_table.n_mm_files -- ;
            break ;
        default:
            xmem_debug(
                    fprintf(stderr, "xmem: unknown memory cell type???");
            );
            break ;
    }

    if (xmemory_table.cell[pos].memtype!=MEMTYPE_MMAP) {
        /* Adjust allocated totals */
        xmemory_table.alloc_total -= xmemory_table.cell[pos].size ;

        /* Print out message in debug mode */
        xmem_debug(
            fprintf(stderr, "xmem: free(%p) %ld bytes in %s (%d)\n",
                    ptr,
                    (long)xmemory_table.cell[pos].size,
                    filename,
                    lineno);
        );
    }
    /* Remove cell from main table */
    xmemory_remcell(pos) ;
    return ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Re-Allocate memory.
  @param    ptr         Pointer to free.
  @param    size        Size (in bytes) to allocate.
  @param    filename    Name of the file where the alloc took place.
  @param    lineno      Line number in the file.
  @return   1 newly allocated pointer.

  This function is a replacement call for realloc. It should never be called
  directly but through a macro instead, as:

  @code
  xmemory_realloc(nmemb, size, __FILE__, __LINE__)
  @endcode
 */
/*----------------------------------------------------------------------------*/
void * xmemory_realloc(void * ptr, size_t size, char * filename, int lineno)
{
    void    *   ptr2 ;
    size_t      small_sz ;
    size_t      ptr_sz ;
    int         pos = -1 ;
    int         i ;
    
    if (xmemory_active==0) return realloc(ptr, size) ;
    if (ptr == NULL) return xmemory_malloc(size, filename, lineno) ;

    /* Get the pointer size */
    for (i=0 ; i<XMEMORY_MAXPTRS ; i++) {
        if (xmemory_table.pointer[i] == NULL) continue ;
        if (xmemory_table.pointer[i] == ptr) {
            pos = i ;
            break ;
        }
    }
    if (pos==-1) {
        fprintf(stderr,
                "xmem: %s (%d) realloc requested on unallocated pointer (%p)\n",
                filename, lineno, ptr);
        /* Pointer sent to system's realloc() function, maybe it should not? */
        return realloc(ptr, size) ;
    }
    ptr_sz = xmemory_table.cell[pos].size ;
    
    /* Compute the smaller size */
    small_sz = size < ptr_sz ? size : ptr_sz ;
    
    /* Allocate the new pointer */
    ptr2 = xmemory_malloc(size, filename, lineno) ;
    
    /* Copy the common data */
    memcpy(ptr2, ptr, small_sz) ;

    /* Free the passed ptr */
    xmemory_free(ptr, filename, lineno) ;
    
    /* Return  */
    return ptr2 ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Duplicate a string using calloc.
  @param    s       String to duplicate.
  @param    filename    Name of the file where the call took place.
  @param    lineno      Line number in the file.
  @return   1 newly allocated character string.

  This function calls in turn calloc to perform the allocation. It should
  never be called directly but only through a macro, like:

  @code
  xmemory_strdup(s, __FILE__, __LINE__)
  @endcode

  This function calls xmemory_malloc() to do the allocation.
 */
/*----------------------------------------------------------------------------*/
char * xmemory_strdup(char * s, char * filename, int lineno)
{
    char    *   t ;

    if (s==NULL) return NULL ;
    if (xmemory_active==0) return strdup(s);
    t = xmemory_malloc(1+strlen(s), filename, lineno);
    return strcpy(t, s);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Display memory status information.
  @return   void

  This function is meant for debugging purposes, but it is recommended to
  call it at the end of every executable making use of the extended memory
  features. This function should be called through the xmemory_status()
  macro, which provides automatically the name of the source file and line
  number where the call happens.
 */
/*----------------------------------------------------------------------------*/
void xmemory_status_(char * filename, int lineno)
{
    int     i ;

#if (XMEMORY_DEBUG>=1)
    fprintf(stderr, "#----- memory diagnostics called from %s (%d) --------\n",
            filename,
            lineno);

    fprintf(stderr,
            "#- Peak memory usage\n"
            "ALL_maxalloc_kb     %ld\n"
            "ALL_maxpointers     %d\n",
            (long)(xmemory_table.alloc_max/1024),
            xmemory_table.max_cells);
    fprintf(stderr,
            "#- Local implementation\n"
            "TAB_ptrs            %d\n"
            "TAB_size            %d bytes\n",
            XMEMORY_MAXPTRS,
            sizeof(xmemory_table));
#ifdef __linux__
    fprintf(stderr,
            "#- Linux specific\n"
            "LINUX_pagesize      %d bytes\n"
            "LINUX_RLIMIT_DATA   %d kb\n",
            xmemory_table.pagesize,
            xmemory_table.rlimit_data);
#endif
#endif

    if (xmemory_table.ncells<1) return ;
    fprintf(stderr, "#----- memory status called from %s (%d) --------\n",
            filename,
            lineno);

    fprintf(stderr,
            "#- ALL status\n"
            "ALL_npointers       %d\n"
            "ALL_size            %ld\n"
            "ALL_maxalloc_kb     %ld\n"
            "ALL_maxpointers     %d\n",
            xmemory_table.ncells,
            (long)xmemory_table.alloc_total,
            (long)(xmemory_table.alloc_max/1024),
            xmemory_table.max_cells);

    if (xmemory_table.alloc_ram > 0) {
        fprintf(stderr, 
                "#- RAM status\n"
                "RAM_alloc           %ld\n",
                (long)xmemory_table.alloc_ram);
    }
    if (xmemory_table.alloc_swap > 0) {
        fprintf(stderr,
                "#- SWP status\n"
                "SWP_alloc           %ld\n"
                "SWP_files           %d\n",
                (long)xmemory_table.alloc_swap,
                xmemory_table.nswapfiles);
    }

    if (xmemory_table.n_mm_files>0) {
        fprintf(stderr,
                "#- MAP status\n"
                "MAP_files           %d\n"
                "MAP_mappings        %d\n",
                xmemory_table.n_mm_files,
                xmemory_table.n_mm_mappings);
    }

    fprintf(stderr, "#- pointer details\n");
    for (i=0 ; i<XMEMORY_MAXPTRS; i++) {
        xmemory_dumpcell(i, stderr);
    }
    return ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Set name of temporary directory for VM files.
  @param    dirname     Name of assigned directory.
  @return   void

  This function assigns a new value for the temporary directory name. It does 
  not check the directory existence or writability, this is up to the caller to
  check that before calling this function.
  Provide a directory name without trailing slash, e.g. "/tmp".
 */
/*----------------------------------------------------------------------------*/
void xmemory_settmpdir(char * dirname)
{
    xmem_debug(
        fprintf(stderr, "xmem: setting tmp dirname to [%s]\n", dirname);
    );
    strncpy(xmemory_tmpdirname, dirname, TMPDIRNAMESZ);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Get name of temporary directory for VM files.
  @return   pointer to statically allocated string within this module.

  This function returns a pointer to an internal (static) string giving the
  current name for temporary directory. Do not modify or try to free the
  returned string.
 */
/*----------------------------------------------------------------------------*/
char * xmemory_gettmpdir(void)
{
    xmem_debug(
        fprintf(stderr,
                "xmem: current tmp dirname is [%s]\n",
                xmemory_tmpdirname);
    );
    return xmemory_tmpdirname ;
}
/* vim: set ts=4 et sw=4 tw=75 */
