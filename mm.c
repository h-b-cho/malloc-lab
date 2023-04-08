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
    "week 06 - team 4",
    /* First member's full name */
    "Haebin Cho",
    /* First member's email address */
    "hbtopkr@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8  // 데이터 정렬. 컴퓨터 메모리에서 데이터는 특정한 바이트 단위로 정렬되어 있어야 효율적으로 처리될 수 있다. ALIGNMENT 값은 데이터를 어떤 크기의 바이트 단위로 정렬할 것인지를 결정한다. 즉, 메모리 할당 및 해제 시, 데이터 형식의 정렬에 필요한 값. 대부분의 시스템에서는 8바이트의 ALIGNMENT 값을 사용한다.

/* rounds up to the nearest multiple of ALIGNMENT */
// size(변수)보다 크면서 가장 가까운 8의 배수로 만들어주는 것이 Align이다. -> 정렬!
// size = 7 : (00000111 + 00000111) & 11111000 = 00001110 & 11111000 = 00001000 = 8
// size = 13 : (00001101 + 00000111) & 11111000 = 00010000 = 16
// 즉, 함수 ALIGN의 역할은 입력된 size가 1 ~ 7 바이트일 때 -> 8 바이트,
// 8 ~ 16 바이트 -> 16 바이트,
// 7 ~ 24 바이트 -> 24 바이트로 정렬하여 할당되도록 하는 매크로인 것이다.
// 여기서 ~는 not 연산자로, 원래 0x7은 0000 0111이고 ~0x7은 1111 1000이 된다.
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)  // 입력된 size를 ALIGNMENT(2의 거듭제곱 값)의 배수로 만들어주는 역할.

/* 메모리 할당 시 */
// size_t는 '부호 없는 32비트 정수'로 unsigned int이다. 따라서 4바이트이다.
// 따라서 메모리 할당 시 기본적으로 header와 footer가 필요하므로 더블워드만큼의 메모리가 필요하다. size_t이므로 4바이트이니 ALIGN을 거치면 8바이트가 된다.
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))  // size_t 자료형의 크기를 ALIGNMENT의 배수로 맞추는 역할.

/* 기본 상수와 매크로 */
#define WSIZE       4                        // 워드 사이즈. (4 byte)
#define DSIZE       8                        // 더블 워드 사이즈. (8 byte)
#define CHUNKSIZE   (1<<12)                  // ==2의 12승(2^12 byte). 초기 free 블록이다. (<<는 shift 연산자. 2진수로 나타낼 경우 1을 왼쪽으로 12칸 이동함을 뜻한다. 따라서 CHUNKSIZE는 4096(= 2^12).)

#define MAX(x, y) (x>y?x:y)                  // 최댓값 함수 매크로.
#define MIN(x, y) (x>y?y:x)                  // 최댓값 함수 매크로.

#define PACK(size, alloc) ((size) | (alloc)) // free 리스트에서 header와 footer를 조작하는 데에는, 많은 양의 캐스팅 변환과 포인터 연산이 사용되기에 애초에 매크로로 만든다.
// PACK 매크로는 size와 alloc 값을 인자로 받아서, size와 alloc 값을 비트OR연산하여 하나의 값으로 결합한 결과를 반환합니다. 두 개의 비트열을 비교하여 하나라도 1이면 결과 비트열의 해당 위치를 1로 설정한다. 예를 들어, 0011과 0101의 비트 OR 연산 결과는 0111이 된다.
// 일반적으로 할당 여부는 1비트로 표현되며, 0은 할당되지 않음을 의미하고 1은 할당됨을 의미. 결과적으로 반환되는 값은 할당 여부를 비트의 마지막 위치에 포함한 크기 값을 갖게 된다.
// 애초에 size의 오른쪽 3자리는 000으로 비어져 있다. 왜? -> 메모리가 더블 워드 크기로 정렬되어 있다고 전제하기 때문이다. 따라서 size는 무조건 8바이트보다 큰 셈이다.

/* 포인터 p가 가르키는 워드의 값을 읽거나, p가 가르키는 워드에 값을 적는 매크로 */
#define GET(p)          (*(unsigned int *)(p))          // 보통 p는 void 포인터라고 한다면 곧바로 *p(* 자체가 역참조)를 써서 참조할 수 없기 때문에, 그리고 우리는 4바이트(1워드)씩 주소 연산을 한다고 전제하기에 unsigned int로 캐스팅 변환을 한다. p가 가르키는 곳의 값을 불러온다.
#define PUT(p, val)     (*(unsigned int *)(p) = (val))  // p가 가르키는 곳에 val를 넣는다.

