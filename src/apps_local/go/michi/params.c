// params.c -- Variable parameters used by the michi program
#include "michi.h"
static char buf[4096];
static int  raw;
// ----------------- Initial Values for Tree Policy parameters ----------------
int   N_SIMS             = 2000;
int   RAVE_EQUIV         = 3500;
int   EXPAND_VISITS      = 8;
int   PRIOR_EVEN         = 10;   // should be even number; 0.5 prior 
int   PRIOR_SELFATARI    = 10;   // negative prior
int   PRIOR_CAPTURE_ONE  = 15;
int   PRIOR_CAPTURE_MANY = 30;
int   PRIOR_PAT3         = 10;
int   PRIOR_LARGEPATTERN = 100;  // most moves have relatively small probability
int   PRIOR_CFG[]        =     {24, 22, 8};
int   LEN_PRIOR_CFG      = (sizeof(PRIOR_CFG)/sizeof(int));
int   PRIOR_EMPTYAREA    = 10;
// --------------- Initial Values for Random Policy parameters-- --------------
double PROB_HEURISTIC_CAPTURE = 0.9;   // probability of heuristic suggestions
double PROB_HEURISTIC_PAT3    = 0.95;  // being taken in playout
double PROB_SSAREJECT = 0.9;// prob of rejecting suggested self-atari in playout
double PROB_RSAREJECT = 0.5;// prob of rejecting random self-atari in playout
                           // this is lower than above to allow nakade
