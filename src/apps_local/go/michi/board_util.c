// board_util.c -- Utilities for the board model
#include "board.h"
static char buf[BUFLEN];
static int  delta[] = { -N-1,   1,  N+1,   -1, -N,  W,  N, -W};

//----------------------------- messages logging ------------------------------
FILE    *flog;             // FILE to log messages
int     c1,c2;             // counters for warning messages
int     nmsg;              // number of written log entries

void too_many_msg(void)
// if too many messages have been logged, print a last error and exit 
{
    fprintf(stderr,"Too many messages have been written in log file "
                       " (maximum 100000)\n");
    fprintf(flog,"Too many messages (maximum 100000)\n");
    exit(-1);
}

void log_fmt_s(char type, const char *msg, const char *s)
// Log a formatted message (string parameter)
{
    // FORK CHANGE: flog is opened by ui.c, which is not vendored, so a null
    // sink is the normal state here rather than an error. log_fmt_i and
    // log_fmt_p both funnel through this function, so this is the only guard
    // needed. The 'E' path below still exits, because it is only reached when
    // an allocation has failed and there is nothing to carry on with.
    if (flog == NULL) {
        if (type != 'E') return;
    } else {
        fprintf(flog, "%c %5d/%3.3d ", type, c1, c2);
        fprintf(flog, msg, s);
        fprintf(flog, "\n");
    }
    if(type == 'E') {
        fprintf(stderr,"\nERROR - ");
        fprintf(stderr, msg, s); fprintf(stderr,"\n");
        exit(-1);
    }
    if(nmsg++ > 1000000) too_many_msg();
}

void log_fmt_i(char type, const char *msg, int n)
// Log a formatted message (int parameter)
{
    sprintf(buf, msg, n);
    log_fmt_s(type, "%s", buf);
}

void log_fmt_p(char type, const char *msg, Point pt)
// Log a formatted message (point parameter)
{
    char str[8];
    sprintf(buf, msg, str_coord(pt,str));
    log_fmt_s(type,"%s", buf);
}

void fatal_error(const char *msg)
{
    log_fmt_s('E', "FATAL ERROR - %s", msg);
    exit(-1);
}

//--------------------------- Print of the Position ---------------------------
static char* colstr  = "@ABCDEFGHJKLMNOPQRST";
void make_pretty(Position *pos, char pretty_board[BOARDSIZE], int *capB
                                                                , int *capW)
{
    for (int k=0 ; k<BOARDSIZE ; k++) {
        if (point_color(pos, k) == BLACK)      pretty_board[k] = 'X';
        else if (point_color(pos, k) == WHITE) pretty_board[k] = 'O';
        else if (point_color(pos, k) == EMPTY) pretty_board[k] = '.';
        else                                   pretty_board[k] = ' ';
    }
    *capW = pos->caps[0];
    *capB = pos->caps[1];
}

void print_board(Position *pos, FILE *f)
// Print visualization of the given board position
{
    // FORK CHANGE: a null sink is the normal state here (see log_fmt_s). The
    // only caller that can reach this with flog unset is michi_assert's failure
    // path, and a crash inside the code that reports a crash reports nothing.
    if (f == NULL) return;
    char pretty_board[BOARDSIZE], strko[8];
    int  capB, capW;

    make_pretty(pos, pretty_board, &capB, &capW);
    fprintf(f,"Move: %-3d   Black: %d caps   White: %d caps   Komi: %.1f",
            board_nmoves(pos), capB, capW, board_komi(pos)); 
    if (board_ko(pos))
        fprintf(f,"   ko: %s", str_coord(board_ko(pos), strko)); 
    fprintf(f,"\n");

    int size = board_size(pos), skipped=N-size+1;
    int k=skipped*(N+1);
    for (int row=skipped ; row<=N ; row++) {
        if (board_last_move(pos) == k+1) fprintf(f, " %-2d(", N-row+1); 
        else                             fprintf(f, " %-2d ", N-row+1);
        k++;
        for (int col=1 ; col<=size ; col++, k++) {
            fprintf(f, "%c", pretty_board[k]);
            if (board_last_move(pos) == k+1)    fprintf(f, "(");
            else if (board_last_move(pos) == k) fprintf(f, ")");
            else                                fprintf(f, " ");
        }
        k += skipped-1;
        fprintf(f, "\n");
    }
    fprintf(f, "    ");
    for (int col=1 ; col<=size ; col++) fprintf(f, "%c ", colstr[col]);
    fprintf(f, "\n\n");
}