/* header 혹은 footer의 값인 size or allocated 여부를 가져오는 매크로 */
#define GET_SIZE(p)     (GET(p) & ~0x7)                 // ~0x7 = 11111000. 블록의 사이즈만 가지고 온다. ex. 1011 0111 & 1111 1000 = 1011 0000으로 사이즈만 읽어옴을 알 수 있다.
#define GET_ALLOC(p)    (GET(p) & 0x1)                  // 0x1 = 00000001. 블록이 할당되었는지 free인지를 나타내는 flag를 읽어온다. ex. 입력값이 1011 일 때, 1011 0111 & 0000 0001 = 0000 0001로 이는 즉 allocated임을 알 수 있다.

/* 블록 포인터 bp(payload를 가르키고 있는 포인터)를 바탕으로 블록의 header와 footer의 주소를 반환하는 매크로 */
#define HDRP(bp)        ((char *)(bp) - WSIZE)                              // header는 payload보다 앞에 있으므로 4바이트(워드)만큼 빼줘서 앞으로 1칸 전진하게 한다.
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)         // footer는 payload에서 블록의 크기만큼 뒤로 간 다음 8바이트(더블 워드)만큼 빼줘서 앞으로 2칸 전진하게 해주면 footer가 나온다.
                                                                            // 이 때 포인터는 char형이어야 4 혹은 8바이트, 즉 정확히 1바이트씩 움직일 수 있다. 만약 int형으로 캐스팅 해주면 - WSIZE 했을 때 16바이트 만큼 움직일 수도 있다.

/* 블록 포인터 bp를 바탕으로, 이전과 다음 블록의 payload를 가르키는 주소를 반환하는 매크로 */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))   // 지금 블록의 payload를 가르키는 bp에, 지금 블록의 header 사이즈를 읽어서 더하면(word 만큼) 다음 블록의 payload를 가르키게 된다.
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))   // 지금 블록의 payload를 가르키는 bp에, 이전 블록의 footer 사이즈를 읽어서 뺴면(double word 만큼) 이전 블록의 payload를 가르키게 된다.

/* define searching method for find suitable free blocks to allocate */
// #define NEXT_FIT                        // define하면 next_fit, 안하면 first_fit으로 탐색한다.

/* global variable & functions */
static char *heap_listp;                // 항상 prologue block을 가리키는 정적 전역 변수 설정.
                                        // static 변수는 함수 내부(지역)에서도 사용이 가능하고 함수 외부(전역)에서도 사용이 가능하다.

// #ifdef NEXT_FIT                         // #ifdef ~ #endif를 통해 조건부로 컴파일이 가능하다. NEXT_FIT이 선언되어 있다면 밑의 변수를 컴파일 할 것이다.
//     static void *last_freep;            // next_fit 사용 시 마지막으로 탐색한 free 블록을 가리키는 포인터이다.
// #endif

static void *extend_heap(size_t words);
static void *coalesce(void* bp);
void mm_free(void* bp);
static void *find_fit(size_t asize);
static void place(void* bp, size_t asize);

void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words%2) ? (words+1)*WSIZE : words*WSIZE;       // words가 홀수로 들어왔다면 짝수로 바꿔준다. 짝수로 들어왔다면 그대로 WSIZE를 곱해준다. ex. 5만큼(5개의 워드 만큼) 확장하라고 하면, 6으로 만들고 24바이트로 만든다.
    
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }// 변환한 사이즈만큼 메모리 확보에 실패하면 NULL이라는 주소값을 반환해 실패했음을 알린다.
    // bp 자체의 값, 즉 주소값이 32bit이므로 long으로 캐스팅(형 변환)한다. 그리고 mem_sbrk() 함수가 실행되므로 bp는 새로운 메모리의 첫 주소값을 가르키게 된다.

    // 새 free 블록의 header와 footer를 정해준다. 자연스럽게 전 epilogue 자리에는 새로운 header가 자리 잡게 된다. 그리고 epilogue는 맨 뒤로 보내지게 된다.
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   // 새 free 블록의 header로, free 이므로 0을 부여.
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   // 새 free 블록의 footer로, free 이므로 0을 부여.
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ // 앞에서 현재 bp(새롭게 늘어난 메모리의 첫 주소 값으로 역시 payload이다)의 header에 값을 부여해주었다. 따라서 이 header의 사이즈 값을 참조해 다음 블록의 payload를 가르킬 수 있고, 이 payload의 직전인 header는 epilogue가 된다.

    return coalesce(bp);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void* ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
}

