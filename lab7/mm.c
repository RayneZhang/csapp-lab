/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "5140219237",
    /* First member's full name */
    "ZhangLei",
    /* First member's email address */
    "rayne_@sjtu.edu.cn",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<13)

#define MAX(x,y) ((x)>(y)? (x):(y))

#define PACK(size,alloc) ((size)|(alloc))

#define GET(p) (*(unsigned int*)(p))
#define PUT(p,val) (*(unsigned int *)(p)=(val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define SUC(bp) ((char*)bp+WSIZE)
#define PREV(bp) (char*)GET(bp)
#define NEXT(bp) (char*)GET(SUC(bp))

#define HDRP(bp) ((char*)bp-WSIZE)
#define FTRP(bp) ((char*)(bp)+GET_SIZE(HDRP(bp))-DSIZE)

#define NEXT_BLKP(bp) ((char* )(bp)+GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char* )(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

static char *heap_listp;
static void *endfree;

extern int mm_init(void);
extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);

static void offlist(void *bp){
    if (PREV(bp) && NEXT(bp)){
	PUT(SUC(PREV(bp)), NEXT(bp));
	PUT(NEXT(bp), PREV(bp));
    }
    else if (PREV(bp)){
	PUT(SUC(PREV(bp)), NULL);
	endfree = PREV(bp);
    }
    else if (NEXT(bp))
	PUT(NEXT(bp), NULL);
    else
	endfree = 0; 
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc){
        return bp;
    }

    else if (prev_alloc && !next_alloc){
        offlist(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
	PUT(SUC(NEXT_BLKP(bp)), GET(SUC(bp)));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    }

    else if (!prev_alloc && next_alloc){
        offlist(bp);

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	PUT(SUC(bp), GET(SUC(PREV_BLKP(bp))));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }

    else {
        offlist(bp);
        offlist(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
	PUT(SUC(NEXT_BLKP(bp)), GET(SUC(PREV_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    /*Allocate an even numer of words to maintain alignment*/
    size = (words%2)? (words+1)*WSIZE : words * WSIZE;
    if((long)(bp=mem_sbrk(size))== -1)
        return NULL;

    /*Initialize free block header/footer and the epilogue header*/
    PUT(HDRP(bp),PACK(size,0));   //free block header
    PUT(FTRP(bp),PACK(size,0));   //free block footer
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));  //New epilogue header

    PUT(bp, endfree);
    PUT(SUC(bp), NULL);
    if (endfree)
        PUT(SUC(endfree), bp);
    endfree = bp;
    /*Coalesce if the previous block was free*/
    return coalesce(bp);
}
 
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    endfree = 0;
    /* Create the initial empty heap*/
    if((heap_listp=mem_sbrk(4*WSIZE))==(void*)-1)
        return -1;
    PUT(heap_listp,0);                       /*Alignment padding*/
    PUT(heap_listp+(1*WSIZE),PACK(2*WSIZE,1)); /*Prologue header*/
    PUT(heap_listp+(2*WSIZE),PACK(2*WSIZE,1)); /*Prologue footer*/
    PUT(heap_listp+(3*WSIZE),PACK(0,1));     /*Epilogue header*/
    heap_listp += (2*WSIZE);

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
    return 0;
}

static void *find_fit(size_t asize){
    void *bp;
    bp = endfree;
    /*first fit search*/
    while (bp != NULL) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
	    return bp;
	bp = PREV(bp);
    }
    return NULL;
}

static void place (void*bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));

    offlist(bp);
    if((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize-asize,0));
        PUT(FTRP(bp),PACK(csize-asize,0));
        
        PUT(bp, endfree);
        PUT(SUC(bp), NULL);
        if (endfree)
            PUT(SUC(endfree), bp);
        endfree = bp;
    }
    else{
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;    //adjusted block size
    size_t extendsize;   //Amount to extend heap if no fit
    void* bp;
    
    /*Ignore spurious requests*/
    if(size==0)
        return NULL;
    
    /*Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else 
        asize = DSIZE * ((size + (DSIZE)+(DSIZE-1))/DSIZE);

    /*Search the free list fore a fit*/
    if((bp = find_fit(asize)) != NULL){
       //offlist(bp);
        place(bp,asize);
        return bp;
    }

    /*No fit found. Get more memory and place the block*/
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    //offlist(bp);
    place(bp,asize);
    return bp;
}



/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    void* bp;
    bp=ptr;
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));

    PUT(bp, endfree);
    PUT(SUC(bp), NULL);
    if (endfree)
	PUT(SUC(endfree), bp);
    endfree = bp;
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{   
    size_t asize;  //adjusted block size
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else 
        asize = DSIZE * ((size + (DSIZE)+(DSIZE-1))/DSIZE);
    
    size_t oldSize=GET_SIZE(HDRP(ptr));
    size_t nextSize=GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    //if two blocks can fit, then we just return the original ptr and adjust the size
    if(!GET_ALLOC(HDRP(NEXT_BLKP(ptr)))){
        if(asize < (oldSize+nextSize-2*DSIZE)){
            void *bp=ptr;
            void* prevfree=PREV(NEXT_BLKP(ptr));
            void* nextfree=NEXT(NEXT_BLKP(ptr));

            PUT(HDRP(bp),PACK(asize,1));
            PUT(FTRP(bp),PACK(asize,1));
            bp=(char*)bp+asize;
            PUT(HDRP(bp),PACK(oldSize+nextSize-asize,0));
            PUT(FTRP(bp),PACK(oldSize+nextSize-asize,0));
            PUT(bp,prevfree);

            if(prevfree)
                PUT(SUC(prevfree),bp);
            if(nextfree)
                PUT(nextfree,bp);
            else endfree=bp;
            return ptr;
        }
    }

    //else...
    void *oldptr = ptr;
    size_t copySize;
    void *newptr;

    if(ptr == NULL)
       return mm_malloc(size);
 
    if (asize==0) {
        mm_free(oldptr);
        return NULL;
    }
   
    else{
        newptr = mm_malloc(size);
        if (newptr == NULL)
           return NULL;
        copySize = GET_SIZE(HDRP(oldptr));
        if (asize < copySize)
           copySize = asize;
        memcpy(newptr, oldptr, copySize-WSIZE);
        mm_free(oldptr);
        return newptr;
    }
}