//------------------ Memory allocation with error checking --------------------
// FORK CHANGE: every allocation michi makes goes through these two, which is
// what makes them the one place to put it somewhere else. On the device that is
// PSRAM: the search tree is the big one -- hundreds of kilobytes at the
// simulation counts this app uses -- and internal SRAM does not have it to
// spare. PSRAM is slower per access and the tree is the least cache-sensitive
// thing here, unlike the 3x3 pattern table, which stays where it was.
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
// PSRAM first, internal RAM only if that fails. A shortfall here ends in
// exit(), which on this device is a reboot, so the fallback is worth the two
// lines: the allocations are a few hundred kilobytes of tree and PSRAM
// fragmenting is the likelier of the two ways to run out.
static void* michi_alloc(size_t n)
{
    void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p == NULL) p = malloc(n);
    return p;
}
#define MICHI_ALLOC(n) michi_alloc(n)
#else
#define MICHI_ALLOC(n) malloc(n)
#endif

void* michi_malloc(size_t size)
// Michi memory allocator - fatal error if allocation failed
{
    void* t = MICHI_ALLOC(size);
    if (t == NULL) {
        log_fmt_i('E',"Memory allocation failed (size = %d bytes)", size);
        exit(-1);
    }
    return t;
}

void* michi_calloc(size_t nmemb, size_t size)
// Michi memory allocator - fatal error if allocation failed
{
    void* t = MICHI_ALLOC(nmemb * size);
    if (t != NULL) memset(t, 0, nmemb * size);
    if (t == NULL) {
        log_fmt_i('E',"Memory allocation failed (size = %d bytes)", size*nmemb);
        exit(-1);
    }
    return t;
}

//-------------------- Parsing and writing coordinates ------------------------
Point parse_coord(char *s)
{
    char c, str[10];
    int  x,y;
    for (int i=0 ; i<9 ; i++) {
        if (s[i]==0) {
            str[i]=0;
            break;
        }
        str[i] = toupper(s[i]);
    } 
    if(strcmp(str, "PASS") == 0) return PASS_MOVE;
    if(isalpha(str[0]) && isdigit(str[1]) 
            && (str[2]==0 || (isdigit(str[2]) && str[3]==0 ))) {
        sscanf(str, "%c%d", &c, &y);
        if (c<'J') x = c-'@';
        else       x = c-'@'-1;
        return (N-y+1)*(N+1) + x;
    }
    else
        return INVALID_VERTEX;
}

char* str_coord(Point pt, char str[8])
{
    if (pt == PASS_MOVE) strcpy(str, "pass");
    else if (pt == RESIGN_MOVE) strcpy(str, "resign");
    else {
        int row = pt/(N+1); int col = (pt%(N+1));
        sprintf(str, "%c%d", '@'+col,N+1-row);
        if (str[0] > 'H') str[0]++;
    }
    return str;
}

void ppoint(Point pt) {
    char str[8];
    str_coord(pt,str);
    printf("%s\n",str);
}

Point parse_sgf_coord(char *s, int size)
{
    int  x,y;

    if(s[0] == 0) return PASS_MOVE;
    x = s[0] - 'a';
    y = s[1] - 'a' + (N - size);
    return (y+1)*(N+1) + x + 1;
}

