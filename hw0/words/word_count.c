/*

Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/

#include "word_count.h"

/* Basic utilities */

char* new_string(char* str) {
    return strcpy((char*) malloc(strlen(str) + 1), str);
}

void init_words(WordCount** wclist) {
  /* Initialize word count. */
  *wclist = (WordCount *) malloc(sizeof(WordCount));
  if (*wclist == NULL) {
      return;
  }

  (*wclist)->count = 0;
  (*wclist)->word = NULL;
  (*wclist)->next = NULL;
}

size_t len_words(WordCount* wchead) {
  size_t len = 0;

  for (; wchead; wchead = wchead->next) {
      len = (wchead->word != NULL) ? len + wchead->count : len;
  }

  return len;
}

WordCount* find_word(WordCount* wchead, char* word) {
  /* Return count for word, if it exists */
  WordCount* wc = NULL;
  if (wchead == NULL) {
      return NULL;
  }

  for (; wchead; wchead = wchead->next) {
      if (wchead->word == NULL) {
          return NULL;
      }
      if (strcmp(wchead->word, word) == 0) {
        wc = wchead;
        break;
      }
  }
  return wc;
}

void add_word(WordCount** wclist, char* word) {
  /* If word is present in word_counts list, increment the count, otw insert with count 1. */
  WordCount *new_word, *found_word = find_word(*wclist, word);

  if (found_word == NULL) {
      if ((*wclist)->word == NULL) {
          new_word = *wclist;
      } else {
          init_words(&new_word);
          new_word->next = *wclist;
          *wclist = new_word;
      }
      /* Allocating memory for word field. */
      new_word->word = (char *) malloc(sizeof(char) * (strlen(word) + 1));
      if (new_word->word == NULL) {
          return;
      }

      strcpy(new_word->word, word);
      new_word->count = 1;
  } else {
      found_word->count += 1;
  }
}

void fprint_words(WordCount* wchead, FILE* ofile) {
  /* print word counts to a file */
  WordCount* wc;
  for (wc = wchead; wc; wc = wc->next) {
      if (wc->word != NULL) {
          fprintf(ofile, "%i\t%s\n", wc->count, wc->word);
      }
  }
}

void deallocate_word(WordCount* word) {
    if (word == NULL) {
        return;
    }

    if (word->word != NULL) {
        free(word->word);
    }

    free(word);
}


void deallocate_list(WordCount* wchead) {
    WordCount* temp;

    while (wchead) {
        temp = wchead;
        wchead = wchead->next;

        deallocate_word(temp);
    }
}