void *coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }// mem_sbrk()를 사용하여 4*WSIZE 바이트만큼 초기 힙 공간을 할당. 반환된 값이 (void *)-1 인 경우에는 힙 공간 할당이 실패했다는 것을 의미한다.

    PUT(heap_listp, 0);                            // Alignment padding으로 unused word이다. 맨 처음 메모리를 8바이트 정렬(더블 워드)을 위해 사용하는 미사용 패딩이다.
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // prologue header로, 맨 처음에서 4바이트 뒤에 header가 온다. 이 header에 사이즈(프롤로그는 8바이트)와 allocated 1(프롤로그는 사용하지 말라는 의미)을 통합한 값을 부여한다.
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // prologue footer로, 값은 header와 동일해야 한다.
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // epilogue header

    heap_listp += (2 * WSIZE); // 초기 프롤로그 블록 다음 위치로 heap_listp를 이동시킨다.

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

void *find_fit(size_t asize)
{
    void *bp;
    for ( bp = heap_listp; GET_SIZE(HDRP(bp))>0; bp = NEXT_BLKP(bp) ) {
        if ( (!GET_ALLOC(HDRP(bp))) && (asize<=GET_SIZE(HDRP(bp))) ) {
            return bp;
        }
    }
    return NULL;
}

void place(void *bp, size_t asize) 
{
    size_t current_size = GET_SIZE(HDRP(bp));

    if ((current_size - asize) < 2*DSIZE ) { // 사이즈 할당하고도 2*DSIZE (최소블럭크기) 이하로 남는다면 이후도 아무 것도 못 넣으니, 그냥 현재 도착한 블록 전체를 할당. 헤더, 푸터만 갱신.
        PUT(HDRP(bp), PACK(current_size, 1));
        PUT(FTRP(bp), PACK(current_size, 1));
    }
    else {                                   // 사이즈 할당하고도 최소블럭크기 이상으로 남으면 블록을 분할하여 딱 맞는 사이즈로 헤더와 푸터를 갱신.
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);             // 남은 블록을 새로운 블록으로서 새롭게 헤더와 푸터 지정.
        PUT(HDRP(bp), PACK(current_size - asize, 0)); 
        PUT(FTRP(bp), PACK(current_size - asize, 0));
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) // 한 마디로 mm_malloc() 함수는 인자로 size를 받으면 그 size에 적합한 메모리를 할당해주는 수행을 한다.
{
    size_t asize;            // 수정되는 블록의 사이즈.
    size_t extendsize;       // 맞는 크기의 가용 블록이 없을 시 확장하는 사이즈.
    char *bp;

    if (size == 0) {
        return NULL;
    }

    if (size <= DSIZE) {
        asize = 2*DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); // 할당하고자 하는 payload의 크기(?)가 8보다 크다면 '이것보다 큰 8의 배수인 숫자'byte로 메모리에 할당할 것이다. (현재 우리의 코드는 위에서 얼라인먼트가 8이라고 설정했다.)
        // 여기서, (DSIZE - 1): 계산을 하면 7인데 왜 7을 더하냐면 예를 들어, 23(size + header + footer)byte일 때
        // 24byte를 할당받아야하므로 우리가 DSIZE를 나누면 몫으로 3이 나와야한다.
        // 그래서 7을 더해서 30으로 만들어주고 몫만 필요하므로 나머지는 신경쓰지 않는 것이다.
        // 핵심은 몫을 구하기 위해 dummy값을 더하고 나머지는 신경쓰지 않아도 된다는 것.
        // 현재 우리의 코드는 위에서 얼라인먼트가 8이라고 설정했으므로 끝에 8로 나눈 몫을 앞에서 8로 곱해준다.
        // (size + DSIZE + 7) == 할당하고자 하는 내용의 크기 + block header(==4) + block footer(==4) + 오로지 적절한 값을 구하기 위하여 더해주는 값 7
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);

    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }

    place(bp, asize);

    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) // 기존에 할당된 메모리 블록의 크기를 변경하고 새로운 메모리 블록을 할당하는 함수이다.
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);

    if (newptr == NULL) {
        return NULL;
    }

    copySize = MIN(size, GET_SIZE(HDRP(oldptr)));   // 복사해야 하는 바이트 크기를 기존 메모리 블록의 크기와 새로운 메모리 블록의 크기 중 작은 값으로 결정한다.

    memcpy(newptr, oldptr, copySize);               // oldptr 영역에서 copySize-1 바이트를 newptr 영역으로 복사한다.
    mm_free(oldptr);

    return newptr;
}