char* str_sgf_coord(Point pt, char str[8], int size)
{
    if (pt == PASS_MOVE) strcpy(str, "pass");
    else if (pt == RESIGN_MOVE) strcpy(str, "resign");
    else {
        int row = pt/(N+1) - 1; int col = (pt%(N+1)) - 1;
        sprintf(str, "%c%c", 'a' + col, 'a' + row - (N-size));
    }
    return str;
}

//------------------------ Costly functions on Slist --------------------------
char* slist_str_as_int(Slist l) {
    buf[0]=0;
    for (int k=1, n=l[0] ; k<=n ; k++) {
        char s[32];
        sprintf(s, " %d", l[k]);
        strcat(buf, s);
    }
    return buf;
}

char* slist_str_as_point(Slist l) {
    buf[0]=0;
    for (int k=1, n=l[0] ; k<=n ; k++) {
        char str[8], s[8];
        sprintf(s, " %s", str_coord(l[k],str));
        strcat(buf, s);
    }
    return buf;
}

void slist_write_as_int(Slist l, FILE *f) 
{
    fprintf(f, "%s", slist_str_as_int(l));
}

void slist_write_as_point(Slist l, FILE *f) 
{
    fprintf(f, "%s", slist_str_as_point(l));
}

void slist_print_as_int(Slist l)
{
    slist_write_as_int(l, stdout);
    printf("\n");
}

void slist_print_as_point(Slist l)
{
    slist_write_as_point(l, stdout);
    printf("\n");
}

char* slist_from_str_as_point(Slist l, char *str)
{
    char *ret, *s;
    Point pt;

    slist_clear(l);
    if (str != NULL) {
        s = strtok(str, " \t\n");
        if (s != NULL) {
            while (s != NULL) {
                pt = parse_coord(s);
                if (pt == INVALID_VERTEX) break;
                slist_push(l, parse_coord(s));    
                s=strtok(NULL," \t");
            }
            if (pt == INVALID_VERTEX) {
                sprintf(buf, "Error - Invalid vertex \"%s\"", s);
                ret = buf;
            }
            else
                ret = "";
        }
        else
            ret = "Error - The list of points is empty";
    }
    else {
        ret = "Error - The list of points is empty";
    }
    return ret;
}

char* block_liberties_as_string(Position *pos, Block b)
// Return the list of liberties of block b as String (in buf)
{
    Point libs[BOARDSIZE];
    block_compute_libs(pos, b, libs, BOARDSIZE);
    return slist_str_as_point(libs);
}

//------------------ Integrity checks of the Position struct ------------------
void dump_env4(Byte env4, Byte true_env4)
{
    for (int i=0 ; i<8 ; i++) {
        if (i == 4) fprintf(stderr, " ");
        if (env4 & 128)
            fprintf(stderr, "1");
        else
            fprintf(stderr, "0");
        env4 <<= 1;
    }
    fprintf(stderr, " (true: ");
    for (int i=0 ; i<8 ; i++) {
        if (i==4) fprintf(stderr, " ");
        if (true_env4 & 128)
            fprintf(stderr, "1");
        else
            fprintf(stderr, "0");
        true_env4 <<= 1;
    }
    fprintf(stderr, ")\n");
}

int env4_OK(Position *pos)
{
    int nerr=0;
    FORALL_POINTS(pos,pt) {
        if (point_color(pos,pt) == OUT) continue;
        if (point_env4(pos,pt) != compute_env4(pos, pt, 0)) {
            log_fmt_p('E', "Incorrect env4[%s]", pt);
            fprintf(stderr, "%s ERR env4 = ", str_coord(pt,buf));
            dump_env4(point_env4(pos,pt), compute_env4(pos,pt,0));
            if (nerr++>9) break;
        }
        if (point_env4d(pos,pt) != compute_env4(pos,pt,4)) {
            log_fmt_p('E', "Incorrect env4d[%s]", pt);
            fprintf(stderr, "%s ERR env4d = ", str_coord(pt,buf));
            dump_env4(point_env4d(pos,pt), compute_env4(pos,pt,4));
            if (nerr++>9) break;
        }
    }
    if (nerr>0) return 0;
    else        return 1;
}

