#ifndef QUIZ_H
#define QUIZ_H

#include "../shared/protocol.h"

int taken_nickname(const char *nickname);
int load_quiz(char *filename, Quiz* quiz);
Question* get_question(Quiz* quiz, int index);
int check_answer (Question* question, const char* answer );
void get_leaderboard(int theme_num, char* leaderboard);
void save_score(int theme_num, const char* nickname, int score, int completed);
int init_player(const char *nickname);
void remove_player(const char *nickname);
void print_players_status(void);
int has_completed_quiz(const char* nickname, int theme_index);

#endif