// ----------------- Initial Values for general parameters --------------------
int    use_dynamic_komi        = 0;
double komi_per_handicap_stone = 7.0;
int    play_until_the_end      = 0;
int    random_seed             = 1;
int    REPORT_PERIOD           = 200000;
int    verbosity               = 2;
double FASTPLAY20_THRES= 0.8;//if at 20% playouts winrate is >this, stop reading
double FASTPLAY5_THRES = 0.95;//if at 5% playouts winrate is >this, stop reading
double RESIGN_THRES            = 0.2;
char   Live_gfx[80]            = "None";
int    Live_gfx_interval       = 1000;
//==================================== Code ===================================
int pk_line;
char *margin[] = {
      "        ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
    "          ",
};
#define PRINT_KEY_VALUE(p, fmt) \
    if (raw) \
        sprintf(key_value, "%30s " #fmt "\n", #p, p); \
    else \
        sprintf(key_value, "%s %30s " #fmt "\n", margin[pk_line], #p, p); \
    strcat(buf, key_value); \
    pk_line++;
   
#define PRINT_BOOL_KEY_VALUE(p, fmt) \
    if (raw) \
        sprintf(key_value, "%30s " #fmt "\n", #p, p); \
    else \
        sprintf(key_value, "%s[bool] %30s " #fmt "\n", margin[pk_line], #p, p); \
    strcat(buf, key_value); \
    pk_line++;
   
#define PRINT_LIST_KEY_VALUE(list, p, fmt) \
    if (raw) \
        sprintf(key_value, "%30s " #fmt "\n", #p, p); \
    else \
        sprintf(key_value, "%s %s " #fmt "\n", list, #p, p); \
    strcat(buf, key_value); \
    pk_line++;

#define READ_VALUE(p, fmt) {                                                  \
    char *value = strtok(NULL," \t\n");                                       \
    if (value == NULL)                                                        \
        ret = "Error! No value";                                              \
    else {                                                                    \
        sscanf(value, #fmt, &p);                                              \
        ret = "";                                                             \
    }                                                                         \
}

#define READ_VALUE_STRING(p, fmt) {                                           \
    char *value = strtok(NULL," \t\n");                                       \
    if (value == NULL)                                                        \
        ret = "Error! No value";                                              \
    else {                                                                    \
        sscanf(value, #fmt, p);                                               \
        ret = "";                                                             \
    }                                                                         \
}

char* param_playout(const char *param) 
// Implement gtp command that list or modify parameters of the playout policy
{
    char *ret=buf, key_value[80];
    char *known_params = "\nPROB_HEURISTIC_CAPTURE\nPROB_HEURISTIC_PAT3\n"
        "PROB_REJECT_SELF_ATARI_RANDOM\nPROB_REJECT_SELF_ATARI_SUGGESTED\n";

    if (param == NULL) {    // List current values
        pk_line = 0;
        buf[0] = 0;
        PRINT_KEY_VALUE(PROB_HEURISTIC_CAPTURE, %.2f);
        PRINT_KEY_VALUE(PROB_HEURISTIC_PAT3, %.2f);
        PRINT_KEY_VALUE(PROB_SSAREJECT, %.2f);
        PRINT_KEY_VALUE(PROB_RSAREJECT, %.2f);
    }
    else if (strcmp(param, "help") == 0)
        ret = known_params;
    else if (strcmp(param, "list") == 0)
        ret = known_params;
    else if (strcmp(param, "PROB_HEURISTIC_CAPTURE") == 0)
        READ_VALUE(PROB_HEURISTIC_CAPTURE, %lf)
    else if (strcmp(param, "PROB_HEURISTIC_PAT3") == 0)
        READ_VALUE(PROB_HEURISTIC_PAT3, %lf)
    else if (strcmp(param, "PROB_SSAREJECT") == 0)
        READ_VALUE(PROB_SSAREJECT, %lf)
    else if (strcmp(param, "PROB_RSAREJECT") == 0)
        READ_VALUE(PROB_RSAREJECT, %lf)
    else {
        log_fmt_s('W',"%s is not a param_playout", param);
        strtok(NULL," \t\n");                   // make the strtok buffer empty
        ret = "Error - not a param_playout";
    }
    return ret;
}

char* param_tree(const char *param) 
// Implement gtp command that list or modify parameters of the tree policy
{
    char *ret=buf, key_value[80];
    char *known_params = "\nN_SIMS\nRAVE_EQUIV\nEXPAND_VISITS\n"
        "PRIOR_EVEN\nPRIOR_SELFATARI\nPRIOR_CAPTURE_ONE\nPRIOR_CAPTURE_MANY\n"
        "PRIOR_PAT3\nPRIOR_LARGEPATTERN\nPRIOR_CFG[0]\nPRIOR_CFG[1]\n"
        "PRIOR_CFG[2]\nPRIOR_EMPTYAREA\n";

    if (param == NULL) {    // List current values
        pk_line = 0;
        buf[0] = 0;
        PRINT_KEY_VALUE(N_SIMS, %d);
        PRINT_KEY_VALUE(RAVE_EQUIV, %d);
        PRINT_KEY_VALUE(EXPAND_VISITS, %d);
        PRINT_KEY_VALUE(PRIOR_EVEN, %d);
        PRINT_KEY_VALUE(PRIOR_SELFATARI, %d);
        PRINT_KEY_VALUE(PRIOR_CAPTURE_ONE, %d);
        PRINT_KEY_VALUE(PRIOR_CAPTURE_MANY, %d);
        PRINT_KEY_VALUE(PRIOR_PAT3, %d);
        PRINT_KEY_VALUE(PRIOR_LARGEPATTERN, %d);
        PRINT_KEY_VALUE(PRIOR_CFG[0], %d);
        PRINT_KEY_VALUE(PRIOR_CFG[1], %d);
        PRINT_KEY_VALUE(PRIOR_CFG[2], %d);
        PRINT_KEY_VALUE(PRIOR_EMPTYAREA, %d);
    }
    else if (strcmp(param, "help") == 0)
        ret = known_params;
    else if (strcmp(param, "list") == 0)
        ret = known_params;
    else if (strcmp(param, "N_SIMS") == 0)
        READ_VALUE(N_SIMS, %d)
    else if (strcmp(param, "RAVE_EQUIV") == 0)
        READ_VALUE(RAVE_EQUIV, %d)
    else if (strcmp(param, "EXPAND_VISITS") == 0)
        READ_VALUE(EXPAND_VISITS, %d)
    else if (strcmp(param, "PRIOR_EVEN") == 0)
        READ_VALUE(PRIOR_EVEN, %d)
    else if (strcmp(param, "PRIOR_SELFATARI") == 0)
        READ_VALUE(PRIOR_SELFATARI, %d)
    else if (strcmp(param, "PRIOR_CAPTURE_ONE") == 0)
        READ_VALUE(PRIOR_CAPTURE_ONE, %d)
    else if (strcmp(param, "PRIOR_CAPTURE_MANY") == 0)
        READ_VALUE(PRIOR_CAPTURE_MANY, %d)
    else if (strcmp(param, "PRIOR_PAT3") == 0)
        READ_VALUE(PRIOR_PAT3, %d)
    else if (strcmp(param, "PRIOR_LARGEPATTERN") == 0)
        READ_VALUE(PRIOR_LARGEPATTERN, %d)
    else if (strcmp(param, "PRIOR_CFG[0]") == 0)
        READ_VALUE(PRIOR_CFG[0], %d)
    else if (strcmp(param, "PRIOR_CFG[1]") == 0)
        READ_VALUE(PRIOR_CFG[1], %d)
    else if (strcmp(param, "PRIOR_CFG[2]") == 0)
        READ_VALUE(PRIOR_CFG[2], %d)
    else if (strcmp(param, "PRIOR_EMPTYAREA") == 0)
        READ_VALUE(PRIOR_EMPTYAREA, %d)
    else {
        log_fmt_s('W',"%s is not a param_tree", param);
        strtok(NULL," \t\n");                   // make the strtok buffer empty
        ret = "Error - not a param_tree";
    }
    return ret;
}

unsigned int true_random_seed(void)
// return a true random seed (which depends on the time)
{
    unsigned int r1, r2, sec, day;
    time_t tm=time(NULL);
    struct tm *tcal=localtime(&tm);
    sec  = tcal->tm_sec + 60*(tcal->tm_min + 60*tcal->tm_hour);
    // day is a coarse (but sufficient for the current purpose) approximation 
    day = tcal->tm_mday + 31*(tcal->tm_mon + 12*tcal->tm_year);
    // Park & Miller random generator (same as qdrandom())
    r1 =  (1664525*sec) + 1013904223;
    r2 = (1664525*day) + 1013904223;
    return (r1^r2);
}

char* param_general(const char *param) 
// Implement gtp command that list or modify general parameters
{
    char *ret=buf, key_value[800];
    char *known_params = "play_until_the_end\nuse_dynamic_komi\nverbosity\n"
                         "komi_per_handicap_stone\n"
                         "REPORT_PERIOD\nRESIGN_THRES\n"
                         "FASTPLAY20_THRES\nFASTPLAY5_THRES\n";

    if (param == NULL) {    // List current values
        pk_line = 0;
        buf[0] = 0;
        margin[0] = "  ";
        margin[2] = "    ";
        PRINT_BOOL_KEY_VALUE(use_dynamic_komi, %d);
        PRINT_KEY_VALUE(komi_per_handicap_stone, %.1lf);
        PRINT_BOOL_KEY_VALUE(play_until_the_end, %d);
        PRINT_KEY_VALUE(random_seed, %d);
        PRINT_KEY_VALUE(REPORT_PERIOD, %d);
        PRINT_KEY_VALUE(verbosity, %d);
        PRINT_KEY_VALUE(RESIGN_THRES, %.2f);
        PRINT_KEY_VALUE(FASTPLAY20_THRES, %.2f);
        PRINT_KEY_VALUE(FASTPLAY5_THRES, %.2f);
        PRINT_LIST_KEY_VALUE("[list/None/best_moves/owner_map/"
                                   "principal_variation/visit_count]",
                                   Live_gfx, %s);
        PRINT_KEY_VALUE(Live_gfx_interval, %d);
        margin[0] = "        ";
        margin[2] = "          ";
    }
    else if (strcmp(param, "help") == 0)
        ret = known_params;
    else if (strcmp(param, "list") == 0)
        ret = known_params;
    else if (strcmp(param, "play_until_the_end") == 0)
        READ_VALUE(play_until_the_end, %d)
    else if (strcmp(param, "use_dynamic_komi") == 0)
        READ_VALUE(use_dynamic_komi, %d)
    else if (strcmp(param, "random_seed") == 0) {
        READ_VALUE(random_seed, %d)
        if (random_seed == 0)
            idum = true_random_seed();
        else
            idum = random_seed;
    }
    else if (strcmp(param, "REPORT_PERIOD") == 0)
        READ_VALUE(REPORT_PERIOD, %d)
    else if (strcmp(param, "verbosity") == 0)
        READ_VALUE(verbosity, %d)
    else if (strcmp(param, "RESIGN_THRES") == 0)
        READ_VALUE(RESIGN_THRES, %lf)
    else if (strcmp(param, "FASTPLAY20_THRES") == 0)
        READ_VALUE(FASTPLAY20_THRES, %lf)
    else if (strcmp(param, "FASTPLAY5_THRES") == 0)
        READ_VALUE(FASTPLAY5_THRES, %lf)
    else if (strcmp(param, "Live_gfx") == 0)
        READ_VALUE_STRING(Live_gfx, %s)
    else if (strcmp(param, "Live_gfx_interval") == 0)
        READ_VALUE(Live_gfx_interval, %d)
    else {
        log_fmt_s('W',"%s is not a param_general", param);
        strtok(NULL," \t\n");                   // make the strtok buffer empty
        ret = "Error - not a param_general";
    }
    return ret;
}

void params_fprint(FILE *f, const char *cmd) 
{
    char *str = strtok(buf,"\n");
    while (str != NULL) {
        fprintf(f, "%-15s %s\n", cmd, str);
        str = strtok(NULL,"\n");
    }
}

void make_params_default(FILE *f)
{
    raw = 1;
    param_general(NULL); params_fprint(f, "param_general");
    param_tree(NULL);    params_fprint(f, "param_tree");
    param_playout(NULL); params_fprint(f, "param_playout");
    raw = 0;
}