int check_block(Position *pos, Point pt)
// return 1 if block at point pt is OK, else 0
{
    Block b=point_block(pos, pt);
    char  str[8], str_pt[8];
    int   nerrs=0;
    Point stones[BOARDSIZE], libs[BOARDSIZE], libs2[BOARDSIZE];

    str_coord(pt, str_pt);
    block_compute_libs(pos, b, libs2, BOARDSIZE);

    // Check points where there is no stone
    if (point_color(pos,pt) == OUT) {
        if (point_block(pos, pt) == 0)
            return 1;
        else {
            sprintf(buf,"Wrong block number %d at %s (which is OUT)", b,
                                                        str_coord(pt, str));
            log_fmt_s('E', buf, NULL);
            return 0;
        }
    }
    if (point_color(pos,pt) == EMPTY) {
        if (point_block(pos, pt) == 0)
            return 1;
        else {
            sprintf(buf,"Wrong block number %d at %s (which is EMPTY)", b,
                                                        str_coord(pt, str));
            log_fmt_s('E', buf, NULL);
            return 0;
        }
    }

    // Check point where there is a stone
    if (point_block(pos, pt) == 0) {
        sprintf(buf,"Wrong block number %d at %s (which is NOT EMPTY)", b,
                                                           str_coord(pt, str));
        log_fmt_s('E', buf, NULL);
        if(nerrs++ > 10) return 0;
    }
    compute_block(pos, pt, stones, libs, 256);
    slist_sort(libs);

    // Global verification
    if (block_size(pos,b) != slist_size(stones)) {
        if (slist_size(stones)>255 && block_size(pos,b)==255) goto next_check;
        sprintf(buf,"Wrong size of block %d at %s (%d instead of %d)",
                b, str_coord(pt, str), block_size(pos, b), slist_size(stones));
        log_fmt_s('E', buf, NULL);
        if(nerrs++ > 10) return 0;
    }
next_check:
    if (block_nlibs(pos,b) != slist_size(libs)) {
        sprintf(buf,"Wrong nlibs of block %d at %s (%d instead of %d)",
                b, str_coord(pt, str), block_nlibs(pos,b), slist_size(libs));
        log_fmt_s('E', buf, NULL);
        log_fmt_s('I', block_liberties_as_string(pos, b), NULL); 
        log_fmt_s('I', slist_str_as_point(libs), NULL); 
        if(nerrs++ > 10) return 0;
    }
    if (block_nlibs(pos,b) ==0) {
        sprintf(buf,"Error block %d(%s) with 0 liberty", b, str_coord(pt, str));
        log_fmt_s('E', buf, NULL);
        if(nerrs++ > 10) return 0;
    }

    // Detailed check of stones
    b = point_block(pos,pt);
    FORALL_IN_SLIST(stones, i) {
        if (point_block(pos,i) != b) {
            sprintf(buf,"Invalid block id (%d) at point %s ",
                    point_block(pos,i), str_coord(i, str));
            log_fmt_s('E', buf, NULL);
            if(nerrs++ > 10) return 0;
        }
    }

    // Detailed check of liberties
    FORALL_IN_SLIST(libs, l) {
        int k = (l-N)>>5, r = (l-N)&31;
        if ((pos->libs[b][k] & (1<<r)) == 0) {
            char str1[8];
            sprintf(buf,"Liberty %s not set for block %d(%s) ",
                                  str_coord(l, str), b, str_coord(pt, str1));
            log_fmt_s('E', buf, NULL);
            if(nerrs++ > 10) return 0;
        }
    }

    if (slist_size(libs2) != slist_size(libs)) {
        sprintf(buf,"Bad liberty list size for block %d(%s) : %d instead of %d"
                , b, str_coord(pt, str), slist_size(libs2), slist_size(libs));
        log_fmt_s('E', buf, NULL);
        if(nerrs++ > 10) return 0;
    }
    FORALL_IN_SLIST(libs2, l) {
        if (point_color(pos, l) != EMPTY) {
            sprintf(buf,"Liberty %s of block %d(%s) is not EMPTY",
                    str_coord(l, str), b, str_pt);
            log_fmt_s('E', buf, NULL);
            if(nerrs++ > 10) return 0;
        }
    }

    return nerrs==0;    // OK if nerrs == 0
}

int blocks_OK(Position *pos, Point pt)
// Return 1 (True) if block at point pt and blocks in contact with pt are OK
{
    Block b=point_block(pos, pt);
    int   k;
    Point n;
    if (check_block(pos, pt)==0) return 0;

    FORALL_NEIGHBORS(pos, pt, k, n)
        if (point_block(pos, n) != b && check_block(pos, n)==0) return 0;
    return 1;               // if all tests succeed blocks around pt are OK
}

int all_blocks_OK(Position *pos)
// Return 1 (True) if all blocks on the board are OK
{
    mark_init(mark2);
    mark(mark2, 0);
    FORALL_POINTS(pos, pt) {
        Block b=point_block(pos, pt); 
        if (!is_marked(mark2, b)) {
            mark(mark2, b);
            if (check_block(pos, pt) == 0) return 0;
        }
    }
    mark_release(mark2);
    return 1;
}

//----------------------- Large board (7 point large border) ------------------
// This large board is used for matching the large patterns

char large_board[LARGE_BOARDSIZE];
int  large_coord[BOARDSIZE]; // coord in the large board of any points of board

void compute_large_coord(void)
// Compute the position in the large board of any point on the board
{
    int lpt, pt;
    for (int y=0 ; y<N ; y++)
       for (int x=0 ; x<N ; x++) {
           pt  = (y+1)*(N+1) + x+1;
           lpt = (y+7)*(N+7) + x+7;
           large_coord[pt] = lpt;
       }
}

void init_large_board(void)
// Initialize large board (i.e. board with a 7 point border)
{
    memset(large_board, '#', LARGE_BOARDSIZE);
    compute_large_coord();
}

int large_board_OK(Position *pos)
{
    char large_board_color[4];
    if (board_color_to_play(pos) == BLACK) {
        large_board_color[EMPTY] = '.';
        large_board_color[OUT]   = '#';
        large_board_color[WHITE] = 'x';
        large_board_color[BLACK] = 'X';
    }
    else {
        large_board_color[EMPTY] = '.';
        large_board_color[OUT]   = '#';
        large_board_color[WHITE] = 'X';
        large_board_color[BLACK] = 'x';
    }

    FORALL_POINTS(pos,pt) {
        if (point_color(pos,pt) == OUT) continue;
        if (large_board_color[point_color(pos,pt)] != large_board[large_coord[pt]])
            return 0;
    }
    return 1;
}

void copy_to_large_board(Position *pos)
// Copy the current position to the large board
{
    char large_board_color[4];
    int lpt=(N+7)*7+7, pt=(N+1)+1;
    if (board_color_to_play(pos) == BLACK) {
        large_board_color[EMPTY] = '.';
        large_board_color[OUT]   = '#';
        large_board_color[WHITE] = 'x';
        large_board_color[BLACK] = 'X';
    }
    else {
        large_board_color[EMPTY] = '.';
        large_board_color[OUT]   = '#';
        large_board_color[WHITE] = 'X';
        large_board_color[BLACK] = 'x';
    }
    for (int y=0 ; y<N ; y++, lpt+=7, pt++)
       for (int x=0 ; x<N ; x++)
           large_board[lpt++] = large_board_color[point_color(pos, pt++)];
    assert(large_board_OK(pos));
}

void print_large_board(FILE *f)
// Print visualization of the current large board position
{
    int k=0;
    fprintf(f,"\n\n");
    for (int row=0 ; row<N+14 ; row++) {
        for (int col=0 ; col<N+7 ; col++,k++) fprintf(f, "%c ", large_board[k]);
        fprintf(f,"\n");
    }
    fprintf(f,"\n\n");
}
